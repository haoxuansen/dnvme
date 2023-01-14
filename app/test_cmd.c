/**
 * @file test_cmd.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "log.h"
#include "dnvme_ioctl.h"
#include "core.h"
#include "cmd.h"
#include "queue.h"
#include "debug.h"
#include "test.h"
#include "test_metrics.h"
#include "test_cmd.h"

static int create_ioq(int fd, struct nvme_sq_info *sq, struct nvme_cq_info *cq)
{
	struct nvme_ccq_wrapper ccq_wrap = {0};
	struct nvme_csq_wrapper csq_wrap = {0};
	int ret;

	ccq_wrap.cqid = cq->cqid;
	ccq_wrap.elements = cq->size;
	ccq_wrap.irq_no = cq->irq_no;
	ccq_wrap.irq_en = cq->irq_en;
	ccq_wrap.contig = 1;

	ret = nvme_create_iocq(fd, &ccq_wrap);
	if (ret < 0) {
		pr_err("failed to create iocq:%u!(%d)\n", cq->cqid, ret);
		return ret;
	}

	csq_wrap.sqid = sq->sqid;
	csq_wrap.cqid = sq->cqid;
	csq_wrap.elements = sq->size;
	csq_wrap.prio = NVME_SQ_PRIO_MEDIUM;
	csq_wrap.contig = 1;

	ret = nvme_create_iosq(fd, &csq_wrap);
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

static int send_io_compare_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
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

	return nvme_io_compare(ndev->fd, &wrap);
}

int case_cmd_send_io_read_cmd(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	int ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev->fd, sq, cq);
	if (ret < 0)
		return ret;

	ret = send_io_read_cmd(tool, sq, 0, 8);
	if (ret < 0) {
		pr_err("failed to read data!(%d)\n", ret);
		goto out;
	}
out:
	ret = delete_ioq(ndev->fd, sq, cq);
	if (ret < 0)
		return ret;

	return 0;
}

int case_cmd_send_io_write_cmd(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	int ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev->fd, sq, cq);
	if (ret < 0)
		return ret;

	ret = send_io_write_cmd(tool, sq, 0, 8);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out;
	}
out:
	ret = delete_ioq(ndev->fd, sq, cq);
	if (ret < 0)
		return ret;

	return 0;
}

int case_cmd_send_io_compare_cmd(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	uint64_t slba = 0;
	uint32_t nlb = 8;
	uint32_t size;
	int ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev->fd, sq, cq);
	if (ret < 0)
		return ret;

	ret = send_io_write_cmd(tool, sq, slba, nlb);
	ret |= send_io_read_cmd(tool, sq, slba, nlb);
	ret |= send_io_compare_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to compare data!(%d)\n", ret);
		goto out;
	}

	/* check again */
	size = ndev->nss[0].lbads * nlb;
	ret = memcmp(tool->wbuf, tool->rbuf, size);
	if (ret != 0) {
		pr_err("failed to compare read/write buffer!\n");
		ret = -EIO;
		goto out;
	}
out:
	ret = delete_ioq(ndev->fd, sq, cq);
	if (ret < 0)
		return ret;

	return 0;
}
