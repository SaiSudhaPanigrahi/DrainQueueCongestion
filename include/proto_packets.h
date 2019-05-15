#ifndef PROTO_PACKET_H_
#define PROTO_PACKET_H_
#include "proto_comm.h"
#include "proto_time.h"
#include <vector>
namespace dqc{
struct PacketStream{
PacketStream():PacketStream(-1,0,0,false){}
PacketStream(uint32_t id,StreamOffset offset,PacketLength len,bool fin)
:PacketStream(id,offset,nullptr,len,fin){}
PacketStream(uint32_t id1,StreamOffset offset1,char*data1,PacketLength len1,bool fin1)
:stream_id(id1),offset(offset1),len(len1),fin(fin1),data_buffer(data1){}
uint32_t stream_id;
StreamOffset offset;
PacketLength len;
bool fin;
const char *data_buffer{nullptr};
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
:send_time(ProtoTime::Zero()),bytes_sent(0),inflight(false),state(SPS_NERVER_SENT){}
TransmissionInfo(ProtoTime send_time,PacketLength bytes_sent)
:send_time(send_time),bytes_sent(bytes_sent),inflight(false),state(SPS_OUT){}
ProtoTime send_time;
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
