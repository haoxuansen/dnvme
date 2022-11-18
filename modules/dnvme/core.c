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
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/errno.h>
#include <linux/mman.h>
#include <linux/dma-mapping.h>
#include <linux/version.h>

#include "dnvme_interface.h"
#include "definitions.h"
#include "sysfuncproto.h"
#include "dnvme_reg.h"
#include "core.h"
#include "dnvme_ioctls.h"
#include "dnvme_queue.h"
#include "dnvme_ds.h"
#include "dnvme_cmds.h"
#include "dnvme_irq.h"
#include "dnvme_cmb.h"

#define DEVICE_NAME			"nvme"
#define DRIVER_NAME			"dnvme"

#define API_VERSION                     0xfff10403 /* IOL - 1.4.3 */
#define DRIVER_VERSION			0x00000001
#define DRIVER_VERSION_STR(VER)		#VER

#define BAR0_BAR1               0x0
#define BAR2_BAR3               0x2
#define BAR4_BAR5               0x4

#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

/* Module globals */
static int dnvme_major;
LIST_HEAD(metrics_dev_ll);
static struct class *dnvme_class;
struct metrics_driver g_metrics_drv;

/*
 * find device from the device linked list. Returns pointer to the
 * device if found otherwise returns NULL.
 */
static struct metrics_device_list *find_device(struct inode *inode)
{
	int dev_found = 1;
	struct metrics_device_list *pmetrics_device;

	/* Loop through the devices available in the metrics list */
	list_for_each_entry(pmetrics_device, &metrics_dev_ll, metrics_device_hd) {

		if (iminor(inode) == pmetrics_device->metrics_device->
			private_dev.minor_no) {

			return pmetrics_device;
		} else {
			dev_found = 0;
		}
	}

	/* The specified device could not be found in the list */
	if (dev_found == 0) {
		pr_err("Cannot find the device with minor no. %d", iminor(inode));
		return NULL;
	}
	return NULL;
}


/*
 * lock_device function will call find_device and if found device locks my
 * taking the mutex. This function returns a pointer to successfully found
 * device.
 */
static struct metrics_device_list *lock_device(struct inode *inode)
{
    struct metrics_device_list *pmetrics_device;
    pmetrics_device = find_device(inode);
    if (pmetrics_device == NULL) {
        pr_err("Cannot find the device with minor no. %d", iminor(inode));
        return NULL;
    }

    /* Grab the Mutex for this device in the linked list */
    mutex_lock(&pmetrics_device->metrics_mtx);
    return pmetrics_device;
}


static void unlock_device(struct  metrics_device_list *pmetrics_device)
{
    if (mutex_is_locked(&pmetrics_device->metrics_mtx)) {
        mutex_unlock(&pmetrics_device->metrics_mtx);
    }
}

/*
 * dnvme_mmap - This function maps the contiguous device mapped area
 * to user space. This is specfic to device which is called though fd.
 */
static int dnvme_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct  metrics_device_list *pmetrics_device; /* Metrics device */
    struct  metrics_sq  *pmetrics_sq_list;  /* SQ linked list               */
    struct  metrics_cq  *pmetrics_cq_list;  /* CQ linked list               */
    struct  metrics_meta *pmeta_data;       /* pointer to meta node         */
    u8 *vir_kern_addr;
    unsigned long pfn = 0;
    struct inode *inode = filp->f_path.dentry->d_inode;
    u32 type;
    u32 id;
    u32 mmap_range;
    int npages;
    int err = 0;

    pr_debug("Device Calling mmap function...");

    pmetrics_device = lock_device(inode);
    if (pmetrics_device == NULL) {
        pr_err("Cannot lock on this device with minor no. %d", iminor(inode));
        err = -ENODEV;
        goto mmap_exit;
    }

    vma->vm_flags |= (VM_IO | VM_RESERVED);

    /* Calculate the id and type from offset */
    type = (vma->vm_pgoff >> 0x12) & 0x3;
    id = vma->vm_pgoff & 0x3FFFF;

    pr_debug("Type = %d", type);
    pr_debug("ID = 0x%x", id);

    /* If type is 1 implies SQ, 0 implies CQ and 2 implies meta data */
    if (type == 0x1) {
        /* Process for SQ */
        if (id > U16_MAX) { /* 16 bits */
            pr_err("SQ Id is greater than 16 bits..");
            err = -EINVAL;
            goto mmap_exit;
        }
        /* Find SQ node in the list with id */
        pmetrics_sq_list = find_sq(pmetrics_device, id);
        if (pmetrics_sq_list == NULL) {
            err = -EBADSLT;
            goto mmap_exit;
        }
        if (pmetrics_sq_list->private_sq.contig == 0) {
            pr_err("MMAP does not work on non contig SQ's");
            #ifndef ENOTSUP
                err = -EOPNOTSUPP; /* aka ENOTSUP in userland for POSIX */
            #else                      /*  parisc does define it separately.*/
                err = -ENOTSUP;
            #endif
            goto mmap_exit;
        }
        vir_kern_addr = pmetrics_sq_list->private_sq.vir_kern_addr;
        mmap_range = pmetrics_sq_list->private_sq.size;
    } else if (type == 0x0) {
        /* Process for CQ */
        if (id > U16_MAX) { /* 16 bits */
            pr_err("CQ Id is greater than 16 bits..");
            err = -EINVAL;
            goto mmap_exit;
        }
        /* Find CQ node in the list with id */
        pmetrics_cq_list = find_cq(pmetrics_device, id);
        if (pmetrics_cq_list == NULL) {
            err = -EBADSLT;
            goto mmap_exit;
        }
        if (pmetrics_cq_list->private_cq.contig == 0) {
            pr_err("MMAP does not work on non contig CQ's");
            #ifndef ENOTSUP
                err = -EOPNOTSUPP; /* aka ENOTSUP in userland for POSIX */
            #else                      /*  parisc does define it separately.*/
                err = -ENOTSUP;
            #endif
            goto mmap_exit;
        }
        vir_kern_addr = pmetrics_cq_list->private_cq.vir_kern_addr;
        mmap_range = pmetrics_cq_list->private_cq.size;
    } else if (type == 0x2) {
        /* Process for Meta data */
        if (id > (2^18)) { /* 18 bits */
            pr_err("Meta Id is greater than 18 bits..");
            err = -EINVAL;
            goto mmap_exit;
        }
        /* find Meta Node data */
        pmeta_data = find_meta_node(pmetrics_device, id);
        if (pmeta_data == NULL) {
            err = -EBADSLT;
            goto mmap_exit;
        }
        vir_kern_addr = pmeta_data->vir_kern_addr;
        mmap_range = pmetrics_device->metrics_meta.meta_buf_size;
    } else {
        err = -EINVAL;
        goto mmap_exit;
    }

    npages = (mmap_range/PAGE_SIZE) + 1;
    if ((npages * PAGE_SIZE) < (vma->vm_end - vma->vm_start)) {
        pr_err("Request to Map more than allocated pages...");
        err = -EINVAL;
        goto mmap_exit;
    }
    pr_debug("Virt Address = 0x%llx", (u64)vir_kern_addr);
    pr_debug("Npages = %d", npages);

    /* Associated struct page ptr for kernel logical address */
    pfn = virt_to_phys(vir_kern_addr) >> PAGE_SHIFT;
    if (!pfn) {
        pr_err("pfn is NULL");
        err = -EINVAL;
        goto mmap_exit;
    }
    pr_debug("PFN = 0x%lx", pfn);

    /* remap kernel memory to userspace */
    err = remap_pfn_range(vma, vma->vm_start, pfn,
                    vma->vm_end - vma->vm_start, vma->vm_page_prot);

mmap_exit:
    unlock_device(pmetrics_device);
    return err;
}


/*
 * This function is called whenever a process tries to do an ioctl on our
 * device file. We get two extra parameters (additional to the inode and file
 * structures, which all device functions get): the number of the ioctl called
 * and the parameter given to the ioctl function.
 *
 * If the ioctl is write or read/write (meaning output is returned to the
 * calling process), the ioctl call returns the output of this function.
 *
 */
static long dnvme_ioctl(struct file *filp, unsigned int ioctl_num,
    unsigned long ioctl_param)
{
    int err = -EINVAL;
    struct metrics_device_list *pmetrics_device;
    struct nvme_create_admn_q *create_admn_q;
    struct inode *inode = inode = filp->f_path.dentry->d_inode;


    pr_debug("Processing IOCTL 0x%08x", ioctl_num);
    pmetrics_device = lock_device(inode);
    if (pmetrics_device == NULL) {
        pr_err("Unable to lock DUT; minor #%d", iminor(inode));
        err = -ENODEV;
        goto ioctl_exit;
    }

    switch (ioctl_num) {

    case NVME_IOCTL_ERR_CHK:
        pr_debug("NVME_IOCTL_ERR_CHK");
        err = device_status_chk(pmetrics_device, (struct device_status *)ioctl_param);
        break;

    case NVME_IOCTL_READ_GENERIC:
        pr_debug("NVME_IOCTL_READ_GENERIC");
        err = driver_generic_read((struct rw_generic *)ioctl_param,
            pmetrics_device);
        break;

    case NVME_IOCTL_WRITE_GENERIC:
        pr_debug("NVME_IOCTL_WRITE_GENERIC");
        err = driver_generic_write((struct rw_generic *)ioctl_param,
            pmetrics_device);
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
        if (copy_from_user(create_admn_q, (void *)ioctl_param,
            sizeof(struct nvme_create_admn_q))) {

            pr_err("Unable to copy from user space");
            kfree(create_admn_q);
            err = -EFAULT;
            break;
        }

        if (create_admn_q->type == ADMIN_SQ) {
            pr_debug("Create ASQ");
            err = driver_create_asq(create_admn_q, pmetrics_device);
        } else if (create_admn_q->type == ADMIN_CQ) {
            pr_debug("Create ACQ");
            err = driver_create_acq(create_admn_q, pmetrics_device);
        } else {
            pr_err("Unknown Q type specified");
            err = -EINVAL;
        }
        kfree(create_admn_q);
        break;

    case NVME_IOCTL_DEVICE_STATE:
        pr_debug("NVME_IOCTL_DEVICE_STATE");
        switch ((enum nvme_state)ioctl_param) {
        case ST_ENABLE:
            pr_debug("Enabling the DUT");
            err = nvme_ctrl_set_state(pmetrics_device, 1);
            break;
        case ST_ENABLE_IOL_TO:
            pr_debug("Enabling the DUT");
            err = iol_nvme_ctrl_set_state(pmetrics_device, 1);
            break;
        case ST_DISABLE_IOL_TO:
            pr_debug("Disabling the DUT");
            if ((err = iol_nvme_ctrl_set_state(pmetrics_device, 0)) == 0) {
                device_cleanup(pmetrics_device, ST_DISABLE);
            }
            break;
        case ST_DISABLE:
        case ST_DISABLE_COMPLETELY:
            pr_debug("Disabling the DUT");
            if ((err = nvme_ctrl_set_state(pmetrics_device, 0)) == 0) {
                device_cleanup(pmetrics_device, (enum nvme_state)ioctl_param);
            }
            break;
        case ST_NVM_SUBSYSTEM:
            pr_debug("Performing NVM Subsystem reset");
            err = nvme_nvm_subsystem_reset(pmetrics_device);
            break;
         default:
            pr_err("Unknown IOCTL parameter");
            err = -EINVAL;
            break;
        }
        break;

    case NVME_IOCTL_GET_Q_METRICS:
        pr_debug("NVME_IOCTL_GET_Q_METRICS");
        err = get_public_qmetrics(pmetrics_device,
            (struct nvme_get_q_metrics *)ioctl_param);
        break;

    case NVME_IOCTL_PREPARE_SQ_CREATION:
        pr_debug("NVME_IOCTL_PREPARE_SQ_CREATION");
        err = driver_nvme_prep_sq((struct nvme_prep_sq *)ioctl_param,
            pmetrics_device);
        break;

    case NVME_IOCTL_PREPARE_CQ_CREATION:
        pr_debug("NVME_IOCTL_PREPARE_CQ_CREATION");
        err = driver_nvme_prep_cq((struct nvme_prep_cq *)ioctl_param,
            pmetrics_device);
        break;

    case NVME_IOCTL_RING_SQ_DOORBELL:
        pr_debug("NVME_IOCTL_RING_SQ_DOORBELL");
        err = nvme_ring_sqx_dbl((u16)ioctl_param, pmetrics_device);
        break;

    case NVME_IOCTL_SEND_64B_CMD:
        pr_debug("NVME_IOCTL_SEND_64B_CMD");
        err = driver_send_64b(pmetrics_device,
            (struct nvme_64b_send *)ioctl_param);
        break;

    case NVME_IOCTL_TOXIC_64B_DWORD:
        pr_debug("NVME_TOXIC_64B_DWORD");
        err = driver_toxic_dword(pmetrics_device,
            (struct backdoor_inject *)ioctl_param);
        break;

    case NVME_IOCTL_DUMP_METRICS:
        pr_debug("NVME_IOCTL_DUMP_METRICS");
        err = driver_log((struct nvme_file *)ioctl_param);
        break;

    case NVME_IOCTL_REAP_INQUIRY:
        pr_debug("NVME_IOCTL_REAP_INQUIRY");
        err = driver_reap_inquiry(pmetrics_device,
            (struct nvme_reap_inquiry *)ioctl_param);
        break;

    case NVME_IOCTL_REAP:
        pr_debug("NVME_IOCTL_REAP");
        err = driver_reap_cq(pmetrics_device, (struct nvme_reap *)ioctl_param);
        break;

    case NVME_IOCTL_GET_DRIVER_METRICS:
        pr_debug("NVME_IOCTL_GET_DRIVER_METRICS");
        if (copy_to_user((struct metrics_driver *)ioctl_param,
            &g_metrics_drv, sizeof(struct metrics_driver))) {

            pr_err("Unable to copy to user space");
            err = -EFAULT;
        } else {
            err = 0;
        }
        break;

    case NVME_IOCTL_METABUF_CREATE:
        pr_debug("NVME_IOCTL_METABUF_CREATE");
        if (ioctl_param > MAX_METABUFF_SIZE) {
            pr_err("Meta buffer size exceeds max(0x%08X) > 0x%08X",
                MAX_METABUFF_SIZE, (u32)ioctl_param);
            err = -EINVAL;
        } else {
            err = metabuff_create(pmetrics_device, (u32)ioctl_param);
        }
        break;

    case NVME_IOCTL_METABUF_ALLOC:
        pr_debug("NVME_IOCTL_METABUF_ALLOC");
        err = metabuff_alloc(pmetrics_device, (u32)ioctl_param);
        break;

    case NVME_IOCTL_METABUF_DELETE:
        pr_debug("NVME_IOCTL_METABUF_DELETE");
        err = metabuff_del(pmetrics_device, (u32)ioctl_param);
        break;

    case NVME_IOCTL_SET_IRQ:
        pr_debug("NVME_IOCTL_SET_IRQ");
        err = nvme_set_irq(pmetrics_device, (struct interrupts *)ioctl_param);
        break;

    case NVME_IOCTL_MASK_IRQ:
        pr_debug("NVME_IOCTL_MASK_IRQ");
        err = nvme_mask_irq(pmetrics_device, (u16)ioctl_param);
        break;

    case NVME_IOCTL_UNMASK_IRQ:
        pr_debug("NVME_IOCTL_UNMASK_IRQ");
        err = nvme_unmask_irq(pmetrics_device, (u16)ioctl_param);
        break;

    case NVME_IOCTL_GET_DEVICE_METRICS:
        pr_debug("NVME_IOCTL_GET_DEVICE_METRICS");
        if (copy_to_user((struct public_metrics_dev *)ioctl_param,
            &pmetrics_device->metrics_device->public_dev,
            sizeof(struct public_metrics_dev))) {

            pr_err("Unable to copy to user space");
            err = -EFAULT;
        } else {
            err = 0;
        }
        break;

    case NVME_IOCTL_MARK_SYSLOG:
        pr_debug("NVME_IOCTL_MARK_SYSLOG");
        err = driver_logstr((struct nvme_logstr *)ioctl_param);
        break;

    //***************************boot partition test MengYu***************************
    //   case NVME_IOCTL_WRITE_BP_BUF:
    //     pr_debug("NVME_IOCTL_WRITE_BP_BUF");
    //     err = driver_nvme_write_bp_buf((struct nvme_write_bp_buf *)ioctl_param, pmetrics_device);
    //     break;  
    //   case NVME_IOCTL_GET_BP_MEM_ADDR:
    //     pr_debug("NVME_IOCTL_GET_BP_MEM_ADDR");
    //     break;  
    //***************************boot partition test MengYu***************************
    default:
        pr_err("Unknown IOCTL");
        break;
    }

ioctl_exit:
    unlock_device(pmetrics_device);
    return err;
}

/*
 * This operation is always the first operation performed on the device file.
 * when the user call open fd, this is where it lands.
 */
static int dnvme_open(struct inode *inode, struct file *filp)
{
    struct metrics_device_list *pmetrics_device;
    int err = 0;

    pr_err("Opening NVMe device");
    pmetrics_device = lock_device(inode);
    if (pmetrics_device == NULL) {
        pr_err("Cannot lock on this device with minor no. %d", iminor(inode));
        err = -ENODEV;
        goto op_exit;
    }

    if (pmetrics_device->metrics_device->private_dev.open_flag == 0) {
        pmetrics_device->metrics_device->private_dev.open_flag = 1;
        device_cleanup(pmetrics_device, ST_DISABLE_COMPLETELY);
    } else {
        pr_err("Attempt to open device multiple times not allowed!");
        err = -EPERM;
    }

op_exit:
    unlock_device(pmetrics_device);
    return err;
}


/*
 * This operation is invoked when the file structure is being released. When
 * the user app close a device then this is where the entry point is. The
 * driver cleans up any memory it has reference to. This ensures a clean state
 * of the device.
 */
static int dnvme_release(struct inode *inode, struct file *filp)
{
	/* Metrics device */
	struct  metrics_device_list *pmetrics_device;
	int err = 0;

    pr_debug("Call to Release the device");
    pmetrics_device = lock_device(inode);
    if (pmetrics_device == NULL) {
        pr_err("Cannot lock on this device with minor # %d", iminor(inode));
        err = -ENODEV;
        goto rel_exit;
    }

    /* Set the device open flag to false */
    pmetrics_device->metrics_device->private_dev.open_flag = 0;
    device_cleanup(pmetrics_device, ST_DISABLE_COMPLETELY);

rel_exit:
    pr_debug("NVMe device closed");
    unlock_device(pmetrics_device);
    return err;
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
	dev_t devno = MKDEV(dnvme_major, nvme_minor);
	struct metrics_device_list *pmetrics_device = NULL;
	int bars = 0;

	pr_info("probe pdev...(cpu:%d %d %d)\n", num_possible_cpus(), 
		num_present_cpus(), num_active_cpus());

	/* Allocate kernel memory for our own internal tracking of this device */
	pmetrics_device = kzalloc(sizeof(struct metrics_device_list), GFP_KERNEL);
	if (!pmetrics_device) {
		pr_err("failed to alloc metrics_device_list!\n");
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

    mutex_init(&pmetrics_device->metrics_mtx);
    pmetrics_device->metrics_device->private_dev.open_flag = 0;
    pmetrics_device->metrics_device->private_dev.minor_no = nvme_minor;

    /* Create an NVMe special device */
    pmetrics_device->metrics_device->private_dev.spcl_dev = device_create(
        dnvme_class, NULL, devno, NULL, DEVICE_NAME"%d", nvme_minor);
    if (IS_ERR(pmetrics_device->metrics_device->private_dev.spcl_dev)) {
        err = PTR_ERR(pmetrics_device->metrics_device->private_dev.spcl_dev);
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
    //     __u32 csts = readl(&pmetrics_device->metrics_device->private_dev.ctrlr_regs->csts);
    //     pr_err("csts: 0x%x", csts);
    // }
    // else
    // {
    //     pr_err("pcie is disable");
    // }

    nvme_map_cmb(pmetrics_device->metrics_device);

    /* Finalize this device and prepare for next one */
    pr_debug("NVMe dev: 0x%x, vendor: 0x%x", pdev->device, pdev->vendor);
    pr_debug("NVMe bus #%d, dev slot: %d", pdev->bus->number,
        PCI_SLOT(pdev->devfn));
    pr_debug("NVMe func: 0x%x, class: 0x%x", PCI_FUNC(pdev->devfn),
        pdev->class);
    list_add_tail(&pmetrics_device->metrics_device_hd, &metrics_dev_ll);
    nvme_minor++;
    return 0;


spcp_fail_out:
    device_del(pmetrics_device->metrics_device->private_dev.spcl_dev);
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
    struct metrics_device_list *pmetrics_device;


    /* Loop through the devices available in the metrics list */
    list_for_each_entry(pmetrics_device, &metrics_dev_ll, metrics_device_hd) {

        pdev = pmetrics_device->metrics_device->private_dev.pdev;
        if (pdev == dev) {

            pr_debug("Removing device: 0x%x, vendor: 0x%x",
                pdev->device, pdev->vendor);
            pr_debug("PCIe bus #%d, slot: %d", pdev->bus->number,
                PCI_SLOT(pdev->devfn));
            pr_debug("PCIe func: 0x%x, class: 0x%x", PCI_FUNC(pdev->devfn),
                pdev->class);

            nvme_release_cmb(pmetrics_device->metrics_device);
            /* Wait for any other dnvme access to finish, then stop further
             * before we free resources to prevent circular issues */
            mutex_lock(&pmetrics_device->metrics_mtx);
            device_cleanup(pmetrics_device, ST_DISABLE_COMPLETELY);
            pci_disable_device(pdev);

            /* Release the selected memory regions that were reserved */
            if (pmetrics_device->metrics_device->private_dev.bar0 != NULL) {
                destroy_dma_pool(pmetrics_device->metrics_device);
                iounmap(pmetrics_device->metrics_device->private_dev.bar0);
                release_mem_region(pci_resource_start(pdev, BAR0_BAR1),
                    pci_resource_len(pdev, BAR0_BAR1));
            }
            if (pmetrics_device->metrics_device->private_dev.bar1 != NULL) {
                iounmap(pmetrics_device->metrics_device->private_dev.bar1);
                release_mem_region(pci_resource_start(pdev, BAR2_BAR3),
                    pci_resource_len(pdev, BAR2_BAR3));
            }
            if (pmetrics_device->metrics_device->private_dev.bar2 != NULL) {
                iounmap(pmetrics_device->metrics_device->private_dev.bar2);
                release_mem_region(pci_resource_start(pdev, BAR4_BAR5),
                    pci_resource_len(pdev, BAR4_BAR5));
            }

            /* Free up the linked list */
            list_del(&pmetrics_device->metrics_cq_list);
            list_del(&pmetrics_device->metrics_sq_list);

            /* Unlock, then destroy all mutexes */
            mutex_unlock(&pmetrics_device->metrics_mtx);
            mutex_destroy(&pmetrics_device->metrics_mtx);
            mutex_destroy(&pmetrics_device->irq_process.irq_track_mtx);

            device_del(pmetrics_device->metrics_device->private_dev.spcl_dev);
        }
    }

    /* Free up the device linked list if there are not items left */
    if (list_empty(&metrics_dev_ll)) {
        list_del(&metrics_dev_ll);
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

	g_metrics_drv.api_version = API_VERSION;
	g_metrics_drv.driver_version = DRIVER_VERSION;

	dnvme_major = register_chrdev(0, DEVICE_NAME, &dnvme_fops);
	if (dnvme_major < 0) {
		pr_err("failed to register chrdev!(%d)\n", dnvme_major);
		return dnvme_major;
	}

	dnvme_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(dnvme_class)) {
		ret = PTR_ERR(dnvme_class);
		pr_err("failed to create %s class!(%d)\n", DEVICE_NAME, ret);
		goto out;
	}

	ret = pci_register_driver(&dnvme_driver);
	if (ret < 0) {
		pr_err("failed to register pci driver!(%d)\n", ret);
		goto out2;
	}

	pr_info("init ok!(api_ver:%x, drv_ver:%x)\n", g_metrics_drv.api_version, 
		g_metrics_drv.driver_version);
	return 0;

out2:
	class_destroy(dnvme_class);
out:
	unregister_chrdev(dnvme_major, DEVICE_NAME);
	return ret;
}
module_init(dnvme_init);

static void __exit dnvme_exit(void)
{
	pci_unregister_driver(&dnvme_driver);
	class_destroy(dnvme_class);
	unregister_chrdev(dnvme_major, DEVICE_NAME);
	pr_info("exit ok!(api_ver:%x, drv_ver:%x)\n", g_metrics_drv.api_version, 
		g_metrics_drv.driver_version);
}
module_exit(dnvme_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:"DRIVER_NAME);
MODULE_AUTHOR("nvmecompliance@intel.com");
MODULE_DESCRIPTION("NVMe compliance suite kernel driver");
MODULE_VERSION(DRIVER_VERSION_STR(DRIVER_VERSION));

