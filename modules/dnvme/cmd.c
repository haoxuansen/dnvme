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
#include "dnvme_ds.h"

#define SGES_PER_PAGE	(PAGE_SIZE / sizeof(struct nvme_sgl_desc))

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

static int dnvme_setup_sgl(struct nvme_device *ndev, struct nvme_64b_cmd *cmd,
	struct nvme_gen_cmd *gcmd, struct nvme_prps *prps)
{
	unsigned int size = cmd->data_buf_size;
	/* The number of SGL data block descriptors required */
	unsigned int nr_desc; 
	unsigned int nr_seg; /* The number of SGL segments required */
	int entry = prps->num_map_pgs;

	if (entry == 1) {
		dnvme_sgl_set_data(&gcmd->dptr.sgl, prps->sg);
		return 0;
	}


	// !TODO: now
	return 0;
}

static int dnvme_data_buf_to_sgl(struct nvme_device *ndev, 
	struct nvme_64b_cmd *cmd, struct nvme_gen_cmd *gcmd, 
	struct nvme_prps *prps)
{
	// !TODO: now
	return 0;
}
