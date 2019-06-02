#pragma once
#include "proto_comm.h"
#include "proto_con_visitor.h"
#include "proto_stream.h"
#include "proto_packets.h"
#include "send_packet_manager.h"
#include "proto_framer.h"
#include "proto_stream_data_producer.h"
#include "proto_socket.h"
#include "alarm.h"
#include <deque>
#include <map>
namespace dqc{
class ProtoCon:public ProtoConVisitor,StreamAckedObserver,
ProtoFrameVisitor,ProtoStreamDataProducer{
public:
    ProtoCon(ProtoClock *clock,AlarmFactory *alarm_factory);
    ~ProtoCon();
    void ProcessUdpPacket(SocketAddress &self,SocketAddress &peer,
                          const ProtoReceivedPacket& packet);
    ProtoStream *GetOrCreateStream(uint32_t id);
    void Close(uint32_t id);
    void set_packet_writer(Socket *writer){
        packet_writer_=writer;
    }
    void set_peer(SocketAddress &peer){peer_=peer;}
    void Process();
    bool CanWrite(HasRetransmittableData has_retrans);
    PacketNumber AllocSeq(){
        return seq_++;
    }
    const TimeDelta GetRetransmissionDelay() const{
        return sent_manager_.GetRetransmissionDelay();
    }
    void OnRetransmissionTimeOut();
    void OnCanWrite();
    virtual void WritevData(uint32_t id,StreamOffset offset,ByteCount len,bool fin) override;
    virtual void OnAckStream(uint32_t id,StreamOffset off,ByteCount len) override;
    //framevisitor
    virtual bool OnStreamFrame(PacketStream &frame) override;
    virtual void OnError(ProtoFramer* framer) override;
    virtual bool OnAckFrameStart(PacketNumber largest_acked,
                                 TimeDelta ack_delay_time) override;
    virtual bool OnAckRange(PacketNumber start, PacketNumber end) override;
    virtual bool OnAckTimestamp(PacketNumber packet_number,
                                ProtoTime timestamp) override;
    virtual bool OnAckFrameEnd(PacketNumber start) override;
    //ProtoStreamDataProducer
    virtual bool WriteStreamData(uint32_t id,
                                 StreamOffset offset,
                                 ByteCount len,
                                 basic::DataWriter *writer) override;
private:
    ProtoStream *CreateStream();
    ProtoStream *GetStream(uint32_t id);
    void NotifyCanSendToStreams();
    int Send();
    bool SendRetransPending(TransType tt);
    void Retransmit(uint32_t id,StreamOffset off,ByteCount len,bool fin,TransType tt);
    ProtoClock *clock_{nullptr};
    std::map<uint32_t,ProtoStream*> streams_;
    std::deque<PacketStream> waiting_info_;
    uint32_t stream_id_{0};
    PacketNumber seq_{1};
    ProtoTime time_of_last_received_packet_;
    SendPacketManager sent_manager_;
    SocketAddress peer_;
    bool first_packet_from_peer_{true};
    ProtoFramer frame_encoder_;
    ProtoFramer frame_decoder_;
    Socket *packet_writer_{nullptr};
    AlarmFactory *alarm_factory_{nullptr};
    std::shared_ptr<Alarm> send_alarm_;
};
}//namespace dqc;
