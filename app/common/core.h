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

#include <stdint.h>

#include "dnvme_ioctl.h"
#include "queue.h"

/**
 * @brief NVMe namespace information
 * 
 * @nsid: Namespace identifier
 * @nsze: The total size of the namespace in logical blocks
 * @lbads: LBA data size in byte
 * @id_ns: NVM command set identify namesapce data structure
 */
struct nvme_ns_info {
	uint32_t	nsid;
	uint64_t	nsze;
	uint32_t	lbads;

	struct nvme_id_ns	id_ns;
};

/**
 * @brief NVMe device information
 * 
 * @nss: An array for storing namespace info.
 * @ns_num_total: The maximum value of a valid NSID for the NVM subsystem
 * 
 * @io_sqes: Actual effective I/O Completion Queue Entry Size. The value
 *  is in bytes and is specified as a power of two (2^n).
 * @io_cqes: Actual effective I/O Submission Queue Entry Size. The value
 *  is in bytes and is specified as a power of two (2^n).
 * @irq_type: The type of interrupt configured
 * @nr_irq: The number of interrupts configured
 */
struct nvme_dev_info {
	int		fd;

	uint16_t	max_sq_num; // 1'base
	uint16_t	max_cq_num; // 1'base

	struct nvme_sq_info	*iosqs;
	struct nvme_cq_info	*iocqs;

	struct nvme_ns_info	*nss;
	uint32_t	ns_num_total;
	uint32_t	ns_num_actual;

	uint32_t	link_speed;
	uint32_t	link_width;
	uint8_t		pmcap_ofst;
	uint8_t		msicap_ofst;
	uint8_t		pxcap_ofst;

	uint8_t		io_sqes;
	uint8_t		io_cqes;

	enum nvme_irq_type	irq_type;
	uint16_t		nr_irq;
	
	struct nvme_cap		cap;
	struct nvme_id_ctrl	id_ctrl;

	struct nvme_ctrl_property	prop;
};

int nvme_reinit(struct nvme_dev_info *ndev, uint32_t asqsz, uint32_t acqsz, 
	enum nvme_irq_type type);

#endif /* !_APP_CORE_H_ */
