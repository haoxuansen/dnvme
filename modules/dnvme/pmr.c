/**
 * @file pmr.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief Persistent Memory Region
 * @version 0.1
 * @date 2023-04-17
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/pci.h>

#include "dnvme.h"
#include "core.h"
#include "io.h"

static int dnvme_pmr_parse_capability(struct nvme_device *ndev, struct nvme_pmr *pmr)
{
	void __iomem *bar0 = ndev->bar[0];
	u32 pmrcap;

	pmrcap = dnvme_readl(bar0, NVME_REG_PMRCAP);

	pmr->bir = NVME_PMRCAP_BIR(pmrcap);
	if (pmr->bir < 2 || pmr->bir > 5) {
		dnvme_err(ndev, "invalid bar no.%u!\n", pmr->bir);
		return -EPERM;
	}

	if (!ndev->bar[pmr->bir]) {
		dnvme_err(ndev, "required to map bar%u first!\n", pmr->bir);
		return -EPERM;
	}

	pmr->timeout = NVME_PMRCAP_PMRTO(pmrcap) * (NVME_PMRCAP_PMRTU(pmrcap) ? 
		(60 * 1000) : 500);
	pmr->cmss = (pmrcap & NVME_PMRCAP_CMSS) ? 1 : 0;
	
	return 0;
}

static int dnvme_pmr_enable_cba(struct nvme_device *ndev, struct nvme_pmr *pmr)
{
	struct pci_dev *pdev = ndev->pdev;
	void __iomem *bar0 = ndev->bar[0];
	u32 pmrmscl, pmrmscu, pmrsts;

	if (!pmr->cmss) {
		dnvme_warn(ndev, "Not support controller memory space!\n");
		return 0;
	}

	pmr->res_addr = pci_resource_start(pdev, pmr->bir);
	pmr->bus_addr = pci_bus_address(pdev, pmr->bir);
	pmr->size = pci_resource_len(pdev, pmr->bir);
	pmrmscu = upper_32_bits(pmr->bus_addr);
	pmrmscl = lower_32_bits(pmr->bus_addr) | NVME_PMRMSCL_CMSE;

	if (pmrmscl & NVME_PMRMSCL_RSVD) {
		dnvme_err(ndev, "PMRMSCL reserved field is not zero!\n");
		return -EPERM;
	}
	dnvme_writel(bar0, NVME_REG_PMRMSCU, pmrmscu);
	dnvme_writel(bar0, NVME_REG_PMRMSCL, pmrmscl);

	pmrsts = dnvme_readl(bar0, NVME_REG_PMRSTS);
	if (pmrsts & NVME_PMRSTS_CBAI) {
		dnvme_err(ndev, "controller base address is invalid!\n");
		return -EPERM;
	}

	dnvme_dbg(ndev, "Controller Memory Space Enable Success!\n");
	return 0;
}

static void dnvme_pmr_disable_cba(struct nvme_device *ndev, struct nvme_pmr *pmr)
{
	void __iomem *bar0 = ndev->bar[0];
	u32 pmrmscl;

	if (!pmr->cmss)
		return;

	pmrmscl = dnvme_readl(bar0, NVME_REG_PMRMSCL);
	pmrmscl &= ~NVME_PMRMSCL_CMSE;
	dnvme_writel(bar0, NVME_REG_PMRMSCL, pmrmscl);
}

static int dnvme_config_pmr(struct nvme_device *ndev, struct nvme_pmr *pmr, bool enable)
{
	void __iomem *bar0 = ndev->bar[0];
	u32 pmrctl, pmrsts;
	unsigned long timeout;

	pmrctl = dnvme_readl(bar0, NVME_REG_PMRCTL);
	if (enable && (pmrctl & NVME_PMRCTL_EN)) {
		dnvme_warn(ndev, "PMR is already enabled!\n");
		return 0;
	} else if (!enable && !(pmrctl & NVME_PMRCTL_EN)) {
		dnvme_warn(ndev, "PMR is already disabled!\n");
		return 0;
	}

	if (enable)
		pmrctl |= NVME_PMRCTL_EN;
	else
		pmrctl &= ~NVME_PMRCTL_EN;
	
	dnvme_writel(bar0, NVME_REG_PMRCTL, pmrctl);

	timeout = jiffies + msecs_to_jiffies(pmr->timeout);

	do {
		pmrsts = dnvme_readl(bar0, NVME_REG_PMRSTS);
		if ((enable && !(pmrsts & NVME_PMRSTS_NRDY)) || 
			(!enable && (pmrsts & NVME_PMRSTS_NRDY)))
			break;
		
		if (time_after(jiffies, timeout)) {
			dnvme_err(ndev, "wait timeout! pmrsts:0x%x\n", pmrsts);
			return -ETIME;
		}
	} while (1);

	return 0;
}

static int dnvme_enable_pmr(struct nvme_device *ndev, struct nvme_pmr *pmr)
{
	return dnvme_config_pmr(ndev, pmr, true);
}

static void dnvme_disable_pmr(struct nvme_device *ndev, struct nvme_pmr *pmr)
{
	dnvme_config_pmr(ndev, pmr, false);
}

int dnvme_map_pmr(struct nvme_device *ndev)
{
	struct nvme_pmr *pmr;
	int ret;

	if (ndev->pmr) {
		dnvme_dbg(ndev, "PMR is already mapped!\n");
		return 0;
	}

	if (!NVME_CAP_PMRS(ndev->reg_cap)) {
		dnvme_warn(ndev, "Not support persistent memory region! skip map\n");
		return -EOPNOTSUPP;
	}

	pmr = kzalloc(sizeof(*pmr), GFP_KERNEL);
	if (!pmr) {
		dnvme_err(ndev, "failed to alloc pmr!\n");
		return -ENOMEM;
	}

	ret = dnvme_pmr_parse_capability(ndev, pmr);
	if (ret < 0)
		goto out_free_pmr;

	ret = dnvme_pmr_enable_cba(ndev, pmr);
	if (ret < 0)
		goto out_free_pmr;

	ret = dnvme_enable_pmr(ndev, pmr);
	if (ret < 0)
		goto out_disable_cba;

	dnvme_info(ndev, "PMR BIR:%u, Addr:0x%llx, Size:0x%llx\n", pmr->bir,
		pmr->bus_addr, pmr->size);

	ndev->pmr = pmr;
	return 0;

out_disable_cba:
	dnvme_pmr_disable_cba(ndev, pmr);
out_free_pmr:
	kfree(pmr);
	return ret;
}

void dnvme_unmap_pmr(struct nvme_device *ndev)
{
	if (!ndev->pmr)
		return;

	dnvme_disable_pmr(ndev, ndev->pmr);
	dnvme_pmr_disable_cba(ndev, ndev->pmr);
	kfree(ndev->pmr);
	ndev->pmr = NULL;
}
