#ifndef FLAG_IMPL_H_
#define FLAG_IMPL_H_
#include "proto_types.h"
#define PROTO_FLAG(type,flag,value) extern type flag;
#include "flag_list.h"
#undef PROTO_FLAG
inline uint64_t GetFlagImpl(uint64_t flag){
    return flag;
}
inline  uint32_t GetFlagImpl(uint32_t flag){
    return flag;
}
#endif

