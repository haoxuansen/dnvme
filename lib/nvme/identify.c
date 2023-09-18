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

