/**
 * @file irq.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-16
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ioctl.h>

#include "log.h"
#include "irq.h"
#include "ioctl.h"

static const char *nvme_irq_type_string(enum nvme_irq_type type)
{
	switch (type) {
	case NVME_INT_NONE:
		return "none";
	case NVME_INT_PIN:
		return "pin";
	case NVME_INT_MSI_SINGLE:
		return "msi single";
	case NVME_INT_MSI_MULTI:
		return "msi multiple";
	case NVME_INT_MSIX:
		return "msix";
	default:
		return "unknown";
	}
}

enum nvme_irq_type nvme_select_irq_type_random(void)
{
	uint8_t num = rand() % 4;

	switch (num) {
	case 0:
		return NVME_INT_PIN;
	case 1:
		return NVME_INT_MSI_SINGLE;
	case 2:
#ifdef AMD_MB_EN
		pr_warn("AMD MB may not support msi-multi, use msi-x replace!\n");
		return NVME_INT_MSIX;
#else
		return NVME_INT_MSI_MULTI;
#endif
	case 3:
		return NVME_INT_MSIX;
	default:
		return NVME_INT_NONE;
	}
}

int nvme_set_irq(int fd, enum nvme_irq_type type, uint16_t nr_irqs)
{
	struct nvme_interrupt irq;
	int ret;

	irq.irq_type = type;
	irq.num_irqs = nr_irqs;

	ret = ioctl(fd, NVME_IOCTL_SET_IRQ, &irq);
	if (ret < 0) {
		pr_err("failed to set %s with num %u!(%d)\n", 
			nvme_irq_type_string(type), nr_irqs, ret);
		return ret;
	}
	return 0;
}

/**
 * @brief Switch interrupt configuration.
 * 
 * @param fd NVMe device file descriptor
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_switch_irq(int fd, enum nvme_irq_type type, uint16_t nr_irqs)
{
	int ret;

	ret = nvme_disable_controller(fd);
	if (ret < 0)
		return ret;

	ret = nvme_set_irq(fd, type, nr_irqs);
	if (ret < 0)
		return ret;
	
	ret = nvme_enable_controller(fd);
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * @brief Mask specified irq number
 * 
 * @param fd NVMe device file descriptor
 * @param irq_no irq number
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_mask_irq(int fd, uint16_t irq_no)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_MASK_IRQ, irq_no);
	if (ret < 0) {
		pr_err("failed to mask irq %u!(%d)\n", irq_no, ret);
		return ret;
	}
	return 0;
}

/**
 * @brief Unmask specified irq number
 * 
 * @param fd NVMe device file descriptor
 * @param irq_no irq number
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_unmask_irq(int fd, uint16_t irq_no)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_UNMASK_IRQ, irq_no);
	if (ret < 0) {
		pr_err("failed to unmask irq %u!(%d)\n", irq_no, ret);
		return ret;
	}
	return 0;
}
