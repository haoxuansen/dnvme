/**
 * @file feature.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief Feature Identifier, definations and data structures related to
 *  Set Feature Command and Get Feature Command
 * @version 0.1
 * @date 2023-01-13
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UAPI_NVME_FEATURE_H_
#define _UAPI_NVME_FEATURE_H_

/**
 * @brief Feature Identifiers
 */
enum {
	NVME_FEAT_ARBITRATION	= 0x01,
	NVME_FEAT_POWER_MGMT	= 0x02, /* Power Management */
	NVME_FEAT_LBA_RANGE	= 0x03,
	NVME_FEAT_TEMP_THRESH	= 0x04,
	NVME_FEAT_ERR_RECOVERY	= 0x05,
	NVME_FEAT_VOLATILE_WC	= 0x06,
	NVME_FEAT_NUM_QUEUES	= 0x07,
	NVME_FEAT_IRQ_COALESCE	= 0x08,
	NVME_FEAT_IRQ_CONFIG	= 0x09,
	NVME_FEAT_WRITE_ATOMIC	= 0x0a,
	NVME_FEAT_ASYNC_EVENT	= 0x0b,
	NVME_FEAT_AUTO_PST	= 0x0c,
	NVME_FEAT_HOST_MEM_BUF	= 0x0d,
	NVME_FEAT_TIMESTAMP	= 0x0e,
	NVME_FEAT_KATO		= 0x0f,
	NVME_FEAT_HCTM		= 0x10,
	NVME_FEAT_NOPSC		= 0x11,
	NVME_FEAT_RRL		= 0x12,
	NVME_FEAT_PLM_CONFIG	= 0x13,
	NVME_FEAT_PLM_WINDOW	= 0x14,
	NVME_FEAT_HOST_BEHAVIOR	= 0x16,
	NVME_FEAT_SANITIZE	= 0x17,
	NVME_FEAT_SW_PROGRESS	= 0x80,
	NVME_FEAT_HOST_ID	= 0x81,
	NVME_FEAT_RESV_MASK	= 0x82,
	NVME_FEAT_RESV_PERSIST	= 0x83,
	NVME_FEAT_WRITE_PROTECT	= 0x84,
	NVME_FEAT_VENDOR_START	= 0xC0,
	NVME_FEAT_VENDOR_END	= 0xFF,
};


/* ==================== NVME_FEAT_POWER_MGMT ==================== */

/* bit[7:5] Workload Hint */
#define NVME_POWER_MGMT_FOR_WH(wh)	(((wh) & 0x7) << 5)
#define NVME_POWER_MGMT_TO_WH(dw)	(((dw) >> 5) & 0x7)
/* bit[4:0] Power State */
#define NVME_POWER_MGMT_FOR_PS(ps)	(((ps) & 0x1f) << 0)
#define NVME_POWER_MGMT_TO_PS(dw)	(((dw) >> 0) & 0x1f)

#endif /* !_UAPI_NVME_FEATURE_H_ */
