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
	nvme_admin_cap_mgmt		= 0x20,
	nvme_admin_lockdown		= 0x24,
	nvme_admin_dbbuf		= 0x7c,
	nvme_admin_fabrics		= 0x7f,
	nvme_admin_format_nvm		= 0x80,
	nvme_admin_security_send	= 0x81,
	nvme_admin_security_recv	= 0x82,
	nvme_admin_sanitize_nvm		= 0x84,
	nvme_admin_get_lba_status	= 0x86,
	nvme_admin_vendor_start		= 0xc0,

	nvme_admin_maxio_write		= 0xc1,
	nvme_admin_maxio_read		= 0xc2,
	nvme_admin_maxio_fwdma_write	= 0xc3,
	nvme_admin_maxio_fwdma_read	= 0xc4,
	nvme_admin_maxio_param_set	= 0xc5,
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
	nvme_cmd_copy		= 0x19,
	nvme_cmd_zone_mgmt_send	= 0x79,
	nvme_cmd_zone_mgmt_recv	= 0x7a,
	nvme_cmd_zone_append	= 0x7d,
};

/*
 * Lowest two bits of our flags field (FUSE field in the spec):
 *
 * @NVME_CMD_FUSE_FIRST:   Fused Operation, first command
 * @NVME_CMD_FUSE_SECOND:  Fused Operation, second command
 *
 * Highest two bits in our flags field (PSDT field in the spec):
 *
 * @NVME_CMD_PSDT_SGL_METABUF:	Use SGLS for this transfer,
 *	If used, MPTR contains addr of single physical buffer (byte aligned).
 * @NVME_CMD_PSDT_SGL_METASEG:	Use SGLS for this transfer,
 *	If used, MPTR contains an address of an SGL segment containing
 *	exactly 1 SGL descriptor (qword aligned).
 *
 * @note See "struct nvme_common_command -> flags" for details.
 */
enum {
	NVME_CMD_FUSE_FIRST	= (1 << 0),
	NVME_CMD_FUSE_SECOND	= (1 << 1),

	NVME_CMD_SGL_METABUF	= (1 << 6),
	NVME_CMD_SGL_METASEG	= (1 << 7),
	NVME_CMD_SGL_ALL	= NVME_CMD_SGL_METABUF | NVME_CMD_SGL_METASEG,
};

/*
 * Descriptor subtype - lower 4 bits of nvme_(keyed_)sgl_desc identifier
 *
 * @NVME_SGL_FMT_ADDRESS:     absolute address of the data block
 * @NVME_SGL_FMT_OFFSET:      relative offset of the in-capsule data block
 * @NVME_SGL_FMT_TRANSPORT_A: transport defined format, value 0xA
 * @NVME_SGL_FMT_INVALIDATE:  RDMA transport specific remote invalidation
 *                            request subtype
 */
enum {
	NVME_SGL_FMT_ADDRESS		= 0x00,
	NVME_SGL_FMT_OFFSET		= 0x01,
	NVME_SGL_FMT_TRANSPORT_A	= 0x0A,
	NVME_SGL_FMT_INVALIDATE		= 0x0f,
};

/*
 * Descriptor type - upper 4 bits of nvme_(keyed_)sgl_desc identifier
 *
 * For struct nvme_sgl_desc:
 *   @NVME_SGL_TYPE_DATA_DESC:		data block descriptor
 *   @NVME_SGL_TYPE_SEG_DESC:		sgl segment descriptor
 *   @NVME_SGL_TYPE_LAST_SEG_DESC:	last sgl segment descriptor
 *
 * For struct nvme_keyed_sgl_desc:
 *   @NVME_SGL_TYPE_KEY_DATA_DESC:	keyed data block descriptor
 *
 * Transport-specific SGL types:
 *   @NVME_SGL_TYPE_TRANSPORT_DATA_DESC:	Transport SGL data dlock descriptor
 */
enum {
	NVME_SGL_TYPE_DATA_DESC			= 0x0,
	NVME_SGL_TYPE_BIT_BUCKET_DESC		= 0x1,
	NVME_SGL_TYPE_SEG_DESC			= 0x2,
	NVME_SGL_TYPE_LAST_SEG_DESC		= 0x3,
	NVME_SGL_TYPE_KEY_DATA_DESC		= 0x4,
	NVME_SGL_TYPE_TRANSPORT_DATA_DESC	= 0x5,
	NVME_SGL_TYPE_VENDOR_DESC		= 0xf,
};

struct nvme_sgl_desc {
	__le64	addr;
	__le32	length;
	__u8	rsvd[3];
	__u8	type;
};

struct nvme_keyed_sgl_desc {
	__le64	addr;
	__u8	length[3];
	__u8	key[4];
	__u8	type;
};

union nvme_data_ptr {
	struct {
		__le64	prp1;
		__le64	prp2;
	};
	struct nvme_sgl_desc	sgl;
	struct nvme_keyed_sgl_desc ksgl;
};

struct nvme_common_command {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__le32			nsid;
	__le32			cdw2[2];
	__le64			metadata;
	union nvme_data_ptr	dptr;
	__le32			cdw10;
	__le32			cdw11;
	__le32			cdw12;
	__le32			cdw13;
	__le32			cdw14;
	__le32			cdw15;
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

/* ==================== nvme_admin_async_event(0x0c) ==================== */

/**
 * @brief Asynchronous Event Type
 *
 * @note Refer to "NVM Express Base Specification R2.0b - ch5.2"
 */
enum nvme_async_event_type {
	NVME_AER_TYPE_ERROR	= 0, /* Error event */
	NVME_AER_TYPE_SMART	= 1, /* SMART / Health Status event */
	NVME_AER_TYPE_NOTICE	= 2, /* Notice event */
};

/**
 * @brief Asynchronous Event Type
 *
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 143"
 */
enum {
	NVME_AER_ERROR		= 0, /* Error Status */
	NVME_AER_SMART		= 1, /* SMART / Health status */
	NVME_AER_NOTICE		= 2,
	NVME_AER_CSS		= 6, /* I/O Command specific status */
	NVME_AER_VS		= 7, /* Vendor specific */
};

/**
 * @brief Asynchronous Event Information - Notice
 * 
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 146"
 */
enum {
	NVME_AER_NOTICE_NS_CHANGED	= 0x00, /* Namespace Attribute Changed */
	NVME_AER_NOTICE_FW_ACT_STARTING = 0x01, /* Firmware Activation Starting */
	NVME_AER_NOTICE_ANA		= 0x03, /* Asymmetric Namespace Access Change */
	NVME_AER_NOTICE_ZONE_CHANGED	= 0xef, /* Zone Descriptor Changed */
	NVME_AER_NOTICE_DISC_CHANGED	= 0xf0, /* Discovery Log Page Change */
};

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
#include "nvme/directive.h"

/* ==================== nvme_admin_dbbuf(0x7c) ==================== */

/**
 * @brief Doorbell Buffer Config
 */
struct nvme_dbbuf {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__u32			rsvd1[5];
	__le64			prp1;
	__le64			prp2;
	__u32			rsvd12[6];
};

/* ==================== nvme_admin_fabrics(0x7f) ==================== */
#include "nvme/fabrics.h"

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
	NVME_RW_FUA			= 1 << 14, /* Force Unit Access */
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

/* ==================== nvme_cmd_resv_report(0x0e) ==================== */

/**
 * @brief Registered Controller Data Structure
 * 
 * @note
 * 	1. See "struct nvme_resv_status -> regctl" for details.
 * 	2. Refer to "NVM Express Base Specification R2.0b - Figure 406"
 */
struct nvme_reg_ctrl {
	__le16	cntlid;
	__u8	rcsts;
	__u8	rsvd3[5];
	__le64	hostid;
	__le64	rkey;
};

/**
 * @brief Registered Controller Extended Data Structure
 * 
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 407"
 */
struct nvme_reg_ctrl_extend {
	__le16	cntlid;
	__u8	rcsts;
	__u8	rsvd3[5];
	__le64	rkey;
	__u8	hostid[16];
	__u8	rsvd32[32];
};

/**
 * @brief Reservation Status Data Structure
 * 
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 404"
 */
struct nvme_resv_status {
	__le32			gen;
	__u8			rtype;
	__u8			regctl[2];
	__u8			rsvd7[2];
	__u8			ptpls;
	__u8			rsvd10[14];
	struct nvme_reg_ctrl	regctl_ds[0];
};

/**
 * @brief Reservation Status Data Structure
 * 
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 405"
 */
struct nvme_resv_status_extend {
	__le32			gen;
	__u8			rtype;
	__u8			regctl[2];
	__u8			rsvd7[2];
	__u8			ptpls;
	__u8			rsvd10[54];
	struct nvme_reg_ctrl_extend	regctl_ds[0];
};


/* ==================== nvme_cmd_copy(0x19) ==================== */

/**
 * @brief Descriptor Format
 * 
 * @note See "struct nvme_copy_cmd -> desc_fmt" for details.
 */
enum {
	NVME_COPY_DESC_FMT_32B	= 0,
	NVME_COPY_DESC_FMT_40B	= 1,
};

struct nvme_copy_desc_fmt0 {
	__le64			rsvd0;
	__le64			slba;
	__le16			length; /* 0's based */
	__le16			rsvd4;
	__le32			rsvd5;
	__u8			tag[4];
	__le16			elbat;
	__le16			elbatm;
};

struct nvme_copy_desc_fmt1 {
	__le64			rsvd0;
	__le64			slba;
	__le16			length; /* 0's based */
	__le16			rsvd4;
	__le32			rsvd5;
	__le16			rsvd6;
	__u8			tag[10];
	__le16			elbat;
	__le16			elbatm;
};

/**
 * @brief Copy Command
 */
struct nvme_copy_cmd {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__le32			nsid;
	__le32			cdw2[2];
	__le64			metadata;
	union nvme_data_ptr	dptr;
	__le64			slba;
	__u8			ranges; /* 0'based */
	__u8			desc_fmt;
	__u8			dtype;
	__u8			control;
	__u8			rsvd13[2];
	__le16			dspec;
	__le32			cdw14;
	__le16			lbat;
	__le16			lbatm;
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
	NVME_ZRA_EXT_ZONE_REPORT	= 1,
};

/**
 * @brief Zone Receive Action Specific
 * 
 * @note See "struct nvme_zone_mgmt_recv_cmd -> zrasf" for details.
 */
enum {
	NVME_ZRASF_ZONE_REPORT_ALL	= 0,
	NVME_ZRASF_ZONE_REPORT_ZSE	= 1,
	NVME_ZRASF_ZONE_REPORT_ZSIO	= 2,
	NVME_ZRASF_ZONE_REPORT_ZSEO	= 3,
	NVME_ZRASF_ZONE_REPORT_ZSC	= 4,
	NVME_ZRASF_ZONE_REPORT_ZSF	= 5,
	NVME_ZRASF_ZONE_REPORT_ZSRO	= 6,
	NVME_ZRASF_ZONE_REPORT_ZSO	= 7,
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
	__le32			numd; /* 0's based */
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

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

struct nvme_command {
	union {
		struct nvme_common_command common;
		struct nvme_rw_command rw;
		struct nvme_identify identify;
		struct nvme_features features;
		struct nvme_create_cq create_cq;
		struct nvme_create_sq create_sq;
		struct nvme_delete_queue delete_queue;
		struct nvme_download_firmware dlfw;
		struct nvme_format_cmd format;
		struct nvme_dsm_cmd dsm;
		struct nvme_write_zeroes_cmd write_zeroes;
		struct nvme_zone_mgmt_send_cmd zms;
		struct nvme_zone_mgmt_recv_cmd zmr;
		struct nvme_abort_cmd abort;
		struct nvme_get_log_page_command get_log_page;
		struct nvmf_common_command fabrics;
		struct nvmf_connect_command connect;
		struct nvmf_property_set_command prop_set;
		struct nvmf_property_get_command prop_get;
		struct nvme_dbbuf dbbuf;
		struct nvme_directive_cmd directive;
	};
};

/**
 * @brief Status Code
 * 
 * @note See "struct nvme_completion -> status" for details.
 */
enum {
	/*
	 * Generic Command Status:
	 */
	NVME_SC_SUCCESS			= 0x0,
	NVME_SC_INVALID_OPCODE		= 0x1,
	NVME_SC_INVALID_FIELD		= 0x2,
	NVME_SC_CMDID_CONFLICT		= 0x3,
	NVME_SC_DATA_XFER_ERROR		= 0x4,
	NVME_SC_POWER_LOSS		= 0x5,
	NVME_SC_INTERNAL		= 0x6,
	NVME_SC_ABORT_REQ		= 0x7,
	NVME_SC_ABORT_QUEUE		= 0x8,
	NVME_SC_FUSED_FAIL		= 0x9,
	NVME_SC_FUSED_MISSING		= 0xa,
	NVME_SC_INVALID_NS		= 0xb,
	NVME_SC_CMD_SEQ_ERROR		= 0xc,
	NVME_SC_SGL_INVALID_LAST	= 0xd,
	NVME_SC_SGL_INVALID_COUNT	= 0xe,
	NVME_SC_SGL_INVALID_DATA	= 0xf,
	NVME_SC_SGL_INVALID_METADATA	= 0x10,
	NVME_SC_SGL_INVALID_TYPE	= 0x11,
	NVME_SC_CMB_INVALID_USE		= 0x12,
	NVME_SC_PRP_INVALID_OFFSET	= 0x13,
	NVME_SC_ATOMIC_WU_EXCEEDED	= 0x14,
	NVME_SC_OP_DENIED		= 0x15,
	NVME_SC_SGL_INVALID_OFFSET	= 0x16,
	/* 17h - Reserved */
	NVME_SC_HOST_ID_INCONSIST	= 0x18,
	NVME_SC_KA_TIMEOUT_EXPIRED	= 0x19,
	NVME_SC_KA_TIMEOUT_INVALID	= 0x1A,
	NVME_SC_ABORTED_PREEMPT_ABORT	= 0x1B,
	NVME_SC_SANITIZE_FAILED		= 0x1C,
	NVME_SC_SANITIZE_IN_PROGRESS	= 0x1D,
	NVME_SC_SGL_INVALID_GRANULARITY	= 0x1E,
	NVME_SC_CMD_NOT_SUP_CMB_QUEUE	= 0x1F,
	NVME_SC_NS_WRITE_PROTECTED	= 0x20,
	NVME_SC_CMD_INTERRUPTED		= 0x21,
	NVME_SC_TRANSIENT_TRANSPORT_ERR	= 0x22,
	NVME_SC_PROHIBIT_BY_LOCKDOWN	= 0x23,
	NVME_SC_ADMIN_CMD_MEDIA_NOTRDY	= 0x24,
	/* 25h to 7Fh - Reserved */

	NVME_SC_LBA_RANGE		= 0x80,
	NVME_SC_CAP_EXCEEDED		= 0x81,
	NVME_SC_NS_NOT_READY		= 0x82,
	NVME_SC_RESERVATION_CONFLICT	= 0x83,
	NVME_SC_FORMAT_IN_PROGRESS	= 0x84,
	NVME_SC_VALUE_SIZE_INVALID	= 0x85,
	NVME_SC_KEY_SIZE_INVALID	= 0x86,
	NVME_SC_KV_KEY_NOT_EXIST	= 0x87,
	NVME_SC_UNRECOVER_ERR		= 0x88,
	NVME_SC_KEY_EXISTS		= 0x89,
	/* 90h to BFh - Reserved */
	/* C0h to FFh - Vendor Specific */

	/*
	 * Command Specific Status:
	 */
	NVME_SC_CQ_INVALID		= 0x100,
	NVME_SC_QID_INVALID		= 0x101,
	NVME_SC_QUEUE_SIZE		= 0x102,
	NVME_SC_ABORT_LIMIT		= 0x103,
	NVME_SC_ABORT_MISSING		= 0x104,
	NVME_SC_ASYNC_LIMIT		= 0x105,
	NVME_SC_FIRMWARE_SLOT		= 0x106,
	NVME_SC_FIRMWARE_IMAGE		= 0x107,
	NVME_SC_INVALID_VECTOR		= 0x108,
	NVME_SC_INVALID_LOG_PAGE	= 0x109,
	NVME_SC_INVALID_FORMAT		= 0x10a,
	NVME_SC_FW_NEEDS_CONV_RESET	= 0x10b,
	NVME_SC_INVALID_QUEUE		= 0x10c,
	NVME_SC_FEATURE_NOT_SAVEABLE	= 0x10d,
	NVME_SC_FEATURE_NOT_CHANGEABLE	= 0x10e,
	NVME_SC_FEATURE_NOT_PER_NS	= 0x10f,
	NVME_SC_FW_NEEDS_SUBSYS_RESET	= 0x110,
	NVME_SC_FW_NEEDS_RESET		= 0x111,
	NVME_SC_FW_NEEDS_MAX_TIME	= 0x112,
	NVME_SC_FW_ACTIVATE_PROHIBITED	= 0x113,
	NVME_SC_OVERLAPPING_RANGE	= 0x114,
	NVME_SC_NS_INSUFFICIENT_CAP	= 0x115,
	NVME_SC_NS_ID_UNAVAILABLE	= 0x116,
	NVME_SC_NS_ALREADY_ATTACHED	= 0x118,
	NVME_SC_NS_IS_PRIVATE		= 0x119,
	NVME_SC_NS_NOT_ATTACHED		= 0x11a,
	NVME_SC_THIN_PROV_NOT_SUPP	= 0x11b,
	NVME_SC_CTRL_LIST_INVALID	= 0x11c,
	NVME_SC_SELT_TEST_IN_PROGRESS	= 0x11d,
	NVME_SC_BP_WRITE_PROHIBITED	= 0x11e,
	NVME_SC_CTRL_ID_INVALID		= 0x11f,
	NVME_SC_SEC_CTRL_STATE_INVALID	= 0x120,
	NVME_SC_CTRL_RES_NUM_INVALID	= 0x121,
	NVME_SC_RESOURCE_ID_INVALID	= 0x122,
	NVME_SC_PMR_SAN_PROHIBITED	= 0x123,
	NVME_SC_ANA_GROUP_ID_INVALID	= 0x124,
	NVME_SC_ANA_ATTACH_FAIL		= 0x125,
	NVME_SC_CAP_INSUFFICIENT	= 0x126,
	NVME_SC_NS_EXCEED_ATTACH_LIMIT	= 0x127,
	NVME_SC_CMD_NOTSUP_PROHIBIT	= 0x128,
	NVME_SC_IOCMD_SET_NOTSUP	= 0x129,
	NVME_SC_IOCMD_SET_NOTEN		= 0x12a,
	NVME_SC_IOCMD_SET_REJECT_COMB	= 0x12b,
	NVME_SC_IOCMD_SET_INVALID	= 0x12c,
	NVME_SC_ID_UNAVAILABLE		= 0x12d,
	/* 12Eh to 16Fh - Reserved */
	/* 170h to 17Fh - Directive Specific */

	/*
	 * I/O Command Set Specific - NVM commands:
	 */
	NVME_SC_BAD_ATTRIBUTES		= 0x180,
	NVME_SC_INVALID_PI		= 0x181, /* Invalid Protection Information */
	NVME_SC_READ_ONLY		= 0x182, /* Attempted Write to Read Only Range */
	NVME_SC_SIZE_EXCEEDED		= 0x183,

	/*
	 * I/O Command Set Specific - Fabrics commands:
	 */
	NVME_SC_CONNECT_FORMAT		= 0x180,
	NVME_SC_CONNECT_CTRL_BUSY	= 0x181,
	NVME_SC_CONNECT_INVALID_PARAM	= 0x182,
	NVME_SC_CONNECT_RESTART_DISC	= 0x183,
	NVME_SC_CONNECT_INVALID_HOST	= 0x184,

	NVME_SC_DISCOVERY_RESTART	= 0x190,
	NVME_SC_AUTH_REQUIRED		= 0x191,

	/*
	 * I/O Command Set Specific - Zoned commands:
	 */
	NVME_SC_ZONE_BOUNDARY_ERROR	= 0x1b8,
	NVME_SC_ZONE_FULL		= 0x1b9,
	NVME_SC_ZONE_READ_ONLY		= 0x1ba,
	NVME_SC_ZONE_OFFLINE		= 0x1bb,
	NVME_SC_ZONE_INVALID_WRITE	= 0x1bc,
	NVME_SC_ZONE_TOO_MANY_ACTIVE	= 0x1bd,
	NVME_SC_ZONE_TOO_MANY_OPEN	= 0x1be,
	NVME_SC_ZONE_INVALID_TRANSITION	= 0x1bf,

	/*
	 * Media and Data Integrity Errors:
	 */
	NVME_SC_WRITE_FAULT		= 0x280,
	NVME_SC_READ_ERROR		= 0x281,
	NVME_SC_GUARD_CHECK		= 0x282,
	NVME_SC_APPTAG_CHECK		= 0x283,
	NVME_SC_REFTAG_CHECK		= 0x284,
	NVME_SC_COMPARE_FAILED		= 0x285,
	NVME_SC_ACCESS_DENIED		= 0x286,
	NVME_SC_UNWRITTEN_BLOCK		= 0x287,

	/*
	 * Path-related Errors:
	 */
	NVME_SC_ANA_PERSISTENT_LOSS	= 0x301,
	NVME_SC_ANA_INACCESSIBLE	= 0x302,
	NVME_SC_ANA_TRANSITION		= 0x303,
	NVME_SC_HOST_PATH_ERROR		= 0x370,
	NVME_SC_HOST_ABORTED_CMD	= 0x371,
};

/**
 * @brief Completion Queue Entry
 * 
 * @note Refer to "NVM Express Base Specification R2.0b - Figure 89"
 */
struct nvme_completion {
	/*
	 * Used by Admin and Fabrics commands to return data:
	 */
	union nvme_result {
		__le16	u16;
		__le32	u32;
		__le64	u64;
	} result;
	__le16	sq_head;	/* how much of this queue may be reclaimed */
	__le16	sq_id;		/* submission queue that generated this entry */
	__u16	command_id;	/* of the command which completed */
	__le16	status;		/* did the command fail, and if so, why? */
#define NVME_CQE_STATUS_TO_PHASE(s)	((le16_to_cpu(s) >> 0) & 0x1)
#define NVME_CQE_STATUS_TO_STATE(s)	((le16_to_cpu(s) >> 1) & 0x7fff)
#define NVME_CQE_STATUS_TO_SC(s)	((le16_to_cpu(s) >> 1) & 0xff)
#define NVME_CQE_STATUS_TO_SCT(s)	((le16_to_cpu(s) >> 9) & 0x7)
#define NVME_CQE_STATUS_TO_CRD(s)	((le16_to_cpu(s) >> 12) & 0x3)
#define NVME_CQE_STATUS_TO_M(s)		((le16_to_cpu(s) >> 14) & 0x1)
#define NVME_CQE_STATUS_TO_DNR(s)	((le16_to_cpu(s) >> 15) & 0x1)
};

#endif /* !_UAPI_NVME_COMMAND_H_ */
