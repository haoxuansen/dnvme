#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "dnvme_interface.h"

#include "common.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_irq.h"
#include "test_init.h"

static int test_flag = SUCCEED;

static char *disp_this_case = "this case will tests MASK CQ interrupt type : msi\n"
                              ".1 use all cq, random irq_no\n"
                              "default parameter: write/read slba=0, nlb = wr_nlb\n"
                              "!!!!!!!!!!!!!!!!BIOS must open VTD!!!!!!!!!!!!!!!!!!\n";
static void int_mask_bit(uint32_t msi_mask_flag)
{
    uint32_t mask_index = 0;
    uint32_t index_max = 9;
    uint32_t mask_bit = 0;
    uint32_t u32_tmp_data = 0;
    ioctl_disable_ctrl(file_desc, ST_DISABLE_COMPLETELY);
    admin_queue_config(file_desc);

    set_irqs(file_desc, INT_MSIX, 9);

    /*
    mask_irqs(file_desc, 1);
    mask_irqs(file_desc, 2);
    mask_irqs(file_desc, 3);
    mask_irqs(file_desc, 4);
    mask_irqs(file_desc, 5);
    mask_irqs(file_desc, 6);
    mask_irqs(file_desc, 7);
    mask_irqs(file_desc, 8);
*/
    //msi_mask_flag = 0x1ff;
    LOG_INFO("msix_mask_flag is %d\n", msi_mask_flag);
    for (mask_index = 0; mask_index < index_max; mask_index++)
    {
        mask_bit = (msi_mask_flag >> mask_index) & 0x1;
        if (mask_bit == 0x1)
        {
            mask_irqs(file_desc, mask_index);
            LOG_INFO("-------mask interrupt %d\n", mask_index);
        }
        else
        {
            umask_irqs(file_desc, mask_index);
            //LOG_INFO("-------unmask interrupt %d\n", mask_index);
        }
    }
    ioctl_enable_ctrl(file_desc);

    u32_tmp_data = 0x00460001;
    ioctl_write_data(file_desc, NVME_REG_CC_OFST, 4, (uint8_t *)&u32_tmp_data);
    //while(1);
}
static void test_all_cq_cmd(uint32_t msi_mask_flag)
{
    uint32_t q_index = 0;

    uint32_t index = 0;
    uint32_t cmd_cnt = 0;
    uint32_t io_sq_id = 1;
    uint32_t io_cq_id = 1;
    uint32_t cq_size = 16384;
    uint32_t sq_size = 16384;
    uint64_t wr_slba = 0;
    uint16_t wr_nlb = 1;
    uint32_t wr_nsid = 1;

    uint16_t irq_no_arr[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    struct create_cq_parameter cq_parameter = {0};
    struct create_sq_parameter sq_parameter = {0};
    uint32_t reap_num;
    uint32_t check_status = 0;
    uint32_t check_admin_status = 0;

    LOG_COLOR(PURPLE_LOG, ".1 use all cq, random irq_no\n");
    if ((msi_mask_flag & 0x1) == 0x1)
    {
        check_admin_status = 1;
    }
    /*******************************************************************************************************************************/
    for (q_index = 1; q_index <= 8; q_index++)
    {
        if (((msi_mask_flag >> q_index) & 0x1) == 0x1)
        {
            check_status = 1;
        }
        else
        {
            check_status = 0;
        }
        io_cq_id = q_index;
        io_sq_id = q_index;
        /**********************************************************************/
        cq_parameter.cq_id = io_cq_id;
        cq_parameter.cq_size = cq_size;
        cq_parameter.contig = 1;
        cq_parameter.irq_en = 1;
        cq_parameter.irq_no = irq_no_arr[q_index - 1];
        //LOG_INFO("create cq: %d, irq_no:%d\n", io_cq_id, cq_parameter.irq_no);
        test_flag |= create_iocq(file_desc, &cq_parameter);
        test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
        //admin error(mask bit0)
        if (check_admin_status == 1)
        {
            if (cq_gain(ADMIN_QUEUE_ID, 1, &reap_num))
            {
                // test_flag = SUCCEED;
                LOG_INFO("int msi admin mask ok\n");
                break;
            }
            else
            {
                test_flag = FAILED;
                LOG_ERROR("int msi admin mask fail\n");
            }
        }
        else
        {
            test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
        }
        //LOG_INFO("  cq:%d reaped ok! reap_num:%d\n", ADMIN_QUEUE_ID, reap_num);

        //LOG_INFO("create sq: %d, assoc cq: %d\n", io_sq_id, io_cq_id);
        sq_parameter.cq_id = io_cq_id;
        sq_parameter.sq_id = io_sq_id;
        sq_parameter.sq_size = sq_size;
        sq_parameter.contig = 1;
        sq_parameter.sq_prio = MEDIUM_PRIO;
        test_flag |= create_iosq(file_desc, &sq_parameter);
        test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);

        test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
        //LOG_INFO("  cq:%d reaped ok! reap_num:%d\n", ADMIN_QUEUE_ID, reap_num);
        /**********************************************************************/
        /**********************************************************************/
        //LOG_INFO("send io cmd\n");
        wr_slba = 0;
        wr_nlb = 128;
        cmd_cnt = 0;
        for (index = 0; index < 5; index++)
        {
            test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
            cmd_cnt++;
            test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
            cmd_cnt++;
            //wr_slba += wr_nlb;
        }
        test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
        //check msi int bit error
        if (check_status == 1)
        {
            if (cq_gain(io_sq_id, cmd_cnt, &reap_num))
            {
                test_flag = SUCCEED;
                LOG_INFO("check msi int %d mask ok\n", q_index);
                continue;
            }
            else
            {
                test_flag = FAILED;
                LOG_ERROR("check msi int %d mask fail\n", q_index);
            }
        }
        else
        {
            LOG_INFO("check msi int %d mask ongoing!!\n", q_index);
            test_flag |= cq_gain(io_sq_id, cmd_cnt, &reap_num);
        }
        /**********************************************************************/
        test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_sq, io_sq_id);
        test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
        test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
        //LOG_INFO("  cq:%d reaped ok! reap_num:%d\n", ADMIN_QUEUE_ID, reap_num);
        test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_cq, io_cq_id);
        test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
        test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
        //LOG_INFO("  cq:%d reaped ok! reap_num:%d\n", ADMIN_QUEUE_ID, reap_num);
        /**********************************************************************/
    }
}
static void int_msix_signal_mask(void)
{
    uint32_t q_index = 0;
    uint32_t index_max = 9;
    uint32_t error_status = 0;
    uint32_t msi_mask_flag = 0;
    LOG_INFO("case_queue_cq_int_msix_signal_mask case start\n");
    for (q_index = 0; q_index < index_max; q_index++)
    {
        test_flag = SUCCEED;
        msi_mask_flag = (0x1 << q_index);
        int_mask_bit(msi_mask_flag);
        test_all_cq_cmd(msi_mask_flag);
        if (test_flag != SUCCEED)
        {
            error_status = 1;
            LOG_ERROR("int_msix_signal_mask %d mask bit fail\n", q_index);
        }
    }
    if (error_status != 0)
    {
        test_flag = FAILED;
        LOG_ERROR("case_queue_cq_int_msix_signal_mask case result is -1!!!!!\n");
    }
    else
    {
        test_flag = SUCCEED;
        LOG_INFO("case_queue_cq_int_msix_signal_mask case result is 1!!!!!\n");
    }
    LOG_INFO("case_queue_cq_int_msix_signal_mask case end!!!\n");
}
static void int_msix_multi_mask(void)
{
    uint32_t q_index = 0;
    uint32_t index_max = 9;
    uint32_t error_status = 0;
    uint32_t msi_mask_flag = 0;
    LOG_INFO("case_queue_cq_int_msix_multi_mask case start\n");
    for (q_index = 0; q_index < index_max; q_index++)
    {
        test_flag = SUCCEED;
        msi_mask_flag = rand() & 0xff;
        msi_mask_flag = (msi_mask_flag << 0x1);
        int_mask_bit(msi_mask_flag);
        test_all_cq_cmd(msi_mask_flag);
        if (test_flag != SUCCEED)
        {
            error_status = 1;
            LOG_ERROR("int_msix_signal_mask %d mask bit fail\n", q_index);
        }
    }
    if (error_status != 0)
    {
        test_flag = FAILED;
        LOG_ERROR("case_queue_cq_int_msix_multi_mask case result is -1!!!!!\n");
    }
    else
    {
        test_flag = SUCCEED;
        LOG_INFO("case_queue_cq_int_msix_multi_mask case result is 1!!!!!\n");
    }
    LOG_INFO("case_queue_cq_int_msix_multi_mask case end!!!\n");
}

static void test_sub(void)
{
    int_msix_signal_mask();
    int_msix_multi_mask();
}

int case_queue_cq_int_msix_mask(void)
{
    LOG_INFO("\n********************\t %s \t********************\n", __FUNCTION__);
    LOG_INFO("%s", disp_this_case);

    test_sub();
    
    test_change_init(file_desc, MAX_ADMIN_QUEUE_SIZE, MAX_ADMIN_QUEUE_SIZE, INT_MSIX, g_nvme_dev.max_sq_num + 1);

    if (test_flag != SUCCEED)
    {
        LOG_INFO("%s test result: \n%s", __FUNCTION__, TEST_FAIL);
    }
    else
    {
        LOG_INFO("%s test result: \n%s", __FUNCTION__, TEST_PASS);
    }
    return test_flag;
}
