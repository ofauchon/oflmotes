#ifndef MY_UTILS_H
#define MY_UTILS_H

#include "kernel_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void dump_hex(void* pData, size_t pLen);
void dump_hex_compact(void* pData, size_t pLen);



#ifdef __cplusplus
}
#endif

#endif /* UTIL_H */
/** @} */

