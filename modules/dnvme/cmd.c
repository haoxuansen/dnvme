/**
 * @file cmd.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-11-24
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/vmalloc.h>
#include <linux/pci.h>

#include "nvme.h"
#include "dnvme_ioctl.h"
#include "core.h"
#include "cmd.h"
#include "dnvme_ds.h"
#include "dnvme_queue.h"

#define SGES_PER_PAGE	(PAGE_SIZE / sizeof(struct nvme_sgl_desc))
#define PRPS_PER_PAGE	(PAGE_SIZE / NVME_PRP_ENTRY_SIZE)

/**
 * @brief Check whether the cmd uses SGLS.
 * 
 * @return true if cmd uses SGLS, otherwise returns false.
 */
bool dnvme_use_sgls(struct nvme_gen_cmd *gcmd, struct nvme_64b_cmd *cmd)
{
	if (!(gcmd->flags & NVME_CMD_SGL_ALL))
		return false;

	if (!cmd->q_id) /* admin sq won't use sgls */
		return false;

	return true;
}

void dnvme_sgl_set_data(struct nvme_sgl_desc *sge, struct scatterlist *sg)
{
	sge->addr = cpu_to_le64(sg_dma_address(sg));
	sge->length = cpu_to_le32(sg_dma_len(sg));
	sge->type = NVME_SGL_FMT_DATA_DESC << 4;
}

void dnvme_sgl_set_seg(struct nvme_sgl_desc *sge, dma_addr_t dma_addr, 
	int entries)
{
	sge->addr = cpu_to_le64(dma_addr);
	if (entries < SGES_PER_PAGE) {
		sge->length = cpu_to_le32(entries * sizeof(*sge));
		sge->type = NVME_SGL_FMT_LAST_SEG_DESC << 4;
	} else {
		sge->length = cpu_to_le32(PAGE_SIZE);
		sge->type = NVME_SGL_FMT_SEG_DESC << 4;
	}
}

static struct scatterlist *dnvme_pages_to_sgl(struct page **pages, int nr_pages, 
	unsigned int offset, unsigned int len)
{
	struct scatterlist *sgl;
	int i;

	sgl = kmalloc(nr_pages * sizeof(*sgl), GFP_KERNEL);
	if (!sgl) {
		dnvme_err("failed to alloc SGL!\n");
		return NULL;
	}

	sg_init_table(sgl, nr_pages);
	for (i = 0; i < nr_pages; i++) {
		if (!pages[i]) {
			dnvme_err("page%d is NULL!\n", i);
			goto out;
		}
		sg_set_page(&sgl[i], pages[i], 
			min_t(unsigned int, len, PAGE_SIZE - offset), offset);
		len -= (PAGE_SIZE - offset);
		offset = 0;
	}
	sg_mark_end(&sgl[nr_pages - 1]);

	return sgl;
out:
	kfree(sgl);
	return NULL;
}

static int dnvme_map_user_page(struct nvme_device *ndev, struct nvme_64b_cmd *cmd,
	struct nvme_gen_cmd *gcmd, struct nvme_prps *prps)
{
	struct scatterlist *sgl;
	struct page **pages;
	struct pci_dev *pdev = ndev->priv.pdev;
	enum dma_data_direction dir = cmd->data_dir;
	unsigned long addr = (unsigned long)cmd->data_buf_ptr;
	unsigned int size = cmd->data_buf_size;
	unsigned int offset;
	void *buf = NULL;
	int nr_pages;
	int ret, i;

	if (!addr || !IS_ALIGNED(addr, 4) || !size) {
		dnvme_err("data buf ptr:0x%lx or size:%u is invalid!\n", 
			addr, size);
		return -EINVAL;
	}

	offset = offset_in_page(addr);
	nr_pages = DIV_ROUND_UP(offset + size, PAGE_SIZE);
	dnvme_vdbg("User buf ptr 0x%lx(0x%x) size:0x%x => nr_pages:0x%x\n",
		addr, offset, size, nr_pages);

	pages = kcalloc(nr_pages, sizeof(*pages), GFP_KERNEL);
	if (!pages) {
		dnvme_err("failed to alloc pages!\n");
		return -ENOMEM;
	}

	/*
	 * Pinning user pages in memory, always assuming writing in case
	 * user space specifies an incorrect direction of data xfer
	 */
	ret = get_user_pages_fast(addr, nr_pages, FOLL_WRITE, pages);
	if (ret < nr_pages) {
		dnvme_err("failed to pin down user pages!\n");
		nr_pages = ret;
		ret = -EFAULT;
		goto out;
	}

	/*
	 * Kernel needs direct access to all Q memory, so discontiguously backed
	 */
	if (cmd->q_id == NVME_AQ_ID && (gcmd->opcode == nvme_admin_create_sq || 
		gcmd->opcode == nvme_admin_create_cq)) {
		/* Note: Not suitable for pages with offsets, but since discontig back'd
        	 *       Q's are required to be page aligned this isn't an issue */
		buf = vmap(pages, nr_pages, VM_MAP, PAGE_KERNEL);
		if (!buf) {
			dnvme_err("failed to map user space to kernel space!\n");
			ret = -EFAULT;
			goto out;
		}
		dnvme_vdbg("Map user to kernel(0x%lx)\n", (unsigned long)buf);
	}

	/* Generate SG List from pinned down pages */
	sgl = dnvme_pages_to_sgl(pages, nr_pages, offset, size);
	if (!sgl)
		goto out2;

	ret = dma_map_sg(&pdev->dev, sgl, nr_pages, dir);
	if (ret != nr_pages) {
		dnvme_err("%d pages => %d sg\n", nr_pages, ret);
		ret = -ENOMEM;
		goto out3;
	}

	kfree(pages);

	prps->sg = sgl;
	prps->num_map_pgs = nr_pages;
	prps->buf = buf;
	prps->data_dir = dir;
	prps->data_buf_addr = addr;
	prps->data_buf_size = size;

	return 0;
out3:
	kfree(sgl);
out2:
	vunmap(buf);
out:
	for (i = 0; i < nr_pages; i++) {
		put_page(pages[i]);
	}
	kfree(pages);
	return ret;
}

static void dnvme_unmap_user_page(struct nvme_device *ndev, struct nvme_prps *prps)
{
	struct pci_dev *pdev = ndev->priv.pdev;
	struct page *pg;
	int i;

	if (!prps)
		return;

	if (prps->buf) {
		vunmap(prps->buf);
		prps->buf = NULL;
	}

	if (prps->sg) {
		dma_unmap_sg(&pdev->dev, prps->sg, prps->num_map_pgs, 
			prps->data_dir);

		for (i = 0; i < prps->num_map_pgs; i++) {
			pg = sg_page(&prps->sg[i]);
			/*
			 * !NOTE: Why set dirty page here? data received from 
			 * device may not sync to user?
			 */
			if (prps->data_dir == DMA_FROM_DEVICE ||
				prps->data_dir == DMA_BIDIRECTIONAL) {
				set_page_dirty_lock(pg);
			}
			put_page(pg);
		}
		kfree(prps->sg);
		prps->sg = NULL;
	}
}

static int dnvme_setup_sgl(struct nvme_device *ndev, struct nvme_gen_cmd *gcmd, 
	struct nvme_prps *prps)
{
	struct dma_pool *pool = ndev->priv.prp_page_pool;
	struct nvme_sgl_desc *sgl_desc;
	struct scatterlist *sg = prps->sg;
	void **prp_list;
	dma_addr_t *prp_dma;
	/* The number of SGL data block descriptors required */
	unsigned int nr_desc = prps->num_map_pgs; 
	unsigned int nr_seg; /* The number of SGL segments required */
	int ret = -ENOMEM;
	int i, j, k;

	if (nr_desc == 1) {
		dnvme_sgl_set_data(&gcmd->dptr.sgl, prps->sg);
		return 0;
	}

	nr_seg = DIV_ROUND_UP(sizeof(struct nvme_sgl_desc) * nr_desc, 
		PAGE_SIZE - sizeof(struct nvme_sgl_desc));

	prp_list = kzalloc(sizeof(void *) * nr_seg, GFP_ATOMIC);
	if (!prp_list) {
		dnvme_err("failed to alloc for prp list!\n");
		return -ENOMEM;
	}

	prp_dma = kzalloc(sizeof(dma_addr_t) * nr_seg, GFP_ATOMIC);
	if (!prp_dma) {
		dnvme_err("failed to alloc for prp dma!\n");
		goto out;
	}

	for (i = 0; i < nr_seg; i++) {
		prp_list[i] = dma_pool_alloc(pool, GFP_ATOMIC, &prp_dma[i]);
		if (!prp_list[i]) {
			dnvme_err("failed to alloc for sgl page!\n");
			goto out2;
		}
	}

	prps->vir_prp_list = (__le64 **)prp_list;
	prps->npages = nr_seg;
	prps->dma = prp_dma;
	dnvme_sgl_set_seg(&gcmd->dptr.sgl, prp_dma[0], nr_desc);

	for (j = 0, k = 0; nr_desc > 0;) {
		WARN_ON(j >= nr_seg || !sg); /* sanity check */

		sgl_desc = prp_list[j];

		if (k == (SGES_PER_PAGE - 1)) {
			/* SGL segment last descriptor pointer to next SGL segment */
			j++;
			dnvme_sgl_set_seg(&sgl_desc[k], prp_dma[j], nr_desc);
			k = 0;
		} else {
			dnvme_sgl_set_data(&sgl_desc[k], sg);
			sg = sg_next(sg);
			k++;
			nr_desc--;
		}
	}
	return 0;
out2:
	for (i--; i >= 0; i--) {
		dma_pool_free(pool, prp_list[i], prp_dma[i]);
	}
	kfree(prp_dma);
out:
	kfree(prp_list);
	return ret;
}

static void dnvme_free_prp_list(struct nvme_device *ndev, struct nvme_prps *prps)
{
	struct dma_pool *pool = ndev->priv.prp_page_pool;
	int i;

	if (!prps)
		return;

	if (prps->vir_prp_list) {
		for (i = 0; i < prps->npages; i++)
			dma_pool_free(pool, prps->vir_prp_list[i], prps->dma[i]);

		kfree(prps->dma);
		prps->dma = NULL;
		kfree(prps->vir_prp_list);
		prps->vir_prp_list = NULL;
	}
}

static int dnvme_setup_prps(struct nvme_device *ndev, struct nvme_64b_cmd *cmd, 
	struct nvme_gen_cmd *gcmd, struct nvme_prps *prps)
{
	enum nvme_64b_cmd_mask cmd_mask = cmd->bit_mask;
	enum nvme_64b_cmd_mask flag;
	struct scatterlist *sg = prps->sg;
	void **prp_list;
	dma_addr_t *prp_dma;
	struct dma_pool *pool = ndev->priv.prp_page_pool;
	__le64 *prp_entry;
	u32 nr_entry = prps->num_map_pgs;
	u32 nr_pages;
	int ret = -ENOMEM;
	int i, j, k;

	if (cmd->q_id == NVME_AQ_ID && (gcmd->opcode == nvme_admin_create_sq ||
		gcmd->opcode == nvme_admin_create_cq)) {

		if (!(cmd_mask & NVME_MASK_PRP1_LIST)) {
			dnvme_err("cmd mask doesn't support PRP1 list!\n");
			return -EINVAL;
		}
		flag = NVME_MASK_PRP1_LIST;
		goto prp_list;
	}

	if (nr_entry == 1) {
		if (!(cmd_mask & NVME_MASK_PRP1_PAGE)) {
			dnvme_err("cmd mask doesn't support PRP1 page!\n");
			return -EINVAL;
		}

		gcmd->dptr.prp1 = cpu_to_le64(sg_dma_address(sg));
		gcmd->dptr.prp2 = 0;
		return 0;
	}

	if (nr_entry == 2) {
		if (!(cmd_mask & NVME_MASK_PRP1_PAGE) || 
			!(cmd_mask & NVME_MASK_PRP2_PAGE)) {
			dnvme_err("cmd mask doesn't support PRP1 or PRP2 page!\n");
			return -EINVAL;
		}

		gcmd->dptr.prp1 = cpu_to_le64(sg_dma_address(sg));
		sg = sg_next(sg);
		gcmd->dptr.prp2 = cpu_to_le64(sg_dma_address(sg));
		return 0;
	}

	if (!(cmd_mask & NVME_MASK_PRP2_LIST)) {
		dnvme_err("cmd mask doesn't support PRP2 list!\n");
		return -EINVAL;
	}
	flag = NVME_MASK_PRP2_LIST;

prp_list:
	nr_pages = DIV_ROUND_UP(NVME_PRP_ENTRY_SIZE * nr_entry, 
		PAGE_SIZE - NVME_PRP_ENTRY_SIZE);
	
	prp_list = kzalloc(sizeof(void *) * nr_pages, GFP_ATOMIC);
	if (!prp_list) {
		dnvme_err("failed to alloc for prp list!\n");
		return -ENOMEM;
	}

	prp_dma = kzalloc(sizeof(dma_addr_t) * nr_pages, GFP_ATOMIC);
	if (!prp_dma) {
		dnvme_err("failed to alloc for prp dma!\n");
		goto out;
	}

	for (i = 0; i < nr_pages; i++) {
		prp_list[i] = dma_pool_alloc(pool, GFP_ATOMIC, &prp_dma[i]);
		if (!prp_list[i]) {
			dnvme_err("failed to alloc for prp page!\n");
			goto out2;
		}
	}

	prps->vir_prp_list = (__le64 **)prp_list;
	prps->npages = nr_pages;
	prps->dma = prp_dma;

	if (flag == NVME_MASK_PRP1_LIST) {
		gcmd->dptr.prp1 = cpu_to_le64(prp_dma[0]);
		gcmd->dptr.prp2 = 0;
	} else if (flag == NVME_MASK_PRP2_LIST) {
		gcmd->dptr.prp1 = cpu_to_le64(sg_dma_address(sg));
		gcmd->dptr.prp2 = cpu_to_le64(prp_dma[0]);
	}

	for (j = 0, k = 0; nr_entry > 0;) {
		WARN_ON(j >= nr_pages || !sg); /* sanity check */

		prp_entry = prp_list[j];

		if (k == (PRPS_PER_PAGE - 1)) {
			/* PRP list last entry pointer to next PRP list */
			j++;
			prp_entry[k] = cpu_to_le64(prp_dma[j]);
			k = 0;
		} else {
			prp_entry[k] = cpu_to_le64(sg_dma_address(sg));
			sg = sg_next(sg);
			k++;
			nr_entry--;
		}
	}

	return 0;
out2:
	for (i--; i >= 0; i--) {
		dma_pool_free(pool, prp_list[i], prp_dma[i]);
	}
	kfree(prp_dma);
out:
	kfree(prp_list);
	return ret;
}

static int dnvme_add_cmd_node(struct nvme_device *ndev, struct nvme_64b_cmd *cmd, 
	struct nvme_gen_cmd *gcmd, struct nvme_prps *prps)
{
	struct nvme_context *ctx = ndev->ctx;
	struct nvme_sq *sq;
	struct nvme_cmd_node *node;

	sq = dnvme_find_sq(ctx, cmd->q_id);
	if (!sq) {
		dnvme_err("SQ(%u) doesn't exist!\n", cmd->q_id);
		return -EBADSLT;
	}

	node = kzalloc(sizeof(*node), GFP_ATOMIC);
	if (!node) {
		dnvme_err("failed to alloc cmd node!\n");
		return -ENOMEM;
	}

	node->unique_id = gcmd->command_id;
	node->opcode = gcmd->opcode;

	if (cmd->q_id == NVME_AQ_ID) {
		struct nvme_create_sq *csq;
		struct nvme_create_cq *ccq;
		struct nvme_delete_queue *dq;

		switch (gcmd->opcode) {
		case nvme_admin_create_sq:
			csq = (struct nvme_create_sq *)gcmd;
			node->persist_q_id = csq->sqid;
			break;

		case nvme_admin_create_cq:
			ccq = (struct nvme_create_cq *)gcmd;
			node->persist_q_id = ccq->cqid;
			break;

		case nvme_admin_delete_sq:
		case nvme_admin_delete_cq:
			dq = (struct nvme_delete_queue *)gcmd;
			node->persist_q_id = dq->qid;
			break;

		default:
			node->persist_q_id = 0;
		}
	} else {
		node->persist_q_id = 0;
	}

	/*
	 *   If cmd is create or delete SQ/CQ, nvme_prps will be copied to
	 * nvme_sq or nvme_cq. Otherwise, nvme_prps is belong to cmd node.
	 */
	if (!node->persist_q_id)
		memcpy(&node->prp_nonpersist, prps, sizeof(struct nvme_prps));

	list_add_tail(&node->entry, &sq->priv.cmd_list);
	return 0;
}

static int dnvme_data_buf_to_sgl(struct nvme_device *ndev, struct nvme_64b_cmd *cmd,
	struct nvme_gen_cmd *gcmd, struct nvme_prps *prps)
{
	int ret;

	ret = dnvme_map_user_page(ndev, cmd, gcmd, prps);
	if (ret < 0)
		return ret;

	ret = dnvme_setup_sgl(ndev, gcmd, prps);
	if (ret < 0)
		goto out;

	ret = dnvme_add_cmd_node(ndev, cmd, gcmd, prps);
	if (ret < 0)
		goto out2;

	return 0;
out2:
	dnvme_free_prp_list(ndev, prps);
out:
	dnvme_unmap_user_page(ndev, prps);
	return ret;
}

static int dnvme_data_buf_to_prp(struct nvme_device *ndev, struct nvme_64b_cmd *cmd,
	struct nvme_gen_cmd *gcmd, struct nvme_prps *prps)
{
	int ret;

	ret = dnvme_map_user_page(ndev, cmd, gcmd, prps);
	if (ret < 0)
		return ret;
	
	ret = dnvme_setup_prps(ndev, cmd, gcmd, prps);
	if (ret < 0)
		goto out;

	ret = dnvme_add_cmd_node(ndev, cmd, gcmd, prps);
	if (ret < 0)
		goto out2;

	return 0;
out2:
	dnvme_free_prp_list(ndev, prps);
out:
	dnvme_unmap_user_page(ndev, prps);
	return ret;
}

int dnvme_prepare_64b_cmd(struct nvme_device *ndev, struct nvme_64b_cmd *cmd, 
	struct nvme_gen_cmd *gcmd, struct nvme_prps *prps)
{
	bool need_prp = false;
	int ret;

	if (cmd->q_id == NVME_AQ_ID) {
		switch (gcmd->opcode) {
		case nvme_admin_create_sq:
		case nvme_admin_create_cq:
			if (cmd->data_buf_ptr) /* discontig */
				need_prp = true;
			break;

		case nvme_admin_delete_sq:
		case nvme_admin_delete_cq:
			break;

		default:
			need_prp = true;
		}
	} else {
		need_prp = true;
	}

	if (!need_prp)
		return dnvme_add_cmd_node(ndev, cmd, gcmd, prps);

	if (dnvme_use_sgls(gcmd, cmd)) {
		ret = dnvme_data_buf_to_sgl(ndev, cmd, gcmd, prps);
		if (ret < 0) {
			dnvme_err("data buffer to SGL err!(%d)\n", ret);
			return ret;
		}
	} else {
		ret = dnvme_data_buf_to_prp(ndev, cmd, gcmd, prps);
		if (ret < 0) {
			dnvme_err("data buffer to PRP err!(%d)\n", ret);
			return ret;
		}
	}

	return 0;
}

void dnvme_release_prps(struct nvme_device *ndev, struct nvme_prps *prps)
{
	dnvme_free_prp_list(ndev, prps);
	dnvme_unmap_user_page(ndev, prps);
}
