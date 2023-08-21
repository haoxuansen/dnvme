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

#include "unittest.h"
#include "test.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"

static int test_flag = 0;
static uint16_t g_wr_nlb = 8;
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

static int sub_case_write_read_compare(void);
static int sub_case_fua_write_read_compare(void);
static int sub_case_write_read_fua_compare(void);

static SubCaseHeader_t sub_case_header = {
    "test_5_fua_wr_rd_cmp",
    "This case will tests write/read cmd, then send compare cmd",
    sub_case_pre,
    sub_case_end,
};
//pr_color(LOG_N_PURPLE, "write_fua->read->compare cmd loop:%d\n", i);

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_write_read_compare, "this case will tests write->read->compare cmd"),
    SUB_CASE(sub_case_fua_write_read_compare, "this case will tests fua_write->read->compare cmd"),
    SUB_CASE(sub_case_write_read_fua_compare, "this case will tests write->read->fua_compare cmd"),
};

static int test_5_fua_wr_rd_cmp(struct nvme_tool *tool)
{
	uint32_t round_idx = 0;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ctrl_property *prop = ctrl->prop;
	uint16_t nr_sq = ctrl->nr_sq;

    cq_size = NVME_CAP_MQES(prop->cap);
    sq_size = NVME_CAP_MQES(prop->cap);

    test_loop = 2;
    g_wr_nlb = 32;

    pr_info("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        pr_info("\ntest cnt: %d\n", round_idx);
        for (uint32_t index = 1; index <= nr_sq; index++)
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
NVME_CASE_SYMBOL(test_5_fua_wr_rd_cmp, "?");
NVME_AUTOCASE_SYMBOL(test_5_fua_wr_rd_cmp);

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

static int sub_case_write_read_compare(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0) {
		pr_err("failed to get nsze!(%d)\n", ret);
		return ret;
	}

    for (uint32_t i = 0; i < (((nsze / g_wr_nlb) > (sq_size - 1)) ? (sq_size - 1) : (nsze / g_wr_nlb)); i++)
    {
        wr_slba = (uint32_t)rand() % (nsze / 2);
        wr_nlb = (uint16_t)rand() % 255 + 1;
        if ((wr_slba + wr_nlb) < nsze)
        {
            mem_set(tool->wbuf, (uint32_t)rand(), wr_nlb * LBA_DAT_SIZE);
            mem_set(tool->rbuf, 0, wr_nlb * LBA_DAT_SIZE);

            /****************************************************************/
            cmd_cnt = 0;
            test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
            cmd_cnt++;
            test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
            pr_div("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
            /****************************************************************/
            cmd_cnt = 0;
            test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
            cmd_cnt++;
            test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
            pr_div("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);

            /****************************************************************/
            test_flag |= ioctl_send_nvme_compare(ndev->fd, io_sq_id, wr_slba, wr_nlb, FUA_DISABLE, tool->rbuf, wr_nlb * LBA_DAT_SIZE);
            test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
            test_flag |= cq_gain(io_cq_id, 1, &reap_num);
            pr_div("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);

            //WARNING!
            test_flag |= ioctl_send_nvme_compare(ndev->fd, io_sq_id, wr_slba + 3, wr_nlb, FUA_DISABLE, tool->rbuf, wr_nlb * LBA_DAT_SIZE);
            test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
            cq_gain(io_cq_id, 1, &reap_num);
            pr_div("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
        }
    }
    return test_flag;
}

static int sub_case_fua_write_read_compare(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0) {
		pr_err("failed to get nsze!(%d)\n", ret);
		return ret;
	}

    for (uint32_t i = 0; i < (((nsze / g_wr_nlb) > (sq_size - 1)) ? (sq_size - 1) : (nsze / g_wr_nlb)); i++)
    {
        wr_slba = (uint32_t)rand() % (nsze / 2);
        wr_nlb = (uint16_t)rand() % 255 + 1;
        if ((wr_slba + wr_nlb) < nsze)
        {
            mem_set(tool->wbuf, (uint32_t)rand(), wr_nlb * LBA_DAT_SIZE);
            mem_set(tool->rbuf, 0, wr_nlb * LBA_DAT_SIZE);
            /****************************************************************/
            cmd_cnt = 0;
            test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, NVME_RW_FUA, tool->wbuf);
            cmd_cnt++;
            test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
            pr_div("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
            /****************************************************************/
            cmd_cnt = 0;
            test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
            cmd_cnt++;
            test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
            pr_div("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
            /****************************************************************/
            test_flag |= ioctl_send_nvme_compare(ndev->fd, io_sq_id, wr_slba, wr_nlb, FUA_DISABLE, tool->rbuf, wr_nlb * LBA_DAT_SIZE);
            test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
            test_flag |= cq_gain(io_cq_id, 1, &reap_num);
            pr_div("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
            //WARNING!
            test_flag |= ioctl_send_nvme_compare(ndev->fd, io_sq_id, wr_slba + 3, wr_nlb, FUA_DISABLE, tool->rbuf, wr_nlb * LBA_DAT_SIZE);
            test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
            cq_gain(io_cq_id, 1, &reap_num);
            pr_div("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
        }
    }
    return test_flag;
}

static int sub_case_write_read_fua_compare(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0) {
		pr_err("failed to get nsze!(%d)\n", ret);
		return ret;
	}

    for (uint32_t i = 0; i < (((nsze / g_wr_nlb) > (sq_size - 1)) ? (sq_size - 1) : (nsze / g_wr_nlb)); i++)
    {
        wr_slba = (uint32_t)rand() % (nsze / 2);
        wr_nlb = (uint16_t)rand() % 255 + 1;
        if ((wr_slba + wr_nlb) < nsze)
        {
            mem_set(tool->wbuf, (uint32_t)rand(), wr_nlb * LBA_DAT_SIZE);
            mem_set(tool->rbuf, 0, wr_nlb * LBA_DAT_SIZE);
            /****************************************************************/
            cmd_cnt = 0;
            test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
            cmd_cnt++;
            test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
            pr_div("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
            /****************************************************************/
            cmd_cnt = 0;
            test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
            cmd_cnt++;
            test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
            pr_div("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
            /****************************************************************/
            test_flag |= ioctl_send_nvme_compare(ndev->fd, io_sq_id, wr_slba, wr_nlb, FUA_ENABLE, tool->rbuf, wr_nlb * LBA_DAT_SIZE);
            test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
            test_flag |= cq_gain(io_cq_id, 1, &reap_num);
            pr_div("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
            //WARNING!
            test_flag |= ioctl_send_nvme_compare(ndev->fd, io_sq_id, wr_slba + 3, wr_nlb, FUA_ENABLE, tool->rbuf, wr_nlb * LBA_DAT_SIZE);
            test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
            cq_gain(io_cq_id, 1, &reap_num);
            pr_div("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
        }
    }
    return test_flag;
}
