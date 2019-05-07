#ifndef PROTO_COMM_H_
#define PROTO_COMM_H_
#include "proto_types.h"
enum ContainsRetransData:uint8_t{
    CON_RE_YES,
    CON_RE_NO,
};
enum SentPacketState:uint8_t{
    SPS_OUT,
    SPS_ACKED,
    SPS_NERVER_SENT,
    SPS_UN_ACKABLE,
};
enum TransType:uint8_t{
    TT_NO_RETRANS,
    TT_FIRST_TRANS=TT_NO_RETRANS,
    TT_LOSS_RETRANS,
    TT_RTO_RETRANS,
};
class StreamAckedObserver{
public:
   virtual void OnAckStream(uint32_t id,StreamOffset off,ByteCount len)=0;
   virtual ~StreamAckedObserver(){}
};
#endif
