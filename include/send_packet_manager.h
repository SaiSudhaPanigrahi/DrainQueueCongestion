#ifndef SEND_PACKET_MANAGER_H_
#define SEND_PACKET_MANAGER_H_
#include "proto_packets.h"
#include "unacked_packet_map.h"
#include "linked_hash_map.h"
#include "ack_frame.h"
namespace dqc{
class SendPacketManager{
public:
    SendPacketManager(StreamAckedObserver *acked_observer);
    bool OnSentPacket(SerializedPacket *packet,PacketNumber old,
                      ContainsRetransData retrans,uint64_t send_ts);
typedef linked_hash_map<PacketNumber,TransmissionInfo,PacketNumberHash> PendingRetransmissionMap;
    PacketNumber GetLeastUnacked(){ return unacked_packets_.GetLeastUnacked();}
    bool HasPendingForRetrans();
    PendingRetransmission NextPendingRetrans();
    void Retransmitted(PacketNumber number);
    void OnAckStart(PacketNumber largest,uint64_t time);
    void OnAckRange(PacketNumber start,PacketNumber end);
    void OnAckEnd(uint64_t time);
    void Test();
    void Test2();
private:
    StreamAckedObserver *acked_observer_{nullptr};
    void InvokeLossDetection(uint64_t time);
    void MarkForRetrans(PacketNumber seq);
    UnackedPacketMap unacked_packets_;
    PendingRetransmissionMap pendings_;
    PacketNumber largest_acked_{0};
    LostPackerVector packets_lost_;
    AckedPacketVector packets_acked_;
    AckFrame last_ack_frame_;
    PacketQueue::const_reverse_iterator ack_packet_itor_;

};
}//namespace dqc;
#endif
