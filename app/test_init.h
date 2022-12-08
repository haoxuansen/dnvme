
#ifndef _TEST_INIT_H_
#define _TEST_INIT_H_

#include "dnvme_ioctl.h"

void random_sq_cq_info(void);

void test_init(int file_desc);

void test_change_init(int file_desc, uint32_t asqsz, uint32_t acqsz, enum nvme_irq_type irq_type, uint16_t num_irqs);
void test_set_admn(int file_desc, uint32_t asqsz, uint32_t acqsz);
void test_change_irqs(int file_desc, enum nvme_irq_type irq_type, uint16_t num_irqs);

int identify_control(int file_desc, void *addr);
int identify_ns(int file_desc, uint32_t nsid, void *addr);

#endif
