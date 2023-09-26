/**
 * @file dnvme.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-04-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UAPI_DNVME_H_
#define _UAPI_DNVME_H_

#include <linux/pci_regs.h>
#include "linux/dma-direction.h"
#include "nvme.h"
#include "dnvme/netlink.h"

#ifndef __user
#define __user
#endif

/* The number of entries in Admin queue */
#define NVME_AQ_MAX_SIZE		4096
/* The number of entries in I/O queue */
#define NVME_IOQ_MIN_SIZE		2 /* at least 2 entries */
#define NVME_IOQ_MAX_SIZE		65536

/* For mmap - vm->pgoff */
/* bit[?:16] Type */
#define NVME_VMPGOFF_FOR_TYPE(n)	((n) << 16)
#define NVME_VMPGOFF_TO_TYPE(n)		((n) >> 16)
#define NVME_VMPGOFF_TYPE_CQ		0
#define NVME_VMPGOFF_TYPE_SQ		1
#define NVME_VMPGOFF_TYPE_META		2
/* bit[15:0] Identify */
#define NVME_VMPGOFF_ID(n)		((n) & 0xffff)

enum {
	NVME_READ_GENERIC = 0,
	NVME_WRITE_GENERIC,

	NVME_GET_SQ_INFO,
	NVME_GET_CQ_INFO,

	NVME_GET_DEV_INFO,
	NVME_GET_CAPABILITY,
	NVME_SET_DEV_STATE,
	NVME_SUBMIT_64B_CMD,

	NVME_CREATE_ADMIN_QUEUE,
	NVME_PREPARE_IOSQ,
	NVME_PREPARE_IOCQ,
	NVME_RING_SQ_DOORBELL,
	NVME_INQUIRY_CQE,
	NVME_REAP_CQE,

	NVME_CREATE_META_NODE,
	NVME_DELETE_META_NODE,

	NVME_SET_IRQ,
	NVME_MASK_IRQ,
	NVME_UNMASK_IRQ,

	NVME_EMPTY_CMD_LIST,

	NVME_ALLOC_HMB,
	NVME_RELEASE_HMB,
	NVME_ACCESS_HMB,
};

enum nvme_region {
	NVME_PCI_CONFIG,
	NVME_BAR0_BAR1,
};

/**
 * @brief The required access width of register or memory space.
 */
enum nvme_access_type {
	NVME_ACCESS_BYTE,
	NVME_ACCESS_WORD,
	NVME_ACCESS_DWORD,
	NVME_ACCESS_QWORD,
};

enum nvme_queue_type {
	NVME_ADMIN_SQ,
	NVME_ADMIN_CQ,
	NVME_CQ,
	NVME_SQ,
};

enum nvme_sq_prio {
	URGENT_PRIO = 0x0,
	HIGH_PRIO = 0x1,
	MEDIUM_PRIO = 0x2,
	LOW_PRIO = 0x3,
};


/**
 * @brief NVMe subsystem or controller status
 */
enum nvme_state {
	NVME_ST_ENABLE, /* Set the NVME Controller to enable state */
	NVME_ST_DISABLE, /* Controller reset without affecting Admin Q */
	NVME_ST_DISABLE_COMPLETE, /* Completely destroy even Admin Q's */
	NVME_ST_SUBSYSTEM_RESET, /* NVM Subsystem reset without affecting Admin Q */
	NVME_ST_PCIE_FLR_RESET,
	NVME_ST_PCIE_HOT_RESET,
	NVME_ST_PCIE_LINKDOWN_RESET,
};

enum nvme_64b_cmd_mask {
	NVME_MASK_PRP1_PAGE = (1 << 0), /* PRP1 can point to a physical page */
	NVME_MASK_PRP1_LIST = (1 << 1), /* PRP1 can point to a PRP list */
	NVME_MASK_PRP2_PAGE = (1 << 2), /* PRP2 can point to a physical page */
	NVME_MASK_PRP2_LIST = (1 << 3), /* PRP2 can point to a PRP list */
	NVME_MASK_MPTR = (1 << 4), /* MPTR may be modified */
	NVME_MASK_PRP_ADDR_OFFSET_ERR = (1 << 5), /* To inject PRP address offset (used for err cases) */
};

/* !FIXME: Some code doesn't follow the rules, and it is possible to replace
 * the members of "enum nvme_irq_type" with constants. */
enum nvme_irq_type {
	NVME_INT_MSI_SINGLE,
	NVME_INT_MSI_MULTI,
	NVME_INT_MSIX,
	NVME_INT_PIN,
	NVME_INT_NONE, /* !TODO: It's better to place header position */
};

/**
 * @brief Parameters for the generic read or write.
 */
struct nvme_access {
	enum nvme_region	region;
	enum nvme_access_type	type;
	uint8_t			*buffer;
	uint32_t		bytes;
	uint32_t		offset;
};

enum nvme_cap_type {
	NVME_CAP_TYPE_PCI = 1,
	NVME_CAP_TYPE_PCIE,
};

struct nvme_get_cap {
	enum nvme_cap_type	type;
	uint32_t		id;
	void			*buf;
	uint32_t		size;
};

/**
 * @elements: No. of elements of size 64 Bytes
 */
struct nvme_admin_queue {
	enum nvme_queue_type	type;
	uint32_t		elements;
};

/**
 * @brief Get the public info of specified queue based on the qid and type.
 * 
 * @bytes: buffer size
 * @buf: store queue public info
 */
struct nvme_get_queue {
	uint16_t	q_id;
	enum nvme_queue_type	type;
	uint32_t	bytes;
	uint8_t		*buf;
};

/**
 * Interface structure for allocating SQ memory. The elements are 1 based
 * values and the CC.IOSQES is 2^n based.
 */
struct nvme_prep_sq {
	uint32_t	elements; /* Total number of entries that need kernel mem */
	uint16_t	sq_id;	  /* The user specified unique SQ ID  */
	uint16_t	cq_id;	  /* Existing or non-existing CQ ID */

	uint32_t 	contig:1; /* Indicates if SQ is contig or not, 1 = contig */
	uint32_t	use_cmb:1;
};

/**
 * Interface structure for allocating CQ memory. The elements are 1 based
 * values and the CC.IOSQES is 2^n based.
 */
struct nvme_prep_cq {
	uint32_t	elements; /* Total number of entries that need kernal mem */
	uint16_t	cq_id;	  /* Existing or non-existing CQ ID. */
	uint8_t 	cq_irq_en;
	uint16_t	cq_irq_no;

	uint32_t 	contig:1; /* Indicates if SQ is contig or not, 1 = contig */
	uint32_t	use_cmb:1;
};

struct nvme_sgl_bit_bucket {
	uint32_t	offset; /* in bytes */
	uint32_t	length; /* in bytes */
};

/**
 * This struct is the basic structure which has important parameter for
 * sending 64 Bytes command to both admin and IO SQ's and CQ's
 *
 * @sqid: Queue ID where the cmd_buf command should go
 * @cid: Command Identifier assigned by driver
 */
struct nvme_64b_cmd {
	uint16_t	sqid;
	uint16_t	cid;
	void 		*cmd_buf_ptr;	/* Virtual Address pointer to 64B command */

	/* BIT MASK for PRP1,PRP2 and metadata pointer */
	enum nvme_64b_cmd_mask	bit_mask;
	/* Data buffer or discontiguous CQ/SQ's user space address */
	void		*data_buf_ptr;
	uint32_t	data_buf_size; /* Size of Data Buffer */
	/* 0=none; 1=to_device, 2=from_device, 3=bidirectional, others illegal */
	enum dma_data_direction	data_dir;

	uint32_t	meta_id;   /* Meta buffer ID when NVME_MASK_MPTR is set */

	uint32_t	use_bit_bucket:1;
	uint32_t	use_user_cid:1;
	uint32_t	use_user_meta:1; /* just for injecting error */
	uint32_t	use_user_prp:1; /* just for injecting error */

	uint32_t			nr_bit_bucket;
	struct nvme_sgl_bit_bucket	*bit_bucket;
};

/**
 * @brief Inquiry the number of ready CQ entries and save it.
 *
 * @cqid: Completion Queue Identify
 * @nr_cqe: The number of CQ entries are ready to be reaped
 */
struct nvme_inquiry {
	uint16_t	cqid;
	uint32_t	nr_cqe;
};

/**
 * Interface structure for setting the desired IRQ type.
 * works for all type of interrupt scheme expect PIN based.
 */
struct nvme_interrupt {
	uint16_t		num_irqs; /* total no. of irqs req by tnvme */
	enum nvme_irq_type	irq_type; /* Active IRQ scheme for this dev */
};

/**
 * This structure defines the parameters required for creating any CQ.
 * It supports both Admin CQ and IO CQ.
 */
struct nvme_cq_public {
	uint16_t	q_id; /* even admin q's are supported here q_id = 0 */
	uint16_t	tail_ptr; /* The value calculated for respective tail_ptr */
	uint16_t	head_ptr; /* Actual value in CQxTDBL for this q_id */
	uint32_t	elements; /* pass the actual elements in this q */
	uint8_t		irq_enabled; /* sets when the irq scheme is active */
	uint16_t	irq_no; /* idx in list; always 0 based */
	uint8_t		pbit_new_entry; /* Indicates if a new entry is in CQ */
	uint8_t		cqes;
};

/**
 * @sqes: SQ entry size, in bytes and specified as a power of two (2^n)
 */
struct nvme_sq_public {
	uint16_t	sq_id; /* Admin SQ are supported with q_id = 0 */
	uint16_t	cq_id; /* The CQ ID to which this SQ is associated */
	uint16_t	tail_ptr; /* Actual value in SQxTDBL for this SQ id */
	/* future SQxTDBL write value based on no. of new cmds copied to SQ */
	uint16_t	tail_ptr_virt; 
	uint16_t	head_ptr; /* Calculate this value based on cmds reaped */
	uint32_t	elements; /* total number of elements in this Q */
	uint8_t		sqes;
};

/**
 * @brief Reap CQ entries
 * 
 * @cqid: Completion Queue Identify
 * @expect: The number of CQ entries expected to be reaped
 * @remained: The number of CQ entries remained to be reaped
 * @reaped: The number of CQ entries actually reaped
 * @buf: The buffer holds CQ entries
 * @size: buffer size
 */
struct nvme_reap {
	uint16_t	cqid;
	uint32_t	expect;
	uint32_t	remained;
	uint32_t	reaped;
	void		*buf;
	uint32_t	size;
};

struct nvme_meta_create {
	uint16_t	id;

	void		*buf; /* for SGL */
	uint32_t	size;

	uint8_t		contig;
};

struct nvme_dev_public {
	int		devno;
	int		family;
};

struct nvme_hmb_alloc {
	uint64_t	desc_list;	/* physical address */
	uint32_t	nr_desc;	/* desciptor entry number */
	uint32_t	bsize[0];	/* buffer size */
};

struct nvme_hmb_access {
	void __user 	*buf;
	uint32_t	length; /* buf length */
	uint32_t	offset;	/* HMB offset */

	uint32_t	option;
#define NVME_HMB_OPT_READ	1
#define NVME_HMB_OPT_WRITE	2
};

#define NVME_IOCTL_GET_SQ_INFO \
	_IOWR('N', NVME_GET_SQ_INFO, struct nvme_sq_public)
#define NVME_IOCTL_GET_CQ_INFO \
	_IOWR('N', NVME_GET_CQ_INFO, struct nvme_cq_public)

#define NVME_IOCTL_GET_DEV_INFO \
	_IOR('N', NVME_GET_DEV_INFO, struct nvme_dev_public)
#define NVME_IOCTL_GET_CAPABILITY \
	_IOWR('N', NVME_GET_CAPABILITY, struct nvme_get_cap)

#define NVME_IOCTL_READ_GENERIC \
	_IOWR('N', NVME_READ_GENERIC, struct nvme_access)
#define NVME_IOCTL_WRITE_GENERIC \
	_IOWR('N', NVME_WRITE_GENERIC, struct nvme_access)

#define NVME_IOCTL_SET_DEV_STATE \
	_IOW('N', NVME_SET_DEV_STATE, enum nvme_state)

#define NVME_IOCTL_CREATE_ADMIN_QUEUE \
	_IOWR('N', NVME_CREATE_ADMIN_QUEUE, struct nvme_admin_queue)

#define NVME_IOCTL_PREPARE_IOSQ \
	_IOWR('N', NVME_PREPARE_IOSQ, struct nvme_prep_sq)
#define NVME_IOCTL_PREPARE_IOCQ \
	_IOWR('N', NVME_PREPARE_IOCQ, struct nvme_prep_cq)

#define NVME_IOCTL_RING_SQ_DOORBELL \
	_IOW('N', NVME_RING_SQ_DOORBELL, uint16_t)

#define NVME_IOCTL_SUBMIT_64B_CMD \
	_IOWR('N', NVME_SUBMIT_64B_CMD, struct nvme_64b_cmd)

#define NVME_IOCTL_INQUIRY_CQE		_IOWR('N', NVME_INQUIRY_CQE, struct nvme_inquiry)
#define NVME_IOCTL_REAP_CQE		_IOWR('N', NVME_REAP_CQE, struct nvme_reap)
#define NVME_IOCTL_EMPTY_CMD_LIST	_IOW('N', NVME_EMPTY_CMD_LIST, uint16_t) /* SQID */

/* uint16_t: assign meta node identify */
#define NVME_IOCTL_CREATE_META_NODE	_IOW('N', NVME_CREATE_META_NODE, struct nvme_meta_create)
/* uint16_t: assign meta node identify */
#define NVME_IOCTL_DELETE_META_NODE	_IOW('N', NVME_DELETE_META_NODE, uint32_t)

#define NVME_IOCTL_SET_IRQ		_IOWR('N', NVME_SET_IRQ, struct nvme_interrupt)
/* uint16_t: specified irq identify */
#define NVME_IOCTL_MASK_IRQ		_IOW('N', NVME_MASK_IRQ, uint16_t)
/* uint16_t: specified irq identify */
#define NVME_IOCTL_UNMASK_IRQ		_IOW('N', NVME_UNMASK_IRQ, uint16_t)

#define NVME_IOCTL_ALLOC_HMB \
	_IOWR('N', NVME_ALLOC_HMB, struct nvme_hmb_alloc)
#define NVME_IOCTL_RELEASE_HMB \
	_IO('N', NVME_RELEASE_HMB)
#define NVME_IOCTL_ACCESS_HMB \
	_IOWR('N', NVME_ACCESS_HMB, struct nvme_hmb_access)

#endif /* !_UAPI_DNVME_H_ */
