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

#include "byteorder.h"
#include "build_bug.h"
#include "nvme.h"
#include "libbase.h"
#include "libnvme.h"
#include "debug.h"

enum {
	NVME_INIT_STAGE1,
	NVME_INIT_STAGE2,
};

static inline void _nvme_check_size(void)
{
	BUILD_BUG_ON(sizeof(struct nvme_common_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_delete_queue) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_create_sq) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_get_log_page_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_create_cq) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_identify) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_abort_cmd) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_features) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_download_firmware) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_directive_cmd) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_dbbuf) != 64);
	BUILD_BUG_ON(sizeof(struct nvmf_common_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvmf_property_set_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvmf_connect_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvmf_property_get_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_format_cmd) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_rw_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_write_zeroes_cmd) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_dsm_cmd) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_copy_cmd) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_zone_mgmt_send_cmd) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_zone_mgmt_recv_cmd) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_command) != 64);

	BUILD_BUG_ON(sizeof(struct nvme_completion) != 16);

	/* Relate to nvme_admin_get_log_page command */
	BUILD_BUG_ON(sizeof(struct nvme_smart_log) != 512);
	BUILD_BUG_ON(sizeof(struct nvme_fw_slot_info_log) != 512);
	BUILD_BUG_ON(sizeof(struct nvme_effects_log) != 4096);
	BUILD_BUG_ON(sizeof(struct nvme_ana_rsp_hdr) != 16);
	BUILD_BUG_ON(sizeof(struct nvme_ana_group_desc) != 32);
	BUILD_BUG_ON(sizeof(struct nvmf_disc_rsp_page_hdr) != 1024);
	BUILD_BUG_ON(sizeof(struct nvmf_disc_rsp_page_entry) != 1024);

	/* Relate to nvme_admin_identify command */
	BUILD_BUG_ON(sizeof(struct nvme_id_ns) != 4096);
	BUILD_BUG_ON(sizeof(struct nvme_id_ctrl) != 4096);
	BUILD_BUG_ON(sizeof(struct nvme_id_power_state) != 32);
	BUILD_BUG_ON(sizeof(struct nvme_id_ns_zns) != 4096);
	BUILD_BUG_ON(sizeof(struct nvme_zns_lbafe) != 16);
	BUILD_BUG_ON(sizeof(struct nvme_id_ctrl_zns) != 4096);

	/* Relate to nvme_admin_set_features command */
	/* Relate to nvme_admin_get_features command */
	BUILD_BUG_ON(sizeof(struct nvme_feat_auto_pst) != 256);
	BUILD_BUG_ON(sizeof(struct nvme_feat_host_behavior) != 512);

	/* Relate to nvme_admin_directive_send command */
	/* Relate to nvme_admin_directive_recv command */
	BUILD_BUG_ON(sizeof(struct nvme_dir_identify_params) != 4096);
	BUILD_BUG_ON(sizeof(struct nvme_dir_streams_params) != 32);

	/* Relate to nvme_fabrics_type_connect command */
	BUILD_BUG_ON(sizeof(struct nvmf_connect_data) != 1024);

	/* Relate to nvme_cmd_copy command */
	BUILD_BUG_ON(sizeof(struct nvme_copy_desc_fmt0) != 32);
	BUILD_BUG_ON(sizeof(struct nvme_copy_desc_fmt1) != 40);

	/* Relate to nvme_cmd_zone_mgmt_recv command */
	BUILD_BUG_ON(sizeof(struct nvme_zone_report) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_zone_descriptor) != 64);
}

void nvme_fill_data(void *buf, uint32_t size)
{
	uint32_t i;

	for (i = 0; i < size; i++) {
		*(uint8_t *)(buf + i) = (uint8_t)rand();
	}
}

int nvme_update_ns_info(struct nvme_dev_info *ndev, struct nvme_ns_instance *ns)
{
	struct nvme_id_ns *id_ns = ns->id_ns;
	int ret;

	ret = nvme_identify_ns_active(ndev, id_ns, ns->nsid);
	if (ret < 0) {
		pr_err("failed to get ns(%u) data!(%d)\n", ns->nsid, ret);
		return ret;
	}

	ns->fmt_idx = NVME_NS_FLBAS_LBA(id_ns->flbas);
	ns->blk_size = 1 << id_ns->lbaf[ns->fmt_idx].ds;
	ns->meta_size = le16_to_cpu(id_ns->lbaf[ns->fmt_idx].ms);
	return 0;
}

/**
 * @brief Request the max number of IOSQ & IOCQ from NVMe controller
 *
 * @return 0 on success, otherwise a negative errno
 */
static int request_io_queue_num(struct nvme_dev_info *ndev, 
	struct nvme_ctrl_instance *ctrl)
{
	struct nvme_completion entry = {0};
	/*
	 * If the value specified is 65535, the controller should abort
	 * the command with a status code of Invalid Field in Command
	 */
	uint16_t iosq = 65534;
	uint16_t iocq = 65534;
	uint16_t cid;
	int ret;

	ret = nvme_cmd_set_feat_num_queues(ndev->fd, iosq, iocq);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, NVME_SC_SUCCESS);
	if (ret < 0)
		return ret;

	ctrl->nr_sq = (le32_to_cpu(entry.result.u32) & 0xffff) + 1;
	ctrl->nr_cq = (le32_to_cpu(entry.result.u32) >> 16) + 1;

	pr_info("Controller has allocated %u IOSQ, %u IOCQ!\n", 
		ctrl->nr_sq, ctrl->nr_cq);
	return 0;
}

static int init_identify_ctrl_data(struct nvme_dev_info *ndev,
	struct nvme_ctrl_instance *ctrl)
{
	struct nvme_id_ctrl *id_ctrl;
	int ret;

	id_ctrl = calloc(1, sizeof(*id_ctrl));
	if (!id_ctrl) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
	}

	ret = nvme_identify_ctrl(ndev, id_ctrl);
	if (ret < 0)
		goto out;

	nvme_display_id_ctrl(id_ctrl);

	ctrl->id_ctrl = id_ctrl;
	return 0;
out:
	free(id_ctrl);
	return ret;
}

static void release_identify_ctrl_data(struct nvme_ctrl_instance *ctrl)
{
	if (!ctrl->id_ctrl) {
		pr_warn("double free? identify ctrl data isn't exist!\n");
		return;
	}

	free(ctrl->id_ctrl);
	ctrl->id_ctrl = NULL;
}

static int init_ctrl_property(struct nvme_dev_info *ndev, 
	struct nvme_ctrl_instance *ctrl)
{
	struct nvme_ctrl_property *prop;
	int fd = ndev->fd;
	int ret;

	prop = calloc(1, sizeof(*prop));
	if (!prop) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
	}

	ret = nvme_read_ctrl_cap(fd, &prop->cap);
	if (ret < 0)
		goto out;

	ret = nvme_read_ctrl_vs(fd, &prop->vs);
	if (ret < 0)
		goto out;

	ret = nvme_read_ctrl_cc(fd, &prop->cc);
	if (ret < 0)
		goto out;

	nvme_display_ctrl_property(prop);

	ctrl->prop = prop;
	return 0;
out:
	free(prop);
	return ret;
}

static void release_ctrl_property(struct nvme_ctrl_instance *ctrl)
{
	if (!ctrl->prop) {
		pr_warn("double free? ctrl property isn't exist!\n");
		return;
	}

	free(ctrl->prop);
	ctrl->prop = NULL;
}

static int init_ctrl_instance(struct nvme_dev_info *ndev, int stage)
{
	struct nvme_ctrl_instance *ctrl;
	int ret;

	switch (stage) {
	case NVME_INIT_STAGE1:
		if (ndev->ctrl) {
			pr_err("ctrl instance is already exist!\n");
			return -EFAULT;
		}

		ctrl = calloc(1, sizeof(*ctrl));
		if (!ctrl) {
			pr_err("failed to alloc ctrl instance!\n");
			return -ENOMEM;
		}

		ret = request_io_queue_num(ndev, ctrl);
		if (ret < 0)
			goto stage1_free_ctrl;

		ret = init_identify_ctrl_data(ndev, ctrl);
		if (ret < 0)
			goto stage1_free_ctrl;

		ndev->ctrl = ctrl;
		break;

	case NVME_INIT_STAGE2:
		if (!ndev->ctrl) {
			pr_err("ctrl instance isn't exist!\n");
			return -EFAULT;
		}

		ret = init_ctrl_property(ndev, ndev->ctrl);
		if (ret < 0)
			return ret;
		break;

	default:
		pr_err("stage(%u) is unknown!\n", stage);
		return -EINVAL;
	}

	return 0;

stage1_free_ctrl:
	free(ctrl);
	return ret;
}

static void release_ctrl_instance(struct nvme_dev_info *ndev, int stage)
{
	if (!ndev->ctrl) {
		pr_warn("double free? ctrl instance isn't exist!\n");
		return;
	}

	switch (stage) {
	case NVME_INIT_STAGE1:
		release_identify_ctrl_data(ndev->ctrl);
		free(ndev->ctrl);
		ndev->ctrl = NULL;
		break;

	case NVME_INIT_STAGE2:
		release_ctrl_property(ndev->ctrl);
		break;

	default:
		pr_warn("stage(%u) is unknown! ignored\n", stage);
	}
}

static int init_ns_instance(struct nvme_dev_info *ndev, 
	struct nvme_ns_instance *ns)
{
	struct nvme_id_ns *id_ns;
	int ret;

	id_ns = calloc(1, sizeof(*id_ns));
	if (!id_ns) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
	}

	ret = nvme_identify_ns_active(ndev, id_ns, ns->nsid);
	if (ret < 0) {
		pr_err("failed to get ns(%u) data!(%d)\n", ns->nsid, ret);
		goto free_id_ns;
	}
	nvme_display_id_ns(id_ns, ns->nsid);

	ns->id_ns = id_ns;
	ns->fmt_idx = NVME_NS_FLBAS_LBA(id_ns->flbas);
	ns->blk_size = 1 << id_ns->lbaf[ns->fmt_idx].ds;
	ns->meta_size = le16_to_cpu(id_ns->lbaf[ns->fmt_idx].ms);

	BUG_ON(ns->blk_size < 512);
	return 0;
free_id_ns:
	free(id_ns);
	return ret;
}

static void release_ns_instance(struct nvme_ns_instance *ns)
{
	if (ns->id_ns) {
		free(ns->id_ns);
		ns->id_ns = NULL;
	}
}

static int init_ns_group(struct nvme_dev_info *ndev)
{
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_group *ns_grp;
	uint32_t nn;
	uint32_t i;
	uint32_t nsid;
	uint32_t last = 0;
	__le32 *ns_list;
	int ret;

	ret = nvme_id_ctrl_nn(ctrl, &nn);
	if (ret < 0)
		return ret;

	if (nn > 1024) {
		pr_warn("Support %u namespace? limit it to 1024!\n", nn);
		nn = 1024;
	}

	ns_grp = calloc(1, sizeof(struct nvme_ns_group) + 
			nn * sizeof(struct nvme_ns_instance));
	if (!ns_grp) {
		pr_err("failed to alloc ns group!\n");
		return -ENOMEM;
	}
	ns_grp->nr_ns = nn;

	ns_list = calloc(1, NVME_IDENTIFY_DATA_SIZE);
	if (!ns_list) {
		pr_err("failed to alloc ns list!\n");
		goto free_ns_group;
	}

	ret = nvme_identify_ns_list_active(ndev, ns_list, 
		NVME_IDENTIFY_DATA_SIZE, 0);
	if (ret < 0) {
		pr_err("failed to get active ns list!(%d)\n", ret);
		goto free_ns_list;
	}

	for (i = 0; i < nn && ns_list[i] != 0; i++) {

		nsid = le32_to_cpu(ns_list[i]);
		if (nsid <= last) {
			pr_err("NSID in list shall increase in order!\n");
			goto free_ns_list;
		}
		last = nsid;

		if (nsid > nn) {
			pr_warn("ignore the remaining namespaces which id >= %u!\n",
				nsid);
			break;
		}
		/* nsid start at 1 (zero is invalid) */
		ns_grp->ns[nsid - 1].nsid = nsid;

		ret = init_ns_instance(ndev, &ns_grp->ns[nsid - 1]);
		if (ret < 0)
			goto rel_ns_instance;

		ns_grp->nr_act++;
	}

	BUG_ON(!ns_grp->nr_act); /* no active namespace? */

	ns_grp->act_list = ns_list;
	ndev->ns_grp = ns_grp;
	return 0;

rel_ns_instance:
	for (i--; i > 0; i--) {
		nsid = le32_to_cpu(ns_list[i]);
		release_ns_instance(&ns_grp->ns[nsid - 1]);
	}
free_ns_list:
	free(ns_list);
free_ns_group:
	free(ns_grp);
	return ret;
}

static void release_ns_group(struct nvme_dev_info *ndev)
{
	struct nvme_ns_group *ns_grp;
	uint32_t i;	

	if (!ndev->ns_grp) {
		pr_warn("double free? ns group isn't exist!\n");
		return;
	}
	ns_grp = ndev->ns_grp;

	for (i = 0; i < ns_grp->nr_ns; i++)
		release_ns_instance(&ns_grp->ns[i]);

	free(ns_grp->act_list);
	ns_grp->act_list = NULL;
	free(ns_grp);
	ndev->ns_grp = NULL;
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
	int ret;

	ret = nvme_disable_controller_complete(ndev->fd);
	if (ret < 0)
		return ret;

	ret = nvme_get_dev_info(ndev->fd, &ndev->dev_pub);
	if (ret < 0)
		return ret;

	ret = nvme_gnl_connect();
	if (ret < 0)
		return ret;
	ndev->sock_fd = ret;

	ret = nvme_create_aq_pair(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE);
	if (ret < 0)
		goto out_gnl_disconnect;

	ret = nvme_set_irq(ndev->fd, NVME_INT_PIN, 1);
	if (ret < 0)
		goto out_gnl_disconnect;
	ndev->irq_type = NVME_INT_PIN;
	ndev->nr_irq = 1;

	ret = nvme_enable_controller(ndev->fd);
	if (ret < 0)
		goto out_gnl_disconnect;

	ret = init_ctrl_instance(ndev, NVME_INIT_STAGE1);
	if (ret < 0)
		goto out_gnl_disconnect;

	return 0;

out_gnl_disconnect:
	nvme_gnl_disconnect(ndev->sock_fd);
	ndev->sock_fd = -1;
	return ret;
}

static void nvme_release_stage1(struct nvme_dev_info *ndev)
{
	release_ctrl_instance(ndev, NVME_INIT_STAGE1);
	nvme_gnl_disconnect(ndev->sock_fd);
	ndev->sock_fd = -1;
}

static int nvme_init_stage2(struct nvme_dev_info *ndev)
{
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	int ret;

	ret = nvme_disable_controller_complete(ndev->fd);
	if (ret < 0)
		return ret;

	ret = nvme_create_aq_pair(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE);
	if (ret < 0)
		return ret;

	/* the number of irq equals to (ACQ + IOCQ) */
	ret = nvme_set_irq(ndev->fd, NVME_INT_MSIX, ctrl->nr_cq + 1);
	if (ret < 0)
		return ret;
	ndev->irq_type = NVME_INT_MSIX;
	ndev->nr_irq = ctrl->nr_cq + 1;

	ret = nvme_enable_controller(ndev->fd);
	if (ret < 0)
		return ret;

	ret = init_ctrl_instance(ndev, NVME_INIT_STAGE2);
	if (ret < 0)
		return ret;

	ret = nvme_init_ioq_info(ndev);
	if (ret < 0)
		goto rel_ctrl_instance;

	ret = init_ns_group(ndev);
	if (ret < 0)
		goto rel_ioq;

	ret = init_capability(ndev);
	if (ret < 0)
		goto rel_ns_group;

	ret = check_link_status(ndev);
	if (ret < 0)
		goto rel_ns_group;

	return 0;
rel_ns_group:
	release_ns_group(ndev);
rel_ioq:
	nvme_deinit_ioq_info(ndev);
rel_ctrl_instance:
	release_ctrl_instance(ndev, NVME_INIT_STAGE2);
	return ret;
}

static void nvme_release_stage2(struct nvme_dev_info *ndev)
{
	release_ns_group(ndev);
	nvme_deinit_ioq_info(ndev);
	release_ctrl_instance(ndev, NVME_INIT_STAGE2);
}

struct nvme_dev_info *nvme_init(const char *devpath)
{
	struct nvme_dev_info *ndev;
	int ret;

	_nvme_check_size();

	ndev = calloc(1, sizeof(*ndev));
	if (!ndev) {
		pr_err("failed to alloc memory!\n");
		return NULL;
	}

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
	nvme_release_stage2(ndev);
	nvme_release_stage1(ndev);
	close(ndev->fd);
	free(ndev);
}

int nvme_reinit(struct nvme_dev_info *ndev, uint32_t asqsz, uint32_t acqsz, 
	enum nvme_irq_type type)
{
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	uint16_t nr_cq = ctrl->nr_cq + 1; /* ACQ + IOCQ */
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
