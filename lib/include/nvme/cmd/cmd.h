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
	uint16_t	apptag;
	uint16_t	appmask;

	uint32_t	dw2;
	uint32_t	dw3;
	uint32_t	dw14;

	void		*buf;
	uint32_t	size;

	uint32_t	meta_id;
	uint64_t	meta_addr;
	uint64_t	prp1;
	uint64_t	prp2;

	uint32_t	use_bit_bucket:1;
	uint32_t	use_user_cid:1;
	uint32_t	use_user_meta:1;
	uint32_t	use_user_prp:1;

	uint32_t			nr_bit_bucket;
	struct nvme_sgl_bit_bucket	*bit_bucket;

	uint16_t	cid;
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

int nvme_cmd_format_nvm(int fd, uint32_t nsid, uint8_t flags, uint32_t dw10);
int nvme_format_nvm(struct nvme_dev_info *ndev, uint32_t nsid, uint8_t flags, 
	uint32_t dw10);

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

