/**
 * @file compiler.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-11-11
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __LINUX_COMPILER_H
#define __LINUX_COMPILER_H

#define __force


#define likely(x)			__builtin_expect(!!(x), 1)
#define unlikely(x)			__builtin_expect(!!(x), 0)

#define __init				__attribute__((constructor))
#define __exit				__attribute__((destructor))

#define __deprecated			__attribute__((deprecated))

#define __used				__attribute__((__used__))
#define __unused			__attribute__((__unused__))

#define __weak				__attribute__((__weak__))

#define __optimize(x)			__attribute__((optimize(x)))

#define __section(section)              __attribute__((__section__(section)))

/* Indirect macros required for expanded argument pasting, eg. __LINE__. */
#define ___PASTE(a,b) a##b
#define __PASTE(a,b) ___PASTE(a,b)

#define __UNIQUE_ID(prefix) __PASTE(__PASTE(__UNIQUE_ID_, prefix), __COUNTER__)

#define __compiletime_warning(message)	__attribute__((__warning__(message)))
#define __compiletime_error(message)	__attribute__((__error__(message)))

#define __compiletime_assert(condition, msg, prefix, suffix)		\
	do {								\
		extern void prefix ## suffix(void) __compiletime_error(msg); \
		if (!(condition))					\
			prefix ## suffix();				\
	} while (0)

#define _compiletime_assert(condition, msg, prefix, suffix) \
	__compiletime_assert(condition, msg, prefix, suffix)

/**
 * compiletime_assert - break build and emit msg if condition is false
 * @condition: a compile-time constant condition to check
 * @msg:       a message to emit if condition is false
 *
 * In tradition of POSIX assert, this macro will break the build if the
 * supplied condition is *false*, emitting the supplied error message if the
 * compiler has support to do so.
 */
#define compiletime_assert(condition, msg) \
	_compiletime_assert(condition, msg, __compiletime_assert_, __COUNTER__)

#endif /* !__LINUX_COMPILER_H */
