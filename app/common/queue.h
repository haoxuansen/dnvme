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

/* Create IO Queue Flags */
#define NVME_CIOQ_F_CQS_BIND_SINGLE_IRQ	(1 << 0)
#define NVME_CIOQ_F_SQS_BIND_SINGLE_CQ	(1 << 1)

struct nvme_dev_info;

/**
 * @brief For create submission queue
 */
struct nvme_csq_wrapper {
	uint16_t	sqid;
	uint16_t	cqid;
	uint32_t	elements;
	uint16_t	prio;
	uint8_t		contig;
	void		*buf;
	uint32_t	size;
};

/**
 * @brief For create completion queue
 */
struct nvme_ccq_wrapper {
	uint16_t	cqid;
	uint32_t	elements;
	uint16_t	irq_no;
	uint8_t		irq_en;
	uint8_t		contig;
	void		*buf;
	uint32_t	size;
};

struct nvme_sq_info {
	uint16_t	sqid;
	uint16_t	cqid;
	uint32_t	size;

	uint16_t	cmd_cnt;
};

struct nvme_cq_info {
	uint16_t	cqid;
	uint16_t	irq_no;
	uint8_t		irq_en;
	uint32_t	size;
};

static inline void nvme_fill_prep_sq(struct nvme_prep_sq *psq, uint16_t sqid,
	uint16_t cqid, uint32_t elements, uint8_t contig)
{
	psq->sq_id = sqid;
	psq->cq_id = cqid;
	psq->elements = elements;
	psq->contig = contig;
}

static inline void nvme_fill_prep_cq(struct nvme_prep_cq *pcq, uint16_t cqid, 
	uint32_t elements, uint8_t contig, uint8_t irq_en, uint16_t irq_no)
{
	pcq->cq_id = cqid;
	pcq->elements = elements;
	pcq->contig = contig;
	pcq->cq_irq_en = irq_en;
	pcq->cq_irq_no = irq_no;
}

int nvme_get_sq_info(int fd, struct nvme_sq_public *sq);
int nvme_get_cq_info(int fd, struct nvme_cq_public *cq);

int nvme_create_asq(int fd, uint32_t elements);
int nvme_create_acq(int fd, uint32_t elements);
int nvme_create_aq_pair(int fd, uint32_t sqsz, uint32_t cqsz);

int nvme_prepare_iosq(int fd, struct nvme_prep_sq *psq);
int nvme_prepare_iocq(int fd, struct nvme_prep_cq *pcq);

int nvme_create_iosq(int fd, struct nvme_csq_wrapper *wrap);
int nvme_create_iocq(int fd, struct nvme_ccq_wrapper *wrap);

int nvme_delete_iosq(int fd, uint16_t sqid);
int nvme_delete_iocq(int fd, uint16_t cqid);

int nvme_delete_all_iosq(int fd, struct nvme_sq_info *sqs, uint16_t nr_sq);
int nvme_delete_all_iocq(int fd, struct nvme_cq_info *cqs, uint16_t nr_cq);

int nvme_create_all_ioq(struct nvme_dev_info *ndev, uint32_t flag);
int nvme_delete_all_ioq(struct nvme_dev_info *ndev);

int nvme_inquiry_cq_entries(int fd, uint16_t cqid);
int nvme_reap_cq_entries(int fd, struct nvme_reap *rp);
int nvme_reap_expect_cqe(int fd, uint16_t cqid, uint32_t expect, void *buf, 
	uint32_t size);

int nvme_valid_cq_entry(struct nvme_completion *entry, uint16_t sqid, 
	uint16_t cid, uint16_t status);
int nvme_check_cq_entries(struct nvme_completion *entries, uint32_t num);

int nvme_ring_sq_doorbell(int fd, uint16_t sqid);

int nvme_init_ioq_info(struct nvme_dev_info *ndev);
void nvme_deinit_ioq_info(struct nvme_dev_info *ndev);

void nvme_swap_ioq_info_random(struct nvme_dev_info *ndev);
void nvme_reinit_ioq_info_random(struct nvme_dev_info *ndev);

struct nvme_sq_info *nvme_find_iosq_info(struct nvme_dev_info *ndev, 
	uint16_t sqid);
struct nvme_cq_info *nvme_find_iocq_info(struct nvme_dev_info *ndev, 
	uint16_t cqid);

#endif /* !_APP_QUEUE_H_ */
