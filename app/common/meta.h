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
#include <sys/mman.h>

#include "dnvme_ioctl.h"

int nvme_create_meta_node(int fd, struct nvme_meta_create *mc);
int nvme_delete_meta_node(int fd, uint16_t id);

void *nvme_map_meta_node(int fd, uint16_t id, uint32_t size);

static inline int nvme_unmap_meta_node(int fd, void *meta, uint32_t size)
{
	return munmap(meta, size);
}

#endif /* !_APP_META_H_ */
