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

#include "dnvme_ioctl.h"
#include "io.h"
#include "core.h"
#include "pci.h"
#include "queue.h"
#include "irq.h"

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
static int dnvme_set_ctrl_state(struct nvme_context *ctx, bool enabled)
{
	struct nvme_device *ndev = ctx->dev;
	void __iomem *bar0 = ndev->priv.bar0;
	u32 cc;

	cc = dnvme_readl(bar0, NVME_REG_CC);
	if (enabled) {
		cc |= NVME_CC_IOSQES | NVME_CC_IOCQES;
		cc |= NVME_CC_ENABLE;
	} else {
		cc &= ~NVME_CC_ENABLE;
	}
	dnvme_writel(bar0, NVME_REG_CC, cc);

	return dnvme_wait_ready(ndev, enabled);
}

static int dnvme_reset_subsystem(struct nvme_context *ctx)
{
	struct nvme_device *ndev = ctx->dev;
	void __iomem *bar0 = ndev->priv.bar0;
	u32 rstval = 0x4e564d65; /* "NVMe" */

	dnvme_writel(bar0, NVME_REG_NSSR, rstval);

	return dnvme_wait_ready(ndev, false);
}

int dnvme_set_device_state(struct nvme_context *ctx, enum nvme_state state)
{
	int ret;

	switch (state) {
	case NVME_ST_ENABLE:
		ret =  dnvme_set_ctrl_state(ctx, true);
		break;

	case NVME_ST_DISABLE:
	case NVME_ST_DISABLE_COMPLETE:
		ret = dnvme_set_ctrl_state(ctx, false);
		if (ret < 0) {
			dnvme_err("failed to set ctrl state(%d)!\n", state);
		} else {
			dnvme_cleanup_context(ctx, state);
		}
		break;

	case NVME_ST_RESET_SUBSYSTEM:
		ret = dnvme_reset_subsystem(ctx);
		/* !NOTICE: It's necessary to clean device here? */
		break;

	default:
		dnvme_err("nvme state(%d) is unkonw!\n", state);
		ret = -EINVAL;
	}

	return ret;
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

int dnvme_prepare_sq(struct nvme_context *ctx, struct nvme_prep_sq __user *uprep)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_sq *sq;
	struct nvme_prep_sq prep;
	int ret;

	if (copy_from_user(&prep, uprep, sizeof(prep))) {
		dnvme_err("failed to copy from user space!\n");
		return -EFAULT;
	}

	if (!prep.elements || prep.elements > ndev->q_depth) {
		dnvme_err("SQ elements(%u) is invalid!\n", prep.elements);
		return -EINVAL;
	}

	ret = dnvme_check_qid_unique(ctx, NVME_SQ, prep.sq_id);
	if (ret < 0)
		return ret;

	if (ndev->prop.cap & NVME_CAP_CQR && !prep.contig) {
		dnvme_err("SQ shall be contig!\n");
		return -EINVAL;
	}

	sq = dnvme_alloc_sq(ctx, &prep, NVME_NVM_IOSQES);
	if (!sq)
		return -ENOMEM;

	sq->priv.bit_mask |= NVME_QF_WAIT_FOR_CREATE;
	return 0;
}

int dnvme_prepare_cq(struct nvme_context *ctx, struct nvme_prep_cq __user *uprep)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_cq *cq;
	struct nvme_prep_cq prep;
	int ret;

	if (copy_from_user(&prep, uprep, sizeof(prep))) {
		dnvme_err("failed to copy from user space!\n");
		return -EFAULT;
	}

	if (!prep.elements || prep.elements > ndev->q_depth) {
		dnvme_err("CQ elements(%u) is invalid!\n", prep.elements);
		return -EINVAL;
	}

	ret = dnvme_check_qid_unique(ctx, NVME_CQ, prep.cq_id);
	if (ret < 0)
		return ret;

	if (ndev->prop.cap & NVME_CAP_CQR && !prep.contig) {
		dnvme_err("CQ shall be contig!\n");
		return -EINVAL;
	}

	cq = dnvme_alloc_cq(ctx, &prep, NVME_NVM_IOCQES);
	if (!cq)
		return -ENOMEM;

	cq->priv.bit_mask |= NVME_QF_WAIT_FOR_CREATE;

	if (cq->pub.irq_enabled) {
		ret = dnvme_create_icq_node(&ctx->irq_set, cq->pub.q_id, cq->pub.irq_no);
		if (ret < 0) {
			dnvme_err("failed to create icq node!(%d)\n", ret);
			goto out;
		}
	}

	return 0;
out:
	dnvme_release_cq(ctx, cq);
	return ret;
}

int dnvme_create_meta_pool(struct nvme_context *ctx, u32 size)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_meta_set *meta_set = &ctx->meta_set;
	struct pci_dev *pdev = ndev->priv.pdev;

	if (meta_set->pool) {
		if (size == meta_set->buf_size)
			return 0;

		dnvme_err("meta pool already exists!(size: %u vs %u)\n",
			meta_set->buf_size, size);
		return -EINVAL;
	}

	meta_set->pool = dma_pool_create("meta_buf", &pdev->dev, size, 
		NVME_META_BUF_ALIGN, 0);
	if (!meta_set->pool) {
		dnvme_err("failed to create meta pool with size %u!\n", size);
		return -ENOMEM;
	}

	meta_set->buf_size = size;
	return 0;
}

void dnvme_destroy_meta_pool(struct nvme_context *ctx)
{
	struct nvme_meta_set *meta_set = &ctx->meta_set;
	struct nvme_meta *meta;
	struct nvme_meta *tmp;

	if (!meta_set->pool)
		return;

	if (!list_empty(&meta_set->meta_list)) {
		list_for_each_entry_safe(meta, tmp, &meta_set->meta_list, entry) {
			list_del(&meta->entry);
			dma_pool_free(meta_set->pool, meta->buf, meta->dma);
			kfree(meta);
		}
	}

	dma_pool_destroy(ctx->meta_set.pool);
	ctx->meta_set.pool = NULL;
	ctx->meta_set.buf_size = 0;
}

int dnvme_create_meta_node(struct nvme_context *ctx, u32 id)
{
	struct nvme_meta_set *meta_set = &ctx->meta_set;
	struct nvme_meta *meta;
	int ret;

	if (!meta_set->pool) {
		dnvme_err("meta pool doesn't exist!\n");
		dnvme_notice("please create meta pool first!\n");
		return -EPERM;
	}

	meta = dnvme_find_meta(ctx, id);
	if (meta) {
		dnvme_err("meta node(%u) already exist!\n", id);
		return -EINVAL;
	}

	meta = kzalloc(sizeof(*meta), GFP_KERNEL);
	if (!meta) {
		dnvme_err("failed to alloc meta node!\n");
		return -ENOMEM;
	}

	meta->buf = dma_pool_alloc(meta_set->pool, GFP_ATOMIC, &meta->dma);
	if (!meta->buf) {
		dnvme_err("failed to alloc meta buf!\n");
		ret = -ENOMEM;
		goto out;
	}
	meta->id = id;
	list_add_tail(&meta->entry, &meta_set->meta_list);

	return 0;
out:
	kfree(meta);
	return ret;
}

void dnvme_delete_meta_node(struct nvme_context *ctx, u32 id)
{
	struct nvme_meta *meta;
	struct dma_pool *pool = ctx->meta_set.pool;

	meta = dnvme_find_meta(ctx, id);
	if (!meta) {
		dnvme_warn("meta node(%u) doesn't exist! "
			"it may be deleted already\n", id);
		return;
	}

	list_del(&meta->entry);
	dma_pool_free(pool, meta->buf, meta->dma);
	kfree(meta);
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
