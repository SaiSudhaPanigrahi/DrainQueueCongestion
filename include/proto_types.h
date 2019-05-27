#pragma once
#include <stdint.h>
#include <cstddef>
#include <vector>
#include <limits>
#ifdef WIN_32
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <errno.h>
#endif
//copy from razor project;
#include "cf_platform.h"
namespace dqc{
typedef uint64_t StreamOffset;
typedef StreamOffset QuicStreamOffset;
typedef uint64_t ByteCount;
typedef ByteCount QuicByteCount;
typedef uint64_t PacketCount;
typedef PacketCount QuicPacketCount;
typedef uint16_t PacketLength;
typedef PacketLength QuicPacketLength;
typedef uint64_t PacketNumber;
typedef PacketNumber QuicPacketNumber;
typedef uint64_t TimeType;
struct AckedPacket;
const PacketNumber UnInitializedPacketNumber=std::numeric_limits<uint64_t>::max();
// Information about a newly lost packet.
struct LostPacket {
  LostPacket(PacketNumber packet_number, PacketLength bytes_lost)
      : packet_number(packet_number), bytes_lost(bytes_lost) {}

  PacketNumber packet_number;
  // Number of bytes sent in the packet that was lost.
  PacketLength bytes_lost;
};
typedef std::vector<LostPacket> LostPacketVector;
typedef std::vector<AckedPacket> AckedPacketVector;
struct AckedPacket{
AckedPacket(PacketNumber seq,PacketLength len,uint64_t ts)
:packet_number(seq),bytes_acked(len),receive_ts(ts){}
PacketNumber packet_number;
PacketLength bytes_acked;
uint64_t receive_ts;
};
#if defined WIN_32
struct iovec{
    void *iov_base;
    size_t iov_len;
};
#else
#include <sys/uio.h>
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
    ProtoPacketNumberLength packet_number_length;
};
enum ContainsRetransData:uint8_t{
    CON_RE_YES,
    CON_RE_NO,
};
//may be the stop waiting send along with stream frame?
//public header 0x00ll0000+seq,
// make these frame protocol similar to quic.
//https://blog.csdn.net/tq08g2z/article/details/77311763

// Defines for all types of congestion control algorithms that can be used in
// QUIC. Note that this is separate from the congestion feedback type -
// some congestion control algorithms may use the same feedback type
// (Reno and Cubic are the classic example for that).
enum CongestionControlType { kCubicBytes, kRenoBytes, kBBR, kPCC, kGoogCC };
ProtoPacketNumberLength ReadPacketNumberLength(uint8_t flag);
ProtoPacketNumberLengthFlag PktNumLen2Flag(ProtoPacketNumberLength byte);
ProtoPacketNumberLength GetMinPktNumLen(uint64_t seq);
}//namespace dqc;
