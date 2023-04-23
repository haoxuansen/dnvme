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
#include <unistd.h>

#include "pci_caps.h"
#include "dnvme.h"
#include "queue.h"

#define msleep(ms)			usleep(1000 * (ms))

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
	uint16_t	ms;

	struct nvme_id_ns	id_ns;
};

/**
 * @brief NVMe device controller properties
 * 
 * @cap: Controller Capabilities Register
 * @vs: Version
 * @cc: Controller Configuration Register
 * @cmbloc: Controller Memory Buffer Location Register
 * @cmbsz: Controller Memory Buffer Size Register
 */
struct nvme_ctrl_property {
	uint64_t	cap;
	uint32_t	vs;
	uint32_t	cc;
	uint32_t	cmbloc;
	uint32_t	cmbsz;
};

/**
 * @brief NVMe device information
 * 
 * @fd: NVMe device file descriptor
 * @vid: Vendor Identifier
 * @io_sqes: Actual effective I/O Completion Queue Entry Size. The value
 *  is in bytes and is specified as a power of two (2^n).
 * @io_cqes: Actual effective I/O Submission Queue Entry Size. The value
 *  is in bytes and is specified as a power of two (2^n).
 * @nss: An array for storing namespace info.
 * @ns_num_total: The maximum value of a valid NSID for the NVM subsystem
 * 
 * @irq_type: The type of interrupt configured
 * @nr_irq: The number of interrupts configured
 */
struct nvme_dev_info {
	int		fd;
	int		sock_fd;

	uint16_t	vid;

	uint16_t	max_sq_num; // 1'base
	uint16_t	max_cq_num; // 1'base

	struct nvme_sq_info	asq;
	struct nvme_cq_info	acq;
	struct nvme_sq_info	*iosqs;
	struct nvme_cq_info	*iocqs;

	uint8_t		io_sqes;
	uint8_t		io_cqes;

	struct nvme_ns_info	*nss;
	uint32_t	ns_num_total;
	uint32_t	ns_num_actual;

	uint32_t	link_speed;
	uint32_t	link_width;

	struct pci_cap_pm	pm;
	struct pci_cap_express	express;
	struct pci_cap_msi	msi;
	struct pci_cap_msix	msix;

	enum nvme_irq_type	irq_type;
	uint16_t		nr_irq;

	struct nvme_id_ctrl	id_ctrl;

	struct nvme_ctrl_property	prop;
	struct nvme_dev_public	dev_pub;
};

int call_system(const char *command);

void nvme_fill_data(void *buf, uint32_t size);
void nvme_dump_data(void *buf, uint32_t size);

struct nvme_dev_info *nvme_init(const char *devpath);
void nvme_deinit(struct nvme_dev_info *ndev);

int nvme_reinit(struct nvme_dev_info *ndev, uint32_t asqsz, uint32_t acqsz, 
	enum nvme_irq_type type);

int nvme_update_ns_info(struct nvme_dev_info *ndev, struct nvme_ns_info *ns);

#endif /* !_APP_CORE_H_ */
