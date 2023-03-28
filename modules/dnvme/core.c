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

#include "dnvme_ioctl.h"

#include "core.h"
#include "proc.h"
#include "pci.h"
#include "io.h"
#include "cmb.h"
#include "cmd.h"
#include "ioctl.h"
#include "irq.h"
#include "queue.h"
#include "debug.h"

#define NVME_MINORS			(1U << MINORBITS)

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

#ifndef VM_RESERVED
#define VM_RESERVED			(VM_DONTEXPAND | VM_DONTDUMP)
#endif
#ifndef ENOTSUP
#define ENOTSUP				EOPNOTSUPP
#endif

LIST_HEAD(nvme_ctx_list);
static DEFINE_MUTEX(nvme_ctx_list_lock);
static struct proc_dir_entry *nvme_proc_dir;

static DEFINE_IDA(nvme_instance_ida);
static dev_t nvme_chr_devt;
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

		if (iminor(inode) == ctx->dev->instance) {
			return ctx;
		}
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
		pr_err("Cannot find the device with minor no. %d!\n", iminor(inode));
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
		dnvme_warn(ctx->dev, "already unlocked, lock missmatch!\n");
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
	struct nvme_device *ndev = ctx->dev;
	struct nvme_sq *sq;
	struct nvme_cq *cq;
	struct nvme_meta *meta;
	u32 type = VMPGOFF_TO_TYPE(vm_pgoff);
	u32 id = VMPGOFF_TO_ID(vm_pgoff);

	switch (type) {
	case VMPGOFF_TYPE_CQ:
		if (id > NVME_CQ_ID_MAX) {
			dnvme_err(ndev, "CQ ID(%u) is out of range!\n", id);
			return -EINVAL;
		}

		cq = dnvme_find_cq(ctx, id);
		if (!cq) {
			dnvme_err(ndev, "Cannot find CQ(%u)!\n", id);
			return -EBADSLT;
		}

		if (cq->priv.contig == 0) {
			dnvme_err(ndev, "Cannot mmap non-contig CQ!\n");
			return -ENOTSUP;
		}
		*kva = cq->priv.buf;
		*size = cq->priv.size;
		break;

	case VMPGOFF_TYPE_SQ:
		if (id > NVME_SQ_ID_MAX) {
			dnvme_err(ndev, "SQ ID(%u) is out of range!\n", id);
			return -EINVAL;
		}

		sq = dnvme_find_sq(ctx, id);
		if (!sq) {
			dnvme_err(ndev, "Cannot find SQ(%u)!\n", id);
			return -EBADSLT;
		}

		if (sq->priv.contig == 0) {
			dnvme_err(ndev, "Cannot mmap non-contig SQ!\n");
			return -ENOTSUP;
		}
		*kva = sq->priv.buf;
		*size = sq->priv.size;
		break;

	case VMPGOFF_TYPE_META:
		if (id > NVME_META_ID_MAX) {
			dnvme_err(ndev, "Meta ID(%u) is out of range!\n", id);
			return -EINVAL;
		}

		meta = dnvme_find_meta(ctx, id);
		if (!meta) {
			dnvme_err(ndev, "Cannot find Meta(%u)!\n", id);
			return -EBADSLT;
		}
		*kva = meta->buf;
		*size = ctx->meta_set.buf_size;
		break;

	default:
		dnvme_err(ndev, "type(%u) is unknown!\n", type);
		return -EINVAL;
	}

	return 0;
}

/*
 * Called to clean up the driver data structures
 */
void dnvme_cleanup_context(struct nvme_context *ctx, enum nvme_state state)
{
	dnvme_clear_interrupt(ctx);
	/* Clean Up the data structures */
	dnvme_delete_all_queues(ctx, state);
	dnvme_destroy_meta_pool(ctx);
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
	struct nvme_device *ndev;
	struct inode *inode = filp->f_path.dentry->d_inode;
	void *mmap_addr;
	u32 mmap_range;
	int npages;
	unsigned long pfn;
	int ret = 0;

	ctx = lock_context(inode);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);
	ndev = ctx->dev;

	vma->vm_flags |= (VM_IO | VM_RESERVED);

	ret = mmap_parse_vmpgoff(ctx, vma->vm_pgoff, &mmap_addr, &mmap_range);
	if (ret < 0)
		goto out;

	/* !NOTE: Why add 1? Could replace by ALIGN()? */
	npages = (mmap_range / PAGE_SIZE) + 1;
	if ((npages * PAGE_SIZE) < (vma->vm_end - vma->vm_start)) {
		dnvme_err(ndev, "Request to Map more than allocated pages...\n");
		ret = -EINVAL;
		goto out;
	}
	dnvme_vdbg(ndev, "K.V.A = 0x%lx, PAGES = %d\n", (unsigned long)mmap_addr, npages);

	/* Associated struct page ptr for kernel logical address */
	pfn = virt_to_phys(mmap_addr) >> PAGE_SHIFT;
	if (!pfn) {
		dnvme_err(ndev, "virt_to_phys err!\n");
		ret = -EFAULT;
		goto out;
	}
	dnvme_vdbg(ndev, "PFN = 0x%lx", pfn);

	ret = remap_pfn_range(vma, vma->vm_start, pfn, 
		vma->vm_end - vma->vm_start, vma->vm_page_prot);
	if (ret < 0)
		dnvme_err(ndev, "remap_pfn_rage err!(%d)\n", ret);

out:
	unlock_context(ctx);
	return ret;
}

static long dnvme_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct nvme_context *ctx;
	struct nvme_device *ndev;
	struct inode *inode = inode = filp->f_path.dentry->d_inode;
	void __user *argp = (void __user *)arg;

	dnvme_dbg(ndev, "cmd num:%u, arg:0x%lx (%s)\n", _IOC_NR(cmd), arg,
		dnvme_ioctl_cmd_string(cmd));

	ctx = lock_context(inode);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);
	ndev = ctx->dev;

	switch (cmd) {
	case NVME_IOCTL_GET_DRIVER_INFO:
		if (copy_to_user(argp, &nvme_drv, sizeof(struct nvme_driver))) {
			dnvme_err(ndev, "failed to copy to user space!\n");
			ret = -EFAULT;
		}
		break;

	case NVME_IOCTL_GET_DEVICE_INFO:
		if (copy_to_user(argp, &ndev->pub, sizeof(ndev->pub))) {
			dnvme_err(ndev, "failed to copy to user space!\n");
			ret = -EFAULT;
		}
		break;

	case NVME_IOCTL_GET_SQ_INFO:
		ret = dnvme_get_sq_info(ctx, argp);
		break;

	case NVME_IOCTL_GET_CQ_INFO:
		ret = dnvme_get_cq_info(ctx, argp);
		break;

	case NVME_IOCTL_READ_GENERIC:
		ret = dnvme_generic_read(ctx, argp);
		break;

	case NVME_IOCTL_WRITE_GENERIC:
		ret = dnvme_generic_write(ctx, argp);
		break;

	case NVME_IOCTL_GET_CAPABILITY:
		ret = dnvme_get_capability(ndev, argp);
		break;

	case NVME_IOCTL_CREATE_ADMIN_QUEUE:
		ret = dnvme_create_admin_queue(ctx, argp);
		break;

	case NVME_IOCTL_SET_DEV_STATE:
		ret = dnvme_set_device_state(ctx, (enum nvme_state)arg);
		break;

	case NVME_IOCTL_PREPARE_IOSQ:
		ret = dnvme_prepare_sq(ctx, argp);
		break;

	case NVME_IOCTL_PREPARE_IOCQ:
		ret = dnvme_prepare_cq(ctx, argp);
		break;

	case NVME_IOCTL_RING_SQ_DOORBELL:
		ret = dnvme_ring_sq_doorbell(ctx, (u16)arg);
		break;

	case NVME_IOCTL_SUBMIT_64B_CMD:
		ret = dnvme_submit_64b_cmd(ctx, argp);
		break;

	case NVME_IOCTL_INQUIRY_CQE:
		ret = dnvme_inquiry_cqe(ctx, argp);
		break;

	case NVME_IOCTL_REAP_CQE:
		ret = dnvme_reap_cqe(ctx, argp);
		break;

	case NVME_IOCTL_CREATE_META_POOL:
		ret = dnvme_create_meta_pool(ctx, (u32)arg);
		break;

	case NVME_IOCTL_DESTROY_META_POOL:
		dnvme_destroy_meta_pool(ctx);
		break;

	case NVME_IOCTL_CREATE_META_NODE:
		ret = dnvme_create_meta_node(ctx, (u32)arg);
		break;

	case NVME_IOCTL_DELETE_META_NODE:
		dnvme_delete_meta_node(ctx, (u32)arg);
		break;

	case NVME_IOCTL_COMPARE_META_NODE:
		ret = dnvme_compare_meta_node(ctx, argp);
		break;

	case NVME_IOCTL_SET_IRQ:
		ret = dnvme_set_interrupt(ctx, argp);
		break;

	case NVME_IOCTL_MASK_IRQ:
		ret = dnvme_mask_interrupt(&ctx->irq_set, (u16)arg);
		break;

	case NVME_IOCTL_UNMASK_IRQ:
		ret = dnvme_unmask_interrupt(&ctx->irq_set, (u16)arg);
		break;

	default:
		dnvme_err(ndev, "cmd(%u) is unknown!\n", _IOC_NR(cmd));
		ret = -EINVAL;
	}

	unlock_context(ctx);
	return ret;
}

static int dnvme_open(struct inode *inode, struct file *filp)
{
	struct nvme_context *ctx;
	struct nvme_device *ndev;
	int ret = 0;

	ctx = lock_context(inode);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);
	ndev = ctx->dev;

	if (ctx->dev->opened) {
		dnvme_err(ndev, "It's not allowed to open device more than once!\n");
		ret = -EPERM;
		goto out;
	}

	ctx->dev->opened = 1;
	/* !TODO: There is no need to clean device */
	dnvme_cleanup_context(ctx, NVME_ST_DISABLE_COMPLETE);
	dnvme_info(ndev, "Open NVMe device ok!\n");
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

	dnvme_info(ctx->dev, "Close NVMe device ...\n");

	ctx->dev->opened = 0;
	/* !TODO: shall reset nvme device before delete I/O queue?
	 * Otherwise, the information saved by the driver may be inconsistent
	 * with the device.
	 */
	dnvme_cleanup_context(ctx, NVME_ST_DISABLE_COMPLETE);
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

/**
 * @brief map pci device resource
 * 
 * @return 0 on success, otherwise a negative errno.
 * @note Refer to NVMe over PCIe Transport Spec R1.0b - ch3.8.1
 * 	BAR0 - Memory Register Base Address, lower 32-bits
 * 	BAR1 - Memory Register Base Address, upper 32-bits
 * 	BAR2 - Index/Data Pair Register Base Address or Vendor Specific
 * 	BAR3..5 - Vendor Specific
 */
static int dnvme_map_resource(struct nvme_context *ctx)
{
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->pdev;
	void __iomem *bar0 = NULL;
	int ret;

	if (!(pci_select_bars(pdev, IORESOURCE_MEM) & BIT(0))) {
		dnvme_err(ndev, "BAR0 (64-bit) is not support!\n");
		return -ENODEV;
	}

	ret = pci_request_region(pdev, 0, "bar0");
	if (ret < 0) {
		dnvme_err(ndev, "BAR0 (64-bit) already in use!(%d)\n", ret);
		return ret;
	}

	bar0 = ioremap(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
	if (!bar0) {
		dnvme_err(ndev, "failed to map BAR0 (64-bit)!\n");
		ret = -ENOMEM;
		goto out;
	}
	dnvme_info(ndev, "BAR0: 0x%llx + 0x%llx mapped to 0x%p!\n", 
		pci_resource_start(pdev, 0), pci_resource_len(pdev, 0), bar0);

	ndev->bar0 = bar0;
	ndev->dbs = bar0 + NVME_REG_DBS;
	return 0;
out:
	pci_release_region(pdev, 0);
	return ret;
}

static void dnvme_unmap_resource(struct nvme_context *ctx)
{
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->pdev;

	if (ndev->bar0) {
		iounmap(ndev->bar0);
		ndev->bar0 = NULL;
		pci_release_region(pdev, 0);
	}
}

static int dnvme_init_irq(struct nvme_context *ctx)
{
	ctx->dev->pub.irq_active.irq_type = NVME_INT_NONE;
	ctx->dev->pub.irq_active.num_irqs = 0;
	/* Will only be read by ISR */
	ctx->irq_set.irq_type = NVME_INT_NONE;
	return 0;
}

/**
 * @brief Alloc nvme context and initialize it. 
 *  
 * @return &struct nvme_context on success, or NULL on error. 
 */
static struct nvme_context *dnvme_alloc_context(struct pci_dev *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct nvme_context *ctx;
	struct nvme_device *ndev;
	struct dma_pool *pool;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		dev_err(dev, "failed to alloc nvme_context!\n");
		return NULL;
	}

	ndev = kzalloc(sizeof(*ndev), GFP_KERNEL);
	if (!ndev) {
		dev_err(dev, "failed to alloc nvme_device!\n");
		goto out_free_ctx;
	}

	/* 1. Initialize nvme device first. */
	ndev->pdev = pdev;

	/* Used to create Coherent DMA mapping for PRP List */
	pool = dma_pool_create("prp page", dev, PAGE_SIZE, PAGE_SIZE, 0);
	if (!pool) {
		dev_err(dev, "failed to create dma pool!\n");
		goto out_free_ndev;
	}
	ndev->page_pool = pool;

	ret = ida_simple_get(&nvme_instance_ida, 0, NVME_MINORS, GFP_KERNEL);
	if (ret < 0)
		goto out_destroy_pool;
	ndev->instance = ret;

	device_initialize(&ndev->dev);
	ndev->dev.devt = MKDEV(MAJOR(nvme_chr_devt), ndev->instance);
	ndev->dev.class = nvme_class;
	ndev->dev.parent = &pdev->dev;
	dev_set_drvdata(&ndev->dev, ndev);
	ret = dev_set_name(&ndev->dev, "nvme%d", ndev->instance);
	if (ret)
		goto out_release_instance;

	cdev_init(&ndev->cdev, &dnvme_fops);
	ndev->cdev.owner = THIS_MODULE;
	ret = cdev_device_add(&ndev->cdev, &ndev->dev);
	if (ret)
		goto out_free_name;

	/* 2. Then initialize nvme context. */
	INIT_LIST_HEAD(&ctx->sq_list);
	INIT_LIST_HEAD(&ctx->cq_list);
	INIT_LIST_HEAD(&ctx->meta_set.meta_list);
	INIT_LIST_HEAD(&ctx->irq_set.irq_list);
	INIT_LIST_HEAD(&ctx->irq_set.work_list);

	mutex_init(&ctx->lock);
	/* Spinlock to protect from kernel preemption in ISR handler */
	spin_lock_init(&ctx->irq_set.spin_lock);

	ctx->dev = ndev;
	ndev->ctx = ctx;
	dnvme_init_irq(ctx);

	return ctx;
out_free_name:
	kfree_const(ndev->dev.kobj.name);
out_release_instance:
	ida_simple_remove(&nvme_instance_ida, ndev->instance);
out_destroy_pool:
	dma_pool_destroy(pool);
out_free_ndev:
	kfree(ndev);
out_free_ctx:
	kfree(ctx);
	return NULL;
}

static void dnvme_release_context(struct nvme_context *ctx)
{
	struct nvme_device *ndev = ctx->dev;
	struct dma_pool *pool = ndev->page_pool;

	cdev_device_del(&ndev->cdev, &ndev->dev);
	kfree_const(ndev->dev.kobj.name);
	ida_simple_remove(&nvme_instance_ida, ndev->instance);
	dma_pool_destroy(pool);
	kfree(ndev);
	kfree(ctx);
}

static int dnvme_set_dma_mask(struct pci_dev *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;

	if (dma_supported(&pdev->dev, DMA_BIT_MASK(64)) == 0) {
		dev_err(dev, "The device unable to address 64 bits of DMA\n");
		return -EPERM;
	}

	ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(64));
	if (ret < 0) {
		dev_err(dev, "Request 64-bit DMA has been rejected!\n");
		return ret;
	}

	ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(64));
	if (ret < 0) {
		dev_err(dev, "Request 64-bit coherent memory has been rejected!\n");
		return ret;
	}

	return ret;
}

static int dnvme_pci_enable(struct nvme_context *ctx)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_ctrl_property *prop = &ndev->prop;
	struct pci_dev *pdev = ndev->pdev;
	void __iomem *bar0 = ndev->bar0;
	int ret;

	pci_set_master(pdev);

	ret = pci_enable_device(pdev);
	if (ret < 0) {
		dnvme_err(ndev, "failed to enable pci device!(%d)\n", ret);
		return ret;
	}

	prop->cap = dnvme_readq(bar0, NVME_REG_CAP);
	ndev->q_depth = NVME_CAP_MQES(prop->cap) + 1;
	ndev->db_stride = 1 << NVME_CAP_STRIDE(prop->cap);

	dnvme_dbg(ndev, "db_stride:%u, q_depth:%u\n", ndev->db_stride, ndev->q_depth);
	return 0;
}

static void dnvme_pci_disable(struct nvme_context *ctx)
{
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->pdev;

	if (pci_is_enabled(pdev))
		pci_disable_device(pdev);
}

/**
 * @brief Abstract important info from the capability of NVMe device.
 * 
 * @param ndev NVMe device
 * @return 0 on success, otherwise a negative errno.
 */
static int dnvme_init_capability(struct nvme_device *ndev)
{
	struct pci_dev *pdev = ndev->pdev;
	struct nvme_capability *cap = &ndev->cap;

	cap->msi = pci_get_msi_cap(pdev, 
		pci_find_capability(pdev, PCI_CAP_ID_MSI));
	cap->msix = pci_get_msix_cap(pdev,
		pci_find_capability(pdev, PCI_CAP_ID_MSIX));
	cap->pm = pci_get_pm_cap(pdev,
		pci_find_capability(pdev, PCI_CAP_ID_PM));
	cap->express = pci_get_express_cap(pdev,
		pci_find_capability(pdev, PCI_CAP_ID_EXP));

	return 0;
}

static void dnvme_deinit_capability(struct nvme_device *ndev)
{
	struct nvme_capability *cap = &ndev->cap;

	if (cap->msi) {
		kfree(cap->msi);
		cap->msi = NULL;
	}
	if (cap->msix) {
		kfree(cap->msix);
		cap->msix = NULL;
	}
	if (cap->pm) {
		kfree(cap->pm);
		cap->pm = NULL;
	}
	if (cap->express) {
		kfree(cap->express);
		cap->express = NULL;
	}
}

static int dnvme_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct nvme_context *ctx;

	dev_info(dev, "probe pdev...(cpu:%d %d %d)\n", num_possible_cpus(), 
		num_present_cpus(), num_active_cpus());

	ret = dnvme_set_dma_mask(pdev);
	if (ret < 0)
		return ret;

	ctx = dnvme_alloc_context(pdev);
	if (!ctx) {
		dev_err(dev, "failed to alloc context!\n");
		return -ENOMEM;
	}

	ret = dnvme_map_resource(ctx);
	if (ret < 0)
		goto out_release_ctx;

	ret = dnvme_pci_enable(ctx);
	if (ret < 0)
		goto out_unmap_res;

	ret = dnvme_init_capability(ctx->dev);
	if (ret < 0)
		goto out_disable_pci;

	ret = dnvme_map_cmb(ctx->dev);
	if (ret < 0)
		goto out_deinit_cap;

	dnvme_create_proc_entry(ctx->dev, nvme_proc_dir);

	/* Finalize this device and prepare for next one */
	dev_info(dev, "NVMe device(0x%x:0x%x) init ok!\n", pdev->vendor, pdev->device);
	dev_vdbg(dev, "NVMe bus #%d, dev slot: %d", pdev->bus->number, 
		PCI_SLOT(pdev->devfn));
	dev_vdbg(dev, "NVMe func: 0x%x, class: 0x%x", PCI_FUNC(pdev->devfn),
		pdev->class);

	mutex_lock(&nvme_ctx_list_lock);
	list_add_tail(&ctx->entry, &nvme_ctx_list);
	mutex_unlock(&nvme_ctx_list_lock);
	return 0;

out_deinit_cap:
	dnvme_deinit_capability(ctx->dev);
out_disable_pci:
	dnvme_pci_disable(ctx);
out_unmap_res:
	dnvme_unmap_resource(ctx);
out_release_ctx:
	dnvme_release_context(ctx);
	return ret;
}

static void dnvme_remove(struct pci_dev *pdev)
{
	bool found = false;
	struct device *dev = &pdev->dev;
	struct nvme_context *ctx;

	dev_info(dev, "Remove NVMe device(0x%x:0x%x) ...\n", pdev->vendor, pdev->device);
	dev_vdbg(dev, "NVMe bus #%d, dev slot: %d", pdev->bus->number, 
		PCI_SLOT(pdev->devfn));
	dev_vdbg(dev, "NVMe func: 0x%x, class: 0x%x", PCI_FUNC(pdev->devfn),
		pdev->class);

	list_for_each_entry(ctx, &nvme_ctx_list, entry) {
		if (pdev == ctx->dev->pdev) {
			found = true;
			break;
		}
	}

	if (!found) {
		dev_warn(dev, "Cannot found dev in list!\n");
		return;
	}

	mutex_lock(&nvme_ctx_list_lock);
	list_del(&ctx->entry);
	mutex_unlock(&nvme_ctx_list_lock);

	if (mutex_is_locked(&ctx->lock)) {
		dev_warn(dev, "nvme context lock is held by user?\n");
	}
	dnvme_destroy_proc_entry(ctx->dev);

	dnvme_cleanup_context(ctx, NVME_ST_DISABLE_COMPLETE);
	dnvme_unmap_cmb(ctx->dev);
	dnvme_deinit_capability(ctx->dev);
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
	.name		= "dnvme",
	.id_table	= dnvme_id_table,
	.probe		= dnvme_probe,
	.remove		= dnvme_remove,
};

static int __init dnvme_init(void)
{
	int ret;

	nvme_drv.api_version = API_VERSION;
	nvme_drv.drv_version = DRIVER_VERSION;

	ret = alloc_chrdev_region(&nvme_chr_devt, 0, NVME_MINORS, "nvme");
	if (ret < 0) {
		pr_err("failed to alloc chrdev!(%d)\n", ret);
		return ret;
	}

	nvme_class = class_create(THIS_MODULE, "nvme");
	if (IS_ERR(nvme_class)) {
		ret = PTR_ERR(nvme_class);
		pr_err("failed to create class!(%d)\n", ret);
		goto out;
	}

	nvme_proc_dir = proc_mkdir("nvme", NULL);
	if (!nvme_proc_dir)
		pr_warn("failed to create proc dir!\n");

	ret = pci_register_driver(&dnvme_driver);
	if (ret < 0) {
		pr_err("failed to register pci driver!(%d)\n", ret);
		goto out2;
	}

	pr_info("init ok!(api_ver:%x, drv_ver:%x)\n", nvme_drv.api_version, 
		nvme_drv.drv_version);
	return 0;

out2:
	class_destroy(nvme_class);
out:
	unregister_chrdev_region(nvme_chr_devt, NVME_MINORS);
	return ret;
}
module_init(dnvme_init);

static void __exit dnvme_exit(void)
{
	pci_unregister_driver(&dnvme_driver);
	proc_remove(nvme_proc_dir);
	class_destroy(nvme_class);
	unregister_chrdev_region(nvme_chr_devt, NVME_MINORS);
	ida_destroy(&nvme_instance_ida);
	pr_info("exit ok!(api_ver:%x, drv_ver:%x)\n", nvme_drv.api_version, 
		nvme_drv.drv_version);
}
module_exit(dnvme_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("nvmecompliance@intel.com");
MODULE_DESCRIPTION("NVMe compliance suite kernel driver");
MODULE_VERSION(DRIVER_VERSION_STR(DRIVER_VERSION));

