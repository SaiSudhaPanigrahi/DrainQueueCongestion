#include <memory.h>
#include "proto_con.h"
#include "proto_utils.h"
#include "byte_codec.h"
namespace dqc{
ProtoCon::ProtoCon():sent_manager_(this)
{
    frame_encoder_.set_data_producer(this);
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
    packet_writer_->WritePacket(src,writer.length(),peer_);
    sent_manager_.OnSentPacket(&serialized,0,CON_RE_YES,ProtoTime::Zero());
    int available=writer.length();
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
    header.packet_number=AllocSeq();
    basic::DataWriter writer(src,sizeof(src));
    AppendPacketHeader(header,&writer);
    uint8_t type=frame_encoder_.GetStreamFrameTypeByte(info,true);
    writer.WriteUInt8(type);
    frame_encoder_.AppendStreamFrame(info,true,&writer);
    ProtoFrame frame(info);
    SerializedPacket serialized;
    serialized.number=header.packet_number;
    serialized.buf=nullptr;//buf addr is not quite useful;
    //TODO add header info;
    serialized.len=writer.length();
    serialized.retransble_frames.push_back(frame);
    DCHECK(packet_writer_);
    printf("%x\n",type);
    DLOG(INFO)<<"packet_writer_ "<<writer.length();
    packet_writer_->WritePacket(src,writer.length(),peer_);
    sent_manager_.OnSentPacket(&serialized,0,CON_RE_YES,ProtoTime::Zero());
    }
}
class FakeReceiver:public PacketWriterInterface,ProtoFrameVisitor{
public:
    FakeReceiver(){
        frame_decoder_.set_visitor(this);
    }
    virtual bool OnStreamFrame(PacketStream &frame) override{
        //std::string str(frame.data_buffer,frame.len);
        DLOG(INFO)<<"recv "<<frame.offset<<" "<<frame.len;
        return true;
    }
    virtual void OnError(ProtoFramer* framer) override{
    }
    virtual bool OnAckFrameStart(PacketNumber largest_acked,
                                 TimeDelta ack_delay_time) override{
        return true;
                                 }
    virtual bool OnAckRange(PacketNumber start,
                            PacketNumber end) override{
                                return true;
                            }
    virtual bool OnAckTimestamp(PacketNumber packet_number,
                                ProtoTime timestamp) override{
                                    return true;
                                }
    virtual bool OnAckFrameEnd(PacketNumber start) override{
        return true;
    }
    virtual int WritePacket(const char*buf,size_t size,su_addr &dst) override{
        std::unique_ptr<char> data(new char[size]);
        memcpy(data.get(),buf,size);
        basic::DataReader r(data.get(),size);
        ProtoPacketHeader header;
        ProcessPacketHeader(&r,header);
        DLOG(INFO)<<"seq "<<header.packet_number;
        frame_decoder_.ProcessFrameData(&r,header);
        return 0;
    }
private:
    ProtoFramer frame_decoder_;
};
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
    FakeReceiver packet_writer;
    set_packet_writer(&packet_writer);
    while(stream->HasBufferedData()){
        stream->OnCanWrite();
    }

    int total_len=0;
    while(!waiting_info_.empty()){
        int len=0;
        len=Send();
        total_len+=len;
    }
    sent_manager_.OnAckStart(2,ProtoTime::Zero());
    sent_manager_.OnAckRange(2,3);
    sent_manager_.OnAckEnd(ProtoTime::Zero());
    if(sent_manager_.HasPendingForRetrans()){
        PendingRetransmission pend=sent_manager_.NextPendingRetrans();
        for(auto frame_it=pend.retransble_frames.begin();
        frame_it!=pend.retransble_frames.end();frame_it++){
            Retransmit(frame_it->stream_frame.stream_id,frame_it->stream_frame.offset,
                       frame_it->stream_frame.len,frame_it->stream_frame.fin);
        }
    }
    sent_manager_.OnAckStart(3,ProtoTime::Zero());
    sent_manager_.OnAckRange(1,4);
    sent_manager_.OnAckEnd(ProtoTime::Zero());
    DLOG(INFO)<<"next "<<sent_manager_.GetLeastUnacked();
    alloc->Delete(data);
}
}//namespace dqc;
