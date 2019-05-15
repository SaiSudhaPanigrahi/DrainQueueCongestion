#ifndef PACKET_WRITER_H_
#define PACKET_WRITER_H_
#include "proto_types.h"
namespace dqc{
class PacketWriterInterface{
public:
    virtual ~PacketWriterInterface(){};
    virtual int WritePacket(const char*buf,size_t size,su_addr &dst)=0;
};
}
#endif
