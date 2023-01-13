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

/* ==================== nvme_admin_get_features ==================== */

#define NVME_FEATURE_SEL(sel)		(((sel) & 0x7) << 8)
#define NVME_FEATURE_SEL_CURRENT	NVME_FEATURE_SEL(0)
#define NVME_FEATURE_SEL_DEFAULT	NVME_FEATURE_SEL(1)
#define NVME_FEATURE_SEL_SAVED		NVME_FEATURE_SEL(2)
#define NVME_FEATURE_SEL_SUP_CAP	NVME_FEATURE_SEL(3)
#define NVME_FEATURE_FID(fid)		((fid) & 0xff)

#endif /* !_UAPI_NVME_COMMAND_H_ */
