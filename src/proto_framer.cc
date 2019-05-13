#include <limits>
#include "proto_framer.h"
#include "ack_frame.h"
#include "logging.h"
#include "proto_comm.h"
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

    return false;
}
}//namespace dqc;
