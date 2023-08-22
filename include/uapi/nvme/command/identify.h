/**
 * @file identify.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-03-21
 * 
 * @copyright Copyright (c) 2023
 * 
 */

/**
 * @brief Controller or Namespace Structure
 * 
 * @note
 * 	1. See "struct nvme_identify -> cns" for details.
 * 	2. Refer to "NVM Express Base Specification R2.0b - Figure 273"
 */
enum {
	NVME_ID_CNS_NS			= 0x00,
	NVME_ID_CNS_CTRL		= 0x01,
	NVME_ID_CNS_NS_ACTIVE_LIST	= 0x02,
	NVME_ID_CNS_NS_DESC_LIST	= 0x03,
	NVME_ID_CNS_NVM_SET_LIST	= 0x04, /* R1.4 */
	NVME_ID_CNS_CS_NS_ACTIVE	= 0x05, /* R2.0 */
	NVME_ID_CNS_CS_CTRL		= 0x06, /* R2.0 */
	NVME_ID_CNS_CS_NS_ACTIVE_LIST	= 0x07, /* R2.0 */
	NVME_ID_CNS_NS_INDEPENDENT	= 0x08, /* R2.0 */
	NVME_ID_CNS_NS_PRESENT_LIST	= 0x10,
	NVME_ID_CNS_NS_PRESENT		= 0x11,
	NVME_ID_CNS_CTRL_NS_LIST	= 0x12,
	NVME_ID_CNS_CTRL_LIST		= 0x13,
	NVME_ID_CNS_PRMRY_CTRL_CAP	= 0x14,
	NVME_ID_CNS_SCNDRY_CTRL_LIST	= 0x15,
	NVME_ID_CNS_NS_GRANULARITY	= 0x16, /* R1.4 */
	NVME_ID_CNS_UUID_LIST		= 0x17, /* R1.4 */
	NVME_ID_CNS_DOMAIN_LIST		= 0x18, /* R2.0 */
	NVME_ID_CNS_EGRP_LIST		= 0x19, /* R2.0 */
	NVME_ID_CNS_CS_NS_ALLOC_LIST	= 0x1a, /* R2.0 */
	NVME_ID_CNS_CS_NS_ALLOC		= 0x1b, /* R2.0 */
	NVME_ID_CNS_CTRL_CSC_LIST	= 0x1c, /* R2.0 */
};

/**
 * @brief Command Set Identifier
 * 
 * @note 
 * 	1. See "struct nvme_identify -> csi" for details.
 * 	2. Refer to "NVM Express Base Specification R2.0b - Figure 274"
 */
enum {
	NVME_CSI_NVM		= 0x00,
	NVME_CSI_KEY		= 0x01,
	NVME_CSI_ZNS		= 0x02,
};

struct nvme_identify {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__le32			nsid;
	__u64			rsvd2[2];
	union nvme_data_ptr	dptr;
	__u8			cns;
	__u8			rsvd3;
	__le16			ctrlid;
	__u8			rsvd11[3];
	__u8			csi;
	__u32			rsvd12[4];
};

#define NVME_IDENTIFY_DATA_SIZE 4096

/* ==================== NVME_ID_CNS_NS(0x00) ==================== */

/**
 * @brief Namespace Features
 * 
 * @note See "struct nvme_id_ns -> nsfeat" for details.
 */
enum {
	NVME_NS_FEAT_THIN	= 1 << 0,
	NVME_NS_FEAT_ATOMICS	= 1 << 1,
	NVME_NS_FEAT_IO_OPT	= 1 << 4,
};

/**
 * @brief Formatted LBA Size
 * 
 * @note See "struct nvme_id_ns -> flbas" for details.
 */
#define NVME_NS_FLBAS_LBA(x)		((((x) & 0x60) >> 1) | ((x) & 0xf))

enum {
	NVME_NS_FLBAS_META_EXT	= 0x10,
};

/**
 * @brief Metadata Capabilities
 * 
 * @note See "struct nvme_id_ns -> mc" for details.
 */
enum {
	NVME_MC_EXTENDED_LBA	= 1 << 0,
	NVME_MC_METADATA_PTR	= 1 << 1,
};

/**
 * @brief End-to-end Data Protection Capabilities
 * 
 * @note See "struct nvme_id_ns -> dpc" for details.
 */
enum {
	NVME_NS_DPC_PI_TYPE1	= 1 << 0,
	NVME_NS_DPC_PI_TYPE2	= 1 << 1,
	NVME_NS_DPC_PI_TYPE3	= 1 << 2,
	NVME_NS_DPC_PI_FIRST	= 1 << 3,
	NVME_NS_DPC_PI_LAST	= 1 << 4,
};

/**
 * @brief End-to-end Data Protection Type Settings
 * 
 * @note See "struct nvme_id_ns -> dps" for details.
 */
enum {
	NVME_NS_DPS_PI_MASK	= 0x7,
	NVME_NS_DPS_PI_TYPE1	= 1,
	NVME_NS_DPS_PI_TYPE2	= 2,
	NVME_NS_DPS_PI_TYPE3	= 3,
	NVME_NS_DPS_PI_FIRST	= 1 << 3,
};

/**
 * @brief Namespace Multi-path I/O and Namespace Sharing Capabilities
 * 
 * @note
 * 	1. See "struct nvme_id_ns -> nmic" for details.
 * 	2. Refer to "NVM Express Base Specification R2.0b - Figure 280"
 */
enum {
	NVME_NS_NMIC_SHARED	= 1 << 0,
};

/**
 * @brief Namespace Features
 * 
 * @note
 * 	1. See "struct nvme_id_ns -> nsattr" for details.
 * 	2. Refer to "NVM Express Base Specification R2.0b - Figure 280"
 */
enum {
	NVME_NS_ATTR_RO		= 1 << 0,
};

/**
 * @brief Relative Performance
 * 
 * @note See "struct nvme_lbaf -> rp" for details.
 */
enum {
	NVME_LBAF_RP_MASK	= 3,
	NVME_LBAF_RP_BEST	= 0,
	NVME_LBAF_RP_BETTER	= 1,
	NVME_LBAF_RP_GOOD	= 2,
	NVME_LBAF_RP_DEGRADED	= 3,
};

/**
 * @brief LBA Format Data Structure
 *
 * @ms: Metadata Size, indicates the number of metadata bytes provided 
 * 	per LBA based on the LBA Data Size indicated.
 * 
 * @note Refer to "NVM Express NVM Command Set Specification R1.0b - Figure 276"
 */
struct nvme_lbaf {
	__le16			ms;
	__u8			ds;
	__u8			rp;
};

/**
 * @brief NVM Command Set Identify Namespace Data Structure (CNS 00h)
 * 
 * @note Refer to "NVM Express NVM Command Set Specification R1.0b - ch4.1.5.1"
 */
struct nvme_id_ns {
	__le64			nsze;
	__le64			ncap;
	__le64			nuse;
	__u8			nsfeat;
	__u8			nlbaf; /* 0's based */
	__u8			flbas;
	__u8			mc;
	__u8			dpc;
	__u8			dps;
	__u8			nmic;
	__u8			rescap;
	__u8			fpi;
	__u8			dlfeat;
	__le16			nawun;
	__le16			nawupf;
	__le16			nacwu;
	__le16			nabsn;
	__le16			nabo;
	__le16			nabspf;
	__le16			noiob;
	__u8			nvmcap[16];
	__le16			npwg;
	__le16			npwa;
	__le16			npdg;
	__le16			npda;
	__le16			nows;
	__le16			mssrl;
	__le32			mcl;
	__u8			msrc; /* 0'based */
	__u8			rsvd81[11];
	__le32			anagrpid;
	__u8			rsvd96[3];
	__u8			nsattr;
	__le16			nvmsetid;
	__le16			endgid;
	__u8			nguid[16];
	__u8			eui64[8];
	struct nvme_lbaf	lbaf[64];
	__u8			vs[3712];
};

/* ==================== NVME_ID_CNS_CTRL(0x01) ==================== */

/**
 * @brief Controller Multi-Path I/O and Namespace Sharing Capabilities
 * 
 * @note See "struct nvme_id_ctrl -> cmic" for details.
 */
enum {
	NVME_CTRL_CMIC_MULTI_CTRL		= 1 << 1,
	NVME_CTRL_CMIC_ANA			= 1 << 3,
};

/**
 * @brief Controller Attributes
 * 
 * @note See "struct nvme_id_ctrl -> ctratt" for details.
 */
enum {
	NVME_CTRL_CTRATT_128_ID			= 1 << 0,
	NVME_CTRL_CTRATT_NON_OP_PSP		= 1 << 1,
	NVME_CTRL_CTRATT_NVM_SETS		= 1 << 2,
	NVME_CTRL_CTRATT_READ_RECV_LVLS		= 1 << 3,
	NVME_CTRL_CTRATT_ENDURANCE_GROUPS	= 1 << 4,
	NVME_CTRL_CTRATT_PREDICTABLE_LAT	= 1 << 5,
	NVME_CTRL_CTRATT_TBKAS			= 1 << 6,
	NVME_CTRL_CTRATT_NAMESPACE_GRANULARITY	= 1 << 7,
	NVME_CTRL_CTRATT_UUID_LIST		= 1 << 9,
};

/**
 * @brief Optional Admin Command Support
 * 
 * @note See "struct nvme_id_ctrl -> oacs" for details.
 */
enum {
	NVME_CTRL_OACS_SEC_SUPP                 = 1 << 0,
	NVME_CTRL_OACS_DIRECTIVES		= 1 << 5,
	NVME_CTRL_OACS_DBBUF_SUPP		= 1 << 8,
};

/**
 * @brief Log Page Attributes
 * 
 * @note See "struct nvme_id_ctrl -> lpa" for details.
 */
enum {
	NVME_CTRL_LPA_CMD_EFFECTS_LOG		= 1 << 1,
};

/**
 * @brief Optional NVM Command Support
 * 
 * @note See "struct nvme_id_ctrl -> oncs" for details.
 */
enum {
	NVME_CTRL_ONCS_COMPARE			= 1 << 0,
	NVME_CTRL_ONCS_WRITE_UNCORRECTABLE	= 1 << 1,
	NVME_CTRL_ONCS_DSM			= 1 << 2,
	NVME_CTRL_ONCS_WRITE_ZEROES		= 1 << 3,
	NVME_CTRL_ONCS_RESERVATIONS		= 1 << 5,
	NVME_CTRL_ONCS_TIMESTAMP		= 1 << 6,
	NVME_CTRL_ONCS_VERIFY			= 1 << 7,
	NVME_CTRL_ONCS_COPY			= 1 << 8,
};

/**
 * @brief Optional NVM Command Support
 * 
 * @note See "struct nvme_id_ctrl -> fuses" for details.
 */
enum {
	NVME_CTRL_FUSES_COMPARE_WRITE	= 1 << 0,
};

/**
 * @brief Volatile Write Cache
 * 
 * @note See "struct nvme_id_ctrl -> vwc" for details.
 */
enum {
	NVME_CTRL_VWC_PRESENT		= 1 << 0,
};


/**
 * @brief Namespace Write Protection Capabilities
 * 
 * @note See "struct nvme_id_ctrl -> nwpc" for details.
 */
enum {
	NVME_CTRL_NWPC_WP	= 1 << 0,
	NVME_CTRL_NWPC_WPUPC	= 1 << 1,
	NVME_CTRL_NWPC_PWP	= 1 << 2,
};

/**
 * @brief SGL Support
 * 
 * @note See "struct nvme_id_ctrl -> sgls" for details.
 */
#define NVME_CTRL_SGLS_SDT(sgls)	(((sgls) >> 8) & 0xff)

enum {
	NVME_CTRL_SGLS_MASK		= 3,
	NVME_CTRL_SGLS_UNSUPPORT	= 0,
	NVME_CTRL_SGLS_SUPPORT		= 1,
	NVME_CTRL_SGLS_SUPPORT4		= 2, /* 4B align */

	NVME_CTRL_SGLS_KEYED_DATA_BLOCK	= 1 << 2, /* desc */
	NVME_CTRL_SGLS_BIT_BUCKET	= 1 << 16, /* desc */
	NVME_CTRL_SGLS_META_BYTE	= 1 << 17,
	NVME_CTRL_SGLS_LARGER_DATA	= 1 << 18,
	NVME_CTRL_SGLS_MPTR_SGL		= 1 << 19,
	NVME_CTRL_SGLS_ADDR_OFFSET	= 1 << 20,
	NVME_CTRL_SGLS_DATA_BLOCK	= 1 << 21, /* desc */
};

/**
 * @note See "struct nvme_id_power_state -> flags" for details.
 */
enum {
	NVME_PS_FLAGS_MAX_POWER_SCALE	= 1 << 0,
	NVME_PS_FLAGS_NON_OP_STATE	= 1 << 1,
};


/**
 * @brief Power State Descriptor Data Structure
 * 
 * @note Refer to "NVMe Base Spec R2.0b - Figure 276"
 */
struct nvme_id_power_state {
	__le16			max_power;	/* centiwatts */
	__u8			rsvd2;
	__u8			flags;
	__le32			entry_lat;	/* microseconds */
	__le32			exit_lat;	/* microseconds */
	__u8			read_tput;
	__u8			read_lat;
	__u8			write_tput;
	__u8			write_lat;
	__le16			idle_power;
	__u8			idle_scale;
	__u8			rsvd19;
	__le16			active_power;
	__u8			active_work_scale;
	__u8			rsvd23[9];
};

/**
 * @brief Identify Controller Data Structure
 * 
 * @note Refer to "NVMe Base Spec R2.0b - Figure 275"
 */
struct nvme_id_ctrl {
	__le16			vid;	/* PCI Vendor ID */
	__le16			ssvid;
	char			sn[20];
	char			mn[40];
	char			fr[8];
	__u8			rab;
	__u8			ieee[3];
	__u8			cmic;
	__u8			mdts;
	__le16			cntlid;
	__le32			ver;
	__le32			rtd3r;
	__le32			rtd3e;
	__le32			oaes;
	__le32			ctratt;
	__u8			rsvd100[28];
	__le16			crdt1;
	__le16			crdt2;
	__le16			crdt3;
	__u8			rsvd134[122];
	__le16			oacs;
	__u8			acl;
	__u8			aerl;
	__u8			frmw;
	__u8			lpa;
	__u8			elpe;
	__u8			npss;
	__u8			avscc;
	__u8			apsta;
	__le16			wctemp;
	__le16			cctemp;
	__le16			mtfa;
	__le32			hmpre;
	__le32			hmmin;
	__u8			tnvmcap[16];
	__u8			unvmcap[16];
	__le32			rpmbs;
	__le16			edstt;
	__u8			dsto;
	__u8			fwug;
	__le16			kas;
	__le16			hctma;
	__le16			mntmt;
	__le16			mxtmt;
	__le32			sanicap;
	__le32			hmminds;
	__le16			hmmaxd;
	__u8			rsvd338[4];
	__u8			anatt;
	__u8			anacap;
	__le32			anagrpmax;
	__le32			nanagrpid;
	__u8			rsvd352[160];
	__u8			sqes;
	__u8			cqes;
	__le16			maxcmd;
	__le32			nn;
	__le16			oncs;
	__le16			fuses;
	__u8			fna;
	__u8			vwc;
	__le16			awun;
	__le16			awupf;
	__u8			nvscc;
	__u8			nwpc;
	__le16			acwu;
	__u8			rsvd534[2];
	__le32			sgls;
	__le32			mnan;
	__u8			rsvd544[224];
	char			subnqn[256];
	__u8			rsvd1024[768];
	__le32			ioccsz;
	__le32			iorcsz;
	__le16			icdoff;
	__u8			ctrattr;
	__u8			msdbd;
	__u8			rsvd1804[244];
	struct nvme_id_power_state	psd[32];
	__u8			vs[1024];
};


/* ==================== NVME_ID_CNS_NS_ACTIVE_LIST(0x02) ==================== */

/* ==================== NVME_ID_CNS_NS_DESC_LIST(0x03) ==================== */

/**
 * @brief Namespace Identifier Type
 * 
 * @note See "struct nvme_ns_id_desc -> nidt" for details.
 */
#define NVME_NIDT_EUI64_LEN	8
#define NVME_NIDT_NGUID_LEN	16
#define NVME_NIDT_UUID_LEN	16
#define NVME_NIDT_CSI_LEN	1

enum {
	NVME_NIDT_EUI64		= 0x01,
	NVME_NIDT_NGUID		= 0x02,
	NVME_NIDT_UUID		= 0x03,
	NVME_NIDT_CSI		= 0x04,
	NVME_NIDT_FENCE		= 0x05,
};

/**
 * @brief Namespace Identification Descriptor list (CNS 03h)
 * 
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 277"
 */
struct nvme_ns_id_desc {
	__u8	nidt;
	__u8	nidl;
	__le16	reserved;
	__u8	nid[0];
};


/* ==================== NVME_ID_CNS_CS_NS_ACTIVE(0x05) ==================== */

/**
 * @brief Extended LBA Format Data Structure
 */
struct nvme_nvm_elbaf {
	__le32			dw0;
};

/**
 * @brief I/O Command Set Specific Identify Namesapce Data Structure 
 *	for the NVM Command Set (CNS 05h, CSI 00h)
 * 
 * @note Refer to "NVM Express NVM Command Set Specification - Figure 100"
 */
struct nvme_id_ns_nvm {
	__le64			lbstm;
	__u8			pic;
	__u8			rsvd9[3];
	struct nvme_nvm_elbaf	elbaf[64];
	__u8			rsvd268[3828];
};

/**
 * @brief LBA Format Extension Data Structure
 */
struct nvme_zns_lbafe {
	__le64			zsze;
	__u8			zdes; /* in 64B units */
	__u8			rsvd9[7];
};

/**
 * @brief I/O Command Set Specific Identify Namesapce Data Structure 
 *	for the Zoned Namespace Command Set (CNS 05h, CSI 02h)
 * 
 * @note Refer to "NVM Express Zoned Namespace Command Set Specification
 * 	R1.1c - ch4.1.5.1"
 */
struct nvme_id_ns_zns {
	__le16			zoc;
	__le16			ozcs;
	__le32			mar;
	__le32			mor;
	__le32			rrl;
	__le32			frl;
	__le32			rrl1;
	__le32			rrl2;
	__le32			rrl3;
	__le32			frl1;
	__le32			frl2;
	__le32			frl3;
	__u8			rsvd44[2772];
	struct nvme_zns_lbafe	lbafe[64];
	__u8			vs[256];
};


/* ==================== NVME_ID_CNS_CS_CTRL(0x06) ==================== */

/**
 * @brief I/O Command Set Specific Identify Controller Data Structure
 * 	for the NVM Command Set
 * 
 * @note Refer to "NVM Express NVM Command Set Specification - Figure 102"
 */
struct nvme_id_ctrl_nvm {
	__u8	vsl;
	__u8	wzsl;
	__u8	wusl;
	__u8	dmrl;
	__le32	dmrsl;
	__le64	dmsl;
	__u8	rsvd16[4080];
};

/**
 * @brief I/O Command Set Specific Identify Controller Data Structure
 * 	for the Zoned Namespace Command Set
 * 
 * @note Refer to "NVM Express Zoned Namespace Command Set Specification
 * 	R1.1c - ch4.1.5.2"
 */
struct nvme_id_ctrl_zns {
	__u8	zasl;
	__u8	rsvd1[4095];
};

/* ==================== NVME_ID_CNS_CTRL_NS_LIST(0x12) ==================== */

/* ==================== NVME_ID_CNS_CTRL_LIST(0x13) ==================== */

/* ==================== NVME_ID_CNS_CTRL_CSC_LIST(0x1c) ==================== */

/**
 * @brief Identify I/O Command Set Data Structure
 */
struct nvme_id_ctrl_csc {
	__le64	vector[512];
};
