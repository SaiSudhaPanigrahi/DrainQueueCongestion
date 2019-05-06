#ifndef PROTO_CON_VISITOR_H_
#define PROTO_CON_VISITOR_H_
#include "proto_types.h"
class ProtoConVisitor{
public:
    virtual void WritevData(uint32_t id,StreamOffset offset,ByteCount len)=0;
    virtual ~ProtoConVisitor(){}
};
#endif
