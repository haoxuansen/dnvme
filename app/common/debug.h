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

#ifndef _APP_DEBUG_H_
#define _APP_DEBUG_H_

#include <stdint.h>

#include "dnvme.h"

void nvme_display_buffer(const char *str, void *buf, uint32_t size);

void nvme_display_id_ctrl(struct nvme_id_ctrl *ctrl);
void nvme_display_id_ns(struct nvme_id_ns *ns, uint32_t nsid);

void nvme_display_cap(uint64_t cap);
void nvme_display_cc(uint32_t cc);

#endif /* !_APP_DEBUG_H_ */
