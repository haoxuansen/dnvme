/**
 * @file pci.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-15
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _APP_PCI_H_
#define _APP_PCI_H_

#include <stdint.h>

#include "dnvme.h"
#include "ioctl.h"

static inline int pci_read_config_data(int fd, uint32_t oft, uint32_t len, void *buf)
{
	return nvme_read_generic(fd, NVME_PCI_CONFIG, oft, len, buf);
}

static inline int pci_read_config_byte(int fd, uint32_t oft, uint8_t *val)
{
	return pci_read_config_data(fd, oft, 1, val);
}

static inline int pci_read_config_word(int fd, uint32_t oft, uint16_t *val)
{
	return pci_read_config_data(fd, oft, 2, val);
}

static inline int pci_read_config_dword(int fd, uint32_t oft, uint32_t *val)
{
	return pci_read_config_data(fd, oft, 4, val);
}

static inline int pci_write_config_data(int fd, uint32_t oft, uint32_t len, void *buf)
{
	return nvme_write_generic(fd, NVME_PCI_CONFIG, oft, len, buf);
}

static inline int pci_write_config_byte(int fd, uint32_t oft, uint8_t val)
{
	return pci_write_config_data(fd, oft, 1, &val);
}

static inline int pci_write_config_word(int fd, uint32_t oft, uint16_t val)
{
	return pci_write_config_data(fd, oft, 2, &val);
}

static inline int pci_write_config_dword(int fd, uint32_t oft, uint32_t val)
{
	return pci_write_config_data(fd, oft, 4, &val);
}

/* ==================== PCI Header ==================== */

static inline int pci_hd_read_command(int fd, uint16_t *val)
{
	return pci_read_config_word(fd, PCI_COMMAND, val);
}

static inline int pci_hd_write_command(int fd, uint16_t val)
{
	return pci_write_config_word(fd, PCI_COMMAND, val);
}

/* ==================== Cap: Power Management ==================== */

static inline int pci_pm_read_capability(int fd, uint8_t oft, uint16_t *val)
{
	return pci_read_config_word(fd, oft + PCI_PM_PMC, val);
}

static inline int pci_pm_read_control_status(int fd, uint8_t oft, uint16_t *val)
{
	return pci_read_config_word(fd, oft + PCI_PM_CTRL, val);
}

static inline int pci_pm_write_control_status(int fd, uint8_t oft, uint16_t val)
{
	return pci_write_config_word(fd, oft + PCI_PM_CTRL, val);
}

/* ==================== Cap: PCI Express ==================== */

static inline int pci_exp_read_device_capability(int fd, uint8_t oft, uint32_t *val)
{
	return pci_read_config_dword(fd, oft + PCI_EXP_DEVCAP, val);
}

static inline int pci_exp_read_device_control(int fd, uint8_t oft, uint16_t *val)
{
	return pci_read_config_word(fd, oft + PCI_EXP_DEVCTL, val);
}

static inline int pci_exp_write_device_control(int fd, uint8_t oft, uint16_t val)
{
	return pci_write_config_word(fd, oft + PCI_EXP_DEVCTL, val);
}

static inline int pci_exp_read_device_state(int fd, uint8_t oft, uint16_t *val)
{
	return pci_read_config_word(fd, oft + PCI_EXP_DEVSTA, val);
}

static inline int pci_exp_read_link_capability(int fd, uint8_t oft, uint32_t *val)
{
	return pci_read_config_dword(fd, oft + PCI_EXP_LNKCAP, val);
}

static inline int pci_exp_read_link_control(int fd, uint8_t oft, uint16_t *val)
{
	return pci_read_config_word(fd, oft + PCI_EXP_LNKCTL, val);
}

static inline int pci_exp_read_link_status(int fd, uint8_t oft, uint16_t *val)
{
	return pci_read_config_word(fd, oft + PCI_EXP_LNKSTA, val);
}

#endif /* !_APP_PCI_H_ */
