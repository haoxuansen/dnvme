/**
 * @file defs.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-11-21
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _UAPI_DEFS_H_
#define _UAPI_DEFS_H_

#define ARRAY_SIZE(x)			(sizeof(x) / sizeof(x[0]))

#define DIV_ROUND_UP(n,d)		(((n) + (d) - 1) / (d))

#define IS_ALIGNED(x, a)		(((x) & ((typeof(x))(a) - 1)) == 0)

#endif /* !_UAPI_DEFS_H_ */

