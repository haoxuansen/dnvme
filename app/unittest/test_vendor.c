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

#if 0
static int case_wrr_with_urgent_priority_class_arbitration(
	struct nvme_tool *tool, struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect effect = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);



	// !TODO: now
	return 0;
}
NVME_CASE_SYMBOL(case_wrr_with_urgent_priority_class_arbitration, "?");
#endif
