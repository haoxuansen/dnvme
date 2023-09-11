/**
 * @file vendor.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-08-22
 * 
 * @copyright Copyright (c) 2023
 * 
 */

enum nvme_admin_vendor_opcode {
	nvme_admin_maxio_nvme_top	= 0xe0,
	nvme_admin_maxio_nvme_func	= 0xe1,
	nvme_admin_maxio_nvme_sqm	= 0xe2, /**< SQ management */
	nvme_admin_maxio_nvme_cqm	= 0xe3, /**< CQ management */
};

enum {
	NVME_MAXIO_OPT_CHECK_RESULT	= 0,
	NVME_MAXIO_OPT_SET_PARAM	= 1,
};

struct nvme_maxio_common_cmd {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__le32			nsid;
	__le32			option;
	__le32			param;
	__le64			metadata;
	union nvme_data_ptr	dptr;
	__le32			bitmap;
	__le32			cdw11;
	__le32			cdw12;
	__le32			cdw13;
	__le32			cdw14;
	__le32			cdw15;
};
