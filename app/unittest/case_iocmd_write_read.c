#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
static uint32_t cq_size = 1024;
static uint32_t sq_size = 1024;
static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint32_t reap_num = 0;

static int sub_case_pre(void);
static int sub_case_end(void);

static int sub_case_write(void);
static int sub_case_read(void);
static int sub_case_write_read(void);
static int sub_case_write_read_2(void);

static SubCaseHeader_t sub_case_header = {
    "case_iocmd_write_read",
    "This case will tests all queue's write/read with diff slba&nlb ",
    sub_case_pre,
    sub_case_end,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_write, "tests write with nlb 1~24,32,64,128,256 match slba 0~7,max_lba/2~max_lba/2+7"),
    SUB_CASE(sub_case_read, "tests read with nlb 1~24,32,64,128,256 match slba 0~7,max_lba/2~max_lba/2+7"),
    SUB_CASE(sub_case_write_read, "tests write read with nlb 1~24,32,64,128,256 match slba 0~7,max_lba/2~max_lba/2+7"),
    SUB_CASE(sub_case_write_read_2, "tests write read with reap 1 cq, but ring 10 doorbell"),
};

static int case_iocmd_write_read(struct nvme_tool *tool)
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
NVME_CASE_SYMBOL(case_iocmd_write_read, "?");
NVME_AUTOCASE_SYMBOL(case_iocmd_write_read);

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

uint16_t wr_nlb_arr[] = {32, 64, 128, 256};

static int sub_case_write(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint64_t nsze;
	uint32_t index0 = 0;
	uint32_t index1 = 0;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0) {
		pr_err("failed to get nsze!(%d)\n", ret);
		return ret;
	}

    cmd_cnt = 0;
    for (index0 = 1; index0 <= 24 + ARRAY_SIZE(wr_nlb_arr); index0++)
    {
        if (index0 <= 24)
        {
            wr_nlb = index0;
        }
        else
        {
            wr_nlb = wr_nlb_arr[index0 - 25];
        }
        for (index1 = 0; index1 <= 7; index1++)
        {
            wr_slba = index1;
            if ((wr_slba + wr_nlb) < nsze)
            {
                test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
                cmd_cnt++;
            }
        }
        for (index1 = (nsze / 2); index1 <= (nsze / 2) + 7; index1++)
        {
            wr_slba = index1;
            if ((wr_slba + wr_nlb) < nsze)
            {
                test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
                cmd_cnt++;
            }
        }
    }
    if (cmd_cnt == 0)
        return -1;
    /**********************************************************************/
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    return test_flag;
}

static int sub_case_read(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint64_t nsze;
	uint32_t index0 = 0;
	uint32_t index1 = 0;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0) {
		pr_err("failed to get nsze!(%d)\n", ret);
		return ret;
	}

    cmd_cnt = 0;
    for (index0 = 1; index0 <= 24 + ARRAY_SIZE(wr_nlb_arr); index0++) //24+4
    {
        if (index0 <= 24)
        {
            wr_nlb = index0;
        }
        else
        {
            wr_nlb = wr_nlb_arr[index0 - 25];
        }
        for (index1 = 0; index1 <= 7; index1++)
        {
            wr_slba = index1;
            if ((wr_slba + wr_nlb) < nsze)
            {
                test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
                cmd_cnt++;
            }
        }
        for (index1 = (nsze / 2); index1 <= (nsze / 2) + 7; index1++)
        {
            wr_slba = index1;
            if ((wr_slba + wr_nlb) < nsze)
            {
                test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
                cmd_cnt++;
            }
        }
    }
    if (cmd_cnt == 0)
        return -1;
    /**********************************************************************/
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    return test_flag;
}

static int sub_case_write_read(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint64_t nsze;
	uint32_t index0 = 0;
	uint32_t index1 = 0;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0) {
		pr_err("failed to get nsze!(%d)\n", ret);
		return ret;
	}

    cmd_cnt = 0;
    for (index0 = 1; index0 <= 24 + ARRAY_SIZE(wr_nlb_arr); index0++)
    {
        if (index0 <= 24)
        {
            wr_nlb = index0;
        }
        else
        {
            wr_nlb = wr_nlb_arr[index0 - 25];
        }
        for (index1 = 0; index1 <= 7; index1++)
        {
            wr_slba = index1;
            if ((wr_slba + wr_nlb) < nsze)
            {
                test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
                cmd_cnt++;
                test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
                cmd_cnt++;
            }
        }
        for (index1 = (nsze / 2); index1 <= (nsze / 2) + 7; index1++)
        {
            wr_slba = index1;
            if ((wr_slba + wr_nlb) < nsze)
            {
                test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
                cmd_cnt++;
                test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
                cmd_cnt++;
            }
        }
    }
    if (cmd_cnt == 0)
        return -1;
    /**********************************************************************/
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    return test_flag;
}

static int sub_case_write_read_2(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint64_t nsze;
	uint32_t index0 = 0;
	uint32_t index1 = 0;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0) {
		pr_err("failed to get nsze!(%d)\n", ret);
		return ret;
	}

    cmd_cnt = 0;
    for (index0 = 0; index0 < 10; index0++)
    {
        wr_slba = (uint32_t)rand() % (nsze / 2);
        wr_nlb = (uint16_t)rand() % 255 + 1;
        for (index1 = 0; index1 < (sq_size / 10); index1++)
        {
            if ((wr_slba + wr_nlb) < nsze)
            {
                test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
                cmd_cnt++;
            }
        }
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    }
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    //**************************************************
    cmd_cnt = 0;
    for (index0 = 0; index0 < 10; index0++)
    {
        wr_slba = (uint32_t)rand() % (nsze / 2);
        wr_nlb = (uint16_t)rand() % 255 + 1;
        for (index1 = 0; index1 < (sq_size / 10); index1++)
        {
            if ((wr_slba + wr_nlb) < nsze)
            {
                test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
                cmd_cnt++;
            }
        }
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    }
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    //**************************************************
    cmd_cnt = 0;
    for (index0 = 0; index0 < 10; index0++)
    {
        wr_slba = (uint32_t)rand() % (nsze / 2);
        wr_nlb = (uint16_t)rand() % 255 + 1;
        for (index1 = 0; index1 < (sq_size / 20); index1++)
        {
            if ((wr_slba + wr_nlb) < nsze)
            {
                test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
                cmd_cnt++;
                test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
                cmd_cnt++;
            }
        }
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    }
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    //**************************************************
    return test_flag;
}
