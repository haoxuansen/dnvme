/**
 * @file queue.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-04-07
 * 
 * @copyright Copyright (c) 2023
 * 
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
#include <linux/pci-p2pdma.h>

#include "nvme.h"
#include "core.h"
#include "trace.h"
#include "io.h"
#include "queue.h"
#include "irq.h"
#include "debug.h"

/**
 * @brief Check whether the Queue ID is unique.
 * 
 * @return 0 if qid is unique, otherwise a negative errno
 */
int dnvme_check_qid_unique(struct nvme_device *ndev, 
	enum nvme_queue_type type, u16 id)
{
	struct nvme_sq *sq;
	struct nvme_cq *cq;

	if (type == NVME_SQ) {
		sq = dnvme_find_sq(ndev, id);
		if (sq) {
			dnvme_err(ndev, "SQ ID(%u) already exists!\n", id);
			return -EEXIST;
		}
	} else if (type == NVME_CQ) {
		cq = dnvme_find_cq(ndev, id);
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

	list_for_each_entry(cmd, &sq->cmd_list, entry) {
		if (id == cmd->id)
			return cmd;
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

struct nvme_sq *dnvme_alloc_sq(struct nvme_device *ndev, 
	struct nvme_prep_sq *prep, u8 sqes)
{
	struct nvme_sq *sq;
	struct pci_dev *pdev = ndev->pdev;
	void *sq_buf;
	u32 sq_size;
	dma_addr_t dma;
	int ret;

	sq = kzalloc(sizeof(*sq), GFP_KERNEL);
	if (!sq) {
		dnvme_err(ndev, "failed to alloc nvme_sq!\n");
		return NULL;
	}

	sq_size = prep->elements << sqes;

	if (prep->contig) {
		if (prep->use_cmb) {
			if (!dnvme_cmb_support_sq(ndev->cmb)) {
				dnvme_err(ndev, "Not support SQ in CMB!\n");
				goto out;
			}

			sq_buf = pci_alloc_p2pmem(pdev, sq_size);
			if (!sq_buf) {
				dnvme_err(ndev, "failed to alloc p2pmem for SQ!\n");
				goto out;
			}
			dma = pci_p2pmem_virt_to_bus(pdev, sq_buf);
			if (!dma) {
				pci_free_p2pmem(pdev, sq_buf, sq_size);
				goto out;
			}
			sq->use_cmb = 1;
		} else {
			sq_buf = dma_alloc_coherent(&pdev->dev, sq_size, &dma, GFP_KERNEL);
			if (!sq_buf) {
				dnvme_err(ndev, "failed to alloc DMA addr for SQ!\n");
				goto out;
			}
		}
		memset(sq_buf, 0, sq_size);

		sq->buf = sq_buf;
		sq->dma = dma;
		sq->contig = 1;
	}

	sq->ndev = ndev;
	sq->pub.sq_id = prep->sq_id;
	sq->pub.cq_id = prep->cq_id;
	sq->pub.elements = prep->elements;
	sq->pub.sqes = sqes;

	INIT_LIST_HEAD(&sq->cmd_list);
	sq->size = sq_size;
	sq->next_cid = 0;
	sq->db = &ndev->dbs[prep->sq_id * 2 * ndev->db_stride];

	dnvme_print_sq(sq);
	
	ret = xa_insert(&ndev->sqs, sq->pub.sq_id, sq, GFP_KERNEL);
	if (ret < 0) {
		dnvme_err(ndev, "failed to insert sq:%u!(%d)\n", 
			sq->pub.sq_id, ret);
		goto out2;
	}
	return sq;
out2:
	if (sq->contig) {
		if (sq->use_cmb) {
			pci_free_p2pmem(pdev, sq->buf, sq->size);
		} else {
			dma_free_coherent(&pdev->dev, sq->size, sq->buf, sq->dma);
		}
	}
out:
	kfree(sq);
	return NULL;
}

void dnvme_release_sq(struct nvme_device *ndev, struct nvme_sq *sq)
{
	struct pci_dev *pdev = ndev->pdev;

	if (unlikely(!sq))
		return;

	xa_erase(&ndev->sqs, sq->pub.sq_id);

	dnvme_delete_cmd_list(ndev, sq);

	if (sq->contig) {
		if (sq->use_cmb)
			pci_free_p2pmem(pdev, sq->buf, sq->size);
		else
			dma_free_coherent(&pdev->dev, sq->size, sq->buf, sq->dma);
	} else {
		dnvme_release_prps(ndev, sq->prps);
		sq->prps = NULL;
	}

	kfree(sq);
}

/**
 * @brief Delete the given SQ node from the linked list and free memory.
 */
static void dnvme_delete_sq(struct nvme_device *ndev, u16 sq_id)
{
	struct nvme_sq *sq;

	sq = dnvme_find_sq(ndev, sq_id);
	if (!sq) {
		dnvme_vdbg(ndev, "SQ(%u) doesn't exist!\n", sq_id);
		return;
	}

	dnvme_release_sq(ndev, sq);
}

struct nvme_cq *dnvme_alloc_cq(struct nvme_device *ndev, 
	struct nvme_prep_cq *prep, u8 cqes)
{
	struct nvme_cq *cq;
	struct pci_dev *pdev = ndev->pdev;
	void *cq_buf;
	u32 cq_size;
	dma_addr_t dma;
	int ret;

	cq = kzalloc(sizeof(*cq), GFP_KERNEL);
	if (!cq) {
		dnvme_err(ndev, "failed to alloc nvme_cq!\n");
		return NULL;
	}

	cq_size = prep->elements << cqes;

	if (prep->contig) {
		if (prep->use_cmb) {
			if (!dnvme_cmb_support_cq(ndev->cmb)) {
				dnvme_err(ndev, "Not support CQ in CMB!\n");
				goto out;
			}

			cq_buf = pci_alloc_p2pmem(pdev, cq_size);
			if (!cq_buf) {
				dnvme_err(ndev, "failed to alloc p2pmem for CQ!\n");
				goto out;
			}
			dma = pci_p2pmem_virt_to_bus(pdev, cq_buf);
			if (!dma) {
				pci_free_p2pmem(pdev, cq_buf, cq_size);
				goto out;
			}
			cq->use_cmb = 1;
		} else {
			cq_buf = dma_alloc_coherent(&pdev->dev, cq_size, &dma, GFP_KERNEL);
			if (!cq_buf) {
				dnvme_err(ndev, "failed to alloc DMA addr for CQ!\n");
				goto out;
			}
		}
		memset(cq_buf, 0, cq_size);

		cq->buf = cq_buf;
		cq->dma = dma;
		cq->contig = 1;
	}

	cq->ndev = ndev;
	cq->pub.q_id = prep->cq_id;
	cq->pub.elements = prep->elements;
	cq->pub.cqes = cqes;
	cq->pub.irq_no = prep->cq_irq_no;
	cq->pub.irq_enabled = prep->cq_irq_en;
	cq->pub.pbit_new_entry = 1;

	cq->size = cq_size;
	cq->db = &ndev->dbs[(prep->cq_id * 2 + 1) * ndev->db_stride];

	dnvme_print_cq(cq);

	ret = xa_insert(&ndev->cqs, cq->pub.q_id, cq, GFP_KERNEL);
	if (ret < 0) {
		dnvme_err(ndev, "failed to insert cq:%u!(%d)\n", 
			cq->pub.q_id, ret);
		goto out2;
	}
	return cq;
out2:
	if (cq->contig) {
		if (cq->use_cmb)
			pci_free_p2pmem(pdev, cq->buf, cq->dma);
		else
			dma_free_coherent(&pdev->dev, cq->size, cq->buf, cq->dma);
	}
out:
	kfree(cq);
	return NULL;
}

void dnvme_release_cq(struct nvme_device *ndev, struct nvme_cq *cq)
{
	struct pci_dev *pdev = ndev->pdev;

	if (unlikely(!cq))
		return;

	xa_erase(&ndev->cqs, cq->pub.q_id);

	if (cq->contig) {
		if (cq->use_cmb)
			pci_free_p2pmem(pdev, cq->buf, cq->dma);
		else
			dma_free_coherent(&pdev->dev, cq->size, cq->buf, cq->dma);
	} else {
		dnvme_release_prps(ndev, cq->prps);
		cq->prps = NULL;
	}

	kfree(cq);
}

/**
 * @brief Delete the given CQ node from the linked list and free memory.
 */
static void dnvme_delete_cq(struct nvme_device *ndev, u16 cq_id)
{
	struct nvme_cq *cq;

	cq = dnvme_find_cq(ndev, cq_id);
	if (!cq) {
		dnvme_vdbg(ndev, "CQ(%u) doesn't exist!\n", cq_id);
		return;
	}

	/* try to delete icq node associated with the CQ */
	dnvme_delete_icq_node(&ndev->irq_set, cq_id, cq->pub.irq_no);

	dnvme_release_cq(ndev, cq);
}


int dnvme_create_asq(struct nvme_device *ndev, u32 elements)
{
	struct nvme_sq *sq;
	struct nvme_prep_sq prep;
	void __iomem *bar0 = ndev->bar[0];
	u32 cc, aqa;
	u16 asq_id = 0; /* admin queue ID is always 0 */
	int ret;

	if (!elements || elements > NVME_AQ_MAX_SIZE) {
		dnvme_err(ndev, "ASQ elements(%u) is invalid!\n", elements);
		return -EINVAL;
	}

	ret = dnvme_check_qid_unique(ndev, NVME_SQ, asq_id);
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

	sq = dnvme_alloc_sq(ndev, &prep, NVME_ADM_SQES);
	if (!sq)
		return -ENOMEM;

	/* Notify NVMe controller */
	aqa = dnvme_readl(bar0, NVME_REG_AQA);
	aqa &= ~NVME_AQA_ASQS_MASK;
	aqa |= NVME_AQA_FOR_ASQS(elements - 1);
	dnvme_writel(bar0, NVME_REG_AQA, aqa);

	dnvme_writeq(bar0, NVME_REG_ASQ, sq->dma);

	dnvme_dbg(ndev, "WRITE AQA:0x%x, ASQ:0x%llx\n", aqa, sq->dma);
	return 0;
}

int dnvme_create_acq(struct nvme_device *ndev, u32 elements)
{
	struct nvme_cq *cq;
	struct nvme_prep_cq prep;
	void __iomem *bar0 = ndev->bar[0];
	u32 cc, aqa;
	u16 acq_id = 0; /* admin queue ID is always 0 */
	int ret;

	if (!elements || elements > NVME_AQ_MAX_SIZE) {
		dnvme_err(ndev, "ACQ elements(%u) is invalid!\n", elements);
		return -EINVAL;
	}

	ret = dnvme_check_qid_unique(ndev, NVME_CQ, acq_id);
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

	cq = dnvme_alloc_cq(ndev, &prep, NVME_ADM_CQES);
	if (!cq)
		return -ENOMEM;

	/* Notify NVMe controller */
	aqa = dnvme_readl(bar0, NVME_REG_AQA);
	aqa &= ~NVME_AQA_ACQS_MASK;
	aqa |= NVME_AQA_FOR_ACQS(elements - 1);
	dnvme_writel(bar0, NVME_REG_AQA, aqa);

	dnvme_writeq(bar0, NVME_REG_ACQ, cq->dma);

	dnvme_dbg(ndev, "WRITE AQA:0x%x, ACQ:0x%llx\n", aqa, cq->dma);
	return 0;
}

/*
 * @brief Reinitialize the admin Submission queue's public parameters, when
 *  a controller is not completely disabled
 */
static void dnvme_reinit_asq(struct nvme_device *ndev, struct nvme_sq *sq)
{
	/* free cmd list for admin sq */
	dnvme_delete_cmd_list(ndev, sq);

	sq->pub.head_ptr = 0;
	sq->pub.tail_ptr = 0;
	sq->pub.tail_ptr_virt = 0;
	sq->next_cid = 0;
	memset(sq->buf, 0, sq->size);
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
	memset(cq->buf, 0, cq->size);
}

int dnvme_ring_sq_doorbell(struct nvme_device *ndev, u16 sq_id)
{
	struct nvme_sq *sq;

	sq = dnvme_find_sq(ndev, sq_id);
	if (!sq) {
		dnvme_err(ndev, "SQ(%u) doesn't exist!\n", sq_id);
		return -EINVAL;
	}

	dnvme_dbg(ndev, "RING SQ(%u) %u => %lx (old:%u)\n", sq_id, 
		sq->pub.tail_ptr_virt, (unsigned long)sq->db,
		sq->pub.tail_ptr);
	sq->pub.tail_ptr = sq->pub.tail_ptr_virt;
	dnvme_writel(sq->db, 0, sq->pub.tail_ptr);
	return 0;
}

/**
 * @brief Delete all queues, unbind from nvme_context and free memory.
 *
 * @param state set NVME_ST_DISABLE_COMPLETE if you want to save Admin Queue.
 */
void dnvme_delete_all_queues(struct nvme_device *ndev, enum nvme_state state)
{
	struct nvme_sq *sq;
	struct nvme_cq *cq;
	void *bar0 = ndev->bar[0];
	bool save_aq = (state == NVME_ST_DISABLE_COMPLETE) ? false : true;
	unsigned long i;

	xa_for_each(&ndev->sqs, i, sq) {
		if (save_aq && sq->pub.sq_id == NVME_AQ_ID) {
			dnvme_vdbg(ndev, "Retaining ASQ from deallocation\n");
			/* drop sq cmds and set to zero the public metrics of asq */
			dnvme_reinit_asq(ndev, sq);
		} else {
			dnvme_release_sq(ndev, sq);
		}
	}

	xa_for_each(&ndev->cqs, i, cq) {
		if (save_aq && cq->pub.q_id == NVME_AQ_ID) {
			dnvme_vdbg(ndev, "Retaining ACQ from deallocation");
			/* set to zero the public metrics of acq */
			dnvme_reinit_acq(cq);
		} else {
			dnvme_release_cq(ndev, cq);
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
	struct nvme_device *ndev = cq->ndev;
	struct nvme_prps *prps = cq->prps;
	struct nvme_completion *entry;
	void *cq_addr;
	u32 remain = 0;
	u8 phase = cq->pub.pbit_new_entry;

	if (cq->contig) {
		cq_addr = cq->buf;
	} else {
		dma_sync_sg_for_cpu(dev, prps->sg, prps->num_map_pgs, 
			prps->data_dir);
		cq_addr = prps->buf;
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

	dnvme_vdbg(ndev, "Inquiry CQ(%u) element:%u, head:%u, tail:%u, remain:%u\n",
		cq->pub.q_id, cq->pub.elements, cq->pub.head_ptr,
		cq->pub.tail_ptr, remain);
	return remain;
}


/**
 * @brief Inquire the number of CQ entries that are waiting to be reaped.
 */
int dnvme_inquiry_cqe(struct nvme_device *ndev, struct nvme_inquiry __user *uinq)
{
	struct nvme_cq *cq;
	struct nvme_inquiry inquiry;

	if (copy_from_user(&inquiry, uinq, sizeof(inquiry))) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		return -EFAULT;
	}

	cq = dnvme_find_cq(ndev, inquiry.cqid);
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
	struct nvme_device *ndev = sq->ndev;
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
		dnvme_delete_cq(ndev, cmd->target_qid);
	} else if (type == NVME_SQ) {
		dnvme_delete_sq(ndev, cmd->target_qid);
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
	dnvme_release_prps(sq->ndev, node->prps);
	node->prps = NULL;
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
static int handle_cmd_completion(struct nvme_device *ndev, 
	struct nvme_completion *cq_entry)
{
	struct nvme_sq *sq;
	struct nvme_cmd *cmd;
	u16 status;
	int ret = 0;

	trace_handle_cmd_completion(cq_entry);

	sq = dnvme_find_sq(ndev, cq_entry->sq_id);
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
static int copy_cq_data(struct nvme_cq *cq, u32 *nr_reap, u8 __user *buffer)
{
	struct nvme_device *ndev = cq->ndev;
	void *cq_head;
	void *cq_base;
	u32 cqes = 1 << cq->pub.cqes;
	int latentErr = 0;

	if (cq->contig)
		cq_base = cq->buf;
	else
		cq_base = cq->prps->buf;

	cq_head = cq_base + ((u32)cq->pub.head_ptr << cq->pub.cqes);

	while (*nr_reap) {
		dnvme_vdbg(ndev, "Reaping CE's, %d left to reap", *nr_reap);

		/* Call the process reap algos based on CE entry */
		latentErr = handle_cmd_completion(ndev, cq_head);
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

		if (cq_head >= (cq_base + cq->size)) {
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
	struct nvme_device *ndev = cq->ndev;
	u32 head = cq->pub.head_ptr;

	head += num_reaped;
	if (head >= cq->pub.elements) {
		cq->pub.pbit_new_entry = !cq->pub.pbit_new_entry;
		head = head % cq->pub.elements;
	}

	cq->pub.head_ptr = (u16)head;
	dnvme_writel(cq->db, 0, cq->pub.head_ptr);

	dnvme_vdbg(ndev, "CQ(%u) head:%u, tail:%u\n", cq->pub.q_id, 
		cq->pub.head_ptr, cq->pub.tail_ptr);
}

int dnvme_reap_cqe(struct nvme_cq *cq, u32 expect, void __user *buf, u32 size)
{
	struct nvme_device *ndev = cq->ndev;
	struct pci_dev *pdev = ndev->pdev;
	enum nvme_irq_type irq_type = ndev->irq_set.irq_type;
	u32 actual, reaped, remain;
	int ret;

	if (!expect) {
		dnvme_err(ndev, "expect num is zero?\n");
		return -EINVAL;
	}

	if (!buf) {
		dnvme_err(ndev, "user buf ptr is NULL!\n");
		return -EFAULT;
	}

	if (size < (expect << cq->pub.cqes)) {
		dnvme_err(ndev, "reqire bigger buf(0x%x)!\n", size);
		return -ENOMEM;
	}

	actual = expect;

	ret = copy_cq_data(cq, &actual, buf);

	reaped = expect - actual;
	if (reaped)
		update_cq_head(cq, reaped);

	remain = dnvme_get_cqe_remain(cq, &pdev->dev);
	if (irq_type != NVME_INT_NONE && cq->pub.irq_enabled == 1 && remain == 0) {
		ret = dnvme_reset_isr_flag(ndev, cq->pub.irq_no);
		if (ret < 0)
			dnvme_warn(ndev, "reset isr fired flag failed\n");

		dnvme_unmask_interrupt(&ndev->irq_set, cq->pub.irq_no);
	}

	return reaped;
}

/* !TODO: obsolete */
int dnvme_reap_cqe_legacy(struct nvme_device *ndev, struct nvme_reap __user *ureap)
{
	enum nvme_irq_type irq_type = ndev->irq_set.irq_type;
	struct pci_dev *pdev = ndev->pdev;
	struct nvme_reap reap;
	struct nvme_cq *cq;
	u32 cqes, expect, actual, remain;
	int ret;

	if (copy_from_user(&reap, ureap, sizeof(reap))) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		return -EFAULT;
	}

	cq = dnvme_find_cq(ndev, reap.cqid);
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

	if (reap.reaped)
		update_cq_head(cq, reap.reaped);

	if (irq_type != NVME_INT_NONE && cq->pub.irq_enabled == 1 &&
		reap.remained == 0) {

		ret = dnvme_reset_isr_flag(ndev, cq->pub.irq_no);
		if (ret < 0) {
			dnvme_err(ndev, "reset isr fired flag failed\n");
			return ret;
		}
		dnvme_unmask_interrupt(&ndev->irq_set, cq->pub.irq_no);
	}

	return 0;
}

