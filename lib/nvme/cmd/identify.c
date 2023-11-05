/**
 * @file identify.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-08-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"

/**
 * @brief Check whether the controller supports extended LBA formats.
 * 
 * @retval 1	support extended LBA formats.
 * @retval 0	don't support extended LBA formats.
 * @retval -ENODEV	identify controller data not exist. 
 */
int nvme_ctrl_support_ext_lba_formats(struct nvme_ctrl_instance *ctrl)
{
	uint32_t ctratt;

	if (!ctrl->id_ctrl)
		return -ENODEV;
	
	ctratt = le32_to_cpu(ctrl->id_ctrl->ctratt);

	return (ctratt & NVME_CTRL_CTRATT_ELBAS) ? 1 : 0;
}

/**
 * @brief Check whether the controller supports Copy command.
 * 
 * @return Details are listed below.
 * 	1: support the NVM Command Set Copy command.
 * 	0: don't support the NVM Command Set Copy command.
 * 	-ENODEV: identify controller data not exist. 
 */
int nvme_ctrl_support_copy_cmd(struct nvme_ctrl_instance *ctrl)
{
	uint16_t oncs;

	if (!ctrl->id_ctrl)
		return -ENODEV;

	oncs = le16_to_cpu(ctrl->id_ctrl->oncs);
	
	return (oncs & NVME_CTRL_ONCS_COPY) ? 1 : 0;
}

/**
 * @brief Check whether the controller supports Write Protect.
 * 
 * @return Details are listed below.
 * 	1: support the Write Protect.
 * 	0: don't support the Write Protect.
 * 	-ENODEV: identify controller data not exist. 
 */
int nvme_ctrl_support_write_protect(struct nvme_ctrl_instance *ctrl)
{
	if (!ctrl->id_ctrl)
		return -ENODEV;
	
	return (ctrl->id_ctrl->nwpc & NVME_CTRL_NWPC_WP) ? 1 : 0;
}

/**
 * @brief Check whether the controller supports SGLs.
 * 
 * @return Details are listed below.
 * 	NVME_CTRL_SGLS_SUPPORT4: support SGLs with 4B align.
 * 	NVME_CTRL_SGLS_SUPPORT: support SGLs.
 * 	NVME_CTRL_SGLS_UNSUPPORT: don't support SGLs.
 * 	-ENODEV: identify controller data not exist. 
 */
int nvme_ctrl_support_sgl(struct nvme_ctrl_instance *ctrl)
{
	if (!ctrl->id_ctrl)
		return -ENODEV;
	
	return le32_to_cpu(ctrl->id_ctrl->sgls) & NVME_CTRL_SGLS_MASK;
}

/**
 * @brief Check whether the controller supports Keyed SGL Data Block descriptor.
 * 
 * @return Details are listed below.
 * 	1: support Keyed SGL Data Block descriptor.
 * 	0: don't support Keyed SGL Data Block descriptor.
 * 	-ENODEV: identify controller data not exist. 
 */
int nvme_ctrl_support_keyed_sgl_data_block(struct nvme_ctrl_instance *ctrl)
{
	if (!ctrl->id_ctrl)
		return -ENODEV;
	
	return (le32_to_cpu(ctrl->id_ctrl->sgls) & 
		NVME_CTRL_SGLS_KEYED_DATA_BLOCK) ? 1 : 0;
}

/**
 * @brief Check whether the controller supports SGL Bit Bucket descriptor.
 * 
 * @return Details are listed below.
 * 	1: support SGL Bit Bucket descriptor.
 * 	0: don't support SGL Bit Bucket descriptor.
 * 	-ENODEV: identify controller data not exist. 
 */
int nvme_ctrl_support_sgl_bit_bucket(struct nvme_ctrl_instance *ctrl)
{
	if (!ctrl->id_ctrl)
		return -ENODEV;
	
	return (le32_to_cpu(ctrl->id_ctrl->sgls) & 
		NVME_CTRL_SGLS_BIT_BUCKET) ? 1 : 0;
}

/**
 * @brief Check whether the controller supports a MPTR containing an SGL descriptor
 * 
 * @retval 1	support a MPTR containing an SGL descriptor.
 * @retval 0	don't support a MPTR containing an SGL descriptor.
 * @retval -ENODEV	identify controller data not exist.
 */
int nvme_ctrl_support_sgl_mptr_sgl(struct nvme_ctrl_instance *ctrl)
{
	if (!ctrl->id_ctrl)
		return -ENODEV;

	return (le32_to_cpu(ctrl->id_ctrl->sgls) & 
		NVME_CTRL_SGLS_MPTR_SGL) ? 1 : 0;
}

/**
 * @brief Check whether the controller supports SGL Data Block descriptor.
 * 
 * @return Details are listed below.
 * 	1: support SGL Data Block descriptor.
 * 	0: don't support SGL Data Block descriptor.
 * 	-ENODEV: identify controller data not exist. 
 */
int nvme_ctrl_support_sgl_data_block(struct nvme_ctrl_instance *ctrl)
{
	if (!ctrl->id_ctrl)
		return -ENODEV;
	
	return (le32_to_cpu(ctrl->id_ctrl->sgls) & 
		NVME_CTRL_SGLS_DATA_BLOCK) ? 1 : 0;
}

/**
 * @brief Check whether the namespace supports ZNS Command Set.
 * 
 * @return Details are listed below.
 * 	1: support zns command set.
 * 	0: don't support zns command set.
 * 	-ENODEV: namespace identify descirptor not exist.
 */
int nvme_ns_support_zns_command_set(struct nvme_ns_instance *ns)
{
	uint8_t csi;

	if (!ns->ns_id_desc_raw || !ns->ns_id_desc[NVME_NIDT_CSI])
		return -ENODEV;

	csi = ns->ns_id_desc[NVME_NIDT_CSI]->nid[0];
	return (csi & NVME_CSI_ZNS) ? 1 : 0;
}

/**
 * @return The number of NVMe power states supported by the controller,
 * 	otherwise a negative errno
 * 
 * @note A controller shall support at least one power state, and may
 * 	support up to 31 additional power states (i.e., up to 32 total).
 */
int nvme_id_ctrl_npss(struct nvme_ctrl_instance *ctrl)
{
	if (!ctrl->id_ctrl)
		return -ENODEV;
	
	return (int)ctrl->id_ctrl->npss + 1; /* covert to 1's based value*/
}

/**
 * @return PCI Vendor ID if success, otherwise a negative errno
 */
int nvme_id_ctrl_vid(struct nvme_ctrl_instance *ctrl)
{
	if (!ctrl->id_ctrl)
		return -ENODEV;

	return le16_to_cpu(ctrl->id_ctrl->vid);
}

/**
 * @brief Get host memory buffer minimum descriptor entry size (in 4KiB units)
 * 
 * @return 0 on success, otherwise a negative errno
 */
int nvme_id_ctrl_hmminds(struct nvme_ctrl_instance *ctrl, uint32_t *hmminds)
{
	if (!ctrl->id_ctrl)
		return -ENODEV;

	*hmminds = le32_to_cpu(ctrl->id_ctrl->hmminds);
	return 0;
}

/**
 * @brief Get host memory maximum descriptors entries
 * 
 * @return HMMAXD on success, otherwise a negative errno
 */
int nvme_id_ctrl_hmmaxd(struct nvme_ctrl_instance *ctrl)
{
	if (!ctrl->id_ctrl)
		return -ENODEV;

	return le16_to_cpu(ctrl->id_ctrl->hmmaxd);
}

/**
 * @brief Get the maximum value of a valid NSID for the NVM subsystem
 * 
 * @return 0 on success, otherwise a negative errno
 */
int nvme_id_ctrl_nn(struct nvme_ctrl_instance *ctrl, uint32_t *nn)
{
	if (!ctrl->id_ctrl)
		return -ENODEV;
	
	*nn = le32_to_cpu(ctrl->id_ctrl->nn);
	return 0;
}

/**
 * @brief Get SGL support
 * 
 * @return 0 on success, otherwise a negative errno
 */
int nvme_id_ctrl_sgls(struct nvme_ctrl_instance *ctrl, uint32_t *sgls)
{
	if (!ctrl->id_ctrl)
		return -ENODEV;
	
	*sgls = le32_to_cpu(ctrl->id_ctrl->sgls);
	return 0;
}

static int check_id_ns_sanity(struct nvme_ns_group *grp, uint32_t nsid)
{
	if (grp->nr_ns < nsid)
		return -EINVAL;

	if (grp->ns[nsid - 1].nsid != nsid)
		return -ENODEV;

	if (!grp->ns[nsid - 1].id_ns)
		return -EFAULT;
	
	return 0;
}

static int check_id_ns_nvm_sanity(struct nvme_ns_group *grp, uint32_t nsid)
{
	if (grp->nr_ns < nsid)
		return -EINVAL;

	if (grp->ns[nsid - 1].nsid != nsid)
		return -ENODEV;

	if (!grp->ns[nsid - 1].id_ns_nvm)
		return -EFAULT;
	
	return 0;
}

/**
 * @brief Get the total size of the namespace in logical blocks
 * 
 * @return 0 on success, otherwise a negative errno
 */
int nvme_id_ns_nsze(struct nvme_ns_group *grp, uint32_t nsid, uint64_t *nsze)
{
	int ret;

	ret = check_id_ns_sanity(grp, nsid);
	if (ret < 0)
		return ret;

	*nsze = le64_to_cpu(grp->ns[nsid - 1].id_ns->nsze);
	return 0;
}

/**
 * @brief Get formatted LBA size
 * 
 * @return flbas on success, otherwise a negative errno
 */
int nvme_id_ns_flbas(struct nvme_ns_group *grp, uint32_t nsid)
{
	int ret;

	ret = check_id_ns_sanity(grp, nsid);
	if (ret < 0)
		return ret;
	
	return grp->ns[nsid - 1].id_ns->flbas;
}

/**
 * @brief Get metadata capabilities
 * 
 * @return mc on success, otherwise a negative errno
 */
int nvme_id_ns_mc(struct nvme_ns_group *grp, uint32_t nsid)
{
	int ret;

	ret = check_id_ns_sanity(grp, nsid);
	if (ret < 0)
		return ret;

	return grp->ns[nsid - 1].id_ns->mc;
}

/**
 * @brief Get end-to-end data protection capabilities
 * 
 * @return dpc on success, otherwise a negative errno
 */
int nvme_id_ns_dpc(struct nvme_ns_group *grp, uint32_t nsid)
{
	int ret;

	ret = check_id_ns_sanity(grp, nsid);
	if (ret < 0)
		return ret;

	return grp->ns[nsid - 1].id_ns->dpc;
}

/**
 * @brief Get end-to-end data protection type settings
 * 
 * @return dps on success, otherwise a negative errno
 */
int nvme_id_ns_dps(struct nvme_ns_group *grp, uint32_t nsid)
{
	int ret;

	ret = check_id_ns_sanity(grp, nsid);
	if (ret < 0)
		return ret;

	return grp->ns[nsid - 1].id_ns->dps;
}

/**
 * @brief Get maximum single source range length
 * 
 * @details Get the maximum number of logical blocks that may be specified
 * 	in the number of logical block field in each valid source range
 * 	entries descriptor of a copy command.
 * 
 * @return mssrl on success, otherwise a negative errno
 */
int nvme_id_ns_mssrl(struct nvme_ns_group *grp, uint32_t nsid)
{
	int ret;

	ret = check_id_ns_sanity(grp, nsid);
	if (ret < 0)
		return ret;
	
	return le16_to_cpu(grp->ns[nsid - 1].id_ns->mssrl);
}

/**
 * @brief Get maximum copy length
 * 
 * @details Get the maximum number of logical blocks that may be specified
 * 	in a copy command.
 * 
 * @return 0 on success, otherwise a negative errno 
 */
int nvme_id_ns_mcl(struct nvme_ns_group *grp, uint32_t nsid, uint32_t *mcl)
{
	int ret;

	ret = check_id_ns_sanity(grp, nsid);
	if (ret < 0)
		return ret;

	*mcl = le32_to_cpu(grp->ns[nsid - 1].id_ns->mcl);
	return 0;
}

/**
 * @brief Get maximum source range count
 * 
 * @details Get the maximum number of Source Range entries that may be used
 * 	to specify source data in a Copy command.
 * 
 * @return The max source range count on success, otherwise a negative errno
 * 
 * @note Covert it to 1's based value in this function.
 */
int nvme_id_ns_msrc(struct nvme_ns_group *grp, uint32_t nsid)
{
	int ret;

	ret = check_id_ns_sanity(grp, nsid);
	if (ret < 0)
		return ret;

	return (int)grp->ns[nsid - 1].id_ns->msrc + 1;
}

/**
 * @brief Get the number of metadata bytes provider per LBA
 * 
 * @return ms on success, otherwise a negative errno
 */
int nvme_id_ns_ms(struct nvme_ns_group *grp, uint32_t nsid)
{
	int ret;

	ret = check_id_ns_sanity(grp, nsid);
	if (ret < 0)
		return ret;

	return grp->ns[nsid - 1].meta_size;
}

/**
 * @brief Get logical block size
 * 
 * @return 0 on success, otherwise a negative errno
 */
int nvme_id_ns_lbads(struct nvme_ns_group *grp, uint32_t nsid, uint32_t *lbads)
{
	int ret;

	ret = check_id_ns_sanity(grp, nsid);
	if (ret < 0)
		return ret;
	
	*lbads = grp->ns[nsid - 1].blk_size;
	return 0;
}

/**
 * @brief Get logical block storage tag mask
 * 
 * @return 0 on success, otherwise a negative errno 
 */
int nvme_id_ns_nvm_lbstm(struct nvme_ns_group *grp, uint32_t nsid, uint64_t *lbstm)
{
	int ret;

	ret = check_id_ns_nvm_sanity(grp, nsid);
	if (ret < 0)
		return ret;

	*lbstm = le64_to_cpu(grp->ns[nsid - 1].id_ns_nvm->lbstm);
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_identify(int fd, struct nvme_identify *identify, void *buf, 
	uint32_t size)
{
	struct nvme_64b_cmd cmd = {0};
	
	BUG_ON(size != NVME_IDENTIFY_DATA_SIZE);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = identify;
	cmd.bit_mask = NVME_MASK_PRP1_PAGE | NVME_MASK_PRP2_PAGE;
	cmd.data_dir = DMA_BIDIRECTIONAL;
	cmd.data_buf_ptr = buf;
	cmd.data_buf_size = size;

	return nvme_submit_64b_cmd(fd, &cmd);
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 * 
 * @note It that specified namespace is an inactive NSID, then the controller
 *  returns a zero filled data structure.
 */
int nvme_cmd_identify_ns_active(int fd, struct nvme_id_ns *ns, uint32_t nsid)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.nsid = cpu_to_le32(nsid);
	identify.cns = NVME_ID_CNS_NS;

	return nvme_cmd_identify(fd, &identify, ns, sizeof(*ns));
}

/**
 * @brief Get Identify Namespace data structure for the specified NSID or
 *  the common namespace capabilities for the NVM Command Set
 * 
 * @param ns point to the identify namespace data structure
 * @param nsid An active NSID
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_identify_ns_active(struct nvme_dev_info *ndev, struct nvme_id_ns *ns, 
	uint32_t nsid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ns_active(ndev->fd, ns, nsid);
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
	
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_identify_ctrl(int fd, struct nvme_id_ctrl *ctrl)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.cns = NVME_ID_CNS_CTRL;

	return nvme_cmd_identify(fd, &identify, ctrl, sizeof(*ctrl));
}

/**
 * @brief Get Identify Controller data structure for the controller 
 *  processing the command.
 * 
 * @param ctrl point to the identify controller data structure
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_identify_ctrl(struct nvme_dev_info *ndev, struct nvme_id_ctrl *ctrl)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ctrl(ndev->fd, ctrl);
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
	
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_identify_ns_list_active(int fd, void *buf, uint32_t size, 
	uint32_t nsid)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.cns = NVME_ID_CNS_NS_ACTIVE_LIST;
	identify.nsid = cpu_to_le32(nsid);

	return nvme_cmd_identify(fd, &identify, buf, size);
}

/**
 * @brief Get Active Namespace ID List
 * 
 * @param nsid This field may be cleared to 0h to retrive a Namespace List
 *  including the namespace starting with NSID of 1h.
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_identify_ns_list_active(struct nvme_dev_info *ndev, void *buf, 
	uint32_t size, uint32_t nsid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ns_list_active(ndev->fd, buf, size, nsid);
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
	
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_identify_ns_desc_list(int fd, void *buf, uint32_t size,
	uint32_t nsid)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.cns = NVME_ID_CNS_NS_DESC_LIST;
	identify.nsid = cpu_to_le32(nsid);

	return nvme_cmd_identify(fd, &identify, buf, size);
}

/**
 * @brief Get Namespace Identification Descriptor list.
 * 
 * @param nsid An active NSID
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_identify_ns_desc_list(struct nvme_dev_info *ndev, void *buf, 
	uint32_t size, uint32_t nsid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ns_desc_list(ndev->fd, buf, size, nsid);
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
	
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_identify_cs_ns(int fd, void *buf, uint32_t size, uint32_t nsid,
	uint8_t csi)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.cns = NVME_ID_CNS_CS_NS_ACTIVE;
	identify.nsid = cpu_to_le32(nsid);
	identify.csi = csi;

	return nvme_cmd_identify(fd, &identify, buf, size);
}

/**
 * @brief Get I/O command set specific identify namespace data structure
 * 
 * @param nsid An active NSID
 * @param csi Command Set Identifier
 * 
 * @return 0 on success, otherwise a negative errno
 */
int nvme_identify_cs_ns(struct nvme_dev_info *ndev, void *buf, uint32_t size, 
	uint32_t nsid, uint8_t csi)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_cs_ns(ndev->fd, buf, size, nsid, csi);
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
	
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_identify_cs_ctrl(int fd, void *buf, uint32_t size, uint8_t csi)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.cns = NVME_ID_CNS_CS_CTRL;
	identify.csi = csi;

	return nvme_cmd_identify(fd, &identify, buf, size);
}

/**
 * @brief Get I/O command set specific identify controller data structure
 * 
 * @param csi Command Set Identifier
 * 
 * @return 0 on success, otherwise a negative errno 
 */
int nvme_identify_cs_ctrl(struct nvme_dev_info *ndev, void *buf, uint32_t size,
	uint8_t csi)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_cs_ctrl(ndev->fd, buf, size, csi);
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
	
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_identify_ns_list_allocated(int fd, void *buf, uint32_t size, 
	uint32_t nsid)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.cns = NVME_ID_CNS_NS_PRESENT_LIST;
	identify.nsid = cpu_to_le32(nsid);

	return nvme_cmd_identify(fd, &identify, buf, size);
}

/**
 * @brief Get Allocated Namespace ID List
 * 
 * @param nsid This field may be cleared to 0h to retrive a Namespace List
 *  including the namespace starting with NSID of 1h.
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_identify_ns_list_allocated(struct nvme_dev_info *ndev, void *buf, 
	uint32_t size, uint32_t nsid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ns_list_allocated(ndev->fd, buf, size, nsid);
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
	
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 * 
 * @note It that specified namespace is an unallocated NSID, then the controller
 *  returns a zero filled data structure.
 */
int nvme_cmd_identify_ns_allocated(int fd, struct nvme_id_ns *ns, uint32_t nsid)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.nsid = cpu_to_le32(nsid);
	identify.cns = NVME_ID_CNS_NS_PRESENT;

	return nvme_cmd_identify(fd, &identify, ns, sizeof(*ns));
}

/**
 * @brief Get Identify Namespace data structure for the specified NSID or
 *  the common namespace capabilities for the NVM Command Set
 * 
 * @param ns point to the identify namespace data structure
 * @param nsid An allocated NSID
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_identify_ns_allocated(struct nvme_dev_info *ndev, 
	struct nvme_id_ns *ns, uint32_t nsid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ns_allocated(ndev->fd, ns, nsid);
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
	
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_identify_ns_attached_ctrl_list(int fd, void *buf, uint32_t size,
	uint32_t nsid, uint16_t cntid)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.cns = NVME_ID_CNS_CTRL_NS_LIST;
	identify.nsid = cpu_to_le32(nsid);
	identify.ctrlid = cpu_to_le16(cntid);

	return nvme_cmd_identify(fd, &identify, buf, size);
}

/**
 * @brief Get Namespace Attached Controller list.
 * 
 * @param cntid A controller list is returned containing a controller 
 *  identifier greater than or equal to the value specified in this field.
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_identify_ns_attached_ctrl_list(struct nvme_dev_info *ndev, void *buf, 
	uint32_t size, uint32_t nsid, uint16_t cntid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ns_attached_ctrl_list(ndev->fd, buf, size, nsid, cntid);
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
	
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_identify_ctrl_list(int fd, void *buf, uint32_t size, 
	uint16_t cntid)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.cns = NVME_ID_CNS_CTRL_LIST;
	identify.ctrlid = cpu_to_le16(cntid);

	return nvme_cmd_identify(fd, &identify, buf, size);
}

/**
 * @brief Get Controller list.
 * 
 * @param cntid A controller list is returned containing a controller 
 *  identifier greater than or equal to the value specified in this field.
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_identify_ctrl_list(struct nvme_dev_info *ndev, void *buf, 
	uint32_t size, uint16_t cntid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ctrl_list(ndev->fd, buf, size, cntid);
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
	
	return 0;
}

int nvme_cmd_identify_ctrl_csc_list(int fd, struct nvme_id_ctrl_csc *csc, 
	uint16_t cntid)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.cns = NVME_ID_CNS_CTRL_CSC_LIST;
	identify.ctrlid = cpu_to_le16(cntid);

	return nvme_cmd_identify(fd, &identify, csc, sizeof(*csc));
}

int nvme_identify_ctrl_csc_list(struct nvme_dev_info *ndev, 
	struct nvme_id_ctrl_csc *csc, uint16_t cntid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ctrl_csc_list(ndev->fd, csc, cntid);
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
	
	return 0;
}
