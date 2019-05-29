#include "send_packet_manager.h"
#include "logging.h"
#include <algorithm>
namespace dqc{
//only retransmitable frame can be marked as inflight;
//hence, only stream has such quality.
SendPacketManager::SendPacketManager(StreamAckedObserver *acked_observer)
:acked_observer_(acked_observer){}
bool SendPacketManager::OnSentPacket(SerializedPacket *packet,PacketNumber old,
                      HasRetransmittableData retrans,ProtoTime send_ts){
    bool set_inflight=(retrans==HAS_RETRANSMITTABLE_DATA);
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
void SendPacketManager::OnAckStart(PacketNumber largest,TimeDelta ack_delay_time,ProtoTime ack_receive_time){
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
void SendPacketManager::OnAckEnd(ProtoTime ack_receive_time){

    std::reverse(packets_acked_.begin(),packets_acked_.end());
    for(auto it=packets_acked_.begin();it!=packets_acked_.end();it++){
        PacketNumber seq=it->packet_number;
        TransmissionInfo *info=unacked_packets_.GetTransmissionInfo(seq);
        if(!info){
            DLOG(WARNING)<<"acked unsent packet "<<seq;
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
    InvokeLossDetection(ack_receive_time);
    LostPacketVector temp1;
    packets_lost_.swap(temp1);
    AckedPacketVector temp2;
    packets_acked_.swap(temp2);
    unacked_packets_.RemoveObsolete();
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
    PacketNumber AllocSeq(){
        return seq_++;
    }
    uint32_t id_{0};
    PacketNumber seq_{1};
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
    OnSentPacket(&stream,PacketNumber(0),HAS_RETRANSMITTABLE_DATA,ProtoTime::Zero());
    OnSentPacket(&ack,PacketNumber(0),HAS_RETRANSMITTABLE_DATA,ProtoTime::Zero());
    }
//lost 2 4, only lost ack frame
//reference from test_proto_framer
    OnAckStart(PacketNumber(10),TimeDelta::Zero(),ProtoTime::Zero());
    OnAckRange(PacketNumber(8),PacketNumber(11));
    OnAckRange(PacketNumber(5),PacketNumber(7));
    OnAckRange(PacketNumber(3),PacketNumber(4));
    OnAckRange(PacketNumber(1),PacketNumber(2));
    OnAckEnd(ProtoTime::Zero());
    /*for(i=1;i<=5;i++){
    SerializedPacket stream=generator.CreateStream();
    OnSentPacket(&stream,0,CON_RE_YES,ProtoTime::Zero());
    }*/
    if(HasPendingForRetrans()){
        DLOG(INFO)<<"has retrans "<<GetLeastUnacked();
    }
    if(should_send_stop_waiting()){
        DLOG(INFO)<<"should generate stop waiting "<<GetLeastUnacked();

    }

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
void SendPacketManager::InvokeLossDetection(ProtoTime time){
    unacked_packets_.InvokeLossDetection(packets_acked_,packets_lost_);
    for(auto it=packets_lost_.begin();
    it!=packets_lost_.end();it++){
        MarkForRetrans(it->packet_number);
    }
    // in repeat acked case;
    PacketNumber least_unacked=unacked_packets_.GetLeastUnacked();
    last_ack_frame_.packets.RemoveUpTo(least_unacked);
}
void SendPacketManager::MarkForRetrans(PacketNumber seq){
    DLOG(WARNING)<<"l "<<seq;
    TransmissionInfo *info=unacked_packets_.GetTransmissionInfo(seq);
    if(info&&info->inflight){
        TransmissionInfo copy;
        copy.bytes_sent=info->bytes_sent;
        copy.inflight=info->inflight;
        copy.send_time=info->send_time;
        copy.retransble_frames.swap(info->retransble_frames);
        unacked_packets_.RemoveLossFromInflight(seq);
        pendings_[seq]=copy;
    }
}
}//namespace dqc;
