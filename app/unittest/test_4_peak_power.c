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
#include <sys/time.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"

#include "unittest.h"
#include "test.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"

static int test_flag = 0;

//Brief & Detail description
static const char *case_brief =
    "This case will tests admin feature cmd\n";

static uint32_t test_loop = 0;

static struct timeval last_time;
static struct timeval curr_time;

static uint64_t perf_ms = 0;
static uint64_t perf_speed = 0;

static uint8_t test_sub(void);
static int test_4_peak_power(struct nvme_tool *tool)
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
        if (-1 == test_flag)
        {
            pr_err("test_flag == -1\n");
            break;
        }
    }

    return test_flag != 0 ? -EPERM : 0;
}
NVME_CASE_SYMBOL(test_4_peak_power, "hc peak power test");

static uint8_t test_sub(void)
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
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ctrl_property *prop = ctrl->prop;
	struct create_cq_parameter cq_parameter = {0};
	struct create_sq_parameter sq_parameter = {0};
	uint32_t reap_num = 0;
	uint16_t nr_sq = ctrl->nr_sq;
   
    mem_set(tool->wbuf, (uint32_t)rand(), wr_nlb * LBA_DAT_SIZE);
    mem_set(tool->rbuf, 0, wr_nlb * LBA_DAT_SIZE);
    sq_size = NVME_CAP_MQES(prop->cap);
    cq_size = NVME_CAP_MQES(prop->cap);

    for (index = 1; index <= nr_sq; index++)
    {
        io_sq_id = index;
        io_cq_id = index;
        pr_info("==>CQID:%d\n", io_cq_id);
        /**********************************************************************/
        pr_color(LOG_N_PURPLE, "  Preparing io_cq_id %d, cq_size = %d \n", io_cq_id, cq_size);
        cq_parameter.cq_size = cq_size;
        cq_parameter.irq_en = 1;
        cq_parameter.contig = 1;
        cq_parameter.irq_no = io_cq_id;
        cq_parameter.cq_id = io_cq_id;
        test_flag |= create_iocq(ndev->fd, &cq_parameter);
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
        test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
        pr_info("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
        /**********************************************************************/
        pr_info("==>SQID:%d\n", io_sq_id);
        pr_color(LOG_N_PURPLE, "  Preparing io_sq_id %d, sq_size = %d \n", io_sq_id, sq_size);
        sq_parameter.sq_size = sq_size;
        sq_parameter.contig = 1;
        sq_parameter.sq_prio = MEDIUM_PRIO;
        sq_parameter.cq_id = io_cq_id;
        sq_parameter.sq_id = io_sq_id;
        test_flag |= create_iosq(ndev->fd, &sq_parameter);
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
        test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
        pr_info("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
        /**********************************************************************/
    }
    gettimeofday(&last_time, NULL);
    for (index = 1; index <= nr_sq; index++)
    {
        io_sq_id = index;
        cmd_cnt = 0;
        wr_slba = 0;
        wr_nlb = 4096;
        for (uint32_t i = 0; i < 1000; i++)
        {
            test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
            cmd_cnt++;
        }
    }
    for (index = 1; index <= nr_sq; index++)
    {
        io_sq_id = index;
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    }
    for (index = 1; index <= nr_sq; index++)
    {
        io_cq_id = index;
        test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    }
    gettimeofday(&curr_time, NULL);
    perf_ms = (curr_time.tv_sec * 1000 + curr_time.tv_usec / 1000) - (last_time.tv_sec * 1000 + last_time.tv_usec / 1000);
    perf_speed = nr_sq * reap_num * wr_nlb / 2;
    pr_info("time:%lu ms,%lu,speed:%luMB/s\n", perf_ms, perf_speed, (perf_speed) / (perf_ms));

    for (index = 1; index <= nr_sq; index++)
    {
        io_sq_id = index;
        io_cq_id = index;
        //pr_color(LOG_N_PURPLE, "  Deleting SQID:%x\n", io_sq_id);
        test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_sq, io_sq_id);
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
        test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
        //pr_info("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
        //pr_color(LOG_N_PURPLE, "  Deleting CQID:%x\n", io_cq_id);
        test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_cq, io_cq_id);
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
        test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
        //pr_info("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
    }
    return test_flag;
}
