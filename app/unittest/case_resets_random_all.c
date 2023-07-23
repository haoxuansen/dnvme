#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>

#include "pci_regs_ext.h"
#include "libbase.h"
#include "libnvme.h"

#include "test.h"

enum reset_type {
	NVME_CTRL_RESET = 0,
	NVME_SUBSYSTEM_RESET,
	PCIE_FLR_RESET,
	PCIE_D0D3_RESET,
	PCIE_HOT_RESET,
	PCIE_LINKDOWN_RESET,
	RESET_TYPE_FENCE,
};

static inline enum reset_type select_reset_type_random(void)
{
	return rand() % RESET_TYPE_FENCE;
}

static const char *reset_type_string(enum reset_type type)
{
	switch (type) {
	case NVME_CTRL_RESET:
		return "NVMe Controller Reset";
	case NVME_SUBSYSTEM_RESET:
		return "NVMe Subsystem Reset";
	case PCIE_FLR_RESET:
		return "PCIe Function Level Reset";
	case PCIE_D0D3_RESET:
		return "PCIe D0&D3 Reset";
	case PCIE_HOT_RESET:
		return "PCIe Hot Reset";
	case PCIE_LINKDOWN_RESET:
		return "PCIe Linkdown Reset";
	default:
		return "Unknown";
	}
}

static int do_reset_random(struct nvme_dev_info *ndev)
{
	enum reset_type rst;
	int ret = -EPERM;

	rst = select_reset_type_random();
	pr_notice("%s ...\n", reset_type_string(rst));

	switch (rst) {
	case NVME_CTRL_RESET:
		ret = nvme_disable_controller_complete(ndev->fd);
		break;

	case NVME_SUBSYSTEM_RESET:
		ret = nvme_reset_subsystem(ndev->fd);
		break;
	
	case PCIE_FLR_RESET:
		ret = pcie_do_flr(ndev->fd);
		if (ret == -EOPNOTSUPP) {
			pr_warn("dev not support FLR!\n");
			ret = 0;
		}
		break;
	
	case PCIE_D0D3_RESET:
		ret = pcie_set_power_state(ndev->fd,  ndev->pm.offset, 
			PCI_PM_CTRL_STATE_D3HOT);
		if (ret < 0) {
			pr_err("failed to set D3 hot state!\n");
			break;
		}

		ret = pcie_set_power_state(ndev->fd,  ndev->pm.offset,
			PCI_PM_CTRL_STATE_D0);
		break;
	
	case PCIE_HOT_RESET:
		ret = pcie_do_hot_reset(ndev->fd);
		break;

	case PCIE_LINKDOWN_RESET:
		ret = pcie_do_link_down(ndev->fd);
		break;
	
	default:
		pr_err("reset type %u is not support!\n", rst);
		return -EINVAL;
	}

	if (ret < 0)
		pr_err("%s err!(%d)\n", reset_type_string(rst), ret);
	else
		pr_info("%s ok!\n", reset_type_string(rst));
	
	return ret;
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

static int send_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = ndev->nss[0].nsid;
	wrap.slba = rand() % (ndev->nss[0].nsze / 2);
	wrap.nlb = 256;
	wrap.buf = tool->rbuf;
	wrap.size = wrap.nlb * ndev->nss[0].lbads;

	if (rand() % 2)
		wrap.control = NVME_RW_FUA;

	BUG_ON(wrap.size > tool->rbuf_size);

	return nvme_io_read(ndev, &wrap);
}

static int send_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = ndev->nss[0].nsid;
	wrap.slba = rand() % (ndev->nss[0].nsze / 2);
	wrap.nlb = 256;
	wrap.buf = tool->wbuf;
	wrap.size = wrap.nlb * ndev->nss[0].lbads;

	BUG_ON(wrap.size > tool->wbuf_size);

	/* skip the initialization of write buffer */

	return nvme_io_write(ndev, &wrap);
}

static int send_io_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq)
{
	int nr_cmds = (rand() % 512) / 2 + 100;
	int i;
	int ret;

	BUG_ON(nr_cmds > sq->size);

	for (i = 0; i < nr_cmds; i++) {
		if (i % 2)
			ret = send_io_read_cmd(tool, sq);
		else
			ret = send_io_write_cmd(tool, sq);

		if (ret < 0)
			return ret;
	}
	return nr_cmds;
}

static void show_case_description(void)
{
	pr_info("Do random reset for 50 times!\n");
	pr_debug("Before each reset, serveral IO Read/Write cmds are submitted to Queue.\n");
	pr_debug("Each reset is randomly selected from the following types:\n"
		"\t %s \n\t %s \n\t %s \n\t %s \n\t %s \n\t %s\n",
		reset_type_string(NVME_CTRL_RESET), 
		reset_type_string(NVME_SUBSYSTEM_RESET),
		reset_type_string(PCIE_FLR_RESET),
		reset_type_string(PCIE_D0D3_RESET),
		reset_type_string(PCIE_HOT_RESET),
		reset_type_string(PCIE_LINKDOWN_RESET));
}

int case_resets_random_all(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	int loop = 50;
	int ret;

	show_case_description();

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	do {
		ret = create_ioq(ndev, sq, cq);
		if (ret < 0)
			return ret;

		ret = send_io_cmd(tool, sq);
		if (ret < 0)
			return ret;
		
		ret = nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
		if (ret < 0)
			return ret;
		
		/* skip reaping CQ entries */

		ret = do_reset_random(ndev);
		if (ret < 0)
			return ret;

		msleep(10);

		ret = nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, 
			NVME_INT_MSIX);
		if (ret < 0)
			return ret;
	} while (--loop > 0);

	if (ret < 0) {
		pr_err("%s\n", TEST_FAIL);
		return ret;
	}

	pr_info("%s\n", TEST_PASS);
	return 0;
}

