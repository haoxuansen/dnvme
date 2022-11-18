#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "../dnvme_interface.h"

#include "common.h"
#include "unittest.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_init.h"
#include "case_all.h"

static int test_flag = SUCCEED;
static uint32_t test_loop = 0;
static uint32_t cmd_cnt = 0;
static uint32_t io_sq_id = 1;
static uint32_t io_cq_id = 1;
// static uint32_t cq_size = 4096;
// static uint32_t sq_size = 4096;
static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;

static struct create_cq_parameter cq_parameter = {0};
static struct create_sq_parameter sq_parameter = {0};
static uint32_t reap_num = 0;
static uint32_t send_num = 0;

static dword_t sub_case_use_1_q_del_it(void);
static dword_t sub_case_use_3_q_del_2_use_remian_del_it(void);
static dword_t sub_case_del_cq_before_sq(void);
static dword_t delete_runing_cmd_queue(void);
static dword_t delete_runing_fua_cmd_queue(void);
static dword_t delete_runing_iocmd_queue(void);

static SubCaseHeader_t sub_case_header = {
    "case_queue_delete_q",
    "This case will tests delete sq with different situation\n",
    NULL,
    NULL,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_use_1_q_del_it, "use 1 sq, delete 1 sq"),
    SUB_CASE(sub_case_use_3_q_del_2_use_remian_del_it, "use 3 sq, delete 2 sq, use remian 1 sq, then delete it"),
    SUB_CASE(sub_case_del_cq_before_sq, "delete an iocq before iosq"),
    SUB_CASE(delete_runing_cmd_queue, "delete a queue when it's cmd is runing"),
    SUB_CASE(delete_runing_fua_cmd_queue, "delete a queue when it's fua cmd is runing"),
    SUB_CASE(delete_runing_iocmd_queue, "delete a queue when iocmd is runing"),
};

int case_queue_delete_q(void)
{
    uint32_t round_idx = 0;

    test_loop = 50;

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
    return test_flag;
}

static dword_t sub_case_use_1_q_del_it(void)
{
    uint32_t cq_size = 1024; //(DWORD_RAND())%(g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes-20)+20;
    uint32_t sq_size = 1024; //(DWORD_RAND())%(g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes-20)+20;

    io_sq_id = rand() % g_nvme_dev.max_sq_num + 1;
    io_cq_id = io_sq_id;
    /**********************************************************************/
    cq_parameter.cq_id = io_cq_id;
    cq_parameter.cq_size = cq_size;
    cq_parameter.contig = 1;
    cq_parameter.irq_en = 1;
    cq_parameter.irq_no = io_cq_id;

    test_flag |= create_iocq(file_desc, &cq_parameter);
    ASSERT(!test_flag);
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);

    LOG_INFO("Preparing contig sq_id = %d, assoc cq_id = %d, sq_size = %d, cq_size = %d\n", io_sq_id, io_cq_id, sq_size, cq_size);
    sq_parameter.cq_id = io_cq_id;
    sq_parameter.sq_id = io_sq_id;
    sq_parameter.sq_size = sq_size;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;
    test_flag |= create_iosq(file_desc, &sq_parameter);
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
    /**********************************************************************/
    cmd_cnt = 0;
    send_num = (WORD_RAND() % 200) + 100;
    for (dword_t i = 0; i < send_num; i++)
    {
        if (wr_slba + wr_nlb < g_nvme_ns_info[0].nsze)
        {
            test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
            cmd_cnt++;
            test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
            cmd_cnt++;
        }
    }
    test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    LOG_DBUG("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);
    /**********************************************************************/
    test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_sq, io_sq_id);
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);

    test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_cq, io_cq_id);
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
    return test_flag;
}

static dword_t sub_case_use_3_q_del_2_use_remian_del_it(void)
{
    uint32_t cq_size = 1024; //(DWORD_RAND())%(g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes-20)+20;
    uint32_t sq_size = 1024; //(DWORD_RAND())%(g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes-20)+20;

    cq_parameter.cq_size = cq_size;
    cq_parameter.irq_en = 1;
    cq_parameter.contig = 1;
    cmd_cnt = 0;
    for (dword_t index = 2; index <= 4; index++)
    {
        cq_parameter.cq_id = index;
        cq_parameter.irq_no = index;
        test_flag |= create_iocq(file_desc, &cq_parameter);
        cmd_cnt++;
    }
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    test_flag |= cq_gain(ADMIN_QUEUE_ID, cmd_cnt, &reap_num);

    /**********************************************************************/
    sq_parameter.sq_size = sq_size;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;
    cmd_cnt = 0;
    for (dword_t index = 2; index <= 4; index++)
    {
        // LOG_INFO("create sq %d, assoc cq = %d\n", index, index);
        sq_parameter.cq_id = index;
        sq_parameter.sq_id = index;
        test_flag |= create_iosq(file_desc, &sq_parameter);
        cmd_cnt++;
    }
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    test_flag |= cq_gain(ADMIN_QUEUE_ID, cmd_cnt, &reap_num);

    /**********************************************************************/
    LOG_INFO("use 3 sq\n");
    for (dword_t index = 2; index <= 4; index++)
    {
        io_sq_id = index;
        test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
        test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
        //wr_slba += wr_nlb;
    }
    for (dword_t index = 2; index <= 4; index++)
    {
        io_sq_id = index;
        test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
    }
    for (dword_t index = 2; index <= 4; index++)
    {
        io_cq_id = index;
        test_flag |= cq_gain(io_cq_id, 2, &reap_num);
    }

    LOG_INFO("delete 2 sq\n");
    /**********************************************************************/
    cmd_cnt = 0;
    for (dword_t index = 2; index <= 3; index++)
    {
        io_sq_id = index;
        test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_sq, io_sq_id);
        cmd_cnt++;
    }
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    test_flag |= cq_gain(ADMIN_QUEUE_ID, cmd_cnt, &reap_num);

    /**********************************************************************/
    cmd_cnt = 0;
    for (dword_t index = 2; index <= 3; index++)
    {
        io_cq_id = index;
        test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_cq, io_cq_id);
        cmd_cnt++;
    }
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    test_flag |= cq_gain(ADMIN_QUEUE_ID, cmd_cnt, &reap_num);
    /**********************************************************************/
    /**********************************************************************/
    LOG_INFO("use remian 1 sq\n");
    for (dword_t index = 4; index <= 4; index++)
    {
        io_sq_id = index;
        test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
        test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
        //wr_slba += RW_BUFFER_SIZE / 512;
    }
    for (dword_t index = 4; index <= 4; index++)
    {
        io_sq_id = index;
        test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
    }
    for (dword_t index = 4; index <= 4; index++)
    {
        io_cq_id = index;
        test_flag |= cq_gain(io_cq_id, 2, &reap_num);
    }

    LOG_INFO("delete remian 2 sq\n");
    /**********************************************************************/
    cmd_cnt = 0;
    for (dword_t index = 4; index <= 4; index++)
    {
        io_sq_id = index;
        test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_sq, io_sq_id);
        cmd_cnt++;
    }
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    test_flag |= cq_gain(ADMIN_QUEUE_ID, cmd_cnt, &reap_num);

    /**********************************************************************/
    cmd_cnt = 0;
    for (dword_t index = 4; index <= 4; index++)
    {
        io_cq_id = index;
        test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_cq, io_cq_id);
        cmd_cnt++;
    }
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    test_flag |= cq_gain(ADMIN_QUEUE_ID, cmd_cnt, &reap_num);

    return test_flag;
}

static dword_t sub_case_del_cq_before_sq(void)
{
    uint32_t cq_size = 1024; //(DWORD_RAND())%(g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes-20)+20;
    uint32_t sq_size = 1024; //(DWORD_RAND())%(g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes-20)+20;

    io_sq_id = rand() % g_nvme_dev.max_sq_num + 1;
    io_cq_id = io_sq_id;
    /**********************************************************************/
    cq_parameter.cq_id = io_cq_id;
    cq_parameter.cq_size = cq_size;
    cq_parameter.contig = 1;
    cq_parameter.irq_en = 1;
    cq_parameter.irq_no = io_cq_id;

    test_flag |= create_iocq(file_desc, &cq_parameter);
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);

    LOG_INFO("Preparing contig sq_id = %d, assoc cq_id = %d, sq_size = %d, cq_size = %d\n", io_sq_id, io_cq_id, sq_size, cq_size);
    sq_parameter.cq_id = io_cq_id;
    sq_parameter.sq_id = io_sq_id;
    sq_parameter.sq_size = sq_size;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;
    test_flag |= create_iosq(file_desc, &sq_parameter);
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
    /**********************************************************************/
    cmd_cnt = 0;
    for (dword_t i = 0; i < 5; i++)
    {
        if (wr_slba + wr_nlb < g_nvme_ns_info[0].nsze)
        {
            test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
            cmd_cnt++;
            test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
            cmd_cnt++;
        }
    }

    test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    LOG_INFO("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);
    /**********************************************************************/
    LOG_INFO("delete_iocq:%d\n", io_cq_id);
    test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_cq, io_cq_id);
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    /*test_flag |= */ cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);

    LOG_INFO("delete_iosq:%d\n", io_sq_id);
    test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_sq, io_sq_id);
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);

    test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_cq, io_cq_id);
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
    return test_flag;
}

static dword_t delete_runing_cmd_queue(void)
{
    uint32_t reap_num;
    uint32_t cmd_cnt = 0;
    uint16_t wr_nlb = 8;
    uint32_t index = 0;
    uint64_t wr_slba = 0;
    byte_t cmd_num_per_q;
    byte_t queue_num = g_nvme_dev.max_sq_num; //BYTE_RAND() % g_nvme_dev.max_sq_num + 1;

    create_all_io_queue(0);

    //***use some queue**************************************************************************************
    for (dword_t qid = 1; qid <= queue_num; qid++)
    {
        cmd_cnt = 0;
        cmd_num_per_q = (rand() % 12 + 1) * 10;
        wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        for (index = 0; index < MIN(cmd_num_per_q, ctrl_sq_info[qid - 1].sq_size) / 2; index++)
        {
            if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
            {
                test_flag |= nvme_io_write_cmd(file_desc, 0, ctrl_sq_info[qid - 1].sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
                cmd_cnt++;
                test_flag |= nvme_io_read_cmd(file_desc, 0, ctrl_sq_info[qid - 1].sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
                cmd_cnt++;
            }
        }
        if (cmd_cnt == 0)
        {
            ASSERT(0);
        }
        test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_sq, ctrl_sq_info[qid - 1].sq_id);
        test_flag |= ioctl_tst_ring_dbl(file_desc, ctrl_sq_info[qid - 1].sq_id);
        test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);

        test_flag |= cq_gain_disp_cq(ctrl_sq_info[qid - 1].cq_id, cmd_cnt, &reap_num, FALSE);

        test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);

        test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_cq, ctrl_sq_info[qid - 1].cq_id);
        test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
        test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
    }
    return test_flag;
}

static dword_t delete_runing_fua_cmd_queue(void)
{

    uint32_t reap_num;
    uint32_t cmd_cnt = 0;
    uint16_t wr_nlb = 8;
    uint32_t index = 0;
    uint64_t wr_slba = 0;
    byte_t cmd_num_per_q;
    byte_t queue_num = g_nvme_dev.max_sq_num; //BYTE_RAND() % g_nvme_dev.max_sq_num + 1;

    create_all_io_queue(0);

    for (dword_t qid = 1; qid <= queue_num; qid++)
    {
        cmd_cnt = 0;
        cmd_num_per_q = (rand() % 12 + 1) * 10;
        wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        for (index = 0; index < MIN(cmd_num_per_q, ctrl_sq_info[qid - 1].sq_size) / 2; index++)
        {
            if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
            {
                test_flag |= nvme_io_write_cmd(file_desc, 0, ctrl_sq_info[qid - 1].sq_id, wr_nsid, wr_slba, wr_nlb, NVME_RW_FUA, write_buffer);
                cmd_cnt++;
            }
        }
        if (cmd_cnt == 0)
        {
            ASSERT(0);
        }
        test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_sq, ctrl_sq_info[qid - 1].sq_id);

        test_flag |= ioctl_tst_ring_dbl(file_desc, ctrl_sq_info[qid - 1].sq_id);
        test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);

        test_flag |= cq_gain_disp_cq(ctrl_sq_info[qid - 1].cq_id, cmd_cnt, &reap_num, FALSE);

        test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);

        test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_cq, ctrl_sq_info[qid - 1].cq_id);
        test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
        test_flag |= cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
    }
    return test_flag;
}

static dword_t delete_runing_iocmd_queue(void)
{
    create_all_io_queue(0);

    //***use some queue**************************************************************************************
    byte_t cmd_num_per_q;
    byte_t queue_num = BYTE_RAND() % g_nvme_dev.max_sq_num + 1;

    wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
    wr_nlb = WORD_RAND() % 255 + 1;
    cmd_num_per_q = (512 / g_nvme_dev.max_sq_num); //WORD_RAND() % 150 + 10;
    for (word_t i = 0; i < queue_num; i++)
    {
        //controller outstanding cmd num is 512
        ctrl_sq_info[i].cmd_cnt = 0;
        for (word_t index = 0; index < cmd_num_per_q; index++)
        {
            if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
            {
                test_flag |= nvme_send_iocmd(file_desc, 0, ctrl_sq_info[i].sq_id, wr_nsid, wr_slba, wr_nlb, write_buffer);
                ctrl_sq_info[i].cmd_cnt++;
            }
        }
    }
    LOG_INFO("Preparing iocmds to %d sq\n", queue_num);
    for (word_t i = 0; i < queue_num; i++)
    {
        test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_sq, ctrl_sq_info[i].sq_id);
    }
    LOG_INFO("Preparing delete %d sq cmd\n", queue_num);

    //***delete queue with cmd runing**************************************************************************************
    for (word_t i = 0; i < queue_num; i++)
    {
        test_flag |= ioctl_tst_ring_dbl(file_desc, ctrl_sq_info[i].sq_id);
    }
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);

    for (word_t i = 0; i < queue_num; i++)
    {
        test_flag |= cq_gain_disp_cq(ctrl_sq_info[i].cq_id, ctrl_sq_info[i].cmd_cnt, &reap_num, FALSE);
    }
    test_flag |= cq_gain(ADMIN_QUEUE_ID, queue_num, &reap_num);
    LOG_INFO("delete %d sq done\n", queue_num);

    for (word_t i = 0; i < queue_num; i++)
    {
        test_flag |= ioctl_delete_ioq(file_desc, nvme_admin_delete_cq, ctrl_sq_info[i].cq_id);
    }
    test_flag |= ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    test_flag |= cq_gain(ADMIN_QUEUE_ID, queue_num, &reap_num);
    //***delete remain queue**************************************************************************************
    for (dword_t sqidx = queue_num; sqidx < g_nvme_dev.max_sq_num; sqidx++)
    {
        ioctl_delete_ioq(file_desc, nvme_admin_delete_sq, ctrl_sq_info[sqidx].sq_id);
    }
    ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    cq_gain(ADMIN_QUEUE_ID, (g_nvme_dev.max_sq_num - queue_num), &reap_num);

    for (dword_t sqidx = queue_num; sqidx < g_nvme_dev.max_sq_num; sqidx++)
    {
        ioctl_delete_ioq(file_desc, nvme_admin_delete_cq, ctrl_sq_info[sqidx].cq_id);
    }
    ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    cq_gain(ADMIN_QUEUE_ID, (g_nvme_dev.max_sq_num - queue_num), &reap_num);
    LOG_INFO("delete remain sq done\n");

    // delete_all_io_queue();
    test_change_init(file_desc, MAX_ADMIN_QUEUE_SIZE, MAX_ADMIN_QUEUE_SIZE, INT_MSIX, g_nvme_dev.max_sq_num + 1);

    return test_flag;
}
