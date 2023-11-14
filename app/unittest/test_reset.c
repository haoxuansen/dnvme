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

#define TEST_IO_CMD_ROUND			256
#define TEST_IO_QUEUE_NUM			2

struct reset_ops {
	 const char	*name;
	 int (*reset)(struct nvme_dev_info *);
};

#define INIT_RESET_OPS(_name, _reset) { .name = _name, .reset = _reset}

struct source_range {
	uint64_t	slba;
	uint32_t	nlb;
};

struct test_cmd_copy {
	uint64_t	slba; 		/* I: target */

	uint8_t		desc_fmt;	/* I: descriptor format */
	void		*desc;
	uint32_t	size;

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

	struct nvme_sq_info *sq[TEST_IO_QUEUE_NUM];
	struct nvme_cq_info *cq[TEST_IO_QUEUE_NUM];
	struct test_cmd_copy *copy[TEST_IO_CMD_ROUND];
};

static struct test_data g_test = {0};

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

static void deinit_test_data_for_cmd_copy(struct test_data *data)
{
	uint32_t i;

	for (i = 0; i < TEST_IO_CMD_ROUND; i++) {
		if (data->copy[i]) {
			copy_desc_exit(data->copy[i]);
			free(data->copy[i]);
			data->copy[i] = NULL;
		}
	}
}

static int init_test_data_for_cmd_copy(struct nvme_dev_info *ndev, 
	struct test_data *data)
{
	struct test_cmd_copy *copy;
	uint32_t sum = 0;
	uint64_t start = 0;
	uint32_t entry;
	uint32_t i, j;
	uint16_t limit = min_t(uint64_t, data->nsze / 4, 256);
	int ret;

	ret = nvme_ctrl_support_copy_cmd(ndev->ctrl);
	if (ret == 0) {
		pr_debug("device not support copy cmd! skip...\n");
		return 0;
	} else if (ret < 0) {
		pr_err("failed to check capability!(%d)\n", ret);
		return ret;
	}

	for (j = 0; j < TEST_IO_CMD_ROUND; j++) {
		entry = rand() % data->msrc + 1; /* 1~msrc */

		copy = zalloc(sizeof(struct test_cmd_copy) +
			entry * sizeof(struct source_range));
		if (!copy) {
			pr_err("failed to alloc memory!\n");
			ret = -ENOMEM;
			goto out;
		}

		copy->ranges = entry;
		copy->desc_fmt = NVME_COPY_DESC_FMT_32B;

		sum = 0;
		start = 0;
		do {
			if (copy->slba)
				pr_warn("target slba + nlb > nsze! try again ?\n");

			do {
				if (sum)
					pr_warn("NLB total is larger than MCL! try again?\n");

				for (i = 0, sum = 0; i < copy->ranges; i++) {
					copy->entry[i].slba = rand() % (data->nsze / 4);
					copy->entry[i].nlb = rand() % min(data->mssrl, limit) + 1;
					sum += copy->entry[i].nlb;

					if (start < (copy->entry[i].slba + copy->entry[i].nlb))
						start = copy->entry[i].slba + copy->entry[i].nlb;
				}
			} while (data->mcl < sum);

			copy->slba = rand() % (data->nsze / 4) + start;

		} while ((copy->slba + sum) > data->nsze);

		ret = copy_desc_init(copy);
		if (ret < 0)
			goto out;

		data->copy[j] = copy;
	}

	return 0;
out:
	deinit_test_data_for_cmd_copy(data);
	return ret;
}

/**
 * @note May re-initialized? ignore...We shall to update this if data changed. 
 */
static int init_test_data(struct nvme_dev_info *ndev, struct test_data *data)
{
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sq;
	unsigned int i;
	int ret;

	if (ctrl->nr_sq < TEST_IO_QUEUE_NUM || ctrl->nr_cq < TEST_IO_QUEUE_NUM) {
		pr_err("nr_sq(%u) or nr_cq(%u) < test queue(%u)!\n",
			ctrl->nr_sq, ctrl->nr_cq, TEST_IO_QUEUE_NUM);
		return -ENODEV;
	}

	for (i = 0; i < TEST_IO_QUEUE_NUM; i++) {
		sq = &ndev->iosqs[i];
		data->cq[i] = nvme_find_iocq_info(ndev, sq->cqid);
		if (!data->cq[i]) {
			pr_err("failed to find iocq(%u)!\n", sq->cqid);
			return -ENODEV;
		}
		data->sq[i] = sq;
	}

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

	ret = init_test_data_for_cmd_copy(ndev, data);
	if (ret < 0)
		return ret;

	return 0;
}

static void deinit_test_data(struct test_data *data)
{
	deinit_test_data_for_cmd_copy(data);
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

static int create_ioqs(struct nvme_dev_info *ndev, struct test_data *data)
{
	int i;
	int ret;

	for (i = 0; i < TEST_IO_QUEUE_NUM; i++) {
		ret = create_ioq(ndev, data->sq[i], data->cq[i]);
		if (ret < 0)
			goto out;
	}

	return 0;
out:
	for (i--; i >= 0; i--)
		delete_ioq(ndev, data->sq[i], data->cq[i]);

	return ret;
}

static int submit_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
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
	wrap.nsid = test->nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->wbuf;
	wrap.size = wrap.nlb * test->lbads;

	BUG_ON(wrap.size > tool->wbuf_size);
	fill_data_with_random(wrap.buf, wrap.size);

	return nvme_cmd_io_write(ndev->fd, &wrap);
}


static int submit_io_copy_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	struct test_cmd_copy *copy)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_copy_wrapper wrap = {0};
	struct test_data *test = &g_test;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = test->nsid;
	wrap.slba = copy->slba;
	wrap.ranges = copy->ranges - 1;
	wrap.desc_fmt = copy->desc_fmt;
	wrap.desc = copy->desc;
	wrap.size = copy->size;

	return nvme_cmd_io_copy(ndev->fd, &wrap);
}

static int prepare_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq)
{
	struct test_data *test = &g_test;
	/* ensure "slba + nlb <= nsze */
	uint64_t slba = rand() % (test->nsze / 2);
	uint32_t nlb = rand() % min_t(uint64_t, test->nsze / 2, 256) + 1;

	return submit_io_read_cmd(tool, sq, slba, nlb);
}

static int prepare_io_read_cmd_invalid_range(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct test_data *test = &g_test;
	/* ensure "slba + nlb > nsze */
	uint32_t nlb = rand() % min_t(uint64_t, test->nsze / 2, 256) + 1;
	uint64_t slba = test->nsze - (nlb / 2);

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

static int prepare_io_write_cmd_invalid_range(struct nvme_tool *tool,
	struct nvme_sq_info *sq)
{
	struct test_data *test = &g_test;
	/* ensure "slba + nlb > nsze */
	uint32_t nlb = rand() % min_t(uint64_t, test->nsze / 2, 256) + 1;
	uint64_t slba = test->nsze - (nlb / 2);

	return submit_io_write_cmd(tool, sq, slba, nlb);
}

static int prepare_io_copy_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint32_t idx)
{
	struct test_data *test = &g_test;

	if (!test->copy[idx])
		return 0; /* not support copy cmd */

	return submit_io_copy_cmd(tool, sq, test->copy[idx]);
}

static int do_io_cmd(struct nvme_tool *tool, struct test_data *data)
{
	struct nvme_sq_info *sq;
	struct nvme_dev_info *ndev = tool->ndev;
	uint32_t cnt;
	uint32_t i;
	int ret;

	sq = data->sq[0];
	cnt = min_t(uint32_t, sq->nr_entry / 6, TEST_IO_CMD_ROUND);
	for (i = 0; i < cnt; i++) {
		ret = prepare_io_read_cmd(tool, sq);
		ret |= prepare_io_write_cmd(tool, sq);
		ret |= prepare_io_copy_cmd(tool, sq, i);

		if (ret < 0) {
			pr_err("failed to submit io cmd!(%d)\n", ret);
			return ret;
		}
	}

	sq = data->sq[1];
	cnt = min_t(uint32_t, sq->nr_entry / 6, TEST_IO_CMD_ROUND);
	for (i = 0; i < cnt; i++) {
		ret = prepare_io_read_cmd_invalid_range(tool, sq);
		ret |= prepare_io_write_cmd_invalid_range(tool, sq);

		if (ret < 0) {
			pr_err("failed to submit io cmd!(%d)\n", ret);
			return ret;
		}
	}

	ret = nvme_ring_sq_doorbell(ndev->fd, data->sq[0]->sqid);
	ret |= nvme_ring_sq_doorbell(ndev->fd, data->sq[1]->sqid);

	return ret;
}

static int do_nvme_ctrl_reset(struct nvme_dev_info *ndev)
{
	return nvme_disable_controller_complete(ndev->fd);
}

static int do_nvme_subsystem_reset(struct nvme_dev_info *ndev)
{
	struct pci_dev_instance *pdev = ndev->pdev;
	uint32_t bar[6];
	uint16_t cmd, bkup;
	int fd = ndev->fd;
	int ret;

	if (nvme_is_maxio_falcon_lite(pdev))
		return nvme_reset_subsystem(ndev->fd);

	/* backup register data */
	ret = pci_read_config_data(fd, PCI_BASE_ADDRESS_0, sizeof(bar), bar);
	if (ret < 0)
		return ret;

	ret = pci_hd_read_command(fd, &cmd);
	if (ret < 0)
		return ret;
	bkup = cmd & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);

	/* do subsystem reset */
	ret = nvme_reset_subsystem(ndev->fd);
	if (ret < 0)
		return ret;
	msleep(100);

	/* restore register data */
	ret = pci_hd_read_command(fd, &cmd);
	if (ret < 0)
		return ret;

	cmd &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
	cmd |= bkup;
	ret = pci_hd_write_command(fd, cmd);
	if (ret < 0)
		return ret;

	ret = pci_write_config_data(fd, PCI_BASE_ADDRESS_0, sizeof(bar), bar);
	if (ret < 0)
		return ret;

	ret = pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * @brief Check whether function level reset is supported
 * 
 * @param pxcap PCI Express Capability offset
 * @return 1 if support FLR, 0 if not support FLR, otherwise a negative errno.
 */
static int pcie_is_support_flr(int fd, uint8_t pxcap)
{
	uint32_t devcap;
	int ret;

	ret = pci_exp_read_device_capability(fd, pxcap, &devcap);
	if (ret < 0)
		return ret;

	return (devcap & PCI_EXP_DEVCAP_FLR) ? 1 : 0;
}

/**
 * @brief Do function level reset
 * 
 * @param pxcap PCI Express Capability offset
 * @return 0 on success, otherwise a negative errno.
 */
static int pcie_do_flr_legacy(int fd, uint8_t pxcap)
{
	uint32_t bar[6];
	uint16_t devctrl;
	uint16_t cmd, cmd_bkup;
	int ret;

	/* backup register data */
	ret = pci_read_config_data(fd, PCI_BASE_ADDRESS_0, sizeof(bar), bar);
	if (ret < 0)
		return ret;
	
	ret = pci_hd_read_command(fd, &cmd);
	if (ret < 0)
		return ret;
	cmd_bkup = cmd & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
	
	/* do function level reset */
	ret = pci_exp_read_device_control(fd, pxcap, &devctrl);
	if (ret < 0)
		return ret;

	devctrl |= PCI_EXP_DEVCTL_BCR_FLR;
	ret = pci_exp_write_device_control(fd, pxcap, devctrl);
	if (ret < 0)
		return ret;

	/*
	 * Refer to PCIe Base Spec R5.0 Ch6.6.2:
	 *   After an FLR has been initiated by writing a 1b to the Initiate
	 * Function Level Reset bit, the Function must complete the FLR 
	 * within 100ms.
	 */
	msleep(100);

	/* restore register data */
	ret = pci_hd_read_command(fd, &cmd);
	if (ret < 0)
		return ret;

	cmd &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
	cmd |= cmd_bkup;
	ret = pci_hd_write_command(fd, cmd);
	if (ret < 0)
		return ret;

	ret = pci_write_config_data(fd, PCI_BASE_ADDRESS_0, sizeof(bar), bar);
	if (ret < 0)
		return ret;

	return 0;
}

static int do_pcie_func_level_reset(struct nvme_dev_info *ndev)
{
	struct pci_dev_instance *pdev = ndev->pdev;
	uint8_t exp_oft = pdev->express.offset;
	int ret;

	ret = pcie_is_support_flr(ndev->fd, exp_oft);
	if (ret < 0)
		return ret;
	else if (ret == 0)
		return -EOPNOTSUPP;

	return pcie_do_flr_legacy(ndev->fd, exp_oft);
	// return pcie_do_flr(ndev->fd);
}

static int do_pcie_d0d3_reset(struct nvme_dev_info *ndev)
{
	struct pci_dev_instance *pdev = ndev->pdev;
	uint8_t pm_oft = pdev->pm.offset;
	int ret;

	ret = pcie_set_power_state(ndev->fd, pm_oft, PCI_PM_CTRL_STATE_D3HOT);
	if (ret < 0) {
		pr_err("failed to set D3 hot state!\n");
		return ret;
	}

	ret = pcie_set_power_state(ndev->fd, pm_oft, PCI_PM_CTRL_STATE_D0);
	if (ret < 0) {
		pr_err("failed to set D0 state!\n");
		return ret;
	}
	return 0;
}

/**
 * @return 0 on success, otherwise a negative errno.
 */
static int pcie_do_hot_reset_legacy(int fd)
{
	uint32_t bar[6];
	uint16_t cmd, bkup;
	int ret;

	/* backup register data */
	ret = pci_read_config_data(fd, PCI_BASE_ADDRESS_0, sizeof(bar), bar);
	if (ret < 0)
		return ret;

	ret = pci_hd_read_command(fd, &cmd);
	if (ret < 0)
		return ret;
	bkup = cmd & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);

	/* do hot reset */
	ret = call_system("setpci -s " RC_PCI_HDR_REG_BRIDGE_CONTROL ".b=40:40");
	if (ret < 0)
		return ret;
	msleep(100);
	
	ret = call_system("setpci -s " RC_PCI_HDR_REG_BRIDGE_CONTROL ".b=00:40");
	if (ret < 0)
		return ret;
	msleep(100);

	/* restore register data */
	ret = pci_hd_read_command(fd, &cmd);
	if (ret < 0)
		return ret;

	cmd &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
	cmd |= bkup;
	ret = pci_hd_write_command(fd, cmd);
	if (ret < 0)
		return ret;

	ret = pci_write_config_data(fd, PCI_BASE_ADDRESS_0, sizeof(bar), bar);
	if (ret < 0)
		return ret;

	ret = pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);
	if (ret < 0)
		return ret;

	return 0;
}

static int do_pcie_hot_reset(struct nvme_dev_info *ndev)
{
	return pcie_do_hot_reset_legacy(ndev->fd);
	// return pcie_do_hot_reset(ndev->fd);
}

/**
 * @return 0 on success, otherwise a negative errno.
 */
static int pcie_do_link_down_legacy(int fd)
{
	uint32_t bar[6];
	uint16_t cmd, bkup;
	int ret;

	/* backup register data */
	ret = pci_read_config_data(fd, PCI_BASE_ADDRESS_0, sizeof(bar), bar);
	if (ret < 0)
		return ret;

	ret = pci_hd_read_command(fd, &cmd);
	if (ret < 0)
		return ret;
	bkup = cmd & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);

	/* do hot reset */
	ret = call_system("setpci -s " RC_PCI_EXP_REG_LINK_CONTROL ".b=10:10");
	if (ret < 0)
		return ret;
	msleep(100);
	
	ret = call_system("setpci -s " RC_PCI_EXP_REG_LINK_CONTROL ".b=00:10");
	if (ret < 0)
		return ret;
	msleep(100);

	/* restore register data */
	ret = pci_hd_read_command(fd, &cmd);
	if (ret < 0)
		return ret;

	cmd &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
	cmd |= bkup;
	ret = pci_hd_write_command(fd, cmd);
	if (ret < 0)
		return ret;

	ret = pci_write_config_data(fd, PCI_BASE_ADDRESS_0, sizeof(bar), bar);
	if (ret < 0)
		return ret;

	ret = pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);
	if (ret < 0)
		return ret;

	return 0;
}

static int do_pcie_linkdown_reset(struct nvme_dev_info *ndev)
{
	return pcie_do_link_down_legacy(ndev->fd);
	// return pcie_do_link_down(ndev->fd);
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

static int case_reset_all_random(struct nvme_tool *tool, struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	int loop = 50;
	int ret;

	ret = init_test_data(ndev, test);
	if (ret < 0)
		return ret;

	do {
		ret = create_ioqs(ndev, test);
		if (ret < 0)
			goto out;

		ret = do_io_cmd(tool, test);
		if (ret < 0)
			goto out;

		ret = do_random_reset(ndev);
		if (ret < 0)
			goto out;

		msleep(10);

		ret = nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, 
			NVME_INT_MSIX);
		if (ret < 0)
			goto out;
	} while (--loop > 0);

out:
	deinit_test_data(test);
	return ret;
}
NVME_CASE_SYMBOL(case_reset_all_random, 
	"Randomly select one of the following reset types and repeat N times\n"
	"\t NVMe Controller Reset / NVMe Subsystem Reset/ PCIe Function Level Reset\n"
	"\t PCIe D0&D3 Reset / PCIe Hot Reset / PCIe Linkdown Reset");
NVME_AUTOCASE_SYMBOL(case_reset_all_random);
