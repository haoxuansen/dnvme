/**
 * @file debug.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-22
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _LIB_NVME_DEBUG_H_
#define _LIB_NVME_DEBUG_H_

#include <stdint.h>

#include "dnvme.h"

struct nvme_ctrl_property;

void nvme_display_buffer(const char *str, void *buf, uint32_t size);

void nvme_display_id_ctrl(struct nvme_id_ctrl *ctrl);
void nvme_display_id_ns(struct nvme_id_ns *ns, uint32_t nsid);

void nvme_display_cap(uint64_t cap);
void nvme_display_cc(uint32_t cc);
void nvme_display_ctrl_property(struct nvme_ctrl_property *prop);

#endif /* !_LIB_NVME_DEBUG_H_ */
