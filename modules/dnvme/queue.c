/*
 * NVM Express Compliance Suite
 * Copyright (c) 2011, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/interrupt.h>

#include "nvme.h"
#include "core.h"
#include "io.h"
#include "cmd.h"
#include "queue.h"
#include "irq.h"
#include "debug.h"

/**
 * @brief Find the SQ node in the given SQ list by the given SQID
 * 
 * @param ctx NVMe device context
 * @param id submission queue identify
 * @return pointer to the SQ node on success. Otherwise returns NULL.
 */
struct nvme_sq *dnvme_find_sq(struct nvme_context *ctx, u16 id)
{
	struct nvme_sq *sq;

	list_for_each_entry(sq, &ctx->sq_list, sq_entry) {
		if (id == sq->pub.sq_id)
			return sq;
	}
	return NULL;
}

/**
 * @brief Find the CQ node in the given CQ list by the given CQID
 * 
 * @param ctx NVMe device context
 * @param id completion queue identify
 * @return pointer to the CQ node on success. Otherwise returns NULL.
 */
struct nvme_cq *dnvme_find_cq(struct nvme_context *ctx, u16 id)
{
	struct nvme_cq *cq;

	list_for_each_entry(cq, &ctx->cq_list, cq_entry) {
		if (id == cq->pub.q_id)
			return cq;
	}
	return NULL;
}


/**
 * @brief Check whether the Queue ID is unique.
 * 
 * @return 0 if qid is unique, otherwise a negative errno
 */
int dnvme_check_qid_unique(struct nvme_context *ctx, 
	enum nvme_queue_type type, u16 id)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_sq *sq;
	struct nvme_cq *cq;

	if (type == NVME_SQ) {
		sq = dnvme_find_sq(ctx, id);
		if (sq) {
			dnvme_err(ndev, "SQ ID(%u) already exists!\n", id);
			return -EEXIST;
		}
	} else if (type == NVME_CQ) {
		cq = dnvme_find_cq(ctx, id);
		if (cq) {
			dnvme_err(ndev, "SQ ID(%u) already exists!\n", id);
			return -EEXIST;
		}
	}
	return 0;
}

/**
 * @brief Find the cmd node in SQ list by the given ID
 * 
 * @param sq submission queue
 * @param id command identify
 * @return pointer to the cmd node on success. Otherwise returns NULL.
 */
struct nvme_cmd *dnvme_find_cmd(struct nvme_sq *sq, u16 id)
{
	struct nvme_cmd *cmd;

	list_for_each_entry(cmd, &sq->priv.cmd_list, entry) {
		if (id == cmd->id)
			return cmd;
	}
	return NULL;
}

/**
 * @brief Find the meta node in the given meta list by the given ID
 * 
 * @param ctx NVMe device context
 * @param id meta identify
 * @return pointer to the meta node on success. Otherwise returns NULL.
 */
struct nvme_meta *dnvme_find_meta(struct nvme_context *ctx, u32 id)
{
	struct nvme_meta *meta;

	list_for_each_entry(meta, &ctx->meta_set.meta_list, entry) {
		if (id == meta->id)
			return meta;
	}
	return NULL;
}

/**
 * @brief Delete the cmd node from the SQ list and free memory.
 * 
 * @param sq The submission queue where the command resides.
 * @param cmd_id command identify
 */
static void dnvme_delete_cmd(struct nvme_cmd *cmd)
{
	if (unlikely(!cmd))
		return;

	list_del(&cmd->entry);
	kfree(cmd);
}

void dnvme_update_sq_tail(struct nvme_sq *sq)
{
	
}

struct nvme_sq *dnvme_alloc_sq(struct nvme_context *ctx, 
	struct nvme_prep_sq *prep, u8 sqes)
{
	struct nvme_sq *sq;
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->pdev;
	void *sq_buf;
	u32 sq_size;
	dma_addr_t dma;

	sq = kzalloc(sizeof(*sq), GFP_KERNEL);
	if (!sq) {
		dnvme_err(ndev, "failed to alloc nvme_sq!\n");
		return NULL;
	}

	sq_size = prep->elements << sqes;

	if (prep->contig) {
		sq_buf = dma_alloc_coherent(&pdev->dev, sq_size, &dma, GFP_KERNEL);
		if (!sq_buf) {
			dnvme_err(ndev, "failed to alloc DMA addr for SQ!\n");
			goto out;
		}
		memset(sq_buf, 0, sq_size);

		sq->priv.buf = sq_buf;
		sq->priv.dma = dma;
	}

	sq->ctx = ctx;
	sq->pub.sq_id = prep->sq_id;
	sq->pub.cq_id = prep->cq_id;
	sq->pub.elements = prep->elements;
	sq->pub.sqes = sqes;

	INIT_LIST_HEAD(&sq->priv.cmd_list);
	sq->priv.size = sq_size;
	sq->priv.unique_cmd_id = 0;
	sq->priv.contig = prep->contig;
	sq->priv.dbs = &ndev->dbs[prep->sq_id * 2 * ndev->db_stride];

	dnvme_print_sq(sq);

	list_add_tail(&sq->sq_entry, &ctx->sq_list);
	return sq;
out:
	kfree(sq);
	return NULL;
}

void dnvme_release_sq(struct nvme_context *ctx, struct nvme_sq *sq)
{
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->pdev;

	if (unlikely(!sq))
		return;

	dnvme_delete_cmd_list(ndev, sq);

	if (sq->priv.contig) {
		dma_free_coherent(&pdev->dev, sq->priv.size, sq->priv.buf, 
			sq->priv.dma);
	} else {
		dnvme_release_prps(ndev, &sq->priv.prps);
	}

	list_del(&sq->sq_entry);
	kfree(sq);
}

/**
 * @brief Delete the given SQ node from the linked list and free memory.
 */
static void dnvme_delete_sq(struct nvme_context *ctx, u16 sq_id)
{
	struct nvme_sq *sq;

	sq = dnvme_find_sq(ctx, sq_id);
	if (!sq) {
		dnvme_vdbg(ctx->dev, "SQ(%u) doesn't exist!\n", sq_id);
		return;
	}

	dnvme_release_sq(ctx, sq);
}

struct nvme_cq *dnvme_alloc_cq(struct nvme_context *ctx, 
	struct nvme_prep_cq *prep, u8 cqes)
{
	struct nvme_cq *cq;
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->pdev;
	void *cq_buf;
	u32 cq_size;
	dma_addr_t dma;

	cq = kzalloc(sizeof(*cq), GFP_KERNEL);
	if (!cq) {
		dnvme_err(ndev, "failed to alloc nvme_cq!\n");
		return NULL;
	}

	cq_size = prep->elements << cqes;

	if (prep->contig) {
		cq_buf = dma_alloc_coherent(&pdev->dev, cq_size, &dma, GFP_KERNEL);
		if (!cq_buf) {
			dnvme_err(ndev, "failed to alloc DMA addr for CQ!\n");
			goto out;
		}
		memset(cq_buf, 0, cq_size);

		cq->priv.buf = cq_buf;
		cq->priv.dma = dma;
	}

	cq->ctx = ctx;
	cq->pub.q_id = prep->cq_id;
	cq->pub.elements = prep->elements;
	cq->pub.cqes = cqes;
	cq->pub.irq_no = prep->cq_irq_no;
	cq->pub.irq_enabled = prep->cq_irq_en;
	cq->pub.pbit_new_entry = 1;

	cq->priv.size = cq_size;
	cq->priv.contig = prep->contig;
	cq->priv.dbs = &ndev->dbs[(prep->cq_id * 2 + 1) * ndev->db_stride];

	dnvme_print_cq(cq);

	list_add_tail(&cq->cq_entry, &ctx->cq_list);
	return cq;
out:
	kfree(cq);
	return NULL;
}

void dnvme_release_cq(struct nvme_context *ctx, struct nvme_cq *cq)
{
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->pdev;

	if (unlikely(!cq))
		return;

	if (cq->priv.contig) {
		dma_free_coherent(&pdev->dev, cq->priv.size, cq->priv.buf, 
			cq->priv.dma);
	} else {
		dnvme_release_prps(ndev, &cq->priv.prps);
	}

	list_del(&cq->cq_entry);
	kfree(cq);
}

/**
 * @brief Delete the given CQ node from the linked list and free memory.
 */
static void dnvme_delete_cq(struct nvme_context *ctx, u16 cq_id)
{
	struct nvme_cq *cq;

	cq = dnvme_find_cq(ctx, cq_id);
	if (!cq) {
		dnvme_vdbg(ctx->dev, "CQ(%u) doesn't exist!\n", cq_id);
		return;
	}

	/* try to delete icq node associated with the CQ */
	dnvme_delete_icq_node(&ctx->irq_set, cq_id, cq->pub.irq_no);

	dnvme_release_cq(ctx, cq);
}


int dnvme_create_asq(struct nvme_context *ctx, u32 elements)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_sq *sq;
	struct nvme_prep_sq prep;
	void __iomem *bar0 = ndev->bar0;
	u32 cc, aqa;
	u16 asq_id = 0; /* admin queue ID is always 0 */
	int ret;

	if (!elements || elements > NVME_AQ_MAX_SIZE) {
		dnvme_err(ndev, "ASQ elements(%u) is invalid!\n", elements);
		return -EINVAL;
	}

	ret = dnvme_check_qid_unique(ctx, NVME_SQ, asq_id);
	if (ret < 0)
		return ret;

	cc = dnvme_readl(bar0, NVME_REG_CC);
	if (cc & NVME_CC_ENABLE) {
		dnvme_err(ndev, "NVMe already enabled!\n");
		return -EPERM;
	}

	prep.elements = elements;
	prep.sq_id = asq_id;
	prep.cq_id = 0;
	prep.contig = 1;

	sq = dnvme_alloc_sq(ctx, &prep, NVME_ADM_SQES);
	if (!sq)
		return -ENOMEM;

	/* Notify NVMe controller */
	aqa = dnvme_readl(bar0, NVME_REG_AQA);
	aqa &= ~NVME_AQA_ASQS_MASK;
	aqa |= NVME_AQA_FOR_ASQS(elements - 1);
	dnvme_writel(bar0, NVME_REG_AQA, aqa);

	dnvme_writeq(bar0, NVME_REG_ASQ, sq->priv.dma);

	dnvme_dbg(ndev, "WRITE AQA:0x%x, ASQ:0x%llx\n", aqa, sq->priv.dma);
	return 0;
}

int dnvme_create_acq(struct nvme_context *ctx, u32 elements)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_cq *cq;
	struct nvme_prep_cq prep;
	void __iomem *bar0 = ndev->bar0;
	u32 cc, aqa;
	u16 acq_id = 0; /* admin queue ID is always 0 */
	int ret;

	if (!elements || elements > NVME_AQ_MAX_SIZE) {
		dnvme_err(ndev, "ACQ elements(%u) is invalid!\n", elements);
		return -EINVAL;
	}

	ret = dnvme_check_qid_unique(ctx, NVME_CQ, acq_id);
	if (ret < 0)
		return ret;

	cc = dnvme_readl(bar0, NVME_REG_CC);
	if (cc & NVME_CC_ENABLE) {
		dnvme_err(ndev, "NVMe already enabled!\n");
		return -EPERM;
	}

	prep.elements = elements;
	prep.cq_id = acq_id;
	prep.contig = 1;
	prep.cq_irq_en = 1;
	prep.cq_irq_no = 0;

	cq = dnvme_alloc_cq(ctx, &prep, NVME_ADM_CQES);
	if (!cq)
		return -ENOMEM;

	/* Notify NVMe controller */
	aqa = dnvme_readl(bar0, NVME_REG_AQA);
	aqa &= ~NVME_AQA_ACQS_MASK;
	aqa |= NVME_AQA_FOR_ACQS(elements - 1);
	dnvme_writel(bar0, NVME_REG_AQA, aqa);

	dnvme_writeq(bar0, NVME_REG_ACQ, cq->priv.dma);

	dnvme_dbg(ndev, "WRITE AQA:0x%x, ACQ:0x%llx\n", aqa, cq->priv.dma);
	return 0;
}

/*
 * @brief Reinitialize the admin Submission queue's public parameters, when
 *  a controller is not completely disabled
 */
static void dnvme_reinit_asq(struct nvme_context *ctx, struct nvme_sq *sq)
{
	/* free cmd list for admin sq */
	dnvme_delete_cmd_list(ctx->dev, sq);

	sq->pub.head_ptr = 0;
	sq->pub.tail_ptr = 0;
	sq->pub.tail_ptr_virt = 0;
	sq->priv.unique_cmd_id = 0;
	memset(sq->priv.buf, 0, sq->priv.size);
}

/*
 * @brief Reinitialize the admin completion queue's public parameters, when
 *  a controller is not completely disabled
 */
static void dnvme_reinit_acq(struct nvme_cq *cq)
{
	cq->pub.head_ptr = 0;
	cq->pub.tail_ptr = 0;
	cq->pub.pbit_new_entry = 1;
	cq->pub.irq_enabled = 1;
	memset(cq->priv.buf, 0, cq->priv.size);
}

int dnvme_ring_sq_doorbell(struct nvme_context *ctx, u16 sq_id)
{
	struct nvme_sq *sq;

	sq = dnvme_find_sq(ctx, sq_id);
	if (!sq) {
		dnvme_err(ctx->dev, "SQ(%u) doesn't exist!\n", sq_id);
		return -EINVAL;
	}

	dnvme_dbg(ctx->dev, "RING SQ(%u) %u => %lx (old:%u)\n", sq_id, 
		sq->pub.tail_ptr_virt, (unsigned long)sq->priv.dbs,
		sq->pub.tail_ptr);
	sq->pub.tail_ptr = sq->pub.tail_ptr_virt;
	writel(sq->pub.tail_ptr, sq->priv.dbs);
	return 0;
}

/**
 * @brief Delete all queues, unbind from nvme_context and free memory.
 *
 * @param state set NVME_ST_DISABLE_COMPLETE if you want to save Admin Queue.
 */
void dnvme_delete_all_queues(struct nvme_context *ctx, enum nvme_state state)
{
	struct nvme_sq *sq;
	struct nvme_sq *sq_tmp;
	struct nvme_cq *cq;
	struct nvme_cq *cq_tmp;
	void *bar0 = ctx->dev->bar0;
	bool save_aq = (state == NVME_ST_DISABLE_COMPLETE) ? false : true;

	list_for_each_entry_safe(sq, sq_tmp, &ctx->sq_list, sq_entry) {
		if (save_aq && sq->pub.sq_id == NVME_AQ_ID) {
			dnvme_vdbg(ctx->dev, "Retaining ASQ from deallocation\n");
			/* drop sq cmds and set to zero the public metrics of asq */
			dnvme_reinit_asq(ctx, sq);
		} else {
			dnvme_release_sq(ctx, sq);
		}
	}

	list_for_each_entry_safe(cq, cq_tmp, &ctx->cq_list, cq_entry) {
		if (save_aq && cq->pub.q_id == NVME_AQ_ID) {
			dnvme_vdbg(ctx->dev, "Retaining ACQ from deallocation");
			/* set to zero the public metrics of acq */
			dnvme_reinit_acq(cq);
		} else {
			dnvme_release_cq(ctx, cq);
		}
	}

	/* if complete disable then reset the controller admin registers. */
	if (!save_aq) {
		/* Set the Registers to default values. */
		dnvme_writel(bar0, NVME_REG_AQA, 0);
		dnvme_writeq(bar0, NVME_REG_ASQ, 0);
		dnvme_writeq(bar0, NVME_REG_ACQ, 0);
	}
}


/**
 * @brief Try to inquire the number of cmd in the CQ that are waiting
 *  to be reaped for any given q_id.
 */
u32 dnvme_get_cqe_remain(struct nvme_cq *cq, struct device *dev)
{
	struct nvme_context *ctx = cq->ctx;
	struct nvme_prps *prps = &cq->priv.prps;
	struct nvme_completion *entry;
	void *cq_addr;
	u32 remain = 0;
	u8 phase = cq->pub.pbit_new_entry;

	if (cq->priv.contig) {
		cq_addr = cq->priv.buf;
	} else {
		dma_sync_sg_for_cpu(dev, prps->sg, prps->num_map_pgs, 
			prps->data_dir);
		cq_addr = cq->priv.prps.buf;
	}

	/* Start from head ptr and update till phase bit incorrect */
	cq->pub.tail_ptr = cq->pub.head_ptr;
	entry = (struct nvme_completion *)cq_addr + cq->pub.tail_ptr;

	/* loop through the entries in the cq */
	while (NVME_CQE_STATUS_TO_PHASE(entry->status) == phase) {

		remain++;
		cq->pub.tail_ptr++;

		/* Q wrapped around? */
		if (cq->pub.tail_ptr >= cq->pub.elements) {
			phase ^= 1;
			cq->pub.tail_ptr = 0;
		}
		entry = (struct nvme_completion *)cq_addr + cq->pub.tail_ptr;
	}

	dnvme_vdbg(ctx->dev, "Inquiry CQ(%u) element:%u, head:%u, tail:%u, remain:%u\n",
		cq->pub.q_id, cq->pub.elements, cq->pub.head_ptr,
		cq->pub.tail_ptr, remain);
	return remain;
}


/**
 * @brief Inquire the number of CQ entries that are waiting to be reaped.
 */
int dnvme_inquiry_cqe(struct nvme_context *ctx, struct nvme_inquiry __user *uinq)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_cq *cq;
	struct nvme_inquiry inquiry;

	if (copy_from_user(&inquiry, uinq, sizeof(inquiry))) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		return -EFAULT;
	}

	cq = dnvme_find_cq(ctx, inquiry.cqid);
	if (!cq) {
		dnvme_err(ndev, "CQ(%u) doesn't exist!\n", inquiry.cqid);
		return -EINVAL;
	}

	inquiry.nr_cqe = dnvme_get_cqe_remain(cq, &ndev->pdev->dev);

	if (copy_to_user(uinq, &inquiry, sizeof(inquiry))) {
		dnvme_err(ndev, "failed to copy to user space!\n");
		return -EFAULT;
	}

	/* Check for hw violation of full Q definition */
	if (inquiry.nr_cqe >= cq->pub.elements) {
		dnvme_err(ndev, "HW violating full Q definition!\n");
		return -EPERM;
	}

	return 0;
}

/**
 * @brief Handle create/delete IO queue command completion 
 */
static int handle_queue_cmd_completion(struct nvme_sq *sq, struct nvme_cmd *cmd,
	enum nvme_queue_type type, bool free_queue)
{
	struct nvme_context *ctx = sq->ctx;
	struct nvme_device *ndev = ctx->dev;
	int ret = 0;

	dnvme_vdbg(ndev, "Queue:%u, CMD:%u, Free:%s\n", cmd->target_qid,
		cmd->id, free_queue ? "true" : "false");

	if (!free_queue)
		goto del_cmd;

	if (cmd->target_qid == NVME_AQ_ID) {
		dnvme_err(ndev, "Trying to delete Admin Queue is blunder!\n");
		ret = -EINVAL;
		goto del_cmd;
	}

	if (type == NVME_CQ) {
		dnvme_delete_cq(ctx, cmd->target_qid);
	} else if (type == NVME_SQ) {
		dnvme_delete_sq(ctx, cmd->target_qid);
	}

del_cmd:
	dnvme_delete_cmd(cmd);
	return ret;
}

/**
 * @brief Handle general command completion. Just delete cmd node in SQ.
 */
static int handle_gen_cmd_completion(struct nvme_sq *sq, struct nvme_cmd *node)
{
	dnvme_release_prps(sq->ctx->dev, &node->prps);
	dnvme_delete_cmd(node);
	return 0;
}

/**
 * @brief Handle admin command completion.
 */
static int handle_admin_cmd_completion(struct nvme_sq *sq, struct nvme_cmd *cmd,
	u16 status)
{
	int ret;

	switch (cmd->opcode) {
	case nvme_admin_delete_sq:
		ret = handle_queue_cmd_completion(sq, cmd, NVME_SQ, (status == 0));
		break;
	case nvme_admin_create_sq:
		ret = handle_queue_cmd_completion(sq, cmd, NVME_SQ, (status != 0));
		break;
	case nvme_admin_delete_cq:
		ret = handle_queue_cmd_completion(sq, cmd, NVME_CQ, (status == 0));
		break;
	case nvme_admin_create_cq:
		ret = handle_queue_cmd_completion(sq, cmd, NVME_CQ, (status != 0));
		break;
	default:
		ret = handle_gen_cmd_completion(sq, cmd);
		break;
	}
	return ret;
}

/**
 * @brief Handle all command completion which include admin & IO command etc.
 */
static int handle_cmd_completion(struct nvme_context *ctx, 
	struct nvme_completion *cq_entry)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_sq *sq;
	struct nvme_cmd *cmd;
	u16 status;
	int ret = 0;

	sq = dnvme_find_sq(ctx, cq_entry->sq_id);
	if (!sq) {
		dnvme_err(ndev, "SQ(%u) doesn't exist!\n", cq_entry->sq_id);
		return -EBADSLT;
	}

	/* update SQ info */
	sq->pub.head_ptr = cq_entry->sq_head;
	status = (NVME_CQE_STATUS_TO_STATE(cq_entry->status) & 0x7ff);

	cmd = dnvme_find_cmd(sq, cq_entry->command_id);
	if (!cmd) {
		dnvme_err(ndev, "CMD(%u) doesn't exist in SQ(%u)!\n",
			cq_entry->command_id, cq_entry->sq_id);
		return -EBADSLT;
	}

	dnvme_vdbg(ndev, "SQ %u, CMD %u - opcode:0x%x, status:0x%x\n",
		cq_entry->sq_id, cq_entry->command_id, 
		cmd->opcode, NVME_CQE_STATUS_TO_STATE(cq_entry->status));

	if (cq_entry->sq_id == NVME_AQ_ID) {
		ret = handle_admin_cmd_completion(sq, cmd, status);
	} else {
		ret = handle_gen_cmd_completion(sq, cmd);
	}

	return ret;
}

/**
 * @brief Copy the cq data to user buffer for the elements reaped.
 */
static int copy_cq_data(struct nvme_cq *cq, u32 *nr_reap, u8 *buffer)
{
	struct nvme_context *ctx = cq->ctx;
	struct nvme_device *ndev = ctx->dev;
	void *cq_head;
	void *cq_base;
	u32 cqes = 1 << cq->pub.cqes;
	int latentErr = 0;

	if (cq->priv.contig)
		cq_base = cq->priv.buf;
	else
		cq_base = cq->priv.prps.buf;

	cq_head = cq_base + ((u32)cq->pub.head_ptr << cq->pub.cqes);

	while (*nr_reap) {
		dnvme_vdbg(ndev, "Reaping CE's, %d left to reap", *nr_reap);

		/* Call the process reap algos based on CE entry */
		latentErr = handle_cmd_completion(ctx, cq_head);
		if (latentErr) {
			dnvme_err(ndev, "Unable to find CE.SQ_id in dnvme metrics");
		}

		/* Copy to user even on err; allows seeing latent err */
		if (copy_to_user(buffer, cq_head, cqes)) {
			dnvme_err(ndev, "Unable to copy request data to user space");
			return -EFAULT;
		}

		cq_head += cqes;     /* Point to next CE entry */
		buffer += cqes;          /* Prepare for next element */
		*nr_reap -= 1;              /* decrease for the one reaped. */

		if (cq_head >= (cq_base + cq->priv.size)) {
			/* Q wrapped so point to base again */
			cq_head = cq_base;
		}

		if (latentErr) {
			/* Latent errors were introduced to allow reaping CE's to user
			* space and also counting them as reaped, because they were
			* successfully copied. However, there was something about the CE
			* that indicated an error, possibly malformed CE by hdw, thus the
			* entire IOCTL should error, but we successfully reaped some CE's
			* which allows tnvme to inspect and trust the copied CE's for debug
			*/
			dnvme_err(ndev, "Detected a partial reap situation; some, not all reaped");
			return latentErr;
		}
	}

	return 0;
}

/**
 * @brief Update the cq head pointer to point to location of the elements
 *  that is to be reaped.
 */
static void update_cq_head(struct nvme_cq *cq, u32 num_reaped)
{
	struct nvme_context *ctx = cq->ctx;
	u32 head = cq->pub.head_ptr;

	head += num_reaped;
	if (head >= cq->pub.elements) {
		cq->pub.pbit_new_entry = !cq->pub.pbit_new_entry;
		head = head % cq->pub.elements;
	}

	cq->pub.head_ptr = (u16)head;

	dnvme_vdbg(ctx->dev, "CQ(%u) head:%u, tail:%u\n", cq->pub.q_id, 
		cq->pub.head_ptr, cq->pub.tail_ptr);
}

int dnvme_reap_cqe(struct nvme_context *ctx, struct nvme_reap __user *ureap)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_interrupt *act_irq = &ndev->pub.irq_active;
	struct pci_dev *pdev = ndev->pdev;
	struct nvme_reap reap;
	struct nvme_cq *cq;
	u32 cqes, expect, actual, remain;
	int ret;

	if (copy_from_user(&reap, ureap, sizeof(reap))) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		return -EFAULT;
	}

	cq = dnvme_find_cq(ctx, reap.cqid);
	if (!cq) {
		dnvme_err(ndev, "CQ(%u) doesn't exist!\n", reap.cqid);
		return -EBADSLT;
	}
	cqes = 1 << cq->pub.cqes;

	/* calculate the number of CQ entries that the user expects to reap */
	if (reap.expect)
		expect = min_t(u32, reap.expect, reap.size >> cq->pub.cqes);
	else
		expect = reap.size >> cq->pub.cqes;

	remain = dnvme_get_cqe_remain(cq, &pdev->dev);
	if (remain >= cq->pub.elements) {
		dnvme_err(ndev, "HW violating full Q definition!\n");
		return -EPERM;
	}

	reap.reaped = min_t(u32, expect, remain);
	reap.remained = remain - reap.reaped;
	actual = reap.reaped;

	ret = copy_cq_data(cq, &actual, reap.buf);

	reap.reaped -= actual;
	reap.remained += actual;

	/* update data to user */
	if (copy_to_user(ureap, &reap, sizeof(reap))) {
		dnvme_err(ndev, "failed to copy to user space!\n");
		return (ret == 0) ? -EFAULT : ret;
	}

	if (reap.reaped) {
		update_cq_head(cq, reap.reaped);
		writel(cq->pub.head_ptr, cq->priv.dbs);
	}

	if (act_irq->irq_type != NVME_INT_NONE && cq->pub.irq_enabled == 1 &&
		reap.remained == 0) {

		ret = dnvme_reset_isr_flag(ctx, cq->pub.irq_no);
		if (ret < 0) {
			dnvme_err(ndev, "reset isr fired flag failed\n");
			return ret;
		}
		dnvme_unmask_interrupt(&ctx->irq_set, cq->pub.irq_no);
	}

	return 0;
}

