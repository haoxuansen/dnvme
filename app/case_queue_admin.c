#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "dnvme_interface.h"

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

static dword_t sub_case_asq_size_loop_array(void);
static dword_t sub_case_asq_size_random(void);

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

    LOG_INFO("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        LOG_INFO("\ntest cnt: %d\n", round_idx);
        sub_case_list_exe(&sub_case_header, sub_case_list, ARRAY_SIZE(sub_case_list));
        if (FAILED == test_flag)
        {
            LOG_ERROR("test_flag == FAILED\n");
            break;
        }
    }
    test_change_init(file_desc, MAX_ADMIN_QUEUE_SIZE, MAX_ADMIN_QUEUE_SIZE, INT_MSIX, g_nvme_dev.max_sq_num + 1);
    return test_flag;
}

static dword_t sub_case_asq_size_loop_array(void)
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
    LOG_INFO("\n1. tests qsize: asq_size range: 2,64,65,66,128,129,130,4096,acq_size range: 2,256,257,258,512,513,4096\n");
    for (cq_size_idx = 0; cq_size_idx < ARRAY_SIZE(acq_size); cq_size_idx++)
    {
        admin_cq_size = acq_size[cq_size_idx];
        for (sq_size_idx = 0; sq_size_idx < ARRAY_SIZE(asq_size); sq_size_idx++)
        {
            admin_sq_size = asq_size[sq_size_idx];
            LOG_COLOR(GREEN_LOG, "\ncfg admin_cq_size:%d, admin_sq_size:%d\n", acq_size[cq_size_idx], asq_size[sq_size_idx]);

            test_change_init(file_desc, admin_sq_size, admin_cq_size, INT_MSIX, g_nvme_dev.max_sq_num + 1);

            cmd_cnt = 0;
            for (index = 0; index < (admin_sq_size - 1); index++)
            {
                test_flag |= keep_alive_cmd(file_desc);
                cmd_cnt++;
            }
            test_flag |= ioctl_tst_ring_dbl(file_desc, 0);
            // usleep(500000);
            nvme_msi_register_test();
            test_flag |= cq_gain(0, cmd_cnt, &reap_num);
            LOG_COLOR(PURPLE_LOG, "  acq:%d reaped ok! reap_num:%d\n", 0, reap_num);
        }
    }
    /*******************************************************************************************************************************/
    LOG_INFO("\n2. Send cmd 2 times, first + scend > q_size. asq_size range: 2,64,65,66,128,129,130,4096\n");
    for (sq_size_idx = 0; sq_size_idx < ARRAY_SIZE(test2_asq_size); sq_size_idx++)
    {
        admin_sq_size = test2_asq_size[sq_size_idx];
        admin_cq_size = test2_asq_size[sq_size_idx];
        LOG_COLOR(GREEN_LOG, "\ncfg acq_size:%d, asq_size:%d\n",
                  test2_asq_size[sq_size_idx], test2_asq_size[sq_size_idx]);
        test_change_init(file_desc, admin_sq_size, admin_cq_size, INT_MSIX, g_nvme_dev.max_sq_num + 1);

        /**********************************************************************/
        cmd_cnt = 0;
        for (index = 0; index < test2_asq_1_send[sq_size_idx]; index++)
        {
            test_flag |= keep_alive_cmd(file_desc);
            cmd_cnt++;
        }
        test_flag |= ioctl_tst_ring_dbl(file_desc, 0);
        test_flag |= cq_gain(0, cmd_cnt, &reap_num);
        LOG_COLOR(PURPLE_LOG, "first send, admin q reaped ok! reap_num:%d\n", reap_num);

        /**********************************************************************/
        cmd_cnt = 0;
        for (index = 0; index < test2_asq_2_send[sq_size_idx]; index++)
        {
            test_flag |= keep_alive_cmd(file_desc);
            cmd_cnt++;
        }
        test_flag |= ioctl_tst_ring_dbl(file_desc, 0);
        test_flag |= cq_gain(0, cmd_cnt, &reap_num);
        LOG_COLOR(PURPLE_LOG, "second send, admin q reaped ok! reap_num:%d\n", reap_num);
    }
    /*******************************************************************************************************************************/
    LOG_INFO("\n3. issue illegal admin cmd opcodes\n");
    test_flag |= admin_illegal_opcode_cmd(file_desc, 0xff);
    test_flag |= ioctl_tst_ring_dbl(file_desc, 0);
    cq_gain(0, 1, &reap_num);
    LOG_COLOR(PURPLE_LOG, "send illegal admin cmd reaped ok! reap_num:%d\n", reap_num);
    /**********************************************************************/
    return test_flag;
}

static dword_t sub_case_asq_size_random(void)
{
    byte_t int_type = 0;
    uint16_t num_irqs;
    uint32_t cmd_cnt = 0;
    uint32_t acqsz = MAX_ADMIN_QUEUE_SIZE;
    uint32_t asqsz = MAX_ADMIN_QUEUE_SIZE;
    for(uint32_t idx = 0; idx < 50; idx++)
    {
        int_type = BYTE_RAND() % 4;
        if (int_type == INT_PIN || int_type == INT_MSI_SINGLE)
        {
            num_irqs = 1;
        }
        else
        {
            num_irqs = g_nvme_dev.max_sq_num + 1;
        }

        asqsz = rand() % (MAX_ADMIN_QUEUE_SIZE - 2) + 2;
        acqsz = rand() % (MAX_ADMIN_QUEUE_SIZE - 2) + 2;

        test_change_init(file_desc, asqsz, acqsz, int_type, num_irqs);

        cmd_cnt = 0;
        for (uint32_t index = 0; index < (asqsz - 1); index++)
        {
            test_flag |= keep_alive_cmd(file_desc);
            cmd_cnt ++;
        }
        test_flag |= ioctl_tst_ring_dbl(file_desc, 0);
        // usleep(500000);
        nvme_msi_register_test();
        test_flag |= cq_gain(0, cmd_cnt, &reap_num);
        LOG_COLOR(PURPLE_LOG, "  acq reaped %d ok!\n", reap_num);
    }
    return test_flag;
}
