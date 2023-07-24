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

int pcie_retrain_link(char *lc)
{
	int ret;
	static char cmd[128] = {0};

	snprintf(cmd, sizeof(cmd), "setpci -s %s.b=20:20", lc);

	ret = call_system(cmd);
	if (ret < 0)
		return ret;

	msleep(100);
	return 0;
}

/**
 * @return 0 on success, otherwise a negative errno.
 */
int pcie_set_power_state(int fd, uint8_t pmcap, uint16_t ps)
{
	uint16_t pmcsr;
	int ret;

	ret = pci_pm_read_control_status(fd, pmcap, &pmcsr);
	if (ret < 0)
		return ret;
	
	pmcsr &= ~PCI_PM_CTRL_STATE_MASK;
	pmcsr |= (ps & PCI_PM_CTRL_STATE_MASK);

	ret = pci_pm_write_control_status(fd, pmcap, pmcsr);
	if (ret < 0)
		return ret;
	
	msleep(10); /* wait ctrlr ready */

	ret = pci_pm_read_control_status(fd, pmcap, &pmcsr);
	if (ret < 0)
		return ret;
	
	if ((pmcsr & PCI_PM_CTRL_STATE_MASK) != (ps & PCI_PM_CTRL_STATE_MASK)) {
		pr_err("expect power state: %u, current power state: %u\n",
			ps & PCI_PM_CTRL_STATE_MASK,
			pmcsr & PCI_PM_CTRL_STATE_MASK);
		return -EPERM;
	}

	return 0;
}