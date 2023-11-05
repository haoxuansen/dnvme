/**
 * @file directive.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-04-27
 * 
 * @copyright Copyright (c) 2023
 * 
 */

int nvme_cmd_dir_rcv_id_param(struct nvme_dev_info *ndev, 
	struct nvme_dir_identify_params *param, uint32_t nsid);

int nvme_cmd_dir_snd_id_enable(struct nvme_dev_info *ndev, 
	uint32_t nsid, uint8_t type, bool enable);

int nvme_cmd_dir_rcv_st_param(struct nvme_dev_info *ndev,
	struct nvme_dir_streams_params *param, uint32_t nsid);

int nvme_cmd_dir_rcv_st_status(struct nvme_dev_info *ndev,
	void *buf, uint32_t size, uint32_t nsid);

int nvme_cmd_dir_rcv_st_resource(struct nvme_dev_info *ndev,
	uint32_t nsid, uint16_t req);

int nvme_cmd_dir_snd_st_rel_id(struct nvme_dev_info *ndev,
	uint32_t nsid, uint16_t id);

int nvme_cmd_dir_snd_st_rel_rsc(struct nvme_dev_info *ndev,
	uint32_t nsid);


int nvme_dir_rcv_id_param(struct nvme_dev_info *ndev, 
	struct nvme_dir_identify_params *param, uint32_t nsid);

int nvme_dir_snd_id_enable(struct nvme_dev_info *ndev, 
	uint32_t nsid, uint8_t type, bool enable);

int nvme_dir_rcv_st_param(struct nvme_dev_info *ndev,
	struct nvme_dir_streams_params *param, uint32_t nsid);

int nvme_dir_rcv_st_status(struct nvme_dev_info *ndev,
	void *buf, uint32_t size, uint32_t nsid);

int nvme_dir_rcv_st_resource(struct nvme_dev_info *ndev,
	uint32_t nsid, uint16_t req);

int nvme_dir_snd_st_rel_id(struct nvme_dev_info *ndev,
	uint32_t nsid, uint16_t id);

int nvme_dir_snd_st_rel_rsc(struct nvme_dev_info *ndev,
	uint32_t nsid);
