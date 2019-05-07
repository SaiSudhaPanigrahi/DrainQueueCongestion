#ifndef ACK_FRAME_H_
#define ACK_FRAME_H_
#include <deque>
#include <vector>
#include "proto_types.h"
#include "interval.h"
class PacketQueue{
public:
    PacketQueue(){}
    ~PacketQueue(){
        Clear();
    }
    void Add(PacketNumber p);
    void AddRange(PacketNumber l,PacketNumber h);
    void Clear(){
        packet_deque_.clear();
    }
    bool Contains(PacketNumber p);
    bool Empty(){
        return packet_deque_.empty();
    }
    void RemoveUpTo(PacketNumber p);
    PacketNumber Min();
    PacketNumber Max();
    void Print();
    typedef std::deque<Interval<PacketNumber>>::iterator iterator;
    typedef std::deque<Interval<PacketNumber>>::const_iterator const_iterator;
    typedef std::deque<Interval<PacketNumber>>::const_reverse_iterator const_reverse_iterator;
    iterator begin(){
        return packet_deque_.begin();
    }
    iterator end(){
        return packet_deque_.end();
    }
    const_reverse_iterator rbegin() const{
        return packet_deque_.rbegin();
    }
    const_reverse_iterator rend() const{
        return packet_deque_.rend();
    }
    size_t size(){
        return packet_deque_.size();
    }
    private:
    std::deque<Interval<PacketNumber>> packet_deque_;
};
typedef std::vector<std::pair<PacketNumber,TimeType>> PacketVector;
struct AckFrame{
    PacketQueue packets;
    PacketVector packet_time;
    PacketNumber largest_acked;
    TimeType ack_delay_time;
};
class RecvPktMger{
public:
    void OnRecvPkt(PacketNumber seq,TimeType time);
private:
    bool first_received_{true};
    PacketNumber largest_received_{0};
    AckFrame frame;
};
void ack_frame_test();
#endif

