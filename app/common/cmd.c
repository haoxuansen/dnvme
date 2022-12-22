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

#include "log.h"
#include "cmd.h"

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
			ccmd->opcode, cmd->q_id, ret);
		return ret;
	}
	return (int)cmd->unique_id;
}

/* !TODO: delete it later */
int nvme_submit_64b_cmd_legacy(int fd, struct nvme_64b_cmd *cmd)
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
			ccmd->opcode, cmd->q_id, ret);
		return ret;
	}
	return 0;
}

int nvme_cmd_create_iosq(int fd, struct nvme_create_sq *csq, uint8_t contig,
	void *buf, uint32_t size)
{
	struct nvme_64b_cmd cmd = {0};

	cmd.q_id = NVME_AQ_ID;
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

int nvme_cmd_create_iocq(int fd, struct nvme_create_cq *ccq, uint8_t contig,
	void *buf, uint32_t size)
{
	struct nvme_64b_cmd cmd = {0};

	cmd.q_id = NVME_AQ_ID;
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
