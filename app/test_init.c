/**
 * @file test_init.c
 * @author yumeng (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2019-10-08
 * 
 * @copyright Copyright (c) 2018-2019 Maxio-Tech
 * 
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <errno.h>

#include "byteorder.h"
#include "dnvme_ioctl.h"
#include "ioctl.h"
#include "pci.h"
#include "irq.h"
#include "queue.h"
#include "cmd.h"
#include "debug.h"

#include "auto_header.h"
#include "common.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_irq.h"
#include "test_cq_gain.h"

#include "test_init.h"


static int request_io_queue_num(int fd, uint16_t *nr_sq, uint16_t *nr_cq)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_set_feat_num_queues(fd, *nr_sq, *nr_cq);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_reap_expect_cqe(fd, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, 0);
	if (ret < 0)
		return ret;

	*nr_sq = le32_to_cpu(entry.result.u32) & 0xffff;
	*nr_cq = le32_to_cpu(entry.result.u32) >> 16;

	return 0;
}

static int init_ns_data(int fd, uint32_t nn)
{
	struct nvme_ns *ns;
	uint32_t idx;
	uint8_t flbas;
	int ret;

	ns = calloc(nn, sizeof(*ns));
	if (!ns) {
		pr_err("failed to alloc for ns!(nn:%u)\n", nn);
		return -ENOMEM;
	}

	/* !FIXME: The identify of namespaces may not start with 1 or be
	 * consecutive
	 *
	 * It's better to get the correct identify by getting active namespace
	 * ID list ?
	 */
	for (idx = 0; idx < nn; idx++) {
		ret = nvme_identify_ns(fd, &ns[idx].id_ns, idx + 1);
		if (ret < 0) {
			pr_err("failed to get ns(%u) data!(%d)\n", idx + 1, ret);
			goto out;
		}
		nvme_display_id_ns(&ns[idx].id_ns);

		ns[idx].nsze = le64_to_cpu(ns[idx].id_ns.nsze);
		flbas = ns[idx].id_ns.flbas & 0xf;
		ns[idx].lbads = 1 << ns[idx].id_ns.lbaf[flbas].ds;

		if (!ns[idx].nsze) {
			pr_err("NS:%u, NSZE is zero?\n", idx + 1);
			ret = -EINVAL;
			goto out;
		}

		if (ns[idx].lbads < 512) {
			pr_err("NS:%u, lbads %u in byte is invalid!\n", 
				idx + 1, ns[idx].lbads);
			ret = -EINVAL;
			goto out;
		}
	}

	g_nvme_ns_info = ns;
	return 0;
out:
	free(ns);
	return ret;
}

static int init_ctrl_property(int fd, struct nvme_ctrl_property *prop)
{
	int ret;

	ret = nvme_read_ctrl_cap(fd, &prop->cap);
	if (ret < 0)
		return ret;

	nvme_display_cap(prop->cap);

	ret = nvme_read_ctrl_cc(fd, &prop->cc);
	if (ret < 0)
		return ret;

	nvme_display_cc(prop->cc);
	return 0;
}

static int init_capability(int fd, struct nvme_dev_info *ndev)
{
	int ret;
	struct nvme_cap *cap = &ndev->cap;
	struct pci_cap *pci = cap->pci;

	ret = nvme_get_capability(g_fd, cap);
	if (ret < 0)
		return ret;
	
	if (pci[PCI_CAP_ID_PM - 1].id != PCI_CAP_ID_PM) {
		pr_err("Not Support Power Management!\n");
		return -ENOTSUP;
	}
	ndev->pmcap_ofst = pci[PCI_CAP_ID_PM - 1].offset;

	if (pci[PCI_CAP_ID_MSI - 1].id != PCI_CAP_ID_MSI) {
		pr_err("Not Support Message Signalled Interrupts!\n");
		return -ENOTSUP;
	}
	ndev->msicap_ofst = pci[PCI_CAP_ID_MSI - 1].offset;

	if (pci[PCI_CAP_ID_EXP - 1].id != PCI_CAP_ID_EXP) {
		pr_err("Not Support PCI Express!\n");
		return -ENOTSUP;
	}
	ndev->pxcap_ofst = pci[PCI_CAP_ID_EXP - 1].offset;

	return 0;
}

static int check_link_status(int fd, struct nvme_dev_info *ndev)
{
	int ret;
	uint32_t link_sts;

	ret = pci_exp_read_link_status(g_fd, ndev->pxcap_ofst, &link_sts);
	if (ret < 0) {
		pr_err("failed to read link status reg!(%d)\n", ret);
		return ret;
	}

	ndev->link_speed = link_sts & PCI_EXP_LNKSTA_CLS;
	ndev->link_width = (link_sts & PCI_EXP_LNKSTA_NLW) >> 
		PCI_EXP_LNKSTA_NLW_SHIFT;

	pr_info("Current Link Speed: Gen%u, Width: x%u\n", 
		ndev->link_speed, ndev->link_width);
	return 0;
}

/**
 * @brief Try to get the basic information of NVMe device. Eg. queue number,
 *  Identify controller data...
 * 
 * @param fd NVMe device file descriptor
 * @return 0 on success, otherwise a negative errno
 */
static int nvme_init_stage1(int fd, struct nvme_dev_info *ndev)
{
	uint16_t iosq_num = 0xff; /* expect */
	uint16_t iocq_num = 0xff;
	int ret;

	ret = nvme_disable_controller_complete(fd);
	if (ret < 0)
		return ret;

	ret = nvme_create_aq_pair(fd, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE);
	if (ret < 0)
		return ret;

	ret = nvme_set_irq(fd, NVME_INT_PIN, 1);
	if (ret < 0)
		return ret;
	ndev->irq_type = NVME_INT_PIN;
	ndev->nr_irq = 1;

	ret = nvme_enable_controller(fd);
	if (ret < 0)
		return ret;

	ret = request_io_queue_num(fd, &iosq_num, &iocq_num);
	if (ret < 0)
		return ret;

	ndev->max_sq_num = iosq_num + 1;
	ndev->max_cq_num = iocq_num + 1;
	pr_info("Controller has allocated %u IOSQ, %u IOCQ!\n", 
		ndev->max_sq_num, ndev->max_cq_num);

	ret = nvme_identify_ctrl(fd, &ndev->id_ctrl);
	if (ret < 0)
		return ret;

	nvme_display_id_ctrl(&ndev->id_ctrl);
	return 0;
}

static int nvme_init_stage2(int fd, struct nvme_dev_info *ndev)
{
	int ret;

	ret = nvme_disable_controller_complete(fd);
	if (ret < 0)
		return ret;

	ret = nvme_create_aq_pair(fd, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE);
	if (ret < 0)
		return ret;

	/* the number of irq equals to (ACQ + IOCQ) */
	ret = nvme_set_irq(fd, NVME_INT_MSIX, ndev->max_cq_num + 1);
	if (ret < 0)
		return ret;
	ndev->irq_type = NVME_INT_MSIX;
	ndev->nr_irq = ndev->max_cq_num + 1;

	ret = nvme_enable_controller(fd);
	if (ret < 0)
		return ret;

	ret = init_ctrl_property(fd, &ndev->prop);
	if (ret < 0)
		return ret;

	ret = nvme_init_ioq_info(ndev);
	if (ret < 0)
		return ret;
	g_ctrl_sq_info = ndev->iosqs; /* !TODO: obsolete */

	ret = init_ns_data(fd, ndev->id_ctrl.nn);
	if (ret < 0)
		return ret;

	ret = init_capability(fd, ndev);
	if (ret < 0)
		return ret;

	ret = check_link_status(fd, ndev);
	if (ret < 0)
		return ret;

	return 0;
}


int nvme_init(struct nvme_dev_info *ndev)
{
	int ret;

	ret = nvme_init_stage1(ndev->fd, ndev);
	if (ret < 0)
		return ret;

	ret = nvme_init_stage2(ndev->fd, ndev);
	if (ret < 0)
		goto out;

	/* default entry size is set by driver */
	ndev->io_sqes = NVME_NVM_IOSQES;
	ndev->io_cqes = NVME_NVM_IOCQES;
	return 0;
out:
	/* !TODO: Required to release allocated source after failed! */
	return ret;
}

