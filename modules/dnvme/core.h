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
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/xarray.h>

#include "pci_caps.h"
#include "dnvme_ioctl.h"

#define NVME_SQ_ID_MAX			U16_MAX
#define NVME_CQ_ID_MAX			U16_MAX
#define NVME_META_ID_MAX		((0x1 << 18) - 1)

#define NVME_META_BUF_ALIGN		4

#define NVME_PRP_ENTRY_SIZE		8 /* in bytes */

//#undef pr_fmt
//#define pr_fmt(fmt)			"[%s,%d]" fmt, __func__, __LINE__

#if !IS_ENABLED(CONFIG_PRINTK_COLOR)
#include "log/color.h"

#define dnvme_err(ndev, fmt, ...) \
	dev_err(&ndev->dev, LOG_COLOR_RED fmt, ##__VA_ARGS__)
#define dnvme_warn(ndev, fmt, ...) \
	dev_warn(&ndev->dev, LOG_COLOR_YELLOW fmt, ##__VA_ARGS__)
#define dnvme_notice(ndev, fmt, ...) \
	dev_notice(&ndev->dev, LOG_COLOR_BLUE fmt, ##__VA_ARGS__)
#define dnvme_info(ndev, fmt, ...) \
	dev_info(&ndev->dev, LOG_COLOR_GREEN fmt, ##__VA_ARGS__)
#define dnvme_dbg(ndev, fmt, ...) \
	dev_dbg(&ndev->dev, LOG_COLOR_NONE fmt, ##__VA_ARGS__)
#define dnvme_vdbg(ndev, fmt, ...) \
	dev_vdbg(&ndev->dev, LOG_COLOR_NONE fmt, ##__VA_ARGS__)

#endif

/**
 * @prp_list: If use PRP, this field point to PRP list pages. If use SGL, 
 *  this field point to the first segment of SGLs.
 * @pg_addr: This is an array for storing physical aaddress of allocated PAGES.
 *  The number of array is @nr_pages.
 * @nr_pages: The number of pages are used to store PRP list or SGL segment.
 */
struct nvme_prps {
	struct dma_pool	*pg_pool;

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
	struct nvme_prps	*prps;
};

/**
 * @brief NVMe SQ private information
 * 
 * @bit_mask: see "struct nvme_cq_private" bit_mask field for details.
 * @cmd_list: This is a list head for managing "struct nvme_cmd"
 */
struct nvme_sq_private {
	void		*buf; /* virtual kernal address using kmalloc */
	dma_addr_t	dma; /* dma mapped address using dma_alloc */
	u32		size; /* len in bytes of allocated Q in kernel */
	u32 __iomem	*dbs; /* Door Bell stride */
	u16		next_cid; /* unique counter for each comand in SQ */
	u8		contig; /* Indicates if prp list is contig or not */
	u8		bit_mask;
	struct list_head	cmd_list;
};

/*
 * struct nvme_cq - representation of a completion queue.
 */
struct nvme_cq {
	struct nvme_cq_public	pub;
	struct nvme_cq_private	priv;
	struct nvme_context	*ctx;
	struct nvme_prps	*prps;
};

/*
 * struct nvme_sq - representation of a submisssion queue.
 */
struct nvme_sq {
	struct nvme_sq_public	pub;
	struct nvme_sq_private	priv;
	struct nvme_context	*ctx;
	struct nvme_prps	*prps;
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
 * @isr_fired: indicate whether the irq is fired
 * @isr_count: count the number of times irq fired
 */
struct nvme_irq {
	struct list_head	irq_entry;
	struct list_head	icq_list;
	u16			irq_id;
	u32			int_vec; /* vec number; assigned by OS */

	atomic_t		isr_fired;
	atomic_t		isr_count;
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

struct nvme_capability {
	struct pci_cap_pm	*pm;
	struct pci_cap_msi	*msi;
	struct pci_cap_msix	*msix;
	struct pci_cap_express	*express;
};

/**
 * @brief Representation of a NVMe device
 * 
 * @dbs: Refer to "NVMe over PCIe Transport Spec R1.0b - ch3.1.2"
 * @cmb_size: Actually mapped CMB size, may less than CMBSZ which is indicated
 *  in Controller Properities. 
 * @cmb_use_sqes: If true, use controller's memory buffer for I/O SQes.
 */
struct nvme_device {
	struct nvme_context	*ctx;
	struct pci_dev	*pdev;
	struct device	dev;
	struct cdev	cdev;
	struct proc_dir_entry	*proc;

	struct xarray	sqs;
	struct xarray	cqs;

	int	instance; /* dev_t minor */

	void __iomem	*bar0;
	u32 __iomem	*dbs;

	struct dma_pool	*cmd_pool; /* PAGE */
	struct dma_pool	*queue_pool; /* PAGE */

	struct nvme_dev_public	pub;
	struct nvme_ctrl_property	prop;
	struct nvme_capability	cap;

	u32	q_depth;
	u32	db_stride;
	u64	cmb_size;
	bool	cmb_use_sqes;

	unsigned int	opened:1;
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
	struct list_head	irq_list; /* IRQ list; sorted by irq_no */

	/* To resolve contention for ISR's getting scheduled on different cores */
	spinlock_t	spin_lock;

	struct {
		u16		irqs;
		void __iomem	*tb; /* Pointer to MSI-X table */
		void __iomem	*pba; /* Pointer to MSI-X PBA */
	} msix;

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
