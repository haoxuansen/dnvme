/**
 * @file queue.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-16
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _APP_QUEUE_H_
#define _APP_QUEUE_H_

#include <stdint.h>

int nvme_create_asq(int fd, uint32_t elements);
int nvme_create_acq(int fd, uint32_t elements);

#endif /* !_APP_QUEUE_H_ */
