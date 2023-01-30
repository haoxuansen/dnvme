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

#include "pci.h"

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

static const char *pci_cap_string(u8 cap_id)
{
	switch (cap_id) {
	case PCI_CAP_ID_PM:
		return "Power Management";
	case PCI_CAP_ID_AGP:
		return "Accelerated Graphics Port";
	case PCI_CAP_ID_VPD:
		return "Vital Product Data";
	case PCI_CAP_ID_SLOTID:
		return "Slot Identification";
	case PCI_CAP_ID_MSI:
		return "Message Signalled Interrupts";
	case PCI_CAP_ID_CHSWP:
		return "CompactPCI HotSwap";
	case PCI_CAP_ID_PCIX:
		return "PCI-X";
	case PCI_CAP_ID_HT:
		return "HyperTransport";
	case PCI_CAP_ID_VNDR:
		return "Vendor-Specific";
	case PCI_CAP_ID_DBG:
		return "Debug port";
	case PCI_CAP_ID_CCRC:
		return "CompactPCI Central Resource Control";
	case PCI_CAP_ID_SHPC:
		return "PCI Standard Hot-Plug Controller";
	case PCI_CAP_ID_SSVID:
		return "Bridge subsystem vendor/device ID";
	case PCI_CAP_ID_AGP3:
		return "AGP Target PCI-PCI bridge";
	case PCI_CAP_ID_SECDEV:
		return "Secure Device";
	case PCI_CAP_ID_EXP:
		return "PCI Express";
	case PCI_CAP_ID_MSIX:
		return "MSI-X";
	case PCI_CAP_ID_SATA:
		return "SATA Data/Index Conf";
	case PCI_CAP_ID_AF:
		return "PCI Advanced Features";
	case PCI_CAP_ID_EA:
		return "PCI Enhanced Allocation";
	default:
		return "Unknown";
	}
}

static const char *pci_ext_cap_string(u16 cap_id)
{
	switch (cap_id) {
	case PCI_EXT_CAP_ID_ERR:
		return "Advanced Error Reporting";
	case PCI_EXT_CAP_ID_VC:
		return "Virtual Channel Capability";
	case PCI_EXT_CAP_ID_DSN:
		return "Device Serial Number";
	case PCI_EXT_CAP_ID_PWR:
		return "Power Budgeting";
	case PCI_EXT_CAP_ID_RCLD:
		return "Root Complex Link Declaration";
	case PCI_EXT_CAP_ID_RCILC:
		return "Root Complex Internal Link Control";
	case PCI_EXT_CAP_ID_RCEC:
		return "Root Complex Event Collector";
	case PCI_EXT_CAP_ID_MFVC:
		return "Multi-Function VC Capability";
	case PCI_EXT_CAP_ID_VC9:
		return "Virtual Channel Capability";
	case PCI_EXT_CAP_ID_RCRB:
		return "Root Complex RB";
	case PCI_EXT_CAP_ID_VNDR:
		return "Vendor-Specific";
	case PCI_EXT_CAP_ID_CAC:
		return "Config Access - obsolete";
	case PCI_EXT_CAP_ID_ACS:
		return "Access Control Services";
	case PCI_EXT_CAP_ID_ARI:
		return "Alternate Routing ID";
	case PCI_EXT_CAP_ID_ATS:
		return "Address Translation Services";
	case PCI_EXT_CAP_ID_SRIOV:
		return "Single Root I/O Virtualization";
	case PCI_EXT_CAP_ID_MRIOV:
		return "Multi Root I/O Virtualization";
	case PCI_EXT_CAP_ID_MCAST:
		return "Multicast";
	case PCI_EXT_CAP_ID_PRI:
		return "Page Request Interface";
	case PCI_EXT_CAP_ID_AMD_XXX:
		return "Reserved for AMD";
	case PCI_EXT_CAP_ID_REBAR:
		return "Resizable BAR";
	case PCI_EXT_CAP_ID_DPA:
		return "Dynamic Power Allocation";
	case PCI_EXT_CAP_ID_TPH:
		return "TPH Requester";
	case PCI_EXT_CAP_ID_LTR:
		return "Latency Tolerance Reporting";
	case PCI_EXT_CAP_ID_SECPCI:
		return "Secondary PCIe Capability";
	case PCI_EXT_CAP_ID_PMUX:
		return "Protocol Multiplexing";
	case PCI_EXT_CAP_ID_PASID:
		return "Process Address Space ID";
	case PCI_EXT_CAP_ID_DPC:
		return "Downstream Port Containment";
	case PCI_EXT_CAP_ID_L1SS:
		return "L1 PM Substates";
	case PCI_EXT_CAP_ID_PTM:
		return "Precision Time Measurement";
	case PCI_EXT_CAP_ID_DLF:
		return "Data Link Feature";
	case PCI_EXT_CAP_ID_PL_16GT:
		return "Physical Layer 16.0 GT/s";
	default:
		return "Unknown";
	}
}

static int pci_parse_msix_cap(struct pci_dev *pdev, struct pci_cap *cap)
{
	struct device *dev = &pdev->dev;
	struct pci_msix_cap *msix;
	int ret;

	msix = kzalloc(sizeof(*msix), GFP_KERNEL);
	if (!msix) {
		dev_err(dev, "failed to alloc pci_msi_cap!\n");
		return -ENOMEM;
	}

	ret = pci_msix_read_mc(pdev, cap->offset, &msix->mc);
	ret |= pci_msix_read_table(pdev, cap->offset, &msix->table);
	ret |= pci_msix_read_pba(pdev, cap->offset, &msix->pba);
	if (ret < 0) {
		dev_err(dev, "failed to read msix capability!\n");
		goto out;
	}
	dev_dbg(dev, "Message Control Register: 0x%x\n", msix->mc);
	dev_dbg(dev, "\tTable Size: %u\n", msix->mc & PCI_MSIX_FLAGS_QSIZE);
	dev_dbg(dev, "Table Offset/BIR Register: 0x%x\n", msix->table);
	dev_dbg(dev, "PBA Offset/BIR Register: 0x%x\n", msix->pba);

	cap->data = msix;
	return 0;
out:
	kfree(msix);
	return ret;
}

static int pci_parse_cap(struct pci_dev *pdev, struct pci_cap *cap)
{
	int ret = 0;

	switch (cap->id) {
	case PCI_CAP_ID_MSIX:
		ret = pci_parse_msix_cap(pdev, cap);
		break;
	default:
		break;
	}

	return ret;
}

int pci_get_caps(struct pci_dev *pdev, struct pci_cap *caps)
{
	int ret;
	struct device *dev = &pdev->dev;
	u16 capinfo;
	u8 now, next, id;
	
	ret = pci_read_cap_ptr(pdev, &now);
	if (ret < 0) {
		dev_err(dev, "failed to read cap ptr!(%d)\n", ret);
		return ret;
	}

	do {
		ret = pcibios_err_to_errno(
			pci_read_config_word(pdev, now, &capinfo));
		if (ret < 0) {
			dev_err(dev, "failed to read config offset(%u)!(%d)\n", 
				next, ret);
			return ret;
		}
		id = capinfo & 0xff;
		next = (capinfo >> 8) & 0xff;

		if (id > PCI_CAP_ID_MAX || id == 0) {
			dev_warn(dev, "ID(0x%02x) is invalid! Stop parsing "
				"cap list!\n", id);
			break;
		}

		dev_dbg(dev, "Support PCI Cap 0x%02x with Offset 0x%02x: %s\n",
			id, now, pci_cap_string(id));
		caps[id - 1].id = id;
		caps[id - 1].offset = now;

		ret = pci_parse_cap(pdev, &caps[id - 1]);
		if (ret < 0)
			return ret;

		now = next;
	} while (next < PCI_CFG_SPACE_SIZE);

	return 0;
}

void pci_put_caps(struct pci_dev *pdev, struct pci_cap *caps)
{
	int i;

	for (i = 0; i < PCI_CAP_ID_MAX; i++) {
		if (caps[i].id && caps[i].data) {
			kfree(caps[i].data);
			caps[i].data = NULL;
		}
	}
}

int pci_get_ext_caps(struct pci_dev *pdev, struct pcie_cap *caps)
{
	struct device *dev = &pdev->dev;
	u32 header;
	u16 now, next, id;
	int ret;

	/*
	 * PCI Express Extended Capabilities start at base of extended
	 * extended configuration region.
	 */
	now = PCI_CFG_SPACE_SIZE;
	do {
		ret = pcibios_err_to_errno(
			pci_read_config_dword(pdev, now, &header));
		if (ret < 0) {
			dev_err(dev, "failed to read config offset(%u)!(%d)\n", 
				next, ret);
			return ret;
		}

		id = PCI_EXT_CAP_ID(header);
		next = PCI_EXT_CAP_NEXT(header);

		if (id > PCI_EXT_CAP_ID_MAX || id == 0) {
			dev_warn(dev, "ID(0x%04x) is invalid! Stop parsing "
				"ext cap list!\n", id);
			break;
		}

		dev_dbg(dev, "Support PCIe Cap 0x%04x with offset 0x%04x: %s\n",
			id, now, pci_ext_cap_string(id));
		caps[id - 1].id = id;
		caps[id - 1].version = PCI_EXT_CAP_VER(header);
		caps[id - 1].offset = now;

		now = next;
	} while (next > PCI_CFG_SPACE_SIZE && next < PCI_CFG_SPACE_EXP_SIZE);

	return 0;
}

void pci_put_ext_caps(struct pci_dev *pdev, struct pcie_cap *caps)
{
	int i;

	for (i = 0; i < PCI_EXT_CAP_ID_MAX; i++) {
		if (caps[i].id && caps[i].data) {
			kfree(caps[i].data);
			caps[i].data = NULL;
		}
	}
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

