/**
 * @file proc.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-03-22
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _DNVME_PROC_H_
#define _DNVME_PROC_H_

#include <linux/proc_fs.h>

#include "core.h"

int dnvme_create_proc_entry(struct nvme_device *ndev, 
	struct proc_dir_entry *parent);

void dnvme_destroy_proc_entry(struct nvme_device *ndev);

#endif /* !_DNVME_PROC_H_ */
