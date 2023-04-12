/**
 * @file debug.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-11-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _DNVME_DEBUG_H_
#define _DNVME_DEBUG_H_

#include "nvme.h"
#include "dnvme.h"
#include "core.h"

#if IS_ENABLED(CONFIG_DNVME_DEBUG)
void dnvme_print_sq(struct nvme_sq *sq);
void dnvme_print_cq(struct nvme_cq *cq);

const char *dnvme_ioctl_cmd_string(unsigned int cmd);
#else
static inline void dnvme_print_sq(struct nvme_sq *sq) {}
static inline void dnvme_print_cq(struct nvme_cq *cq) {}

static inline const char *dnvme_ioctl_cmd_string(unsigned int cmd)
{
	return "";
}
#endif /* !IS_ENABLED(CONFIG_DNVME_DEBUG) */

#endif /* !_DNVME_DEBUG_H_ */

