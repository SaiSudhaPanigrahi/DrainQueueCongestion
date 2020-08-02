#pragma once
#include "ns-linux-types.h"
#ifdef __cplusplus
extern "C" {
#endif
//Returns: pseudo-random number in interval [0, ep_ro)
u32 prandom_u32_max(u32 ep_ro);
#ifdef __cplusplus
}
#endif