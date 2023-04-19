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
#include "dnvme.h"

#define NVME_PRP_ENTRY_SIZE		8 /* in bytes */
#define NVME_SGES_PER_PAGE		(PAGE_SIZE / sizeof(struct nvme_sgl_desc))
#define NVME_PRPS_PER_PAGE		(PAGE_SIZE / NVME_PRP_ENTRY_SIZE)

#define PCI_BAR_MAX_NUM			6

#undef pr_fmt
#define pr_fmt(fmt)			"[%s,%d]" fmt, __func__, __LINE__

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

	void		**prp_list;
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

/*
 * struct nvme_cq - representation of a completion queue.
 */
struct nvme_cq {
	struct nvme_cq_public	pub;
	struct nvme_device	*ndev;

	/* For contiguous queue */
	void			*buf; /* store CQ entries */
	dma_addr_t		dma; /* physical addr of @buf */
	u32			size; /* lenth in bytes of @buf */

	/* For discontiguous queue */
	struct nvme_prps	*prps;

	u32 __iomem		*db; /* head doorbell */

	unsigned int		contig:1; /* queue is contiguous? */
	unsigned int		created:1; /* queue has been created? */
	unsigned int		use_cmb:1; /* queue is located in CMB? */
};

/*
 * struct nvme_sq - representation of a submisssion queue.
 */
struct nvme_sq {
	struct nvme_sq_public	pub;
	struct nvme_device	*ndev;
	struct list_head	cmd_list;

	/* For contiguous queue */
	void			*buf; /* store CQ entries */
	dma_addr_t		dma; /* physical addr of @buf */
	u32			size; /* lenth in bytes of @buf */

	/* For discontiguous queue */
	struct nvme_prps	*prps;

	u32 __iomem		*db; /* tail doorbell */
	u16			next_cid; /* command identifier */

	unsigned int		contig:1; /* queue is contiguous? */
	unsigned int		created:1; /* queue has been created? */
	unsigned int		use_cmb:1; /* queue is located in CMB? */
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
 * @irq_entry: nvme_irq is managed by nvme_irq_set
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
 * Structure for meta data buffer allocations.
 */
struct nvme_meta {
	u32			id;

	/* For contigous buffer */
	void			*buf;
	dma_addr_t		dma;
	u32			size;

	/* For SGL list */
	struct nvme_prps	*prps;

	unsigned int		contig:1;
};

struct nvme_capability {
	struct pci_cap_pm	*pm;
	struct pci_cap_msi	*msi;
	struct pci_cap_msix	*msix;
	struct pci_cap_express	*express;
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

	enum nvme_irq_type	irq_type;
	u16			nr_irq;

	struct workqueue_struct	*wq;
	struct list_head	work_list;
};

/**
 * @brief Describe Persistent Memory Region information
 */
struct nvme_pmr {
	u32		bir; /* base indicator register */
	u32		timeout; /* ms */

	resource_size_t	addr;
	resource_size_t size;

	unsigned int	cmss:1;
};

/**
 * @brief Describe Controller Memory Buffer information
 */
struct nvme_cmb {
	u8		bar;

	u64		res_addr; /* resource address */
	u64		bus_addr;
	u64		size;
	u64		offset;

	/* CMBSZ Capability */
	unsigned int	sqs:1;
	unsigned int	cqs:1;
	unsigned int	lists:1;
	unsigned int	rds:1;
	unsigned int	wds:1;
	/* CMBLOC Capability */
	unsigned int	cqmms:1;
	unsigned int	cqpds:1;
	unsigned int	cdpmls:1;
	unsigned int	cdpcils:1;
	unsigned int	cdmmms:1;
	unsigned int	cqda:1;
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
	struct list_head	entry;
	struct pci_dev	*pdev;
	struct device	dev;
	struct cdev	cdev;
	struct proc_dir_entry	*proc;

	struct xarray	sqs;
	struct xarray	cqs;
	struct xarray	meta;

	struct mutex	lock;

	int	instance; /* dev_t minor */

	void __iomem	*bar[PCI_BAR_MAX_NUM];
	u32 __iomem	*dbs;

	struct dma_pool	*cmd_pool;
	struct dma_pool	*queue_pool;
	struct dma_pool *meta_pool;

	struct nvme_irq_set	irq_set;
	struct nvme_capability	cap;
	struct nvme_pmr		*pmr;
	struct nvme_cmb		*cmb;

	u64	reg_cap;
	u32	q_depth;
	u32	db_stride;

	unsigned int	opened:1;
};

extern struct list_head nvme_dev_list;
extern int nvme_gnl_id;

static inline struct nvme_device *dnvme_irq_to_device(struct nvme_irq_set *irq_set)
{
	return container_of(irq_set, struct nvme_device, irq_set);
}

struct nvme_device *dnvme_lock_device(int instance);
void dnvme_unlock_device(struct nvme_device *ndev);

void dnvme_cleanup_device(struct nvme_device *ndev, enum nvme_state state);

/* ==================== Related to "cmb.c" ==================== */

bool dnvme_cmb_support_sq(struct nvme_cmb *cmb);
bool dnvme_cmb_support_cq(struct nvme_cmb *cmb);

int dnvme_map_cmb(struct nvme_device *ndev);
void dnvme_unmap_cmb(struct nvme_device *ndev);

/* ==================== Related to "cmd.c" ==================== */

void dnvme_sgl_set_data(struct nvme_sgl_desc *sge, struct scatterlist *sg);
void dnvme_sgl_set_seg(struct nvme_sgl_desc *sge, dma_addr_t dma_addr, 
	int entries);

int dnvme_map_user_page(struct nvme_device *ndev, struct nvme_prps *prps, 
	void __user *data, unsigned int size, 
	enum dma_data_direction dir, bool access);
void dnvme_unmap_user_page(struct nvme_device *ndev, struct nvme_prps *prps);

void dnvme_release_prps(struct nvme_device *ndev, struct nvme_prps *prps);

void dnvme_delete_cmd_list(struct nvme_device *ndev, struct nvme_sq *sq);

int dnvme_submit_64b_cmd(struct nvme_device *ndev, struct nvme_64b_cmd __user *ucmd);

/* ==================== Related to "meta.c" ==================== */

int dnvme_create_meta_node(struct nvme_device *ndev, 
	struct nvme_meta_create __user *umc);
void dnvme_delete_meta_id(struct nvme_device *ndev, u32 id);

void dnvme_delete_meta_nodes(struct nvme_device *ndev);

/* ==================== Related to "netlink.c" ==================== */

int dnvme_gnl_init(void);
void dnvme_gnl_exit(void);

/* ==================== Related to "pmr.c" ==================== */

int dnvme_map_pmr(struct nvme_device *ndev);
void dnvme_unmap_pmr(struct nvme_device *ndev);

#endif /* !_DNVME_CORE_H_ */

