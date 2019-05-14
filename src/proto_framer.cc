#include <limits>
#include "proto_framer.h"
#include "ack_frame.h"
#include "logging.h"
#include "proto_comm.h"
#define QUIC_PREDICT_FALSE_IMPL(x) x
#define QUIC_PREDICT_FALSE(x) QUIC_PREDICT_FALSE_IMPL(x)
namespace dqc{
// Number of bytes reserved for the frame type preceding each frame.
const size_t kFrameTypeSize = 1;
// Size in bytes reserved for the delta time of the largest observed
// packet number in ack frames.
const size_t kDeltaTimeLargestObservedSize = 2;
// Size in bytes reserved for the number of received packets with timestamps.
const size_t kNumTimestampsSize = 1;
// Size in bytes reserved for the number of ack blocks in ack frames.
const size_t kNumberOfAckBlocksSize = 1;
// Acks may have only one ack block.
const uint8_t kHasMultipleAckBlocksOffset = 5;
// packet number size shift used in AckFrames.
const uint8_t kSequenceNumberLengthNumBits = 2;
const uint8_t kActBlockLengthOffset = 0;
const uint8_t kLargestAckedOffset = 2;
const uint8_t kProtoFrameTypeStreamMask = 0x80;
const uint8_t kProtoFrameTypeAckMask = 0x40;


// Timestamps are 4 bytes followed by 2 bytes.
const uint8_t kQuicNumTimestampsLength = 1;
const uint8_t kQuicFirstTimestampLength = 4;
const uint8_t kQuicTimestampLength = 2;
// Gaps between packet numbers are 1 byte.
const uint8_t kQuicTimestampPacketNumberGapLength = 1;


// Create a mask that sets the last |num_bits| to 1 and the rest to 0.
inline uint8_t GetMaskFromNumBits(uint8_t num_bits) {
  return (1u << num_bits) - 1;
}
static void SetBits(uint8_t* flags, uint8_t val, uint8_t num_bits, uint8_t offset){
  DCHECK_LE(val, GetMaskFromNumBits(num_bits));
  *flags |= val << offset;
}
// Set the bit at position |offset| to |val| in |flags|.
void SetBit(uint8_t* flags, bool val, uint8_t offset) {
  SetBits(flags, val ? 1 : 0, 1, offset);
}
size_t GetMinAckFrameSize(PacketNumber largest_observed_length){
  size_t min_size = kFrameTypeSize + largest_observed_length +
                    kDeltaTimeLargestObservedSize;
  return min_size + kNumTimestampsSize;
}
size_t GetAckFrameTimeStampSize(const AckFrame& ack) {
  if (ack.received_packet_times.empty()) {
    return 0;
  }

  return kQuicNumTimestampsLength + kQuicFirstTimestampLength +
         (kQuicTimestampLength + kQuicTimestampPacketNumberGapLength) *
             (ack.received_packet_times.size() - 1);
}
ProtoFramer::AckFrameInfo::AckFrameInfo()
:max_block_length(0)
,first_block_length(0)
,num_ack_blocks(0){
}
ProtoFramer::AckFrameInfo ProtoFramer::GetAckFrameInfo(const AckFrame& frame){
  AckFrameInfo new_ack_info;
  if (frame.packets.Empty()) {
    return new_ack_info;
  }
  // The first block is the last interval. It isn't encoded with the gap-length
  // encoding, so skip it.
  new_ack_info.first_block_length = frame.packets.LastIntervalLength();
  auto itr = frame.packets.rbegin();
  PacketNumber previous_start = itr->Min();
  new_ack_info.max_block_length = itr->Length();
  ++itr;

  // Don't do any more work after getting information for 256 ACK blocks; any
  // more can't be encoded anyway.
  for (; itr != frame.packets.rend() &&
         new_ack_info.num_ack_blocks < std::numeric_limits<uint8_t>::max();
       previous_start = itr->Min(), ++itr) {
    const auto& interval = *itr;
    const PacketNumber total_gap = previous_start - interval.Max();
    new_ack_info.num_ack_blocks +=
        (total_gap + std::numeric_limits<uint8_t>::max() - 1) /
        std::numeric_limits<uint8_t>::max();
    new_ack_info.max_block_length =
        std::max(new_ack_info.max_block_length, interval.Length());
  }
  return new_ack_info;
}
bool ProtoFramer::AppendPacketNumber(ProtoPacketNumberLength packet_number_length,
                                 PacketNumber packet_number,
                                 basic::DataWriter* writer){
  size_t length = packet_number_length;
  if (length != 1 && length != 2 && length != 4 && length != 6 && length != 8) {
    DLOG(FATAL)<< "Invalid packet_number_length: " << length;
    return false;
  }
  return writer->WriteBytesToUInt64(packet_number_length, packet_number);
}
bool ProtoFramer::AppendAckBlock(uint8_t gap,
                             ProtoPacketNumberLength length_length,
                             PacketNumber length,
                             basic::DataWriter* writer){
  return writer->WriteUInt8(gap) &&
         AppendPacketNumber(length_length, length, writer);
}
bool ProtoFramer::AppendAckFrameAndTypeByte(const AckFrame& frame,basic::DataWriter *writer){
  const AckFrameInfo new_ack_info = GetAckFrameInfo(frame);
  PacketNumber largest_acked = LargestAcked(frame);
  ProtoPacketNumberLength largest_acked_length =
      GetMinPktNumLen(largest_acked);
  ProtoPacketNumberLength ack_block_length = GetMinPktNumLen(
    new_ack_info.max_block_length);
  // Calculate available bytes for timestamps and ack blocks.
  int32_t available_timestamp_and_ack_block_bytes =
      writer->capacity() - writer->length() - ack_block_length -
      GetMinAckFrameSize(largest_acked_length) -
      (new_ack_info.num_ack_blocks != 0 ? kNumberOfAckBlocksSize : 0);
  DCHECK_LE(0, available_timestamp_and_ack_block_bytes);

  // Write out the type byte by setting the low order bits and doing shifts
  // to make room for the next bit flags to be set.
  // Whether there are multiple ack blocks.
  uint8_t type_byte = 0;
  SetBit(&type_byte, new_ack_info.num_ack_blocks != 0,
         kHasMultipleAckBlocksOffset);

  SetBits(&type_byte, PktNumLen2Flag(largest_acked_length),
          kSequenceNumberLengthNumBits, kLargestAckedOffset);

  SetBits(&type_byte, PktNumLen2Flag(ack_block_length),
          kSequenceNumberLengthNumBits, kActBlockLengthOffset);

  type_byte |= kProtoFrameTypeAckMask;

  if (!writer->WriteUInt8(type_byte)) {
    return false;
  }

  size_t max_num_ack_blocks = available_timestamp_and_ack_block_bytes /
                              (ack_block_length + PACKET_NUMBER_1BYTE);
  // Number of ack blocks.
  size_t num_ack_blocks =
      std::min(new_ack_info.num_ack_blocks, max_num_ack_blocks);
  if (num_ack_blocks > std::numeric_limits<uint8_t>::max()) {
    num_ack_blocks = std::numeric_limits<uint8_t>::max();
  }
  // Largest acked.
  if (!AppendPacketNumber(largest_acked_length, largest_acked, writer)) {
    return false;
  }
  // Largest acked delta time.
  uint64_t ack_delay_time_us = basic::kUFloat16MaxValue;
  if (!frame.ack_delay_time.IsInfinite()) {
    DCHECK_LE(0u, frame.ack_delay_time.ToMicroseconds());
    ack_delay_time_us = frame.ack_delay_time.ToMicroseconds();
  }
  if (!writer->WriteUFloat16(ack_delay_time_us)) {
    return false;
  }
  if (num_ack_blocks > 0) {
    if (!writer->WriteBytes(&num_ack_blocks, 1)) {
      return false;
    }
  }
  // First ack block length.
  if (!AppendPacketNumber(ack_block_length, new_ack_info.first_block_length,
                          writer)) {
    return false;
  }
  // Ack blocks.
  if (num_ack_blocks > 0) {
    size_t num_ack_blocks_written = 0;
    // Append, in descending order from the largest ACKed packet, a series of
    // ACK blocks that represents the successfully acknoweldged packets. Each
    // appended gap/block length represents a descending delta from the previous
    // block. i.e.:
    // |--- length ---|--- gap ---|--- length ---|--- gap ---|--- largest ---|
    // For gaps larger than can be represented by a single encoded gap, a 0
    // length gap of the maximum is used, i.e.:
    // |--- length ---|--- gap ---|- 0 -|--- gap ---|--- largest ---|
    auto itr = frame.packets.rbegin();
    PacketNumber previous_start = itr->min();
    ++itr;

    for (;
         itr != frame.packets.rend() && num_ack_blocks_written < num_ack_blocks;
         previous_start = itr->min(), ++itr) {
      const auto& interval = *itr;
      const PacketNumber total_gap = previous_start - interval.max();
      const size_t num_encoded_gaps =
          (total_gap + std::numeric_limits<uint8_t>::max() - 1) /
          std::numeric_limits<uint8_t>::max();
      DCHECK_LE(0u, num_encoded_gaps);

      // Append empty ACK blocks because the gap is longer than a single gap.
      for (size_t i = 1;
           i < num_encoded_gaps && num_ack_blocks_written < num_ack_blocks;
           ++i) {
        if (!AppendAckBlock(std::numeric_limits<uint8_t>::max(),
                            ack_block_length, 0, writer)) {
          return false;
        }
        ++num_ack_blocks_written;
      }
      if (num_ack_blocks_written >= num_ack_blocks) {
        if (QUIC_PREDICT_FALSE(num_ack_blocks_written != num_ack_blocks)) {
          DLOG(FATAL)<< "Wrote " << num_ack_blocks_written
                   << ", expected to write " << num_ack_blocks;
        }
        break;
      }

      const uint8_t last_gap =
          total_gap -
          (num_encoded_gaps - 1) * std::numeric_limits<uint8_t>::max();
      // Append the final ACK block with a non-empty size.
      if (!AppendAckBlock(last_gap, ack_block_length, interval.Length(),
                          writer)) {
        return false;
      }
      ++num_ack_blocks_written;
    }
    DCHECK_EQ(num_ack_blocks, num_ack_blocks_written);
  }

  // Timestamps.
  // If we don't process timestamps or if we don't have enough available space
  // to append all the timestamps, don't append any of them.
  if (process_timestamps_ && writer->capacity() - writer->length() >=
                                 GetAckFrameTimeStampSize(frame)) {
    if (!AppendTimestampsToAckFrame(frame, writer)) {
      return false;
    }
  } else {
    uint8_t num_received_packets = 0;
    if (!writer->WriteBytes(&num_received_packets, 1)) {
      return false;
    }
  }
    return true;
}
bool ProtoFramer::AppendTimestampsToAckFrame (const AckFrame& frame,basic::DataWriter* writer){
  DCHECK_GE(std::numeric_limits<uint8_t>::max(),
            frame.received_packet_times.size());
  // num_received_packets is only 1 byte.
  if (frame.received_packet_times.size() >
      std::numeric_limits<uint8_t>::max()) {
    return false;
  }

  uint8_t num_received_packets = frame.received_packet_times.size();
  if (!writer->WriteBytes(&num_received_packets, 1)) {
    return false;
  }
  if (num_received_packets == 0) {
    return true;
  }
  auto it = frame.received_packet_times.begin();
  PacketNumber packet_number = it->first;
  PacketNumber delta_from_largest_observed =
      LargestAcked(frame) - packet_number;
  DCHECK_GE(std::numeric_limits<uint8_t>::max(), delta_from_largest_observed);
  if (delta_from_largest_observed > std::numeric_limits<uint8_t>::max()) {
    return false;
  }
  if (!writer->WriteUInt8(delta_from_largest_observed)) {
    return false;
  }
  // Use the lowest 4 bytes of the time delta from the creation_time_.
  const uint64_t time_epoch_delta_us = UINT64_C(1) << 32;
  uint32_t time_delta_us =
      static_cast<uint32_t>((it->second - creation_time_).ToMicroseconds() &
                            (time_epoch_delta_us - 1));
  if (!writer->WriteUInt32(time_delta_us)) {
    return false;
  }
  ProtoTime prev_time = it->second;

  for (++it; it != frame.received_packet_times.end(); ++it) {
    packet_number = it->first;
    delta_from_largest_observed = LargestAcked(frame) - packet_number;

    if (delta_from_largest_observed > std::numeric_limits<uint8_t>::max()) {
      return false;
    }

    if (!writer->WriteUInt8(delta_from_largest_observed)) {
      return false;
    }

    uint64_t frame_time_delta_us = (it->second - prev_time).ToMicroseconds();
    prev_time = it->second;
    if (!writer->WriteUFloat16(frame_time_delta_us)) {
      return false;
    }
  }
  return true;
}
bool ProtoFramer::RaiseError(ProtoErrorCode error){
  DLOG(INFO) << "Error: " << ProtoErrorCodeToString(error)
                  << " detail: " << detailed_error_;
  if(visitor_){
        visitor_->OnError(this);
  }
  return false;
}
}//namespace dqc;
