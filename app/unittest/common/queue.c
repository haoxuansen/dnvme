/**
 * @file queue.c
 * @author yeqiang_xu (yeqiang_xu@maxio-tech.com)
 * @brief 
 * @version 0.1
 * @date 2023-11-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"
#include "test.h"

#define UT_CQ_F_CHECK_STATUS		BIT(0)

int ut_create_pair_io_queue(struct case_data *priv, struct nvme_sq_info *sq,
	struct nvme_cq_info *cq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	struct nvme_ccq_wrapper ccq_wrap = {0};
	struct nvme_csq_wrapper csq_wrap = {0};
	int ret;

	if (!cq) {
		cq = nvme_find_iocq_info(ndev, sq->cqid);
		if (!cq) {
			pr_err("failed to find iocq(%u)!\n", sq->cqid);
			return -ENOMEM;
		}
	}

	ccq_wrap.cqid = cq->cqid;
	ccq_wrap.elements = cq->nr_entry;
	ccq_wrap.irq_no = cq->irq_no;
	ccq_wrap.irq_en = cq->irq_en;
	ccq_wrap.contig = cq->contig;
	if (!cq->contig) {
		ccq_wrap.buf = cq->buf;
		ccq_wrap.size = cq->nr_entry << ndev->io_cqes;

		BUG_ON(ccq_wrap.size > cq->buf_size);
	}

	ut_rpt_record_case_step(rpt, 
		"Create I/O CQ => id %u, entry %u, irq %s, num %u",
		cq->cqid, cq->nr_entry, cq->irq_en ? "enable" : "disable", 
		cq->irq_no);
	ret = nvme_create_iocq(ndev, &ccq_wrap);
	if (ret < 0) {
		pr_err("failed to create iocq:%u!(%d)\n", cq->cqid, ret);
		return ret;
	}

	csq_wrap.sqid = sq->sqid;
	csq_wrap.cqid = sq->cqid;
	csq_wrap.elements = sq->nr_entry;
	csq_wrap.prio = sq->prio;
	csq_wrap.contig = sq->contig;
	if (!sq->contig) {
		csq_wrap.buf = sq->buf;
		csq_wrap.size = sq->nr_entry << ndev->io_sqes;

		BUG_ON(csq_wrap.size > sq->buf_size);
	}

	ut_rpt_record_case_step(rpt, 
		"Create I/O SQ => id %u, entry %u, bind CQ(%u)",
		sq->sqid, sq->nr_entry, sq->cqid);
	ret = nvme_create_iosq(ndev, &csq_wrap);
	if (ret < 0) {
		pr_err("failed to create iosq:%u!(%d)\n", sq->sqid, ret);
		return ret;
	}
	return 0;
}

int ut_create_pair_io_queues(struct case_data *priv, struct nvme_sq_info **sq,
	struct nvme_cq_info **cq, int nr_pair)
{
	int ret;
	int i;

	for (i = 0; i < nr_pair; i++) {
		if (cq)
			ret = ut_create_pair_io_queue(priv, sq[i], cq[i]);
		else
			ret = ut_create_pair_io_queue(priv, sq[i], NULL);
		
		if (ret < 0)
			return ret;
	}
	return 0;
}

int ut_delete_pair_io_queue(struct case_data *priv, struct nvme_sq_info *sq,
	struct nvme_cq_info *cq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;
	uint16_t cqid = cq ? cq->cqid : sq->cqid;
	int ret;

        ut_rpt_record_case_step(rpt, "Delete I/O SQ => id %u", sq->sqid);
	ret = nvme_delete_iosq(ndev, sq->sqid);
	if (ret < 0) {
		pr_err("failed to delete iosq:%u!(%d)\n", sq->sqid, ret);
		return ret;
	}

	ut_rpt_record_case_step(rpt, "Delete I/O CQ => id %u", cqid);
	ret = nvme_delete_iocq(ndev, cqid);
	if (ret < 0) {
		pr_err("failed to delete iocq:%u!(%d)\n", cqid, ret);
		return ret;
	}
	return 0;
}

int ut_delete_pair_io_queues(struct case_data *priv, struct nvme_sq_info **sq,
	struct nvme_cq_info **cq, int nr_pair)
{
	int ret;
	int i;

	for (i = 0; i < nr_pair; i++) {
		if (cq)
			ret = ut_delete_pair_io_queue(priv, sq[i], cq[i]);
		else
			ret = ut_delete_pair_io_queue(priv, sq[i], NULL);
		
		if (ret < 0)
			return ret;
	}
	return 0;
}

int ut_ring_sq_doorbell(struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct case_report *rpt = &priv->rpt;

	ut_rpt_record_case_step(rpt, "Ring SQ:%u doorbell!", sq->sqid);
	return nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
}

int ut_ring_sqs_doorbell(struct case_data *priv, struct nvme_sq_info **sq,
	int nr_sq)
{
	int ret;
	int i;

	for (i = 0; i < nr_sq; i++) {
		ret = ut_ring_sq_doorbell(priv, sq[i]);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int ut_reap_cq_entry(struct case_data *priv, struct nvme_cq_info *cq,
	int nr_entry, uint32_t flag)
{
	struct nvme_tool *tool = priv->tool;
	struct nvme_dev_info *ndev = priv->tool->ndev;
	uint32_t nr_reap;
	int ret;

	do {
		nr_reap = min_t(uint32_t, cq->nr_entry, nr_entry);

		ret = nvme_gnl_cmd_reap_cqe(ndev, cq->cqid, nr_reap, 
			tool->entry, tool->entry_size);
		if (ret != nr_reap) {
			pr_err("expect reap %u, actual reaped %d!\n", 
				nr_reap, ret);
			return ret < 0 ? ret : -ETIME;
		}
		nr_entry -= nr_reap;

		if (flag & UT_CQ_F_CHECK_STATUS) {
			ret = nvme_check_cq_entries(tool->entry, nr_reap);
			if (ret < 0)
				return ret;
		}
	} while (nr_entry);

	return 0;
}

static int ut_reap_cq_entry_by_id(struct case_data *priv, uint16_t cqid, 
	int nr_entry, uint32_t flag)
{
	struct nvme_cq_info *cq;

	cq = nvme_find_iocq_info(priv->tool->ndev, cqid);
	if (!cq)
		return -ENODEV;

	return ut_reap_cq_entry(priv, cq, nr_entry, flag);
}

int ut_reap_cq_entry_check_status(struct case_data *priv, 
	struct nvme_cq_info *cq, int nr_entry)
{
	return ut_reap_cq_entry(priv, cq, nr_entry, UT_CQ_F_CHECK_STATUS);
}

int ut_reap_cq_entry_check_status_by_id(struct case_data *priv, 
	uint16_t cqid, int nr_entry)
{
	return ut_reap_cq_entry_by_id(priv, cqid, nr_entry, UT_CQ_F_CHECK_STATUS);
}

int ut_reap_cq_entry_no_check(struct case_data *priv, 
	struct nvme_cq_info *cq, int nr_entry)
{
	return ut_reap_cq_entry(priv, cq, nr_entry, 0);
}

int ut_reap_cq_entry_no_check_by_id(struct case_data *priv, uint16_t cqid,
	int nr_entry)
{
	return ut_reap_cq_entry_by_id(priv, cqid, nr_entry, 0);
}

