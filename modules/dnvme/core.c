/**
 * @file core.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-04-07
 * 
 * @copyright Copyright (c) 2023
 * 
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

#include "dnvme.h"

#include "core.h"
#include "proc.h"
#include "pci.h"
#include "io.h"
#include "ioctl.h"
#include "irq.h"
#include "queue.h"
#include "debug.h"

#define NVME_MINORS			(1U << MINORBITS)

LIST_HEAD(nvme_dev_list);
int nvme_gnl_id;

static DEFINE_MUTEX(nvme_dev_list_lock);
static struct proc_dir_entry *nvme_proc_dir;

static DEFINE_IDA(nvme_instance_ida);
static dev_t nvme_chr_devt;
static struct class *nvme_class;

/**
 * @brief Find nvme_device from linked list. 
 *  
 * @return &struct nvme_device on success, or ERR_PTR() on error. 
 */
static struct nvme_device *find_device(int instance)
{
	struct nvme_device *ndev;

	list_for_each_entry(ndev, &nvme_dev_list, entry) {

		if (instance == ndev->instance) {
			return ndev;
		}
	}
	return ERR_PTR(-ENODEV);
}

/**
 * @brief Lock the mutex if find nvme_device.
 * 
 * @return &struct nvme_device on success, or ERR_PTR() on error. 
 */
struct nvme_device *dnvme_lock_device(int instance)
{
	struct nvme_device *ndev;

	ndev = find_device(instance);
	if (IS_ERR(ndev)) {
		pr_err("Cannot find the device with instance %d!\n", instance);
		return ndev;
	}

	mutex_lock(&ndev->lock);
	return ndev;
}

void dnvme_unlock_device(struct nvme_device *ndev)
{
	if (mutex_is_locked(&ndev->lock)) {
		mutex_unlock(&ndev->lock);
	} else {
		dnvme_warn(ndev, "already unlocked, lock missmatch!\n");
	}
}

/**
 * @brief Parse vm_paoff in "struct vm_area_struct"
 * 
 * @param kva kernal virtual address
 * @param size mmap size
 * @return 0 on success, otherwise a negative errno.
 */
static int mmap_parse_vmpgoff(struct nvme_device *ndev, unsigned long vm_pgoff,
	void **kva, u32 *size)
{
	struct nvme_sq *sq;
	struct nvme_cq *cq;
	struct nvme_meta *meta;
	u32 type = NVME_VMPGOFF_TO_TYPE(vm_pgoff);
	u32 id = NVME_VMPGOFF_ID(vm_pgoff);

	switch (type) {
	case NVME_VMPGOFF_TYPE_CQ:
		cq = dnvme_find_cq(ndev, id);
		if (!cq) {
			dnvme_err(ndev, "Cannot find CQ(%u)!\n", id);
			return -EBADSLT;
		}

		if (!cq->contig) {
			dnvme_err(ndev, "Cannot map non-contig CQ!\n");
			return -EOPNOTSUPP;
		}
		*kva = cq->buf;
		*size = cq->size;
		break;

	case NVME_VMPGOFF_TYPE_SQ:
		sq = dnvme_find_sq(ndev, id);
		if (!sq) {
			dnvme_err(ndev, "Cannot find SQ(%u)!\n", id);
			return -EBADSLT;
		}

		if (!sq->contig) {
			dnvme_err(ndev, "Cannot map non-contig SQ!\n");
			return -EOPNOTSUPP;
		}
		*kva = sq->buf;
		*size = sq->size;
		break;

	case NVME_VMPGOFF_TYPE_META:
		meta = dnvme_find_meta(ndev, id);
		if (!meta) {
			dnvme_err(ndev, "Cannot find Meta(%u)!\n", id);
			return -EBADSLT;
		}

		if (!meta->contig) {
			dnvme_err(ndev, "Cannot map SGL meta!\n");
			return -EOPNOTSUPP;
		}

		*kva = meta->buf;
		*size = meta->size;
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
void dnvme_cleanup_device(struct nvme_device *ndev, enum nvme_state state)
{
	dnvme_clean_interrupt(ndev);
	/* Clean Up the data structures */
	dnvme_delete_all_queues(ndev, state);
	dnvme_delete_meta_nodes(ndev);
	dnvme_release_hmb(ndev);
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
	struct nvme_device *ndev;
	struct inode *inode = filp->f_path.dentry->d_inode;
	void *map_addr;
	u32 map_size;
	int npages;
	unsigned long pfn;
	int ret = 0;

	ndev = dnvme_lock_device(iminor(inode));
	if (IS_ERR(ndev))
		return PTR_ERR(ndev);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
	vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_DONTDUMP);
#else
	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;
#endif
	ret = mmap_parse_vmpgoff(ndev, vma->vm_pgoff, &map_addr, &map_size);
	if (ret < 0)
		goto out;

	npages = DIV_ROUND_UP(map_size, PAGE_SIZE);
	if ((npages * PAGE_SIZE) < (vma->vm_end - vma->vm_start)) {
		dnvme_err(ndev, "Request to Map more than allocated pages...\n");
		ret = -EINVAL;
		goto out;
	}
	dnvme_vdbg(ndev, "K.V.A = 0x%lx, PAGES = %d\n", (unsigned long)map_addr, npages);

	/* Associated struct page ptr for kernel logical address */
	pfn = virt_to_phys(map_addr) >> PAGE_SHIFT;
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
	dnvme_unlock_device(ndev);
	return ret;
}

static long dnvme_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct nvme_device *ndev;
	struct inode *inode = inode = filp->f_path.dentry->d_inode;
	void __user *argp = (void __user *)arg;

	dnvme_dbg(ndev, "cmd num:%u, arg:0x%lx (%s)\n", _IOC_NR(cmd), arg,
		dnvme_ioctl_cmd_string(cmd));

	ndev = dnvme_lock_device(iminor(inode));
	if (IS_ERR(ndev))
		return PTR_ERR(ndev);

	switch (cmd) {
	case NVME_IOCTL_GET_SQ_INFO:
		ret = dnvme_get_sq_info(ndev, argp);
		break;

	case NVME_IOCTL_GET_CQ_INFO:
		ret = dnvme_get_cq_info(ndev, argp);
		break;

	case NVME_IOCTL_READ_GENERIC:
		ret = dnvme_generic_read(ndev, argp);
		break;

	case NVME_IOCTL_WRITE_GENERIC:
		ret = dnvme_generic_write(ndev, argp);
		break;

	case NVME_IOCTL_GET_PCI_BDF:
		ret = dnvme_get_pci_bdf(ndev, argp);
		break;

	case NVME_IOCTL_GET_DEV_INFO:
		ret = dnvme_get_dev_info(ndev, argp);
		break;

	case NVME_IOCTL_GET_CAPABILITY:
		ret = dnvme_get_capability(ndev, argp);
		break;

	case NVME_IOCTL_CREATE_ADMIN_QUEUE:
		ret = dnvme_create_admin_queue(ndev, argp);
		break;

	case NVME_IOCTL_SET_DEV_STATE:
		ret = dnvme_set_device_state(ndev, (enum nvme_state)arg);
		break;

	case NVME_IOCTL_PREPARE_IOSQ:
		ret = dnvme_prepare_sq(ndev, argp);
		break;

	case NVME_IOCTL_PREPARE_IOCQ:
		ret = dnvme_prepare_cq(ndev, argp);
		break;

	case NVME_IOCTL_RING_SQ_DOORBELL:
		ret = dnvme_ring_sq_doorbell(ndev, (u16)arg);
		break;

	case NVME_IOCTL_SUBMIT_64B_CMD:
		ret = dnvme_submit_64b_cmd(ndev, argp);
		break;

	case NVME_IOCTL_TAMPER_CMD:
		ret = dnvme_tamper_cmd(ndev, argp);
		break;

	case NVME_IOCTL_EMPTY_CMD_LIST:
	{
		struct nvme_sq *sq = dnvme_find_sq(ndev, (u16)arg);

		if (sq) {
			dnvme_delete_cmd_list(ndev, sq);
		} else {
			pr_err("failed to find SQ(%u)!\n", (u16)arg);
			ret = -EINVAL;
		}
		break;
	}
	case NVME_IOCTL_INQUIRY_CQE:
		ret = dnvme_inquiry_cqe(ndev, argp);
		break;

	case NVME_IOCTL_REAP_CQE:
		ret = dnvme_reap_cqe_legacy(ndev, argp);
		break;

	case NVME_IOCTL_CREATE_META_NODE:
		ret = dnvme_create_meta_node(ndev, argp);
		break;

	case NVME_IOCTL_DELETE_META_NODE:
		dnvme_delete_meta_id(ndev, (u32)arg);
		break;

	case NVME_IOCTL_SET_IRQ:
		ret = dnvme_set_interrupt(ndev, argp);
		break;

	case NVME_IOCTL_MASK_IRQ:
		ret = dnvme_mask_interrupt(&ndev->irq_set, (u16)arg);
		break;

	case NVME_IOCTL_UNMASK_IRQ:
		ret = dnvme_unmask_interrupt(&ndev->irq_set, (u16)arg);
		break;

	case NVME_IOCTL_ALLOC_HMB:
		ret = dnvme_alloc_hmb(ndev, argp);
		break;

	case NVME_IOCTL_RELEASE_HMB:
		ret = dnvme_release_hmb(ndev);
		break;

	case NT_IOCTL_IOPS:
		ret = dnvme_test_iops(ndev, argp);
		break;
	default:
		dnvme_err(ndev, "cmd(%u) is unknown!\n", _IOC_NR(cmd));
		ret = -EINVAL;
	}

	dnvme_unlock_device(ndev);
	return ret;
}

static int dnvme_open(struct inode *inode, struct file *filp)
{
	struct nvme_device *ndev;
	int ret = 0;

	ndev = dnvme_lock_device(iminor(inode));
	if (IS_ERR(ndev))
		return PTR_ERR(ndev);

	if (ndev->opened) {
		dnvme_err(ndev, "It's not allowed to open device more than once!\n");
		ret = -EPERM;
		goto out;
	}

	ndev->opened = 1;
	dnvme_info(ndev, "Open NVMe device ok!\n");
out:
	dnvme_unlock_device(ndev);
	return ret;
}

static int dnvme_release(struct inode *inode, struct file *filp)
{
	struct nvme_device *ndev;

	ndev = dnvme_lock_device(iminor(inode));
	if (IS_ERR(ndev))
		return PTR_ERR(ndev);

	dnvme_info(ndev, "Close NVMe device ...\n");

	ndev->opened = 0;
	/* !TODO: shall reset nvme device before delete I/O queue?
	 * Otherwise, the information saved by the driver may be inconsistent
	 * with the device.
	 */
	dnvme_cleanup_device(ndev, NVME_ST_DISABLE_COMPLETE);
	dnvme_unlock_device(ndev);
	return 0;
}

static const struct file_operations dnvme_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= dnvme_ioctl,
	.open		= dnvme_open,
	.release	= dnvme_release,
	.mmap		= dnvme_mmap, /* !TODO: set map or unmap flag? */
};

static int dnvme_map_bar(struct nvme_device *ndev, int idx)
{
	struct pci_dev *pdev = ndev->pdev;
	const char *name[PCI_BAR_MAX_NUM] = { 
		"bar0", "bar1", "bar2", "bar3", "bar4", "bar5"
	};
	int ret;

	if (unlikely(ndev->bar[idx])) {
		dnvme_warn(ndev, "BAR%d is mapped already!\n", idx);
		return 0;
	}

	ret = pci_request_region(pdev, idx, name[idx]);
	if (ret < 0) {
		dnvme_err(ndev, "BAR%d is already in use!(%d)\n", idx, ret);
		return ret;
	}

	ndev->bar[idx] = ioremap(pci_resource_start(pdev, idx), 
		pci_resource_len(pdev, idx));
	if (!ndev->bar[idx]) {
		dnvme_err(ndev, "failed to map BAR%d!\n", idx);
		ret = -ENOMEM;
		goto out;
	}
	dnvme_info(ndev, "BAR%d: 0x%llx + 0x%llx mapped to 0x%p!\n", 
		idx, pci_resource_start(pdev, idx), pci_resource_len(pdev, idx), 
		ndev->bar[idx]);
	return 0;

out:
	pci_release_region(pdev, idx);
	return ret;
}

static void dnvme_unmap_bar(struct nvme_device *ndev, int idx)
{
	struct pci_dev *pdev = ndev->pdev;

	if (unlikely(!ndev->bar[idx]))
		return;

	iounmap(ndev->bar[idx]);
	ndev->bar[idx] = NULL;
	pci_release_region(pdev, idx);
}

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
static int dnvme_map_resource(struct nvme_device *ndev)
{
	struct pci_dev *pdev = ndev->pdev;
	int bars;
	int idx;
	int ret;

	bars = pci_select_bars(pdev, IORESOURCE_MEM);
	if (!(bars & BIT(0))) {
		dnvme_err(ndev, "BAR0 (64-bit) is not support!\n");
		return -ENODEV;
	}

	for (idx = 0; idx < PCI_BAR_MAX_NUM; idx++) {
		if (bars & BIT(idx)) {
			ret = dnvme_map_bar(ndev, idx);
			if (ret < 0)
				goto out;
		}
	}

	ndev->dbs = ndev->bar[0] + NVME_REG_DBS;
	return 0;
out:
	for (idx--; idx >= 0; idx--) {
		dnvme_unmap_bar(ndev, idx);
	}
	return ret;
}

static void dnvme_unmap_resource(struct nvme_device *ndev)
{
	int idx;

	for (idx = 0; idx < PCI_BAR_MAX_NUM; idx++)
		dnvme_unmap_bar(ndev, idx);

	ndev->dbs = NULL;
}

static int dnvme_init_irq(struct nvme_device *ndev)
{
	ndev->irq_set.irq_type = NVME_INT_NONE;
	ndev->irq_set.irq_name = dnvme_irq_type_name(ndev->irq_set.irq_type);
	ndev->irq_set.nr_irq = 0;
	return 0;
}

/**
 * @brief Create coherent DMA mapping for PRP & SGL List. 
 *  
 * @return 0 on success, otherwise a negative errno. 
 */
static int dnvme_create_pool(struct nvme_device *ndev)
{
	struct device *dev = &ndev->pdev->dev;
	struct dma_pool *pool;

	pool = dma_pool_create("cmd", dev, PAGE_SIZE, PAGE_SIZE, 0);
	if (!pool) {
		dev_err(dev, "failed to create dma pool for cmd!\n");
		return -ENOMEM;
	}
	ndev->cmd_pool = pool;

	pool = dma_pool_create("queue", dev, PAGE_SIZE, PAGE_SIZE, 0);
	if (!pool) {
		dev_err(dev, "failed to create dma pool for queue!\n");
		goto out_destroy_cmd_pool;
	}
	ndev->queue_pool = pool;

	pool = dma_pool_create("meta", dev, PAGE_SIZE, PAGE_SIZE, 0);
	if (!pool) {
		dev_err(dev, "failed to create dma pool for meta!\n");
		goto out_destroy_queue_pool;
	}
	ndev->meta_pool = pool;

	return 0;

out_destroy_queue_pool:
	dma_pool_destroy(ndev->queue_pool);
	ndev->queue_pool = NULL;
out_destroy_cmd_pool:
	dma_pool_destroy(ndev->cmd_pool);
	ndev->cmd_pool = NULL;
	return -ENOMEM;
}

static void dnvme_destroy_pool(struct nvme_device *ndev)
{
	dma_pool_destroy(ndev->meta_pool);
	ndev->meta_pool = NULL;
	dma_pool_destroy(ndev->queue_pool);
	ndev->queue_pool = NULL;
	dma_pool_destroy(ndev->cmd_pool);
	ndev->cmd_pool = NULL;
}

/**
 * @brief Alloc nvme_device and initialize it. 
 *  
 * @return &struct nvme_device on success, or NULL on error. 
 */
static struct nvme_device *dnvme_alloc_device(struct pci_dev *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct nvme_device *ndev;

	ndev = kzalloc(sizeof(*ndev), GFP_KERNEL);
	if (!ndev) {
		dev_err(dev, "failed to alloc nvme_device!\n");
		return NULL;
	}

	/* 1. Initialize nvme device first. */
	ndev->pdev = pdev;

	ret = dnvme_create_pool(ndev);
	if (ret < 0)
		goto out_free_ndev;

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

	xa_init(&ndev->sqs);
	xa_init(&ndev->cqs);
	xa_init(&ndev->meta);

	INIT_LIST_HEAD(&ndev->irq_set.irq_list);
	INIT_LIST_HEAD(&ndev->irq_set.work_list);

	mutex_init(&ndev->lock);
	/* Spinlock to protect from kernel preemption in ISR handler */
	spin_lock_init(&ndev->irq_set.spin_lock);

	dnvme_init_irq(ndev);

	return ndev;
out_free_name:
	kfree_const(ndev->dev.kobj.name);
out_release_instance:
	ida_simple_remove(&nvme_instance_ida, ndev->instance);
out_destroy_pool:
	dnvme_destroy_pool(ndev);
out_free_ndev:
	kfree(ndev);
	return NULL;
}

static void dnvme_release_device(struct nvme_device *ndev)
{
	cdev_device_del(&ndev->cdev, &ndev->dev);
	kfree_const(ndev->dev.kobj.name);
	ida_simple_remove(&nvme_instance_ida, ndev->instance);
	dnvme_destroy_pool(ndev);
	kfree(ndev);
}

static int dnvme_set_dma_mask(struct pci_dev *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 5, 0)
	if (dma_supported(&pdev->dev, DMA_BIT_MASK(64)) == 0) {
		dev_err(dev, "The device unable to address 64 bits of DMA\n");
		return -EPERM;
	}
#endif
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

static int dnvme_pci_enable(struct nvme_device *ndev)
{
	struct pci_dev *pdev = ndev->pdev;
	void __iomem *bar0 = ndev->bar[0];
	int ret;

	pci_set_master(pdev);

	ret = pci_enable_device(pdev);
	if (ret < 0) {
		dnvme_err(ndev, "failed to enable pci device!(%d)\n", ret);
		return ret;
	}

	ndev->reg_cap = dnvme_readq(bar0, NVME_REG_CAP);
	ndev->q_depth = NVME_CAP_MQES(ndev->reg_cap) + 1;
	ndev->db_stride = 1 << NVME_CAP_STRIDE(ndev->reg_cap);

	dnvme_dbg(ndev, "db_stride:%u, q_depth:%u\n", ndev->db_stride, ndev->q_depth);
	return 0;
}

static void dnvme_pci_disable(struct nvme_device *ndev)
{
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
	cap->ltr = pci_ext_get_ltr_cap(pdev, 
		pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_LTR));
	cap->l1ss = pci_ext_get_l1ss_cap(pdev, 
		pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_L1SS));

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
	if (cap->ltr) {
		kfree(cap->ltr);
		cap->ltr = NULL;
	}
	if (cap->l1ss) {
		kfree(cap->l1ss);
		cap->l1ss = NULL;
	}
}

static int dnvme_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct nvme_device *ndev;
	struct device *dev = &pdev->dev;
	int ret;

	dev_info(dev, "probe pdev...(cpu:%d %d %d)\n", num_possible_cpus(), 
		num_present_cpus(), num_active_cpus());

	ret = dnvme_set_dma_mask(pdev);
	if (ret < 0)
		return ret;

	ndev = dnvme_alloc_device(pdev);
	if (!ndev) {
		dev_err(dev, "failed to alloc context!\n");
		return -ENOMEM;
	}

	ret = dnvme_map_resource(ndev);
	if (ret < 0)
		goto out_release_ctx;

	ret = dnvme_pci_enable(ndev);
	if (ret < 0)
		goto out_unmap_res;

	ret = dnvme_init_capability(ndev);
	if (ret < 0)
		goto out_disable_pci;

	ret = dnvme_map_cmb(ndev);
	if (ret < 0 && ret != -EOPNOTSUPP)
		goto out_deinit_cap;

	ret = dnvme_map_pmr(ndev);
	if (ret < 0 && ret != -EOPNOTSUPP)
		goto out_unmap_cmb;

	dnvme_create_proc_entry(ndev, nvme_proc_dir);

	/* Finalize this device and prepare for next one */
	dev_info(dev, "NVMe devno.%d(0x%x:0x%x) init ok!\n", 
		ndev->instance, pdev->vendor, pdev->device);
	dev_vdbg(dev, "NVMe bus #%d, dev slot: %d", pdev->bus->number, 
		PCI_SLOT(pdev->devfn));
	dev_vdbg(dev, "NVMe func: 0x%x, class: 0x%x", PCI_FUNC(pdev->devfn),
		pdev->class);

	mutex_lock(&nvme_dev_list_lock);
	list_add_tail(&ndev->entry, &nvme_dev_list);
	mutex_unlock(&nvme_dev_list_lock);
	return 0;

out_unmap_cmb:
	dnvme_unmap_cmb(ndev);
out_deinit_cap:
	dnvme_deinit_capability(ndev);
out_disable_pci:
	dnvme_pci_disable(ndev);
out_unmap_res:
	dnvme_unmap_resource(ndev);
out_release_ctx:
	dnvme_release_device(ndev);
	return ret;
}

static void dnvme_remove(struct pci_dev *pdev)
{
	bool found = false;
	struct device *dev = &pdev->dev;
	struct nvme_device *ndev;

	dev_info(dev, "Remove NVMe device(0x%x:0x%x) ...\n", pdev->vendor, pdev->device);
	dev_vdbg(dev, "NVMe bus #%d, dev slot: %d", pdev->bus->number, 
		PCI_SLOT(pdev->devfn));
	dev_vdbg(dev, "NVMe func: 0x%x, class: 0x%x", PCI_FUNC(pdev->devfn),
		pdev->class);

	list_for_each_entry(ndev, &nvme_dev_list, entry) {
		if (pdev == ndev->pdev) {
			found = true;
			break;
		}
	}

	if (!found) {
		dev_warn(dev, "Cannot found dev in list!\n");
		return;
	}

	mutex_lock(&nvme_dev_list_lock);
	list_del(&ndev->entry);
	mutex_unlock(&nvme_dev_list_lock);

	if (mutex_is_locked(&ndev->lock)) {
		dev_warn(dev, "nvme context lock is held by user?\n");
	}
	dnvme_destroy_proc_entry(ndev);

	dnvme_cleanup_device(ndev, NVME_ST_DISABLE_COMPLETE);
	dnvme_unmap_pmr(ndev);
	dnvme_unmap_cmb(ndev);
	dnvme_deinit_capability(ndev);
	pci_disable_device(pdev);

	dnvme_unmap_resource(ndev);
	dnvme_release_device(ndev);
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

	ret = dnvme_gnl_init();
	if (ret < 0)
		return ret;
	nvme_gnl_id = ret;

	ret = alloc_chrdev_region(&nvme_chr_devt, 0, NVME_MINORS, "nvme");
	if (ret < 0) {
		pr_err("failed to alloc chrdev!(%d)\n", ret);
		goto out_exit_gnl;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
	nvme_class = class_create("nvme");
#else
	nvme_class = class_create(THIS_MODULE, "nvme");
#endif
	if (IS_ERR(nvme_class)) {
		ret = PTR_ERR(nvme_class);
		pr_err("failed to create class!(%d)\n", ret);
		goto out_release_chrdev;
	}

	nvme_proc_dir = proc_mkdir("nvme", NULL);
	if (!nvme_proc_dir)
		pr_warn("failed to create proc dir!\n");

	ret = pci_register_driver(&dnvme_driver);
	if (ret < 0) {
		pr_err("failed to register pci driver!(%d)\n", ret);
		goto out_destroy_class;
	}

	pr_info("init ok!(gnl_id:%d)\n", nvme_gnl_id);
	return 0;

out_destroy_class:
	class_destroy(nvme_class);
out_release_chrdev:
	unregister_chrdev_region(nvme_chr_devt, NVME_MINORS);
out_exit_gnl:
	dnvme_gnl_exit();
	return ret;
}
module_init(dnvme_init);

static void __exit dnvme_exit(void)
{
	pci_unregister_driver(&dnvme_driver);
	proc_remove(nvme_proc_dir);
	class_destroy(nvme_class);
	unregister_chrdev_region(nvme_chr_devt, NVME_MINORS);
	dnvme_gnl_exit();
	ida_destroy(&nvme_instance_ida);
	pr_info("exit ok!\n");
}
module_exit(dnvme_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("yeqiang_xu@maxio-tech.com");
MODULE_DESCRIPTION("NVMe over PCIe Transport driver");
MODULE_VERSION("v1.0.0");

