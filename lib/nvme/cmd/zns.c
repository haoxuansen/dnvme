/**
 * @file cmd_zns.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-08-14
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"

int nvme_cmd_io_zone_manage_send(int fd, struct nvme_zone_mgnt_send_wrapper *wrap)
{
	struct nvme_zone_mgmt_send_cmd send = {0};
	struct nvme_64b_cmd cmd = {0};

	send.opcode = nvme_cmd_zone_mgmt_send;
	send.nsid = cpu_to_le32(wrap->nsid);
	send.slba = cpu_to_le64(wrap->slba);
	send.zsa = wrap->action;
	send.select_all = wrap->select_all;

	cmd.sqid = wrap->sqid;
	cmd.cmd_buf_ptr = &send;
	cmd.bit_mask = NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST |
		NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST;
	cmd.data_buf_ptr = wrap->buf;
	cmd.data_buf_size = wrap->size;
	cmd.data_dir = DMA_BIDIRECTIONAL;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_io_zone_manage_send(struct nvme_dev_info *ndev, 
				struct nvme_zone_mgnt_send_wrapper *wrap)
{
	struct nvme_completion entry = {0};
	uint16_t cid;

	cid = CHK_EXPR_NUM_LT0_RTN(
		nvme_cmd_io_zone_manage_send(ndev->fd, wrap), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_ring_sq_doorbell(ndev->fd, wrap->sqid), -EPERM);
	CHK_EXPR_NUM_NE_RTN(
		nvme_gnl_cmd_reap_cqe(ndev, wrap->cqid, 1, &entry, sizeof(entry)),
		1, -ETIME);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_valid_cq_entry(&entry, wrap->sqid, cid, wrap->status),
		-EPERM);
	
	return 0;
}

int nvme_cmd_io_zone_manage_receive(int fd, struct nvme_zone_mgnt_recv_wrapper *wrap)
{
	struct nvme_zone_mgmt_recv_cmd recv = {0};
	struct nvme_64b_cmd cmd = {0};

	BUG_ON(!IS_ALIGNED(wrap->size, 4));

	recv.opcode = nvme_cmd_zone_mgmt_recv;
	recv.nsid = cpu_to_le32(wrap->nsid);
	recv.slba = cpu_to_le64(wrap->slba);
	/* covert to dwords and 0's based value */
	recv.numd = cpu_to_le32((wrap->size >> 2) - 1);
	recv.zra = wrap->action;
	recv.zrasf = wrap->specific;
	recv.pr = wrap->partial_report;

	cmd.sqid = wrap->sqid;
	cmd.cmd_buf_ptr = &recv;
	cmd.bit_mask = NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST |
		NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST;
	cmd.data_buf_ptr = wrap->buf;
	cmd.data_buf_size = wrap->size;
	cmd.data_dir = DMA_BIDIRECTIONAL;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_io_zone_manage_receive(struct nvme_dev_info *ndev, 
				struct nvme_zone_mgnt_recv_wrapper *wrap)
{
	struct nvme_completion entry = {0};
	uint16_t cid;

	cid = CHK_EXPR_NUM_LT0_RTN(
		nvme_cmd_io_zone_manage_receive(ndev->fd, wrap), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_ring_sq_doorbell(ndev->fd, wrap->sqid), -EPERM);
	CHK_EXPR_NUM_NE_RTN(
		nvme_gnl_cmd_reap_cqe(ndev, wrap->cqid, 1, &entry, sizeof(entry)),
		1, -ETIME);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_valid_cq_entry(&entry, wrap->sqid, cid, wrap->status),
		-EPERM);
	
	return 0;
}

int nvme_cmd_io_zone_append(int fd, struct nvme_zone_append_wrapper *wrap)
{
	struct nvme_zone_append_cmd append = {0};
	struct nvme_64b_cmd cmd = {0};

	append.opcode = nvme_cmd_zone_append;
	append.flags = wrap->flags;
	append.nsid = cpu_to_le32(wrap->nsid);
	append.zslba = cpu_to_le64(wrap->zslba);
	append.length = cpu_to_le16((uint16_t)(wrap->nlb - 1));
	append.control = cpu_to_le16(wrap->piremap << 9);

	cmd.sqid = wrap->sqid;
	cmd.cmd_buf_ptr = &append;
	cmd.bit_mask = NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST |
		NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST;
	cmd.data_buf_ptr = wrap->buf;
	cmd.data_buf_size = wrap->size;
	cmd.data_dir = DMA_BIDIRECTIONAL;
	cmd.meta_id = wrap->meta_id;
	if (cmd.meta_id)
		cmd.bit_mask |= NVME_MASK_MPTR;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_io_zone_append(struct nvme_dev_info *ndev, 
				struct nvme_zone_append_wrapper *wrap)
{
	struct nvme_completion entry = {0};
	uint16_t cid;

	cid = CHK_EXPR_NUM_LT0_RTN(
		nvme_cmd_io_zone_append(ndev->fd, wrap), -EPERM);
	CHK_EXPR_NUM_LT0_RTN(
		nvme_ring_sq_doorbell(ndev->fd, wrap->sqid), -EPERM);
	CHK_EXPR_NUM_NE_RTN(
		nvme_gnl_cmd_reap_cqe(ndev, wrap->cqid, 1, &entry, sizeof(entry)),
		1, -ETIME);

	if (wrap->check_none)
		return 0;

	CHK_EXPR_NUM_LT0_RTN(
		nvme_valid_cq_entry(&entry, wrap->sqid, cid, wrap->status),
		-EPERM);
	
	return 0;
}

