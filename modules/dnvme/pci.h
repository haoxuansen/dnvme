/**
 * @file pci.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-11-18
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _DNVME_PCI_H_
#define _DNVME_PCI_H_

#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/pci_ids.h>

#include "pci_caps.h"
#include "pci_regs_ext.h"
#include "dnvme_ioctl.h"

#define PCI_CAP_ID_MASK			0x00ff
#define PCI_CAP_NEXT_MASK		0xff00

#define PCI_MSI_VEC_MAX			32
#define PCI_MSIX_VEC_MAX		2048

/**
 * @base: Base Class Code
 * @sub: Sub-Class Code
 * @prog: Programing Interface
 */
struct pci_class_code {
	u8	base;
	u8	sub;
	u8	prog;
};

static inline int pci_read_vendor_id(struct pci_dev *pdev, u16 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_word(pdev, PCI_VENDOR_ID, data));
}

static inline int pci_read_device_id(struct pci_dev *pdev, u16 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_word(pdev, PCI_DEVICE_ID, data));
}

static inline int pci_read_command(struct pci_dev *pdev, u16 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_word(pdev, PCI_COMMAND, data));
}

static inline int pci_read_status(struct pci_dev *pdev, u16 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_word(pdev, PCI_STATUS, data));
}

static inline int pci_read_revision_id(struct pci_dev *pdev, u8 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_byte(pdev, PCI_REVISION_ID, data));
}

static inline int pci_read_cache_line_size(struct pci_dev *pdev, u8 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_byte(pdev, PCI_CACHE_LINE_SIZE, data));
}

static inline int pci_read_latency_timer(struct pci_dev *pdev, u8 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_byte(pdev, PCI_LATENCY_TIMER, data));
}

static inline int pci_read_header_type(struct pci_dev *pdev, u8 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_byte(pdev, PCI_HEADER_TYPE, data));
}

/**
 * @brief Read BIST(Built-In Self Test)
 */
static inline int pci_read_bist(struct pci_dev *pdev, u8 *data)
{
	return pcibios_err_to_errno(pci_read_config_byte(pdev, PCI_BIST, data));
}

/**
 * @brief Read Capabilities Pointer
 */
static inline int pci_read_cap_ptr(struct pci_dev *pdev, u8 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_byte(pdev, PCI_CAPABILITY_LIST, data));
}

static inline int pci_read_int_line(struct pci_dev *pdev, u8 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_byte(pdev, PCI_INTERRUPT_LINE, data));
}

static inline int pci_read_int_pin(struct pci_dev *pdev, u8 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_byte(pdev, PCI_INTERRUPT_PIN, data));
}

static inline int pci_msi_read_mc(struct pci_dev *pdev, u32 oft, u16 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_word(pdev, oft + PCI_MSI_FLAGS, data));
}

static inline int pci_msix_read_mc(struct pci_dev *pdev, u32 oft, u16 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_word(pdev, oft + PCI_MSIX_FLAGS, data));
}

static inline int pci_msix_read_table(struct pci_dev *pdev, u32 oft, u32 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_dword(pdev, oft + PCI_MSIX_TABLE, data));
}

static inline int pci_msix_read_pba(struct pci_dev *pdev, u32 oft, u32 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_dword(pdev, oft + PCI_MSIX_PBA, data));
}

int pci_read_cfg_data(struct pci_dev *pdev, void *buf, u32 len);
int pci_read_class_code(struct pci_dev *pdev, struct pci_class_code *data);

int pci_enable_int_pin(struct pci_dev *pdev);
int pci_disable_int_pin(struct pci_dev *pdev);

struct pci_cap_msi *pci_get_msi_cap(struct pci_dev *pdev, u8 offset);
struct pci_cap_msix *pci_get_msix_cap(struct pci_dev *pdev, u16 offset);
struct pci_cap_pm *pci_get_pm_cap(struct pci_dev *pdev, u8 offset);
struct pci_cap_express *pci_get_express_cap(struct pci_dev *pdev, u8 offset);

int pcie_do_flr_reset(struct pci_dev *pdev);
int pcie_do_hot_reset(struct pci_dev *pdev);
int pcie_do_linkdown_reset(struct pci_dev *pdev);

#endif /* !_DNVME_PCI_H_ */

