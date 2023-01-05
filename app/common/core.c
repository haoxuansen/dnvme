/**
 * @file core.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-04
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <errno.h>

#include "log.h"
#include "ioctl.h"
#include "queue.h"
#include "irq.h"
#include "core.h"

int nvme_reinit(struct nvme_dev_info *ndev, uint32_t asqsz, uint32_t acqsz, 
	enum nvme_irq_type type)
{
	uint16_t nr_cq = ndev->max_cq_num + 1; /* ACQ + IOCQ */
	uint16_t nr_irq;
	int ret;

	ret = nvme_disable_controller_complete(ndev->fd);
	if (ret < 0)
		return ret;

	ret = nvme_create_aq_pair(ndev->fd, asqsz, acqsz);
	if (ret < 0)
		return ret;

	if (type == NVME_INT_PIN || type == NVME_INT_MSI_SINGLE) {
		nr_irq = 1;
	} else if (type == NVME_INT_MSI_MULTI) {
		/* !TODO: It's better to check irq limit by parsing MSI Cap */
		nr_irq = nr_cq > 32 ? 32 : nr_cq;
	} else if (type == NVME_INT_MSIX) {
		/* !TODO: It's better to check irq limit by parsing MSI-X Cap */
		nr_irq = nr_cq > 2048 ? 2048 : nr_cq;
	}

	ret = nvme_set_irq(ndev->fd, type, nr_irq);
	if (ret < 0) {
		ndev->irq_type = NVME_INT_NONE;
		ndev->nr_irq = 0;
		return ret;
	}
	ndev->irq_type = type;
	ndev->nr_irq = nr_irq;

	ret = nvme_enable_controller(ndev->fd);
	if (ret < 0)
		return ret;

	return 0;
}
