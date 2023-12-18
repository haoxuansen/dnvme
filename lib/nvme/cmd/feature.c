/**
 * @file feature.c
 * @author yeqiang_xu (yeqiang_xu@maxio-tech.com)
 * @brief 
 * @version 0.1
 * @date 2023-11-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <errno.h>

#include "byteorder.h"
#include "libbase.h"
#include "libnvme.h"

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_set_feature(int fd, uint32_t nsid, uint32_t fid, uint32_t dw11)
{
	struct nvme_64b_cmd cmd = {0};
	struct nvme_features feat = {0};

	feat.opcode = nvme_admin_set_features;
	feat.nsid = cpu_to_le32(nsid);
	feat.fid = cpu_to_le32(fid);
	feat.dword11 = cpu_to_le32(dw11);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &feat;

	return nvme_submit_64b_cmd(fd, &cmd);
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_get_feature(int fd, uint32_t nsid, uint32_t fid, uint32_t dw11)
{
	struct nvme_64b_cmd cmd = {0};
	struct nvme_features feat = {0};

	feat.opcode = nvme_admin_get_features;
	feat.nsid = cpu_to_le32(nsid);
	feat.fid = cpu_to_le32(fid);
	feat.dword11 = cpu_to_le32(dw11);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &feat;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_cmd_set_feat_hmb(int fd, struct nvme_hmb_wrapper *wrap)
{
	struct nvme_64b_cmd cmd = {0};
	struct nvme_features feat = {0};

	feat.opcode = nvme_admin_set_features;
	feat.fid = cpu_to_le32(wrap->sel | NVME_FEAT_HOST_MEM_BUF);
	feat.dword11 = cpu_to_le32(wrap->dw11);
	feat.dword12 = cpu_to_le32(wrap->hsize);
	feat.dword13 = cpu_to_le32(lower_32_bits(wrap->hmdla));
	feat.dword14 = cpu_to_le32(upper_32_bits(wrap->hmdla));
	feat.dword15 = cpu_to_le32(wrap->hmdlec);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &feat;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_cmd_get_feat_hmb(int fd, struct nvme_hmb_wrapper *wrap)
{
	struct nvme_64b_cmd cmd = {0};
	struct nvme_features feat = {0};

	feat.opcode = nvme_admin_get_features;
	feat.fid = cpu_to_le32(wrap->sel | NVME_FEAT_HOST_MEM_BUF);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &feat;
	cmd.bit_mask = NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST |
		NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST;
	cmd.data_buf_ptr = wrap->attr;
	cmd.data_buf_size = sizeof(struct nvme_feat_hmb_attribute);
	cmd.data_dir = DMA_BIDIRECTIONAL;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_cmd_set_feat_host_behavior(int fd, uint32_t sel, 
	struct nvme_feat_host_behavior *behavior)
{
	struct nvme_64b_cmd cmd = {0};
	struct nvme_features feat = {0};

	feat.opcode = nvme_admin_set_features;
	feat.fid = cpu_to_le32(sel | NVME_FEAT_HOST_BEHAVIOR);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &feat;
	cmd.bit_mask = NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST |
		NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST;
	cmd.data_buf_ptr = behavior;
	cmd.data_buf_size = sizeof(struct nvme_feat_host_behavior);
	cmd.data_dir = DMA_BIDIRECTIONAL;
	
	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_cmd_get_feat_host_behavior(int fd, uint32_t sel,
	struct nvme_feat_host_behavior *behavior)
{
	struct nvme_64b_cmd cmd = {0};
	struct nvme_features feat = {0};

	feat.opcode = nvme_admin_get_features;
	feat.fid = cpu_to_le32(sel | NVME_FEAT_HOST_BEHAVIOR);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &feat;
	cmd.bit_mask = NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST |
		NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST;
	cmd.data_buf_ptr = behavior;
	cmd.data_buf_size = sizeof(struct nvme_feat_host_behavior);
	cmd.data_dir = DMA_BIDIRECTIONAL;
	
	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_set_feat_arbitration(struct nvme_dev_info *ndev, 
	struct nvme_feat_arb_wrapper *wrap)
{
	struct nvme_completion entry = {0};
	uint16_t cid;

	cid = CHK_EXPR_NUM_LT0_RTN(
		nvme_cmd_set_feat_arbitration(ndev->fd, wrap), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID), -EPERM);
	CHK_EXPR_NUM_NE_RTN(
		nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry)),
		1, -ETIME);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, NVME_SC_SUCCESS),
		-EPERM);

	return 0;
}

int nvme_set_feat_hmb(struct nvme_dev_info *ndev, struct nvme_hmb_wrapper *wrap)
{
	struct nvme_completion entry = {0};
	uint16_t cid;

	cid = CHK_EXPR_NUM_LT0_RTN(
		nvme_cmd_set_feat_hmb(ndev->fd, wrap), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID), -EPERM);
	CHK_EXPR_NUM_NE_RTN(
		nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry)),
		1, -ETIME);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, NVME_SC_SUCCESS),
		-EPERM);

	return 0;
}

int nvme_get_feat_hmb(struct nvme_dev_info *ndev, struct nvme_hmb_wrapper *wrap)
{
	struct nvme_completion entry = {0};
	uint16_t cid;

	cid = CHK_EXPR_NUM_LT0_RTN(
		nvme_cmd_get_feat_hmb(ndev->fd, wrap), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID), -EPERM);
	CHK_EXPR_NUM_NE_RTN(
		nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry)),
		1, -ETIME);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, NVME_SC_SUCCESS),
		-EPERM);

	return le32_to_cpu(entry.result.u32) & 0x3;
}

int nvme_set_feat_host_behavior(struct nvme_dev_info *ndev, uint32_t sel, 
	struct nvme_feat_host_behavior *behavior)
{
	struct nvme_completion entry = {0};
	uint16_t cid;

	cid = CHK_EXPR_NUM_LT0_RTN(
		nvme_cmd_set_feat_host_behavior(ndev->fd, sel, behavior), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID), -EPERM);
	CHK_EXPR_NUM_NE_RTN(
		nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry)),
		1, -ETIME);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, NVME_SC_SUCCESS),
		-EPERM);

	return 0;
}

int nvme_get_feat_host_behavior(struct nvme_dev_info *ndev, uint32_t sel,
	struct nvme_feat_host_behavior *behavior)
{
	struct nvme_completion entry = {0};
	uint16_t cid;

	cid = CHK_EXPR_NUM_LT0_RTN(
		nvme_cmd_get_feat_host_behavior(ndev->fd, sel, behavior), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID), -EPERM);
	CHK_EXPR_NUM_NE_RTN(
		nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry)),
		1, -ETIME);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, NVME_SC_SUCCESS),
		-EPERM);

	return 0;
}

int nvme_set_feat_iocs_profile(struct nvme_dev_info *ndev, uint32_t sel, 
	uint32_t index)
{
	struct nvme_completion entry = {0};
	uint16_t cid;

	cid = CHK_EXPR_NUM_LT0_RTN(
		nvme_cmd_set_feat_iocs_profile(ndev->fd, sel, index), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID), -EPERM);
	CHK_EXPR_NUM_NE_RTN(
		nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry)),
		1, -ETIME);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, NVME_SC_SUCCESS),
		-EPERM);

	return 0;
}

/**
 * @brief Get the index of the currently selected I/O command set combination
 *
 * @return IOCSCI on succcess, otherwise a negative errno
 */
int nvme_get_feat_iocs_profile(struct nvme_dev_info *ndev, uint32_t sel)
{
	struct nvme_completion entry = {0};
	uint16_t cid;

	cid = CHK_EXPR_NUM_LT0_RTN(
		nvme_cmd_get_feat_iocs_profile(ndev->fd, sel), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID), -EPERM);
	CHK_EXPR_NUM_NE_RTN(
		nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry)),
		1, -ETIME);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, NVME_SC_SUCCESS),
		-EPERM);

	return le32_to_cpu(entry.result.u32) & NVME_IOCS_CI_MASK;
}

/**
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_set_feat_write_protect(struct nvme_dev_info *ndev, uint32_t nsid, 
	uint32_t sel, uint32_t state)
{
	struct nvme_completion entry = {0};
	uint16_t cid;

	cid = CHK_EXPR_NUM_LT0_RTN(
		nvme_cmd_set_feat_write_protect(ndev->fd, nsid, sel, state), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID), -EPERM);
	CHK_EXPR_NUM_NE_RTN(
		nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry)),
		1, -ETIME);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, NVME_SC_SUCCESS),
		-EPERM);

	return 0;
}

/**
 * @return namespace write protect state on succcess, otherwise a negative errno
 */
int nvme_get_feat_write_protect(struct nvme_dev_info *ndev, uint32_t nsid, 
	uint32_t sel)
{
	struct nvme_completion entry = {0};
	uint16_t cid;

	cid = CHK_EXPR_NUM_LT0_RTN(
		nvme_cmd_get_feat_write_protect(ndev->fd, nsid, sel), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID), -EPERM);
	CHK_EXPR_NUM_NE_RTN(
		nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry)),
		1, -ETIME);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, NVME_SC_SUCCESS),
		-EPERM);

	return le32_to_cpu(entry.result.u32) & NVME_NS_WPS_MASK;
}

