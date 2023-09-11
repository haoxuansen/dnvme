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

int nvme_cmd_maxio_nvme_cqm_set_param(int fd, uint32_t bitmap, uint32_t param)
{
	struct nvme_maxio_common_cmd common = {0};
	struct nvme_64b_cmd cmd = {0};

	common.opcode = nvme_admin_maxio_nvme_cqm;
	common.option = cpu_to_le32(NVME_MAXIO_OPT_SET_PARAM);
	common.param = cpu_to_le32(param);
	common.bitmap = cpu_to_le32(bitmap);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &common;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_maxio_nvme_cqm_set_param(struct nvme_dev_info *ndev,
	uint32_t bitmap, uint32_t param)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_maxio_nvme_cqm_set_param(ndev->fd, bitmap, param);
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

int nvme_cmd_maxio_nvme_cqm_check_result(int fd, uint32_t bitmap)
{
	struct nvme_maxio_common_cmd common = {0};
	struct nvme_64b_cmd cmd = {0};

	common.opcode = nvme_admin_maxio_nvme_cqm;
	common.option = cpu_to_le32(NVME_MAXIO_OPT_CHECK_RESULT);
	common.bitmap = cpu_to_le32(bitmap);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &common;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_maxio_nvme_cqm_check_result(struct nvme_dev_info *ndev, 
	uint32_t bitmap)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_maxio_nvme_cqm_check_result(ndev->fd, bitmap);
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


