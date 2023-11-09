/**
 * @file cmd_vendor.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-09-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"

int nvme_cmd_maxio_set_param_fmt1(int fd, uint32_t subcmd, 
	uint32_t param, uint8_t opcode)
{
	struct nvme_maxio_common_cmd common = {0};
	struct nvme_64b_cmd cmd = {0};

	common.opcode = opcode;
	common.option = cpu_to_le32(NVME_MAXIO_OPT_SET_PARAM);
	common.param = cpu_to_le32(param);
	common.subcmd = cpu_to_le32(subcmd);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &common;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_cmd_maxio_set_param_fmt2(int fd, uint32_t subcmd, 
	struct nvme_maxio_cmd_param *param, uint8_t opcode)
{
	struct nvme_maxio_common_cmd common = {0};
	struct nvme_64b_cmd cmd = {0};

	common.opcode = opcode;
	common.option = cpu_to_le32(NVME_MAXIO_OPT_SET_PARAM);
	common.param = cpu_to_le32(param->dw3);
	common.subcmd = cpu_to_le32(subcmd);
	common.cdw11 = cpu_to_le32(param->dw11);
	common.cdw12 = cpu_to_le32(param->dw12);
	common.cdw13 = cpu_to_le32(param->dw13);
	common.cdw14 = cpu_to_le32(param->dw14);
	common.cdw15 = cpu_to_le32(param->dw15);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &common;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_maxio_set_param_fmt1(struct nvme_dev_info *ndev,
	uint32_t subcmd, uint32_t param, uint8_t opcode)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_maxio_set_param_fmt1(ndev->fd, subcmd, param, opcode);
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

int nvme_maxio_set_param_fmt2(struct nvme_dev_info *ndev,
	uint32_t subcmd, struct nvme_maxio_cmd_param *param, uint8_t opcode)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_maxio_set_param_fmt2(ndev->fd, subcmd, param, opcode);
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

int nvme_cmd_maxio_check_result_fmt1(int fd, uint32_t subcmd, 
	uint8_t opcode)
{
	struct nvme_maxio_common_cmd common = {0};
	struct nvme_64b_cmd cmd = {0};

	common.opcode = opcode;
	common.option = cpu_to_le32(NVME_MAXIO_OPT_CHECK_RESULT);
	common.subcmd = cpu_to_le32(subcmd);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &common;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_maxio_check_result_fmt1(struct nvme_dev_info *ndev, 
	uint32_t subcmd, uint8_t opcode)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_maxio_check_result_fmt1(ndev->fd, subcmd, opcode);
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

int nvme_cmd_maxio_nvme_case_check_result(int fd, uint32_t subcmd,
	void *rbuf, uint32_t size)
{
	struct nvme_maxio_common_cmd common = {0};
	struct nvme_64b_cmd cmd = {0};

	common.opcode = nvme_admin_maxio_nvme_case;
	common.option = cpu_to_le32(NVME_MAXIO_OPT_CHECK_RESULT);
	common.param = cpu_to_le32(size);
	common.subcmd = cpu_to_le32(subcmd);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &common;
	cmd.bit_mask = NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST |
		NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST;
	cmd.data_buf_ptr = rbuf;
	cmd.data_buf_size = size;
	cmd.data_dir = DMA_BIDIRECTIONAL;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_maxio_nvme_case_check_result(struct nvme_dev_info *ndev, 
	uint32_t subcmd, void *rbuf, uint32_t size)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_maxio_nvme_case_check_result(ndev->fd, subcmd, rbuf, size);
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

