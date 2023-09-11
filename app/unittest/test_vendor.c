/**
 * @file test_vendor.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-09-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"
#include "test.h"

struct test_data {
	uint32_t	nsid;
	uint64_t	nsze;
	uint32_t	lbads;
};

static struct test_data g_test = {0};

static int init_test_data(struct nvme_dev_info *ndev, struct test_data *data)
{
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	int ret;

	/* use first active namespace as default */
	data->nsid = le32_to_cpu(ns_grp->act_list[0]);

	ret = nvme_id_ns_lbads(ns_grp, data->nsid, &data->lbads);
	if (ret < 0) {
		pr_err("failed to get lbads!(%d)\n", ret);
		return ret;
	}
	/* we have checked once, skip the check below */
	nvme_id_ns_nsze(ns_grp, data->nsid, &data->nsze);

	return 0;
}

static int create_ioq(struct nvme_dev_info *ndev, struct nvme_sq_info *sq, 
	struct nvme_cq_info *cq)
{
	struct nvme_ccq_wrapper ccq_wrap = {0};
	struct nvme_csq_wrapper csq_wrap = {0};
	int ret;

	ccq_wrap.cqid = cq->cqid;
	ccq_wrap.elements = cq->nr_entry;
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
	csq_wrap.elements = sq->nr_entry;
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

static int submit_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};
	struct test_data *test = &g_test;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = test->nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->wbuf;
	wrap.size = wrap.nlb * test->lbads;

	BUG_ON(wrap.size > tool->wbuf_size);
	fill_data_with_random(wrap.buf, wrap.size);

	return nvme_cmd_io_write(ndev->fd, &wrap);
}

static int prepare_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq)
{
	struct test_data *test = &g_test;
	/* ensure "slba + nlb <= nsze */
	uint64_t slba = rand() % (test->nsze / 2);
	uint32_t nlb = rand() % min_t(uint64_t, test->nsze / 2, 256) + 1;

	return submit_io_write_cmd(tool, sq, slba, nlb);
}

static int do_io_cmds_together(struct nvme_tool *tool, struct nvme_sq_info *sq,
	struct nvme_cq_info *cq, uint32_t cmds)
{
	struct nvme_dev_info *ndev = tool->ndev;
	uint32_t nr_cmd = min_t(uint32_t, sq->nr_entry, cmds);
	uint32_t nr_reap;
	uint32_t i;
	int ret;

	for (i = 0; i < nr_cmd; i++) {
		ret = prepare_io_write_cmd(tool, sq);
		if (ret < 0)
			return ret;
	}

	ret = nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
	if (ret < 0)
		return ret;

	do {
		nr_reap = min_t(uint32_t, cq->nr_entry, nr_cmd);

		ret = nvme_gnl_cmd_reap_cqe(ndev, sq->cqid, nr_reap, 
			tool->entry, tool->entry_size);
		if (ret != nr_reap) {
			pr_err("expect reap %u, actual reaped %d!\n", 
				nr_reap, ret);
			return ret < 0 ? ret : -ETIME;
		}
		nr_cmd -= nr_reap;
	} while (nr_cmd);

	return 0;
}

static int case_cq_full_threshold_limit_sq_fetch_cmd(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct test_data *test = &g_test;
	uint32_t nr_entry = rand() % 10 + 1;
	int ret;

	ret = init_test_data(ndev, test);
	if (ret < 0)
		return ret;

	ret = nvme_maxio_nvme_cqm_set_param(ndev, BIT(0), nr_entry);
	if (ret < 0)
		return ret;

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	if (cq->nr_entry != nr_entry)
		swap(cq->nr_entry, nr_entry);

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		goto restore;

	ret = do_io_cmds_together(tool, sq, cq, 50);
	if (ret < 0)
		goto del_ioq;

	ret = nvme_maxio_nvme_cqm_check_result(ndev, BIT(0));
	if (ret < 0)
		goto del_ioq;

del_ioq:
	ret |= delete_ioq(ndev, sq, cq);
restore:
	/* restore the value avoid affecting next test */
	if (cq->nr_entry != nr_entry)
		swap(cq->nr_entry, nr_entry);

	return ret;
}
NVME_CASE_SYMBOL(case_cq_full_threshold_limit_sq_fetch_cmd, "?");
