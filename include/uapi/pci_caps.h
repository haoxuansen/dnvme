/**
 * @file pci_caps.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-03-20
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UAPI_PCI_CAPS_H_
#define _UAPI_PCI_CAPS_H_

/**
 * @brief 
 * 
 * @pmc: Power Management Capabilities Register
 * @pmcsr: Power Management Control/Status Register
 */
struct pci_cap_pm {
	uint8_t		offset;
	uint16_t	pmc;
	uint16_t	pmcsr;
};

struct pci_cap_msi {
	uint8_t		offset;
	uint16_t	mc;
};

struct pci_cap_express {
	uint8_t		offset;
	uint16_t	exp_cap;
	uint32_t	dev_cap;
};

/**
 * @brief 
 * 
 * @mc: Message Control Register
 * @table: Table Offset/Table BIR Register
 * @pba: PBA Offset/PBA BIR Register
 */
struct pci_cap_msix {
	uint16_t	offset;
	uint16_t	mc;
	uint32_t	table;
	uint32_t	pba;
};

#endif /* !_UAPI_PCI_CAPS_H_ */
