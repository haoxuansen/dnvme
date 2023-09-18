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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"
#include "test.h"
#include "test_metrics.h"

struct source_range {
	uint64_t	slba;
	uint32_t	nlb;
};

/*
 * @note
 *	I: Required to initialize
 * 	R: Return value
 */
struct test_cmd_copy {
	uint64_t	slba; 		/* I: target */
	uint16_t	status; 	/* R: save status code */

	uint8_t		desc_fmt;	/* I: descriptor format */
	void		*desc;
	uint32_t	size;

	void		*rbuf;
	uint32_t	rbuf_size;

	uint32_t	skip_write:1;

	/*
	 * The following fields shall be placed at the end of this structure
	 */
	uint32_t	ranges;		/* I */
	struct source_range	entry[0];	/* I */
};

struct test_data {
	uint32_t	nsid;
	uint64_t	nsze;
	uint32_t	lbads;

	uint32_t	mcl;
	uint32_t	msrc;
	uint16_t	mssrl;

	uint8_t		flags;
	uint16_t	control;
	uint16_t	cid;

	/* capability */
	uint32_t	is_sup_sgl:1;
	uint32_t	is_sup_sgl_bit_bucket:1;
	/* configuration */
	uint32_t	is_use_sgl_bit_bucket:1;
	uint32_t	is_use_custom_cid:1;

	uint32_t	nr_bit_bucket;
	struct nvme_sgl_bit_bucket	*bit_bucket;
};

static struct test_data g_test = {0};

static void test_data_reset_cmd_fields(struct test_data *data)
{
	data->flags = 0;
	data->control = 0;
	data->is_use_sgl_bit_bucket = 0;
	data->nr_bit_bucket = 0;
	data->bit_bucket = NULL;
	data->is_use_custom_cid = 0;
	data->cid = 0;
}

/**
 * @note May re-initialized? ignore...We shall to update this if data changed. 
 */
static int init_test_data(struct nvme_dev_info *ndev, struct test_data *data)
{
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	int ret;

	if (nvme_ctrl_support_sgl(ctrl) > 0 && 
		nvme_ctrl_support_sgl_data_block(ctrl) > 0)
		data->is_sup_sgl = 1;
	if (nvme_ctrl_support_sgl_bit_bucket(ctrl) > 0)
		data->is_sup_sgl_bit_bucket = 1;

	/* use first active namespace as default */
	data->nsid = le32_to_cpu(ns_grp->act_list[0]);

	ret = nvme_id_ns_lbads(ns_grp, data->nsid, &data->lbads);
	if (ret < 0) {
		pr_err("failed to get lbads!(%d)\n", ret);
		return ret;
	}
	/* we have checked once, skip the check below */
	nvme_id_ns_nsze(ns_grp, data->nsid, &data->nsze);

	data->msrc = (uint16_t)nvme_id_ns_msrc(ns_grp, data->nsid);
	data->mssrl = (uint16_t)nvme_id_ns_mssrl(ns_grp, data->nsid);
	nvme_id_ns_mcl(ns_grp, data->nsid, &data->mcl);

	return 0;
}

static int create_ioq(struct nvme_dev_info *ndev, struct nvme_sq_info *sq, 
	struct nvme_cq_info *cq)
{
	struct nvme_ccq_wrapper ccq_wrap = {0};
	struct nvme_csq_wrapper csq_wrap = {0};
	int ret;

	ccq_wrap.cqid = cq->cqid;
	ccq_wrap.elements = cq->nr_entry;
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
	csq_wrap.elements = sq->nr_entry;
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

static int submit_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
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
	wrap.buf = tool->rbuf;
	wrap.size = wrap.nlb * test->lbads;
	wrap.control = test->control;

	if (test->is_use_custom_cid) {
		wrap.use_user_cid = 1;
		wrap.cid = test->cid;
	}
	BUG_ON(wrap.size > tool->rbuf_size);
	memset(wrap.buf, 0, wrap.size);

	return nvme_cmd_io_read(ndev->fd, &wrap);
}

static int submit_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
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

	if (test->is_use_custom_cid) {
		wrap.use_user_cid = 1;
		wrap.cid = test->cid;
	}
	BUG_ON(wrap.size > tool->wbuf_size);
	fill_data_with_random(wrap.buf, wrap.size);

	return nvme_cmd_io_write(ndev->fd, &wrap);
}

static int prepare_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq)
{
	struct test_data *test = &g_test;
	/* ensure "slba + nlb <= nsze */
	uint64_t slba = rand() % (test->nsze / 2);
	uint32_t nlb = rand() % min_t(uint64_t, test->nsze / 2, 256) + 1;

	return submit_io_read_cmd(tool, sq, slba, nlb);
}

static int prepare_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq)
{
	struct test_data *test = &g_test;
	/* ensure "slba + nlb <= nsze */
	uint64_t slba = rand() % (test->nsze / 2);
	uint32_t nlb = rand() % min_t(uint64_t, test->nsze / 2, 256) + 1;

	return submit_io_write_cmd(tool, sq, slba, nlb);
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

	if (test->is_use_sgl_bit_bucket) {
		wrap.use_bit_bucket = 1;
		wrap.nr_bit_bucket = test->nr_bit_bucket;
		wrap.bit_bucket = test->bit_bucket;
	}
	BUG_ON(wrap.size > size);

	return nvme_io_read(ndev, &wrap);
}

static int send_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
{
	return __send_io_read_cmd(tool, sq, slba, nlb, 
		tool->rbuf, tool->rbuf_size);
}

static int send_io_read_cmd_for_copy(struct nvme_tool *tool, 
	struct nvme_sq_info *sq, struct test_cmd_copy *copy)
{
	struct test_data *test = &g_test;
	uint32_t i;
	uint32_t offset = 0;
	int ret;

	for (i = 0; i < copy->ranges; i++) {
		BUG_ON(copy->rbuf_size < offset);

		ret = __send_io_read_cmd(tool, sq, copy->entry[i].slba, 
			copy->entry[i].nlb, copy->rbuf + offset, 
			copy->rbuf_size - offset);
		if (ret < 0) {
			pr_err("failed to read NLB(%u) from SLBA(0x%lx)!(%d)\n",
				copy->entry[i].nlb, copy->entry[i].slba, ret);
			return ret;
		}
		offset += copy->entry[i].nlb * test->lbads;
	}
	return 0;
}

static int send_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
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

	if (test->is_use_sgl_bit_bucket) {
		wrap.use_bit_bucket = 1;
		wrap.nr_bit_bucket = test->nr_bit_bucket;
		wrap.bit_bucket = test->bit_bucket;
	}
	BUG_ON(wrap.size > tool->wbuf_size);
	fill_data_with_random(wrap.buf, wrap.size);

	return nvme_io_write(ndev, &wrap);
}

static int send_io_compare_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
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
	wrap.buf = tool->rbuf;
	wrap.size = wrap.nlb * test->lbads;

	BUG_ON(wrap.size > tool->rbuf_size);

	return nvme_io_compare(ndev, &wrap);
}

static int copy_desc_fmt0_init(struct nvme_copy_desc_fmt0 *desc, 
	struct source_range *range, uint32_t num)
{
	uint32_t i;

	memset(desc, 0, sizeof(*desc) * num);
	for (i = 0; i < num; i++) {
		desc[i].slba = cpu_to_le64(range[i].slba);
		desc[i].length = cpu_to_le16(range[i].nlb - 1);
	}
	return 0;
}

static int copy_desc_fmt1_init(struct nvme_copy_desc_fmt1 *desc,
	struct source_range *range, uint32_t num)
{
	uint32_t i;

	memset(desc, 0, sizeof(*desc) * num);
	for (i = 0; i < num; i++) {
		desc[i].slba = cpu_to_le64(range[i].slba);
		desc[i].length = cpu_to_le16(range[i].nlb - 1);
	}
	return 0;
}

static int copy_desc_init(struct test_cmd_copy *copy)
{
	void *desc;
	int ret;

	switch (copy->desc_fmt) {
	case NVME_COPY_DESC_FMT_32B:
	default:
		ret = posix_memalign(&desc, NVME_TOOL_RW_BUF_ALIGN, 
			sizeof(struct nvme_copy_desc_fmt0) * copy->ranges);
		if (ret) {
			pr_err("failed to alloc copy descriptor!\n");
			return -ENOMEM;
		}
		copy_desc_fmt0_init(desc, copy->entry, copy->ranges);
		copy->size = sizeof(struct nvme_copy_desc_fmt0) * copy->ranges;
		break;
	
	case NVME_COPY_DESC_FMT_40B:
		ret = posix_memalign(&desc, NVME_TOOL_RW_BUF_ALIGN,
			sizeof(struct nvme_copy_desc_fmt1) * copy->ranges);
		if (ret) {
			pr_err("failed to alloc copy descriptor!\n");
			return -ENOMEM;
		}
		copy_desc_fmt1_init(desc, copy->entry, copy->ranges);
		copy->size = sizeof(struct nvme_copy_desc_fmt1) * copy->ranges;
		break;
	}

	copy->desc = desc;
	return 0;
}

static void copy_desc_exit(struct test_cmd_copy *copy)
{
	if (!copy || !copy->desc)
		return;

	free(copy->desc);
	copy->desc = NULL;
	copy->size = 0;
}

static int send_io_copy_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	struct test_cmd_copy *copy)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_copy_wrapper wrap = {0};
	struct test_data *test = &g_test;
	uint32_t i;
	uint16_t cid;
	int ret;

	if (unlikely(copy->skip_write))
		goto skip_write;

	/*
	 * Avoid the read portion of a copy operation attemps to access
	 * a deallocated or unwritten logical block.
	 */
	for (i = 0; i < copy->ranges; i++) {
		ret = send_io_write_cmd(tool, sq, copy->entry[i].slba, 
			copy->entry[i].nlb);
		if (ret < 0) {
			pr_err("failed to write NLB(%u) to SLBA(0x%lx)!(%d)\n",
				copy->entry[i].nlb, copy->entry[i].slba, ret);
			return ret;
		}
		pr_debug("source range[%u] slba:%lu, nlb:%u\n", i,
			copy->entry[i].slba, copy->entry[i].nlb);
	}
	pr_debug("target slba:%lu\n", copy->slba);
skip_write:
	ret = copy_desc_init(copy);
	if (ret < 0)
		return ret;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = test->nsid;
	wrap.slba = copy->slba;
	wrap.ranges = copy->ranges - 1;
	wrap.desc_fmt = copy->desc_fmt;
	wrap.desc = copy->desc;
	wrap.size = copy->size;

	ret = nvme_cmd_io_copy(ndev->fd, &wrap);
	if (ret < 0) {
		pr_err("failed to submit copy command!(%d)\n", ret);
		goto out;
	}
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
	if (ret < 0)
		goto out;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, sq->cqid, 1, tool->entry, 
		tool->entry_size);
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		ret = ret < 0 ? ret : -ETIME;
		goto out;
	}

	if (tool->entry[0].command_id != cid) {
		pr_err("CID:%u vs %u\n", cid, tool->entry[0].command_id);
		ret = -EINVAL;
		goto out;
	}
	copy->status = NVME_CQE_STATUS_TO_STATE(tool->entry[0].status);
out:
	copy_desc_exit(copy);
	return ret;
}

/**
 * @brief Read command shall process success
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_read_success(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct test_data *test = &g_test;
	/* ensure "slba + nlb <= nsze" */
	uint64_t slba = rand() % (test->nsze / 2);
	uint32_t nlb = rand() % min_t(uint64_t, 256, (test->nsze / 2)) + 1;
	int ret;

	test_data_reset_cmd_fields(test);
	ret = send_io_read_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to read data! SLBA:0x%llx, NLB:%u(%d)\n", 
			slba, nlb, ret);
		goto out;
	}
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Read command with FUA flag shall process success
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_read_fua_success(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct test_data *test = &g_test;
	/* ensure "slba + nlb <= nsze" */
	uint64_t slba = rand() % (test->nsze / 2);
	uint32_t nlb = rand() % min_t(uint64_t, 256, (test->nsze / 2)) + 1;
	int ret;

	test_data_reset_cmd_fields(test);
	test->control = NVME_RW_FUA;
	ret = send_io_read_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to read data! SLBA:0x%llx, NLB:%u(%d)\n", 
			slba, nlb, ret);
		goto out;
	}
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

static int subcase_read_invlid_cid(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	struct nvme_completion *entry = NULL;
	uint32_t nr_cmd = rand() % min_t(uint32_t, sq->nr_entry / 2, 256) + 1;
	uint32_t inject = rand() % (nr_cmd - 1) + 1;
	uint16_t cid;
	uint32_t i;
	uint32_t match = 0;
	int ret;

	test_data_reset_cmd_fields(test);

	ret = prepare_io_read_cmd(tool, sq);
	if (ret < 0)
		return ret;
	cid = ret;

	for (i = 1; i < nr_cmd; i++) {
		if (i == inject) {
			test->is_use_custom_cid = 1;
			test->cid = cid;
		}
		ret = prepare_io_read_cmd(tool, sq);
		if (ret < 0)
			return ret;

		if (i == inject)
			test->is_use_custom_cid = 0;
	}
	pr_debug("nr_cmd: %u, inject idx: %u, cid: %u\n", nr_cmd, inject, cid);

	ret = nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
	if (ret < 0)
		return ret;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, sq->cqid, nr_cmd, &tool->entry,
		tool->entry_size);
	if (ret != nr_cmd) {
		pr_err("expect reap %u, actual reaped %d!\n", nr_cmd, ret);
		return ret < 0 ? ret : -ETIME;
	}

	for (i = 0; i < nr_cmd; i++) {
		entry = &tool->entry[i];

		if (entry->command_id != cid)
			continue;
		
		if (match == 0) {
			if (NVME_CQE_STATUS_TO_STATE(entry->status) == 
				NVME_SC_SUCCESS) {
				match += 1;
			} else {
				pr_err("first cmd expect success, but failed!\n");
				return -EIO;
			}
		} else if (match == 1) {
			if (NVME_CQE_STATUS_TO_STATE(entry->status) ==
				NVME_SC_CMDID_CONFLICT) {
				match += 1;
				break;
			} else {
				pr_err("second cmd expect failed, but status "
					"is %u!\n",
					NVME_CQE_STATUS_TO_STATE(entry->status));
				return -EIO;
			}
		}
	}

	if (match != 2) {
		pr_err("failed to match(%u) cid!\n", match);
		return -ENOENT;
	}
	return 0;
}

/**
 * @brief Read command uses SGL bit bucket descriptor
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_read_use_sgl_bit_bucket(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct test_data *test = &g_test;
	struct nvme_sgl_bit_bucket *bit_bucket = NULL;
	/* ensure "slba + nlb <= nsze" */
	uint64_t slba = rand() % (test->nsze / 4);
	uint32_t nlb = rand() % min_t(uint64_t, 256, (test->nsze / 4)) + 1;
	uint32_t nr_bit_bucket = rand() % 8 + 1;
	uint32_t skip = 0;
	uint32_t i;
	int ret;

	if (!test->is_sup_sgl || !test->is_sup_sgl_bit_bucket)
		return -EOPNOTSUPP;

	bit_bucket = calloc(nr_bit_bucket, sizeof(struct nvme_sgl_bit_bucket));
	if (!bit_bucket) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}

	do {
		if (skip) {
			pr_warn("slba + nlb + skip > nsze! try again...\n");
			skip = 0;
		}

		for (i = 0; i < nr_bit_bucket; i++) {
			bit_bucket[i].offset = (rand() % nlb) * test->lbads;
			/* Length = 0h is a valid SGL Bit Bucket descriptor */
			bit_bucket[i].length = (rand() % 16) * test->lbads;
			skip += bit_bucket[i].length / test->lbads;
		}
	} while ((slba + nlb + skip) > test->nsze);

	test_data_reset_cmd_fields(test);
	test->flags = NVME_CMD_SGL_METABUF;
	test->is_use_sgl_bit_bucket = 1;
	test->nr_bit_bucket = nr_bit_bucket;
	test->bit_bucket = bit_bucket;

	pr_debug("SLBA:0x%llx, NLB:%u, bit_bucket:%u\n",
		slba, nlb, nr_bit_bucket);
	for (i = 0; i < nr_bit_bucket; i++) {
		pr_debug("idx:%u, offset:0x%x, length:0x%x\n",
			i, bit_bucket[i].offset, bit_bucket[i].length);
	}

	ret = send_io_read_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to read data!(%d)\n", ret);
		goto free_bit_bucket;
	}

free_bit_bucket:
	free(bit_bucket);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Write command shall process success
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_write_success(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct test_data *test = &g_test;
	/* ensure "slba + nlb <= nsze" */
	uint64_t slba = rand() % (test->nsze / 2);
	uint32_t nlb = rand() % min_t(uint64_t, 256, (test->nsze / 2)) + 1;
	int ret;

	test_data_reset_cmd_fields(test);
	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to write data! SLBA:0x%llx, NLB:%u(%d)\n", 
			slba, nlb, ret);
		goto out;
	}
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Write command with FUA flag shall process success
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_write_fua_success(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct test_data *test = &g_test;
	/* ensure "slba + nlb <= nsze" */
	uint64_t slba = rand() % (test->nsze / 2);
	uint32_t nlb = rand() % min_t(uint64_t, 256, (test->nsze / 2)) + 1;
	int ret;

	test_data_reset_cmd_fields(test);
	test->control = NVME_RW_FUA;
	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to write data! SLBA:0x%llx, NLB:%u(%d)\n", 
			slba, nlb, ret);
		goto out;
	}
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

static int subcase_write_invlid_cid(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	struct nvme_completion *entry = NULL;
	uint32_t nr_cmd = rand() % min_t(uint32_t, sq->nr_entry / 2, 256) + 1;
	uint32_t inject = rand() % (nr_cmd - 1) + 1;
	uint16_t cid;
	uint32_t i;
	uint32_t match = 0;
	int ret;

	test_data_reset_cmd_fields(test);

	ret = prepare_io_write_cmd(tool, sq);
	if (ret < 0)
		return ret;
	cid = ret;

	for (i = 1; i < nr_cmd; i++) {
		if (i == inject) {
			test->is_use_custom_cid = 1;
			test->cid = cid;
		}
		ret = prepare_io_write_cmd(tool, sq);
		if (ret < 0)
			return ret;

		if (i == inject)
			test->is_use_custom_cid = 0;
	}
	pr_debug("nr_cmd: %u, inject idx: %u, cid: %u\n", nr_cmd, inject, cid);

	ret = nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
	if (ret < 0)
		return ret;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, sq->cqid, nr_cmd, &tool->entry,
		tool->entry_size);
	if (ret != nr_cmd) {
		pr_err("expect reap %u, actual reaped %d!\n", nr_cmd, ret);
		return ret < 0 ? ret : -ETIME;
	}

	for (i = 0; i < nr_cmd; i++) {
		entry = &tool->entry[i];

		if (entry->command_id != cid)
			continue;
		
		if (match == 0) {
			if (NVME_CQE_STATUS_TO_STATE(entry->status) == 
				NVME_SC_SUCCESS) {
				match += 1;
			} else {
				pr_err("first cmd expect success, but failed!\n");
				return -EIO;
			}
		} else if (match == 1) {
			if (NVME_CQE_STATUS_TO_STATE(entry->status) ==
				NVME_SC_CMDID_CONFLICT) {
				match += 1;
				break;
			} else {
				pr_err("second cmd expect failed, but status "
					"is %u!\n",
					NVME_CQE_STATUS_TO_STATE(entry->status));
				return -EIO;
			}
		}
	}

	if (match != 2) {
		pr_err("failed to match(%u) cid!\n", match);
		return -ENOENT;
	}
	return 0;
}

/**
 * @brief Write command uses SGL bit bucket descriptor
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the SGL Bit Bucket descriptor describes a source data buffer,
 * 	then the Bit Bucket descriptor shall be treated as if the Length
 * 	field were cleared to 0h (the Bit Bucket descriptor has no effect)
 */
static int subcase_write_use_sgl_bit_bucket(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct test_data *test = &g_test;
	struct nvme_sgl_bit_bucket *bit_bucket = NULL;
	/* ensure "slba + nlb <= nsze" */
	uint64_t slba = rand() % (test->nsze / 4);
	uint32_t nlb = rand() % min_t(uint64_t, 256, (test->nsze / 4)) + 1;
	uint32_t nr_bit_bucket = rand() % 8 + 1;
	uint32_t skip = 0;
	uint32_t i;
	int ret;

	if (!test->is_sup_sgl || !test->is_sup_sgl_bit_bucket)
		return -EOPNOTSUPP;

	bit_bucket = calloc(nr_bit_bucket, sizeof(struct nvme_sgl_bit_bucket));
	if (!bit_bucket) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}

	do {
		if (skip) {
			pr_warn("slba + nlb + skip > nsze! try again...\n");
			skip = 0;
		}

		for (i = 0; i < nr_bit_bucket; i++) {
			bit_bucket[i].offset = (rand() % nlb) * test->lbads;
			/* Length = 0h is a valid SGL Bit Bucket descriptor */
			bit_bucket[i].length = (rand() % 16) * test->lbads;
			skip += bit_bucket[i].length / test->lbads;
		}
	} while ((slba + nlb + skip) > test->nsze);

	test_data_reset_cmd_fields(test);
	test->flags = NVME_CMD_SGL_METABUF;
	test->is_use_sgl_bit_bucket = 1;
	test->nr_bit_bucket = nr_bit_bucket;
	test->bit_bucket = bit_bucket;

	pr_debug("SLBA:0x%llx, NLB:%u, bit_bucket:%u\n",
		slba, nlb, nr_bit_bucket);
	for (i = 0; i < nr_bit_bucket; i++) {
		pr_debug("idx:%u, offset:0x%x, length:0x%x\n",
			i, bit_bucket[i].offset, bit_bucket[i].length);
	}

	ret = send_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto free_bit_bucket;
	}
	test_data_reset_cmd_fields(test);

	/* readback to check the bit bucket descriptor has no effect */
	ret = send_io_read_cmd(tool, sq, slba, nlb);
	if (ret < 0) {
		pr_err("failed to read data! SLBA:0x%llx, NLB:%u(%d)\n", 
			slba, nlb, ret);
		goto free_bit_bucket;
	}

	if (memcmp(tool->rbuf, tool->wbuf, nlb * test->lbads)) {
		pr_err("rbuf vs wbuf is different!\n");
		dump_data_to_file(tool->rbuf, nlb * test->lbads, "./rbuf.bin");
		dump_data_to_file(tool->wbuf, nlb * test->lbads, "./wbuf.bin");
		ret = -EIO;
		goto free_bit_bucket;
	}

free_bit_bucket:
	free(bit_bucket);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Copy command shall process success
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_copy_success(struct nvme_tool *tool, 
	struct nvme_sq_info *sq)
{
	struct test_cmd_copy *copy = NULL;
	struct test_data *test = &g_test;
	uint32_t sum = 0;
	uint64_t start = 0;
	uint32_t entry;
	uint32_t i;
	uint16_t limit = min((test->nsze / 4), (uint64_t)256);
	int ret;

	entry = rand() % test->msrc + 1; /* 1~msrc */

	copy = zalloc(sizeof(struct test_cmd_copy) +
			entry * sizeof(struct source_range));
	if (!copy) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = posix_memalign(&copy->rbuf, NVME_TOOL_RW_BUF_ALIGN, 
		NVME_TOOL_RW_BUF_SIZE);
	if (ret) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto free_copy;
	}
	copy->rbuf_size = NVME_TOOL_RW_BUF_SIZE;

	copy->ranges = entry;
	copy->desc_fmt = NVME_COPY_DESC_FMT_32B;

	do {
		if (copy->slba)
			pr_warn("target slba + nlb > nsze! try again ?\n");

		do {
			if (sum)
				pr_warn("NLB total is larger than MCL! try again?\n");

			for (i = 0, sum = 0; i < copy->ranges; i++) {
				copy->entry[i].slba = rand() % (test->nsze / 4);
				copy->entry[i].nlb = rand() % min(test->mssrl, limit) + 1;
				sum += copy->entry[i].nlb;

				if (start < (copy->entry[i].slba + copy->entry[i].nlb))
					start = copy->entry[i].slba + copy->entry[i].nlb;
			}
		} while (test->mcl < sum);

		copy->slba = rand() % (test->nsze / 4) + start;

	} while ((copy->slba + sum) > test->nsze);

	test_data_reset_cmd_fields(test);

	ret = send_io_copy_cmd(tool, sq, copy);
	if (ret < 0)
		goto free_rbuf;
	
	if (copy->status != NVME_SC_SUCCESS) {
		pr_err("status: 0x%x vs 0x%x\n", NVME_SC_SUCCESS, copy->status);
		ret = -EINVAL;
		goto free_rbuf;
	}

	ret = send_io_read_cmd_for_copy(tool, sq, copy);
	if (ret < 0)
		goto free_rbuf;

	ret = send_io_read_cmd(tool, sq, copy->slba, sum);
	if (ret < 0)
		goto free_rbuf;

	if (memcmp(copy->rbuf, tool->rbuf, sum * test->lbads)) {
		pr_err("source data differs from target data! try dump...\n");
		dump_data_to_file(copy->rbuf, sum * test->lbads, "./source.bin");
		dump_data_to_file(tool->rbuf, sum * test->lbads, "./target.bin");
		ret = -EIO;
		goto free_rbuf;
	}

free_rbuf:
	free(copy->rbuf);
free_copy:
	free(copy);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Copy command shall process failed due to dest LBA range contains
 * 	read-only blocks.
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int subcase_copy_to_read_only(struct nvme_tool *tool, 
	struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_cmd_copy *copy = NULL;
	struct test_data *test = &g_test;
	uint16_t limit = min_t(uint64_t, (test->nsze / 4), 256);
	int recovery = 0;
	int ret, tmp;

	ret = nvme_ctrl_support_write_protect(ndev->ctrl);
	if (ret == 0) {
		pr_warn("device not support write protect! skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	} else if (ret < 0) {
		pr_err("failed to check capability!(%d)\n", ret);
		goto out;
	}

	ret = nvme_get_feat_write_protect(ndev, test->nsid, NVME_FEAT_SEL_CUR);
	if (ret < 0)
		goto out;
	
	if (ret == NVME_NS_NO_WRITE_PROTECT) {
		ret = nvme_set_feat_write_protect(ndev, test->nsid, 
			NVME_FEAT_SEL_CUR, NVME_NS_WRITE_PROTECT);
		if (ret < 0) {
			pr_err("failed to set nsid(%u) to write protect "
				"state!(%d)\n", test->nsid, ret);
			goto out;
		}
		recovery = 1;
	}

	copy = zalloc(sizeof(struct test_cmd_copy) + 
		sizeof(struct source_range));
	if (!copy) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out_recovery;
	}

	copy->ranges = 1;
	copy->desc_fmt = NVME_COPY_DESC_FMT_32B;
	copy->entry[0].slba = rand() % (test->nsze / 4);
	copy->entry[0].nlb = rand() % min(test->mssrl, limit) + 1;
	copy->slba = copy->entry[0].slba + copy->entry[0].nlb +
		rand() % (test->nsze / 4);
	copy->skip_write = 1;

	test_data_reset_cmd_fields(test);

	ret = send_io_copy_cmd(tool, sq, copy);
	if (ret < 0)
		goto free_copy;
	
	if (copy->status != NVME_SC_READ_ONLY) {
		pr_err("status: 0x%x vs 0x%x\n", NVME_SC_READ_ONLY, copy->status);
		ret = -EINVAL;
		goto free_copy;
	}

free_copy:
	free(copy);
out_recovery:
	if (recovery) {
		/*
		 * If use @ret to receive return value, may destroy the origin 
		 * value of @ret.(eg: @ret<0, new=0 ==> @ret=0, lost errno)
		 */
		tmp = nvme_set_feat_write_protect(ndev, test->nsid, 
			NVME_FEAT_SEL_CUR, NVME_NS_NO_WRITE_PROTECT);
		if (tmp < 0) {
			pr_err("failed to set nsid(%u) to no write protect "
				"state!(%d)\n", test->nsid, ret);
			ret |= tmp;
		}
	}
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Copy command shall process failed due to invalid descriptor format
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the Copy Descriptor Format specified in the Descriptor Format
 *  field is not supported by the controller, then the command shall
 *  be aborted with a status code of Invalid Field in Command.
 */
static int subcase_copy_invalid_desc_format(struct nvme_tool *tool, 
	struct nvme_sq_info *sq)
{
	struct test_cmd_copy *copy = NULL;
	struct test_data *test = &g_test;
	int ret;

	copy = zalloc(sizeof(struct test_cmd_copy) + 
				sizeof(struct source_range));
	if (!copy) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}
	copy->ranges = 1;
	copy->slba = rand() % (test->nsze / 4) + (test->nsze / 4);
	/* set invlid descriptor format */
	copy->desc_fmt = 0xff;

	copy->entry[0].slba = rand() % (test->nsze / 4);
	copy->entry[0].nlb = 1;

	test_data_reset_cmd_fields(test);

	ret = send_io_copy_cmd(tool, sq, copy);
	if (ret < 0)
		goto out2;

	if (copy->status != NVME_SC_INVALID_FIELD) {
		pr_err("status: 0x%x vs 0x%x\n", NVME_SC_INVALID_FIELD, 
			copy->status);
		ret = -EINVAL;
		goto out2;
	}

out2:
	free(copy);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Copy command shall process failed due to mismatch descriptor format
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note The command shall be aborted with the status code of Invalid 
 *  Namespace or Format if one of the following condtion is met.
 * 	1) 16b Guard PI(Protection Info) && Descriptor Format != 0h
 * 	2) 32b or 64b Guard PI && Descriptor Format != 1h
 */
static int __unused subcase_copy_mismatch_desc_format(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	return -EOPNOTSUPP;
}

/**
 * @brief Copy command shall process failed due to invalid range num
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the number of Source Range entries is greater than the 
 *  value in the MSRC field, then the Copy command shall be aborted
 *  with a status code of Command Size Limit Exceeded.
 */
static int subcase_copy_invalid_range_num(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct test_cmd_copy *copy = NULL;
	struct test_data *test = &g_test;
	uint32_t sum = 0;
	uint32_t entry;
	uint32_t i;
	uint16_t limit = 256;
	int ret;

	/*
	 * NR field in copy command is 8-bit width, the max value is 0xff.
	 * If MSRC is 0xff, we can't set NR larger!
	 *
	 * MSRC max value convert to 1's based is (0xff + 1)
	 */
	if (test->msrc > 0xff) {
		pr_warn("Max source range count is 0xff! skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	/* set invalid range num */
	entry = test->msrc + 1;

	copy = zalloc(sizeof(struct test_cmd_copy) +
			entry * sizeof(struct source_range));
	if (!copy) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}

	copy->ranges = entry;
	copy->slba = rand() % (test->nsze / 4) + (test->nsze / 4);
	copy->desc_fmt = NVME_COPY_DESC_FMT_32B;

	do {
		if (sum)
			pr_warn("NLB total is larger than MCL! try again?");

		for (i = 0, sum = 0; i < copy->ranges; i++) {
			copy->entry[i].slba = rand() % (test->nsze / 4);
			copy->entry[i].nlb = rand() % min(test->mssrl, limit) + 1;
			sum += copy->entry[i].nlb;
		}
	} while (test->mcl < sum);

	test_data_reset_cmd_fields(test);

	ret = send_io_copy_cmd(tool, sq, copy);
	if (ret < 0)
		goto out2;

	if (copy->status != NVME_SC_SIZE_EXCEEDED) {
		pr_err("status: 0x%x vs 0x%x\n", NVME_SC_SIZE_EXCEEDED, 
			copy->status);
		ret = -EINVAL;
		goto out2;
	}

out2:
	free(copy);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Copy command shall process failed due to invalid NLB
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If a valid Source Range Entry specifies a Number of 
 *  Logical Blocks field that is greater than the value in the 
 *  MSSRL field, the the Copy command shall be aborted with a
 *  status code of Command Size Limit Exceeded.
 */
static int subcase_copy_invalid_nlb_single(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct test_cmd_copy *copy = NULL;
	struct test_data *test = &g_test;
	uint32_t sum = 0;
	uint32_t inject;
	uint32_t dw0;
	uint32_t entry;
	uint32_t i;
	uint16_t limit = 256;
	int ret;

	/*
	 * NLB field in copy descriptor is 16-bit width, the max value 
	 * is 0xffff. If MSSRL is 0xffff, we can't set NLB larger!
	 */
	if (test->mssrl == 0xffff) {
		pr_warn("Max single source range length is 0xffff! skip...\n");
		ret = -EOPNOTSUPP;
		goto out;
	}

	entry = rand() % test->msrc + 1; /* 1~msrc */

	copy = zalloc(sizeof(struct test_cmd_copy) + 
			entry * sizeof(struct source_range));
	if (!copy) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}

	copy->ranges = entry;
	copy->slba = rand() % (test->nsze / 4) + (test->nsze / 4);
	copy->desc_fmt = NVME_COPY_DESC_FMT_32B;

	do {
		if (sum)
			pr_warn("NLB total is larger than MCL! try again?");

		for (i = 0, sum = 0; i < copy->ranges; i++) {
			copy->entry[i].slba = rand() % (test->nsze / 4);
			copy->entry[i].nlb = rand() % min(test->mssrl, limit) + 1;
			sum += copy->entry[i].nlb;
		}
	} while (test->mcl < sum);

	inject = rand() % entry;
	/* set invalid NLB in random source range */
	copy->entry[inject].nlb = test->mssrl + 1;

	test_data_reset_cmd_fields(test);

	ret = send_io_copy_cmd(tool, sq, copy);
	if (ret < 0)
		goto out2;

	if (copy->status != NVME_SC_SIZE_EXCEEDED) {
		pr_err("status: 0x%x vs 0x%x\n", NVME_SC_SIZE_EXCEEDED, 
			copy->status);
		ret = -EINVAL;
		goto out2;
	}

	/*
	 * Dword 0 of the completion queue entry contains the number of 
	 * the lowest numbered Source Range entry that was not successfully
	 * copied.
	 */
	dw0 = le32_to_cpu(tool->entry[0].result.u32);
	if (dw0 != inject) {
		pr_err("expect dw0 is %u, actual dw0 is %u!\n", inject, dw0);
		ret = -EINVAL;
		goto out2;
	}

out2:
	free(copy);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Copy command shall process failed due to invalid NLB sum
 * 
 * @return 0 on success, otherwise a negative errno
 * 
 * @note If the sum of all Number of Logical Blocks fields in all
 *  Source Range entries is greater than the value in the MCL field,
 *  then the Copy command shall be aborted with a status of Command
 *  Size Limit Exceeded.
 */
static int subcase_copy_invalid_nlb_sum(struct nvme_tool *tool, 
	struct nvme_sq_info *sq)
{
	struct test_cmd_copy *copy = NULL;
	struct test_data *test = &g_test;
	uint32_t sum;
	uint32_t entry;
	uint32_t i;
	int ret;

	sum = (uint32_t)test->msrc * (uint32_t)test->mssrl;
	if (sum <= test->mcl) {
		pr_warn("(MSRC + 1) * MSSRL <= MCL : 0x%x vs 0x%x! skip...\n", 
			sum, test->mcl);
		ret = -EOPNOTSUPP;
		goto out;
	}
	entry = test->msrc;

	copy = zalloc(sizeof(struct test_cmd_copy) + 
			entry * sizeof(struct source_range));
	if (!copy) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}

	copy->ranges = entry;
	copy->slba = rand() % (test->nsze / 4) + (test->nsze / 4);
	copy->desc_fmt = NVME_COPY_DESC_FMT_32B;

	for (i = 0; i < copy->ranges; i++) {
		copy->entry[i].slba = rand() % (test->nsze / 4);
		/* ensure the sum of NLB greater than MCL */
		copy->entry[i].nlb = test->mssrl;
	}

	test_data_reset_cmd_fields(test);

	ret = send_io_copy_cmd(tool, sq, copy);
	if (ret < 0)
		goto out2;

	if (copy->status != NVME_SC_SIZE_EXCEEDED) {
		pr_err("status: 0x%x vs 0x%x\n", NVME_SC_SIZE_EXCEEDED, 
			copy->status);
		ret = -EINVAL;
		goto out2;
	}

out2:
	free(copy);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

static int case_cmd_io_read(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct test_data *test = &g_test;
	int ret;

	ret = init_test_data(ndev, test);
	if (ret < 0)
		return ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	ret |= subcase_read_success(tool, sq);
	ret |= subcase_read_fua_success(tool, sq);
	ret |= subcase_read_invlid_cid(tool, sq);
	ret |= subcase_read_use_sgl_bit_bucket(tool, sq);

	nvme_display_subcase_report();

	ret |= delete_ioq(ndev, sq, cq);
	return ret;
}
NVME_CASE_CMD_SYMBOL(case_cmd_io_read, 
	"Send I/O read command to IOSQ in various scenarios");

static int case_cmd_io_write(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct test_data *test = &g_test;
	int ret;

	ret = init_test_data(ndev, test);
	if (ret < 0)
		return ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	ret |= subcase_write_success(tool, sq);
	ret |= subcase_write_fua_success(tool, sq);
	ret |= subcase_write_invlid_cid(tool, sq);
	ret |= subcase_write_use_sgl_bit_bucket(tool, sq);

	nvme_display_subcase_report();

	ret |= delete_ioq(ndev, sq, cq);
	return ret;
}
NVME_CASE_CMD_SYMBOL(case_cmd_io_write, 
	"Send a I/O write command to IOSQ in various scenarios");

/**
 * @brief Send a I/O compare command to IOSQ
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int case_cmd_io_compare(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct test_data *test = &g_test;
	uint64_t slba = 0;
	uint32_t nlb = 8;
	int ret;

	ret = init_test_data(ndev, test);
	if (ret < 0)
		return ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev, sq, cq);
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
	ret = memcmp(tool->wbuf, tool->rbuf, test->lbads * nlb);
	if (ret != 0) {
		pr_err("failed to compare read/write buffer!\n");
		ret = -EIO;
		goto out;
	}
out:
	ret |= delete_ioq(ndev, sq, cq);
	return ret;
}
NVME_CASE_CMD_SYMBOL(case_cmd_io_compare, 
	"Send a I/O compare command to IOSQ");

static int case_cmd_io_copy(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct test_data *test = &g_test;
	int ret;

	ret = nvme_ctrl_support_copy_cmd(ndev->ctrl);
	if (ret == 0) {
		pr_warn("device not support copy cmd! skip...\n");
		return -EOPNOTSUPP;
	} else if (ret < 0) {
		pr_err("failed to check capability!(%d)\n", ret);
		return ret;
	}

	ret = init_test_data(ndev, test);
	if (ret < 0)
		return ret;

	if (!test->mssrl || !test->mcl || test->mssrl > test->mcl) {
		pr_err("MSSRL:0x%x or MCL:0x%x is invalid!\n", 
			le16_to_cpu(test->mssrl), le32_to_cpu(test->mcl));
		return -EINVAL;
	}

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	ret |= subcase_copy_success(tool, sq);
	ret |= subcase_copy_to_read_only(tool, sq);
	ret |= subcase_copy_invalid_desc_format(tool, sq);
	ret |= subcase_copy_invalid_range_num(tool, sq);
	ret |= subcase_copy_invalid_nlb_single(tool, sq);
	ret |= subcase_copy_invalid_nlb_sum(tool, sq);

	nvme_display_subcase_report();

	ret |= delete_ioq(ndev, sq, cq);
	return ret;
}
NVME_CASE_CMD_SYMBOL(case_cmd_io_copy, 
	"Send I/O copy command to IOSQ in various scenarios");
