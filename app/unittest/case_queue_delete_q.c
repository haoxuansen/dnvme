#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"

#include "test.h"
#include "unittest.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"

static uint32_t test_loop = 0;
static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;

static int sub_case_use_1_q_del_it(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_ccq_wrapper ccq_wrap = {0};
	struct nvme_csq_wrapper csq_wrap = {0};
	uint64_t nsze;
	uint32_t cmd_cnt, send_num;
	uint32_t i;
	uint16_t qid;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0) {
		pr_err("failed to get nsze!(%d)\n", ret);
		return ret;
	}

	qid = rand() % ctrl->nr_sq + 1;

	ccq_wrap.cqid = qid;
	ccq_wrap.elements = 1024;
	ccq_wrap.irq_no = qid;
	ccq_wrap.irq_en = 1;
	ccq_wrap.contig = 1;

	ret = nvme_create_iocq(ndev, &ccq_wrap);
	if (ret < 0) {
		pr_err("failed to create iocq(%u)!(%d)\n", ccq_wrap.cqid, ret);
		return ret;
	}

	csq_wrap.sqid = qid;
	csq_wrap.cqid = qid;
	csq_wrap.elements = 1024;
	csq_wrap.prio = NVME_SQ_PRIO_MEDIUM;
	csq_wrap.contig = 1;

	ret = nvme_create_iosq(ndev, &csq_wrap);
	if (ret < 0) {
		pr_err("failed to create iosq(%u)!(%d)\n", csq_wrap.sqid, ret);
		return ret;
	}

	cmd_cnt = 0;
	send_num = rand() % 200 + 100;
	for (i = 0; i < send_num; i++) {
		if (wr_slba + wr_nlb < nsze) {
			ret |= nvme_io_write_cmd(ndev->fd, 0, qid, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
			cmd_cnt++;
			ret |= nvme_io_read_cmd(ndev->fd, 0, qid, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
			cmd_cnt++;
		}
	}

	if (ret < 0) {
		pr_err("failed to submit io write/read cmd!(%d)\n", ret);
		return ret;
	}
	pr_debug("Submit %u cmds to SQ(%u)!\n", cmd_cnt, qid);

	ret = nvme_ring_sq_doorbell(ndev->fd, qid);
	if (ret < 0)
		return ret;

	ret = nvme_gnl_cmd_reap_cqe(ndev, qid, cmd_cnt, tool->entry, tool->entry_size);
	if (ret != cmd_cnt) {
		pr_err("expect reap %u, actual reaped %d!\n", cmd_cnt, ret);
		return ret < 0 ? ret : -ETIME;
	}
	pr_debug("Reaped %d from CQ(%u)!\n", ret, qid);

	ret = nvme_delete_iosq(ndev, qid);
	if (ret < 0) {
		pr_err("failed to delete iosq(%u)!(%d)\n", qid, ret);
		return ret;
	}

	ret = nvme_delete_iocq(ndev, qid);
	if (ret < 0) {
		pr_err("failed to delete iocq(%u)!(%d)\n", qid, ret);
		return ret;
	}

	return 0;
}

static int sub_case_use_3_q_del_2_use_remian_del_it(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ccq_wrapper ccq_wrap = {0};
	struct nvme_csq_wrapper csq_wrap = {0};
	uint16_t i;
	int ret;

	pr_debug("Create 3 IOCQ: 2/3/4...\n");
	ccq_wrap.elements = 1024;
	ccq_wrap.irq_en = 1;
	ccq_wrap.contig = 1;

	for (i = 2; i <= 4; i++) {
		ccq_wrap.cqid = i;
		ccq_wrap.irq_no = i;

		ret = nvme_create_iocq(ndev, &ccq_wrap);
		if (ret < 0) {
			pr_err("failed to create iocq(%u)!(%d)\n", ccq_wrap.cqid, ret);
			return ret;
		}
	}

	pr_debug("Create 3 IOSQ: 2/3/4...\n");
	csq_wrap.elements = 1024;
	csq_wrap.prio = NVME_SQ_PRIO_MEDIUM;
	csq_wrap.contig = 1;

	for (i = 2; i <= 4; i++) {
		csq_wrap.sqid = i;
		csq_wrap.cqid = i;

		ret = nvme_create_iosq(ndev, &csq_wrap);
		if (ret < 0) {
			pr_err("failed to create iosq(%u)!(%d)\n", csq_wrap.sqid, ret);
			return ret;
		}
	}

	pr_debug("Use 3 IOSQ: 2/3/4...\n");

	for (i = 2; i <= 4; i++) {
		ret = nvme_io_write_cmd(ndev->fd, 0, i, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
		if (ret < 0)
			return ret;
		ret = nvme_io_read_cmd(ndev->fd, 0, i, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
		if (ret < 0)
			return ret;
	}

	for (i = 2; i <= 4; i++) {
		ret = nvme_ring_sq_doorbell(ndev->fd, i);
		if (ret < 0)
			return ret;
	}

	for (i = 2; i <= 4; i++) {
		ret = nvme_gnl_cmd_reap_cqe(ndev, i, 2, tool->entry, tool->entry_size);
		if (ret != 2)
			return ret < 0 ? ret : -ETIME;
	}

	pr_debug("Delete 2 IOSQ & IOCQ: 2/3...\n");
	for (i = 2; i <= 3; i++) {
		ret = nvme_delete_iosq(ndev, i);
		if (ret < 0) {
			pr_err("failed to delete iosq(%u)!(%d)\n", i, ret);
			return ret;
		}
	}

	for (i = 2; i <= 3; i++) {
		ret = nvme_delete_iocq(ndev, i);
		if (ret < 0) {
			pr_err("failed to delete iocq(%u)!(%d)\n", i, ret);
			return ret;
		}
	}

	pr_debug("Use remain 1 IOSQ: 4...\n");
	ret = nvme_io_write_cmd(ndev->fd, 0, 4, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
	if (ret < 0)
		return ret;
	ret = nvme_io_read_cmd(ndev->fd, 0, 4, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
	if (ret < 0)
		return ret;
	ret = nvme_ring_sq_doorbell(ndev->fd, 4);
	if (ret < 0)
		return ret;
	ret = nvme_gnl_cmd_reap_cqe(ndev, 4, 2, tool->entry, tool->entry_size);
	if (ret != 2)
		return ret < 0 ? ret : -ETIME;

	pr_debug("Delete remain 1 IOSQ & IOCQ: 4...\n");
	ret = nvme_delete_iosq(ndev, 4);
	if (ret < 0) {
		pr_err("failed to delete iosq(%u)!(%d)\n", i, ret);
		return ret;
	}

	ret = nvme_delete_iocq(ndev, 4);
	if (ret < 0) {
		pr_err("failed to delete iocq(%u)!(%d)\n", i, ret);
		return ret;
	}

	return 0;
}

static int sub_case_del_cq_before_sq(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_ccq_wrapper ccq_wrap = {0};
	struct nvme_csq_wrapper csq_wrap = {0};
	struct nvme_completion entry = {0};
	uint64_t nsze;
	uint32_t i, cmd_cnt;
	uint16_t qid;
	uint16_t cid;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0) {
		pr_err("failed to get nsze!(%d)\n", ret);
		return ret;
	}

	/* 1. Create IOCQ first, then create IOSQ */
	qid = rand() % ctrl->nr_sq + 1;

	ccq_wrap.cqid = qid;
	ccq_wrap.elements = 1024;
	ccq_wrap.irq_no = qid;
	ccq_wrap.irq_en = 1;
	ccq_wrap.contig = 1;

	ret = nvme_create_iocq(ndev, &ccq_wrap);
	if (ret < 0) {
		pr_err("failed to create iocq(%u)!(%d)\n", ccq_wrap.cqid, ret);
		return ret;
	}

	csq_wrap.sqid = qid;
	csq_wrap.cqid = qid;
	csq_wrap.elements = 1024;
	csq_wrap.prio = NVME_SQ_PRIO_MEDIUM;
	csq_wrap.contig = 1;

	ret = nvme_create_iosq(ndev, &csq_wrap);
	if (ret < 0) {
		pr_err("failed to create iosq(%u)!(%d)\n", csq_wrap.sqid, ret);
		return ret;
	}

	/* 2. Use IOSQ & IOCQ */
	cmd_cnt = 0;
	for (i = 0; i < 5; i++) {
		if (wr_slba + wr_nlb < nsze) {
			ret |= nvme_io_write_cmd(ndev->fd, 0, qid, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
			cmd_cnt++;
			ret |= nvme_io_read_cmd(ndev->fd, 0, qid, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
			cmd_cnt++;
		}
	}

	if (ret < 0) {
		pr_err("failed to submit io write/read cmd!(%d)\n", ret);
		return ret;
	}
	pr_debug("Submit %u cmds to SQ(%u)!\n", cmd_cnt, qid);

	ret = nvme_ring_sq_doorbell(ndev->fd, qid);
	if (ret < 0)
		return ret;

	ret = nvme_gnl_cmd_reap_cqe(ndev, qid, cmd_cnt, tool->entry, tool->entry_size);
	if (ret != cmd_cnt) {
		pr_err("expect reap %u, actual reaped %d!\n", cmd_cnt, ret);
		return ret < 0 ? ret : -ETIME;
	}
	pr_debug("Reaped %d from CQ(%u)!\n", ret, qid);

	/* 3. Delete IOCQ first */
	pr_debug("Try to delete iocq first...\n");
	ret = nvme_cmd_delete_iocq(ndev->fd, qid);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	if (le16_to_cpu(entry.sq_id) != NVME_AQ_ID) {
		pr_err("CQ entry mismatch! qid:%u vs %u\n", 
			le16_to_cpu(entry.sq_id), NVME_AQ_ID);
		return -EBADSLT;
	}

	if (entry.command_id != cid) {
		pr_err("CQ entry mismatch! cid:%u vs %u\n", 
			entry.command_id, cid);
		return -EBADSLT;
	}

	if (NVME_CQE_STATUS_TO_STATE(entry.status) == 0) {
		pr_err("Delete iocq first ok???\n");
		return -EPERM;
	}
	pr_debug("Failed to delete iocq first, please delete iosq first...\n");

	/* 4. Delete IOSQ first, then delete IOCQ */
	ret = nvme_delete_iosq(ndev, qid);
	if (ret < 0) {
		pr_err("failed to delete iosq(%u)!(%d)\n", qid, ret);
		return ret;
	}

	ret = nvme_delete_iocq(ndev, qid);
	if (ret < 0) {
		pr_err("failed to delete iocq(%u)!(%d)\n", qid, ret);
		return ret;
	}

	return 0;
}

static int check_sq_head(struct nvme_completion *entries, int reaped)
{
	if (reaped <= 0)
		return -EPERM;

	pr_debug("sq_head: %u, reaped: %d\n", 
		le16_to_cpu(entries[reaped - 1].sq_head), reaped);

	if (le16_to_cpu(entries[reaped - 1].sq_head) == reaped)
		return 0;
	else
		return -EPERM;
}

static int delete_runing_cmd_queue(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sqs = ndev->iosqs;
	struct nvme_completion entry = {0};
	enum nvme_irq_type type;
	uint64_t nsze;
	uint16_t cid;
	int ret;
	uint32_t qid;
	uint32_t cmd_cnt = 0;
	uint16_t wr_nlb = 8;
	uint32_t index = 0;
	uint64_t wr_slba = 0;
	uint8_t cmd_num_per_q;
	uint8_t queue_num = ctrl->nr_sq;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0) {
		pr_err("failed to get nsze!(%d)\n", ret);
		return ret;
	}

	type = nvme_select_irq_type_random();
	ret = nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, type);
	if (ret < 0)
		return ret;

	ret = nvme_create_all_ioq(ndev, 0);
	if (ret < 0)
		return ret;

	/* use several queues */
	for (qid = 1; qid <= queue_num; qid++) {
		cmd_cnt = 0;
		cmd_num_per_q = (rand() % 12 + 1) * 10;
		wr_slba = (uint32_t)rand() % (nsze / 2);
		wr_nlb = (uint16_t)rand() % 255 + 1;
		for (index = 0; index < min_t(uint32_t, cmd_num_per_q, sqs[qid - 1].nr_entry) / 2; index++)
		{
			if ((wr_slba + wr_nlb) < nsze)
			{
				ret |= nvme_io_write_cmd(ndev->fd, 0, sqs[qid - 1].sqid, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
				cmd_cnt++;
				ret |= nvme_io_read_cmd(ndev->fd, 0, sqs[qid - 1].sqid, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
				cmd_cnt++;
			}
		}

		if (ret < 0) {
			pr_err("failed to submit io write/read cmd!(%d)\n", ret);
			return ret;
		}
		pr_debug("Submit %u cmds to SQ(%u)!\n", cmd_cnt, qid);

		ret = nvme_cmd_delete_iosq(ndev->fd, sqs[qid - 1].sqid);
		if (ret < 0)
			return ret;
		cid = ret;

		ret = nvme_ring_sq_doorbell(ndev->fd, sqs[qid - 1].sqid);
		if (ret < 0)
			return ret;
		ret = nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
		if (ret < 0)
			return ret;

		ret = nvme_gnl_cmd_reap_cqe(ndev, sqs[qid - 1].cqid, 
			cmd_cnt, tool->entry, tool->entry_size);
		if (ret != cmd_cnt) {
			pr_notice("SQ:%u, CQ:%u, reaped:%d, expect:%u\n", 
				sqs[qid - 1].sqid, 
				sqs[qid - 1].cqid,
				ret, cmd_cnt);
			ret = check_sq_head(tool->entry, ret);
			if (ret < 0)
				return ret;
		}

		ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
		if (ret != 1)
			return ret < 0 ? ret : -ETIME;

		ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, NVME_SC_SUCCESS);
		if (ret < 0)
			return ret;

		ret = nvme_delete_iocq(ndev, sqs[qid - 1].cqid);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int delete_runing_fua_cmd_queue(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sqs = ndev->iosqs;
	struct nvme_completion entry = {0};
	enum nvme_irq_type type;
	uint64_t nsze;
	uint16_t cid;
	int ret;
	uint32_t cmd_cnt = 0;
	uint16_t wr_nlb = 8;
	uint32_t index = 0;
	uint64_t wr_slba = 0;
	uint8_t cmd_num_per_q;
	uint8_t queue_num = ctrl->nr_sq;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0) {
		pr_err("failed to get nsze!(%d)\n", ret);
		return ret;
	}

	type = nvme_select_irq_type_random();
	ret = nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, type);
	if (ret < 0)
		return ret;

	ret = nvme_create_all_ioq(ndev, 0);
	if (ret < 0)
		return ret;

	for (uint32_t qid = 1; qid <= queue_num; qid++)
	{
		cmd_cnt = 0;
		cmd_num_per_q = (rand() % 12 + 1) * 10;
		wr_slba = (uint32_t)rand() % (nsze / 2);
		wr_nlb = (uint16_t)rand() % 255 + 1;
		for (index = 0; index < min_t(uint32_t, cmd_num_per_q, sqs[qid - 1].nr_entry) / 2; index++)
		{
			if ((wr_slba + wr_nlb) < nsze)
			{
				ret |= nvme_io_write_cmd(ndev->fd, 0, sqs[qid - 1].sqid, wr_nsid, wr_slba, wr_nlb, NVME_RW_FUA, tool->wbuf);
				cmd_cnt++;
			}
		}

		if (ret < 0) {
			pr_err("failed to submit io write cmd!(%d)\n", ret);
			return ret;
		}
		pr_debug("Submit %u cmds to SQ(%u)!\n", cmd_cnt, qid);

		ret = nvme_cmd_delete_iosq(ndev->fd, sqs[qid - 1].sqid);
		if (ret < 0)
			return ret;
		cid = ret;

		ret = nvme_ring_sq_doorbell(ndev->fd, sqs[qid - 1].sqid);
		if (ret < 0)
			return ret;
		ret = nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
		if (ret < 0)
			return ret;

		ret = nvme_gnl_cmd_reap_cqe(ndev, sqs[qid - 1].cqid, 
			cmd_cnt, tool->entry, tool->entry_size);
		if (ret != cmd_cnt) {
			pr_notice("SQ:%u, CQ:%u, reaped:%d, expect:%u\n", 
				sqs[qid - 1].sqid, 
				sqs[qid - 1].cqid,
				ret, cmd_cnt);
			ret = check_sq_head(tool->entry, ret);
			if (ret < 0)
				return ret;
		}

		ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
		if (ret != 1)
			return ret < 0 ? ret : -ETIME;

		ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, NVME_SC_SUCCESS);
		if (ret < 0)
			return ret;

		ret = nvme_delete_iocq(ndev, sqs[qid - 1].cqid);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int delete_runing_iocmd_queue(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sqs = ndev->iosqs;
	enum nvme_irq_type type;
	uint16_t nr_sq = ctrl->nr_sq;
	uint16_t i;
	uint32_t sqidx;
	uint16_t index;
	uint8_t cmd_num_per_q;
	uint8_t queue_num;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0) {
		pr_err("failed to get nsze!(%d)\n", ret);
		return ret;
	}

	type = nvme_select_irq_type_random();
	ret = nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, type);
	if (ret < 0)
		return ret;

	ret = nvme_create_all_ioq(ndev, 0);
	if (ret < 0)
		return ret;

	/* use several queues */

	queue_num = (uint8_t)rand() % nr_sq + 1;

	wr_slba = (uint32_t)rand() % (nsze / 2);
	wr_nlb = (uint16_t)rand() % 255 + 1;
	cmd_num_per_q = (512 / nr_sq); //(uint16_t)rand() % 150 + 10;
	for (i = 0; i < queue_num; i++)
	{
		//controller outstanding cmd num is 512
		sqs[i].cmd_cnt = 0;
		for (index = 0; index < cmd_num_per_q; index++) {
			if ((wr_slba + wr_nlb) < nsze) {
				ret |= nvme_send_iocmd(ndev->fd, 0, sqs[i].sqid, 
					wr_nsid, wr_slba, wr_nlb, tool->wbuf);
				sqs[i].cmd_cnt++;
			}
		}

		if (ret < 0) {
			pr_err("failed to submit io read/write/compare cmd!(%d)\n", ret);
			return ret;
		}
		pr_debug("Submit %u cmds to SQ(%u)!\n", 
			sqs[i].cmd_cnt, sqs[i].sqid);
	}

	for (i = 0; i < queue_num; i++) {
		ret = nvme_cmd_delete_iosq(ndev->fd, sqs[i].sqid);
		if (ret < 0) {
			pr_err("failed to submit delete iosq cmd!(%d)\n", ret);
			return ret;
		}
	}

	for (i = 0; i < queue_num; i++)
		ret |= nvme_ring_sq_doorbell(ndev->fd, sqs[i].sqid);

	ret |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	for (i = 0; i < queue_num; i++) {
		ret = nvme_gnl_cmd_reap_cqe(ndev, sqs[i].cqid, 
			sqs[i].cmd_cnt, tool->entry, tool->entry_size);
		if (ret != sqs[i].cmd_cnt) {
			pr_notice("SQ:%u, CQ:%u, reaped:%d, expect:%u\n", 
				sqs[i].sqid, 
				sqs[i].cqid,
				ret, sqs[i].cmd_cnt);
			ret = check_sq_head(tool->entry, ret);
			if (ret < 0)
				return ret;
		}
	}

	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, queue_num, 
		tool->entry, tool->entry_size);
	if (ret != queue_num)
		return ret < 0 ? ret : -ETIME;

	ret = nvme_check_cq_entries(tool->entry, queue_num);
	if (ret < 0)
		return ret;

	/* delete iocq */
	for (i = 0; i < queue_num; i++) {
		ret = nvme_cmd_delete_iocq(ndev->fd, sqs[i].cqid);
		if (ret < 0) {
			pr_err("failed to submit delete iocq cmd!(%d)\n", ret);
			return ret;
		}
	}

	ret = nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, queue_num, 
		tool->entry, tool->entry_size);
	if (ret != queue_num)
		return ret < 0 ? ret : -ETIME;

	ret = nvme_check_cq_entries(tool->entry, queue_num);
	if (ret < 0)
		return ret;

	/* delete remain queue */
	for (sqidx = queue_num; sqidx < nr_sq; sqidx++)
	{
		ret = nvme_delete_iosq(ndev, sqs[sqidx].sqid);
		if (ret < 0) {
			pr_err("failed to delete iosq(%u)!(%d)\n", 
				sqs[sqidx].sqid, ret);
			return ret;
		}
	}

	for (sqidx = queue_num; sqidx < nr_sq; sqidx++)
	{
		ret = nvme_delete_iocq(ndev, sqs[sqidx].cqid);
		if (ret < 0) {
			pr_err("failed to delete iocq(%u)!(%d)\n", 
				sqs[sqidx].cqid, ret);
			return ret;
		}
	}

	// delete_all_io_queue();
	nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSIX);

	return 0;
}

static SubCaseHeader_t sub_case_header = {
	"case_queue_delete_q",
	"This case will tests delete sq with different situation\n",
	NULL,
	NULL,
};

static SubCase_t sub_case_list[] = {
	SUB_CASE(sub_case_use_1_q_del_it, "use 1 sq, delete 1 sq"),
	SUB_CASE(sub_case_use_3_q_del_2_use_remian_del_it, "use 3 sq, delete 2 sq, use remian 1 sq, then delete it"),
	SUB_CASE(sub_case_del_cq_before_sq, "delete an iocq before iosq"),
	SUB_CASE(delete_runing_cmd_queue, "delete a queue when it's cmd is runing"),
	SUB_CASE(delete_runing_fua_cmd_queue, "delete a queue when it's fua cmd is runing"),
	SUB_CASE(delete_runing_iocmd_queue, "delete a queue when iocmd is runing"),
};

static int case_queue_delete_q(struct nvme_tool *tool, struct case_data *priv)
{
	uint32_t ret;
	uint32_t loop;

	test_loop = 50;

	pr_info("test will loop number: %d\n", test_loop);

	for (loop = 1; loop <= test_loop; loop++) {
		pr_info("test cnt: %d\n", loop);
		ret = sub_case_list_exe(&sub_case_header, sub_case_list, ARRAY_SIZE(sub_case_list));
		if (ret > 0) {
			pr_err("%u subcase failed!\n", ret);
			return -EPERM;
		}
	}
	return 0;
}
NVME_CASE_SYMBOL(case_queue_delete_q, "?");
NVME_AUTOCASE_SYMBOL(case_queue_delete_q);

