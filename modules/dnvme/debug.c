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
