#ifndef SEND_RECEIVE_H_
#define SEND_RECEIVE_H_
#include "proto_con.h"
#include "received_packet_manager.h"
#include "socket.h"
namespace dqc{
class FakeAckFrameReceive{
public:
    virtual ~FakeAckFrameReceive(){}
    virtual void OnPeerData(SocketAddress &peer,char *data,int len,ProtoTime &recipt_time)=0;
};
class Sender:public FakeAckFrameReceive{
public:
    Sender();
    ~Sender();
    void Process();
    void set_address(SocketAddress addr){local_=addr;}
    void set_packet_writer(Socket *sock);
    void DataGenerator(int times);
    void OnPeerData(SocketAddress &peer,char *data,
                    int len,ProtoTime &recipt_time) override;
private:
    Socket *sock_{nullptr};
    ProtoCon connection_;
    ProtoStream *stream_{nullptr};
    SocketAddress local_;
    uint32_t stream_id_{0};
};
class Receiver :public ProtoFrameVisitor, public Socket{
public:
    Receiver();
    ~Receiver();
    void Process();
    void set_address(SocketAddress addr){local_=addr;}
    void set_ack_sink(FakeAckFrameReceive *sink){feed_ack_=sink;}
    bool OnStreamFrame(PacketStream &frame) override;
    void OnError(ProtoFramer* framer) override;
    bool OnAckFrameStart(PacketNumber largest_acked,
                         TimeDelta ack_delay_time) override;
    bool OnAckRange(PacketNumber start,
                    PacketNumber end) override;
    bool OnAckTimestamp(PacketNumber packet_number,
                        ProtoTime timestamp) override;
    bool OnAckFrameEnd(PacketNumber start) override;
    bool OnStopWaitingFrame(const PacketNumber least_unacked) override;
    int SendTo(const char*buf,size_t size,SocketAddress &dst) override;
    void SendAckFrame(ProtoTime now);
    PacketNumber AllocSeq(){
        return seq_++;
    }
private:
    Socket *sock_{nullptr};
    ReceivdPacketManager received_packet_manager_;
    ProtoFramer frame_decoder_;
    ProtoFramer framer_encoder_;
    FakeAckFrameReceive *feed_ack_{nullptr};
    SocketAddress local_;
    ProtoTime base_time_{ProtoTime::Zero()};
    TimeDelta offset_{TimeDelta::FromMilliseconds(10)};
    TimeDelta one_way_delay_{TimeDelta::FromMilliseconds(100)};
    int count_{1};
    PacketNumber seq_{1};
};
void send_receiver_test();
}
#endif
