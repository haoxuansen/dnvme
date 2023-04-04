/**
 * @file meta.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-16
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _APP_META_H_
#define _APP_META_H_

#include <stdint.h>
#include "dnvme_ioctl.h"

int nvme_create_meta_node(int fd, struct nvme_meta_create *mc);
int nvme_delete_meta_node(int fd, uint32_t id);

int nvme_compare_meta_node(int fd, uint32_t id1, uint32_t id2);

#endif /* !_APP_META_H_ */
