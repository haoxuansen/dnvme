#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "dnvme_ioctl.h"
#include "queue.h"

#include "common.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"

static int test_flag = SUCCEED;

static char *disp_this_case = "this case will tests creat IO sq with different q size\n"
                              "each q(1-8) will be tests in this case,\n"
                              "1. Full cmd with q_size, range: 2,3,4,16,32,64,128,512,1024,2048,4096,8192,16384,32768,60000,65534\n"
                              "2. Send cmd 2 times, first + scend > sq_size. test sq size is 8,32,128,1024,4096,8192\n"
                              "default write/read buffer is 4k (nlb = 8).\n";

static void test_sub(void)
{
    uint32_t q_index = 0;
    uint32_t qsize_index = 0;

    uint32_t index = 0;
    uint32_t cmd_cnt = 0;
    uint64_t wr_slba = 0;
    uint16_t wr_nlb = 8;
    uint32_t wr_nsid = 1;

    uint32_t io_sq_id = 1;
    uint32_t io_cq_id = 1;
    uint32_t cq_size = 4096;
    uint32_t sq_size = 4096;
    uint32_t q_size[] = {3, 4, 16, 32, 64, 128, 512, 1024, 2048, 4096, 8192, 16384, 32768, 60000, 65534};

    uint32_t test2_sq_size[] = {8, 32, 128, 1024, 4096, 8192};
    uint32_t test2_sq_1_send[] = {5, 15, 60, 550, 2500, 5000};
    uint32_t test2_sq_2_send[] = {7, 30, 80, 650, 3500, 4000};
    uint32_t test2_cq_size[] = {11, 45, 140, 1200, 4000, 9000};

    struct create_cq_parameter cq_parameter = {0};
    struct create_sq_parameter sq_parameter = {0};
    uint32_t reap_num = 0;

    /*******************************************************************************************************************************/
    /*******************************************************************************************************************************/
    pr_info("\n1. Full cmd with q_size, range: 2,3,4,16,32,64,128,512,1024,2048,4096,8192,16384,32768,60000,65534\n");
    io_cq_id = 1;
    io_sq_id = 1;

    cq_parameter.contig = 1;
    cq_parameter.irq_en = 1;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;

    for (qsize_index = 0; qsize_index < (ARRAY_SIZE(q_size)); qsize_index++)
    {
        cq_size = q_size[qsize_index];
        sq_size = q_size[qsize_index];
        cq_parameter.cq_size = cq_size;
        sq_parameter.sq_size = sq_size;
        for (q_index = 1; q_index <= g_nvme_dev.max_sq_num; q_index++)
        {
            /**********************************************************************/
            io_cq_id = q_index;
            cq_parameter.irq_no = io_cq_id;
            cq_parameter.cq_id = io_cq_id;
            test_flag |= create_iocq(g_fd, &cq_parameter);
            test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
            test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
            pr_debug("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

            /**********************************************************************/
            io_sq_id = q_index;
            sq_parameter.cq_id = io_cq_id;
            sq_parameter.sq_id = io_sq_id;
            test_flag |= create_iosq(g_fd, &sq_parameter);
            test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
            test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
            pr_debug("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

            /**********************************************************************/
            //fill full cmd with q
            wr_slba = 0;
            wr_nlb = 8;
            cmd_cnt = 0;
            for (index = 0; index < (uint32_t)(sq_parameter.sq_size - 1) / 2; index++)
            {
                // pr_info("cmd_cnt:%d\n", cmd_cnt);
                test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf);
                cmd_cnt++;
                test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_read_buf);
                cmd_cnt++;

                //wr_slba += wr_nlb;
            }
            pr_info("q_id:%d, q_size:%d(full cmd), cmd_cnt:%d\n", q_index, sq_size, cmd_cnt);

            test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
            pr_debug("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);

            /**********************************************************************/
            test_flag |= ioctl_delete_ioq(g_fd, nvme_admin_delete_sq, io_sq_id);
            test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
            test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
            pr_debug("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

            test_flag |= ioctl_delete_ioq(g_fd, nvme_admin_delete_cq, io_cq_id);
            test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
            test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
            pr_debug("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
        }
    }
    /*******************************************************************************************************************************/
    /*******************************************************************************************************************************/
    pr_info("\n2. Send cmd 2 times, first + scend > sq_size. test sq size is 8,32,128,1024,4096,8192\n");
    io_cq_id = 1;
    io_sq_id = 1;

    cq_parameter.contig = 1;
    cq_parameter.irq_en = 1;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;

    for (qsize_index = 0; qsize_index < (ARRAY_SIZE(test2_sq_size)); qsize_index++)
    {
        cq_size = test2_cq_size[qsize_index];
        sq_size = test2_sq_size[qsize_index];
        cq_parameter.cq_size = cq_size;
        sq_parameter.sq_size = sq_size;
        for (q_index = 1; q_index <= g_nvme_dev.max_sq_num; q_index++)
        {
            /**********************************************************************/
            io_cq_id = q_index;
            cq_parameter.irq_no = io_cq_id;
            cq_parameter.cq_id = io_cq_id;
            test_flag |= create_iocq(g_fd, &cq_parameter);
            test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
            test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
            pr_debug("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

            /**********************************************************************/
            io_sq_id = q_index;
            sq_parameter.cq_id = io_cq_id;
            sq_parameter.sq_id = io_sq_id;
            test_flag |= create_iosq(g_fd, &sq_parameter);
            test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
            test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
            pr_debug("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

            /**********************************************************************/
            //first send read cmd
            wr_slba = 0;
            wr_nlb = 8;
            cmd_cnt = 0;
            for (index = 1; index <= test2_sq_1_send[qsize_index] / 2; index++)
            {
                // pr_info("cmd_cnt:%d\n", cmd_cnt);
                test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf);
                cmd_cnt++;
                test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_read_buf);
                cmd_cnt++;
                //wr_slba += wr_nlb;
            }
            pr_info("first send q_id:%d, sq_size:%d, cmd_cnt:%d\n", q_index, sq_size, cmd_cnt);

            test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
            pr_debug("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);

            /**********************************************************************/
            //second send write cmd
            wr_slba = 0;
            wr_nlb = 8;
            cmd_cnt = 0;
            for (index = 1; index <= test2_sq_2_send[qsize_index] / 2; index++)
            {
                test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf);
                cmd_cnt++;
                test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_read_buf);
                cmd_cnt++;
                // wr_slba += wr_nlb;
            }
            pr_info("second send q_id:%d, sq_size:%d, cmd_cnt:%d\n", q_index, sq_size, cmd_cnt);

            test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
            pr_debug("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);

            /**********************************************************************/
            test_flag |= ioctl_delete_ioq(g_fd, nvme_admin_delete_sq, io_sq_id);
            test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
            test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
            pr_debug("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

            test_flag |= ioctl_delete_ioq(g_fd, nvme_admin_delete_cq, io_cq_id);
            test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
            test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
            pr_debug("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
        }
    }
    /*******************************************************************************************************************************/
}

int case_queue_create_q_size(void)
{
    int test_round = 0;
    pr_info("\n********************\t %s \t********************\n", __FUNCTION__);
    pr_info("%s", disp_this_case);

    //Warning: this sub case will take a long time a round.
    for (test_round = 1; test_round <= 1; test_round++)
    {
        pr_info("\ntest_round: %d\n", test_round);
        test_sub();
    }

    if (test_flag != SUCCEED)
    {
        pr_info("%s test result: \n%s", __FUNCTION__, TEST_FAIL);
    }
    else
    {
        pr_info("%s test result: \n%s", __FUNCTION__, TEST_PASS);
    }
    return test_flag;
}
