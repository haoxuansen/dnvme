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

int nvme_compare_meta_node(int fd, uint32_t id1, uint32_t id2)
{
	struct nvme_meta_compare cmp;
	int ret;

	cmp.id1 = id1;
	cmp.id2 = id2;

	ret = ioctl(fd, NVME_IOCTL_COMPARE_META_NODE, &cmp);
	if (ret < 0) {
		pr_err("The meta data of %u and %u are different?(%d)\n", 
			id1, id2, ret);
		return ret;
	}
	return 0;
}

