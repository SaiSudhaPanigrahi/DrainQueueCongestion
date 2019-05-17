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
#include "rtt_stats.h"
#include "packet_writer.h"
#include "proto_con.h"
#include "proto_stream_data_producer.h"
#include "proto_packets.h"
#include "proto_stream_sequencer.h"
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
class FakeDataProducer:public ProtoStreamDataProducer{
public:
    void SetData(const char*data,size_t len){
        data_=data;
        len_=len;
    }
    bool WriteStreamData(uint32_t id,
                                 StreamOffset offset,
                                 ByteCount len,
                                 basic::DataWriter *writer) override{
    DLOG(INFO)<<offset<<" "<<len;
    return writer->WriteBytes(data_,len);
}
private:
    const char *data_{nullptr};
    uint32_t len_{0};
};
class DebugStreamFrame:public ProtoFrameVisitor{
public:
    virtual bool OnStreamFrame(PacketStream &frame) override{
        std::string str(frame.data_buffer,frame.len);
        DLOG(INFO)<<"recv "<<str.c_str();
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
};
class FakeReceiver:public PacketWriterInterface{
public:
    //first to implement stream encoder and decoder;
    int WritePacket(const char*buf,size_t size,su_addr &dst) override{

        return 0;
    }
private:
    ReceivdPacketManager recv_manager_;
};
}

void test_proto_framer(){
    dqc::ProtoFramer framer;
    dqc::DebugAck ack_visitor;
    dqc::ReceivdPacketManager receiver;
    dqc::ProtoTime ts(dqc::ProtoTime::Zero());
    for(int i=1;i<=2;i++){
        if(1==i||7==i){
            continue;
        }
        PacketNumber seq=static_cast<PacketNumber>(i);
        receiver.RecordPacketReceived(seq,ts);
    }
    //receiver.RecordPacketReceived(3,ts);
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
void test_time(){
    RttStats rtt_stats_;
    rtt_stats_.UpdateRtt(TimeDelta::FromMilliseconds(100),TimeDelta::Zero(),ProtoTime::Zero());
    rtt_stats_.UpdateRtt(TimeDelta::FromMilliseconds(120),TimeDelta::Zero(),ProtoTime::Zero());
    TimeDelta srtt=rtt_stats_.smoothed_rtt();
    DLOG(INFO)<<srtt.ToMilliseconds();
}
void test_stream_endecode(){
    std::string data("hello word");
    FakeDataProducer producer;
    producer.SetData(data.data(),data.length());
    std::string data1("hello word1");
    PacketStream stream1(0,data.length(),data1.length(),false);
    ProtoFramer encoder;
    PacketStream stream(0,0,data.length(),false);
    char wbuf[1500];
    basic::DataWriter w(wbuf,1500);
    encoder.set_data_producer(&producer);
    //add type stream frame
    // true ,no data length
    uint8_t type=encoder.GetStreamFrameTypeByte(stream,false);
    w.WriteUInt8(type);//offset 0 will ignored
    uint32_t print_type=type;
    printf("type %x\n",print_type);
    encoder.AppendStreamFrame(stream,false,&w);
    type=encoder.GetStreamFrameTypeByte(stream1,false);
    w.WriteUInt8(type);
    producer.SetData(data1.data(),data1.length());
    encoder.AppendStreamFrame(stream1,false,&w);
    basic::DataReader r(wbuf,w.length());
    ProtoFramer decoder;
    DebugStreamFrame frame_visitor;
    decoder.set_visitor(&frame_visitor);
    ProtoPacketHeader header;
    decoder.ProcessFrameData(&r,header);
}
void test_readbuf(){
    /*IntervalSet<int> readble;
    readble.Add(100,200);
    IntervalSet<int> read2;
    read2.Swap(&readble);
    auto it=read2.UpperBound(100);
    if(it==read2.end()){
        DLOG(INFO)<<read2.Size();
    }else{
        if(it==read2.begin()){
            //should not be happen,
            //first add then upper_bound
            DLOG(WARNING)<<"all small";
        }else{
            it--;
            DLOG(INFO)<<it->min()<<" "<<it->max();
        }
    }*/
    char data[1500];
    ProtoStreamSequencer sequencer(nullptr);
    sequencer.OnFrameData(1500,data,1500);
    DLOG(INFO)<<sequencer.ReadbleSize();
    sequencer.OnFrameData(0,data,1500);
    DLOG(INFO)<<sequencer.ReadbleSize();
}
void test_test(){
    //ack_frame_test();
    //proto_types_test();
    //interval_test();
    //byte_order_test();
   //test_proto_framer();
   //test_stream_endecode();
   //test_ufloat();
    //test_time();
    test_readbuf();
}
