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
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "dnvme_ioctl.h"

#include "common.h"
#include "unittest.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "case_all.h"

static uint32_t io_sq_id = 1;
static uint32_t io_cq_id = 1;
static uint32_t cq_size = 4096;
static uint32_t sq_size = 4096;
static uint16_t wr_nlb = 8;
static uint64_t wr_slba = 0;
static uint32_t wr_nsid = 1;
static uint8_t cmp_fg = 0;

static uint32_t sub_case_pre(void);
static uint32_t sub_case_end(void);
static uint32_t sub_case_write_order(void);
static uint32_t sub_case_write_random(void);
static uint32_t sub_case_read_order(void);
static uint32_t sub_case_read_random(void);
static uint32_t sub_case_write_read_verify(void);
static uint32_t sub_case_sgl_write_read_verify(void);

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

int test_0_full_disk_wr(void)
{
    int test_flag = SUCCEED;
    uint32_t round_idx = 0;
    uint32_t test_loop = 1;
    sq_size = g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes;
    cq_size = sq_size;
    wr_nsid = 1;
    wr_nlb = 8;
    wr_slba = 0;
    io_sq_id = io_cq_id =1;

    pr_info("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        pr_info("\nloop cnt: %d\n", round_idx);
        for (uint32_t index = 1; index <= g_nvme_dev.max_sq_num; index++)
        {
            io_sq_id = index;
            io_cq_id = index;
            test_flag = sub_case_list_exe(&sub_case_header, sub_case_list, ARRAY_SIZE(sub_case_list));
        }
        if (test_flag)
            return FAILED;
    }
    return SUCCEED;
}

static uint32_t sub_case_pre(void)
{
    int test_flag = SUCCEED;
    pr_info("==>QID:%d\n", io_sq_id);
    pr_color(LOG_COLOR_PURPLE, "  Create contig cq_id:%d, cq_size = %d\n", io_cq_id, cq_size);
    test_flag |= nvme_create_contig_iocq(g_fd, io_cq_id, cq_size, ENABLE, io_cq_id);

    pr_color(LOG_COLOR_PURPLE, "  Create contig sq_id:%d, assoc cq_id = %d, sq_size = %d\n", io_sq_id, io_cq_id, sq_size);
    test_flag |= nvme_create_contig_iosq(g_fd, io_sq_id, io_cq_id, sq_size, MEDIUM_PRIO);
    return test_flag;
}
static uint32_t sub_case_end(void)
{
    int test_flag = SUCCEED;
    pr_color(LOG_COLOR_PURPLE, "  Deleting SQID:%d,CQID:%d\n", io_sq_id, io_cq_id);
    test_flag |= nvme_delete_ioq(g_fd, nvme_admin_delete_sq, io_sq_id);
    test_flag |= nvme_delete_ioq(g_fd, nvme_admin_delete_cq, io_cq_id);
    return test_flag;
}

static uint32_t sub_case_write_order(void)
{
    int test_flag = SUCCEED;
    uint32_t cmd_cnt = 0;
    wr_slba = 0;
    wr_nlb = 8;
    for (uint32_t i = 0; i < 1024; i++)
    {
        if (wr_slba + wr_nlb < g_nvme_ns_info[NSIDX(wr_nsid)].nsze)
        {
            test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf);
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
        test_flag |= nvme_ring_dbl_and_reap_cq(g_fd, io_sq_id, io_cq_id, cmd_cnt);
        pr_info("  cq:%d reaped ok! cmd_cnt:%d\n", io_cq_id, cmd_cnt);
    }
    return test_flag;
}

static uint32_t sub_case_write_random(void)
{
    int test_flag = SUCCEED;
    uint32_t cmd_cnt = 0;
    for (uint32_t i = 0; i < 128; i++)
    {
        wr_slba = DWORD_RAND() % (g_nvme_ns_info[NSIDX(wr_nsid)].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        if (wr_slba + wr_nlb < g_nvme_ns_info[NSIDX(wr_nsid)].nsze)
        {
            test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf);
            cmd_cnt++;
        }
    }
    if (cmd_cnt > 0)
    {
        test_flag |= nvme_ring_dbl_and_reap_cq(g_fd, io_sq_id, io_cq_id, cmd_cnt);
        pr_info("  cq:%d reaped ok! cmd_cnt:%d\n", io_cq_id, cmd_cnt);
    }
    return test_flag;
}

static uint32_t sub_case_read_order(void)
{
    int test_flag = SUCCEED;
    uint32_t cmd_cnt = 0;
    wr_slba = 0;
    wr_nlb = 8;
    for (uint32_t i = 0; i < 1024; i++)
    {
        if (wr_slba + wr_nlb < g_nvme_ns_info[NSIDX(wr_nsid)].nsze)
        {
            test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_read_buf);
            cmd_cnt++;
            wr_slba += wr_nlb;
        }
        else
        {
            wr_slba = 0;
        }
    }
    test_flag |= nvme_ring_dbl_and_reap_cq(g_fd, io_sq_id, io_cq_id, cmd_cnt);
    pr_info("  cq:%d reaped ok! cmd_cnt:%d\n", io_cq_id, cmd_cnt);
    return test_flag;
}

static uint32_t sub_case_read_random(void)
{
    int test_flag = SUCCEED;
    uint32_t cmd_cnt = 0;
    for (uint32_t i = 0; i < 128; i++)
    {
        wr_slba = DWORD_RAND() % (g_nvme_ns_info[NSIDX(wr_nsid)].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        if ((wr_slba + wr_nlb) < g_nvme_ns_info[NSIDX(wr_nsid)].nsze)
        {
            test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_read_buf);
            cmd_cnt++;
        }
    }
    if (cmd_cnt > 0)
    {
        test_flag |= nvme_ring_dbl_and_reap_cq(g_fd, io_sq_id, io_cq_id, cmd_cnt);
        pr_info("  cq:%d reaped ok! cmd_cnt:%d\n", io_cq_id, cmd_cnt);
    }
    return test_flag;
}

static uint32_t sub_case_write_read_verify_1(void);
static uint32_t sub_case_write_read_verify(void)
{
    int test_flag = SUCCEED;
    for (uint32_t i = 0; i < 100; i++)
        test_flag = sub_case_write_read_verify_1();
    return test_flag;
}

static uint32_t sub_case_write_read_verify_1(void)
{
    int test_flag = SUCCEED;
    uint32_t cmd_cnt = 0;
    #if 1
    static uint32_t patcnt;
    // memset(g_write_buf, BYTE_RAND(), wr_nlb * LBA_DATA_SIZE(wr_nsid));
    // memset(g_read_buf, 0, wr_nlb * LBA_DATA_SIZE(wr_nsid));
    for (uint32_t i = 0; i < 16; i++)
    {
        for (uint32_t idx = 0; idx < (16*1024); idx += 4)
        {
            *((uint32_t *)(g_write_buf + idx + (16*1024*i))) = ((idx<<16)|patcnt);//DWORD_RAND();
        }
        patcnt++;
    }
    memset(g_read_buf, 0, 16*1024*16);

    wr_slba = 0;//DWORD_RAND() % (g_nvme_ns_info[NSIDX(wr_nsid)].nsze / 2);
    wr_nlb = 32;//WORD_RAND() % 255 + 1;

    cmd_cnt = 0;
    for (uint32_t i = 0; i < 16; i++)
    {
        cmd_cnt++;
        test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf+(16*1024*i));
        if(test_flag<0)
            goto OUT;
        wr_slba += 32;
    }
    test_flag |= nvme_ring_dbl_and_reap_cq(g_fd, io_sq_id, io_cq_id, cmd_cnt);
    if(test_flag<0)
        goto OUT;
    // pr_info("  cq:%d wr cnt:%d\n", io_cq_id, cmd_cnt);
    
    cmd_cnt = 0;
    wr_slba = 0;//DWORD_RAND() % (g_nvme_ns_info[NSIDX(wr_nsid)].nsze / 2);
    wr_nlb = 32;//WORD_RAND() % 255 + 1;
    for (uint32_t i = 0; i < 16; i++)
    {
        cmd_cnt++;
        test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_read_buf+(16*1024*i));
        if(test_flag<0)
            goto OUT;
        wr_slba += 32;
    }
    test_flag |= nvme_ring_dbl_and_reap_cq(g_fd, io_sq_id, io_cq_id, cmd_cnt);
    if(test_flag<0)
        goto OUT;
    // pr_info("  cq:%d rd cnt:%d\n", io_cq_id, cmd_cnt);

    cmp_fg = memcmp(g_write_buf, g_read_buf, 16*1024*16);
    test_flag |= cmp_fg;
    if (cmp_fg != SUCCEED)
    {
        pr_info("\nwrite_buffer Data:\n");
        mem_disp(g_write_buf, 16*1024*16);
        pr_info("\nRead_buffer Data:\n");
        mem_disp(g_read_buf, 16*1024*16);
        //break;
    }
    #else
    for (uint32_t i = 0; i < 16; i++)
    {
        wr_slba = 0;//DWORD_RAND() % (g_nvme_ns_info[NSIDX(wr_nsid)].nsze / 2);
        wr_nlb = 32;//WORD_RAND() % 255 + 1;
        if ((wr_slba + wr_nlb) < g_nvme_ns_info[NSIDX(wr_nsid)].nsze)
        {
            cmd_cnt++;
            test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf);
            if(test_flag<0)
                goto OUT;
            test_flag |= nvme_ring_dbl_and_reap_cq(g_fd, io_sq_id, io_cq_id, 1);
            if(test_flag<0)
                goto OUT;
            
            test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_read_buf);
            if(test_flag<0)
                goto OUT;
            test_flag |= nvme_ring_dbl_and_reap_cq(g_fd, io_sq_id, io_cq_id, 1);
            if(test_flag<0)
                goto OUT;

            cmp_fg = memcmp(g_write_buf, g_read_buf, wr_nlb * LBA_DATA_SIZE(wr_nsid));
            test_flag |= cmp_fg;
            if (cmp_fg != SUCCEED)
            {
                pr_info("[E] i:%d,wr_slba:%lx,wr_nlb:%x\n", i, wr_slba, wr_nlb);
                pr_info("\nwrite_buffer Data:\n");
                mem_disp(g_write_buf, wr_nlb * LBA_DATA_SIZE(wr_nsid));
                pr_info("\nRead_buffer Data:\n");
                mem_disp(g_read_buf, wr_nlb * LBA_DATA_SIZE(wr_nsid));
                //break;
            }
        }
    }
    pr_info("  cq:%d wr/rd check ok! cnt:%d\n", io_cq_id, cmd_cnt);
    #endif
    OUT:
    return test_flag;
}

static uint32_t sub_case_sgl_write_read_verify(void)
{
    uint8_t flags = 0;
    int test_flag = SUCCEED;
    uint32_t cmd_cnt = 0;
    pr_info("ctrl.sgls:%#x\n", g_nvme_dev.id_ctrl.sgls);
    memset(g_write_buf, BYTE_RAND(), wr_nlb * LBA_DATA_SIZE(wr_nsid));
    memset(g_read_buf, 0, wr_nlb * LBA_DATA_SIZE(wr_nsid));
    for (uint32_t i = 0; i < 128; i++)
    {
        wr_slba = DWORD_RAND() % (g_nvme_ns_info[NSIDX(wr_nsid)].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        if ((wr_slba + wr_nlb) < g_nvme_ns_info[NSIDX(wr_nsid)].nsze)
        {
            cmd_cnt++;
            MEM32_GET(g_write_buf+wr_nlb * LBA_DATA_SIZE(wr_nsid) - 4) = cmd_cnt;
            MEM32_GET(g_read_buf+wr_nlb * LBA_DATA_SIZE(wr_nsid) - 4) = 0;
            if(g_nvme_dev.id_ctrl.sgls & ((1 << 0) | (1 << 1)))
                flags = NVME_CMD_SGL_METASEG;
            else
                goto SKIP;

            test_flag |= nvme_io_write_cmd(g_fd, flags, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf);
            if(test_flag<0)
                goto OUT;
            test_flag |= nvme_ring_dbl_and_reap_cq(g_fd, io_sq_id, io_cq_id, 1);
            if(test_flag<0)
                goto OUT;
            
            test_flag |= nvme_io_read_cmd(g_fd, flags, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_read_buf);
            if(test_flag<0)
                goto OUT;
            test_flag |= nvme_ring_dbl_and_reap_cq(g_fd, io_sq_id, io_cq_id, 1);
            if(test_flag<0)
                goto OUT;

            cmp_fg = memcmp(g_write_buf, g_read_buf, wr_nlb * LBA_DATA_SIZE(wr_nsid));
            test_flag |= cmp_fg;
            if (cmp_fg != SUCCEED)
            {
                pr_info("[E] i:%d,wr_slba:%lx,wr_nlb:%x\n", i, wr_slba, wr_nlb);
                pr_info("\nwrite_buffer Data:\n");
                mem_disp(g_write_buf, wr_nlb * LBA_DATA_SIZE(wr_nsid));
                pr_info("\nRead_buffer Data:\n");
                mem_disp(g_read_buf, wr_nlb * LBA_DATA_SIZE(wr_nsid));
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
