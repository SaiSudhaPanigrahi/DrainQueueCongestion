#include "unacked_packet_map.h"
#include "logging.h"
namespace dqc{
void UnackedPacketMap::AddSentPacket(SerializedPacket *packet,PacketNumber old,uint64_t send_ts,bool set_flight)
{
    if(none_sent_){
        least_unacked_=packet->number;
        none_sent_=false;
    }
    // packet sent in none continue mode
    while(least_unacked_+unacked_packets_.size()<packet->number){
        unacked_packets_.push_back(TransmissionInfo());
    }
    PacketLength bytes_sent=packet->len;
    TransmissionInfo info(send_ts,bytes_sent);
    if(set_flight){
        bytes_inflight_+=bytes_sent;
        info.inflight=true;
    }
    unacked_packets_.push_back(info);
    unacked_packets_.back().retransble_frames.swap(packet->retransble_frames);
}
TransmissionInfo *UnackedPacketMap::GetTransmissionInfo(PacketNumber seq){
    TransmissionInfo *info=nullptr;
    if(none_sent_||seq<least_unacked_||(least_unacked_+unacked_packets_.size())<=seq){
        return info;
    }
    info=&unacked_packets_[seq-least_unacked_];
    return info;
}
bool UnackedPacketMap::IsUnacked(PacketNumber seq){
    if(none_sent_){
        return false;
    }
    if(seq<least_unacked_||(least_unacked_+unacked_packets_.size())<=seq){
        return false;
    }
    TransmissionInfo *info=GetTransmissionInfo(seq);
    if(info&&info->state==SPS_OUT){
        return true;
    }
    return false;
}
void UnackedPacketMap::InvokeLossDetection(AckedPacketVector &packets_acked,LostPackerVector &packets_lost){
    if(packets_acked.empty()){
        return;
    }
    auto acked_it=packets_acked.begin();
    PacketNumber first_seq=acked_it->seq;
    DCHECK(first_seq>=least_unacked_);
    TransmissionInfo *info;
    PacketNumber i=least_unacked_;
    for(;i<first_seq;i++){
        info=GetTransmissionInfo(i);
        if(info&&info->inflight){
            packets_lost.push_back(i);
        }
    }
    for(acked_it;acked_it!=packets_acked.end();acked_it++){
        auto next=acked_it+1;
        if(next!=packets_acked.end()){
            PacketNumber left=acked_it->seq;
            PacketNumber right=next->seq;
            DLOG(WARNING)<<left<<" "<<right;
            DCHECK(left<right);
            for(i=left+1;i<right;i++){
                info=GetTransmissionInfo(i);
                if(info&&info->inflight){
                    packets_lost.push_back(i);
                }
            }
        }
    }
}
void UnackedPacketMap::RemoveFromInflight(PacketNumber seq){
    TransmissionInfo *info=GetTransmissionInfo(seq);
    if(info&&info->inflight){
        ByteCount bytes_sent=info->bytes_sent;
        bytes_sent=std::min(bytes_sent,bytes_inflight_);
        bytes_inflight_-=bytes_sent;
        info->inflight=false;
    }
}
void UnackedPacketMap::RemoveObsolete(){
    while(!unacked_packets_.empty()){
        auto it=unacked_packets_.begin();
        if(it->inflight){
            break;
        }
        unacked_packets_.pop_front();
        least_unacked_++;
    }
}
}//namespace dqc;
