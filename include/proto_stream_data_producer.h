#ifndef PROTTO_STREAM_DATA_PRODUCER_H_
#define PROTTO_STREAM_DATA_PRODUCER_H_
#include "proto_types.h"
#include "byte_codec.h"
namespace dqc{
class ProtoStreamDataProducer{
public:
    virtual ~ProtoStreamDataProducer(){};
    virtual bool WriteStreamData(uint32_t id,
                                 StreamOffset offset,
                                 ByteCount len,
                                 basic::DataWriter *writer)=0;
};
}
#endif
