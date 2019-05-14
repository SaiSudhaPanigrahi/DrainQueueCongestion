#include <stdio.h>
#include <string>
#include <iostream>
#include <limits>
#include "ack_frame.h"
#include "interval.h"
#include "proto_types.h"
#include "byte_codec.h"
#include "proto_framer.h"
#include "logging.h"
#include "fun_test.h"
#include "received_packet_manager.h"
using namespace dqc;
using namespace std;
using namespace basic;
void proto_types_test()
{
        uint64_t seq=0;
    //seq=1<<32; will got 0xFFFFFFFF100000000;
    //just be careful for such error;
    seq=UINT32_C(1)<<31;
    uint8_t len=dqc::GetMinPktNumLen(seq);
    DLOG(INFO)<<seq<<" "<<len;
}
void ack_frame_test(){
    PacketQueue packets;
    int i=1;
    for (i=1;i<10;i++){
        if(i==3){continue;}
        if(i==6){continue;}
        packets.Add(i);
    }
    packets.Print();
    packets.RemoveUpTo(6);
    //packets.Add(5);
   // packets.AddRange(1,3);
    packets.Print();
}
void interval_test(){
    Interval<PacketNumber> interval;
    if(interval.Empty()){
        printf("hello\n");
    }
    interval.SetMin(1);
    interval.SetMax(3);
    PacketNumber length=interval.Length();
    DLOG(INFO)<<length;
}
void byte_order_test(){
    uint64_t a=std::numeric_limits<uint8_t>::max();
    uint16_t b=1234;
    uint32_t c=43217;
    uint64_t d=123456789;
    char buf[1500];
    basic::DataWriter w(buf,1500,basic::NETWORK_ORDER);
    w.WriteBytesToUInt64(2,a);
    w.WriteUInt16(b);
    w.WriteUInt32(c);
    w.WriteUInt64(d);
    char *hello="hello world";
    std::string piece(hello);
    DLOG(INFO)<<"piece "<<piece.length();
    w.WriteUInt16(piece.length());
    w.WriteBytes((void*)(piece.data()),piece.length());
    basic::DataReader r(buf,1500,basic::NETWORK_ORDER);
    uint16_t a1=0;
    r.ReadUInt16(&a1);
    std::cout<<std::to_string(a1)<<std::endl;
    uint64_t b1=0;
    r.ReadBytesToUInt64(2,&b1);
    std::cout<<std::to_string(b1)<<std::endl;
    r.ReadBytesToUInt64(4,&b1);
    std::cout<<std::to_string(b1)<<std::endl;
    r.ReadBytesToUInt64(8,&b1);
    std::cout<<std::to_string(b1)<<std::endl;
    std::string result;
    r.ReadStringPiece16(&result);
    DLOG(INFO)<<"piece "<<result.length()<<" "<<result;
}
void test_ufloat(){
    char buf[1500];
    basic::DataWriter w(buf,1500,basic::NETWORK_ORDER);
    uint64_t origin=0x7FF8000;
    w.WriteUFloat16(origin);
    DLOG(INFO)<<w.length();
    basic::DataReader r(buf,1500,basic::NETWORK_ORDER);
    uint64_t decoded;
    r.ReadUFloat16(&decoded);
    printf("%llx\n",decoded);
}
namespace dqc{
class DebugAck: public ProtoFrameVisitor{
public:
    DebugAck(){}
    ~DebugAck(){}
    //return falase, stop process stream;
    virtual bool OnStreamFrame(PacketStream &frame) override{
        DLOG(INFO)<<"should not be called";
        return true;
    }
    virtual void OnError(ProtoFramer* framer) override{

    }
    virtual bool OnAckFrameStart(PacketNumber largest_acked,
                                 TimeDelta ack_delay_time) override{
        DLOG(INFO)<<largest_acked;
        return true;
    }
    virtual bool OnAckRange(PacketNumber start, PacketNumber end) override{
        DLOG(INFO)<<start<<" "<<end;
        return true;
    }
    virtual bool OnAckTimestamp(PacketNumber packet_number,
                                ProtoTime timestamp) override{
        return true;
     }
    virtual bool OnAckFrameEnd(PacketNumber start) override{
        DLOG(INFO)<<start;
        return true;
    }
};
}

void test_proto_framer(){
    dqc::ProtoFramer framer;
    dqc::DebugAck ack_visitor;
    dqc::ReceivdPacketManager receiver;
    dqc::ProtoTime ts(dqc::ProtoTime::Zero());
    for(int i=1;i<=10;i++){
        if(3==i||7==i){
            continue;
        }
        PacketNumber seq=static_cast<PacketNumber>(i);
        receiver.RecordPacketReceived(seq,ts);
    }
    const AckFrame &ack_frame=receiver.GetUpdateAckFrame();
    char wbuf[1500];
    DataWriter w(wbuf,1500,basic::NETWORK_ORDER);
    framer.AppendAckFrameAndTypeByte(ack_frame,&w);
    basic::DataReader r(wbuf,w.length(),basic::NETWORK_ORDER);
    dqc::ProtoFramer decoder;
    decoder.set_visitor(&ack_visitor);
    ProtoPacketHeader fakeheader;
    decoder.ProcessFrameData(&r,fakeheader);
}
