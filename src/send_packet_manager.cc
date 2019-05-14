#include "send_packet_manager.h"
#include "logging.h"
#include <algorithm>
namespace dqc{
SendPacketManager::SendPacketManager(StreamAckedObserver *acked_observer)
:acked_observer_(acked_observer){}
bool SendPacketManager::OnSentPacket(SerializedPacket *packet,PacketNumber old,
                      ContainsRetransData retrans,uint64_t send_ts){
    bool set_inflight=(retrans==CON_RE_YES);
    unacked_packets_.AddSentPacket(packet,old,send_ts,set_inflight);
    return true;
}
bool SendPacketManager::HasPendingForRetrans(){
    return !pendings_.empty();
}
PendingRetransmission SendPacketManager::NextPendingRetrans(){
    PendingRetransmission pending;
    PendingRetransmissionMap::iterator found=pendings_.begin();
    pending.number=found->first;
    found->second.retransble_frames.swap(pending.retransble_frames);
    pendings_.erase(found);
    return pending;

}
void SendPacketManager::Retransmitted(PacketNumber number){
    PendingRetransmissionMap::iterator found=pendings_.find(number);
    if(found!=pendings_.end()){
        pendings_.erase(found);
    }else{
        DLOG(WARNING)<<number<<" not exist";
    }
}
void SendPacketManager::OnAckStart(PacketNumber largest,uint64_t time){
    /*if(unacked_packets_.IsUnacked(largest)){

    }*/
    ack_packet_itor_=last_ack_frame_.packets.rbegin();
}
void SendPacketManager::OnAckRange(PacketNumber start,PacketNumber end){
    PacketNumber least_unakced=unacked_packets_.GetLeastUnacked();
    if(end<=least_unakced){
        return ;
    }
    start=std::max(start,least_unakced);
    PacketNumber acked=end-1;
    PacketNumber newly_acked_start=start;
    if(ack_packet_itor_!=last_ack_frame_.packets.rend()){
        newly_acked_start=std::max(start,ack_packet_itor_->Max());
    }
    for(;acked>=newly_acked_start;acked--){
        packets_acked_.push_back(AckedPacket(acked,0,0));
    }
}
void SendPacketManager::OnAckEnd(uint64_t time){

    std::reverse(packets_acked_.begin(),packets_acked_.end());
    for(auto it=packets_acked_.begin();it!=packets_acked_.end();it++){
        PacketNumber seq=it->seq;
        TransmissionInfo *info=unacked_packets_.GetTransmissionInfo(seq);
        if(!info){
            DLOG(WARNING)<<"acked unsent packet";
        }else{
            if(info->inflight){
                if(acked_observer_){
                    for(auto frame_it=info->retransble_frames.begin();
                    frame_it!=info->retransble_frames.end();frame_it++){
                        acked_observer_->OnAckStream(frame_it->stream_frame.stream_id,
                                                     frame_it->stream_frame.offset,
                                                     frame_it->stream_frame.len);
                    }
                }
               unacked_packets_.RemoveFromInflight(seq);
            }
            last_ack_frame_.packets.Add(seq);
        }
    }
    InvokeLossDetection(time);
}
class PacketGenerator{
public:
    PacketGenerator(){}
    SerializedPacket CreateStream(){
        PacketStream stream(id_,offset_,stream_len_,false);
        offset_+=stream_len_;
        ProtoFrame frame(stream);
        SerializedPacket packet;
        packet.number=AllocSeq();
        packet.len=stream_len_;
        packet.retransble_frames.push_back(frame);
        return packet;
    }
    SerializedPacket CreateAck(){
        SerializedPacket packet;
        packet.number=AllocSeq();
        packet.len=ack_len_;
        return packet;
    }
private:
    uint32_t AllocSeq(){
        return seq_++;
    }
    uint32_t id_{0};
    uint32_t seq_{1};
    uint32_t ack_len_{20};
    uint32_t stream_len_{1000};
    uint64_t offset_{0};
};
void SendPacketManager::Test(){
    PacketGenerator generator;
    int i=0;
    for(i=1;i<=10;i+=2){
    SerializedPacket stream=generator.CreateStream();
    SerializedPacket ack=generator.CreateAck();
    OnSentPacket(&stream,0,CON_RE_YES,0);
    OnSentPacket(&ack,0,CON_RE_NO,0);
    }
//lost 2 3 4 6 7
    OnAckStart(10,0);
    OnAckRange(8,11);
    OnAckRange(5,6);
    OnAckRange(1,2);
    OnAckEnd(0);
    for(i=1;i<=5;i++){
    SerializedPacket stream=generator.CreateStream();
    OnSentPacket(&stream,0,CON_RE_YES,0);
    }
    // l 11
    OnAckStart(13,0);
    OnAckRange(12,14);
    OnAckRange(1,11);
    OnAckEnd(0);
    OnAckStart(15,0);
    OnAckRange(15,16);
    OnAckRange(1,14);
    OnAckEnd(0);
    DLOG(INFO)<<"size "<<last_ack_frame_.packets.size();
}
void SendPacketManager::Test2(){
 if(HasPendingForRetrans()){
        PendingRetransmission pending=
        NextPendingRetrans();
        int len=pending.retransble_frames.size();
        StreamOffset off=0;
        if(len>0){
            off=pending.retransble_frames.front().stream_frame.offset;
        }
        DLOG(INFO)<<len<<" "<<off;
        Retransmitted(pending.number);
}
    PacketNumber least_unacked=unacked_packets_.GetLeastUnacked();
    DLOG(INFO)<<"least unacked "<<least_unacked;
    TransmissionInfo *info=unacked_packets_.GetTransmissionInfo(least_unacked);
    if(!info){
        DLOG(INFO)<<"not sent";
    }
}
void SendPacketManager::InvokeLossDetection(uint64_t time){
    unacked_packets_.InvokeLossDetection(packets_acked_,packets_lost_);
    for(auto it=packets_lost_.begin();
    it!=packets_lost_.end();it++){
        MarkForRetrans(*it);
    }
    LostPackerVector temp1;
    packets_lost_.swap(temp1);
    AckedPacketVector temp2;
    packets_acked_.swap(temp2);
    // in repeat acked case;
    PacketNumber least_unacked=unacked_packets_.GetLeastUnacked();
    last_ack_frame_.packets.RemoveUpTo(least_unacked);
    unacked_packets_.RemoveObsolete();

}
void SendPacketManager::MarkForRetrans(PacketNumber seq){
    #if !defined (NDEBUG)
    DLOG(WARNING)<<"l "<<seq;
    #endif
    TransmissionInfo *info=unacked_packets_.GetTransmissionInfo(seq);
    if(info&&info->inflight){
        TransmissionInfo copy;
        copy.bytes_sent=info->bytes_sent;
        copy.inflight=info->inflight;
        copy.send_time=info->send_time;
        copy.retransble_frames.swap(info->retransble_frames);
        unacked_packets_.RemoveFromInflight(seq);
        pendings_[seq]=copy;
    }
}
}//namespace dqc;
