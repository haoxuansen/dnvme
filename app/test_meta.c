/**
 * @file test_meta.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-03-23
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "log.h"
#include "core.h"
#include "cmd.h"
#include "queue.h"
#include "meta.h"

#include "test.h"
#include "test_meta.h"

struct meta_config {
	uint16_t	ms; /* metadata size */
	uint32_t	lbads; /* LBA data size */

	uint32_t	meta_ext:1;
};

static int create_ioq(int fd, struct nvme_sq_info *sq, struct nvme_cq_info *cq)
{
	struct nvme_ccq_wrapper ccq_wrap = {0};
	struct nvme_csq_wrapper csq_wrap = {0};
	int ret;

	ccq_wrap.cqid = cq->cqid;
	ccq_wrap.elements = cq->size;
	ccq_wrap.irq_no = cq->irq_no;
	ccq_wrap.irq_en = cq->irq_en;
	ccq_wrap.contig = 1;

	ret = nvme_create_iocq(fd, &ccq_wrap);
	if (ret < 0) {
		pr_err("failed to create iocq:%u!(%d)\n", cq->cqid, ret);
		return ret;
	}

	csq_wrap.sqid = sq->sqid;
	csq_wrap.cqid = sq->cqid;
	csq_wrap.elements = sq->size;
	csq_wrap.prio = NVME_SQ_PRIO_MEDIUM;
	csq_wrap.contig = 1;

	ret = nvme_create_iosq(fd, &csq_wrap);
	if (ret < 0) {
		pr_err("failed to create iosq:%u!(%d)\n", sq->sqid, ret);
		return ret;
	}

	return 0;
}

static int delete_ioq(int fd, struct nvme_sq_info *sq, struct nvme_cq_info *cq)
{
	int ret;

	ret = nvme_delete_iosq(fd, sq->sqid);
	if (ret < 0) {
		pr_err("failed to delete iosq:%u!(%d)\n", sq->sqid, ret);
		return ret;
	}

	ret = nvme_delete_iocq(fd, cq->cqid);
	if (ret < 0) {
		pr_err("failed to delete iocq:%u!(%d)\n", cq->cqid, ret);
		return ret;
	}

	return 0;
}

static int send_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, uint32_t meta_id)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};
	uint32_t i;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = ndev->nss[0].nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->wbuf;
	if (meta_id) {
		wrap.size = wrap.nlb * ndev->nss[0].lbads;
		wrap.meta_id = meta_id;
	} else {
		wrap.size = wrap.nlb * (ndev->nss[0].lbads + ndev->nss[0].ms);
	}

	BUG_ON(wrap.size > tool->wbuf_size);

	for (i = 0; i < (wrap.size / 4); i+= 4) {
		*(uint32_t *)(wrap.buf + i) = (uint32_t)rand();
	}
	pr_debug("write size: %u\n", wrap.size);

	return nvme_io_write(ndev->fd, &wrap);
}

static int send_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, uint32_t meta_id)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = ndev->nss[0].nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->rbuf;
	if (meta_id) {
		wrap.size = wrap.nlb * ndev->nss[0].lbads;
		wrap.meta_id = meta_id;
	} else {
		wrap.size = wrap.nlb * (ndev->nss[0].lbads + ndev->nss[0].ms);
	}

	BUG_ON(wrap.size > tool->rbuf_size);

	memset(wrap.buf, 0, wrap.size);

	pr_debug("read size: %u\n", wrap.size);

	return nvme_io_read(ndev->fd, &wrap);
}

static int check_meta_capability(struct nvme_id_ns *ns, 
	struct meta_config *cfg)
{
	if (cfg->meta_ext && !(ns->mc & NVME_MC_EXTENDED_LBA)) {
		pr_warn("Not support contiguous with LBA Data!\n");
		return -EOPNOTSUPP;
	}

	if (!cfg->meta_ext && !(ns->mc & NVME_MC_METADATA_PTR)) {
		pr_warn("Not support transferred as separate buffer!\n");
		return -EOPNOTSUPP;
	}

	return 0;
}

static int select_lbaf(struct nvme_id_ns *ns, struct meta_config *cfg)
{
	struct nvme_lbaf *lbaf;
	uint32_t i;

	for (i = 0; i <= ns->nlbaf; i++) {
		lbaf = &ns->lbaf[i];

		if (lbaf->ms == cfg->ms && (1 << lbaf->ds) == cfg->lbads)
			return i;
	}

	return -EPERM;
}

static int prepare_meta_config(struct nvme_dev_info *ndev, 
	struct meta_config *cfg)
{
	struct nvme_ns_info *ns = &ndev->nss[0];
	bool need_format = false;
	uint32_t dw10;
	uint32_t lbaf;
	int ret;

	ret = check_meta_capability(&ns->id_ns, cfg);
	if (ret < 0)
		return ret;

	if ((cfg->meta_ext && !(ns->id_ns.flbas & NVME_NS_FLBAS_META_EXT)) || 
		(!cfg->meta_ext && (ns->id_ns.flbas & NVME_NS_FLBAS_META_EXT))) {
		need_format = true;
	}
	lbaf = NVME_NS_FLBAS_LBA(ns->id_ns.flbas);

	if (ns->ms != cfg->ms || ns->lbads != cfg->lbads) {
		ret = select_lbaf(&ns->id_ns, cfg);
		if (ret < 0) {
			pr_err("failed to find lbaf for ms(%u), lbads(%u)\n",
				cfg->ms, cfg->lbads);
			return ret;
		}
		lbaf = ret;
		need_format = true;
		pr_debug("select lbaf index:%d for ms:%u, lbads:%u\n",
			lbaf, cfg->ms, cfg->lbads);
	}

	if (!need_format) {
		pr_debug("Don't need to format namespace!\n");
		return 0;
	}

	dw10 = NVME_FMT_LBAFL(lbaf) | NVME_FMT_LBAFU(lbaf);
	if (cfg->meta_ext)
		dw10 |= NVME_FMT_MSET_EXT;
	
	dw10 &= ~NVME_FMT_SES_MASK;
	dw10 |= NVME_FMT_SES_USER;
	
	ret = nvme_update_ns_info(ndev->fd, ns);
	if (ret < 0)
		return ret;
	pr_debug("update ns info ok!\n");
	
	ret = nvme_format_nvm(ndev->fd, ns->nsid, 0, dw10);
	if (ret < 0) {
		pr_err("failed to format ns:0x%x!(%d)\n", ns->nsid, ret);
		return ret;
	}
	pr_debug("format ns ok!\n");
	
	ret = nvme_update_ns_info(ndev->fd, ns);
	if (ret < 0)
		return ret;
	
	pr_debug("after format ns:0x%x, lbads:%u, ms:%u, flbas:0x%x\n",
		ns->nsid, ns->lbads, ns->ms, ns->id_ns.flbas);

	return 0;
}


int case_meta_xfer_separate(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct meta_config cfg = {0};
	uint32_t wid = 1, rid = 2;
	uint32_t nlb = 8;
	uint32_t size;
	int ret;

	pr_notice("Please enter the meta size: ");
	scanf("%hu", &cfg.ms);

	cfg.lbads = 512;
	cfg.meta_ext = 0;

	ret = prepare_meta_config(ndev, &cfg);
	if (ret < 0)
		return ret;

	ret = nvme_create_meta_pool(ndev->fd, cfg.ms * nlb);
	if (ret < 0)
		return ret;

	ret = nvme_create_meta_node(ndev->fd, rid);
	if (ret < 0)
		goto out_destroy_pool;

	ret = nvme_create_meta_node(ndev->fd, wid);
	if (ret < 0)
		goto out_destroy_pool;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		ret = -ENODEV;
		goto out_destroy_pool;
	}

	ret = create_ioq(ndev->fd, sq, cq);
	if (ret < 0)
		goto out_destroy_pool;

	ret = send_io_write_cmd(tool, sq, 0, nlb, wid);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out_del_ioq;
	}

	ret = send_io_read_cmd(tool, sq, 0, nlb, rid);
	if (ret < 0) {
		pr_err("failed to read data!(%d)\n", ret);
		goto out_del_ioq;
	}
	size = ndev->nss[0].lbads * nlb;

	ret = memcmp(tool->rbuf, tool->wbuf, size);
	if (ret) {
		pr_err("failed to compare r/w data!(%d)\n", ret);
		goto out_del_ioq;
	}

	ret = nvme_compare_meta_node(ndev->fd, wid, rid);
	if (ret < 0) {
		pr_err("failed to compare r/w meta!(%d)\n", ret);
		goto out_del_ioq;
	}

out_del_ioq:
	delete_ioq(ndev->fd, sq, cq);
out_destroy_pool:
	nvme_destroy_meta_pool(ndev->fd);
	return ret;
}

int case_meta_xfer_extlba(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct meta_config cfg = {0};
	uint32_t size;
	int ret;

	cfg.ms = 8;
	cfg.lbads = 512;
	cfg.meta_ext = 1;

	ret = prepare_meta_config(ndev, &cfg);
	if (ret < 0)
		return ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev->fd, sq, cq);
	if (ret < 0)
		return ret;

	ret = send_io_write_cmd(tool, sq, 0, 8, 0);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out_del_ioq;
	}

	ret = send_io_read_cmd(tool, sq, 0, 8, 0);
	if (ret < 0) {
		pr_err("failed to read data!(%d)\n", ret);
		goto out_del_ioq;
	}
	size = (ndev->nss[0].lbads + ndev->nss[0].ms) * 8;

	ret = memcmp(tool->rbuf, tool->wbuf, size);
	if (ret) {
		pr_err("failed to compare r/w data!(%d)\n", ret);
		ret = -EIO;
		goto out_del_ioq;
	}
	pr_info("meta data r/w compare ok!\n");

out_del_ioq:
	delete_ioq(ndev->fd, sq, cq);
	return ret;
}

int case_meta_create_pool(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	uint32_t size;

	pr_notice("Please enter the meta size which is used to crerate pool: ");
	scanf("%u", &size);

	if (!size || size > 4096) {
		pr_warn("meta size %u is invalid! stopped.\n", size);
		return 0;
	}

	return nvme_create_meta_pool(ndev->fd, size);
}

int case_meta_destroy_pool(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;

	return nvme_destroy_meta_pool(ndev->fd);
}

int case_meta_create_node(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	uint32_t id;

	pr_notice("Please enter the meta ID which is used to crerate node: ");
	scanf("%u", &id);

	if (!id) {
		pr_warn("meta ID %u is invalid! stopped.\n", id);
		return 0;
	}

	return nvme_create_meta_node(ndev->fd, id);
}

int case_meta_delete_node(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	uint32_t id;

	pr_notice("Please enter the meta ID which is used to crerate node: ");
	scanf("%u", &id);

	if (!id) {
		pr_warn("meta ID %u is invalid! stopped.\n", id);
		return 0;
	}

	return nvme_delete_meta_node(ndev->fd, id);
}

