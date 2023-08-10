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

#include "libbase.h"
#include "libnvme.h"

#include "test.h"

struct meta_config {
	uint16_t	ms; /* metadata size */
	uint32_t	lbads; /* LBA data size */

	uint32_t	meta_ext:1; /* true:extblk, false:separate buffer*/
	uint32_t	meta_sgl:1;
};

static int create_ioq(struct nvme_dev_info *ndev, struct nvme_sq_info *sq, 
	struct nvme_cq_info *cq)
{
	struct nvme_ccq_wrapper ccq_wrap = {0};
	struct nvme_csq_wrapper csq_wrap = {0};
	int ret;

	ccq_wrap.cqid = cq->cqid;
	ccq_wrap.elements = cq->size;
	ccq_wrap.irq_no = cq->irq_no;
	ccq_wrap.irq_en = cq->irq_en;
	ccq_wrap.contig = 1;

	ret = nvme_create_iocq(ndev, &ccq_wrap);
	if (ret < 0) {
		pr_err("failed to create iocq:%u!(%d)\n", cq->cqid, ret);
		return ret;
	}

	csq_wrap.sqid = sq->sqid;
	csq_wrap.cqid = sq->cqid;
	csq_wrap.elements = sq->size;
	csq_wrap.prio = NVME_SQ_PRIO_MEDIUM;
	csq_wrap.contig = 1;

	ret = nvme_create_iosq(ndev, &csq_wrap);
	if (ret < 0) {
		pr_err("failed to create iosq:%u!(%d)\n", sq->sqid, ret);
		return ret;
	}

	return 0;
}

static int delete_ioq(struct nvme_dev_info *ndev, struct nvme_sq_info *sq, 
	struct nvme_cq_info *cq)
{
	int ret;

	ret = nvme_delete_iosq(ndev, sq->sqid);
	if (ret < 0) {
		pr_err("failed to delete iosq:%u!(%d)\n", sq->sqid, ret);
		return ret;
	}

	ret = nvme_delete_iocq(ndev, cq->cqid);
	if (ret < 0) {
		pr_err("failed to delete iocq:%u!(%d)\n", cq->cqid, ret);
		return ret;
	}

	return 0;
}

static int send_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, uint32_t meta_id, uint8_t flags)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_rwc_wrapper wrap = {0};
	uint32_t lbads;
	uint16_t ms;
	int ret;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.flags = flags;
	/* use first active namespace as default */
	wrap.nsid = le32_to_cpu(ns_grp->act_list[0]);
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->wbuf;

	ret = nvme_id_ns_lbads(ns_grp, wrap.nsid, &lbads);
	if (ret < 0) {
		pr_err("failed to get lbads!(%d)\n", ret);
		return ret;
	}
	ms = (uint16_t)nvme_id_ns_ms(ns_grp, wrap.nsid);

	if (meta_id) {
		wrap.size = wrap.nlb * lbads;
		wrap.meta_id = meta_id;
	} else {
		wrap.size = wrap.nlb * (lbads + ms);
	}

	BUG_ON(wrap.size > tool->wbuf_size);
	nvme_fill_data(wrap.buf, wrap.size);

	return nvme_io_write(ndev, &wrap);
}

static int send_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, uint32_t meta_id, uint8_t flags)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_rwc_wrapper wrap = {0};
	uint32_t lbads;
	uint16_t ms;
	int ret;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.flags = flags;
	/* use first active namespace as default */
	wrap.nsid = le32_to_cpu(ns_grp->act_list[0]);
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->rbuf;

	ret = nvme_id_ns_lbads(ns_grp, wrap.nsid, &lbads);
	if (ret < 0) {
		pr_err("failed to get lbads!(%d)\n", ret);
		return ret;
	}
	ms = (uint16_t)nvme_id_ns_ms(ns_grp, wrap.nsid);

	if (meta_id) {
		wrap.size = wrap.nlb * lbads;
		wrap.meta_id = meta_id;
	} else {
		wrap.size = wrap.nlb * (lbads + ms);
	}

	BUG_ON(wrap.size > tool->rbuf_size);

	memset(wrap.buf, 0, wrap.size);

	return nvme_io_read(ndev, &wrap);
}

static int check_ctrl_capability(struct nvme_id_ctrl *ctrl,
	struct meta_config *cfg)
{
	uint32_t sgls = le32_to_cpu(ctrl->sgls);

	if (cfg->meta_sgl && !(sgls & NVME_CTRL_SGLS_MPTR_SGL)) {
		pr_warn("MPTR doesn't support SGL!\n");
		return -EOPNOTSUPP;
	}

	return 0;
}

static int check_ns_capability(struct nvme_id_ns *ns, 
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
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_instance *ns;
	struct nvme_id_ns *id_ns;
	bool need_format = false;
	/* use first active namespace as default */
	uint32_t nsid = le32_to_cpu(ns_grp->act_list[0]);
	uint32_t dw10;
	uint32_t lbaf;
	int ret;

	ns = &ns_grp->ns[nsid - 1];
	id_ns = ns->id_ns;

	ret = check_ns_capability(id_ns, cfg);
	if (ret < 0)
		return ret;

	ret = check_ctrl_capability(ctrl->id_ctrl, cfg);
	if (ret < 0)
		return ret;

	if ((cfg->meta_ext && !(id_ns->flbas & NVME_NS_FLBAS_META_EXT)) || 
		(!cfg->meta_ext && (id_ns->flbas & NVME_NS_FLBAS_META_EXT))) {
		need_format = true;
	}
	lbaf = NVME_NS_FLBAS_LBA(id_ns->flbas);

	if (ns->meta_size != cfg->ms || ns->blk_size != cfg->lbads) {
		ret = select_lbaf(id_ns, cfg);
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
	
	ret = nvme_update_ns_info(ndev, ns);
	if (ret < 0)
		return ret;
	pr_debug("update ns info ok!\n");
	
	ret = nvme_format_nvm(ndev, ns->nsid, 0, dw10);
	if (ret < 0) {
		pr_err("failed to format ns:0x%x!(%d)\n", ns->nsid, ret);
		return ret;
	}
	pr_debug("format ns ok!\n");
	
	ret = nvme_update_ns_info(ndev, ns);
	if (ret < 0)
		return ret;
	
	pr_debug("after format ns:0x%x, lbads:%u, ms:%u, flbas:0x%x\n",
		ns->nsid, ns->blk_size, ns->meta_size, id_ns->flbas);

	return 0;
}

static void fill_meta_create(struct nvme_meta_create *mc, void *buf, 
	uint32_t size, uint16_t id)
{
	memset(mc, 0, sizeof(*mc));
	mc->id = id;
	mc->size = size;
	if (buf)
		mc->buf = buf;
	else
		mc->contig = 1;
}

static int create_meta_nodes(int fd, void **rnode, void **wnode, 
	uint16_t rid, uint16_t wid, uint32_t size)
{
	struct nvme_meta_create mc;
	void *rbuf = NULL;
	void *wbuf = NULL;
	int ret;

	fill_meta_create(&mc, *rnode, size, rid);
	ret = nvme_create_meta_node(fd, &mc);
	if (ret < 0)
		return ret;

	fill_meta_create(&mc, *wnode, size, wid);
	ret = nvme_create_meta_node(fd, &mc);
	if (ret < 0)
		goto out_del_rnode;

	if (!*rnode) {
		rbuf = nvme_map_meta_node(fd, rid, size);
		if (!rbuf) {
			pr_err("failed to map meta:%u!\n", rid);
			ret = -EPERM;
			goto out_del_wnode;
		}
		*rnode = rbuf;
	}
	memset(*rnode, 0, size);

	if (!*wnode) {
		wbuf = nvme_map_meta_node(fd, wid, size);
		if (!wbuf) {
			pr_err("failed to map meta:%u!\n", wid);
			ret = -EPERM;
			goto out_unmap_rnode;
		}
		*wnode = wbuf;
	}
	nvme_fill_data(*wnode, size);
	return 0;

out_unmap_rnode:
	if (rbuf)
		nvme_unmap_meta_node(rbuf, size);
out_del_wnode:
	nvme_delete_meta_node(fd, wid);
out_del_rnode:
	nvme_delete_meta_node(fd, rid);
	return ret;
}

static int delete_meta_nodes(int fd, void *rnode, void *wnode, 
	uint16_t rid, uint16_t wid, uint32_t size)
{
	int ret = 0;

	if (rnode)
		ret |= nvme_unmap_meta_node(rnode, size);
	if (wnode)
		ret |= nvme_unmap_meta_node(wnode, size);

	ret |= nvme_delete_meta_node(fd, rid);
	ret |= nvme_delete_meta_node(fd, wid);
	return ret;
}

/**
 * @brief Meta data is transferred as separate buffer, eg: SGL
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int case_meta_xfer_separate_sgl(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct meta_config cfg = {0};
	/* use first active namespace as default */
	uint32_t nsid = le32_to_cpu(ns_grp->act_list[0]);
	uint8_t flags = NVME_CMD_SGL_METASEG;
	uint32_t wid = 1, rid = 2;
	uint32_t nlb = 8;
	uint32_t size;
	uint32_t lbads;
	int ret;

	ret = nvme_id_ns_lbads(ns_grp, nsid, &lbads);
	if (ret < 0)
		return ret;

	pr_notice("Please enter the meta size: ");
	scanf("%hu", &cfg.ms);

	cfg.lbads = 512;
	cfg.meta_ext = 0;
	cfg.meta_sgl = 1;

	ret = prepare_meta_config(ndev, &cfg);
	if (ret < 0)
		return ret;

	ret = create_meta_nodes(ndev->fd, &tool->meta_rbuf, &tool->meta_wbuf,
		rid, wid, cfg.ms * nlb);
	if (ret < 0)
		return ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		ret = -ENODEV;
		goto out_del_meta;
	}

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		goto out_del_meta;

	ret = send_io_write_cmd(tool, sq, 0, nlb, wid, flags);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out_del_ioq;
	}

	ret = send_io_read_cmd(tool, sq, 0, nlb, rid, flags);
	if (ret < 0) {
		pr_err("failed to read data!(%d)\n", ret);
		goto out_del_ioq;
	}
	size = lbads * nlb;

	ret = memcmp(tool->rbuf, tool->wbuf, size);
	if (ret) {
		pr_err("failed to compare r/w data!(%d)\n", ret);
		ret = -EIO;
		goto out_del_ioq;
	}

	ret = memcmp(tool->meta_rbuf, tool->meta_wbuf, cfg.ms * nlb);
	if (ret) {
		pr_err("failed to compare meta data!(%d)\n", ret);
		ret = -EIO;
		goto out_del_ioq;
	}

out_del_ioq:
	ret |= delete_ioq(ndev, sq, cq);
out_del_meta:
	ret |= delete_meta_nodes(ndev->fd, NULL, NULL, rid, wid, cfg.ms * nlb);
	return ret;
}
NVME_CASE_META_SYMBOL(case_meta_xfer_separate_sgl, 
	"Meta data is transferred as SGL (Scatter/Gather List)");

/**
 * @brief Meta data is transferred as separate buffer, eg: PRP
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int case_meta_xfer_separate_prp(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct meta_config cfg = {0};
	/* use first active namespace as default */
	uint32_t nsid = le32_to_cpu(ns_grp->act_list[0]);
	void *rmeta = NULL;
	void *wmeta = NULL;
	uint32_t wid = 1, rid = 2;
	uint32_t nlb = 8;
	uint32_t size;
	uint32_t lbads;
	int ret;

	ret = nvme_id_ns_lbads(ns_grp, nsid, &lbads);
	if (ret < 0)
		return ret;

	pr_notice("Please enter the meta size: ");
	scanf("%hu", &cfg.ms);

	cfg.lbads = 512;
	cfg.meta_ext = 0;

	ret = prepare_meta_config(ndev, &cfg);
	if (ret < 0)
		return ret;

	ret = create_meta_nodes(ndev->fd, &rmeta, &wmeta, rid, wid, cfg.ms * nlb);
	if (ret < 0)
		return ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		ret = -ENODEV;
		goto out_del_meta;
	}

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		goto out_del_meta;

	ret = send_io_write_cmd(tool, sq, 0, nlb, wid, 0);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out_del_ioq;
	}

	ret = send_io_read_cmd(tool, sq, 0, nlb, rid, 0);
	if (ret < 0) {
		pr_err("failed to read data!(%d)\n", ret);
		goto out_del_ioq;
	}
	size = lbads * nlb;

	ret = memcmp(tool->rbuf, tool->wbuf, size);
	if (ret) {
		pr_err("failed to compare r/w data!(%d)\n", ret);
		ret = -EIO;
		goto out_del_ioq;
	}

	ret = memcmp(rmeta, wmeta, cfg.ms * nlb);
	if (ret) {
		pr_err("failed to compare r/w meta!(%d)\n", ret);
		ret = -EIO;
		goto out_del_ioq;
	}

out_del_ioq:
	ret |= delete_ioq(ndev, sq, cq);
out_del_meta:
	ret |= delete_meta_nodes(ndev->fd, rmeta, wmeta, rid, wid, cfg.ms * nlb);
	return ret;
}
NVME_CASE_META_SYMBOL(case_meta_xfer_separate_prp, 
	"Meta data is transferred as PRP (Phsical Region Page)");

/**
 * @brief Meta data is transferred contiguous with LBA Data
 * 
 * @return 0 on success, otherwise a negative errno
 */
static int case_meta_xfer_contig_lba(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct meta_config cfg = {0};
	/* use first active namespace as default */
	uint32_t nsid = le32_to_cpu(ns_grp->act_list[0]);
	uint32_t nlb = 8;
	uint32_t size;
	uint32_t lbads;
	uint16_t ms;
	int ret;

	ret = nvme_id_ns_lbads(ns_grp, nsid, &lbads);
	if (ret < 0)
		return ret;
	ms = (uint16_t)nvme_id_ns_ms(ns_grp, nsid);

	pr_notice("Please enter the meta size: ");
	scanf("%hu", &cfg.ms);

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

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	ret = send_io_write_cmd(tool, sq, 0, nlb, 0, 0);
	if (ret < 0) {
		pr_err("failed to write data!(%d)\n", ret);
		goto out_del_ioq;
	}

	ret = send_io_read_cmd(tool, sq, 0, nlb, 0, 0);
	if (ret < 0) {
		pr_err("failed to read data!(%d)\n", ret);
		goto out_del_ioq;
	}
	size = (lbads + ms) * nlb;

	ret = memcmp(tool->rbuf, tool->wbuf, size);
	if (ret) {
		pr_err("failed to compare r/w data!(%d)\n", ret);
		ret = -EIO;
		goto out_del_ioq;
	}
	pr_info("meta data r/w compare ok!\n");

out_del_ioq:
	ret |= delete_ioq(ndev, sq, cq);
	return ret;
}
NVME_CASE_META_SYMBOL(case_meta_xfer_contig_lba, 
	"Meta data is transferred contiguous with LBA data");

/**
 * @brief Create meta node which is contiguous, then delete it later.
 * 
 * @return 0 on success, otherwise a negative errno. 
 */
static int case_meta_node_contiguous(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_meta_create mc = {0};
	void *meta;
	int ret;

	pr_notice("Please enter the meta node ID: ");
	scanf("%hu", &mc.id);

	pr_notice("Please enter the meta node size: ");
	scanf("%u", &mc.size);

	mc.contig = 1;

	ret = nvme_create_meta_node(ndev->fd, &mc);
	if (ret < 0)
		return ret;

	meta = nvme_map_meta_node(ndev->fd, mc.id, mc.size);
	if (!meta) {
		pr_err("failed to map meta node!\n");
		ret = -EPERM;
		goto out_del_node;
	}

	pr_notice("Is ready to dump meta data: ");
	scanf("%d", &ret);

	pr_debug("Origin meta data:\n");
	dump_data_to_console(meta, mc.size);

	nvme_fill_data(meta, mc.size);
	pr_debug("New meta data:\n");
	dump_data_to_console(meta, mc.size);

	pr_notice("Is ready to delete meta node: ");
	scanf("%d", &ret);

	ret = nvme_unmap_meta_node(meta, mc.size);
	if (ret < 0) {
		pr_err("failed to unmap meta node!\n");
		goto out_del_node;
	}

out_del_node:
	ret |= nvme_delete_meta_node(ndev->fd, mc.id);
	return ret;
}
NVME_CASE_META_SYMBOL(case_meta_node_contiguous, 
	"Create a contiguous meta node and delete it later");
