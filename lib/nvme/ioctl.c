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

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "libbase.h"
#include "libnvme.h"

static const char *nvme_state_string(enum nvme_state state)
{
	switch (state) {
	case NVME_ST_ENABLE:
		return "enable controller";
	case NVME_ST_DISABLE:
	case NVME_ST_DISABLE_COMPLETE:
		return "disable controller";
	case NVME_ST_SUBSYSTEM_RESET:
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

void *nvme_mmap(int fd, uint16_t id, uint32_t size, uint32_t type)
{
	size_t pg_size;
	size_t pgoff;
	void *addr;

	pg_size = sysconf(_SC_PAGE_SIZE);
	pgoff = NVME_VMPGOFF_FOR_TYPE(type) | id;

	addr = mmap(NULL, size, PROT_READ | PROT_WRITE, 
		MAP_SHARED, fd, pg_size * pgoff);
	if (MAP_FAILED == addr) {
		pr_err("failed to mmap addr!\n");
		return NULL;
	}

	return addr;
}

int nvme_get_pci_bdf(int fd, uint16_t *bdf)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_GET_PCI_BDF, bdf);
	if (ret < 0) {
		pr_err("failed to get pci bdf!(%d)\n", ret);
		return ret;
	}
	return 0;
}

int nvme_get_dev_info(int fd, struct nvme_dev_public *pub)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_GET_DEV_INFO, pub);
	if (ret < 0) {
		pr_err("failed to get dev info!(%d)\n", ret);
		return ret;
	}
	return 0;
}

int nvme_get_capability(int fd, uint32_t id, void *buf, uint32_t size, 
	enum nvme_cap_type type)
{
	int ret;

	struct nvme_get_cap gcap = {0};

	gcap.type = type;
	gcap.id = id;
	gcap.buf = buf;
	gcap.size = size;

	ret = ioctl(fd, NVME_IOCTL_GET_CAPABILITY, &gcap);
	if (ret < 0) {
		pr_err("failed to get capability!(%d)\n", ret);
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

int nvme_alloc_host_mem_buffer(int fd, struct nvme_hmb_alloc *alloc)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_ALLOC_HMB, alloc);
	if (ret < 0) {
		pr_err("failed to alloc hmb!(%d)\n", ret);
		return ret;
	}
	return 0;
}

int nvme_release_host_mem_buffer(int fd)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_RELEASE_HMB);
	if (ret < 0) {
		pr_err("failed to release hmb!(%d)\n", ret);
		return ret;
	}
	return 0;
}

