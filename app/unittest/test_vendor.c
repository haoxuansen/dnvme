/**
 * @file test_vendor.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-10-30
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"
#include "test.h"

#define SUBCMD(x)			BIT((x))

/**
 * @brief Check whether hardware I/O command timeout feature takes effect
 * 
 * @return 0 on success, otherwise a negative errno 
 * 
 */
static int case_hw_io_cmd_timeout_check(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	int ret;
	
	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_nvme_top);

	ret = nvme_maxio_nvme_top_set_param(ndev, SUBCMD(0), 0);
	if (ret < 0)
		return ret;

	ut_rpt_record_case_step(rpt, "Sleep 1s");
	msleep(1000);

	ret = ut_create_pair_io_queue(priv, sq, NULL);
	if (ret < 0)
		return ret;

	ret = ut_send_io_random_cmd(priv, sq, UT_CMD_SEL_IO_READ | 
		UT_CMD_SEL_IO_WRITE);
	if (ret < 0)
		goto out;

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_nvme_top);
	ret = nvme_maxio_nvme_top_check_result(ndev, SUBCMD(0));
	if (ret < 0)
		goto out;
out:
	ret |= ut_delete_pair_io_queue(priv, sq, NULL);
	return ret;
}
NVME_CASE_SYMBOL(case_hw_io_cmd_timeout_check, "?");

#if IS_ENABLED(CONFIG_DEBUG_NO_DEVICE)
static int case_cq_full_threshold_limit_sq_fetch_cmd(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct case_report *rpt = &priv->rpt;
	uint32_t nr_entry = rand() % 10 + 1;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command(0x%x): Set Param - CQ entry num %u",
		nvme_admin_maxio_nvme_cqm, nr_entry);
	ut_rpt_record_case_step(rpt, 
		"Send vendor command(0x%x): Check Result",
		nvme_admin_maxio_nvme_cqm);
	
	return 0;
}
NVME_CASE_SYMBOL(case_cq_full_threshold_limit_sq_fetch_cmd, "?");
#else
/**
 * @brief Limit the number of commands fetched from SQ by setting a threshold
 * 
 * @return 0 on success, otherwise a negative errno 
 */
static int case_cq_full_threshold_limit_sq_fetch_cmd(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	uint32_t nr_entry = rand() % 10 + 1;
	uint32_t nr_cmd = min_t(uint32_t, sq->nr_entry, 50);
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param(CQ size:%u)",
		nvme_admin_maxio_nvme_cqm, nr_entry);
	ret = nvme_maxio_nvme_cqm_set_param(ndev, SUBCMD(0), nr_entry);
	if (ret < 0)
		return ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	if (cq->nr_entry != nr_entry)
		swap(cq->nr_entry, nr_entry);
	
	ret = ut_create_pair_io_queue(priv, sq, cq);
	if (ret < 0)
		goto restore;

	ret = ut_submit_io_random_cmds(priv, sq, UT_CMD_SEL_IO_READ | 
		UT_CMD_SEL_IO_WRITE, nr_cmd);
	if (ret < 0)
		goto del_ioq;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0)
		goto del_ioq;
	
	ret = ut_reap_cq_entry_no_check(priv, cq, nr_cmd);
	if (ret < 0)
		goto del_ioq;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_nvme_cqm);
	ret = nvme_maxio_nvme_cqm_check_result(ndev, SUBCMD(0));
	if (ret < 0)
		goto del_ioq;

del_ioq:
	ret |= ut_delete_pair_io_queue(priv, sq, cq);
restore:
	/* restore the value avoid affecting next test */
	if (cq->nr_entry != nr_entry)
		swap(cq->nr_entry, nr_entry);
	return ret;
}
NVME_CASE_SYMBOL(case_cq_full_threshold_limit_sq_fetch_cmd, "?");
#endif

static int case_hw_rdma_ftl_size_limit(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	uint32_t pending = rand() % 10 + 1; /* 1 ~ 10 */
	uint32_t nr_cmd = 3 * pending;
	int ret;
	
	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);
	effect.cmd.read.use_nlb = 1;
	effect.cmd.read.nlb = 1;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: %u",
		nvme_admin_maxio_nvme_hwrdma, pending);
	
	ret = nvme_maxio_nvme_hwrdma_set_param(ndev, SUBCMD(0), pending);
	if (ret < 0)
		return ret;

	ut_rpt_record_case_step(rpt, "Sleep 1s");
	msleep(1000);

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	ret = ut_create_pair_io_queue(priv, sq, cq);
	if (ret < 0)
		return ret;

	ret = ut_submit_io_read_cmds_random_region(priv, sq, nr_cmd);
	if (ret < 0)
		goto del_ioq;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0)
		goto del_ioq;

	ret = ut_reap_cq_entry_no_check(priv, cq, nr_cmd);
	if (ret < 0)
		goto del_ioq;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_nvme_hwrdma);
	ret = nvme_maxio_nvme_hwrdma_check_result(ndev, SUBCMD(0));
	if (ret < 0)
		goto del_ioq;

del_ioq:
	ret |= ut_delete_pair_io_queue(priv, sq, cq);
	return ret;
}
NVME_CASE_SYMBOL(case_hw_rdma_ftl_size_limit, "?");

static int case_hw_rdma_ftl_rreq_if_en(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	/* bit[5:0] each bit stand a FTL IF(1: enable) */
	uint32_t interface[6] = {0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f};
	int i;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);
	effect.cmd.read.use_nlb = 1;
	effect.cmd.read.nlb = 1;

	ret = ut_create_pair_io_queue(priv, sq, NULL);
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(interface); i++) {
		ut_rpt_record_case_step(rpt, 
			"Send vendor command:0x%x - Set Param: 0x%x",
			nvme_admin_maxio_nvme_hwrdma, interface[i]);
		ret = nvme_maxio_nvme_hwrdma_set_param(ndev, SUBCMD(1), interface[i]);
		if (ret < 0)
			goto del_ioq;

		ut_rpt_record_case_step(rpt, "Sleep 1s");
		msleep(1000);

		ret = ut_send_io_read_cmds_random_region(priv, sq, 12);
		if (ret < 0)
			goto del_ioq;
		
		ut_rpt_record_case_step(rpt, 
			"Send vendor command:0x%x - Check Result",
			nvme_admin_maxio_nvme_hwrdma);
		ret = nvme_maxio_nvme_hwrdma_check_result(ndev, SUBCMD(1));
		if (ret < 0)
			goto del_ioq;
	}

del_ioq:
	ret |= ut_delete_pair_io_queue(priv, sq, NULL);
	return ret;
}
NVME_CASE_SYMBOL(case_hw_rdma_ftl_rreq_if_en, "?");

static int case_hw_rdma_ftl_if_namespace_bind(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	uint32_t idx = rand() % ns_grp->nr_act;
	uint32_t ftl_if = rand() % 6 + 1;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[idx]);
	effect.cmd.read.use_nlb = 1;
	effect.cmd.read.nlb = 1;

	/* param only allocate 16bit for nsid */
	BUG_ON(effect.nsid >> 16);

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: NSID 0x%x,"
		" FTL interface 0x%x",
		nvme_admin_maxio_nvme_hwrdma, effect.nsid, ftl_if);
	ret = nvme_maxio_nvme_hwrdma_set_param(ndev, SUBCMD(2), 
		((effect.nsid & 0xffff) << 16) | (ftl_if & 0xffff));
	if (ret < 0)
		return ret;

	ret = ut_create_pair_io_queue(priv, sq, NULL);
	if (ret < 0)
		return ret;

	ret = ut_send_io_read_cmds_random_region(priv, sq, 12);
	if (ret < 0)
		goto del_ioq;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_nvme_hwrdma);
	ret = nvme_maxio_nvme_hwrdma_check_result(ndev, SUBCMD(2));
	if (ret < 0)
		goto del_ioq;

del_ioq:
	ret |= ut_delete_pair_io_queue(priv, sq, NULL);
	return ret;
}
NVME_CASE_SYMBOL(case_hw_rdma_ftl_if_namespace_bind, "?");

static int case_hw_wdma_ftl_size_limit(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	uint32_t pending = rand() % 0x10 + 1; /* 1 ~ 0x10 */
	uint32_t nr_cmd = 3 * pending;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);
	effect.cmd.write.use_nlb = 1;
	effect.cmd.write.nlb = 1;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: %u,"
		" FTL interface 0x%x",
		nvme_admin_maxio_nvme_hwwdma, pending);
	ret = nvme_maxio_nvme_hwwdma_set_param(ndev, SUBCMD(0), pending);
	if (ret < 0)
		return ret;

	ut_rpt_record_case_step(rpt, "Sleep 1s");
	msleep(1000);

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	ret = ut_create_pair_io_queue(priv, sq, cq);
	if (ret < 0)
		return ret;

	ret = ut_submit_io_write_cmds_random_d_r(priv, sq, nr_cmd);
	if (ret < 0)
		goto del_ioq;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0)
		goto del_ioq;

	ret = ut_reap_cq_entry_no_check(priv, cq, nr_cmd);
	if (ret < 0)
		goto del_ioq;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_nvme_hwwdma);
	ret = nvme_maxio_nvme_hwwdma_check_result(ndev, SUBCMD(0));
	if (ret < 0)
		goto del_ioq;

del_ioq:
	ret |= ut_delete_pair_io_queue(priv, sq, cq);
	return ret;
}
NVME_CASE_SYMBOL(case_hw_wdma_ftl_size_limit, "?");

static int case_hw_wdma_ftl_wreq_if_en(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	/* bit[5:0] each bit stand a FTL IF(1: enable) */
	uint32_t interface[6] = {0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f};
	int i;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);
	effect.cmd.write.use_nlb = 1;
	effect.cmd.write.nlb = 1;

	ret = ut_create_pair_io_queue(priv, sq, NULL);
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(interface); i++) {
		ut_rpt_record_case_step(rpt, 
			"Send vendor command:0x%x - Set Param: 0x%x",
			nvme_admin_maxio_nvme_hwwdma, interface[i]);
		ret = nvme_maxio_nvme_hwwdma_set_param(ndev, SUBCMD(1), interface[i]);
		if (ret < 0)
			goto del_ioq;

		ut_rpt_record_case_step(rpt, "Sleep 1s");
		msleep(1000);

		ret = ut_send_io_write_cmds_random_d_r(priv, sq, 12);
		if (ret < 0)
			goto del_ioq;

		ut_rpt_record_case_step(rpt, 
			"Send vendor command:0x%x - Check Result",
			nvme_admin_maxio_nvme_hwwdma);
		ret = nvme_maxio_nvme_hwwdma_check_result(ndev, SUBCMD(1));
		if (ret < 0)
			goto del_ioq;
	}

del_ioq:
	ret |= ut_delete_pair_io_queue(priv, sq, NULL);
	return ret;
}
NVME_CASE_SYMBOL(case_hw_wdma_ftl_wreq_if_en, "?");

static int __case_wrr_with_urgent_priority_class_arbitration(
	struct nvme_tool *tool, struct case_data *priv, int time)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	struct nvme_feat_arb_wrapper arb = {0};
	void *buf;
	uint32_t max_pair = min_t(uint32_t, ctrl->nr_sq, ctrl->nr_cq);
	uint32_t nr_cmd = rand() % 100 + 1; /* 1 ~ 100 */
	uint32_t nr_pair = rand() % min_t(uint32_t, max_pair, 64) + 1;
	int i;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = nr_pair;
	param.dw11 = rand() % 2; /* 0 or 1 */

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: nr_pair_queue %u,"
		" analyse progress %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(0), &param);
	if (ret < 0) 
		return ret;

	arb.hpw = rand() % 101; /* 0 ~ 100 */
	arb.mpw = rand() % 101;
	arb.lpw = rand() % 101;
	arb.burst = rand() % 7; /* 0 ~ 6 => 1 ~ 64 */

	ret = nvme_set_feat_arbitration(ndev, &arb);
	if (ret < 0)
		return ret;

	ret = ut_create_pair_io_queues(priv, ndev->iosqs, NULL, nr_pair);
	if (ret < 0)
		return ret;

	ret = ut_submit_io_random_cmds_to_sqs(priv, ndev->iosqs, 
		UT_CMD_SEL_ALL, nr_cmd, nr_pair);
	if (ret < 0)
		goto del_ioq;

	ret = ut_ring_sqs_doorbell(priv, ndev->iosqs, nr_pair);
	if (ret < 0)
		goto del_ioq;
	
	for (i = 0; i < nr_pair; i++) {
		ret = ut_reap_cq_entry_no_check_by_id(priv, 
			ndev->iosqs[i].cqid, nr_cmd);
		if (ret < 0)
			goto del_ioq;
	}

	ret = posix_memalign(&buf, NVME_TOOL_RW_BUF_ALIGN, SZ_16K);
	if (ret) {
		pr_err("failed to alloc buf!\n");
		ret = -ENOMEM;
		goto del_ioq;
	}

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_nvme_case);
	ret = nvme_maxio_nvme_case_check_result(ndev, SUBCMD(0), buf, SZ_16K);
	if (ret < 0)
		goto rel_mem;

	dump_data_to_fmt_file(buf, SZ_16K, "%s/%s-%d.bin", 
		NVME_TOOL_LOG_FILE_PATH, __func__, time);

rel_mem:
	free(buf);
del_ioq:
	ret |= ut_delete_pair_io_queues(priv, ndev->iosqs, NULL, param.dw3);
	return ret;
}

static int case_wrr_with_urgent_priority_class_arbitration(
	struct nvme_tool *tool, struct case_data *priv)
{
	struct case_report *rpt = &priv->rpt;
	int loop = 1;
	int ret;

	for (; loop <= 10; loop++) {
		ut_rpt_record_case_step(rpt, "loop %d...", loop);

		ret = __case_wrr_with_urgent_priority_class_arbitration(
			tool, priv, loop);
		if (ret < 0)
			return ret;
	}

	return 0;
}
NVME_CASE_SYMBOL(case_wrr_with_urgent_priority_class_arbitration, "?");

static int cmd_sanity_check_according_by_protocol_sn1(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 1;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.cmd.compare.flags = NVME_CMD_FUSE_FIRST;
	ret = ut_submit_io_compare_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;
	
	memset(&effect.cmd, 0, sizeof(effect.cmd));
	effect.cmd.write.flags = NVME_CMD_FUSE_SECOND;
	ret = ut_submit_io_write_cmd_random_d_r(priv, sq);
	if (ret < 0)
		return ret;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0)
		return ret;
	
	ret = ut_reap_cq_entry_no_check_by_id(priv, sq->cqid, 2);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn2(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 2;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;
	
	effect.cmd.write.flags = NVME_CMD_FUSE_FIRST;
	ret = ut_submit_io_write_cmd_random_d_r(priv, sq);
	if (ret < 0)
		return ret;

	memset(&effect.cmd, 0, sizeof(effect.cmd));
	effect.cmd.compare.flags = NVME_CMD_FUSE_SECOND;
	ret = ut_submit_io_compare_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0)
		return ret;
	
	ret = ut_reap_cq_entry_no_check_by_id(priv, sq->cqid, 2);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn3(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 3;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.cmd.compare.flags = NVME_CMD_FUSE_FIRST;
	ret = ut_submit_io_compare_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;
	
	memset(&effect.cmd, 0, sizeof(effect.cmd));
	ret = ut_submit_io_read_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0)
		return ret;
	
	ret = ut_reap_cq_entry_no_check_by_id(priv, sq->cqid, 2);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn4(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 4;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	ret = ut_submit_io_read_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;
	
	effect.cmd.write.flags = NVME_CMD_FUSE_SECOND;
	ret = ut_submit_io_write_cmd_random_d_r(priv, sq);
	if (ret < 0)
		return ret;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0)
		return ret;
	
	ret = ut_reap_cq_entry_no_check_by_id(priv, sq->cqid, 2);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn5(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 5;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.cmd.compare.flags = NVME_CMD_FUSE_FIRST;
	ret = ut_submit_io_compare_cmd(priv, sq, 0, 1);
	if (ret < 0)
		return ret;
	
	memset(&effect.cmd, 0, sizeof(effect.cmd));
	effect.cmd.write.flags = NVME_CMD_FUSE_SECOND;
	ret = ut_submit_io_write_cmd(priv, sq, 1, 1);
	if (ret < 0)
		return ret;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0)
		return ret;
	
	ret = ut_reap_cq_entry_no_check_by_id(priv, sq->cqid, 2);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn6(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 6;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.cmd.compare.flags = NVME_CMD_FUSE_FIRST;
	effect.cmd.compare.use_nlb = 1;
	effect.cmd.compare.nlb = 16;
	ret = ut_submit_io_compare_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;
	
	memset(&effect.cmd, 0, sizeof(effect.cmd));
	effect.cmd.write.flags = NVME_CMD_FUSE_SECOND;
	effect.cmd.write.use_nlb = 1;
	effect.cmd.write.nlb = 16;
	ret = ut_submit_io_write_cmd_random_d_r(priv, sq);
	if (ret < 0)
		return ret;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0)
		return ret;
	
	ret = ut_reap_cq_entry_no_check_by_id(priv, sq->cqid, 2);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn7(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 7;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.cmd.compare.flags = NVME_CMD_FUSE_FIRST;
	ret = ut_submit_io_compare_cmd(priv, sq, 0, 10);
	if (ret < 0)
		return ret;

	memset(&effect.cmd, 0, sizeof(effect.cmd));
	effect.cmd.write.flags = NVME_CMD_FUSE_SECOND;
	ret = ut_submit_io_write_cmd(priv, sq, 0, 10);
	if (ret < 0)
		return ret;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0)
		return ret;
	
	ret = ut_reap_cq_entry_no_check_by_id(priv, sq->cqid, 2);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn8(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 8;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.cmd.write.flags = NVME_CMD_FUSE_SECOND;
	ret = ut_submit_io_write_cmd_random_d_r(priv, sq);
	if (ret < 0)
		return ret;
	
	memset(&effect.cmd, 0, sizeof(effect.cmd));
	effect.cmd.compare.flags = NVME_CMD_FUSE_FIRST;
	ret = ut_submit_io_compare_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0)
		return ret;
	
	ret = ut_reap_cq_entry_no_check_by_id(priv, sq->cqid, 2);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn9(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 9;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.cmd.copy.use_fmt = 1;
	effect.cmd.copy.fmt = NVME_COPY_DESC_FMT_32B;
	effect.check_none = 1;
	ret = ut_send_io_copy_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn10(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 10;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.cmd.copy.use_fmt = 1;
	effect.cmd.copy.fmt = NVME_COPY_DESC_FMT_40B;
	effect.check_none = 1;
	ret = ut_send_io_copy_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn11(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 11;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.cmd.copy.use_fmt = 1;
	effect.cmd.copy.fmt = NVME_COPY_DESC_FMT_32B;
	effect.cmd.copy.use_desc = 1;
	effect.cmd.copy.nr_desc = 11;
	effect.check_none = 1;
	ret = ut_send_io_copy_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn12(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 12;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.cmd.copy.use_fmt = 1;
	effect.cmd.copy.fmt = NVME_COPY_DESC_FMT_40B;
	effect.check_none = 1;
	ret = ut_send_io_copy_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn13(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 13;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.cmd.copy.use_fmt = 1;
	effect.cmd.copy.fmt = NVME_COPY_DESC_FMT_32B;
	effect.check_none = 1;
	ret = ut_send_io_copy_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn14(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 14;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.cmd.copy.prinfow = 0x8; /* set PRACT = 1*/
	effect.check_none = 1;
	ret = ut_send_io_copy_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn15(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 15;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.cmd.copy.prinfor = 0x8; /* set PRACT = 1*/
	effect.check_none = 1;
	ret = ut_send_io_copy_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn16(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 16;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	ret = ut_send_io_copy_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn17(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 17;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.cmd.copy.use_fmt = 1;
	effect.cmd.copy.fmt = NVME_COPY_DESC_FMT_32B;
	effect.check_none = 1;
	ret = ut_send_io_copy_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn18(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	struct copy_resource *copy = NULL;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 18;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	copy = ut_alloc_copy_resource(1, NVME_COPY_DESC_FMT_32B);
	if (!copy)
		return -ENOMEM;

	copy->entry[0].slba = 0;
	copy->entry[0].nlb = 51;
	copy->slba = 51;
	effect.check_none = 1;
	ret = ut_send_io_copy_cmd(priv, sq, copy);
	if (ret < 0)
		goto out;

out:
	ut_release_copy_resource(copy);
	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn19(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	struct copy_resource *copy = NULL;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 19;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	copy = ut_alloc_copy_resource(2, NVME_COPY_DESC_FMT_32B);
	if (!copy)
		return -ENOMEM;

	copy->entry[0].slba = 0;
	copy->entry[0].nlb = 30;
	copy->entry[1].slba = 30;
	copy->entry[1].nlb = 30;
	copy->slba = 60;
	effect.check_none = 1;
	ret = ut_send_io_copy_cmd(priv, sq, copy);
	if (ret < 0)
		goto out;

out:
	ut_release_copy_resource(copy);
	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn20(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 20;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.cmd.copy.use_fmt = 1;
	effect.cmd.copy.fmt = NVME_COPY_DESC_FMT_32B;
	effect.check_none = 1;
	ret = ut_send_io_copy_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn21(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	struct copy_resource *copy = NULL;
	uint64_t nsze;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 21;

	ret = nvme_id_ns_nsze(ns_grp, effect.nsid, &nsze);
	if (ret < 0)
		return ret;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	copy = ut_alloc_copy_resource(1, NVME_COPY_DESC_FMT_32B);
	if (!copy)
		return -ENOMEM;

	copy->entry[0].slba = nsze - 1;
	copy->entry[0].nlb = 8;
	copy->slba = 0;
	effect.check_none = 1;
	ret = ut_send_io_copy_cmd(priv, sq, copy);
	if (ret < 0)
		goto out;

out:
	ut_release_copy_resource(copy);
	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn22(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 22;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	ret = ut_send_io_verify_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn23(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 23;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	effect.cmd.verify.use_nlb = 1;
	effect.cmd.verify.nlb = 17;
	ret = ut_send_io_verify_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn24(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 24;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	effect.cmd.verify.prinfo = 0x8; /* set PRACT = 1 */
	ret = ut_send_io_verify_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn25(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 25;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	ret = ut_send_io_compare_cmd_random_region(priv, sq);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn26(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 26;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	switch (rand() % 5) {
	case 0:
		effect.cmd.read.flags = NVME_CMD_SGL_METABUF;
		ret = ut_send_io_read_cmd_random_region(priv, sq);
		break;
	case 1:
		effect.cmd.write.flags = NVME_CMD_SGL_METABUF;
		ret = ut_send_io_write_cmd_random_d_r(priv, sq);
		break;
	case 2:
		effect.cmd.compare.flags = NVME_CMD_SGL_METABUF;
		ret = ut_send_io_compare_cmd_random_region(priv, sq);
		break;
	case 3:
		effect.cmd.copy.flags = NVME_CMD_SGL_METABUF;
		ret = ut_send_io_copy_cmd_random_region(priv, sq);
		break;
	case 4:
		effect.cmd.fused.flags = NVME_CMD_SGL_METABUF;
		ret = ut_send_io_fused_cw_cmd_random_region(priv, sq);
		break;
	}

	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn27(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 27;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	switch (rand() % 5) {
	case 0:
		effect.cmd.read.flags = NVME_CMD_SGL_METASEG;
		ret = ut_send_io_read_cmd_random_region(priv, sq);
		break;
	case 1:
		effect.cmd.write.flags = NVME_CMD_SGL_METASEG;
		ret = ut_send_io_write_cmd_random_d_r(priv, sq);
		break;
	case 2:
		effect.cmd.compare.flags = NVME_CMD_SGL_METASEG;
		ret = ut_send_io_compare_cmd_random_region(priv, sq);
		break;
	case 3:
		effect.cmd.copy.flags = NVME_CMD_SGL_METASEG;
		ret = ut_send_io_copy_cmd_random_region(priv, sq);
		break;
	case 4:
		effect.cmd.fused.flags = NVME_CMD_SGL_METASEG;
		ret = ut_send_io_fused_cw_cmd_random_region(priv, sq);
		break;
	}

	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn28(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 28;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	ret = ut_send_io_random_cmd(priv, sq, UT_CMD_SEL_IO_READ |
		UT_CMD_SEL_IO_WRITE | UT_CMD_SEL_IO_COMPARE |
		UT_CMD_SEL_IO_COPY | UT_CMD_SEL_IO_FUSED_CW |
		UT_CMD_SEL_IO_VERIFY);
	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn29(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 29;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	ret = ut_send_io_random_cmd(priv, sq, UT_CMD_SEL_IO_READ |
		UT_CMD_SEL_IO_WRITE | UT_CMD_SEL_IO_COMPARE |
		UT_CMD_SEL_IO_COPY | UT_CMD_SEL_IO_FUSED_CW |
		UT_CMD_SEL_IO_VERIFY);
	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn30(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 30;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	switch (rand() % 3) {
	case 0:
		ret = ut_send_io_write_cmd_random_d_r(priv, sq);
		break;
	case 1:
		ret = ut_send_io_copy_cmd_random_region(priv, sq);
		break;
	case 2:
		ret = ut_send_io_zone_append_cmd(priv, sq, 0, 1);
		break;
	}

	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn31(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	struct copy_resource *copy = NULL;
	uint64_t nsze;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 31;

	ret = nvme_id_ns_nsze(ns_grp, effect.nsid, &nsze);
	if (ret < 0)
		return ret;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	switch (rand() % 6) {
	case 0:
		ret = ut_send_io_read_cmd(priv, sq, nsze - 1, 8);
		break;
	case 1:
		ret = ut_send_io_write_cmd(priv, sq, nsze -1, 8);
		break;
	case 2:
		ret = ut_send_io_compare_cmd(priv, sq, nsze - 1, 8);
		break;
	case 3:
		copy = ut_alloc_copy_resource(1, NVME_COPY_DESC_FMT_32B);
		if (!copy)
			return -ENOMEM;
		
		copy->slba = 0;
		copy->entry[0].slba = nsze - 1;
		copy->entry[0].nlb = 8;
		ret = ut_send_io_copy_cmd(priv, sq, copy);
		ut_release_copy_resource(copy);
		break;
	case 4:
		ret = ut_send_io_fused_cw_cmd(priv, sq, nsze - 1, 8);
		break;
	case 5:
		ret = ut_send_io_verify_cmd(priv, sq, nsze - 1, 8);
		break;
	}

	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn32(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 32;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	effect.inject_nsid = 1;
	effect.inject.nsid = 17;
	ret = ut_send_io_random_cmd(priv, sq, UT_CMD_SEL_IO_READ |
		UT_CMD_SEL_IO_WRITE | UT_CMD_SEL_IO_COMPARE |
		UT_CMD_SEL_IO_COPY | UT_CMD_SEL_IO_FUSED_CW |
		UT_CMD_SEL_IO_VERIFY);
	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn33(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 33;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	switch (rand() % 5) {
	case 0:
		effect.cmd.read.flags = NVME_CMD_SGL_METABUF;
		ret = ut_send_io_read_cmd_random_region(priv, sq);
		break;
	case 1:
		effect.cmd.write.flags = NVME_CMD_SGL_METABUF;
		ret = ut_send_io_write_cmd_random_d_r(priv, sq);
		break;
	case 2:
		effect.cmd.compare.flags = NVME_CMD_SGL_METABUF;
		ret = ut_send_io_compare_cmd_random_region(priv, sq);
		break;
	case 3:
		effect.cmd.copy.flags = NVME_CMD_SGL_METABUF;
		ret = ut_send_io_copy_cmd_random_region(priv, sq);
		break;
	case 4:
		effect.cmd.fused.flags = NVME_CMD_SGL_METABUF;
		ret = ut_send_io_fused_cw_cmd_random_region(priv, sq);
		break;
	}

	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn34(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 34;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	effect.inject_prp = 1;
	effect.inject.prp1 = 1;
	effect.inject.prp2 = 1;
	ret = ut_send_io_random_cmd(priv, sq, UT_CMD_SEL_IO_READ |
		UT_CMD_SEL_IO_WRITE | UT_CMD_SEL_IO_COMPARE |
		UT_CMD_SEL_IO_COPY | UT_CMD_SEL_IO_FUSED_CW);
	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn35(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 35;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;

	return ut_send_io_zone_append_cmd(priv, sq, 0, 1);
}

static int cmd_sanity_check_according_by_protocol_sn36(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 36;
	param.dw12 = 1;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u, dw12 %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11, param.dw12);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	effect.cmd.zone_append.piremap = 0;
	ret = ut_send_io_zone_append_cmd(priv, sq, 0, 1);
	if (ret < 0)
		return ret;
	
	param.dw12 = 2;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u, dw12 %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11, param.dw12);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;
	
	effect.cmd.zone_append.piremap = 1;
	ret = ut_send_io_zone_append_cmd(priv, sq, 0, 1);
	if (ret < 0)
		return ret;

	return 0;
}

static int cmd_sanity_check_according_by_protocol_sn37(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 37;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	switch (rand() % 2) {
	case 0:
		effect.cmd.write.dtype = NVME_DIR_STREAMS;
		ret = ut_send_io_write_cmd_random_d_r(priv, sq);
		break;
	case 1:
		effect.cmd.copy.dtype = NVME_DIR_STREAMS;
		ret = ut_send_io_copy_cmd_random_region(priv, sq);
		break;
	}
	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn38(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 38;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	switch (rand() % 2) {
	case 0:
		effect.cmd.write.dtype = NVME_DIR_STREAMS;
		effect.cmd.write.dspec = 201;
		ret = ut_send_io_write_cmd_random_d_r(priv, sq);
		break;
	case 1:
		effect.cmd.copy.dtype = NVME_DIR_STREAMS;
		effect.cmd.copy.dspec = 201;
		ret = ut_send_io_copy_cmd_random_region(priv, sq);
		break;
	}

	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn39(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 39;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	switch (rand() % 2) {
	case 0:
		effect.cmd.write.dtype = NVME_DIR_STREAMS;
		effect.cmd.write.dspec = 100;
		ret = ut_send_io_write_cmd_random_d_r(priv, sq);
		break;
	case 1:
		effect.cmd.copy.dtype = NVME_DIR_STREAMS;
		effect.cmd.copy.dspec = 100;
		ret = ut_send_io_copy_cmd_random_region(priv, sq);
		break;
	}

	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn40(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 40;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	ret = ut_send_io_random_cmd(priv, sq, UT_CMD_SEL_IO_READ |
		UT_CMD_SEL_IO_WRITE | UT_CMD_SEL_IO_COMPARE |
		UT_CMD_SEL_IO_COPY | UT_CMD_SEL_IO_FUSED_CW);
	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn41(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 41;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	ret = ut_send_io_random_cmd(priv, sq, UT_CMD_SEL_IO_READ |
		UT_CMD_SEL_IO_WRITE | UT_CMD_SEL_IO_COMPARE |
		UT_CMD_SEL_IO_COPY | UT_CMD_SEL_IO_FUSED_CW);
	return ret;
}

static int cmd_sanity_check_according_by_protocol_sn42(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;
	param.dw11 = 42;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u, sn %u",
		nvme_admin_maxio_nvme_case, param.dw3, param.dw11);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(1), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	ret = ut_send_io_random_cmd(priv, sq, UT_CMD_SEL_IO_READ |
		UT_CMD_SEL_IO_WRITE | UT_CMD_SEL_IO_COMPARE |
		UT_CMD_SEL_IO_COPY | UT_CMD_SEL_IO_FUSED_CW);
	return ret;
}

static int __case_cmd_sanity_check_according_by_protocol(
	struct nvme_tool *tool, struct case_data *priv, int time)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	void *buf;
	int ret;

	ret = ut_create_pair_io_queue(priv, sq, NULL);
	if (ret < 0)
		return ret;

	ret |= cmd_sanity_check_according_by_protocol_sn1(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn2(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn3(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn4(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn5(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn6(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn7(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn8(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn9(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn10(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn11(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn12(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn13(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn14(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn15(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn16(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn17(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn18(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn19(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn20(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn21(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn22(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn23(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn24(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn25(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn26(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn27(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn28(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn29(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn30(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn31(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn32(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn33(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn34(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn35(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn36(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn37(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn38(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn39(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn40(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn41(priv, sq);
	ret |= cmd_sanity_check_according_by_protocol_sn42(priv, sq);
	if (ret < 0)
		goto del_ioq;

	ret = posix_memalign(&buf, NVME_TOOL_RW_BUF_ALIGN, SZ_16K);
	if (ret) {
		pr_err("failed to alloc buf!\n");
		ret = -ENOMEM;
		goto del_ioq;
	}

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_nvme_case);
	ret = nvme_maxio_nvme_case_check_result(ndev, SUBCMD(1), buf, SZ_16K);
	if (ret < 0)
		goto rel_mem;

	dump_data_to_fmt_file(buf, SZ_16K, "%s/%s-%d.bin",
		NVME_TOOL_LOG_FILE_PATH, __func__, time);

rel_mem:
	free(buf);
del_ioq:
	ret |= ut_delete_pair_io_queue(priv, sq, NULL);
	return ret;
}

/**
 * @brief Verify that the controller processing command is valid
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int case_cmd_sanity_check_according_by_protocol(
	struct nvme_tool *tool, struct case_data *priv)
{
	struct case_report *rpt = &priv->rpt;
	int loop = 1;
	int ret;

	for (; loop <= 10; loop++) {
		ut_rpt_record_case_step(rpt, "loop %d...", loop);

		ret = __case_cmd_sanity_check_according_by_protocol(
			tool, priv, loop);
		if (ret < 0)
			return ret;
	}
	return 0;
}
NVME_CASE_SYMBOL(case_cmd_sanity_check_according_by_protocol, "?");

static int ftl_interface_selectable_by_multi_mode_subcase1(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 1;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u",
		nvme_admin_maxio_nvme_case, param.dw3);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(2), &param);
	if (ret < 0)
		return ret;
	
	effect.check_none = 1;
	return ut_send_io_random_cmd(priv, sq, UT_CMD_SEL_IO_READ | 
		UT_CMD_SEL_IO_WRITE | UT_CMD_SEL_IO_COMPARE |
		UT_CMD_SEL_IO_COPY | UT_CMD_SEL_IO_FUSED_CW);
}

static int ftl_interface_selectable_by_multi_mode_subcase2(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	uint32_t i;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 2;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u",
		nvme_admin_maxio_nvme_case, param.dw3);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(2), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	effect.inject_nsid = 1;
	for (i = 1; i <= 16; i++) {
		effect.inject.nsid = i;
		ret = ut_send_io_random_cmd(priv, sq, UT_CMD_SEL_IO_READ | 
			UT_CMD_SEL_IO_WRITE | UT_CMD_SEL_IO_COMPARE |
			UT_CMD_SEL_IO_COPY | UT_CMD_SEL_IO_FUSED_CW);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ftl_interface_selectable_by_multi_mode_subcase3(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int nr_cmd;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 3;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u",
		nvme_admin_maxio_nvme_case, param.dw3);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(2), &param);
	if (ret < 0)
		return ret;

	ret = ut_submit_io_random_cmds(priv, sq, UT_CMD_SEL_IO_READ |
		UT_CMD_SEL_IO_WRITE | UT_CMD_SEL_IO_COMPARE |
		UT_CMD_SEL_IO_FUSED_CW, 11);
	if (ret < 0)
		return ret;
	nr_cmd = ret;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0)
		return ret;
	
	ret = ut_reap_cq_entry_no_check_by_id(priv, sq->cqid, nr_cmd);
	if (ret < 0)
		return ret;
	
	return 0;
}

static int ftl_interface_selectable_by_multi_mode_subcase4(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	struct copy_resource *copy = NULL;
	uint64_t nsze;
	uint64_t slba;
	uint32_t nlb;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 4;

	ret = nvme_id_ns_nsze(ns_grp, effect.nsid, &nsze);
	if (ret < 0)
		return ret;

	slba = rand() % min_t(uint64_t, nsze / 2, 51);
	nlb = rand() % min_t(uint64_t, nsze - slba, 256) + 1;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u",
		nvme_admin_maxio_nvme_case, param.dw3);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(2), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	switch (rand() % 5) {
	case 0:
		ret = ut_send_io_read_cmd(priv, sq, slba, nlb);
		break;
	case 1:
		ret = ut_send_io_write_cmd(priv, sq, slba, nlb);
		break;
	case 2:
		ret = ut_send_io_compare_cmd(priv, sq, slba, nlb);
		break;
	case 3:
		copy = ut_alloc_copy_resource(1, NVME_COPY_DESC_FMT_32B);
		if (!copy)
			return -ENOMEM;
		
		copy->slba = 0;
		copy->entry[0].slba = slba;
		copy->entry[0].nlb = nlb;
		ret = ut_send_io_copy_cmd(priv, sq, copy);
		ut_release_copy_resource(copy);
		break;
	case 4:
		ret = ut_send_io_fused_cw_cmd(priv, sq, slba, nlb);
		break;
	}

	return ret;
}

static int ftl_interface_selectable_by_multi_mode_subcase5(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 5;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u",
		nvme_admin_maxio_nvme_case, param.dw3);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(2), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	switch (rand() % 2) {
	case 0:
		effect.cmd.write.dtype = NVME_DIR_STREAMS;
		effect.cmd.write.dspec = 100;
		ret = ut_send_io_write_cmd_random_d_r(priv, sq);
		break;
	case 1:
		effect.cmd.copy.dtype = NVME_DIR_STREAMS;
		effect.cmd.copy.dspec = 100;
		ret = ut_send_io_copy_cmd_random_region(priv, sq);
		break;
	}

	return ret;
}

static int ftl_interface_selectable_by_multi_mode_subcase6(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	int nr_cmd;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 6;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u",
		nvme_admin_maxio_nvme_case, param.dw3);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(2), &param);
	if (ret < 0)
		return ret;

	ret = ut_submit_io_random_cmds(priv, sq, UT_CMD_SEL_IO_READ |
		UT_CMD_SEL_IO_WRITE | UT_CMD_SEL_IO_COMPARE |
		UT_CMD_SEL_IO_FUSED_CW, 101);
	if (ret < 0)
		return ret;
	nr_cmd = ret;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0)
		return ret;
	
	ret = ut_reap_cq_entry_no_check_by_id(priv, sq->cqid, nr_cmd);
	if (ret < 0)
		return ret;

	return 0;
}

static int ftl_interface_selectable_by_multi_mode_subcase7(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	struct copy_resource *copy = NULL;
	uint64_t nsze;
	uint64_t slba;
	uint32_t nlb;
	int nr_cmd = 11;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 7;

	ret = nvme_id_ns_nsze(ns_grp, effect.nsid, &nsze);
	if (ret < 0)
		return ret;

	slba = rand() % (nsze / 2);
	nlb = rand() % min_t(uint64_t, nsze - slba, 256) + 1;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u",
		nvme_admin_maxio_nvme_case, param.dw3);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(2), &param);
	if (ret < 0)
		return ret;

	switch (rand() % 5) {
	case 0:
		ret = ut_submit_io_read_cmds(priv, sq, slba, nlb, nr_cmd);
		break;
	case 1:
		ret = ut_submit_io_write_cmds(priv, sq, slba, nlb, nr_cmd);
		break;
	case 2:
		ret = ut_submit_io_compare_cmds(priv, sq, slba, nlb, nr_cmd);
		break;
	case 3:
		copy = ut_alloc_copy_resource(1, NVME_COPY_DESC_FMT_32B);
		if (!copy)
			return -ENOMEM;
		copy->slba = 0;
		copy->entry[0].slba = slba;
		copy->entry[0].nlb = nlb;

		ret = ut_submit_io_copy_cmds(priv, sq, copy, nr_cmd);
		break;
	case 4:
		ret = ut_submit_io_fused_cw_cmds(priv, sq, slba, nlb, nr_cmd);
		nr_cmd *= 2;
		break;
	}
	if (ret < 0)
		goto out;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0)
		goto out;
	
	ret = ut_reap_cq_entry_no_check_by_id(priv, sq->cqid, nr_cmd);
	if (ret < 0)
		goto out;

	ret = 0;
out:
	if (copy)
		ut_release_copy_resource(copy);
	return ret;
}

static int ftl_interface_selectable_by_multi_mode_subcase8(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	struct copy_resource *copy = NULL;
	uint64_t nsze;
	uint64_t slba;
	uint32_t nlb;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	param.dw3 = 8;

	ret = nvme_id_ns_nsze(ns_grp, effect.nsid, &nsze);
	if (ret < 0)
		return ret;

	slba = rand() % min_t(uint64_t, nsze / 2, 51);
	nlb = rand() % min_t(uint64_t, nsze - slba, 50) + 1;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: subcase %u",
		nvme_admin_maxio_nvme_case, param.dw3);
	ret = nvme_maxio_nvme_case_set_param(ndev, SUBCMD(2), &param);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	switch (rand() % 5) {
	case 0:
		ret = ut_send_io_read_cmd(priv, sq, slba, nlb);
		break;
	case 1:
		ret = ut_send_io_write_cmd(priv, sq, slba, nlb);
		break;
	case 2:
		ret = ut_send_io_compare_cmd(priv, sq, slba, nlb);
		break;
	case 3:
		copy = ut_alloc_copy_resource(1, NVME_COPY_DESC_FMT_32B);
		if (!copy)
			return -ENOMEM;
		copy->slba = 0;
		copy->entry[0].slba = slba;
		copy->entry[0].nlb = nlb;

		ret = ut_send_io_copy_cmd(priv, sq, copy);
		ut_release_copy_resource(copy);
		break;
	case 4:
		ret = ut_send_io_fused_cw_cmd(priv, sq, slba, nlb);
		break;
	}

	return ret;
}

static int __case_ftl_interface_selectable_by_multi_mode(
	struct nvme_tool *tool, struct case_data *priv, int time)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	void *buf;
	int ret;

	ret = ut_create_pair_io_queue(priv, sq, NULL);
	if (ret < 0)
		return ret;

	ret |= ftl_interface_selectable_by_multi_mode_subcase1(priv, sq);
	ret |= ftl_interface_selectable_by_multi_mode_subcase2(priv, sq);
	ret |= ftl_interface_selectable_by_multi_mode_subcase3(priv, sq);
	ret |= ftl_interface_selectable_by_multi_mode_subcase4(priv, sq);
	ret |= ftl_interface_selectable_by_multi_mode_subcase5(priv, sq);
	ret |= ftl_interface_selectable_by_multi_mode_subcase6(priv, sq);
	ret |= ftl_interface_selectable_by_multi_mode_subcase7(priv, sq);
	ret |= ftl_interface_selectable_by_multi_mode_subcase8(priv, sq);
	if (ret < 0)
		goto del_ioq;
	
	ret = posix_memalign(&buf, NVME_TOOL_RW_BUF_ALIGN, SZ_16K);
	if (ret) {
		pr_err("failed to alloc buf!\n");
		ret = -ENOMEM;
		goto del_ioq;
	}

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_nvme_case);
	ret = nvme_maxio_nvme_case_check_result(ndev, SUBCMD(2), buf, SZ_16K);
	if (ret < 0)
		goto rel_mem;

	dump_data_to_fmt_file(buf, SZ_16K, "%s/%s-%d.bin",
		NVME_TOOL_LOG_FILE_PATH, __func__, time);
rel_mem:
	free(buf);
del_ioq:
	ret |= ut_delete_pair_io_queue(priv, sq, NULL);
	return ret;
}

static int case_ftl_interface_selectable_by_multi_mode(
	struct nvme_tool *tool, struct case_data *priv)
{
	struct case_report *rpt = &priv->rpt;
	int loop = 1;
	int ret;

	for (; loop <= 50; loop++) {
		ut_rpt_record_case_step(rpt, "loop %d...", loop);

		ret = __case_ftl_interface_selectable_by_multi_mode(
			tool, priv, loop);
		if (ret < 0)
			return ret;
	}
	return 0;
}
NVME_CASE_SYMBOL(case_ftl_interface_selectable_by_multi_mode, "?");

static int __case_fwdma_buf2buf_test(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param param = {0};
	uint64_t nsze;
	uint64_t slba;
	uint32_t blk_size;
	uint32_t len;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	ret = nvme_id_ns_nsze(ns_grp, effect.nsid, &nsze);
	if (ret < 0)
		return ret;
	nvme_id_ns_lbads(ns_grp, effect.nsid, &blk_size);

	/* ensure "SLBA + len < NSZE */
	slba = rand() % (nsze / 2);
	len = rand() % min_t(uint64_t, (nsze - slba) * blk_size, SZ_128K);
	len = (len == 0) ? 4 : ALIGN(len, 4);

	param.dw3 = rand() % 2; /* bit0 = 0 or 1 */
	param.dw13 = lower_32_bits(slba);
	param.dw14 = upper_32_bits(slba);
	param.dw15 = len;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw3 0x%x, dw13 0x%x"
		" dw14 0x%x, dw15 0x%x",
		nvme_admin_maxio_fwdma_fwdma, param.dw3, param.dw13, 
		param.dw14, param.dw15);
	ret = nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(0), &param);
	if (ret < 0)
		return ret;

	return 0;
}

static int case_fwdma_buf2buf_test(struct nvme_tool *tool, 
	struct case_data *priv)
{
	int loop = 1;
	int ret;

	for (; loop <= 1000; loop++) {
		ret = __case_fwdma_buf2buf_test(tool, priv);
		if (ret < 0)
			return ret;
	}
	return 0;
}
NVME_CASE_SYMBOL(case_fwdma_buf2buf_test, "?");

static int __case_fwdma_buf2buf_bufpoint(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_get_param gparam = {0};
	struct nvme_maxio_set_param sparam = {0};
	uint64_t nsze;
	uint64_t slba;
	uint32_t blk_size;
	uint32_t buf_size;
	uint32_t buf_oft;
	uint32_t data_len;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	ret = nvme_id_ns_nsze(ns_grp, effect.nsid, &nsze);
	if (ret < 0)
		return ret;
	nvme_id_ns_lbads(ns_grp, effect.nsid, &blk_size);

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Get Param",
		nvme_admin_maxio_fwdma_fwdma);
	ret = nvme_maxio_fwdma_fwdma_get_param(ndev, SUBCMD(1), &gparam);
	if (ret < 0)
		return ret;
	
	/* ensure "SLBA + buf_size < NSZE" */
	buf_size = (gparam.dw0 & 0x1) ? SZ_4K : SZ_8K;
	slba = rand() % min_t(uint64_t, nsze / 2, nsze - (buf_size / blk_size));
	/* ensure "buf_oft + data_len < buf_size" */
	data_len = ALIGN(rand() % buf_size + 1, 4);
	buf_oft = (buf_size == data_len) ? 0 : (rand() % (buf_size - data_len));

	sparam.dw3 = rand() % 8; /* random bit[0, 2] */
	if (!(gparam.dw0 & 0x1))
		sparam.dw3 |= (1 << 3);
	sparam.dw3 |= (buf_oft << 16);
	sparam.dw13 = lower_32_bits(slba);
	sparam.dw14 = upper_32_bits(slba);
	sparam.dw15 = data_len;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw3 0x%x, dw13 0x%x"
		" dw14 0x%x, dw15 0x%x",
		nvme_admin_maxio_fwdma_fwdma, sparam.dw3, sparam.dw13, 
		sparam.dw14, sparam.dw15);
	ret = nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(1), &sparam);
	if (ret < 0)
		return ret;

	return 0;
}

static int case_fwdma_buf2buf_bufpoint(struct nvme_tool *tool, 
	struct case_data *priv)
{
	int loop = 1;
	int ret;

	for (; loop <= 1000; loop++) {
		ret = __case_fwdma_buf2buf_bufpoint(tool, priv);
		if (ret < 0)
			return ret;
	}
	return 0;
}
NVME_CASE_SYMBOL(case_fwdma_buf2buf_bufpoint, "?");

static int case_fwdma_buf2buf_mpi(struct nvme_tool *tool, 
	struct case_data *priv)
{
	return -EOPNOTSUPP;
}
NVME_CASE_SYMBOL(case_fwdma_buf2buf_mpi, "?");

static int case_fwdma_buf2buf_sram_tcm_addr_bypass(struct nvme_tool *tool, 
	struct case_data *priv)
{
	return -EOPNOTSUPP;
}
NVME_CASE_SYMBOL(case_fwdma_buf2buf_sram_tcm_addr_bypass, "?");

static int case_fwdma_ut_max_sq_size_bypass(struct nvme_tool *tool, 
	struct case_data *priv)
{
	return -EOPNOTSUPP;
}
NVME_CASE_SYMBOL(case_fwdma_ut_max_sq_size_bypass, "?");

static int case_fwdma_ut_buf2host_host2buf(struct nvme_tool *tool, 
	struct case_data *priv)
{
	return -EOPNOTSUPP;
}
NVME_CASE_SYMBOL(case_fwdma_ut_buf2host_host2buf, "?");

static int case_fwdma_ut_reg2host_host2reg(struct nvme_tool *tool, 
	struct case_data *priv)
{
	return -EOPNOTSUPP;
}
NVME_CASE_SYMBOL(case_fwdma_ut_reg2host_host2reg, "?");

static int fwdma_ut_hmb_engine_test_1st(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_hmb_alloc *hmb_alloc;
	struct nvme_hmb_wrapper wrap = {0};
	uint32_t cc, hmminds, hmmaxd;
	uint32_t page_size;
	uint32_t min_buf_size;
	uint32_t max_desc_entry;
	uint32_t entry;
	uint32_t total;
	int i, ret;

	CHK_EXPR_NUM_LT0_RTN(
		nvme_read_ctrl_cc(ndev->fd, &cc), -EIO);
	page_size = 1 << (12 + NVME_CC_TO_MPS(cc));

	CHK_EXPR_NUM_LT0_RTN(
		nvme_id_ctrl_hmminds(ndev->ctrl, &hmminds), -ENODEV);
	min_buf_size = hmminds ? (hmminds * SZ_4K) : page_size;

	hmmaxd = CHK_EXPR_NUM_LT0_RTN(
		nvme_id_ctrl_hmmaxd(ndev->ctrl), -ENODEV);
	max_desc_entry = hmmaxd ? hmmaxd : 8;
	entry = rand() % max_desc_entry + 1;

	hmb_alloc = CHK_EXPR_PTR_EQ0_RTN(
		zalloc(sizeof(struct nvme_hmb_alloc) + 
			sizeof(uint32_t) * entry), -ENOMEM);
	hmb_alloc->nr_desc = entry;
	hmb_alloc->page_size = page_size;

	for (i = 0; i < entry; i++)
		hmb_alloc->bsize[i] = DIV_ROUND_UP(min_buf_size, page_size);
	
	total = entry * ALIGN(min_buf_size, page_size);
	if (total < SZ_2M) {
		hmb_alloc->bsize[entry - 1] += 
			DIV_ROUND_UP(SZ_2M - total, page_size);
		total += ALIGN(SZ_2M - total, page_size);
	}

	CHK_EXPR_NUM_LT0_GOTO(
		nvme_alloc_host_mem_buffer(ndev->fd, hmb_alloc),
		ret, -ENOMEM, free_hmb_alloc);

	/* enable host memory buffer */
	wrap.sel = NVME_FEAT_SEL_CUR;
	wrap.dw11 = NVME_HOST_MEM_ENABLE;
	wrap.hsize = total / page_size;
	wrap.hmdla = hmb_alloc->desc_list;
	wrap.hmdlec = entry;

	CHK_EXPR_NUM_LT0_GOTO(
		nvme_set_feat_hmb(ndev, &wrap), ret, -EPERM, free_hmb_buf);

	free(hmb_alloc);
	return 0;

free_hmb_buf:
	ret |= nvme_release_host_mem_buffer(ndev->fd);
free_hmb_alloc:
	free(hmb_alloc);
	return ret;
}

static int fwdma_ut_hmb_engine_test_2nd(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};

	sparam.dw11 = 1;
	sparam.dw3 = rand() % 4;
	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw3 0x%x dw11 0x%x",
		nvme_admin_maxio_fwdma_fwdma, sparam.dw3, sparam.dw11);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(7), &sparam),
		-EPERM);

	return 0;
}

static int fwdma_ut_hmb_engine_test_3rd(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};

	sparam.dw11 = 2;
	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw3 0x%x dw11 0x%x",
		nvme_admin_maxio_fwdma_fwdma, sparam.dw3, sparam.dw11);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(7), &sparam),
		-EPERM);

	sparam.dw11 = 3;
	sparam.dw3 = rand() % 64 + 1;
	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw3 0x%x dw11 0x%x",
		nvme_admin_maxio_fwdma_fwdma, sparam.dw3, sparam.dw11);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(7), &sparam),
		-EPERM);
	
	memset(&sparam, 0, sizeof(sparam));
	sparam.dw11 = 4;
	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw3 0x%x dw11 0x%x",
		nvme_admin_maxio_fwdma_fwdma, sparam.dw3, sparam.dw11);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(7), &sparam),
		-EPERM);

	sparam.dw11 = 5;
	sparam.dw3 = rand() % 64 + 1;
	sparam.dw12 = rand();
	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw3 0x%x dw11 0x%x"
		" dw12 0x%x", nvme_admin_maxio_fwdma_fwdma, sparam.dw3, 
		sparam.dw11, sparam.dw12);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(7), &sparam),
		-EPERM);

	memset(&sparam, 0, sizeof(sparam));
	sparam.dw11 = 6;
	sparam.dw3 = rand() % 1025;
	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw3 0x%x dw11 0x%x",
		nvme_admin_maxio_fwdma_fwdma, sparam.dw3, sparam.dw11);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(7), &sparam),
		-EPERM);
	
	memset(&sparam, 0, sizeof(sparam));
	sparam.dw11 = 7;
	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw3 0x%x dw11 0x%x",
		nvme_admin_maxio_fwdma_fwdma, sparam.dw3, sparam.dw11);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(7), &sparam),
		-EPERM);

	memset(&sparam, 0, sizeof(sparam));
	sparam.dw11 = 8;
	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw3 0x%x dw11 0x%x",
		nvme_admin_maxio_fwdma_fwdma, sparam.dw3, sparam.dw11);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(7), &sparam),
		-EPERM);

	memset(&sparam, 0, sizeof(sparam));
	sparam.dw11 = 9;
	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw3 0x%x dw11 0x%x",
		nvme_admin_maxio_fwdma_fwdma, sparam.dw3, sparam.dw11);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(7), &sparam),
		-EPERM);
	return 0;
}

static int fwdma_ut_hmb_engine_test_last(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_hmb_wrapper wrap = {0};

	wrap.sel = NVME_FEAT_SEL_CUR;

	/* disable host memory buffer and release resource */
	CHK_EXPR_NUM_LT0_RTN(
		nvme_set_feat_hmb(ndev, &wrap), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_release_host_mem_buffer(ndev->fd), -EPERM);
	return 0;
}

static int case_fwdma_ut_hmb_engine_test(struct nvme_tool *tool, 
	struct case_data *priv)
{
	int ret;

	ret = fwdma_ut_hmb_engine_test_1st(tool, priv);
	if (ret < 0)
		return ret;
	ret = fwdma_ut_hmb_engine_test_2nd(tool, priv);
	if (ret < 0)
		goto out;
	ret = fwdma_ut_hmb_engine_test_3rd(tool, priv);
	if (ret < 0)
		goto out;
out:
	ret |= fwdma_ut_hmb_engine_test_last(tool, priv);
	return ret;
}
NVME_CASE_SYMBOL(case_fwdma_ut_hmb_engine_test, "?");

static int fwdma_ut_fwdma_mix_case_check_1st(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint32_t method; ///< 0: AES, 1: SM4
	uint32_t key_len; ///< 0: 128bits, 1: 256bits
	uint32_t mode; ///< 1: XTS
	uint32_t verify;
	uint32_t key_type;
	uint32_t data_len;
	int ret;

	method = rand() % 2;
	key_len = (method == 1) ? 0 : (rand() % 2);
	mode = rand() % 4;
	verify = rand() % 2;
	key_type = rand() % 2;

	sparam.dw3 = (method & 0x1) | (key_len & 0x2) | (mode & 0xc) |
		(verify & 0x10) | (key_type & 0x20);
	sparam.dw11 = 1;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw3 0x%x",
		nvme_admin_maxio_fwdma_fwdma, sparam.dw3);
	ret = nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(8), &sparam);
	if (ret < 0)
		return ret;

	if (mode == 1)
		data_len = ALIGN(rand() % (SZ_32K - SZ_512) + 1, 16) + (SZ_512 - 16);
	else
		data_len = ALIGN(rand() % SZ_32K + 1, 16);

	return data_len;
}

static int fwdma_ut_fwdma_mix_case_check_remain(struct nvme_tool *tool, 
	struct case_data *priv, uint32_t data_len)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint64_t slba = 0;
	int ret;

	BUG_ON(tool->wbuf_size < SZ_32K);

	sparam.dw11 = 2;
	sparam.dw13 = lower_32_bits(slba);
	sparam.dw14 = upper_32_bits(slba);
	sparam.dw15 = data_len;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw11 0x%x, dw13 0x%x"
		" dw14 0x%x, dw15 0x%x",
		nvme_admin_maxio_fwdma_fwdma, sparam.dw11, sparam.dw13, 
		sparam.dw14, sparam.dw15);
	ret = nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(8), &sparam);
	if (ret < 0)
		return ret;

	sparam.dw11 = 3;
	sparam.buf = tool->wbuf;
	sparam.buf_size = SZ_32K;
	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw11 0x%x, dw13 0x%x"
		" dw14 0x%x, dw15 0x%x",
		nvme_admin_maxio_fwdma_fwdma, sparam.dw11, sparam.dw13, 
		sparam.dw14, sparam.dw15);
	ret = nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(8), &sparam);
	if (ret < 0)
		return ret;

	sparam.dw11 = 4;
	sparam.dw3 = rand() % 2;
	if (sparam.dw3)
		memset(sparam.buf, UINT_MAX, sparam.buf_size);
	else
		memset(sparam.buf, 0, sparam.buf_size);

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw3 0x%x dw11 0x%x "
		"dw13 0x%x dw14 0x%x, dw15 0x%x",
		nvme_admin_maxio_fwdma_fwdma, sparam.dw3, sparam.dw11, 
		sparam.dw13, sparam.dw14, sparam.dw15);
	ret = nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(8), &sparam);
	if (ret < 0)
		return ret;

	sparam.dw11 = 5;
	sparam.dw3 = 0;
	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw11 0x%x, dw13 0x%x"
		" dw14 0x%x, dw15 0x%x",
		nvme_admin_maxio_fwdma_fwdma, sparam.dw11, sparam.dw13, 
		sparam.dw14, sparam.dw15);
	ret = nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(8), &sparam);
	if (ret < 0)
		return ret;

	sparam.dw11 = 6;
	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw11 0x%x, dw13 0x%x"
		" dw14 0x%x, dw15 0x%x",
		nvme_admin_maxio_fwdma_fwdma, sparam.dw11, sparam.dw13, 
		sparam.dw14, sparam.dw15);
	ret = nvme_maxio_fwdma_fwdma_set_param(ndev, SUBCMD(8), &sparam);
	if (ret < 0)
		return ret;

	return 0;
}

static int case_fwdma_ut_fwdma_mix_case_check(struct nvme_tool *tool, 
	struct case_data *priv)
{
	int ret;

	ret = fwdma_ut_fwdma_mix_case_check_1st(tool, priv);
	if (ret < 0)
		return ret;
	ret = fwdma_ut_fwdma_mix_case_check_remain(tool, priv, ret);
	if (ret < 0)
		return ret;

	return 0;
}
NVME_CASE_SYMBOL(case_fwdma_ut_fwdma_mix_case_check, "?");

static int case_fwdma_ut_buf2buf_vendor_e2e(struct nvme_tool *tool, 
	struct case_data *priv)
{
	return -EOPNOTSUPP;
}
NVME_CASE_SYMBOL(case_fwdma_ut_buf2buf_vendor_e2e, "?");

static int case_fwdma_ut_buf2buf_pad_data(struct nvme_tool *tool, 
	struct case_data *priv)
{
	return -EOPNOTSUPP;
}
NVME_CASE_SYMBOL(case_fwdma_ut_buf2buf_pad_data, "?");

static int case_host_enable_ltr_message(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint8_t cap_exp = ndev->pdev->express.offset;
	uint16_t cap_ltr = ndev->pdev->ltr.offset;
	uint16_t lat_set = 0x100f; ///< 15ms
	uint16_t lat_snoop;
	uint16_t lat_no_snoop;
	uint16_t link_ctrl;
	uint16_t dev_ctrl2;
	int fd = ndev->fd;

	if (!cap_ltr) {
		pr_warn("device not support LTR capacity!\n");
		return -EOPNOTSUPP;
	}

	/* backup register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_snoop_latency(fd, cap_ltr, &lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_no_snoop_latency(fd, cap_ltr, 
			&lat_no_snoop), -EIO);
	/* set device max snoop & no-snoop latency */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	/* close device L1 */
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_link_control(fd, cap_exp, &link_ctrl), -EIO);
	link_ctrl &= ~PCI_EXP_LNKCTL_ASPM_L1;
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_write_link_control(fd, cap_exp, link_ctrl), -EIO);
	/* close device LTR */
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_device_control2(fd, cap_exp, &dev_ctrl2), -EIO);
	dev_ctrl2 &= ~PCI_EXP_DEVCTL2_LTR_EN;
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_write_device_control2(fd, cap_exp, dev_ctrl2), -EIO);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_set_param(ndev, SUBCMD(0), &sparam),
		-EPERM);

	msleep(100);

	/* open device LTR */
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_device_control2(fd, cap_exp, &dev_ctrl2), -EIO);
	dev_ctrl2 |= PCI_EXP_DEVCTL2_LTR_EN;
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_write_device_control2(fd, cap_exp, dev_ctrl2), -EIO);

	msleep(100);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_check_result(ndev, SUBCMD(0)), 
		-EPERM);
	/* restore register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, 
			lat_no_snoop), -EIO);
	return 0;
}
NVME_CASE_SYMBOL(case_host_enable_ltr_message, "?");

static int case_l1tol0_ltr_message(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint8_t cap_exp = ndev->pdev->express.offset;
	uint16_t cap_ltr = ndev->pdev->ltr.offset;
	uint16_t lat_set = 0x100f; ///< 15ms
	uint16_t lat_snoop;
	uint16_t lat_no_snoop;
	uint16_t link_ctrl;
	char cmd[128];
	int fd = ndev->fd;

	if (!cap_ltr) {
		pr_warn("device not support LTR capacity!\n");
		return -EOPNOTSUPP;
	}

	/* backup register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_snoop_latency(fd, cap_ltr, &lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_no_snoop_latency(fd, cap_ltr, 
			&lat_no_snoop), -EIO);
	/* set device max snoop & no-snoop latency */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	/* open device L1 */
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_link_control(fd, cap_exp, &link_ctrl), -EIO);
	link_ctrl |= PCI_EXP_LNKCTL_ASPM_L1;
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_write_link_control(fd, cap_exp, link_ctrl), -EIO);
	/* open device link partner L1 */
	snprintf(cmd, sizeof(cmd), "setpci -s %s.w=02:02",
		RC_PCI_EXP_REG_LINK_CONTROL);
	CHK_EXPR_NUM_LT0_RTN(call_system(cmd), -EPERM);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_set_param(ndev, SUBCMD(1), &sparam),
		-EPERM);

	msleep(100);

	/* close device L1 */
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_link_control(fd, cap_exp, &link_ctrl), -EIO);
	link_ctrl &= ~PCI_EXP_LNKCTL_ASPM_L1;
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_write_link_control(fd, cap_exp, link_ctrl), -EIO);	

	msleep(100);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_check_result(ndev, SUBCMD(1)), 
		-EPERM);
	/* restore register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, 
			lat_no_snoop), -EIO);
	return 0;
}
NVME_CASE_SYMBOL(case_l1tol0_ltr_message, "?");

static int case_l0tol1_ltr_message(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint8_t cap_exp = ndev->pdev->express.offset;
	uint16_t cap_ltr = ndev->pdev->ltr.offset;
	uint16_t cap_l1ss = ndev->pdev->l1ss.offset;
	uint16_t lat_set = 0x100f; ///< 15ms
	uint16_t lat_snoop;
	uint16_t lat_no_snoop;
	uint16_t link_ctrl;
	uint16_t dev_sts;
	uint32_t l1ss_ctrl;
	char cmd[128];
	int fd = ndev->fd;

	if (!cap_ltr || !cap_l1ss) {
		pr_warn("device not support LTR/L1SS capacity!\n");
		return -EOPNOTSUPP;
	}

	if (RC_PCIE_L1SS_CAP_OFFSET) {
		pr_warn("host not support L1SS capacity!\n");
		return -EOPNOTSUPP;
	}

	/* backup register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_snoop_latency(fd, cap_ltr, &lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_no_snoop_latency(fd, cap_ltr, 
			&lat_no_snoop), -EIO);
	/* set device max snoop & no-snoop latency */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	/* close device L1CPM & L1.1 & L1.2 */
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_link_control(fd, cap_exp, &link_ctrl), -EIO);
	link_ctrl &= ~PCI_EXP_LNKCTL_CLKREQ_EN;
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_write_link_control(fd, cap_exp, link_ctrl), -EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_l1ss_read_control(fd, cap_l1ss, &l1ss_ctrl), -EIO);
	l1ss_ctrl &= ~(PCI_L1SS_CTL1_ASPM_L1_2 | PCI_L1SS_CTL1_ASPM_L1_1);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_l1ss_write_control(fd, cap_l1ss, l1ss_ctrl), -EIO);
	/* close host L1CPM & L1.1 & L1.2 */
	snprintf(cmd, sizeof(cmd), "setpci -s %s.w=0:100",
		RC_PCI_EXP_REG_LINK_CONTROL);
	CHK_EXPR_NUM_LT0_RTN(call_system(cmd), -EPERM);
	snprintf(cmd, sizeof(cmd), "setpci -s %s.l=0:c",
		RC_PCIE_L1SS_REG_CTRL1);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_set_param(ndev, SUBCMD(2), &sparam),
		-EPERM);

	msleep(100);
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_device_state(fd, cap_exp, &dev_sts), -EIO);
	msleep(100);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_check_result(ndev, SUBCMD(2)), 
		-EPERM);

	/* restore register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, 
			lat_no_snoop), -EIO);
	return 0;
}
NVME_CASE_SYMBOL(case_l0tol1_ltr_message, "?");

static int case_l0tol1cpm_ltr_message(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint8_t cap_exp = ndev->pdev->express.offset;
	uint16_t cap_ltr = ndev->pdev->ltr.offset;
	uint16_t cap_l1ss = ndev->pdev->l1ss.offset;
	uint16_t lat_set = 0x100f; ///< 15ms
	uint16_t lat_snoop;
	uint16_t lat_no_snoop;
	uint16_t link_ctrl;
	uint16_t dev_sts;
	uint32_t l1ss_ctrl;
	int fd = ndev->fd;

	if (!cap_ltr || !cap_l1ss) {
		pr_warn("device not support LTR/L1SS capacity!\n");
		return -EOPNOTSUPP;
	}

	/* backup register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_snoop_latency(fd, cap_ltr, &lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_no_snoop_latency(fd, cap_ltr, 
			&lat_no_snoop), -EIO);
	/* set device max snoop & no-snoop latency */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	/* close device L1.1 & L1.2 */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_l1ss_read_control(fd, cap_l1ss, &l1ss_ctrl), -EIO);
	l1ss_ctrl &= ~(PCI_L1SS_CTL1_ASPM_L1_2 | PCI_L1SS_CTL1_ASPM_L1_1);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_l1ss_write_control(fd, cap_l1ss, l1ss_ctrl), -EIO);
	/* open device L1 & L1CPM */
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_link_control(fd, cap_exp, &link_ctrl), -EIO);
	link_ctrl |= PCI_EXP_LNKCTL_CLKREQ_EN | PCI_EXP_LNKCTL_ASPM_L1;
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_write_link_control(fd, cap_exp, link_ctrl), -EIO);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_set_param(ndev, SUBCMD(3), &sparam),
		-EPERM);

	msleep(100);
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_device_state(fd, cap_exp, &dev_sts), -EIO);
	msleep(100);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_check_result(ndev, SUBCMD(3)), 
		-EPERM);

	/* restore register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, 
			lat_no_snoop), -EIO);
	return 0;
}
NVME_CASE_SYMBOL(case_l0tol1cpm_ltr_message, "?");

static int case_l0tol11_ltr_message(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint8_t cap_exp = ndev->pdev->express.offset;
	uint16_t cap_ltr = ndev->pdev->ltr.offset;
	uint16_t cap_l1ss = ndev->pdev->l1ss.offset;
	uint16_t lat_set = 0x100f; ///< 15ms
	uint16_t lat_snoop;
	uint16_t lat_no_snoop;
	uint16_t link_ctrl;
	uint16_t dev_sts;
	uint32_t l1ss_ctrl;
	int fd = ndev->fd;

	if (!cap_ltr || !cap_l1ss) {
		pr_warn("device not support LTR/L1SS capacity!\n");
		return -EOPNOTSUPP;
	}

	/* backup register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_snoop_latency(fd, cap_ltr, &lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_no_snoop_latency(fd, cap_ltr, 
			&lat_no_snoop), -EIO);
	/* set device max snoop & no-snoop latency */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	/* close device L1.2, open device L1 & L1CPM & L1.1 */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_l1ss_read_control(fd, cap_l1ss, &l1ss_ctrl), -EIO);
	l1ss_ctrl &= ~PCI_L1SS_CTL1_ASPM_L1_2;
	l1ss_ctrl |= PCI_L1SS_CTL1_ASPM_L1_1;
	CHK_EXPR_NUM_LT0_RTN(
		pcie_l1ss_write_control(fd, cap_l1ss, l1ss_ctrl), -EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_link_control(fd, cap_exp, &link_ctrl), -EIO);
	link_ctrl |= PCI_EXP_LNKCTL_CLKREQ_EN | PCI_EXP_LNKCTL_ASPM_L1;
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_write_link_control(fd, cap_exp, link_ctrl), -EIO);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_set_param(ndev, SUBCMD(4), &sparam),
		-EPERM);

	msleep(100);
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_device_state(fd, cap_exp, &dev_sts), -EIO);
	msleep(100);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_check_result(ndev, SUBCMD(4)), 
		-EPERM);

	/* restore register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, 
			lat_no_snoop), -EIO);
	return 0;
}
NVME_CASE_SYMBOL(case_l0tol11_ltr_message, "?");

static int case_l0tol12_ltr_message(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint8_t cap_exp = ndev->pdev->express.offset;
	uint16_t cap_ltr = ndev->pdev->ltr.offset;
	uint16_t cap_l1ss = ndev->pdev->l1ss.offset;
	uint16_t lat_set = 0x100f; ///< 15ms
	uint16_t lat_snoop;
	uint16_t lat_no_snoop;
	uint16_t link_ctrl;
	uint16_t dev_sts;
	uint32_t l1ss_ctrl;
	int fd = ndev->fd;

	if (!cap_ltr || !cap_l1ss) {
		pr_warn("device not support LTR/L1SS capacity!\n");
		return -EOPNOTSUPP;
	}

	/* backup register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_snoop_latency(fd, cap_ltr, &lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_no_snoop_latency(fd, cap_ltr, 
			&lat_no_snoop), -EIO);
	/* set device max snoop & no-snoop latency */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	/* open device L1 & L1CPM & L1.1 & L1.2 */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_l1ss_read_control(fd, cap_l1ss, &l1ss_ctrl), -EIO);
	l1ss_ctrl |= PCI_L1SS_CTL1_ASPM_L1_2 | PCI_L1SS_CTL1_ASPM_L1_1;
	CHK_EXPR_NUM_LT0_RTN(
		pcie_l1ss_write_control(fd, cap_l1ss, l1ss_ctrl), -EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_link_control(fd, cap_exp, &link_ctrl), -EIO);
	link_ctrl |= PCI_EXP_LNKCTL_CLKREQ_EN | PCI_EXP_LNKCTL_ASPM_L1;
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_write_link_control(fd, cap_exp, link_ctrl), -EIO);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_set_param(ndev, SUBCMD(5), &sparam),
		-EPERM);

	msleep(100);
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_device_state(fd, cap_exp, &dev_sts), -EIO);
	msleep(100);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_check_result(ndev, SUBCMD(5)), 
		-EPERM);

	/* restore register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, 
			lat_no_snoop), -EIO);
	return 0;
}
NVME_CASE_SYMBOL(case_l0tol12_ltr_message, "?");

static int case_host_enable_ltr_over_max_mode(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint8_t cap_exp = ndev->pdev->express.offset;
	uint16_t cap_ltr = ndev->pdev->ltr.offset;
	uint16_t lat_set = 0x1001; ///< 1ms
	uint16_t lat_snoop;
	uint16_t lat_no_snoop;
	uint16_t link_ctrl;
	uint16_t dev_ctrl2;
	int fd = ndev->fd;

	if (!cap_ltr) {
		pr_warn("device not support LTR capacity!\n");
		return -EOPNOTSUPP;
	}

	/* backup register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_snoop_latency(fd, cap_ltr, &lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_no_snoop_latency(fd, cap_ltr, 
			&lat_no_snoop), -EIO);
	/* set device max snoop & no-snoop latency */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	/* close device L1 */
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_link_control(fd, cap_exp, &link_ctrl), -EIO);
	link_ctrl &= ~PCI_EXP_LNKCTL_ASPM_L1;
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_write_link_control(fd, cap_exp, link_ctrl), -EIO);
	/* close device LTR */
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_device_control2(fd, cap_exp, &dev_ctrl2), -EIO);
	dev_ctrl2 &= ~PCI_EXP_DEVCTL2_LTR_EN;
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_write_device_control2(fd, cap_exp, dev_ctrl2), -EIO);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_set_param(ndev, SUBCMD(6), &sparam),
		-EPERM);

	msleep(100);
	/* open device LTR */
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_device_control2(fd, cap_exp, &dev_ctrl2), -EIO);
	dev_ctrl2 |= PCI_EXP_DEVCTL2_LTR_EN;
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_write_device_control2(fd, cap_exp, dev_ctrl2), -EIO);
	
	msleep(100);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_check_result(ndev, SUBCMD(6)), 
		-EPERM);

	/* restore register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, 
			lat_no_snoop), -EIO);
	return 0;
}
NVME_CASE_SYMBOL(case_host_enable_ltr_over_max_mode, "?");

static int case_l1_to_l0_ltr_over_max_mode(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint8_t cap_exp = ndev->pdev->express.offset;
	uint16_t cap_ltr = ndev->pdev->ltr.offset;
	uint16_t lat_set = 0x1001; ///< 1ms
	uint16_t lat_snoop;
	uint16_t lat_no_snoop;
	uint16_t dev_sts;
	int fd = ndev->fd;

	if (!cap_ltr) {
		pr_warn("device not support LTR capacity!\n");
		return -EOPNOTSUPP;
	}

	/* backup register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_snoop_latency(fd, cap_ltr, &lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_no_snoop_latency(fd, cap_ltr, 
			&lat_no_snoop), -EIO);
	/* set device max snoop & no-snoop latency */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, lat_set), -EIO);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_set_param(ndev, SUBCMD(7), &sparam),
		-EPERM);

	msleep(100);
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_device_state(fd, cap_exp, &dev_sts), -EIO);
	msleep(100);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_check_result(ndev, SUBCMD(7)), 
		-EPERM);

	/* restore register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, 
			lat_no_snoop), -EIO);
	return 0;
}
NVME_CASE_SYMBOL(case_l1_to_l0_ltr_over_max_mode, "?");

static int case_l0_to_l1_ltr_over_max_mode(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint8_t cap_exp = ndev->pdev->express.offset;
	uint16_t cap_ltr = ndev->pdev->ltr.offset;
	uint16_t cap_l1ss = ndev->pdev->l1ss.offset;
	uint16_t lat_set = 0x1001; ///< 1ms
	uint16_t lat_snoop;
	uint16_t lat_no_snoop;
	uint16_t link_ctrl;
	uint16_t dev_sts;
	uint32_t l1ss_ctrl;
	int fd = ndev->fd;

	if (!cap_ltr || !cap_l1ss) {
		pr_warn("device not support LTR/L1SS capacity!\n");
		return -EOPNOTSUPP;
	}

	/* backup register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_snoop_latency(fd, cap_ltr, &lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_no_snoop_latency(fd, cap_ltr, 
			&lat_no_snoop), -EIO);
	/* set device max snoop & no-snoop latency */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	/* open device L1 & L1CPM & L1.1 & L1.2 */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_l1ss_read_control(fd, cap_l1ss, &l1ss_ctrl), -EIO);
	l1ss_ctrl |= PCI_L1SS_CTL1_ASPM_L1_2 | PCI_L1SS_CTL1_ASPM_L1_1;
	CHK_EXPR_NUM_LT0_RTN(
		pcie_l1ss_write_control(fd, cap_l1ss, l1ss_ctrl), -EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_link_control(fd, cap_exp, &link_ctrl), -EIO);
	link_ctrl |= PCI_EXP_LNKCTL_CLKREQ_EN | PCI_EXP_LNKCTL_ASPM_L1;
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_write_link_control(fd, cap_exp, link_ctrl), -EIO);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_set_param(ndev, SUBCMD(8), &sparam),
		-EPERM);

	msleep(100);
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_device_state(fd, cap_exp, &dev_sts), -EIO);
	msleep(100);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_check_result(ndev, SUBCMD(8)), 
		-EPERM);

	/* restore register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, 
			lat_no_snoop), -EIO);
	return 0;
}
NVME_CASE_SYMBOL(case_l0_to_l1_ltr_over_max_mode, "?");

static int case_less_ltr_threshold_mode(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint8_t cap_exp = ndev->pdev->express.offset;
	uint16_t cap_ltr = ndev->pdev->ltr.offset;
	uint16_t cap_l1ss = ndev->pdev->l1ss.offset;
	uint16_t lat_set = 0x100f; ///< 15ms
	uint16_t lat_snoop;
	uint16_t lat_no_snoop;
	uint16_t dev_sts;
	uint32_t l1ss_ctrl;
	uint32_t l12_th_value;
	uint32_t l12_th_scale;
	int fd = ndev->fd;

	if (!cap_ltr || !cap_l1ss) {
		pr_warn("device not support LTR/L1SS capacity!\n");
		return -EOPNOTSUPP;
	}

	/* backup register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_snoop_latency(fd, cap_ltr, &lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_read_max_no_snoop_latency(fd, cap_ltr, 
			&lat_no_snoop), -EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_l1ss_read_control(fd, cap_l1ss, &l1ss_ctrl), -EIO);
	l12_th_value = l1ss_ctrl & PCI_L1SS_CTL1_LTR_L12_TH_VALUE;
	l12_th_scale = l1ss_ctrl & PCI_L1SS_CTL1_LTR_L12_TH_SCALE;
	/* set device max snoop & no-snoop latency */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, lat_set), -EIO);
	/* set device LTR_L1.2_THRESHOLD = 3ms */
	l1ss_ctrl &= ~PCI_L1SS_CTL1_LTR_L12_TH_VALUE;
	l1ss_ctrl |= (0x3 << 16);
	l1ss_ctrl &= ~PCI_L1SS_CTL1_LTR_L12_TH_SCALE;
	l1ss_ctrl |= (0x4 << 29);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_l1ss_write_control(fd, cap_l1ss, l1ss_ctrl), -EIO);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_set_param(ndev, SUBCMD(9), &sparam),
		-EPERM);

	msleep(100);
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_device_state(fd, cap_exp, &dev_sts), -EIO);
	msleep(100);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_check_result(ndev, SUBCMD(9)), 
		-EPERM);

	/* restore register */
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_snoop_latency(fd, cap_ltr, lat_snoop),
		-EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_ltr_write_max_no_snoop_latency(fd, cap_ltr, 
			lat_no_snoop), -EIO);
	CHK_EXPR_NUM_LT0_RTN(
		pcie_l1ss_read_control(fd, cap_l1ss, &l1ss_ctrl), -EIO);
	l1ss_ctrl &= ~PCI_L1SS_CTL1_LTR_L12_TH_VALUE;
	l1ss_ctrl |= l12_th_value;
	l1ss_ctrl &= ~PCI_L1SS_CTL1_LTR_L12_TH_SCALE;
	l1ss_ctrl |= l12_th_scale;
	CHK_EXPR_NUM_LT0_RTN(
		pcie_l1ss_write_control(fd, cap_l1ss, l1ss_ctrl), -EIO);
	return 0;
}
NVME_CASE_SYMBOL(case_less_ltr_threshold_mode, "?");

static int case_drs_message(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	char cmd[128];

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_set_param(ndev, SUBCMD(10), &sparam),
		-EPERM);

	msleep(100);

	/* hot reset*/
	snprintf(cmd, sizeof(cmd), "setpci -s %s.b=40:40", 
		RC_PCI_HDR_REG_BRIDGE_CONTROL);
	CHK_EXPR_NUM_LT0_RTN(call_system(cmd), -EPERM);
	msleep(10);
	snprintf(cmd, sizeof(cmd), "setpci -s %s.b=0:40", 
		RC_PCI_HDR_REG_BRIDGE_CONTROL);
	CHK_EXPR_NUM_LT0_RTN(call_system(cmd), -EPERM);

	msleep(100);

	CHK_EXPR_NUM_LT0_RTN(
		nvme_reinit(ndev, ndev->asq.nr_entry, ndev->acq.nr_entry,
			ndev->irq_type), -ENODEV);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_check_result(ndev, SUBCMD(10)), 
		-EPERM);

	return 0;
}
NVME_CASE_SYMBOL(case_drs_message, "?");

static int case_frs_message(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint8_t cap_exp = ndev->pdev->express.offset;
	uint16_t dev_ctrl;
	int fd = ndev->fd;

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_set_param(ndev, SUBCMD(11), &sparam),
		-EPERM);

	msleep(100);

	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_device_control(fd, cap_exp, &dev_ctrl), -EIO);
	dev_ctrl |= PCI_EXP_DEVCTL_BCR_FLR;
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_write_device_control(fd, cap_exp, dev_ctrl), -EIO);

	msleep(100);

	CHK_EXPR_NUM_LT0_RTN(
		nvme_reinit(ndev, ndev->asq.nr_entry, ndev->acq.nr_entry,
			ndev->irq_type), -ENODEV);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_msg);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_message_check_result(ndev, SUBCMD(11)), 
		-EPERM);

	return 0;
}
NVME_CASE_SYMBOL(case_frs_message, "?");

static int cfgwr_interrupt_single_dword(struct nvme_tool *tool, 
	struct case_data *priv, uint32_t oft)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint32_t val;
	int ret;

	sparam.dw3 = oft;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: 0x%x",
		nvme_admin_maxio_pcie_interrupt, sparam.dw3);
	ret = nvme_maxio_pcie_interrupt_set_param(ndev, SUBCMD(0), &sparam);
	if (ret < 0)
		return ret;

	msleep(100);

	ret = pci_read_config_dword(ndev->fd, oft, &val);
	if (ret < 0)
		return ret;

	ret = pci_write_config_dword(ndev->fd, oft, val);
	if (ret < 0)
		return ret;

	msleep(100);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_interrupt);
	ret = nvme_maxio_pcie_interrupt_check_result(ndev, SUBCMD(0));
	if (ret < 0)
		return ret;

	return 0;
}

static int cfgwr_interrupt_travel_config_header(struct nvme_tool *tool, 
	struct case_data *priv)
{
	uint32_t i;
	int ret;

	for (i = 0; i < PCI_STD_HEADER_SIZEOF; i += 4) {
		ret = cfgwr_interrupt_single_dword(tool, priv, i);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int cfgwr_interrupt_travel_pci_capability(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	uint8_t pos = PCI_CAPABILITY_LIST;
	uint8_t id;
	uint16_t entry;
	uint32_t len;
	uint32_t i;
	int ttl = PCI_CAP_ID_MAX + 1;
	int ret;

	ret = pci_read_config_byte(ndev->fd, pos, &pos);
	if (ret < 0)
		return ret;

	while (ttl--) {
		if (pos < 0x40)
			break;
		pos &= ~3;
		ret = pci_read_config_word(ndev->fd, pos, &entry);
		if (ret < 0)
			return ret;

		id = entry & 0xff;
		if (!id)
			break;

		switch (id) {
		case PCI_CAP_ID_PM:
			len = PCI_PM_SIZEOF;
			break;
		case PCI_CAP_ID_AGP:
			len = PCI_AGP_SIZEOF;
			break;
		case PCI_CAP_ID_VPD:
			len = PCI_CAP_VPD_SIZEOF;
			break;
		case PCI_CAP_ID_SLOTID:
			len = 0;
			break;
		case PCI_CAP_ID_MSI:
		{
			uint16_t mc;

			ret = pci_read_config_word(ndev->fd, pos + 2, &mc);
			if (ret < 0)
				return ret;
			
			if (mc & PCI_MSI_FLAGS_64BIT) {
				if (mc & PCI_MSI_FLAGS_MASKBIT)
					len = 0x18;
				else
					len = 0x10;
			} else {
				if (mc & PCI_MSI_FLAGS_MASKBIT)
					len = 0x14;
				else
					len = 0xc;
			}
			break;
		}
		case PCI_CAP_ID_CHSWP:
		case PCI_CAP_ID_PCIX:
		case PCI_CAP_ID_HT:
		case PCI_CAP_ID_VNDR:
		case PCI_CAP_ID_DBG:
		case PCI_CAP_ID_CCRC:
		case PCI_CAP_ID_SHPC:
		case PCI_CAP_ID_SSVID:
		case PCI_CAP_ID_AGP3:
		case PCI_CAP_ID_SECDEV:
			len = 0;
			break;
		case PCI_CAP_ID_EXP:
			len = 0x3c;
			break;
		case PCI_CAP_ID_MSIX:
			len = PCI_CAP_MSIX_SIZEOF;
			break;
		case PCI_CAP_ID_SATA:
			len = 0;
			break;
		case PCI_CAP_ID_AF:
			len = PCI_CAP_AF_SIZEOF;
			break;
		case PCI_CAP_ID_EA:
			len = 0;
			break;
		default:
			pr_err("ID:0x%x is unknown!\n", id);
			return -EINVAL;
		}

		for (i = 0; i < len; i += 4) {
			ret = cfgwr_interrupt_single_dword(tool, priv, 
				(uint32_t)pos + i);
			if (ret < 0)
				return ret;
		}

		pos = entry >> 8; /* next capability offset */
	}
	return 0;
}

static int cfgwr_interrupt_travel_pcie_capability(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	uint16_t pos = 0x100;
	uint16_t id;
	uint32_t entry;
	uint32_t len;
	uint32_t i;
	int ttl = PCI_EXT_CAP_ID_MAX + 1;
	int ret;

	while (ttl--) {
		ret = pci_read_config_dword(ndev->fd, pos, &entry);
		if (ret < 0)
			return ret;

		id = entry & 0xffff;
		if (!id)
			break;
		
		switch (id) {
		case PCI_EXT_CAP_ID_ERR:
		case PCI_EXT_CAP_ID_VC:
		case PCI_EXT_CAP_ID_DSN:
		case PCI_EXT_CAP_ID_PWR:
		case PCI_EXT_CAP_ID_RCLD:
		case PCI_EXT_CAP_ID_RCILC:
		case PCI_EXT_CAP_ID_RCEC:
		case PCI_EXT_CAP_ID_MFVC:
		case PCI_EXT_CAP_ID_VC9:
		case PCI_EXT_CAP_ID_RCRB:
		case PCI_EXT_CAP_ID_VNDR:
		case PCI_EXT_CAP_ID_CAC:
		case PCI_EXT_CAP_ID_ACS:
		case PCI_EXT_CAP_ID_ARI:
		case PCI_EXT_CAP_ID_ATS:
		case PCI_EXT_CAP_ID_SRIOV:
		case PCI_EXT_CAP_ID_MRIOV:
		case PCI_EXT_CAP_ID_MCAST:
		case PCI_EXT_CAP_ID_PRI:
		case PCI_EXT_CAP_ID_AMD_XXX:
		case PCI_EXT_CAP_ID_REBAR:
		case PCI_EXT_CAP_ID_DPA:
		case PCI_EXT_CAP_ID_TPH:
		case PCI_EXT_CAP_ID_LTR:
		case PCI_EXT_CAP_ID_SECPCI:
		case PCI_EXT_CAP_ID_PMUX:
		case PCI_EXT_CAP_ID_PASID:
		case PCI_EXT_CAP_ID_DPC:
		case PCI_EXT_CAP_ID_L1SS:
		case PCI_EXT_CAP_ID_PTM:
		case PCI_EXT_CAP_ID_DLF:
		case PCI_EXT_CAP_ID_PL_16GT:
			len = 0;
			break;
		default:
			pr_err("ID:0x%x is unknown!\n", id);
			return -EINVAL;
		}

		for (i = 0; i < len; i += 4) {
			ret = cfgwr_interrupt_single_dword(tool, priv, 
				(uint32_t)pos + i);
			if (ret < 0)
				return ret;
		}

		pos = entry >> 20; /* next capability offset */
		if (pos < 0x100 || pos > 0xfff)
			break;
	}
	return 0;
}

static int case_cfgwr_interrupt(struct nvme_tool *tool, 
	struct case_data *priv)
{
	int ret;

	/*
	 * !NOTE: Get register region by vendor cmd is more convenient than
	 * parsing capability.
	 */
	ret = cfgwr_interrupt_travel_config_header(tool, priv);
	ret |= cfgwr_interrupt_travel_pci_capability(tool, priv);
	ret |= cfgwr_interrupt_travel_pcie_capability(tool, priv);

	return ret;
}
NVME_CASE_SYMBOL(case_cfgwr_interrupt, "?");

static int case_ltssm_state_change_interrupt(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	char cmd[128];
	uint32_t arr[20] = {
		0x1, 0x102, 0x204, 0x407, 0x708,
		0x809, 0x90a, 0xa0b, 0xb0c, 0xc11,
		0x110d, 0xd0e, 0xe0d, 0xd0f, 0xf10,
		0xf07, 0x1011, 0x1113, 0x1314, 0x140d,
	};
	uint8_t exp_oft = ndev->pdev->express.offset;
	uint16_t link_ctrl2;
	int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(arr); i++) {
		sparam.dw3 = arr[i];

		ut_rpt_record_case_step(rpt, 
			"Send vendor command:0x%x - Set Param: 0x%x",
			nvme_admin_maxio_pcie_interrupt, sparam.dw3);
		ret = nvme_maxio_pcie_interrupt_set_param(ndev, SUBCMD(1), &sparam);
		if (ret < 0)
			return ret;

		msleep(100);

		/* down link speed */
		ret = pci_exp_read_link_control2(ndev->fd, exp_oft, &link_ctrl2);
		if (ret < 0)
			return ret;

		link_ctrl2 &= ~PCI_EXP_LNKCTL2_TLS;
		link_ctrl2 |= PCI_EXP_LNKCTL2_TLS_2_5GT;
		ret = pci_exp_write_link_control2(ndev->fd, exp_oft, link_ctrl2);
		if (ret < 0)
			return ret;

		/* retrain link */
		snprintf(cmd, sizeof(cmd), "setpci -s %s.w=20:20", 
			RC_PCI_EXP_REG_LINK_CONTROL);
		ret = call_system(cmd);
		if (ret < 0)
			return ret;
		msleep(10);

		/* down link width */
		ret = pci_write_config_dword(ndev->fd, 0x8c0, 1);
		if (ret < 0)
			return ret;

		/* retrain link */
		snprintf(cmd, sizeof(cmd), "setpci -s %s.w=20:20", 
			RC_PCI_EXP_REG_LINK_CONTROL);
		ret = call_system(cmd);
		if (ret < 0)
			return ret;
		msleep(10);

		/* hot reset */
		snprintf(cmd, sizeof(cmd), "setpci -s %s.b=40:40",
			RC_PCI_HDR_REG_BRIDGE_CONTROL);
		ret = call_system(cmd);
		if (ret < 0)
			return ret;
		msleep(100);
		snprintf(cmd, sizeof(cmd), "setpci -s %s.b=0:40",
			RC_PCI_HDR_REG_BRIDGE_CONTROL);
		ret = call_system(cmd);
		if (ret < 0)
			return ret;
		msleep(100);

		ut_rpt_record_case_step(rpt, 
			"Send vendor command:0x%x - Check Result",
			nvme_admin_maxio_pcie_interrupt);
		ret = nvme_maxio_pcie_interrupt_check_result(ndev, SUBCMD(1));
		if (ret < 0)
			return ret;
	}

	return 0;
}
NVME_CASE_SYMBOL(case_ltssm_state_change_interrupt, "?");

static int case_pcie_rdlh_interrupt(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint32_t link_cap;
	char buf[16] = { "0x" };
	char cmd[128];
	FILE *fp;
	int ret;

	snprintf(cmd, sizeof(cmd), "setpci -s %s.l", 
		RC_PCI_EXP_REG_LINK_CAPABILITY);
	fp = popen(cmd, "r");
	if (!fp) {
		pr_err("failed to read link capability register!\n");
		return -EPERM;
	}
	fread(buf + strlen(buf), sizeof(char), sizeof(buf) - strlen(buf), fp);
	pclose(fp);

	link_cap = strtoul(buf, NULL, 0);
	sparam.dw3 = link_cap & PCI_EXP_LNKCAP_SLS;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: 0x%x",
		nvme_admin_maxio_pcie_interrupt, sparam.dw3);
	ret = nvme_maxio_pcie_interrupt_set_param(ndev, SUBCMD(2), &sparam);
	if (ret < 0)
		return ret;

	msleep(100);

	/* hot reset */
	snprintf(cmd, sizeof(cmd), "setpci -s %s.b=40:40",
		RC_PCI_HDR_REG_BRIDGE_CONTROL);
	ret = call_system(cmd);
	if (ret < 0)
		return ret;
	msleep(10);
	snprintf(cmd, sizeof(cmd), "setpci -s %s.b=0:40",
		RC_PCI_HDR_REG_BRIDGE_CONTROL);
	ret = call_system(cmd);
	if (ret < 0)
		return ret;
	msleep(100);

	ret = nvme_reinit(ndev, ndev->asq.nr_entry, ndev->acq.nr_entry,
		ndev->irq_type);
	if (ret < 0)
		return ret;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_interrupt);
	ret = nvme_maxio_pcie_interrupt_check_result(ndev, SUBCMD(2));
	if (ret < 0)
		return ret;

	return 0;
}
NVME_CASE_SYMBOL(case_pcie_rdlh_interrupt, "?");

static int case_pcie_speed_down_interrupt(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint8_t exp_oft = ndev->pdev->express.offset;
	uint16_t link_ctrl2;
	char cmd[128];
	int ret;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_interrupt);
	ret = nvme_maxio_pcie_interrupt_set_param(ndev, SUBCMD(3), &sparam);
	if (ret < 0)
		return ret;

	msleep(100);

	/* down link speed */
	ret = pci_exp_read_link_control2(ndev->fd, exp_oft, &link_ctrl2);
	if (ret < 0)
		return ret;

	link_ctrl2 &= ~PCI_EXP_LNKCTL2_TLS;
	link_ctrl2 |= PCI_EXP_LNKCTL2_TLS_2_5GT;
	ret = pci_exp_write_link_control2(ndev->fd, exp_oft, link_ctrl2);
	if (ret < 0)
		return ret;

	/* retrain link */
	snprintf(cmd, sizeof(cmd), "setpci -s %s.w=20:20", 
		RC_PCI_EXP_REG_LINK_CONTROL);
	ret = call_system(cmd);
	if (ret < 0)
		return ret;
	msleep(100);

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_interrupt);
	ret = nvme_maxio_pcie_interrupt_check_result(ndev, SUBCMD(3));
	if (ret < 0)
		return ret;

	/* set link speed to Gen5 */
	ret = pci_exp_read_link_control2(ndev->fd, exp_oft, &link_ctrl2);
	if (ret < 0)
		return ret;

	link_ctrl2 &= ~PCI_EXP_LNKCTL2_TLS;
	link_ctrl2 |= PCI_EXP_LNKCTL2_TLS_32_0GT;
	ret = pci_exp_write_link_control2(ndev->fd, exp_oft, link_ctrl2);
	if (ret < 0)
		return ret;

	/* retrain link */
	snprintf(cmd, sizeof(cmd), "setpci -s %s.w=20:20", 
		RC_PCI_EXP_REG_LINK_CONTROL);
	ret = call_system(cmd);
	if (ret < 0)
		return ret;

	return 0;
}
NVME_CASE_SYMBOL(case_pcie_speed_down_interrupt, "?");

static int case_aspm_l1sub_disable_by_fw(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint8_t exp_oft = ndev->pdev->express.offset;
	uint16_t l1ss_oft = ndev->pdev->l1ss.offset;
	uint16_t link_ctrl;
	uint32_t l1ss_ctrl;
	int ret;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_special);
	ret = nvme_maxio_pcie_special_set_param(ndev, SUBCMD(0), &sparam);
	if (ret < 0)
		return ret;

	msleep(100);

	ret = pci_exp_read_link_control(ndev->fd, exp_oft, &link_ctrl);
	if (ret < 0)
		return ret;
	
	link_ctrl |= PCI_EXP_LNKCTL_CLKREQ_EN;
	ret = pci_exp_write_link_control(ndev->fd, exp_oft, link_ctrl);
	if (ret < 0)
		return ret;
	
	ret = pcie_l1ss_read_control(ndev->fd, l1ss_oft, &l1ss_ctrl);
	if (ret < 0)
		return ret;
	
	l1ss_ctrl |= PCI_L1SS_CTL1_ASPM_L1_2;
	l1ss_ctrl |= PCI_L1SS_CTL1_ASPM_L1_1;
	ret = pcie_l1ss_write_control(ndev->fd, l1ss_oft, l1ss_ctrl);
	if (ret < 0)
		return ret;

	ret = pci_exp_read_link_control(ndev->fd, exp_oft, &link_ctrl);
	if (ret < 0)
		return ret;
	
	link_ctrl &= ~PCI_EXP_LNKCTL_ASPMC;
	link_ctrl |= PCI_EXP_LNKCTL_ASPM_L1;
	ret = pci_exp_write_link_control(ndev->fd, exp_oft, link_ctrl);
	if (ret < 0)
		return ret;
	
	msleep(100);

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_special);
	ret = nvme_maxio_pcie_special_check_result(ndev, SUBCMD(0));
	if (ret < 0)
		return ret;

	return 0;
}
NVME_CASE_SYMBOL(case_aspm_l1sub_disable_by_fw, "?");

static int case_data_rate_register_in_l12(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint8_t exp_oft = ndev->pdev->express.offset;
	uint16_t l1ss_oft = ndev->pdev->l1ss.offset;
	uint16_t link_ctrl;
	uint32_t l1ss_ctrl;
	int ret;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_special);
	ret = nvme_maxio_pcie_special_set_param(ndev, SUBCMD(1), &sparam);
	if (ret < 0)
		return ret;
	
	msleep(100);

	ret = pcie_l1ss_read_control(ndev->fd, l1ss_oft, &l1ss_ctrl);
	if (ret < 0)
		return ret;
	
	l1ss_ctrl |= PCI_L1SS_CTL1_ASPM_L1_2;
	ret = pcie_l1ss_write_control(ndev->fd, l1ss_oft, l1ss_ctrl);
	if (ret < 0)
		return ret;

	ret = pci_exp_read_link_control(ndev->fd, exp_oft, &link_ctrl);
	if (ret < 0)
		return ret;
	
	link_ctrl &= ~PCI_EXP_LNKCTL_ASPMC;
	link_ctrl |= PCI_EXP_LNKCTL_ASPM_L1;
	ret = pci_exp_write_link_control(ndev->fd, exp_oft, link_ctrl);
	if (ret < 0)
		return ret;
	
	msleep(100);

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_special);
	ret = nvme_maxio_pcie_special_check_result(ndev, SUBCMD(1));
	if (ret < 0)
		return ret;

	return 0;
}
NVME_CASE_SYMBOL(case_data_rate_register_in_l12, "?");

static int case_hw_control_request_retry(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint8_t cap_exp = ndev->pdev->express.offset;
	uint16_t dev_ctrl;
	int fd = ndev->fd;

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_special);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_special_set_param(ndev, SUBCMD(2), &sparam),
		-EPERM);

	msleep(100);

	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_device_control(fd, cap_exp, &dev_ctrl), -EIO);
	dev_ctrl |= PCI_EXP_DEVCTL_BCR_FLR;
	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_write_device_control(fd, cap_exp, dev_ctrl), -EIO);
	
	msleep(100);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_special);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_special_check_result(ndev, SUBCMD(2)), -EPERM);

	return 0;
}
NVME_CASE_SYMBOL(case_hw_control_request_retry, "?");

static int case_d3_not_block_message(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	uint8_t cap_pm = ndev->pdev->pm.offset;
	uint16_t pm_ctrl;
	int fd = ndev->fd;

	CHK_EXPR_NUM_LT0_RTN(
		pci_pm_read_control_status(fd, cap_pm, &pm_ctrl), -EIO);
	pm_ctrl &= ~PCI_PM_CTRL_STATE_MASK;
	pm_ctrl |= PCI_PM_CTRL_STATE_D3HOT;
	CHK_EXPR_NUM_LT0_RTN(
		pci_pm_write_control_status(fd, cap_pm, pm_ctrl), -EIO);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_special);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_special_set_param(ndev, SUBCMD(4), &sparam),
		-EPERM);
	
	msleep(100);

	ut_rpt_record_case_step(rpt, "Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_special);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_pcie_special_check_result(ndev, SUBCMD(4)), -EPERM);

	return 0;
}
NVME_CASE_SYMBOL(case_d3_not_block_message, "?");

static int case_internal_cpld_mps_check(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	struct nvme_maxio_set_param sparam = {0};
	uint64_t slba;
	uint64_t nsze;
	uint32_t nlb;
	uint32_t blk_size;
	uint8_t exp_oft = ndev->pdev->express.offset;
	uint16_t ep_dev_ctrl;
	uint16_t rc_dev_ctrl;
	uint16_t dev_ctrl;
	uint16_t mps = (rand() % 2) ? PCI_EXP_DEVCTL_PAYLOAD_128B :
		PCI_EXP_DEVCTL_READRQ_256B;
	char buf[16] = { "0x" };
	char cmd[128];
	FILE *fp;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	ret = pci_exp_read_device_control(ndev->fd, exp_oft, &ep_dev_ctrl);
	if (ret < 0)
		return ret;

	snprintf(cmd, sizeof(cmd), "setpci -s %s.w", 
		RC_PCI_EXP_REG_DEVICE_CONTROL);
	fp = popen(cmd, "r");
	if (!fp) {
		pr_err("failed to read device control register!\n");
		return -EPERM;
	}
	fread(buf + strlen(buf), sizeof(char), sizeof(buf) - strlen(buf), fp);
	pclose(fp);

	rc_dev_ctrl = strtoul(buf, NULL, 0);

	/* set device MPS & MRRS */
	dev_ctrl = ep_dev_ctrl & ~(PCI_EXP_DEVCTL_PAYLOAD | PCI_EXP_DEVCTL_READRQ);
	dev_ctrl |= mps;
	dev_ctrl |= PCI_EXP_DEVCTL_READRQ_1024B;
	ret = pci_exp_write_device_control(ndev->fd, exp_oft, dev_ctrl);
	if (ret < 0)
		return ret;

	/* set partner MPS */
	mps = (mps == PCI_EXP_DEVCTL_PAYLOAD_128B) ? 
		PCI_EXP_DEVCTL_READRQ_256B : PCI_EXP_DEVCTL_PAYLOAD_512B;
	dev_ctrl = rc_dev_ctrl & ~PCI_EXP_DEVCTL_PAYLOAD;
	dev_ctrl |= mps;
	
	snprintf(cmd, sizeof(cmd), "setpci -s %s.w=%x:ffff",
		RC_PCI_EXP_REG_DEVICE_CONTROL, dev_ctrl);
	ret = call_system(cmd);
	if (ret < 0)
		return ret;

	/* send vendor cmd */
	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param",
		nvme_admin_maxio_pcie_special);
	ret = nvme_maxio_pcie_special_set_param(ndev, SUBCMD(5), &sparam);
	if (ret < 0)
		return ret;

	ret = nvme_id_ns_nsze(ns_grp, effect.nsid, &nsze);
	if (ret < 0)
		return ret;
	nvme_id_ns_lbads(ns_grp, effect.nsid, &blk_size);

	slba = rand() % (nsze / 2);
	nlb = ALIGN(SZ_4K / blk_size, blk_size);

	ret = ut_create_pair_io_queue(priv, sq, NULL);
	if (ret < 0)
		return ret;

	effect.check_none = 1;
	ret = ut_send_io_write_cmd(priv, sq, slba, nlb);
	if (ret < 0)
		goto del_ioq;

	msleep(100);

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Check Result",
		nvme_admin_maxio_pcie_special);
	ret = nvme_maxio_pcie_special_check_result(ndev, SUBCMD(5));
	if (ret < 0)
		goto del_ioq;

	/* recovery device MPS & MRRS */
	ret = pci_exp_read_device_control(ndev->fd, exp_oft, &dev_ctrl);
	if (ret < 0)
		goto del_ioq;
	
	dev_ctrl &= ~(PCI_EXP_DEVCTL_PAYLOAD | PCI_EXP_DEVCTL_READRQ);
	dev_ctrl |= (ep_dev_ctrl & PCI_EXP_DEVCTL_PAYLOAD);
	dev_ctrl |= (ep_dev_ctrl & PCI_EXP_DEVCTL_READRQ);
	ret = pci_exp_write_device_control(ndev->fd, exp_oft, dev_ctrl);
	if (ret < 0)
		goto del_ioq;

	/* recovery partner MPS */
	snprintf(cmd, sizeof(cmd), "setpci -s %s.w=%x:ffff",
		RC_PCI_EXP_REG_DEVICE_CONTROL, rc_dev_ctrl);
	ret = call_system(cmd);
	if (ret < 0)
		goto del_ioq;

del_ioq:
	ret |= ut_delete_pair_io_queue(priv, sq, NULL);
	return ret;
}
NVME_CASE_SYMBOL(case_internal_cpld_mps_check, "?");

static int case_bdf_check(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};
	int ret;

	sparam.dw3 = pdev->bdf;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: 0x%x",
		nvme_admin_maxio_pcie_special, sparam.dw3);
	ret = nvme_maxio_pcie_special_set_param(ndev, SUBCMD(6), &sparam);
	if (ret < 0)
		return ret;

	return 0;
}
NVME_CASE_SYMBOL(case_bdf_check, "?");
