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

static dword_t sub_case_pre(void);
static dword_t sub_case_end(void);

static dword_t sub_case_write(void);
static dword_t sub_case_read(void);
static dword_t sub_case_write_read(void);
static dword_t sub_case_write_read_2(void);

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

int case_iocmd_write_read(void)
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

uint16_t wr_nlb_arr[] = {32, 64, 128, 256};

static dword_t sub_case_write(void)
{
    uint32_t index0 = 0;
    uint32_t index1 = 0;
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
            if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
            {
                test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
                cmd_cnt++;
            }
        }
        for (index1 = (g_nvme_ns_info[0].nsze / 2); index1 <= (g_nvme_ns_info[0].nsze / 2) + 7; index1++)
        {
            wr_slba = index1;
            if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
            {
                test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
                cmd_cnt++;
            }
        }
    }
    if (cmd_cnt == 0)
        return FAILED;
    /**********************************************************************/
    test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    return test_flag;
}

static dword_t sub_case_read(void)
{
    uint32_t index0 = 0;
    uint32_t index1 = 0;
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
            if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
            {
                test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
                cmd_cnt++;
            }
        }
        for (index1 = (g_nvme_ns_info[0].nsze / 2); index1 <= (g_nvme_ns_info[0].nsze / 2) + 7; index1++)
        {
            wr_slba = index1;
            if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
            {
                test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
                cmd_cnt++;
            }
        }
    }
    if (cmd_cnt == 0)
        return FAILED;
    /**********************************************************************/
    test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    return test_flag;
}

static dword_t sub_case_write_read(void)
{
    uint32_t index0 = 0;
    uint32_t index1 = 0;
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
            if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
            {
                test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
                cmd_cnt++;
                test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
                cmd_cnt++;
            }
        }
        for (index1 = (g_nvme_ns_info[0].nsze / 2); index1 <= (g_nvme_ns_info[0].nsze / 2) + 7; index1++)
        {
            wr_slba = index1;
            if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
            {
                test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
                cmd_cnt++;
                test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
                cmd_cnt++;
            }
        }
    }
    if (cmd_cnt == 0)
        return FAILED;
    /**********************************************************************/
    test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    return test_flag;
}

static dword_t sub_case_write_read_2(void)
{
    uint32_t index0 = 0;
    uint32_t index1 = 0;
    cmd_cnt = 0;
    for (index0 = 0; index0 < 10; index0++)
    {
        wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        for (index1 = 0; index1 < (sq_size / 10); index1++)
        {
            if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
            {
                test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
                cmd_cnt++;
            }
        }
        test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
    }
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    //**************************************************
    cmd_cnt = 0;
    for (index0 = 0; index0 < 10; index0++)
    {
        wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        for (index1 = 0; index1 < (sq_size / 10); index1++)
        {
            if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
            {
                test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
                cmd_cnt++;
            }
        }
        test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
    }
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    //**************************************************
    cmd_cnt = 0;
    for (index0 = 0; index0 < 10; index0++)
    {
        wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        for (index1 = 0; index1 < (sq_size / 20); index1++)
        {
            if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
            {
                test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
                cmd_cnt++;
                test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
                cmd_cnt++;
            }
        }
        test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
    }
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    //**************************************************
    return test_flag;
}
