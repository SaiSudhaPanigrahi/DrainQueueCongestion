#ifndef PROTO_FRAMER_H_
#define PROTO_FRAMER_H_
#include "proto_types.h"
#include "byte_codec.h"
namespace dqc{
struct AckFrame;
class ProtoFramer{
public:
bool AppendAckFrameAndTypeByte(const AckFrame& frame,basic::DataWriter *writer);
  struct AckFrameInfo {
    AckFrameInfo();
    AckFrameInfo(const AckFrameInfo& other)=default;
    ~AckFrameInfo(){}

    // The maximum ack block length.
    PacketNumber max_block_length;
    // Length of first ack block.
    PacketNumber first_block_length;
    // Number of ACK blocks needed for the ACK frame.
    size_t num_ack_blocks;
  };
  static AckFrameInfo GetAckFrameInfo(const AckFrame& frame);
  static bool AppendPacketNumber(ProtoPacketNumberLength packet_number_length,
                                 PacketNumber packet_number,
                                 basic::DataWriter* writer);
  // Allows enabling or disabling of timestamp processing and serialization.
  void set_process_timestamps(bool process_timestamps) {
    process_timestamps_ = process_timestamps;
  }
private:
    bool process_timestamps_{false};
};
size_t GetMinAckFrameSize(PacketNumber largest_observed_length);
}//namespace dqc;
#endif
