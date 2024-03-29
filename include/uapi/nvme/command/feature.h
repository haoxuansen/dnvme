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

/**
 * @brief Feature Identifiers
 * 
 * @note See "struct nvme_features -> fid" for details.
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
	NVME_FEAT_RRL		= 0x12, /* R1.4 */
	NVME_FEAT_PLM_CONFIG	= 0x13, /* R1.4 */
	NVME_FEAT_PLM_WINDOW	= 0x14, /* R1.4 */
	NVME_FEAT_LBA_STS_RI	= 0x15, /* R1.4 */
	NVME_FEAT_HOST_BEHAVIOR	= 0x16, /* R1.4 */
	NVME_FEAT_SANITIZE	= 0x17, /* R1.4 */
	NVME_FEAT_EGRP_EVENT	= 0x18, /* R1.4 */
	NVME_FEAT_IOCS_PROFILE	= 0x19, /* R2.0 */
	NVME_FEAT_SPINUP_CTRL	= 0x1a, /* R2.0 */
	NVME_FEAT_ECTRL_META	= 0x7d, /* R2.0 */
	NVME_FEAT_CTRL_META	= 0x7e, /* R2.0 */
	NVME_FEAT_NS_META	= 0x7f, /* R2.0 */
	NVME_FEAT_SW_PROGRESS	= 0x80, /* R2.0 */
	NVME_FEAT_HOST_ID	= 0x81, /* R2.0 */
	NVME_FEAT_RESV_MASK	= 0x82, /* R2.0 */
	NVME_FEAT_RESV_PERSIST	= 0x83, /* R2.0 */
	NVME_FEAT_WRITE_PROTECT	= 0x84, /* R2.0 */
	NVME_FEAT_VENDOR_START	= 0xC0,
	NVME_FEAT_VENDOR_END	= 0xFF,
};

/**
 * @brief Select
 * 
 * @note See "struct nvme_features -> fid" for details.
 */
enum {
	NVME_FEAT_SEL_CUR	= 0 << 8,
	NVME_FEAT_SEL_DEF	= 1 << 8,
	NVME_FEAT_SEL_SAVED	= 2 << 8,
	NVME_FEAT_SEL_SUP_CAP	= 3 << 8,
};

struct nvme_features {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__le32			nsid;
	__u64			rsvd2[2];
	union nvme_data_ptr	dptr;
	__le32			fid;
	__le32			dword11;
	__le32                  dword12;
	__le32                  dword13;
	__le32                  dword14;
	__le32                  dword15;
};

/* ==================== NVME_FEAT_POWER_MGMT(0x02) ==================== */

/* bit[7:5] Workload Hint */
#define NVME_POWER_MGMT_FOR_WH(wh)	(((wh) & 0x7) << 5)
#define NVME_POWER_MGMT_TO_WH(dw)	(((dw) >> 5) & 0x7)
/* bit[4:0] Power State */
#define NVME_POWER_MGMT_FOR_PS(ps)	(((ps) & 0x1f) << 0)
#define NVME_POWER_MGMT_TO_PS(dw)	(((dw) >> 0) & 0x1f)

/* ==================== NVME_FEAT_LBA_RANGE(0x03) ==================== */

/**
 * @note See "struct nvme_lba_range_type -> type" for details
 */
enum {
	NVME_LBART_TYPE_GENERAL	= 0x00, /* General Purpose */
	NVME_LBART_TYPE_FS	= 0x01, /* Filesystem */
	NVME_LBART_TYPE_RAID	= 0x02,
	NVME_LBART_TYPE_CACHE	= 0x03,
	NVME_LBART_TYPE_SWAP	= 0x04,
};

/**
 * @note See "struct nvme_lba_range_type -> attr" for details
 */
enum {
	NVME_LBART_ATTR_TEMP	= 1 << 0,
	NVME_LBART_ATTR_HIDE	= 1 << 1,
};

/**
 * @brief LBA Range Type - Data Structure Entry
 * 
 * @note Refer to "NVM Express NVM Command Set Specification R1.0b - Figure 83"
 */
struct nvme_feat_lba_range_type {
	__u8			type;
	__u8			attr;
	__u8			rsvd2[14];
	__le64			slba;
	__le64			nlb;
	__u8			guid[16];
	__u8			rsvd48[16];
};


/* ==================== NVME_FEAT_TEMP_THRESH(0x04) ==================== */

enum {
	NVME_TEMP_THRESH_MASK		= 0xffff,
	NVME_TEMP_THRESH_SELECT_SHIFT	= 16,
	NVME_TEMP_THRESH_TYPE_UNDER	= 0x100000,
};


/* ==================== NVME_FEAT_ASYNC_EVENT(0x0b) ==================== */

enum {
	NVME_AEN_BIT_NS_ATTR		= 8,
	NVME_AEN_BIT_FW_ACT		= 9,
	NVME_AEN_BIT_ANA_CHANGE		= 11,
	NVME_AEN_BIT_DISC_CHANGE	= 31,
};

/**
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 326"
 */
enum {
	/* Namespace Attribute Notices */
	NVME_AEN_CFG_NS_ATTR		= 1 << NVME_AEN_BIT_NS_ATTR,
	/* Firmware Activation Notices */
	NVME_AEN_CFG_FW_ACT		= 1 << NVME_AEN_BIT_FW_ACT,
	/* Asymmetric Namespace Access Change Notices */
	NVME_AEN_CFG_ANA_CHANGE		= 1 << NVME_AEN_BIT_ANA_CHANGE,
	/* Discovery Log Page Change Notification */
	NVME_AEN_CFG_DISC_CHANGE	= 1 << NVME_AEN_BIT_DISC_CHANGE,
};

/* ==================== NVME_FEAT_AUTO_PST(0x0c) ==================== */

/**
 * @brief Autonomous Power State Transition
 * 
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 328"
 */
struct nvme_feat_auto_pst {
	__le64 entries[32];
};

/* ==================== NVME_FEAT_HOST_MEM_BUF(0x0d) ==================== */

enum {
	NVME_HOST_MEM_ENABLE	= 1 << 0,
	NVME_HOST_MEM_RETURN	= 1 << 1,
};


/**
 * @brief Host Memory Buffer Descriptor
 * 
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 336"
 */
struct nvme_feat_hmb_descriptor {
	__le64			addr;
	__le32			size; ///< unit: PAGE_SIZE(CC.MPS)
	__u8			rsvd12[4];
};

/**
 * @brief Host Memory Buffer - Attribute Data Structure
 * 
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 338"
 */
struct nvme_feat_hmb_attribute {
	__le32		hsize; ///< unit: PAGE_SIZE(CC.MPS)
	__le32		hmdlal;
	__le32		hmdlau;
	__le32		hmdlec;
	__u8		rsvd16[4080];
};

/* ==================== NVME_FEAT_HOST_BEHAVIOR(0x16) ==================== */

/**
 * @brief Advanced Command Retry Enable
 * 
 * @note See "struct nvme_feat_host_behavior -> acre" for details.
 */
enum {
	NVME_ENABLE_ACRE	= 1,
};

struct nvme_feat_host_behavior {
	__u8	acre;
	__u8	etdas;
	__u8	lbafee;
	__u8	resv1[509];
};


/* ==================== NVME_FEAT_IOCS_PROFILE(0x19) ==================== */

#define NVME_IOCS_CI_MASK		0x1ff


/* ==================== NVME_FEAT_WRITE_PROTECT(0x84) ==================== */

#define NVME_NS_WPS_MASK		0x7

/**
 * @brief Write Protect State
 */
enum {
	NVME_NS_NO_WRITE_PROTECT = 0,
	NVME_NS_WRITE_PROTECT,
	NVME_NS_WRITE_PROTECT_POWER_CYCLE,
	NVME_NS_WRITE_PROTECT_PERMANENT,
};
