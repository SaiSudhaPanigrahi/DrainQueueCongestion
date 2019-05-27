#include "unacked_packet_map.h"
#include "logging.h"
namespace dqc{
void UnackedPacketMap::AddSentPacket(SerializedPacket *packet,PacketNumber old,ProtoTime send_ts,bool set_flight)
{
    if(none_sent_){
        least_unacked_=packet->number;
        none_sent_=false;
    }
    // packet sent in none continue mode
    while(least_unacked_+unacked_packets_.size()<packet->number){
        DLOG(INFO)<<"seen unsent ";
        unacked_packets_.push_back(TransmissionInfo());
    }
    PacketLength bytes_sent=packet->len;
    TransmissionInfo info(send_ts,bytes_sent);
    if(set_flight){
        bytes_in_flight_+=bytes_sent;
        info.inflight=true;
    }
    unacked_packets_.push_back(info);
    unacked_packets_.back().retransble_frames.swap(packet->retransble_frames);
}
TransmissionInfo *UnackedPacketMap::GetTransmissionInfo(PacketNumber seq){
    TransmissionInfo *info=nullptr;
    if(none_sent_||seq<least_unacked_||(least_unacked_+unacked_packets_.size())<=seq){
        DLOG(INFO)<<"null info unack "<<least_unacked_<<" "<<seq;
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
void UnackedPacketMap::InvokeLossDetection(AckedPacketVector &packets_acked,LostPacketVector &packets_lost){
    if(packets_acked.empty()){
        return;
    }
    generate_stop_waiting_=false;
    auto acked_it=packets_acked.begin();
    PacketNumber first_seq=acked_it->packet_number;
    DCHECK(first_seq>=least_unacked_);
    TransmissionInfo *info;
    PacketNumber i=least_unacked_;
    for(;i<first_seq;i++){
        info=GetTransmissionInfo(i);
        if(info&&info->inflight){
            packets_lost.emplace_back(i,info->bytes_sent);
        }else{
            generate_stop_waiting_=true;
        }
    }
    for(;acked_it!=packets_acked.end();acked_it++){
        auto next=acked_it+1;
        if(next!=packets_acked.end()){
            PacketNumber left=acked_it->packet_number;
            PacketNumber right=next->packet_number;
            //DLOG(WARNING)<<left<<" "<<right;
            DCHECK(left<right);
            for(i=left+1;i<right;i++){
                info=GetTransmissionInfo(i);
                if(info&&info->inflight){
                    packets_lost.emplace_back(i,info->bytes_sent);
                }else{
                    generate_stop_waiting_=true;
                }
            }
        }
    }
}
void UnackedPacketMap::RemoveFromInflight(PacketNumber seq){
    TransmissionInfo *info=GetTransmissionInfo(seq);
    if(info&&info->inflight){
        ByteCount bytes_sent=info->bytes_sent;
        bytes_sent=std::min(bytes_sent,bytes_in_flight_);
        bytes_in_flight_-=bytes_sent;
        info->inflight=false;
    }
}
void UnackedPacketMap::RemoveLossFromInflight(PacketNumber seq){
    RemoveFromInflight(seq);
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
