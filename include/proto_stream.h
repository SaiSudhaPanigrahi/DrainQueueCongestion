#ifndef PROTO_STREAM_H_
#define PROTO_STREAM_H_
#include "memslice.h"
#include "proto_con_visitor.h"
#include "proto_stream_data_producer.h"
#include "proto_stream_sequencer.h"
#include <string>
namespace dqc{
struct PacketStream;
class ProtoStream:public ProtoStreamSequencer::StreamInterface{
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
    virtual void OnStreamFrame(PacketStream &frame);
    virtual void OnDataAvailable() override;
private:
    void WriteBufferedData();
    void OnConsumedData(ByteCount len);
    ProtoConVisitor *visitor_{nullptr};
    ProtoStreamSequencer sequencer_;
    uint32_t stream_id_{0};
    StreamSendBuffer send_buf_;
    std::map<StreamOffset,ByteCount> sent_info_;
};
}
#endif
