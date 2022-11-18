/*
 * NVM Express Compliance Suite
 * Copyright (c) 2011, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>

#include "dnvme_sts_chk.h"
#include "dnvme_queue.h"
#include "definitions.h"
#include "core.h"
#include "dnvme_reg.h"


/*
 *  pci_status_chk  - PCI device status check function
 *  which checks error registers and set kernel
 *  alert if a error is detected.
 */
int pci_status_chk(u16 device_data)
{
    int status = 0;

    pr_debug("PCI Device Status (STS) Data = 0x%X", device_data);
    pr_debug("Checking all the PCI register error bits");
    if(device_data & DEV_ERR_MASK) 
	{
        if(device_data & DPE) 
		{
            pr_err("Device Status - DPE Set");
            pr_err("Detected Data parity Error");
        }
		
        if(device_data & DPD) 
		{
            pr_err("Device Status - DPD Set");
            pr_err("Detected Master Data Parity Error");
        }
		
        if(device_data & RMA) 
		{
            pr_err("Device Status - RMA Set");
            pr_err("Received Master Abort...");
        }
		
        if(device_data & RTA) 
		{
            pr_err("Device Status - RTA Set");
            pr_err("Received Target Abort...");
        }
		status = -1;
    }

    if((device_data & CAP_LIST_BIT_MASK) == 0) 
	{
        pr_err("In STS, the CL bit indicates empty Capabilites list.");
        pr_err("Empty Capabilites list: The controller should support PCI Power Mnmt as a min.");
        status = -1;
    }

    return status;
}


/*
 * nvme_controller_status - This function checks the controller status
 */
int nvme_controller_status(struct nvme_ctrl_reg __iomem *ctrlr_regs, u32 *sts)
{
    int status = 0;
    u32 u32data;

    pr_debug("Checking the NVME Controller Status (CSTS)...");
	
    u32data = readl(&ctrlr_regs->csts);
	*sts = u32data;
	pr_debug("NVME Controller Status CSTS = 0x%X", u32data);

	if(u32data & NVME_CSTS_CFS_BIT_MASK) 
	{
        status = -1;
        pr_err("NVME Controller Fatal Status (CFS) is set...");
    }

	if((u32data & NVME_CSTS_RDY_BIT_MASK) == 0x0)
	{
        pr_debug("NVME Controller is not ready (RDY)...");
    }

	pr_debug("The Shutdown Status of the NVME Controller (SHST):");
    switch((u32data & NVME_CSTS_SHUTDOWN_BIT_MASK) >> 2)
	{
	    case NVME_CSTS_NRML_OPER:
	        pr_debug("No Shutdown requested");
	        break;
	    case NVME_CSTS_SHT_OCC:
	        pr_debug("Shutdown Processing occurring");
	        break;
	    case NVME_CSTS_SHT_COMP:
	        pr_debug("Shutdown Process Complete");
	        break;
	    case NVME_CSTS_SHT_RSVD:
	        pr_debug("Reserved Bits set");
	        break;
    }

    return status;
}


/*
 * device_status_next  - This function will check if the NVME device supports
 * NEXT capability item in the linked list. If the device supports the NEXT
 * capabilty then it goes into each of the status registers and checks the
 * device current state. It reports back to the caller wither 0 or -1.
 * Print out to the kernel message details of the status.
 */
int pcie_cap_chk(struct pci_dev *pdev, u16 *cap_support, u16 *pm_cs, u16 *msi_mc, u16 *msix_mc, 
					 u16 *pcie_dev_st)
{
    int status     = 0;
    int ret_val   = 0;
    u16 reg_cap_id    = 0;
    u8 next_item;
    u8 power_management_feature = 0;

    pr_debug("Checking NEXT Capabilities of the NVME Controller");
    pr_debug("Checks if PMCS is supported as a minimum");

    /* Check if CAP pointer points to next available linked list registers in the PCI Header. */
    ret_val = pci_read_config_byte(pdev, CAP_REG, &next_item);
    if(ret_val<0) 
	{
        pr_err("pci_read_config failed in driver error check");
    }
    pr_debug("CAP_REG Contents = 0x%X", next_item);

    /* Read 16 bits of data from the Next pointer as PMS bit which is must */
    ret_val = pci_read_config_word(pdev, next_item, &reg_cap_id);
    if(ret_val<0) 
	{
        pr_err("pci_read_config failed in driver error check");
    }

    /* Enter into loop if not zero value, next_item is set to 1 for entering this loop for first time.*/
    while(reg_cap_id || next_item) 
	{
        pr_debug("CAP Value 16 Bits = 0x%X", reg_cap_id);

        switch(reg_cap_id & CAP_ID_MASK) 
		{
	    	case PMCAP_ID:
	        	pr_debug("PCI Pwr Mgmt is Supported (PMCS Exists)");
	            pr_debug("Checking PCI Pwr Mgmt Capabilities Status");

	            power_management_feature = 1; 		/* Set power management is supported */
				*cap_support |= PCI_CAP_SUPPORT_PM;
				status = pmcs_status_chk(pdev, next_item, pm_cs);
	            break;
					
	        case MSICAP_ID:
	            pr_debug("Checking MSI Capabilities");
				*cap_support |= PCI_CAP_SUPPORT_MSI;
	            status = msi_cap_status_chk(pdev, next_item, msi_mc);
	            break;
					
	        case MSIXCAP_ID:
	            pr_debug("Checking MSI-X Capabilities");
				*cap_support |= PCI_CAP_SUPPORT_MSIX;
	            status = msix_cap_status_chk(pdev, next_item, msix_mc);
	            break;
					
	        case PXCAP_ID:
	            pr_debug("Checking PCI Express Capabilities");
				*cap_support |= PCI_CAP_SUPPORT_PCIE;
	            status = pxcap_status_chk(pdev, next_item, pcie_dev_st);
	            break;
					
	        default:
	            pr_err("Next Device Status check in default case!!!");
	            break;
        } 

		next_item = (reg_cap_id & NEXT_CAP_MASK) >> 8;
		
        if(next_item) 		/* Get the next item in the linked lint */
		{
            ret_val = pci_read_config_word(pdev, next_item, &reg_cap_id);
            if(ret_val<0) 
			{
                pr_err("pci_read_config failed in driver error check");
            }
        } 
		else 
		{
            pr_debug("No NEXT item in the list exiting...2");
            break;
        }
    }

    /* Check if PCI Power Management cap is supported as a min */
    if(power_management_feature == 0) 
	{
        pr_err("The controller should support PCI Pwr management as a min");
        pr_err("PCI Power Management Capability is not Supported.");
        status = -1;
    }

    return status;
}


/*
 * pmcs_status_chk: This function checks the pci power management
 * control and status.
 * PMCAP + 4h --> PMCS.
 */
int pmcs_status_chk(struct pci_dev *pdev, u8 pci_cfg_pmcs_offset, u16 *sts)
{
	u16 data;
	int ret_val;
	
	if(pci_cfg_pmcs_offset > MAX_PCI_HDR) 
	{
		/* Compute the PMCS offset from CAP data */
	    pci_cfg_pmcs_offset +=  PMCS;
	    ret_val = pci_read_config_word(pdev, pci_cfg_pmcs_offset, &data);
	    if(ret_val<0) 
		{
	        pr_err("pci_read_config failed");
			return ret_val;
	    }
	} 
	else 
	{
	    pr_debug("Invalid offset = 0x%x", pci_cfg_pmcs_offset);
		return -1;
	}

	*sts = data;
	pr_debug("PCI Power Management Control and Status = %x", data);
    return 0;
}


/*
 * device_status_msicap: This function checks the Message Signaled Interrupt
 * control and status bits.
 */
int msi_cap_status_chk(struct pci_dev *pdev, u8 pci_cfg_msi_offset, u16 *sts)
{
	u16 data;
	int ret_val;
	
	if(pci_cfg_msi_offset > MAX_PCI_HDR) 
	{
	    pci_cfg_msi_offset +=  2;
	    ret_val = pci_read_config_word(pdev, pci_cfg_msi_offset, &data);
	    if(ret_val<0) 
		{
	        pr_err("pci_read_config failed");
			return ret_val;
	    }
	} 
	else 
	{
	    pr_debug("Invalid offset = 0x%x", pci_cfg_msi_offset);
		return -1;
	}

	*sts = data;
	pr_debug("PCI MSI Cap Message Control = %x", data);
    return 0;
}



/*
 * device_status_msixcap: This func checks the Message Signaled Interrupt - X
 * control and status bits.
 */
int msix_cap_status_chk(struct pci_dev *pdev, u8 pci_cfg_msix_offset, u16 *sts)
{
	u16 data;
	int ret_val;
	
	if(pci_cfg_msix_offset > MAX_PCI_HDR) 
	{
	    pci_cfg_msix_offset +=  2;
	    ret_val = pci_read_config_word(pdev, pci_cfg_msix_offset, &data);
	    if(ret_val<0) 
		{
	        pr_err("pci_read_config failed");
			return ret_val;
	    }
	} 
	else 
	{
	    pr_debug("Invalid offset = 0x%x", pci_cfg_msix_offset);
		return -1;
	}

	*sts = data;
	pr_debug("PCI MSI-X Cap Message Control= %x", data);
    return 0;
}



/*
 * device_status_pxcap: This func checks the PCI Express
 * Capability status register
 */
int pxcap_status_chk(struct pci_dev *pdev, u8 base_offset, u16 *sts)
{
    int status = 0;
    u16 pxcap_sts_reg;

    base_offset += NVME_PXCAP_PXDS_OFF;			 	/* Compute the PXDS offset from the PXCAP */

    /* Read the device status register value into pxcap_sts_reg */
    status = pci_read_config_word(pdev, base_offset, &pxcap_sts_reg);
    if(status<0) 
	{
        pr_err("pci_read_config failed in driver error check");
    }
	*sts = pxcap_sts_reg;
	
    /* Check in device status register if any error bit is set */
        
    if(pxcap_sts_reg & NVME_PXDS_CED) 					/* Check if Correctable error is detected */
	{
        status = -1;
        pr_err("Correctable Error Detected (CED) in PXDS");
    }
        
    if(pxcap_sts_reg & NVME_PXDS_NFED) 			/* Check if Non fatal error is detected */
	{
        status = -1;
        pr_err("Non-Fatal Error Detected (NFED) in PXDS");
    }
       
    if(pxcap_sts_reg & NVME_PXDS_FED) 			/* Check if fatal error is detected */
	{
        status = -1;
        pr_err("Fatal Error Detected (FED) in PXDS");
    }
        
    if(pxcap_sts_reg & NVME_PXDS_URD) 			/* Check if Unsupported Request detected */
	{	
        status = -1;
        pr_err("Unsupported Request Detected (URD) in PXDS");
    }
       
    if(pxcap_sts_reg & NVME_PXDS_APD) 			/* Check if AUX Power detected */
	{
        pr_debug("AUX POWER Detected (APD) in PXDS");
    }
        
    if(pxcap_sts_reg & NVME_PXDS_TP) 			/* Check if Transactions Pending */
	{
       pr_debug("Transactions Pending (TP) in PXDS");
    }
		
    return status;
}

/*
 * device_status_aerap: This func checks the status register of
 * Advanced error reporting capability of PCI express device.
 */
int device_status_aercap(struct pci_dev *pdev, u16 base_offset)
{
    int status = 0;
    u16 offset; /* Offset 16 bit for PCIE space */
    u32 u32aer_sts = 0; /* AER Cap Status data */
    u32 u32aer_msk = 0; /* AER Mask bits data */
    int ret_code = 0; /* Return code for pci reads */

    pr_debug("Offset in AER CAP= 0x%X", base_offset);
    pr_debug("Checking Advanced Err Capability Status Regs (AERUCES and AERCS)");

    /* Compute the offset of AER Uncorrectable error status */
    offset = base_offset + NVME_AERUCES_OFFSET;

    /* Read the aer status bits */
    ret_code = pci_read_config_dword(pdev, offset, &u32aer_sts);
    if (ret_code < 0) {
        pr_err("pci_read_config failed in driver error check");
    }
    /* Mask the reserved bits */
    u32aer_sts &= ~NVME_AERUCES_RSVD;

    /* compute the mask offset */
    offset = base_offset + NVME_AERUCEM_OFFSET;

    /* get the mask bits */
    ret_code = pci_read_config_dword(pdev, offset, &u32aer_msk);
    if (ret_code < 0) {
        pr_err("pci_read_config failed in driver error check");
    }
   /* zero out the reserved bits */
   u32aer_msk &= ~NVME_AERUCES_RSVD;

   /*
   * Complement the Mask Registers and check if unmasked
   * bits have a error set
   */
    if (u32aer_sts & ~u32aer_msk) {
        /* Data Link Protocol Error check */
        if ((u32aer_sts & NVME_AERUCES_DLPES) >> 4) {
            status = -1;
            pr_err("Data Link Protocol Error Status is Set (DLPES)");
        }
        /* Pointed TLP status, not an error. */
        if ((u32aer_sts & NVME_AERUCES_PTS) >> 12) {
            pr_err("Poisoned TLP Status (PTS)");
        }
        /* Check if Flow control Protocol error is set */
        if ((u32aer_sts & NVME_AERUCES_FCPES) >> 13) {
            status = -1;
            pr_err("Flow Control Protocol Error Status (FCPES)");
        }
        /* check if completion time out status is set */
        if ((u32aer_sts & NVME_AERUCES_CTS) >> 14) {
            status = -1;
            pr_err("Completion Time Out Status (CTS)");
        }
        /* check if completer Abort Status is set */
        if ((u32aer_sts & NVME_AERUCES_CAS) >> 15) {
            status = -1;
            pr_err("Completer Abort Status (CAS)");
        }
        /* Check if Unexpected completion status is set */
        if ((u32aer_sts & NVME_AERUCES_UCS) >> 16) {
            status = -1;
            pr_err("Unexpected Completion Status (UCS)");
        }
        /* Check if Receiver Over Flow status is set, status not error */
        if ((u32aer_sts & NVME_AERUCES_ROS) >> 17) {
            pr_err("Receiver Overflow Status (ROS)");
        }
        /* Check if Malformed TLP Status is set, not an error */
        if ((u32aer_sts & NVME_AERUCES_MTS) >> 18) {
            pr_err("Malformed TLP Status (MTS)");
        }
        /* ECRC error status check */
        if ((u32aer_sts & NVME_AERUCES_ECRCES) >> 19) {
            status = -1;
            pr_err("ECRC Error Status (ECRCES)");
        }
        /* Unsupported Request Error Status*/
        if ((u32aer_sts & NVME_AERUCES_URES) >> 20) {
            status = -1;
            pr_err("Unsupported Request Error Status (URES)");
        }
        /* Acs violation status check */
        if ((u32aer_sts & NVME_AERUCES_ACSVS) >> 21) {
            status = -1;
            pr_err("ACS Violation Status (ACSVS)");
        }
        /* uncorrectable error status check */
        if ((u32aer_sts & NVME_AERUCES_UIES) >> 22) {
            status = -1;
            pr_err("Uncorrectable Internal Error Status (UIES)");
        }
        /* MC blocked TLP status check, not an error*/
        if ((u32aer_sts & NVME_AERUCES_MCBTS) >> 23) {
            pr_err("MC Blocked TLP Status (MCBTS)");
        }
        /* Atomic Op Egress blocked status, not an error */
        if ((u32aer_sts & NVME_AERUCES_AOEBS) >> 24) {
            pr_err("AtomicOp Egress Blocked Status (AOEBS)");
        }
        /* TLP prefix blocked error status. */
        if ((u32aer_sts & NVME_AERUCES_TPBES) >> 25) {
            status = -1;
            pr_err("TLP Prefix Blocked Error Status (TPBES)");
        }
    }

    /* Compute the offset for AER Correctable Error Status Register */
    offset = base_offset + NVME_AERCS_OFFSET;

    /* Read data from pcie space into u32aer_sts */
    ret_code = pci_read_config_dword(pdev, offset, &u32aer_sts);
    if (ret_code < 0) {
        pr_err("pci_read_config failed in driver error check");
    }
    /* zero out Reserved Bits*/
    u32aer_sts &= ~NVME_AERCS_RSVD;

    /* Compute the offset for AER correctable Mask register */
    offset = base_offset + NVME_AERCM_OFFSET;

    /*
     * Read the masked bits. When they are set to 1 it means that the s/w
     * should ignore those errors
     */
    ret_code = pci_read_config_dword(pdev, offset, &u32aer_msk);
    if (ret_code < 0) {
        pr_err("pci_read_config failed in driver error check");
    }
    /* Zero out any reserved bits if they are set */
    u32aer_msk &= ~NVME_AERCS_RSVD;

    /*
     * Complement the mask so that any value which is 1 is not be tested
     * so it becomes zero. Remaining unmasked bits are bit wise anded to
     * check if any error is set which is not masked.
     */
    if (u32aer_sts & ~u32aer_msk) {
            /* Checked if receiver error status is set */
            if (u32aer_sts & NVME_AERCS_RES) {
                status = -1;
                pr_err("Receiver Error Status (RES)");
            }

        /* check if Bad TLP status is set */
        if ((u32aer_sts & NVME_AERCS_BTS) >> 6) {
            status = -1;
            pr_err("BAD TLP Status (BTS)");
        }
        /* check if BAD DLP is set */
        if ((u32aer_sts & NVME_AERCS_BDS) >> 7) {
            status = -1;
            pr_err("BAD DLLP Status (BDS)");
        }
        /* Check if RRS is set, status not an error */
        if ((u32aer_sts & NVME_AERCS_RRS) >> 8) {
            pr_err("REPLAY_NUM Rollover Status (RRS)");
        }
        /* Check if RTS is set */
        if ((u32aer_sts & NVME_AERCS_RTS) >> 12) {
            status = -1;
            pr_err("Replay Timer Timeout Status (RTS)");
        }
        /* Check if non fatal error is set */
        if ((u32aer_sts & NVME_AERCS_ANFES) >> 13) {
            status = -1;
            pr_err("Advisory Non Fatal Error Status (ANFES)");
        }
        /* Check if CIES is set */
        if ((u32aer_sts & NVME_AERCS_CIES) >> 14) {
            status = -1;
            pr_err("Corrected Internal Error Status (CIES)");
        }
        /* check if HLOS is set, Status not an error */
        if ((u32aer_sts & NVME_AERCS_HLOS) >> 15) {
            pr_err("Header Log Overflow Status (HLOS)");
        }
    }

    return status;
}
