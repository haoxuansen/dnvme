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
	nvme_admin_maxio_nvme_hwrdma	= 0xe4, /**< Hardware Read DMA */
	nvme_admin_maxio_nvme_hwwdma	= 0xe5, /**< Hardware Write DMA */
	nvme_admin_maxio_nvme_case	= 0xef,
	nvme_admin_maxio_pcie_msg	= 0xd0,
	nvme_admin_maxio_pcie_interrupt	= 0xd1,
	nvme_admin_maxio_pcie_special	= 0xd2,
	nvme_admin_maxio_fwdma_fwdma	= 0xf0,
	nvme_admin_maxio_fwdma_opal	= 0xf1, /**< Opal */
	nvme_admin_maxio_fwdma_dpu	= 0xf2, /**< Data Path Unit */
};

enum nvme_admin_vendor_option {
	NVME_MAXIO_OPT_CHECK_RESULT	= 0,
	NVME_MAXIO_OPT_SET_PARAM	= 1, /**< Set parameter */
	NVME_MAXIO_OPT_GET_PARAM	= 2, /**< Get parameter */
};

/**
 * @brief Each maxio command general format
 * 
 * @verbatim embed:rst:leading-asterisk
 * 
 * 	+--------+----------------+-----------------------+-------------------------------------+
 * 	| Offset | Field          | Description           | Note                                |
 * 	+========+================+=======================+=====================================+
 * 	| DW0    | opcode         | Vendor Opcode         | :enum:`nvme_admin_vendor_opcode`    |
 * 	|        +----------------+-----------------------+-------------------------------------+
 * 	|        | flags          | ...                   | Refer to "NVMe Base Spec"           |
 * 	|        +----------------+-----------------------+                                     |
 * 	|        | command_id     | Command Identifier    |                                     |
 * 	+--------+----------------+-----------------------+                                     |
 * 	| DW1    | nsid           | Namespace Identifier  |                                     |
 * 	+--------+----------------+-----------------------+-------------------------------------+
 * 	| DW2    | option         | Vendor Operation      | :enum:`nvme_admin_vendor_option`    |
 * 	+--------+----------------+-----------------------+-------------------------------------+
 * 	| DW3    | param          | Parameter             | Sub-command Specific                |
 * 	+--------+----------------+-----------------------+-------------------------------------+
 * 	| DW4~9  | ...            | ...                   | Refer to "NVMe Base Spec"           |
 * 	+--------+----------------+-----------------------+-------------------------------------+
 * 	| DW10   | subcmd         | Sub-command           | Each bit represent a subcmd now     |
 * 	+--------+----------------+-----------------------+-------------------------------------+
 * 	| DW11   | cdw11          | Reserved              | Sub-command Specific                |
 * 	+--------+----------------+                       |                                     |
 * 	| DW12   | cdw12          |                       |                                     |
 * 	+--------+----------------+                       |                                     |
 * 	| DW13   | cdw13          |                       |                                     |
 * 	+--------+----------------+                       |                                     |
 * 	| DW14   | cdw14          |                       |                                     |
 * 	+--------+----------------+                       |                                     |
 * 	| DW15   | cdw15          |                       |                                     |
 * 	+--------+----------------+-----------------------+-------------------------------------+
 * 
 * @endverbatim
 */
struct nvme_maxio_common_cmd {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__le32			nsid;
	__le32			option;
	__le32			param;
	__le64			metadata;
	union nvme_data_ptr	dptr;
	__le32			subcmd;
	__le32			cdw11;
	__le32			cdw12;
	__le32			cdw13;
	__le32			cdw14;
	__le32			cdw15;
};
