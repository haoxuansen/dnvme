
#ifndef _TEST_INIT_H_
#define _TEST_INIT_H_

#include "dnvme_ioctl.h"

#include "test_metrics.h"

void random_sq_cq_info(void);

int test_init(int fd, struct nvme_dev_info *ndev);

void test_change_init(int g_fd, uint32_t asqsz, uint32_t acqsz, enum nvme_irq_type irq_type, uint16_t num_irqs);
void test_change_irqs(int g_fd, enum nvme_irq_type irq_type, uint16_t num_irqs);

#endif
