/**
 * @file irq.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-04-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _DNVME_IRQ_H_
#define _DNVME_IRQ_H_

#include "core.h"
#include "dnvme_ioctl.h"

int dnvme_reset_isr_flag(struct nvme_context *nvme_ctx_list, u16 irq_no);

int dnvme_create_icq_node(struct nvme_irq_set *irq_set, u16 cq_id, u16 irq_no);
void dnvme_delete_icq_node(struct nvme_irq_set *irq_set, u16 cq_id, u16 irq_no);

int dnvme_set_interrupt(struct nvme_context *ctx, struct nvme_interrupt __user *uirq);
void dnvme_clean_interrupt(struct nvme_context *ctx);

int dnvme_mask_interrupt(struct nvme_irq_set *irq, u16 irq_no);
int dnvme_unmask_interrupt(struct nvme_irq_set *irq, u16 irq_no);

irqreturn_t dnvme_interrupt(int irq, void *data);

#endif /* !_DNVME_IRQ_H_ */
