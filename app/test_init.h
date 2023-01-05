
#ifndef _TEST_INIT_H_
#define _TEST_INIT_H_

#include "dnvme_ioctl.h"

#include "test_metrics.h"

int nvme_init(struct nvme_dev_info *ndev);

int nvme_reinit(int fd, uint32_t asqsz, uint32_t acqsz, enum nvme_irq_type type,
	uint16_t nr_irqs);

#endif
