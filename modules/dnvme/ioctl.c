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
#include <linux/pci_regs.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include "io.h"
#include "core.h"
#include "config.h"

#include "dnvme_ioctl.h"
#include "definitions.h"
#include "dnvme_reg.h"
#include "sysfuncproto.h"
#include "dnvme_sts_chk.h"
#include "dnvme_queue.h"
#include "dnvme_cmds.h"
#include "dnvme_ds.h"
#include "dnvme_irq.h"

static int dnvme_wait_ready(struct nvme_device *ndev, bool enabled)
{
	u64 cap;
	unsigned long timeout;
	void __iomem *bar0 = ndev->priv.bar0;
	u32 bit = enabled ? NVME_CSTS_RDY : 0;
	u32 csts;

	cap = dnvme_readq(bar0, NVME_REG_CAP);
	timeout = ((NVME_CAP_TIMEOUT(cap) + 1) * HZ / 2) + jiffies;

	while (1) {
		csts = dnvme_readl(bar0, NVME_REG_CSTS);
		if (csts == ~0)
			return -ENODEV;
		if ((csts & NVME_CSTS_RDY) == bit)
			break;
		
		usleep_range(1000, 2000);

		if (time_after(jiffies, timeout)) {
			dnvme_err("Device not ready; aborting %s, CSTS=0x%x\n",
				enabled ? "init" : "reset", csts);
			return -ETIME;
		}
	}

	return 0;
}

/**
 * @brief Set controller state
 * 
 * @param ctx NVMe context
 * @return 0 on success, otherwise a negative errno.
 */
int dnvme_set_ctrl_state(struct nvme_context *ctx, bool enabled)
{
	struct nvme_device *ndev = ctx->dev;
	void __iomem *bar0 = ndev->priv.bar0;
	u32 cc;

	cc = dnvme_readl(bar0, NVME_REG_CC);
	if (enabled)
		cc |= NVME_CC_ENABLE;
	else
		cc &= ~NVME_CC_ENABLE;
	dnvme_writel(bar0, NVME_REG_CC, cc);

	return dnvme_wait_ready(ndev, enabled);
}

int dnvme_reset_subsystem(struct nvme_context *ctx)
{
	struct nvme_device *ndev = ctx->dev;
	void __iomem *bar0 = ndev->priv.bar0;
	u32 rstval = 0x4e564d65; /* "NVMe" */

	dnvme_writel(bar0, NVME_REG_NSSR, rstval);

	return dnvme_wait_ready(ndev, false);
}

int dnvme_get_capability(struct nvme_context *ctx, struct nvme_capability __user *ucap)
{
	struct nvme_capability *cap; /* we may extend this struct later */
	struct pci_dev *pdev = ctx->dev->priv.pdev;
	int ret = 0;

	cap = kzalloc(sizeof(*cap), GFP_KERNEL);
	if (!cap) {
		dnvme_err("failed to alloc nvme_capability!\n");
		return -ENOMEM;
	}

	ret = pci_get_capability(pdev, cap);
	if (ret < 0)
		goto out;
	
	if (copy_to_user(ucap, cap, sizeof(*ucap))) {
		dnvme_err("failed to copy to user space!\n");
		ret = -EFAULT;
		goto out;
	}

out:
	kfree(cap);
	return ret;
}

/**
 * @brief Check access offset and size is align with access type.
 * 
 * @return 0 if check success, otherwise a negative errno.
 */
static int dnvme_check_access_align(struct nvme_access *access)
{
	switch (access->type) {
	case NVME_ACCESS_QWORD:
		if ((access->bytes % 8) != 0 || (access->offset % 4) != 0) {
			dnvme_err("offset(%u) shall dword-align, "
				"bytes(%u) shall qword-align!\n", 
				access->offset, access->bytes);
			return -EINVAL;
		}
		break;

	case NVME_ACCESS_DWORD:
		if ((access->bytes % 4) != 0 || (access->offset % 4) != 0) {
			dnvme_err("offset(%u) or bytes(%u) shall dword-align!\n",
				access->offset, access->bytes);
			return -EINVAL;
		}
		break;
	
	case NVME_ACCESS_WORD:
		if ((access->bytes % 2) != 0 || (access->offset % 2) != 0) {
			dnvme_err("offset(%u) or bytes(%u) shall word-align!\n",
				access->offset, access->bytes);
			return -EINVAL;
		}
		break;
	
	case NVME_ACCESS_BYTE:
	default:
		break;
	}

	return 0;
}

/**
 * @brief Read data from NVMe device
 * 
 * @param ctx NVMe device context
 * @return 0 on success, otherwise a negative errno.
 */
int dnvme_generic_read(struct nvme_context *ctx, struct nvme_access __user *uaccess)
{
	int ret = 0;
	void *buf;
	struct nvme_access access;
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->priv.pdev;

	if (copy_from_user(&access, uaccess, sizeof(struct nvme_access))) {
		dnvme_err("Failed to copy from user space!\n");
		return -EFAULT;
	}

	if (!access.bytes) {
		dnvme_dbg("Access size is zero!\n");
		return 0;
	}

	ret = dnvme_check_access_align(&access);
	if (ret < 0)
		return ret;

	buf = kzalloc(access.bytes, GFP_KERNEL);
	if (!buf) {
		dnvme_err("Failed to alloc %ubytes!\n", access.bytes);
		return -ENOMEM;
	}

	switch (access.region) {
	case NVME_PCI_HEADER:
		dnvme_dbg("READ PCI Header Space: 0x%x+0x%x\n", 
			access.offset, access.bytes);
		ret = dnvme_read_from_config(pdev, &access, buf);
		if (ret < 0)
			goto out;
		break;

	case NVME_BAR0_BAR1:
		dnvme_dbg("READ NVMe BAR0~1: 0x%x+0x%x\n", 
			access.offset, access.bytes);
		ret = dnvme_read_from_bar(ndev->priv.bar0, &access, buf);
		if (ret < 0)
			goto out;
		break;

	default:
		dnvme_err("Access region(%d) is unkonwn!\n", access.region);
		ret = -EINVAL;
		goto out;
	}

	if (copy_to_user(access.buffer, buf, access.bytes)) {
		dnvme_err("Failed to copy to user space!\n");
		ret = -EFAULT;
		goto out;
	}

out:
	kfree(buf);
	return ret;
}

/**
 * @brief Write data to NVMe device
 * 
 * @param ctx NVMe device context
 * @return 0 on success, otherwise a negative errno.
 */
int dnvme_generic_write(struct nvme_context *ctx, struct nvme_access __user *uaccess)
{
	int ret = 0;
	void *buf;
	struct nvme_access access;
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->priv.pdev;

	if (copy_from_user(&access, uaccess, sizeof(struct nvme_access))) {
		dnvme_err("Failed to copy from user space!\n");
		return -EFAULT;
	}

	if (!access.bytes) {
		dnvme_dbg("Access size is zero!\n");
		return 0;
	}

	ret = dnvme_check_access_align(&access);
	if (ret < 0)
		return ret;

	buf = kzalloc(access.bytes, GFP_KERNEL);
	if (!buf) {
		dnvme_err("Failed to alloc %ubytes!\n", access.bytes);
		return -ENOMEM;
	}

	if (copy_from_user(buf, access.buffer, access.bytes)) {
		dnvme_err("Failed to copy from user space!\n");
		ret = -EFAULT;
		goto out;
	}

	switch (access.region) {
	case NVME_PCI_HEADER:
		dnvme_dbg("WRITE PCI Header Space: 0x%x+0x%x\n",
			access.offset, access.bytes);
		ret = dnvme_write_to_config(pdev, &access, buf);
		if (ret < 0)
			goto out;
		break;
	
	case NVME_BAR0_BAR1:
		dnvme_dbg("WRITE NVMe BAR0~1: 0x%x+0x%x\n",
			access.offset, access.bytes);
		ret = dnvme_write_to_bar(ndev->priv.bar0, &access, buf);
		if (ret < 0)
			goto out;
		break;

	default:
		dnvme_err("Access region(%d) is unkonwn!\n", access.region);
		ret = -EINVAL;
		goto out;
	}

out:
	kfree(buf);
	return ret;
}

/**
 * @brief Create admin queue
 * 
 * @return 0 on success, otherwise a negative errno.
 */
int dnvme_create_admin_queue(struct nvme_context *ctx, 
	struct nvme_admin_queue __user *uaq)
{
	int ret;
	struct nvme_admin_queue aq;

	if (copy_from_user(&aq, uaq, sizeof(aq))) {
		dnvme_err("failed to copy from user space!\n");
		return -EFAULT;
	}

	if (aq.type == NVME_ADMIN_SQ) {
		ret = dnvme_create_asq(ctx, aq.elements);
	} else if (aq.type == NVME_ADMIN_CQ) {
		ret = dnvme_create_acq(ctx, aq.elements);
	} else {
		dnvme_err("queue type(%d) is unknown!\n", aq.type);
		return -EINVAL;
	}

	return ret;
}

/*
 * Allocate a dma pool for the requested size. Initialize the DMA pool pointer
 * with DWORD alignment and associate it with the active device.
 */
int metabuff_create(struct nvme_context *pmetrics_device_elem,
    u32 alloc_size)
{
    /* First Check if the meta pool already exists */
    if (pmetrics_device_elem->meta_set.pool != NULL) {
        if (alloc_size == pmetrics_device_elem->meta_set.buf_size) {
            return 0;
        }
        dnvme_err("Meta Pool already exists, of a different size");
        return -EINVAL;
    }

    /* Create coherent DMA mapping for meta data buffer creation */
    pmetrics_device_elem->meta_set.pool = dma_pool_create
        ("meta_buff", &pmetrics_device_elem->dev->
        priv.pdev->dev, alloc_size, sizeof(u32), 0);
    if (pmetrics_device_elem->meta_set.pool == NULL) {
        dnvme_err("Creation of DMA Pool failed size = 0x%08X", alloc_size);
        return -ENOMEM;
    }

    pmetrics_device_elem->meta_set.buf_size = alloc_size;
    return 0;
}


/*
 * alloc a meta buffer node when user request and allocate a consistent
 * dma memory from the meta dma pool. Add this node into the meta data
 * linked list.
 */
int metabuff_alloc(struct nvme_context *pmetrics_device_elem,
    u32 id)
{
    struct nvme_meta *pmeta_data = NULL;
    int err = 0;


    /* Check if parameters passed to this function are valid */
    if (pmetrics_device_elem->meta_set.pool == NULL) {
        dnvme_info("Call to Create the meta data pool first...");
        dnvme_err("Meta data pool is not created");
        return -EINVAL;
    }

    pmeta_data = dnvme_find_meta(pmetrics_device_elem, id);
    if (pmeta_data != NULL) {
        dnvme_err("Meta ID = %d already exists", pmeta_data->id);
        return -EINVAL;
    }

    /* Allocate memory to meta_set for each node */
    pmeta_data = kmalloc(sizeof(struct nvme_meta), GFP_KERNEL);
    if (pmeta_data == NULL) {
        dnvme_err("Allocation to contain meta data node failed");
        err = -ENOMEM;
        goto fail_out;
    }

    /* Allocate DMA memory for the meta data buffer */
    pmeta_data->id = id;
    pmeta_data->buf = dma_pool_alloc(pmetrics_device_elem->
        meta_set.pool, GFP_ATOMIC, &pmeta_data->dma);
    if (pmeta_data->buf == NULL) {
        dnvme_err("Allocation for meta data buffer failed");
        err = -ENOMEM;
        goto fail_out;
    }

    /* Add the meta data node into the linked list */
    list_add_tail(&pmeta_data->entry, &pmetrics_device_elem->
        meta_set.meta_list);
    return err;

fail_out:
    if (pmeta_data != NULL) {
        kfree(pmeta_data);
    }
    return err;
}


/*
 * Delete the meta buffer node for given meta id from the linked list.
 * First Free the dma pool allocated memory then delete the entry from the
 * linked list and finally free the node memory from the kernel.
 */
int metabuff_del(struct nvme_context *pmetrics_device,
    u32 id)
{
    struct nvme_meta *pmeta_data;

    /* Check if invalid parameters are passed */
    if (pmetrics_device->meta_set.pool == NULL) {
        dnvme_err("Meta data pool is not created, nothing to delete");
        return -EINVAL;
    }

    /* Check if meta node id exists */
    pmeta_data = dnvme_find_meta(pmetrics_device, id);
    if (pmeta_data == NULL) {
        dnvme_dbg("Meta ID does not exists, it is already deleted");
        return 0;
    }

    /* Free the DMA memory if exists */
    if (pmeta_data->buf != NULL) {
        dma_pool_free(pmetrics_device->meta_set.pool,
            pmeta_data->buf, pmeta_data->dma);
    }

    /* Remove from the linked list and free the node */
    list_del(&pmeta_data->entry);
    kfree(pmeta_data);
    return 0;
}

/*
 * deallocate_mb - This function will start freeing up the memory and
 * nodes for the meta buffers allocated during the alloc and create meta.
 */
void deallocate_mb(struct nvme_context *pmetrics_device)
{
    struct nvme_meta *pmeta_data = NULL;
    struct nvme_meta *pmeta_data_next = NULL;

    /* do not assume the node exists always */
    if (pmetrics_device->meta_set.pool == NULL) {
        dnvme_dbg("Meta node is not allocated..");
        return;
    }
    /* Loop for each meta data node */
    list_for_each_entry_safe(pmeta_data, pmeta_data_next,
        &(pmetrics_device->meta_set.meta_list), entry) {

        /* free the dma memory if exists */
        if (pmeta_data->buf != NULL) {
            dma_pool_free(pmetrics_device->meta_set.pool,
                pmeta_data->buf, pmeta_data->dma);
        }
        /* Remove from the linked list and free the node */
        list_del(&pmeta_data->entry);
        kfree(pmeta_data);
    }

    /* check if it has dma pool created then destroy */
    if (pmetrics_device->meta_set.pool != NULL) {
        dma_pool_destroy(pmetrics_device->meta_set.pool);
        pmetrics_device->meta_set.pool = NULL;
    }
    pmetrics_device->meta_set.buf_size = 0;

    /* Prepare a clean list, empty, ready for next use */
    INIT_LIST_HEAD(&pmetrics_device->meta_set.meta_list);
}


int driver_toxic_dword(struct nvme_context *pmetrics_device,
    struct backdoor_inject *err_inject)
{
    int err = -EINVAL;
    u32 *tgt_dword;                     /* DWORD which needs updating */
    u32 entry_size = 64;                /* Assumption is for ASQ */
    struct nvme_sq *pmetrics_sq;     /* Ptr to specific SQ of interest */
    struct backdoor_inject *user_data = NULL;
#ifdef DEBUG
    int i;
#endif

    /* Allocating memory for user struct in kernel space */
    user_data = kmalloc(sizeof(struct backdoor_inject), GFP_KERNEL);
    if (user_data == NULL) {
        dnvme_err("Unable to alloc kernel memory to copy user data");
        err = -ENOMEM;
        goto fail_out;
    }
    if (copy_from_user(user_data, err_inject, sizeof(struct backdoor_inject))) {
        dnvme_err("Unable to copy from user space");
        err = -EFAULT;
        goto fail_out;
    }

    /* Get the required SQ for which command should be modified */
    pmetrics_sq = dnvme_find_sq(pmetrics_device, user_data->q_id);
    if (pmetrics_sq == NULL) {
        dnvme_err("SQ ID = %d does not exist", user_data->q_id);
        err = -EPERM;
        goto fail_out;
    }

    /* If this SQ is an IOSQ, not ASQ, then lookup the element size */
    if (pmetrics_sq->pub.sq_id != 0) {
        entry_size = (pmetrics_sq->priv.size /
            pmetrics_sq->pub.elements);
    }

    /* The cmd for which is being updated, better not have rung its doorbell */
    if (pmetrics_sq->pub.tail_ptr_virt <
        pmetrics_sq->pub.tail_ptr) {

        // Handle wrapping state of the SQ
        if ((user_data->cmd_ptr < pmetrics_sq->pub.tail_ptr) &&
            (user_data->cmd_ptr >= pmetrics_sq->pub.tail_ptr_virt)) {

            dnvme_err("Already rung doorbell for cmd idx = %d",
                user_data->cmd_ptr);
            err = -EINVAL;
            goto fail_out;
            }

    } else { // no wrap condition
        if (user_data->cmd_ptr < pmetrics_sq->pub.tail_ptr) {
            dnvme_err("Already rung doorbell for cmd idx = %d",
                user_data->cmd_ptr);
            err = -EINVAL;
            goto fail_out;
        }
    }

    /* Validate requirement: [0 -> (CreateIOSQ.DW10.SIZE-1)] */
    if (user_data->cmd_ptr >= pmetrics_sq->pub.elements) {
        dnvme_err("SQ ID %d contains only %d elements",
            pmetrics_sq->pub.sq_id, pmetrics_sq->pub.elements);
        err = -EPERM;
        goto fail_out;
    }

    /* Validate requirement: [0 -> (CC.IOSQES-1)] */
    if (user_data->dword >= entry_size) {
        dnvme_err("SQ ID %d elements are only of size %d bytes",
            pmetrics_sq->pub.sq_id, entry_size);
        err = -EPERM;
        goto fail_out;
    }

    /* Inject the requested bit values into the appropriate place */
    if (pmetrics_sq->priv.contig) {
#ifdef DEBUG
        for (i = 0; i < entry_size; i += sizeof(u32)) {
            dnvme_dbg("B4 cmd DW%d = 0x%08X", (int)(i / sizeof(u32)),
                *((u32 *)(pmetrics_sq->priv.buf +
                (user_data->cmd_ptr * entry_size) + i)));
        }
#endif
        tgt_dword = (u32 *)(pmetrics_sq->priv.buf +
            (user_data->cmd_ptr * entry_size) +
            (user_data->dword * sizeof(u32)));
        dnvme_dbg("B4 tgt_DW%d = 0x%08X", user_data->dword, *tgt_dword);
        *tgt_dword &= ~user_data->value_mask;
        *tgt_dword |= (user_data->value & user_data->value_mask);
        dnvme_dbg("After tgt_DW%d = 0x%08X", user_data->dword, *tgt_dword);
#ifdef DEBUG
        for (i = 0; i < entry_size; i += sizeof(u32)) {
            dnvme_dbg("After cmd DW%d = 0x%08X", (int)(i / sizeof(u32)),
                *((u32 *)(pmetrics_sq->priv.buf +
                (user_data->cmd_ptr * entry_size) + i)));
        }
#endif
    } else {
#ifdef DEBUG
        for (i = 0; i < entry_size; i += sizeof(u32)) {
            dnvme_dbg("B4 cmd DW%d = 0x%08X", (int)(i / sizeof(u32)),
                *((u32 *)(pmetrics_sq->priv.prp_persist.buf +
                (user_data->cmd_ptr * entry_size) + i)));
        }
#endif
        tgt_dword = (u32 *)(pmetrics_sq->priv.prp_persist.buf +
            (user_data->cmd_ptr * entry_size) +
            (user_data->dword * sizeof(u32)));
        dnvme_dbg("B4 tgt_DW%d = 0x%08X", user_data->dword, *tgt_dword);
        *tgt_dword &= ~user_data->value_mask;
        *tgt_dword |= (user_data->value & user_data->value_mask);
        dnvme_dbg("After tgt_DW%d = 0x%08X", user_data->dword, *tgt_dword);

        dma_sync_sg_for_device(pmetrics_device->dev->
            priv.dmadev, pmetrics_sq->priv.prp_persist.sg,
            pmetrics_sq->priv.prp_persist.num_map_pgs,
            pmetrics_sq->priv.prp_persist.data_dir);

#ifdef DEBUG
        for (i = 0; i < entry_size; i += sizeof(u32)) {
            dnvme_dbg("After cmd DW%d = 0x%08X", (int)(i / sizeof(u32)),
                *((u32 *)(pmetrics_sq->priv.prp_persist.buf +
                (user_data->cmd_ptr * entry_size) + i)));
        }
#endif
    }
    return 0;


fail_out:
    if (user_data != NULL) {
        kfree(user_data);
    }
    dnvme_dbg("Injecting toxic cmd bits failed");
    return err;
}


int driver_send_64b(struct nvme_context *pmetrics_device,
    struct nvme_64b_send *cmd_request)
{
    int err = -EINVAL;
    u32 cmd_buf_size = 0;
    /* Particular SQ from linked list of SQ's for device */
    struct nvme_sq *pmetrics_sq;
    /* SQ represented by the CMD.QID */
    struct nvme_sq *p_cmd_sq;
    /* Particular CQ (within CMD) from linked list of Q's for device */
    struct nvme_cq *p_cmd_cq;
    /* struct describing the meta buf */
    struct nvme_meta *meta_buf;
    /* Kernel space memory for passed in command */
    void *nvme_cmd_ker = NULL;
    /* Pointer to passed in command DW0-DW9 */
    struct nvme_gen_cmd *nvme_gen_cmd;
    /* Pointer to Gen IOSQ command */
    struct nvme_create_sq *nvme_create_sq;
    /* Pointer to Gen IOCQ command */
    struct nvme_create_cq *nvme_create_cq;
    /* Pointer to Delete IO Q command */
    struct nvme_del_q *nvme_del_q;
    /* void * pointer to check validity of Queues */
    void *q_ptr = NULL;
    struct nvme_prps prps; /* Pointer to PRP List */
    struct nvme_64b_send *user_data = NULL;


    /* Allocating memory for user struct in kernel space */
    user_data = kmalloc(sizeof(struct nvme_64b_send), GFP_KERNEL);
    if (user_data == NULL) {
        dnvme_err("Unable to alloc kernel memory to copy user data");
        err = -ENOMEM;
        goto free_out;
    }
    if (copy_from_user(user_data, cmd_request, sizeof(struct nvme_64b_send))) {
        dnvme_err("Unable to copy from user space");
        err = -EFAULT;
        goto free_out;
    }

    /* Initial invalid arguments checking */
    if (NULL == user_data->cmd_buf_ptr) {
        dnvme_err("Command Buffer does not exist");
        goto free_out;
    } else if (
        (user_data->data_buf_size != 0 && NULL == user_data->data_buf_ptr) ||
        (user_data->data_buf_size == 0 && NULL != user_data->data_buf_ptr)) {

        dnvme_err("Data buffer size and data buffer inconsistent");
        goto free_out;
    }

    /* Get the required SQ through which command should be sent */
    pmetrics_sq = dnvme_find_sq(pmetrics_device, user_data->q_id);
    if (pmetrics_sq == NULL) {
        dnvme_err("SQ ID = %d does not exist", user_data->q_id);
        err = -EPERM;
        goto free_out;
    }
    /* Learn the command size */
    cmd_buf_size =
        (pmetrics_sq->priv.size / pmetrics_sq->pub.elements);

    /* Check for SQ is full */
    if ((((u32)pmetrics_sq->pub.tail_ptr_virt + 1UL) %
        pmetrics_sq->pub.elements) ==
        (u32)pmetrics_sq->pub.head_ptr) {

        dnvme_err("SQ is full");
        err = -EPERM;
        goto free_out;
    }

    /* Allocating memory for the command in kernel space */
    nvme_cmd_ker = kmalloc(cmd_buf_size, GFP_ATOMIC | __GFP_ZERO);
    if (nvme_cmd_ker == NULL) {
        dnvme_err("Unable to allocate kernel memory");
        err = -ENOMEM;
        goto free_out;
    }
    if (copy_from_user(nvme_cmd_ker, user_data->cmd_buf_ptr, cmd_buf_size)) {
        dnvme_err("Invalid copy from user space");
        err = -EFAULT;
        goto free_out;
    }

    nvme_gen_cmd = (struct nvme_gen_cmd *)nvme_cmd_ker;
    memset(&prps, 0, sizeof(prps));

    /* Copy and Increment the CMD ID, copy back to user space so can see ID */
    user_data->unique_id = pmetrics_sq->priv.unique_cmd_id++;
    //2021/05/15 meng_yu https://github.com/nvmecompliance/dnvme/issues/7
    nvme_gen_cmd->command_id = user_data->unique_id; 
    if (copy_to_user(cmd_request, user_data, sizeof(struct nvme_64b_send))) {
        dnvme_err("Unable to copy to user space");
        err = -EFAULT;
        goto fail_out;
    }

    /* Handling meta buffer */
    if (user_data->bit_mask & MASK_MPTR) {
        meta_buf = dnvme_find_meta(pmetrics_device, user_data->meta_buf_id);
        if (NULL == meta_buf) {
            dnvme_err("Meta Buff ID not found");
            err = -EINVAL;
            goto fail_out;
        }
        /* Add the required information to the command */
        nvme_gen_cmd->metadata = cpu_to_le64(meta_buf->dma);
        dnvme_dbg("Metadata address: 0x%llx", nvme_gen_cmd->metadata);
    }

    /* Special handling for opcodes 0x00,0x01,0x04 and 0x05 of Admin cmd set */
    if ((user_data->q_id == 0) && (nvme_gen_cmd->opcode == 0x01)) {
        /* Create IOSQ command */
        nvme_create_sq = (struct nvme_create_sq *) nvme_cmd_ker;

        /* Get the required SQ from the global linked list from CMD.DW10.QID */
        list_for_each_entry(p_cmd_sq, &pmetrics_device->sq_list,
            sq_entry) {
            if (nvme_create_sq->sqid == p_cmd_sq->pub.sq_id) {
                q_ptr = (struct  nvme_sq  *)p_cmd_sq;
                break;
            }
        }
        if (q_ptr == NULL) {
            err = -EPERM;
            dnvme_err("SQID node present in create SQ, but lookup found nothing");
            goto fail_out;
        }

        /* Sanity Checks */
        if( ((nvme_create_sq->sq_flags & CDW11_PC) && (p_cmd_sq->priv.contig == 0)) ||
            (!(nvme_create_sq->sq_flags & CDW11_PC) && (p_cmd_sq->priv.contig != 0)) ) 
        {
            dnvme_err("Sanity Checks:sq_flags Contig flag out of sync with what cmd states for SQ");
            goto fail_out;
        } 
		else if( (p_cmd_sq->priv.contig == 0 && user_data->data_buf_ptr == NULL) ||
                 (p_cmd_sq->priv.contig != 0 && p_cmd_sq->priv.buf == NULL) ) 
        {
            dnvme_err("Sanity Checks:buf_ptr Contig flag out of sync with what cmd states for SQ");
            goto fail_out;
        } else if ((p_cmd_sq->priv.bit_mask & UNIQUE_QID_FLAG) == 0) {
            /* Avoid duplicate Queue creation */
            dnvme_err("Required Queue already created!");
            err = -EINVAL;
            goto fail_out;
        }

        if (p_cmd_sq->priv.contig == 0) {
            /* Creation of Discontiguous IO SQ */
            if (p_cmd_sq->priv.size != user_data->data_buf_size) {
                dnvme_err("Contig flag out of sync with what cmd states for SQ");
                goto fail_out;
            }
            err = prep_send64b_cmd(pmetrics_device->dev,
                pmetrics_sq, user_data, &prps, nvme_gen_cmd,
                nvme_create_sq->sqid, DISCONTG_IO_Q, PRP_PRESENT);
            if (err < 0) {
                dnvme_err("Failure to prepare 64 byte command");
                goto fail_out;
            }
        } else {
            /* Contig IOSQ creation */
            err = prep_send64b_cmd(pmetrics_device->dev,
                pmetrics_sq, user_data, &prps, nvme_gen_cmd,
                nvme_create_sq->sqid, CONTG_IO_Q, PRP_ABSENT);
            if (err < 0) {
                dnvme_err("Failure to prepare 64 byte command");
                goto fail_out;
            }
            nvme_gen_cmd->dptr.prp1 = cpu_to_le64(p_cmd_sq->priv.dma);
        }
		
        /* Fill the persistent entry structure */
        memcpy(&p_cmd_sq->priv.prp_persist, &prps, sizeof(prps));

        /* Resetting the unique QID bitmask flag */
        p_cmd_sq->priv.bit_mask =
            (p_cmd_sq->priv.bit_mask & ~UNIQUE_QID_FLAG);

    } else if ((user_data->q_id == 0) && (nvme_gen_cmd->opcode == 0x05)) {
        /* Create IOCQ command */
        nvme_create_cq = (struct nvme_create_cq *) nvme_cmd_ker;

        /* Get the required CQ from the global linked list
         * represented by CMD.DW10.QID */
        list_for_each_entry(p_cmd_cq, &pmetrics_device->cq_list,
            cq_entry) {

            if (nvme_create_cq->cqid == p_cmd_cq->pub.q_id) {
                q_ptr = (struct  nvme_cq  *)p_cmd_cq;
                break;
            }
        }
        if (q_ptr == NULL) {
            err = -EPERM;
            dnvme_err("CQID node present in create CQ, but lookup found nothing");
            goto fail_out;
        }

        /* Sanity Checks */
        if( ((nvme_create_cq->cq_flags & CDW11_PC) && (p_cmd_cq->priv.contig == 0)) || 
			(!(nvme_create_cq->cq_flags & CDW11_PC) && (p_cmd_cq->priv.contig != 0)) ) 
		{
            dnvme_err("Sanity Checks:cq_flags Contig flag out of sync with what cmd states for CQ");
            goto fail_out;
        } 
		else if( (p_cmd_cq->priv.contig==0 && user_data->data_buf_ptr==NULL) ||
                 (p_cmd_cq->priv.contig != 0 && p_cmd_cq->priv.buf==NULL) ) 
        {
            dnvme_err("Sanity Checks:buf_ptr Contig flag out of sync with what cmd states for CQ");
            goto fail_out;
        } else if ((p_cmd_cq->priv.bit_mask & UNIQUE_QID_FLAG) == 0) {
            /* Avoid duplicate Queue creation */
            dnvme_err("Required Queue already created!");
            err = -EINVAL;
            goto fail_out;
        }

        /* Check if interrupts should be enabled for IO CQ */
        if (nvme_create_cq->cq_flags & CDW11_IEN) {
            /* Check the Interrupt scheme set up */
            if (pmetrics_device->dev->pub.irq_active.irq_type
                == INT_NONE) {
                dnvme_err("Interrupt scheme and Create IOCQ cmd out of sync");
                err = -EINVAL;
                goto fail_out;
            }
        }

        if(p_cmd_cq->priv.contig == 0)  				// Discontig IOCQ creation 
		{
            if(p_cmd_cq->priv.size != user_data->data_buf_size) 
			{
                dnvme_err("p_cmd_cq->priv.size:%x != user_data->data_buf_size:%x",p_cmd_cq->priv.size,user_data->data_buf_size);
                dnvme_err("Contig flag out of sync with what cmd states for CQ");
                goto fail_out;
            }
            err = prep_send64b_cmd( pmetrics_device->dev, pmetrics_sq, user_data, &prps,
                				    nvme_gen_cmd, nvme_create_cq->cqid, DISCONTG_IO_Q, PRP_PRESENT );
            dnvme_err("Discontig IOCQ creation: p_cmd_cq->pub.head_ptr:%x",p_cmd_cq->pub.head_ptr);
            if(err<0) 
			{
                dnvme_err("Failure to prepare 64 byte command");
                goto fail_out;
            }
        } else {
            /* Contig IOCQ creation */
            err = prep_send64b_cmd(pmetrics_device->dev,
                pmetrics_sq, user_data, &prps, nvme_gen_cmd,
                nvme_create_cq->cqid, CONTG_IO_Q, PRP_ABSENT);
            if (err < 0) {
                dnvme_err("Failure to prepare 64 byte command");
                goto fail_out;
            }
            nvme_gen_cmd->dptr.prp1 = cpu_to_le64(p_cmd_cq->priv.dma);
        }

        /* Fill the persistent entry structure */
        memcpy(&p_cmd_cq->priv.prp_persist, &prps, sizeof(prps));

        /* Resetting the unique QID bitmask flag */
        p_cmd_cq->priv.bit_mask =
            (p_cmd_cq->priv.bit_mask & ~UNIQUE_QID_FLAG);

    } else if ((user_data->q_id == 0) && (nvme_gen_cmd->opcode == 0x00)) {
        /* Delete IOSQ case */
        nvme_del_q = (struct nvme_del_q *) nvme_cmd_ker;

        if (user_data->data_buf_ptr != NULL) {
            dnvme_err("Invalid argument for opcode 0x00");
            goto fail_out;
        }

        err = prep_send64b_cmd(pmetrics_device->dev,
            pmetrics_sq, user_data, &prps, nvme_gen_cmd, nvme_del_q->qid,
             0, PRP_ABSENT);

        if (err < 0) {
            dnvme_err("Failure to prepare 64 byte command");
            goto fail_out;
        }

    } else if ((user_data->q_id == 0) && (nvme_gen_cmd->opcode == 0x04)) {
        /* Delete IOCQ case */
        nvme_del_q = (struct nvme_del_q *) nvme_cmd_ker;

        if (user_data->data_buf_ptr != NULL) {
            dnvme_err("Invalid argument for opcode 0x00");
            goto fail_out;
        }

        err = prep_send64b_cmd(pmetrics_device->dev,
            pmetrics_sq, user_data, &prps, nvme_gen_cmd, nvme_del_q->qid,
            0, PRP_ABSENT);

        if (err < 0) {
            dnvme_err("Failure to prepare 64 byte command");
            goto fail_out;
        }

    } else {
        /* For rest of the commands */
        if (user_data->data_buf_ptr != NULL) {
            err = prep_send64b_cmd(pmetrics_device->dev,
                pmetrics_sq, user_data, &prps, nvme_gen_cmd,
                PERSIST_QID_0, DATA_BUF, PRP_PRESENT);
            if (err < 0) {
                dnvme_err("Failure to prepare 64 byte command");
                goto fail_out;
            }
        }
    }

    /* Copying the command in to appropriate SQ and handling sync issues */
    if(pmetrics_sq->priv.contig) 
	{
        memcpy((pmetrics_sq->priv.buf + ((u32)pmetrics_sq->pub.tail_ptr_virt * cmd_buf_size)),
                nvme_cmd_ker, cmd_buf_size);

        //dnvme_err("@@@@@@@ test: 0x%x", *(uint8_t *)(pmetrics_sq->priv.buf + ((u32)pmetrics_sq->pub.tail_ptr_virt * cmd_buf_size) + 44));        
    } 
	else 
	{
        memcpy((pmetrics_sq->priv.prp_persist.buf + ((u32)pmetrics_sq->pub.tail_ptr_virt * cmd_buf_size)),
                nvme_cmd_ker, cmd_buf_size);
		
        dma_sync_sg_for_device(pmetrics_device->dev->priv.dmadev, pmetrics_sq->priv.prp_persist.sg, 
            				   pmetrics_sq->priv.prp_persist.num_map_pgs, pmetrics_sq->priv.prp_persist.data_dir);
    }

    /* Increment the Tail pointer and handle roll over conditions */
    pmetrics_sq->pub.tail_ptr_virt = (u16)(((u32)pmetrics_sq->pub.tail_ptr_virt + 1UL) % 
                                                  pmetrics_sq->pub.elements);
    // dnvme_err("@@@@@@@ sqid:%x..cqid:%x,tail_ptr_virt:%#x,elements:%#x,",
    //         pmetrics_sq->pub.sq_id,
    //         pmetrics_sq->pub.cq_id,
    //         pmetrics_sq->pub.tail_ptr_virt, 
    //         pmetrics_sq->pub.elements);        

    kfree(nvme_cmd_ker);
    kfree(user_data);
    dnvme_dbg("Command sent successfully");
    return 0;

fail_out:
    pmetrics_sq->priv.unique_cmd_id--;
free_out:
    if (nvme_cmd_ker != NULL) {
        kfree(nvme_cmd_ker);
    }
    if (user_data != NULL) {
        kfree(user_data);
    }
    dnvme_err("Sending of command failed");
    return err;
}


int dnvme_get_queue(struct nvme_context *ctx, struct nvme_get_queue __user *uq)
{
	struct nvme_get_queue q;
	struct nvme_sq *sq;
	struct nvme_cq *cq;

	if (copy_from_user(&q, uq, sizeof(q))) {
		dnvme_err("failed to copy from user space!\n");
		return -EFAULT;
	}

	switch (q.type) {
	case NVME_SQ:
		if (q.bytes < sizeof(struct nvme_sq_public)) {
			dnvme_err("No sufficient buf to copy SQ info!\n");
			return -EINVAL;
		}

		sq = dnvme_find_sq(ctx, q.q_id);
		if (!sq) {
			dnvme_err("SQ(%u) doesn't exist!\n", q.q_id);
			return -EBADSLT;
		}

		if (copy_to_user(q.buf, &sq->pub, sizeof(struct nvme_sq_public))) {
			dnvme_err("failed to copy to user space!\n");
			return -EFAULT;
		}
		break;

	case NVME_CQ:
		if (q.bytes < sizeof(struct nvme_cq_public)) {
			dnvme_err("No sufficient buf to copy CQ info!\n");
			return -EINVAL;
		}

		cq = dnvme_find_cq(ctx, q.q_id);
		if (!cq) {
			dnvme_err("CQ(%u) doesn't exist!\n", q.q_id);
			return -EBADSLT;
		}

		if (copy_to_user(q.buf, &cq->pub, sizeof(struct nvme_cq_public))) {
			dnvme_err("failed to copy to user space!\n");
			return -EFAULT;
		}
		break;

	default:
		dnvme_err("queue type(%d) is unknown!\n", q.type);
		return -EINVAL;
	}

	return 0;
}

/*
 * driver_nvme_prep_sq - This function will try to allocate kernel
 * space for the corresponding unique SQ. If the sq_id is duplicated
 * this will return error to the caller. If the kernel memory is not
 * available then fail and return NOMEM error code.
 */
int driver_nvme_prep_sq(struct nvme_prep_sq *prep_sq, struct nvme_context *ctx)
{
    int err;
    struct nvme_prep_sq *user_data = NULL;
    struct nvme_sq *sq = NULL;
    struct nvme_device *ndev = ctx->dev;


    /* Allocating memory for user struct in kernel space */
    user_data = kmalloc(sizeof(struct nvme_prep_sq), GFP_KERNEL);
    if (user_data == NULL) {
        dnvme_err("Unable to alloc kernel memory to copy user data");
        err = -ENOMEM;
        goto fail_out;
    }
    if (copy_from_user(user_data, prep_sq, sizeof(struct nvme_prep_sq))) {
        dnvme_err("Unable to copy from user space");
        err = -EFAULT;
        goto fail_out;
    }

    err = dnvme_check_qid_unique(ctx, NVME_SQ, user_data->sq_id);
    if (err != 0) {
        dnvme_err("SQ ID is not unique.");
        goto fail_out;
    }

    if (READQ(&ndev->priv.ctrlr_regs->cap) & REGMASK_CAP_CQR) {
        if (user_data->contig == 0) {
            dnvme_err("Device doesn't support discontig Q memory");
            err = -ENOMEM;
            goto fail_out;
        }
    }

    dnvme_dbg("Allocating SQ node in linked list.");
    sq = kmalloc(sizeof(struct nvme_sq), GFP_KERNEL);
    if (sq == NULL) {
        dnvme_err("Failed kernel alloc for SQ metrics node");
        err = -ENOMEM;
        goto fail_out;
    }

    /* Filling the data elements of sq metrics. */
    memset(sq, 0, sizeof(struct nvme_sq));
    sq->pub.sq_id = user_data->sq_id;
    sq->pub.cq_id = user_data->cq_id;
    sq->pub.elements = user_data->elements;
    sq->priv.contig = user_data->contig;

    err = nvme_prepare_sq(sq, ndev);
    if (err < 0) {
        dnvme_err("nvme_prepare_sq fail");
        goto fail_out;
    }
    // dnvme_err("pre_sq:%d,cqid%d", pmetrics_sq_node->pub.sq_id, pmetrics_sq_node->pub.cq_id);

    INIT_LIST_HEAD(&(sq->priv.cmd_list));
    sq->priv.bit_mask = (sq->priv.bit_mask | UNIQUE_QID_FLAG);

    /* Add this element to the end of the list */
    list_add_tail(&sq->sq_entry, &ctx->sq_list);

    kfree(user_data);
    return 0;

fail_out:
    if (sq != NULL) {
        kfree(sq);
    }
    if (user_data != NULL) {
        kfree(user_data);
    }
    return err;
}


/*
 * driver_nvme_prep_cq - This function will try to allocate kernel
 * space for the corresponding unique CQ. If the cq_id is duplicated
 * this will return error to the caller. If the kernel memory is not
 * available then fail and return NOMEM error code.
 */
int driver_nvme_prep_cq(struct nvme_prep_cq *prep_cq, struct nvme_context *ctx)
{
    int err;
    struct nvme_prep_cq *user_data = NULL;
    struct nvme_cq *cq = NULL;
    struct nvme_device *ndev = ctx->dev;


    /* Allocating memory for user struct in kernel space */
    user_data = kmalloc(sizeof(struct nvme_prep_cq), GFP_KERNEL);
    if (user_data == NULL) {
        dnvme_err("Unable to alloc kernel memory to copy user data");
        err = -ENOMEM;
        goto fail_out;
    }
    if (copy_from_user(user_data, prep_cq, sizeof(struct nvme_prep_cq))) {
        dnvme_err("Unable to copy from user space");
        err = -EFAULT;
        goto fail_out;
    }

    err = dnvme_check_qid_unique(ctx, NVME_CQ, user_data->cq_id);
    if (err != 0) {
        dnvme_err("CQ ID is not unique");
        goto fail_out;
    }

    if (READQ(&ndev->priv.ctrlr_regs->cap) & REGMASK_CAP_CQR) {
        if (user_data->contig == 0) {
            dnvme_err("Device doesn't support discontig Q memory");
            err = -ENOMEM;
            goto fail_out;
        }
    }

    dnvme_dbg("Allocating CQ node in linked list.");
    cq = kmalloc(sizeof(struct nvme_cq), GFP_KERNEL);
    if (cq == NULL) {
        dnvme_err("Failed kernel alloc for CQ metrics node");
        err = -ENOMEM;
        goto fail_out;
    }

    /* Filling the data elements of sq metrics. */
    memset(cq, 0, sizeof(struct nvme_cq));
    cq->pub.q_id = user_data->cq_id;
    cq->pub.elements = user_data->elements;
    cq->pub.irq_enabled = user_data->cq_irq_en;
    cq->pub.irq_no = user_data->cq_irq_no;
    cq->priv.contig = user_data->contig;

    err = nvme_prepare_cq(cq, ndev);
    if (err < 0) {
        dnvme_err("nvme_prepare_cq fail");
        goto fail_out;
    }
    // dnvme_err("pre_cq:%d",pmetrics_cq_node->pub.q_id);

    cq->pub.pbit_new_entry = 1;
    cq->priv.bit_mask =
        (cq->priv.bit_mask | UNIQUE_QID_FLAG);

    /* Add this element to the end of the list */
    list_add_tail(&cq->cq_entry, &ctx->cq_list);

	if(cq->pub.irq_enabled)
	{
		err = update_cq_irqtrack(ctx, cq->pub.q_id, cq->pub.irq_no);
		if(err<0) 
		{
            		dnvme_err("update_cq_irqtrack fail");
			goto fail_out;
		}
	}

    kfree(user_data);
    return 0;

fail_out:
    if (cq != NULL) {
        kfree(cq);
    }
    if(user_data != NULL) 
	{
        kfree(user_data);
    }
    return err;
}
#if 0
/*
Boot Partition Memory Buffer Base Address (BMBBA): Specifies the 64-bit physical
address for the Boot Partition Memory Buffer. This address shall be 4KB aligned. Note
that this field contains the 52 most significant bits of the 64 bit address.
add by yumeng 2019.4.22
 */
int driver_nvme_write_bp_buf(struct nvme_write_bp_buf *nvme_data, struct nvme_context *pmetrics_device)
{
    u16 index;
    int err = -EINVAL;
    void *datap = NULL;
    //void *bp_buf = NULL;
    struct nvme_device *nvme_dev;
    struct nvme_write_bp_buf *user_data = NULL;
    uint64_t BMBBA = 0;

    //this function is error!!
    goto fail_out;

    /* Allocating memory for user struct in kernel space */
    user_data = kmalloc(sizeof(struct nvme_write_bp_buf), GFP_KERNEL);
    if (user_data == NULL) {
        dnvme_err("Unable to alloc kernel memory to copy user data");
        err = -ENOMEM;
        goto fail_out;
    }
    if (copy_from_user(user_data, nvme_data, sizeof(struct nvme_write_bp_buf))) {
        dnvme_err("Unable to copy from user space");
        err = -EFAULT;
        goto fail_out;
    }

    /* Allocating memory for the data in kernel space */
    datap = kmalloc(sizeof(uint64_t), GFP_KERNEL | __GFP_ZERO);
    if (!datap) {
        dnvme_err("Unable to allocate kernel memory");
        return -ENOMEM;
    }

    //error!!!
    //there is error! kernel space memory is cann't kmalloc to user space memory
    //error!!!
    user_data->bp_buf = kmalloc(user_data->bp_buf_size, GFP_KERNEL | __GFP_ZERO);
    if (!user_data->bp_buf) {
        dnvme_err("Unable to allocate kernel memory");
        return -ENOMEM;
    }

    BMBBA = (uint64_t)(user_data->bp_buf);

    dnvme_err("Boot Partition Memory Buffer Base Address:0x%lx",BMBBA);

    nvme_dev = pmetrics_device->dev;

    /* Copying user space buffer to kernel memory */
    if (copy_from_user(datap, (uint8_t *)&BMBBA, sizeof(uint64_t))) {
        dnvme_err("Invalid copy from user space");
        err = -EFAULT;
        goto fail_out;
    }

    switch (user_data->type) {
    case NVME_BAR0_BAR1:
        err = write_nvme_reg_generic(nvme_dev->priv.bar0,
            datap, user_data->bytes, user_data->offset, user_data->acc_type);
        if (err < 0) {
            dnvme_err("Write NVME Space failed");
            goto fail_out;
        }
        break;
    default:
        dnvme_dbg("Could not find switch case using default");
        err = -EINVAL;
        break;
    }
    /* Fall through upon success is meant to be */

fail_out:
    if (datap != NULL) {
        kfree(datap);
    }
    if (user_data != NULL) {
        kfree(user_data);
    }
    if (bp_buf != NULL) {
        kfree(user_data->bp_buf);
    }
    
    return err;
}

#endif
