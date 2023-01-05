/**
 * @file core.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-04
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <errno.h>

#include "log.h"
#include "core.h"

#if 0
void nvme_swap_sq_random(struct nvme_dev_info *ndev, struct nvme_sq_info *sq, uint32_t flag)
{
	struct nvme_ctrl_property *prop = &ndev->prop;
	struct nvme_sq_info tmp;
	uint32_t nr_sq = ndev->max_sq_num;
	uint32_t nr_cq = ndev->max_cq_num;
	uint32_t num;
	uint32_t i;

	for (i = 0; i < nr_sq; i++) {
		num = i + rand() % (nr_sq - i);
		tmp.cq_id = sq[i].cq_id;
		sq[i].cq_id = sq[num].cq_id;
		sq[num].cq_id = tmp.cq_id;

		num = i + rand() % (nr_sq - i);
		tmp.cq_int_vct = sq[i].cq_int_vct;
		sq[i].cq_int_vct = sq[num].cq_int_vct;
		sq[num].cq_int_vct = tmp.cq_int_vct;

		num = i + rand() % (nr_sq - i);
		tmp.sq_size = sq[i].sq_size;
		sq[i].sq_size = sq[num].sq_size;
		sq[num].sq_size = tmp.sq_size;

		num = i + rand() % (nr_sq - i);
		tmp.cq_size = sq[i].cq_size;
		sq[i].
	}
}
#endif
