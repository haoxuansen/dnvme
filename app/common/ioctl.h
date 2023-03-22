/**
 * @file ioctl.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-15
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _APP_IOCTL_H_
#define _APP_IOCTL_H_

#include <stdint.h>
#include "dnvme_ioctl.h"

int nvme_get_driver_info(int fd, struct nvme_driver *drv);
int nvme_get_device_info(int fd, struct nvme_dev_public *dev);

int nvme_get_capability(int fd, uint32_t id, void *buf, uint32_t size, 
	enum nvme_cap_type type);

static inline int nvme_get_pci_capability(int fd, uint32_t id, void *buf, 
	uint32_t size)
{
	return nvme_get_capability(fd, id, buf, size, NVME_CAP_TYPE_PCI);
}

static inline int nvme_get_pcie_capability(int fd, uint32_t id, void *buf, 
	uint32_t size)
{
	return nvme_get_capability(fd, id, buf, size, NVME_CAP_TYPE_PCIE);
}

int nvme_read_generic(int fd, enum nvme_region region, uint32_t oft, 
	uint32_t len, void *buf);
int nvme_write_generic(int fd, enum nvme_region region, uint32_t oft, 
	uint32_t len, void *buf);

static inline int nvme_read_ctrl_property(int fd, uint32_t oft, uint32_t len, 
	void *buf)
{
	return nvme_read_generic(fd, NVME_BAR0_BAR1, oft, len, buf);
}

static inline int nvme_read_ctrl_cap(int fd, uint64_t *val)
{
	return nvme_read_ctrl_property(fd, NVME_REG_CAP, 8, val);
}

static inline int nvme_read_ctrl_vs(int fd, uint32_t *val)
{
	return nvme_read_ctrl_property(fd, NVME_REG_VS, 4, val);
}

static inline int nvme_read_ctrl_cc(int fd, uint32_t *val)
{
	return nvme_read_ctrl_property(fd, NVME_REG_CC, 4, val);
}

static inline int nvme_write_ctrl_property(int fd, uint32_t oft, uint32_t len,
	void *buf)
{
	return nvme_write_generic(fd, NVME_BAR0_BAR1, oft, len, buf);
}

int nvme_set_device_state(int fd, enum nvme_state state);

static inline int nvme_enable_controller(int fd)
{
	return nvme_set_device_state(fd, NVME_ST_ENABLE);
}

static inline int nvme_disable_controller(int fd)
{
	return nvme_set_device_state(fd, NVME_ST_DISABLE);
}

static inline int nvme_disable_controller_complete(int fd)
{
	return nvme_set_device_state(fd, NVME_ST_DISABLE_COMPLETE);
}

static inline int nvme_reset_subsystem(int fd)
{
	return nvme_set_device_state(fd, NVME_ST_SUBSYSTEM_RESET);
}

int nvme_dump_log(int fd, char *file);

#endif /* !_APP_IOCTL_H_ */
