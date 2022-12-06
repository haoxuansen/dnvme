/**
 * @file cmb.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief controller memory buffer
 * @version 0.1
 * @date 2022-11-22
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
#include <linux/pci-p2pdma.h>
#endif

#include "nvme.h"
#include "io.h"
#include "core.h"

static int use_cmb_sqes = true;
module_param(use_cmb_sqes, int, 0444);
MODULE_PARM_DESC(use_cmb_sqes, "use controller's memory buffer for I/O SQes");

static u64 dnvme_cmb_size_unit(u32 cmbsz)
{
	u8 szu = (cmbsz >> NVME_CMBSZ_SZU_SHIFT) & NVME_CMBSZ_SZU_MASK;

	return 1ULL << (12 + 4 * szu);
}

static u32 dnvme_cmb_size(u32 cmbsz)
{
	return (cmbsz >> NVME_CMBSZ_SZ_SHIFT) & NVME_CMBSZ_SZ_MASK;
}

int dnvme_map_cmb(struct nvme_device *ndev)
{
	u64 size, offset;
	resource_size_t bar_size;
	struct nvme_dev_private *priv = &ndev->priv;
	struct nvme_ctrl_property *prop = &ndev->prop;
	struct pci_dev *pdev = priv->pdev;
	int bar;
	int ret;

	if (ndev->cmb_size) {
		dnvme_warn("CMB has been mapped! So skip here\n");
		return 0;
	}

	if (NVME_CAP_CMBS(prop->cap)) {
		/*
		 * Enable the CMBLOC and CMBSZ properties, otherwise CMBSZ and
		 * CMBLOC are cleared to 0h.
		 */
		dnvme_writel(priv->bar0, NVME_REG_CMBMSC, NVME_CMBMSC_CRE);
	}

	prop->cmbsz = dnvme_readl(priv->bar0, NVME_REG_CMBSZ);
	if (!prop->cmbsz) {
		dnvme_warn("Not support to map CMB which size is zero!\n");
		return 0;
	}
	prop->cmbloc = dnvme_readl(priv->bar0, NVME_REG_CMBLOC);

	size = dnvme_cmb_size_unit(prop->cmbsz) * dnvme_cmb_size(prop->cmbsz);
	offset = dnvme_cmb_size_unit(prop->cmbsz) * NVME_CMB_OFST(prop->cmbloc);
	bar = NVME_CMB_BIR(prop->cmbloc);
	bar_size = pci_resource_len(pdev, bar);

	if (offset > bar_size) {
		dnvme_err("CMB offset(0x%llx) greater than BAR size(0x%llx)\n",
			offset, bar_size);
		return -EPERM;
	}

	/*
	 * Tell the controller about the host side address mapping the CMB,
	 * and enable CMB decoding for the NVMe 1.4+ scheme:
	 */
	if (NVME_CAP_CMBS(prop->cap)) {
		dnvme_writeq(priv->bar0, NVME_REG_CMBMSC, NVME_CMBMSC_CRE | 
			NVME_CMBMSC_CMSE | (pci_bus_address(pdev, bar) + offset));
	}

	/*
	 * Controllers may support a CMB size larger than their BAR,
	 * for example, due to being behind a bridge. Reduce the CMB to
	 * the reported size of the BAR
	 */
	if (size > bar_size - offset)
		size = bar_size - offset;

	ret = pci_p2pdma_add_resource(pdev, bar, size, offset);
	if (ret < 0) {
		dnvme_err("pci_p2pdma_add_resource err!(%d)\n", ret);
		return ret;
	}

	ndev->cmb_size = size;
	ndev->cmb_use_sqes = use_cmb_sqes && (prop->cmbsz & NVME_CMBSZ_SQS);

	if ((prop->cmbsz & (NVME_CMBSZ_WDS | NVME_CMBSZ_RDS)) ==
			(NVME_CMBSZ_WDS | NVME_CMBSZ_RDS))
		pci_p2pmem_publish(pdev, true);
	
	dnvme_dbg("CMB is mapped ok!\n");
	return 0;
}

void dnvme_unmap_cmb(struct nvme_device *ndev)
{
	if (ndev->cmb_size) {
		/* !TODO: Need do something more?(eg.remove resource) */
		ndev->cmb_size = 0;
	}
}
