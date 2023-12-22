/**
 * @file test_hmb.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief Test host memory buffer
 * @version 0.1
 * @date 2023-08-17
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

	uint32_t	ranges;		/* I */
	struct source_range	entry[0];	/* I */
};

struct test_data {
	uint32_t	nsid;
	uint32_t	lbads;
	uint64_t	nsze;

	uint32_t	mcl;
	uint32_t	msrc;
	uint16_t	mssrl;

	uint32_t	page_size;
	uint32_t	min_buf_size;
	uint32_t	max_desc_entry;
};

static struct test_data g_test = {0};

static int init_test_data(struct nvme_dev_info *ndev, struct test_data *data)
{
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint32_t cc;
	uint32_t hmminds;
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

	data->msrc = (uint16_t)nvme_id_ns_msrc(ns_grp, data->nsid);
	data->mssrl = (uint16_t)nvme_id_ns_mssrl(ns_grp, data->nsid);
	nvme_id_ns_mcl(ns_grp, data->nsid, &data->mcl);

	ret = nvme_read_ctrl_cc(ndev->fd, &cc);
	if (ret < 0) {
		pr_err("failed to get controller configuration!(%d)\n", ret);
		return ret;
	}
	data->page_size = 1 << (12 + NVME_CC_TO_MPS(cc));

	ret = nvme_id_ctrl_hmminds(ndev->ctrl, &hmminds);
	if (ret < 0) {
		pr_err("failed to get hmminds!(%d)\n", ret);
		return ret;
	}
	if (!hmminds) {
		pr_notice("Set the default value of min_buf_size to 0x%x\n",
			data->page_size);
		data->min_buf_size = data->page_size;
	} else {
		data->min_buf_size = hmminds * SZ_4K;
	}
	
	data->max_desc_entry = (uint32_t)nvme_id_ctrl_hmmaxd(ndev->ctrl);
	if (!data->max_desc_entry) {
		pr_notice("Set the default value of max_desc_entry to 8\n");
		data->max_desc_entry = 8;
	}

	pr_debug("page_size: 0x%x, min_buf_size: 0x%x, max_desc_entry: %u\n",
		data->page_size, data->min_buf_size, data->max_desc_entry);
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

static int __send_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, uint16_t control, void *rbuf, uint32_t size)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};
	struct test_data *test = &g_test;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = test->nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = rbuf;
	wrap.size = wrap.nlb * test->lbads;
	wrap.control = control;

	BUG_ON(wrap.size > size);

	return nvme_io_read(ndev, &wrap);
}

static int send_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, uint16_t control)
{
	return __send_io_read_cmd(tool, sq, slba, nlb, control, 
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
			copy->entry[i].nlb, 0, copy->rbuf + offset, 
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
	uint64_t slba, uint32_t nlb, uint16_t control)
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
	wrap.control = control;

	BUG_ON(wrap.size > tool->wbuf_size);
	fill_data_with_random(wrap.buf, wrap.size);

	return nvme_io_write(ndev, &wrap);
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

	/*
	 * Avoid the read portion of a copy operation attemps to access
	 * a deallocated or unwritten logical block.
	 */
	for (i = 0; i < copy->ranges; i++) {
		ret = send_io_write_cmd(tool, sq, copy->entry[i].slba, 
			copy->entry[i].nlb, 0);
		if (ret < 0) {
			pr_err("failed to write NLB(%u) to SLBA(0x%lx)!(%d)\n",
				copy->entry[i].nlb, copy->entry[i].slba, ret);
			return ret;
		}
		pr_debug("source range[%u] slba:%lu, nlb:%u\n", i,
			copy->entry[i].slba, copy->entry[i].nlb);
	}
	pr_debug("target slba:%lu\n", copy->slba);

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

static int do_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq)
{
	struct test_data *test = &g_test;
	/* ensure "slba + nlb <= nsze */
	uint64_t slba = rand() % (test->nsze / 2);
	uint32_t nlb = rand() % min_t(uint64_t, test->nsze / 2, 256) + 1;

	return send_io_read_cmd(tool, sq, slba, nlb, 0);
}

static int do_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq)
{
	struct test_data *test = &g_test;
	/* ensure "slba + nlb <= nsze */
	uint64_t slba = rand() % (test->nsze / 2);
	uint32_t nlb = rand() % min_t(uint64_t, test->nsze / 2, 256) + 1;

	return send_io_write_cmd(tool, sq, slba, nlb, 0);
}

static int do_io_copy_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_cmd_copy *copy = NULL;
	struct test_data *test = &g_test;
	uint32_t sum = 0;
	uint64_t start = 0;
	uint32_t entry = rand() % test->msrc + 1; /* 1~msrc */
	uint32_t i;
	uint16_t limit = min_t(uint64_t, test->nsze / 4, 256);
	int ret;

	ret = nvme_ctrl_support_copy_cmd(ndev->ctrl);
	if (ret == 0) {
		pr_debug("device not support copy cmd! skip...\n");
		return 0;
	} else if (ret < 0) {
		pr_err("failed to check capability!(%d)\n", ret);
		return ret;
	}

	copy = zalloc(sizeof(struct test_cmd_copy) +
			entry * sizeof(struct source_range));
	if (!copy) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
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

	ret = send_io_read_cmd(tool, sq, copy->slba, sum, 0);
	if (ret < 0)
		goto free_rbuf;

	if (memcmp(copy->rbuf, tool->rbuf, sum * test->lbads)) {
		pr_err("source data differs from target data! try dump...\n");
		dump_data_to_fmt_file(copy->rbuf, sum * test->lbads, 
			"%s/%s-source.bin", NVME_TOOL_LOG_FILE_PATH, __func__);
		dump_data_to_fmt_file(tool->rbuf, sum * test->lbads, 
			"%s/%s-target.bin", NVME_TOOL_LOG_FILE_PATH, __func__);
		ret = -EIO;
		goto free_rbuf;
	}

free_rbuf:
	free(copy->rbuf);
free_copy:
	free(copy);
	return ret;
}

static int do_io_cmd(struct nvme_tool *tool)
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

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	ret |= do_io_read_cmd(tool, sq);
	ret |= do_io_write_cmd(tool, sq);
	ret |= do_io_copy_cmd(tool, sq);

	ret |= delete_ioq(ndev, sq, cq);
	return ret;
}

/**
 * @brief Send commands during host memory buffer is enabled
 * 
 * @return 0 on success, otherwise a negative errno.
 */
static int subcase_hmb_work_normal(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	struct nvme_hmb_alloc *hmb_alloc;
	struct nvme_hmb_wrapper wrap = {0};
	uint32_t pg_size = DIV_ROUND_UP(test->min_buf_size, test->page_size);
	uint32_t entry = rand() % test->max_desc_entry + 1;
	uint32_t i;
	int ret, tmp;

	hmb_alloc = zalloc(sizeof(struct nvme_hmb_alloc) + 
		sizeof(uint32_t) * entry);
	if (!hmb_alloc) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}
	hmb_alloc->nr_desc = entry;
	hmb_alloc->page_size = test->page_size;

	for (i = 0; i < entry; i++)
		hmb_alloc->bsize[i] = pg_size;

	ret = nvme_alloc_host_mem_buffer(ndev->fd, hmb_alloc);
	if (ret < 0)
		goto free_hmb_alloc;

	/* enable host memory buffer */
	wrap.sel = NVME_FEAT_SEL_CUR;
	wrap.dw11 = NVME_HOST_MEM_ENABLE;
	wrap.hsize = entry * pg_size;
	wrap.hmdla = hmb_alloc->desc_list;
	wrap.hmdlec = entry;

	ret = nvme_set_feat_hmb(ndev, &wrap);
	if (ret < 0)
		goto free_hmb_buf;

	ret = do_io_cmd(tool);
	if (ret < 0)
		goto disable_hmb;

disable_hmb:
	memset(&wrap, 0, sizeof(wrap));
	wrap.sel = NVME_FEAT_SEL_CUR;

	tmp = nvme_set_feat_hmb(ndev, &wrap);
	if (tmp < 0) {
		ret |= tmp;
		/* skip release if device still using HMB? */
		goto free_hmb_alloc;
	}

free_hmb_buf:
	tmp = nvme_release_host_mem_buffer(ndev->fd);
	if (tmp < 0)
		ret |= tmp;
free_hmb_alloc:
	free(hmb_alloc);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

/**
 * @brief Enable the host memory buffer repeatly
 * 
 * @return 0 on success, otherwise a negative errno.
 * 
 * @note If the host memory buffer is enabled, then a Set Features command
 * 	to enable the host memory buffer shall abort with a status code
 * 	of Command Sequence Error.
 */
static int subcase_hmb_double_enable(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	struct nvme_hmb_alloc *hmb_alloc;
	struct nvme_hmb_wrapper wrap = {0};
	struct nvme_completion entry = {0};
	uint32_t pg_size = DIV_ROUND_UP(test->min_buf_size, test->page_size);
	uint32_t nr_desc = rand() % test->max_desc_entry + 1;
	uint32_t i;
	uint16_t cid;
	int ret, tmp;

	hmb_alloc = zalloc(sizeof(struct nvme_hmb_alloc) + 
		sizeof(uint32_t) * nr_desc);
	if (!hmb_alloc) {
		pr_err("failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}
	hmb_alloc->nr_desc = nr_desc;
	hmb_alloc->page_size = test->page_size;

	for (i = 0; i < nr_desc; i++)
		hmb_alloc->bsize[i] = pg_size;

	ret = nvme_alloc_host_mem_buffer(ndev->fd, hmb_alloc);
	if (ret < 0)
		goto free_hmb_alloc;

	/* enable host memory buffer */
	wrap.sel = NVME_FEAT_SEL_CUR;
	wrap.dw11 = NVME_HOST_MEM_ENABLE;
	wrap.hsize = nr_desc * pg_size;
	wrap.hmdla = hmb_alloc->desc_list;
	wrap.hmdlec = nr_desc;

	ret = nvme_set_feat_hmb(ndev, &wrap);
	if (ret < 0)
		goto free_hmb_buf;

	/* enable host memory buffer again */
	ret = nvme_cmd_set_feat_hmb(ndev->fd, &wrap);
	if (ret < 0)
		goto disable_hmb;
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	if (ret < 0)
		goto disable_hmb;

	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		ret = -ETIME;
		goto disable_hmb;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, NVME_SC_CMD_SEQ_ERROR);
	if (ret < 0)
		goto disable_hmb;

disable_hmb:
	memset(&wrap, 0, sizeof(wrap));
	wrap.sel = NVME_FEAT_SEL_CUR;

	tmp = nvme_set_feat_hmb(ndev, &wrap);
	if (tmp < 0) {
		ret |= tmp;
		/* skip release if device still using HMB? */
		goto free_hmb_alloc;
	}

free_hmb_buf:
	tmp = nvme_release_host_mem_buffer(ndev->fd);
	if (tmp < 0)
		ret |= tmp;
free_hmb_alloc:
	free(hmb_alloc);
out:
	nvme_record_subcase_result(__func__, ret);
	return ret;
}

static int case_host_mem_buffer(struct nvme_tool *tool, struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	int ret;

	ret = init_test_data(ndev, test);
	if (ret < 0)
		return ret;

	ret |= subcase_hmb_work_normal(tool);
	ret |= subcase_hmb_double_enable(tool);

	nvme_display_subcase_report();
	return ret;
}
NVME_CASE_SYMBOL(case_host_mem_buffer, 
	"Test host memory buffer in various scenarios");
