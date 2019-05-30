#pragma once
#include "proto_packets.h"
#include "unacked_packet_map.h"
#include "linked_hash_map.h"
#include "ack_frame.h"
#include "proto_pacing_sender.h"
#include "rtt_stats.h"
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
    //TODO handle rto
    void OnRetransmissionTimeOut();
    // Retransmits two packets for an RTO and removes any non-retransmittable
    // packets from flight.
    void RetransmitRtoPackets();
    void OnAckStart(PacketNumber largest_acked,TimeDelta ack_delay_time,ProtoTime ack_receive_time);
    void OnAckRange(PacketNumber start,PacketNumber end);
    AckResult OnAckEnd(ProtoTime ack_receive_time);
    // in order to handle ack,stop_waiting frame lost;
    bool should_send_stop_waiting(){
        return unacked_packets_.should_send_stop_waiting();
    }
    void MaybeInvokeCongestionEvent(bool rtt_updated,
                                  ByteCount prior_in_flight,
                                  ProtoTime event_time);
    void Test();
    void Test2();
    const TimeDelta GetRetransmissionDelay() const{
        return GetRetransmissionDelay(consecutive_rto_count_);
    }
private:
      // Update the RTT if the ack is for the largest acked packet number.
  // Returns true if the rtt was updated.
    bool MaybeUpdateRTT(PacketNumber largest_acked,
                      TimeDelta ack_delay_time,
                      ProtoTime ack_receive_time);
    // Called after packets have been marked handled with last received ack frame.
    void PostProcessNewlyAckedPackets(ProtoTime ack_receive_time,
                                      bool rtt_updated,
                                      ByteCount prior_bytes_in_flight);
    void InvokeLossDetection(ProtoTime time);
    void MarkForRetrans(PacketNumber seq);
    void PostToPending(PacketNumber seq,TransmissionInfo &info);
    void ClearAckedAndLossVector();
    const TimeDelta GetRetransmissionDelay(size_t consecutive_rto_count) const ;
    StreamAckedObserver *acked_observer_{nullptr};
    UnackedPacketMap unacked_packets_;
    PendingRetransmissionMap pendings_;
    PacketNumber largest_acked_{0};
    LostPacketVector packets_lost_;
    AckedPacketVector packets_acked_;
    AckFrame last_ack_frame_;
    PacketQueue::const_reverse_iterator ack_packet_itor_;
    bool using_pacing_{true};
    bool rtt_updated_{false};
    RttStats rtt_stats_;
    size_t consecutive_rto_count_{0};
    TimeDelta min_rto_timeout_;
    PacingSender pacing_sender_;
    SendAlgorithmInterface *send_algorithm_{nullptr};
};
}//namespace dqc;
