#ifndef PROTO_STREAM_H_
#define PROTO_STREAM_H_
#include "memslice.h"
#include "proto_con_visitor.h"
#include <string>
namespace dqc{
class ProtoStream{
public:
    ProtoStream(ProtoConVisitor *visitor,uint32_t id);
    ~ProtoStream(){}
    void WriteDataToBuffer(std::string &piece);
    void OnAck(StreamOffset offset,ByteCount len);
    uint64_t BufferedBytes() const;
    ByteCount Inflight();
    bool HasBufferedData() const;
    void OnCanWrite();
    bool WriteStreamData(StreamOffset offset,ByteCount len,basic::DataWriter *writer);
    uint32_t id(){
        return stream_id_;
    }
    void SimuPacketGenerator();
private:
    void WriteBufferedData();
    void OnConsumedData(ByteCount len);
    ProtoConVisitor *visitor_{nullptr};
    uint32_t stream_id_{0};
    StreamSendBuffer send_buf_;
    std::map<StreamOffset,ByteCount> sent_info_;
};
}
#endif
