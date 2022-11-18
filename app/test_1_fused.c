/**
 * @file test_1_fused.c
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

#include "dnvme_interface.h"

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
static uint32_t cq_size = 4096;
static uint32_t sq_size = 4096;
static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint32_t reap_num = 0;

static dword_t sub_case_pre(void);
static dword_t sub_case_end(void);

static dword_t sub_case_cmpare_write_fused_cmd(void);

static SubCaseHeader_t sub_case_header = {
    "test_1_fused",
    "This case will tests fused cmd",
    sub_case_pre,
    sub_case_end,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_cmpare_write_fused_cmd, "send write read, then send compare&write fused cmd"),
};

int test_1_fused(void)
{
    uint32_t round_idx = 0;

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

static dword_t sub_case_pre(void)
{
    pr_info("==>QID:%d\n", io_sq_id);
    pr_color(LOG_COLOR_PURPLE, "  Create contig cq_id:%d, cq_size = %d\n", io_cq_id, cq_size);
    test_flag |= nvme_create_contig_iocq(file_desc, io_cq_id, cq_size, ENABLE, io_cq_id);

    pr_color(LOG_COLOR_PURPLE, "  Create contig sq_id:%d, assoc cq_id = %d, sq_size = %d\n", io_sq_id, io_cq_id, sq_size);
    test_flag |= nvme_create_contig_iosq(file_desc, io_sq_id, io_cq_id, sq_size, MEDIUM_PRIO);
    return test_flag;
}
static dword_t sub_case_end(void)
{
    pr_color(LOG_COLOR_PURPLE, "  Deleting SQID:%d,CQID:%d\n", io_sq_id, io_cq_id);
    test_flag |= nvme_delete_ioq(file_desc, nvme_admin_delete_sq, io_sq_id);
    test_flag |= nvme_delete_ioq(file_desc, nvme_admin_delete_cq, io_cq_id);
    return test_flag;
}

static dword_t sub_case_cmpare_write_fused_cmd(void)
{
    for (uint32_t ns_idx = 0; ns_idx < g_nvme_dev.id_ctrl.nn; ns_idx++)
    {
        wr_nsid = ns_idx + 1;
        wr_slba = 0;
        wr_nlb = WORD_RAND() % 32 + 1;

        mem_set(write_buffer, DWORD_RAND(), wr_nlb * LBA_DATA_SIZE(wr_nsid));
        mem_set(read_buffer, 0, wr_nlb * LBA_DATA_SIZE(wr_nsid));

        pr_info("sq_id:%d nsid:%d lbads:%d slba:%ld nlb:%d\n", io_sq_id, wr_nsid, LBA_DATA_SIZE(wr_nsid), wr_slba, wr_nlb);

        wr_slba = 0;
        wr_nlb = 64;

        cmd_cnt = 0;
        test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
        if (test_flag == SUCCEED)
        {
            cmd_cnt++;
            test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
        }
        else
        {
            goto out;
        }

        cmd_cnt = 0;
        test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
        if (test_flag == SUCCEED)
        {
            cmd_cnt++;
            test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
        }
        else
        {
            goto out;
        }
        pr_info("write read done!\n");

        pr_info("start send cmpare & write fused cmd\n");
        test_flag |= nvme_io_compare_cmd(file_desc, NVME_CMD_FUSE_FIRST, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
        test_flag |= nvme_io_write_cmd(file_desc, NVME_CMD_FUSE_SECOND, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
        if (test_flag == SUCCEED)
        {
            test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
            test_flag |= cq_gain(io_cq_id, 2, &reap_num);
        }
        else
        {
            goto out;
        }
        pr_info("start send cmpare & write fused done\n");
    }
out:
    return test_flag;
}