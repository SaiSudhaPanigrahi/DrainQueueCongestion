#ifndef PROTO_FRAMER_H_
#define PROTO_FRAMER_H_
#include "proto_types.h"
#include "byte_codec.h"
#include "proto_time.h"
#include "proto_error_codes.h"
#include "proto_packets.h"
namespace dqc{
struct AckFrame;
class ProtoFramer;
class ProtoFrameVisitor{
public:
  virtual ~ProtoFrameVisitor(){}
  virtual bool OnStreamFrame(PacketStream &frame)=0;
  virtual void OnError(ProtoFramer* framer) = 0;
  // Called when largest acked of an AckFrame has been parsed.
  virtual bool OnAckFrameStart(PacketNumber largest_acked,
                               TimeDelta ack_delay_time) = 0;

  // Called when ack range [start, end) of an AckFrame has been parsed.
  virtual bool OnAckRange(PacketNumber start, PacketNumber end) = 0;

  // Called when a timestamp in the AckFrame has been parsed.
  virtual bool OnAckTimestamp(PacketNumber packet_number,
                              ProtoTime timestamp) = 0;

  // Called after the last ack range in an AckFrame has been parsed.
  // |start| is the starting value of the last ack range.
  virtual bool OnAckFrameEnd(PacketNumber start) = 0;
};
class ProtoFramer{
public:
  bool AppendAckFrameAndTypeByte(const AckFrame& frame,basic::DataWriter *writer);
  bool ProcessFrameData(basic::DataReader* reader, const ProtoPacketHeader& header);
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
  // Appends a single ACK block to |writer| and returns true if the block was
  // successfully appended.
  static bool AppendAckBlock(uint8_t gap,
                             ProtoPacketNumberLength length_length,
                             PacketNumber length,
                             basic::DataWriter* writer);
  bool AppendTimestampsToAckFrame (const AckFrame& frame,basic::DataWriter* writer);
  // Allows enabling or disabling of timestamp processing and serialization.
  void set_process_timestamps(bool process_timestamps) {
    process_timestamps_ = process_timestamps;
  }
  void set_visitor(ProtoFrameVisitor *visitor){
    visitor_=visitor;
  }
private:
    bool ProcessStreamFrame(basic::DataReader* reader,
                            uint8_t frame_type,
                            PacketStream* frame);
    bool ProcessAckFrame(basic::DataReader* reader, uint8_t frame_type);
    void set_detailed_error(const char* error) { detailed_error_ = error; }
    bool RaiseError(ProtoErrorCode error);
    bool process_timestamps_{false};
    ProtoTime creation_time_{ProtoTime::Zero()};
    ProtoFrameVisitor *visitor_{nullptr};
    const char *detailed_error_{nullptr};
};
size_t GetMinAckFrameSize(PacketNumber largest_observed_length);
size_t GetAckFrameTimeStampSize(const AckFrame& ack);
}//namespace dqc;
#endif
