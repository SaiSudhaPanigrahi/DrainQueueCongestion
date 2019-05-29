#pragma once
#include "proto_packets.h"
#include "unacked_packet_map.h"
#include "linked_hash_map.h"
#include "ack_frame.h"
namespace dqc{
class SendPacketManager{
public:
    SendPacketManager(StreamAckedObserver *acked_observer);
    bool OnSentPacket(SerializedPacket *packet,PacketNumber old,
                      HasRetransmittableData retrans,ProtoTime send_ts);
typedef linked_hash_map<PacketNumber,TransmissionInfo,QuicPacketNumberHash> PendingRetransmissionMap;
    PacketNumber GetLeastUnacked(){ return unacked_packets_.GetLeastUnacked();}
    bool HasPendingForRetrans();
    PendingRetransmission NextPendingRetrans();
    void Retransmitted(PacketNumber number);
    void OnAckStart(PacketNumber largest,TimeDelta ack_delay_time,ProtoTime ack_receive_time);
    void OnAckRange(PacketNumber start,PacketNumber end);
    void OnAckEnd(ProtoTime ack_receive_time);
    // in order to handle ack,stop_waiting frame lost;
    bool should_send_stop_waiting(){
        return unacked_packets_.should_send_stop_waiting();
    }
    void Test();
    void Test2();
private:
    StreamAckedObserver *acked_observer_{nullptr};
    void InvokeLossDetection(ProtoTime time);
    void MarkForRetrans(PacketNumber seq);
    UnackedPacketMap unacked_packets_;
    PendingRetransmissionMap pendings_;
    PacketNumber largest_acked_{0};
    LostPacketVector packets_lost_;
    AckedPacketVector packets_acked_;
    AckFrame last_ack_frame_;
    PacketQueue::const_reverse_iterator ack_packet_itor_;

};
}//namespace dqc;
