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

#include "dnvme_ioctl.h"

int nvme_create_asq(int fd, uint32_t elements);
int nvme_create_acq(int fd, uint32_t elements);

int nvme_prepare_iosq(int fd, uint16_t sqid, uint16_t cqid, uint32_t elements, 
	uint8_t contig);
int nvme_prepare_iocq(int fd, uint16_t cqid, uint32_t elements, uint8_t contig,
	uint8_t irq_en, uint16_t irq_no);

#endif /* !_APP_QUEUE_H_ */
