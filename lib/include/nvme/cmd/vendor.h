/**
 * @file cmd_vendor.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-09-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

int nvme_cmd_maxio_set_param_fmt1(int fd, uint32_t subcmd, 
	uint32_t param, uint8_t opcode);
int nvme_maxio_set_param_fmt1(struct nvme_dev_info *ndev,
	uint32_t subcmd, uint32_t param, uint8_t opcode);

static inline int nvme_cmd_maxio_nvme_top_set_param(int fd, 
	uint32_t subcmd, uint32_t param)
{
	return nvme_cmd_maxio_set_param_fmt1(fd, subcmd, param, 
		nvme_admin_maxio_nvme_top);
}

static inline int nvme_maxio_nvme_top_set_param(struct nvme_dev_info *ndev, 
	uint32_t subcmd, uint32_t param)
{
	return nvme_maxio_set_param_fmt1(ndev, subcmd, param,
		nvme_admin_maxio_nvme_top);
}

static inline int nvme_cmd_maxio_nvme_cqm_set_param(int fd, 
	uint32_t subcmd, uint32_t param)
{
	return nvme_cmd_maxio_set_param_fmt1(fd, subcmd, param, 
		nvme_admin_maxio_nvme_cqm);
}

static inline int nvme_maxio_nvme_cqm_set_param(struct nvme_dev_info *ndev,
	uint32_t subcmd, uint32_t param)
{
	return nvme_maxio_set_param_fmt1(ndev, subcmd, param,
		nvme_admin_maxio_nvme_cqm);
}

static inline int nvme_cmd_maxio_nvme_hwrdma_set_param(int fd, 
	uint32_t subcmd, uint32_t param)
{
	return nvme_cmd_maxio_set_param_fmt1(fd, subcmd, param, 
		nvme_admin_maxio_nvme_hwrdma);
}

static inline int nvme_maxio_nvme_hwrdma_set_param(struct nvme_dev_info *ndev,
	uint32_t subcmd, uint32_t param)
{
	return nvme_maxio_set_param_fmt1(ndev, subcmd, param,
		nvme_admin_maxio_nvme_hwrdma);
}

static inline int nvme_cmd_maxio_nvme_hwwdma_set_param(int fd, 
	uint32_t subcmd, uint32_t param)
{
	return nvme_cmd_maxio_set_param_fmt1(fd, subcmd, param, 
		nvme_admin_maxio_nvme_hwwdma);
}

static inline int nvme_maxio_nvme_hwwdma_set_param(struct nvme_dev_info *ndev,
	uint32_t subcmd, uint32_t param)
{
	return nvme_maxio_set_param_fmt1(ndev, subcmd, param,
		nvme_admin_maxio_nvme_hwwdma);
}

int nvme_cmd_maxio_check_result_fmt1(int fd, uint32_t subcmd, 
	uint8_t opcode);
int nvme_maxio_check_result_fmt1(struct nvme_dev_info *ndev, 
	uint32_t subcmd, uint8_t opcode);

static inline int nvme_cmd_maxio_nvme_top_check_result(int fd, 
	uint32_t subcmd)
{
	return nvme_cmd_maxio_check_result_fmt1(fd, subcmd, 
		nvme_admin_maxio_nvme_top);
}

static inline int nvme_maxio_nvme_top_check_result(struct nvme_dev_info *ndev, 
	uint32_t subcmd)
{
	return nvme_maxio_check_result_fmt1(ndev, subcmd, 
		nvme_admin_maxio_nvme_top);
}

static inline int nvme_cmd_maxio_nvme_cqm_check_result(int fd, 
	uint32_t subcmd)
{
	return nvme_cmd_maxio_check_result_fmt1(fd, subcmd, 
		nvme_admin_maxio_nvme_cqm);
}

static inline int nvme_maxio_nvme_cqm_check_result(struct nvme_dev_info *ndev, 
	uint32_t subcmd)
{
	return nvme_maxio_check_result_fmt1(ndev, subcmd, 
		nvme_admin_maxio_nvme_cqm);
}

static inline int nvme_cmd_maxio_nvme_hwrdma_check_result(int fd, 
	uint32_t subcmd)
{
	return nvme_cmd_maxio_check_result_fmt1(fd, subcmd, 
		nvme_admin_maxio_nvme_hwrdma);
}

static inline int nvme_maxio_nvme_hwrdma_check_result(struct nvme_dev_info *ndev, 
	uint32_t subcmd)
{
	return nvme_maxio_check_result_fmt1(ndev, subcmd, 
		nvme_admin_maxio_nvme_hwrdma);
}

static inline int nvme_cmd_maxio_nvme_hwwdma_check_result(int fd, 
	uint32_t subcmd)
{
	return nvme_cmd_maxio_check_result_fmt1(fd, subcmd, 
		nvme_admin_maxio_nvme_hwwdma);
}

static inline int nvme_maxio_nvme_hwwdma_check_result(struct nvme_dev_info *ndev, 
	uint32_t subcmd)
{
	return nvme_maxio_check_result_fmt1(ndev, subcmd, 
		nvme_admin_maxio_nvme_hwwdma);
}
