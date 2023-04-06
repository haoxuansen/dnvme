/**
 * @file core.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-04
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "log.h"
#include "byteorder.h"

#include "core.h"
#include "ioctl.h"
#include "queue.h"
#include "irq.h"
#include "cmd.h"
#include "pci.h"
#include "debug.h"

/**
 * @return 0 on success, otherwise a negative errno.
 */
int call_system(const char *command)
{
	int status;

	status = system(command);
	if (-1 == status) {
		pr_err("system err!\n");
		return -EPERM;
	}

	if (!WIFEXITED(status)) {
		pr_err("abort with status:%d!\n", WEXITSTATUS(status));
		return -EPERM;
	}

	if (WEXITSTATUS(status)) {
		pr_err("failed to execute '%s'!(%d)\n", command, 
			WEXITSTATUS(status));
		return -EPERM;
	}

	return 0;
}

void nvme_fill_data(void *buf, uint32_t size)
{
	uint32_t i;

	for (i = 0; i < size; i++) {
		*(uint8_t *)(buf + i) = (uint8_t)rand();
	}
}

void nvme_dump_data(void *buf, uint32_t size)
{
	uint32_t oft = 0;
	char string[256];
	char *str = string;
	unsigned char *ptr = buf;
	int ret;

	while (size >= 0x10) {
		snprintf(str, sizeof(string), 
			"%x: %02x %02x %02x %02x %02x %02x %02x %02x "
			"%02x %02x %02x %02x %02x %02x %02x %02x", oft, 
			ptr[oft], ptr[oft + 1], ptr[oft + 2], ptr[oft + 3],
			ptr[oft + 4], ptr[oft + 5], ptr[oft + 6], ptr[oft + 7],
			ptr[oft + 8], ptr[oft + 9], ptr[oft + 10], ptr[oft + 11],
			ptr[oft + 12], ptr[oft + 13], ptr[oft + 14], ptr[oft + 15]);
		pr_debug("%s\n", str);
		size -= 0x10;
		oft += 0x10;
	}

	if (!size)
		return;

	ret = snprintf(str, sizeof(string), "%x:", oft);

	if (size >= 8) {
		ret += snprintf(str + ret, sizeof(string) - ret, 
			" %02x %02x %02x %02x %02x %02x %02x %02x", 
			ptr[oft], ptr[oft + 1], ptr[oft + 2], ptr[oft + 3],
			ptr[oft + 4], ptr[oft + 5], ptr[oft + 6], ptr[oft + 7]);
		size -= 8;
		oft += 8;
	}

	if (size >= 4) {
		ret += snprintf(str + ret, sizeof(string) - ret, 
			" %02x %02x %02x %02x", 
			ptr[oft], ptr[oft + 1], ptr[oft + 2], ptr[oft + 3]);
		size -= 4;
		oft += 4;
	}

	if (size >= 2) {
		ret += snprintf(str + ret, sizeof(string) - ret, 
			" %02x %02x", ptr[oft], ptr[oft + 1]);
		size -= 2;
		oft += 2;
	}

	if (size >= 1) {
		ret += snprintf(str + ret, sizeof(string) - ret, 
			" %02x", ptr[oft]);
		size -= 1;
		oft += 1;
	}

	pr_debug("%s\n", str);
}

int nvme_update_ns_info(int fd, struct nvme_ns_info *ns)
{
	int ret;
	uint8_t flbas;

	ret = nvme_identify_ns_active(fd, &ns->id_ns, ns->nsid);
	if (ret < 0) {
		pr_err("failed to get ns(%u) data!(%d)\n", ns->nsid, ret);
		return ret;
	}

	ns->nsze = le64_to_cpu(ns->id_ns.nsze);
	flbas = NVME_NS_FLBAS_LBA(ns->id_ns.flbas);
	ns->lbads = 1 << ns->id_ns.lbaf[flbas].ds;
	ns->ms = le16_to_cpu(ns->id_ns.lbaf[flbas].ms);
	return 0;
}

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

static int init_ns_data(struct nvme_dev_info *ndev)
{
	struct nvme_ns_info *ns;
	uint32_t nn = le32_to_cpu(ndev->id_ctrl.nn);
	uint32_t last = 0;
	uint32_t i;
	uint8_t flbas;
	__le32 *ns_list;
	int ret = -ENOMEM;

	WARN_ON(nn > 1024);

	ns = calloc(nn, sizeof(*ns));
	if (!ns) {
		pr_err("failed to alloc for ns!(nn:%u)\n", nn);
		return -ENOMEM;
	}

	ns_list = malloc(NVME_IDENTIFY_DATA_SIZE);
	if (!ns_list) {
		pr_err("failed to allo for ns list!\n");
		goto out;
	}
	memset(ns_list, 0, NVME_IDENTIFY_DATA_SIZE);

	ret = nvme_identify_ns_list_active(ndev->fd, ns_list, 
		NVME_IDENTIFY_DATA_SIZE, 0);
	if (ret < 0) {
		pr_err("failed to get active ns list!(%d)\n", ret);
		goto out2;
	}

	for (i = 0; i < 1024 && ns_list[i] != 0; i++) {
		BUG_ON(i >= nn);

		ns[i].nsid = le32_to_cpu(ns_list[i]);
		if (ns[i].nsid <= last) {
			pr_err("The NSID in list must increase in order!\n");
			goto out2;
		}
		last = ns[i].nsid;

		ret = nvme_identify_ns_active(ndev->fd, &ns[i].id_ns, ns[i].nsid);
		if (ret < 0) {
			pr_err("failed to get ns(%u) data!(%d)\n", ns[i].nsid, ret);
			goto out2;
		}
		nvme_display_id_ns(&ns[i].id_ns, ns[i].nsid);

		ns[i].nsze = le64_to_cpu(ns[i].id_ns.nsze);
		flbas = NVME_NS_FLBAS_LBA(ns[i].id_ns.flbas);
		ns[i].lbads = 1 << ns[i].id_ns.lbaf[flbas].ds;
		ns[i].ms = le16_to_cpu(ns[i].id_ns.lbaf[flbas].ms);

		if (!ns[i].nsze || ns[i].lbads < 512) {
			pr_err("invlid nsze(%llu) or lbads(%u) in ns(%u)\n", 
				ns[i].nsze, ns[i].lbads, ns[i].nsid);
			ret = -EINVAL;
			goto out2;
		}
	}

	ndev->nss = ns;
	ndev->ns_num_total = nn;
	ndev->ns_num_actual = i;

	free(ns_list);
	return 0;
out2:
	free(ns_list);
out:
	free(ns);
	return ret;
}

static void deinit_ns_data(struct nvme_dev_info *ndev)
{
	free(ndev->nss);
	ndev->nss = NULL;
	ndev->ns_num_total = 0;
	ndev->ns_num_actual = 0;
}

static int init_ctrl_property(int fd, struct nvme_ctrl_property *prop)
{
	int ret;

	ret = nvme_read_ctrl_cap(fd, &prop->cap);
	if (ret < 0)
		return ret;

	nvme_display_cap(prop->cap);

	ret = nvme_read_ctrl_vs(fd, &prop->vs);
	if (ret < 0)
		return ret;

	pr_notice("NVMe Version: %u.%u.%u\n", NVME_VS_MJR(prop->vs), 
		NVME_VS_MNR(prop->vs), NVME_VS_TER(prop->vs));

	ret = nvme_read_ctrl_cc(fd, &prop->cc);
	if (ret < 0)
		return ret;

	nvme_display_cc(prop->cc);
	return 0;
}

static int init_capability(struct nvme_dev_info *ndev)
{
	int ret;

	ret = nvme_get_pci_capability(ndev->fd, PCI_CAP_ID_MSI, 
					&ndev->msi, sizeof(ndev->msi));
	if (ret < 0)
		return ret;

	ret = nvme_get_pci_capability(ndev->fd, PCI_CAP_ID_MSIX,
					&ndev->msix, sizeof(ndev->msix));
	if (ret < 0)
		return ret;

	ret = nvme_get_pci_capability(ndev->fd, PCI_CAP_ID_PM,
					&ndev->pm, sizeof(ndev->pm));
	if (ret < 0)
		return ret;

	ret = nvme_get_pci_capability(ndev->fd, PCI_CAP_ID_EXP,
					&ndev->express, sizeof(ndev->express));
	if (ret < 0)
		return ret;

	return 0;
}

static int check_link_status(struct nvme_dev_info *ndev)
{
	int ret;
	uint16_t link_sts;

	ret = pci_exp_read_link_status(ndev->fd, ndev->express.offset, &link_sts);
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
 * @return 0 on success, otherwise a negative errno
 */
static int nvme_init_stage1(struct nvme_dev_info *ndev)
{
	uint16_t iosq_num = 0xff; /* expect */
	uint16_t iocq_num = 0xff;
	int ret;

	ret = nvme_disable_controller_complete(ndev->fd);
	if (ret < 0)
		return ret;

	ret = nvme_create_aq_pair(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE);
	if (ret < 0)
		return ret;

	ret = nvme_set_irq(ndev->fd, NVME_INT_PIN, 1);
	if (ret < 0)
		return ret;
	ndev->irq_type = NVME_INT_PIN;
	ndev->nr_irq = 1;

	ret = nvme_enable_controller(ndev->fd);
	if (ret < 0)
		return ret;

	ret = request_io_queue_num(ndev->fd, &iosq_num, &iocq_num);
	if (ret < 0)
		return ret;

	ndev->max_sq_num = iosq_num + 1;
	ndev->max_cq_num = iocq_num + 1;
	pr_info("Controller has allocated %u IOSQ, %u IOCQ!\n", 
		ndev->max_sq_num, ndev->max_cq_num);

	ret = nvme_identify_ctrl(ndev->fd, &ndev->id_ctrl);
	if (ret < 0)
		return ret;

	ndev->vid = le16_to_cpu(ndev->id_ctrl.vid);
	nvme_display_id_ctrl(&ndev->id_ctrl);
	return 0;
}

static int nvme_init_stage2(struct nvme_dev_info *ndev)
{
	int ret;

	ret = nvme_disable_controller_complete(ndev->fd);
	if (ret < 0)
		return ret;

	ret = nvme_create_aq_pair(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE);
	if (ret < 0)
		return ret;

	/* the number of irq equals to (ACQ + IOCQ) */
	ret = nvme_set_irq(ndev->fd, NVME_INT_MSIX, ndev->max_cq_num + 1);
	if (ret < 0)
		return ret;
	ndev->irq_type = NVME_INT_MSIX;
	ndev->nr_irq = ndev->max_cq_num + 1;

	ret = nvme_enable_controller(ndev->fd);
	if (ret < 0)
		return ret;

	ret = init_ctrl_property(ndev->fd, &ndev->prop);
	if (ret < 0)
		return ret;

	ret = nvme_init_ioq_info(ndev);
	if (ret < 0)
		return ret;

	ret = init_ns_data(ndev);
	if (ret < 0)
		goto out;

	ret = init_capability(ndev);
	if (ret < 0)
		goto out2;

	ret = check_link_status(ndev);
	if (ret < 0)
		goto out2;

	return 0;
out2:
	deinit_ns_data(ndev);
out:
	nvme_deinit_ioq_info(ndev);
	return ret;
}

struct nvme_dev_info *nvme_init(const char *devpath)
{
	struct nvme_dev_info *ndev;
	int ret;

	ndev = malloc(sizeof(*ndev));
	if (!ndev) {
		pr_err("failed to alloc nvme_dev_info!\n");
		return NULL;
	}
	memset(ndev, 0, sizeof(*ndev));

	ndev->fd = open(devpath, O_RDWR);
	if (ndev->fd < 0) {
		pr_err("failed to open %s: %s!\n", devpath, strerror(errno));
		goto out;
	}
	pr_info("open %s ok!\n", devpath);

	ret = nvme_init_stage1(ndev);
	if (ret < 0)
		goto out;

	ret = nvme_init_stage2(ndev);
	if (ret < 0)
		goto out;

	/* default entry size is set by driver */
	ndev->io_sqes = NVME_NVM_IOSQES;
	ndev->io_cqes = NVME_NVM_IOCQES;
	return ndev;
out:
	free(ndev);
	return NULL;
}

void nvme_deinit(struct nvme_dev_info *ndev)
{
	deinit_ns_data(ndev);
	nvme_deinit_ioq_info(ndev);

	close(ndev->fd);
	free(ndev);
}

int nvme_reinit(struct nvme_dev_info *ndev, uint32_t asqsz, uint32_t acqsz, 
	enum nvme_irq_type type)
{
	uint16_t nr_cq = ndev->max_cq_num + 1; /* ACQ + IOCQ */
	uint16_t nr_irq;
	int ret;

	ret = nvme_disable_controller_complete(ndev->fd);
	if (ret < 0)
		return ret;

	ret = nvme_create_aq_pair(ndev, asqsz, acqsz);
	if (ret < 0)
		return ret;

	if (type == NVME_INT_PIN || type == NVME_INT_MSI_SINGLE) {
		nr_irq = 1;
	} else if (type == NVME_INT_MSI_MULTI) {
		/* !TODO: It's better to check irq limit by parsing MSI Cap */
		nr_irq = nr_cq > 32 ? 32 : nr_cq;
	} else if (type == NVME_INT_MSIX) {
		/* !TODO: It's better to check irq limit by parsing MSI-X Cap */
		nr_irq = nr_cq > 2048 ? 2048 : nr_cq;
	}

	ret = nvme_set_irq(ndev->fd, type, nr_irq);
	if (ret < 0) {
		ndev->irq_type = NVME_INT_NONE;
		ndev->nr_irq = 0;
		return ret;
	}
	ndev->irq_type = type;
	ndev->nr_irq = nr_irq;

	ret = nvme_enable_controller(ndev->fd);
	if (ret < 0)
		return ret;

	return 0;
}
