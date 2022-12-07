/**
 * @file core.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-11-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef _DNVME_CORE_H_
#define _DNVME_CORE_H_

#include <linux/limits.h>
#include <linux/dma-mapping.h>

#include "dnvme_ioctl.h"

#define NVME_ASQ_ENTRY_MAX		4096
#define NVME_ACQ_ENTRY_MAX		4096

#define NVME_SQ_ID_MAX			U16_MAX
#define NVME_CQ_ID_MAX			U16_MAX
#define NVME_META_ID_MAX		((0x1 << 18) - 1)

#define NVME_META_BUF_ALIGN		4

#define NVME_PRP_ENTRY_SIZE		8 /* in bytes */

#undef pr_fmt
#define pr_fmt(fmt)			"[%s,%d]" fmt, __func__, __LINE__

#if !IS_ENABLED(CONFIG_PRINTK_COLOR)
#include "log/color.h"

#define dnvme_err(fmt, ...) \
	pr_err(LOG_COLOR_RED fmt, ##__VA_ARGS__)
#define dnvme_warn(fmt, ...) \
	pr_warn(LOG_COLOR_YELLOW fmt, ##__VA_ARGS__)
#define dnvme_notice(fmt, ...) \
	pr_notice(LOG_COLOR_BLUE fmt, ##__VA_ARGS__)
#define dnvme_info(fmt, ...) \
	pr_info(LOG_COLOR_GREEN fmt, ##__VA_ARGS__)
#define dnvme_dbg(fmt, ...) \
	pr_debug(LOG_COLOR_NONE fmt, ##__VA_ARGS__)

#ifdef VERBOSE_DEBUG
#define dnvme_vdbg(fmt, ...) \
	pr_debug(LOG_COLOR_NONE fmt, ##__VA_ARGS__)
#else
#define dnvme_vdbg(fmt, ...)
#endif /* !VERBOSE_DEBUG */
#endif

/**
 * @prp_list: If use PRP, this field point to PRP list pages. If use SGL, 
 *  this field point to the first segment of SGLs.
 * @pg_addr: This is an array for storing physical aaddress of allocated PAGES.
 *  The number of array is @nr_pages.
 * @nr_pages: The number of pages are used to store PRP list or SGL segment.
 */
struct nvme_prps {
	__le64		**prp_list;
	dma_addr_t	*pg_addr;
	u32		nr_pages;

	u8	*buf; /* K.V.A for pinned down pages */

	struct scatterlist	*sg;
	u32	num_map_pgs;
	/* Size of data buffer for the specific command */
	u32	data_buf_size;
	/* Address of data buffer for the specific command */
	u64	data_buf_addr;
	enum dma_data_direction	data_dir;
};

/**
 * @brief NVMe CQ private information
 */
struct nvme_cq_private {
	u8		*buf; /* phy addr ptr to the q's alloc to kern mem */
	dma_addr_t	dma; /* dma mapped address using dma_alloc */
	u32		size; /* length in bytes of the alloc Q in kernel */
	u32 __iomem	*dbs; /* Door Bell stride  */
	u8		contig; /* Indicates if prp list is contig or not */
	u8		bit_mask;
#define NVME_QF_WAIT_FOR_CREATE         (1 << 0)

	struct nvme_prps	prps; /* PRP element in CQ */
};

/**
 * @brief Snapshot of the command sent by user.
 * 
 * @id: Command identifier is assigned by driver.
 * @sqid: Specify the SQ to process the command.
 * @target_qid: Target queue ID used for Create/Delete Q's, never == 0
 * @opcode: Command operation code specified in NVMe specification
 * @entry: @nvme_cmd is managed by @nvme_sq
 * @prps: see @nvme_prps for details
 */
struct nvme_cmd {
	u16	id;
	u16	sqid;
	u16	target_qid;
	u8	opcode;
	struct list_head	entry;
	struct nvme_prps	prps;
};

/**
 * @brief NVMe SQ private information
 * 
 * @bit_mask: see "struct nvme_cq_private" bit_mask field for details.
 * @prps: see "struct nvme_prps" for details.
 * @cmd_list: This is a list head for managing "struct nvme_cmd"
 */
struct nvme_sq_private {
	void		*buf; /* virtual kernal address using kmalloc */
	dma_addr_t	dma; /* dma mapped address using dma_alloc */
	u32		size; /* len in bytes of allocated Q in kernel */
	u32 __iomem	*dbs; /* Door Bell stride */
	u16		unique_cmd_id; /* unique counter for each comand in SQ */
	u8		contig; /* Indicates if prp list is contig or not */
	u8		bit_mask;
	struct nvme_prps	prps;
	struct list_head	cmd_list;
};

/*
 * struct nvme_cq - representation of a completion queue.
 */
struct nvme_cq {
	struct list_head	cq_entry;
	struct nvme_cq_public	pub;
	struct nvme_cq_private	priv;
	struct nvme_context	*ctx;
};

/*
 * struct nvme_sq - representation of a submisssion queue.
 */
struct nvme_sq {
	struct list_head	sq_entry;
	struct nvme_sq_public	pub;
	struct nvme_sq_private	priv;
	struct nvme_context	*ctx;
};

/*
 * Structure with cq track parameters for interrupt related functionality.
 * Note:- Struct used for u16 for future additions
 */
struct nvme_icq {
	struct list_head	entry; /* linked list head for irq CQ trk */
	u16			cq_id; /* Completion Q id */
};

/**
 * @irq_entry: nvme_irq is managed by nvme_meta_set
 * @icq_list: manage nvme_icq nodes
 * @irq_id: irq identify, always 0 based
 */
struct nvme_irq {
	struct list_head	irq_entry;
	struct list_head	icq_list;
	u16			irq_id;
	u32			int_vec; /* vec number; assigned by OS */
	u8			isr_fired; /* flag to indicate if irq has fired */
	u32			isr_count; /* total no. of times irq fired */
};

/*
 * structure for meta data per device parameters.
 */
struct nvme_meta_set {
	struct list_head	meta_list;
	struct dma_pool		*pool;
	u32			buf_size;
};

/*
 * Structure for meta data buffer allocations.
 */
struct nvme_meta {
	struct list_head	entry;
	u32			id;
	void			*buf;
	dma_addr_t		dma;
};

/*
 * Structure for Nvme device private parameters. These parameters are
 * device specific and populated while the nvme device is being opened
 * or during probe.
 */
struct nvme_dev_private {
	struct pci_dev	*pdev; /* Pointer to the PCIe device */
	struct device	*spcl_dev; /* Special device file */
	void __iomem	*bar0; /* 64 bit BAR0 memory mapped ctrlr regs */
	void __iomem	*bar1; /* 64 bit BAR1 I/O mapped registers */
	void __iomem	*bar2; /* 64 bit BAR2 memory mapped MSIX table */
	u32 __iomem	*dbs;
	struct dma_pool	*prp_page_pool; /* Mem for PRP List */
	struct device	*dmadev; /* Pointer to the dma device from pdev */
	int	minor; /* Minor no. of the device being used */
	u8	opened; /* Allows device opening only once */
};

/**
 * @brief NVMe device controller properties
 * 
 * @cap: Controller Capabilities Register
 * @cmbloc: Controller Memory Buffer Location Register
 * @cmbsz: Controller Memory Buffer Size Register
 */
struct nvme_ctrl_property {
	u64	cap;
	u32	cmbloc;
	u32	cmbsz;
};

/**
 * @brief Representation of a NVMe device
 * 
 * @cmb_size: Actually mapped CMB size, may less than CMBSZ which is indicated
 *  in Controller Properities. 
 * @cmb_use_sqes: If true, use controller's memory buffer for I/O SQes.
 */
struct nvme_device {
	struct nvme_dev_private	priv;
	struct nvme_dev_public	pub;
	struct nvme_ctrl_property	prop;
	struct nvme_context	*ctx;
	u32	q_depth;
	u32	db_stride;
	u64	cmb_size;
	bool	cmb_use_sqes;
};

/**
 * @entry: nvme_work is managed by nvme_irq_set
 * @irq_id: see nvme_irq.irq_id for details
 * @int_vec: see nvme_irq.int_vec for details
 */
struct nvme_work {
	struct list_head	entry;
	struct work_struct	work;
	u16	irq_id;
	u32	int_vec;
	struct nvme_irq_set	*irq_set;
};

/*
 * Irq Processing structure to hold all the irq parameters per device.
 */
struct nvme_irq_set {
	/* mtx_lock is used only while traversing/editing/deleting the
	 * irq_list
	 */
	struct list_head	irq_list; /* IRQ list; sorted by irq_no */
	struct mutex	mtx_lock; /* Mutex for access to irq_list */

	/* To resolve contention for ISR's getting scheduled on different cores */
	spinlock_t	spin_lock;

	/* Mask pointer for ISR (read both in ISR and BH) */
	/* Pointer to MSI-X table offset or INTMS register */
	u8 __iomem	*mask_ptr;
	/* Will only be read by ISR and set once per SET/DISABLE of IRQ scheme */
	u8		irq_type; /* Type of IRQ set */

	struct workqueue_struct	*wq;
	struct list_head	work_list;
};

/**
 * @brief NVMe device context info
 * 
 * @entry: This context is managed by global list head named "nvme_ctx_list"
 * @cq_list: This is a list head for managing nvme_cq.
 * @sq_list: This is a list head for managing nvme_sq.
 * @dev: NVMe device info
 * @lock: Get this lock before access context.
 * @meta_set: see @nvme_meta_set for details.
 * @irq_set: see @nvme_irq_set for details.
 */
struct nvme_context {
	struct list_head	entry;
	struct list_head	cq_list;
	struct list_head	sq_list;
	struct nvme_device	*dev;
	struct mutex		lock;
	struct nvme_meta_set	meta_set;
	struct nvme_irq_set	irq_set;
};

extern struct list_head nvme_ctx_list;

static inline struct nvme_context *dnvme_irq_to_context(struct nvme_irq_set *irq_set)
{
	return container_of(irq_set, struct nvme_context, irq_set);
}

void dnvme_cleanup_context(struct nvme_context *ctx, enum nvme_state state);

#endif /* !_DNVME_CORE_H_ */
