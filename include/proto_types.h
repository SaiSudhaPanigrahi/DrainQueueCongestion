#ifndef PROTO_TYPES_H_
#define PROTO_TYPES_H_
#include <stdint.h>
#include <cstddef>
#include <vector>
namespace dqc{
typedef uint64_t StreamOffset;
typedef uint64_t ByteCount;
typedef uint16_t PacketLength;
typedef uint64_t PacketNumber;
typedef uint64_t TimeType;
struct AckedPacket;
typedef std::vector<PacketNumber> LostPackerVector;
typedef std::vector<AckedPacket> AckedPacketVector;
struct AckedPacket{
AckedPacket(PacketNumber seq,PacketLength len,uint64_t ts)
:seq(seq),len(len),receive_ts(ts){}
PacketNumber seq;
PacketLength len;
uint64_t receive_ts;
};
#if defined WIN_32
struct iovec{
    void *iov_base;
    size_t iov_len;
};
#else
#inlcude <sys/uio.h>
#endif
class PacketNumberHash{
public:
    //here const is a must,or else hash functuon is not
    uint64_t operator()(const PacketNumber &number) const{
        return number;
    }
};
enum ProtoPacketNumberLength:uint8_t{
    PACKET_NUMBER_1BYTE=1,
    PACKET_NUMBER_2BYTE=2,
    PACKET_NUMBER_3BYTE=3,
    PACKET_NUMBER_4BYTE=4,
    PACKET_NUMBER_6BYTE=6,
};
enum ProtoPacketNumberLengthFlag:uint8_t{
    PACKET_FLAG_1BYTE=0,
    PACKET_FLAG_2BYTE=1,
    PACKET_FLAG_3BYTE=1<<1,
    PACKET_FLAG_4BYTE=1<<1|1,
};
enum ProtoFrameType:uint8_t{
    PROTO_FRAME_STOP_WAITING=6,
    PROTO_PING_FRAME = 7,
    PROTO_FRAME_STREAM,
    PTOTO_FRAME_ACK,
};
//header is NULL;
struct ProtoPacketHeader{
    uint64_t con_id;
    PacketNumber packet_number;
};
//may be the stop waiting send along with stream frame?
//public header 0x00ll0000+seq,
// make these frame protocol similar to quic.
//https://blog.csdn.net/tq08g2z/article/details/77311763
//type+payload
ProtoPacketNumberLength ReadPacketNumberLength(uint8_t flag);
ProtoPacketNumberLengthFlag PktNumLen2Flag(ProtoPacketNumberLength byte);
ProtoPacketNumberLength GetMinPktNumLen(uint64_t seq);
}//namespace dqc;
#endif
