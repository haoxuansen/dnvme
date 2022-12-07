/**
 * @file byteorder.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-07
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _UAPI_BYTEORDER_H_
#define _UAPI_BYTEORDER_H_

/* !TODO: Assume CPU is little-endian here
 */

#include "byteorder/little_endian.h"

#define cpu_to_le64 __cpu_to_le64
#define le64_to_cpu __le64_to_cpu
#define cpu_to_le32 __cpu_to_le32
#define le32_to_cpu __le32_to_cpu
#define cpu_to_le16 __cpu_to_le16
#define le16_to_cpu __le16_to_cpu

#endif /* !_UAPI_BYTEORDER_H_ */
