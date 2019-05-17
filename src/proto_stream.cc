#include "proto_stream.h"
#include "proto_packets.h"
#include "flag_impl.h"
#include "proto_utils.h"
#include "logging.h"
namespace dqc{
ProtoStream::ProtoStream(ProtoConVisitor *visitor,uint32_t id)
:visitor_(visitor)
,sequencer_(this)
,stream_id_(id)
,send_buf_(AbstractAlloc::Instance()){
}
void ProtoStream::WriteDataToBuffer(std::string &piece){
    struct iovec io(MakeIovec(piece));
    uint32_t len=piece.size();
    StreamOffset offset=send_buf_.BufferedOffset();
    send_buf_.SaveMemData(&io,1,offset,len);
    //WriteBufferedData();
}
void ProtoStream::OnAck(StreamOffset offset,ByteCount len){
    std::map<StreamOffset,ByteCount>::iterator found=sent_info_.find(offset);
    if(found!=sent_info_.end()){
        send_buf_.Acked(offset,len);
        sent_info_.erase(found);
    }else{
        DLOG(WARNING)<<offset<<" already acked";
    }
}
uint64_t ProtoStream::BufferedBytes() const{
    return send_buf_.BufferedOffset()-send_buf_.StreamBytesWritten();
}
ByteCount ProtoStream::Inflight(){
    return send_buf_.Inflight();
}
bool ProtoStream::HasBufferedData() const{
    return BufferedBytes()>0;
}
void ProtoStream::OnCanWrite(){
    WriteBufferedData();
}
bool ProtoStream::WriteStreamData(StreamOffset offset,ByteCount len,basic::DataWriter *writer){
    bool ret=false;
    ret=send_buf_.WriteStreamData(offset,len,writer);
    if(ret){
        std::map<StreamOffset,ByteCount>::iterator found=sent_info_.find(offset);
        if(found!=sent_info_.end()){
            DLOG(WARNING)<<"retransmission?";
        }else{
            sent_info_.insert(std::make_pair(offset,len));
        }
    }
    return ret;
}
void ProtoStream::OnStreamFrame(PacketStream &frame){
    sequencer_.OnFrameData(frame.offset,frame.data_buffer,frame.len);
}
void ProtoStream::OnDataAvailable(){
    //TODO
}
void ProtoStream::WriteBufferedData(){
    uint64_t payload_len=FLAG_packet_payload;
    uint64_t buffered=BufferedBytes();
    bool fin=false;
    if(buffered>0){
        uint64_t len=std::min(payload_len,buffered);
        StreamOffset offset=send_buf_.StreamBytesWritten();
        OnConsumedData(len);
        visitor_->WritevData(id(),offset,len,fin);
    }
}
void ProtoStream::OnConsumedData(ByteCount len){
    send_buf_.Consumed(len);
}
}//namespace dqc;
