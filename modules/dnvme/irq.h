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

#include "core.h"
#include "dnvme_ioctl.h"

int dnvme_inquiry_cqe_with_isr(struct nvme_cq *cq, u32 *num_remaining, u32 *isr_count);

int dnvme_reset_isr_flag(struct nvme_context *nvme_ctx_list, u16 irq_no);

int dnvme_create_icq_node(struct nvme_irq_set *irq_set, u16 cq_id, u16 irq_no);
void dnvme_delete_icq_node(struct nvme_irq_set *irq_set, u16 cq_id, u16 irq_no);

int dnvme_set_interrupt(struct nvme_context *ctx, struct nvme_interrupt __user *uirq);
void dnvme_clear_interrupt(struct nvme_context *ctx);

int dnvme_mask_interrupt(struct nvme_irq_set *irq, u16 irq_no);
int dnvme_unmask_interrupt(struct nvme_irq_set *irq, u16 irq_no);

irqreturn_t dnvme_interrupt(int irq, void *data);

#endif /* !_DNVME_IRQ_H_ */
