/**
 * @file case_perf_rw_iops.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2024-07-05
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
#include <sys/ioctl.h>

#include "libbase.h"
#include "libnvme.h"
#include "test.h"

#define TEST_QUEUE_DEPTH	1024
#define TEST_RW_SIZE		SZ_4K

static void *g_iops_buf = NULL;

static int alloc_iops_buffer(void)
{
	int ret;

	ret = posix_memalign(&g_iops_buf, SZ_4K, TEST_RW_SIZE);
	if (ret) {
		pr_err("failed to alloc iops buffer!\n");
		return -ENOMEM;
	}
	return 0;
}

static void release_iops_buffer(void)
{
	free(g_iops_buf);
	g_iops_buf = NULL;
}

static int submit_4k_cmds(struct case_data *priv, struct nvme_sq_info *sq, 
	bool write)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_config_effect effect = {0};
	uint32_t lbads;
	uint32_t nlb;
	uint32_t size = TEST_RW_SIZE;
	int ret;

	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	ret = nvme_id_ns_lbads(ns_grp, effect.nsid, &lbads);
	if (ret < 0)
		return ret;

	nlb = size / lbads;
	BUG_ON(size % lbads);

	if (write) {
		fill_data_with_incseq(g_iops_buf, size);
		effect.cmd.write.buf = g_iops_buf;
		effect.cmd.write.size = size;
	} else {
		effect.cmd.read.buf = g_iops_buf;
		effect.cmd.read.size = size;
	}
	priv->cfg.effect = &effect;

	if (write)
		ret = ut_submit_io_write_cmds(priv, sq, 0, nlb, TEST_QUEUE_DEPTH - 1);
	else
		ret = ut_submit_io_read_cmds(priv, sq, 0, nlb, TEST_QUEUE_DEPTH - 1);

	if (ret < 0) {
		pr_err("ERR-%d: submit cmds!\n", ret);
		return ret;
	}
	return 0;
}

static inline int submit_read_4k_cmds(struct case_data *priv, 
	struct nvme_sq_info *sq)
{
	return submit_4k_cmds(priv, sq, false);
}

static inline int submit_write_4k_cmds(struct case_data *priv, 
	struct nvme_sq_info *sq)
{
	return submit_4k_cmds(priv, sq, true);
}

static int ring_dbl_and_wait_for_period(struct case_data *priv, 
	struct nvme_sq_info *sq, int wait_ms, bool write)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nt_iops iops = {0};
	int ret;

	iops.sqid = sq->sqid;
	iops.cqid = sq->cqid;
	iops.time = wait_ms;

	ret = ioctl(ndev->fd, NT_IOCTL_IOPS, &iops);
	if (ret < 0) {
		pr_err("ERR-%d: do iops!\n", ret);
		return ret;
	}
	pr_info("%s Perf: %u KIOPS\n", write ? "Write" : "Read",
		iops.perf);
	return 0;
}

/**
 * @WARNING: May report "malloc(): unsorted double linked list corrupted"
 */
static int case_perf_read_iops(struct nvme_tool *tool,
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq = NULL;
	int ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq)
		return -ENOENT;
	
	sq->nr_entry = TEST_QUEUE_DEPTH;
	cq->nr_entry = TEST_QUEUE_DEPTH;

	ret = ut_create_pair_io_queue(priv, sq, cq);
	if (ret < 0)
		return ret;

	ret = alloc_iops_buffer();
	if (ret < 0)
		goto del_queue;

	ret = submit_read_4k_cmds(priv, sq);
	if (ret < 0)
		goto rls_buf;

	ret = ring_dbl_and_wait_for_period(priv, sq, 2000, false);
rls_buf:
	release_iops_buffer();
del_queue:
	ret |= ut_delete_pair_io_queue(priv, sq, NULL);
	return ret;
}
NVME_CASE_SYMBOL(case_perf_read_iops, "?");

static int case_perf_write_iops(struct nvme_tool *tool,
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq = NULL;
	int ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq)
		return -ENOENT;
	
	sq->nr_entry = TEST_QUEUE_DEPTH;
	cq->nr_entry = TEST_QUEUE_DEPTH;

	ret = ut_create_pair_io_queue(priv, sq, cq);
	if (ret < 0)
		return ret;

	ret = alloc_iops_buffer();
	if (ret < 0)
		goto del_queue;

	ret = submit_write_4k_cmds(priv, sq);
	if (ret < 0)
		goto rls_buf;

	ret = ring_dbl_and_wait_for_period(priv, sq, 2000, true);
rls_buf:
	release_iops_buffer();
del_queue:
	ret |= ut_delete_pair_io_queue(priv, sq, NULL);
	return ret;
}
NVME_CASE_SYMBOL(case_perf_write_iops, "?");
