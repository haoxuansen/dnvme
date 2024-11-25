#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"

#include "test.h"

#define DEF_LOOP_NUM		1
#define DEF_CMD_NUM			512
#define DEF_WRITE_SIZE		SZ_4K /* block size align */

struct cmd_group {
	int			id;

	struct {
		void		*buf;
		uint32_t	size;
		int			part;
	} read;
	struct {
		void		*buf;
		uint32_t	size;
		int			part;
	} write;
	struct {
		struct copy_resource *res;
		int			src_part;
		int			dst_part;
	} copy;

	/* flag */
	uint32_t	submit_io_read:1;
	uint32_t	submit_io_write:1;
	uint32_t	submit_io_compare:1;
	uint32_t	submit_io_verify:1;
	uint32_t	submit_io_copy:1;
};

struct cmd_set {
	int			nr_grp;
	int			nr_used_grp;
	int			nr_submit_cmd;
	struct cmd_group *grp[0];
};

static struct cmd_group *cmd_group_alloc(uint32_t buf_size)
{
	struct cmd_group *grp = NULL;
	int ret;

	grp = zalloc(sizeof(struct cmd_group));
	if (!grp) {
		pr_err("failed to alloc cmd group!\n");
		return NULL;
	}

	ret = posix_memalign(&grp->read.buf, getpagesize(), buf_size);
	if (ret) {
		pr_err("failed to alloc read buf!\n");
		goto free_grp;
	}
	grp->read.size = buf_size;

	ret = posix_memalign(&grp->write.buf, getpagesize(), buf_size);
	if (ret) {
		pr_err("failed to alloc write buf!\n");
		goto free_rbuf;
	}
	grp->write.size = buf_size;

	grp->copy.res = ut_alloc_copy_resource(1, NVME_COPY_DESC_FMT_32B);
	if (!grp->copy.res) {
		pr_err("failed to alloc copy desc!\n");
		goto free_wbuf;
	}
	return grp;

free_wbuf:
	free(grp->write.buf);
free_rbuf:
	free(grp->read.buf);
free_grp:
	free(grp);
	return NULL;
}

static void cmd_group_release(struct cmd_group *grp)
{
	if (!grp)
		return;
	
	ut_release_copy_resource(grp->copy.res);
	free(grp->read.buf);
	free(grp->write.buf);
	free(grp);
}

static inline int cmd_group_init_buffer(struct cmd_group *grp)
{
	fill_data_with_random(grp->write.buf, grp->write.size);
	return 0;
}

static struct cmd_set *cmd_set_alloc(int nr_grp, uint32_t buf_size) 
{
	struct cmd_set *set = NULL;
	int i;

	set = zalloc(sizeof(struct cmd_set) + nr_grp * sizeof(struct cmd_group*));
	if (!set) {
		pr_err("failed to alloc cmd set!\n");
		return NULL;
	}
	set->nr_grp = nr_grp; 

	for (i = 0; i < nr_grp; i++) {
		set->grp[i] = cmd_group_alloc(buf_size);
		if (!set->grp[i]) {
			pr_err("failed to alloc cmd group%d!\n", i);
			goto out;
		}
		set->grp[i]->id = i;
	}

	return set;
out:
	for (i--; i >= 0; i--) {
		cmd_group_release(set->grp[i]);
	}
	free(set);
	return NULL;
}

static void cmd_set_release(struct cmd_set *set)
{
	int i;

	if (!set)
		return;

	for (i = 0; i < set->nr_grp; i++) {
		cmd_group_release(set->grp[i]);
		set->grp[i] = NULL;
	}
	free(set);
}

static int cmd_set_init_buffer(struct cmd_set *set)
{
	int i;

	for (i = 0; i < set->nr_grp; i++) {
		cmd_group_init_buffer(set->grp[i]);
	}
	return 0;
}

static int round_read_write_submit(struct case_data *priv,
	struct nvme_sq_info *sq, struct cmd_set *set)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_config_effect effect = {0};
	uint64_t slba = 0;
	uint32_t lbads;
	uint32_t nlb;
	int i;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	ret = nvme_id_ns_lbads(ns_grp, effect.nsid, &lbads);
	if (ret < 0)
		return ret;

	for (i = 0; i < set->nr_grp; i++) {
		nlb = set->grp[i]->write.size / lbads;
		BUG_ON(set->grp[i]->write.size % lbads);

		effect.cmd.write.buf = set->grp[i]->write.buf;
		effect.cmd.write.size = set->grp[i]->write.size;
		ret = ut_submit_io_write_cmd(priv, sq, slba, nlb);
		if (ret < 0) {
			pr_err("ERR-%d: submit write cmd!\n", ret);
			return ret;
		}

		nlb = set->grp[i]->read.size / lbads;
		BUG_ON(set->grp[i]->read.size % lbads);

		effect.cmd.read.buf = set->grp[i]->read.buf;
		effect.cmd.read.size = set->grp[i]->read.size;
		ret = ut_submit_io_read_cmd(priv, sq, slba, nlb);
		if (ret < 0) {
			pr_err("ERR-%d: submit read cmd!\n", ret);
			return ret;
		}
	}

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0) {
		pr_err("ERR-%d: ring doorbell!\n", ret);
		return ret;
	}

	pr_info("r/w cmds submit ok!\n");
	return 0;
}

static int round_read_write_polling(struct case_data *priv, 
	struct nvme_cq_info *cq, struct cmd_set *set)
{
	int nr_cmd;
	int ret;

	nr_cmd = set->nr_grp * 2;

	ret = ut_reap_cq_entry_check_status(priv, cq, nr_cmd);
	if (ret < 0) {
		pr_err("ERR-%d: reap cq!\n", ret);
		return ret;
	}
	pr_info("r/w cmds cqe reap ok!\n");
	return 0;
}

static int round_read_write_verify(struct cmd_set *set)
{
	int ret;
	int i;

	pr_notice("start verify data...\n");
	for (i = 0; i < set->nr_grp; i++) {
		ret = memcmp(set->grp[i]->read.buf, set->grp[i]->write.buf, 
			set->grp[i]->read.size);
		if (ret != 0) {
			pr_err("ERR: read vs write!(grp:%d)\n", i);
			return -EIO;
		}
	}
	pr_info("r/w data verify ok!\n");
	return 0;
}

static int round_read_write(struct case_data *priv, struct cmd_set *set)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = NULL;
	struct nvme_cq_info *cq = NULL;
	int ret;

	sq = &ndev->iosqs[0];
	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq)
		return -ENOENT;
	
	sq->nr_entry = 1024; /* !TODO: check cap */
	cq->nr_entry = 1024;

	ret = ut_create_pair_io_queue(priv, sq, cq);
	if (ret < 0)
		return ret;	

	ret = round_read_write_submit(priv, sq, set);
	if (ret < 0)
		goto del_ioq;
	
	ret = round_read_write_polling(priv, cq, set);
	if (ret < 0)
		goto del_ioq;
	
	ret = round_read_write_verify(set);
	if (ret < 0)
		goto del_ioq;

del_ioq:
	ut_delete_pair_io_queue(priv, sq, NULL);
	return ret;
}

static int round_rwcv_submit(struct case_data *priv,
	struct nvme_sq_info *sq, struct cmd_set *set)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_config_effect effect = {0};
	uint64_t slba = 0;
	uint32_t lbads;
	uint32_t nlb;
	uint32_t nr_cmd = 0;
	int i;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	ret = nvme_id_ns_lbads(ns_grp, effect.nsid, &lbads);
	if (ret < 0)
		return ret;
	
	for (i = 0; i < set->nr_grp && nr_cmd < DEF_CMD_NUM; i++) {
		nlb = set->grp[i]->write.size / lbads;
		BUG_ON(set->grp[i]->write.size % lbads);

		effect.cmd.write.buf = set->grp[i]->write.buf;
		effect.cmd.write.size = set->grp[i]->write.size;
		ret = ut_submit_io_write_cmd(priv, sq, slba, nlb);
		if (ret < 0) {
			pr_err("ERR-%d: submit write cmd!\n", ret);
			return ret;
		}
		nr_cmd++;

		nlb = set->grp[i]->read.size / lbads;
		BUG_ON(set->grp[i]->read.size % lbads);

		effect.cmd.read.buf = set->grp[i]->read.buf;
		effect.cmd.read.size = set->grp[i]->read.size;
		ret = ut_submit_io_read_cmd(priv, sq, slba, nlb);
		if (ret < 0) {
			pr_err("ERR-%d: submit read cmd!\n", ret);
			return ret;
		}
		nr_cmd++;

		if (rand() & 0x1) {
			nlb = set->grp[i]->write.size / lbads;
			effect.cmd.compare.buf = set->grp[i]->write.buf;
			effect.cmd.compare.size = set->grp[i]->write.size;
			ret = ut_submit_io_compare_cmd(priv, sq, slba, nlb);
			if (ret < 0) {
				pr_err("ERR-%d: submit compare cmd!\n", ret);
				return ret;
			}
			set->grp[i]->submit_io_compare = 1;
			nr_cmd++;
		}
		if (rand() & 0x1) {
			nlb = set->grp[i]->write.size / lbads;
			ret = ut_submit_io_verify_cmd(priv, sq, slba, nlb);
			if (ret < 0) {
				pr_err("ERR-%d: submit verify cmd!\n", ret);
				return ret;
			}
			set->grp[i]->submit_io_verify = 1;
			nr_cmd++;
		}
	}
	set->nr_used_grp = i;
	set->nr_submit_cmd = nr_cmd;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0) {
		pr_err("ERR-%d: ring doorbell!\n", ret);
		return ret;
	}

	pr_info("r/w cmds submit ok!(grp:%d)\n", set->nr_used_grp);
	return 0;
}

static int round_rwcv_polling(struct case_data *priv, 
	struct nvme_cq_info *cq, struct cmd_set *set)
{
	int ret;

	ret = ut_reap_cq_entry_check_status(priv, cq, set->nr_submit_cmd);
	if (ret < 0) {
		pr_err("ERR-%d: reap cq!\n", ret);
		return ret;
	}
	pr_info("r/w cmds cqe reap ok!\n");
	return 0;
}

static int round_rwcv_verify(struct cmd_set *set)
{
	int ret;
	int i;

	pr_notice("start verify data...\n");
	for (i = 0; i < set->nr_used_grp; i++) {
		ret = memcmp(set->grp[i]->read.buf, set->grp[i]->write.buf, 
			set->grp[i]->read.size);
		if (ret != 0) {
			pr_err("ERR: read vs write!(grp:%d)\n", i);
			return -EIO;
		}
	}
	pr_info("data verify ok!\n");
	return 0;
}

static int round_rwcv(struct case_data *priv, struct cmd_set *set)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = NULL;
	struct nvme_cq_info *cq = NULL;
	int ret;

	sq = &ndev->iosqs[0];
	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq)
		return -ENOENT;
	
	sq->nr_entry = 1024; /* !TODO: check cap */
	cq->nr_entry = 1024;

	ret = ut_create_pair_io_queue(priv, sq, cq);
	if (ret < 0)
		return ret;	

	ret = round_rwcv_submit(priv, sq, set);
	if (ret < 0)
		goto del_ioq;
	
	ret = round_rwcv_polling(priv, cq, set);
	if (ret < 0)
		goto del_ioq;
	
	ret = round_rwcv_verify(set);
	if (ret < 0)
		goto del_ioq;

del_ioq:
	ut_delete_pair_io_queue(priv, sq, NULL);
	return ret;
}

static int round_rwcvc_submit(struct case_data *priv,
	struct nvme_sq_info *sq, struct cmd_set *set)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_config_effect effect = {0};
	struct copy_resource *copy = NULL;
	uint64_t slba0 = 0;
	uint64_t slba1 = 0;
	uint64_t slba;
	uint32_t lbads;
	uint32_t nlb;
	uint32_t nr_cmd = 0;
	int i;
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	ret = nvme_id_ns_lbads(ns_grp, effect.nsid, &lbads);
	if (ret < 0)
		return ret;

	BUG_ON(DEF_WRITE_SIZE % lbads);
	slba1 = DEF_WRITE_SIZE / lbads;
	nlb = DEF_WRITE_SIZE / lbads;
	slba = (rand() % 2) ? slba0 : slba1;

	for (i = 0; i < set->nr_grp && nr_cmd < DEF_CMD_NUM; i++) {
		if (i != 0 || (rand() & 0x1)) {
			effect.cmd.write.buf = set->grp[i]->write.buf;
			effect.cmd.write.size = set->grp[i]->write.size;
			ret = ut_submit_io_write_cmd(priv, sq, slba, nlb);
			if (ret < 0) {
				pr_err("ERR-%d: submit write cmd!\n", ret);
				return ret;
			}
			set->grp[i]->write.part = (slba == slba0) ? 0 : 1;
			set->grp[i]->submit_io_write = 1;
			nr_cmd++;
		}

		if (i != 0 || (rand() & 0x1)) {
			effect.cmd.read.buf = set->grp[i]->read.buf;
			effect.cmd.read.size = set->grp[i]->read.size;
			ret = ut_submit_io_read_cmd(priv, sq, slba, nlb);
			if (ret < 0) {
				pr_err("ERR-%d: submit read cmd!\n", ret);
				return ret;
			}
			set->grp[i]->read.part = (slba == slba0) ? 0 : 1;
			set->grp[i]->submit_io_read = 1;
			nr_cmd++;
		}

		if (rand() & 0x1) {
			effect.cmd.compare.buf = set->grp[i]->write.buf;
			effect.cmd.compare.size = set->grp[i]->write.size;
			ret = ut_submit_io_compare_cmd(priv, sq, slba, nlb);
			if (ret < 0) {
				pr_err("ERR-%d: submit compare cmd!\n", ret);
				return ret;
			}
			set->grp[i]->submit_io_compare = 1;
			nr_cmd++;
		}
		if (rand() & 0x1) {
			ret = ut_submit_io_verify_cmd(priv, sq, slba, nlb);
			if (ret < 0) {
				pr_err("ERR-%d: submit verify cmd!\n", ret);
				return ret;
			}
			set->grp[i]->submit_io_verify = 1;
			nr_cmd++;
		}
		if (rand() & 0x1) {
			copy = set->grp[i]->copy.res;
			if (rand() & 0x1) {
				copy->slba = slba0;
				copy->entry[0].slba = slba1;
				set->grp[i]->copy.dst_part = 1;
				set->grp[i]->copy.src_part = 2;
			} else {
				copy->slba = slba1;
				copy->entry[0].slba = slba0;
				set->grp[i]->copy.dst_part = 2;
				set->grp[i]->copy.src_part = 1;
			}
			copy->entry[0].nlb = nlb;

			ret = ut_submit_io_copy_cmd(priv, sq, copy);
			if (ret < 0) {
				pr_err("ERR-%d: submit copy cmd!\n", ret);
				return ret;
			}
			set->grp[i]->submit_io_copy = 1;
			nr_cmd++;
		}
	}
	set->nr_used_grp = i;
	set->nr_submit_cmd = nr_cmd;

	ret = ut_ring_sq_doorbell(priv, sq);
	if (ret < 0) {
		pr_err("ERR-%d: ring doorbell!\n", ret);
		return ret;
	}

	pr_info("r/w cmds submit ok!(grp:%d)\n", set->nr_used_grp);
	return 0;
}

static int round_rwcvc_polling(struct case_data *priv, 
	struct nvme_cq_info *cq, struct cmd_set *set)
{
	int ret;

	ret = ut_reap_cq_entry_no_check(priv, cq, set->nr_submit_cmd);
	if (ret < 0) {
		pr_err("ERR-%d: reap cq!\n", ret);
		return ret;
	}
	pr_info("r/w cmds cqe reap ok!\n");
	return 0;
}

/**
 * @note W->R->C->V->C
 */
static int round_rwcvc_verify(struct cmd_set *set)
{
	void *buf[2] = {NULL, NULL};
	int ret;
	int i;

	pr_notice("start verify data...\n");
	for (i = 0; i < set->nr_grp; i++) {
		if (set->grp[i]->submit_io_write) {
			buf[set->grp[i]->write.part] = set->grp[i]->write.buf;
		}
		if (set->grp[i]->submit_io_read) {
			if (buf[set->grp[i]->read.part]) {
				ret = memcmp(set->grp[i]->read.buf, buf[set->grp[i]->read.part],
					set->grp[i]->read.size);
				if (ret != 0) {
					pr_err("ERR: read (grp:%d)\n", i);
					return -EIO;
				}
			}
		}
		if (set->grp[i]->submit_io_copy) {
			if (buf[set->grp[i]->copy.src_part]) {
				buf[set->grp[i]->copy.dst_part] = buf[set->grp[i]->copy.src_part];
			}
		}
	}
	pr_info("data verify ok!\n");
	return 0;
}

static int round_rwcvc(struct case_data *priv, struct cmd_set *set)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = NULL;
	struct nvme_cq_info *cq = NULL;
	int ret;

	sq = &ndev->iosqs[0];
	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq)
		return -ENOENT;
	
	sq->nr_entry = 1024; /* !TODO: check cap */
	cq->nr_entry = 1024;

	ret = ut_create_pair_io_queue(priv, sq, cq);
	if (ret < 0)
		return ret;	

	ret = round_rwcvc_submit(priv, sq, set);
	if (ret < 0)
		goto del_ioq;
	
	ret = round_rwcvc_polling(priv, cq, set);
	if (ret < 0)
		goto del_ioq;
	
	ret = round_rwcvc_verify(set);
	if (ret < 0)
		goto del_ioq;

del_ioq:
	ut_delete_pair_io_queue(priv, sq, NULL);
	return ret;
}

/**
 * @note Each group submit read/write cmd
 */
static int subcase_read_write(struct case_data *priv)
{
	struct cmd_set *set = NULL;
	int loop = 0;
	int ret;

	set = cmd_set_alloc(DEF_CMD_NUM / 2, DEF_WRITE_SIZE);
	if (!set)
		return -ENOMEM;
	
	cmd_set_init_buffer(set);

	while (loop < DEF_LOOP_NUM) {
		ret = round_read_write(priv, set);
		if (ret < 0) {
			pr_err("loop %d fail!\n", loop);
			break;
		}
		pr_info("loop %d pass!\n", loop);
	}

	cmd_set_release(set);
	return ret;
}

/**
 * @note
 *   1. Each group submit read/write cmd
 *   2. Each group random submit compare/verify cmd
 */
static int subcase_rwcv(struct case_data *priv)
{
	struct cmd_set *set = NULL;
	int loop = 0;
	int ret;

	set = cmd_set_alloc(DEF_CMD_NUM / 2, DEF_WRITE_SIZE);
	if (!set)
		return -ENOMEM;
	
	cmd_set_init_buffer(set);

	while (loop < DEF_LOOP_NUM) {
		ret = round_rwcv(priv, set);
		if (ret < 0) {
			pr_err("loop %d fail!\n", loop);
			break;
		}
		pr_info("loop %d pass!\n", loop);
	}

	cmd_set_release(set);
	return ret;
}

/**
 * @note Each group random submit read/write/compare/verify/copy cmd
 */
static int subcase_rwcvc(struct case_data *priv)
{
	struct cmd_set *set = NULL;
	int loop = 0;
	int ret;

	set = cmd_set_alloc(DEF_CMD_NUM, DEF_WRITE_SIZE);
	if (!set)
		return -ENOMEM;
	
	cmd_set_init_buffer(set);

	while (loop < DEF_LOOP_NUM) {
		ret = round_rwcvc(priv, set);
		if (ret < 0) {
			pr_err("loop %d fail!\n", loop);
			break;
		}
		pr_info("loop %d pass!\n", loop);
	}

	cmd_set_release(set);
	return ret;
}

static int case_rw_consistency(struct nvme_tool *tool, struct case_data *priv)
{
	int ret = 0;

	ret |= subcase_read_write(priv);
	ret |= subcase_rwcv(priv);
	ret |= subcase_rwcvc(priv);

	return ret;
}
NVME_CASE_SYMBOL(case_rw_consistency, "?");
