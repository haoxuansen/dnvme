/**
 * @file command.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief NVMe Admin & IO command
 * @version 0.1
 * @date 2023-01-13
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UAPI_NVME_COMMAND_H_
#define _UAPI_NVME_COMMAND_H_

/**
 * @brief Operation code for Admin Commands
 * 
 * @note All commands, including vendor specific commands, shall follow this
 *  convention:
 *    00b = no data transfer; 01b = host to controller;
 *    10b = controller to host; 11b = bidirectional;
 */
enum nvme_admin_opcode {
	nvme_admin_delete_sq		= 0x00,
	nvme_admin_create_sq		= 0x01,
	nvme_admin_get_log_page		= 0x02,
	nvme_admin_delete_cq		= 0x04,
	nvme_admin_create_cq		= 0x05,
	nvme_admin_identify		= 0x06,
	nvme_admin_abort_cmd		= 0x08,
	nvme_admin_set_features		= 0x09,
	nvme_admin_get_features		= 0x0a,
	nvme_admin_async_event		= 0x0c,
	nvme_admin_ns_mgmt		= 0x0d,
	nvme_admin_activate_fw		= 0x10,
	nvme_admin_download_fw		= 0x11,
	nvme_admin_dev_self_test	= 0x14,
	nvme_admin_ns_attach		= 0x15,
	nvme_admin_keep_alive		= 0x18,
	nvme_admin_directive_send	= 0x19,
	nvme_admin_directive_recv	= 0x1a,
	nvme_admin_virtual_mgmt		= 0x1c,
	nvme_admin_nvme_mi_send		= 0x1d,
	nvme_admin_nvme_mi_recv		= 0x1e,
	nvme_admin_dbbuf		= 0x7c,
	nvme_admin_format_nvm		= 0x80,
	nvme_admin_security_send	= 0x81,
	nvme_admin_security_recv	= 0x82,
	nvme_admin_sanitize_nvm		= 0x84,
	nvme_admin_get_lba_status	= 0x86,
	nvme_admin_vendor_start		= 0xc0,

	nvme_admin_vendor_write		= 0xc1,
	nvme_admin_vendor_read		= 0xc2,
	nvme_admin_vendor_fwdma_write	= 0xc3,
	nvme_admin_vendor_fwdma_read	= 0xc4,
	nvme_admin_vendor_para_set	= 0xc5,
};

/**
 * @brief Operation code for I/O Commands
 * 
 * @note All commands, including vendor specific commands, shall follow this
 *  convention:
 *    00b = no data transfer; 01b = host to controller;
 *    10b = controller to host; 11b = bidirectional;
 */
enum nvme_io_opcode {
	nvme_cmd_flush		= 0x00,
	nvme_cmd_write		= 0x01,
	nvme_cmd_read		= 0x02,
	nvme_cmd_write_uncor	= 0x04,
	nvme_cmd_compare	= 0x05,
	nvme_cmd_write_zeroes	= 0x08,
	nvme_cmd_dsm		= 0x09,
	nvme_cmd_verify		= 0x0c,
	nvme_cmd_resv_register	= 0x0d,
	nvme_cmd_resv_report	= 0x0e,
	nvme_cmd_resv_acquire	= 0x11,
	nvme_cmd_resv_release	= 0x15,
	nvme_cmd_zone_mgmt_send	= 0x79,
	nvme_cmd_zone_mgmt_recv	= 0x7a,
	nvme_cmd_zone_append	= 0x7d,
};

/* ~~~~~~~~~~~~~~~~~~~~ nvme_admin_opcode ~~~~~~~~~~~~~~~~~~~~ */

/* ==================== nvme_admin_delete_sq(0x00) ==================== */

/**
 * @brief Delete I/O Submisssion or Completion Queue Command
 */
struct nvme_delete_queue {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__u32			rsvd1[9];
	__le16			qid;
	__u16			rsvd10;
	__u32			rsvd11[5];
};

/* ==================== nvme_admin_create_sq(0x01) ==================== */

/**
 * @brief Queue Priority
 * 
 * @note See "struct nvme_create_sq -> sq_flags" for details.
 */
enum {
	NVME_SQ_PRIO_URGENT	= 0 << 1,
	NVME_SQ_PRIO_HIGH	= 1 << 1,
	NVME_SQ_PRIO_MEDIUM	= 2 << 1,
	NVME_SQ_PRIO_LOW	= 3 << 1,
};

/**
 * @brief Create I/O Submission Queue Command
 * 
 * @note Refer to "NVM Express Base Specification R2.0b - ch5.5"
 */
struct nvme_create_sq {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__u32			rsvd1[5];
	__le64			prp1;
	__u64			rsvd8;
	__le16			sqid;
	__le16			qsize;
	__le16			sq_flags;
	__le16			cqid;
	__u32			rsvd12[4];
};

/* ==================== nvme_admin_get_log_page(0x02) ==================== */
#include "nvme/log_page.h"

/* ==================== nvme_admin_delete_cq(0x04) ==================== */

/* ==================== nvme_admin_create_cq(0x05) ==================== */

/**
 * @note 
 * 	1. See "struct nvme_create_sq -> sq_flags" for details.
 * 	2. See "struct nvme_create_cq -> cq_flags" for details.
 */
enum {
	NVME_QUEUE_PHYS_CONTIG	= 1 << 0,
	NVME_CQ_IRQ_ENABLED	= 1 << 1,
};

/**
 * @brief Create I/O Completion Queue Command
 */
struct nvme_create_cq {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__u32			rsvd1[5];
	__le64			prp1;
	__u64			rsvd8;
	__le16			cqid;
	__le16			qsize;
	__le16			cq_flags;
	__le16			irq_vector;
	__u32			rsvd12[4];
};

/* ==================== nvme_admin_identify(0x06) ==================== */
#include "nvme/identify.h"

/* ==================== nvme_admin_abort_cmd(0x08) ==================== */

struct nvme_abort_cmd {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__u32			rsvd1[9];
	__le16			sqid;
	__u16			cid;
	__u32			rsvd11[5];
};

/* ==================== nvme_admin_set_features(0x09) ==================== */
/* ==================== nvme_admin_get_features(0x0a) ==================== */
#include "nvme/feature.h"

/* ==================== nvme_admin_download_fw(0x11) ==================== */

struct nvme_download_firmware {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__u32			rsvd1[5];
	union nvme_data_ptr	dptr;
	__le32			numd;
	__le32			offset;
	__u32			rsvd12[4];
};


/* ==================== nvme_admin_directive_send(0x19) ==================== */
/* ==================== nvme_admin_directive_recv(0x1a) ==================== */

/**
 * @brief Directive Types
 * 
 * @note See "struct nvme_directive_cmd -> dtype" for details.
 */
enum {
	NVME_DIR_IDENTIFY		= 0x00,
	NVME_DIR_STREAMS		= 0x01,
};

enum {
	NVME_DIR_SND_ID_OP_ENABLE	= 0x01,
	NVME_DIR_SND_ST_OP_REL_ID	= 0x01,
	NVME_DIR_SND_ST_OP_REL_RSC	= 0x02,
	NVME_DIR_RCV_ID_OP_PARAM	= 0x01,
	NVME_DIR_RCV_ST_OP_PARAM	= 0x01,
	NVME_DIR_RCV_ST_OP_STATUS	= 0x02,
	NVME_DIR_RCV_ST_OP_RESOURCE	= 0x03,
	NVME_DIR_ENDIR			= 0x01,
};

struct nvme_directive_cmd {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__le32			nsid;
	__u64			rsvd2[2];
	union nvme_data_ptr	dptr;
	__le32			numd;
	__u8			doper;
	__u8			dtype;
	__le16			dspec;
	__u8			endir;
	__u8			tdtype;
	__u16			rsvd15;

	__u32			rsvd16[3];
};


/* ==================== nvme_admin_format_nvm(0x80) ==================== */

#define NVME_FMT_LBAFL(x)	((x) & 0xf)
#define NVME_FMT_LBAFU(x)	(((x) & 0x30) << 8) /* bit[13:12] */

enum {
	NVME_FMT_MSET_EXT	= 1 << 4,
};

/**
 * @brief Secure Erase Settings
 * 
 * @note See "struct nvme_format_cmd -> cdw10" for details.
 */
enum {
	NVME_FMT_SES_MASK	= 7 << 9,
	NVME_FMT_SES_NONE	= 0 << 9,
	NVME_FMT_SES_USER	= 1 << 9,
	NVME_FMT_SES_CRYP	= 2 << 9,
};

struct nvme_format_cmd {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__le32			nsid;
	__u64			rsvd2[4];
	__le32			cdw10;
	__u32			rsvd11[5];
};

/* ~~~~~~~~~~~~~~~~~~~~ nvme_io_opcode ~~~~~~~~~~~~~~~~~~~~ */


/* ==================== nvme_cmd_flush(0x00) ==================== */

/* ==================== nvme_cmd_write(0x01) ==================== */
/* ==================== nvme_cmd_read(0x02) ==================== */

/**
 * @note See "struct nvme_rw_command -> control" for details.
 */
enum {
	NVME_RW_DTYPE_STREAMS		= 1 << 4,
	NVME_RW_PRINFO_PRCHK_REF	= 1 << 10,
	NVME_RW_PRINFO_PRCHK_APP	= 1 << 11,
	NVME_RW_PRINFO_PRCHK_GUARD	= 1 << 12,
	NVME_RW_PRINFO_PRACT		= 1 << 13,
	NVME_RW_FUA			= 1 << 14,
	NVME_RW_LR			= 1 << 15,
};

/**
 * @brief Dataset Management
 * 
 * @note See "struct nvme_rw_command -> dsmgmt" for details.
 */
enum {
	NVME_RW_DSM_FREQ_UNSPEC		= 0,
	NVME_RW_DSM_FREQ_TYPICAL	= 1,
	NVME_RW_DSM_FREQ_RARE		= 2,
	NVME_RW_DSM_FREQ_READS		= 3,
	NVME_RW_DSM_FREQ_WRITES		= 4,
	NVME_RW_DSM_FREQ_RW		= 5,
	NVME_RW_DSM_FREQ_ONCE		= 6,
	NVME_RW_DSM_FREQ_PREFETCH	= 7,
	NVME_RW_DSM_FREQ_TEMP		= 8,

	NVME_RW_DSM_LATENCY_NONE	= 0 << 4,
	NVME_RW_DSM_LATENCY_IDLE	= 1 << 4,
	NVME_RW_DSM_LATENCY_NORM	= 2 << 4,
	NVME_RW_DSM_LATENCY_LOW		= 3 << 4,

	NVME_RW_DSM_SEQ_REQ		= 1 << 6,
	NVME_RW_DSM_COMPRESSED		= 1 << 7,
};

struct nvme_rw_command {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__le32			nsid;
	__u64			rsvd2;
	__le64			metadata;
	union nvme_data_ptr	dptr;
	__le64			slba;
	__le16			length;
	__le16			control;
	__le32			dsmgmt;
	__le32			reftag;
	__le16			apptag;
	__le16			appmask;
};

/* ==================== nvme_cmd_write_zeroes(0x08) ==================== */

struct nvme_write_zeroes_cmd {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__le32			nsid;
	__u64			rsvd2;
	__le64			metadata;
	union nvme_data_ptr	dptr;
	__le64			slba;
	__le16			length;
	__le16			control;
	__le32			dsmgmt;
	__le32			reftag;
	__le16			apptag;
	__le16			appmask;
};

/* ==================== nvme_cmd_dsm(0x09) ==================== */

/**
 * @note See "struct nvme_dsm_cmd -> attributes" for details.
 */
enum {
	NVME_DSMGMT_IDR		= 1 << 0,
	NVME_DSMGMT_IDW		= 1 << 1,
	NVME_DSMGMT_AD		= 1 << 2,
};

/**
 * @brief Dataset Management Command
 */
struct nvme_dsm_cmd {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__le32			nsid;
	__u64			rsvd2[2];
	union nvme_data_ptr	dptr;
	__le32			nr;
	__le32			attributes;
	__u32			rsvd12[4];
};

#define NVME_DSM_MAX_RANGES	256

struct nvme_dsm_range {
	__le32			cattr;
	__le32			nlb;
	__le64			slba;
};


/* ==================== nvme_cmd_zone_mgmt_send(0x79) ==================== */

/**
 * @brief Zone Send Action
 * 
 * @note See "struct nvme_zone_mgmt_send_cmd -> zsa" for details.
 */
enum nvme_zone_mgmt_action {
	NVME_ZONE_CLOSE		= 0x1,
	NVME_ZONE_FINISH	= 0x2,
	NVME_ZONE_OPEN		= 0x3,
	NVME_ZONE_RESET		= 0x4,
	NVME_ZONE_OFFLINE	= 0x5,
	NVME_ZONE_SET_DESC_EXT	= 0x10,
};

struct nvme_zone_mgmt_send_cmd {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__le32			nsid;
	__le32			cdw2[2];
	__le64			metadata;
	union nvme_data_ptr	dptr;
	__le64			slba;
	__le32			cdw12;
	__u8			zsa;
	__u8			select_all;
	__u8			rsvd13[2];
	__le32			cdw14[2];
};

/* ==================== nvme_cmd_zone_mgmt_recv(0x7a) ==================== */

/**
 * @brief Zone Receive Action
 * 
 * @note See "struct nvme_zone_mgmt_recv_cmd -> zra" for details.
 */
enum {
	NVME_ZRA_ZONE_REPORT		= 0,
};

/**
 * @brief Zone Receive Action Specific
 * 
 * @note See "struct nvme_zone_mgmt_recv_cmd -> zrasf" for details.
 */
enum {
	NVME_ZRASF_ZONE_REPORT_ALL	= 0,
};

/**
 * @brief Zone Receive Action Specific Features
 * 
 * @note See "struct nvme_zone_mgmt_recv_cmd -> pr" for details.
 */
enum {
	NVME_REPORT_ZONE_PARTIAL	= 1,
};

/**
 * @brief Zone Management Receive Command
 * 
 * @note Refer to "NVM Express Zoned Namespace Command Set Specification 
 * 	R1.1c - ch3.4.2"
 */
struct nvme_zone_mgmt_recv_cmd {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__le32			nsid;
	__le64			rsvd2[2];
	union nvme_data_ptr	dptr;
	__le64			slba;
	__le32			numd;
	__u8			zra;
	__u8			zrasf;
	__u8			pr;
	__u8			rsvd13;
	__le32			cdw14[2];
};

/**
 * @brief Zone Type
 * 
 * @note See "struct nvme_zone_descriptor -> zt" for details.
 */
enum {
	NVME_ZONE_TYPE_SEQWRITE_REQ	= 0x2,
};

/**
 * @brief Zone Descriptor Data Structure
 * 
 * @note Refer to "NVM Express Zoned Namespace Command Set Specification 
 * 	R1.1c - ch3.4.2.2.3"
 */
struct nvme_zone_descriptor {
	__u8		zt;
	__u8		zs;
	__u8		za;
	__u8		rsvd3[5];
	__le64		zcap;
	__le64		zslba;
	__le64		wp;
	__u8		rsvd32[32];
};

struct nvme_zone_report {
	__le64		nr_zones;
	__u8		resv8[56];
	struct nvme_zone_descriptor entries[];
};

/* ==================== nvme_cmd_zone_append(0x7d) ==================== */

/**
 * @note See "struct nvme_rw_command -> control" for details.
 */
enum {
	NVME_RW_APPEND_PIREMAP		= 1 << 9,
};

#endif /* !_UAPI_NVME_COMMAND_H_ */
