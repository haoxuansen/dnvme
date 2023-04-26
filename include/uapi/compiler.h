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

#ifndef _UAPI_COMPILER_H_
#define _UAPI_COMPILER_H_

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
#ifndef __weak
#define __weak				__attribute__((weak))
#endif

#ifndef __nonnull
#define __nonnull(x)			__attribute__((nonnull(x)))
#endif

#define __optimize(x)			__attribute__((optimize(x)))

#endif /* _UAPI_COMPILER_H_ */
