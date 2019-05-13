#ifndef PROTO_PACKET_H_
#define PROTO_PACKET_H_
#include "proto_comm.h"
#include <vector>
namespace dqc{
struct PacketStream{
PacketStream(uint32_t id1,StreamOffset offset1,ByteCount len1)
:id(id1),offset(offset1),len(len1){}
uint32_t id;
StreamOffset offset;
ByteCount len;
};
struct ProtoFrame{
explicit ProtoFrame(PacketStream frame):stream_frame(frame){
}
PacketStream stream_frame;
};
typedef std::vector<ProtoFrame> ProtoFrames;
struct SerializedPacket{
PacketNumber  number;
char *buf;
PacketLength len;
bool has_ack;
bool has_stop_waitting;
ProtoFrames retransble_frames;
};
struct TransmissionInfo{
TransmissionInfo()
:send_time(0),bytes_sent(0),inflight(false),state(SPS_NERVER_SENT){}
TransmissionInfo(uint64_t send_time,PacketLength bytes_sent)
:send_time(send_time),bytes_sent(bytes_sent),inflight(false),state(SPS_OUT){}
uint64_t send_time;
PacketLength bytes_sent;
bool inflight;
SentPacketState state;
ProtoFrames retransble_frames;
};
struct PendingRetransmission{
PacketNumber  number;
ProtoFrames retransble_frames;
};
}//namespace dqc;
#endif
