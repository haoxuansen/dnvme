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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "libbase.h"
#include "libnvme.h"

#include "test.h"
#include "test_metrics.h"

struct test_data {
	uint32_t	nsid;
	uint64_t	nsze;
	uint32_t	lbads;
};

static struct test_data g_test = {0};

static int init_test_data(struct nvme_dev_info *ndev, struct test_data *data)
{
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	int ret;

	/* use first active namespace as default */
	data->nsid = le32_to_cpu(ns_grp->act_list[0]);

	ret = nvme_id_ns_lbads(ns_grp, data->nsid, &data->lbads);
	if (ret < 0) {
		pr_err("failed to get lbads!(%d)\n", ret);
		return ret;
	}
	/* we have checked once, skip the check below */
	nvme_id_ns_nsze(ns_grp, data->nsid, &data->nsze);
	return 0;
}

static bool is_support_sgl(struct nvme_id_ctrl *ctrl)
{
	if ((ctrl->sgls & NVME_CTRL_SGLS_MASK) && 
		(ctrl->sgls & NVME_CTRL_SGLS_DATA_BLOCK)) {
		return true;
	}
	return false;
}

static bool is_support_discontig(struct nvme_ctrl_instance *ctrl)
{
	struct nvme_ctrl_property *prop = ctrl->prop;

	if (NVME_CAP_CQR(prop->cap))
		return false;

	return true;
}

static int create_ioq(struct nvme_tool *tool, struct nvme_sq_info *sq, 
	struct nvme_cq_info *cq, uint8_t contig)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ccq_wrapper ccq_wrap = {0};
	struct nvme_csq_wrapper csq_wrap = {0};
	int ret;

	ccq_wrap.cqid = cq->cqid;
	ccq_wrap.elements = cq->nr_entry;
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

	ret = nvme_create_iocq(ndev, &ccq_wrap);
	if (ret < 0) {
		pr_err("failed to create iocq:%u!(%d)\n", cq->cqid, ret);
		return ret;
	}

	csq_wrap.sqid = sq->sqid;
	csq_wrap.cqid = sq->cqid;
	csq_wrap.elements = sq->nr_entry;
	csq_wrap.prio = NVME_SQ_PRIO_MEDIUM;
	if (contig) {
		csq_wrap.contig = 1;
	} else {
		csq_wrap.contig = 0;
		csq_wrap.buf = tool->sq_buf;
		csq_wrap.size = csq_wrap.elements << ndev->io_sqes;

		BUG_ON(csq_wrap.size > tool->sq_buf_size);
	}

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
	uint64_t slba, uint32_t nlb, uint8_t flags)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};
	struct test_data *test = &g_test;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.flags = flags;
	wrap.nsid = test->nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->rbuf;
	wrap.size = wrap.nlb * test->lbads;

	BUG_ON(wrap.size > tool->rbuf_size);

	memset(wrap.buf, 0, wrap.size);

	return nvme_io_read(ndev, &wrap);
}

static int send_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, uint8_t flags)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};
	struct test_data *test = &g_test;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.flags = flags;
	wrap.nsid = test->nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->wbuf;
	wrap.size = wrap.nlb * test->lbads;

	BUG_ON(wrap.size > tool->wbuf_size);
	fill_data_with_random(wrap.buf, wrap.size);

	return nvme_io_write(ndev, &wrap);
}

/**
 * @brief Send I/O write and read cmds which use PRP list, then compare
 *  whether the data written and read back is same?
 * 
 * @return 0 on success, otherwise a negative errno.
 */
static int subcase_iorw_prp(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint32_t loop)
{
	struct test_data *test = &g_test;
	uint32_t nlb = 8;
	int ret;

	while (loop--) {
		ret = send_io_write_cmd(tool, sq, 0, nlb, 0);
		if (ret < 0)
			goto out;

		ret = send_io_read_cmd(tool, sq, 0, nlb, 0);
		if (ret < 0)
			goto out;

		ret = memcmp(tool->wbuf, tool->rbuf, nlb * test->lbads);
		if (ret) {
			pr_err("failed to compare r/w data!(%d)\n", ret);
			ret = -EIO;
			goto out;
		}
		pr_div("loop remaining: %u\n", loop);
	}

out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Send I/O write and read cmds which use SGL list, then compare
 *  whether the data written and read back is same?
 * 
 * @return 0 on success, otherwise a negative errno.
 */
static int subcase_iorw_sgl(struct nvme_tool *tool, struct nvme_sq_info *sq, 
	uint32_t loop)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_id_ctrl *id_ctrl = ctrl->id_ctrl;
	struct test_data *test = &g_test;
	uint8_t flags = NVME_CMD_SGL_METABUF;
	uint32_t nlb = 8;
	int ret;

	if (!loop) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (!is_support_sgl(id_ctrl)) {
		pr_warn("Not support SGL!\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	while (loop--) {
		ret = send_io_write_cmd(tool, sq, 0, nlb, flags);
		if (ret < 0)
			goto out;

		ret = send_io_read_cmd(tool, sq, 0, nlb, flags);
		if (ret < 0)
			goto out;

		ret = memcmp(tool->wbuf, tool->rbuf, nlb * test->lbads);
		if (ret) {
			pr_err("failed to compare r/w data!(%d)\n", ret);
			ret = -EIO;
			goto out;
		}
		pr_debug("loop remaining: %u\n", loop);
	}
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Send I/O read and write command to ASQ
 * 
 * @return 0 on success, otherwise a negative errno.
 */
static int case_queue_iocmd_to_asq(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	int ret;

	ret = init_test_data(ndev, &g_test);
	if (ret < 0)
		return ret;

	ret = send_io_read_cmd(tool, &ndev->asq, 0, 8, 0);
	if (ret < 0) {
		pr_debug("ASQ failed to execute IO read cmd!\n");
	} else {
		pr_err("ASQ execute IO write cmd success?\n");
		return -EPERM;
	}

	ret = send_io_write_cmd(tool, &ndev->asq, 0, 8, 0);
	if (ret < 0) {
		pr_debug("ASQ failed to execute IO write cmd!\n");
	} else {
		pr_err("ASQ execute IO write cmd success?\n");
		return -EPERM;
	}

	return 0;
}
NVME_CASE_QUEUE_SYMBOL(case_queue_iocmd_to_asq,
	"Send I/O read and write command to ASQ");

/**
 * @brief Create I/O CQ & SQ which is contiguous, check whether the queue
 *  works properly, and finally delete the queue.
 * 
 * @return 0 on success, otherwise a negative errno.
 */
static int case_queue_contiguous(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	int ret = 0;

	ret = init_test_data(ndev, &g_test);
	if (ret < 0)
		return ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(tool, sq, cq, 1);
	if (ret < 0)
		return ret;

	ret |= subcase_iorw_prp(tool, sq, 10);
	ret |= subcase_iorw_sgl(tool, sq, 10);

	nvme_display_subcase_report();

	ret |= delete_ioq(ndev, sq, cq);
	return ret;
}
NVME_CASE_QUEUE_SYMBOL(case_queue_contiguous, "Test contiguous I/O queue");

/**
 * @brief Create I/O CQ & SQ which is discontiguous, check whether the queue
 *  works properly, and finally delete the queue.
 * 
 * @return 0 on success, otherwise a negative errno. 
 * 
 * @warning Queues may not work properly after repeated creation and deletion
 */
static int case_queue_discontiguous(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	int ret = 0;

	if (!is_support_discontig(ndev->ctrl))
		return -EOPNOTSUPP;

	ret = init_test_data(ndev, &g_test);
	if (ret < 0)
		return ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(tool, sq, cq, 0);
	if (ret < 0)
		return ret;

	ret |= subcase_iorw_prp(tool, sq, 1);
	ret |= subcase_iorw_sgl(tool, sq, 1);

	nvme_display_subcase_report();

	ret |= delete_ioq(ndev, sq, cq);
	return ret;
}
NVME_CASE_QUEUE_SYMBOL(case_queue_discontiguous, "Test discontiguous I/O queue");
