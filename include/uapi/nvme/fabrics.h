/**
 * @file fabrics.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-04-26
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UAPI_NVME_FABRICS_H_
#define _UAPI_NVME_FABRICS_H_

enum nvmf_capsule_command {
	nvme_fabrics_type_property_set	= 0x00,
	nvme_fabrics_type_connect	= 0x01,
	nvme_fabrics_type_property_get	= 0x04,
	nvme_fabrics_type_auth_send	= 0x05,
	nvme_fabrics_type_auth_receive	= 0x06,
	nvme_fabrics_type_disconnect	= 0x08,
};

struct nvmf_common_command {
	__u8	opcode;
	__u8	resv1;
	__u16	command_id;
	__u8	fctype;
	__u8	resv2[35];
	__u8	ts[24];
};


/* ==================== nvme_fabrics_type_property_set(0x00) ==================== */

struct nvmf_property_set_command {
	__u8		opcode;
	__u8		resv1;
	__u16		command_id;
	__u8		fctype;
	__u8		resv2[35];
	__u8		attrib;
	__u8		resv3[3];
	__le32		offset;
	__le64		value;
	__u8		resv4[8];
};


/* ==================== nvme_fabrics_type_connect(0x01) ==================== */

/**
 * @brief Connect Attributes
 * 
 * @note See "struct nvmf_connect_command -> cattr" for details.
 */
enum {
	NVME_CONNECT_DISABLE_SQFLOW	= 1 << 2,
};

struct nvmf_connect_command {
	__u8		opcode;
	__u8		resv1;
	__u16		command_id;
	__u8		fctype;
	__u8		resv2[19];
	union nvme_data_ptr dptr;
	__le16		recfmt;
	__le16		qid;
	__le16		sqsize;
	__u8		cattr;
	__u8		resv3;
	__le32		kato;
	__u8		resv4[12];
};

/**
 * @note Refer to "NVM Express over Fabrics Revision 1.1 - Figure 20"
 */
struct nvmf_connect_data {
	guid_t		hostid;
	__le16		cntlid;
	char		resv4[238];
	char		subsysnqn[NVMF_NQN_FIELD_LEN];
	char		hostnqn[NVMF_NQN_FIELD_LEN];
	char		resv5[256];
};


/* ==================== nvme_fabrics_type_property_get(0x04) ==================== */

struct nvmf_property_get_command {
	__u8		opcode;
	__u8		resv1;
	__u16		command_id;
	__u8		fctype;
	__u8		resv2[35];
	__u8		attrib;
	__u8		resv3[3];
	__le32		offset;
	__u8		resv4[16];
};

#endif /* !_UAPI_NVME_FABRICS_H_ */
