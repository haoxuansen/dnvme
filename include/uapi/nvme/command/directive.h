/**
 * @file directive.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-04-23
 * 
 * @copyright Copyright (c) 2023
 * 
 */

/**
 * @brief Directive Types
 * 
 * @note See "struct nvme_directive_cmd -> dtype" for details.
 */
enum {
	NVME_DIR_IDENTIFY		= 0x00,
	NVME_DIR_STREAMS		= 0x01,
};

/**
 * @brief Directive Operation
 * 
 * @note See "struct nvme_directive_cmd -> dspec" for details.
 */
enum {
	/* Identify - Directive Operations */
	NVME_DIR_SND_ID_OP_ENABLE	= 0x01, /* enable directive */
	NVME_DIR_RCV_ID_OP_PARAM	= 0x01, /* return parameters */

	/* Streams - Directive Operations */
	NVME_DIR_SND_ST_OP_REL_ID	= 0x01, /* release identifier */
	NVME_DIR_SND_ST_OP_REL_RSC	= 0x02, /* release resources */
	NVME_DIR_RCV_ST_OP_PARAM	= 0x01, /* return parameters */
	NVME_DIR_RCV_ST_OP_STATUS	= 0x02, /* get status */
	NVME_DIR_RCV_ST_OP_RESOURCE	= 0x03, /* allocate resources */
};

/**
 * @brief Enable Directive
 * 
 * @note See "struct nvme_directive_cmd -> endir" for details.
 */
enum {
	NVME_DIR_ENDIR			= 0x01,
};

struct nvme_directive_cmd {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__le32			nsid;
	__u64			rsvd2[2];
	union nvme_data_ptr	dptr;
	__le32			numd; /* 0's based value */
	__u8			doper;
	__u8			dtype;
	__le16			dspec;

	union {
		struct {
			__u8	endir;
			__u8	tdtype;
		};
		__le16		nsr; /* namespace streams requested */
		__le32		u32;
	} dw12;

	__u32			rsvd13[3];
};

/* ==================== NVME_DIR_RCV_ID_OP_PARAM ==================== */

/**
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 419"
 */
struct nvme_dir_identify_params {
	__u8	dir_sup[32];
	__u8	dir_en[32];
	__u8	rsvd[4032];
};


/* ==================== NVME_DIR_RCV_ST_OP_PARAM ==================== */

/**
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 425"
 */
struct nvme_dir_streams_params {
	__le16	msl;
	__le16	nssa;
	__le16	nsso;
	__u8	rsvd[10];
	__le32	sws;
	__le16	sgs;
	__le16	nsa;
	__le16	nso;
	__u8	rsvd2[6];
};
