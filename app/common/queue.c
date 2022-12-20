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
#include <stdint.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "byteorder.h"
#include "log.h"
#include "queue.h"
#include "cmd.h"

#define NVME_REAP_CQ_TIMEUNIT		100 /* us */
/* 10s */
#define NVME_REAP_CQ_TIMEOUT		(1000 * 1000 * 10 / NVME_REAP_CQ_TIMEUNIT)

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

#if 0
int nvme_create_iocq(int fd, uint16_t cqid, uint32_t elements, uint8_t contig,
	uint8_t irq_en, uint16_t irq_no)
{
	int ret;

	ret = nvme_prepare_iocq(fd, cqid, elements, contig, irq_en, irq_no);
	if (ret < 0)
		return ret;
	
	ret = nvme_cmd_create_iocq(fd, contig, );

	// !TODO: now
	return 0;
}
#endif

static void nvme_display_cq_entry(struct nvme_completion *entry)
{
	pr_debug("SQ:%u, CMD:0x%x, Head:0x%x, P:%u, Status:%u|%u|%u|0x%x|0x%x,"
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

	inq.q_id = cqid;

	ret = ioctl(fd, NVME_IOCTL_INQUIRY_CQE, &inq);
	if (ret < 0) {
		pr_err("failed to inquiry CQ(%u)!(%d)\n", cqid, ret);
		return ret;
	}

	return (int)inq.num_remaining;
}

int nvme_reap_cq_entries(int fd, struct nvme_reap *rp)
{
	int ret;

	ret = ioctl(fd, NVME_IOCTL_REAP_CQE, rp);
	if (ret < 0) {
		pr_err("failed to reap CQ(%u)!(%d)\n", rp->q_id, ret);
		return ret;
	}
	return 0;
}

/**
 * @brief Reap the expected number of CQ entries.
 * 
 * @param fd NVMe device file descriptor
 * @param expect The number of CQ entries expected to be reaaped.
 * @param buf For store reaped CQ entries
 * @param size The size of @buf
 * @return The number of CQ entries actually reaped if success, otherwise
 *  a negative errno
 */
int nvme_reap_expect_cqe(int fd, uint16_t cqid, uint32_t expect, void *buf, 
	uint32_t size)
{
	struct nvme_reap rp = {0};
	uint8_t cqes = (cqid == NVME_AQ_ID) ? NVME_ADM_CQES : NVME_NVM_IOCQES;
	uint32_t reaped = 0;
	uint32_t timeout = 0;
	int ret;

	if (size < (expect << cqes)) {
		pr_err("buf size(%u) is too small!\n", size);
		return -EINVAL;
	}

	rp.q_id = cqid;
	rp.elements = expect;
	rp.buffer = buf;
	rp.size = size;

	while (reaped < expect) {
		ret = nvme_reap_cq_entries(fd, &rp);
		if (ret < 0)
			break;

		if (rp.num_reaped) {
			reaped += rp.num_reaped;
			rp.elements = expect - reaped;
			rp.buffer += (rp.num_reaped << cqes);
			rp.size -= (rp.num_reaped << cqes);
			rp.num_reaped = 0;

			timeout = 0;
		}

		usleep(NVME_REAP_CQ_TIMEUNIT);
		timeout++;

		if (timeout >= NVME_REAP_CQ_TIMEOUT) {
			pr_err("timeout! CQ:%u, expect:%u, reaped:%u\n", 
				cqid, expect, reaped);
			ret = -ETIME;
			break;
		}
	}

	return reaped == 0 ? ret : (int)reaped;
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
