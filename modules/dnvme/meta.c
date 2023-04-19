/**
 * @file meta.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-04-04
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/random.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/vmalloc.h>

#include "queue.h"
#include "core.h"

static int dnvme_meta_setup_sgl(struct nvme_device *ndev, struct nvme_prps *prps)
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
	
	nr_seg = (nr_desc == 1) ? 1 : (DIV_ROUND_UP(
			sizeof(struct nvme_sgl_desc) * nr_desc, 
			PAGE_SIZE - sizeof(struct nvme_sgl_desc)) + 1);

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
	sgl_desc = prp_list[0];
	prps->prp_list = prp_list;
	prps->pg_addr = prp_dma;
	prps->nr_pages = nr_seg;

	if (nr_desc == 1) {
		dnvme_sgl_set_data(&sgl_desc[0], sg);
		return 0;
	}

	dnvme_sgl_set_seg(&sgl_desc[0], prp_dma[1], nr_desc);

	for (j = 1, k= 0; nr_desc > 0;) {
		WARN_ON(j >= nr_seg || !sg);

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

static int dnvme_meta_prepare_sgl(struct nvme_device *ndev, 
	struct nvme_meta *meta, void __user *buf, unsigned int size)
{
	struct nvme_prps *prps;
	int ret;

	prps = kzalloc(sizeof(*prps), GFP_KERNEL);
	if (!prps) {
		dnvme_err(ndev, "failed to alloc prps!\n");
		return -ENOMEM;
	}
	prps->pg_pool = ndev->meta_pool;

	ret = dnvme_map_user_page(ndev, prps, buf, size, DMA_BIDIRECTIONAL, true);
	if (ret < 0)
		goto out_free_prp;

	ret = dnvme_meta_setup_sgl(ndev, prps);
	if (ret < 0)
		goto out_unmap_page;

	meta->prps = prps;
	return 0;

out_unmap_page:
	dnvme_unmap_user_page(ndev, prps);
out_free_prp:
	kfree(prps);
	return ret;
}

int dnvme_create_meta_node(struct nvme_device *ndev, 
	struct nvme_meta_create __user *umc)
{
	struct nvme_meta_create mc;
	struct nvme_meta *meta;
	struct pci_dev *pdev = ndev->pdev;
	int ret;

	if (copy_from_user(&mc, umc, sizeof(mc))) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		return -EFAULT;
	}

	if (!mc.contig && !mc.buf) {
		dnvme_err(ndev, "use SGL list, but buf is NULL!\n");
		return -EINVAL;
	}

	dnvme_dbg(ndev, "meta ID:%u, size:0x%x\n", mc.id, mc.size);

	meta = dnvme_find_meta(ndev, mc.id);
	if (meta) {
		dnvme_err(ndev, "meta node(%u) already exist!\n", mc.id);
		return -EEXIST;
	}

	meta = kzalloc(sizeof(*meta), GFP_KERNEL);
	if (!meta) {
		dnvme_err(ndev, "failed to alloc meta node!\n");
		return -ENOMEM;
	}
	meta->id = mc.id;
	meta->size = mc.size;

	if (mc.contig) {
		meta->buf = dma_alloc_coherent(&pdev->dev, meta->size, 
			&meta->dma, GFP_KERNEL);
		if (!meta->buf) {
			dnvme_err(ndev, "failed to alloc DMA addr for meta!\n");
			ret = -ENOMEM;
			goto out_free_meta;
		}
		get_random_bytes(meta->buf, meta->size);

		meta->contig = 1;
	} else {
		ret = dnvme_meta_prepare_sgl(ndev, meta, mc.buf, mc.size);
		if (ret < 0)
			goto out_free_meta;
	}

	ret = xa_insert(&ndev->meta, meta->id, meta, GFP_KERNEL);
	if (ret < 0) {
		dnvme_err(ndev, "failed to insert meta:%u!(%d)\n", 
			meta->id, ret);
		goto out_free_prp;
	}

	return 0;

out_free_prp:
	if (mc.contig) {
		dma_free_coherent(&pdev->dev, meta->size, meta->buf, meta->dma);
	} else {
		dnvme_release_prps(ndev, meta->prps);
		meta->prps = NULL;
	}

out_free_meta:
	kfree(meta);
	return ret;
}

static void dnvme_delete_meta_node(struct nvme_device *ndev, 
	struct nvme_meta *meta)
{
	struct pci_dev *pdev = ndev->pdev;

	xa_erase(&ndev->meta, meta->id);

	if (meta->contig) {
		dma_free_coherent(&pdev->dev, meta->size, meta->buf, meta->dma);
	} else {
		dnvme_release_prps(ndev, meta->prps);
		meta->prps = NULL;
	}
	kfree(meta);
}

void dnvme_delete_meta_nodes(struct nvme_device *ndev)
{
	struct nvme_meta *meta;
	unsigned long i;

	xa_for_each(&ndev->meta, i, meta) {
		dnvme_delete_meta_node(ndev, meta);
	}
}

void dnvme_delete_meta_id(struct nvme_device *ndev, u32 id)
{
	struct nvme_meta *meta;

	meta = dnvme_find_meta(ndev, id);
	if (!meta) {
		dnvme_warn(ndev, "meta node(%u) doesn't exist! "
			"it may be deleted already\n", id);
		return;
	}
	dnvme_delete_meta_node(ndev, meta);
}

