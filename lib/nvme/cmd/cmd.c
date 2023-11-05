/**
 * @file cmd.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-19
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "compiler.h"
#include "libbase.h"
#include "libnvme.h"

/**
 * @brief Submit the specified command
 * 
 * @param fd NVMe device file descriptor
 * @param cmd The command to be submitted
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_submit_64b_cmd(int fd, struct nvme_64b_cmd *cmd)
{
	struct nvme_common_command *ccmd;
	int ret;

	if (!cmd->cmd_buf_ptr) {
		pr_err("cmd buf pointer is NULL!\n");
		return -EINVAL;
	}
	ccmd = cmd->cmd_buf_ptr;

	ret = ioctl(fd, NVME_IOCTL_SUBMIT_64B_CMD, cmd);
	if (ret < 0) {
		pr_err("failed to submit cmd(0x%x) to SQ(%u)!(%d)\n", 
			ccmd->opcode, cmd->sqid, ret);
		return ret;
	}
	return (int)cmd->cid;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_keep_alive(int fd)
{
	struct nvme_common_command ccmd = {0};
	struct nvme_64b_cmd cmd = {0};

	ccmd.opcode = nvme_admin_keep_alive;

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &ccmd;

	return nvme_submit_64b_cmd(fd, &cmd);
}

/**
 * @brief Submit command to create a I/O submission queue
 * 
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_create_iosq(int fd, struct nvme_create_sq *csq, uint8_t contig,
	void *buf, uint32_t size)
{
	struct nvme_64b_cmd cmd = {0};

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = csq;
	/* !TODO: Although it is always right to use bidirection, it is
	 * better to choose the right direction */
	cmd.data_dir = DMA_BIDIRECTIONAL;

	if (contig)
		cmd.bit_mask = NVME_MASK_PRP1_PAGE;
	else
		cmd.bit_mask = NVME_MASK_PRP1_LIST;

	cmd.data_buf_ptr = buf;
	cmd.data_buf_size = size;

	return nvme_submit_64b_cmd(fd, &cmd);
}

/**
 * @brief Submit command to create a I/O completion queue
 * 
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_create_iocq(int fd, struct nvme_create_cq *ccq, uint8_t contig,
	void *buf, uint32_t size)
{
	struct nvme_64b_cmd cmd = {0};

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = ccq;
	/* !TODO: Although it is always right to use bidirection, it is
	 * better to choose the right direction */
	cmd.data_dir = DMA_BIDIRECTIONAL;

	if (contig)
		cmd.bit_mask = NVME_MASK_PRP1_PAGE;
	else
		cmd.bit_mask = NVME_MASK_PRP1_LIST;

	cmd.data_buf_ptr = buf;
	cmd.data_buf_size = size;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_cmd_delete_iosq(int fd, uint16_t sqid)
{
	struct nvme_64b_cmd cmd = {0};
	struct nvme_delete_queue dq = {0};

	dq.opcode = nvme_admin_delete_sq;
	dq.qid = cpu_to_le16(sqid);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &dq;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_cmd_delete_iocq(int fd, uint16_t cqid)
{
	struct nvme_64b_cmd cmd = {0};
	struct nvme_delete_queue dq = {0};

	dq.opcode = nvme_admin_delete_cq;
	dq.qid = cpu_to_le16(cqid);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &dq;

	return nvme_submit_64b_cmd(fd, &cmd);
}


int nvme_cmd_format_nvm(int fd, uint32_t nsid, uint8_t flags, uint32_t dw10)
{
	struct nvme_64b_cmd cmd = {0};
	struct nvme_format_cmd fmt = {0};

	fmt.opcode = nvme_admin_format_nvm;
	fmt.flags = flags;
	fmt.nsid = cpu_to_le32(nsid);
	fmt.cdw10 = cpu_to_le32(dw10);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &fmt;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_format_nvm(struct nvme_dev_info *ndev, uint32_t nsid, uint8_t flags, 
	uint32_t dw10)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_format_nvm(ndev->fd, nsid, flags, dw10);
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

int nvme_cmd_io_rw_common(int fd, struct nvme_rwc_wrapper *wrap, uint8_t opcode)
{
	struct nvme_rw_command rwc = {0};
	struct nvme_64b_cmd cmd = {0};

	rwc.opcode = opcode;
	rwc.flags = wrap->flags;

	if (wrap->use_user_cid) {
		cmd.use_user_cid = 1;
		rwc.command_id = wrap->cid;
	}
	rwc.nsid = cpu_to_le32(wrap->nsid);
	rwc.cdw2 = cpu_to_le32(wrap->dw2);
	rwc.cdw3 = cpu_to_le32(wrap->dw3);

	if (wrap->use_user_meta) {
		cmd.use_user_meta = 1;
		rwc.metadata = cpu_to_le64(wrap->meta_addr);
	} else if (wrap->meta_id) {
		cmd.meta_id = wrap->meta_id;
		cmd.bit_mask |= NVME_MASK_MPTR;
	}
	if (wrap->use_user_prp) {
		cmd.use_user_prp = 1;
		rwc.dptr.prp1 = cpu_to_le64(wrap->prp1);
		rwc.dptr.prp2 = cpu_to_le64(wrap->prp2);
	}

	rwc.slba = cpu_to_le64(wrap->slba);
	rwc.length = cpu_to_le16((uint16_t)(wrap->nlb - 1)); /* 0'base */
	rwc.control = cpu_to_le16(wrap->control);
	rwc.reftag = cpu_to_le32(wrap->dw14);
	rwc.apptag = cpu_to_le16(wrap->apptag);
	rwc.appmask = cpu_to_le16(wrap->appmask);
	
	cmd.sqid = wrap->sqid;
	cmd.cmd_buf_ptr = &rwc;
	cmd.bit_mask |= NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST |
		NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST;
	cmd.data_buf_ptr = wrap->buf;
	cmd.data_buf_size = wrap->size;
	cmd.data_dir = DMA_BIDIRECTIONAL;

	if (wrap->use_bit_bucket) {
		cmd.use_bit_bucket = 1;
		cmd.nr_bit_bucket = wrap->nr_bit_bucket;
		cmd.bit_bucket = wrap->bit_bucket;
		BUG_ON(!cmd.nr_bit_bucket || !cmd.bit_bucket);
	}

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_io_rw_common(struct nvme_dev_info *ndev, struct nvme_rwc_wrapper *wrap, 
	uint8_t opcode)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_io_rw_common(ndev->fd, wrap, opcode);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, wrap->sqid);
	if (ret < 0)
		return ret;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, wrap->cqid, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, wrap->sqid, cid, NVME_SC_SUCCESS);
	if (ret < 0)
		return ret;
	
	return 0;
}

int nvme_cmd_io_copy(int fd, struct nvme_copy_wrapper *wrap)
{
	struct nvme_copy_cmd copy = {0};
	struct nvme_64b_cmd cmd = {0};

	copy.opcode = nvme_cmd_copy;
	copy.flags = wrap->flags;
	copy.nsid = cpu_to_le32(wrap->nsid);
	copy.slba = cpu_to_le64(wrap->slba);
	copy.ranges = wrap->ranges;
	copy.desc_fmt = wrap->desc_fmt;

	cmd.sqid = wrap->sqid;
	cmd.cmd_buf_ptr = &copy;
	cmd.bit_mask = NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST |
		NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST;
	cmd.data_buf_ptr = wrap->desc;
	cmd.data_buf_size = wrap->size;
	cmd.data_dir = DMA_BIDIRECTIONAL;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_io_copy(struct nvme_dev_info *ndev, struct nvme_copy_wrapper *wrap)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_io_copy(ndev->fd, wrap);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, wrap->sqid);
	if (ret < 0)
		return ret;

	ret = nvme_gnl_cmd_reap_cqe(ndev, wrap->cqid, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, wrap->sqid, cid, NVME_SC_SUCCESS);
	if (ret < 0)
		return ret;
	
	return 0;
}
