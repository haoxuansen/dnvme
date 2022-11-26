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

#include "core.h"
#include "io.h"
#include "debug.h"

#include "definitions.h"
#include "dnvme_reg.h"
#include "dnvme_queue.h"
#include "dnvme_ds.h"
#include "dnvme_cmds.h"
#include "dnvme_irq.h"

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
	struct nvme_sq *sq;
	struct nvme_cq *cq;

	if (type == NVME_SQ) {
		sq = dnvme_find_sq(ctx, id);
		if (sq) {
			dnvme_err("SQ ID(%u) already exists!\n", id);
			return -EEXIST;
		}
	} else if (type == NVME_CQ) {
		cq = dnvme_find_cq(ctx, id);
		if (cq) {
			dnvme_err("SQ ID(%u) already exists!\n", id);
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
struct nvme_cmd_node *dnvme_find_cmd(struct nvme_sq *sq, u16 id)
{
	struct nvme_cmd_node *cmd;

	list_for_each_entry(cmd, &sq->priv.cmd_list, entry) {
		if (id == cmd->unique_id)
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
 * @brief Delete the cmd node from the SQ list.
 * 
 * @param sq The submission queue where the command resides.
 * @param cmd_id command identify
 * @return 0 on success, otherwise a negative errno.
 */
static int dnvme_delete_cmd(struct nvme_sq *sq, u16 cmd_id)
{
	struct nvme_cmd_node *cmd;

	cmd = dnvme_find_cmd(sq, cmd_id);
	if (!cmd) {
		dnvme_err("cmd(%u) not exist in SQ(%d)\n", cmd_id, sq->pub.sq_id);
		return -EBADSLT;
	}

	list_del(&cmd->entry);
	kfree(cmd);
	return 0;
}

/*
 * Called to clean up the driver data structures
 */
void device_cleanup(struct nvme_context *pmetrics_device,
    enum nvme_state new_state)
{
    /* Clean the IRQ data structures */
    release_irq(pmetrics_device);
    /* Clean Up the data structures */
    deallocate_all_queues(pmetrics_device, new_state);
    /* Clean up meta buffers in all disable cases */
    deallocate_mb(pmetrics_device);
}

struct nvme_sq *dnvme_alloc_sq(struct nvme_context *ctx, 
	struct nvme_prep_sq *prep, u8 sqes)
{
	struct nvme_sq *sq;
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->priv.pdev;
	void *sq_buf;
	u32 sq_size;
	dma_addr_t dma;

	sq = kzalloc(sizeof(*sq), GFP_KERNEL);
	if (!sq) {
		dnvme_err("failed to alloc nvme_sq!\n");
		return NULL;
	}

	sq_size = prep->elements << sqes;

	if (prep->contig) {
		sq_buf = dma_alloc_coherent(&pdev->dev, sq_size, &dma, GFP_KERNEL);
		if (!sq_buf) {
			dnvme_err("failed to alloc DMA addr for SQ!\n");
			goto out;
		}
		memset(sq_buf, 0, sq_size);

		sq->priv.buf = sq_buf;
		sq->priv.dma = dma;
	}

	sq->pub.sq_id = prep->sq_id;
	sq->pub.cq_id = prep->cq_id;
	sq->pub.elements = prep->elements;
	sq->pub.sqes = sqes;

	INIT_LIST_HEAD(&sq->priv.cmd_list);
	sq->priv.size = sq_size;
	sq->priv.unique_cmd_id = 0;
	sq->priv.contig = prep->contig;
	sq->priv.dbs = &ndev->priv.dbs[prep->sq_id * 2 * ndev->db_stride];

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
	struct pci_dev *pdev = ndev->priv.pdev;

	list_del(&sq->sq_entry);

	if (sq->priv.contig && sq->priv.buf)
		dma_free_coherent(&pdev->dev, sq->priv.size, sq->priv.buf, 
			sq->priv.dma);
	kfree(sq);
}

struct nvme_cq *dnvme_alloc_cq(struct nvme_context *ctx, 
	struct nvme_prep_cq *prep, u8 cqes)
{
	struct nvme_cq *cq;
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->priv.pdev;
	void *cq_buf;
	u32 cq_size;
	dma_addr_t dma;

	cq = kzalloc(sizeof(*cq), GFP_KERNEL);
	if (!cq) {
		dnvme_err("failed to alloc nvme_cq!\n");
		return NULL;
	}

	cq_size = prep->elements << cqes;

	if (prep->contig) {
		cq_buf = dma_alloc_coherent(&pdev->dev, cq_size, &dma, GFP_KERNEL);
		if (!cq_buf) {
			dnvme_err("failed to alloc DMA addr for CQ!\n");
			goto out;
		}
		memset(cq_buf, 0, cq_size);

		cq->priv.buf = cq_buf;
		cq->priv.dma = dma;
	}

	cq->pub.q_id = prep->cq_id;
	cq->pub.elements = prep->elements;
	cq->pub.cqes = cqes;
	cq->pub.irq_no = prep->cq_irq_no;
	cq->pub.irq_enabled = prep->cq_irq_en;
	cq->pub.pbit_new_entry = 1;

	cq->priv.size = cq_size;
	cq->priv.contig = prep->contig;
	cq->priv.dbs = &ndev->priv.dbs[(prep->cq_id * 2 + 1) * ndev->db_stride];

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
	struct pci_dev *pdev = ndev->priv.pdev;

	list_del(&cq->cq_entry);

	if (cq->priv.contig && cq->priv.buf)
		dma_free_coherent(&pdev->dev, cq->priv.size, cq->priv.buf, 
			cq->priv.dma);
	kfree(cq);
}

int dnvme_create_asq(struct nvme_context *ctx, u32 elements)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_sq *sq;
	struct nvme_prep_sq prep;
	void __iomem *bar0 = ndev->priv.bar0;
	u32 cc, aqa;
	u16 asq_id = 0; /* admin queue ID is always 0 */
	int ret;

	if (!elements || elements > NVME_ASQ_ENTRY_MAX) {
		dnvme_err("ASQ elements(%u) is invalid!\n", elements);
		return -EINVAL;
	}

	ret = dnvme_check_qid_unique(ctx, NVME_SQ, asq_id);
	if (ret < 0)
		return ret;

	cc = dnvme_readl(bar0, NVME_REG_CC);
	if (cc & NVME_CC_ENABLE) {
		dnvme_err("NVMe already enabled!\n");
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

	dnvme_dbg("WRITE AQA:0x%x, ASQ:0x%llx\n", aqa, sq->priv.dma);
	return 0;
}

int dnvme_create_acq(struct nvme_context *ctx, u32 elements)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_cq *cq;
	struct nvme_prep_cq prep;
	void __iomem *bar0 = ndev->priv.bar0;
	u32 cc, aqa;
	u16 acq_id = 0; /* admin queue ID is always 0 */
	int ret;

	if (!elements || elements > NVME_ACQ_ENTRY_MAX) {
		dnvme_err("ACQ elements(%u) is invalid!\n", elements);
		return -EINVAL;
	}

	ret = dnvme_check_qid_unique(ctx, NVME_CQ, acq_id);
	if (ret < 0)
		return ret;

	cc = dnvme_readl(bar0, NVME_REG_CC);
	if (cc & NVME_CC_ENABLE) {
		dnvme_err("NVMe already enabled!\n");
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

	dnvme_dbg("WRITE AQA:0x%x, ACQ:0x%llx\n", aqa, cq->priv.dma);
	return 0;
}

int dnvme_ring_sq_doorbell(struct nvme_context *ctx, u16 sq_id)
{
	struct nvme_sq *sq;

	sq = dnvme_find_sq(ctx, sq_id);
	if (!sq) {
		dnvme_err("SQ(%u) doesn't exist!\n", sq_id);
		return -EINVAL;
	}

	dnvme_dbg("RING SQ(%u) %u => %lx (old:%u)\n", sq_id, 
		sq->pub.tail_ptr_virt, (unsigned long)sq->priv.dbs,
		sq->pub.tail_ptr);
	sq->pub.tail_ptr = sq->pub.tail_ptr_virt;
	writel(sq->pub.tail_ptr, sq->priv.dbs);
	return 0;
}

/*
 * Deallocate function is called when we want to free up the kernel or prp
 * memory based on the contig flag. The kernel memory is given back, nodes
 * from the cq list are deleted.
 */
static void deallocate_metrics_cq(struct device *dev,
        struct  nvme_cq  *pmetrics_cq_list,
        struct  nvme_context *pmetrics_device)
{
    /* Delete memory for all nvme_cq for current id here */
    if (pmetrics_cq_list->priv.contig == 0) {
        /* Deletes the PRP persist entry */
        dnvme_delete_prps(pmetrics_device->dev,
            &pmetrics_cq_list->priv.prp_persist);

    } else {
        /* Contiguous CQ, so free the DMA memory */
        dma_free_coherent(dev, pmetrics_cq_list->priv.size,
            (void *)pmetrics_cq_list->priv.buf,
            pmetrics_cq_list->priv.dma);

    }
    /* Delete the current cq entry from the list, and free it */
    list_del(&pmetrics_cq_list->cq_entry);
    kfree(pmetrics_cq_list);
}


/*
 * Deallocate function is called when we want to free up the kernel or prp
 * memory based on the contig flag. The kernel memory is given back, nodes
 * from the sq list are deleted. The cmds tracked are dropped and nodes in
 * command list are deleted.
 */
static void nvme_release_sq(struct device *dev, struct nvme_sq *sq, 
	struct nvme_context *ctx)
{
	/* Clean the Cmd track list */
	dnvme_delete_cmd_list(ctx->dev, sq);

	if (sq->priv.contig == 0) {
		/* Deletes the PRP persist entry */
		dnvme_delete_prps(ctx->dev,
		&sq->priv.prp_persist);
	} else {
		/* Contiguous SQ, so free the DMA memory */
		dma_free_coherent(dev, sq->priv.size,
		(void *)sq->priv.buf,
		sq->priv.dma);
	}

	/* Delete the current sq entry from the list */
	list_del(&sq->sq_entry);
	kfree(sq);
}


/*
 * Reinitialize the admin completion queue's public parameters, when
 * a controller is not completely disabled
 */
static void reinit_admn_cq(struct nvme_cq *pmetrics_cq_list)
{
    /* reinit required params in admin node */
    pmetrics_cq_list->pub.head_ptr = 0;
    pmetrics_cq_list->pub.tail_ptr = 0;
    pmetrics_cq_list->pub.pbit_new_entry = 1;
    memset(pmetrics_cq_list->priv.buf, 0,
        pmetrics_cq_list->priv.size);
    pmetrics_cq_list->pub.irq_enabled = 1;
}


/*
 * Reinitialize the admin Submission queue's public parameters, when
 * a controller is not completely disabled
 */
static void reinit_admn_sq(struct nvme_sq *pmetrics_sq_list,
    struct nvme_context *pmetrics_device)
{
    /* Free command track list for admin */
    dnvme_delete_cmd_list(pmetrics_device->dev, pmetrics_sq_list);

    /* reinit required params in admin node */
    pmetrics_sq_list->pub.head_ptr = 0;
    pmetrics_sq_list->pub.tail_ptr = 0;
    pmetrics_sq_list->pub.tail_ptr_virt = 0;
    pmetrics_sq_list->priv.unique_cmd_id = 0;
}


/*
 * deallocate_all_queues - This function will start freeing up the memory for
 * the queues (SQ and CQ) allocated during the prepare queues. The parameter
 * 'new_state', NVME_ST_DISABLE or NVME_ST_DISABLE_COMPLETE, identifies if you need to
 * clear Admin Q along with other Q's.
 */
void deallocate_all_queues(struct nvme_context *pmetrics_device,
    enum nvme_state new_state)
{
    char preserve_admin_qs = (new_state == NVME_ST_DISABLE_COMPLETE) ? 0 : -1;
    struct  nvme_sq  *pmetrics_sq_list;
    struct  nvme_sq  *pmetrics_sq_next;
    struct  nvme_cq  *pmetrics_cq_list;
    struct  nvme_cq  *pmetrics_cq_next;
    struct device *dev;


    dev = &pmetrics_device->dev->priv.pdev->dev;

    /* Loop for each sq node */
    list_for_each_entry_safe(pmetrics_sq_list, pmetrics_sq_next,
        &pmetrics_device->sq_list, sq_entry) {

        /* Check if Admin Q is excluded or not */
        if (preserve_admin_qs && (pmetrics_sq_list->pub.sq_id == 0)) {
            dnvme_vdbg("Retaining ASQ from deallocation");
            /* drop sq cmds and set to zero the public metrics of asq */
            reinit_admn_sq(pmetrics_sq_list, pmetrics_device);
        } else {
            /* Call the generic deallocate sq function */
            nvme_release_sq(dev, pmetrics_sq_list, pmetrics_device);
        }
    }

    /* Loop for each cq node */
    list_for_each_entry_safe(pmetrics_cq_list, pmetrics_cq_next,
        &pmetrics_device->cq_list, cq_entry) {

        /* Check if Admin Q is excluded or not */
        if (preserve_admin_qs && pmetrics_cq_list->pub.q_id == 0) {
            dnvme_vdbg("Retaining ACQ from deallocation");
            /* set to zero the public metrics of acq */
            reinit_admn_cq(pmetrics_cq_list);
        } else {
            /* Call the generic deallocate cq function */
            deallocate_metrics_cq(dev, pmetrics_cq_list, pmetrics_device);
        }
    }

    /* if complete disable then reset the controller admin registers. */
    if (! preserve_admin_qs) {
        /* Set the Registers to default values. */
        /* Write 0 to AQA */
        writel(0x0, &pmetrics_device->dev->priv.
            ctrlr_regs->aqa);
        /* Write 0 to the DMA address into ASQ base address */
        WRITEQ(0x0, &pmetrics_device->dev->priv.
            ctrlr_regs->asq);
        /* Write 0 to the DMA address into ACQ base address */
        WRITEQ(0x0, &pmetrics_device->dev->priv.
            ctrlr_regs->acq);
    }
}


/*
 *  reap_inquiry - This generic function will try to inquire the number of
 *  commands in the Completion Queue that are waiting to be reaped for any
 *  given q_id.
 */
u32 reap_inquiry(struct nvme_cq  *pmetrics_cq_node, struct device *dev)
{
    u8 tmp_pbit;                    /* Local phase bit      */
    /* mem head ptr in cq, base address for queue */
    u8 *q_head_ptr, *queue_base_addr;
    struct cq_completion *cq_entry; /* cq entry format      */
    u32 comp_entry_size = 16;       /* acq entry size       */
    u32 num_remaining = 0;          /* reap elem remaining  */


    /* If IO CQ set the completion Q entry size */
    if (pmetrics_cq_node->pub.q_id != 0) {
        comp_entry_size = (pmetrics_cq_node->priv.size /
            pmetrics_cq_node->pub.elements);
    }

    /* local tmp phase bit */
    tmp_pbit = pmetrics_cq_node->pub.pbit_new_entry;

    /* point the address to corresponding head ptr */
    if (pmetrics_cq_node->priv.contig != 0) {
        queue_base_addr = pmetrics_cq_node->priv.buf;
        q_head_ptr = pmetrics_cq_node->priv.buf +
            (comp_entry_size * (u32)pmetrics_cq_node->pub.head_ptr);
    } else {
        /* do sync and update when pointer to discontig Q is reaped inq */
        dma_sync_sg_for_cpu(dev, pmetrics_cq_node->priv.prp_persist.sg,
            pmetrics_cq_node->priv.prp_persist.num_map_pgs,
            pmetrics_cq_node->priv.prp_persist.data_dir);
        queue_base_addr =
            pmetrics_cq_node->priv.prp_persist.buf;
        q_head_ptr = queue_base_addr +
            (comp_entry_size * (u32)pmetrics_cq_node->pub.head_ptr);

    }

    /* Start from head ptr and update till phase bit incorrect */
    pmetrics_cq_node->pub.tail_ptr =
        pmetrics_cq_node->pub.head_ptr;

    dnvme_vdbg("Reap Inquiry on CQ_ID:PBit:EntrySize = %d:%d:%d",
        pmetrics_cq_node->pub.q_id, tmp_pbit, comp_entry_size);
    dnvme_vdbg("CQ Hd Ptr = %d", pmetrics_cq_node->pub.head_ptr);
    dnvme_vdbg("Rp Inq. Tail Ptr before = %d", pmetrics_cq_node->pub.
        tail_ptr);

    /* loop through the entries in the cq */
    while (1) {
        cq_entry = (struct cq_completion *)q_head_ptr;
        if (cq_entry->phase_bit == tmp_pbit) {

            pmetrics_cq_node->pub.tail_ptr += 1;
            q_head_ptr += comp_entry_size;
            num_remaining += 1;

            /* Q wrapped around */
            if (q_head_ptr >= (queue_base_addr +
                pmetrics_cq_node->priv.size)) {

                tmp_pbit = !tmp_pbit;
                q_head_ptr = queue_base_addr;
                pmetrics_cq_node->pub.tail_ptr = 0;
            }
        } else {    /* we reached stale element */
            break;
        }
    }

    dnvme_vdbg("Rp Inq. Tail Ptr After = %d", pmetrics_cq_node->pub.
        tail_ptr);
    dnvme_vdbg("cq.elements = %d", pmetrics_cq_node->pub.elements);
    dnvme_vdbg("Number of elements remaining = %d", num_remaining);
    return num_remaining;
}


/*
 *  driver_reap_inquiry - This function will try to inquire the number of
 *  commands in the CQ that are waiting to be reaped.
 */
int driver_reap_inquiry(struct nvme_context *pmetrics_device,
    struct nvme_reap_inquiry *usr_reap_inq)
{
    int err = 0;
    struct nvme_cq *pmetrics_cq_node;   /* ptr to cq node */
    struct nvme_reap_inquiry *user_data = NULL;


    /* Allocating memory for user struct in kernel space */
    user_data = kmalloc(sizeof(struct nvme_reap_inquiry), GFP_KERNEL);
    if (user_data == NULL) {
        dnvme_err("Unable to alloc kernel memory to copy user data");
        err = -ENOMEM;
        goto fail_out;
    }
    if (copy_from_user(user_data, usr_reap_inq,
        sizeof(struct nvme_reap_inquiry))) {

        dnvme_err("Unable to copy from user space");
        err = -EFAULT;
        goto fail_out;
    }

    /* Find given CQ in list */
    pmetrics_cq_node = dnvme_find_cq(pmetrics_device, user_data->q_id);
    if (pmetrics_cq_node == NULL) {
        /* if the control comes here it implies q id not in list */
        dnvme_err("CQ ID = %d is not in list", user_data->q_id);
        err = -ENODEV;
        goto fail_out;
    }
    /* Initializing ISR count for all the possible cases */
    user_data->isr_count = 0;
    /* Note: If ISR's are enabled then ACQ will always be attached to INT 0 */
    if (pmetrics_device->dev->pub.irq_active.irq_type
        == INT_NONE) {

        /* Process reap inquiry for non-isr case */
        dnvme_vdbg("Non-ISR Reap Inq on CQ = %d",
            pmetrics_cq_node->pub.q_id);
        user_data->num_remaining = reap_inquiry(pmetrics_cq_node,
            &pmetrics_device->dev->priv.pdev->dev);
    } else {
        /* When INT scheme is other than INT_NONE */
        /* If the irq is enabled, process reap_inq isr else
         * do polling based inq
         */
        if (pmetrics_cq_node->pub.irq_enabled == 0) {
            /* Process reap inquiry for non-isr case */
            dnvme_vdbg("Non-ISR Reap Inq on CQ = %d",
                pmetrics_cq_node->pub.q_id);
            user_data->num_remaining = reap_inquiry(pmetrics_cq_node,
                &pmetrics_device->dev->priv.pdev->dev);
        } else {
            dnvme_vdbg("ISR Reap Inq on CQ = %d",
                pmetrics_cq_node->pub.q_id);
            /* Lock onto irq mutex for reap inquiry. */
            mutex_lock(&pmetrics_device->irq_process.irq_track_mtx);
            /* Process ISR based reap inquiry as isr is enabled */
            err = reap_inquiry_isr(pmetrics_cq_node, pmetrics_device,
                &user_data->num_remaining, &user_data->isr_count);
            /* unlock irq track mutex here */
            mutex_unlock(&pmetrics_device->irq_process.irq_track_mtx);
            /* delay err checking to return after mutex unlock */
            if (err < 0) {
                dnvme_err("ISR Reap Inquiry failed...");
                err = -EINVAL;
                goto fail_out;
             }
         }
    }

    /* Copy to user the remaining elements in this q */
    if (copy_to_user(usr_reap_inq, user_data,
        sizeof(struct nvme_reap_inquiry))) {

        dnvme_err("Unable to copy to user space");
        err = -EFAULT;
        goto fail_out;
    }

    /* Check for hw violation of full Q definition */
    if (user_data->num_remaining >= pmetrics_cq_node->pub.elements) {
        dnvme_err("HW violating full Q definition");
        err = -EINVAL;
        goto fail_out;
    }
    /* Fall through is intended */

fail_out:
    if (user_data != NULL) {
        kfree(user_data);
    }
    return err;
}


/*
 * Remove the given sq node from the linked list.
 */
static int dnvme_delete_sq(struct nvme_context *pmetrics_device,
    u16 sq_id)
{
    struct nvme_sq *pmetrics_sq_node;

    pmetrics_sq_node = dnvme_find_sq(pmetrics_device, sq_id);
    if (pmetrics_sq_node == NULL) {
        dnvme_err("SQ ID = %d does not exist", sq_id);
        return -EBADSLT; /* Invalid slot */
    }

    nvme_release_sq(&pmetrics_device->dev->priv.
        pdev->dev, pmetrics_sq_node, pmetrics_device);
    return 0;
}


/*
 * remove the given cq node from the linked list.Also if
 * the CQ node is present inside the IRQ track list it deletes it
 */
static int dnvme_delete_cq(struct  nvme_context *pmetrics_device,
    u16 cq_id)
{
    struct  nvme_cq  *pmetrics_cq_node;
    int err = 0;

    pmetrics_cq_node = dnvme_find_cq(pmetrics_device, cq_id);
    if (pmetrics_cq_node == NULL) {
        dnvme_err("CQ ID = %d does not exist", cq_id);
        return -EBADSLT;
    }

    /* If irq is enabled then clean up the irq track list
     * NOTE:- only for IO queues
     */
    if (pmetrics_cq_node->pub.irq_enabled != 0) {
        if (remove_icq_node(pmetrics_device, cq_id, pmetrics_cq_node->
            pub.irq_no) < 0) {
            dnvme_err("Removal of IRQ CQ node failed. ");
            err = -EINVAL;
        }
    }

    deallocate_metrics_cq(&pmetrics_device->dev->priv.
        pdev->dev, pmetrics_cq_node, pmetrics_device);
    return err;
}


static int process_algo_q(struct nvme_sq *pmetrics_sq_node,
    struct nvme_cmd_node *pcmd_node, u8 free_q_entry,
    struct  nvme_context *pmetrics_device,
    enum nvme_queue_type type)
{
    int err = 0;

    dnvme_vdbg("Persist Q Id = %d", pcmd_node->persist_q_id);
    dnvme_vdbg("Unique Cmd Id = %d", pcmd_node->unique_id);
    dnvme_vdbg("free_q_entry = %d", free_q_entry);

    if (free_q_entry) {
        if (pcmd_node->persist_q_id == 0) {
            dnvme_err("Trying to delete ACQ is blunder");
            err = -EINVAL;
            return err;
        }
        if (type == NVME_CQ) {
            err = dnvme_delete_cq(pmetrics_device, pcmd_node->persist_q_id);
            if (err != 0) {
                dnvme_err("CQ Removal failed...");
                return err;
            }

        } else if (type == NVME_SQ) {
            err = dnvme_delete_sq(pmetrics_device, pcmd_node->persist_q_id);
            if (err != 0) {
                dnvme_err("SQ Removal failed...");
                return err;
            }
        }
    }
    err = dnvme_delete_cmd(pmetrics_sq_node, pcmd_node->unique_id);
    if (err != 0) {
        dnvme_err("Cmd Removal failed...");
        return err;
    }

    return err;
}


static int process_algo_gen(struct nvme_sq *pmetrics_sq_node,
    u16 cmd_id, struct  nvme_context *pmetrics_device)
{
    int err;
    struct nvme_cmd_node *pcmd_node;

    /* Find the command ndoe */
    pcmd_node = dnvme_find_cmd(pmetrics_sq_node, cmd_id);
    if (pcmd_node == NULL) {
        dnvme_err("Command id = %d does not exist", cmd_id);
        return -EBADSLT; /* Invalid slot */
    }

    dnvme_delete_prps(pmetrics_device->dev, &pcmd_node->prp_nonpersist);
    err = dnvme_delete_cmd(pmetrics_sq_node, cmd_id);
    return err;
}


static int process_admin_cmd(struct nvme_sq *pmetrics_sq_node,
    struct nvme_cmd_node *pcmd_node, u16 status,
    struct  nvme_context *pmetrics_device)
{
    int err = 0;

    switch (pcmd_node->opcode) {
    case 0x00:
        /* Delete IOSQ */
        err = process_algo_q(pmetrics_sq_node, pcmd_node, (status == 0),
            pmetrics_device, NVME_SQ);
        break;
    case 0x01:
        /* Create IOSQ */
        err = process_algo_q(pmetrics_sq_node, pcmd_node, (status != 0),
            pmetrics_device, NVME_SQ);
        break;
    case 0x04:
        /* Delete IOCQ */
        err = process_algo_q(pmetrics_sq_node, pcmd_node, (status == 0),
            pmetrics_device, NVME_CQ);
        break;
    case 0x05:
        /* Create IOCQ */
        err = process_algo_q(pmetrics_sq_node, pcmd_node, (status != 0),
            pmetrics_device, NVME_CQ);
        break;
    default:
        /* General algo */
        err = process_algo_gen(pmetrics_sq_node, pcmd_node->unique_id,
            pmetrics_device);
        break;
    }
    return err;
}


/*
 * Process various algorithms depending on the Completion entry in a CQ
 * This works for both Admin and IO CQ entries.
 */
static int process_reap_algos(struct cq_completion *cq_entry,
    struct  nvme_context *pmetrics_device)
{
    int err = 0;
    u16 ceStatus;
    struct nvme_sq *pmetrics_sq_node = NULL;
    struct nvme_cmd_node *pcmd_node = NULL;


    /* Find sq node for given sq id in CE */
    pmetrics_sq_node = dnvme_find_sq(pmetrics_device, cq_entry->sq_identifier);
    if (pmetrics_sq_node == NULL) {
        dnvme_err("SQ ID = %d does not exist", cq_entry->sq_identifier);
        /* Error must be EBADSLT per design; user may want to reap all entry */
        return -EBADSLT; /* Invalid slot */
    }

    /* Update our understanding of the corresponding hdw SQ head ptr */
    pmetrics_sq_node->pub.head_ptr = cq_entry->sq_head_ptr;
    ceStatus = (cq_entry->status_field & 0x7ff);
    dnvme_vdbg("(SCT, SC) = 0x%04X", ceStatus);

    /* Find command in sq node */
    pcmd_node = dnvme_find_cmd(pmetrics_sq_node, cq_entry->cmd_identifier);
    if (pcmd_node != NULL) {
        /* A command node exists, now is it an admin cmd or not? */
        if (cq_entry->sq_identifier == 0) {
            dnvme_vdbg("Admin cmd set processing");
            err = process_admin_cmd(pmetrics_sq_node, pcmd_node, ceStatus,
                pmetrics_device);
        } else {
            dnvme_vdbg("NVM or other cmd set processing");
            err = process_algo_gen(pmetrics_sq_node, pcmd_node->unique_id,
                pmetrics_device);
        }
    }
    return err;
}

/*
 * Copy the cq data to user buffer for the elements reaped.
 */
static int copy_cq_data(struct nvme_cq  *pmetrics_cq_node, u8 *cq_head_ptr,
    u32 comp_entry_size, u32 *num_should_reap, u8 *buffer,
    struct  nvme_context *pmetrics_device)
{
    int latentErr = 0;
    u8 *queue_base_addr; /* Base address for Queue */

    if (pmetrics_cq_node->priv.contig != 0) {
        queue_base_addr = pmetrics_cq_node->priv.buf;
    } else {
        /* Point to discontig Q memory here */
        queue_base_addr =
            pmetrics_cq_node->priv.prp_persist.buf;
    }

    while (*num_should_reap) {
        dnvme_vdbg("Reaping CE's, %d left to reap", *num_should_reap);

        /* Call the process reap algos based on CE entry */
        latentErr = process_reap_algos((struct cq_completion *)cq_head_ptr,
            pmetrics_device);
        if (latentErr) {
            dnvme_err("Unable to find CE.SQ_id in dnvme metrics");
        }

        /* Copy to user even on err; allows seeing latent err */
        if (copy_to_user(buffer, cq_head_ptr, comp_entry_size)) {
            dnvme_err("Unable to copy request data to user space");
            return -EFAULT;
        }

        cq_head_ptr += comp_entry_size;     /* Point to next CE entry */
        buffer += comp_entry_size;          /* Prepare for next element */
        *num_should_reap -= 1;              /* decrease for the one reaped. */

        if (cq_head_ptr >= (queue_base_addr +
            pmetrics_cq_node->priv.size)) {

            /* Q wrapped so point to base again */
            cq_head_ptr = queue_base_addr;
        }

        if (latentErr) {
            /* Latent errors were introduced to allow reaping CE's to user
             * space and also counting them as reaped, because they were
             * successfully copied. However, there was something about the CE
             * that indicated an error, possibly malformed CE by hdw, thus the
             * entire IOCTL should error, but we successfully reaped some CE's
             * which allows tnvme to inspect and trust the copied CE's for debug
             */
            dnvme_err("Detected a partial reap situation; some, not all reaped");
            return latentErr;
        }
    }

    return 0;
}

/*
 * move the cq head pointer to point to location of the elements that is
 * to be reaped.
 */
static void pos_cq_head_ptr(struct nvme_cq  *pmetrics_cq_node,
    u32 num_reaped)
{
    u32 temp_head_ptr = pmetrics_cq_node->pub.head_ptr;

    temp_head_ptr += num_reaped;
    if (temp_head_ptr >= pmetrics_cq_node->pub.elements) {
        pmetrics_cq_node->pub.pbit_new_entry =
            !pmetrics_cq_node->pub.pbit_new_entry;
        temp_head_ptr = temp_head_ptr % pmetrics_cq_node->pub.elements;
    }

    pmetrics_cq_node->pub.head_ptr = (u16)temp_head_ptr;
    dnvme_vdbg("Head ptr = %d", pmetrics_cq_node->pub.head_ptr);
    dnvme_vdbg("Tail ptr = %d", pmetrics_cq_node->pub.tail_ptr);
}


/*
 * Reap the number of elements specified for the given CQ id and send
 * the reaped elements back. This is the main place and only place where
 * head_ptr is updated. The pbit_new_entry is inverted when Q wraps.
 */
int driver_reap_cq(struct  nvme_context *pmetrics_device,
    struct nvme_reap *usr_reap_data)
{
    int err;
    u32 num_will_fit;
    u32 num_could_reap;
    u32 num_should_reap;
    struct nvme_cq *pmetrics_cq_node;   /* ptr to CQ node in ll */
    u32 comp_entry_size = 16;               /* Assumption is for ACQ */
    u8 *queue_base_addr;    /* base addr for both contig and discontig queues */
    struct nvme_reap *user_data = NULL;


    /* Allocating memory for user struct in kernel space */
    user_data = kmalloc(sizeof(struct nvme_reap), GFP_KERNEL);
    if (user_data == NULL) {
        dnvme_err("Unable to alloc kernel memory to copy user data");
        err = -ENOMEM;
        goto fail_out;
    }
    if (copy_from_user(user_data, usr_reap_data, sizeof(struct nvme_reap))) {
        dnvme_err("Unable to copy from user space");
        err = -EFAULT;
        goto fail_out;
    }

    /* Find CQ with given id from user */
    pmetrics_cq_node = dnvme_find_cq(pmetrics_device, user_data->q_id);
    if (pmetrics_cq_node == NULL) {
        dnvme_err("CQ ID = %d not found", user_data->q_id);
        err = -EBADSLT;
        goto fail_out;
    }

    /* Initializing ISR count for all the possible cases */
    user_data->isr_count = 0;

    /* Call the reap inquiry on this CQ, see how many unreaped elements exist */
    /* Check if the IRQ is enabled and process accordingly */
    if (pmetrics_device->dev->pub.irq_active.irq_type
        == INT_NONE) {

        /* Process reap inquiry for non-isr case */
        num_could_reap = reap_inquiry(pmetrics_cq_node, &pmetrics_device->
            dev->priv.pdev->dev);
    } else {
        if (pmetrics_cq_node->pub.irq_enabled == 0) {
            /* Process reap inquiry for non-isr case */
            num_could_reap = reap_inquiry(pmetrics_cq_node, &pmetrics_device->
                dev->priv.pdev->dev);

        } else { /* ISR Reap additions for IRQ support as irq_enabled is set */
            /* Lock the IRQ mutex to guarantee coherence with bottom half. */
            mutex_lock(&pmetrics_device->irq_process.irq_track_mtx);
            /* Process ISR based reap inquiry as isr is enabled */
            err = reap_inquiry_isr(pmetrics_cq_node, pmetrics_device,
                &num_could_reap, &user_data->isr_count);
            if (err < 0) {
                dnvme_err("ISR Reap Inquiry failed...");
                goto mtx_unlk;
            }
        }
    }

    /* Check for hw violation of full Q definition */
    if (num_could_reap >= pmetrics_cq_node->pub.elements) {
        dnvme_err("HW violating full Q definition");
        err = -EINVAL;
        goto mtx_unlk;
    }

    /* If this CQ is an IOCQ, not ACQ, then lookup the CE size */
    if (pmetrics_cq_node->pub.q_id != 0) {
        comp_entry_size = (pmetrics_cq_node->priv.size) /
            (pmetrics_cq_node->pub.elements);
    }
    dnvme_vdbg("Tail ptr position before reaping = %d",
        pmetrics_cq_node->pub.tail_ptr);
    dnvme_vdbg("Detected CE size = 0x%04X", comp_entry_size);
    dnvme_vdbg("%d elements could be reaped", num_could_reap);
    if (num_could_reap == 0) {
        dnvme_vdbg("All elements reaped, CQ is empty");
        user_data->num_remaining = 0;
        user_data->num_reaped = 0;
    }

    /* Is this request asking for every CE element? */
    if (user_data->elements == 0) {
        user_data->elements = num_could_reap;
    }
    num_will_fit = (user_data->size / comp_entry_size);

    dnvme_vdbg("Requesting to reap %d elements", user_data->elements);
    dnvme_vdbg("User space reap buffer size = %d", user_data->size);
    dnvme_vdbg("Total buffer bytes needed to satisfy request = %d",
        num_could_reap * comp_entry_size);
    dnvme_vdbg("num elements which fit in buffer = %d", num_will_fit);

    /* Assume we can fit all which are requested, then adjust if necessary */
    num_should_reap = num_could_reap;
    user_data->num_remaining = 0;

    /* Adjust our assumption based on size and elements */
    if (user_data->elements <= num_could_reap) {
    	// !FIXME: num_should_reap may greater than "user_data->elements"

        if (user_data->size < (num_could_reap * comp_entry_size)) {
            /* Buffer not large enough to hold all requested */
            num_should_reap = num_will_fit;
            user_data->num_remaining = (num_could_reap - num_should_reap);
        }

    } else {    /* Asking for more elements than presently exist in CQ */
        if (user_data->size < (num_could_reap * comp_entry_size)) {
            if (num_could_reap > num_will_fit) {
                /* Buffer not large enough to hold all requested */
                num_should_reap = num_will_fit;
                user_data->num_remaining = (num_could_reap - num_should_reap);
            }
        }
    }
    user_data->num_reaped = num_should_reap;    /* Expect success */

    dnvme_vdbg("num elements will attempt to reap = %d", num_should_reap);
    dnvme_vdbg("num elements expected to remain after reap = %d",
        user_data->num_remaining);
    dnvme_vdbg("Head ptr before reaping = %d",
        pmetrics_cq_node->pub.head_ptr);

    /* Get the required base address */
    if (pmetrics_cq_node->priv.contig != 0) {
        queue_base_addr = pmetrics_cq_node->priv.buf;
    } else {
        /* Point to discontig Q memory here */
        queue_base_addr =
            pmetrics_cq_node->priv.prp_persist.buf;
    }

    /* Copy the number of CE's we should be able to reap */
    err = copy_cq_data(pmetrics_cq_node, (queue_base_addr +
        (comp_entry_size * (u32)pmetrics_cq_node->pub.head_ptr)),
        comp_entry_size, &num_should_reap, user_data->buffer,
        pmetrics_device);

    /* Reevaluate our success during reaping */
    user_data->num_reaped -= num_should_reap;
    user_data->num_remaining += num_should_reap;
    dnvme_vdbg("num CE's reaped = %d, num CE's remaining = %d",
        user_data->num_reaped, user_data->num_remaining);

    /* Updating the user structure */
    if (copy_to_user(usr_reap_data, user_data, sizeof(struct nvme_reap))) {
        dnvme_err("Unable to copy request data to user space");
        err = (err == 0) ? -EFAULT : err;
        goto mtx_unlk;
    }

    /* Update system with number actually reaped */
    if(user_data->num_reaped)
    {
        pos_cq_head_ptr(pmetrics_cq_node, user_data->num_reaped);
        writel(pmetrics_cq_node->pub.head_ptr, pmetrics_cq_node->
        priv.dbs);
        //dnvme_err("@@@@@@@@@cqid:%x hdbl:%x",pmetrics_cq_node->pub.q_id,pmetrics_cq_node->pub.head_ptr);
    }

    /* if 0 CE in a given cq, then reset the isr flag. */
    if ((pmetrics_cq_node->pub.irq_enabled == 1) &&
        (user_data->num_remaining == 0) &&
        (pmetrics_device->dev->pub.irq_active.irq_type !=
        INT_NONE)) {

        /* reset isr fired flag for the particular irq_no */
        if (reset_isr_flag(pmetrics_device,
            pmetrics_cq_node->pub.irq_no) < 0) {
            dnvme_err("reset isr fired flag failed");
            err = (err == 0) ? -EINVAL : err;
        }
    }

    /* Unmask the irq for which it was masked in Top Half */
    //MengYu add. if tests interrupt, should commit this line
    unmask_interrupts(pmetrics_cq_node->pub.irq_no,
        &pmetrics_device->irq_process);
    /* Fall through is intended */

mtx_unlk:
    if ((pmetrics_cq_node->pub.irq_enabled == 1) &&
        (pmetrics_device->dev->pub.irq_active.irq_type !=
        INT_NONE)) {

        mutex_unlock(&pmetrics_device->irq_process.irq_track_mtx);
    }
fail_out:
    if (user_data != NULL) {
        kfree(user_data);
    }
    return err;
}
