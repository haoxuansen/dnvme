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

#include <linux/kernel.h>

#include "pci.h"

/**
 * @brief Read pci class code
 * 
 * @param dev pci device
 * @return 0 on success, otherwise a negative errno.
 */
int pci_read_class_code(const struct pci_dev *dev, struct pci_class_code *data)
{
	int ret;
	u32 reg;

	ret = pci_read_config_dword(dev, PCI_CLASS_REVISION, &reg);
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

/**
 * @brief Get pci capability
 * 
 * @param pdev pci device
 * @param cap Each bit represents a capability supported by pci device.
 * @return 0 on success, otherwise a negative errno.
 */
int pci_get_capability(struct pci_dev *pdev, struct nvme_capability *cap)
{
	int ret;
	struct device *dev = &pdev->dev;
	u16 capinfo;
	u8 now, next, id;

	ret = pci_read_cap_ptr(pdev, &now);
	if (ret < 0)
		return ret;

	do {
		ret = pcibios_err_to_errno(
			pci_read_config_word(pdev, now, &capinfo));
		if (ret < 0)
		{
			dev_err(dev, "failed to read config offset(%u)!(%d)\n", 
				next, ret);
			return ret;
		}
		id = capinfo & 0xff;
		next = (capinfo >> 8) & 0xff;

		switch (id) {
		case PCI_CAP_ID_PM:
			dev_dbg(dev, "Support PCI Power Management!\n");
			set_bit(PCI_CAP_SUPPORT_PM, cap->pci_cap_support);
			cap->pci_cap_offset[PCI_CAP_SUPPORT_PM] = now;
			break;

		case PCI_CAP_ID_AGP:
		case PCI_CAP_ID_VPD:
		case PCI_CAP_ID_SLOTID:
			break;

		case PCI_CAP_ID_MSI:
			dev_dbg(dev, "Support Message Signaled Interrupts!\n");
			set_bit(PCI_CAP_SUPPORT_MSI, cap->pci_cap_support);
			cap->pci_cap_offset[PCI_CAP_SUPPORT_MSI] = now;
			break;

		case PCI_CAP_ID_CHSWP:
		case PCI_CAP_ID_PCIX:
		case PCI_CAP_ID_HT:
		case PCI_CAP_ID_VNDR:
		case PCI_CAP_ID_DBG:
		case PCI_CAP_ID_CCRC:
		case PCI_CAP_ID_SHPC:
		case PCI_CAP_ID_SSVID:
		case PCI_CAP_ID_AGP3:
		case PCI_CAP_ID_SECDEV:
		case PCI_CAP_ID_EXP:
			dev_dbg(dev, "Support PCI Express!\n");
			set_bit(PCI_CAP_SUPPORT_PCIE, cap->pci_cap_support);
			cap->pci_cap_offset[PCI_CAP_SUPPORT_PCIE] = now;
			break;

		case PCI_CAP_ID_MSIX:
			dev_dbg(dev, "Support MSI-X!\n");
			set_bit(PCI_CAP_SUPPORT_MSIX, cap->pci_cap_support);
			cap->pci_cap_offset[PCI_CAP_SUPPORT_MSIX] = now;
			break;

		case PCI_CAP_ID_SATA:
		case PCI_CAP_ID_AF:
		case PCI_CAP_ID_EA:
			break;
		default:
			dev_warn(dev, "cap id(0x%x) is unknown!\n", id);
		}
		now = next; /* next capacibility offset */

	} while (id != PCI_CAP_LIST_ID && next);

	return 0;
}

void pci_parse_status(struct pci_dev *pdev, u16 status)
{
	struct device *dev = &pdev->dev;

	if (status & PCI_STATUS_DETECTED_PARITY)
		dev_err(dev, "Detected Parity Error!\n");
	if (status & PCI_STATUS_SIG_SYSTEM_ERROR)
		dev_err(dev, "Signaled System Error!\n");
	if (status & PCI_STATUS_REC_MASTER_ABORT)
		dev_err(dev, "Received Master Abort!\n");
	if (status & PCI_STATUS_REC_TARGET_ABORT)
		dev_err(dev, "Received Target Abort!\n");
	if (status & PCI_STATUS_SIG_TARGET_ABORT)
		dev_err(dev, "Signaled Target Abort!\n");
	if (status & PCI_STATUS_PARITY)
		dev_err(dev, "Master Data Parity Error!\n");
	
	if (status & PCI_STATUS_CAP_LIST)
		dev_warn(dev, "Extended Capability list item Not Exist!\n");
}
