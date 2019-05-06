#ifndef PROTO_CON_H_
#define PROTO_CON_H_
#include "proto_types.h"
#include "proto_con_visitor.h"
#include "proto_stream.h"
#include "proto_packets.h"
#include <deque>
#include <map>
class ProtoCon:public ProtoConVisitor{
public:
    ProtoCon();
    ~ProtoCon();
    virtual void WritevData(uint32_t id,StreamOffset offset,ByteCount len) override;
    ProtoStream *GetStream(uint32_t id);
    void Close(uint32_t id);
    void Test();
    PacketNumber AllocSeq(){
        return seq_++;
    }
private:
    ProtoStream *CreateStream();
    int Send(ProtoStream *stream,char *buf);
    std::map<uint32_t,ProtoStream*> streams_;
    std::deque<PacketStream> waiting_info_;
    uint32_t stream_id_{0};
    PacketNumber seq_{1};
};
#endif
