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

int nvme_read_generic(int fd, enum nvme_region region, uint32_t oft, 
	uint32_t len, void *buf);
int nvme_write_generic(int fd, enum nvme_region region, uint32_t oft, 
	uint32_t len, void *buf);

static inline int nvme_read_ctrl_property(int fd, uint32_t oft, uint32_t len, 
	void *buf)
{
	return nvme_read_generic(fd, NVME_BAR0_BAR1, oft, len, buf);
}

static inline int nvme_write_ctrl_property(int fd, uint32_t oft, uint32_t len,
	void *buf)
{
	return nvme_write_generic(fd, NVME_BAR0_BAR1, oft, len, buf);
}

#endif /* !_APP_IOCTL_H_ */
