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
#include <sys/time.h>

#include "dnvme_interface.h"

#include "common.h"
#include "unittest.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "case_all.h"

static int test_flag = SUCCEED;

//Brief & Detail description
static const char *case_brief =
    "This case will tests admin feature cmd\n";

static uint32_t test_loop = 0;

static struct timeval last_time;
static struct timeval curr_time;

static uint64_t perf_ms = 0;
static uint64_t perf_speed = 0;

static byte_t test_sub(void);
int test_4_peak_power(void)
{
    uint32_t round_idx = 0;
    pr_info("\n********************\t %s \t********************\n", __FUNCTION__);
    pr_info("%s", case_brief);

    test_loop = 1;
    pr_info("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        pr_info("\ntest cnt: %d\n", round_idx);
        test_sub();
        if (FAILED == test_flag)
        {
            pr_err("test_flag == FAILED\n");
            break;
        }
    }

    if (test_flag != SUCCEED)
        pr_info("%s test result: \n%s", __FUNCTION__, TEST_FAIL);
    else
        pr_info("%s test result: \n%s", __FUNCTION__, TEST_PASS);
    return test_flag;
}

static byte_t test_sub(void)
{
    uint32_t index = 0;
    uint32_t cmd_cnt = 0;
    uint32_t io_sq_id = 1;
    uint32_t io_cq_id = 1;
    uint32_t cq_size = 4096;
    uint32_t sq_size = 4096;
    uint64_t wr_slba = 0;
    uint16_t wr_nlb = 8;
    uint32_t wr_nsid = 1;
    struct create_cq_parameter cq_parameter = {0};
    struct create_sq_parameter sq_parameter = {0};
    uint32_t reap_num = 0;
    mem_set(write_buffer, DWORD_RAND(), wr_nlb * LBA_DAT_SIZE);
    mem_set(read_buffer, 0, wr_nlb * LBA_DAT_SIZE);
    sq_size = g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes;
    cq_size = g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes;

    for (index = 1; index <= g_nvme_dev.max_sq_num; index++)
    {
        io_sq_id = index;
        io_cq_id = index;
        pr_info("==>CQID:%d\n", io_cq_id);
        /**********************************************************************/
        pr_color(LOG_COLOR_PURPLE, "  Preparing io_cq_id %d, cq_size = %d \n", io_cq_id, cq_size);
        cq_parameter.cq_size = cq_size;
        cq_parameter.irq_en = 1;
        cq_parameter.contig = 1;
        cq_parameter.irq_no = io_cq_id;
        cq_parameter.cq_id = io_cq_id;
        test_flag |= create_iocq(file_desc, &cq_parameter);
        test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
        test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
        pr_info("  cq:%d reaped ok! reap_num:%d\n", ADMIN_QUEUE_ID, reap_num);
        /**********************************************************************/
        pr_info("==>SQID:%d\n", io_sq_id);
        pr_color(LOG_COLOR_PURPLE, "  Preparing io_sq_id %d, sq_size = %d \n", io_sq_id, sq_size);
        sq_parameter.sq_size = sq_size;
        sq_parameter.contig = 1;
        sq_parameter.sq_prio = MEDIUM_PRIO;
        sq_parameter.cq_id = io_cq_id;
        sq_parameter.sq_id = io_sq_id;
        test_flag |= create_iosq(file_desc, &sq_parameter);
        test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
        test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
        pr_info("  cq:%d reaped ok! reap_num:%d\n", ADMIN_QUEUE_ID, reap_num);
        /**********************************************************************/
    }
    gettimeofday(&last_time, NULL);
    for (index = 1; index <= g_nvme_dev.max_sq_num; index++)
    {
        io_sq_id = index;
        cmd_cnt = 0;
        wr_slba = 0;
        wr_nlb = 4096;
        for (uint32_t i = 0; i < 1000; i++)
        {
            test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
            cmd_cnt++;
        }
    }
    for (index = 1; index <= g_nvme_dev.max_sq_num; index++)
    {
        io_sq_id = index;
        test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
    }
    for (index = 1; index <= g_nvme_dev.max_sq_num; index++)
    {
        io_cq_id = index;
        test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    }
    gettimeofday(&curr_time, NULL);
    perf_ms = (curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000) - (last_time.tv_sec * 1000 + last_time.tv_usec / 1000);
    perf_speed = g_nvme_dev.max_sq_num * reap_num * wr_nlb / 2;
    pr_info("time:%lu ms,%lu,speed:%luMB/s\n", perf_ms, perf_speed, (perf_speed) / (perf_ms));

    for (index = 1; index <= g_nvme_dev.max_sq_num; index++)
    {
        io_sq_id = index;
        io_cq_id = index;
        //pr_color(LOG_COLOR_PURPLE, "  Deleting SQID:%x\n", io_sq_id);
        test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_sq, io_sq_id);
        test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
        test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
        //pr_info("  cq:%d reaped ok! reap_num:%d\n", ADMIN_QUEUE_ID, reap_num);
        //pr_color(LOG_COLOR_PURPLE, "  Deleting CQID:%x\n", io_cq_id);
        test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_cq, io_cq_id);
        test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
        test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
        //pr_info("  cq:%d reaped ok! reap_num:%d\n", ADMIN_QUEUE_ID, reap_num);
    }
    return test_flag;
}
