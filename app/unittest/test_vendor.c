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

	ret = nvme_maxio_nvme_top_set_param(ndev, BIT(0), 0);
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
	ret = nvme_maxio_nvme_top_check_result(ndev, BIT(0));
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
	ret = nvme_maxio_nvme_cqm_set_param(ndev, BIT(0), nr_entry);
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
	ret = nvme_maxio_nvme_cqm_check_result(ndev, BIT(0));
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
	
	ret = nvme_maxio_nvme_hwrdma_set_param(ndev, BIT(0), pending);
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
	ret = nvme_maxio_nvme_hwrdma_check_result(ndev, BIT(0));
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
		ret = nvme_maxio_nvme_hwrdma_set_param(ndev, BIT(1), interface[i]);
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
		ret = nvme_maxio_nvme_hwrdma_check_result(ndev, BIT(1));
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
	ret = nvme_maxio_nvme_hwrdma_set_param(ndev, BIT(2), 
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
	ret = nvme_maxio_nvme_hwrdma_check_result(ndev, BIT(2));
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
	ret = nvme_maxio_nvme_hwwdma_set_param(ndev, BIT(0), pending);
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
	ret = nvme_maxio_nvme_hwwdma_check_result(ndev, BIT(0));
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
		ret = nvme_maxio_nvme_hwwdma_set_param(ndev, BIT(1), interface[i]);
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
		ret = nvme_maxio_nvme_hwwdma_check_result(ndev, BIT(1));
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(0), &param);
	if (ret < 0) 
		return ret;

	arb.hpw = rand() % 101; /* 0 ~ 100 */
	arb.mpw = rand() % 101;
	arb.lpw = rand() % 101;
	arb.burst = rand() % 7; /* 0 ~ 6 => 1 ~ 64 */

	ret = nvme_set_feat_arbitration(ndev, &arb);
	if (ret < 0)
		return ret;

	ret = ut_create_pair_io_queues(priv, &ndev->iosqs, NULL, nr_pair);
	if (ret < 0)
		return ret;

	ret = ut_submit_io_random_cmds_to_sqs(priv, &ndev->iosqs, 
		UT_CMD_SEL_ALL, nr_cmd, nr_pair);
	if (ret < 0)
		goto del_ioq;

	ret = ut_ring_sqs_doorbell(priv, &ndev->iosqs, nr_pair);
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
	ret = nvme_maxio_nvme_case_check_result(ndev, BIT(0), buf, SZ_16K);
	if (ret < 0)
		goto rel_mem;

	dump_data_to_fmt_file(buf, SZ_16K, "%s/%s-%d.bin", 
		NVME_TOOL_LOG_FILE_PATH, __func__, time);

rel_mem:
	free(buf);
del_ioq:
	ret |= ut_delete_pair_io_queues(priv, &ndev->iosqs, NULL, param.dw3);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(1), &param);
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
	ret = nvme_maxio_nvme_case_check_result(ndev, BIT(1), buf, SZ_16K);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(2), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(2), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(2), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(2), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(2), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(2), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(2), &param);
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
	ret = nvme_maxio_nvme_case_set_param(ndev, BIT(2), &param);
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
	ret = nvme_maxio_nvme_case_check_result(ndev, BIT(2), buf, SZ_16K);
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
	ret = nvme_maxio_fwdma_fwdma_set_param(ndev, BIT(0), &param);
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
	ret = nvme_maxio_fwdma_fwdma_get_param(ndev, BIT(1), &gparam);
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
	ret = nvme_maxio_fwdma_fwdma_set_param(ndev, BIT(1), &sparam);
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

static int case_fwdma_ut_hmb_engine_test(struct nvme_tool *tool, 
	struct case_data *priv)
{
	return -EOPNOTSUPP;
}
NVME_CASE_SYMBOL(case_fwdma_ut_hmb_engine_test, "?");

static int case_fwdma_ut_fwdma_mix_case_check(struct nvme_tool *tool, 
	struct case_data *priv)
{
	return -EOPNOTSUPP;
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

