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

#include "base/log.h"
#include "base/minmax.h"

#define msleep(ms)			usleep(1000 * (ms))

/* ==================== related to "base.c" ==================== */

int call_system(const char *command);

int dump_data_to_console(void *buf, uint32_t size);
int dump_data_to_file(void *buf, uint32_t size, const char *file);

#endif /* !_UAPI_LIBBASE_H_ */
