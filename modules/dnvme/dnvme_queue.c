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

#include "definitions.h"
#include "core.h"
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
	struct nvme_cmd *cmd;

	cmd = dnvme_find_cmd(sq, cmd_id);
	if (!cmd) {
		pr_err("cmd(%u) not exist in SQ(%d)\n", cmd_id, sq->pub.sq_id);
		return -EBADSLT;
	}

	list_del(&cmd->entry);
	kfree(cmd);
	return 0;
}

/*
 * nvme_ctrlrdy_capto - This function is used for checking if the controller
 * has transitioned its state after CC.EN has been toggled. This will wait a
 * min of CAP.TO seconds before failing.
 */
int nvme_ctrlrdy_capto(struct nvme_device *pnvme_dev, u8 rdy_val)
{
    u64 timer_delay;    /* Timer delay read from CAP.TO register */
    u64 ini, end;

    /* Read in the value of CAP.TO */
    timer_delay = ((readl(&pnvme_dev->priv.ctrlr_regs->cap)
        >> NVME_TO_SHIFT_MASK) & 0xff);
    timer_delay = (timer_delay * CAP_TO_UNIT);

    pr_debug("Checking NVME Device Status (CSTS.RDY = %hhu)...", rdy_val);
    pr_debug("Timer Expires in %lld ms", timer_delay);
    ini = get_jiffies_64();

    /* Check if the device status set to ready */
    while ((readl(&pnvme_dev->priv.ctrlr_regs->csts) & NVME_CSTS_RDY)
            != rdy_val) {
        pr_debug("Waiting...");
        msleep(250);
        end = get_jiffies_64();
        if (end < ini) {
            /* Roll over */
            ini = ULLONG_MAX - ini;
            end = ini + end;
        }
        if ((jiffies_to_msecs(end - ini) % CAP_TO_UNIT) == 0)
        {
            pr_err("CSTS.RDY set to %hhu...", rdy_val);
        }
        /* Check if the time out occurred */
        if (jiffies_to_msecs(end - ini) > timer_delay) {
            pr_err("CSTS.RDY set to %hhu with TO",rdy_val);
            return -EINVAL;
        }
    }
    pr_debug("NVME Controller CSTS.RDY set to %hhu within CAP.TO", rdy_val);
    return 0;
}


/*
 * iol_nvme_ctrlrdy_capto - This function is used for checking if the controller
 * has transitioned its state after CC.EN has been toggled. This will
 * wait a minimum of CAP.TO seconds. If the actual time for CSTS.RDY to
 * transition is greater than CAP.TO, then fails. Polls CSTS.RDY in increments
 * of 100 ms.
 */
int iol_nvme_ctrlrdy_capto(struct nvme_device *pnvme_dev, u8 rdy_val)
{
    u64 timer_delay;    /* Timer delay read from CAP.TO register */
    u64 ini, end;

    /* Read in the value of CAP.TO */
    timer_delay = ((readl(&pnvme_dev->priv.ctrlr_regs->cap)
        >> NVME_TO_SHIFT_MASK) & 0xff);
    timer_delay = (timer_delay * CAP_TO_UNIT);

    pr_debug("Checking NVME Device Status (CSTS.RDY = %hhu)...", rdy_val);
    pr_debug("Timer Expires in %lld ms", timer_delay);
    ini = get_jiffies_64();

    /* Check if the device status set to ready */
    while ((readl(&pnvme_dev->priv.ctrlr_regs->csts) & NVME_CSTS_RDY)
            != rdy_val) {
        pr_debug("Waiting...");
        msleep(100);
        end = get_jiffies_64();
        if (end < ini) {
            /* Roll over */
            ini = ULLONG_MAX - ini;
            end = ini + end;
        }
        /* Check if the time out occurred */
        if (jiffies_to_msecs(end - ini) > timer_delay) {
            pr_err("CSTS.RDY set to %hhu with TO",rdy_val);
            return -EINVAL;
        }
    }

    pr_debug("NVME Controller CSTS.RDY set to %hhu within CAP.TO", rdy_val);
    return 0;
}


int iol_nvme_ctrl_set_state(struct nvme_context *pmetrics_device, u8 state)
{
    struct nvme_device *pnvme_dev;
    u32 regCC;

    /* get the device from the list */
    pnvme_dev = pmetrics_device->dev;

    regCC = readl(&pnvme_dev->priv.ctrlr_regs->cc);
    regCC = (regCC & ~NVME_CC_ENABLE) | (state & 0x1);  /* Set bit 0 (CC.EN) to state */
    writel(regCC, &pnvme_dev->priv.ctrlr_regs->cc);

    /* Check the Timeout flag */
    if (iol_nvme_ctrlrdy_capto(pnvme_dev, state) != 0) {
        pr_err("CSTS.RDY set to %hhu with TO",state);
        return -EINVAL;
    }
    return 0;
}


int nvme_ctrl_set_state(struct nvme_context *pmetrics_device, u8 state)
{
    struct nvme_device *pnvme_dev;
    u32 regCC;

    /* get the device from the list */
    pnvme_dev = pmetrics_device->dev;

    regCC = readl(&pnvme_dev->priv.ctrlr_regs->cc);
    regCC = (regCC & ~NVME_CC_ENABLE) | (state & 0x1);  /* Set bit 0 (CC.EN) to state */
    writel(regCC, &pnvme_dev->priv.ctrlr_regs->cc);

    /* Check the Timeout flag */
    if (nvme_ctrlrdy_capto(pnvme_dev, state) != 0) {
        pr_err("CSTS.RDY set to %hhu with TO",state);
        return -EINVAL;
    }
    return 0;
}


/*
 * nvme_nvm_subsystem_reset - NVME NVM subsystem reset function. This will
 * write the value 4E564D65h ("NVMe") to the NSSR register in order to trigger
 * an NVM Subsystem reset. Checks for timer expiration and returns success if
 * the ctrl is rdy before timeout.
 */
int nvme_nvm_subsystem_reset(struct nvme_context *pmetrics_device)
{
    struct nvme_device *pnvme_dev;
    
    u32 regVal = 0x4E564D65; //“NVMe”

    /* get the device from the list */
    pnvme_dev = pmetrics_device->dev;

    /* write the value to the register */
    writel(regVal, &pnvme_dev->priv.ctrlr_regs->nssr);

    /* Check the Timeout flag */
    if (nvme_ctrlrdy_capto(pnvme_dev, 0) != 0) {
        u8 i;
        /* poll until constant T/O since subsystem reset time is not defined */
        for (i = 0; i < 200; i++) {
            msleep(100);
            if (!(readl(&pnvme_dev->priv.ctrlr_regs->csts)
                & NVME_CSTS_RDY)) {
                return 0;
            }
        }
        pr_err("subsystem_reset ctrlr failed. CSTS.RDY=1 after T/O");
        return -EINVAL;
    }
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


/*
 * create_admn_sq - This routine is called when the driver invokes the ioctl for
 * admn sq creation. It returns success if the submission q creation is success
 * after dma_coherent_alloc else returns failure at any step which fails.
 */
int create_admn_sq(struct nvme_device *pnvme_dev, u32 qsize,
    struct  nvme_sq  *pmetrics_sq_list)
{
    u16 asq_id;         /* Admin Submission Q Id */
    u32 aqa;            /* Admin Q attributes in 32 bits size */
    u32 tmp_aqa;        /* Temp var to hold admin q attributes */
    u32 asq_depth = 0;  /* the size of bytes to allocate */
    int ret_code = 0;

    pr_debug("Creating Admin Submission Queue...");

    /* As the Admin Q ID is always 0*/
    asq_id = 0;

    /* Checking for overflow or underflow. */
    if ((qsize > MAX_AQ_ENTRIES) || (qsize == 0)) {
        pr_err("ASQ entries is more than MAX Q size or specified NULL");
        ret_code = -EINVAL;
        goto asq_out;
    }

    /*
    * As the qsize send is in number of entries this computes the no. of bytes
    * computed.
    */
    asq_depth = qsize * 64;
    pr_debug("ASQ Depth: 0x%x", asq_depth);

    /*
     * The function dma_alloc_coherent  maps the dma address for ASQ which gets
     * the DMA mapped address from the kernel virtual address.
     */
    pmetrics_sq_list->priv.buf =
        dma_alloc_coherent(&pnvme_dev->priv.pdev->dev, asq_depth,
        &pmetrics_sq_list->priv.sq_dma_addr, GFP_KERNEL);
    if (!pmetrics_sq_list->priv.buf) {
        pr_err("Unable to allocate DMA Address for ASQ!!");
        ret_code = -ENOMEM;
        goto asq_out;
    }

    /* Zero out all ASQ memory before processing */
    memset(pmetrics_sq_list->priv.buf, 0, asq_depth);

    pr_debug("Virtual ASQ DMA Address: 0x%llx",
        (u64)pmetrics_sq_list->priv.buf);

    /* Read, Modify, Write  the aqa as per the q size requested */
    aqa = (qsize - 1) & ASQS_MASK; /* asqs is zero based value */
    tmp_aqa = readl(&pnvme_dev->priv.ctrlr_regs->aqa);
    tmp_aqa &= ~ASQS_MASK;
    aqa |= tmp_aqa;

    pr_debug("Mod Attributes from AQA Reg = 0x%x", tmp_aqa);
    pr_debug("AQA Attributes in ASQ:0x%x", aqa);

    /* Write new ASQ size using AQA */
    writel(aqa, &pnvme_dev->priv.ctrlr_regs->aqa);

    /* Write the DMA address into ASQ base address */
    WRITEQ(pmetrics_sq_list->priv.sq_dma_addr,
        &pnvme_dev->priv.ctrlr_regs->asq);
#ifdef DEBUG
    /* Debug statements */
    pr_debug("Admin CQ Base Address = 0x%x",
        (u32)readl(&pnvme_dev->priv.ctrlr_regs->acq));
    /* Read the AQA attributes after writing and check */
    tmp_aqa = readl(&pnvme_dev->priv.ctrlr_regs->aqa);

    pr_debug("Reading AQA after writing = 0x%x", tmp_aqa);

    /* Read the status register and printout to log */
    tmp_aqa = readl(&pnvme_dev->priv.ctrlr_regs->csts);

    pr_debug("Reading status reg = 0x%x", tmp_aqa);
#endif

    /* Set the door bell of ASQ to 0x1000 as per spec 1.0b */
    pmetrics_sq_list->priv.dbs = (u32 __iomem *)
        (pnvme_dev->priv.bar0 + NVME_SQ0TBDL);

    /* set private members in sq metrics */
    pmetrics_sq_list->priv.size = asq_depth;
    pmetrics_sq_list->priv.unique_cmd_id = 0;
    pmetrics_sq_list->priv.contig = 1;
    return ret_code;

asq_out:
    if (pmetrics_sq_list->priv.buf != NULL) {
        /* Admin SQ dma mem allocated, so free the DMA memory */
        dma_free_coherent(&pnvme_dev->priv.pdev->dev, asq_depth,
            (void *)pmetrics_sq_list->priv.buf,
        pmetrics_sq_list->priv.sq_dma_addr);
    }
    return ret_code;
}


/*
 * create_admn_cq - This routine is called when the driver invokes the ioctl for
 * admn cq creation. It returns success if the completion q creation is success
 * after dma_coherent_alloc else returns failure at any step which fails.
 */
int create_admn_cq(struct nvme_device *pnvme_dev, u32 qsize,
    struct  nvme_cq  *pmetrics_cq_list)
{
    int ret_code = 0; /* Ret code set to SUCCESS check for otherwise */
    u16 acq_id;             /* Admin Submission Q Id                       */
    u32 aqa;                /* Admin Q attributes in 32 bits size          */
    u32 tmp_aqa;            /* local var to hold admin q attributes        */
    u32 acq_depth = 0;      /* local var to cal nbytes based on elements   */
    u8  cap_dstrd;          /* local var to cal the doorbell stride.       */

    pr_debug("Creating Admin Completion Queue...");

    /* As the Admin Q ID is always 0*/
    acq_id = 0;

    /* Checking for overflow or underflow. */
    if ((qsize > MAX_AQ_ENTRIES) || (qsize == 0)) {
        pr_err("ASQ size is more than MAX Q size or specified NULL");
        ret_code = -EINVAL;
        goto acq_out;
    }
    /*
    * As the qsize send is in number of entries this computes the no. of bytes
    * computed.
    */
    acq_depth = qsize * 16;
    pr_debug("ACQ Depth: 0x%x", acq_depth);
    /*
     * The function dma_alloc_coherent  maps the dma address for ACQ which gets
     * the DMA mapped address from the kernel virtual address.
     */
    pmetrics_cq_list->priv.buf =
        dma_alloc_coherent(&pnvme_dev->priv.pdev->dev, acq_depth,
        &pmetrics_cq_list->priv.cq_dma_addr, GFP_KERNEL);
    if (!pmetrics_cq_list->priv.buf) {
        pr_err("Unable to allocate DMA Address for ACQ!!");
        ret_code = -ENOMEM;
        goto acq_out;
    }

    /* Zero out all ACQ memory before processing */
    memset(pmetrics_cq_list->priv.buf, 0, acq_depth);

    pr_debug("Virtual ACQ DMA Address: 0x%llx",
            (u64)pmetrics_cq_list->priv.buf);
    pr_debug("ACQ DMA Address: 0x%llx",
            (u64)pmetrics_cq_list->priv.cq_dma_addr);

    /* Read, Modify and write the Admin Q attributes */
    aqa = (qsize - 1) << 16; /* acqs is zero based value */
    aqa &= ACQS_MASK;
    tmp_aqa = readl(&pnvme_dev->priv.ctrlr_regs->aqa);
    tmp_aqa &= ~ACQS_MASK;

    /* Final value to write to AQA Register */
    aqa |= tmp_aqa;

    pr_debug("Modified Attributes (AQA) = 0x%x", tmp_aqa);
    pr_debug("AQA Attributes in ACQ:0x%x", aqa);

    /* Write new ASQ size using AQA */
    writel(aqa, &pnvme_dev->priv.ctrlr_regs->aqa);
    /* Write the DMA address into ACQ base address */
    WRITEQ(pmetrics_cq_list->priv.cq_dma_addr,
            &pnvme_dev->priv.ctrlr_regs->acq);
#ifdef DEBUG
    /* Read the AQA attributes after writing and check */
    tmp_aqa = readl(&pnvme_dev->priv.ctrlr_regs->aqa);
    pr_debug("Reading AQA after writing in ACQ = 0x%x\n", tmp_aqa);
#endif

    pmetrics_cq_list->priv.size = acq_depth;
    pmetrics_cq_list->priv.contig = 1;

    /* Get the door bell stride from CAP register */
    cap_dstrd = ((READQ(&pnvme_dev->priv.ctrlr_regs->cap) >> 32) & 0xF);
    /* CQ 0 Head DoorBell admin computed used doorbell stride. */
    pmetrics_cq_list->priv.dbs = (u32 __iomem *)
        (pnvme_dev->priv.bar0 + NVME_SQ0TBDL + (4 << cap_dstrd));
    return ret_code;

acq_out:
    if (pmetrics_cq_list->priv.buf != NULL) {
        /* Admin CQ dma mem allocated, so free the DMA memory */
        dma_free_coherent(&pnvme_dev->priv.pdev->dev, acq_depth,
            (void *)pmetrics_cq_list->priv.buf,
            pmetrics_cq_list->priv.cq_dma_addr);
    }
    return ret_code;
}


/*
 * nvme_prepare_sq - This routine is called when the driver invokes the ioctl
 * for IO SQ Creation. It will retrieve the q size from IOSQES from CC.
 */
int nvme_prepare_sq(struct  nvme_sq  *pmetrics_sq_list,
    struct nvme_device *pnvme_dev)
{
    int ret_code = -ENOMEM;
    u32 regCC = 0;
    u8 cap_dstrd;


    regCC = readl(&pnvme_dev->priv.ctrlr_regs->cc);
    regCC = ((regCC >> 16) & 0xF);   /* Extract the IOSQES from CC */
    pr_debug("CC.IOSQES = 0x%x, 2^x = %d", regCC, (1 << regCC));
    //2021/05/15 meng_yu https://hub.fastgit.org/nvmecompliance/dnvme/issues/5
    pmetrics_sq_list->priv.size =
        (pmetrics_sq_list->pub.elements * (u32)(1 << regCC));

#ifdef DEBUG
    {
        u32 cap_mqes = 0;

        /* Check to see if the entries exceed the Max Q entries supported */
        cap_mqes = ((readl(&pnvme_dev->priv.ctrlr_regs->cap) & 0xFFFF) + 1);
        pr_debug("Elements: (Max Q:Actual Q) = 0x%x:0x%x", cap_mqes,
            pmetrics_sq_list->pub.elements);
        /* I should not return from here if exceeds */
        if (pmetrics_sq_list->pub.elements > cap_mqes) {
            pr_err("The IO SQ id = %d exceeds maximum elements allowed!",
                pmetrics_sq_list->pub.sq_id);
        }
    }
#endif

    /*
     * call dma_alloc_coherent or SQ which gets DMA mapped address from
     * the kernel virtual address. != 0 is contiguous SQ as per design.
     */
    if (pmetrics_sq_list->priv.contig != 0) {
        /* Assume that the future CMD.DW11.PC bit will be set to one. */
        pmetrics_sq_list->priv.buf = dma_alloc_coherent(
            &pnvme_dev->priv.pdev->dev, pmetrics_sq_list->priv.
            size, &pmetrics_sq_list->priv.sq_dma_addr, GFP_KERNEL);
        if (pmetrics_sq_list->priv.buf == NULL) {
            pr_err("Unable to allocate DMA mem for IOSQ");
            ret_code = -ENOMEM;
            goto psq_out;
        }
        /* Zero out allows seeing any cmds which could be placed within */
        memset(pmetrics_sq_list->priv.buf, 0,
            pmetrics_sq_list->priv.size);
    }

    /* Each new IOSQ resets its unique cmd counter, start from a known place */
    pmetrics_sq_list->priv.unique_cmd_id = 0;

    // Learn of the doorbell stride
    cap_dstrd = ((READQ(&pnvme_dev->priv.ctrlr_regs->cap) >> 32) & 0xF);
    pr_debug("CAP DSTRD Value = 0x%x", cap_dstrd);

    pmetrics_sq_list->priv.dbs = (u32 __iomem *)
        (pnvme_dev->priv.bar0 + NVME_SQ0TBDL +
        ((2 * pmetrics_sq_list->pub.sq_id) * (4 << cap_dstrd)));
    return 0;

psq_out:
    if (pmetrics_sq_list->priv.buf != NULL) {
        dma_free_coherent(&pnvme_dev->priv.pdev->dev, pmetrics_sq_list->
            priv.size, (void *)pmetrics_sq_list->priv.
            buf, pmetrics_sq_list->priv.sq_dma_addr);
    }
    return ret_code;
}


/*
 * nvme_prepare_cq - This routine is called when the driver invokes the ioctl
 * for IO CQ Preparation. It will retrieve the q size from IOSQES from CC.
 */
int nvme_prepare_cq(struct  nvme_cq  *pmetrics_cq_list,
    struct nvme_device *pnvme_dev)
{
    int ret_code = -ENOMEM;
    u32 regCC = 0;
    u8 cap_dstrd;

    regCC = readl(&pnvme_dev->priv.ctrlr_regs->cc);
    regCC = ((regCC >> 20) & 0xF);    /* Extract the IOCQES from CC */
    pr_debug("CC.IOCQES = 0x%x, 2^x = %d", regCC, (1 << regCC));
    //2021/05/15 meng_yu https://hub.fastgit.org/nvmecompliance/dnvme/issues/5
    pmetrics_cq_list->priv.size =
        (pmetrics_cq_list->pub.elements * (u32)(1 << regCC));

#ifdef DEBUG
    {
        u32 cap_mqes = 0;

        /* Check to see if the entries exceed the Max Q entries supported */
        cap_mqes = ((readl(&pnvme_dev->priv.ctrlr_regs->cap) & 0xFFFF) + 1);
        pr_debug("Max CQ:Actual CQ elements = 0x%x:0x%x", cap_mqes,
            pmetrics_cq_list->pub.elements);
        /* I should not return from here if exceeds */
        if (pmetrics_cq_list->pub.elements > cap_mqes) {
            pr_err("The IO CQ id = %d exceeds maximum elements allowed!",
                pmetrics_cq_list->pub.q_id);
        }
    }
#endif

    /*
     * call dma_alloc_coherent or SQ which gets DMA mapped address from
     * the kernel virtual address. != 0 is contiguous SQ as per design.
     */
    if (pmetrics_cq_list->priv.contig != 0) {
        /* Assume that the future CMD.DW11.PC bit will be set to one. */
        pmetrics_cq_list->priv.buf = dma_alloc_coherent(
            &pnvme_dev->priv.pdev->dev, pmetrics_cq_list->priv.
            size, &pmetrics_cq_list->priv.cq_dma_addr, GFP_KERNEL);
        if (pmetrics_cq_list->priv.buf == NULL) {
            pr_err("Unable to allocate DMA mem for IOCQ");
            ret_code = -ENOMEM;
            goto pcq_out;
        }
        /* Zero out; forces reset of all P-bit entries */
        memset(pmetrics_cq_list->priv.buf, 0,
            pmetrics_cq_list->priv.size);
    }

    // Learn of the doorbell stride
    cap_dstrd = ((READQ(&pnvme_dev->priv.ctrlr_regs->cap) >> 32) & 0xF);
    pr_debug("CAP.DSTRD = 0x%x", cap_dstrd);

    /* Here CQ also used SQ0TDBL offset i.e., 0x1000h. */
    pmetrics_cq_list->priv.dbs = (u32 __iomem *)
        (pnvme_dev->priv.bar0 + NVME_SQ0TBDL +
        ((2 * pmetrics_cq_list->pub.q_id + 1) * (4 << cap_dstrd)));
    return 0;

pcq_out:
    if (pmetrics_cq_list->priv.buf != NULL) {
        dma_free_coherent(&pnvme_dev->priv.pdev->dev, pmetrics_cq_list->
            priv.size, (void *)pmetrics_cq_list->priv.
            buf, pmetrics_cq_list->priv.cq_dma_addr);
    }
    return ret_code;
}


/*
 * nvme_ring_sqx_dbl - This routine is called when the driver invokes the ioctl
 * for Ring SQ doorbell. It will retrieve the q from the linked list, copy the
 * tail_ptr with virtual pointer, and write the tail pointer value to SqxTDBL
 * already in dbs.
 */
int nvme_ring_sqx_dbl(u16 ring_sqx, struct nvme_context *pmetrics_device)
{
    struct nvme_sq *pmetrics_sq;

    pmetrics_sq = dnvme_find_sq(pmetrics_device, ring_sqx);
    if (pmetrics_sq == NULL) {
        pr_err("SQ ID = %d does not exist", ring_sqx);
        return -EINVAL;
    }

    pr_debug("SQ_ID= %d found in kernel metrics.",
        pmetrics_sq->pub.sq_id);
    pr_debug("\tvirt_tail_ptr = 0x%x; tail_ptr = 0x%x",
        pmetrics_sq->pub.tail_ptr_virt,
        pmetrics_sq->pub.tail_ptr);
    pr_debug("\tdbs = %p; bar0 = %p", pmetrics_sq->priv.dbs,
        pmetrics_device->dev->priv.bar0);
    /* Copy tail_prt_virt to tail_prt */
    pmetrics_sq->pub.tail_ptr = pmetrics_sq->pub.tail_ptr_virt;
    /* Ring the doorbell with tail_prt */
    writel(pmetrics_sq->pub.tail_ptr, pmetrics_sq->priv.dbs);
    // pr_err("########sqx_dbl sqid:%x,cqid:%x tdbl:%x",
    //         pmetrics_sq->pub.sq_id,
    //         pmetrics_sq->pub.cq_id,
    //         pmetrics_sq->pub.tail_ptr);
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
            pmetrics_cq_list->priv.cq_dma_addr);

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
static void dnvme_release_sq(struct device *dev, struct nvme_sq *sq, 
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
		sq->priv.sq_dma_addr);
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
            pr_debug("Retaining ASQ from deallocation");
            /* drop sq cmds and set to zero the public metrics of asq */
            reinit_admn_sq(pmetrics_sq_list, pmetrics_device);
        } else {
            /* Call the generic deallocate sq function */
            dnvme_release_sq(dev, pmetrics_sq_list, pmetrics_device);
        }
    }

    /* Loop for each cq node */
    list_for_each_entry_safe(pmetrics_cq_list, pmetrics_cq_next,
        &pmetrics_device->cq_list, cq_entry) {

        /* Check if Admin Q is excluded or not */
        if (preserve_admin_qs && pmetrics_cq_list->pub.q_id == 0) {
            pr_debug("Retaining ACQ from deallocation");
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

    pr_debug("Reap Inquiry on CQ_ID:PBit:EntrySize = %d:%d:%d",
        pmetrics_cq_node->pub.q_id, tmp_pbit, comp_entry_size);
    pr_debug("CQ Hd Ptr = %d", pmetrics_cq_node->pub.head_ptr);
    pr_debug("Rp Inq. Tail Ptr before = %d", pmetrics_cq_node->pub.
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

    pr_debug("Rp Inq. Tail Ptr After = %d", pmetrics_cq_node->pub.
        tail_ptr);
    pr_debug("cq.elements = %d", pmetrics_cq_node->pub.elements);
    pr_debug("Number of elements remaining = %d", num_remaining);
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
        pr_err("Unable to alloc kernel memory to copy user data");
        err = -ENOMEM;
        goto fail_out;
    }
    if (copy_from_user(user_data, usr_reap_inq,
        sizeof(struct nvme_reap_inquiry))) {

        pr_err("Unable to copy from user space");
        err = -EFAULT;
        goto fail_out;
    }

    /* Find given CQ in list */
    pmetrics_cq_node = dnvme_find_cq(pmetrics_device, user_data->q_id);
    if (pmetrics_cq_node == NULL) {
        /* if the control comes here it implies q id not in list */
        pr_err("CQ ID = %d is not in list", user_data->q_id);
        err = -ENODEV;
        goto fail_out;
    }
    /* Initializing ISR count for all the possible cases */
    user_data->isr_count = 0;
    /* Note: If ISR's are enabled then ACQ will always be attached to INT 0 */
    if (pmetrics_device->dev->pub.irq_active.irq_type
        == INT_NONE) {

        /* Process reap inquiry for non-isr case */
        pr_debug("Non-ISR Reap Inq on CQ = %d",
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
            pr_debug("Non-ISR Reap Inq on CQ = %d",
                pmetrics_cq_node->pub.q_id);
            user_data->num_remaining = reap_inquiry(pmetrics_cq_node,
                &pmetrics_device->dev->priv.pdev->dev);
        } else {
            pr_debug("ISR Reap Inq on CQ = %d",
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
                pr_err("ISR Reap Inquiry failed...");
                err = -EINVAL;
                goto fail_out;
             }
         }
    }

    /* Copy to user the remaining elements in this q */
    if (copy_to_user(usr_reap_inq, user_data,
        sizeof(struct nvme_reap_inquiry))) {

        pr_err("Unable to copy to user space");
        err = -EFAULT;
        goto fail_out;
    }

    /* Check for hw violation of full Q definition */
    if (user_data->num_remaining >= pmetrics_cq_node->pub.elements) {
        pr_err("HW violating full Q definition");
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
        pr_err("SQ ID = %d does not exist", sq_id);
        return -EBADSLT; /* Invalid slot */
    }

    dnvme_release_sq(&pmetrics_device->dev->priv.
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
        pr_err("CQ ID = %d does not exist", cq_id);
        return -EBADSLT;
    }

    /* If irq is enabled then clean up the irq track list
     * NOTE:- only for IO queues
     */
    if (pmetrics_cq_node->pub.irq_enabled != 0) {
        if (remove_icq_node(pmetrics_device, cq_id, pmetrics_cq_node->
            pub.irq_no) < 0) {
            pr_err("Removal of IRQ CQ node failed. ");
            err = -EINVAL;
        }
    }

    deallocate_metrics_cq(&pmetrics_device->dev->priv.
        pdev->dev, pmetrics_cq_node, pmetrics_device);
    return err;
}


static int process_algo_q(struct nvme_sq *pmetrics_sq_node,
    struct nvme_cmd *pcmd_node, u8 free_q_entry,
    struct  nvme_context *pmetrics_device,
    enum metrics_type type)
{
    int err = 0;

    pr_debug("Persist Q Id = %d", pcmd_node->persist_q_id);
    pr_debug("Unique Cmd Id = %d", pcmd_node->unique_id);
    pr_debug("free_q_entry = %d", free_q_entry);

    if (free_q_entry) {
        if (pcmd_node->persist_q_id == 0) {
            pr_err("Trying to delete ACQ is blunder");
            err = -EINVAL;
            return err;
        }
        if (type == METRICS_CQ) {
            err = dnvme_delete_cq(pmetrics_device, pcmd_node->persist_q_id);
            if (err != 0) {
                pr_err("CQ Removal failed...");
                return err;
            }

        } else if (type == METRICS_SQ) {
            err = dnvme_delete_sq(pmetrics_device, pcmd_node->persist_q_id);
            if (err != 0) {
                pr_err("SQ Removal failed...");
                return err;
            }
        }
    }
    err = dnvme_delete_cmd(pmetrics_sq_node, pcmd_node->unique_id);
    if (err != 0) {
        pr_err("Cmd Removal failed...");
        return err;
    }

    return err;
}


static int process_algo_gen(struct nvme_sq *pmetrics_sq_node,
    u16 cmd_id, struct  nvme_context *pmetrics_device)
{
    int err;
    struct nvme_cmd *pcmd_node;

    /* Find the command ndoe */
    pcmd_node = dnvme_find_cmd(pmetrics_sq_node, cmd_id);
    if (pcmd_node == NULL) {
        pr_err("Command id = %d does not exist", cmd_id);
        return -EBADSLT; /* Invalid slot */
    }

    dnvme_delete_prps(pmetrics_device->dev, &pcmd_node->prp_nonpersist);
    err = dnvme_delete_cmd(pmetrics_sq_node, cmd_id);
    return err;
}


static int process_admin_cmd(struct nvme_sq *pmetrics_sq_node,
    struct nvme_cmd *pcmd_node, u16 status,
    struct  nvme_context *pmetrics_device)
{
    int err = 0;

    switch (pcmd_node->opcode) {
    case 0x00:
        /* Delete IOSQ */
        err = process_algo_q(pmetrics_sq_node, pcmd_node, (status == 0),
            pmetrics_device, METRICS_SQ);
        break;
    case 0x01:
        /* Create IOSQ */
        err = process_algo_q(pmetrics_sq_node, pcmd_node, (status != 0),
            pmetrics_device, METRICS_SQ);
        break;
    case 0x04:
        /* Delete IOCQ */
        err = process_algo_q(pmetrics_sq_node, pcmd_node, (status == 0),
            pmetrics_device, METRICS_CQ);
        break;
    case 0x05:
        /* Create IOCQ */
        err = process_algo_q(pmetrics_sq_node, pcmd_node, (status != 0),
            pmetrics_device, METRICS_CQ);
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
    struct nvme_cmd *pcmd_node = NULL;


    /* Find sq node for given sq id in CE */
    pmetrics_sq_node = dnvme_find_sq(pmetrics_device, cq_entry->sq_identifier);
    if (pmetrics_sq_node == NULL) {
        pr_err("SQ ID = %d does not exist", cq_entry->sq_identifier);
        /* Error must be EBADSLT per design; user may want to reap all entry */
        return -EBADSLT; /* Invalid slot */
    }

    /* Update our understanding of the corresponding hdw SQ head ptr */
    pmetrics_sq_node->pub.head_ptr = cq_entry->sq_head_ptr;
    ceStatus = (cq_entry->status_field & 0x7ff);
    pr_debug("(SCT, SC) = 0x%04X", ceStatus);

    /* Find command in sq node */
    pcmd_node = dnvme_find_cmd(pmetrics_sq_node, cq_entry->cmd_identifier);
    if (pcmd_node != NULL) {
        /* A command node exists, now is it an admin cmd or not? */
        if (cq_entry->sq_identifier == 0) {
            pr_debug("Admin cmd set processing");
            err = process_admin_cmd(pmetrics_sq_node, pcmd_node, ceStatus,
                pmetrics_device);
        } else {
            pr_debug("NVM or other cmd set processing");
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
        pr_debug("Reaping CE's, %d left to reap", *num_should_reap);

        /* Call the process reap algos based on CE entry */
        latentErr = process_reap_algos((struct cq_completion *)cq_head_ptr,
            pmetrics_device);
        if (latentErr) {
            pr_err("Unable to find CE.SQ_id in dnvme metrics");
        }

        /* Copy to user even on err; allows seeing latent err */
        if (copy_to_user(buffer, cq_head_ptr, comp_entry_size)) {
            pr_err("Unable to copy request data to user space");
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
            pr_err("Detected a partial reap situation; some, not all reaped");
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
    pr_debug("Head ptr = %d", pmetrics_cq_node->pub.head_ptr);
    pr_debug("Tail ptr = %d", pmetrics_cq_node->pub.tail_ptr);
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
        pr_err("Unable to alloc kernel memory to copy user data");
        err = -ENOMEM;
        goto fail_out;
    }
    if (copy_from_user(user_data, usr_reap_data, sizeof(struct nvme_reap))) {
        pr_err("Unable to copy from user space");
        err = -EFAULT;
        goto fail_out;
    }

    /* Find CQ with given id from user */
    pmetrics_cq_node = dnvme_find_cq(pmetrics_device, user_data->q_id);
    if (pmetrics_cq_node == NULL) {
        pr_err("CQ ID = %d not found", user_data->q_id);
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
                pr_err("ISR Reap Inquiry failed...");
                goto mtx_unlk;
            }
        }
    }

    /* Check for hw violation of full Q definition */
    if (num_could_reap >= pmetrics_cq_node->pub.elements) {
        pr_err("HW violating full Q definition");
        err = -EINVAL;
        goto mtx_unlk;
    }

    /* If this CQ is an IOCQ, not ACQ, then lookup the CE size */
    if (pmetrics_cq_node->pub.q_id != 0) {
        comp_entry_size = (pmetrics_cq_node->priv.size) /
            (pmetrics_cq_node->pub.elements);
    }
    pr_debug("Tail ptr position before reaping = %d",
        pmetrics_cq_node->pub.tail_ptr);
    pr_debug("Detected CE size = 0x%04X", comp_entry_size);
    pr_debug("%d elements could be reaped", num_could_reap);
    if (num_could_reap == 0) {
        pr_debug("All elements reaped, CQ is empty");
        user_data->num_remaining = 0;
        user_data->num_reaped = 0;
    }

    /* Is this request asking for every CE element? */
    if (user_data->elements == 0) {
        user_data->elements = num_could_reap;
    }
    num_will_fit = (user_data->size / comp_entry_size);

    pr_debug("Requesting to reap %d elements", user_data->elements);
    pr_debug("User space reap buffer size = %d", user_data->size);
    pr_debug("Total buffer bytes needed to satisfy request = %d",
        num_could_reap * comp_entry_size);
    pr_debug("num elements which fit in buffer = %d", num_will_fit);

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

    pr_debug("num elements will attempt to reap = %d", num_should_reap);
    pr_debug("num elements expected to remain after reap = %d",
        user_data->num_remaining);
    pr_debug("Head ptr before reaping = %d",
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
    pr_debug("num CE's reaped = %d, num CE's remaining = %d",
        user_data->num_reaped, user_data->num_remaining);

    /* Updating the user structure */
    if (copy_to_user(usr_reap_data, user_data, sizeof(struct nvme_reap))) {
        pr_err("Unable to copy request data to user space");
        err = (err == 0) ? -EFAULT : err;
        goto mtx_unlk;
    }

    /* Update system with number actually reaped */
    if(user_data->num_reaped)
    {
        pos_cq_head_ptr(pmetrics_cq_node, user_data->num_reaped);
        writel(pmetrics_cq_node->pub.head_ptr, pmetrics_cq_node->
        priv.dbs);
        //pr_err("@@@@@@@@@cqid:%x hdbl:%x",pmetrics_cq_node->pub.q_id,pmetrics_cq_node->pub.head_ptr);
    }

    /* if 0 CE in a given cq, then reset the isr flag. */
    if ((pmetrics_cq_node->pub.irq_enabled == 1) &&
        (user_data->num_remaining == 0) &&
        (pmetrics_device->dev->pub.irq_active.irq_type !=
        INT_NONE)) {

        /* reset isr fired flag for the particular irq_no */
        if (reset_isr_flag(pmetrics_device,
            pmetrics_cq_node->pub.irq_no) < 0) {
            pr_err("reset isr fired flag failed");
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
