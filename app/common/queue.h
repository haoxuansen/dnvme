/**
 * @file queue.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-16
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _APP_QUEUE_H_
#define _APP_QUEUE_H_

#include <stdint.h>

#include "dnvme_ioctl.h"

int nvme_get_sq_info(int fd, struct nvme_sq_public *sq);
int nvme_get_cq_info(int fd, struct nvme_cq_public *cq);

int nvme_create_asq(int fd, uint32_t elements);
int nvme_create_acq(int fd, uint32_t elements);

int nvme_prepare_iosq(int fd, uint16_t sqid, uint16_t cqid, uint32_t elements, 
	uint8_t contig);
int nvme_prepare_iocq(int fd, uint16_t cqid, uint32_t elements, uint8_t contig,
	uint8_t irq_en, uint16_t irq_no);

int nvme_submit_64b_cmd(int fd, struct nvme_64b_cmd *cmd);

int nvme_inquiry_cq_entries(int fd, uint16_t cqid);
int nvme_reap_cq_entries(int fd, struct nvme_reap *rp);

int nvme_ring_sq_doorbell(int fd, uint16_t sqid);

#endif /* !_APP_QUEUE_H_ */
