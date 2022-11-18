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

#include "dnvme_interface.h"
#include "definitions.h"
#include "sysfuncproto.h"
#include "dnvme_reg.h"
#include "core.h"
#include "dnvme_ioctl.h"
#include "dnvme_queue.h"
#include "dnvme_ds.h"
#include "dnvme_cmds.h"
#include "dnvme_irq.h"
#include "dnvme_cmb.h"

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
static struct class *nvme_class;
static struct nvme_driver nvme_drv;


/**
 * @brief Find nvme_context from linked list. 
 *  
 * @return pointer to the nvme_context if found, otherwise errno. 
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
 * @return pointer to the nvme_context if locked, otherwise errno. 
 */
static struct nvme_context *lock_context(struct inode *inode)
{
	struct nvme_context *ctx;

	ctx = find_context(inode);
	if (IS_ERR(ctx)) {
		pr_err("Cannot find the device with minor no. %d\n", iminor(inode));
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
		pr_warn("already unlocked, lock missmatch!\n");
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
			pr_err("CQ ID(%u) is out of range!\n", id);
			return -EINVAL;
		}

		cq = dnvme_find_cq(ctx, id);
		if (!cq) {
			pr_err("Cannot find CQ(%u)!\n", id);
			return -EBADSLT;
		}

		if (cq->priv.contig == 0) {
			pr_err("Cannot mmap non-contig CQ!\n");
			return -ENOTSUP;
		}
		*kva = cq->priv.buf;
		*size = cq->priv.size;
		break;

	case VMPGOFF_TYPE_SQ:
		if (id > NVME_SQ_ID_MAX) {
			pr_err("SQ ID(%u) is out of range!\n", id);
			return -EINVAL;
		}

		sq = dnvme_find_sq(ctx, id);
		if (!sq) {
			pr_err("Cannot find SQ(%u)!\n", id);
			return -EBADSLT;
		}

		if (sq->priv.contig == 0) {
			pr_err("Cannot mmap non-contig SQ!\n");
			return -ENOTSUP;
		}
		*kva = sq->priv.buf;
		*size = sq->priv.size;
		break;

	case VMPGOFF_TYPE_META:
		if (id > NVME_META_ID_MAX) {
			pr_err("Meta ID(%u) is out of range!\n", id);
			return -EINVAL;
		}

		meta = dnvme_find_meta(ctx, id);
		if (!meta) {
			pr_err("Cannot find Meta(%u)!\n", id);
			return -EBADSLT;
		}
		*kva = meta->buf;
		*size = ctx->meta_set.buf_size;
		break;

	default:
		pr_err("type(%u) is unknown!\n", type);
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
		pr_err("Request to Map more than allocated pages...\n");
		ret = -EINVAL;
		goto out;
	}
	pr_debug("K.V.A = 0x%lx, PAGES = %d\n", (unsigned long)mmap_addr, npages);

	/* Associated struct page ptr for kernel logical address */
	pfn = virt_to_phys(mmap_addr) >> PAGE_SHIFT;
	if (!pfn) {
		pr_err("virt_to_phys err!\n");
		ret = -EFAULT;
		goto out;
	}
	pr_debug("PFN = 0x%lx", pfn);

	ret = remap_pfn_range(vma, vma->vm_start, pfn, 
		vma->vm_end - vma->vm_start, vma->vm_page_prot);
	if (ret < 0)
		pr_err("remap_pfn_rage err!(%d)\n", ret);

out:
	unlock_context(ctx);
	return ret;
}

static long dnvme_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = -EINVAL;
	struct nvme_context *ctx;
	struct nvme_create_admn_q *create_admn_q;
	struct inode *inode = inode = filp->f_path.dentry->d_inode;
	void __user *argp = (void __user *)arg;

	pr_debug("cmd num:%u, arg:0x%lx", _IOC_NR(cmd), arg);
	ctx = lock_context(inode);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	switch (cmd) {
	case NVME_IOCTL_ERR_CHK:
		err = device_status_chk(ctx, (struct device_status *)arg);
		break;

	case NVME_IOCTL_READ_GENERIC:
		err = dnvme_generic_read(ctx, argp);
		break;

	case NVME_IOCTL_WRITE_GENERIC:
		err = dnvme_generic_write(ctx, argp);
		break;

	case NVME_IOCTL_CREATE_ADMN_Q:
		pr_debug("NVME_IOCTL_CREATE_ADMN_Q");
		/* Allocating memory for user struct in kernel space */
		create_admn_q = kmalloc(sizeof(struct nvme_create_admn_q), GFP_KERNEL);
		if (create_admn_q == NULL) {
		pr_err("Unable to alloc kernel memory to copy user data");
		err = -ENOMEM;
		break;
		}
		if (copy_from_user(create_admn_q, (void *)arg,
		sizeof(struct nvme_create_admn_q))) {

		pr_err("Unable to copy from user space");
		kfree(create_admn_q);
		err = -EFAULT;
		break;
		}

		if (create_admn_q->type == ADMIN_SQ) {
		pr_debug("Create ASQ");
		err = driver_create_asq(create_admn_q, ctx);
		} else if (create_admn_q->type == ADMIN_CQ) {
		pr_debug("Create ACQ");
		err = driver_create_acq(create_admn_q, ctx);
		} else {
		pr_err("Unknown Q type specified");
		err = -EINVAL;
		}
		kfree(create_admn_q);
		break;

	case NVME_IOCTL_DEVICE_STATE:
		pr_debug("NVME_IOCTL_DEVICE_STATE");
		switch ((enum nvme_state)arg) {
		case NVME_ST_ENABLE:
		pr_debug("Enabling the DUT");
		err = nvme_ctrl_set_state(ctx, 1);
		break;
		case NVME_ST_ENABLE_IOL_TO:
		pr_debug("Enabling the DUT");
		err = iol_nvme_ctrl_set_state(ctx, 1);
		break;
		case NVME_ST_DISABLE_IOL_TO:
		pr_debug("Disabling the DUT");
		if ((err = iol_nvme_ctrl_set_state(ctx, 0)) == 0) {
			device_cleanup(ctx, NVME_ST_DISABLE);
		}
		break;
		case NVME_ST_DISABLE:
		case NVME_ST_DISABLE_COMPLETE:
		pr_debug("Disabling the DUT");
		if ((err = nvme_ctrl_set_state(ctx, 0)) == 0) {
			device_cleanup(ctx, (enum nvme_state)arg);
		}
		break;
		case NVME_ST_NVM_SUBSYSTEM:
		pr_debug("Performing NVM Subsystem reset");
		err = nvme_nvm_subsystem_reset(ctx);
		break;
		default:
		pr_err("Unknown IOCTL parameter");
		err = -EINVAL;
		break;
		}
		break;

	case NVME_IOCTL_GET_Q_METRICS:
		pr_debug("NVME_IOCTL_GET_Q_METRICS");
		err = get_public_qmetrics(ctx,
		(struct nvme_get_q_metrics *)arg);
		break;

	case NVME_IOCTL_PREPARE_SQ_CREATION:
		pr_debug("NVME_IOCTL_PREPARE_SQ_CREATION");
		err = driver_nvme_prep_sq((struct nvme_prep_sq *)arg,
		ctx);
		break;

	case NVME_IOCTL_PREPARE_CQ_CREATION:
		pr_debug("NVME_IOCTL_PREPARE_CQ_CREATION");
		err = driver_nvme_prep_cq((struct nvme_prep_cq *)arg,
		ctx);
		break;

	case NVME_IOCTL_RING_SQ_DOORBELL:
		pr_debug("NVME_IOCTL_RING_SQ_DOORBELL");
		err = nvme_ring_sqx_dbl((u16)arg, ctx);
		break;

	case NVME_IOCTL_SEND_64B_CMD:
		pr_debug("NVME_IOCTL_SEND_64B_CMD");
		err = driver_send_64b(ctx,
		(struct nvme_64b_send *)arg);
		break;

	case NVME_IOCTL_TOXIC_64B_DWORD:
		pr_debug("NVME_TOXIC_64B_DWORD");
		err = driver_toxic_dword(ctx,
		(struct backdoor_inject *)arg);
		break;

	case NVME_IOCTL_DUMP_METRICS:
		pr_debug("NVME_IOCTL_DUMP_METRICS");
		err = driver_log((struct nvme_file *)arg);
		break;

	case NVME_IOCTL_REAP_INQUIRY:
		pr_debug("NVME_IOCTL_REAP_INQUIRY");
		err = driver_reap_inquiry(ctx,
		(struct nvme_reap_inquiry *)arg);
		break;

	case NVME_IOCTL_REAP:
		pr_debug("NVME_IOCTL_REAP");
		err = driver_reap_cq(ctx, (struct nvme_reap *)arg);
		break;

	case NVME_IOCTL_GET_DRIVER_METRICS:
		pr_debug("NVME_IOCTL_GET_DRIVER_METRICS");
		if (copy_to_user((struct nvme_driver *)arg,
		&nvme_drv, sizeof(struct nvme_driver))) {

		pr_err("Unable to copy to user space");
		err = -EFAULT;
		} else {
		err = 0;
		}
		break;

	case NVME_IOCTL_METABUF_CREATE:
		pr_debug("NVME_IOCTL_METABUF_CREATE");
		if (arg > MAX_METABUFF_SIZE) {
		pr_err("Meta buffer size exceeds max(0x%08X) > 0x%08X",
			MAX_METABUFF_SIZE, (u32)arg);
		err = -EINVAL;
		} else {
		err = metabuff_create(ctx, (u32)arg);
		}
		break;

	case NVME_IOCTL_METABUF_ALLOC:
		pr_debug("NVME_IOCTL_METABUF_ALLOC");
		err = metabuff_alloc(ctx, (u32)arg);
		break;

	case NVME_IOCTL_METABUF_DELETE:
		pr_debug("NVME_IOCTL_METABUF_DELETE");
		err = metabuff_del(ctx, (u32)arg);
		break;

	case NVME_IOCTL_SET_IRQ:
		pr_debug("NVME_IOCTL_SET_IRQ");
		err = nvme_set_irq(ctx, (struct interrupts *)arg);
		break;

	case NVME_IOCTL_MASK_IRQ:
		pr_debug("NVME_IOCTL_MASK_IRQ");
		err = nvme_mask_irq(ctx, (u16)arg);
		break;

	case NVME_IOCTL_UNMASK_IRQ:
		pr_debug("NVME_IOCTL_UNMASK_IRQ");
		err = nvme_unmask_irq(ctx, (u16)arg);
		break;

	case NVME_IOCTL_GET_DEVICE_METRICS:
		pr_debug("NVME_IOCTL_GET_DEVICE_METRICS");
		if (copy_to_user((struct nvme_dev_public *)arg,
		&ctx->dev->pub,
		sizeof(struct nvme_dev_public))) {

		pr_err("Unable to copy to user space");
		err = -EFAULT;
		} else {
		err = 0;
		}
		break;

	case NVME_IOCTL_MARK_SYSLOG:
		pr_debug("NVME_IOCTL_MARK_SYSLOG");
		err = driver_logstr((struct nvme_logstr *)arg);
		break;

	//***************************boot partition test MengYu***************************
	//   case NVME_IOCTL_WRITE_BP_BUF:
	//     pr_debug("NVME_IOCTL_WRITE_BP_BUF");
	//     err = driver_nvme_write_bp_buf((struct nvme_write_bp_buf *)arg, ctx);
	//     break;  
	//   case NVME_IOCTL_GET_BP_MEM_ADDR:
	//     pr_debug("NVME_IOCTL_GET_BP_MEM_ADDR");
	//     break;  
	//***************************boot partition test MengYu***************************
	default:
		pr_err("Unknown IOCTL");
		break;
	}

	unlock_context(ctx);
	return err;
}

static int dnvme_open(struct inode *inode, struct file *filp)
{
	struct nvme_context *ctx;
	int ret = 0;

	ctx = lock_context(inode);
	if (IS_ERR(ctx)) {
		pr_err("Cannot lock on this device with minor no. %d\n", iminor(inode));
		ret = PTR_ERR(ctx);
		goto out;
	}

	if (ctx->dev->priv.opened) {
		pr_err("It's not allowed to open device more than once!\n");
		ret = -EPERM;
		goto out;
	}

	ctx->dev->priv.opened = 1;
	device_cleanup(ctx, NVME_ST_DISABLE_COMPLETE);
	pr_info("Open NVMe device ok!\n");
out:
	unlock_context(ctx);
	return ret;
}

static int dnvme_release(struct inode *inode, struct file *filp)
{
	struct nvme_context *ctx;
	int ret = 0;

	ctx = lock_context(inode);
	if (IS_ERR(ctx)) {
		pr_err("Cannot lock on this device with minor no. %d\n", iminor(inode));
		ret = PTR_ERR(ctx);
		goto out;
	}

	ctx->dev->priv.opened = 0;
	device_cleanup(ctx, NVME_ST_DISABLE_COMPLETE);
	pr_info("NVMe device is closed!\n");
out:
	unlock_context(ctx);
	return ret;
}

static const struct file_operations dnvme_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= dnvme_ioctl,
	.open		= dnvme_open,
	.release	= dnvme_release,
	.mmap		= dnvme_mmap,
};

static int dnvme_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int err = -EINVAL;
	void __iomem *bar0 = NULL;
	void __iomem *bar1 = NULL;
	void __iomem *bar2 = NULL;
	static int nvme_minor = 0;
	dev_t devno = MKDEV(nvme_major, nvme_minor);
	struct nvme_context *pmetrics_device = NULL;
	int bars = 0;

	pr_info("probe pdev...(cpu:%d %d %d)\n", num_possible_cpus(), 
		num_present_cpus(), num_active_cpus());

	/* Allocate kernel memory for our own internal tracking of this device */
	pmetrics_device = kzalloc(sizeof(struct nvme_context), GFP_KERNEL);
	if (!pmetrics_device) {
		pr_err("failed to alloc nvme_context!\n");
		return -ENOMEM;
	}

	/* !TODO: Replace by @pci_request_mem_regions */

	/* Get the bitmask value of the BAR's supported by device */
	bars = pci_select_bars(pdev, IORESOURCE_MEM);

	/* Map BAR0 & BAR1 (BAR0 for 64-bit); ctrlr register memory mapped */
	if (request_mem_region(pci_resource_start(pdev, BAR0_BAR1),
		pci_resource_len(pdev, BAR0_BAR1), DRIVER_NAME) == NULL) {
		pr_err("BAR0 memory already in use");
		goto fail_out;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,8,0) //2021.1.22 meng_yu
	bar2 = ioremap(pci_resource_start(pdev, BAR4_BAR5),
		pci_resource_len(pdev, BAR4_BAR5));
#else
	bar2 = ioremap_nocache(pci_resource_start(pdev, BAR4_BAR5),
		pci_resource_len(pdev, BAR4_BAR5));
#endif

    bar0 = ioremap(pci_resource_start(pdev, BAR0_BAR1),
        pci_resource_len(pdev, BAR0_BAR1));
    if (bar0 == NULL) {
        pr_err("Mapping BAR0 mem map'd registers failed");
        goto remap_fail_out;
    }

    /* Map BAR2 & BAR3 (BAR1 for 64-bit); I/O mapped registers  */
    if (bars & (1 << BAR2_BAR3)) {
        if (request_mem_region(pci_resource_start(pdev, BAR2_BAR3),
            pci_resource_len(pdev, BAR2_BAR3), DRIVER_NAME) == NULL) {
            pr_err("BAR1 (64 bit) memory already in use");
            goto remap_fail_out;
        }

        bar1 = pci_iomap(pdev, BAR2_BAR3, pci_resource_len(pdev, BAR2_BAR3));
        if (bar1 == NULL) {
            pr_err("Mapping BAR1 (64 bit) mem map'd registers failed");
            goto remap_fail_out;
        }
    } else {
        pr_info("BAR1 (64 bit) not supported by DUT");
    }


    /* Map BAR4 & BAR5 (BAR2 for 64-bit); MSIX table memory mapped */
    if (bars & (1 << BAR4_BAR5)) {
        if (request_mem_region(pci_resource_start(pdev, BAR4_BAR5),
            pci_resource_len(pdev, BAR4_BAR5), DRIVER_NAME) == NULL) {
            pr_err("BAR2 (64 bit) memory already in use");
            goto remap_fail_out;
        }

        #if LINUX_VERSION_CODE >= KERNEL_VERSION(5,8,0) //2021.1.22 meng_yu
        bar2 = ioremap(pci_resource_start(pdev, BAR4_BAR5),
            pci_resource_len(pdev, BAR4_BAR5));
        
        #else
        bar2 = ioremap_nocache(pci_resource_start(pdev, BAR4_BAR5),
            pci_resource_len(pdev, BAR4_BAR5));
        #endif

        bar2 = ioremap(pci_resource_start(pdev, BAR4_BAR5),
            pci_resource_len(pdev, BAR4_BAR5));
        if (bar2 == NULL) {
            pr_err("Mapping BAR2 (64 bit) mem map'd registers failed");
            goto remap_fail_out;
        }
    } else {
        pr_info("BAR2 (64 bit) not supported by DUT");
    }


    pci_set_master(pdev);
    if (dma_supported(&pdev->dev, DMA_BIT_MASK(64)) == 0) {
        pr_err("The device unable to address 64 bits of DMA");
        goto remap_fail_out;
    }
    else if ((err = dma_set_mask(&pdev->dev, DMA_BIT_MASK(64)))) {
        pr_err("Requesting 64 bit DMA has been rejected");
        goto remap_fail_out;
    }
    else if ((err = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(64)))) {
        pr_err("Requesting 64 bit coherent memory has been rejected");
        goto remap_fail_out;
    }

    err = driver_ioctl_init(pdev, bar0, bar1, bar2, pmetrics_device);
    if (err < 0) {
        pr_err("Failed to init dnvme's internal state metrics");
        goto remap_fail_out;
    }

    mutex_init(&pmetrics_device->lock);
    pmetrics_device->dev->priv.opened = 0;
    pmetrics_device->dev->priv.minor = nvme_minor;

    /* Create an NVMe special device */
    pmetrics_device->dev->priv.spcl_dev = device_create(
        nvme_class, NULL, devno, NULL, DEVICE_NAME"%d", nvme_minor);
    if (IS_ERR(pmetrics_device->dev->priv.spcl_dev)) {
        err = PTR_ERR(pmetrics_device->dev->priv.spcl_dev);
        pr_err("Creation of special device file failed: %d", err);
        goto remap_fail_out;
    }

    err = pci_enable_device(pdev);
    if (err < 0) {
        pr_err("Enabling the PCIe device has failed: 0x%04X", err);
        goto spcp_fail_out;
    }
    // if(pci_is_enabled(pdev))
    // {
    //     __u32 csts = readl(&pmetrics_device->dev->priv.ctrlr_regs->csts);
    //     pr_err("csts: 0x%x", csts);
    // }
    // else
    // {
    //     pr_err("pcie is disable");
    // }

    nvme_map_cmb(pmetrics_device->dev);

    /* Finalize this device and prepare for next one */
    pr_debug("NVMe dev: 0x%x, vendor: 0x%x", pdev->device, pdev->vendor);
    pr_debug("NVMe bus #%d, dev slot: %d", pdev->bus->number,
        PCI_SLOT(pdev->devfn));
    pr_debug("NVMe func: 0x%x, class: 0x%x", PCI_FUNC(pdev->devfn),
        pdev->class);
    list_add_tail(&pmetrics_device->entry, &nvme_ctx_list);
    nvme_minor++;
    return 0;


spcp_fail_out:
    device_del(pmetrics_device->dev->priv.spcl_dev);
remap_fail_out:
    if (bar0 != NULL) {
        iounmap(bar0);
        release_mem_region(pci_resource_start(pdev, BAR0_BAR1),
            pci_resource_len(pdev, BAR0_BAR1));
    }
    if (bar1 != NULL) {
        iounmap(bar1);
        release_mem_region(pci_resource_start(pdev, BAR2_BAR3),
            pci_resource_len(pdev, BAR2_BAR3));
    }
    if (bar2 != NULL) {
        iounmap(bar2);
        release_mem_region(pci_resource_start(pdev, BAR4_BAR5),
            pci_resource_len(pdev, BAR4_BAR5));
    }
fail_out:
    if (pmetrics_device != NULL) {
        kfree(pmetrics_device);
    }
    return err;
}


static void dnvme_remove(struct pci_dev *dev)
{
    struct pci_dev *pdev;
    struct nvme_context *pmetrics_device;


    /* Loop through the devices available in the metrics list */
    list_for_each_entry(pmetrics_device, &nvme_ctx_list, entry) {

        pdev = pmetrics_device->dev->priv.pdev;
        if (pdev == dev) {

            pr_debug("Removing device: 0x%x, vendor: 0x%x",
                pdev->device, pdev->vendor);
            pr_debug("PCIe bus #%d, slot: %d", pdev->bus->number,
                PCI_SLOT(pdev->devfn));
            pr_debug("PCIe func: 0x%x, class: 0x%x", PCI_FUNC(pdev->devfn),
                pdev->class);

            nvme_release_cmb(pmetrics_device->dev);
            /* Wait for any other dnvme access to finish, then stop further
             * before we free resources to prevent circular issues */
            mutex_lock(&pmetrics_device->lock);
            device_cleanup(pmetrics_device, NVME_ST_DISABLE_COMPLETE);
            pci_disable_device(pdev);

            /* Release the selected memory regions that were reserved */
            if (pmetrics_device->dev->priv.bar0 != NULL) {
                destroy_dma_pool(pmetrics_device->dev);
                iounmap(pmetrics_device->dev->priv.bar0);
                release_mem_region(pci_resource_start(pdev, BAR0_BAR1),
                    pci_resource_len(pdev, BAR0_BAR1));
            }
            if (pmetrics_device->dev->priv.bar1 != NULL) {
                iounmap(pmetrics_device->dev->priv.bar1);
                release_mem_region(pci_resource_start(pdev, BAR2_BAR3),
                    pci_resource_len(pdev, BAR2_BAR3));
            }
            if (pmetrics_device->dev->priv.bar2 != NULL) {
                iounmap(pmetrics_device->dev->priv.bar2);
                release_mem_region(pci_resource_start(pdev, BAR4_BAR5),
                    pci_resource_len(pdev, BAR4_BAR5));
            }

            /* Free up the linked list */
            list_del(&pmetrics_device->cq_list);
            list_del(&pmetrics_device->sq_list);

            /* Unlock, then destroy all mutexes */
            mutex_unlock(&pmetrics_device->lock);
            mutex_destroy(&pmetrics_device->lock);
            mutex_destroy(&pmetrics_device->irq_process.irq_track_mtx);

            device_del(pmetrics_device->dev->priv.spcl_dev);
        }
    }

    /* Free up the device linked list if there are not items left */
    if (list_empty(&nvme_ctx_list)) {
        list_del(&nvme_ctx_list);
    }
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
		pr_err("failed to register chrdev!(%d)\n", nvme_major);
		return nvme_major;
	}

	nvme_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(nvme_class)) {
		ret = PTR_ERR(nvme_class);
		pr_err("failed to create %s class!(%d)\n", DEVICE_NAME, ret);
		goto out;
	}

	ret = pci_register_driver(&dnvme_driver);
	if (ret < 0) {
		pr_err("failed to register pci driver!(%d)\n", ret);
		goto out2;
	}

	pr_info("init ok!(api_ver:%x, drv_ver:%x)\n", nvme_drv.api_version, 
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
	pr_info("exit ok!(api_ver:%x, drv_ver:%x)\n", nvme_drv.api_version, 
		nvme_drv.driver_version);
}
module_exit(dnvme_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:"DRIVER_NAME);
MODULE_AUTHOR("nvmecompliance@intel.com");
MODULE_DESCRIPTION("NVMe compliance suite kernel driver");
MODULE_VERSION(DRIVER_VERSION_STR(DRIVER_VERSION));
