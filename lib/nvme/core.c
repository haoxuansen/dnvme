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
	BUILD_BUG_ON(sizeof(struct nvme_verify_cmd) != 64);
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
	BUILD_BUG_ON(sizeof(struct nvme_id_ns_nvm) != 4096);
	BUILD_BUG_ON(sizeof(struct nvme_id_ns_zns) != 4096);
	BUILD_BUG_ON(sizeof(struct nvme_zns_lbafe) != 16);
	BUILD_BUG_ON(sizeof(struct nvme_id_ctrl_nvm) != 4096);
	BUILD_BUG_ON(sizeof(struct nvme_id_ctrl_zns) != 4096);
	BUILD_BUG_ON(sizeof(struct nvme_id_ctrl_csc) != 4096);

	/* Relate to nvme_admin_set_features command */
	/* Relate to nvme_admin_get_features command */
	BUILD_BUG_ON(sizeof(struct nvme_feat_lba_range_type) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_feat_auto_pst) != 256);
	BUILD_BUG_ON(sizeof(struct nvme_feat_hmb_attribute) != 4096);
	BUILD_BUG_ON(sizeof(struct nvme_feat_host_behavior) != 512);

	/* Relate to nvme_admin_directive_send command */
	/* Relate to nvme_admin_directive_recv command */
	BUILD_BUG_ON(sizeof(struct nvme_dir_identify_params) != 4096);
	BUILD_BUG_ON(sizeof(struct nvme_dir_streams_params) != 32);

	/* Relate to nvme_fabrics_type_connect command */
	BUILD_BUG_ON(sizeof(struct nvmf_connect_data) != 1024);

	/* Relate to nvme_cmd_resv_report command */
	BUILD_BUG_ON(sizeof(struct nvme_reg_ctrl) != 24);
	BUILD_BUG_ON(sizeof(struct nvme_resv_status) != 24);
	BUILD_BUG_ON(sizeof(struct nvme_reg_ctrl_extend) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_resv_status_extend) != 64);

	/* Relate to nvme_cmd_copy command */
	BUILD_BUG_ON(sizeof(struct nvme_copy_desc_fmt0) != 32);
	BUILD_BUG_ON(sizeof(struct nvme_copy_desc_fmt1) != 40);

	/* Relate to nvme_cmd_zone_mgmt_recv command */
	BUILD_BUG_ON(sizeof(struct nvme_zone_report) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_zone_descriptor) != 64);
}

int nvme_update_ns_instance(struct nvme_dev_info *ndev, 
		struct nvme_ns_instance *ns, enum nvme_event evt)
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

	if (ns->id_ns_nvm)
		ret = nvme_identify_cs_ns(ndev, ns->id_ns_nvm, 
			NVME_IDENTIFY_DATA_SIZE, ns->nsid, NVME_CSI_NVM);
	else if (ns->id_ns_zns)
		ret = nvme_identify_cs_ns(ndev, ns->id_ns_zns,
			NVME_IDENTIFY_DATA_SIZE, ns->nsid, NVME_CSI_ZNS);
	else
		return 0;

	if (ret < 0) {
		pr_err("failed to get identify ns(%u) data for csi!(%d)\n",
			ns->nsid, ret);
		return ret;
	}
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

	cid = CHK_EXPR_NUM_LT0_RTN(
		nvme_cmd_set_feat_num_queues(ndev->fd, iosq, iocq), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID), -EPERM);
	CHK_EXPR_NUM_NE_RTN(
		nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry)),
		1, -ETIME);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, NVME_SC_SUCCESS), 
		-EPERM);

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

	id_ctrl = zalloc(sizeof(*id_ctrl));
	if (!id_ctrl) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
	}

	CHK_EXPR_NUM_LT0_GOTO(
		nvme_identify_ctrl(ndev, id_ctrl), ret, -EPERM, out);

	nvme_display_id_ctrl(id_ctrl);

	ctrl->id_ctrl = id_ctrl;
	return 0;
out:
	free(id_ctrl);
	return ret;
}

static void deinit_identify_ctrl_data(struct nvme_ctrl_instance *ctrl)
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

	prop = zalloc(sizeof(*prop));
	if (!prop) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
	}

	CHK_EXPR_NUM_LT0_GOTO(
		nvme_read_ctrl_cap(fd, &prop->cap), ret, -EPERM, out);
	CHK_EXPR_NUM_LT0_GOTO(
		nvme_read_ctrl_vs(fd, &prop->vs), ret, -EPERM, out);
	CHK_EXPR_NUM_LT0_GOTO(
		nvme_read_ctrl_cc(fd, &prop->cc), ret, -EPERM, out);

	nvme_display_ctrl_property(prop);

	ctrl->prop = prop;
	return 0;
out:
	free(prop);
	return ret;
}

static void deinit_ctrl_property(struct nvme_ctrl_instance *ctrl)
{
	if (!ctrl->prop) {
		pr_warn("double free? ctrl property isn't exist!\n");
		return;
	}

	free(ctrl->prop);
	ctrl->prop = NULL;
}

static int init_id_cs_ctrl(struct nvme_dev_info *ndev, 
	struct nvme_ctrl_instance *ctrl)
{
	struct nvme_ctrl_property *prop = ctrl->prop;
	uint32_t cap_css = NVME_CAP_CSS(prop->cap);
	uint32_t cc_css;
	uint64_t vector;
	int ret;

	if (nvme_version(ctrl) < NVME_VS(2, 0, 0))
		return 0;

	CHK_EXPR_NUM_LT0_RTN(nvme_read_ctrl_cc(ndev->fd, &prop->cc), -EPERM);
	cc_css = NVME_CC_TO_CSS(prop->cc);

	if ((cap_css & NVME_CAP_CSS_CSI) && (cc_css == NVME_CC_CSS_CSI)) {
		ctrl->id_ctrl_csc = zalloc(sizeof(struct nvme_id_ctrl_csc));
		if (!ctrl->id_ctrl_csc) {
			pr_err("failed to alloc memory!\n");
			return -ENOMEM;
		}

		ret = nvme_identify_ctrl_csc_list(ndev, ctrl->id_ctrl_csc, 0xffff);
		if (ret < 0) {
			pr_err("failed to get I/O cmd set combination list!(%d)\n", ret);
			goto out;
		}

		CHK_EXPR_NUM_LT0_GOTO(
			nvme_get_feat_iocs_profile(ndev, NVME_FEAT_SEL_CUR),
			ret, -EPERM, out);

		vector = le64_to_cpu(ctrl->id_ctrl_csc->vector[ret]);
		if (vector & BIT(NVME_CSI_NVM)) {
			ctrl->id_ctrl_nvm = zalloc(sizeof(struct nvme_id_ctrl_nvm));
			if (!ctrl->id_ctrl_nvm) {
				pr_err("failed to alloc memory!\n");
				ret = -ENOMEM;
				goto out;
			}

			ret = nvme_identify_cs_ctrl(ndev, ctrl->id_ctrl_nvm, 
				sizeof(struct nvme_id_ctrl_nvm), NVME_CSI_NVM);
			if (ret < 0) {
				pr_err("failed to get NVM cmd set identify "
					"ctrl data!(%d)\n", ret);
				goto out;
			}
		}
		if (vector & BIT(NVME_CSI_ZNS)) {
			ctrl->id_ctrl_zns = zalloc(sizeof(struct nvme_id_ctrl_zns));
			if (!ctrl->id_ctrl_zns) {
				pr_err("failed to alloc memory!\n");
				ret = -ENOMEM;
				goto out;
			}

			ret = nvme_identify_cs_ctrl(ndev, ctrl->id_ctrl_zns, 
				sizeof(struct nvme_id_ctrl_zns), NVME_CSI_ZNS);
			if (ret < 0) {
				pr_err("failed to get ZNS cmd set identify "
					"ctrl data!(%d)\n", ret);
				goto out;
			}
		}

	} 

	return 0;
out:
	if (ctrl->id_ctrl_zns) {
		free(ctrl->id_ctrl_zns);
		ctrl->id_ctrl_zns = NULL;
	}
	if (ctrl->id_ctrl_nvm) {
		free(ctrl->id_ctrl_nvm);
		ctrl->id_ctrl_nvm = NULL;
	}
	if (ctrl->id_ctrl_csc) {
		free(ctrl->id_ctrl_csc);
		ctrl->id_ctrl_csc = NULL;
	}
	return ret;
}

static void deinit_id_cs_ctrl(struct nvme_ctrl_instance *ctrl)
{
	if (ctrl->id_ctrl_zns) {
		free(ctrl->id_ctrl_zns);
		ctrl->id_ctrl_zns = NULL;
	}
	if (ctrl->id_ctrl_nvm) {
		free(ctrl->id_ctrl_nvm);
		ctrl->id_ctrl_nvm = NULL;
	}
	if (ctrl->id_ctrl_csc) {
		free(ctrl->id_ctrl_csc);
		ctrl->id_ctrl_csc = NULL;
	}
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

		ctrl = zalloc(sizeof(*ctrl));
		if (!ctrl) {
			pr_err("failed to alloc ctrl instance!\n");
			return -ENOMEM;
		}

		CHK_EXPR_NUM_LT0_GOTO(
			request_io_queue_num(ndev, ctrl),
			ret, -EPERM, stage1_free_ctrl);
		CHK_EXPR_NUM_LT0_GOTO(
			init_identify_ctrl_data(ndev, ctrl),
			ret, -EPERM, stage1_free_ctrl);

		ndev->ctrl = ctrl;
		break;

	case NVME_INIT_STAGE2:
		if (!ndev->ctrl) {
			pr_err("ctrl instance isn't exist!\n");
			return -EFAULT;
		}

		CHK_EXPR_NUM_LT0_RTN(
			init_ctrl_property(ndev, ndev->ctrl), -EPERM);
		CHK_EXPR_NUM_LT0_GOTO(
			init_id_cs_ctrl(ndev, ndev->ctrl), ret, -EPERM, 
			stage2_exit_ctrl_prop);
		break;

	default:
		pr_err("stage(%u) is unknown!\n", stage);
		return -EINVAL;
	}

	return 0;

stage1_free_ctrl:
	free(ctrl);
	return ret;

stage2_exit_ctrl_prop:
	deinit_ctrl_property(ndev->ctrl);
	return ret;
}

static void deinit_ctrl_instance(struct nvme_dev_info *ndev, int stage)
{
	if (!ndev->ctrl) {
		pr_warn("double free? ctrl instance isn't exist!\n");
		return;
	}

	switch (stage) {
	case NVME_INIT_STAGE1:
		deinit_identify_ctrl_data(ndev->ctrl);
		free(ndev->ctrl);
		ndev->ctrl = NULL;
		break;

	case NVME_INIT_STAGE2:
		deinit_id_cs_ctrl(ndev->ctrl);
		deinit_ctrl_property(ndev->ctrl);
		break;

	default:
		pr_warn("stage(%u) is unknown! ignored\n", stage);
	}
}

static int init_id_ns(struct nvme_dev_info *ndev, struct nvme_ns_instance *ns)
{
	struct nvme_id_ns *id_ns;
	int ret;

	id_ns = zalloc(sizeof(*id_ns));
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

static void deinit_id_ns(struct nvme_ns_instance *ns)
{
	if (ns->id_ns) {
		free(ns->id_ns);
		ns->id_ns = NULL;
	}
}

static int parse_ns_id_desc(struct nvme_ns_instance *ns, void *data, uint32_t size)
{
	struct nvme_ns_id_desc *desc;
	void *list = data;
	int remain = size;
	uint8_t len;
	int ret = -EINVAL;

	for (desc = list; desc->nidt != 0; desc = list) {
		switch (desc->nidt) {
		case NVME_NIDT_EUI64:
			len = NVME_NIDT_EUI64_LEN;
			break;
		case NVME_NIDT_NGUID:
			len = NVME_NIDT_NGUID_LEN;
			break;
		case NVME_NIDT_UUID:
			len = NVME_NIDT_UUID_LEN;
			break;
		case NVME_NIDT_CSI:
			len = NVME_NIDT_CSI_LEN;
			break;
		default:
			pr_err("namespace identifer type(%u) is invalid!\n", 
				desc->nidt);
			goto out;
		}

		if (desc->nidl != len) {
			pr_err("NIDL shall be %u for NIDT(%u), but actually is %u!\n",
				len, desc->nidt, desc->nidl);
			goto out;
		}

		if (ns->ns_id_desc[desc->nidt]) {
			pr_err("multiple desc with the same namespace "
				"identifier type(%u)!\n", desc->nidt);
			goto out;
		}
		ns->ns_id_desc[desc->nidt] = desc;
		list += sizeof(struct nvme_ns_id_desc) + len;
		remain -= (sizeof(struct nvme_ns_id_desc) + len);

		if (remain <= sizeof(struct nvme_ns_id_desc)) {
			pr_warn("remain size is too small! skip ...\n");
			break;
		}
	}
	ns->ns_id_desc_raw = data;
	return 0;
out:
	dump_data_to_console(data, size, "namespace identifier desc list");
	memset(ns->ns_id_desc, 0, sizeof(ns->ns_id_desc));
	return ret;
}

static int init_ns_id_desc(struct nvme_dev_info *ndev, 
	struct nvme_ns_instance *ns)
{
	void *data;
	int ret;

	data = zalloc(NVME_IDENTIFY_DATA_SIZE);
	if (!data) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
	}

	ret = nvme_identify_ns_desc_list(ndev, data, NVME_IDENTIFY_DATA_SIZE,
		ns->nsid);
	if (ret < 0) {
		pr_err("failed to get ns desc list!(%d)\n", ret);
		goto free_data;
	}

	ret = parse_ns_id_desc(ns, data, NVME_IDENTIFY_DATA_SIZE);
	if (ret < 0)
		goto free_data;

	return 0;
free_data:
	free(data);
	return ret;
}

static void deinit_ns_id_desc(struct nvme_ns_instance *ns)
{
	if (ns->ns_id_desc_raw) {
		memset(ns->ns_id_desc, 0, sizeof(ns->ns_id_desc));
		free(ns->ns_id_desc_raw);
		ns->ns_id_desc_raw = NULL;
	}
}

static int init_id_cs_ns(struct nvme_dev_info *ndev, 
	struct nvme_ns_instance *ns)
{
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	void *data;
	uint8_t csi;
	int ret;

	if (nvme_version(ctrl) < NVME_VS(2, 0, 0))
		return 0;

	if (!ns->ns_id_desc[NVME_NIDT_CSI]) {
		pr_warn("CSI descriptor don't exist! skip...\n");
		return 0;
	}
	csi = ns->ns_id_desc[NVME_NIDT_CSI]->nid[0];

	data = zalloc(NVME_IDENTIFY_DATA_SIZE);
	if (!data) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
	}

	ret = nvme_identify_cs_ns(ndev, data, NVME_IDENTIFY_DATA_SIZE, 
		ns->nsid, csi);
	if (ret < 0) {
		pr_err("failed to get identify ns(%u) data for csi(%u)!(%d)\n",
			ns->nsid, csi, ret);
		goto free_data;
	}

	switch (csi) {
	case NVME_CSI_NVM:
		ns->id_ns_nvm = data;
		break;
	case NVME_CSI_ZNS:
		ns->id_ns_zns = data;
		break;
	default:
		pr_warn("csi(%u) is unknown!\n", csi);
		goto free_data;
	}

	return 0;
free_data:
	free(data);
	return ret;
}

static void deinit_id_cs_ns(struct nvme_ns_instance *ns)
{
	if (ns->id_ns_nvm) {
		free(ns->id_ns_nvm);
		ns->id_ns_nvm = NULL;
	}
	if (ns->id_ns_zns) {
		free(ns->id_ns_zns);
		ns->id_ns_zns = NULL;
	}
}

static int init_ns_instance(struct nvme_dev_info *ndev, 
	struct nvme_ns_instance *ns)
{
	int ret;

	ret = init_id_ns(ndev, ns);
	if (ret < 0)
		return ret;

	ret = init_ns_id_desc(ndev, ns);
	if (ret < 0)
		goto exit_id_ns;

	ret = init_id_cs_ns(ndev, ns);
	if (ret < 0)
		goto exit_ns_id_desc;

	return 0;
exit_ns_id_desc:
	deinit_ns_id_desc(ns);
exit_id_ns:
	deinit_id_ns(ns);
	return ret;
}

static void deinit_ns_instance(struct nvme_ns_instance *ns)
{
	deinit_id_cs_ns(ns);
	deinit_ns_id_desc(ns);
	deinit_id_ns(ns);
}

static int init_ns_group(struct nvme_dev_info *ndev)
{
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_group *ns_grp;
	uint32_t nn;
	int i;
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

	ns_grp = zalloc(sizeof(struct nvme_ns_group) + 
			nn * sizeof(struct nvme_ns_instance));
	if (!ns_grp) {
		pr_err("failed to alloc ns group!\n");
		return -ENOMEM;
	}
	ns_grp->nr_ns = nn;

	ns_list = zalloc(NVME_IDENTIFY_DATA_SIZE);
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
			goto exit_ns_instance;

		ns_grp->nr_act++;
	}

	BUG_ON(!ns_grp->nr_act); /* no active namespace? */

	ns_grp->act_list = ns_list;
	ndev->ns_grp = ns_grp;
	return 0;

exit_ns_instance:
	for (i--; i >= 0; i--) {
		nsid = le32_to_cpu(ns_list[i]);
		deinit_ns_instance(&ns_grp->ns[nsid - 1]);
	}
free_ns_list:
	free(ns_list);
free_ns_group:
	free(ns_grp);
	return ret;
}

static void deinit_ns_group(struct nvme_dev_info *ndev)
{
	struct nvme_ns_group *ns_grp;
	uint32_t i;	

	if (!ndev->ns_grp) {
		pr_warn("double free? ns group isn't exist!\n");
		return;
	}
	ns_grp = ndev->ns_grp;

	for (i = 0; i < ns_grp->nr_ns; i++)
		deinit_ns_instance(&ns_grp->ns[i]);

	free(ns_grp->act_list);
	ns_grp->act_list = NULL;
	free(ns_grp);
	ndev->ns_grp = NULL;
}

static int init_pci_dev_instance(struct nvme_dev_info *ndev)
{
	struct pci_dev_instance *pdev = NULL;
	uint16_t link_sts;
	int fd = ndev->fd;
	int ret;

	pdev = zalloc(sizeof(*pdev));
	if (!pdev) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
	}

	ret = nvme_get_pci_bdf(fd, &pdev->bdf);
	if (ret < 0)
		goto free_pdev;

	ret = pci_hd_read_vendor_id(fd, &pdev->vendor_id);
	ret |= pci_hd_read_device_id(fd, &pdev->device_id);

	ret |= nvme_get_pci_capability(fd, PCI_CAP_ID_MSI, 
					&pdev->msi, sizeof(pdev->msi));
	ret |= nvme_get_pci_capability(fd, PCI_CAP_ID_MSIX,
					&pdev->msix, sizeof(pdev->msix));
	ret |= nvme_get_pci_capability(fd, PCI_CAP_ID_PM,
					&pdev->pm, sizeof(pdev->pm));
	ret |= nvme_get_pci_capability(fd, PCI_CAP_ID_EXP,
					&pdev->express, sizeof(pdev->express));
	nvme_get_pcie_capability(fd, PCI_EXT_CAP_ID_LTR, &pdev->ltr, 
		sizeof(pdev->ltr)); /* optional */
	nvme_get_pcie_capability(fd, PCI_EXT_CAP_ID_L1SS, &pdev->l1ss, 
		sizeof(pdev->l1ss)); /* optional */

	if (ret < 0)
		goto free_pdev;

	ret = pci_exp_read_link_status(fd, pdev->express.offset, &link_sts);
	if (ret < 0) {
		pr_err("failed to read link status reg!(%d)\n", ret);
		goto free_pdev;
	}
	pdev->link_speed = link_sts & PCI_EXP_LNKSTA_CLS;
	pdev->link_width = (link_sts & PCI_EXP_LNKSTA_NLW) >> 
		PCI_EXP_LNKSTA_NLW_SHIFT;

	pr_info("Current Link Speed: Gen%u, Width: x%u\n", 
		pdev->link_speed, pdev->link_width);

	pdev->msi_cap_vectors = 1 << ((pdev->msi.mc & PCI_MSI_FLAGS_QMASK) >> 1);
	pdev->msix_cap_vectors = (pdev->msix.mc & PCI_MSIX_FLAGS_QSIZE) + 1;

	pr_info("MSI support %u vectors, MSI-X support %u vectors\n", 
		pdev->msi_cap_vectors, pdev->msix_cap_vectors);

	ndev->pdev = pdev;
	return 0;

free_pdev:
	free(pdev);
	return ret;
}

static void deinit_pci_dev_instance(struct nvme_dev_info *ndev)
{
	if (!ndev->pdev) {
		pr_warn("double free? pci device isn't exist!\n");
		return;
	}

	free(ndev->pdev);
	ndev->pdev = NULL;
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

	CHK_EXPR_NUM_LT0_RTN(
		nvme_disable_controller_complete(ndev->fd), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_get_dev_info(ndev->fd, &ndev->dev_pub), -EPERM);
	ndev->sock_fd = CHK_EXPR_NUM_LT0_RTN(nvme_gnl_connect(), -EPERM);

	CHK_EXPR_NUM_LT0_GOTO(
		nvme_create_aq_pair(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE),
		ret, -EPERM, out_gnl_disconnect);
	CHK_EXPR_NUM_LT0_GOTO(
		nvme_set_irq(ndev->fd, NVME_INT_PIN, 1), 
		ret, -EPERM, out_gnl_disconnect);

	ndev->irq_type = NVME_INT_PIN;
	ndev->nr_irq = 1;

	CHK_EXPR_NUM_LT0_GOTO(
		nvme_enable_controller(ndev->fd),
		ret, -EPERM, out_gnl_disconnect);
	CHK_EXPR_NUM_LT0_GOTO(
		init_ctrl_instance(ndev, NVME_INIT_STAGE1),
		ret, -EPERM, out_gnl_disconnect);
	return 0;

out_gnl_disconnect:
	nvme_gnl_disconnect(ndev->sock_fd);
	ndev->sock_fd = -1;
	return ret;
}

static void nvme_release_stage1(struct nvme_dev_info *ndev)
{
	deinit_ctrl_instance(ndev, NVME_INIT_STAGE1);
	nvme_gnl_disconnect(ndev->sock_fd);
	ndev->sock_fd = -1;
}

static int nvme_init_stage2(struct nvme_dev_info *ndev)
{
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct pci_dev_instance *pdev;
	int ret;

	CHK_EXPR_NUM_LT0_RTN(
		nvme_disable_controller_complete(ndev->fd), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_create_aq_pair(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE),
		-EPERM);
	CHK_EXPR_NUM_LT0_RTN(init_pci_dev_instance(ndev), -EPERM);
	pdev = ndev->pdev;

	ndev->irq_type = NVME_INT_MSIX;
	if (ctrl->nr_cq + 1 > pdev->msix_cap_vectors)
		ndev->nr_irq = pdev->msix_cap_vectors;
	else
		ndev->nr_irq = ctrl->nr_cq + 1;

	CHK_EXPR_NUM_LT0_GOTO(
		nvme_set_irq(ndev->fd, ndev->irq_type, ndev->nr_irq), 
		ret, -EPERM, exit_pci_dev_instance);

	CHK_EXPR_NUM_LT0_GOTO(nvme_enable_controller(ndev->fd),
		ret, -EPERM, exit_pci_dev_instance);
	CHK_EXPR_NUM_LT0_GOTO(init_ctrl_instance(ndev, NVME_INIT_STAGE2),
		ret, -EPERM, exit_pci_dev_instance);
	CHK_EXPR_NUM_LT0_GOTO(nvme_init_ioq_info(ndev), 
		ret, -EPERM, exit_ctrl_instance);
	CHK_EXPR_NUM_LT0_GOTO(init_ns_group(ndev), 
		ret, -EPERM, exit_ioq);

	return 0;
exit_ioq:
	nvme_deinit_ioq_info(ndev);
exit_ctrl_instance:
	deinit_ctrl_instance(ndev, NVME_INIT_STAGE2);
exit_pci_dev_instance:
	deinit_pci_dev_instance(ndev);
	return ret;
}

static void nvme_release_stage2(struct nvme_dev_info *ndev)
{
	deinit_ns_group(ndev);
	nvme_deinit_ioq_info(ndev);
	deinit_ctrl_instance(ndev, NVME_INIT_STAGE2);
	deinit_pci_dev_instance(ndev);
}

struct nvme_dev_info *nvme_init(const char *devpath)
{
	struct nvme_dev_info *ndev;
	int ret;

	_nvme_check_size();

	ndev = zalloc(sizeof(*ndev));
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

	CHK_EXPR_NUM_LT0_RTN(
		nvme_disable_controller_complete(ndev->fd), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_create_aq_pair(ndev, asqsz, acqsz), -EPERM);

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
		pr_err("failed to set irq!(%d)\n", ret);
		ndev->irq_type = NVME_INT_NONE;
		ndev->nr_irq = 0;
		return ret;
	}
	ndev->irq_type = type;
	ndev->nr_irq = nr_irq;

	CHK_EXPR_NUM_LT0_RTN(
		nvme_enable_controller(ndev->fd), -EPERM);

	return 0;
}
