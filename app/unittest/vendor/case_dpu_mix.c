/**
 * @file case_dpu_mix.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2024-01-23
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "build_bug.h"
#include "libbase.h"
#include "libnvme.h"
#include "test.h"
#include "vendor.h"

enum {
	DPU_MIX_HWDMA_MODE_DEFAULT = 0,
	DPU_MIX_HWDMA_MODE_LAA = 1,
	DPU_MIX_HWDMA_MODE_FENCE
};

struct utc_dpu_mix_hw_wdma {
	uint32_t	mc_switch:4;
	uint32_t	en_ftl_if:2;
	uint32_t	mode:2;
	uint32_t	dw0_8_15:8;
	uint16_t	burst;

	struct {
		uint8_t		laa_mask:4;
		uint8_t		rsvd4_7:4;
		uint8_t		laa_shift;
	} ftl_if0;
	uint16_t	dw1_16_31;

	struct {
		uint8_t		laa_mask:4;
		uint8_t		rsvd4_7:4;
		uint8_t		laa_shift;
	} ftl_if1;
	uint16_t	dw2_16_31;
	uint32_t	dw3;
	uint32_t	random[16];

	uint32_t	req_max_pend:12;
	uint32_t	req_max_size:10;
	uint32_t	req_ff_depth:3;
	uint32_t	req_fuse_cmd_cut_en:1;
	uint32_t	dw20_26_31:6;

	uint32_t	dw21_0:1;
	uint32_t	req_max_cmd:10;
	uint32_t	req_cmd_limit:8;
	uint32_t	req_min_size:10;
	uint32_t	req_ff_thr:3;

	uint32_t	dw22_to_63[42];
};
#define UTC_DPU_MIX_HW_WDMA_SIZE	SZ_256

struct utc_dpu_mix_hw_rdma {
	uint32_t	dw0_0_3:4;
	uint32_t	en_ftl_if:2;
	uint32_t	mode:2;
	uint32_t	dw0_8_15:8;
	uint16_t	burst;

	struct {
		uint8_t		laa_mask:4;
		uint8_t		rsvd4_7:4;
		uint8_t		laa_shift;
	} ftl_if0;
	uint16_t	dw1_16_31;

	struct {
		uint8_t		laa_mask:4;
		uint8_t		rsvd4_7:4;
		uint8_t		laa_shift;
	} ftl_if1;
	uint16_t	dw2_16_31;
	uint32_t	dw3;
	uint32_t	random[16];

	uint32_t	req_max_pend:12;
	uint32_t	req_max_size:10;
	uint32_t	req_ff_depth:3;
	uint32_t	req_lba_bdry:4;
	uint32_t	req_lba_bdry_cut_en:1;
	uint32_t	dw20_30_31:2;

	uint32_t	dw21_0:1;
	uint32_t	req_max_cmd:10;
	uint32_t	req_cmd_limit:8;
	uint32_t	req_min_size:10;
	uint32_t	req_ff_thr:3;

	uint32_t	dw22_to_63[42];
};
#define UTC_DPU_MIX_HW_RDMA_SIZE	SZ_256

struct utc_dpu_mix_param {
	uint32_t	dw0;
	uint32_t	dw1;
	uint32_t	mc:1; ///< Multi CMD
	uint32_t	dw2:31;
	struct {
		uint16_t	nsid;
		uint16_t	k_type:3; ///< Key Type
		uint16_t	k_fmt:3; ///< Key Format
		uint16_t	dpu:1;
		uint16_t	opal:1;
		uint16_t	k_comb:4;
		uint16_t	rsvd28_30:3;
		uint16_t	enable:1;
	} ns[64];
	struct {
		uint32_t	rsp_w_cmd_early:1;
		uint32_t	pcq_w_cmd:1;
		uint32_t	fused_cmd:1;
		uint32_t	atomic:1;
		uint32_t	pcq_fua:1;
		uint32_t	rsvd5_31:27;
	} sqm;
	struct utc_dpu_mix_hw_wdma	wdma;
	struct utc_dpu_mix_hw_rdma	rdma;
};
#define UTC_DPU_MIX_PARAM_SIZE		(4 * 196)

static void dpu_mix_check_size(void)
{
	BUILD_BUG_ON(sizeof(struct utc_dpu_mix_hw_wdma) != UTC_DPU_MIX_HW_WDMA_SIZE);
	BUILD_BUG_ON(sizeof(struct utc_dpu_mix_hw_rdma) != UTC_DPU_MIX_HW_RDMA_SIZE);
	BUILD_BUG_ON(sizeof(struct utc_dpu_mix_param) != UTC_DPU_MIX_PARAM_SIZE);
}

static void dpu_mix_clear_config(struct case_config_effect *cfg)
{
	memset(&cfg->cmd, 0, sizeof(cfg->cmd));
}

static void dpu_mix_init_ns_map(struct nvme_tool *tool, uint64_t *bitmap)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint64_t mask;
	uint64_t random;

	mask = (ns_grp->nr_act < 64) ? (BIT_ULL(ns_grp->nr_act) - 1) : U64_MAX;

	/* Select at lease one namespace */
	do {
		random = ((uint64_t)rand() << 32) | (uint64_t)rand();
		*bitmap = random & mask;
	} while (*bitmap == 0);
}

static void dpu_mix_init_wdma_param(struct utc_dpu_mix_param *param)
{
	struct utc_dpu_mix_hw_wdma *wdma = &param->wdma;

	if (param->mc) {
		wdma->mc_switch = rand() % 3;
		wdma->mode = rand() % DPU_MIX_HWDMA_MODE_FENCE;
	} else {
		wdma->mode = DPU_MIX_HWDMA_MODE_DEFAULT;
	}

	wdma->en_ftl_if = rand() % 2;
	wdma->burst = rand() % 11;
	
	if (wdma->mode == DPU_MIX_HWDMA_MODE_LAA) {
		if (wdma->en_ftl_if == 0 || wdma->en_ftl_if == 2) {
			wdma->ftl_if0.laa_mask = rand() % 3;
			wdma->ftl_if0.laa_shift = rand() % 4;
		}
		if (wdma->en_ftl_if == 1 || wdma->en_ftl_if == 2) {
			wdma->ftl_if1.laa_mask = rand() % 3;
			wdma->ftl_if1.laa_shift = rand() % 4;
		}
	}

	fill_data_with_random(wdma->random, sizeof(wdma->random));

	wdma->req_max_pend = rand() % 65 + 64;
	wdma->req_max_size = rand() % 64 + 1;
	wdma->req_ff_depth = rand() % 8;
	wdma->req_fuse_cmd_cut_en = param->sqm.fused_cmd ? (rand() % 2) : 0;

	if (param->mc) {
		wdma->req_max_cmd = rand() % wdma->req_max_size + 1;
		wdma->req_cmd_limit = rand() % 6;
		wdma->req_min_size = rand() % wdma->req_max_size + 1;
		wdma->req_ff_thr = rand() % 8;
	}
}

static void dpu_mix_init_rdma_param(struct utc_dpu_mix_param *param)
{
	struct utc_dpu_mix_hw_rdma *rdma = &param->rdma;

	rdma->en_ftl_if = rand() % 2;
	rdma->mode = param->mc ? (rand() % DPU_MIX_HWDMA_MODE_FENCE) :
		DPU_MIX_HWDMA_MODE_DEFAULT;
	rdma->burst = rand() % 11;

	if (rdma->mode == DPU_MIX_HWDMA_MODE_LAA) {
		if (rdma->en_ftl_if == 0 || rdma->en_ftl_if == 2) {
			rdma->ftl_if0.laa_mask = rand() % 3;
			rdma->ftl_if0.laa_shift = rand() % 4;
		}
		if (rdma->en_ftl_if == 1 || rdma->en_ftl_if == 2) {
			rdma->ftl_if1.laa_mask = rand() % 3;
			rdma->ftl_if1.laa_shift = rand() % 4;
		}
	}

	fill_data_with_random(rdma->random, sizeof(rdma->random));

	rdma->req_max_size = rand() % 256 + 1;
	rdma->req_max_pend = rand() % (257 - rdma->req_max_size) + rdma->req_max_size;
	rdma->req_ff_depth = rand() % 8;

	if (!param->mc) {
		rdma->req_lba_bdry = rand() % 16;
		rdma->req_lba_bdry_cut_en = 1;
	}

	if (param->mc) {
		rdma->req_max_cmd = rand() % rdma->req_max_size + 1;
		rdma->req_cmd_limit = rand() % 6;
		rdma->req_min_size = rand() % rdma->req_max_size + 1;
		rdma->req_ff_thr = rand() % 8;
	}
}

/**
 * @brief 
 * 
 * @return The number of namespace selected
 */
static int dpu_mix_init_param(struct nvme_tool *tool, 
	struct utc_dpu_mix_param *param, uint64_t ns_map)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint8_t key_fmt = rand() % 2 + 1;
	int bit;
	int i;

	memset(param, 0, sizeof(*param));

	param->mc = rand() % 2;
	for (i = 0; i < ARRAY_SIZE(param->ns); i++) {
		bit = ffsll(ns_map) - 1;
		if (bit < 0)
			break;
		
		ns_map ^= BIT_ULL(bit);

		param->ns[i].nsid = le32_to_cpu(ns_grp->act_list[bit]);
		param->ns[i].k_type = rand() % 2 + 1;
		param->ns[i].k_fmt = key_fmt;
		param->ns[i].dpu = rand() % 2;
		param->ns[i].opal = rand() % 2;
		param->ns[i].k_comb = rand() % 4 + 1;
		param->ns[i].enable = 1;
	}
	param->sqm.rsp_w_cmd_early = rand() % 2;
	param->sqm.pcq_w_cmd = rand() % 2;
	param->sqm.fused_cmd = rand() % 2;
	param->sqm.atomic = rand() % 2;
	param->sqm.pcq_fua = rand() % 2;

	dpu_mix_init_wdma_param(param);
	dpu_mix_init_rdma_param(param);

	return i;
}

static int dpu_mix_send_param(struct case_data *priv, 
	struct utc_dpu_mix_param *param)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_maxio_set_param sparam = {0};

	sparam.buf = param;
	sparam.buf_size = sizeof(*param);
	sparam.dw3 = sparam.buf_size;

	ut_rpt_record_case_step(rpt, 
		"Send vendor command:0x%x - Set Param: dw3 %u",
		nvme_admin_maxio_fwdma_dpu, sparam.dw3);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_maxio_fwdma_dpu_set_param(ndev, UTC_CMD_DPU_MIX, &sparam),
		-EPERM);
	
	return 0;
}

static int dpu_mix_init_queue(struct case_data *priv, uint32_t *nr_q_pair, 
	uint32_t nr_ns)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	uint32_t nr_q_max;
	uint32_t rate;

	nr_q_max = min(ctrl->nr_sq, ctrl->nr_cq);
	rate = nr_q_max / nr_ns;
	if (!rate) {
		pr_err("namespace num:%u > queue num:%u !\n", nr_ns, nr_q_max);
		return -EPERM;
	}
	rate = rand() % rate + 1;
	*nr_q_pair = rate * nr_ns;

	CHK_EXPR_NUM_LT0_RTN(
		ut_create_pair_io_queues(priv, ndev->iosqs, NULL, *nr_q_pair),
		-EPERM);
	
	return rate;
}

static int dpu_mix_deinit_queue(struct case_data *priv, uint32_t nr_q_pair)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = tool->ndev;

	return ut_delete_pair_io_queues(priv, ndev->iosqs, NULL, nr_q_pair);
}

static int dpu_mix_send_write_cmd(struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct case_config_effect *effect = priv->cfg.effect;
	uint64_t slba;
	uint32_t nlb;
	uint32_t blk_size;
	uint64_t nsze;

	dpu_mix_clear_config(effect);

	CHK_EXPR_NUM_LT0_RTN(
		nvme_id_ns_nsze(ndev->ns_grp, effect->nsid, &nsze), -EPERM);
	nvme_id_ns_lbads(ndev->ns_grp, effect->nsid, &blk_size);

	/* ensure "slba + nlb <= nsze */
	slba = rand() % (nsze / 2);
	nlb = rand() % min_t(uint64_t, nsze / 2, 256) + 1;

	BUG_ON(blk_size * nlb > tool->wbuf_size);
	fill_data_with_random(tool->wbuf, blk_size * nlb);

	CHK_EXPR_NUM_LT0_RTN(
		ut_send_io_write_cmd(priv, sq, slba, nlb), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		ut_send_io_read_cmd(priv, sq, slba, nlb), -EPERM);

	if (memcmp(tool->rbuf, tool->wbuf, nlb * blk_size)) {
		pr_err("rbuf vs wbuf is different!\n");
		dump_data_to_fmt_file(tool->rbuf, nlb * blk_size, 
			"%s/%s-rbuf.bin", NVME_TOOL_LOG_FILE_PATH, __func__);
		dump_data_to_fmt_file(tool->wbuf, nlb * blk_size, 
			"%s/%s-wbuf.bin", NVME_TOOL_LOG_FILE_PATH, __func__);
		return -EIO;
	}
	return 0;
}

static int dpu_mix_copy_cmd_write(struct case_data *priv, struct nvme_sq_info *sq,
	struct copy_resource *copy)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct case_config_effect *effect = priv->cfg.effect;
	uint32_t blk_size;
	uint32_t blk_oft = 0;
	int i;

	dpu_mix_clear_config(effect);
	nvme_id_ns_lbads(ndev->ns_grp, effect->nsid, &blk_size);

	for (i = 0; i < copy->nr_entry; i++) {
		effect->cmd.write.buf = tool->wbuf + blk_size * blk_oft;
		effect->cmd.write.size = tool->wbuf_size - blk_size * blk_oft;
		CHK_EXPR_NUM_LT0_RTN(
			ut_send_io_write_cmd(priv, sq, copy->entry[i].slba, 
				copy->entry[i].nlb), -EPERM);
		blk_oft += copy->entry[i].nlb;
	}
	return 0;
}

static int dpu_mix_send_copy_cmd(struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_config_effect *effect = priv->cfg.effect;
	struct copy_resource *copy = NULL;
	uint32_t nlb_total = 0;
	uint32_t blk_size;
	uint64_t nsze;
	uint32_t mcl;
	uint32_t msrc;
	uint32_t mssrl;
	uint32_t nr_desc;
	uint64_t blk_slice;
	uint32_t entry_nlb_limit;
	uint8_t format;
	int ret;
	int i;

	CHK_EXPR_NUM_LT0_RTN(
		nvme_id_ns_nsze(ns_grp, effect->nsid, &nsze), -EPERM);
	nvme_id_ns_lbads(ns_grp, effect->nsid, &blk_size);

	msrc = (uint16_t)nvme_id_ns_msrc(ns_grp, effect->nsid);
	mssrl = (uint16_t)nvme_id_ns_mssrl(ns_grp, effect->nsid);
	nvme_id_ns_mcl(ns_grp, effect->nsid, &mcl);

	nr_desc = rand() % min_t(uint32_t, msrc, 16) + 1;
	format = (rand() % 2) ? NVME_COPY_DESC_FMT_40B : NVME_COPY_DESC_FMT_32B;

	copy = ut_alloc_copy_resource(nr_desc, format);
	if (!copy) {
		pr_err("failed to alloc copy resource!\n");
		return -ENOMEM;
	}

	/* Each desc use a blk_slice to avoid overlap */
	blk_slice = (nsze >> 1) / nr_desc;
	entry_nlb_limit = min3(blk_slice, (uint64_t)mssrl, (uint64_t)mcl / nr_desc);
	BUG_ON(!entry_nlb_limit);
	if (entry_nlb_limit > 64)
		entry_nlb_limit = 64;

	for (i = 0; i < nr_desc; i++) {
		copy->entry[i].slba = blk_slice * i + rand() % blk_slice;
		copy->entry[i].nlb = 
			rand() % (blk_slice * (i + 1) - copy->entry[i].slba) + 1;
		nlb_total += copy->entry[i].nlb;
	}
	copy->slba = nsze >> 1;

	BUG_ON(blk_size * nlb_total > tool->wbuf_size);
	fill_data_with_random(tool->wbuf, blk_size * nlb_total);

	CHK_EXPR_NUM_LT0_GOTO(
		dpu_mix_copy_cmd_write(priv, sq, copy), ret, -EPERM, out);

	dpu_mix_clear_config(effect);
	CHK_EXPR_NUM_LT0_GOTO(
		ut_send_io_copy_cmd(priv, sq, copy), ret, -EPERM, out);
	CHK_EXPR_NUM_LT0_GOTO(
		ut_send_io_read_cmd(priv, sq, copy->slba, nlb_total),
		ret, -EPERM, out);

out:
	ut_release_copy_resource(copy);
	return ret;
}

static int dpu_mix_send_fused_cw_cmd(struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct case_config_effect *effect = priv->cfg.effect;
	uint64_t slba;
	uint32_t nlb;
	uint32_t blk_size;
	uint64_t nsze;

	CHK_EXPR_NUM_LT0_RTN(
		nvme_id_ns_nsze(ndev->ns_grp, effect->nsid, &nsze), -EPERM);
	nvme_id_ns_lbads(ndev->ns_grp, effect->nsid, &blk_size);

	/* ensure "slba + nlb <= nsze */
	slba = rand() % (nsze / 2);
	nlb = rand() % min_t(uint64_t, nsze / 2, 256) + 1;

	dpu_mix_clear_config(effect);
	CHK_EXPR_NUM_LT0_RTN(
		ut_send_io_read_cmd(priv, sq, slba, nlb), -EPERM);

	BUG_ON(blk_size * nlb > tool->wbuf_size);
	fill_data_with_random(tool->wbuf, blk_size * nlb);

	dpu_mix_clear_config(effect);
	effect->cmd.fused.buf1 = tool->rbuf;
	effect->cmd.fused.buf1_size = tool->rbuf_size;
	effect->cmd.fused.buf2 = tool->wbuf;
	effect->cmd.fused.buf2_size = tool->wbuf_size;

	CHK_EXPR_NUM_LT0_RTN(
		ut_send_io_fused_cw_cmd(priv, sq, slba, nlb), -EPERM);

	return 0;
}

static int dpu_mix_send_compare_cmd(struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_config_effect *effect = priv->cfg.effect;
	uint64_t slba;
	uint32_t nlb;
	uint64_t nsze;

	CHK_EXPR_NUM_LT0_RTN(
		nvme_id_ns_nsze(ndev->ns_grp, effect->nsid, &nsze), -EPERM);

	/* ensure "slba + nlb <= nsze */
	slba = rand() % (nsze / 2);
	nlb = rand() % min_t(uint64_t, nsze / 2, 256) + 1;

	dpu_mix_clear_config(effect);
	CHK_EXPR_NUM_LT0_RTN(
		ut_send_io_read_cmd(priv, sq, slba, nlb), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		ut_send_io_compare_cmd(priv, sq, slba, nlb), -EPERM);

	return 0;
}

/**
 * @note Each of the following commands is processed one by one
 *  1. Write Command + Read Command, verify that the data read and written
 *      are consistent.
 *  2. Copy Command: Write before copy, Read after copy, also verify data
 *  3. Fused Command which is composed of Compare command and Write command
 *  4. Compare Command
 */
static int dpu_mix_queue_cmds_send_1by1(struct case_data *priv, uint32_t q_idx)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[q_idx];
	int nr_cmd = 512;
	int select;
	int ret;

	if (sq->nr_entry / 2 < nr_cmd)
		nr_cmd = sq->nr_entry / 2;

	while (--nr_cmd >= 0) {
		select = rand() % 4;
		switch (select) {
		case 0:
			ret = dpu_mix_send_write_cmd(priv, sq);
			break;
		case 1:
			ret = dpu_mix_send_copy_cmd(priv, sq);
			break;
		case 2:
			ret = dpu_mix_send_fused_cw_cmd(priv, sq);
			break;
		case 3:
			ret = dpu_mix_send_compare_cmd(priv, sq);
			break;
		}

		if (ret < 0) {
			pr_err("failed to send %d cmd!\n", select);
			return ret;
		}
	}
	return 0;
}

/**
 * @note All of the following commands is processed together
 *  1. Write Command
 *  2. Read Command
 *  3. Copy Command: Not support yet! Need alloc/release memory for each command
 *  4. Fused Command which is composed of Compare command and Write command
 *  5. Compare Command
 */
static int dpu_mix_queue_cmds_submit_together(struct case_data *priv, 
	uint32_t q_idx)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[q_idx];
	uint32_t nr_cmd = 512;
	int ret;

	if (sq->nr_entry / 2 < nr_cmd)
		nr_cmd = sq->nr_entry / 2;

	ret = ut_submit_io_random_cmds(priv, sq, UT_CMD_SEL_IO_READ | 
		UT_CMD_SEL_IO_WRITE | UT_CMD_SEL_IO_COMPARE |
		UT_CMD_SEL_IO_FUSED_CW, nr_cmd);
	if (ret < 0) {
		pr_err("failed to submit random cmds!\n");
		return ret;
	}
	sq->cmd_cnt = ret;
	return 0;
}

static int dpu_mix_queue_cqe_reap_together(struct case_data *priv, 
	uint32_t q_idx)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[q_idx];
	int ret;

	ret = ut_reap_cq_entry_no_check_by_id(priv, sq->cqid, sq->cmd_cnt);
	if (ret < 0) {
		pr_err("failed to reap CQ entry from CQ(%u)!\n", sq->cqid);
		return ret;
	}
	return 0;
}

static int dpu_mix_travel_mode1(struct case_data *priv, uint32_t nr_ns, 
	uint32_t nr_q_pair, uint64_t ns_map)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_config_effect *effect = priv->cfg.effect;
	uint64_t bitmap;
	int bit;
	int rate;
	int i, j;

	rate = nr_q_pair / nr_ns;
	for (i = 0; i < rate; i++) {
		bitmap = ns_map;
		for (j = 0; j < nr_ns; j++) {
			bit = ffsll(bitmap) - 1;
			if (bit < 0)
				break;

			bitmap ^= BIT_ULL(bit);

			effect->nsid = le32_to_cpu(ns_grp->act_list[bit]);
			CHK_EXPR_NUM_LT0_RTN(dpu_mix_queue_cmds_send_1by1(
				priv, i * nr_ns + j), -EPERM);
		}
	}

	return 0;
}

static int dpu_mix_travel_mode2(struct case_data *priv, uint32_t nr_ns, 
	uint32_t nr_q_pair, uint64_t ns_map)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_config_effect *effect = priv->cfg.effect;
	uint64_t bitmap;
	int bit;
	int rate;
	int i, j;

	rate = nr_q_pair / nr_ns;
	for (i = 0; i < rate; i++) {
		bitmap = ns_map;
		for (j = 0; j < nr_ns; j++) {
			bit = ffsll(bitmap) - 1;
			if (bit < 0)
				break;

			bitmap ^= BIT_ULL(bit);

			effect->nsid = le32_to_cpu(ns_grp->act_list[bit]);
			CHK_EXPR_NUM_LT0_RTN(dpu_mix_queue_cmds_submit_together(
				priv, i * nr_ns + j), -EPERM);
		}
	}

	CHK_EXPR_NUM_LT0_RTN(
		ut_ring_sqs_doorbell(priv, ndev->iosqs, nr_q_pair), -EPERM);
	
	for (i = 0; i < rate; i++) {
		for (j = 0; j < nr_ns; j++) {
			CHK_EXPR_NUM_LT0_RTN(dpu_mix_queue_cqe_reap_together(
				priv, i * nr_ns + j), -EPERM);
		}
	}

	return 0;
}

static int dpu_mix_work(struct case_data *priv, struct utc_dpu_mix_param *param)
{
	struct nvme_tool *tool = priv->tool;
	uint64_t ns_map;
	uint32_t ns_cnt;
	uint32_t nr_q_pair;
	int ret;

	dpu_mix_init_ns_map(tool, &ns_map);
	ns_cnt = dpu_mix_init_param(tool, param, ns_map);
	ret = dpu_mix_send_param(priv, param);
	if (ret < 0)
		return ret;

	ret = dpu_mix_init_queue(priv, &nr_q_pair, ns_cnt);
	if (ret < 0)
		return ret;

	CHK_EXPR_NUM_LT0_GOTO(
		dpu_mix_travel_mode1(priv, ns_cnt, nr_q_pair, ns_map), 
		ret, -EPERM, deinit_queue);
	CHK_EXPR_NUM_LT0_GOTO(
		dpu_mix_travel_mode2(priv, ns_cnt, nr_q_pair, ns_map), 
		ret, -EPERM, deinit_queue);

deinit_queue:
	ret |= dpu_mix_deinit_queue(priv, nr_q_pair);
	return ret;
}

static int case_dpu_mix(struct nvme_tool *tool, struct case_data *priv)
{
	struct utc_dpu_mix_param *param = NULL;
	int loop = 1;
	int ret;

	dpu_mix_check_size();

	ret = posix_memalign((void **)&param, NVME_TOOL_RW_BUF_ALIGN, sizeof(*param));
	if (ret) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
	}

	ret = ut_case_alloc_config_effect(&priv->cfg);
	if (ret < 0)
		goto rls_param;

	do {
		ret = dpu_mix_work(priv, param);
		if (ret < 0)
			break;
	} while (--loop > 0);

	ut_case_release_config_effect(&priv->cfg);
rls_param:
	free(param);
	return ret;
}
NVME_CASE_SYMBOL(case_dpu_mix, "?");
