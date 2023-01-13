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

#define pr_fmt(fmt)			fmt

#include "byteorder.h"
#include "log.h"
#include "debug.h"

void nvme_display_buffer(const char *str, void *buf, uint32_t size)
{
	uint32_t *data = buf;
	uint32_t cnt = (size % 4 != 0) ? (size / 4 + 1) : (size / 4);
	uint32_t i;

	pr_debug("display '%s' with size:0x%x\n", str, size);

	for (i = 0; i < cnt; i += 4) {
		pr_debug("0x%08x: %08x %08x %08x %08x\n", 4 * i, 
			data[i], data[i + 1], data[i + 2], data[i + 3]);
	}
}

static void nvme_display_power_state(struct nvme_id_power_state *ps, uint8_t num)
{
	uint32_t mp; /* mW */
	uint8_t i;

	BUG_ON(num > 32);

	pr_debug("---+--------+----------+----------+-----+-----+-----+----\n");
	pr_debug("PS |     MP |    ENLAT |    EXLAT | RRT | RRL | RWT | RWL\n");
	pr_debug("---+--------+----------+----------+-----+-----+-----+----\n");
	for (i = 0; i < num; i++) {
		if (ps[i].flags & NVME_PS_FLAGS_MAX_POWER_SCALE)
			mp = le16_to_cpu(ps[i].max_power) / 10;
		else
			mp = (uint32_t)le16_to_cpu(ps[i].max_power) * 10;


		pr_debug("%2u | %6u | %8u | %8u |  %2u |  %2u |  %2u |  %2u\n", 
			i, mp, le32_to_cpu(ps[i].entry_lat), 
			le32_to_cpu(ps[i].exit_lat),
			ps[i].read_tput, ps[i].read_lat, ps[i].write_tput,
			ps[i].write_lat);
	}
	pr_debug("NOTE:\n");
	pr_debug("1. PS: Power State\n");
	pr_debug("2. MP: Maximum Power (mW)\n");
	pr_debug("3. ENLAT: Entry Latency (us)\n");
	pr_debug("4. EXLAT: Exit Latency (us)\n");
	pr_debug("5. RRT: Relative Read Through\n");
	pr_debug("6. RRL: Relative Read Latency\n");
	pr_debug("7. RWT: Relative Write Throughput\n");
	pr_debug("8. RWL: Relative Write Latency\n");
	pr_debug("\n");
}

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

	pr_debug("Number of Power States Support: %u (0's based)\n", ctrl->npss);

	pr_debug("Submission Queue Entry Size: 0x%02x\n", ctrl->sqes);
	pr_debug("Completion Queue Entry Size: 0x%02x\n", ctrl->cqes);
	pr_debug("Maximum Outstanding Commands: %u\n", le16_to_cpu(ctrl->maxcmd));
	pr_debug("Number of Namespaces: %u\n", le32_to_cpu(ctrl->nn));

	pr_debug("SGL Support: 0x%x\n", le32_to_cpu(ctrl->sgls));

	pr_debug("----- Power State Descriptor Data -----\n");
	nvme_display_power_state(ctrl->psd, ctrl->npss + 1);
	pr_debug("\n");
}

void nvme_display_id_ns(struct nvme_id_ns *ns, uint32_t nsid)
{
	uint8_t flbas = ns->flbas & 0xf;

	pr_debug("===== Identify Namespace(%u) Data =====\n", nsid);

	pr_debug("Namespace Size: 0x%llx\n", le64_to_cpu(ns->nsze));

	pr_debug("Number of LBA Formats: %u\n", ns->nlbaf);
	pr_debug("Formatted LBA Size: 0x%02x\n", ns->flbas);

	pr_debug("----- LBA Format Data -----\n");
	pr_debug("Metadata Size: %u\n", le16_to_cpu(ns->lbaf[flbas].ms));
	pr_debug("LBA Data Size: %u\n", ns->lbaf[flbas].ds);
	pr_debug("Relative Performance: 0x%x\n", ns->lbaf[flbas].rp & 0x3);
	pr_debug("\n");
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
	pr_debug("\n");
}

void nvme_display_cc(uint32_t cc)
{
	pr_debug("~~~~~ Controller Configuration: 0x%x ~~~~~\n", cc);
	pr_debug("\n");
}
