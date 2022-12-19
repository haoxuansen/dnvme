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

#include <stdio.h>
#include <stdint.h>
#include <sys/ioctl.h>

#include "log.h"
#include "dnvme_ioctl.h"
#include "meta.h"

int nvme_create_meta_pool(int fd, uint32_t size)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_CREATE_META_POOL, size);
	if (ret < 0) {
		pr_err("failed to create meta pool with size 0x%x!(%d)\n",
			size, ret);
		return ret;
	}
	return 0;
}

int nvme_destroy_meta_pool(int fd)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_DESTROY_META_POOL);
	if (ret < 0) {
		pr_err("failed to destroy meta pool!(%d)\n", ret);
		return ret;
	}
	return 0;
}

int nvme_create_meta_node(int fd, uint32_t id)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_CREATE_META_NODE, id);
	if (ret < 0) {
		pr_err("failed to create meta node %u!(%d)\n", id, ret);
		return ret;
	}
	return 0;
}

int nvme_delete_meta_node(int fd, uint32_t id)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_DELETE_META_NODE, id);
	if (ret < 0) {
		pr_err("failed to delete meta node %u!(%d)\n", id, ret);
		return ret;
	}
	return 0;
}
