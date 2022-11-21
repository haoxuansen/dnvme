/**
 * @file hweight.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-11-21
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _UAPI_BITOPS_HWEIGHT_H_
#define _UAPI_BITOPS_HWEIGHT_H_

unsigned int __sw_hweight8(unsigned int w);
unsigned int __sw_hweight16(unsigned int w);
unsigned int __sw_hweight32(unsigned int w);
unsigned long __sw_hweight64(unsigned long long w);

#endif /* !_UAPI_BITOPS_HWEIGHT_H_ */
