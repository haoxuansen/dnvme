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

#ifndef _UAPI_LIB_NVME_CMD_VENDOR_H_
#define _UAPI_LIB_NVME_CMD_VENDOR_H_

int nvme_cmd_maxio_nvme_cqm_set_param(int fd, uint32_t bitmap, uint32_t param);
int nvme_maxio_nvme_cqm_set_param(struct nvme_dev_info *ndev,
	uint32_t bitmap, uint32_t param);

int nvme_cmd_maxio_nvme_cqm_check_result(int fd, uint32_t bitmap);
int nvme_maxio_nvme_cqm_check_result(struct nvme_dev_info *ndev, 
	uint32_t bitmap);

#endif /* !_UAPI_LIB_NVME_CMD_VENDOR_H_ */
