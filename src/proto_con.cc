#include <memory.h>
#include "proto_con.h"
#include "proto_utils.h"
#include "byte_codec.h"
namespace dqc{
ProtoCon::ProtoCon()
:time_of_last_received_packet_(ProtoTime::Zero())
,sent_manager_(this)
{
    frame_encoder_.set_data_producer(this);
    //to decode ack frame;
    frame_decoder_.set_visitor(this);
}
ProtoCon::~ProtoCon(){
    ProtoStream *stream=nullptr;
    while(!streams_.empty()){
        auto it=streams_.begin();
        stream=it->second;
        streams_.erase(it);
        delete stream;
    }
}
void ProtoCon::ProcessUdpPacket(SocketAddress &self,SocketAddress &peer,
                          const ProtoReceivedPacket& packet){
    if(!first_packet_from_peer_){
        if(peer!=peer_){
            DLOG(INFO)<<"wrong peer";
            return ;
        }
    }
    if(first_packet_from_peer_){
        peer_=peer;
        first_packet_from_peer_=false;
    }
    time_of_last_received_packet_=packet.receipe_time();
    basic::DataReader r(packet.packet(),packet.length());
    ProtoPacketHeader header;
    ProcessPacketHeader(&r,header);
    frame_decoder_.ProcessFrameData(&r,header);
}
void ProtoCon::WritevData(uint32_t id,StreamOffset offset,ByteCount len,bool fin){

    waiting_info_.emplace_back(id,offset,len,fin);
}
void ProtoCon::OnAckStream(uint32_t id,StreamOffset off,ByteCount len){
    ProtoStream *stream=GetStream(id);
    if(stream){
        DLOG(INFO)<<id<<" "<<off<<" "<<len;
        stream->OnAck(off,len);
    }
}
ProtoStream *ProtoCon::GetOrCreateStream(uint32_t id){
    ProtoStream *stream=GetStream(id);
    if(!stream){
        stream=CreateStream();
    }
    return stream;
}
void ProtoCon::Close(uint32_t id){

}
bool ProtoCon::OnStreamFrame(PacketStream &frame){
    DLOG(INFO)<<"should not be called";
    return true;
}
void ProtoCon::OnError(ProtoFramer* framer){
    DLOG(INFO)<<"frame error";
}
bool ProtoCon::OnAckFrameStart(PacketNumber largest_acked,
                                 TimeDelta ack_delay_time){
  DLOG(INFO)<<largest_acked<<" "<<std::to_string(ack_delay_time.ToMilliseconds());
  sent_manager_.OnAckStart(largest_acked,ack_delay_time,time_of_last_received_packet_);
  return true;
}
bool ProtoCon::OnAckRange(PacketNumber start, PacketNumber end){
    DLOG(INFO)<<start<<" "<<end;
    sent_manager_.OnAckRange(start,end);
    return true;
}
bool ProtoCon::OnAckTimestamp(PacketNumber packet_number,
                                ProtoTime timestamp){
    return true;
}
bool ProtoCon::OnAckFrameEnd(PacketNumber start){
    DLOG(INFO)<<start;
    sent_manager_.OnAckEnd(time_of_last_received_packet_);
    return true;
}
bool ProtoCon::WriteStreamData(uint32_t id,
                               StreamOffset offset,
                               ByteCount len,
                               basic::DataWriter *writer){
    ProtoStream *stream=GetStream(id);
    if(!stream){
        return false;
    }
    return stream->WriteStreamData(offset,len,writer);
}
ProtoStream *ProtoCon::CreateStream(){
    uint32_t id=stream_id_;
    ProtoStream *stream=new ProtoStream(this,id);
    stream_id_++;
    streams_.insert(std::make_pair(id,stream));
    return stream;
}
ProtoStream *ProtoCon::GetStream(uint32_t id){
    ProtoStream *stream=nullptr;
    std::map<uint32_t,ProtoStream*>::iterator found=streams_.find(id);
    if(found!=streams_.end()){
        stream=found->second;
    }
    return stream;
}
int ProtoCon::Send(){
    if(waiting_info_.empty()){
        return 0;
    }
    struct PacketStream info=waiting_info_.front();
    waiting_info_.pop_front();
    char src[1500];
    memset(src,0,sizeof(src));
    ProtoPacketHeader header;
    header.packet_number=AllocSeq();
    basic::DataWriter writer(src,sizeof(src));
    AppendPacketHeader(header,&writer);
    ProtoFrame frame(info);
    uint8_t type=frame_encoder_.GetStreamFrameTypeByte(info,true);
    writer.WriteUInt8(type);
    frame_encoder_.AppendStreamFrame(info,true,&writer);
    SerializedPacket serialized;
    serialized.number=header.packet_number;
    serialized.buf=nullptr;//buf addr is not quite useful;
    //TODO add header info;
    serialized.len=writer.length();
    serialized.retransble_frames.push_back(frame);
    DCHECK(packet_writer_);
    sent_manager_.OnSentPacket(&serialized,PacketNumber(0),HAS_RETRANSMITTABLE_DATA,ProtoTime::Zero());
    int available=writer.length();
    packet_writer_->SendTo(src,writer.length(),peer_);
    return available;
}
void ProtoCon::Retransmit(uint32_t id,StreamOffset off,ByteCount len,bool fin){
    ProtoStream *stream=GetStream(id);
    if(stream){
    DLOG(INFO)<<"retrans "<<off<<" "<<len;
    struct PacketStream info(id,off,len,fin);
    char src[1500];
    memset(src,0,sizeof(src));
    ProtoPacketHeader header;
    PacketNumber seq=AllocSeq();
    header.packet_number=seq;
    basic::DataWriter writer(src,sizeof(src));
    AppendPacketHeader(header,&writer);
    PacketNumber unacked=sent_manager_.GetLeastUnacked();
    DLOG(INFO)<<"send stop waiting "<<seq<<" "<<unacked;
    writer.WriteUInt8(PROTO_FRAME_STOP_WAITING);
    frame_encoder_.AppendStopWaitingFrame(header,unacked,&writer);
    uint8_t type=frame_encoder_.GetStreamFrameTypeByte(info,true);
    writer.WriteUInt8(type);
    frame_encoder_.AppendStreamFrame(info,true,&writer);
    ProtoFrame frame(info);
    SerializedPacket serialized;
    serialized.number=header.packet_number;
    serialized.buf=nullptr;//buf addr is not quite useful;
    serialized.len=writer.length();
    serialized.retransble_frames.push_back(frame);
//    printf("%x\n",type);
    sent_manager_.OnSentPacket(&serialized,QuicPacketNumber(0),HAS_RETRANSMITTABLE_DATA,ProtoTime::Zero());
    packet_writer_->SendTo(src,writer.length(),peer_);
    }
}
void ProtoCon::Process(uint32_t stream_id){
    ProtoStream *stream=GetOrCreateStream(stream_id);
    if(!packet_writer_){
        DLOG(INFO)<<"set writer first";
        return;
    }
    //only send one packet out
    bool packet_send=false;
    if(sent_manager_.HasPendingForRetrans()){
        PendingRetransmission pend=sent_manager_.NextPendingRetrans();
        for(auto frame_it=pend.retransble_frames.begin();
        frame_it!=pend.retransble_frames.end();frame_it++){
            Retransmit(frame_it->stream_frame.stream_id,frame_it->stream_frame.offset,
                       frame_it->stream_frame.len,frame_it->stream_frame.fin);
        }
        packet_send=true;
    }
    if(!packet_send){
        if(stream->HasBufferedData()){
            stream->OnCanWrite();
        }
        if(!waiting_info_.empty()){
            Send();
        }
    }
}
}//namespace dqc;
