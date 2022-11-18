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

#ifndef likely
#define likely(x)			__builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x)			__builtin_expect(!!(x), 0)
#endif

#endif /* _UAPI_COMPILER_H_ */
