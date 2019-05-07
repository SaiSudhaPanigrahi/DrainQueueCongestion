#ifndef PROsTO_CON_H_
#define PROTO_CON_H_
#include "proto_comm.h"
#include "proto_con_visitor.h"
#include "proto_stream.h"
#include "proto_packets.h"
#include "send_packet_manager.h"
#include <deque>
#include <map>
class ProtoCon:public ProtoConVisitor,StreamAckedObserver{
public:
    ProtoCon();
    ~ProtoCon();
    virtual void WritevData(uint32_t id,StreamOffset offset,ByteCount len) override;
    virtual void OnAckStream(uint32_t id,StreamOffset off,ByteCount len) override;
    ProtoStream *GetOrCreateStream(uint32_t id);
    void Close(uint32_t id);
    void Test();
    PacketNumber AllocSeq(){
        return seq_++;
    }
private:
    ProtoStream *CreateStream();
    ProtoStream *GetStream(uint32_t id);
    int Send(ProtoStream *stream,char *buf);
    void Retransmit(uint32_t id,StreamOffset off,ByteCount len);
    std::map<uint32_t,ProtoStream*> streams_;
    std::deque<PacketStream> waiting_info_;
    uint32_t stream_id_{0};
    PacketNumber seq_{1};
    SendPacketManager sent_manager_;
};
#endif
