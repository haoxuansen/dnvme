/**
 * @file test_mix.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief Miscellaneous Cases
 * @version 0.1
 * @date 2023-07-20
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#include "compiler.h"
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

/**
 * @return 0 if bus master disabled, 1 if bus master enabled. 
 * 	Otherwise a negative errno.
 */
static int __unused check_bus_master(int fd, bool enable)
{
	uint16_t cmd;
	int ret;

	ret = pci_hd_read_command(fd, &cmd);
	if (ret < 0)
		return ret;
	
	return (cmd & PCI_COMMAND_MASTER) ? 1 : 0;
}

/**
 * @brief Enable or disable bus master
 * 
 * @return 0 on success, otherwise a negative errno.
 */
static int enable_bus_master(int fd, bool enable)
{
	uint16_t cmd;
	int ret;

	ret = pci_hd_read_command(fd, &cmd);
	if (ret < 0)
		return ret;
	
	if (enable && !(cmd & PCI_COMMAND_MASTER)) {
		cmd |= PCI_COMMAND_MASTER;
	} else if (!enable && (cmd & PCI_COMMAND_MASTER)) {
		cmd &= ~PCI_COMMAND_MASTER;
	} else {
		pr_debug("bus master already %s!\n",
			enable ? "enabled" : "disabled");
		return 0;
	}

	return pci_hd_write_command(fd, cmd);
}

static int submit_io_read_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
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
	wrap.buf = tool->rbuf;
	wrap.size = wrap.nlb * test->lbads;

	BUG_ON(wrap.size > tool->rbuf_size);

	memset(wrap.buf, 0, wrap.size);

	return nvme_cmd_io_read(ndev->fd, &wrap);
}

static int submit_io_write_cmd(struct nvme_tool *tool, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_rwc_wrapper wrap = {0};
	struct test_data *test = &g_test;
	uint32_t i;

	wrap.sqid = sq->sqid;
	wrap.cqid = sq->cqid;
	wrap.nsid = test->nsid;
	wrap.slba = slba;
	wrap.nlb = nlb;
	wrap.buf = tool->wbuf;
	wrap.size = wrap.nlb * test->lbads;

	BUG_ON(wrap.size > tool->wbuf_size);

	for (i = 0; i < wrap.size; i+= 4) {
		*(uint32_t *)(wrap.buf + i) = (uint32_t)rand();
	}

	return nvme_cmd_io_write(ndev->fd, &wrap);
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

/**
 * @brief Disable bus master while reading or writing
 * 
 * @return 0 on success, otherwise a negative errno. 
 */
static int case_disable_bus_master(struct nvme_tool *tool, struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	struct nvme_cq_info *cq;
	struct nvme_completion entry = {0};
	struct test_data *test = &g_test;
	struct timeval start, end;
	uint64_t slba;
	uint32_t nlb = rand() % 256 + 1; /* 1~256 */
	uint32_t num = sq->nr_entry / 4;
	uint32_t timeout = 0;
	uint16_t cid;
	uint32_t i;
	int ret;

	ret = init_test_data(ndev, test);
	if (ret < 0)
		return ret;

	slba = rand() % (test->nsze / 2);

	cq = nvme_find_iocq_info(ndev, sq->cqid);
	if (!cq) {
		pr_err("failed to find iocq(%u)!\n", sq->cqid);
		return -ENODEV;
	}

	ret = create_ioq(ndev, sq, cq);
	if (ret < 0)
		return ret;

	pr_debug("create iosq(%u) and iocq(%u) ok!\n", sq->sqid, sq->cqid);

	/*
	 * Counts the time spent in command processing.
	 */
	sq->cmd_cnt = 0;

	for (i = 0; i < num; i++) {
		ret = submit_io_write_cmd(tool, sq, slba, nlb);
		ret |= submit_io_read_cmd(tool, sq, slba, nlb);
		if (ret < 0)
			goto out;
		
		sq->cmd_cnt += 2;
	}

	ret = gettimeofday(&start, NULL);
	if (ret < 0) {
		pr_err("failed to get first timestamp!\n");
		goto out;
	}

	ret = nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
	if (ret < 0)
		goto out;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, sq->cqid, sq->cmd_cnt, 
		tool->entry, tool->entry_size);
	if (ret != sq->cmd_cnt) {
		pr_err("expect reap %u, actual reaped %d!\n", sq->cmd_cnt, ret);
		ret = ret < 0 ? ret : -ETIME;
		goto out;
	}

	ret = gettimeofday(&end, NULL);
	if (ret < 0) {
		pr_err("failed to get end timestamp!\n");
		goto out;
	}

	if (end.tv_sec == start.tv_sec) {
		timeout = 2000; /* ms */
	} else {
		timeout = (end.tv_sec - start.tv_sec) * 2000;
	}
	pr_debug("set timeout %ums\n", timeout);

	/*
	 * Controller deal I/O cmds while bus master disabled.
	 */
	sq->cmd_cnt = 0;

	for (i = 0; i < num; i++) {
		ret = submit_io_write_cmd(tool, sq, slba, nlb);
		ret |= submit_io_read_cmd(tool, sq, slba, nlb);
		if (ret < 0)
			goto out;
		
		sq->cmd_cnt += 2;
	}

	ret = nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
	if (ret < 0)
		goto out;
	
	ret = enable_bus_master(ndev->fd, false);
	if (ret < 0)
		goto out;

	ret = nvme_gnl_cmd_reap_cqe_timeout(ndev, sq->cqid, sq->cmd_cnt, 
		tool->entry, tool->entry_size, timeout);
	if (ret == sq->cmd_cnt) {
		pr_err("actual reaped shall not equal to expect num "
			"under bus master disabled!\n");
		ret = -EPERM;
		goto out;
	}
	pr_debug("expect reap %u, actual reaped %d!\n", sq->cmd_cnt, ret);

	/*
	 * The controller not respond to some cmds with CQE after bus master 
	 * disabled. The driver doesn't know what happened, and still records
	 * these cmd nodes in list.
	 * 
	 * Notify the driver to delete cmd nodes. 
	 */
	ret = nvme_empty_sq_cmdlist(ndev->fd, sq->sqid);
	if (ret < 0)
		goto out;

	/*
	 * Recovery bus master and check cmd is being processed properly
	 */
	ret = enable_bus_master(ndev->fd, true);
	if (ret < 0)
		goto out;

	ret = submit_io_write_cmd(tool, sq, slba, nlb);
	if (ret < 0)
		goto out;
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, sq->sqid);
	if (ret < 0)
		goto out;

	ret = nvme_gnl_cmd_reap_cqe(ndev, sq->cqid, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		ret = ret < 0 ? ret : -ETIME;
		goto out;
	}

	ret = nvme_valid_cq_entry(&entry, sq->sqid, cid, NVME_SC_SUCCESS);
	if (ret < 0)
		goto out;

out:
	delete_ioq(ndev, sq, cq);
	return ret;
}
NVME_CASE_SYMBOL(case_disable_bus_master, 
	"Disable bus master while reading or writing");
