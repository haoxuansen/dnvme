/**
 * @file .c
 * @author yumeng (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2019-10-08
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "libbase.h"
#include "libnvme.h"

#include "test.h"
#include "unittest.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"

static int test_flag = 0;
static uint32_t test_loop = 0;
static uint32_t cmd_cnt = 0;
static uint32_t io_sq_id = 1;
static uint32_t io_cq_id = 1;
static uint32_t cq_size = 4096;
static uint32_t sq_size = 4096;
static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint32_t reap_num = 0;

static int sub_case_pre(void);
static int sub_case_end(void);

static int sub_case_all_ns_wr_rd_cmp(void);

static SubCaseHeader_t sub_case_header = {
    "test_6_all_ns_lbads_test",
    "This case will tests all namespace write/read, then check data",
    sub_case_pre,
    sub_case_end,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_all_ns_wr_rd_cmp, "this case will tests all namespece write->read then check data"),
};

static int test_6_all_ns_lbads_test(struct nvme_tool *tool, struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
    uint32_t round_idx = 0;

    test_loop = 2;

    pr_info("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        pr_info("\ntest cnt: %d\n", round_idx);
        for (uint32_t index = 1; index <= ctrl->nr_sq; index++)
        {
            io_sq_id = index;
            io_cq_id = index;
            sub_case_list_exe(&sub_case_header, sub_case_list, ARRAY_SIZE(sub_case_list));
        }
        if (-1 == test_flag)
        {
            pr_err("test_flag == -1\n");
            break;
        }
    }
    return test_flag;
}
NVME_CASE_SYMBOL(test_6_all_ns_lbads_test, "?");
NVME_AUTOCASE_SYMBOL(test_6_all_ns_lbads_test);

static int sub_case_pre(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;

    pr_info("==>QID:%d\n", io_sq_id);
    pr_color(LOG_N_PURPLE, "  Create contig cq_id:%d, cq_size = %d\n", io_cq_id, cq_size);
    test_flag |= nvme_create_contig_iocq(ndev->fd, io_cq_id, cq_size, 1, io_cq_id);

    pr_color(LOG_N_PURPLE, "  Create contig sq_id:%d, assoc cq_id = %d, sq_size = %d\n", io_sq_id, io_cq_id, sq_size);
    test_flag |= nvme_create_contig_iosq(ndev->fd, io_sq_id, io_cq_id, sq_size, MEDIUM_PRIO);
    return test_flag;
}
static int sub_case_end(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;

    pr_color(LOG_N_PURPLE, "  Deleting SQID:%d,CQID:%d\n", io_sq_id, io_cq_id);
    test_flag |= nvme_delete_ioq(ndev->fd, nvme_admin_delete_sq, io_sq_id);
    test_flag |= nvme_delete_ioq(ndev->fd, nvme_admin_delete_cq, io_cq_id);
    return test_flag;
}

static int sub_case_all_ns_wr_rd_cmp(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint32_t lbads;
	uint32_t nn = 0;
	int ret;

	ret = nvme_id_ns_lbads(ns_grp, wr_nsid, &lbads);
	if (ret < 0)
		return ret;

	nvme_id_ctrl_nn(ndev->ctrl, &nn);

	for (uint32_t ns_idx = 0; ns_idx < nn; ns_idx++)
	{
		wr_nsid = ns_idx + 1;
		wr_slba = 0;
		wr_nlb = (uint16_t)rand() % 32 + 1;

		mem_set(tool->wbuf, (uint32_t)rand(), wr_nlb * lbads);
		mem_set(tool->rbuf, 0, wr_nlb * lbads);

		pr_info("sq_id:%d nsid:%d lbads:%d slba:%ld nlb:%d\n", io_sq_id, 
			wr_nsid, lbads, wr_slba, wr_nlb);

		cmd_cnt = 0;
		test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
		if (test_flag == 0)
		{
			cmd_cnt++;
			test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
			test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
		}
		else
		{
			goto out;
		}

		cmd_cnt = 0;
		test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
		if (test_flag == 0)
		{
			cmd_cnt++;
			test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
			test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
		}
		else
		{
			goto out;
		}
		if (dw_cmp(tool->wbuf, tool->rbuf, wr_nlb * lbads))
		{
			test_flag |= -1;
		}
	}
out:
	return test_flag;
}
