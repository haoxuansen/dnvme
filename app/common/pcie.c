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

#include <stdlib.h>
#include <errno.h>

#include "pci_regs_ext.h"
#include "log.h"

#include "core.h"
#include "pci.h"
#include "pcie.h"

int pcie_retrain_link(void)
{
	int ret;

	ret = call_system("setpci -s " RC_CAP_LINK_CONTROL ".b=20:20");
	if (ret < 0)
		return ret;

	msleep(100);
	return 0;
}

/**
 * @brief Check whether function level reset is supported
 * 
 * @param pxcap PCI Express Capability offset
 * @return 1 if support FLR, 0 if not support FLR, otherwise a negative errno.
 */
int pcie_is_support_flr(int fd, uint8_t pxcap)
{
	uint32_t devcap;
	int ret;

	ret = pci_exp_read_device_capability(fd, pxcap, &devcap);
	if (ret < 0)
		return ret;

	return (devcap & PCI_EXP_DEVCAP_FLR) ? 1 : 0;
}

/**
 * @brief Do function level reset
 * 
 * @param pxcap PCI Express Capability offset
 * @return 0 on success, otherwise a negative errno.
 */
int pcie_do_flr(int fd, uint8_t pxcap)
{
	uint32_t bar[6];
	uint16_t devctrl;
	uint16_t cmd, cmd_bkup;
	int ret;

	/* backup register data */
	ret = pci_read_config_data(fd, PCI_BASE_ADDRESS_0, sizeof(bar), bar);
	if (ret < 0)
		return ret;
	
	ret = pci_hd_read_command(fd, &cmd);
	if (ret < 0)
		return ret;
	cmd_bkup = cmd & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
	
	/* do function level reset */
	ret = pci_exp_read_device_control(fd, pxcap, &devctrl);
	if (ret < 0)
		return ret;

	devctrl |= PCI_EXP_DEVCTL_BCR_FLR;
	ret = pci_exp_write_device_control(fd, pxcap, devctrl);
	if (ret < 0)
		return ret;

	/*
	 * Refer to PCIe Base Spec R5.0 Ch6.6.2:
	 *   After an FLR has been initiated by writing a 1b to the Initiate
	 * Function Level Reset bit, the Function must complete the FLR 
	 * within 100ms.
	 */
	msleep(100);

	/* restore register data */
	ret = pci_hd_read_command(fd, &cmd);
	if (ret < 0)
		return ret;

	cmd &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
	cmd |= cmd_bkup;
	ret = pci_hd_write_command(fd, cmd);
	if (ret < 0)
		return ret;

	ret = pci_write_config_data(fd, PCI_BASE_ADDRESS_0, sizeof(bar), bar);
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * @return 0 on success, otherwise a negative errno.
 */
int pcie_do_hot_reset(int fd)
{
	uint32_t bar[6];
	uint16_t cmd, bkup;
	int ret;

	/* backup register data */
	ret = pci_read_config_data(fd, PCI_BASE_ADDRESS_0, sizeof(bar), bar);
	if (ret < 0)
		return ret;

	ret = pci_hd_read_command(fd, &cmd);
	if (ret < 0)
		return ret;
	bkup = cmd & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);

	/* do hot reset */
	ret = call_system("setpci -s " RC_BRIDGE_CONTROL ".b=40:40");
	if (ret < 0)
		return ret;
	msleep(50);
	
	ret = call_system("setpci -s " RC_BRIDGE_CONTROL ".b=00:40");
	if (ret < 0)
		return ret;
	msleep(50);

	/* restore register data */
	ret = pci_hd_read_command(fd, &cmd);
	if (ret < 0)
		return ret;

	cmd &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
	cmd |= bkup;
	ret = pci_hd_write_command(fd, cmd);
	if (ret < 0)
		return ret;

	ret = pci_write_config_data(fd, PCI_BASE_ADDRESS_0, sizeof(bar), bar);
	if (ret < 0)
		return ret;

	ret = pcie_retrain_link();
	if (ret < 0)
		return ret;

	return 0;
}

/**
 * @return 0 on success, otherwise a negative errno.
 */
int pcie_do_link_down(int fd)
{
	uint32_t bar[6];
	uint16_t cmd, bkup;
	int ret;

	/* backup register data */
	ret = pci_read_config_data(fd, PCI_BASE_ADDRESS_0, sizeof(bar), bar);
	if (ret < 0)
		return ret;

	ret = pci_hd_read_command(fd, &cmd);
	if (ret < 0)
		return ret;
	bkup = cmd & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);

	/* do hot reset */
	ret = call_system("setpci -s " RC_CAP_LINK_CONTROL ".b=10:10");
	if (ret < 0)
		return ret;
	msleep(100);
	
	ret = call_system("setpci -s " RC_CAP_LINK_CONTROL ".b=00:10");
	if (ret < 0)
		return ret;
	msleep(100);

	/* restore register data */
	ret = pci_hd_read_command(fd, &cmd);
	if (ret < 0)
		return ret;

	cmd &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
	cmd |= bkup;
	ret = pci_hd_write_command(fd, cmd);
	if (ret < 0)
		return ret;

	ret = pci_write_config_data(fd, PCI_BASE_ADDRESS_0, sizeof(bar), bar);
	if (ret < 0)
		return ret;

	ret = pcie_retrain_link();
	if (ret < 0)
		return ret;

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