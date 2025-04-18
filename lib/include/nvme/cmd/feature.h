/**
 * @file feature.h
 * @author yeqiang_xu (yeqiang_xu@maxio-tech.com)
 * @brief 
 * @version 0.1
 * @date 2023-11-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

struct nvme_feat_arb_wrapper {
	uint8_t		hpw; /**< High Priority Weight */
	uint8_t		mpw;
	uint8_t		lpw;
	uint8_t		burst;
};

int nvme_cmd_set_feature(int fd, uint32_t nsid, uint32_t fid, uint32_t dw11);
int nvme_cmd_get_feature(int fd, uint32_t nsid, uint32_t fid, uint32_t dw11);;

static inline int nvme_cmd_set_feat_arbitration(int fd, 
	struct nvme_feat_arb_wrapper *wrap)
{
	return nvme_cmd_set_feature(fd, 0, NVME_FEAT_ARBITRATION,
		((uint32_t)wrap->hpw << 24) | ((uint32_t)wrap->mpw << 16) | 
		((uint32_t)wrap->lpw << 8) | ((uint32_t)wrap->burst & 0x7));
}

int nvme_set_feat_arbitration(struct nvme_dev_info *ndev, 
	struct nvme_feat_arb_wrapper *wrap);

/**
 * @brief Set feature named power management
 * 
 * @param ps Power State
 * @param wh Workload Hint
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
static inline int nvme_cmd_set_feat_power_mgmt(int fd, uint8_t ps, uint8_t wh)
{
	return nvme_cmd_set_feature(fd, 0, NVME_FEAT_POWER_MGMT, 
		NVME_POWER_MGMT_FOR_WH(wh) | NVME_POWER_MGMT_FOR_PS(ps));
}
int nvme_set_feat_power_mgmt(struct nvme_dev_info *ndev, uint8_t ps, uint8_t wh);

static inline int nvme_cmd_get_feat_power_mgmt(int fd, uint32_t sel)
{
	return nvme_cmd_get_feature(fd, 0, sel | NVME_FEAT_POWER_MGMT, 0);
}

static inline int nvme_cmd_set_feat_num_queues(int fd, uint16_t nr_sq, 
	uint16_t nr_cq)
{
	return nvme_cmd_set_feature(fd, 0, NVME_FEAT_NUM_QUEUES, 
		((uint32_t)nr_cq << 16) | nr_sq);
}

int nvme_cmd_set_feat_hmb(int fd, struct nvme_hmb_wrapper *wrap);
int nvme_set_feat_hmb(struct nvme_dev_info *ndev, struct nvme_hmb_wrapper *wrap);

int nvme_cmd_get_feat_hmb(int fd, struct nvme_hmb_wrapper *wrap);
int nvme_get_feat_hmb(struct nvme_dev_info *ndev, struct nvme_hmb_wrapper *wrap);

int nvme_cmd_set_feat_host_behavior(int fd, uint32_t sel, 
	struct nvme_feat_host_behavior *behavior);
int nvme_set_feat_host_behavior(struct nvme_dev_info *ndev, uint32_t sel, 
	struct nvme_feat_host_behavior *behavior);

int nvme_cmd_get_feat_host_behavior(int fd, uint32_t sel,
	struct nvme_feat_host_behavior *behavior);
int nvme_get_feat_host_behavior(struct nvme_dev_info *ndev, uint32_t sel,
	struct nvme_feat_host_behavior *behavior);

static inline int nvme_cmd_set_feat_iocs_profile(int fd, uint32_t sel, 
	uint32_t index)
{
	return nvme_cmd_set_feature(fd, 0, sel | NVME_FEAT_IOCS_PROFILE, index);
}
int nvme_set_feat_iocs_profile(struct nvme_dev_info *ndev, uint32_t sel, 
	uint32_t index);

static inline int nvme_cmd_get_feat_iocs_profile(int fd, uint32_t sel)
{
	return nvme_cmd_get_feature(fd, 0, sel | NVME_FEAT_IOCS_PROFILE, 0);
}
int nvme_get_feat_iocs_profile(struct nvme_dev_info *ndev, uint32_t sel);

static inline int nvme_cmd_set_feat_write_protect(int fd, uint32_t nsid, 
	uint32_t sel, uint32_t state)
{
	return nvme_cmd_set_feature(fd, nsid, sel | NVME_FEAT_WRITE_PROTECT, state);
}
int nvme_set_feat_write_protect(struct nvme_dev_info *ndev, uint32_t nsid, 
	uint32_t sel, uint32_t state);

static inline int nvme_cmd_get_feat_write_protect(int fd, uint32_t nsid, 
	uint32_t sel)
{
	return nvme_cmd_get_feature(fd, nsid, sel | NVME_FEAT_WRITE_PROTECT, 0);
}
int nvme_get_feat_write_protect(struct nvme_dev_info *ndev, uint32_t nsid, 
	uint32_t sel);

