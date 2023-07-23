/**
 * @file bitops.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-11-21
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _LINUX_BITOPS_H
#define _LINUX_BITOPS_H

#include "defs.h"

#define BITS_PER_BYTE			8

#define BITS_PER_LONG			(BITS_PER_BYTE * sizeof(long))
#define BITS_TO_LONGS(nr)		DIV_ROUND_UP(nr, BITS_PER_LONG)

#define BITS_PER_LONG_LONG		64

static inline void set_bit(unsigned int nr, unsigned long *addr)
{
	addr[nr / BITS_PER_LONG] |= 1UL << (nr % BITS_PER_LONG);
}

static inline void clear_bit(unsigned int nr, unsigned long *addr)
{
	addr[nr / BITS_PER_LONG] &= ~(1UL << (nr % BITS_PER_LONG));
}

static inline int test_bit(unsigned int nr, const unsigned long *addr)
{
	return ((1UL << (nr % BITS_PER_LONG)) &
		(((unsigned long *)addr)[nr / BITS_PER_LONG])) != 0;
}

static inline int test_and_clear_bit(unsigned int nr, unsigned long *addr)
{
	if (test_bit(nr, addr))
	{
		clear_bit(nr, addr);
		return 1;
	}
	return 0;
}

#endif /* !_LINUX_BITOPS_H */
