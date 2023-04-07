/**
 * @file cmd.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-04-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/scatterlist.h>
#include <linux/vmalloc.h>

#include "trace.h"
#include "core.h"
#include "queue.h"
#include "debug.h"

/**
 * @brief Check whether the cmd uses SGLS.
 * 
 * @return true if cmd uses SGLS, otherwise returns false.
 */
static bool dnvme_use_sgls(struct nvme_common_command *ccmd, struct nvme_64b_cmd *cmd)
{
	if (!(ccmd->flags & NVME_CMD_SGL_ALL))
		return false;

	if (!cmd->sqid) /* admin sq won't use sgls */
		return false;

	return true;
}

void dnvme_sgl_set_data(struct nvme_sgl_desc *sge, struct scatterlist *sg)
{
	sge->addr = cpu_to_le64(sg_dma_address(sg));
	sge->length = cpu_to_le32(sg_dma_len(sg));
	sge->type = NVME_SGL_TYPE_DATA_DESC << 4;
	trace_dnvme_sgl_set_data(sge);
}

void dnvme_sgl_set_seg(struct nvme_sgl_desc *sge, dma_addr_t dma_addr, 
	int entries)
{
	sge->addr = cpu_to_le64(dma_addr);
	if (entries < NVME_SGES_PER_PAGE) {
		sge->length = cpu_to_le32(entries * sizeof(*sge));
		sge->type = NVME_SGL_TYPE_LAST_SEG_DESC << 4;
	} else {
		sge->length = cpu_to_le32(PAGE_SIZE);
		sge->type = NVME_SGL_TYPE_SEG_DESC << 4;
	}
	trace_dnvme_sgl_set_seg(sge);
}

static struct scatterlist *dnvme_pages_to_sgl(struct nvme_device *ndev, 
	struct page **pages, int nr_pages, 
	unsigned int offset, unsigned int len)
{
	struct scatterlist *sgl;
	int i;

	sgl = kmalloc(nr_pages * sizeof(*sgl), GFP_KERNEL);
	if (!sgl) {
		dnvme_err(ndev, "failed to alloc SGL!\n");
		return NULL;
	}

	sg_init_table(sgl, nr_pages);
	for (i = 0; i < nr_pages; i++) {
		if (!pages[i]) {
			dnvme_err(ndev, "page%d is NULL!\n", i);
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

int dnvme_map_user_page(struct nvme_device *ndev, struct nvme_prps *prps, 
	void __user *data, unsigned int size, 
	enum dma_data_direction dir, bool access)
{
	struct scatterlist *sgl;
	struct page **pages;
	struct pci_dev *pdev = ndev->pdev;
	unsigned long addr = (unsigned long)data;
	unsigned int offset;
	void *buf = NULL;
	int nr_pages;
	int ret, i;

	if (!addr || !IS_ALIGNED(addr, 4) || !size) {
		dnvme_err(ndev, "data buf ptr:0x%lx or size:%u is invalid!\n", 
			addr, size);
		return -EINVAL;
	}

	/* !TODO: It's better to use physical address to calculate the offset.
	 *
	 * Because the virtual address passed from the user space is currently
	 * used, the actual mapping range will be larger than the actual data
	 * length.
	 */
	offset = offset_in_page(addr);
	nr_pages = DIV_ROUND_UP(offset + size, PAGE_SIZE);
	dnvme_vdbg(ndev, "User buf ptr 0x%lx(0x%x) size:0x%x => nr_pages:0x%x\n",
		addr, offset, size, nr_pages);

	pages = kcalloc(nr_pages, sizeof(*pages), GFP_KERNEL);
	if (!pages) {
		dnvme_err(ndev, "failed to alloc pages!\n");
		return -ENOMEM;
	}

	/*
	 * Pinning user pages in memory, always assuming writing in case
	 * user space specifies an incorrect direction of data xfer
	 */
	ret = get_user_pages_fast(addr, nr_pages, FOLL_WRITE, pages);
	if (ret < nr_pages) {
		dnvme_err(ndev, "failed to pin down user pages!\n");
		nr_pages = ret;
		ret = -EFAULT;
		goto out;
	}

	/*
	 * Kernel needs direct access to all Q memory, so discontiguously backed
	 */
	if (access) {
		/* Note: Not suitable for pages with offsets, but since discontig back'd
        	 *       Q's are required to be page aligned this isn't an issue */
		buf = vmap(pages, nr_pages, VM_MAP, PAGE_KERNEL);
		if (!buf) {
			dnvme_err(ndev, "failed to map user space to kernel space!\n");
			ret = -EFAULT;
			goto out;
		}
		dnvme_vdbg(ndev, "Map user to kernel(0x%lx)\n", (unsigned long)buf);
	}

	/* Generate SG List from pinned down pages */
	sgl = dnvme_pages_to_sgl(ndev, pages, nr_pages, offset, size);
	if (!sgl)
		goto out2;

	ret = dma_map_sg(&pdev->dev, sgl, nr_pages, dir);
	if (ret != nr_pages) {
		dnvme_err(ndev, "%d pages => %d sg\n", nr_pages, ret);
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

static int dnvme_cmd_map_user_page(struct nvme_device *ndev, 
		struct nvme_64b_cmd *cmd,
		struct nvme_common_command *ccmd, 
		struct nvme_prps *prps)
{
	bool access = false;

	if (cmd->sqid == NVME_AQ_ID && (ccmd->opcode == nvme_admin_create_sq || 
		ccmd->opcode == nvme_admin_create_cq)) {
		access = true;
	}

	return dnvme_map_user_page(ndev, prps, cmd->data_buf_ptr, 
				cmd->data_buf_size, cmd->data_dir, access);
}

void dnvme_unmap_user_page(struct nvme_device *ndev, struct nvme_prps *prps)
{
	struct pci_dev *pdev = ndev->pdev;
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

static int dnvme_cmd_setup_sgl(struct nvme_device *ndev, 
		struct nvme_common_command *ccmd, 
		struct nvme_prps *prps)
{
	struct dma_pool *pool = prps->pg_pool;
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
		dnvme_sgl_set_data(&ccmd->dptr.sgl, prps->sg);
		return 0;
	}

	nr_seg = DIV_ROUND_UP(sizeof(struct nvme_sgl_desc) * nr_desc, 
		PAGE_SIZE - sizeof(struct nvme_sgl_desc));

	prp_list = kzalloc(sizeof(void *) * nr_seg, GFP_KERNEL);
	if (!prp_list) {
		dnvme_err(ndev, "failed to alloc for prp list!\n");
		return -ENOMEM;
	}

	prp_dma = kzalloc(sizeof(dma_addr_t) * nr_seg, GFP_KERNEL);
	if (!prp_dma) {
		dnvme_err(ndev, "failed to alloc for prp dma!\n");
		goto out;
	}

	for (i = 0; i < nr_seg; i++) {
		prp_list[i] = dma_pool_alloc(pool, GFP_KERNEL | __GFP_ZERO, &prp_dma[i]);
		if (!prp_list[i]) {
			dnvme_err(ndev, "failed to alloc for sgl page!\n");
			goto out2;
		}
	}

	prps->prp_list = prp_list;
	prps->nr_pages = nr_seg;
	prps->pg_addr = prp_dma;
	dnvme_sgl_set_seg(&ccmd->dptr.sgl, prp_dma[0], nr_desc);

	for (j = 0, k = 0; nr_desc > 0;) {
		WARN_ON(j >= nr_seg || !sg); /* sanity check */

		sgl_desc = prp_list[j];

		if (k == (NVME_SGES_PER_PAGE - 1)) {
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
	struct dma_pool *pool = prps->pg_pool;
	int i;

	if (!prps)
		return;

	if (prps->prp_list) {
		for (i = 0; i < prps->nr_pages; i++)
			dma_pool_free(pool, prps->prp_list[i], prps->pg_addr[i]);

		kfree(prps->pg_addr);
		prps->pg_addr = NULL;
		kfree(prps->prp_list);
		prps->prp_list = NULL;
	}
}

static int dnvme_setup_prps(struct nvme_device *ndev, struct nvme_64b_cmd *cmd, 
	struct nvme_common_command *ccmd, struct nvme_prps *prps)
{
	enum nvme_64b_cmd_mask prp_mask = cmd->bit_mask;
	enum nvme_64b_cmd_mask flag;
	struct scatterlist *sg = prps->sg;
	void **prp_list;
	dma_addr_t *prp_dma;
	struct dma_pool *pool = prps->pg_pool;
	__le64 *prp_entry;
	int buf_len = prps->data_buf_size;
	u32 nr_pages, nr_entry, pg_oft;
	dma_addr_t dma_addr;
	int dma_len;
	int ret = -ENOMEM;
	int i, j, k;

	dma_addr = sg_dma_address(sg);
	dma_len = sg_dma_len(sg);
	pg_oft = offset_in_page(dma_addr);

	if (cmd->sqid == NVME_AQ_ID && (ccmd->opcode == nvme_admin_create_sq ||
		ccmd->opcode == nvme_admin_create_cq)) {

		if (!(prp_mask & NVME_MASK_PRP1_LIST)) {
			dnvme_err(ndev, "cmd mask doesn't support PRP1 list!\n");
			return -EINVAL;
		}
		flag = NVME_MASK_PRP1_LIST;
		goto prp_list;
	}

	if (!(prp_mask & NVME_MASK_PRP1_PAGE)) {
		dnvme_err(ndev, "cmd mask doesn't support PRP1 page!\n");
		return -EINVAL;
	}
	ccmd->dptr.prp1 = cpu_to_le64(dma_addr);
	buf_len -= (PAGE_SIZE - pg_oft);
	dma_len -= (PAGE_SIZE - pg_oft);

	if (buf_len <= 0)
		return 0;

	/* If pages were contiguous in memory use same SG Entry */
	if (dma_len) {
		dma_addr += (PAGE_SIZE - pg_oft);
	} else {
		sg = sg_next(sg);
		dma_addr = sg_dma_address(sg);
		dma_len = sg_dma_len(sg);
	}
	pg_oft = 0;

	if (buf_len <= PAGE_SIZE) {
		if (!(prp_mask & NVME_MASK_PRP2_PAGE)) {
			dnvme_err(ndev, "cmd mask doesn't support PRP2 page!\n");
			return -EINVAL;
		}

		ccmd->dptr.prp2 = cpu_to_le64(dma_addr);
		return 0;
	}

	if (!(prp_mask & NVME_MASK_PRP2_LIST)) {
		dnvme_err(ndev, "cmd mask doesn't support PRP2 list!\n");
		return -EINVAL;
	}
	flag = NVME_MASK_PRP2_LIST;

prp_list:
	nr_entry = DIV_ROUND_UP(pg_oft + buf_len, PAGE_SIZE);
	nr_pages = DIV_ROUND_UP(NVME_PRP_ENTRY_SIZE * nr_entry, 
		PAGE_SIZE - NVME_PRP_ENTRY_SIZE);
	
	prp_list = kzalloc(sizeof(void *) * nr_pages, GFP_KERNEL);
	if (!prp_list) {
		dnvme_err(ndev, "failed to alloc for prp list!\n");
		return -ENOMEM;
	}

	prp_dma = kzalloc(sizeof(dma_addr_t) * nr_pages, GFP_KERNEL);
	if (!prp_dma) {
		dnvme_err(ndev, "failed to alloc for prp dma!\n");
		goto out;
	}

	for (i = 0; i < nr_pages; i++) {
		prp_list[i] = dma_pool_alloc(pool, GFP_KERNEL | __GFP_ZERO, &prp_dma[i]);
		if (!prp_list[i]) {
			dnvme_err(ndev, "failed to alloc for prp page!\n");
			goto out2;
		}
	}

	prps->prp_list = prp_list;
	prps->nr_pages = nr_pages;
	prps->pg_addr = prp_dma;

	if (flag == NVME_MASK_PRP1_LIST) {
		ccmd->dptr.prp1 = cpu_to_le64(prp_dma[0]);
		ccmd->dptr.prp2 = 0;
	} else if (flag == NVME_MASK_PRP2_LIST) {
		/* prp1 has configured before */
		ccmd->dptr.prp2 = cpu_to_le64(prp_dma[0]);
	}

	for (j = 0, k = 0; buf_len > 0;) {
		WARN_ON(j >= nr_pages || !sg); /* sanity check */

		prp_entry = prp_list[j];

		if (k == (NVME_PRPS_PER_PAGE - 1)) {
			/* PRP list last entry pointer to next PRP list */
			j++;
			prp_entry[k] = cpu_to_le64(prp_dma[j]);
			k = 0;
		} else {
			prp_entry[k] = cpu_to_le64(dma_addr);
			buf_len -= (PAGE_SIZE - pg_oft);
			dma_len -= (PAGE_SIZE - pg_oft);
			pg_oft = 0;

			if (buf_len <= 0)
				break;

			/* If pages were contiguous in memory use same SG Entry */
			if (dma_len > 0) {
				dma_addr += (PAGE_SIZE - pg_oft);
			} else if (dma_len < 0) {
				dnvme_err(ndev, "DMA data length is illegal!\n");
				ret = -EFAULT;
				goto out2;
			} else {
				sg = sg_next(sg);
				dma_addr = sg_dma_address(sg);
				dma_len = sg_dma_len(sg);
			}

			k++;
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
	struct nvme_common_command *ccmd, struct nvme_prps *prps)
{
	struct nvme_sq *sq;
	struct nvme_sq *wait_sq = NULL;
	struct nvme_cq *wait_cq = NULL;
	struct nvme_cmd *node;

	sq = dnvme_find_sq(ndev, cmd->sqid);
	if (!sq) {
		dnvme_err(ndev, "SQ(%u) doesn't exist!\n", cmd->sqid);
		return -EBADSLT;
	}

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		dnvme_err(ndev, "failed to alloc cmd node!\n");
		return -ENOMEM;
	}

	node->id = ccmd->command_id;
	node->opcode = ccmd->opcode;
	node->sqid = cmd->sqid;

	if (cmd->sqid == NVME_AQ_ID) {
		struct nvme_create_sq *csq;
		struct nvme_create_cq *ccq;
		struct nvme_delete_queue *dq;

		switch (ccmd->opcode) {
		case nvme_admin_create_sq:
			csq = (struct nvme_create_sq *)ccmd;
			node->target_qid = le16_to_cpu(csq->sqid);
			wait_sq = dnvme_find_sq(ndev, node->target_qid);
			BUG_ON(!wait_sq);
			break;

		case nvme_admin_create_cq:
			ccq = (struct nvme_create_cq *)ccmd;
			node->target_qid = le16_to_cpu(ccq->cqid);
			wait_cq = dnvme_find_cq(ndev, node->target_qid);
			BUG_ON(!wait_cq);
			break;

		case nvme_admin_delete_sq:
		case nvme_admin_delete_cq:
			dq = (struct nvme_delete_queue *)ccmd;
			node->target_qid = dq->qid;
			break;

		default:
			node->target_qid = 0;
		}
	} else {
		node->target_qid = 0;
	}

	if (!prps)
		goto out_add_node;

	if (node->target_qid) {
		if (wait_sq)
			wait_sq->prps = prps;
		if (wait_cq)
			wait_cq->prps = prps;
	} else {
		node->prps = prps;
	}

out_add_node:
	list_add_tail(&node->entry, &sq->cmd_list);
	return 0;
}

static int dnvme_prepare_64b_cmd(struct nvme_device *ndev, 
	struct nvme_64b_cmd *cmd, struct nvme_common_command *ccmd)
{
	struct nvme_prps *prps;
	struct dma_pool *pool = NULL;
	bool need_prp = false;
	int ret;

	if (cmd->sqid == NVME_AQ_ID) {
		switch (ccmd->opcode) {
		case nvme_admin_create_sq:
		case nvme_admin_create_cq:
			if (cmd->data_buf_ptr) { /* discontig */
				need_prp = true;
				pool = ndev->queue_pool;
			}
			break;

		case nvme_admin_delete_sq:
		case nvme_admin_delete_cq:
			break;

		default:
			if (cmd->data_buf_ptr)
				need_prp = true;
		}
	} else {
		if (cmd->data_buf_ptr)
			need_prp = true;
	}

	if (!need_prp)
		return dnvme_add_cmd_node(ndev, cmd, ccmd, NULL);

	prps = kzalloc(sizeof(*prps), GFP_KERNEL);
	if (!prps) {
		dnvme_err(ndev, "failed to alloc PRPs!\n");
		return -ENOMEM;
	}
	prps->pg_pool = pool ? pool : ndev->cmd_pool;

	ret = dnvme_cmd_map_user_page(ndev, cmd, ccmd, prps);
	if (ret < 0)
		goto out_free_prp;

	if (dnvme_use_sgls(ccmd, cmd))
		ret = dnvme_cmd_setup_sgl(ndev, ccmd, prps);
	else
		ret = dnvme_setup_prps(ndev, cmd, ccmd, prps);

	if (ret < 0)
		goto out_unmap_page;

	ret = dnvme_add_cmd_node(ndev, cmd, ccmd, prps);
	if (ret < 0)
		goto out_free_prp_list;

	return 0;

out_free_prp_list:
	dnvme_free_prp_list(ndev, prps);
out_unmap_page:
	dnvme_unmap_user_page(ndev, prps);
out_free_prp:
	kfree(prps);
	return ret;	
}

void dnvme_release_prps(struct nvme_device *ndev, struct nvme_prps *prps)
{
	dnvme_free_prp_list(ndev, prps);
	dnvme_unmap_user_page(ndev, prps);
	kfree(prps);
}

/**
 * @brief Delete command list in the SQ
 * 
 * @param ndev NVMe device
 * @param sq the specified submission queue
 */
void dnvme_delete_cmd_list(struct nvme_device *ndev, struct nvme_sq *sq)
{
	struct nvme_cmd *cmd;
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &sq->cmd_list) {
		cmd = list_entry(pos, struct nvme_cmd, entry);
		list_del(pos);

		dnvme_release_prps(ndev, cmd->prps);
		cmd->prps = NULL;
		kfree(cmd);
	}
}

static int dnvme_create_iosq(struct nvme_context *ctx, struct nvme_64b_cmd *cmd,
	struct nvme_common_command *ccmd)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_create_sq *csq = (struct nvme_create_sq *)ccmd;
	struct nvme_sq *wait_sq;
	int ret;

	wait_sq = dnvme_find_sq(ndev, csq->sqid);
	if (!wait_sq) {
		dnvme_err(ndev, "SQ(%u) doesn't exist!\n", csq->sqid);
		return -EINVAL;
	}

	if ((csq->sq_flags & NVME_QUEUE_PHYS_CONTIG && 
			!test_bit(NVME_QF_BUF_CONTIG, &wait_sq->flags)) ||
		(!(csq->sq_flags & NVME_QUEUE_PHYS_CONTIG) && 
			test_bit(NVME_QF_BUF_CONTIG, &wait_sq->flags))) {
		dnvme_err(ndev, "Sanity Check: sq_flags & contig mismatch!\n");
		return -EINVAL;
	}

	if ((!test_bit(NVME_QF_BUF_CONTIG, &wait_sq->flags) && 
			cmd->data_buf_ptr == NULL) ||
		(test_bit(NVME_QF_BUF_CONTIG, &wait_sq->flags) && 
			wait_sq->buf == NULL)) {
		dnvme_err(ndev, "Sanity Check: contig, data_buf_ptr, buf mismatch!\n");
		return -EINVAL;
	}

	if (!test_bit(NVME_QF_WAIT_FOR_CREATE, &wait_sq->flags)) {
		dnvme_err(ndev, "SQ(%u) already created!\n", csq->sqid);
		return -EPERM;
	}

	ret = dnvme_prepare_64b_cmd(ndev, cmd, ccmd);
	if (ret < 0) {
		dnvme_err(ndev, "failed to prepare 64-byte cmd!\n");
		return ret;
	}

	if (test_bit(NVME_QF_BUF_CONTIG, &wait_sq->flags)) {
		ccmd->dptr.prp1 = cpu_to_le64(wait_sq->dma);
		ccmd->dptr.prp2 = 0;
	}

	clear_bit(NVME_QF_WAIT_FOR_CREATE, &wait_sq->flags);
	return 0;
}

static int dnvme_delete_iosq(struct nvme_context *ctx, struct nvme_64b_cmd *cmd,
	struct nvme_common_command *ccmd)
{
	struct nvme_device *ndev = ctx->dev;
	int ret;

	ret = dnvme_prepare_64b_cmd(ndev, cmd, ccmd);
	if (ret < 0) {
		dnvme_err(ndev, "failed to prepare 64-byte cmd!\n");
		return ret;
	}

	return 0;
}

static int dnvme_create_iocq(struct nvme_context *ctx, struct nvme_64b_cmd *cmd,
	struct nvme_common_command *ccmd)
{
	struct nvme_cq *wait_cq;
	struct nvme_device *ndev = ctx->dev;
	struct nvme_create_cq *ccq = (struct nvme_create_cq *)ccmd;
	int ret;

	wait_cq = dnvme_find_cq(ndev, ccq->cqid);
	if (!wait_cq) {
		dnvme_err(ndev, "The waiting CQ(%u) doesn't exist!\n", ccq->cqid);
		return -EBADSLT;
	}

	if ((ccq->cq_flags & NVME_QUEUE_PHYS_CONTIG && 
			!test_bit(NVME_QF_BUF_CONTIG, &wait_cq->flags)) ||
		(!(ccq->cq_flags & NVME_QUEUE_PHYS_CONTIG) && 
			test_bit(NVME_QF_BUF_CONTIG, &wait_cq->flags))) {
		dnvme_err(ndev, "Sanity Check: sq_flags & contig mismatch!\n");
		return -EINVAL;
	}

	if ((!test_bit(NVME_QF_BUF_CONTIG, &wait_cq->flags) && 
			cmd->data_buf_ptr == NULL) ||
		(test_bit(NVME_QF_BUF_CONTIG, &wait_cq->flags) && 
			wait_cq->buf == NULL)) {
		dnvme_err(ndev, "Sanity Check: contig, data_buf_ptr, buf mismatch!\n");
		return -EINVAL;
	}

	if (!test_bit(NVME_QF_WAIT_FOR_CREATE, &wait_cq->flags)) {
		dnvme_err(ndev, "CQ(%u) already created!\n", ccq->cqid);
		return -EINVAL;
	}

	/* Check if interrupts should be enabled for IO CQ */
	if (ccq->cq_flags & NVME_CQ_IRQ_ENABLED) {
		if (ctx->dev->pub.irq_active.irq_type == NVME_INT_NONE) {
			dnvme_err(ndev, "act irq_type is none!\n");
			return -EINVAL;
		}
	}

	ret = dnvme_prepare_64b_cmd(ndev, cmd, ccmd);
	if (ret < 0) {
		dnvme_err(ndev, "failed to prepare 64-byte cmd!\n");
		return ret;
	}

	if (test_bit(NVME_QF_BUF_CONTIG, &wait_cq->flags)) {
		ccmd->dptr.prp1 = cpu_to_le64(wait_cq->dma);
		ccmd->dptr.prp2 = 0;
	}

	clear_bit(NVME_QF_WAIT_FOR_CREATE, &wait_cq->flags);
	return 0;
}

static int dnvme_delete_iocq(struct nvme_context *ctx, struct nvme_64b_cmd *cmd,
	struct nvme_common_command *ccmd)
{
	struct nvme_device *ndev = ctx->dev;
	int ret;

	ret = dnvme_prepare_64b_cmd(ndev, cmd, ccmd);
	if (ret < 0) {
		dnvme_err(ndev, "failed to prepare 64-byte cmd!\n");
		return ret;
	}
	return 0;
}

static int dnvme_deal_ccmd(struct nvme_context *ctx, struct nvme_64b_cmd *cmd,
	struct nvme_common_command *ccmd)
{
	struct nvme_device *ndev = ctx->dev;
	int ret;

	ret = dnvme_prepare_64b_cmd(ndev, cmd, ccmd);
	if (ret < 0) {
		dnvme_err(ndev, "failed to prepare 64-byte cmd!\n");
		return ret;
	}

	return 0;
}

static int dnvme_fill_mptr(struct nvme_device *ndev, 
			struct nvme_common_command *ccmd, u32 meta_id)
{
	struct nvme_meta *meta;

	meta = dnvme_find_meta(ndev, meta_id);
	if (!meta) {
		dnvme_err(ndev, "Meta(%u) doesn't exist!\n", meta_id);
		return -EINVAL;
	}

	if (test_bit(NVME_META_F_BUF_CONTIG, &meta->flags)) {
		ccmd->metadata = cpu_to_le64(meta->dma);
	} else {
		ccmd->metadata = cpu_to_le64(meta->prps->pg_addr[0]);
	}

	return 0;
}

int dnvme_submit_64b_cmd(struct nvme_context *ctx, struct nvme_64b_cmd __user *ucmd)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_sq *sq;
	struct nvme_64b_cmd cmd;
	struct nvme_common_command *ccmd;
	struct pci_dev *pdev = ndev->pdev;
	void *cmd_buf;
	int ret = 0;

	if (copy_from_user(&cmd, ucmd, sizeof(cmd))) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		return -EFAULT;
	}

	if (!cmd.cmd_buf_ptr) {
		dnvme_err(ndev, "cmd buf ptr is NULL!\n");
		return -EFAULT;
	}

	if ((cmd.data_buf_size && !cmd.data_buf_ptr) || 
		(!cmd.data_buf_size && cmd.data_buf_ptr)) {
		dnvme_err(ndev, "data buf size and ptr are inconsistent!\n");
		return -EINVAL;
	}

	/* Get the SQ for sending this command */
	sq = dnvme_find_sq(ndev, cmd.sqid);
	if (!sq) {
		dnvme_err(ndev, "SQ(%u) doesn't exist!\n", cmd.sqid);
		return -EBADSLT;
	}

	if (dnvme_sq_is_full(sq)) {
		dnvme_err(ndev, "SQ(%u) is full!\n", cmd.sqid);
		return -EBUSY;
	}

	cmd_buf = kzalloc(1 << sq->pub.sqes, GFP_KERNEL);
	if (!cmd_buf) {
		dnvme_err(ndev, "failed to alloc cmd buf!\n");
		return -ENOMEM;
	}

	if (copy_from_user(cmd_buf, cmd.cmd_buf_ptr, 1 << sq->pub.sqes)) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		ret = -EFAULT;
		goto out;
	}

	ccmd = (struct nvme_common_command *)cmd_buf;

	cmd.cid = sq->next_cid++;
	ccmd->command_id = cmd.cid;

	if (copy_to_user(ucmd, &cmd, sizeof(cmd))) {
		dnvme_err(ndev, "failed to copy to user space!\n");
		ret = -EFAULT;
		goto out;
	}

	if (cmd.bit_mask & NVME_MASK_MPTR) {
		ret = dnvme_fill_mptr(ndev, ccmd, cmd.meta_id);
		if (ret < 0)
			goto out;
	}

	if (cmd.sqid == NVME_AQ_ID) {
		switch (ccmd->opcode) {
		case nvme_admin_delete_sq:
			ret = dnvme_delete_iosq(ctx, &cmd, ccmd);
			if (ret < 0)
				goto out2;
			break;

		case nvme_admin_create_sq:
			ret = dnvme_create_iosq(ctx, &cmd, ccmd);
			if (ret < 0)
				goto out2;
			break;

		case nvme_admin_delete_cq:
			ret = dnvme_delete_iocq(ctx, &cmd, ccmd);
			if (ret < 0)
				goto out2;
			break;

		case nvme_admin_create_cq:
			ret = dnvme_create_iocq(ctx, &cmd, ccmd);
			if (ret < 0)
				goto out2;
			break;

		default:
			ret = dnvme_deal_ccmd(ctx, &cmd, ccmd);
			if (ret < 0)
				goto out2;
			break;
		}
	} else {
		ret = dnvme_deal_ccmd(ctx, &cmd, ccmd);
		if (ret < 0)
			goto out2;
	}
	trace_dnvme_submit_64b_cmd(&ndev->dev, ccmd, cmd.sqid);

	/* Copying the command in to appropriate SQ and handling sync issues */
	if (test_bit(NVME_QF_BUF_CONTIG, &sq->flags)) {
		memcpy((sq->buf + ((u32)sq->pub.tail_ptr_virt << sq->pub.sqes)),
			ccmd, 1 << sq->pub.sqes);
	} else {
		struct nvme_prps *prps = sq->prps;

		memcpy((prps->buf + 
			((u32)sq->pub.tail_ptr_virt << sq->pub.sqes)),
			ccmd, 1 << sq->pub.sqes);
		dma_sync_sg_for_device(&pdev->dev, prps->sg, 
			prps->num_map_pgs, prps->data_dir);
	}

	/* Increment the Tail pointer and handle roll over conditions */
	sq->pub.tail_ptr_virt = (u16)(((u32)sq->pub.tail_ptr_virt + 1) % sq->pub.elements);

	kfree(cmd_buf);
	return 0;
out2:
	sq->next_cid--;
out:
	kfree(cmd_buf);
	return ret;
}

