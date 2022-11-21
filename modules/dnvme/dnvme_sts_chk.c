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
