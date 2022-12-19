/**
 * @file cmd.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-19
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _APP_CMD_H_
#define _APP_CMD_H_

#include <stdint.h>

#include "dnvme_ioctl.h"

int nvme_submit_64b_cmd(int fd, struct nvme_64b_cmd *cmd);

#endif /* !_APP_CMD_H_ */
