
#ifndef _TEST_INIT_H_
#define _TEST_INIT_H_

#include "dnvme_ioctl.h"

#include "test_metrics.h"

int nvme_init(struct nvme_dev_info *ndev);
void nvme_deinit(struct nvme_dev_info *ndev);

#endif
