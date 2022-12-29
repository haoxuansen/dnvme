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
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "dnvme_ioctl.h"
#include "queue.h"

#include "common.h"
#include "unittest.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "case_all.h"

static int test_flag = SUCCEED;
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

int test_3_adm_wr_cache_fua(void)
{
    uint32_t round_idx = 0;
    struct nvme_ctrl_property *prop = &g_nvme_dev.prop;
    
    cq_size = NVME_CAP_MQES(prop->cap);
    sq_size = NVME_CAP_MQES(prop->cap);

    test_loop = 2;

    pr_info("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        pr_info("\ntest cnt: %d\n", round_idx);
        for (uint32_t index = 1; index <= g_nvme_dev.max_sq_num; index++)
        {
            io_sq_id = index;
            io_cq_id = index;
            sub_case_list_exe(&sub_case_header, sub_case_list, ARRAY_SIZE(sub_case_list));
        }
        if (FAILED == test_flag)
        {
            pr_err("test_flag == FAILED\n");
            break;
        }
    }
    return test_flag;
}

static int sub_case_pre(void)
{
    pr_info("==>QID:%d\n", io_sq_id);
    pr_color(LOG_COLOR_PURPLE, "  Create contig cq_id:%d, cq_size = %d\n", io_cq_id, cq_size);
    test_flag |= nvme_create_contig_iocq(g_fd, io_cq_id, cq_size, ENABLE, io_cq_id);

    pr_color(LOG_COLOR_PURPLE, "  Create contig sq_id:%d, assoc cq_id = %d, sq_size = %d\n", io_sq_id, io_cq_id, sq_size);
    test_flag |= nvme_create_contig_iosq(g_fd, io_sq_id, io_cq_id, sq_size, MEDIUM_PRIO);
    return test_flag;
}

static int sub_case_end(void)
{
    pr_color(LOG_COLOR_PURPLE, "  Deleting SQID:%d,CQID:%d\n", io_sq_id, io_cq_id);
    test_flag |= nvme_delete_ioq(g_fd, nvme_admin_delete_sq, io_sq_id);
    test_flag |= nvme_delete_ioq(g_fd, nvme_admin_delete_cq, io_cq_id);
    return test_flag;
}

static int sub_case_disable_volatile_wc(void)
{
    test_flag |= nvme_set_feature_cmd(g_fd, 1, NVME_FEAT_VOLATILE_WC, false, 0);
    if (test_flag == FAILED)
        return test_flag;
    pr_info("NVME_FEAT_VOLATILE_WC:%d\n", false);
    test_flag |= nvme_admin_ring_dbl_reap_cq(g_fd);
    wr_nsid = 1;
    mem_set(g_write_buf, DWORD_RAND(), wr_nlb * LBA_DATA_SIZE(wr_nsid));
    cmd_cnt = 0;
    for (uint32_t i = 0; i < (DWORD_RAND() % 50 + 30); i++)
    {
        wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
        {
            test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf);
            if (test_flag == FAILED)
                return test_flag;
            cmd_cnt++;
        }
    }
    test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);
    return test_flag;
}

static int sub_case_enable_volatile_wc(void)
{
    test_flag |= nvme_set_feature_cmd(g_fd, 1, NVME_FEAT_VOLATILE_WC, true, 0);
    if (test_flag == FAILED)
        return test_flag;
    pr_info("NVME_FEAT_VOLATILE_WC:%d\n", true);
    test_flag |= nvme_admin_ring_dbl_reap_cq(g_fd);
    wr_nsid = 1;
    mem_set(g_write_buf, DWORD_RAND(), wr_nlb * LBA_DATA_SIZE(wr_nsid));
    cmd_cnt = 0;
    for (uint32_t i = 0; i < (DWORD_RAND() % 50 + 30); i++)
    {
        wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
        {
            test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf);
            if (test_flag == FAILED)
                return test_flag;
            cmd_cnt++;
        }
    }
    test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);
    return test_flag;
}
