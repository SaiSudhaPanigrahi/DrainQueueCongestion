#ifndef BYTE_CODEC_H_
#define BYTE_CODEC_H_
#include "byte_order.h"
namespace basic{
enum Endianness{
    NETWORK_ORDER, //big
    HOST_ORDER,//little
};
class DataReader{
public:
    DataReader(const char* buf,uint32_t len);
    DataReader(const char* buf,uint32_t len,Endianness endianness);
    ~DataReader(){}
    bool ReadUInt8(uint8_t *result);
    bool ReadUInt16(uint16_t *result);
    bool ReadUInt32(uint32_t *result);
    bool ReadUInt64(uint64_t *result);
    bool ReadBytes(void*result,uint32_t size);
private:
    bool CanRead(uint32_t bytes);
    void OnFailure();
    const char *data_{0};
    const uint32_t len_{0};
    uint32_t pos_{0};
    Endianness endianness_{HOST_ORDER};
};
class DataWriter{
public:
    DataWriter(char* buf,uint32_t len);
    DataWriter(char* buf,uint32_t len,Endianness endianness);
    ~DataWriter(){}
    int length(){
        return pos_;
    }
    bool WriteUInt8(uint8_t value);
    bool WriteUInt16(uint16_t value);
    bool WriteUInt32(uint32_t value);
    bool WriteUInt64(uint64_t value);
    bool WriteBytes(const void *value,uint32_t size);
private:
    char* BeginWrite(uint32_t bytes);
    char *buf_{0};
    uint32_t pos_{0};
    uint32_t capacity_{0};
    Endianness endianness_{HOST_ORDER};
};
}
#endif
