/**
 * @file cmd.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-10-31
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

#define UT_CMD_F_OPTION_MASK		0xf
#define UT_CMD_F_OPTION_SUBMIT		1
#define UT_CMD_F_OPTION_SEND		2
#define UT_CMD_F_DATA_RANDOM		BIT(8) /**< use random data */

static const char *ut_cmd_flag_option_name(uint32_t flag)
{
	switch (flag & UT_CMD_F_OPTION_MASK) {
	case UT_CMD_F_OPTION_SUBMIT:
		return "Submit";
	case UT_CMD_F_OPTION_SEND:
		return "Send";
	default:
		return "Unknown";
	}
}

struct copy_resource *ut_alloc_copy_resource(uint32_t nr_desc, uint8_t format)
{
	struct copy_resource *source = NULL;
	int ret;

	source = zalloc(sizeof(struct copy_resource) + 
		nr_desc * sizeof(struct copy_range));
	if (!source) {
		pr_err("failed to alloc copy resource!\n");
		return NULL;
	}
	source->nr_entry = nr_desc;
	source->format = format;


	switch (format) {
	case NVME_COPY_DESC_FMT_32B:
		source->size = sizeof(struct nvme_copy_desc_fmt0) * nr_desc;
		break;
	case NVME_COPY_DESC_FMT_40B:
		source->size = sizeof(struct nvme_copy_desc_fmt1) * nr_desc;
		break;
	default:
		pr_err("Invalid format: %d!\n", format);
		goto out;
	}

	ret = posix_memalign(&source->desc, NVME_TOOL_RW_BUF_ALIGN, 
		source->size);
	if (ret) {
		pr_err("failed to alloc copy descriptors!\n");
		goto out;
	}
	memset(source->desc, 0, source->size);
	return source;
out:
	free(source);
	return NULL;
}

void ut_release_copy_resource(struct copy_resource *copy)
{
	if (!copy) {
		pr_debug("double free ?\n");
		return;
	}
	free(copy->desc);
	free(copy);
}

static int ut_init_copy_descs(struct copy_resource *copy)
{
	struct copy_range *entry = copy->entry;
	struct nvme_copy_desc_fmt0 *fmt0;
	struct nvme_copy_desc_fmt1 *fmt1;
	int i;

	switch (copy->format) {
	case NVME_COPY_DESC_FMT_32B:
		fmt0 = copy->desc;

		for (i = 0; i < copy->nr_entry; i++) {
			fmt0[i].slba = cpu_to_le64(entry[i].slba);
			fmt0[i].length = cpu_to_le16(entry[i].nlb - 1);
		}
		break;
	
	case NVME_COPY_DESC_FMT_40B:
		fmt1 = copy->desc;

		for (i = 0; i < copy->nr_entry; i++) {
			fmt1[i].slba = cpu_to_le64(entry[i].slba);
			fmt1[i].length = cpu_to_le16(entry[i].nlb - 1);
		}
		break;

	default:
		pr_err("invalid format: %d!\n", copy->format);
		return -EINVAL;
	}
	return 0;
}

static int ut_deal_io_read_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, uint32_t flag)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect *effect = priv->cfg.effect;
	struct nvme_rwc_wrapper wrap = {0};
	void *buf;
	uint32_t buf_size;
	uint32_t blk_size;
	int ret;

	ut_rpt_record_case_step(rpt, 
		"%s a I/O read cmd(SLBA:0x%lx, NLB:%u) => NSID: 0x%x, SQ: %u",
		ut_cmd_flag_option_name(flag), 
		slba, nlb, effect->nsid, sq->sqid);

	ret = nvme_id_ns_lbads(ndev->ns_grp, effect->nsid, &blk_size);
	if (ret < 0)
		return ret;

	if (effect->cmd.read.buf) {
		buf = effect->cmd.read.buf;
		buf_size = effect->cmd.read.size;
	} else {
		buf = tool->rbuf;
		buf_size = tool->rbuf_size;
	}

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.flags = effect->cmd.read.flags;
	if (effect->inject_nsid)
		wrap.nsid = effect->inject.nsid;
	else
		wrap.nsid = effect->nsid;

	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.control = effect->cmd.read.control;
	wrap.buf = buf;
	wrap.size = nlb * blk_size;
	wrap.check_none = effect->check_none;

	BUG_ON(wrap.size > buf_size);
	memset(wrap.buf, 0, wrap.size);

	switch (flag & UT_CMD_F_OPTION_MASK) {
	case UT_CMD_F_OPTION_SUBMIT:
		ret = nvme_cmd_io_read(ndev->fd, &wrap);
		break;
	case UT_CMD_F_OPTION_SEND:
		ret = nvme_io_read(ndev, &wrap);
		break;
	default:
		pr_err("invalid flag: 0x%x\n", flag);
		ret = -EINVAL;
	}

	return ret;
}

static int ut_deal_io_write_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, uint32_t flag)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect *effect = priv->cfg.effect;
	struct nvme_rwc_wrapper wrap = {0};
	void *buf;
	uint32_t buf_size;
	uint32_t blk_size;
	int ret;

	ut_rpt_record_case_step(rpt, 
		"%s a I/O write cmd(SLBA:0x%lx, NLB:%u) => NSID: 0x%x, SQ: %u",
		ut_cmd_flag_option_name(flag), 
		slba, nlb, effect->nsid, sq->sqid);

	ret = nvme_id_ns_lbads(ndev->ns_grp, effect->nsid, &blk_size);
	if (ret < 0)
		return ret;

	if (effect->cmd.write.buf) {
		buf = effect->cmd.write.buf;
		buf_size = effect->cmd.write.size;
	} else {
		buf = tool->wbuf;
		buf_size = tool->wbuf_size;
	}

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.flags = effect->cmd.write.flags;
	if (effect->inject_nsid)
		wrap.nsid = effect->inject.nsid;
	else
		wrap.nsid = effect->nsid;

	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.control = effect->cmd.read.control;
	wrap.buf = buf;
	wrap.size = nlb * blk_size;
	wrap.control = effect->cmd.write.dtype << 4;
	wrap.dspec = effect->cmd.write.dspec;
	wrap.check_none = effect->check_none;

	BUG_ON(wrap.size > buf_size);
	if (flag & UT_CMD_F_DATA_RANDOM)
		fill_data_with_random(wrap.buf, wrap.size);

	switch (flag & UT_CMD_F_OPTION_MASK) {
	case UT_CMD_F_OPTION_SUBMIT:
		ret = nvme_cmd_io_write(ndev->fd, &wrap);
		break;
	case UT_CMD_F_OPTION_SEND:
		ret = nvme_io_write(ndev, &wrap);
		break;
	default:
		pr_err("invalid flag: 0x%x\n", flag);
		ret = -EINVAL;
	}

	return ret;
}

static int ut_deal_io_compare_cmd(struct case_data *priv, 
	struct nvme_sq_info *sq, uint64_t slba, uint32_t nlb, uint32_t flag)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect *effect = priv->cfg.effect;
	struct nvme_rwc_wrapper wrap = {0};
	void *buf;
	uint32_t buf_size;
	uint32_t blk_size;
	int ret;

	ut_rpt_record_case_step(rpt, 
		"%s a I/O compare cmd(SLBA:0x%lx, NLB:%u) => NSID: 0x%x, SQ: %u",
		ut_cmd_flag_option_name(flag), 
		slba, nlb, effect->nsid, sq->sqid);

	ret = nvme_id_ns_lbads(ndev->ns_grp, effect->nsid, &blk_size);
	if (ret < 0)
		return ret;

	if (effect->cmd.compare.buf) {
		buf = effect->cmd.compare.buf;
		buf_size = effect->cmd.compare.size;
	} else {
		buf = tool->rbuf;
		buf_size = tool->rbuf_size;
	}

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.flags = effect->cmd.compare.flags;
	if (effect->inject_nsid)
		wrap.nsid = effect->inject.nsid;
	else
		wrap.nsid = effect->nsid;

	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = buf;
	wrap.size = nlb * blk_size;
	wrap.check_none = effect->check_none;

	BUG_ON(wrap.size > buf_size);

	switch (flag & UT_CMD_F_OPTION_MASK) {
	case UT_CMD_F_OPTION_SUBMIT:
		ret = nvme_cmd_io_compare(ndev->fd, &wrap);
		break;
	case UT_CMD_F_OPTION_SEND:
		ret = nvme_io_compare(ndev, &wrap);
		break;
	default:
		pr_err("invalid flag: 0x%x\n", flag);
		ret = -EINVAL;
	}
	
	return ret;
}

static int ut_deal_io_verify_cmd(struct case_data *priv, 
	struct nvme_sq_info *sq, uint64_t slba, uint32_t nlb, uint32_t flag)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect *effect = priv->cfg.effect;
	struct nvme_verify_wrapper wrap = {0};
	int ret;

	ut_rpt_record_case_step(rpt, 
		"%s a I/O verify cmd(SLBA:0x%lx, NLB:%u) => NSID: 0x%x, SQ: %u",
		ut_cmd_flag_option_name(flag), 
		slba, nlb, effect->nsid, sq->sqid);

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.flags = effect->cmd.verify.flags;
	if (effect->inject_nsid)
		wrap.nsid = effect->inject.nsid;
	else
		wrap.nsid = effect->nsid;

	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.prinfo = effect->cmd.verify.prinfo;
	wrap.check_none = effect->check_none;

	switch (flag & UT_CMD_F_OPTION_MASK) {
	case UT_CMD_F_OPTION_SUBMIT:
		ret = nvme_cmd_io_verify(ndev->fd, &wrap);
		break;
	case UT_CMD_F_OPTION_SEND:
		ret = nvme_io_verify(ndev, &wrap);
		break;
	default:
		pr_err("invalid flag: 0x%x\n", flag);
		ret = -EINVAL;
	}

	return ret;
}

static int ut_deal_io_copy_cmd(struct case_data *priv, 
	struct nvme_sq_info *sq, struct copy_resource *copy, uint32_t flag)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect *effect = priv->cfg.effect;
	struct nvme_copy_wrapper wrap = {0};
	int ret;

	ut_rpt_record_case_step(rpt, 
		"%s a I/O copy cmd(SLBA:0x%lx, desc:%u|%u) => NSID: 0x%x, SQ: %u",
		ut_cmd_flag_option_name(flag), copy->slba, copy->format, 
		copy->nr_entry, effect->nsid, sq->sqid);

	ret = ut_init_copy_descs(copy);
	if (ret < 0)
		return ret;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.flags = effect->cmd.copy.flags;
	if (effect->inject_nsid)
		wrap.nsid = effect->inject.nsid;
	else
		wrap.nsid = effect->nsid;

	wrap.slba = copy->slba;
	wrap.ranges = copy->nr_entry - 1;
	wrap.desc_fmt = copy->format;
	wrap.desc = copy->desc;
	wrap.size = copy->size;
	wrap.prinfor = effect->cmd.copy.prinfor;
	wrap.dtype = effect->cmd.copy.dtype;
	wrap.prinfow = effect->cmd.copy.prinfow;
	wrap.dspec = effect->cmd.copy.dspec;
	wrap.check_none = effect->check_none;

	switch (flag & UT_CMD_F_OPTION_MASK) {
	case UT_CMD_F_OPTION_SUBMIT:
		ret = nvme_cmd_io_copy(ndev->fd, &wrap);
		break;
	case UT_CMD_F_OPTION_SEND:
		ret = nvme_io_copy(ndev, &wrap);
		break;
	default:
		pr_err("invalid flag: 0x%x\n", flag);
		ret = -EINVAL;
	}

	return ret;
}

static int ut_deal_io_zone_append_cmd(struct case_data *priv, 
	struct nvme_sq_info *sq, uint64_t zslba, uint32_t nlb, uint32_t flag)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect *effect = priv->cfg.effect;
	struct nvme_zone_append_wrapper wrap = {0};
	void *buf;
	uint32_t buf_size;
	uint32_t blk_size;
	int ret;

	ut_rpt_record_case_step(rpt, 
		"%s a I/O zone append cmd(ZSLBA:0x%lx, NLB:%u) "
		"=> NSID: 0x%x, SQ: %u",
		ut_cmd_flag_option_name(flag), 
		zslba, nlb, effect->nsid, sq->sqid);

	ret = nvme_id_ns_lbads(ndev->ns_grp, effect->nsid, &blk_size);
	if (ret < 0)
		return ret;

	if (effect->cmd.zone_append.buf) {
		buf = effect->cmd.zone_append.buf;
		buf_size = effect->cmd.zone_append.size;
	} else {
		buf = tool->wbuf;
		buf_size = tool->wbuf_size;
	}

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	if (effect->inject_nsid)
		wrap.nsid = effect->inject.nsid;
	else
		wrap.nsid = effect->nsid;

	wrap.zslba = zslba;
	wrap.nlb = nlb;
	wrap.piremap = effect->cmd.zone_append.piremap;
	wrap.buf = buf;
	wrap.size = nlb * blk_size;
	wrap.check_none = effect->check_none;

	BUG_ON(wrap.size > buf_size);

	switch (flag & UT_CMD_F_OPTION_MASK) {
	case UT_CMD_F_OPTION_SUBMIT:
		ret = nvme_cmd_io_zone_append(ndev->fd, &wrap);
		break;
	case UT_CMD_F_OPTION_SEND:
		ret = nvme_io_zone_append(ndev, &wrap);
		break;
	default:
		pr_err("invalid flag: 0x%x\n", flag);
		ret = -EINVAL;
	}

	return ret;
}

/**
 * @brief Submit I/O read command: read data from specified region
 * 
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 * @note If user doesn't provide read buffer, then use default buffer 
 * 	prepared in nvme tool.
 */
int ut_submit_io_read_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
{
	return ut_deal_io_read_cmd(priv, sq, slba, nlb, UT_CMD_F_OPTION_SUBMIT);
}

int ut_submit_io_read_cmds(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, int nr_cmd)
{
	int ret;

	for (; nr_cmd > 0; nr_cmd--) {
		ret = ut_submit_io_read_cmd(priv, sq, slba, nlb);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/**
 * @brief Submit I/O read command to read random region in the specified 
 * 	namespace.
 * 
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 * @note If user doesn't provide read buffer, then use default buffer 
 * 	prepared in nvme tool.
 */
int ut_submit_io_read_cmd_random_region(struct case_data *priv, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_config_effect *effect = priv->cfg.effect;
	uint64_t slba;
	uint32_t nlb;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ndev->ns_grp, effect->nsid, &nsze);
	if (ret < 0)
		return ret;

	/* ensure "slba + nlb <= nsze */
	slba = rand() % (nsze / 2);
	if (effect->cmd.read.use_nlb && effect->cmd.read.nlb)
		nlb = effect->cmd.read.nlb;
	else
		nlb = rand() % min_t(uint64_t, nsze / 2, 256) + 1;

	if (rand() % 2)
		effect->cmd.read.control |= NVME_RW_FUA;
	else
		effect->cmd.read.control &= ~NVME_RW_FUA;

	return ut_submit_io_read_cmd(priv, sq, slba, nlb);
}

int ut_submit_io_read_cmds_random_region(struct case_data *priv,
	struct nvme_sq_info *sq, int nr_cmd)
{
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect *effect = priv->cfg.effect;
	int recovery = 0;
	int ret;
	int i;

	ut_rpt_record_case_step(rpt, 
		"Submit %d I/O read cmds => NSID: 0x%x, SQ: %u",
		nr_cmd, effect->nsid, sq->sqid);

	if (!rpt->rcd_pause) {
		rpt->rcd_pause = 1;
		recovery = 1;
	}

	for (i = 0; i < nr_cmd; i++) {
		ret = ut_submit_io_read_cmd_random_region(priv, sq);
		if (ret < 0)
			goto out;
	}

out:
	if (recovery)
		rpt->rcd_pause = 0;

	return ret;
}

/**
 * @brief Submit I/O write command: write data to specified region
 * 
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 * @note If user doesn't provide write buffer, then use default buffer 
 * 	prepared in nvme tool.
 */
int ut_submit_io_write_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
{
	return ut_deal_io_write_cmd(priv, sq, slba, nlb, UT_CMD_F_OPTION_SUBMIT);
}

int ut_submit_io_write_cmds(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, int nr_cmd)
{
	int ret;

	for (; nr_cmd > 0; nr_cmd--) {
		ret = ut_submit_io_write_cmd(priv, sq, slba, nlb);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int ut_submit_io_write_cmd_random_data(struct case_data *priv, 
	struct nvme_sq_info *sq, uint64_t slba, uint32_t nlb)
{
	return ut_deal_io_write_cmd(priv, sq, slba, nlb, 
		UT_CMD_F_OPTION_SUBMIT | UT_CMD_F_DATA_RANDOM);
}

/**
 * @brief Submit I/O write command: write random data to random region
 * 
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int ut_submit_io_write_cmd_random_d_r(struct case_data *priv,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_config_effect *effect = priv->cfg.effect;
	uint64_t slba;
	uint32_t nlb;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ndev->ns_grp, effect->nsid, &nsze);
	if (ret < 0)
		return ret;

	slba = rand() % (nsze / 2);
	if (effect->cmd.write.use_nlb && effect->cmd.write.nlb)
		nlb = effect->cmd.write.nlb;
	else
		nlb = rand() % min_t(uint64_t, nsze / 2, 256) + 1;

	if (rand() % 2)
		effect->cmd.write.control |= NVME_RW_FUA;
	else
		effect->cmd.write.control &= ~NVME_RW_FUA;

	return ut_submit_io_write_cmd_random_data(priv, sq, slba, nlb);
}

int ut_submit_io_write_cmds_random_d_r(struct case_data *priv,
	struct nvme_sq_info *sq, int nr_cmd)
{
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect *effect = priv->cfg.effect;
	int recovery = 0;
	int ret;
	int i;

	ut_rpt_record_case_step(rpt, 
		"Submit %d I/O write cmds => NSID: 0x%x, SQ: %u",
		nr_cmd, effect->nsid, sq->sqid);

	if (!rpt->rcd_pause) {
		rpt->rcd_pause = 1;
		recovery = 1;
	}

	for (i = 0; i < nr_cmd; i++) {
		ret = ut_submit_io_write_cmd_random_d_r(priv, sq);
		if (ret < 0)
			goto out;
	}

out:
	if (recovery)
		rpt->rcd_pause = 0;

	return ret;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int ut_submit_io_compare_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
{
	return ut_deal_io_compare_cmd(priv, sq, slba, nlb, UT_CMD_F_OPTION_SUBMIT);
}

int ut_submit_io_compare_cmds(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, int nr_cmd)
{
	int ret;

	for (; nr_cmd > 0; nr_cmd--) {
		ret = ut_submit_io_compare_cmd(priv, sq, slba, nlb);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int ut_submit_io_compare_cmd_random_region(struct case_data *priv, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_config_effect *effect = priv->cfg.effect;
	uint64_t slba;
	uint32_t nlb;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ndev->ns_grp, effect->nsid, &nsze);
	if (ret < 0)
		return ret;

	/* ensure "slba + nlb <= nsze */
	slba = rand() % (nsze / 2);
	if (effect->cmd.compare.use_nlb && effect->cmd.compare.nlb)
		nlb = effect->cmd.compare.nlb;
	else
		nlb = rand() % min_t(uint64_t, nsze / 2, 256) + 1;

	return ut_submit_io_compare_cmd(priv, sq, slba, nlb);
}

int ut_submit_io_compare_cmds_random_region(struct case_data *priv,
	struct nvme_sq_info *sq, int nr_cmd)
{
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect *effect = priv->cfg.effect;
	int recovery = 0;
	int ret;
	int i;

	ut_rpt_record_case_step(rpt, 
		"Submit %d I/O compare cmds => NSID: 0x%x, SQ: %u",
		nr_cmd, effect->nsid, sq->sqid);

	if (!rpt->rcd_pause) {
		rpt->rcd_pause = 1;
		recovery = 1;
	}

	for (i = 0; i < nr_cmd; i++) {
		ret = ut_submit_io_compare_cmd_random_region(priv, sq);
		if (ret < 0)
			goto out;
	}

out:
	if (recovery)
		rpt->rcd_pause = 0;

	return ret;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int ut_submit_io_verify_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
{
	return ut_deal_io_verify_cmd(priv, sq, slba, nlb, UT_CMD_F_OPTION_SUBMIT);
}

int ut_submit_io_verify_cmds(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, int nr_cmd)
{
	int ret;

	for (; nr_cmd > 0; nr_cmd--) {
		ret = ut_submit_io_verify_cmd(priv, sq, slba, nlb);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int ut_submit_io_verify_cmd_random_region(struct case_data *priv, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_config_effect *effect = priv->cfg.effect;
	uint64_t nsze;
	uint64_t slba;
	uint32_t nlb;
	uint32_t blk_size;
	uint32_t pg_size;
	uint32_t limit;
	uint8_t vsl;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, effect->nsid, &nsze);
	if (ret < 0)
		return ret;

	/* ensure "slba + nlb <= nsze */
	slba = rand() % (nsze / 2);
	if (effect->cmd.verify.use_nlb && effect->cmd.verify.nlb) {
		nlb = effect->cmd.verify.nlb;
	} else {
		ret = nvme_id_ctrl_nvm_vsl(ndev->ctrl);
		if (ret < 0)
			return ret;
		vsl = ret;

		nlb = rand() % min_t(uint64_t, nsze / 2, 256) + 1;
		if (vsl) {
			pg_size = nvme_cap_mem_page_size_min(ndev->ctrl);
			nvme_id_ns_lbads(ns_grp, effect->nsid, &blk_size);
			limit = pg_size * (1 << vsl) / blk_size;
			nlb = min_t(uint32_t, nlb, limit);
		}
	}
	
	return ut_submit_io_verify_cmd(priv, sq, slba, nlb);
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int ut_submit_io_copy_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	struct copy_resource *copy)
{
	return ut_deal_io_copy_cmd(priv, sq, copy, UT_CMD_F_OPTION_SUBMIT);
}

int ut_submit_io_copy_cmds(struct case_data *priv, struct nvme_sq_info *sq,
	struct copy_resource *copy, int nr_cmd)
{
	int ret;

	for (; nr_cmd > 0; nr_cmd--) {
		ret = ut_submit_io_copy_cmd(priv, sq, copy);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int ut_submit_io_fused_cw_cmd(struct case_data *priv, struct nvme_sq_info *sq, 
	uint64_t slba, uint32_t nlb)
{
	struct config_fused_cmd bkup;
	struct case_config_effect *effect = priv->cfg.effect;
	int ret;

	memcpy(&bkup, &effect->cmd.fused, sizeof(bkup));

	memset(&effect->cmd, 0, sizeof(effect->cmd));
	effect->cmd.compare.flags = NVME_CMD_FUSE_FIRST | bkup.flags;
	if (bkup.buf1) {
		effect->cmd.compare.buf = bkup.buf1;
		effect->cmd.compare.size = bkup.buf1_size;
	}

	ret = ut_submit_io_compare_cmd(priv, sq, slba, nlb);
	if (ret < 0)
		return ret;

	memset(&effect->cmd, 0, sizeof(effect->cmd));
	effect->cmd.write.flags = NVME_CMD_FUSE_SECOND | bkup.flags;
	if (bkup.buf2) {
		effect->cmd.write.buf = bkup.buf2;
		effect->cmd.write.size = bkup.buf2_size;
	}

	ret = ut_submit_io_write_cmd(priv, sq, slba, nlb);
	if (ret < 0)
		return ret;

	return 0;
}

int ut_submit_io_fused_cw_cmds(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, int nr_cmd)
{
	int ret;

	for (; nr_cmd > 0; nr_cmd--) {
		ret = ut_submit_io_fused_cw_cmd(priv, sq, slba, nlb);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int ut_submit_io_fused_cw_cmd_random_region(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_config_effect *effect = priv->cfg.effect;
	uint64_t slba;
	uint32_t nlb;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ndev->ns_grp, effect->nsid, &nsze);
	if (ret < 0)
		return ret;

	slba = rand() % (nsze / 2);
	if (effect->cmd.fused.use_nlb && effect->cmd.fused.nlb)
		nlb = effect->cmd.fused.nlb;
	else
		nlb = rand() % min_t(uint64_t, nsze / 2, 256) + 1;

	return ut_submit_io_fused_cw_cmd(priv, sq, slba, nlb);
}

int ut_submit_io_zone_append_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t zslba, uint32_t nlb)
{
	return ut_deal_io_zone_append_cmd(priv, sq, zslba, nlb, 
		UT_CMD_F_OPTION_SUBMIT);
}

/**
 * @return the number of commands submitted on success, otherwise a negative
 *  errno.
 */
int ut_submit_io_random_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint32_t select)
{
	uint32_t num;
	int ret;

	select &= ~UT_CMD_SEL_IO_COPY;
	if (!select) {
		pr_warn("nothing selected!\n");
		return 0;
	}

	do {
		num = rand() % UT_CMD_SEL_NUM;
	} while (!(select & BIT(num)));

	switch (BIT(num)) {
	case UT_CMD_SEL_IO_READ:
		ret = ut_submit_io_read_cmd_random_region(priv, sq);
		break;
	case UT_CMD_SEL_IO_WRITE:
		ret = ut_submit_io_write_cmd_random_d_r(priv, sq);
		break;
	case UT_CMD_SEL_IO_COMPARE:
		ret = ut_submit_io_compare_cmd_random_region(priv, sq);
		break;
	case UT_CMD_SEL_IO_VERIFY:
		ret = ut_submit_io_verify_cmd_random_region(priv, sq);
		break;
	case UT_CMD_SEL_IO_FUSED_CW:
		ret = ut_submit_io_fused_cw_cmd_random_region(priv, sq);
		break;
	default:
		pr_err("select bit %u is invalid!\n", num);
		ret = -EINVAL;
	}

	return ret < 0 ? ret : ((BIT(num) == UT_CMD_SEL_IO_FUSED_CW) ? 2 : 1);
}

/**
 * @return the number of commands submitted on success, otherwise a negative
 *  errno.
 */
int ut_submit_io_random_cmds(struct case_data *priv, struct nvme_sq_info *sq,
	uint32_t select, int nr_cmd)
{
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect *effect = priv->cfg.effect;
	int submitted = 0;
	int ret;

	ut_rpt_record_case_step(rpt, 
		"Submit %d I/O random cmds => NSID: 0x%x, SQ: %u",
		nr_cmd, effect->nsid, sq->sqid);

	for (; nr_cmd > 0; nr_cmd--) {
		ret = ut_submit_io_random_cmd(priv, sq, select);
		if (ret < 0)
			return ret;
		submitted += ret;
	}

	return submitted;
}

int ut_submit_io_random_cmds_to_sqs(struct case_data *priv, 
	struct nvme_sq_info *sq, uint32_t select, int nr_cmd, int nr_sq)
{
	int ret;
	int i;

	for (i = 0; i < nr_sq; i++) {
		ret = ut_submit_io_random_cmds(priv, &sq[i], select, nr_cmd);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/**
 * @brief Send I/O read command include retrieving CQ completion data. 
 * 
 * @return 0 on success, otherwise a negative errno
 * @note If user doesn't provide read buffer, then use default buffer 
 * 	prepared in nvme tool.
 */
int ut_send_io_read_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
{
	return ut_deal_io_read_cmd(priv, sq, slba, nlb, UT_CMD_F_OPTION_SEND);
}

int ut_send_io_read_cmd_random_region(struct case_data *priv, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_config_effect *effect = priv->cfg.effect;
	uint64_t slba;
	uint32_t nlb;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ndev->ns_grp, effect->nsid, &nsze);
	if (ret < 0)
		return ret;

	slba = rand() % (nsze / 2);
	if (effect->cmd.read.use_nlb && effect->cmd.read.nlb)
		nlb = effect->cmd.read.nlb;
	else
		nlb = rand() % min_t(uint64_t, nsze / 2, 256) + 1;

	if (rand() % 2)
		effect->cmd.read.control |= NVME_RW_FUA;
	else
		effect->cmd.read.control &= ~NVME_RW_FUA;

	return ut_send_io_read_cmd(priv, sq, slba, nlb);
}

int ut_send_io_read_cmds_random_region(struct case_data *priv,
	struct nvme_sq_info *sq, int nr_cmd)
{
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect *effect = priv->cfg.effect;
	int recovery = 0;
	int ret;
	int i;

	ut_rpt_record_case_step(rpt, 
		"Send %d I/O read cmds => NSID: 0x%x, SQ: %u",
		nr_cmd, effect->nsid, sq->sqid);

	if (!rpt->rcd_pause) {
		rpt->rcd_pause = 1;
		recovery = 1;
	}

	for (i = 0; i < nr_cmd; i++) {
		ret = ut_send_io_read_cmd_random_region(priv, sq);
		if (ret < 0)
			goto out;
	}

out:
	if (recovery)
		rpt->rcd_pause = 0;

	return ret;
}

int ut_send_io_write_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
{
	return ut_deal_io_write_cmd(priv, sq, slba, nlb, UT_CMD_F_OPTION_SEND);
}

int ut_send_io_write_cmd_random_data(struct case_data *priv, 
	struct nvme_sq_info *sq, uint64_t slba, uint32_t nlb)
{
	return ut_deal_io_write_cmd(priv, sq, slba, nlb, 
		UT_CMD_F_OPTION_SEND | UT_CMD_F_DATA_RANDOM);
}

int ut_send_io_write_cmd_random_d_r(struct case_data *priv,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_config_effect *effect = priv->cfg.effect;
	uint64_t slba;
	uint32_t nlb;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ndev->ns_grp, effect->nsid, &nsze);
	if (ret < 0)
		return ret;

	/* ensure "slba + nlb <= nsze */
	slba = rand() % (nsze / 2);
	if (effect->cmd.write.use_nlb && effect->cmd.write.nlb)
		nlb = effect->cmd.write.nlb;
	else
		nlb = rand() % min_t(uint64_t, nsze / 2, 256) + 1;

	if (rand() % 2)
		effect->cmd.write.control |= NVME_RW_FUA;
	else
		effect->cmd.write.control &= ~NVME_RW_FUA;

	return ut_send_io_write_cmd_random_data(priv, sq, slba, nlb);
}

int ut_send_io_write_cmds_random_d_r(struct case_data *priv,
	struct nvme_sq_info *sq, int nr_cmd)
{
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect *effect = priv->cfg.effect;
	int recovery = 0;
	int ret;
	int i;

	ut_rpt_record_case_step(rpt, 
		"Send %d I/O write cmds => NSID: 0x%x, SQ: %u",
		nr_cmd, effect->nsid, sq->sqid);

	if (!rpt->rcd_pause) {
		rpt->rcd_pause = 1;
		recovery = 1;
	}

	for (i = 0; i < nr_cmd; i++) {
		ret = ut_send_io_write_cmd_random_d_r(priv, sq);
		if (ret < 0)
			goto out;
	}

out:
	if (recovery)
		rpt->rcd_pause = 0;

	return ret;
}

int ut_send_io_compare_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
{
	return ut_deal_io_compare_cmd(priv, sq, slba, nlb, UT_CMD_F_OPTION_SEND);
}

int ut_send_io_compare_cmd_random_region(struct case_data *priv, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_config_effect *effect = priv->cfg.effect;
	uint64_t slba;
	uint32_t nlb;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ndev->ns_grp, effect->nsid, &nsze);
	if (ret < 0)
		return ret;

	/* ensure "slba + nlb <= nsze */
	slba = rand() % (nsze / 2);
	if (effect->cmd.compare.use_nlb && effect->cmd.compare.nlb)
		nlb = effect->cmd.compare.nlb;
	else
		nlb = rand() % min_t(uint64_t, nsze / 2, 256) + 1;

	return ut_send_io_compare_cmd(priv, sq, slba, nlb);
}

int ut_send_io_verify_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
{
	return ut_deal_io_verify_cmd(priv, sq, slba, nlb, UT_CMD_F_OPTION_SEND);
}

int ut_send_io_verify_cmd_random_region(struct case_data *priv, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_config_effect *effect = priv->cfg.effect;
	uint64_t nsze;
	uint64_t slba;
	uint32_t nlb;
	uint32_t blk_size;
	uint32_t pg_size;
	uint32_t limit;
	uint8_t vsl;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, effect->nsid, &nsze);
	if (ret < 0)
		return ret;

	/* ensure "slba + nlb <= nsze */
	slba = rand() % (nsze / 2);
	if (effect->cmd.verify.use_nlb && effect->cmd.verify.nlb) {
		nlb = effect->cmd.verify.nlb;
	} else {
		ret = nvme_id_ctrl_nvm_vsl(ndev->ctrl);
		if (ret < 0)
			return ret;
		vsl = ret;

		nlb = rand() % min_t(uint64_t, nsze / 2, 256) + 1;
		if (vsl) {
			pg_size = nvme_cap_mem_page_size_min(ndev->ctrl);
			nvme_id_ns_lbads(ns_grp, effect->nsid, &blk_size);
			limit = pg_size * (1 << vsl) / blk_size;
			nlb = min_t(uint32_t, nlb, limit);
		}
	}
	
	return ut_send_io_verify_cmd(priv, sq, slba, nlb);
}

int ut_send_io_copy_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	struct copy_resource *copy)
{
	return ut_deal_io_copy_cmd(priv, sq, copy, UT_CMD_F_OPTION_SEND);
}

int ut_send_io_copy_cmd_random_region(struct case_data *priv, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_config_effect *effect = priv->cfg.effect;
	struct copy_resource *copy = NULL;
	uint64_t nsze;
	uint32_t mcl;
	uint32_t msrc;
	uint16_t mssrl;
	uint32_t nr_desc;
	uint32_t sum = 0;
	uint16_t limit;
	uint8_t format;
	int i;
	int retry = 100;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, effect->nsid, &nsze);
	if (ret < 0)
		return ret;
	ret = nvme_id_ns_mcl(ns_grp, effect->nsid, &mcl);
	if (ret < 0)
		return ret;

	msrc = nvme_id_ns_msrc(ns_grp, effect->nsid);
	mssrl = nvme_id_ns_mssrl(ns_grp, effect->nsid);

	limit = min3(nsze / 4, (uint64_t)mssrl, (uint64_t)256);
	if (effect->cmd.copy.use_desc && effect->cmd.copy.nr_desc)
		nr_desc = effect->cmd.copy.nr_desc;
	else
		nr_desc = rand() % min_t(uint32_t, msrc, 16) + 1;

	if (effect->cmd.copy.use_fmt)
		format = effect->cmd.copy.fmt;
	else
		format = (rand() % 2) ? NVME_COPY_DESC_FMT_40B : NVME_COPY_DESC_FMT_32B;

	copy = ut_alloc_copy_resource(nr_desc, format);
	if (!copy)
		return -ENOMEM;

	do {
		if (copy->slba) {
			pr_warn("target slba + nlb > nsze! try again?\n");
			retry--;
		}

		do {
			if (sum) {
				pr_warn("total nlb > MCL! try again?\n");
				retry--;
			}

			if (retry < 0) {
				pr_err("failed to generate random configuration!");
				goto out;
			}

			for (i = 0, sum = 0; i < copy->nr_entry; i++) {
				copy->entry[i].slba = rand() % (nsze / 4);
				copy->entry[i].nlb = rand() % limit + 1;
				sum += copy->entry[i].nlb;
			}

		} while (sum > mcl);
		copy->slba = rand() % (nsze / 4);

	} while ((copy->slba + sum) > nsze);

	ret = ut_send_io_copy_cmd(priv, sq, copy);
	if (ret < 0)
		goto out;

out:
	ut_release_copy_resource(copy);
	return ret;
}

int ut_send_io_fused_cw_cmd(struct case_data *priv, 
	struct nvme_sq_info *sq, uint64_t slba, uint32_t nlb)
{
	struct config_fused_cmd bkup;
	struct case_config_effect *effect = priv->cfg.effect;
	int ret;

	memcpy(&bkup, &effect->cmd.fused, sizeof(bkup));

	memset(&effect->cmd, 0, sizeof(effect->cmd));
	effect->cmd.compare.flags = NVME_CMD_FUSE_FIRST | bkup.flags;
	if (bkup.buf1) {
		effect->cmd.compare.buf = bkup.buf1;
		effect->cmd.compare.size = bkup.buf1_size;
	}

	ret = ut_submit_io_compare_cmd(priv, sq, slba, nlb);
	if (ret < 0)
		return ret;

	memset(&effect->cmd, 0, sizeof(effect->cmd));
	effect->cmd.write.flags = NVME_CMD_FUSE_SECOND | bkup.flags;
	if (bkup.buf2) {
		effect->cmd.write.buf = bkup.buf2;
		effect->cmd.write.size = bkup.buf2_size;
	}

	ret = ut_submit_io_write_cmd(priv, sq, slba, nlb);
	if (ret < 0)
		return ret;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0)
		return ret;

	if (effect->check_none)
		ret = ut_reap_cq_entry_no_check_by_id(priv, sq->cqid, 2);
	else
		ret = ut_reap_cq_entry_check_status_by_id(priv, sq->cqid, 2);

	return ret;
}

int ut_send_io_fused_cw_cmd_random_region(
	struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_config_effect *effect = priv->cfg.effect;
	uint64_t slba;
	uint32_t nlb;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ndev->ns_grp, effect->nsid, &nsze);
	if (ret < 0)
		return ret;

	slba = rand() % (nsze / 2);
	if (effect->cmd.fused.use_nlb && effect->cmd.fused.nlb)
		nlb = effect->cmd.fused.nlb;
	else
		nlb = rand() % min_t(uint64_t, nsze / 2, 256) + 1;

	return ut_send_io_fused_cw_cmd(priv, sq, slba, nlb);
}

int ut_send_io_zone_append_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t zslba, uint32_t nlb)
{
	return ut_deal_io_zone_append_cmd(priv, sq, zslba, nlb, 
		UT_CMD_F_OPTION_SEND);
}

int ut_send_io_random_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint32_t select)
{
	uint32_t num;
	int ret;

	if (!select) {
		pr_warn("nothing selected!\n");
		return 0;
	}

	do {
		num = rand() % UT_CMD_SEL_NUM;
	} while (!(select & BIT(num)));

	switch (BIT(num)) {
	case UT_CMD_SEL_IO_READ:
		ret = ut_send_io_read_cmd_random_region(priv, sq);
		break;
	case UT_CMD_SEL_IO_WRITE:
		ret = ut_send_io_write_cmd_random_d_r(priv, sq);
		break;
	case UT_CMD_SEL_IO_COMPARE:
		ret = ut_send_io_compare_cmd_random_region(priv, sq);
		break;
	case UT_CMD_SEL_IO_VERIFY:
		ret = ut_send_io_verify_cmd_random_region(priv, sq);
		break;
	case UT_CMD_SEL_IO_COPY:
		ret = ut_send_io_copy_cmd_random_region(priv, sq);
		break;
	case UT_CMD_SEL_IO_FUSED_CW:
		ret = ut_send_io_fused_cw_cmd_random_region(priv, sq);
		break;
	default:
		pr_err("select bit %u is invalid!\n", num);
		ret = -EINVAL;
	}
	return ret;
}

int ut_send_io_random_cmds(struct case_data *priv, struct nvme_sq_info *sq,
	uint32_t select, int nr_cmd)
{
	struct case_report *rpt = &priv->rpt;
	struct case_config_effect *effect = priv->cfg.effect;
	int ret;

	ut_rpt_record_case_step(rpt, 
		"Send %d I/O random cmds => NSID: 0x%x, SQ: %u",
		nr_cmd, effect->nsid, sq->sqid);

	for (; nr_cmd > 0; nr_cmd--) {
		ret = ut_send_io_random_cmd(priv, sq, select);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int ut_send_io_random_cmds_to_sqs(struct case_data *priv, 
	struct nvme_sq_info *sq, uint32_t select, int nr_cmd, int nr_sq)
{
	int ret;
	int i;

	for (i = 0; i < nr_sq; i++) {
		ret = ut_send_io_random_cmds(priv, &sq[i], select, nr_cmd);
		if (ret < 0)
			return ret;
	}
	return 0;
}

int ut_modify_cmd_prp(struct case_data *priv, struct nvme_sq_info *sq, 
	uint16_t cid, uint64_t prp1, uint64_t prp2)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_cmd_tamper tamper = {0};
	struct nvme_common_command *cmd = NULL;
	int ret;

	tamper.sqid = sq->sqid;
	tamper.cid = cid;
	tamper.option = NVME_TAMPER_OPT_INQUIRY;
	ret = nvme_tamper_cmd(ndev->fd, &tamper);
	if (ret < 0)
		return ret;

	cmd = &tamper.cmd;
	cmd->dptr.prp1 = cpu_to_le64(prp1);
	cmd->dptr.prp2 = cpu_to_le64(prp2);
	tamper.option = NVME_TAMPER_OPT_MODIFY;

	ret = nvme_tamper_cmd(ndev->fd, &tamper);
	if (ret < 0)
		return ret;

	return 0;
}

