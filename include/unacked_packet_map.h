#pragma once
#include "proto_packets.h"
#include "proto_time.h"
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
    ProtoTime GetLastPacketSentTime() const;
    void InvokeLossDetection(AckedPacketVector &packets_acked,LostPacketVector &packets_lost);
    void RemoveFromInflight(PacketNumber seq);
    void RemoveLossFromInflight(PacketNumber seq);
    void RemoveObsolete();
    typedef std::deque<TransmissionInfo> DequeUnackedPacketMap;
    typedef DequeUnackedPacketMap::const_iterator const_iterator;
    typedef DequeUnackedPacketMap::iterator iterator;

    const_iterator begin() const { return unacked_packets_.begin(); }
    const_iterator end() const { return unacked_packets_.end(); }
    iterator begin() { return unacked_packets_.begin(); }
    iterator end() { return unacked_packets_.end(); }
private:
    DequeUnackedPacketMap unacked_packets_;
    PacketNumber least_unacked_;
    ByteCount bytes_in_flight_{0};
    bool generate_stop_waiting_{false};
    PacketNumber  largest_newly_acked_;
};
}//namespace dqc;
