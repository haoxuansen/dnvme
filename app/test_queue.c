/**
 * @file test_queue.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-06
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <errno.h>
#include <stdlib.h>

#include "log.h"
#include "dnvme_ioctl.h"

#include "core.h"
#include "cmd.h"
#include "test.h"
#include "test_metrics.h"
#include "test_queue.h"

static int create_ioq(struct nvme_tool *tool, struct nvme_sq_info *sq, 
	struct nvme_cq_info *cq, uint8_t contig)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ccq_wrapper ccq_wrap = {0};
	struct nvme_csq_wrapper csq_wrap = {0};
	int ret;

	ccq_wrap.cqid = cq->cqid;
	ccq_wrap.elements = cq->size;
	ccq_wrap.irq_no = cq->irq_no;
	ccq_wrap.irq_en = cq->irq_en;
	if (contig) {
		ccq_wrap.contig = 1;
	} else {
		ccq_wrap.contig = 0;
		ccq_wrap.buf = tool->cq_buf;
		ccq_wrap.size = ccq_wrap.elements << ndev->io_cqes;

		BUG_ON(ccq_wrap.size > tool->cq_buf_size);
	}

	ret = nvme_create_iocq(ndev->fd, &ccq_wrap);
	if (ret < 0) {
		pr_err("failed to create iocq:%u!(%d)\n", cq->cqid, ret);
		return ret;
	}

	csq_wrap.sqid = sq->sqid;
	csq_wrap.cqid = sq->cqid;
	csq_wrap.elements = sq->size;
	csq_wrap.prio = NVME_SQ_PRIO_MEDIUM;
	if (contig) {
		csq_wrap.contig = 1;
	} else {
		csq_wrap.contig = 0;
		csq_wrap.buf = tool->sq_buf;
		csq_wrap.size = csq_wrap.elements << ndev->io_sqes;

		BUG_ON(csq_wrap.size > tool->sq_buf_size);
	}

	ret = nvme_create_iosq(ndev->fd, &csq_wrap);
	if (ret < 0) {
		pr_err("failed to create iosq:%u!(%d)\n", sq->sqid, ret);
		return ret;
	}

	return 0;
}

static int delete_ioq(int fd, struct nvme_sq_info *sq, struct nvme_cq_info *cq)
{
	int ret;

	ret = nvme_delete_iosq(fd, sq->sqid);
	if (ret < 0) {
		pr_err("failed to delete iosq:%u!(%d)\n", sq->sqid, ret);
		return ret;
	}

	ret = nvme_delete_iocq(fd, cq->cqid);
	if (ret < 0) {
		pr_err("failed to delete iocq:%u!(%d)\n", cq->cqid, ret);
		return ret;
	}

	return 0;
}

static int send_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = ndev->nss[0].nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->rbuf;
	wrap.size = wrap.nlb * ndev->nss[0].lbads;

	BUG_ON(wrap.size > tool->rbuf_size);

	return nvme_io_read(ndev->fd, &wrap);
}

static int send_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};
	uint32_t i;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = ndev->nss[0].nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->wbuf;
	wrap.size = wrap.nlb * ndev->nss[0].lbads;

	BUG_ON(wrap.size > tool->wbuf_size);

	for (i = 0; i < (wrap.size / 4); i+= 4) {
		*(uint32_t *)(wrap.buf + i) = (uint32_t)rand();
	}

	return nvme_io_write(ndev->fd, &wrap);
}

#if 0
static int send_io_read_to_asq(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_info *nss = ndev->nss;
	struct nvme_rwc_wrapper wrap = {0};
	int ret;

	wrap.sqid = NVME_AQ_ID;
	wrap.cqid = NVME_AQ_ID;
	wrap.nsid = nss[0].nsid;
	wrap.slba = 0;
	wrap.nlb = 8;
	wrap.buf = tool->rbuf;
	wrap.size = wrap.nlb * nss[0].lbads;

	ret = nvme_io_read(ndev->fd, &wrap);
	if (ret < 0) {
		pr_debug("ASQ failed to execute IO read cmd!\n");
		return 0;
	} else {
		pr_err("ASQ execute IO read cmd success?\n");
		return -EPERM;
	}
}

static int send_io_write_to_asq(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_info *nss = ndev->nss;
	struct nvme_rwc_wrapper wrap = {0};
	int ret;

	wrap.sqid = NVME_AQ_ID;
	wrap.cqid = NVME_AQ_ID;
	wrap.nsid = nss[0].nsid;
	wrap.slba = 0;
	wrap.nlb = 8;
	wrap.buf = tool->wbuf;
	wrap.size = wrap.nlb * nss[0].lbads;

	ret = nvme_io_write(ndev->fd, &wrap);
	if (ret < 0) {
		pr_debug("ASQ failed to execute IO write cmd!\n");
		return 0;
	} else {
		pr_err("ASQ execute IO write cmd success?\n");
		return -EPERM;
	}
}
#endif

int case_queue_iocmd_to_asq(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	int ret;

	ret = send_io_read_cmd(tool, &ndev->asq, 0, 8);
	if (ret < 0) {
		pr_debug("ASQ failed to execute IO read cmd!\n");
	} else {
		pr_err("ASQ execute IO write cmd success?\n");
		return -EPERM;
	}

	ret = send_io_write_cmd(tool, &ndev->asq, 0, 8);
	if (ret < 0) {
		pr_debug("ASQ failed to execute IO write cmd!\n");
	} else {
		pr_err("ASQ execute IO write cmd success?\n");
		return -EPERM;
	}

	return 0;
}

/**
 * @brief Create contiguous IOSQ & IOCQ, then delete it.
 * 
 * @return 0 on success, otherwise a negative errno.
 * @note After the queue is created, a command is submitted to confirm
 *  that the created queue is working properly.
 */
int case_queue_create_and_delete_contig_queue(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	int ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(tool, sq, cq, 1);
	if (ret < 0)
		return ret;

	ret = send_io_read_cmd(tool, sq, 0, 8);
	if (ret < 0)
		return ret;

	ret = delete_ioq(ndev->fd, sq, cq);
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * @brief Create discontiguous IOSQ & IOCQ, then delete it.
 * 
 * @return 0 on success, otherwise a negative errno.
 * @note After the queue is created, a command is submitted to confirm
 *  that the created queue is working properly.
 */
int case_queue_create_and_delete_discontig_queue(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	int ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(tool, sq, cq, 0);
	if (ret < 0)
		return ret;

	ret = send_io_read_cmd(tool, sq, 0, 8);
	if (ret < 0)
		return ret;

	ret = delete_ioq(ndev->fd, sq, cq);
	if (ret < 0)
		return ret;

	return 0;
}
