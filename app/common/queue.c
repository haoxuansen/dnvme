/**
 * @file queue.c
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
#include "queue.h"

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

int nvme_prepare_iosq(int fd, uint16_t sqid, uint16_t cqid, uint32_t elements, 
	uint8_t contig)
{
	struct nvme_prep_sq psq;
	int ret;

	psq.sq_id = sqid;
	psq.cq_id = cqid;
	psq.elements = elements;
	psq.contig = contig;

	ret = ioctl(fd, NVME_IOCTL_PREPARE_IOSQ, &psq);
	if (ret < 0) {
		pr_err("failed to prepare iosq %u!(%d)\n", sqid, ret);
		return ret;
	}
	return 0;
}

int nvme_prepare_iocq(int fd, uint16_t cqid, uint32_t elements, uint8_t contig,
	uint8_t irq_en, uint16_t irq_no)
{
	struct nvme_prep_cq pcq;
	int ret;

	pcq.cq_id = cqid;
	pcq.elements = elements;
	pcq.contig = contig;
	pcq.cq_irq_en = irq_en;
	pcq.cq_irq_no = irq_no;

	ret = ioctl(fd, NVME_IOCTL_PREPARE_IOCQ, &pcq);
	if (ret < 0) {
		pr_err("failed to prepare iocq %u!(%d)\n", cqid, ret);
		return ret;
	}
	return 0;
}
