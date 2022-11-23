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
#include <linux/pci_ids.h>

#include "dnvme_ioctl.h"

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

static inline int pci_read_vendor_id(const struct pci_dev *dev, u16 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_word(dev, PCI_VENDOR_ID, data));
}

static inline int pci_read_device_id(const struct pci_dev *dev, u16 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_word(dev, PCI_DEVICE_ID, data));
}

static inline int pci_read_command(const struct pci_dev *dev, u16 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_word(dev, PCI_COMMAND, data));
}

static inline int pci_read_status(const struct pci_dev *dev, u16 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_word(dev, PCI_STATUS, data));
}

static inline int pci_read_revision_id(const struct pci_dev *dev, u8 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_byte(dev, PCI_REVISION_ID, data));
}

static inline int pci_read_cache_line_size(const struct pci_dev *dev, u8 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_byte(dev, PCI_CACHE_LINE_SIZE, data));
}

static inline int pci_read_latency_timer(const struct pci_dev *dev, u8 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_byte(dev, PCI_LATENCY_TIMER, data));
}

static inline int pci_read_header_type(const struct pci_dev *dev, u8 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_byte(dev, PCI_HEADER_TYPE, data));
}

/**
 * @brief Read BIST(Built-In Self Test)
 */
static inline int pci_read_bist(const struct pci_dev *dev, u8 *data)
{
	return pcibios_err_to_errno(pci_read_config_byte(dev, PCI_BIST, data));
}

/**
 * @brief Read Capabilities Pointer
 */
static inline int pci_read_cap_ptr(const struct pci_dev *dev, u8 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_byte(dev, PCI_CAPABILITY_LIST, data));
}

static inline int pci_read_int_line(const struct pci_dev *dev, u8 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_byte(dev, PCI_INTERRUPT_LINE, data));
}

static inline int pci_read_int_pin(const struct pci_dev *dev, u8 *data)
{
	return pcibios_err_to_errno(
		pci_read_config_byte(dev, PCI_INTERRUPT_PIN, data));
}


int pci_read_class_code(const struct pci_dev *dev, struct pci_class_code *data);

int pci_get_capability(struct pci_dev *pdev, struct nvme_capability *cap);

void pci_parse_status(struct pci_dev *pdev, u16 status);

#endif /* !_DNVME_PCI_H_ */
