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

struct nvme_zone_mgnt_send_wrapper {
	uint16_t	sqid;
	uint16_t	cqid;

	uint32_t	nsid;
	uint64_t	slba;
	uint8_t		action;

	uint32_t	select_all:1;

	void		*buf;
	uint32_t	size;
};

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

int nvme_cmd_zone_manage_send(int fd, struct nvme_zone_mgnt_send_wrapper *wrap);
int nvme_zone_manage_send(struct nvme_dev_info *ndev, 
				struct nvme_zone_mgnt_send_wrapper *wrap);

int nvme_cmd_zone_manage_receive(int fd, struct nvme_zone_mgnt_recv_wrapper *wrap);
int nvme_zone_manage_receive(struct nvme_dev_info *ndev, 
				struct nvme_zone_mgnt_recv_wrapper *wrap);

#endif /* !_UAPI_LIB_NVME_CMD_ZNS_H_ */
