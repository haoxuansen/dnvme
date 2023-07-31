/**
 * @file test_fused.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief NVMe Fused Operation
 * @version 0.1
 * @date 2023-07-24
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "compiler.h"
#include "nvme.h"
#include "libbase.h"
#include "libnvme.h"
#include "test.h"


static bool is_support_fused(uint16_t fuses)
{
	return (fuses & NVME_CTRL_FUSES_COMPARE_WRITE) ? true : false;
}

static int create_ioq(struct nvme_dev_info *ndev, struct nvme_sq_info *sq, 
	struct nvme_cq_info *cq)
{
	struct nvme_ccq_wrapper ccq_wrap = {0};
	struct nvme_csq_wrapper csq_wrap = {0};
	int ret;

	ccq_wrap.cqid = cq->cqid;
	ccq_wrap.elements = cq->size;
	ccq_wrap.irq_no = cq->irq_no;
	ccq_wrap.irq_en = cq->irq_en;
	ccq_wrap.contig = 1;

	ret = nvme_create_iocq(ndev, &ccq_wrap);
	if (ret < 0) {
		pr_err("failed to create iocq:%u!(%d)\n", cq->cqid, ret);
		return ret;
	}

	csq_wrap.sqid = sq->sqid;
	csq_wrap.cqid = sq->cqid;
	csq_wrap.elements = sq->size;
	csq_wrap.prio = NVME_SQ_PRIO_MEDIUM;
	csq_wrap.contig = 1;

	ret = nvme_create_iosq(ndev, &csq_wrap);
	if (ret < 0) {
		pr_err("failed to create iosq:%u!(%d)\n", sq->sqid, ret);
		return ret;
	}

	return 0;
}

static int delete_ioq(struct nvme_dev_info *ndev, struct nvme_sq_info *sq, 
	struct nvme_cq_info *cq)
{
	int ret;

	ret = nvme_delete_iosq(ndev, sq->sqid);
	if (ret < 0) {
		pr_err("failed to delete iosq:%u!(%d)\n", sq->sqid, ret);
		return ret;
	}

	ret = nvme_delete_iocq(ndev, cq->cqid);
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

	memset(wrap.buf, 0, wrap.size);

	return nvme_io_read(ndev, &wrap);
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

	for (i = 0; i < wrap.size; i+= 4) {
		*(uint32_t *)(wrap.buf + i) = (uint32_t)rand();
	}

	return nvme_io_write(ndev, &wrap);
}

static int submit_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, uint8_t flags)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.flags = flags;
	wrap.nsid = ndev->nss[0].nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->rbuf;
	wrap.size = wrap.nlb * ndev->nss[0].lbads;

	BUG_ON(wrap.size > tool->rbuf_size);

	memset(wrap.buf, 0, wrap.size);

	return nvme_cmd_io_read(ndev->fd, &wrap);
}

static int submit_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, uint8_t flags)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};
	uint32_t i;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.flags = flags;
	wrap.nsid = ndev->nss[0].nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->wbuf;
	wrap.size = wrap.nlb * ndev->nss[0].lbads;

	BUG_ON(wrap.size > tool->wbuf_size);

	for (i = 0; i < wrap.size; i+= 4) {
		*(uint32_t *)(wrap.buf + i) = (uint32_t)rand();
	}

	return nvme_cmd_io_write(ndev->fd, &wrap);
}

static int submit_io_compare_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, uint8_t flags)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.flags = flags;
	wrap.nsid = ndev->nss[0].nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->rbuf;
	wrap.size = wrap.nlb * ndev->nss[0].lbads;

	BUG_ON(wrap.size > tool->rbuf_size);

	return nvme_cmd_io_compare(ndev->fd, &wrap);
}

/**
 * @brief Compare and write fused operation success
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_compare_write_success(struct nvme_tool *tool, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	uint64_t slba = rand() % (ndev->nss[0].nsze / 2);
	uint32_t nlb = rand() % 256 + 1; /* 1~256 */
	uint16_t ccid, wcid;
	int ret;

	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0)
		goto out;

	ret = send_io_read_cmd(tool, sq, slba, nlb);
	if (ret < 0)
		goto out;

	ret = submit_io_compare_cmd(tool, sq, slba, nlb, NVME_CMD_FUSE_FIRST);
	if (ret < 0)
		goto out;
	ccid = ret;

	ret = submit_io_write_cmd(tool, sq, slba, nlb, NVME_CMD_FUSE_SECOND);
	if (ret < 0)
		goto out;
	wcid = ret;
	sq->cmd_cnt = 2;
	
	ret = nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
	if (ret < 0)
		goto out;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, sq->cqid, sq->cmd_cnt, 
		tool->entry, tool->entry_size);
	if (ret != sq->cmd_cnt) {
		pr_err("expect reap %u, actual reaped %d!\n", sq->cmd_cnt, ret);
		ret = ret < 0 ? ret : -ETIME;
		goto out;
	}

	ret = nvme_valid_cq_entry(&tool->entry[0], sq->sqid, ccid, 
		NVME_SC_SUCCESS);
	if (ret < 0)
		goto out;

	ret = nvme_valid_cq_entry(&tool->entry[1], sq->sqid, wcid, 
		NVME_SC_SUCCESS);
	if (ret < 0)
		goto out;

out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Compare and write fused operation aborted
 * 
 * @details The first command in the sequence failed, then the second
 *  command in the sequence shall be aborted.
 * 
 *  	The first command status code is Compare Failure.
 *  	The second command status code is Command Aborted due to Failed
 *  Fused Command.
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_compare_write_aborted(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	uint64_t slba = rand() % (ndev->nss[0].nsze / 2);
	uint32_t nlb = rand() % 256 + 1; /* 1~256 */
	uint16_t ccid, wcid;
	int ret;

	/*
	 * We expect the compare command to fail, so rbuf data shall not
	 * equal to wbuf data. 
	 */
	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0)
		goto out;

	memset(tool->rbuf, 0, tool->rbuf_size);

	ret = submit_io_compare_cmd(tool, sq, slba, nlb, NVME_CMD_FUSE_FIRST);
	if (ret < 0)
		goto out;
	ccid = ret;

	ret = submit_io_write_cmd(tool, sq, slba, nlb, NVME_CMD_FUSE_SECOND);
	if (ret < 0)
		goto out;
	wcid = ret;
	sq->cmd_cnt = 2;

	ret = nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
	if (ret < 0)
		goto out;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, sq->cqid, sq->cmd_cnt, 
		tool->entry, tool->entry_size);
	if (ret != sq->cmd_cnt) {
		pr_err("expect reap %u, actual reaped %d!\n", sq->cmd_cnt, ret);
		ret = ret < 0 ? ret : -ETIME;
		goto out;
	}

	ret = nvme_valid_cq_entry(&tool->entry[0], sq->sqid, ccid, 
		NVME_SC_COMPARE_FAILED);
	if (ret < 0)
		goto out;
	
	ret = nvme_valid_cq_entry(&tool->entry[1], sq->sqid, wcid, 
		NVME_SC_FUSED_FAIL);
	if (ret < 0)
		goto out;

out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Compare and write fused operation invalid
 * 
 * @details If the LBA ranges do not match, the commands should be aborted
 *  with status of Invalid Field in Commands.
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_compare_write_invalid(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	uint64_t slba = rand() % (ndev->nss[0].nsze / 2);
	uint32_t nlb = rand() % 256 + 1; /* 1~256 */
	uint16_t ccid, wcid;
	int ret;

	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0)
		goto out;

	ret = send_io_read_cmd(tool, sq, slba, nlb);
	if (ret < 0)
		goto out;

	ret = submit_io_compare_cmd(tool, sq, slba, nlb, NVME_CMD_FUSE_FIRST);
	if (ret < 0)
		goto out;
	ccid = ret;

	/* ensure the LBA ranges do not match between compare and write cmd */
	ret = submit_io_write_cmd(tool, sq, slba, nlb + 1, NVME_CMD_FUSE_SECOND);
	if (ret < 0)
		goto out;
	wcid = ret;
	sq->cmd_cnt = 2;

	ret = nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
	if (ret < 0)
		goto out;

	ret = nvme_gnl_cmd_reap_cqe(ndev, sq->cqid, sq->cmd_cnt, 
		tool->entry, tool->entry_size);
	if (ret != sq->cmd_cnt) {
		pr_err("expect reap %u, actual reaped %d!\n", sq->cmd_cnt, ret);
		ret = ret < 0 ? ret : -ETIME;
		goto out;
	}

	ret = nvme_valid_cq_entry(&tool->entry[0], sq->sqid, ccid, 
		NVME_SC_INVALID_FIELD);
	if (ret < 0)
		goto out;

	ret = nvme_valid_cq_entry(&tool->entry[1], sq->sqid, wcid, 
		NVME_SC_INVALID_FIELD);
	if (ret < 0)
		goto out;

out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Compare and write fused operation discontiguous
 * 
 * @details Insert read cmd in the middle of fused operation. 
 *  Fused operation is expected to fail.
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_compare_write_discontig(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_completion *entry;
	struct nvme_dev_info *ndev = tool->ndev;
	uint64_t slba = rand() % (ndev->nss[0].nsze / 2);
	uint32_t nlb = rand() % 256 + 1; /* 1~256 */
	uint16_t ccid, wcid, rcid;
	int ret;

	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0)
		goto out;

	ret = send_io_read_cmd(tool, sq, slba, nlb);
	if (ret < 0)
		goto out;

	ret = submit_io_compare_cmd(tool, sq, slba, nlb, NVME_CMD_FUSE_FIRST);
	if (ret < 0)
		goto out;
	ccid = ret;

	ret = submit_io_read_cmd(tool, sq, slba, nlb, 0);
	if (ret < 0)
		goto out;
	rcid = ret;

	ret = submit_io_write_cmd(tool, sq, slba, nlb, NVME_CMD_FUSE_SECOND);
	if (ret < 0)
		goto out;
	wcid = ret;
	sq->cmd_cnt = 3;

	ret = nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
	if (ret < 0)
		goto out;

	ret = nvme_gnl_cmd_reap_cqe(ndev, sq->cqid, sq->cmd_cnt, 
		tool->entry, tool->entry_size);
	if (ret != sq->cmd_cnt) {
		pr_err("expect reap %u, actual reaped %d!\n", sq->cmd_cnt, ret);
		ret = ret < 0 ? ret : -ETIME;
		goto out;
	}

	/* Check the data of CQ entries reaped  */
	ret = -EINVAL;

	entry = nvme_find_cq_entry(tool->entry, sq->cmd_cnt, ccid);
	if (!entry) {
		pr_err("failed to find compare command CQE!\n");
		goto out;
	}

	if (NVME_CQE_STATUS_TO_STATE(entry->status) == NVME_SC_SUCCESS) {
		pr_err("compare command CQE status is %u!\n", 
			NVME_CQE_STATUS_TO_STATE(entry->status));
		goto out;
	}

	entry = nvme_find_cq_entry(tool->entry, sq->cmd_cnt, rcid);
	if (!entry) {
		pr_err("failed to find read command CQE!\n");
		goto out;
	}

	if (NVME_CQE_STATUS_TO_STATE(entry->status) != NVME_SC_SUCCESS) {
		pr_err("read command CQE status is %u!\n", 
			NVME_CQE_STATUS_TO_STATE(entry->status));
		goto out;
	}

	entry = nvme_find_cq_entry(tool->entry, sq->cmd_cnt, wcid);
	if (!entry) {
		pr_err("failed to find write command CQE!\n");
		goto out;
	}

	if (NVME_CQE_STATUS_TO_STATE(entry->status) == NVME_SC_SUCCESS) {
		pr_err("write command CQE status is %u!\n", 
			NVME_CQE_STATUS_TO_STATE(entry->status));
		goto out;
	}

	ret = 0;
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Compare and write fused operation
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the controller does not support the requested fused operation,
 *  then the controller should abort the command with a status code
 *  of Invalid Field in Command. 
 */
static int __unused subcase_compare_write_nonsupport(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	uint64_t slba = rand() % (ndev->nss[0].nsze / 2);
	uint32_t nlb = rand() % 256 + 1; /* 1~256 */
	uint16_t ccid, wcid;
	int ret;

	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0)
		goto out;

	ret = send_io_read_cmd(tool, sq, slba, nlb);
	if (ret < 0)
		goto out;

	ret = submit_io_compare_cmd(tool, sq, slba, nlb, NVME_CMD_FUSE_FIRST);
	if (ret < 0)
		goto out;
	ccid = ret;

	ret = submit_io_write_cmd(tool, sq, slba, nlb, NVME_CMD_FUSE_SECOND);
	if (ret < 0)
		goto out;
	wcid = ret;
	sq->cmd_cnt = 2;

	ret = nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
	if (ret < 0)
		goto out;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, sq->cqid, sq->cmd_cnt, 
		tool->entry, tool->entry_size);
	if (ret != sq->cmd_cnt) {
		pr_err("expect reap %u, actual reaped %d!\n", sq->cmd_cnt, ret);
		ret = ret < 0 ? ret : -ETIME;
		goto out;
	}

	ret = nvme_valid_cq_entry(&tool->entry[0], sq->sqid, ccid, 
		NVME_SC_INVALID_FIELD);
	if (ret < 0)
		goto out;

	ret = nvme_valid_cq_entry(&tool->entry[1], sq->sqid, wcid, 
		NVME_SC_INVALID_FIELD);
	if (ret < 0)
		goto out;

out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

int case_fused_operation(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_id_ctrl *id_ctrl = &ndev->id_ctrl;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	int ret;

	if (!is_support_fused(le16_to_cpu(id_ctrl->fuses))) {
		pr_warn("controller not support fused operation!\n");
		return -EOPNOTSUPP;
	}

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	ret |= subcase_compare_write_success(tool, sq);
	ret |= subcase_compare_write_aborted(tool, sq);
	ret |= subcase_compare_write_invalid(tool, sq);
	ret |= subcase_compare_write_discontig(tool, sq);

	nvme_display_subcase_result();

	ret |= delete_ioq(ndev, sq, cq);
	return ret;
}
