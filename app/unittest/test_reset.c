/**
 * @file test_reset.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-03-31
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "pci_regs_ext.h"
#include "libbase.h"
#include "libnvme.h"
#include "test.h"

struct reset_ops {
	 const char	*name;
	 int (*reset)(struct nvme_dev_info *);
};

#define INIT_RESET_OPS(_name, _reset) { .name = _name, .reset = _reset}

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
	uint64_t	nsze;
	uint32_t	lbads;

	uint32_t	mcl;
	uint32_t	msrc;
	uint16_t	mssrl;
};

static struct test_data g_test = {0};

/**
 * @note May re-initialized? ignore...We shall to update this if data changed. 
 */
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
	uint32_t i;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = test->nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->wbuf;
	wrap.size = wrap.nlb * test->lbads;
	wrap.control = control;

	BUG_ON(wrap.size > tool->wbuf_size);

	for (i = 0; i < (wrap.size / 4); i+= 4) {
		*(uint32_t *)(wrap.buf + i) = (uint32_t)rand();
	}

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
		ret = posix_memalign(&desc, CONFIG_UNVME_RW_BUF_ALIGN, 
			sizeof(struct nvme_copy_desc_fmt0) * copy->ranges);
		if (ret) {
			pr_err("failed to alloc copy descriptor!\n");
			return -ENOMEM;
		}
		copy_desc_fmt0_init(desc, copy->entry, copy->ranges);
		copy->size = sizeof(struct nvme_copy_desc_fmt0) * copy->ranges;
		break;
	
	case NVME_COPY_DESC_FMT_40B:
		ret = posix_memalign(&desc, CONFIG_UNVME_RW_BUF_ALIGN,
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
	uint64_t slba = rand() / (test->nsze / 2);
	uint32_t nlb = rand() / min((test->nsze / 2), (uint64_t)256) + 1;

	return send_io_read_cmd(tool, sq, slba, nlb, 0);
}

static int do_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq)
{
	struct test_data *test = &g_test;
	/* ensure "slba + nlb <= nsze */
	uint64_t slba = rand() / (test->nsze / 2);
	uint32_t nlb = rand() / min((test->nsze / 2), (uint64_t)256) + 1;

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
	uint16_t limit = min((test->nsze / 4), (uint64_t)256);
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

	ret = posix_memalign(&copy->rbuf, CONFIG_UNVME_RW_BUF_ALIGN, 
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
		dump_data_to_file(copy->rbuf, sum * test->lbads, "./source.bin");
		dump_data_to_file(tool->rbuf, sum * test->lbads, "./target.bin");
		ret = -EIO;
		goto free_rbuf;
	}

free_rbuf:
	free(copy->rbuf);
free_copy:
	free(copy);
	return ret;
}

static int do_io_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq)
{
	int ret;

	ret = do_io_read_cmd(tool, sq);
	ret |= do_io_write_cmd(tool, sq);
	ret |= do_io_copy_cmd(tool, sq);

	return ret;
}

static int do_nvme_ctrl_reset(struct nvme_dev_info *ndev)
{
	return nvme_disable_controller_complete(ndev->fd);
}

static int do_nvme_subsystem_reset(struct nvme_dev_info *ndev)
{
	return nvme_reset_subsystem(ndev->fd);
}

static int do_pcie_func_level_reset(struct nvme_dev_info *ndev)
{
	return pcie_do_flr(ndev->fd);
}

static int do_pcie_d0d3_reset(struct nvme_dev_info *ndev)
{
	int ret;

	ret = pcie_set_power_state(ndev->fd,  ndev->pm.offset, 
		PCI_PM_CTRL_STATE_D3HOT);
	if (ret < 0) {
		pr_err("failed to set D3 hot state!\n");
		return ret;
	}

	ret = pcie_set_power_state(ndev->fd,  ndev->pm.offset,
		PCI_PM_CTRL_STATE_D0);
	if (ret < 0) {
		pr_err("failed to set D0 state!\n");
		return ret;
	}
	return 0;
}

static int do_pcie_hot_reset(struct nvme_dev_info *ndev)
{
	return pcie_do_hot_reset(ndev->fd);
}

static int do_pcie_linkdown_reset(struct nvme_dev_info *ndev)
{
	return pcie_do_link_down(ndev->fd);
}

static int do_random_reset(struct nvme_dev_info *ndev)
{
	static struct reset_ops ops[] = {
		INIT_RESET_OPS("NVMe Controller Reset", do_nvme_ctrl_reset),
		INIT_RESET_OPS("NVMe Subsystem Reset", do_nvme_subsystem_reset),
		INIT_RESET_OPS("PCIe Function Level Reset", do_pcie_func_level_reset),
		INIT_RESET_OPS("PCIe D0&D3 Reset", do_pcie_d0d3_reset),
		INIT_RESET_OPS("PCIe Hot Reset", do_pcie_hot_reset),
		INIT_RESET_OPS("PCIe Linkdown Reset", do_pcie_linkdown_reset),
	};
	int sel = rand() % ARRAY_SIZE(ops);
	int ret;
	
	ret = ops[sel].reset(ndev);
	if (ret < 0) {
		if (ret == -EOPNOTSUPP) {
			pr_warn("device not support %s!\n", ops[sel].name);
			return 0;
		}
		pr_err("failed to do %s!(%d)\n", ops[sel].name, ret);
		return ret;
	}
	pr_debug("do %s ok!\n", ops[sel].name);
	return 0;
}

static int case_reset_all_random(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct test_data *test = &g_test;
	int loop = 50;
	int ret;

	ret = init_test_data(ndev, test);
	if (ret < 0)
		return ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	do {
		ret = create_ioq(ndev, sq, cq);
		if (ret < 0)
			return ret;

		ret = do_io_cmd(tool, sq);
		if (ret < 0)
			return ret;

		ret = do_random_reset(ndev);
		if (ret < 0)
			return ret;

		msleep(10);

		ret = nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, 
			NVME_INT_MSIX);
		if (ret < 0)
			return ret;
	} while (--loop > 0);

	return ret;
}
NVME_CASE_SYMBOL(case_reset_all_random, 
	"Randomly select one of the following reset types and repeat N times\n"
	"\t NVMe Controller Reset / NVMe Subsystem Reset/ PCIe Function Level Reset\n"
	"\t PCIe D0&D3 Reset / PCIe Hot Reset / PCIe Linkdown Reset");
NVME_AUTOCASE_SYMBOL(case_reset_all_random);
