/**
 * @file irq.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief UAPI related to interrupt processing, which can be called by 
 *  different test cases.
 * @version 0.1
 * @date 2022-12-16
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _APP_IRQ_H_
#define _APP_IRQ_H_

#include <stdint.h>

#include "dnvme.h"

enum nvme_irq_type nvme_select_irq_type_random(void);

int nvme_set_irq(int fd, enum nvme_irq_type type, uint16_t nr_irqs);
int nvme_switch_irq(int fd, enum nvme_irq_type type, uint16_t nr_irqs);

int nvme_mask_irq(int fd, uint16_t irq_no);
int nvme_unmask_irq(int fd, uint16_t irq_no);

#endif /* !_APP_IRQ_H_ */
