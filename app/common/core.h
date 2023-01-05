/**
 * @file core.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-04
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _APP_CORE_H_
#define _APP_CORE_H_

#include "dnvme_ioctl.h"

struct nvme_sq_info {
	uint16_t	sq_id;
	uint16_t	cq_id;

	uint32_t	sq_size;
	uint32_t	cq_size;

	uint16_t	cq_int_vct;
	uint16_t	cmd_cnt;
};

struct nvme_cq_info {
	uint16_t	cqid;
	uint16_t	irq_no;
	uint32_t	size;
};

/**
 * @brief NVMe device information
 * 
 * @io_sqes: Actual effective I/O Completion Queue Entry Size. The value
 *  is in bytes and is specified as a power of two (2^n).
 * @io_cqes: Actual effective I/O Submission Queue Entry Size. The value
 *  is in bytes and is specified as a power of two (2^n).
 */
struct nvme_dev_info {
	int		fd;

	uint32_t	max_sq_num; // 1'base
	uint32_t	max_cq_num; // 1'base

	struct nvme_sq_info	*sqs;
	struct nvme_cq_info	*cqs;

	uint32_t	link_speed;
	uint32_t	link_width;
	uint8_t		pmcap_ofst;
	uint8_t		msicap_ofst;
	uint8_t		pxcap_ofst;

	uint8_t		io_sqes;
	uint8_t		io_cqes;

	enum nvme_irq_type	irq_type;
	struct nvme_cap		cap;
	struct nvme_id_ctrl	id_ctrl;

	struct nvme_ctrl_property	prop;
};

#endif /* !_APP_CORE_H_ */
