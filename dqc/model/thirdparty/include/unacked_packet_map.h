#pragma once
#include "proto_packets.h"
#include "proto_time.h"
#include <deque>
namespace dqc{
class UnackedPacketMapInfoInterface{
public:
    virtual PacketNumber GetLeastUnacked() const=0;
    virtual ByteCount bytes_in_flight() const=0;
    virtual ~UnackedPacketMapInfoInterface(){}
};
class UnackedPacketMap:public  UnackedPacketMapInfoInterface{
public:
      // Returns the sum of bytes from all packets in flight.
    ~UnackedPacketMap() override{}
    ByteCount bytes_in_flight() const override;
    PacketNumber GetLeastUnacked() const override;
    void AddSentPacket(SerializedPacket *packet,PacketNumber old,ProtoTime send_ts,HasRetransmittableData has_retrans);
    TransmissionInfo *GetTransmissionInfo(PacketNumber seq);
    
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
    PacketNumber  largest_newly_acked_;
};
}//namespace dqc;
