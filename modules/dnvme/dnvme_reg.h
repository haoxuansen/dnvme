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

#ifndef _DNVME_REG_H_
#define _DNVME_REG_H_

enum nvme_access_type;

#pragma pack(push)
#pragma pack(4)

/**
 * nvme_ctrl_reg defines the register space for the
 * nvme controller registers as defined in NVME Spec 1.0b.
 */
struct nvme_ctrl_reg {
    u64    cap;    /* Controller Capabilities */
    u32    vs;     /* Version */
    u32    intms;  /* Interrupt Mask Set */
    u32    intmc;  /* Interrupt Mask Clear */
    u32    cc;     /* Controller Configuration */
    u32    rsvd1;  /* Reserved */
    u32    csts;   /* Controller Status */
    u32    nssr;   /* NVM Subsystem Reset */
    u32    aqa;    /* Admin Queue Attributes */
    u64    asq;    /* Admin SQ Base Address */
    u64    acq;    /* Admin CQ Base Address */
    u32    cmbsz;  /* 0x38: Controller Memory Buffer Location */
    u32    cmbloc; /* 0x3c: Controller Memory Buffer Size */
    u32    bpinfo; /* 0x40: Boot Partition Information */
    u32    bprsel; /* 0x44: Boot Partition Read Select */
    u64    bpmbl;  /* 0x48: Boot Partition Memory Buffer Location */
    u64    cmbmsc; /* 0x50: Controller Memory Buffer Memory Space Control*/
    u32    cmbsts; /* 0x58: Controller Memory Buffer Memory Status*/
    u32    rsvd2[0x369];
    u32    pmrcap; /* 0x0e00: Persistent Memory Capabilities */
    u32    pmrctl; /* 0x0e04: Persistent Memory Region Control */
    u32    pmrsts; /* 0x0e08: Persistent Memory Region Status */
    u32    pmrebs; /* 0x0e0c: Persistent Memory Region Elasticity */
    u32    pmrswtp; /* 0x0e10: Persistent Memory Region Sustained Write Throughput */
    u32    pmrmscl; /* 0x0e14: Persistent Memory Region Memory Space Control Lower*/
    u32    pmrmscu; /* 0x0e18: Persistent Memory Region Memory Space Control Upper*/
    u32    rsvd3[0x79];
    u32    sqtdbl; /* 0x1000: SQ 0 Tail Doorbell */
    u32    sqhdbl; /* 0x1004: SQ 0 Head Doorbell */
};

#pragma pack(pop)

#define REGMASK_CAP_CQR     (1 << 16)

u64 READQ(const volatile void __iomem *addr);
void WRITEQ(u64 val, volatile void __iomem *addr);

/**
 * read_nvme_reg_generic function is a generic function which
 * reads data from the controller registers of the nvme with
 * user specified offset and bytes. Copies data back to udata
 * pointer which points to user space buffer.
 *
 */
int read_nvme_reg_generic(u8 __iomem *bar0,
        u8 *udata, u32 nbytes, u32 offset, enum nvme_access_type acc_type);

/**
 * write_nvme_reg_generic function is a generic function which
 * writes data to the controller registers of the nvme with
 * user specified offset and bytes.
 */
int write_nvme_reg_generic(u8 __iomem *bar0,
        u8 *udata, u32 nbytes, u32 offset, enum nvme_access_type acc_type);

#endif
