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

#include "dnvme_ioctl.h"
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

#endif /* !_APP_PCI_H_ */
