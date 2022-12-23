/**
 * @file debug.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-22
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "byteorder.h"
#include "log.h"
#include "debug.h"

void nvme_display_id_ctrl(struct nvme_id_ctrl *ctrl)
{
	pr_debug("===== Identify Controller Data =====\n");

	pr_debug("PCI Vendor ID: 0x%04x\n", le16_to_cpu(ctrl->vid));
	pr_debug("PCI Subsystem Vendor ID: 0x%04x\n", le16_to_cpu(ctrl->ssvid));
	pr_debug("Serial Number: %s\n", ctrl->sn);
	pr_debug("Model Number: %s\n", ctrl->mn);

	pr_debug("Maximum Data Transfer Size: %u\n", ctrl->mdts);
	pr_debug("Controller ID: 0x%04x\n", le16_to_cpu(ctrl->cntlid));
	pr_debug("Version: 0x%x\n", ctrl->ver);

	pr_debug("Submission Queue Entry Size: 0x%02x\n", ctrl->sqes);
	pr_debug("Completion Queue Entry Size: 0x%02x\n", ctrl->cqes);
	pr_debug("Maximum Outstanding Commands: %u\n", le16_to_cpu(ctrl->maxcmd));
	pr_debug("Number of Namespaces: %u\n", le32_to_cpu(ctrl->nn));

	pr_debug("SGL Support: 0x%x\n", le32_to_cpu(ctrl->sgls));
}

void nvme_display_id_ns(struct nvme_id_ns *ns)
{
	uint8_t flbas = ns->flbas & 0xf;

	pr_debug("===== Identify Namespace Data =====\n");

	pr_debug("Namespace Size: 0x%llx\n", le64_to_cpu(ns->nsze));

	pr_debug("Number of LBA Formats: %u\n", ns->nlbaf);
	pr_debug("Formatted LBA Size: 0x%02x\n", ns->flbas);

	pr_debug("----- LBA Format Data -----\n");
	pr_debug("Metadata Size: %u\n", le16_to_cpu(ns->lbaf[flbas].ms));
	pr_debug("LBA Data Size: %u\n", ns->lbaf[flbas].ds);
	pr_debug("Relative Performance: 0x%x\n", ns->lbaf[flbas].rp & 0x3);
}

void nvme_display_cap(uint64_t cap)
{
	pr_debug("~~~~~ Controller Capabilities: 0x%llx ~~~~~\n", cap);

	pr_debug("Maximum Queue Entries Supported: %u (0's based)\n", 
		NVME_CAP_MQES(cap));

	pr_debug("Arbitration Mechanism Supported: 0x%x\n", 
		NVME_CAP_AMS(cap));

	pr_debug("Timeout: %u\n", NVME_CAP_TIMEOUT(cap));

	pr_debug("Memory Page Size Minimum: %u\n", NVME_CAP_MPSMIN(cap));
	pr_debug("Memory Page Size Maximum: %u\n", NVME_CAP_MPSMAX(cap));
}

void nvme_display_cc(uint32_t cc)
{
	pr_debug("~~~~~ Controller Configuration: 0x%x ~~~~~\n", cc);
}
