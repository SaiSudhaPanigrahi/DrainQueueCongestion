#include "send_receive.h"
#include "logging.h"
#include "random.h"
namespace dqc{
const char *ip="127.0.0.1";
const uint16_t send_port=1234;
const uint16_t recv_port=4321;
class FakeReceiver:public Socket,ProtoFrameVisitor{
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
    virtual int SendTo(const char*buf,size_t size,SocketAddress &dst) override{
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
    SocketAddress local_;
    FakeAckFrameReceive *feed_ack_{nullptr};
};
Sender::Sender(){
    sock_=new UdpSocket();
    sock_->Bind(ip,send_port);
    connection_.set_packet_writer(sock_);
    stream_=connection_.GetOrCreateStream(stream_id_);
}
Sender::~Sender(){
    if(sock_){
        delete sock_;
    }
}
void Sender::Process(){
    connection_.Process(stream_id_);
}
void Sender::set_packet_writer(Socket *sock){
    connection_.set_packet_writer(sock);
}
void Sender::DataGenerator(int times){
    char data[1500];
    int i=0;
    for (i=0;i<1500;i++){
        data[i]=RandomLetter::Instance()->GetLetter();
    }
    std::string piece(data,1500);
    for(i=0;i<times;i++){
        stream_->WriteDataToBuffer(piece);
    }
}
void Sender::OnPeerData(SocketAddress &peer,char *data,
                    int len,ProtoTime &recipt_time){
    ProtoReceivedPacket packet(data,len,recipt_time);
    connection_.ProcessUdpPacket(local_,peer,packet);
}
Receiver::Receiver(){
    frame_decoder_.set_visitor(this);
}
Receiver::~Receiver(){

}
void Receiver::Process(){

}
bool Receiver::OnStreamFrame(PacketStream &frame){
    return true;
}
void Receiver::OnError(ProtoFramer* framer){
}
bool Receiver::OnAckFrameStart(PacketNumber largest_acked,
                               TimeDelta ack_delay_time){
    DLOG(INFO)<<"SNC";
    return false;
}
bool Receiver::OnAckRange(PacketNumber start,
                          PacketNumber end){
    DLOG(INFO)<<"SNC";
    return false;
}
bool Receiver::OnAckTimestamp(PacketNumber packet_number,
                              ProtoTime timestamp){
    DLOG(INFO)<<"SNC";
    return false;
}
bool Receiver::OnAckFrameEnd(PacketNumber start){
    DLOG(INFO)<<"SNC";
    return false;
}
bool Receiver::OnStopWaitingFrame(const PacketNumber least_unacked){
    DLOG(INFO)<<"stop waiting "<<least_unacked;
    received_packet_manager_.DontWaitForPacketsBefore(least_unacked);
    return true;
}
int Receiver::SendTo(const char*buf,size_t size,SocketAddress &dst){
    std::unique_ptr<char> data(new char[size]);
    memcpy(data.get(),buf,size);
    basic::DataReader r(data.get(),size);
    ProtoPacketHeader header;
    ProcessPacketHeader(&r,header);
    PacketNumber seq=header.packet_number;
    DLOG(INFO)<<"sendrecv seq "<<seq;
    bool drop=false;
    if(2==seq||4==seq){
        drop=true;
    }
    ProtoTime receive_time=ProtoTime::Zero();
    if(!drop){
    receive_time=base_time_+offset_*count_;
    // to prevent process stop_waiting first;
    received_packet_manager_.RecordPacketReceived(seq,receive_time);
    frame_decoder_.ProcessFrameData(&r,header);
    }
    count_++;
    if(!drop){
        SendAckFrame(receive_time);
    }
    return size;
}
void Receiver::SendAckFrame(ProtoTime now){
    ProtoTime ack_delay=now+one_way_delay_;
    const AckFrame &ack_frame=received_packet_manager_.GetUpdateAckFrame(now);
    char wbuf[1500];
    basic::DataWriter writer(wbuf,1500,basic::NETWORK_ORDER);
    ProtoPacketHeader header;
    header.packet_number=AllocSeq();
    AppendPacketHeader(header,&writer);
    framer_encoder_.AppendAckFrameAndTypeByte(ack_frame,&writer);
    if(feed_ack_){
        feed_ack_->OnPeerData(local_,wbuf,writer.length(),ack_delay);
    }
}
void send_receiver_test(){
    SocketAddress send_addr(ip,send_port);
    SocketAddress recv_addr(ip,recv_port);
    Sender sender;
    sender.set_address(send_addr);
    Receiver receiver;
    receiver.set_address(recv_addr);
    receiver.set_ack_sink(&sender);
    sender.set_packet_writer(&receiver);
    sender.DataGenerator(10);
    for(int i=0;i<15;i++){
        sender.Process();
    }
}
}
