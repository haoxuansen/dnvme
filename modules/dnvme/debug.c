/**
 * @file debug.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-11-23
 * 
 * @copyright Copyright (c) 2022
 * 
 */

// #define DEBUG

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include "core.h"
#include "debug.h"

#define DBG_STR_SIZE			1024

static char dbgstr[DBG_STR_SIZE];

void dnvme_print_ccmd(struct nvme_common_command *ccmd)
{
	char *buf = dbgstr;
	int oft = 0;

	oft = snprintf(buf, DBG_STR_SIZE, "opcode: 0x%x\n", ccmd->opcode);

	oft += snprintf(buf + oft, DBG_STR_SIZE - oft, 
		"flags: 0x%x\n", ccmd->flags);
	oft += snprintf(buf + oft, DBG_STR_SIZE - oft, 
		"command_id: %u\n", ccmd->command_id);
	oft += snprintf(buf + oft, DBG_STR_SIZE - oft, 
		"nsid: 0x%x\n", le32_to_cpu(ccmd->nsid));
	oft += snprintf(buf + oft, DBG_STR_SIZE - oft, 
		"cdw2: 0x%x 0x%x\n", le32_to_cpu(ccmd->cdw2[0]), 
		le32_to_cpu(ccmd->cdw2[1]));
	oft += snprintf(buf + oft, DBG_STR_SIZE - oft, 
		"metadata: 0x%llx\n", le64_to_cpu(ccmd->metadata));
	oft += snprintf(buf + oft, DBG_STR_SIZE - oft, 
		"prp1: 0x%llx\n", le64_to_cpu(ccmd->dptr.prp1));
	oft += snprintf(buf + oft, DBG_STR_SIZE - oft, 
		"prp2: 0x%llx\n", le64_to_cpu(ccmd->dptr.prp2));
	oft += snprintf(buf + oft, DBG_STR_SIZE - oft, 
		"cdw10: 0x%x\n", le32_to_cpu(ccmd->cdw10));
	oft += snprintf(buf + oft, DBG_STR_SIZE - oft, 
		"cdw11: 0x%x\n", le32_to_cpu(ccmd->cdw11));
	oft += snprintf(buf + oft, DBG_STR_SIZE - oft, 
		"cdw12: 0x%x\n", le32_to_cpu(ccmd->cdw12));
	oft += snprintf(buf + oft, DBG_STR_SIZE - oft, 
		"cdw13: 0x%x\n", le32_to_cpu(ccmd->cdw13));
	oft += snprintf(buf + oft, DBG_STR_SIZE - oft, 
		"cdw14: 0x%x\n", le32_to_cpu(ccmd->cdw14));
	oft += snprintf(buf + oft, DBG_STR_SIZE - oft, 
		"cdw15: 0x%x\n", le32_to_cpu(ccmd->cdw15));
	dnvme_dbg(ndev, "%s", buf);
}

void dnvme_print_sq(struct nvme_sq *sq)
{
	char *buf = dbgstr;
	int oft = 0;

	oft = snprintf(buf, DBG_STR_SIZE, "SQ(%u) bind to CQ(%u)...$\n", 
		sq->pub.sq_id, sq->pub.cq_id);

	oft += snprintf(buf + oft, DBG_STR_SIZE - oft, 
		"\tbuf:0x%llx, dma:0x%llx, size:%u\n",
		(dma_addr_t)sq->priv.buf, sq->priv.dma, sq->priv.size);
	oft += snprintf(buf + oft, DBG_STR_SIZE - oft,
		"\tdbs:0x%llx, slot:%u, sqes:%u\n",
		(dma_addr_t)sq->priv.dbs, sq->pub.elements, sq->pub.sqes);
	dnvme_dbg(ndev, "%s", buf);
}

void dnvme_print_cq(struct nvme_cq *cq)
{
	char *buf = dbgstr;
	int oft = 0;

	oft = snprintf(buf, DBG_STR_SIZE, "CQ(%u)...$\n", cq->pub.q_id);

	oft += snprintf(buf + oft, DBG_STR_SIZE - oft, 
		"\tbuf:0x%llx, dma:0x%llx, size:%u\n",
		(dma_addr_t)cq->priv.buf, cq->priv.dma, cq->priv.size);
	oft += snprintf(buf + oft, DBG_STR_SIZE - oft,
		"\tdbs:0x%llx, slot:%u, cqes:%u\n",
		(dma_addr_t)cq->priv.dbs, cq->pub.elements, cq->pub.cqes);
	dnvme_dbg(ndev, "%s", buf);
}

const char *dnvme_ioctl_cmd_string(unsigned int cmd)
{
	switch (cmd) {
	case NVME_IOCTL_GET_DRIVER_INFO:
		return "NVME_GET_DRIVER_INFO";
	case NVME_IOCTL_GET_DEVICE_INFO:
		return "NVME_GET_DEVICE_INFO";
	case NVME_IOCTL_GET_SQ_INFO:
		return "NVME_GET_SQ_INFO";
	case NVME_IOCTL_GET_CQ_INFO:
		return "NVME_GET_CQ_INFO";
	case NVME_IOCTL_GET_CAPABILITY:
		return "NVME_GET_CAPABILITY";

	case NVME_IOCTL_READ_GENERIC:
		return "NVME_READ_GENERIC";
	case NVME_IOCTL_WRITE_GENERIC:
		return "NVME_WRITE_GENERIC";

	case NVME_IOCTL_SET_DEV_STATE:
		return "NVME_SET_DEV_STATE";

	case NVME_IOCTL_CREATE_ADMIN_QUEUE:
		return "NVME_CREATE_ADMIN_QUEUE";

	case NVME_IOCTL_PREPARE_IOSQ:
		return "NVME_PREPARE_IOSQ";
	case NVME_IOCTL_PREPARE_IOCQ:
		return "NVME_PREPARE_IOCQ";

	case NVME_IOCTL_RING_SQ_DOORBELL:
		return "NVME_RING_SQ_DOORBELL";

	case NVME_IOCTL_SUBMIT_64B_CMD:
		return "NVME_SUBMIT_64B_CMD";

	case NVME_IOCTL_INQUIRY_CQE:
		return "NVME_INQUIRY_CQE";
	case NVME_IOCTL_REAP_CQE:
		return "NVME_REAP_CQE";

	case NVME_IOCTL_CREATE_META_NODE:
		return "NVME_CREATE_META_NODE";
	case NVME_IOCTL_DELETE_META_NODE:
		return "NVME_DELETE_META_NODE";
	case NVME_IOCTL_CREATE_META_POOL:
		return "NVME_CREATE_META_POOL";
	case NVME_IOCTL_DESTROY_META_POOL:
		return "NVME_DESTROY_META_POOL";

	case NVME_IOCTL_DUMP_LOG_FILE:
		return "NVME_DUMP_LOG_FILE";

	default:
		return "UNKNOWN";
	}
}

