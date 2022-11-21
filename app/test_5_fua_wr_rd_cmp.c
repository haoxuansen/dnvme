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

#include "common.h"
#include "unittest.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "case_all.h"

static int test_flag = SUCCEED;
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

static dword_t sub_case_pre(void);
static dword_t sub_case_end(void);

static dword_t sub_case_write_read_compare(void);
static dword_t sub_case_fua_write_read_compare(void);
static dword_t sub_case_write_read_fua_compare(void);

static SubCaseHeader_t sub_case_header = {
    "test_5_fua_wr_rd_cmp",
    "This case will tests write/read cmd, then send compare cmd",
    sub_case_pre,
    sub_case_end,
};
//pr_color(LOG_COLOR_PURPLE, "write_fua->read->compare cmd loop:%d\n", i);

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_write_read_compare, "this case will tests write->read->compare cmd"),
    SUB_CASE(sub_case_fua_write_read_compare, "this case will tests fua_write->read->compare cmd"),
    SUB_CASE(sub_case_write_read_fua_compare, "this case will tests write->read->fua_compare cmd"),
};

int test_5_fua_wr_rd_cmp(void)
{
    uint32_t round_idx = 0;

    cq_size = g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes;
    sq_size = g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes;

    test_loop = 2;
    g_wr_nlb = 32;

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

static dword_t sub_case_write_read_compare(void)
{
    for (uint32_t i = 0; i < (((g_nvme_ns_info[0].nsze / g_wr_nlb) > (sq_size - 1)) ? (sq_size - 1) : (g_nvme_ns_info[0].nsze / g_wr_nlb)); i++)
    {
        wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
        {
            mem_set(write_buffer, DWORD_RAND(), wr_nlb * LBA_DAT_SIZE);
            mem_set(read_buffer, 0, wr_nlb * LBA_DAT_SIZE);

            /****************************************************************/
            cmd_cnt = 0;
            test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
            cmd_cnt++;
            test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
            pr_debug("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
            /****************************************************************/
            cmd_cnt = 0;
            test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
            cmd_cnt++;
            test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
            pr_debug("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);

            /****************************************************************/
            test_flag |= ioctl_send_nvme_compare(file_desc, io_sq_id, wr_slba, wr_nlb, FUA_DISABLE, read_buffer, wr_nlb * LBA_DAT_SIZE);
            test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
            test_flag |= cq_gain(io_cq_id, 1, &reap_num);
            pr_debug("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);

            //WARNING!
            test_flag |= ioctl_send_nvme_compare(file_desc, io_sq_id, wr_slba + 3, wr_nlb, FUA_DISABLE, read_buffer, wr_nlb * LBA_DAT_SIZE);
            test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
            cq_gain(io_cq_id, 1, &reap_num);
            pr_debug("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
        }
    }
    return test_flag;
}

static dword_t sub_case_fua_write_read_compare(void)
{
    for (uint32_t i = 0; i < (((g_nvme_ns_info[0].nsze / g_wr_nlb) > (sq_size - 1)) ? (sq_size - 1) : (g_nvme_ns_info[0].nsze / g_wr_nlb)); i++)
    {
        wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
        {
            mem_set(write_buffer, DWORD_RAND(), wr_nlb * LBA_DAT_SIZE);
            mem_set(read_buffer, 0, wr_nlb * LBA_DAT_SIZE);
            /****************************************************************/
            cmd_cnt = 0;
            test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, NVME_RW_FUA, write_buffer);
            cmd_cnt++;
            test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
            pr_debug("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
            /****************************************************************/
            cmd_cnt = 0;
            test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
            cmd_cnt++;
            test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
            pr_debug("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
            /****************************************************************/
            test_flag |= ioctl_send_nvme_compare(file_desc, io_sq_id, wr_slba, wr_nlb, FUA_DISABLE, read_buffer, wr_nlb * LBA_DAT_SIZE);
            test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
            test_flag |= cq_gain(io_cq_id, 1, &reap_num);
            pr_debug("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
            //WARNING!
            test_flag |= ioctl_send_nvme_compare(file_desc, io_sq_id, wr_slba + 3, wr_nlb, FUA_DISABLE, read_buffer, wr_nlb * LBA_DAT_SIZE);
            test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
            cq_gain(io_cq_id, 1, &reap_num);
            pr_debug("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
        }
    }
    return test_flag;
}

static dword_t sub_case_write_read_fua_compare(void)
{
    for (uint32_t i = 0; i < (((g_nvme_ns_info[0].nsze / g_wr_nlb) > (sq_size - 1)) ? (sq_size - 1) : (g_nvme_ns_info[0].nsze / g_wr_nlb)); i++)
    {
        wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
        {
            mem_set(write_buffer, DWORD_RAND(), wr_nlb * LBA_DAT_SIZE);
            mem_set(read_buffer, 0, wr_nlb * LBA_DAT_SIZE);
            /****************************************************************/
            cmd_cnt = 0;
            test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
            cmd_cnt++;
            test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
            pr_debug("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
            /****************************************************************/
            cmd_cnt = 0;
            test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
            cmd_cnt++;
            test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
            pr_debug("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
            /****************************************************************/
            test_flag |= ioctl_send_nvme_compare(file_desc, io_sq_id, wr_slba, wr_nlb, FUA_ENABLE, read_buffer, wr_nlb * LBA_DAT_SIZE);
            test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
            test_flag |= cq_gain(io_cq_id, 1, &reap_num);
            pr_debug("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
            //WARNING!
            test_flag |= ioctl_send_nvme_compare(file_desc, io_sq_id, wr_slba + 3, wr_nlb, FUA_ENABLE, read_buffer, wr_nlb * LBA_DAT_SIZE);
            test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
            cq_gain(io_cq_id, 1, &reap_num);
            pr_debug("  cq:%#x reaped ok! reap_num:%d\n", io_cq_id, reap_num);
        }
    }
    return test_flag;
}
