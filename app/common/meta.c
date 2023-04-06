/**
 * @file meta.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-16
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#include "defs.h"
#include "log.h"
#include "meta.h"

/**
 * @brief Map contiguous meta node to user space
 * 
 * @param id meta node identifier
 * @param size meta node size
 * @return meta buffer pointer if success, otherwise returns NULL.
 */
void *nvme_map_meta_node(int fd, uint16_t id, uint32_t size)
{
	size_t pg_size;
	size_t pgoff;
	void *meta;

	pg_size = sysconf(_SC_PAGE_SIZE);
	pgoff = NVME_VMPGOFF_FOR_TYPE(NVME_VMPGOFF_TYPE_META) | id;

	meta = mmap(NULL, size, PROT_READ | PROT_WRITE, 
		MAP_SHARED, fd, pg_size * pgoff);
	if (MAP_FAILED == meta)
		return NULL;

	return meta;
}

int nvme_create_meta_node(int fd, struct nvme_meta_create *mc)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_CREATE_META_NODE, mc);
	if (ret < 0) {
		pr_err("failed to create meta node %u!(%d)\n", mc->id, ret);
		return ret;
	}
	return 0;
}

int nvme_delete_meta_node(int fd, uint16_t id)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_DELETE_META_NODE, id);
	if (ret < 0) {
		pr_err("failed to delete meta node %u!(%d)\n", id, ret);
		return ret;
	}
	return 0;
}

