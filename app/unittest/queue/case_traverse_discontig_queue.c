/**
 * @file case_traverse_discontig_queue.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2024-06-14
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"
#include "test.h"

static int traverse_discontig_queue(struct case_data *priv, 
	struct nvme_sq_info *sq, struct nvme_cq_info *cq)
{
	uint32_t remain = sq->nr_entry;
	uint32_t every = cq->nr_entry / 2;
	uint32_t submit;

	while (remain) {
		submit = remain > every ? every : remain;

		CHK_EXPR_NUM_LT0_RTN(
			ut_submit_io_read_cmds(priv, sq, 0, 8, submit), -EIO);
		CHK_EXPR_NUM_LT0_RTN(
			ut_ring_sq_doorbell(priv, sq), -EIO);
		CHK_EXPR_NUM_LT0_RTN(
			ut_reap_cq_entry_no_check(priv, cq, submit), -ETIME);

		remain -= submit;
	}
	return 0;
}

static int case_traverse_discontig_queue(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq = NULL;
	struct case_config_effect *effect = NULL;
	uint8_t bk_sq_contig, bk_cq_contig;
	int ret;

	if (!nvme_cap_support_discontig_queue(ndev->ctrl))
		return -EOPNOTSUPP;

	ret = ut_case_alloc_config_effect(&priv->cfg);
	if (ret < 0)
		return ret;
	effect = priv->cfg.effect;
	effect->nsid = le32_to_cpu(ns_grp->act_list[0]);

	CHK_EXPR_PTR_EQ0_GOTO(nvme_find_iocq_info(ndev, sq->cqid),
		cq, ret, -ENOENT, rls_cfg);
	bk_cq_contig = cq->contig;
	bk_sq_contig = sq->contig;

	cq->contig = 0;
	cq->buf = tool->cq_buf;
	cq->buf_size = tool->cq_buf_size;
	sq->contig = 0;
	sq->buf = tool->sq_buf;
	sq->buf_size = tool->sq_buf_size;

	ret = ut_create_pair_io_queue(priv, sq, cq);
	if (ret < 0)
		goto rls_cfg;

	ret = traverse_discontig_queue(priv, sq, cq);
	if (ret < 0)
		goto del_queue;

del_queue:
	ret |= ut_delete_pair_io_queue(priv, sq, cq);

	cq->contig = bk_cq_contig;
	sq->contig = bk_sq_contig;
rls_cfg:
	ut_case_release_config_effect(&priv->cfg);
	return ret;
}
NVME_CASE_SYMBOL(case_traverse_discontig_queue, "?");
