/**
 * @file test_0_full_disk_wr.c
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

static uint32_t io_sq_id = 1;
static uint32_t io_cq_id = 1;
static uint32_t cq_size = 4096;
static uint32_t sq_size = 4096;
static uint16_t wr_nlb = 8;
static uint64_t wr_slba = 0;
static uint32_t wr_nsid = 1;
static uint8_t cmp_fg = 0;

static int sub_case_pre(void);
static int sub_case_end(void);
static int sub_case_write_order(void);
static int sub_case_write_random(void);
static int sub_case_read_order(void);
static int sub_case_read_random(void);
static int sub_case_write_read_verify(void);
static int sub_case_sgl_write_read_verify(void);

static SubCaseHeader_t sub_case_header = {
    "test_0_full_disk_wr",
    "This case will tests write/read cmd, then compare data",
    sub_case_pre,
    sub_case_end,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_write_order, "send write cmd by order"),
    SUB_CASE(sub_case_write_random, "random slba/nlb send write cmd"),
    SUB_CASE(sub_case_read_order, "send read cmd by order"),
    SUB_CASE(sub_case_read_random, "random slba/nlb send read cmd"),
    SUB_CASE(sub_case_write_read_verify, "send write/read cmd, then compare data"),
    SUB_CASE(sub_case_sgl_write_read_verify, "use sgl mode send write/read cmd, then compare data"),
};

static int test_0_full_disk_wr(struct nvme_tool *tool, struct case_data *priv)
{
	int test_flag = 0;
	uint32_t round_idx = 0;
	uint32_t test_loop = 1;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ctrl_property *prop = ctrl->prop;

    sq_size = NVME_CAP_MQES(prop->cap);
    cq_size = sq_size;
    wr_nsid = 1;
    wr_nlb = 8;
    wr_slba = 0;
    io_sq_id = io_cq_id =1;

    pr_info("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        pr_info("\nloop cnt: %d\n", round_idx);
        for (uint32_t index = 1; index <= ctrl->nr_sq; index++)
        {
            io_sq_id = index;
            io_cq_id = index;
            test_flag = sub_case_list_exe(&sub_case_header, sub_case_list, ARRAY_SIZE(sub_case_list));
	    DBG_ON(test_flag < 0);
        }
        if (test_flag)
            return -1;
    }
    return 0;
}
NVME_CASE_SYMBOL(test_0_full_disk_wr, "?");
NVME_AUTOCASE_SYMBOL(test_0_full_disk_wr);

static int sub_case_pre(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    int test_flag = 0;
    pr_info("==>QID:%d\n", io_sq_id);
    pr_color(LOG_N_PURPLE, "  Create contig cq_id:%d, cq_size = %d\n", io_cq_id, cq_size);
    test_flag |= nvme_create_contig_iocq(ndev->fd, io_cq_id, cq_size, 1, io_cq_id);
    DBG_ON(test_flag < 0);

    pr_color(LOG_N_PURPLE, "  Create contig sq_id:%d, assoc cq_id = %d, sq_size = %d\n", io_sq_id, io_cq_id, sq_size);
    test_flag |= nvme_create_contig_iosq(ndev->fd, io_sq_id, io_cq_id, sq_size, MEDIUM_PRIO);
    DBG_ON(test_flag < 0);
    return test_flag;
}
static int sub_case_end(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    int test_flag = 0;
    pr_color(LOG_N_PURPLE, "  Deleting SQID:%d,CQID:%d\n", io_sq_id, io_cq_id);
    test_flag |= nvme_delete_ioq(ndev->fd, nvme_admin_delete_sq, io_sq_id);
    DBG_ON(test_flag < 0);
    test_flag |= nvme_delete_ioq(ndev->fd, nvme_admin_delete_cq, io_cq_id);
    DBG_ON(test_flag < 0);
    return test_flag;
}

static int sub_case_write_order(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	int test_flag = 0;
	uint32_t cmd_cnt = 0;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0)
		return ret;

	wr_slba = 0;
	wr_nlb = 8;
    for (uint32_t i = 0; i < 1024; i++)
    {
        if (wr_slba + wr_nlb < nsze)
        {
            test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
	    DBG_ON(test_flag < 0);
            cmd_cnt++;
            wr_slba += wr_nlb;
        }
        else
        {
            wr_slba = 0;
        }
    }
    if (cmd_cnt > 0)
    {
        test_flag |= nvme_ring_dbl_and_reap_cq(ndev->fd, io_sq_id, io_cq_id, cmd_cnt);
	DBG_ON(test_flag < 0);
        pr_info("  cq:%d reaped ok! cmd_cnt:%d\n", io_cq_id, cmd_cnt);
    }
    return test_flag;
}

static int sub_case_write_random(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	int test_flag = 0;
	uint32_t cmd_cnt = 0;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0)
		return ret;

    for (uint32_t i = 0; i < 128; i++)
    {
        wr_slba = (uint32_t)rand() % (nsze / 2);
        wr_nlb = (uint16_t)rand() % 255 + 1;
        if (wr_slba + wr_nlb < nsze)
        {
            test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
	    DBG_ON(test_flag < 0);
            cmd_cnt++;
        }
    }
    if (cmd_cnt > 0)
    {
        test_flag |= nvme_ring_dbl_and_reap_cq(ndev->fd, io_sq_id, io_cq_id, cmd_cnt);
	DBG_ON(test_flag < 0);
        pr_info("  cq:%d reaped ok! cmd_cnt:%d\n", io_cq_id, cmd_cnt);
    }
    return test_flag;
}

static int sub_case_read_order(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	int test_flag = 0;
	uint32_t cmd_cnt = 0;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0)
		return ret;

	wr_slba = 0;
	wr_nlb = 8;
	for (uint32_t i = 0; i < 1024; i++)
	{
		if (wr_slba + wr_nlb < nsze)
		{
			test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
			DBG_ON(test_flag < 0);
			cmd_cnt++;
			wr_slba += wr_nlb;
		}
		else
		{
			wr_slba = 0;
		}
	}
	test_flag |= nvme_ring_dbl_and_reap_cq(ndev->fd, io_sq_id, io_cq_id, cmd_cnt);
	DBG_ON(test_flag < 0);
	pr_info("  cq:%d reaped ok! cmd_cnt:%d\n", io_cq_id, cmd_cnt);
	return test_flag;
}

static int sub_case_read_random(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	int test_flag = 0;
	uint32_t cmd_cnt = 0;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0)
		return ret;

	for (uint32_t i = 0; i < 128; i++)
	{
		wr_slba = (uint32_t)rand() % (nsze / 2);
		wr_nlb = (uint16_t)rand() % 255 + 1;
		if ((wr_slba + wr_nlb) < nsze)
		{
			test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
			DBG_ON(test_flag < 0);
			cmd_cnt++;
		}
	}
	if (cmd_cnt > 0)
	{
		test_flag |= nvme_ring_dbl_and_reap_cq(ndev->fd, io_sq_id, io_cq_id, cmd_cnt);
		DBG_ON(test_flag < 0);
		pr_info("  cq:%d reaped ok! cmd_cnt:%d\n", io_cq_id, cmd_cnt);
	}
	return test_flag;
}

static int sub_case_write_read_verify_1(void);
static int sub_case_write_read_verify(void)
{
    int test_flag = 0;
    for (uint32_t i = 0; i < 100; i++) {
        test_flag = sub_case_write_read_verify_1();
	DBG_ON(test_flag < 0);
    }
    return test_flag;
}

static int sub_case_write_read_verify_1(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    int test_flag = 0;
    uint32_t cmd_cnt = 0;
    #if 1
    static uint32_t patcnt;

    for (uint32_t i = 0; i < 16; i++)
    {
        for (uint32_t idx = 0; idx < (16*1024); idx += 4)
        {
            *((uint32_t *)(tool->wbuf + idx + (16*1024*i))) = ((idx<<16)|patcnt);//(uint32_t)rand();
        }
        patcnt++;
    }
    memset(tool->rbuf, 0, 16*1024*16);

    wr_slba = 0;//(uint32_t)rand() % (ndev->nss[NSIDX(wr_nsid)].nsze / 2);
    wr_nlb = 32;//(uint16_t)rand() % 255 + 1;

    cmd_cnt = 0;
    for (uint32_t i = 0; i < 16; i++)
    {
        cmd_cnt++;
        test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf+(16*1024*i));
	DBG_ON(test_flag < 0);
        if(test_flag<0)
            goto OUT;
        wr_slba += 32;
    }
    test_flag |= nvme_ring_dbl_and_reap_cq(ndev->fd, io_sq_id, io_cq_id, cmd_cnt);
    DBG_ON(test_flag < 0);
    if(test_flag<0)
        goto OUT;
    // pr_info("  cq:%d wr cnt:%d\n", io_cq_id, cmd_cnt);
    
    cmd_cnt = 0;
    wr_slba = 0;//(uint32_t)rand() % (ndev->nss[NSIDX(wr_nsid)].nsze / 2);
    wr_nlb = 32;//(uint16_t)rand() % 255 + 1;
    for (uint32_t i = 0; i < 16; i++)
    {
        cmd_cnt++;
        test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf+(16*1024*i));
	DBG_ON(test_flag < 0);
        if(test_flag<0)
            goto OUT;
        wr_slba += 32;
    }
    test_flag |= nvme_ring_dbl_and_reap_cq(ndev->fd, io_sq_id, io_cq_id, cmd_cnt);
    DBG_ON(test_flag < 0);
    if(test_flag<0)
        goto OUT;
    // pr_info("  cq:%d rd cnt:%d\n", io_cq_id, cmd_cnt);

    cmp_fg = memcmp(tool->wbuf, tool->rbuf, 16*1024*16);
    test_flag |= cmp_fg;
    if (cmp_fg != 0)
    {
        pr_info("\nwrite_buffer Data:\n");
        mem_disp(tool->wbuf, 16*1024*16);
        pr_info("\nRead_buffer Data:\n");
        mem_disp(tool->rbuf, 16*1024*16);
        //break;
    }
    #else
    for (uint32_t i = 0; i < 16; i++)
    {
        wr_slba = 0;//(uint32_t)rand() % (ndev->nss[NSIDX(wr_nsid)].nsze / 2);
        wr_nlb = 32;//(uint16_t)rand() % 255 + 1;
        if ((wr_slba + wr_nlb) < ndev->nss[NSIDX(wr_nsid)].nsze)
        {
            cmd_cnt++;
            test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
            if(test_flag<0)
                goto OUT;
            test_flag |= nvme_ring_dbl_and_reap_cq(g_fd, io_sq_id, io_cq_id, 1);
            if(test_flag<0)
                goto OUT;
            
            test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
            if(test_flag<0)
                goto OUT;
            test_flag |= nvme_ring_dbl_and_reap_cq(g_fd, io_sq_id, io_cq_id, 1);
            if(test_flag<0)
                goto OUT;

            cmp_fg = memcmp(tool->wbuf, tool->rbuf, wr_nlb * LBA_DATA_SIZE(wr_nsid));
            test_flag |= cmp_fg;
            if (cmp_fg != 0)
            {
                pr_info("[E] i:%d,wr_slba:%lx,wr_nlb:%x\n", i, wr_slba, wr_nlb);
                pr_info("\nwrite_buffer Data:\n");
                mem_disp(tool->wbuf, wr_nlb * LBA_DATA_SIZE(wr_nsid));
                pr_info("\nRead_buffer Data:\n");
                mem_disp(tool->rbuf, wr_nlb * LBA_DATA_SIZE(wr_nsid));
                //break;
            }
        }
    }
    pr_info("  cq:%d wr/rd check ok! cnt:%d\n", io_cq_id, cmd_cnt);
    #endif
    OUT:
    return test_flag;
}

static int sub_case_sgl_write_read_verify(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint8_t flags = 0;
	int test_flag = 0;
	uint32_t cmd_cnt = 0;
	uint32_t sgls = 0;
	uint64_t nsze;
	uint32_t lbads;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0)
		return ret;

	nvme_id_ns_lbads(ns_grp, wr_nsid, &lbads);
	nvme_id_ctrl_sgls(ctrl, &sgls);

	pr_info("ctrl.sgls:%#x\n", sgls);
	memset(tool->wbuf, (uint8_t)rand(), wr_nlb * lbads);
	memset(tool->rbuf, 0, wr_nlb * lbads);
	for (uint32_t i = 0; i < 128; i++)
	{
		wr_slba = (uint32_t)rand() % (nsze / 2);
		wr_nlb = (uint16_t)rand() % 255 + 1;
		if ((wr_slba + wr_nlb) < nsze)
		{
			cmd_cnt++;
			*(volatile uint32_t *)(tool->wbuf+wr_nlb * lbads - 4) = cmd_cnt;
			*(volatile uint32_t *)(tool->rbuf+wr_nlb * lbads - 4) = 0;
			if(sgls & ((1 << 0) | (1 << 1)))
				flags = NVME_CMD_SGL_METASEG;
			else
				goto SKIP;

			test_flag |= nvme_io_write_cmd(ndev->fd, flags, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
			DBG_ON(test_flag < 0);
			if(test_flag<0)
				goto OUT;
			test_flag |= nvme_ring_dbl_and_reap_cq(ndev->fd, io_sq_id, io_cq_id, 1);
			DBG_ON(test_flag < 0);
			if(test_flag<0)
				goto OUT;

			test_flag |= nvme_io_read_cmd(ndev->fd, flags, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
			DBG_ON(test_flag < 0);
			if(test_flag<0)
				goto OUT;
			test_flag |= nvme_ring_dbl_and_reap_cq(ndev->fd, io_sq_id, io_cq_id, 1);
			DBG_ON(test_flag < 0);
			if(test_flag<0)
				goto OUT;

			cmp_fg = memcmp(tool->wbuf, tool->rbuf, wr_nlb * lbads);
			test_flag |= cmp_fg;
			if (cmp_fg != 0)
			{
				pr_info("[E] i:%d,wr_slba:%lx,wr_nlb:%x\n", i, wr_slba, wr_nlb);
				pr_info("\nwrite_buffer Data:\n");
				mem_disp(tool->wbuf, wr_nlb * lbads);
				pr_info("\nRead_buffer Data:\n");
				mem_disp(tool->rbuf, wr_nlb * lbads);
				break;
			}
		}
	}
	pr_info("  cq:%d wr/rd check ok! cnt:%d\n", io_cq_id, cmd_cnt);
OUT:
	return test_flag;
SKIP:
	return SKIPED;
}
