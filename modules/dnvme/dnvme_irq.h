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

#ifndef _DNVME_IRQ_H_
#define _DNVME_IRQ_H_

#include "dnvme_ds.h"
#include "core.h"
#include "dnvme_queue.h"
#include "dnvme_ioctl.h"
#include "dnvme_sts_chk.h"
#include "definitions.h"

/* Max IRQ vectors for MSI SINGLE IRQ scheme */
#define     MAX_IRQ_VEC_MSI_SIN     1

/* Max IRQ vectors for MSI MULTI IRQ scheme */
#define     MAX_IRQ_VEC_MSI_MUL     32

/* Max IRQ vectors for MSI X IRQ scheme */
#define     MAX_IRQ_VEC_MSI_X       2048

/* Command offset in PCI space */
#define     CMD_OFFSET              0x4

/* Bit mask for pin# interrupts */
#define     PIN_INT_BIT_MASK        (1 << 10)

/* Entry size in bytes for each entry in MSIX table */
#define     MSIX_ENTRY_SIZE         16

/* MSIX Table vector control offset in bytes */
#define     MSIX_VEC_CTRL           12

/* Table size mask bits for MSIX */
#define     MSIX_TS                 0x7FF
#define     MSIX_TBIR               0x07
#define     MSIX_PBIR               0x07

/* Mask for MSIX enable */
#define     MSIX_ENABLE             0x8000

/* Mask for MSI enable bit */
#define     MSI_ENABLE             0x1

/* Mask for MSI Multi message enable bits */
//#define     MSI_MME                 0x70
#define     MSI_MMC                 0x0E

/* Interrupt vector mask set(IVMS) register offset */
#define     INTMS_OFFSET            0x0C

/* Interrupt vector mask clear(IVMC) register offset */
#define     INTMC_OFFSET            0x10

/*
 * dnvme_inquiry_cqe_with_isr will process reap inquiry for the given cq using irq_vec
 * and isr_fired flags from two nodes, public cq node and nvme_irq list node.
 * It fills the num_remaining with number of elements remaining or 0 based on
 * CE entries. If the IRQ aggregation is enabled it returns 0 if aggregation
 * limit is not reached.
 */
int dnvme_inquiry_cqe_with_isr(struct nvme_cq *cq, u32 *num_remaining, u32 *isr_count);


int dnvme_reset_isr_flag(struct nvme_context *nvme_ctx_list, u16 irq_no);

int dnvme_create_icq_node(struct nvme_irq_set *irq_set, u16 cq_id, u16 irq_no);
void dnvme_delete_icq_node(struct nvme_irq_set *irq_set, u16 cq_id, u16 irq_no);

int dnvme_set_interrupt(struct nvme_context *ctx, struct nvme_interrupt __user *uirq);
void dnvme_clear_interrupt(struct nvme_context *ctx);

int dnvme_mask_interrupt(struct nvme_irq_set *irq, u16 irq_no);
int dnvme_unmask_interrupt(struct nvme_irq_set *irq, u16 irq_no);

irqreturn_t dnvme_interrupt(int irq, void *data);

#endif
