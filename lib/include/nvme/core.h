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

#ifndef _UAPI_LIB_NVME_CORE_H_
#define _UAPI_LIB_NVME_CORE_H_

enum nvme_event {
	NVME_EVT_FORMAT_NVM,
};

struct pci_dev_instance {
	uint16_t		vendor_id;
	uint16_t		device_id;

	uint32_t		link_speed;
	uint32_t		link_width;

	struct pci_cap_pm	pm;
	struct pci_cap_express	express;
	struct pci_cap_msi	msi;
	struct pci_cap_msix	msix;
};

/*
 * @nsid: Namespace identifier
 * @fmt_idx: The format index was used to format the namespace
 * @blk_size: Logical block size
 * @meta_size: The number of metadata bytes provided per LBA
 */
struct nvme_ns_instance {
	uint32_t		nsid;

	/*
	 * The folling fields required to be updated after the namespace
	 * is reformatted.
	 */
	uint8_t 		fmt_idx;
	uint32_t		blk_size;
	uint16_t		meta_size;

	struct nvme_id_ns	*id_ns;
	struct nvme_ns_id_desc	*ns_id_desc[NVME_NIDT_FENCE];
	void			*ns_id_desc_raw;
	struct nvme_id_ns_nvm	*id_ns_nvm;
	struct nvme_id_ns_zns	*id_ns_zns;
};

/*
 * @nr_act: The number of active namespace associated with the controller
 * @act_list: An array for save active NSIDs
 */
struct nvme_ns_group {
	uint32_t		nr_act;
	__le32			*act_list;

	uint32_t		nr_ns;
	struct nvme_ns_instance	ns[0];
};

struct nvme_ctrl_property {
	uint64_t	cap;
	uint32_t	vs;
	uint32_t	cc;
};

/*
 * @nr_sq: The number of IOSQ allocated by the controller (1's based)
 * @nr_cq: The number of IOCQ allocated by the controller (1's based)
 */
struct nvme_ctrl_instance {
	uint16_t		nr_sq;
	uint16_t		nr_cq;

	struct nvme_id_ctrl	*id_ctrl;
	struct nvme_ctrl_property	*prop;
	struct nvme_id_ctrl_nvm	*id_ctrl_nvm;
	struct nvme_id_ctrl_zns	*id_ctrl_zns;
	struct nvme_id_ctrl_csc *id_ctrl_csc;
};

/*
 * @cmd_cnt: Records the number of commands submitted to the queue, 
 * 	excluding those that have been executed and retrieved CQE.
 */
struct nvme_sq_info {
	uint16_t	sqid;
	uint16_t	cqid;
	uint32_t	nr_entry; /* 1's based */

	uint16_t	cmd_cnt;
};

struct nvme_cq_info {
	uint16_t	cqid;
	uint16_t	irq_no;
	uint8_t		irq_en;
	uint32_t	nr_entry; /* 1's based */
};

/**
 * @brief NVMe device information
 * 
 * @fd: NVMe device file descriptor
 * @io_sqes: Actual effective I/O Completion Queue Entry Size. The value
 *  is in bytes and is specified as a power of two (2^n).
 * @io_cqes: Actual effective I/O Submission Queue Entry Size. The value
 *  is in bytes and is specified as a power of two (2^n).
 * 
 * @irq_type: The type of interrupt configured
 * @nr_irq: The number of interrupts configured
 */
struct nvme_dev_info {
	int		fd;
	int		sock_fd;

	struct nvme_sq_info	asq;
	struct nvme_cq_info	acq;
	struct nvme_sq_info	*iosqs;
	struct nvme_cq_info	*iocqs;

	uint8_t		io_sqes;
	uint8_t		io_cqes;

	struct pci_dev_instance		*pdev;
	struct nvme_ns_group		*ns_grp;
	struct nvme_ctrl_instance	*ctrl;

	enum nvme_irq_type	irq_type;
	uint16_t		nr_irq;

	struct nvme_dev_public	dev_pub;
};

static inline int nvme_is_maxio_falcon_lite(struct pci_dev_instance *pdev)
{
	return (pdev->vendor_id == PCI_VENDOR_ID_MAXIO && 
		pdev->device_id == PCI_DEVICE_ID_FALCON_LITE) ? 1 : 0;
}

struct nvme_dev_info *nvme_init(const char *devpath);
void nvme_deinit(struct nvme_dev_info *ndev);

int nvme_reinit(struct nvme_dev_info *ndev, uint32_t asqsz, uint32_t acqsz, 
	enum nvme_irq_type type);

int nvme_update_ns_instance(struct nvme_dev_info *ndev, 
		struct nvme_ns_instance *ns, enum nvme_event evt);

#endif /* !_UAPI_LIB_NVME_CORE_H_ */
