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

static u64 dnvme_cmb_size_unit(u32 cmbsz)
{
	u8 szu = NVME_CMBSZ_SZU(cmbsz);

	return 1ULL << (12 + 4 * szu);
}

static u32 dnvme_cmb_size(u32 cmbsz)
{
	return NVME_CMBSZ_SZ(cmbsz);
}

static int dnvme_cmb_parse_capability(struct nvme_cmb *cmb, u32 cmbsz, u32 cmbloc)
{
	if (cmbsz & NVME_CMBSZ_SQS)
		cmb->sqs = 1;
	if (cmbsz & NVME_CMBSZ_CQS)
		cmb->cqs = 1;
	if (cmbsz & NVME_CMBSZ_LISTS)
		cmb->lists = 1;
	if (cmbsz & NVME_CMBSZ_RDS)
		cmb->rds = 1;
	if (cmbsz & NVME_CMBSZ_WDS)
		cmb->wds = 1;
	
	if (cmbloc & NVME_CMBLOC_CQMMS)
		cmb->cqmms = 1;
	if (cmbloc & NVME_CMBLOC_CQPDS)
		cmb->cqpds = 1;
	if (cmbloc & NVME_CMBLOC_CDPMLS)
		cmb->cdpmls = 1;
	if (cmbloc & NVME_CMBLOC_CDPCILS)
		cmb->cdpcils = 1;
	if (cmbloc & NVME_CMBLOC_CDMMMS)
		cmb->cdmmms = 1;
	if (cmbloc & NVME_CMBLOC_CQDA)
		cmb->cqda = 1;
	return 0;
}

static int dnvme_cmb_setup(struct nvme_device *ndev, struct nvme_cmb *cmb)
{
	struct pci_dev *pdev = ndev->pdev;
	void __iomem *bar0 = ndev->bar[0];
	resource_size_t bar_size;
	u32 cmbsz, cmbloc;
	int ret;

	/*
	 * Enable the CMBLOC and CMBSZ property, otherwise CMBSZ and 
	 * CMBLOC are cleared to 0h.
	 */
	dnvme_writel(bar0, NVME_REG_CMBMSC, NVME_CMBMSC_CRE);

	cmbsz = dnvme_readl(bar0, NVME_REG_CMBSZ);
	if (!cmbsz) {
		dnvme_warn(ndev, "Not support to map CMB which size is zero!\n");
		return -EOPNOTSUPP;
	}
	cmbloc = dnvme_readl(bar0, NVME_REG_CMBLOC);

	dnvme_cmb_parse_capability(cmb, cmbsz, cmbloc);

	cmb->bar = NVME_CMBLOC_BIR(cmbloc);
	cmb->size = dnvme_cmb_size_unit(cmbsz) * dnvme_cmb_size(cmbsz);
	cmb->offset = dnvme_cmb_size_unit(cmbsz) * NVME_CMBLOC_OFST(cmbloc);
	cmb->res_addr = pci_resource_start(pdev, cmb->bar);
	cmb->bus_addr = pci_bus_address(pdev, cmb->bar);

	bar_size = pci_resource_len(pdev, cmb->bar);

	if (cmb->offset > bar_size) {
		dnvme_err(ndev, "CMB offset(0x%llx) greater than BAR size(0x%llx)\n",
			cmb->offset, bar_size);
		return -EPERM;
	}

	/*
	 * Tell the controller about the host side address mapping the CMB,
	 * and enable CMB decoding for the NVMe 1.4+ scheme:
	 */
	dnvme_writeq(bar0, NVME_REG_CMBMSC, NVME_CMBMSC_CRE | 
		NVME_CMBMSC_CMSE | (cmb->bus_addr + cmb->offset));

	/*
	 * Controllers may support a CMB size larger than their BAR,
	 * for example, due to being behind a bridge. Reduce the CMB to
	 * the reported size of the BAR
	 */
	if (cmb->size > bar_size - cmb->offset)
		cmb->size = bar_size - cmb->offset;

	ret = pci_p2pdma_add_resource(pdev, cmb->bar, cmb->size, cmb->offset);
	if (ret < 0) {
		dnvme_err(ndev, "failed to add p2pdma resource!(%d)\n", ret);
		return ret;
	}

	if ((cmbsz & (NVME_CMBSZ_WDS | NVME_CMBSZ_RDS)) == 
				(NVME_CMBSZ_WDS | NVME_CMBSZ_RDS))
		pci_p2pmem_publish(pdev, true);

	return 0;
}

int dnvme_map_cmb(struct nvme_device *ndev)
{
	struct nvme_cmb *cmb;
	int ret;

	if (ndev->cmb) {
		dnvme_dbg(ndev, "CMB is already mapped!\n");
		return 0;
	}

	if (!NVME_CAP_CMBS(ndev->reg_cap)) {
		dnvme_warn(ndev, "Not support controller memory buffer! skip map\n");
		return -EOPNOTSUPP;
	}

	cmb = kzalloc(sizeof(*cmb), GFP_KERNEL);
	if (!cmb) {
		dnvme_err(ndev, "failed to alloc cmb!\n");
		return -ENOMEM;
	}

	ret = dnvme_cmb_setup(ndev, cmb);
	if (ret < 0)
		goto out_free_cmb;

	dnvme_info(ndev, "CMB BAR:%u, Res Addr:0x%llx, Bus Addr:0x%llx, "
		"Size:0x%llx\n", cmb->bar, cmb->res_addr + cmb->offset, 
		cmb->bus_addr + cmb->offset, cmb->size);
	
	ndev->cmb = cmb;
	return 0;

out_free_cmb:
	kfree(cmb);
	return ret;
}

void dnvme_unmap_cmb(struct nvme_device *ndev)
{
	if (!ndev->cmb)
		return;
	
	/* !TODO: Need do something more?(eg.remove resource) */
	kfree(ndev->cmb);
	ndev->cmb = NULL;
}
