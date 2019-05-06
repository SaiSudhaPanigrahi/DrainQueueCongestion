#ifndef PROTO_TYPES_H_
#define PROTO_TYPES_H_
#include <stdint.h>
#include <cstddef>
#include <vector>
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
#endif
