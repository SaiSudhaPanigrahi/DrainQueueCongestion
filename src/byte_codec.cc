#include "byte_codec.h"
#include <memory.h>
#include <stdio.h>
namespace basic{
DataReader::DataReader(const char* buf,uint32_t len)
:DataReader(buf,len,NETWORK_ORDER){}
DataReader::DataReader(const char* buf,uint32_t len,Endianness endianness)
:data_(buf)
,len_(len)
,pos_(0)
,endianness_(endianness){
}
bool DataReader::ReadUInt8(uint8_t *result){
    return ReadBytes(result, sizeof(uint8_t));
}
bool DataReader::ReadUInt16(uint16_t *result){
    if(!ReadBytes(result,sizeof(uint16_t))){
        return false;
    }
    if(endianness_ == NETWORK_ORDER){
        *result=basic::NetToHost16(*result);
    }
    return true;
}
bool DataReader::ReadUInt32(uint32_t *result){
    if(!ReadBytes(result,sizeof(uint32_t))){
        return false;
    }
    if(endianness_ == NETWORK_ORDER){
        *result=basic::NetToHost32(*result);
    }
    return true;
}
bool DataReader::ReadUInt64(uint64_t *result){
    if(!ReadBytes(result,sizeof(uint64_t))){
        return false;
    }
    if(endianness_ == NETWORK_ORDER){
        *result=basic::NetToHost64(*result);
    }
    return true;
}
bool DataReader::ReadBytes(void*result,uint32_t size){
    if(!CanRead(size)){
        OnFailure();
        return false;
    }
    memcpy(result,data_+pos_,size);
    pos_+=size;
    return true;
}
bool DataReader::CanRead(uint32_t bytes){
    return bytes<=(len_-pos_);
}
void DataReader::OnFailure(){
    pos_=len_;
}
DataWriter::DataWriter(char* buf,uint32_t len)
:DataWriter(buf,len,NETWORK_ORDER){
}
DataWriter::DataWriter(char* buf,uint32_t len,Endianness endianness)
:buf_(buf)
,pos_(0)
,capacity_(len)
,endianness_(endianness){
}
bool DataWriter::WriteUInt8(uint8_t value){
    return WriteBytes(&value,sizeof(uint8_t));
}
bool DataWriter::WriteUInt16(uint16_t value){
    if(endianness_ == NETWORK_ORDER){
        value=basic::HostToNet16(value);
    }
    return WriteBytes(&value,sizeof(uint16_t));
}
bool DataWriter::WriteUInt32(uint32_t value){
    if(endianness_ == NETWORK_ORDER){
        value=basic::HostToNet32(value);
    }
    return WriteBytes(&value,sizeof(uint32_t));
}
bool DataWriter::WriteUInt64(uint64_t value){
    if(endianness_ == NETWORK_ORDER){
        value=basic::HostToNet64(value);
    }
    return WriteBytes(&value,sizeof(uint64_t));
}
bool DataWriter::WriteBytes(const void *value,uint32_t size){
    char *dst=BeginWrite(size);
    if(!dst){
        return false;
    }
    memcpy((void*)dst,value,size);
    pos_+=size;
    return true;
}
char* DataWriter::BeginWrite(uint32_t bytes){
    if(pos_>capacity_){
        return nullptr;
    }
    if(capacity_-pos_<bytes){
        return nullptr;
    }
    return buf_+pos_;
}
}
