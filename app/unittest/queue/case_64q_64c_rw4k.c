/**
 * @file case_64q_64c_rw4k.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @details
 *  1. Create 64 SQs and CQs which size is 128 entries.
 *  2. Submit 64 Cmds to each SQ.
 *  3. Ring SQs doorbell together.
 *  4. Record the time from ringing doorbell to retrieving CQ entries.
 * @version 0.1
 * @date 2024-06-20
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <sys/time.h>

#include "libbase.h"
#include "libnvme.h"
#include "test.h"

#define TEST_QUEUE_NUM	64
#define TEST_CMD_NUM	64
#define TEST_RW_SIZE	SZ_4K


static int do_64q_64c_read4k(struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_config_effect effect = {0};
	struct nvme_sq_info *sq;
	struct timeval start;
	struct timeval end;
	uint32_t lbads;
	uint32_t nlb;
	void *rbuf;
	int i;
	int ret;

	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	ret = nvme_id_ns_lbads(ns_grp, effect.nsid, &lbads);
	if (ret < 0)
		return ret;
	
	nlb = TEST_RW_SIZE / lbads;
	BUG_ON(TEST_RW_SIZE % lbads);

	ret = posix_memalign(&rbuf, SZ_4K, TEST_RW_SIZE);
	if (ret) {
		pr_err("failed to alloc read buffer!\n");
		return -ENOMEM;
	}
	effect.cmd.read.buf = rbuf;
	effect.cmd.read.size = TEST_RW_SIZE;
	priv->cfg.effect = &effect;

	for (i = 0; i < TEST_QUEUE_NUM; i++) {
		sq = &ndev->iosqs[i];
		ret = ut_submit_io_read_cmds(priv, sq, 0, nlb, TEST_CMD_NUM);
		if (ret < 0) {
			pr_err("ERR-%d: submit cmds!\n", ret);
			goto rls_rbuf;
		}
	}

	gettimeofday(&start, NULL);

	ret = ut_ring_sqs_doorbell(priv, ndev->iosqs, TEST_QUEUE_NUM);
	if (ret < 0) {
		pr_err("ERR-%d: ring doorbell!\n", ret);
		goto rls_rbuf;
	}

	for (i = 0; i < TEST_QUEUE_NUM; i++) {
		sq = &ndev->iosqs[i];
		ret = ut_reap_cq_entry_no_check_by_id(priv, sq->cqid, TEST_CMD_NUM);
		if (ret < 0) {
			pr_err("ERR-%d: reap cq!\n", ret);
			goto rls_rbuf;
		}
	}
	gettimeofday(&end, NULL);
	pr_info("Time => %ldsec, %ldus ~ %ldsec, %ldus\n", 
		start.tv_sec, start.tv_usec, end.tv_sec, end.tv_usec);

rls_rbuf:
	free(rbuf);
	return ret;
}

static int do_64q_64c_write4k(struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_config_effect effect = {0};
	struct nvme_sq_info *sq;
	struct timeval start;
	struct timeval end;
	uint32_t lbads;
	uint32_t nlb;
	void *wbuf;
	int i;
	int ret;

	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	ret = nvme_id_ns_lbads(ns_grp, effect.nsid, &lbads);
	if (ret < 0)
		return ret;
	
	nlb = TEST_RW_SIZE / lbads;
	BUG_ON(TEST_RW_SIZE % lbads);

	ret = posix_memalign(&wbuf, SZ_4K, TEST_RW_SIZE);
	if (ret) {
		pr_err("failed to alloc write buffer!\n");
		return -ENOMEM;
	}
	effect.cmd.write.buf = wbuf;
	effect.cmd.write.size = TEST_RW_SIZE;
	priv->cfg.effect = &effect;

	for (i = 0; i < TEST_QUEUE_NUM; i++) {
		sq = &ndev->iosqs[i];
		ret = ut_submit_io_write_cmds(priv, sq, 0, nlb, TEST_CMD_NUM);
		if (ret < 0) {
			pr_err("ERR-%d: submit cmds!\n", ret);
			goto rls_wbuf;
		}
	}

	gettimeofday(&start, NULL);

	ret = ut_ring_sqs_doorbell(priv, ndev->iosqs, TEST_QUEUE_NUM);
	if (ret < 0) {
		pr_err("ERR-%d: ring doorbell!\n", ret);
		goto rls_wbuf;
	}

	for (i = 0; i < TEST_QUEUE_NUM; i++) {
		sq = &ndev->iosqs[i];
		ret = ut_reap_cq_entry_no_check_by_id(priv, sq->cqid, TEST_CMD_NUM);
		if (ret < 0) {
			pr_err("ERR-%d: reap cq!\n", ret);
			goto rls_wbuf;
		}
	}
	gettimeofday(&end, NULL);
	pr_info("Time => %ldsec, %ldus ~ %ldsec, %ldus\n", 
		start.tv_sec, start.tv_usec, end.tv_sec, end.tv_usec);

rls_wbuf:
	free(wbuf);
	return ret;
}

static int case_64q_64c_rw4k(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_sq_info *sq = NULL;
	struct nvme_cq_info *cq = NULL;
	int i, ret;

	if (ctrl->nr_sq < TEST_QUEUE_NUM || ctrl->nr_cq < TEST_QUEUE_NUM) {
		pr_warn("The number of SQ or CQ is out of limit!\n");
		return -EOPNOTSUPP;
	}

	for (i = 0; i < TEST_QUEUE_NUM; i++) {
			sq = &ndev->iosqs[i];
			cq = nvme_find_iocq_info(ndev, sq->cqid);
			if (!cq)
				return -ENOENT;
			
			sq->nr_entry = 128;
			cq->nr_entry = 128;

			ret = ut_create_pair_io_queue(priv, sq, cq);
			if (ret < 0)
				goto del_queue;
	}

	ret = do_64q_64c_read4k(priv);
	ret |= do_64q_64c_write4k(priv);

del_queue:
	for (i--; i >= 0; i--) {
		sq = &ndev->iosqs[i];
		ret |= ut_delete_pair_io_queue(priv, sq, NULL);
	}
	return ret;
}
NVME_CASE_SYMBOL(case_64q_64c_rw4k, "?");
