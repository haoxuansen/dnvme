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
#include "test_init.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"

static int test_flag = SUCCEED;
static uint32_t test_loop = 0;
// static uint32_t io_sq_id = 1;
// static uint32_t io_cq_id = 1;
// static uint64_t wr_slba = 0;
// static uint16_t wr_nlb = 8;
// static uint32_t wr_nsid = 1;
static uint32_t reap_num = 0;

static uint32_t sub_case_asq_size_loop_array(void);
static uint32_t sub_case_asq_size_random(void);

static SubCaseHeader_t sub_case_header = {
    "case_queue_cq_int_all",
    "this case will tests admin queue's functions\r\n",
    NULL,
    NULL,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_asq_size_loop_array, "loop all sq/cq size array, and illegal admin opcodes"),
    SUB_CASE(sub_case_asq_size_random, "random adm sq/cq size"),
};

int case_queue_admin(void)
{
    uint32_t round_idx = 0;

    test_loop = 1;

    pr_info("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        pr_info("\ntest cnt: %d\n", round_idx);
        sub_case_list_exe(&sub_case_header, sub_case_list, ARRAY_SIZE(sub_case_list));
        if (FAILED == test_flag)
        {
            pr_err("test_flag == FAILED\n");
            break;
        }
    }
    test_change_init(g_fd, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSIX, g_nvme_dev.max_sq_num + 1);
    return test_flag;
}

static uint32_t sub_case_asq_size_loop_array(void)
{
    uint32_t sq_size_idx = 0;
    uint32_t cq_size_idx = 0;
    uint32_t index = 0;
    uint32_t cmd_cnt = 0;
    uint32_t asq_size[] = {2, 64, 65, 66, 128, 129, 130, 4096};
    uint32_t acq_size[] = {2, 256, 257, 258, 512, 513, 4096};
    // uint32_t asq_size[] = {64, 65, 66, 128, 129, 130, 4096};
    // uint32_t acq_size[] = {256, 257, 258, 512, 513, 4096};

    uint32_t test2_asq_size[] = {3, 64, 66, 128, 129, 4096};
    uint32_t test2_asq_1_send[] = {2, 40, 40, 80, 80, 3000};
    uint32_t test2_asq_2_send[] = {2, 30, 40, 50, 60, 2000};
    //uint32_t test2_acq_size[] = {4, 70, 80, 130, 12, 4096};

    uint32_t admin_cq_size = 4096;
    uint32_t admin_sq_size = 4096;

    /*******************************************************************************************************************************/
    pr_info("\n1. tests qsize: asq_size range: 2,64,65,66,128,129,130,4096,acq_size range: 2,256,257,258,512,513,4096\n");
    for (cq_size_idx = 0; cq_size_idx < ARRAY_SIZE(acq_size); cq_size_idx++)
    {
        admin_cq_size = acq_size[cq_size_idx];
        for (sq_size_idx = 0; sq_size_idx < ARRAY_SIZE(asq_size); sq_size_idx++)
        {
            admin_sq_size = asq_size[sq_size_idx];
            pr_color(LOG_COLOR_GREEN, "\ncfg admin_cq_size:%d, admin_sq_size:%d\n", acq_size[cq_size_idx], asq_size[sq_size_idx]);

            test_change_init(g_fd, admin_sq_size, admin_cq_size, NVME_INT_MSIX, g_nvme_dev.max_sq_num + 1);

            cmd_cnt = 0;
            for (index = 0; index < (admin_sq_size - 1); index++)
            {
                test_flag |= keep_alive_cmd(g_fd);
                cmd_cnt++;
            }
            test_flag |= nvme_ring_sq_doorbell(g_fd, 0);
            // usleep(500000);
            nvme_msi_register_test();
            test_flag |= cq_gain(0, cmd_cnt, &reap_num);
            pr_color(LOG_COLOR_PURPLE, "  acq:%d reaped ok! reap_num:%d\n", 0, reap_num);
        }
    }
    /*******************************************************************************************************************************/
    pr_info("\n2. Send cmd 2 times, first + scend > q_size. asq_size range: 2,64,65,66,128,129,130,4096\n");
    for (sq_size_idx = 0; sq_size_idx < ARRAY_SIZE(test2_asq_size); sq_size_idx++)
    {
        admin_sq_size = test2_asq_size[sq_size_idx];
        admin_cq_size = test2_asq_size[sq_size_idx];
        pr_color(LOG_COLOR_GREEN, "\ncfg acq_size:%d, asq_size:%d\n",
                  test2_asq_size[sq_size_idx], test2_asq_size[sq_size_idx]);
        test_change_init(g_fd, admin_sq_size, admin_cq_size, NVME_INT_MSIX, g_nvme_dev.max_sq_num + 1);

        /**********************************************************************/
        cmd_cnt = 0;
        for (index = 0; index < test2_asq_1_send[sq_size_idx]; index++)
        {
            test_flag |= keep_alive_cmd(g_fd);
            cmd_cnt++;
        }
        test_flag |= nvme_ring_sq_doorbell(g_fd, 0);
        test_flag |= cq_gain(0, cmd_cnt, &reap_num);
        pr_color(LOG_COLOR_PURPLE, "first send, admin q reaped ok! reap_num:%d\n", reap_num);

        /**********************************************************************/
        cmd_cnt = 0;
        for (index = 0; index < test2_asq_2_send[sq_size_idx]; index++)
        {
            test_flag |= keep_alive_cmd(g_fd);
            cmd_cnt++;
        }
        test_flag |= nvme_ring_sq_doorbell(g_fd, 0);
        test_flag |= cq_gain(0, cmd_cnt, &reap_num);
        pr_color(LOG_COLOR_PURPLE, "second send, admin q reaped ok! reap_num:%d\n", reap_num);
    }
    /*******************************************************************************************************************************/
    pr_info("\n3. issue illegal admin cmd opcodes\n");
    test_flag |= admin_illegal_opcode_cmd(g_fd, 0xff);
    test_flag |= nvme_ring_sq_doorbell(g_fd, 0);
    cq_gain(0, 1, &reap_num);
    pr_color(LOG_COLOR_PURPLE, "send illegal admin cmd reaped ok! reap_num:%d\n", reap_num);
    /**********************************************************************/
    return test_flag;
}

static uint32_t sub_case_asq_size_random(void)
{
    uint8_t int_type = 0;
    uint16_t num_irqs;
    uint32_t cmd_cnt = 0;
    uint32_t acqsz = NVME_AQ_MAX_SIZE;
    uint32_t asqsz = NVME_AQ_MAX_SIZE;
    for(uint32_t idx = 0; idx < 50; idx++)
    {
        int_type = BYTE_RAND() % 4;
        if (int_type == NVME_INT_PIN || int_type == NVME_INT_MSI_SINGLE)
        {
            num_irqs = 1;
        }
        else
        {
            num_irqs = g_nvme_dev.max_sq_num + 1;
        }

        asqsz = rand() % (NVME_AQ_MAX_SIZE - 2) + 2;
        acqsz = rand() % (NVME_AQ_MAX_SIZE - 2) + 2;

        test_change_init(g_fd, asqsz, acqsz, int_type, num_irqs);

        cmd_cnt = 0;
        for (uint32_t index = 0; index < (asqsz - 1); index++)
        {
            test_flag |= keep_alive_cmd(g_fd);
            cmd_cnt ++;
        }
        test_flag |= nvme_ring_sq_doorbell(g_fd, 0);
        // usleep(500000);
        nvme_msi_register_test();
        test_flag |= cq_gain(0, cmd_cnt, &reap_num);
        pr_color(LOG_COLOR_PURPLE, "  acq reaped %d ok!\n", reap_num);
    }
    return test_flag;
}
