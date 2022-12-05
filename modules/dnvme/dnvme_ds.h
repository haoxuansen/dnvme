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

#ifndef _DNVME_DS_H_
#define _DNVME_DS_H_

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>

#include "dnvme_ioctl.h"

/* To store the max vector locations */
#define    MAX_VEC_SLT              2048

/**
 * @dma: An array used to hold dma address of prp page. The number of array
 *  number is @npages.
 */
struct nvme_prps {
	u32	npages; /* No. of pages inside the PRP List */
	u32	type; /* refers to types of PRP Possible */
	/* List of virtual pointers to PRP List pages */
	__le64	**vir_prp_list;
	dma_addr_t	*dma;
	__le64	prp1; /* Physical address in PRP1 of command */
	__le64	prp2; /* Physical address in PRP2 of command */
	dma_addr_t	first_dma; /* First entry in PRP List */

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
#define NVME_QF_WAIT_FOR_CREATE         0x01

	struct nvme_prps	prp_persist; /* PRP element in CQ */
};

/*
 *    Structure definition for tracking the commands.
 */
struct nvme_cmd {
	u16	unique_id;      /* driver assigned unique id for a particular cmd */
	u16	persist_q_id;   /* target Q ID used for Create/Delete Q's, never == 0 */
	u8	opcode;         /* command opcode as per spec */
	struct list_head	entry; /* link-list using the kernel list */
	struct nvme_prps	prp_nonpersist; /* Non persistent PRP entries */
};

/**
 * @brief NVMe SQ private information
 * 
 * @bit_mask: see struct nvme_cq_private bit_mask field for details.
 */
struct nvme_sq_private {
	void		*buf; /* virtual kernal address using kmalloc */
	dma_addr_t	dma; /* dma mapped address using dma_alloc */
	u32		size; /* len in bytes of allocated Q in kernel */
	u32 __iomem	*dbs; /* Door Bell stride */
	u16		unique_cmd_id; /* unique counter for each comand in SQ */
	u8		contig; /* Indicates if prp list is contig or not */
	u8		bit_mask;
	struct nvme_prps	prp_persist; /* PRP element in CQ */
	struct list_head	cmd_list; /* link-list head for nvme_cmd list */
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
	struct nvme_ctrl_reg __iomem	*ctrlr_regs;  /* Pointer to reg space */
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
	u8	irq_type; /* Type of IRQ set */

	/* Used by ISR to enqueue work to BH */
	struct workqueue_struct	*wq; /* Wq per device */

	/* Used by BH to dequeue work and process on it */
	/* Head of nvme_work's list */
	/* Remains static throughout the lifetime of the interrupts */
	struct list_head	work_list;
};

/*
 * Structure which defines the device list for all the data structures
 * that are defined.
 */
struct nvme_context {
	struct list_head	entry; /* metrics linked list head */
	struct list_head	cq_list; /* CQ linked list */
	struct list_head	sq_list; /* SQ linked list */
	struct nvme_device	*dev; /* Pointer to this nvme device */
	struct mutex		lock; /* Mutex for locking per device */
	struct nvme_meta_set	meta_set; /* Pointer to meta data buff */
	struct nvme_irq_set	irq_set; /* IRQ processing structure */
};

/* Global linked list for the entire data structure for all devices. */
extern struct list_head nvme_ctx_list;

#endif
