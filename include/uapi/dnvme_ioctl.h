/*
 * NVM Express Compliance Suite
 * Copyright (c) 2011, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _UAPI_DNVME_IOCTLS_H_
#define _UAPI_DNVME_IOCTLS_H_

#include <linux/pci_regs.h>
#include "linux/dma-direction.h"
#include "bitops.h"
#include "nvme.h"

/* The number of entries in Admin queue */
#define NVME_AQ_MAX_SIZE		4096
/* The number of entries in I/O queue */
#define NVME_IOQ_MIN_SIZE		2 /* at least 2 entries */
#define NVME_IOQ_MAX_SIZE		65536

enum {
	NVME_READ_GENERIC = 0,
	NVME_WRITE_GENERIC,

	NVME_GET_DRIVER_INFO,
	NVME_GET_DEVICE_INFO,
	NVME_GET_SQ_INFO,
	NVME_GET_CQ_INFO,

	NVME_GET_CAPABILITY,
	NVME_SET_DEV_STATE,
	NVME_SUBMIT_64B_CMD,

	NVME_CREATE_ADMIN_QUEUE,
	NVME_PREPARE_IOSQ,
	NVME_PREPARE_IOCQ,
	NVME_RING_SQ_DOORBELL,
	NVME_DUMP_LOG_FILE,
	NVME_INQUIRY_CQE,
	NVME_REAP_CQE,

	NVME_CREATE_META_POOL,
	NVME_DESTROY_META_POOL,
	NVME_CREATE_META_NODE,
	NVME_DELETE_META_NODE,

	NVME_SET_IRQ,
	NVME_MASK_IRQ,
	NVME_UNMASK_IRQ,
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
	NVME_ST_RESET_SUBSYSTEM, /* NVM Subsystem reset without affecting Admin Q */
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

struct nvme_driver {
	uint32_t drv_version;
	uint32_t api_version;
};

/**
 * @brief NVMe device controller properties
 * 
 * @cap: Controller Capabilities Register
 * @cc: Controller Configuration Register
 * @cmbloc: Controller Memory Buffer Location Register
 * @cmbsz: Controller Memory Buffer Size Register
 */
struct nvme_ctrl_property {
	uint64_t	cap;
	uint32_t	cc;
	uint32_t	cmbloc;
	uint32_t	cmbsz;
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

struct pci_cap {
	uint8_t			id;
	uint8_t			offset;
	void			*data;
};

struct pcie_cap {
	uint16_t		id;
	uint8_t			version;
	uint16_t		offset;
	void			*data;
};

struct nvme_cap {
	struct pci_cap	pci[PCI_CAP_ID_MAX];
	struct pcie_cap	pcie[PCI_EXT_CAP_ID_MAX];
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
	uint8_t 	contig;    /* Indicates if SQ is contig or not, 1 = contig */
};

/**
 * Interface structure for allocating CQ memory. The elements are 1 based
 * values and the CC.IOSQES is 2^n based.
 */
struct nvme_prep_cq {
	uint32_t	elements; /* Total number of entries that need kernal mem */
	uint16_t	cq_id;	  /* Existing or non-existing CQ ID. */
	uint8_t 	contig;    /* Indicates if SQ is contig or not, 1 = contig */
	uint8_t 	cq_irq_en;
	uint16_t	cq_irq_no;
};

/**
 * This struct is the basic structure which has important parameter for
 * sending 64 Bytes command to both admin and IO SQ's and CQ's
 */
struct nvme_64b_cmd {
	/* BIT MASK for PRP1,PRP2 and metadata pointer */
	enum nvme_64b_cmd_mask	bit_mask;
	/* Data buffer or discontiguous CQ/SQ's user space address */
	void		*data_buf_ptr;
	uint32_t	data_buf_size; /* Size of Data Buffer */
	/* 0=none; 1=to_device, 2=from_device, 3=bidirectional, others illegal */
	enum dma_data_direction	data_dir;

	void 		*cmd_buf_ptr;	/* Virtual Address pointer to 64B command */
	uint32_t	meta_buf_id;   /* Meta buffer ID when NVME_MASK_MPTR is set */
	uint16_t	unique_id;     /* Value returned back to user space */
	uint16_t	q_id;	       /* Queue ID where the cmd_buf command should go */
};

/**
 * @brief Inquiry the number of ready CQ entries and save it.
 *
 * @q_id: CQ identify
 * @num_remaining: return the number of cmds waiting to be reaped
 * @isr_count: return the number of times the irq fired which bind to CQ
 */
struct nvme_inquiry {
	uint16_t	q_id;
	uint32_t	num_remaining;
	uint32_t	isr_count;
};

/**
 * Interface structure for setting the desired IRQ type.
 * works for all type of interrupt scheme expect PIN based.
 */
struct nvme_interrupt {
	uint16_t		num_irqs; /* total no. of irqs req by tnvme */
	enum nvme_irq_type	irq_type; /* Active IRQ scheme for this dev */
};

struct nvme_dev_public {
	struct nvme_interrupt	irq_active; /* Active IRQ state of the nvme device */
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
 * Interface structure for reap ioctl. Admin Q and all IO Q's are supported.
 */
struct nvme_reap {
	uint16_t q_id;          /* CQ ID to reap commands for */
	uint32_t elements;      /* Get the no. of elements to be reaped */
	uint32_t num_remaining; /* return no. of cmds waiting for this cq */
	uint32_t num_reaped;    /* Return no. of elements reaped */
	uint8_t  *buffer;       /* Buffer to copy reaped data */
	/* no of times isr was fired which is associated with cq reaped on */
	uint32_t isr_count;
	uint32_t size;          /* Size of buffer to fill data to */
};

/**
 * @name: The file name includes its path information.
 * @len: The length of file name (in bytes).
 */
struct nvme_log_file {
	const char	*name;
	uint16_t	len;
};

#define NVME_IOCTL_GET_DRIVER_INFO \
	_IOR('N', NVME_GET_DRIVER_INFO, struct nvme_driver)
#define NVME_IOCTL_GET_DEVICE_INFO \
	_IOR('N', NVME_GET_DEVICE_INFO, struct nvme_dev_public)
#define NVME_IOCTL_GET_SQ_INFO \
	_IOWR('N', NVME_GET_SQ_INFO, struct nvme_sq_public)
#define NVME_IOCTL_GET_CQ_INFO \
	_IOWR('N', NVME_GET_CQ_INFO, struct nvme_cq_public)

#define NVME_IOCTL_GET_CAPABILITY \
	_IOWR('N', NVME_GET_CAPABILITY, struct nvme_cap)

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

/* uint16_t: assign meta node identify */
#define NVME_IOCTL_CREATE_META_NODE	_IOW('N', NVME_CREATE_META_NODE, uint32_t)
/* uint16_t: assign meta node identify */
#define NVME_IOCTL_DELETE_META_NODE	_IOW('N', NVME_DELETE_META_NODE, uint32_t)
/* uint32_t: assign meta buf size */
#define NVME_IOCTL_CREATE_META_POOL	_IOW('N', NVME_CREATE_META_POOL, uint32_t)
#define NVME_IOCTL_DESTROY_META_POOL	_IO('N', NVME_DESTROY_META_POOL)

#define NVME_IOCTL_SET_IRQ		_IOWR('N', NVME_SET_IRQ, struct nvme_interrupt)
/* uint16_t: specified irq identify */
#define NVME_IOCTL_MASK_IRQ		_IOW('N', NVME_MASK_IRQ, uint16_t)
/* uint16_t: specified irq identify */
#define NVME_IOCTL_UNMASK_IRQ		_IOW('N', NVME_UNMASK_IRQ, uint16_t)

#define NVME_IOCTL_DUMP_LOG_FILE \
	_IOWR('N', NVME_DUMP_LOG_FILE, struct nvme_log_file)

#endif /* _UAPI_DNVME_IOCTLS_H_ */
