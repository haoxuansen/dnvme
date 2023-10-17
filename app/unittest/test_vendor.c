/**
 * @file test_vendor.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-09-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"
#include "test.h"

struct test_data {
	uint32_t	nsid;
	uint64_t	nsze;
	uint32_t	lbads;

	uint8_t		flags;
	uint16_t	control;

	char		rpt_buf[SZ_1K];
};

static struct test_data g_test = {0};

static int init_test_data(struct nvme_tool *tool, struct test_data *data)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	int ret;

	memset(data, 0, sizeof(struct test_data));
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

static int create_ioq(struct nvme_dev_info *ndev, struct nvme_sq_info *sq, 
	struct nvme_cq_info *cq, struct json_node *step_node)
{
	struct nvme_ccq_wrapper ccq_wrap = {0};
	struct nvme_csq_wrapper csq_wrap = {0};
	struct test_data *test = &g_test;
	int ret;

	ccq_wrap.cqid = cq->cqid;
	ccq_wrap.elements = cq->nr_entry;
	ccq_wrap.irq_no = cq->irq_no;
	ccq_wrap.irq_en = cq->irq_en;
	ccq_wrap.contig = 1;

	if (step_node) {
		nvme_record_create_cq(cq, test->rpt_buf, sizeof(test->rpt_buf));
		json_add_content_to_step_node(step_node, test->rpt_buf);
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
	csq_wrap.contig = 1;

	if (step_node) {
		nvme_record_create_sq(sq, test->rpt_buf, sizeof(test->rpt_buf));
		json_add_content_to_step_node(step_node, test->rpt_buf);
	}

	ret = nvme_create_iosq(ndev, &csq_wrap);
	if (ret < 0) {
		pr_err("failed to create iosq:%u!(%d)\n", sq->sqid, ret);
		return ret;
	}

	return 0;
}

static int delete_ioq(struct nvme_dev_info *ndev, struct nvme_sq_info *sq, 
	struct nvme_cq_info *cq, struct json_node *step_node)
{
	struct test_data *test = &g_test;
	int ret;

	if (step_node) {
		nvme_record_delete_sq(sq, test->rpt_buf, sizeof(test->rpt_buf));
		json_add_content_to_step_node(step_node, test->rpt_buf);
	}

	ret = nvme_delete_iosq(ndev, sq->sqid);
	if (ret < 0) {
		pr_err("failed to delete iosq:%u!(%d)\n", sq->sqid, ret);
		return ret;
	}

	if (step_node) {
		nvme_record_delete_cq(cq, test->rpt_buf, sizeof(test->rpt_buf));
		json_add_content_to_step_node(step_node, test->rpt_buf);
	}

	ret = nvme_delete_iocq(ndev, cq->cqid);
	if (ret < 0) {
		pr_err("failed to delete iocq:%u!(%d)\n", cq->cqid, ret);
		return ret;
	}

	return 0;
}

static int submit_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};
	struct test_data *test = &g_test;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = test->nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->wbuf;
	wrap.size = wrap.nlb * test->lbads;

	BUG_ON(wrap.size > tool->wbuf_size);
	fill_data_with_random(wrap.buf, wrap.size);

	return nvme_cmd_io_write(ndev->fd, &wrap);
}

static int __send_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, void *rbuf, uint32_t size)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};
	struct test_data *test = &g_test;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.flags = test->flags;
	wrap.nsid = test->nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = rbuf;
	wrap.size = wrap.nlb * test->lbads;
	wrap.control = test->control;

	BUG_ON(wrap.size > size);

	return nvme_io_read(ndev, &wrap);
}

static int send_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, struct json_node *step_node)
{
	struct test_data *test = &g_test;

	if (step_node) {
		snprintf(test->rpt_buf, sizeof(test->rpt_buf),
			"Send IO read cmd to SQ(%u) - SLBA:0x%lx, NLB:0x%x",
			sq->sqid, slba, nlb);
		json_add_content_to_step_node(step_node, test->rpt_buf);
	}

	return __send_io_read_cmd(tool, sq, slba, nlb, 
		tool->rbuf, tool->rbuf_size);
}

static int send_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, struct json_node *step_node)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};
	struct test_data *test = &g_test;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.flags = test->flags;
	wrap.nsid = test->nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->wbuf;
	wrap.size = wrap.nlb * test->lbads;
	wrap.control = test->control;

	BUG_ON(wrap.size > tool->wbuf_size);
	fill_data_with_random(wrap.buf, wrap.size);

	if (step_node) {
		snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
			"Send IO write cmd to SQ(%u) - SLBA:0x%lx, NLB:0x%x",
			sq->sqid, slba, nlb);
		json_add_content_to_step_node(step_node, test->rpt_buf);
	}
	return nvme_io_write(ndev, &wrap);
}

static int do_io_cmds_together(struct nvme_tool *tool, struct nvme_sq_info *sq,
	struct nvme_cq_info *cq, uint32_t cmds, struct json_node *step_node)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	/* ensure "slba + nlb <= nsze */
	uint64_t slba = rand() % (test->nsze / 2);
	uint32_t nlb = rand() % min_t(uint64_t, test->nsze / 2, 256) + 1;
	uint32_t nr_cmd = min_t(uint32_t, sq->nr_entry, cmds);
	uint32_t nr_reap;
	uint32_t i;
	int ret;

	snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
		"Submit %u IO write cmds to SQ(%u)", sq->sqid, cmds);
	json_add_content_to_step_node(step_node, test->rpt_buf);

	for (i = 0; i < nr_cmd; i++) {
		ret = submit_io_write_cmd(tool, sq, slba, nlb);
		if (ret < 0)
			return ret;
	}

	snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
		"Ring SQ(%u) doorbell", sq->sqid);
	json_add_content_to_step_node(step_node, test->rpt_buf);

	ret = nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
	if (ret < 0)
		return ret;

	do {
		nr_reap = min_t(uint32_t, cq->nr_entry, nr_cmd);

		ret = nvme_gnl_cmd_reap_cqe(ndev, sq->cqid, nr_reap, 
			tool->entry, tool->entry_size);
		if (ret != nr_reap) {
			pr_err("expect reap %u, actual reaped %d!\n", 
				nr_reap, ret);
			return ret < 0 ? ret : -ETIME;
		}
		nr_cmd -= nr_reap;
	} while (nr_cmd);

	return 0;
}

static int case_hw_io_cmd_timeout_check(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct test_data *test = &g_test;
	struct json_node *case_node;
	struct json_node *step_node;
	uint64_t slba;
	uint32_t nlb;
	int ret = 0;

	case_node = json_add_case_node(json_get_case_set(tool->report), 
		__func__, false);
	step_node = json_add_step_node(case_node);

	ret = init_test_data(tool, test);
	if (ret < 0)
		goto out;

	snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
		"Send vendor command(0x%x): Set Param",
		nvme_admin_maxio_nvme_top);
	json_add_content_to_step_node(step_node, test->rpt_buf);

	ret = nvme_maxio_nvme_top_set_param(ndev, BIT(0), 0);
	if (ret < 0)
		goto out;

	msleep(1000);

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		ret = -ENODEV;
		goto out;
	}

	ret = create_ioq(ndev, sq, cq, step_node);
	if (ret < 0)
		goto out;

	/* ensure "slba + nlb <= nsze */
	slba = rand() % (test->nsze / 2);
	nlb = rand() % min_t(uint64_t, test->nsze / 2, 256) + 1;

	ret = send_io_write_cmd(tool, sq, slba, nlb, step_node);
	if (ret < 0)
		goto del_ioq;

	snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
		"Send vendor command(0x%x): Check Result",
		nvme_admin_maxio_nvme_top);
	json_add_content_to_step_node(step_node, test->rpt_buf);

	ret = nvme_maxio_nvme_top_check_result(ndev, BIT(0));
	if (ret < 0)
		goto del_ioq;

del_ioq:
	ret |= delete_ioq(ndev, sq, cq, step_node);
out:
	json_add_result_node(case_node, ret);
	return ret;
}
NVME_CASE_SYMBOL(case_hw_io_cmd_timeout_check, "?");

#if IS_ENABLED(CONFIG_DEBUG_NO_DEVICE)
static int case_cq_full_threshold_limit_sq_fetch_cmd(struct nvme_tool *tool)
{
	struct test_data *test = &g_test;
	struct json_node *case_node;
	struct json_node *step_node;
	uint32_t nr_entry = rand() % 10 + 1;
	int ret = 0;

	case_node = json_add_case_node(json_get_case_set(tool->report), 
		__func__, false);
	step_node = json_add_step_node(case_node);

	snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
		"Send vendor command(0x%x): Set Param - CQ entry num %u",
		nvme_admin_maxio_nvme_cqm, nr_entry);
	json_add_content_to_step_node(step_node, test->rpt_buf);

	snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
		"Send vendor command(0x%x): Check Result",
		nvme_admin_maxio_nvme_cqm);
	json_add_content_to_step_node(step_node, test->rpt_buf);

	json_add_result_node(case_node, ret);
	return ret;
}
NVME_CASE_SYMBOL(case_cq_full_threshold_limit_sq_fetch_cmd, "?");
#else
static int case_cq_full_threshold_limit_sq_fetch_cmd(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct test_data *test = &g_test;
	struct json_node *case_node;
	struct json_node *step_node;
	uint32_t nr_entry = rand() % 10 + 1;
	int ret;

	case_node = json_add_case_node(json_get_case_set(tool->report), 
		__func__, false);
	step_node = json_add_step_node(case_node);

	ret = init_test_data(tool, test);
	if (ret < 0)
		goto out;

	snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
		"Send vendor command(0x%x): Set Param - CQ entry num %u",
		nvme_admin_maxio_nvme_cqm, nr_entry);
	json_add_content_to_step_node(step_node, test->rpt_buf);

	ret = nvme_maxio_nvme_cqm_set_param(ndev, BIT(0), nr_entry);
	if (ret < 0)
		goto out;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		ret = -ENODEV;
		goto out;
	}

	if (cq->nr_entry != nr_entry)
		swap(cq->nr_entry, nr_entry);

	ret = create_ioq(ndev, sq, cq, step_node);
	if (ret < 0)
		goto restore;

	ret = do_io_cmds_together(tool, sq, cq, 50, step_node);
	if (ret < 0)
		goto del_ioq;

	snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
		"Send vendor command(0x%x): Check Result",
		nvme_admin_maxio_nvme_cqm);
	json_add_content_to_step_node(step_node, test->rpt_buf);

	ret = nvme_maxio_nvme_cqm_check_result(ndev, BIT(0));
	if (ret < 0)
		goto del_ioq;

del_ioq:
	ret |= delete_ioq(ndev, sq, cq, step_node);
restore:
	/* restore the value avoid affecting next test */
	if (cq->nr_entry != nr_entry)
		swap(cq->nr_entry, nr_entry);
out:
	json_add_result_node(case_node, ret);
	return ret;
}
NVME_CASE_SYMBOL(case_cq_full_threshold_limit_sq_fetch_cmd, "?");
#endif /* !IS_ENABLED(CONFIG_DEBUG_NO_DEVICE) */

static int case_hw_rdma_ftl_size_limit(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct test_data *test = &g_test;
	struct json_node *case_node;
	struct json_node *step_node;
	uint64_t slba;
	uint32_t nlb = 1;
	uint32_t pending = rand() % 10 + 1; /* 1 ~ 10 */
	int i;
	int ret;

	case_node = json_add_case_node(json_get_case_set(tool->report), 
		__func__, false);
	step_node = json_add_step_node(case_node);

	ret = init_test_data(tool, test);
	if (ret < 0)
		goto out;

	snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
		"Send vendor command(0x%x): Set Param - pending num %u",
		nvme_admin_maxio_nvme_hwrdma, pending);
	json_add_content_to_step_node(step_node, test->rpt_buf);

	ret = nvme_maxio_nvme_hwrdma_set_param(ndev, BIT(0), pending);
	if (ret < 0)
		goto out;

	msleep(1000);

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		ret = -ENODEV;
		goto out;
	}

	ret = create_ioq(ndev, sq, cq, step_node);
	if (ret < 0)
		goto out;

	for (i = 0; i < (2 * pending); i++) {
		slba = rand() % (test->nsze / 2);

		ret = send_io_read_cmd(tool, sq, slba, nlb, step_node);
		if (ret < 0)
			goto del_ioq;
	}

	snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
		"Send vendor command(0x%x): Check Result",
		nvme_admin_maxio_nvme_hwrdma);
	json_add_content_to_step_node(step_node, test->rpt_buf);

	ret = nvme_maxio_nvme_hwrdma_check_result(ndev, BIT(0));
	if (ret < 0)
		goto del_ioq;

del_ioq:
	ret |= delete_ioq(ndev, sq, cq, step_node);
out:
	json_add_result_node(case_node, ret);
	return ret;
}
NVME_CASE_SYMBOL(case_hw_rdma_ftl_size_limit, "?");

static int case_hw_rdma_ftl_rreq_if_en(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct test_data *test = &g_test;
	struct json_node *case_node;
	struct json_node *step_node;
	uint64_t slba;
	uint32_t nlb = 1;
	/* bit[5:0] each bit stand a FTL IF(1: enable) */
	uint32_t interface[6] = {0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f};
	int i, j;
	int ret;

	case_node = json_add_case_node(json_get_case_set(tool->report), 
		__func__, false);
	step_node = json_add_step_node(case_node);

	ret = init_test_data(tool, test);
	if (ret < 0)
		goto out;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		ret = -ENODEV;
		goto out;
	}

	ret = create_ioq(ndev, sq, cq, step_node);
	if (ret < 0)
		goto out;

	for (j = 0; j < ARRAY_SIZE(interface); j++) {
		snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
			"Send vendor command(0x%x): Set Param - FTL_IF_MAP 0x%x",
			nvme_admin_maxio_nvme_hwrdma, interface[j]);
		json_add_content_to_step_node(step_node, test->rpt_buf);

		ret = nvme_maxio_nvme_hwrdma_set_param(ndev, BIT(1), interface[j]);
		if (ret < 0)
			goto del_ioq;

		msleep(1000);

		for (i = 0; i < 12; i++) {
			slba = rand() % (test->nsze / 2);

			ret = send_io_read_cmd(tool, sq, slba, nlb, step_node);
			if (ret < 0)
				goto del_ioq;
		}
		snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
			"Send vendor command(0x%x): Check Result",
			nvme_admin_maxio_nvme_hwrdma);
		json_add_content_to_step_node(step_node, test->rpt_buf);

		ret = nvme_maxio_nvme_hwrdma_check_result(ndev, BIT(1));
		if (ret < 0)
			goto del_ioq;
	}

del_ioq:
	ret |= delete_ioq(ndev, sq, cq, step_node);
out:
	json_add_result_node(case_node, ret);
	return ret;
}
NVME_CASE_SYMBOL(case_hw_rdma_ftl_rreq_if_en, "?");

static int case_hw_rdma_ftl_if_namespace_bind(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct test_data *test = &g_test;
	struct json_node *case_node;
	struct json_node *step_node;
	uint64_t slba;
	uint32_t nlb = 1;
	uint32_t idx = rand() % ns_grp->nr_act;
	uint32_t ftl_if = rand() % 6 + 1;
	int i;
	int ret;

	case_node = json_add_case_node(json_get_case_set(tool->report), 
		__func__, false);
	step_node = json_add_step_node(case_node);

	ret = init_test_data(tool, test);
	if (ret < 0)
		goto out;
	
	test->nsid = le32_to_cpu(ns_grp->act_list[idx]);

	ret = nvme_id_ns_lbads(ns_grp, test->nsid, &test->lbads);
	if (ret < 0) {
		pr_err("failed to get lbads!(%d)\n", ret);
		goto out;
	}
	nvme_id_ns_nsze(ns_grp, test->nsid, &test->nsze);

	BUG_ON(test->nsid >> 16);

	snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
		"Send vendor command(0x%x): Set Param - NSID 0x%x, "
		"FTL_IF_ID 0x%x", nvme_admin_maxio_nvme_hwrdma, 
		test->nsid, ftl_if);
	json_add_content_to_step_node(step_node, test->rpt_buf);

	ret = nvme_maxio_nvme_hwrdma_set_param(ndev, BIT(2), 
		((test->nsid & 0xffff) << 16) | (ftl_if & 0xffff));
	if (ret < 0)
		goto out;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		ret = -ENODEV;
		goto out;
	}

	ret = create_ioq(ndev, sq, cq, step_node);
	if (ret < 0)
		goto out;

	for (i = 0; i < 12; i++) {
		slba = rand() % (test->nsze / 2);

		ret = send_io_read_cmd(tool, sq, slba, nlb, step_node);
		if (ret < 0)
			goto del_ioq;
	}
	snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
		"Send vendor command(0x%x): Check Result",
		nvme_admin_maxio_nvme_hwrdma);
	json_add_content_to_step_node(step_node, test->rpt_buf);

	ret = nvme_maxio_nvme_hwrdma_check_result(ndev, BIT(2));
	if (ret < 0)
		goto del_ioq;

del_ioq:
	ret |= delete_ioq(ndev, sq, cq, step_node);
out:
	json_add_result_node(case_node, ret);
	return ret;
}
NVME_CASE_SYMBOL(case_hw_rdma_ftl_if_namespace_bind, "?");

static int case_hw_wdma_ftl_size_limit(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct test_data *test = &g_test;
	struct json_node *case_node;
	struct json_node *step_node;
	uint64_t slba;
	uint32_t nlb = 1;
	uint32_t pending = rand() % 0x10 + 1; /* 1 ~ 0x10 */
	int i;
	int ret;

	case_node = json_add_case_node(json_get_case_set(tool->report), 
		__func__, false);
	step_node = json_add_step_node(case_node);

	ret = init_test_data(tool, test);
	if (ret < 0)
		goto out;

	snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
		"Send vendor command(0x%x): Set Param - pending num %u",
		nvme_admin_maxio_nvme_hwwdma, pending);
	json_add_content_to_step_node(step_node, test->rpt_buf);

	ret = nvme_maxio_nvme_hwwdma_set_param(ndev, BIT(0), pending);
	if (ret < 0)
		goto out;

	msleep(1000);

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		ret = -ENODEV;
		goto out;
	}

	ret = create_ioq(ndev, sq, cq, step_node);
	if (ret < 0)
		goto out;

	for (i = 0; i < (2 * pending); i++) {
		slba = rand() % (test->nsze / 2);

		ret = send_io_write_cmd(tool, sq, slba, nlb, step_node);
		if (ret < 0)
			goto del_ioq;
	}

	snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
		"Send vendor command(0x%x): Check Result",
		nvme_admin_maxio_nvme_hwwdma);
	json_add_content_to_step_node(step_node, test->rpt_buf);

	ret = nvme_maxio_nvme_hwwdma_check_result(ndev, BIT(0));
	if (ret < 0)
		goto del_ioq;

del_ioq:
	ret |= delete_ioq(ndev, sq, cq, step_node);
out:
	json_add_result_node(case_node, ret);
	return ret;
}
NVME_CASE_SYMBOL(case_hw_wdma_ftl_size_limit, "?");

static int case_hw_wdma_ftl_wreq_if_en(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct test_data *test = &g_test;
	struct json_node *case_node;
	struct json_node *step_node;
	uint64_t slba;
	uint32_t nlb = 1;
	/* bit[5:0] each bit stand a FTL IF(1: enable) */
	uint32_t interface[6] = {0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f};
	int i, j;
	int ret;

	case_node = json_add_case_node(json_get_case_set(tool->report), 
		__func__, false);
	step_node = json_add_step_node(case_node);

	ret = init_test_data(tool, test);
	if (ret < 0)
		goto out;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		ret = -ENODEV;
		goto out;
	}

	ret = create_ioq(ndev, sq, cq, step_node);
	if (ret < 0)
		goto out;

	for (j = 0; j < ARRAY_SIZE(interface); j++) {
		snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
			"Send vendor command(0x%x): Set Param - FTL_IF_MAP 0x%x",
			nvme_admin_maxio_nvme_hwwdma, interface[j]);
		json_add_content_to_step_node(step_node, test->rpt_buf);

		ret = nvme_maxio_nvme_hwwdma_set_param(ndev, BIT(1), interface[j]);
		if (ret < 0)
			goto del_ioq;

		msleep(1000);

		for (i = 0; i < 12; i++) {
			slba = rand() % (test->nsze / 2);

			ret = send_io_read_cmd(tool, sq, slba, nlb, step_node);
			if (ret < 0)
				goto del_ioq;
		}
		snprintf(test->rpt_buf, sizeof(test->rpt_buf), 
			"Send vendor command(0x%x): Check Result",
			nvme_admin_maxio_nvme_hwwdma);
		json_add_content_to_step_node(step_node, test->rpt_buf);

		ret = nvme_maxio_nvme_hwwdma_check_result(ndev, BIT(1));
		if (ret < 0)
			goto del_ioq;
	}

del_ioq:
	ret |= delete_ioq(ndev, sq, cq, step_node);
out:
	json_add_result_node(case_node, ret);
	return ret;
}
NVME_CASE_SYMBOL(case_hw_wdma_ftl_wreq_if_en, "?");
