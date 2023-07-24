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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "byteorder.h"
#include "libbase.h"
#include "libnvme.h"

int nvme_get_sq_info(int fd, struct nvme_sq_public *sq)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_GET_SQ_INFO, sq);
	if (ret < 0) {
		pr_err("failed to get SQ(%u) info!(%d)\n", sq->sq_id, ret);
		return ret;
	}
	return 0;
}

int nvme_get_cq_info(int fd, struct nvme_cq_public *cq)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_GET_CQ_INFO, cq);
	if (ret < 0) {
		pr_err("failed to get CQ(%u) info!(%d)\n", cq->q_id, ret);
		return ret;
	}
	return 0;
}

/**
 * @brief Create ASQ and update ASQ info to @ndev
 * 
 * @param elements The number of entries
 * @return 0 on success, otherwise a negative errno
 */
int nvme_create_asq(struct nvme_dev_info *ndev, uint32_t elements)
{
	struct nvme_admin_queue asq;
	int ret;

	asq.type = NVME_ADMIN_SQ;
	asq.elements = elements;

	ret = ioctl(ndev->fd, NVME_IOCTL_CREATE_ADMIN_QUEUE, &asq);
	if (ret < 0) {
		pr_err("failed to create asq!(%d)\n", ret);
		return ret;
	}
	ndev->asq.sqid = NVME_AQ_ID;
	ndev->asq.cqid = NVME_AQ_ID;
	ndev->asq.size = elements;
	return 0;
}

/**
 * @brief Create ACQ and update ACQ info to @ndev
 * 
 * @param elements The number of entries
 * @return 0 on success, otherwise a negative errno
 */
int nvme_create_acq(struct nvme_dev_info *ndev, uint32_t elements)
{
	struct nvme_admin_queue acq;
	int ret;

	acq.type = NVME_ADMIN_CQ;
	acq.elements = elements;

	ret = ioctl(ndev->fd, NVME_IOCTL_CREATE_ADMIN_QUEUE, &acq);
	if (ret < 0) {
		pr_err("failed to create acq!(%d)\n", ret);
		return ret;
	}
	ndev->acq.cqid = NVME_AQ_ID;
	ndev->acq.size = elements;
	return 0;
}

/**
 * @brief Create ASQ & ACQ, update ASQ & ACQ info to @ndev
 * 
 * @param sqsz Specify the size of ASQ in entries.
 * @param cqsz Specify the size of ACQ in entries.
 * @return 0 on success, otherwise a negative errno
 */
int nvme_create_aq_pair(struct nvme_dev_info *ndev, uint32_t sqsz, uint32_t cqsz)
{
	int ret;

	ret = nvme_create_acq(ndev, cqsz);
	if (ret < 0)
		return ret;
	ret = nvme_create_asq(ndev, sqsz);
	if (ret < 0)
		return ret;
	return 0;
}

int nvme_prepare_iosq(int fd, struct nvme_prep_sq *psq)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_PREPARE_IOSQ, psq);
	if (ret < 0) {
		pr_err("failed to prepare iosq %u!(%d)\n", psq->sq_id, ret);
		return ret;
	}
	return 0;
}

int nvme_prepare_iocq(int fd, struct nvme_prep_cq *pcq)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_PREPARE_IOCQ, pcq);
	if (ret < 0) {
		pr_err("failed to prepare iocq %u!(%d)\n", pcq->cq_id, ret);
		return ret;
	}
	return 0;
}

int nvme_create_iosq(struct nvme_dev_info *ndev, struct nvme_csq_wrapper *wrap)
{
	struct nvme_prep_sq psq = {0};
	struct nvme_create_sq csq = {0};
	struct nvme_completion entry = {0};
	int fd = ndev->fd;
	uint16_t cid;
	int ret;

	nvme_fill_prep_sq(&psq, wrap->sqid, wrap->cqid, wrap->elements, 
		wrap->contig);
	ret = nvme_prepare_iosq(fd, &psq);
	if (ret < 0)
		return ret;
	
	nvme_cmd_fill_create_sq(&csq, wrap->sqid, wrap->cqid, wrap->elements,
		wrap->contig, wrap->prio);
	ret = nvme_cmd_create_iosq(fd, &csq, wrap->contig, wrap->buf, wrap->size);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, 0);
	if (ret < 0)
		return ret;

	return 0;
}

int nvme_delete_iosq(struct nvme_dev_info *ndev, uint16_t sqid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int fd = ndev->fd;
	int ret;

	ret = nvme_cmd_delete_iosq(fd, sqid);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, 0);
	if (ret < 0)
		return ret;
	
	return 0;
}

int nvme_create_all_iosq(struct nvme_dev_info *ndev, struct nvme_sq_info *sqs, 
	uint16_t nr_sq)
{
	struct nvme_csq_wrapper wrap = {0};
	uint16_t i;
	int ret;
	
	for (i = 0; i < nr_sq; i++) {
		wrap.sqid = sqs[i].sqid;
		wrap.cqid = sqs[i].cqid;
		wrap.elements = sqs[i].size;
		wrap.prio = NVME_SQ_PRIO_MEDIUM;
		wrap.contig = 1;

		ret = nvme_create_iosq(ndev, &wrap);
		if (ret < 0) {
			pr_err("failed to create iosq(%u)!(%d)\n",
				sqs[i].sqid, ret);
			return ret;
		}
	}
	return 0;
}

/**
 * @brief Delete all IOSQs.
 * 
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_delete_all_iosq(struct nvme_dev_info *ndev, struct nvme_sq_info *sqs, 
	uint16_t nr_sq)
{
	uint16_t i;
	int ret;

	for (i = 0; i < nr_sq; i++) {
		ret = nvme_delete_iosq(ndev, sqs[i].sqid);
		if (ret < 0) {
			pr_err("failed to delete iosq(%u)!(%d)\n", 
				sqs[i].sqid, ret);
			return ret;
		}
	}
	return 0;
}

/**
 * @brief Create a I/O completion queue
 * 
 * @param fd NVMe device file descriptor
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_create_iocq(struct nvme_dev_info *ndev, struct nvme_ccq_wrapper *wrap)
{
	struct nvme_prep_cq pcq = {0};
	struct nvme_create_cq ccq = {0};
	struct nvme_completion entry = {0};
	uint16_t cid;
	int fd = ndev->fd;
	int ret;

	nvme_fill_prep_cq(&pcq, wrap->cqid, wrap->elements, wrap->contig, 
		wrap->irq_en, wrap->irq_no);
	ret = nvme_prepare_iocq(fd, &pcq);
	if (ret < 0)
		return ret;

	nvme_cmd_fill_create_cq(&ccq, wrap->cqid, wrap->elements, wrap->contig,
		wrap->irq_en, wrap->irq_no);
	ret = nvme_cmd_create_iocq(fd, &ccq, wrap->contig, wrap->buf, wrap->size);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, 0);
	if (ret < 0)
		return ret;

	return 0;
}

int nvme_delete_iocq(struct nvme_dev_info *ndev, uint16_t cqid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int fd = ndev->fd;
	int ret;

	ret = nvme_cmd_delete_iocq(fd, cqid);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, 0);
	if (ret < 0)
		return ret;
	
	return 0;
}

int nvme_create_all_iocq(struct nvme_dev_info *ndev, struct nvme_cq_info *cqs, 
	uint16_t nr_cq)
{
	struct nvme_ccq_wrapper wrap = {0};
	uint16_t i;
	int ret;

	for (i = 0; i < nr_cq; i++) {
		wrap.cqid = cqs[i].cqid;
		wrap.elements = cqs[i].size;
		wrap.irq_no = cqs[i].irq_no;
		wrap.irq_en = cqs[i].irq_en;
		wrap.contig = 1;

		ret = nvme_create_iocq(ndev, &wrap);
		if (ret < 0) {
			pr_err("failed to create iocq(%u)!(%d)\n",
				cqs[i].cqid, ret);
			return ret;
		}
	}
	return 0;
}

/**
 * @brief Delete all IOCQs.
 * 
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_delete_all_iocq(struct nvme_dev_info *ndev, struct nvme_cq_info *cqs, 
	uint16_t nr_cq)
{
	uint16_t i;
	int ret;

	for (i = 0; i < nr_cq; i++) {
		ret = nvme_delete_iocq(ndev, cqs[i].cqid);
		if (ret < 0) {
			pr_err("failed to delete iocq(%u)!(%d)\n", 
				cqs[i].cqid, ret);
			return ret;
		}
	}
	return 0;
}

int nvme_create_all_ioq(struct nvme_dev_info *ndev, uint32_t flag)
{
	struct nvme_cq_info *cqs = ndev->iocqs;
	uint16_t irq_no;
	uint16_t i;
	int ret;

	// nvme_reinit_ioq_info_random(ndev);
	nvme_swap_ioq_info_random(ndev);

	if (flag & NVME_CIOQ_F_CQS_BIND_SINGLE_IRQ) {
		if (ndev->irq_type == NVME_INT_NONE || ndev->nr_irq == 0) {
			pr_err("irq_type is none or nr_irq is zero!\n");
			return -EPERM;
		}
		irq_no = rand() % ndev->nr_irq;

		for (i = 0; i < ndev->max_cq_num; i++)
			cqs[i].irq_no = irq_no;
	}

	ret = nvme_create_all_iocq(ndev, ndev->iocqs, ndev->max_cq_num);
	if (ret < 0)
		return ret;
	
	ret = nvme_create_all_iosq(ndev, ndev->iosqs, ndev->max_sq_num);
	if (ret < 0)
		return ret;

	return 0;
}

int nvme_delete_all_ioq(struct nvme_dev_info *ndev)
{
	int ret;

	ret = nvme_delete_all_iosq(ndev, ndev->iosqs, ndev->max_sq_num);
	if (ret < 0)
		return ret;
	
	ret = nvme_delete_all_iocq(ndev, ndev->iocqs, ndev->max_cq_num);
	if (ret < 0)
		return ret;
	
	return 0;
}

static void nvme_display_cq_entry(struct nvme_completion *entry)
{
	pr_bub("SQ:%u, CMD:0x%x, Head:0x%x, P:%u, Status:%u|%u|%u|0x%x|0x%x,"
		" QWord:0x%llx\n",
		le16_to_cpu(entry->sq_id), entry->command_id, 
		le16_to_cpu(entry->sq_head), 
		NVME_CQE_STATUS_TO_PHASE(entry->status), 
		NVME_CQE_STATUS_TO_DNR(entry->status),
		NVME_CQE_STATUS_TO_M(entry->status),
		NVME_CQE_STATUS_TO_CRD(entry->status),
		NVME_CQE_STATUS_TO_SCT(entry->status),
		NVME_CQE_STATUS_TO_SC(entry->status),
		le64_to_cpu(entry->result.u64));
}

struct nvme_completion *nvme_find_cq_entry(struct nvme_completion *entries, 
	uint32_t num, uint16_t cid)
{
	struct nvme_completion *entry;
	uint32_t i;

	for (i = 0; i < num; i++) {
		entry = &entries[i];

		if (entry->command_id == cid)
			return entry;
	}
	return NULL;
}

/**
 * @brief Validate CQ entry
 * 
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_valid_cq_entry(struct nvme_completion *entry, uint16_t sqid, 
	uint16_t cid, uint16_t status)
{
	if (le16_to_cpu(entry->sq_id) != sqid) {
		pr_err("SQ:%u vs %u\n", sqid, le16_to_cpu(entry->sq_id));
		return -EINVAL;
	}

	if (entry->command_id != cid) {
		pr_err("CID:%u vs %u\n", cid, entry->command_id);
		return -EINVAL;
	}

	if (NVME_CQE_STATUS_TO_STATE(entry->status) != status) {
		pr_err("status:0x%x vs 0x%x\n", status, 
			NVME_CQE_STATUS_TO_STATE(entry->status));
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief Check whether the status of entries is correct
 * 
 * @param entries pointer to the array holding entry
 * @param num The number of entries required to be check
 * @return 0 if check ok, otherwise a negative errno.
 */
int nvme_check_cq_entries(struct nvme_completion *entries, uint32_t num)
{
	struct nvme_completion *entry;
	int ret = 0;
	uint32_t i;

	for (i = 0; i < num; i++) {
		entry = &entries[i];

		if (NVME_CQE_STATUS_TO_STATE(entry->status)) {
			pr_warn("SQ:%u, CMD:0x%x, Head:0x%x, Status:0x%x\n",
				le16_to_cpu(entry->sq_id), entry->command_id, 
				le16_to_cpu(entry->sq_head), 
				le16_to_cpu(entry->status));
			ret--;
		}
		nvme_display_cq_entry(entry);
	}
	return ret;
}

/**
 * @brief Inquiry the number of CQ entries to be processed in the specified SQ
 * 
 * @param fd NVMe device file descriptor
 * @param cqid Completion Queue Identify
 * @return The number of CQ entries ready to be processed if success,
 *  otherwise a negative errno.
 */
int nvme_inquiry_cq_entries(int fd, uint16_t cqid)
{
	struct nvme_inquiry inq = {0};
	int ret;

	inq.cqid = cqid;

	ret = ioctl(fd, NVME_IOCTL_INQUIRY_CQE, &inq);
	if (ret < 0) {
		pr_err("failed to inquiry CQ(%u)!(%d)\n", cqid, ret);
		return ret;
	}

	return (int)inq.nr_cqe;
}

int nvme_reap_cq_entries(int fd, struct nvme_reap *rp)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_REAP_CQE, rp);
	if (ret < 0) {
		pr_err("failed to reap CQ(%u)!(%d)\n", rp->cqid, ret);
		return ret;
	}
	return 0;
}

int nvme_ring_sq_doorbell(int fd, uint16_t sqid)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_RING_SQ_DOORBELL, sqid);
	if (ret < 0) {
		pr_err("failed to ring SQ(%u)!\n", sqid);
		return ret;
	}
	return 0;
}

int nvme_empty_sq_cmdlist(int fd, uint16_t sqid)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_EMPTY_CMD_LIST, sqid);
	if (ret < 0) {
		pr_err("failed to empty SQ(%u)!(%d)\n", sqid, ret);
		return ret;
	}
	return 0;
}

static int nvme_alloc_iosq_info(struct nvme_dev_info *ndev)
{
	struct nvme_sq_info *sq;
	struct nvme_ctrl_property *prop = &ndev->prop;
	uint16_t nr_sq = ndev->max_sq_num;
	uint16_t mqes = NVME_CAP_MQES(prop->cap);
	uint16_t qid;

	sq = calloc(nr_sq, sizeof(*sq));
	if (!sq) {
		pr_err("failed to alloc iosq info!(nr:%u)\n", nr_sq);
		return -ENOMEM;
	}

	for (qid = 1; qid <= nr_sq; qid++) {
		sq[qid - 1].sqid = qid;
		sq[qid - 1].cqid = qid;
		sq[qid - 1].size = rand() % (mqes - 512) + 512;
	}

	ndev->iosqs = sq;
	return 0;
}

static int nvme_alloc_iocq_info(struct nvme_dev_info *ndev)
{
	struct nvme_cq_info *cq;
	struct nvme_ctrl_property *prop = &ndev->prop;
	uint16_t nr_cq = ndev->max_cq_num;
	uint16_t mqes = NVME_CAP_MQES(prop->cap);
	uint16_t qid;

	cq = calloc(nr_cq, sizeof(*cq));
	if (!cq) {
		pr_err("failed to alloc iocq info!(nr:%u)\n", nr_cq);
		return -ENOMEM;
	}

	for (qid = 1; qid <= nr_cq; qid++) {
		cq[qid - 1].cqid = qid;
		cq[qid - 1].irq_no = qid;
		cq[qid - 1].size = rand() % (mqes - 512) + 512;
	}

	ndev->iocqs = cq;
	return 0;
}

int nvme_init_ioq_info(struct nvme_dev_info *ndev)
{
	int ret;

	ret = nvme_alloc_iosq_info(ndev);
	if (ret < 0)
		return ret;
	
	ret = nvme_alloc_iocq_info(ndev);
	if (ret < 0)
		goto out;

	return 0;
out:
	free(ndev->iosqs);
	ndev->iosqs = NULL;
	return ret;
}

void nvme_deinit_ioq_info(struct nvme_dev_info *ndev)
{
	free(ndev->iosqs);
	ndev->iosqs = NULL;
	free(ndev->iocqs);
	ndev->iocqs = NULL;
}

struct nvme_sq_info *nvme_find_iosq_info(struct nvme_dev_info *ndev, 
	uint16_t sqid)
{
	if (sqid > ndev->max_sq_num)
		return NULL;
	
	if (ndev->iosqs[sqid - 1].sqid != sqid)
		return NULL;

	return &ndev->iosqs[sqid - 1];
}

struct nvme_cq_info *nvme_find_iocq_info(struct nvme_dev_info *ndev, 
	uint16_t cqid)
{
	if (cqid > ndev->max_cq_num)
		return NULL;
	
	if (ndev->iocqs[cqid - 1].cqid != cqid)
		return NULL;
	
	return &ndev->iocqs[cqid - 1];
}

void nvme_reinit_ioq_info_random(struct nvme_dev_info *ndev)
{
	struct nvme_sq_info *sqs = ndev->iosqs;
	struct nvme_cq_info *cqs = ndev->iocqs;
	struct nvme_ctrl_property *prop = &ndev->prop;
	uint16_t mqes = NVME_CAP_MQES(prop->cap);
	uint16_t nr_cq = ndev->max_cq_num;
	uint16_t nr_sq = ndev->max_sq_num;
	uint16_t i;

	for (i = 0; i < nr_sq; i++) {
		sqs[i].cqid = rand() % nr_cq + 1; /* cqid range: 1 ~ nr_cq */
		sqs[i].size = rand() % (mqes - 512) + 512;

		pr_debug("SQ:%u, element:%u, bind CQ:%u\n",
			sqs[i].sqid, sqs[i].size, sqs[i].cqid);
	}

	for (i = 0; i < nr_cq; i++) {
		if (ndev->irq_type == NVME_INT_NONE || ndev->nr_irq == 0) {
			cqs[i].irq_en = 0;
		} else {
			cqs[i].irq_en = 1;
			cqs[i].irq_no = rand() % ndev->nr_irq;
		}
		cqs[i].size = rand() % (mqes - 512) + 512;

		pr_debug("CQ:%u, element:%u, irq_en:%u, irq_no:%u\n",
			cqs[i].cqid, cqs[i].size, cqs[i].irq_en, cqs[i].irq_no);
	}
}

static void nvme_swap_iosq_info_random(struct nvme_sq_info *sqs, uint16_t nr_sq)
{
	struct nvme_sq_info tmp;
	uint16_t i, num;

	/* The last sq info has been exchanged with the previous one */
	for (i = 0; i < (nr_sq - 1); i++) {
		num = i + rand() % (nr_sq - i);
		tmp.cqid = sqs[i].cqid;
		sqs[i].cqid = sqs[num].cqid;
		sqs[num].cqid = tmp.cqid;

		num = i + rand() % (nr_sq - i);
		tmp.size = sqs[i].size;
		sqs[i].size = sqs[num].size;
		sqs[num].size = tmp.size;
	}
}

static void nvme_swap_iocq_info_random(struct nvme_cq_info *cqs, uint16_t nr_cq, 
	uint16_t nr_irq)
{
	struct nvme_cq_info tmp;
	uint16_t i, num;

	for (i = 0; i < nr_cq; i++) {
		if (nr_irq) {
			cqs[i].irq_no = rand() % nr_irq;
			cqs[i].irq_en = 1;
		} else {
			cqs[i].irq_en = 0;
		}
	}

	/* The last cq info has been exchanged with the previous one */
	for (i = 0; i < (nr_cq - 1); i++) {
		num = i + rand() % (nr_cq - i);
		tmp.size = cqs[i].size;
		cqs[i].size = cqs[num].size;
		cqs[num].size = tmp.size;
	}
}

void nvme_swap_ioq_info_random(struct nvme_dev_info *ndev)
{
	nvme_swap_iosq_info_random(ndev->iosqs, ndev->max_sq_num);
	nvme_swap_iocq_info_random(ndev->iocqs, ndev->max_cq_num, ndev->nr_irq);
}
