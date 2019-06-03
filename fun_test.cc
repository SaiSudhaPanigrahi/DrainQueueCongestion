#include <stdio.h>
#include <string>
#include <iostream>
#include <limits>
#include <memory>
#include "ack_frame.h"
#include "interval.h"
#include "proto_types.h"
#include "byte_codec.h"
#include "proto_framer.h"
#include "logging.h"
#include "fun_test.h"
#include "received_packet_manager.h"
#include "rtt_stats.h"
#include "proto_socket.h"
#include "proto_stream.h"
#include "proto_con.h"
#include "proto_stream_data_producer.h"
#include "proto_packets.h"
#include "proto_stream_sequencer.h"
#include "socket_address.h"
#include "cf_platform.h"
#include "send_receive.h"
#include "proto_constants.h"
#include "packet_number_indexed_queue.h"
#include "proto_pacing_sender.h"
#include "ns3/log.h"
using namespace dqc;
using namespace std;
using namespace basic;
static const char *local_ip="127.0.0.1";
static const uint16_t send_port=1234;
static const uint16_t recv_port=4321;
void proto_types_test()
{
        PacketNumber seq;
    //seq=1<<32; will got 0xFFFFFFFF100000000;
    //just be careful for such error;
    seq=PacketNumber(UINT32_C(1)<<31);
    uint8_t len=dqc::GetMinPktNumLen(seq);
    DLOG(INFO)<<seq<<" "<<len;
}
void ack_frame_test(){
    PacketQueue packets;
    int i=1;
    for (i=1;i<10;i++){
        PacketNumber seq(i);
        if(i==3){continue;}
        if(i==6){continue;}
        packets.Add(seq);
    }
    packets.Print();

    packets.RemoveUpTo(PacketNumber(6));
    packets.Print();
}
void interval_test(){
    Interval<PacketNumber> interval;
    if(interval.Empty()){
        printf("hello\n");
    }
    interval.SetMin(PacketNumber(1));
    interval.SetMax(PacketNumber(3));
    uint64_t length=interval.Length();
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
    const char *hello="hello world";
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
        DLOG(INFO)<<"frame error";
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
    bool OnStopWaitingFrame(const PacketNumber least_unacked){
        DLOG(INFO)<<"stop_waiting "<<least_unacked;
        return true;
    }
};
class FakeReceiver:public Socket{
public:
    //first to implement stream encoder and decoder;
    int SendTo(const char*buf,size_t size,SocketAddress &dst) override{

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
    for(int i=1;i<=10;i++){
        if(2==i||4==i||7==i){
            continue;
        }
        PacketNumber seq(i);
        receiver.RecordPacketReceived(seq,ts);
    }
    receiver.DontWaitForPacketsBefore(PacketNumber(3));
    const AckFrame &ack_frame=receiver.GetUpdateAckFrame(ts);
    char wbuf[1500];
    DataWriter w(wbuf,1500,basic::NETWORK_ORDER);
    framer.AppendAckFrameAndTypeByte(ack_frame,&w);
    basic::DataReader r(wbuf,w.length(),basic::NETWORK_ORDER);
    dqc::ProtoFramer decoder;
    decoder.set_visitor(&ack_visitor);
    ProtoPacketHeader fakeheader;
    decoder.ProcessFrameData(&r,fakeheader);
}
void test_rtt(){
    RttStats rtt_stats_;
    rtt_stats_.UpdateRtt(TimeDelta::FromMilliseconds(100),TimeDelta::Zero(),ProtoTime::Zero());
    rtt_stats_.UpdateRtt(TimeDelta::FromMilliseconds(120),TimeDelta::Zero(),ProtoTime::Zero());
    TimeDelta srtt=rtt_stats_.smoothed_rtt();
    DLOG(INFO)<<srtt.ToMilliseconds();
}
void test_stream_endecode(){
    std::string data("hello word, why");
    FakeDataProducer producer;
    producer.SetData(data.data(),data.length());
    std::string data1("hello word1");
    PacketStream stream(0,0,data.length(),false);
    PacketStream stream1(0,data.length(),data1.length(),false);
    ProtoFramer encoder;
    char wbuf[1500];
    basic::DataWriter w(wbuf,1500);
    encoder.set_data_producer(&producer);
    ProtoPacketHeader header1;
    header1.packet_number=PacketNumber(10);
    PacketNumber unacked1(4);
    AppendPacketHeader(header1,&w);
    w.WriteUInt8(PROTO_FRAME_STOP_WAITING);
    encoder.AppendStopWaitingFrame(header1,unacked1,&w);
    //add type stream frame
    // true ,no data length
    uint8_t type=encoder.GetStreamFrameTypeByte(stream,false);
    w.WriteUInt8(type);//offset 0 will ignored
    uint32_t print_type=type;
    printf("type %x\n",print_type);
    encoder.AppendStreamFrame(stream,false,&w);

    type=encoder.GetStreamFrameTypeByte(stream1,false);
    w.WriteUInt8(type);
    print_type=type;
    printf("type %x\n",print_type);
    producer.SetData(data1.data(),data1.length());
    encoder.AppendStreamFrame(stream1,false,&w);

    basic::DataReader r(wbuf,w.length());
    ProtoPacketHeader header2;
    ProcessPacketHeader(&r,header2);
    ProtoFramer decoder;
    DebugStreamFrame frame_visitor;
    decoder.set_visitor(&frame_visitor);
    decoder.ProcessFrameData(&r,header2);
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
    int i=0;
    for (i=0;i<1500;i++){
        data[i]=RandomLetter::Instance()->GetLetter();
    }
    ProtoStreamSequencer sequencer(nullptr);
    sequencer.OnFrameData(1500,data,1500);
    DLOG(INFO)<<sequencer.ReadbleBytes();
    sequencer.OnFrameData(0,data,1500);
    DLOG(INFO)<<sequencer.ReadbleBytes();
    size_t iov_len=sequencer.GetIovLength();
    std::unique_ptr<iovec[]> iovs(new iovec[iov_len]);
    int read=0;
    read=sequencer.GetReadableRegions(iovs.get(),iov_len);
    iovec *temp=iovs.get();
    char rbuf[3000];
    size_t off=0;
    for(int i=0;i<read;i++){
        memcpy(rbuf+off,temp[i].iov_base,temp[i].iov_len);
        off+=temp[i].iov_len;
        if(off>=3000){
            break;
        }
    }
    sequencer.MarkConsumed(off);
    for(i=0;i<1500;i++){
        if(data[i]!=rbuf[i]){
            break;
        }
    }
    DLOG(INFO)<<i;
    AbstractAlloc::Instance()->CheckMemLeak();
}
void test_stop_waiting(){
    ProtoPacketHeader header1;
    header1.packet_number=PacketNumber(10);
    PacketNumber unacked1(4);
    char buf[20];
    basic::DataWriter w(buf,20);
    AppendPacketHeader(header1,&w);
    w.WriteUInt8(PROTO_FRAME_STOP_WAITING);
    ProtoFramer framer1;
    framer1.AppendStopWaitingFrame(header1,unacked1,&w);
    basic::DataReader r(buf,w.length());
    ProtoPacketHeader header2;
    ProcessPacketHeader(&r,header2);
    uint8_t temp_type=0;
    r.ReadUInt8(&temp_type);
    int stop_waiting_type=temp_type;
    DLOG(INFO)<<"stop type "<<stop_waiting_type;
    PacketNumber unacked2(0);
    framer1.ProcessStopWaitingFrame(&r,header2,&unacked2);
    DLOG(INFO)<<header2.packet_number<<" "<<unacked2;
}
namespace dqc{
const TimeDelta granularity=TimeDelta::FromMilliseconds(1);
CounterDelegate::CounterDelegate(AlarmTest *test){
        test_=test;
}
CounterDelegate::~CounterDelegate(){
    printf("delete delegate\n");
}
void CounterDelegate::OnAlarm(){
    count_++;
    printf("%d\n",count_);
    test_->OnTrigger();
}
ProtoTime AlarmTest::TimeOut(int64_t gap){
    int64_t now_ms=TimeMillis();
    TimeDelta delta=TimeDelta::FromMilliseconds(now_ms);
    ProtoTime now=ProtoTime::Zero()+delta;
    delta=TimeDelta::FromMilliseconds(gap);
    ProtoTime timeout=now+delta;
    return timeout;
}
void AlarmTest::OnTrigger(){

    ProtoTime timeout=TimeOut(100);
    alarm_->Update(timeout,granularity);
}
}
void test_alarm(){
    MainEngine engine;
    ProcessAlarmFactory factory(&engine);
    AlarmTest alarmtest;
    SystemClock clock;
    std::unique_ptr<CounterDelegate> delegate(new CounterDelegate(&alarmtest));
    alarmtest.alarm_=std::move(factory.CreateAlarm(std::move(delegate)));
    alarmtest.alarm_->Set(alarmtest.TimeOut(10));
    ProtoTime now=clock.Now();
    ProtoTime stop_time=now+TimeDelta::FromMilliseconds(5000);
    while(1){
        now=clock.Now();
        engine.HeartBeat(now);
        if(now>stop_time){
            break;
        }
    }
    //alarmtest.~AlarmTest();
    //engine.~MainEngine();
}
void test_socket_address(){
    std::string str("139.168.1.23");
    SocketAddress add(str,1234);
    std::string addr_str=add.ToString();
    std::cout<<addr_str<<std::endl;
}
void* socket_client(void *arg){
    const char *msg="hello server";
    SocketAddress serv_addr(local_ip,recv_port);
    UdpSocket socket;
    if(socket.Bind(local_ip,recv_port)!=0){
        DLOG(INFO)<<"client bind failed";
        return nullptr;
    }
    socket.SendTo(msg,strlen(msg),serv_addr);
    int64_t now=TimeMillis();
    int64_t stop=now+2000;
    char buf[1000];
    while(true){
        int ret;
        SocketAddress peer;
        ret=socket.RecvFrom(buf,1000,peer);
        if(ret>0){
            DLOG(INFO)<<"from server "<<peer.ToString();
        }
        if(TimeMillis()>stop){
            break;
        }
    }
    return nullptr;
}
void *socket_server(void*arg){
    UdpSocket socket;
    if(socket.Bind(local_ip,recv_port)!=0){
        DLOG(INFO)<<"server bind failed";
        return nullptr;
    }
    int64_t now=TimeMillis();
    int64_t stop=now+4000;
    char buf[1000];
    while(true){
        int ret;
        SocketAddress peer;
        memset(buf,0,1000);
        ret=socket.RecvFrom(buf,1000,peer);
        if(ret>0){
            std::string msg(buf,strlen(buf));
            socket.SendTo(buf,strlen(buf),peer);
            DLOG(INFO)<<"from client "<<peer.ToString()<<" "<<msg;
        }
        if(TimeMillis()>stop){
            break;
        }
    }
    return nullptr;
}
const char *server="server";
const char *client="client";
void thread_test(){
    su_thread th1=su_create_thread(server,socket_server,nullptr);
    TimeSleep(100);
    su_thread th2=su_create_thread(client,socket_client,nullptr);
    TimeSleep(7000);
    su_destroy_thread(th1);
    su_destroy_thread(th2);
}
void stream_test(){
    ProtoStream stream(nullptr,0);
    stream.set_max_send_buf_len(1500*10);

   // DLOG(INFO)<<"size "<<piece.size();
    int i=0;
    for(i=0;i<12;i++){
        char buf[1500];
        std::string piece(buf,1500);
        bool success=stream.WriteDataToBuffer(piece);
        if(!success){
            DLOG(INFO)<<"buf full "<<i<<" "<<stream.Inflight()
            <<" "<<stream.BufferedBytes();
        }else{
            DLOG(INFO)<<"buf "<<i<<" "<<stream.Inflight()
            <<" "<<stream.BufferedBytes();
        }
    }
}
void test_sender_receiver(){
    dqc::SystemClock clock;
    Sender sender(&clock);
    sender.Bind(local_ip,send_port);
    Receiver receiver(&clock);
    receiver.Bind(local_ip,recv_port);
    //to test rto;
    //receiver.set_nerver_feedack(true);
    SocketAddress serv_addr=receiver.get_local_addr();
    sender.set_peer(serv_addr);
    //sender.set_test_rto_flag(true);
    //sender.DataGenerator(100);
    TimeDelta run_time(TimeDelta::FromMilliseconds(10000));
    ProtoTime now=clock.Now();
    ProtoTime stop=now+run_time;
    while(true){
        ProtoTime now=clock.Now();
        sender.Process();
        receiver.Process();
        if(now>stop){
            break;
        }
    }
    AbstractAlloc *alloc=AbstractAlloc::Instance();
    alloc->CheckMemLeak();
}
class ConnectionState{
public:
    ConnectionState(){
        std::cout<<"default builder"<<std::endl;
    }
    ConnectionState(int seq):seq_{seq}{
    }
    int get_seq() const{return seq_;}
private:
    int seq_{0};
};
void packet_indexed_queue_test(){
    PacketNumberIndexedQueue<ConnectionState> seqs_;
    seqs_.Emplace(QuicPacketNumber(1),1);
    seqs_.Emplace(QuicPacketNumber(3),3);
    seqs_.Emplace(QuicPacketNumber(5),5);
    DLOG(INFO)<<seqs_.number_of_present_entries();
    PacketNumber first=seqs_.first_packet();
    PacketNumber last=seqs_.last_packet();
    PacketNumber seq(4);
    seq=std::min(seq,last);
    PacketNumber it=first;
    for(;it<=seq;it++){
        seqs_.Remove(it,[](const ConnectionState &entry){
            std::cout<<entry.get_seq()<<std::endl;
                     });
    }
    DLOG(INFO)<<seqs_.number_of_present_entries();
}
void pacing_test(){
    PacingSender pacer;
    SystemClock clock;
    RttStats rtt_stats;
    UnackedPacketMap unack;
    Random random;
    random.seed(0x12234);
    SendAlgorithmInterface *algo=SendAlgorithmInterface::Create(&clock,&rtt_stats,
                                                                &unack,kBBR,&random,
                                                                kMinInitialCongestionWindow);
    pacer.set_sender(algo);
    PacketLength MTU=1400;
    TimeDelta delta(TimeDelta::FromMilliseconds(10));
    ProtoTime now=ProtoTime::Zero()+delta;
    ByteCount inflight=0;
    TimeDelta next=pacer.TimeUntilSend(now,inflight);
    PacketNumber seq(1);
    if(next==TimeDelta::Zero()){
        inflight+=MTU;
        pacer.OnPacketSent(now,inflight,seq,MTU,HAS_RETRANSMITTABLE_DATA);
    }
    now=now+next;
    int i=1;
    for(;i<12;i++){
        next=pacer.TimeUntilSend(now,inflight);
        seq++;
        if(next==TimeDelta::Zero()){
            inflight+=MTU;
            pacer.OnPacketSent(now,inflight,seq,MTU,HAS_RETRANSMITTABLE_DATA);
        }
        if(next!=TimeDelta::Zero()){
            break;
        }
        now=now+next;
    }
    DLOG(INFO)<<next;
    TimeDelta forever=TimeDelta::Infinite();
    DLOG(INFO)<<"forever "<<forever;
    delete algo;
}
#include "ns3/global-stream.h"
#include "proto_bandwidth.h"
#include "proto_constants.h"
NS_LOG_COMPONENT_DEFINE("test");
void test_trivial(){
    std::string filename("error.log");
    std::ios::openmode filemode=std::ios_base::out;
    ns3::GlobalStream::Create(filename,filemode);
    ns3::LogComponentEnable("test",ns3::LOG_ALL);
    ns3::LogComponentEnable("proto_pacing",ns3::LOG_ALL);
    ns3::LogComponentEnable("proto_bbr",ns3::LOG_ALL);
    NS_LOG_INFO("hello ns3 log");
    SystemClock clock;
    SendPacketManager manager(&clock,nullptr);
    TimeDelta delta=manager.GetRetransmissionDelay();
    DLOG(INFO)<<delta;
    manager.SetSendAlgorithm(kBBR);
    QuicBandwidth pacing_bw=manager.PacingRate();
    float kDefaultHighGain = 2.885f;
    ByteCount ini_cwnd=10 * kDefaultTCPMSS;
    TimeDelta rtt(TimeDelta::FromMilliseconds(100));
    QuicBandwidth bw=kDefaultHighGain*(QuicBandwidth::FromBytesAndTimeDelta(ini_cwnd,rtt));
    TimeDelta wait=bw.TransferTime(1000);
    NS_LOG_INFO("pacing rate "<<pacing_bw);
    NS_LOG_FILE_INFO(bw<<wait);
}
void test_test(){
    //ack_frame_test();
    //proto_types_test();
    //interval_test();
    //byte_order_test();
   //test_proto_framer();
   //stream_test();
   //test_stream_endecode();
   //test_ufloat();
    //test_readbuf();
    //test_alarm();
    //test_stop_waiting();
    //test_socket_address();
    //thread_test();
    //simu_send_receiver_test();
    //test_sender_receiver();
    //packet_indexed_queue_test();
    //pacing_test();
    test_trivial();
}
