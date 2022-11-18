/**
 * @file dnvme_cmb.c
 * @author yumeng (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-11
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)//2021.06.08 meng_yu add
#include <linux/pci-p2pdma.h>
#endif

#include "definitions.h"
#include "dnvme_reg.h"
#include "dnvme_ds.h"
#include "sysdnvme.h"

static bool use_cmb_sqes = true;

static u64 nvme_cmb_size_unit(struct nvme_device *dev)
{
	u8 szu = (dev->cmbsz >> NVME_CMBSZ_SZU_SHIFT) & NVME_CMBSZ_SZU_MASK;

	return 1ULL << (12 + 4 * szu);
}

static u32 nvme_cmb_size(struct nvme_device *dev)
{
	return (dev->cmbsz >> NVME_CMBSZ_SZ_SHIFT) & NVME_CMBSZ_SZ_MASK;
}

void nvme_map_cmb(struct nvme_device *dev)
{
	u64 size, offset;
	resource_size_t bar_size;
	struct pci_dev *pdev = dev->private_dev.pdev;
	int bar;
	if (dev->cmb_size)
		return;
    dev->cmbsz = readl(&dev->private_dev.ctrlr_regs->cmbsz);
	pr_info("cmbsz:%x",dev->cmbsz);        
	if (!dev->cmbsz)
		return;
	dev->cmbloc = readl(&dev->private_dev.ctrlr_regs->cmbloc);
	pr_info("cmbloc:%x",dev->cmbloc);        
	size = nvme_cmb_size_unit(dev) * nvme_cmb_size(dev);
	offset = nvme_cmb_size_unit(dev) * NVME_CMB_OFST(dev->cmbloc);
	bar = NVME_CMB_BIR(dev->cmbloc);
	bar_size = pci_resource_len(pdev, bar);
	// pr_debug("CMB:%llx %llx %x %x",size,offset,bar, *((int*)bar_size));
	pr_info("cmb:%llx %llx %x",size,offset,bar);

	if (offset > bar_size)
		return;

	/*
	 * Controllers may support a CMB size larger than their BAR,
	 * for example, due to being behind a bridge. Reduce the CMB to
	 * the reported size of the BAR
	 */
	if (size > bar_size - offset)
		size = bar_size - offset;

	if (pci_p2pdma_add_resource(pdev, bar, size, offset)) {
		pr_err("failed to register the CMB");
		return;
	}

	dev->cmb_size = size;
	dev->cmb_use_sqes = use_cmb_sqes && (dev->cmbsz & NVME_CMBSZ_SQS);

	if ((dev->cmbsz & (NVME_CMBSZ_WDS | NVME_CMBSZ_RDS)) ==
			(NVME_CMBSZ_WDS | NVME_CMBSZ_RDS))
		pci_p2pmem_publish(pdev, true);

	// if (sysfs_add_file_to_group(&dev->private_dev.spcl_dev->kobj,
	// 			    &dev_attr_cmb.attr, NULL))
	// 	pr_err("failed to add sysfs attribute for CMB\n");
}


inline void nvme_release_cmb(struct nvme_device *dev)
{
	if (dev->cmb_size) {
		// sysfs_remove_file_from_group(&dev->private_dev.spcl_dev->kobj,
		// 			     &dev_attr_cmb.attr, NULL);
		dev->cmb_size = 0;
	}
}
