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

#ifndef likely
#define likely(x)			__builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x)			__builtin_expect(!!(x), 0)
#endif

#ifndef __init
#define __init				__attribute__((constructor))
#endif
#ifndef __exit
#define __exit				__attribute__((destructor))
#endif

#ifndef __deprecated
#define __deprecated			__attribute__((deprecated))
#endif

#ifndef __unused
#define __unused			__attribute__((unused))
#endif

#ifndef __weak
#define __weak				__attribute__((weak))
#endif

#ifndef __nonnull
#define __nonnull(x)			__attribute__((nonnull(x)))
#endif

#define __optimize(x)			__attribute__((optimize(x)))

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
