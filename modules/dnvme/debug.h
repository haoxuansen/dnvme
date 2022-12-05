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
#include "dnvme_ds.h"
#include "dnvme_ioctl.h"

void dnvme_print_ccmd(struct nvme_common_command *ccmd);

void dnvme_print_sq(struct nvme_sq *sq);
void dnvme_print_cq(struct nvme_cq *cq);

const char *dnvme_ioctl_cmd_string(unsigned int cmd);

#endif /* !_DNVME_DEBUG_H_ */

