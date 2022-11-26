/**
 * @file cmd.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-11-25
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _DNVME_CMD_H_
#define _DNVME_CMD_H_

#include <linux/scatterlist.h>

#include "dnvme_ioctl.h"
#include "dnvme_ds.h"

#define NVME_PRP_ENTRY_SIZE		8 /* in bytes */

bool dnvme_use_sgls(struct nvme_gen_cmd *gcmd, struct nvme_64b_cmd *cmd);

void dnvme_sgl_set_data(struct nvme_sgl_desc *sge, struct scatterlist *sg);
void dnvme_sgl_set_seg(struct nvme_sgl_desc *sge, dma_addr_t dma_addr, 
	int entries);

int dnvme_prepare_64b_cmd(struct nvme_device *ndev, struct nvme_64b_cmd *cmd, 
	struct nvme_gen_cmd *gcmd, struct nvme_prps *prps);

void dnvme_release_prps(struct nvme_device *ndev, struct nvme_prps *prps);

#endif /* !_DNVME_CMD_H_ */
