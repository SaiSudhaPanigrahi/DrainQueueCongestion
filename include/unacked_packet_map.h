#pragma once
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
      // Returns the sum of bytes from all packets in flight.
    ByteCount bytes_in_flight() const { return bytes_in_flight_; }
    void AddSentPacket(SerializedPacket *packet,PacketNumber old,ProtoTime send_ts,bool set_flight);
    TransmissionInfo *GetTransmissionInfo(PacketNumber seq);
    PacketNumber GetLeastUnacked() const{ return least_unacked_;}
    bool IsUnacked(PacketNumber seq);
    void InvokeLossDetection(AckedPacketVector &packets_acked,LostPacketVector &packets_lost);
    void RemoveFromInflight(PacketNumber seq);
    void RemoveLossFromInflight(PacketNumber seq);
    void RemoveObsolete();
private:
    std::deque<TransmissionInfo> unacked_packets_;
    bool none_sent_{true};
    PacketNumber least_unacked_{0};
    ByteCount bytes_in_flight_{0};
    bool generate_stop_waiting_{false};
};
}//namespace dqc;
