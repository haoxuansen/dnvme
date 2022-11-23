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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpumask.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/mman.h>
#include <linux/dma-mapping.h>
#include <linux/version.h>

#include "core.h"
#include "io.h"
#include "cmb.h"
#include "debug.h"

#include "definitions.h"
#include "sysfuncproto.h"
#include "dnvme_reg.h"
#include "dnvme_ioctl.h"
#include "dnvme_queue.h"
#include "dnvme_ds.h"
#include "dnvme_cmds.h"
#include "dnvme_irq.h"

#define DEVICE_NAME			"nvme"
#define DRIVER_NAME			"dnvme"

#define API_VERSION                     0xfff10403
#define DRIVER_VERSION			0x20221115
#define DRIVER_VERSION_STR(VER)		#VER

/* bit[19:18] Type */
#define VMPGOFF_TO_TYPE(n)		(((n) >> 18) & 0x3)
#define VMPGOFF_TYPE_CQ			0
#define VMPGOFF_TYPE_SQ			1
#define VMPGOFF_TYPE_META		2
/* bit[17:0] Identify */
#define VMPGOFF_TO_ID(n)		(((n) >> 0) & 0x3ffff)

#define BAR0_BAR1			0x0
#define BAR2_BAR3			0x2
#define BAR4_BAR5			0x4

#ifndef VM_RESERVED
#define VM_RESERVED			(VM_DONTEXPAND | VM_DONTDUMP)
#endif
#ifndef ENOTSUP
#define ENOTSUP				EOPNOTSUPP
#endif

LIST_HEAD(nvme_ctx_list);

static int nvme_major;
static int nvme_minor = 0;
static struct class *nvme_class;
static struct nvme_driver nvme_drv;

/**
 * @brief Find nvme_context from linked list. 
 *  
 * @return &struct nvme_context on success, or ERR_PTR() on error. 
 */
static struct nvme_context *find_context(struct inode *inode)
{
	struct nvme_context *ctx;

	list_for_each_entry(ctx, &nvme_ctx_list, entry) {

		if (iminor(inode) == ctx->dev->priv.minor)
			return ctx;
	}
	return ERR_PTR(-ENODEV);
}

/**
 * @brief Lock the mutex if find nvme_context.
 * 
 * @return &struct nvme_context on success, or ERR_PTR() on error. 
 */
static struct nvme_context *lock_context(struct inode *inode)
{
	struct nvme_context *ctx;

	ctx = find_context(inode);
	if (IS_ERR(ctx)) {
		dnvme_err("Cannot find the device with minor no. %d\n", iminor(inode));
		return ctx;
	}

	mutex_lock(&ctx->lock);
	return ctx;
}

static void unlock_context(struct nvme_context *ctx)
{
	if (mutex_is_locked(&ctx->lock)) {
		mutex_unlock(&ctx->lock);
	} else {
		dnvme_warn("already unlocked, lock missmatch!\n");
	}
}

/**
 * @brief Parse vm_paoff in "struct vm_area_struct"
 * 
 * @param kva kernal virtual address
 * @param size mmap size
 * @return 0 on success, otherwise a negative errno.
 */
static int mmap_parse_vmpgoff(struct nvme_context *ctx, unsigned long vm_pgoff,
	void **kva, u32 *size)
{
	struct nvme_sq *sq;
	struct nvme_cq *cq;
	struct nvme_meta *meta;
	u32 type = VMPGOFF_TO_TYPE(vm_pgoff);
	u32 id = VMPGOFF_TO_ID(vm_pgoff);

	switch (type) {
	case VMPGOFF_TYPE_CQ:
		if (id > NVME_CQ_ID_MAX) {
			dnvme_err("CQ ID(%u) is out of range!\n", id);
			return -EINVAL;
		}

		cq = dnvme_find_cq(ctx, id);
		if (!cq) {
			dnvme_err("Cannot find CQ(%u)!\n", id);
			return -EBADSLT;
		}

		if (cq->priv.contig == 0) {
			dnvme_err("Cannot mmap non-contig CQ!\n");
			return -ENOTSUP;
		}
		*kva = cq->priv.buf;
		*size = cq->priv.size;
		break;

	case VMPGOFF_TYPE_SQ:
		if (id > NVME_SQ_ID_MAX) {
			dnvme_err("SQ ID(%u) is out of range!\n", id);
			return -EINVAL;
		}

		sq = dnvme_find_sq(ctx, id);
		if (!sq) {
			dnvme_err("Cannot find SQ(%u)!\n", id);
			return -EBADSLT;
		}

		if (sq->priv.contig == 0) {
			dnvme_err("Cannot mmap non-contig SQ!\n");
			return -ENOTSUP;
		}
		*kva = sq->priv.buf;
		*size = sq->priv.size;
		break;

	case VMPGOFF_TYPE_META:
		if (id > NVME_META_ID_MAX) {
			dnvme_err("Meta ID(%u) is out of range!\n", id);
			return -EINVAL;
		}

		meta = dnvme_find_meta(ctx, id);
		if (!meta) {
			dnvme_err("Cannot find Meta(%u)!\n", id);
			return -EBADSLT;
		}
		*kva = meta->buf;
		*size = ctx->meta_set.buf_size;
		break;

	default:
		dnvme_err("type(%u) is unknown!\n", type);
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief Maps the contiguous device mapped area to user space.
 * 
 * @param vma
 *   vm_pgoff: bit[19:18] - Type(0: CQ, 1: SQ, 2: meta data)
 *             bit[17:0] - Identify
 * @return 0 on success, otherwise a negative errno.
 */
static int dnvme_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct nvme_context *ctx;
	struct inode *inode = filp->f_path.dentry->d_inode;
	void *mmap_addr;
	u32 mmap_range;
	int npages;
	unsigned long pfn;
	int ret = 0;

	ctx = lock_context(inode);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	vma->vm_flags |= (VM_IO | VM_RESERVED);

	ret = mmap_parse_vmpgoff(ctx, vma->vm_pgoff, &mmap_addr, &mmap_range);
	if (ret < 0)
		goto out;

	/* !NOTE: Why add 1? Could replace by ALIGN()? */
	npages = (mmap_range / PAGE_SIZE) + 1;
	if ((npages * PAGE_SIZE) < (vma->vm_end - vma->vm_start)) {
		dnvme_err("Request to Map more than allocated pages...\n");
		ret = -EINVAL;
		goto out;
	}
	dnvme_vdbg("K.V.A = 0x%lx, PAGES = %d\n", (unsigned long)mmap_addr, npages);

	/* Associated struct page ptr for kernel logical address */
	pfn = virt_to_phys(mmap_addr) >> PAGE_SHIFT;
	if (!pfn) {
		dnvme_err("virt_to_phys err!\n");
		ret = -EFAULT;
		goto out;
	}
	dnvme_vdbg("PFN = 0x%lx", pfn);

	ret = remap_pfn_range(vma, vma->vm_start, pfn, 
		vma->vm_end - vma->vm_start, vma->vm_page_prot);
	if (ret < 0)
		dnvme_err("remap_pfn_rage err!(%d)\n", ret);

out:
	unlock_context(ctx);
	return ret;
}

static long dnvme_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = -EINVAL;
	struct nvme_context *ctx;
	struct inode *inode = inode = filp->f_path.dentry->d_inode;
	void __user *argp = (void __user *)arg;

	dnvme_dbg("cmd num:%u, arg:0x%lx (%s)\n", _IOC_NR(cmd), arg,
		dnvme_ioctl_cmd_string(cmd));
	ctx = lock_context(inode);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	switch (cmd) {
	case NVME_IOCTL_READ_GENERIC:
		ret = dnvme_generic_read(ctx, argp);
		break;

	case NVME_IOCTL_WRITE_GENERIC:
		ret = dnvme_generic_write(ctx, argp);
		break;

	case NVME_IOCTL_GET_CAPABILITY:
		ret = dnvme_get_capability(ctx, argp);
		break;

	case NVME_IOCTL_CREATE_ADMIN_QUEUE:
		ret = dnvme_create_admin_queue(ctx, argp);
		break;

	case NVME_IOCTL_SET_DEV_STATE:
		switch (arg) {
		case NVME_ST_ENABLE:
			ret = dnvme_set_ctrl_state(ctx, true);
			break;

		case NVME_ST_DISABLE:
		case NVME_ST_DISABLE_COMPLETE:
			if ((ret = dnvme_set_ctrl_state(ctx, false)) == 0) {
				device_cleanup(ctx, (enum nvme_state)arg);
			}
			break;

		case NVME_ST_RESET_SUBSYSTEM:
			ret = dnvme_reset_subsystem(ctx);
			/* !NOTICE: It's necessary to clean device here? */
			break;

		default:
			dnvme_err("nvme state(%lu) is unkonw!\n", arg);
			return -EINVAL;
		}
		break;

	case NVME_IOCTL_GET_QUEUE:
		ret = dnvme_get_queue(ctx, argp);
		break;

	case NVME_IOCTL_PREPARE_SQ_CREATION:
		ret = dnvme_prepare_sq(ctx, argp);
		break;

	case NVME_IOCTL_PREPARE_CQ_CREATION:
		ret = dnvme_prepare_cq(ctx, argp);
		break;

	case NVME_IOCTL_RING_SQ_DOORBELL:
		ret = dnvme_ring_sq_doorbell(ctx, (u16)arg);
		break;

	case NVME_IOCTL_SEND_64B_CMD:
		ret = dnvme_send_64b_cmd(ctx, argp);
		break;

	case NVME_IOCTL_TOXIC_64B_DWORD:
		ret = driver_toxic_dword(ctx, (struct backdoor_inject *)arg);
		break;

	case NVME_IOCTL_DUMP_METRICS:
		dnvme_vdbg("NVME_IOCTL_DUMP_METRICS");
		ret = driver_log((struct nvme_file *)arg);
		break;

	case NVME_IOCTL_REAP_INQUIRY:
		dnvme_vdbg("NVME_IOCTL_REAP_INQUIRY");
		ret = driver_reap_inquiry(ctx,
		(struct nvme_reap_inquiry *)arg);
		break;

	case NVME_IOCTL_REAP:
		dnvme_vdbg("NVME_IOCTL_REAP");
		ret = driver_reap_cq(ctx, (struct nvme_reap *)arg);
		break;

	case NVME_IOCTL_GET_DRIVER_METRICS:
		dnvme_vdbg("NVME_IOCTL_GET_DRIVER_METRICS");
		if (copy_to_user((struct nvme_driver *)arg,
		&nvme_drv, sizeof(struct nvme_driver))) {

		dnvme_err("Unable to copy to user space");
		ret = -EFAULT;
		} else {
		ret = 0;
		}
		break;

	case NVME_IOCTL_METABUF_CREATE:
		dnvme_vdbg("NVME_IOCTL_METABUF_CREATE");
		if (arg > MAX_METABUFF_SIZE) {
		dnvme_err("Meta buffer size exceeds max(0x%08X) > 0x%08X",
			MAX_METABUFF_SIZE, (u32)arg);
		ret = -EINVAL;
		} else {
		ret = metabuff_create(ctx, (u32)arg);
		}
		break;

	case NVME_IOCTL_METABUF_ALLOC:
		dnvme_vdbg("NVME_IOCTL_METABUF_ALLOC");
		ret = metabuff_alloc(ctx, (u32)arg);
		break;

	case NVME_IOCTL_METABUF_DELETE:
		dnvme_vdbg("NVME_IOCTL_METABUF_DELETE");
		ret = metabuff_del(ctx, (u32)arg);
		break;

	case NVME_IOCTL_SET_IRQ:
		dnvme_vdbg("NVME_IOCTL_SET_IRQ");
		ret = nvme_set_irq(ctx, (struct interrupts *)arg);
		break;

	case NVME_IOCTL_MASK_IRQ:
		dnvme_vdbg("NVME_IOCTL_MASK_IRQ");
		ret = nvme_mask_irq(ctx, (u16)arg);
		break;

	case NVME_IOCTL_UNMASK_IRQ:
		dnvme_vdbg("NVME_IOCTL_UNMASK_IRQ");
		ret = nvme_unmask_irq(ctx, (u16)arg);
		break;

	case NVME_IOCTL_GET_DEVICE_METRICS:
		dnvme_vdbg("NVME_IOCTL_GET_DEVICE_METRICS");
		if (copy_to_user((struct nvme_dev_public *)arg,
		&ctx->dev->pub,
		sizeof(struct nvme_dev_public))) {

		dnvme_err("Unable to copy to user space");
		ret = -EFAULT;
		} else {
		ret = 0;
		}
		break;

	case NVME_IOCTL_MARK_SYSLOG:
		dnvme_vdbg("NVME_IOCTL_MARK_SYSLOG");
		ret = driver_logstr((struct nvme_logstr *)arg);
		break;

	//***************************boot partition test MengYu***************************
	//   case NVME_IOCTL_WRITE_BP_BUF:
	//     dnvme_vdbg("NVME_IOCTL_WRITE_BP_BUF");
	//     err = driver_nvme_write_bp_buf((struct nvme_write_bp_buf *)arg, ctx);
	//     break;  
	//   case NVME_IOCTL_GET_BP_MEM_ADDR:
	//     dnvme_vdbg("NVME_IOCTL_GET_BP_MEM_ADDR");
	//     break;  
	//***************************boot partition test MengYu***************************
	default:
		dnvme_err("Unknown IOCTL");
		break;
	}

	unlock_context(ctx);
	return ret;
}

static int dnvme_open(struct inode *inode, struct file *filp)
{
	struct nvme_context *ctx;
	int ret = 0;

	ctx = lock_context(inode);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	if (ctx->dev->priv.opened) {
		dnvme_err("It's not allowed to open device more than once!\n");
		ret = -EPERM;
		goto out;
	}

	ctx->dev->priv.opened = 1;
	device_cleanup(ctx, NVME_ST_DISABLE_COMPLETE);
	dnvme_info("Open NVMe device ok!\n");
out:
	unlock_context(ctx);
	return ret;
}

static int dnvme_release(struct inode *inode, struct file *filp)
{
	struct nvme_context *ctx;

	ctx = lock_context(inode);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	dnvme_info("Close NVMe device ...\n");

	ctx->dev->priv.opened = 0;
	device_cleanup(ctx, NVME_ST_DISABLE_COMPLETE);
	unlock_context(ctx);
	return 0;
}

static const struct file_operations dnvme_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= dnvme_ioctl,
	.open		= dnvme_open,
	.release	= dnvme_release,
	.mmap		= dnvme_mmap,
};

static int dnvme_map_resource(struct nvme_context *ctx)
{
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->priv.pdev;
	unsigned long bars = 0;
	void __iomem *bar0 = NULL;
	void __iomem *bar1 = NULL;
	void __iomem *bar2 = NULL;
	int ret;

	/* Get the bitmask value of the BAR's supported by device */
	bars = (unsigned long)pci_select_bars(pdev, IORESOURCE_MEM);

	if (!test_bit(BAR0_BAR1, &bars)) {
		dnvme_err("BAR0 (64-bit) is not support!\n");
		return -ENODEV;
	}

	/* !TODO: Replace by @pci_request_mem_regions */
	if (request_mem_region(pci_resource_start(pdev, BAR0_BAR1),
		pci_resource_len(pdev, BAR0_BAR1), DRIVER_NAME) == NULL) {
		dnvme_err("BAR0 (64-bit) memory already in use!\n");
		return -EBUSY;
	}

	bar0 = ioremap(pci_resource_start(pdev, BAR0_BAR1),
		pci_resource_len(pdev, BAR0_BAR1));
	if (!bar0) {
		dnvme_err("Failed to map BAR0 (64-bit)!\n");
		ret = -EIO;
		goto out;
	}
	dnvme_info("BAR0 (64-bit) mapped to 0x%p ok!\n", bar0);

	/* Map BAR2 & BAR3 (BAR1 for 64-bit); I/O mapped registers  */
	if (test_bit(BAR2_BAR3, &bars)) {
		if (request_mem_region(pci_resource_start(pdev, BAR2_BAR3),
            		pci_resource_len(pdev, BAR2_BAR3), DRIVER_NAME) == NULL) {
			dnvme_err("BAR1 (64-bit) memory already in use!\n");
			ret = -EBUSY;
			goto out2;
		}

		bar1 = pci_iomap(pdev, BAR2_BAR3, pci_resource_len(pdev, BAR2_BAR3));
		if (!bar1) {
			dnvme_err("Failed to map BAR1 (64-bit)!\n");
			ret = -EIO;
			goto out3;
		}
		dnvme_info("BAR1 (64-bit) mapped to 0x%p ok!\n", bar1);
	} else {
		dnvme_warn("BAR1 (64-bit) is not support!\n");
	}

	/* Map BAR4 & BAR5 (BAR2 for 64-bit); MSIX table memory mapped */
	if (test_bit(BAR4_BAR5, &bars)) {
		if (request_mem_region(pci_resource_start(pdev, BAR4_BAR5),
			pci_resource_len(pdev, BAR4_BAR5), DRIVER_NAME) == NULL) {
			dnvme_err("BAR2 (64-bit) memory already in use!\n");
			ret = -EBUSY;
			goto out4;
		}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,8,0)
		bar2 = ioremap(pci_resource_start(pdev, BAR4_BAR5),
			pci_resource_len(pdev, BAR4_BAR5));
#else
		bar2 = ioremap_nocache(pci_resource_start(pdev, BAR4_BAR5),
			pci_resource_len(pdev, BAR4_BAR5));
#endif
		if (!bar2) {
			dnvme_err("Failed to map BAR2 (64-bit)!\n");
			ret = -EIO;
			goto out5;
		}
		dnvme_info("BAR2 (64-bit) mapped to 0x%p ok!\n", bar2);
	} else {
		dnvme_warn("BAR2 (64-bit) is not support!\n");
	}

	ndev->priv.bar0 = bar0;
	ndev->priv.bar1 = bar1;
	ndev->priv.bar2 = bar2;
	ndev->priv.dbs = bar0 + NVME_REG_DBS;
	ndev->priv.ctrlr_regs = bar0;
	return 0;
out5:
	if (test_bit(BAR4_BAR5, &bars))
		release_mem_region(pci_resource_start(pdev, BAR4_BAR5),
			pci_resource_len(pdev, BAR4_BAR5));
out4:
	if (bar1)
		iounmap(bar1);
out3:
	if (test_bit(BAR2_BAR3, &bars))
		release_mem_region(pci_resource_start(pdev, BAR2_BAR3),
			pci_resource_len(pdev, BAR2_BAR3));
out2:
	iounmap(bar0);
out:
	release_mem_region(pci_resource_start(pdev, BAR0_BAR1),
		pci_resource_len(pdev, BAR0_BAR1));
	return ret;
}

static void dnvme_unmap_resource(struct nvme_context *ctx)
{
	struct nvme_dev_private *priv = &ctx->dev->priv;
	struct pci_dev *pdev = priv->pdev;

	if (priv->bar2) {
		iounmap(priv->bar2);
		release_mem_region(pci_resource_start(pdev, BAR4_BAR5),
			pci_resource_len(pdev, BAR4_BAR5));
		priv->bar2 = NULL;
	}

	if (priv->bar1) {
		iounmap(priv->bar1);
		release_mem_region(pci_resource_start(pdev, BAR2_BAR3),
			pci_resource_len(pdev, BAR2_BAR3));
		priv->bar1 = NULL;
	}

	if (priv->bar0) {
		iounmap(priv->bar0);
		release_mem_region(pci_resource_start(pdev, BAR0_BAR1),
			pci_resource_len(pdev, BAR0_BAR1));
		priv->bar0 = NULL;
	}
}

static int dnvme_init_irq(struct nvme_context *ctx)
{
	ctx->dev->pub.irq_active.irq_type = INT_NONE;
	ctx->dev->pub.irq_active.num_irqs = 0;
	/* Will only be read by ISR */
	ctx->irq_process.irq_type = INT_NONE;
	return 0;
}

/**
 * @brief Alloc nvme context and initialize it. 
 *  
 * @return &struct nvme_context on success, or ERR_PTR() on error. 
 */
static struct nvme_context *dnvme_alloc_context(struct pci_dev *pdev)
{
	int ret = -ENOMEM;
	struct device *dev;
	struct nvme_context *ctx;
	struct nvme_device *ndev;
	struct dma_pool *pool;
	dev_t devno = MKDEV(nvme_major, nvme_minor);

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		dnvme_err("failed to alloc nvme_context!\n");
		return ERR_PTR(ret);
	}

	ndev = kzalloc(sizeof(*ndev), GFP_KERNEL);
	if (!ndev) {
		dnvme_err("failed to alloc nvme_device!\n");
		goto out;
	}

	/* 1. Initialize nvme device first. */
	ndev->priv.pdev = pdev;
	ndev->priv.dmadev = &pdev->dev;

	/* Used to create Coherent DMA mapping for PRP List */
	pool = dma_pool_create("prp page", &pdev->dev, PAGE_SIZE, PAGE_SIZE, 0);
	if (!pool) {
		dnvme_err("failed to create dma pool!\n");
		goto out2;
	}
	ndev->priv.prp_page_pool = pool;

	dev = device_create(nvme_class, NULL, devno, NULL, DEVICE_NAME"%d", nvme_minor);
	if (IS_ERR(dev)) {
		dnvme_err("failed to create device(%s%d)!\n", DEVICE_NAME, nvme_minor);
		ret = PTR_ERR(dev);
		goto out3;
	}
	dnvme_vdbg("Create device(%s%d) success!\n", DEVICE_NAME, nvme_minor);
	ndev->priv.spcl_dev = dev;
	ndev->priv.minor = nvme_minor;
	nvme_minor++;

	/* 2. Then initialize nvme context. */
	INIT_LIST_HEAD(&ctx->sq_list);
	INIT_LIST_HEAD(&ctx->cq_list);
	INIT_LIST_HEAD(&ctx->meta_set.meta_list);
	INIT_LIST_HEAD(&ctx->irq_process.irq_track_list);
	INIT_LIST_HEAD(&ctx->irq_process.wrk_item_list);

	mutex_init(&ctx->lock);
	mutex_init(&ctx->irq_process.irq_track_mtx);
	/* Spinlock to protect from kernel preemption in ISR handler */
	spin_lock_init(&ctx->irq_process.isr_spin_lock);

	ctx->dev = ndev;
	dnvme_init_irq(ctx);

	return ctx;
out3:
	dma_pool_destroy(pool);
out2:
	kfree(ndev);
out:
	kfree(ctx);
	return ERR_PTR(ret);
}

static void dnvme_release_context(struct nvme_context *ctx)
{
	struct nvme_device *ndev = ctx->dev;
	struct dma_pool *pool = ndev->priv.prp_page_pool;

	device_destroy(nvme_class, MKDEV(nvme_major, ndev->priv.minor));
	/* !FIXME: Shall recycle minor at here! */
	dma_pool_destroy(pool);
	kfree(ndev);
	kfree(ctx);
}

static int dnvme_set_dma_mask(struct pci_dev *pdev)
{
	int ret;

	if (dma_supported(&pdev->dev, DMA_BIT_MASK(64)) == 0) {
		dnvme_err("The device unable to address 64 bits of DMA\n");
		return -EPERM;
	}

	ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(64));
	if (ret < 0) {
		dnvme_err("Request 64-bit DMA has been rejected!\n");
		return ret;
	}

	ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(64));
	if (ret < 0) {
		dnvme_err("Request 64-bit coherent memory has been rejected!\n");
		return ret;
	}

	return ret;
}

static int dnvme_pci_enable(struct nvme_context *ctx)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_ctrl_property *prop = &ndev->prop;
	struct pci_dev *pdev = ndev->priv.pdev;
	void __iomem *bar0 = ndev->priv.bar0;
	int ret;

	pci_set_master(pdev);

	ret = pci_enable_device(pdev);
	if (ret < 0) {
		dnvme_err("failed to enable pci device!(%d)\n", ret);
		return ret;
	}

	prop->cap = dnvme_readq(bar0, NVME_REG_CAP);
	ndev->q_depth = NVME_CAP_MQES(prop->cap) + 1;
	ndev->db_stride = 1 << NVME_CAP_STRIDE(prop->cap);

	dnvme_dbg("db_stride:%u, q_depth:%u\n", ndev->db_stride, ndev->q_depth);
	return 0;
}

static void dnvme_pci_disable(struct nvme_context *ctx)
{
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->priv.pdev;

	if (pci_is_enabled(pdev))
		pci_disable_device(pdev);
}

static int dnvme_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int ret;
	struct nvme_context *ctx;

	dnvme_info("probe pdev...(cpu:%d %d %d)\n", num_possible_cpus(), 
		num_present_cpus(), num_active_cpus());

	ret = dnvme_set_dma_mask(pdev);
	if (ret < 0)
		return ret;

	ctx = dnvme_alloc_context(pdev);
	if (IS_ERR(ctx)) {
		ret = PTR_ERR(ctx);
		dnvme_err("failed to alloc context!(%d)\n", ret);
		return ret;
	}

	ret = dnvme_map_resource(ctx);
	if (ret < 0)
		goto out;

	ret = dnvme_pci_enable(ctx);
	if (ret < 0)
		goto out2;

	ret = dnvme_map_cmb(ctx->dev);
	if (ret < 0)
		goto out3;

	/* Finalize this device and prepare for next one */
	dnvme_info("NVMe device(0x%x:0x%x) init ok!\n", pdev->vendor, pdev->device);
	dnvme_vdbg("NVMe bus #%d, dev slot: %d", pdev->bus->number, 
		PCI_SLOT(pdev->devfn));
	dnvme_vdbg("NVMe func: 0x%x, class: 0x%x", PCI_FUNC(pdev->devfn),
		pdev->class);
	list_add_tail(&ctx->entry, &nvme_ctx_list);
	return 0;
out3:
	dnvme_pci_disable(ctx);
out2:
	dnvme_unmap_resource(ctx);
out:
	dnvme_release_context(ctx);
	return ret;
}

static void dnvme_remove(struct pci_dev *pdev)
{
	bool found = false;
	struct nvme_context *ctx;

	dnvme_info("Remove NVMe device(0x%x:0x%x) ...\n", pdev->vendor, pdev->device);
	dnvme_vdbg("NVMe bus #%d, dev slot: %d", pdev->bus->number, 
		PCI_SLOT(pdev->devfn));
	dnvme_vdbg("NVMe func: 0x%x, class: 0x%x", PCI_FUNC(pdev->devfn),
		pdev->class);

	list_for_each_entry(ctx, &nvme_ctx_list, entry) {
		if (pdev == ctx->dev->priv.pdev) {
			found = true;
			break;
		}
	}

	if (!found) {
		dnvme_warn("Cannot found dev in list!\n");
		return;
	}

	/* Wait for any other dnvme access to finish */
	mutex_lock(&ctx->lock);
	list_del(&ctx->entry);
	mutex_unlock(&ctx->lock);

	device_cleanup(ctx, NVME_ST_DISABLE_COMPLETE);
	dnvme_unmap_cmb(ctx->dev);
	pci_disable_device(pdev);

	dnvme_unmap_resource(ctx);
	dnvme_release_context(ctx);
}

static struct pci_device_id dnvme_id_table[] = {
	{ PCI_DEVICE_CLASS(PCI_CLASS_STORAGE_EXPRESS, 0xffffff) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, dnvme_id_table);

static struct pci_driver dnvme_driver = {
	.name		= DRIVER_NAME,
	.id_table	= dnvme_id_table,
	.probe		= dnvme_probe,
	.remove		= dnvme_remove,
};

static int __init dnvme_init(void)
{
	int ret;

	nvme_drv.api_version = API_VERSION;
	nvme_drv.driver_version = DRIVER_VERSION;

	nvme_major = register_chrdev(0, DEVICE_NAME, &dnvme_fops);
	if (nvme_major < 0) {
		dnvme_err("failed to register chrdev!(%d)\n", nvme_major);
		return nvme_major;
	}

	nvme_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(nvme_class)) {
		ret = PTR_ERR(nvme_class);
		dnvme_err("failed to create %s class!(%d)\n", DEVICE_NAME, ret);
		goto out;
	}

	ret = pci_register_driver(&dnvme_driver);
	if (ret < 0) {
		dnvme_err("failed to register pci driver!(%d)\n", ret);
		goto out2;
	}

	dnvme_info("init ok!(api_ver:%x, drv_ver:%x)\n", nvme_drv.api_version, 
		nvme_drv.driver_version);
	return 0;

out2:
	class_destroy(nvme_class);
out:
	unregister_chrdev(nvme_major, DEVICE_NAME);
	return ret;
}
module_init(dnvme_init);

static void __exit dnvme_exit(void)
{
	pci_unregister_driver(&dnvme_driver);
	class_destroy(nvme_class);
	unregister_chrdev(nvme_major, DEVICE_NAME);
	dnvme_info("exit ok!(api_ver:%x, drv_ver:%x)\n", nvme_drv.api_version, 
		nvme_drv.driver_version);
}
module_exit(dnvme_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:"DRIVER_NAME);
MODULE_AUTHOR("nvmecompliance@intel.com");
MODULE_DESCRIPTION("NVMe compliance suite kernel driver");
MODULE_VERSION(DRIVER_VERSION_STR(DRIVER_VERSION));

