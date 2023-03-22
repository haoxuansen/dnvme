/**
 * @file pci.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief Access PCI or PCIe configuration space, etc.
 * @version 0.1
 * @date 2022-11-18
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#define DEBUG

#include <linux/kernel.h>
#include <linux/delay.h>

#include "pci.h"

int pci_read_cfg_data(struct pci_dev *pdev, void *buf, u32 len)
{
	u32 oft = 0;
	int ret = 0;

	while (len >= 4) {
		ret |= pcibios_err_to_errno(
			pci_read_config_dword(pdev, oft, buf + oft));
		oft += 4;
		len -= 4;
	}

	if (len >= 2) {
		ret |= pcibios_err_to_errno(
			pci_read_config_word(pdev, oft, buf + oft));
		oft += 2;
		len -= 2;
	}

	if (len >= 1) {
		ret |= pcibios_err_to_errno(
			pci_read_config_byte(pdev, oft, buf + oft));
	}

	return ret;
}

/**
 * @brief Read pci class code
 * 
 * @param dev pci device
 * @return 0 on success, otherwise a negative errno.
 */
int pci_read_class_code(struct pci_dev *pdev, struct pci_class_code *data)
{
	int ret;
	u32 reg;

	ret = pci_read_config_dword(pdev, PCI_CLASS_REVISION, &reg);
	if (ret == PCIBIOS_SUCCESSFUL) {
		data->base = (reg >> 24) & 0xff;
		data->sub = (reg >> 16) & 0xff;
		data->prog = (reg >> 8) & 0xff;
	}
	
	return pcibios_err_to_errno(ret);
}

int pci_enable_int_pin(struct pci_dev *pdev)
{
	int ret;
	u16 val;

	ret = pci_read_config_word(pdev, PCI_COMMAND, &val);
	if (ret == PCIBIOS_SUCCESSFUL) {
		val &= ~PCI_COMMAND_INTX_DISABLE;
		ret = pci_write_config_word(pdev, PCI_COMMAND, val);
	}

	return pcibios_err_to_errno(ret);
}

int pci_disable_int_pin(struct pci_dev *pdev)
{
	int ret;
	u16 val;

	ret = pci_read_config_word(pdev, PCI_COMMAND, &val);
	if (ret == PCIBIOS_SUCCESSFUL) {
		val |= PCI_COMMAND_INTX_DISABLE;
		ret = pci_write_config_word(pdev, PCI_COMMAND, val);
	}

	return pcibios_err_to_errno(ret);
}

static void print_msi_cap(struct pci_dev *pdev, struct pci_cap_msi *cap)
{
	struct device *dev = &pdev->dev;

	dev_dbg(dev, "========== MSI Capablity(0x%02x) ==========\n", 
			cap->offset);
	dev_dbg(dev, "Message Control Register: 0x%04x\n", cap->mc);
	dev_dbg(dev, "\t Max Vector Support: %u\n",
			1 << ((cap->mc & PCI_MSI_FLAGS_QMASK) >> 1));
	dev_dbg(dev, "\t 64-bit Address Capable: %s\n",
		(cap->mc & PCI_MSI_FLAGS_64BIT) ? "true" : "false");
	dev_dbg(dev, "\t Per-Vector Masking Capable: %s\n", 
		(cap->mc & PCI_MSI_FLAGS_MASKBIT) ? "true" : "false");
	dev_dbg(dev, "\t Extended Message Data Capable: %s\n",
		(cap->mc & PCI_MSI_FLAGS_EXT_MSG_DATA_CAP) ? "true" : "false");
}

static void print_msix_cap(struct pci_dev *pdev, struct pci_cap_msix *cap)
{
	struct device *dev = &pdev->dev;

	dev_dbg(dev, "========== MSI-X Capablity(0x%02x) ==========\n",
			cap->offset);
	dev_dbg(dev, "Message Control Register: 0x%04x\n", cap->mc);
	dev_dbg(dev, "\t Max Vector Support: %u\n", 
			(cap->mc & PCI_MSIX_FLAGS_QSIZE) + 1);
	dev_dbg(dev, "Table Offset/BIR Register: 0x%08x\n", cap->table);
	dev_dbg(dev, "\t Table Offset: 0x%x\n", 
			cap->table & PCI_MSIX_TABLE_OFFSET);
	dev_dbg(dev, "\t Table BIR: %u\n", 
			cap->table & PCI_MSIX_TABLE_BIR);
	dev_dbg(dev, "PBA Offset/BIR Register: 0x%08x\n", cap->pba);
	dev_dbg(dev, "\t PBA Offset: 0x%x\n", cap->pba & PCI_MSIX_PBA_OFFSET);
	dev_dbg(dev, "\t PBA BIR: %u\n", cap->pba & PCI_MSIX_PBA_BIR);
}

static void print_pm_cap(struct pci_dev *pdev, struct pci_cap_pm *cap)
{
	struct device *dev = &pdev->dev;

	dev_dbg(dev, "========== Power Management Capablity(0x%02x) ==========\n",
			cap->offset);
	dev_dbg(dev, "Power Management Capability Register: 0x%04x\n", cap->pmc);
	dev_dbg(dev, "\t D1 Support: %s\n", 
		(cap->pmc & PCI_PM_CAP_D1) ? "true" : "false");
	dev_dbg(dev, "\t D2 Support: %s\n", 
		(cap->pmc & PCI_PM_CAP_D2) ? "true" : "false");
	dev_dbg(dev, "Power Management Control/Status Register: 0x%04x\n", 
		cap->pmcsr);
}

static void print_express_cap(struct pci_dev *pdev, struct pci_cap_express *cap)
{
	struct device *dev = &pdev->dev;

	dev_dbg(dev, "========== PCI Express Capablity(0x%02x) ==========\n", 
			cap->offset);
	dev_dbg(dev, "PCI Express Capability Register: 0x%08x\n", cap->exp_cap);
	dev_dbg(dev, "\t Device/Port Type: 0x%x\n", 
			(cap->exp_cap & PCI_EXP_FLAGS_TYPE) >> 4);
	dev_dbg(dev, "Device Capability Register: 0x%08x\n", cap->dev_cap);
	dev_dbg(dev, "\t Max Payload Size: %u\n",
			1 << ((cap->dev_cap & PCI_EXP_DEVCAP_PAYLOAD) + 7));
}

struct pci_cap_msi *pci_get_msi_cap(struct pci_dev *pdev, u8 offset)
{
	struct device *dev = &pdev->dev;
	struct pci_cap_msi *cap;
	int ret;

	if (!offset)
		return NULL;

	cap = kzalloc(sizeof(*cap), GFP_KERNEL);
	if (!cap)
		return NULL;

	cap->offset = offset;

	ret = pci_msi_read_mc(pdev, offset, &cap->mc);
	if (ret < 0) {
		dev_warn(dev, "msi_cap read is wrong!\n");
	}
	print_msi_cap(pdev, cap);

	return cap;
}

struct pci_cap_msix *pci_get_msix_cap(struct pci_dev *pdev, u16 offset)
{
	struct device *dev = &pdev->dev;
	struct pci_cap_msix *cap;
	int ret;

	if (!offset)
		return NULL;

	cap = kzalloc(sizeof(*cap), GFP_KERNEL);
	if (!cap)
		return NULL;

	cap->offset = offset;

	ret = pci_msix_read_mc(pdev, cap->offset, &cap->mc);
	ret |= pci_msix_read_table(pdev, cap->offset, &cap->table);
	ret |= pci_msix_read_pba(pdev, cap->offset, &cap->pba);
	if (ret < 0) {
		dev_warn(dev, "msix_cap read is wrong!\n");
	}
	print_msix_cap(pdev, cap);

	return cap;
}

struct pci_cap_pm *pci_get_pm_cap(struct pci_dev *pdev, u8 offset)
{
	struct device *dev = &pdev->dev;
	struct pci_cap_pm *cap;
	int ret;

	if (!offset)
		return NULL;

	cap = kzalloc(sizeof(*cap), GFP_KERNEL);
	if (!cap)
		return NULL;

	cap->offset = offset;

	ret = pcibios_err_to_errno(pci_read_config_word(pdev, 
					offset + PCI_PM_PMC, &cap->pmc));
	ret |= pcibios_err_to_errno(pci_read_config_word(pdev, 
					offset + PCI_PM_CTRL, &cap->pmcsr));
	if (ret < 0) {
		dev_warn(dev, "pm_cap read is wrong!\n");
	}
	print_pm_cap(pdev, cap);

	return cap;
}

struct pci_cap_express *pci_get_express_cap(struct pci_dev *pdev, u8 offset)
{
	struct device *dev = &pdev->dev;
	struct pci_cap_express *cap;
	int ret;

	if (!offset)
		return NULL;

	cap = kzalloc(sizeof(*cap), GFP_KERNEL);
	if (!cap)
		return NULL;

	cap->offset = offset;

	ret = pcibios_err_to_errno(pci_read_config_word(pdev, 
				offset + PCI_EXP_FLAGS, &cap->exp_cap));
	ret |= pcibios_err_to_errno(pci_read_config_dword(pdev, 
				offset + PCI_EXP_DEVCAP, &cap->dev_cap));
	if (ret < 0) {
		dev_warn(dev, "exp_cap read is wrong!\n");
	}
	print_express_cap(pdev, cap);

	return cap;
}

/*
 * NOTE: don't use @pcie_has_flr, system may not support this func in 
 *   different version!
 */
int pcie_do_flr_reset(struct pci_dev *pdev)
{
	u32 cap;

	pcie_capability_read_dword(pdev, PCI_EXP_DEVCAP, &cap);
	if (!(cap & PCI_EXP_DEVCAP_FLR))
		return -EOPNOTSUPP;

	pci_save_state(pdev);
	pcie_flr(pdev);
	pci_restore_state(pdev);
	return 0;
}

int pcie_do_hot_reset(struct pci_dev *pdev)
{
	struct pci_dev *bridge = pdev->bus->self;

	pci_save_state(pdev);
	pci_bridge_secondary_bus_reset(bridge);
	pci_restore_state(pdev);
	return 0;
}

static int pcie_set_link(struct pci_dev *pdev, bool enable)
{
	u16 lnk_ctrl;

	pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &lnk_ctrl);
	if (enable)
		lnk_ctrl &= ~PCI_EXP_LNKCTL_LD;
	else
		lnk_ctrl |= PCI_EXP_LNKCTL_LD;

	pcie_capability_write_word(pdev, PCI_EXP_LNKCTL, lnk_ctrl);
	return 0;
}

int pcie_do_linkdown_reset(struct pci_dev *pdev)
{
	pci_save_state(pdev);
	pcie_set_link(pdev, false);
	msleep(100);
	pcie_set_link(pdev, true);
	pci_restore_state(pdev);
	return 0;
}

