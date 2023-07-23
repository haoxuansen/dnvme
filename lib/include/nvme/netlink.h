/**
 * @file netlink.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-04-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UAPI_LIB_NVME_NETLINK_H_
#define _UAPI_LIB_NVME_NETLINK_H_

#include <stdint.h>
#include "core.h"

int nvme_gnl_cmd_reap_cqe(struct nvme_dev_info *ndev, uint16_t cqid,
	uint32_t expect, void *buf, uint32_t size);

int nvme_gnl_connect(void);
void nvme_gnl_disconnect(int sockfd);

#endif /* !_UAPI_LIB_NVME_NETLINK_H_ */
