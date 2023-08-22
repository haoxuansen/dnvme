/**
 * @file log_page.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-03-23
 * 
 * @copyright Copyright (c) 2023
 * 
 */

/**
 * @brief Log Page Identifiers
 * 
 * @note 
 * 	1. See "struct nvme_get_log_page_command -> lid" for details.
 * 	2. Refer to "NVM Express Base Specification R2.0b - Figure 202"
 */
enum {
	NVME_LOG_ERROR			= 0x01,
	NVME_LOG_SMART			= 0x02,
	NVME_LOG_FW_SLOT		= 0x03,
	NVME_LOG_CHANGED_NS		= 0x04,
	NVME_LOG_CMD_EFFECTS		= 0x05,
	NVME_LOG_DEVICE_SELF_TEST	= 0x06,
	NVME_LOG_TELEMETRY_HOST		= 0x07,
	NVME_LOG_TELEMETRY_CTRL		= 0x08,
	NVME_LOG_ENDURANCE_GROUP	= 0x09, /* R1.4 */
	NVME_LOG_LATENCY_NVM_SET	= 0x0a, /* R1.4 */
	NVME_LOG_LATENCY_EVENT		= 0x0b, /* R1.4 */
	NVME_LOG_ANA			= 0x0c, /* R1.4 */
	NVME_LOG_PERSISTENT_EVENT	= 0x0d, /* R1.4 */
	NVME_LOG_LBA_STATUS		= 0x0e, /* R1.4 */
	NVME_LOG_ENDURANCE_GROUP_EVENT	= 0x0f, /* R1.4 */
	NVME_LOG_MEDIA_UNIT_STATUS	= 0x10, /* R2.0 */
	NVME_LOG_CAP_CFG_LIST		= 0x11, /* R2.0 */
	NVME_LOG_FEAT_ID_EFFECTS	= 0x12, /* R2.0 */
	NVME_LOG_MI_CMD_EFFECTS		= 0x13, /* R2.0 */
	NVME_LOG_CMD_FEAT_LOCKDOWN	= 0x14, /* R2.0 */
	NVME_LOG_BOOT_PARTITION		= 0x15, /* R2.0 */
	NVME_LOG_ROTATIONAL_MEDIA	= 0x16, /* R2.0 */
	NVME_LOG_DISCOVERY		= 0x70,
	NVME_LOG_RESERVATION		= 0x80, /* R2.0 */
	NVME_LOG_SANITIZE_STATUS	= 0x81, /* R2.0 */
};

/**
 * @brief Log Specific Field
 * 
 * @note See "struct nvme_get_log_page_command -> lsp" for details.
 */
enum {
	NVME_ANA_LOG_RGO	= 1 << 0,
};

struct nvme_get_log_page_command {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__le32			nsid;
	__u64			rsvd2[2];
	union nvme_data_ptr	dptr;
	__u8			lid;
	__u8			lsp; /* upper 4 bits reserved */
	__le16			numdl;
	__le16			numdu;
	__u16			rsvd11;
	union {
		struct {
			__le32 lpol;
			__le32 lpou;
		};
		__le64 lpo;
	};
	__u8			rsvd14[3];
	__u8			csi;
	__u32			rsvd15;
};

/* ==================== NVME_LOG_ERROR(0x01) ==================== */

/**
 * @brief Error Information Log Entry
 * 
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 206"
 */
struct nvme_error_info_log {
	__le64		err_cnt;
	__le16		sqid;
	__le16		cmdid;
	__le16		status;
	__le16		pel;
	__le64		lba;
	__le32		nsid;
	__u8		vs;
	__u8		trtype;
	__u8		rsvd30[2];
	__le64		cs;
	__le16		tts;
	__u8		rsvd42[22];
};

/* ==================== NVME_LOG_SMART(0x02) ==================== */

/**
 * @note See "struct nvme_smart_log -> critical_warning" for details.
 */
enum {
	NVME_SMART_CRIT_SPARE		= 1 << 0,
	NVME_SMART_CRIT_TEMPERATURE	= 1 << 1,
	NVME_SMART_CRIT_RELIABILITY	= 1 << 2,
	NVME_SMART_CRIT_MEDIA		= 1 << 3,
	NVME_SMART_CRIT_VOLATILE_MEMORY	= 1 << 4,
};

/**
 * @brief SMART / Health Information
 * 
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 207"
 */
struct nvme_smart_log {
	__u8			critical_warning;
	__u8			temperature[2];
	__u8			avail_spare;
	__u8			spare_thresh;
	__u8			percent_used;
	__u8			endu_grp_crit_warn_sumry;
	__u8			rsvd7[25];
	__u8			data_units_read[16];
	__u8			data_units_written[16];
	__u8			host_reads[16];
	__u8			host_writes[16];
	__u8			ctrl_busy_time[16];
	__u8			power_cycles[16];
	__u8			power_on_hours[16];
	__u8			unsafe_shutdowns[16];
	__u8			media_errors[16];
	__u8			num_err_log_entries[16];
	__le32			warning_temp_time;
	__le32			critical_comp_time;
	__le16			temp_sensor[8];
	__le32			thm_temp1_trans_count;
	__le32			thm_temp2_trans_count;
	__le32			thm_temp1_total_time;
	__le32			thm_temp2_total_time;
	__u8			rsvd232[280];
};


/* ==================== NVME_LOG_FW_SLOT(0x03) ==================== */

/**
 * @brief Firmware Slot Information
 * 
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 209"
 */
struct nvme_fw_slot_info_log {
	__u8			afi;
	__u8			rsvd1[7];
	__le64			frs[7];
	__u8			rsvd64[448];
};

/* ==================== NVME_LOG_CHANGED_NS(0x04) ==================== */

#define NVME_MAX_CHANGED_NAMESPACES	1024


/* ==================== NVME_LOG_CMD_EFFECTS(0x05) ==================== */

enum {
	NVME_CMD_EFFECTS_CSUPP		= 1 << 0,
	NVME_CMD_EFFECTS_LBCC		= 1 << 1,
	NVME_CMD_EFFECTS_NCC		= 1 << 2,
	NVME_CMD_EFFECTS_NIC		= 1 << 3,
	NVME_CMD_EFFECTS_CCC		= 1 << 4,
	NVME_CMD_EFFECTS_CSE_MASK	= 3 << 16,
	NVME_CMD_EFFECTS_UUID_SEL	= 1 << 19,
};

/**
 * @brief Commands Supported and Effects
 * 
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 210"
 */
struct nvme_effects_log {
	__le32 acs[256];
	__le32 iocs[256];
	__u8   resv[2048];
};


/* ==================== NVME_LOG_ANA(0x0c) ==================== */

/**
 * @brief Asymmetric Namespace Access State
 * 
 * @note See "struct nvme_ana_group_desc -> state" for details.
 */
enum nvme_ana_state {
	NVME_ANA_OPTIMIZED		= 0x01,
	NVME_ANA_NONOPTIMIZED		= 0x02,
	NVME_ANA_INACCESSIBLE		= 0x03,
	NVME_ANA_PERSISTENT_LOSS	= 0x04,
	NVME_ANA_CHANGE			= 0x0f,
};

struct nvme_ana_group_desc {
	__le32	grpid;
	__le32	nnsids;
	__le64	chgcnt;
	__u8	state;
	__u8	rsvd17[15];
	__le32	nsids[];
};

/**
 * @brief Asymmetric Namespace Access Log
 */
struct nvme_ana_rsp_hdr {
	__le64	chgcnt;
	__le16	ngrps;
	__le16	rsvd10[3];
};

/* ==================== NVME_LOG_DISCOVERY(0x70) ==================== */

/**
 * @brief Transport Type
 * 
 * @note See "struct nvmf_disc_rsp_page_entry -> trtype" for details.
 */
enum {
	NVMF_TRTYPE_RDMA	= 1,	/* RDMA */
	NVMF_TRTYPE_FC		= 2,	/* Fibre Channel */
	NVMF_TRTYPE_TCP		= 3,	/* TCP/IP */
	NVMF_TRTYPE_LOOP	= 254,	/* Reserved for host usage */
	NVMF_TRTYPE_MAX,
};

/**
 * @brief Address Family
 * 
 * @note See "struct nvmf_disc_rsp_page_entry -> adrfam" for details.
 */
enum {
	NVMF_ADDR_FAMILY_IP4	= 1,	/* IP4 */
	NVMF_ADDR_FAMILY_IP6	= 2,	/* IP6 */
	NVMF_ADDR_FAMILY_IB	= 3,	/* InfiniBand */
	NVMF_ADDR_FAMILY_FC	= 4,	/* Fibre Channel */
	NVMF_ADDR_FAMILY_LOOP	= 254,	/* Reserved for host usage */
	NVMF_ADDR_FAMILY_MAX,
};

/**
 * @brief Transport Requirement
 * 
 * @note See "struct nvmf_disc_rsp_page_entry -> treq" for details.
 */
enum {
	NVME_TREQ_SECURE_CHANNEL_MASK = 0x3,
	NVMF_TREQ_NOT_SPECIFIED	= 0,		/* Not specified */
	NVMF_TREQ_REQUIRED	= 1,		/* Required */
	NVMF_TREQ_NOT_REQUIRED	= 2,		/* Not Required */

	NVMF_TREQ_DISABLE_SQFLOW = (1 << 2),	/* Supports SQ flow control disable */
};

/**
 * @brief Controller ID
 * 
 * @note
 *	1. See "struct nvmf_disc_rsp_page_entry -> cntlid" for details.
 *	2. cntlid of value 0 is considered illegal in the fabrics world.
 *	   Devices based on earlier specs did not have the subsystem concept;
 *	   therefore, those devices had their cntlid value set to 0 as a result.
 */
enum {
	NVME_CNTLID_MIN		= 1,
	NVME_CNTLID_MAX		= 0xffef,
	NVME_CNTLID_STATIC	= 0xfffe,
	NVME_CNTLID_DYNAMIC	= 0xffff,
};

/**
 * @brief RDMA QP Service Type
 * 
 * @note
 * 	1. See "struct nvmf_disc_rsp_page_entry -> tsas -> rdma -> qptype"
 * 	 for details.
 * 	2. Refer to "NVMe over Fabrics R1.1 - Figure 49"
 */
enum {
	NVMF_RDMA_QPTYPE_CONNECTED	= 1, /* Reliable Connected */
	NVMF_RDMA_QPTYPE_DATAGRAM	= 2, /* Reliable Datagram */
};

/**
 * @brief RDMA Provider Type
 *
 * @note
 *	1. See "struct nvmf_disc_rsp_page_entry -> tsas -> rdma -> prtype"
 *	 for details.
 *	2. Refer to "NVMe over Fabrics R1.1 - Figure 49"
 */
enum {
	NVMF_RDMA_PRTYPE_NOT_SPECIFIED	= 1, /* No Provider Specified */
	NVMF_RDMA_PRTYPE_IB		= 2, /* InfiniBand */
	NVMF_RDMA_PRTYPE_ROCE		= 3, /* InfiniBand RoCE */
	NVMF_RDMA_PRTYPE_ROCEV2		= 4, /* InfiniBand RoCEV2 */
	NVMF_RDMA_PRTYPE_IWARP		= 5, /* IWARP */
};

/**
 * @brief RDMA Connection Management Service
 * 
 * @note
 * 	1. See "struct nvmf_disc_rsp_page_entry -> tsas -> rdma -> cms"
 *	 for details.
 *	2. Refer to "NVMe over Fabrics R1.1 - Figure 49"
 */
enum {
	NVMF_RDMA_CMS_RDMA_CM	= 1, /* Sockets based endpoint addressing */
};

/**
 * @brief Discovery log page entry
 * 
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 264"
 */
struct nvmf_disc_rsp_page_entry {
	__u8		trtype;
	__u8		adrfam;
	__u8		subtype;
	__u8		treq;
	__le16		portid;
	__le16		cntlid;
	__le16		asqsz;
	__u8		resv8[22];
	char		trsvcid[32];
	__u8		resv64[192];
	char		subnqn[NVMF_NQN_FIELD_LEN];
	char		traddr[256];
	union tsas {
		char		common[256];
		struct rdma {
			__u8	qptype;
			__u8	prtype;
			__u8	cms;
			__u8	resv3[5];
			__u16	pkey;
			__u8	resv10[246];
		} rdma;
	} tsas;
};

/**
 * @brief Discovery log page header
 * 
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 265"
 */
struct nvmf_disc_rsp_page_hdr {
	__le64		genctr;
	__le64		numrec;
	__le16		recfmt;
	__u8		resv14[1006];
	struct nvmf_disc_rsp_page_entry entries[];
};
