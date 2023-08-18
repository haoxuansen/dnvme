/**
 * @file cmd_zns.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-08-18
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UAPI_LIB_NVME_CMD_ZNS_H_
#define _UAPI_LIB_NVME_CMD_ZNS_H_

struct nvme_zone_mgnt_recv_wrapper {
	uint16_t	sqid;
	uint16_t	cqid;

	uint32_t	nsid;
	uint64_t	slba;
	uint8_t		action;
	uint8_t		specific;

	uint32_t	partial_report:1;

	void		*buf;
	uint32_t	size;
};

#endif /* !_UAPI_LIB_NVME_CMD_ZNS_H_ */
