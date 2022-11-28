/**
 * @file log.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-11-29
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _DNVME_LOG_H_
#define _DNVME_LOG_H_

#include "dnvme_ioctl.h"

int dnvme_dump_log_file(struct nvme_log_file __user *ulog_file);

#endif /* !_DNVME_LOG_H_ */
