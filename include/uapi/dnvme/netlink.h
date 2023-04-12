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

#ifndef _UAPI_DNVME_NETLINK_H_
#define _UAPI_DNVME_NETLINK_H_

enum {
	NVME_GNL_CMD_UNSPEC,
	NVME_GNL_CMD_REAP_CQE,
	__NVME_GNL_CMD_MAX,
};

#define NVME_GNL_CMD_MAX		(__NVME_GNL_CMD_MAX - 1)

/**
 * @brief Generic Netlink Attribute
 */
enum {
	NVME_GNL_ATTR_UNSPEC,
	NVME_GNL_ATTR_TIMEOUT, /* unit:ms */
	NVME_GNL_ATTR_DEVNO,
	NVME_GNL_ATTR_SQID,
	NVME_GNL_ATTR_CQID,
	NVME_GNL_ATTR_OPT_NUM,
	NVME_GNL_ATTR_OPT_DATA,
	NVME_GNL_ATTR_OPT_BUF_PTR,
	NVME_GNL_ATTR_OPT_BUF_SIZE,
	NVME_GNL_ATTR_OPT_STATUS,
	__NVME_GNL_ATTR_MAX,
};

#define NVME_GNL_ATTR_MAX		(__NVME_GNL_ATTR_MAX - 1)

struct nvme_gnl_test {
	uint16_t	cqid;
	uint16_t	expect;
};

struct nvme_gnl_reap_cqe {
	void		*buf;
	uint32_t	size;
};

#endif /* !_UAPI_DNVME_NETLINK_H_ */
