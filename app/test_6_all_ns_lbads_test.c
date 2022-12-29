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

int test_6_all_ns_lbads_test(void)
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

static int sub_case_all_ns_wr_rd_cmp(void)
{
    for (uint32_t ns_idx = 0; ns_idx < g_nvme_dev.id_ctrl.nn; ns_idx++)
    {
        wr_nsid = ns_idx + 1;
        wr_slba = 0;
        // wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
        wr_nlb = WORD_RAND() % 32 + 1;

        mem_set(g_write_buf, DWORD_RAND(), wr_nlb * LBA_DATA_SIZE(wr_nsid));
        mem_set(g_read_buf, 0, wr_nlb * LBA_DATA_SIZE(wr_nsid));

        pr_info("sq_id:%d nsid:%d lbads:%d slba:%ld nlb:%d\n", io_sq_id, wr_nsid, LBA_DATA_SIZE(wr_nsid), wr_slba, wr_nlb);

        cmd_cnt = 0;
        test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf);
        if (test_flag == SUCCEED)
        {
            cmd_cnt++;
            test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
        }
        else
        {
            goto out;
        }

        cmd_cnt = 0;
        test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_read_buf);
        if (test_flag == SUCCEED)
        {
            cmd_cnt++;
            test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
        }
        else
        {
            goto out;
        }
        if (dw_cmp(g_write_buf, g_read_buf, wr_nlb * LBA_DATA_SIZE(wr_nsid)))
        {
            test_flag |= FAILED;
        }
    }
out:
    return test_flag;
}
