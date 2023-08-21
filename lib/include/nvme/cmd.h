/**
 * @file cmd.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-19
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _UAPI_LIB_NVME_CMD_H_
#define _UAPI_LIB_NVME_CMD_H_

/**
 * @brief For nvme_rw_command
 * 
 * @flags: Command Dword 0 bit[15:8]
 * @slba: Start Logical Block Address
 * @nlb: Number of Logical Blocks, 1'base
 * @control: Command Dword 12 bit[31:16]
 */
struct nvme_rwc_wrapper {
	uint16_t	sqid;
	uint16_t	cqid;

	uint8_t		flags;
	uint32_t	nsid;
	uint64_t	slba;
	uint32_t	nlb;
	uint16_t	control;

	void		*buf;
	uint32_t	size;

	uint32_t	meta_id;
};

struct nvme_copy_wrapper {
	uint16_t	sqid;
	uint16_t	cqid;

	uint8_t		flags;
	uint32_t	nsid;
	uint64_t	slba;
	uint8_t		ranges; /* 0'based */
	uint8_t		desc_fmt;

	void		*desc;
	uint32_t	size;
};

struct nvme_hmb_wrapper {
	uint32_t	sel;

	/* for set feature */
	uint32_t	dw11;
	uint32_t	hsize;
	uint64_t	hmdla;
	uint32_t	hmdlec;
	/* for get feature */
	struct nvme_feat_hmb_attribute *attr;
};

static inline void nvme_cmd_fill_create_sq(struct nvme_create_sq *csq, 
	uint16_t sqid, uint16_t cqid, uint32_t elements, uint8_t contig,
	uint16_t prio)
{
	csq->opcode = nvme_admin_create_sq;
	csq->sqid = cpu_to_le16(sqid);
	csq->qsize = cpu_to_le16(elements - 1); /* 0's based */
	if (contig)
		csq->sq_flags |= cpu_to_le16(NVME_QUEUE_PHYS_CONTIG);
	csq->sq_flags |= cpu_to_le16(prio);
	csq->cqid = cpu_to_le16(cqid);
}

static inline void nvme_cmd_fill_create_cq(struct nvme_create_cq *ccq, 
	uint16_t cqid, uint32_t elements, uint8_t contig, uint8_t irq_en,
	uint16_t irq_no)
{
	ccq->opcode = nvme_admin_create_cq;
	ccq->cqid = cpu_to_le16(cqid);
	ccq->qsize = cpu_to_le16(elements - 1); /* 0's based */
	if (contig)
		ccq->cq_flags |= cpu_to_le16(NVME_QUEUE_PHYS_CONTIG);
	if (irq_en)
		ccq->cq_flags |= cpu_to_le16(NVME_CQ_IRQ_ENABLED);
	ccq->irq_vector = cpu_to_le16(irq_no);
}

int nvme_submit_64b_cmd(int fd, struct nvme_64b_cmd *cmd);

int nvme_cmd_keep_alive(int fd);

int nvme_cmd_create_iosq(int fd, struct nvme_create_sq *csq, uint8_t contig,
	void *buf, uint32_t size);
int nvme_cmd_create_iocq(int fd, struct nvme_create_cq *ccq, uint8_t contig,
	void *buf, uint32_t size);

int nvme_cmd_delete_iosq(int fd, uint16_t sqid);
int nvme_cmd_delete_iocq(int fd, uint16_t cqid);

int nvme_cmd_set_feature(int fd, uint32_t nsid, uint32_t fid, uint32_t dw11);
int nvme_cmd_get_feature(int fd, uint32_t nsid, uint32_t fid, uint32_t dw11);;

/**
 * @brief Set feature named power management
 * 
 * @param ps Power State
 * @param wh Workload Hint
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
static inline int nvme_cmd_set_feat_power_mgmt(int fd, uint8_t ps, uint8_t wh)
{
	return nvme_cmd_set_feature(fd, 0, NVME_FEAT_POWER_MGMT, 
		NVME_POWER_MGMT_FOR_WH(wh) | NVME_POWER_MGMT_FOR_PS(ps));
}

static inline int nvme_cmd_get_feat_power_mgmt(int fd, uint32_t sel)
{
	return nvme_cmd_get_feature(fd, 0, sel | NVME_FEAT_POWER_MGMT, 0);
}

static inline int nvme_cmd_set_feat_num_queues(int fd, uint16_t nr_sq, 
	uint16_t nr_cq)
{
	return nvme_cmd_set_feature(fd, 0, NVME_FEAT_NUM_QUEUES, 
		((uint32_t)nr_cq << 16) | nr_sq);
}

int nvme_cmd_set_feat_hmb(int fd, struct nvme_hmb_wrapper *wrap);
int nvme_set_feat_hmb(struct nvme_dev_info *ndev, struct nvme_hmb_wrapper *wrap);

int nvme_cmd_get_feat_hmb(int fd, struct nvme_hmb_wrapper *wrap);
int nvme_get_feat_hmb(struct nvme_dev_info *ndev, struct nvme_hmb_wrapper *wrap);

static inline int nvme_cmd_set_feat_iocs_profile(int fd, uint32_t sel, 
	uint32_t index)
{
	return nvme_cmd_set_feature(fd, 0, sel | NVME_FEAT_IOCS_PROFILE, index);
}
int nvme_set_feat_iocs_profile(struct nvme_dev_info *ndev, uint32_t sel, 
	uint32_t index);

static inline int nvme_cmd_get_feat_iocs_profile(int fd, uint32_t sel)
{
	return nvme_cmd_get_feature(fd, 0, sel | NVME_FEAT_IOCS_PROFILE, 0);
}
int nvme_get_feat_iocs_profile(struct nvme_dev_info *ndev, uint32_t sel);

static inline int nvme_cmd_set_feat_write_protect(int fd, uint32_t nsid, 
	uint32_t sel, uint32_t state)
{
	return nvme_cmd_set_feature(fd, nsid, sel | NVME_FEAT_WRITE_PROTECT, state);
}
int nvme_set_feat_write_protect(struct nvme_dev_info *ndev, uint32_t nsid, 
	uint32_t sel, uint32_t state);

static inline int nvme_cmd_get_feat_write_protect(int fd, uint32_t nsid, 
	uint32_t sel)
{
	return nvme_cmd_get_feature(fd, nsid, sel | NVME_FEAT_WRITE_PROTECT, 0);
}
int nvme_get_feat_write_protect(struct nvme_dev_info *ndev, uint32_t nsid, 
	uint32_t sel);

int nvme_cmd_format_nvm(int fd, uint32_t nsid, uint8_t flags, uint32_t dw10);
int nvme_format_nvm(struct nvme_dev_info *ndev, uint32_t nsid, uint8_t flags, 
	uint32_t dw10);

int nvme_cmd_identify(int fd, struct nvme_identify *identify, void *buf, 
	uint32_t size);

int nvme_cmd_identify_ns_active(int fd, struct nvme_id_ns *ns, uint32_t nsid);
int nvme_identify_ns_active(struct nvme_dev_info *ndev, struct nvme_id_ns *ns, 
	uint32_t nsid);

int nvme_cmd_identify_ctrl(int fd, struct nvme_id_ctrl *ctrl);
int nvme_identify_ctrl(struct nvme_dev_info *ndev, struct nvme_id_ctrl *ctrl);

int nvme_cmd_identify_ns_list_active(int fd, void *buf, uint32_t size, 
	uint32_t nsid);
int nvme_identify_ns_list_active(struct nvme_dev_info *ndev, void *buf, 
	uint32_t size, uint32_t nsid);

int nvme_cmd_identify_ns_desc_list(int fd, void *buf, uint32_t size,
	uint32_t nsid);
int nvme_identify_ns_desc_list(struct nvme_dev_info *ndev, void *buf, 
	uint32_t size, uint32_t nsid);

int nvme_cmd_identify_cs_ns(int fd, void *buf, uint32_t size, uint32_t nsid,
	uint8_t csi);
int nvme_identify_cs_ns(struct nvme_dev_info *ndev, void *buf, uint32_t size, 
	uint32_t nsid, uint8_t csi);

int nvme_cmd_identify_cs_ctrl(int fd, void *buf, uint32_t size, uint8_t csi);
int nvme_identify_cs_ctrl(struct nvme_dev_info *ndev, void *buf, uint32_t size,
	uint8_t csi);

int nvme_cmd_identify_ns_list_allocated(int fd, void *buf, uint32_t size, 
	uint32_t nsid);
int nvme_identify_ns_list_allocated(struct nvme_dev_info *ndev, void *buf, 
	uint32_t size, uint32_t nsid);

int nvme_cmd_identify_ns_allocated(int fd, struct nvme_id_ns *ns, uint32_t nsid);
int nvme_identify_ns_allocated(struct nvme_dev_info *ndev, 
	struct nvme_id_ns *ns, uint32_t nsid);

int nvme_cmd_identify_ns_attached_ctrl_list(int fd, void *buf, uint32_t size,
	uint32_t nsid, uint16_t cntid);
int nvme_identify_ns_attached_ctrl_list(struct nvme_dev_info *ndev, void *buf, 
	uint32_t size, uint32_t nsid, uint16_t cntid);

int nvme_cmd_identify_ctrl_list(int fd, void *buf, uint32_t size, 
	uint16_t cntid);
int nvme_identify_ctrl_list(struct nvme_dev_info *ndev, void *buf, 
	uint32_t size, uint16_t cntid);

int nvme_cmd_identify_ctrl_csc_list(int fd, struct nvme_id_ctrl_csc *csc, 
	uint16_t cntid);
int nvme_identify_ctrl_csc_list(struct nvme_dev_info *ndev, 
	struct nvme_id_ctrl_csc *csc, uint16_t cntid);

int nvme_cmd_io_rw_common(int fd, struct nvme_rwc_wrapper *wrap, uint8_t opcode);
int nvme_io_rw_common(struct nvme_dev_info *ndev, struct nvme_rwc_wrapper *wrap, 
	uint8_t opcode);

static inline int nvme_cmd_io_read(int fd, struct nvme_rwc_wrapper *wrap)
{
	return nvme_cmd_io_rw_common(fd, wrap, nvme_cmd_read);
}

static inline int nvme_io_read(struct nvme_dev_info *ndev, 
	struct nvme_rwc_wrapper *wrap)
{
	return nvme_io_rw_common(ndev, wrap, nvme_cmd_read);
}

static inline int nvme_cmd_io_write(int fd, struct nvme_rwc_wrapper *wrap)
{
	return nvme_cmd_io_rw_common(fd, wrap, nvme_cmd_write);
}

static inline int nvme_io_write(struct nvme_dev_info *ndev, 
	struct nvme_rwc_wrapper *wrap)
{
	return nvme_io_rw_common(ndev, wrap, nvme_cmd_write);
}

static inline int nvme_cmd_io_compare(int fd, struct nvme_rwc_wrapper *wrap)
{
	return nvme_cmd_io_rw_common(fd, wrap, nvme_cmd_compare);
}

static inline int nvme_io_compare(struct nvme_dev_info *ndev, 
	struct nvme_rwc_wrapper *wrap)
{
	return nvme_io_rw_common(ndev, wrap, nvme_cmd_compare);
}

int nvme_cmd_io_copy(int fd, struct nvme_copy_wrapper *wrap);
int nvme_io_copy(struct nvme_dev_info *ndev, struct nvme_copy_wrapper *wrap);

#endif /* !_UAPI_LIB_NVME_CMD_H_ */
