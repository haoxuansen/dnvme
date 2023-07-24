/**
 * @file pcie.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UAPI_LIB_NVME_PCIE_H_
#define _UAPI_LIB_NVME_PCIE_H_

#include <stdint.h>
#include "ioctl.h"

static inline int pcie_do_flr(int fd)
{
	return nvme_set_device_state(fd, NVME_ST_PCIE_FLR_RESET);
}

static inline int pcie_do_hot_reset(int fd)
{
	return nvme_set_device_state(fd, NVME_ST_PCIE_HOT_RESET);
}

static inline int pcie_do_link_down(int fd)
{
	return nvme_set_device_state(fd, NVME_ST_PCIE_LINKDOWN_RESET);
}

int pcie_retrain_link(char *lc);

int pcie_set_power_state(int fd, uint8_t pmcap, uint16_t ps);

#endif /* !_UAPI_LIB_NVME_PCIE_H_ */
