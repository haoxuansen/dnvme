/**
 * @file queue.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-04-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _DNVME_QUEUE_H_
#define _DNVME_QUEUE_H_

#include "core.h"

/**
 * @breif Check whether the SQ is full
 *
 * @return 1 if SQ is full, otherwise return 0.
 */
static inline int dnvme_sq_is_full(struct nvme_sq *sq)
{
	return (((u32)sq->pub.tail_ptr_virt + 1) % sq->pub.elements) ==
		sq->pub.head_ptr ? 1 : 0;
}

/**
 * @breif Check whether the SQ is empty
 *
 * @return 1 if SQ is empty, otherwise return 0.
 */
static inline int dnvme_sq_is_empty(struct nvme_sq *sq)
{
	return sq->pub.tail_ptr_virt == sq->pub.head_ptr ? 1 : 0;
}

/**
 * @brief Find the SQ node by the given SQID
 * 
 * @param id submission queue identify
 * @return pointer to the SQ node on success. Otherwise returns NULL.
 */
static inline struct nvme_sq *dnvme_find_sq(struct nvme_device *ndev, u16 id)
{
	return xa_load(&ndev->sqs, id);
}

/**
 * @brief Find the CQ node by the given CQID
 * 
 * @param id completion queue identify
 * @return pointer to the CQ node on success. Otherwise returns NULL.
 */
static inline struct nvme_cq *dnvme_find_cq(struct nvme_device *ndev, u16 id)
{
	return xa_load(&ndev->cqs, id);
}

/**
 * @brief Find the meta node by the given ID
 * 
 * @param id meta identify
 * @return pointer to the meta node on success. Otherwise returns NULL.
 */
static inline struct nvme_meta *dnvme_find_meta(struct nvme_device *ndev, u32 id)
{
	return xa_load(&ndev->meta, id);
}


int dnvme_check_qid_unique(struct nvme_device *ndev, 
	enum nvme_queue_type type, u16 id);

struct nvme_cmd *dnvme_find_cmd(struct nvme_sq *sq, u16 id);
struct nvme_sq *dnvme_alloc_sq(struct nvme_device *ndev, 
	struct nvme_prep_sq *prep, u8 sqes);
void dnvme_release_sq(struct nvme_device *ndev, struct nvme_sq *sq);

struct nvme_cq *dnvme_alloc_cq(struct nvme_device *ndev, 
	struct nvme_prep_cq *prep, u8 cqes);
void dnvme_release_cq(struct nvme_device *ndev, struct nvme_cq *sq);

int dnvme_create_asq(struct nvme_device *ndev, u32 elements);
int dnvme_create_acq(struct nvme_device *ndev, u32 elements);

void dnvme_delete_all_queues(struct nvme_device *ndev, enum nvme_state state);

int dnvme_ring_sq_doorbell(struct nvme_device *ndev, u16 sq_id);

u32 dnvme_get_cqe_remain(struct nvme_cq *cq, struct device *dev);
int dnvme_inquiry_cqe(struct nvme_device *ndev, struct nvme_inquiry __user *uinq);

int dnvme_reap_cqe(struct nvme_cq *cq, u32 expect, void __user *buf, u32 size);
int dnvme_reap_cqe_legacy(struct nvme_device *ndev, struct nvme_reap __user *ureap);

#endif /* !_DNVME_QUEUE_H_ */
