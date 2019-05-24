#ifndef UNACKED_PACKET_MAP_H_
#define UNACKED_PACKET_MAP_H_
#include "proto_packets.h"
#include <deque>
namespace dqc{
class UnackedPacketMap{
public:
    bool should_send_stop_waiting() {
        bool ret=generate_stop_waiting_;
        generate_stop_waiting_=false;
        return ret;
    }
    void AddSentPacket(SerializedPacket *packet,PacketNumber old,ProtoTime send_ts,bool set_flight);
    TransmissionInfo *GetTransmissionInfo(PacketNumber seq);
    PacketNumber GetLeastUnacked(){ return least_unacked_;}
    bool IsUnacked(PacketNumber seq);
    void InvokeLossDetection(AckedPacketVector &packets_acked,LostPackerVector &packets_lost);
    void RemoveFromInflight(PacketNumber seq);
    void RemoveObsolete();
private:
    std::deque<TransmissionInfo> unacked_packets_;
    bool none_sent_{true};
    PacketNumber least_unacked_{0};
    ByteCount bytes_inflight_{0};
    bool generate_stop_waiting_{false};
};
}//namespace dqc;
#endif
