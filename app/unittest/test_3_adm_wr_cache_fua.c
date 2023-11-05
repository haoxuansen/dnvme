/**
 * @file test_3_adm_wr_cache_fua.c
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
static uint32_t cq_size = 1024;
static uint32_t sq_size = 1024;
static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint32_t reap_num = 0;

static int sub_case_pre(void);
static int sub_case_end(void);

static int sub_case_disable_volatile_wc(void);
static int sub_case_enable_volatile_wc(void);

static SubCaseHeader_t sub_case_header = {
    "test_3_adm_wr_cache_fua",
    "This case will tests wr_cache cmd for fua",
    sub_case_pre,
    sub_case_end,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_disable_volatile_wc, "send set feature cmd disable VOLATILE_WC, then send write cmd"),
    SUB_CASE(sub_case_enable_volatile_wc, "send set feature cmd enable VOLATILE_WC, then send write cmd"),
};

static int test_3_adm_wr_cache_fua(struct nvme_tool *tool, struct case_data *priv)
{
	uint32_t round_idx = 0;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ctrl_property *prop = ctrl->prop;
    
    cq_size = NVME_CAP_MQES(prop->cap);
    sq_size = NVME_CAP_MQES(prop->cap);

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
NVME_CASE_SYMBOL(test_3_adm_wr_cache_fua, "?");
NVME_AUTOCASE_SYMBOL(test_3_adm_wr_cache_fua);

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

static int sub_case_disable_volatile_wc(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint32_t lbads;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_lbads(ns_grp, wr_nsid, &lbads);
	if (ret < 0) {
		pr_err("failed to get lbads!(%d)\n", ret);
		return ret;
	}
	nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);

    test_flag |= nvme_set_feature_cmd(ndev->fd, 1, NVME_FEAT_VOLATILE_WC, false, 0);
    if (test_flag == -1)
        return test_flag;
    pr_info("NVME_FEAT_VOLATILE_WC:%d\n", false);
    test_flag |= nvme_admin_ring_dbl_reap_cq(ndev->fd);
    wr_nsid = 1;
    mem_set(tool->wbuf, (uint32_t)rand(), wr_nlb * lbads);
    cmd_cnt = 0;
    for (uint32_t i = 0; i < ((uint32_t)rand() % 50 + 30); i++)
    {
        wr_slba = (uint32_t)rand() % (nsze / 2);
        wr_nlb = (uint16_t)rand() % 255 + 1;
        if ((wr_slba + wr_nlb) < nsze)
        {
            test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
            if (test_flag == -1)
                return test_flag;
            cmd_cnt++;
        }
    }
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);
    return test_flag;
}

static int sub_case_enable_volatile_wc(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint32_t lbads;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_lbads(ns_grp, wr_nsid, &lbads);
	if (ret < 0) {
		pr_err("failed to get lbads!(%d)\n", ret);
		return ret;
	}
	nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);

    test_flag |= nvme_set_feature_cmd(ndev->fd, 1, NVME_FEAT_VOLATILE_WC, true, 0);
    if (test_flag == -1)
        return test_flag;
    pr_info("NVME_FEAT_VOLATILE_WC:%d\n", true);
    test_flag |= nvme_admin_ring_dbl_reap_cq(ndev->fd);
    wr_nsid = 1;
    mem_set(tool->wbuf, (uint32_t)rand(), wr_nlb * lbads);
    cmd_cnt = 0;
    for (uint32_t i = 0; i < ((uint32_t)rand() % 50 + 30); i++)
    {
        wr_slba = (uint32_t)rand() % (nsze / 2);
        wr_nlb = (uint16_t)rand() % 255 + 1;
        if ((wr_slba + wr_nlb) < nsze)
        {
            test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
            if (test_flag == -1)
                return test_flag;
            cmd_cnt++;
        }
    }
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);
    return test_flag;
}
