/**
 * @file limits.h
 * @author yeqiang_xu (yeqiang_xu@maxio-tech.com)
 * @brief 
 * @version 0.1
 * @date 2023-08-13
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UAPI_LIB_BASE_LIMITS_H_
#define _UAPI_LIB_BASE_LIMITS_H_

#define USHRT_MAX	((unsigned short)~0U)
#define SHRT_MAX	((short)(USHRT_MAX >> 1))
#define SHRT_MIN	((short)(-SHRT_MAX - 1))
#define UINT_MAX	(~0U)
#define INT_MAX		((int)(UINT_MAX >> 1))
#define INT_MIN		(-INT_MAX - 1)
#define ULONG_MAX	(~0UL)
#define LONG_MAX	((long)(ULONG_MAX >> 1))
#define LONG_MIN	(-LONG_MAX - 1)
#define ULLONG_MAX	(~0ULL)
#define LLONG_MAX	((long long)(ULLONG_MAX >> 1))
#define LLONG_MIN	(-LLONG_MAX - 1)

#define U8_MAX		((u8)~0U)
#define S8_MAX		((s8)(U8_MAX >> 1))
#define S8_MIN		((s8)(-S8_MAX - 1))
#define U16_MAX		((u16)~0U)
#define S16_MAX		((s16)(U16_MAX >> 1))
#define S16_MIN		((s16)(-S16_MAX - 1))
#define U32_MAX		((u32)~0U)
#define S32_MAX		((s32)(U32_MAX >> 1))
#define S32_MIN		((s32)(-S32_MAX - 1))
#define U64_MAX		((u64)~0ULL)
#define S64_MAX		((s64)(U64_MAX >> 1))
#define S64_MIN		((s64)(-S64_MAX - 1))

#endif /* !_UAPI_LIB_BASE_LIMITS_H_ */
