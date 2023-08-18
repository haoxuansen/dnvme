/**
 * @file libbase.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-07-21
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UAPI_LIBBASE_H_
#define _UAPI_LIBBASE_H_

#include <unistd.h> /* for "@usleep" */
#include <stdint.h> /* for "uint32_t" */
#include <stdlib.h> /* for "@calloc" */

#define msleep(ms)			usleep(1000 * (ms))

#define ARRAY_SIZE(x)			(sizeof(x) / sizeof(x[0]))

#define DIV_ROUND_UP(n,d)		(((n) + (d) - 1) / (d))

#define ALIGN(x, a)			(((x) + (a) - 1) & ~((a) - 1))
#define IS_ALIGNED(x, a)		(((x) & ((typeof(x))(a) - 1)) == 0)

#define BIT(x)				(1UL << (x))

/**
 * upper_32_bits - return bits 32-63 of a number
 * @n: the number we're accessing
 *
 * A basic shift-right of a 64- or 32-bit quantity.  Use this to suppress
 * the "right shift count >= width of type" warning when that quantity is
 * 32-bits.
 */
#define upper_32_bits(n)		((u32)(((n) >> 16) >> 16))

/**
 * lower_32_bits - return bits 0-31 of a number
 * @n: the number we're accessing
 */
#define lower_32_bits(n) 		((u32)((n) & 0xffffffff))

#include "base/types.h"
#include "base/limits.h"
#include "base/bitops.h"
#include "base/log.h"
#include "base/minmax.h"

/* ==================== related to "base.c" ==================== */

static inline void *zalloc(size_t size)
{
	return calloc(1, size);
}

int call_system(const char *command);

int fill_data_with_incseq(void *buf, uint32_t size);
int fill_data_with_decseq(void *buf, uint32_t size);
int fill_data_with_random(void *buf, uint32_t size);

int dump_data_to_console(void *buf, uint32_t size, const char *desc);
int dump_data_to_file(void *buf, uint32_t size, const char *file);

#endif /* !_UAPI_LIBBASE_H_ */
