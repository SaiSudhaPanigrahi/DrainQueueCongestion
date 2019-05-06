#include <memory.h>
#include "proto_con.h"
#include "proto_utils.h"
#include "byte_codec.h"
ProtoCon::ProtoCon(){

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
void ProtoCon::WritevData(uint32_t id,StreamOffset offset,ByteCount len){

    waiting_info_.emplace_back(id,offset,len);
}
ProtoStream *ProtoCon::GetStream(uint32_t id){
    ProtoStream *stream=nullptr;
    auto it=streams_.find(id);
    if(it!=streams_.end()){
        stream=it->second;
    }else{
        stream=CreateStream();
    }
    return stream;
}
void ProtoCon::Close(uint32_t id){

}
void ProtoCon::Test(){
    uint32_t id=0;
    ProtoStream *stream=GetStream(id);
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
    stream->OnAck(1400,100);
    stream->OnAck(0,1400);
    for (i=0;i<off;i++){
        if(data[i]!=buf[i]){
            break;
        }
    }
    printf("i %d\n",i);
    alloc->Delete(data);
}
ProtoStream *ProtoCon::CreateStream(){
    uint32_t id=stream_id_;
    ProtoStream *stream=new ProtoStream(this,id);
    stream_id_++;
    streams_.insert(std::make_pair(id,stream));
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
    int available=writer.length();
    memcpy(buf,src,available);
    printf("available %d\n",available);
    return available;
}
