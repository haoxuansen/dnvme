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

#define DEBUG

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include "core.h"
#include "dnvme_ds.h"
#include "dnvme_ioctl.h"

static char dbgstr[1024];

void dnvme_print_sq(struct nvme_sq *sq)
{
	char *buf = dbgstr;
	int oft = 0;

	oft = sprintf(buf, "SQ(%u) bind to CQ(%u)...$\n", 
		sq->pub.sq_id, sq->pub.cq_id);

	oft += sprintf(buf + oft, "\tbuf:0x%llx, dma:0x%llx, size:%u\n",
		(dma_addr_t)sq->priv.buf, sq->priv.dma, sq->priv.size);
	oft += sprintf(buf + oft, "\tdbs:0x%llx, slot:%u, sqes:%u\n",
		(dma_addr_t)sq->priv.dbs, sq->pub.elements, sq->pub.sqes);
	dnvme_dbg("%s", buf);
}

void dnvme_print_cq(struct nvme_cq *cq)
{
	char *buf = dbgstr;
	int oft = 0;

	oft = sprintf(buf, "CQ(%u)...$\n", cq->pub.q_id);

	oft += sprintf(buf + oft, "\tbuf:0x%llx, dma:0x%llx, size:%u\n",
		(dma_addr_t)cq->priv.buf, cq->priv.dma, cq->priv.size);
	oft += sprintf(buf + oft, "\tdbs:0x%llx, slot:%u, cqes:%u\n",
		(dma_addr_t)cq->priv.dbs, cq->pub.elements, cq->pub.cqes);
	dnvme_dbg("%s", buf);
}

const char *dnvme_ioctl_cmd_string(unsigned int cmd)
{
	switch (cmd) {
	case NVME_IOCTL_GET_DRIVER_INFO:
		return "NVME_GET_DRIVER_INFO";
	case NVME_GET_DEV_INFO:
		return "NVME_GET_DEV_INFO";
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

	case NVME_IOCTL_PREPARE_SQ_CREATION:
		return "NVME_PREPARE_SQ_CREATION";
	case NVME_IOCTL_PREPARE_CQ_CREATION:
		return "NVME_PREPARE_CQ_CREATION";

	case NVME_IOCTL_GET_QUEUE:
		return "NVME_GET_QUEUE";

	case NVME_IOCTL_RING_SQ_DOORBELL:
		return "NVME_RING_SQ_DOORBELL";

	case NVME_IOCTL_SEND_64B_CMD:
		return "NVME_SEND_64B_CMD";

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

