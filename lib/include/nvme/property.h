/**
 * @file property.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-11-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */


static inline bool nvme_cap_support_discontig_queue(struct nvme_ctrl_instance *ctrl)
{
	return NVME_CAP_CQR(ctrl->prop->cap) ? false : true;
}

/**
 * @brief Get the minimum host memory page size(in Byte) that the controller
 *  supports.
 */
static inline uint32_t nvme_cap_mem_page_size_min(struct nvme_ctrl_instance *ctrl)
{
	return 1 << (12 + NVME_CAP_MPSMIN(ctrl->prop->cap));
}

/**
 * @brief Get the maximum host memory page size(in Byte) that the controller
 *  supports.
 */
static inline uint32_t nvme_cap_mem_page_size_max(struct nvme_ctrl_instance *ctrl)
{
	return 1 << (12 + NVME_CAP_MPSMAX(ctrl->prop->cap));
}

static inline uint32_t nvme_version(struct nvme_ctrl_instance *ctrl)
{
	return ctrl->prop->vs;
}
