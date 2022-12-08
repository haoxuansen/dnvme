
#ifndef _TEST_INIT_H_
#define _TEST_INIT_H_

#include "dnvme_ioctl.h"

void random_sq_cq_info(void);

void test_init(int g_fd);

void test_change_init(int g_fd, uint32_t asqsz, uint32_t acqsz, enum nvme_irq_type irq_type, uint16_t num_irqs);
void test_set_admn(int g_fd, uint32_t asqsz, uint32_t acqsz);
void test_change_irqs(int g_fd, enum nvme_irq_type irq_type, uint16_t num_irqs);

int identify_control(int g_fd, void *addr);
int identify_ns(int g_fd, uint32_t nsid, void *addr);

#endif
