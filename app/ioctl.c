/**
 * @file ioctl.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-15
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <stdio.h>
#include <sys/ioctl.h>

#include "defs.h"
#include "log.h"
#include "ioctl.h"

static const char *nvme_state_string(enum nvme_state state)
{
	switch (state) {
	case NVME_ST_ENABLE:
		return "enable controller";
	case NVME_ST_DISABLE:
	case NVME_ST_DISABLE_COMPLETE:
		return "disable controller";
	case NVME_ST_RESET_SUBSYSTEM:
		return "reset subsystem";
	default:
		return "unknown";
	}
}

static const char *nvme_region_string(enum nvme_region region)
{
	switch (region) {
	case NVME_PCI_CONFIG:
		return "pci config space";
	case NVME_BAR0_BAR1:
		return "nvme properity";
	default:
		return "unknown";
	}
}

static enum nvme_access_type nvme_select_access_type(uint32_t oft, uint32_t len)
{
	if (IS_ALIGNED(oft, 4) && IS_ALIGNED(len, 4))
		return NVME_ACCESS_DWORD;
	if (IS_ALIGNED(oft, 2) && IS_ALIGNED(len, 2))
		return NVME_ACCESS_WORD;
	
	return NVME_ACCESS_BYTE;
}

int nvme_get_driver_info(int fd, struct nvme_driver *drv)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_GET_DRIVER_INFO, drv);
	if (ret < 0) {
		pr_err("failed to get driver info!(%d)\n", ret);
		return ret;
	}
	return 0;
}

int nvme_get_device_info(int fd, struct nvme_dev_public *dev)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_GET_DEVICE_INFO, dev);
	if (ret < 0) {
		pr_err("failed to get device info!(%d)\n", ret);
		return ret;
	}
	return 0;
}

/**
 * @brief Read data from the specified region of the NVMe device.
 * 
 * @return 0 on success, otherwise a negative errno. 
 * @note It's not necessary to check @buf alignment, because kernel will
 *  alloc for it again.
 */
int nvme_read_generic(int fd, enum nvme_region region, uint32_t oft, 
	uint32_t len, void *buf)
{
	struct nvme_access data;
	int ret;

	data.region = region;
	data.type = nvme_select_access_type(oft, len);
	data.buffer = buf;
	data.offset = oft;
	data.bytes = len;

	ret = ioctl(fd, NVME_IOCTL_READ_GENERIC, &data);
	if (ret < 0) {
		pr_err("failed to read data(0x%x:0x%x) from %s!(%d)\n", 
			oft, len, nvme_region_string(region), ret);
		return ret;
	}
	return 0;
}

/**
 * @brief Write data to the specified region of the NVMe device.
 * 
 * @return 0 on success, otherwise a negative errno. 
 * @note It's not necessary to check @buf alignment, because kernel will
 *  alloc for it again.
 */
int nvme_write_generic(int fd, enum nvme_region region, uint32_t oft, 
	uint32_t len, void *buf)
{
	struct nvme_access data;
	int ret;

	data.region = region;
	data.type = nvme_select_access_type(oft, len);
	data.buffer = buf;
	data.offset = oft;
	data.bytes = len;

	ret = ioctl(fd, NVME_IOCTL_WRITE_GENERIC, &data);
	if (ret < 0) {
		pr_err("failed to write data(0x%x:0x%x) to %s!(%d)\n", 
			oft, len, nvme_region_string(region), ret);
		return ret;
	}
	return 0;
}

int nvme_set_device_state(int fd, enum nvme_state state)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_SET_DEV_STATE, state);
	if (ret < 0) {
		pr_err("failed to %s!(%d)\n", nvme_state_string(state), ret);
		return ret;
	}
	return 0;
}

int nvme_create_asq(int fd, uint32_t elements)
{
	struct nvme_admin_queue asq;
	int ret;

	asq.type = NVME_ADMIN_SQ;
	asq.elements = elements;

	ret = ioctl(fd, NVME_IOCTL_CREATE_ADMIN_QUEUE, &asq);
	if (ret < 0) {
		pr_err("failed to create asq!(%d)\n", ret);
		return ret;
	}
	return 0;
}

int nvme_create_acq(int fd, uint32_t elements)
{
	struct nvme_admin_queue acq;
	int ret;

	acq.type = NVME_ADMIN_CQ;
	acq.elements = elements;

	ret = ioctl(fd, NVME_IOCTL_CREATE_ADMIN_QUEUE, &acq);
	if (ret < 0) {
		pr_err("failed to create acq!(%d)\n", ret);
		return ret;
	}
	return 0;
}
