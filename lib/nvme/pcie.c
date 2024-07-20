/**
 * @file pcie.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "pci_regs_ext.h"
#include "libbase.h"
#include "libnvme.h"

int pcie_set_link_speed(char *lc2, int speed)
{
	char cmd[128] = {0};
	int ret;

	if (speed < 0 || speed > 6) {
		pr_err("ERR: speed %d is invalid!\n", speed);
		return -EINVAL;
	}

	snprintf(cmd, sizeof(cmd), "setpci -s %s.b=%d:f", lc2, speed);

	ret = call_system(cmd);
	if (ret < 0)
		return ret;

	return 0;
}

int pcie_retrain_link(char *lc)
{
	int ret;
	char cmd[128] = {0};

	snprintf(cmd, sizeof(cmd), "setpci -s %s.b=20:20", lc);

	ret = call_system(cmd);
	if (ret < 0)
		return ret;

	msleep(100);
	return 0;
}

int pcie_check_link_speed_width(int fd, uint8_t expcap, int speed, int width)
{
	uint16_t status;

	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_link_status(fd, expcap, &status), -EPERM);
	
	if (speed && (status & PCI_EXP_LNKSTA_CLS) != speed)
		goto err;
	if (width && ((status & PCI_EXP_LNKSTA_NLW) >> 4) != width)
		goto err;

	return 0;
err:
	pr_err("ERR: Cur link_sts 0x%x; Expect speed %d width %d\n", 
		status, speed, width);
	return -EPERM;
}

int pcie_get_link_speed_width(int fd, uint8_t expcap, int *speed, int *width)
{
	uint16_t status;

	CHK_EXPR_NUM_LT0_RTN(
		pci_exp_read_link_status(fd, expcap, &status), -EPERM);
	
	*speed = status & PCI_EXP_LNKSTA_CLS;
	*width = (status & PCI_EXP_LNKSTA_NLW) >> 4;
	return 0;
}

/**
 * @return 0 on success, otherwise a negative errno.
 */
int pcie_set_power_state(int fd, uint8_t pmcap, uint16_t ps)
{
	uint16_t pmcsr;

	CHK_EXPR_NUM_LT0_RTN(
		pci_pm_read_control_status(fd, pmcap, &pmcsr), -EPERM);
	
	pmcsr &= ~PCI_PM_CTRL_STATE_MASK;
	pmcsr |= (ps & PCI_PM_CTRL_STATE_MASK);

	CHK_EXPR_NUM_LT0_RTN(
		pci_pm_write_control_status(fd, pmcap, pmcsr), -EPERM);
	
	msleep(10); /* wait ctrlr ready */

	CHK_EXPR_NUM_LT0_RTN(
		pci_pm_read_control_status(fd, pmcap, &pmcsr), -EPERM);
	
	if ((pmcsr & PCI_PM_CTRL_STATE_MASK) != (ps & PCI_PM_CTRL_STATE_MASK)) {
		pr_err("expect power state: %u, current power state: %u\n",
			ps & PCI_PM_CTRL_STATE_MASK,
			pmcsr & PCI_PM_CTRL_STATE_MASK);
		return -EPERM;
	}

	return 0;
}