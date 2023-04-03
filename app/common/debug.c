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

static const char *lbaf_rp_string(uint8_t rp)
{
	switch (rp) {
	case NVME_LBAF_RP_BEST:
		return "Best";
	case NVME_LBAF_RP_BETTER:
		return "Better";
	case NVME_LBAF_RP_GOOD:
		return "Good";
	case NVME_LBAF_RP_DEGRADED:
		return "Degraded";
	}
	return "";
}

static const char *cap_cps_string(uint8_t cps)
{
	switch (cps) {
	case NVME_CAP_CPS_UNKNOWN:
		return "Not Reported";
	case NVME_CAP_CPS_CTRL:
		return "Controller";
	case NVME_CAP_CPS_DOMAIN:
		return "Domain";
	case NVME_CAP_CPS_SUBSYSTEM:
		return "Subsystem";
	}
	return "";
}

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

static void nvme_display_sgl_support(uint32_t sgls)
{
	pr_debug("---------- SGL Support(0x%x) ----------\n", sgls);

	if (!sgls)
		return;

	pr_debug("[Y] Support SGL with %u B align for data blocks\n",
		(sgls & NVME_CTRL_SGLS_MASK) == NVME_CTRL_SGLS_SUPPORT ? 1 : 4);

	pr_debug("[%s] Support Data Block Descriptor\n", 
			(sgls & NVME_CTRL_SGLS_DATA_BLOCK) ? "Y" : "N");
	pr_debug("[%s] Support Keyed Data Block Descriptor\n",
			(sgls & NVME_CTRL_SGLS_KEYED_DATA_BLOCK) ? "Y" : "N");
	pr_debug("[%s] Support Bit Bucket Descriptor\n",
			(sgls & NVME_CTRL_SGLS_BIT_BUCKET) ? "Y" : "N");
	pr_debug("[%s] Support byte aligned contiguous physical buffer of metadata\n",
			(sgls & NVME_CTRL_SGLS_META_BYTE) ? "Y" : "N");
	pr_debug("[%s] Support SGL length larger than the amount "
		"of data to be transferred\n", 
			(sgls & NVME_CTRL_SGLS_LARGER_DATA) ? "Y" : "N");
	pr_debug("[%s] Support Metadata Pointer contains an address"
		" of an SGL segment\n",
			(sgls & NVME_CTRL_SGLS_MPTR_SGL) ? "Y" : "N");
	pr_debug("[%s] Support the Address field in SGL specify an offset\n",
			(sgls & NVME_CTRL_SGLS_ADDR_OFFSET) ? "Y" : "N");

	if (NVME_CTRL_SGLS_SDT(sgls))
		pr_debug("Recommended max number of SGL desc in a cmd: %u\n", 
			NVME_CTRL_SGLS_SDT(sgls));
	else
		pr_debug("Recommended max number of SGL desc isn't reported!\n");

	pr_debug("\n");
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
	pr_debug("========== Identify Controller Data ==========\n");

	pr_debug("PCI Vendor ID: 0x%04x\n", le16_to_cpu(ctrl->vid));
	pr_debug("PCI Subsystem Vendor ID: 0x%04x\n", le16_to_cpu(ctrl->ssvid));
	pr_debug("Serial Number: %s\n", ctrl->sn);
	pr_debug("Model Number: %s\n", ctrl->mn);

	if (ctrl->mdts)
		pr_debug("Maximum Data Transfer Size: %u * CAP.MPSMIN\n", 
			1 << ctrl->mdts);
	else
		pr_debug("No Maximum Data Transfer Size!\n");

	pr_debug("Controller ID: 0x%04x\n", le16_to_cpu(ctrl->cntlid));
	pr_debug("Version: 0x%x\n", ctrl->ver);

	pr_debug("Number of Power States Support: %u\n", ctrl->npss + 1);

	pr_debug("Max SQ Entry Size: %u B\n", 1 << (ctrl->sqes >> 4));
	pr_debug("Min SQ Entry Size: %u B\n", 1 << (ctrl->sqes & 0xf));
	pr_debug("Max CQ Entry Size: %u B\n", 1 << (ctrl->cqes >> 4));
	pr_debug("Min CQ Entry Size: %u B\n", 1 << (ctrl->cqes & 0xf));

	if (le16_to_cpu(ctrl->maxcmd))
		pr_debug("Max Outstanding Cmds: %u\n", le16_to_cpu(ctrl->maxcmd));
	else
		pr_debug("Max Outstanding Cmds Not Reported!\n");

	pr_debug("Number of Namespaces: %u\n", le32_to_cpu(ctrl->nn));

	nvme_display_sgl_support(le32_to_cpu(ctrl->sgls));

	pr_debug("---------- Power State Descriptor Data ----------\n");
	nvme_display_power_state(ctrl->psd, ctrl->npss + 1);
	pr_debug("\n");
}

static void nvme_display_lbaf(struct nvme_lbaf *lbaf)
{
	pr_debug("Metadata Size: %u byte(s)\n", le16_to_cpu(lbaf->ms));
	pr_debug("LBA Data Size: %u byte(s)\n", 1 << lbaf->ds);
	pr_debug("%s Performance\n\n", 
		lbaf_rp_string(lbaf->rp & NVME_LBAF_RP_MASK));
}

void nvme_display_id_ns(struct nvme_id_ns *ns, uint32_t nsid)
{
	uint32_t i;

	pr_debug("===== Identify Namespace(%u) Data =====\n", nsid);

	pr_debug("Namespace Size: 0x%llx\n", le64_to_cpu(ns->nsze));

	pr_debug("Number of LBA Formats: %u\n", ns->nlbaf + 1);
	pr_debug("FLBAS:\n");
	pr_debug("\t Format Index: %u\n", NVME_NS_FLBAS_LBA(ns->flbas));
	pr_debug("\t Meta Data %s\n", (ns->flbas & NVME_NS_FLBAS_META_EXT) ? 
		"Contiguous with LBA Data" : "Transferred as Separate Buffer");

	pr_debug("Metadata Capability: 0x%02x\n", ns->mc);
	pr_debug("\t [%s] Support Contiguous with LBA Data\n",
		(ns->mc & NVME_MC_EXTENDED_LBA) ? "Y" : "N");
	pr_debug("\t [%s] Support Transferred as Separate Buffer\n",
		(ns->mc & NVME_MC_METADATA_PTR) ? "Y" : "N");

	pr_debug("End-to-end Data Protection Capability: 0x%02x\n", ns->dpc);
	pr_debug("End-to-end Data Protection Type Setting: 0x%02x\n", ns->dps);

	for (i = 0; i <= ns->nlbaf; i++) {
		pr_debug("----- LBA Format Data(%u) -----\n", i);
		nvme_display_lbaf(&ns->lbaf[i]);
	}
}

void nvme_display_cap(uint64_t cap)
{
	pr_debug("~~~~~ Controller Capabilities: 0x%llx ~~~~~\n", cap);

	pr_debug("Maximum Queue Entries Supported: %u\n", 
		NVME_CAP_MQES(cap) + 1);

	pr_debug("[%s] Support Non-contiguous I/O SQ & CQ\n",
				NVME_CAP_CQR(cap) ? "N" : "Y");
	pr_debug("[%s] Support Arbitration Mechanism\n", 
				NVME_CAP_AMS(cap) ? "Y" : "N");
	if (NVME_CAP_AMS(cap)) {
		pr_debug("\t [%s] Support Weight Round Robin with Urgent Priority\n",
			(NVME_CAP_AMS(cap) & NVME_CAP_AMS_WRRUPC) ? "Y" : "N");
		pr_debug("\t [%s] Support Vendor Specific\n",
			(NVME_CAP_AMS(cap) & NVME_CAP_AMS_VENDOR) ? "Y" : "N");
	}

	pr_debug("Doorbell Stride: %u bytes\n", 
				1 << (2 + NVME_CAP_STRIDE(cap)));
	pr_debug("[%s] Support NVM Subsystem Reset\n",
				NVME_CAP_NSSRC(cap) ? "Y" : "N");
	pr_debug("[%s] Support Command Sets\n",
				NVME_CAP_CSS(cap) ? "Y" : "N");
	pr_debug("[%s] Support Boot Partition\n", 
				NVME_CAP_BPS(cap) ? "Y" : "N");
	pr_debug("Controller Power Scope: %s\n", 
				cap_cps_string(NVME_CAP_CPS(cap)));
	pr_debug("Memory Page Size Minimum: %uKB\n", 
				1 << (2 + NVME_CAP_MPSMIN(cap)));
	pr_debug("Memory Page Size Maximum: %uKB\n", 
				1 << (2 + NVME_CAP_MPSMAX(cap)));
	pr_debug("\n");
}

void nvme_display_cc(uint32_t cc)
{
	pr_debug("~~~~~ Controller Configuration: 0x%x ~~~~~\n", cc);
	pr_debug("\n");
}
