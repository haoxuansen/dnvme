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
#include <stdint.h>
#include <sys/ioctl.h>

#include "log.h"
#include "irq.h"

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
