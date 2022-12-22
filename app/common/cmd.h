/**
 * @file cmd.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-19
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _APP_CMD_H_
#define _APP_CMD_H_

#include <stdint.h>

#include "byteorder.h"
#include "dnvme_ioctl.h"

static inline void nvme_cmd_fill_create_sq(struct nvme_create_sq *csq, 
	uint16_t sqid, uint16_t cqid, uint32_t elements, uint8_t contig,
	uint16_t prio)
{
	csq->opcode = nvme_admin_create_sq;
	csq->sqid = cpu_to_le16(sqid);
	csq->qsize = cpu_to_le16(elements - 1); /* 0's based */
	if (contig)
		csq->sq_flags |= cpu_to_le16(NVME_CSQ_F_PC);
	csq->sq_flags |= cpu_to_le16(prio);
	csq->cqid = cpu_to_le16(cqid);
}

static inline void nvme_cmd_fill_create_cq(struct nvme_create_cq *ccq, 
	uint16_t cqid, uint32_t elements, uint8_t contig, uint8_t irq_en,
	uint16_t irq_no)
{
	ccq->opcode = nvme_admin_create_cq;
	ccq->cqid = cpu_to_le16(cqid);
	ccq->qsize = cpu_to_le16(elements - 1); /* 0's based */
	if (contig)
		ccq->cq_flags |= cpu_to_le16(NVME_CCQ_F_PC);
	if (irq_en)
		ccq->cq_flags |= cpu_to_le16(NVME_CCQ_F_IEN);
	ccq->irq_vector = cpu_to_le16(irq_no);
}

int nvme_submit_64b_cmd(int fd, struct nvme_64b_cmd *cmd);

int nvme_cmd_create_iosq(int fd, struct nvme_create_sq *csq, uint8_t contig,
	void *buf, uint32_t size);
int nvme_cmd_create_iocq(int fd, struct nvme_create_cq *ccq, uint8_t contig,
	void *buf, uint32_t size);

#endif /* !_APP_CMD_H_ */
