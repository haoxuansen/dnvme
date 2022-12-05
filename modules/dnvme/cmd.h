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
#include "dnvme_cmds.h"

#define NVME_PRP_ENTRY_SIZE		8 /* in bytes */

/* Enum specifying Writes/Reads to mapped pages and other general enums */
enum {
    PERSIST_QID_0 = 0, /* Default value of Persist queue ID */
    CDW11_PC = 1, /* Mask for checking CDW11.PC of create IO Q cmds */
    CDW11_IEN = 2, /* Mask to check if CDW11.IEN is set */
};

int dnvme_prepare_64b_cmd(struct nvme_device *ndev, struct nvme_64b_cmd *cmd, 
	struct nvme_gen_cmd *gcmd, struct nvme_prps *prps);

void dnvme_release_prps(struct nvme_device *ndev, struct nvme_prps *prps);

void dnvme_delete_cmd_list(struct nvme_device *ndev, struct nvme_sq *sq);

#endif /* !_DNVME_CMD_H_ */
