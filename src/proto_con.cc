#include <memory.h>
#include "proto_con.h"
#include "proto_utils.h"
#include "byte_codec.h"
namespace dqc{
ProtoCon::ProtoCon():sent_manager_(this){

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

}
bool ProtoCon::OnAckFrameStart(PacketNumber largest_acked,
                                 TimeDelta ack_delay_time){
  DLOG(INFO)<<largest_acked;
  return true;
}
bool ProtoCon::OnAckRange(PacketNumber start, PacketNumber end){
    DLOG(INFO)<<start<<" "<<end;
    return true;
}
bool ProtoCon::OnAckTimestamp(PacketNumber packet_number,
                                ProtoTime timestamp){
    return true;
}
bool ProtoCon::OnAckFrameEnd(PacketNumber start){
    DLOG(INFO)<<start;
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
int ProtoCon::Send(ProtoStream *stream,char *buf){
    if(waiting_info_.empty()){
        return 0;
    }
    struct PacketStream info=waiting_info_.front();
    waiting_info_.pop_front();
    char src[1500];
    memset(src,0,sizeof(src));
    basic::DataWriter writer(src,sizeof(src));
    stream->WriteStreamData(info.offset,info.len,&writer);
    ProtoFrame frame(info);
    SerializedPacket serialized;
    serialized.number=AllocSeq();
    serialized.buf=nullptr;//buf addr is not quite useful;
    //TODO add header info;
    serialized.len=writer.length();
    serialized.retransble_frames.push_back(frame);
    sent_manager_.OnSentPacket(&serialized,0,CON_RE_YES,ProtoTime::Zero());
    int available=writer.length();
    memcpy(buf,src,available);
    return available;
}
void ProtoCon::Retransmit(uint32_t id,StreamOffset off,ByteCount len,bool fin){
    ProtoStream *stream=GetStream(id);
    if(stream){
        DLOG(INFO)<<"retrans "<<off;
    char src[1500];
    memset(src,0,sizeof(src));
    basic::DataWriter writer(src,sizeof(src));
    stream->WriteStreamData(off,len,&writer);
    struct PacketStream info(id,off,len,fin);
    ProtoFrame frame(info);
    SerializedPacket serialized;
    serialized.number=AllocSeq();
    serialized.buf=nullptr;//buf addr is not quite useful;
    //TODO add header info;
    serialized.len=writer.length();
    serialized.retransble_frames.push_back(frame);
    sent_manager_.OnSentPacket(&serialized,0,CON_RE_YES,ProtoTime::Zero());
    }
}
void ProtoCon::Test(){
    uint32_t id=0;
    ProtoStream *stream=GetOrCreateStream(id);
    AbstractAlloc *alloc=AbstractAlloc::Instance();
    char *data=alloc->New(1500,MY_FROM_HERE);
    int i=0;
    for (i=0;i<1500;i++){
        data[i]=RandomLetter::Instance()->GetLetter();
    }
    std::string piece(data,1500);
    stream->WriteDataToBuffer(piece);

    while(stream->HasBufferedData()){
        stream->OnCanWrite();
    }
    char buf[1500];
    char *rbuf=buf;
    int off=0;
    while(!waiting_info_.empty()){
        int len=0;
        len=Send(stream,rbuf+off);
        off+=len;
    }
    sent_manager_.OnAckStart(2,0);
    sent_manager_.OnAckRange(2,3);
    sent_manager_.OnAckEnd(0);
    if(sent_manager_.HasPendingForRetrans()){
        PendingRetransmission pend=sent_manager_.NextPendingRetrans();
        for(auto frame_it=pend.retransble_frames.begin();
        frame_it!=pend.retransble_frames.end();frame_it++){
            Retransmit(frame_it->stream_frame.stream_id,frame_it->stream_frame.offset,
                       frame_it->stream_frame.len,frame_it->stream_frame.fin);
        }
    }
    sent_manager_.OnAckStart(3,0);
    sent_manager_.OnAckRange(1,4);
    sent_manager_.OnAckEnd(0);
    DLOG(INFO)<<"next "<<sent_manager_.GetLeastUnacked();
    alloc->Delete(data);
}
}//namespace dqc;
