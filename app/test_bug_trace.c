/**
 * @file test_bug_trace.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2021-01-16
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include <stdio.h>
#include <stdlib.h>
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
#include "test_irq.h"
#include "test_init.h"
#include "test_bug_trace.h"

static int test_flag = SUCCEED;
static uint32_t cmd_cnt = 0;
static uint32_t io_sq_id = 1;
static uint32_t io_cq_id = 1;
static uint32_t cq_size = 4;
static uint32_t sq_size = 64;
static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint32_t reap_num = 0;

static dword_t sub_case_pre(void)
{
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

/**
 * @brief tests iocmd read csts.rdy will hang at pcie gen1/gen2
 * 
 * @return dword_t 
 */
dword_t iocmd_cstc_rdy_test(void)
{
    dword_t loop;
    cq_size = sq_size = 1024;
    io_sq_id = io_cq_id = 1;
    wr_nsid = 1;
    wr_slba = 0;
    // wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
    wr_nlb = 8; //WORD_RAND() % 32 + 1;
    pr_color(LOG_COLOR_PURPLE, "tests iocmd read csts.rdy will hang at pcie gen1/gen2 \r\n");

    pr_color(LOG_COLOR_CYAN, "pls enter loop cnt:");
    fflush(stdout);
    scanf("%d", &loop);
    sub_case_pre();
    for (dword_t cnt = 0; cnt < loop; cnt++)
    {
        if (cnt % 100 == 0)
            pr_info("cnt:%d %d%%\n\r", cnt, cnt * 100 / loop);
        cmd_cnt = 0;
        for (int iocnt = 0; iocnt < 1000; iocnt++)
        {
            test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
            cmd_cnt++;
        }
        test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
        ioctl_read_data(file_desc, NVME_REG_CSTS_OFST, 4);
        test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
        ioctl_read_data(file_desc, NVME_REG_CSTS_OFST, 4);
        cmd_cnt = 0;
        for (int iocnt = 0; iocnt < 1000; iocnt++)
        {
            test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
            cmd_cnt++;
        }
        test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
        ioctl_read_data(file_desc, NVME_REG_CSTS_OFST, 4);
        test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
        ioctl_read_data(file_desc, NVME_REG_CSTS_OFST, 4);
    }
    sub_case_end();
    return SUCCEED;
}

/**
 * @brief test for device's dbl will error bug
 * 
 * @return dword_t 
 */
dword_t reg_bug_trace(void)
{
    pr_color(LOG_COLOR_RED, "tests device's dbl will error bug \r\n");

    dword_t u32_tmp_data = ioctl_read_data(file_desc, 0x1620, 4);
    pr_color(LOG_COLOR_PURPLE, "read 0x1620: %x\n", u32_tmp_data);
    u32_tmp_data = 0xffffffff;
    for (int i = (0x1000 + (8 * 20)); i < 0x3000; i += 4)
    {
        ioctl_write_data(file_desc, i, 4, (uint8_t *)&u32_tmp_data);
    }

    u32_tmp_data = ioctl_read_data(file_desc, 0x1620, 4);
    pr_color(LOG_COLOR_PURPLE, "after write,read 0x1620: %x\n", u32_tmp_data);
    for (int i = 0; i < 0x3000; i += 4)
    {
        u32_tmp_data = ioctl_read_data(file_desc, i, 4);
        pr_color(LOG_COLOR_PURPLE, "r ofst %x=%x\n", i, u32_tmp_data);
    }
    return SUCCEED;
}