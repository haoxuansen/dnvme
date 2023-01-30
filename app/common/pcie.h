/**
 * @file pcie.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _APP_PCIE_H_
#define _APP_PCIE_H_

#include <stdint.h>

int pcie_is_support_flr(int fd, uint8_t pxcap);

int pcie_do_flr(int fd, uint8_t pxcap);
int pcie_do_hot_reset(int fd);
int pcie_do_link_down(int fd);

int pcie_set_power_state(int fd, uint8_t pmcap, uint16_t ps);

#endif /* !_APP_PCIE_H_ */
