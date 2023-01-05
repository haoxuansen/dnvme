#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "dnvme_ioctl.h"
#include "pci.h"
#include "irq.h"
#include "queue.h"

#include "common.h"
#include "unittest.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_irq.h"
#include "test_init.h"

static int test_flag = SUCCEED;
static uint32_t test_loop = 0;
static uint32_t io_sq_id = 1;
static uint32_t io_cq_id = 1;
static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint32_t reap_num = 0;

static int sub_case_int_queue_mask(void);
static int sub_case_pending_bit(void);

static SubCaseHeader_t sub_case_header = {
    "case_queue_cq_int_all",
    "This case will tests all cq interrupt (pin/msi_single/msi_multi/msix) mask\r\n"
    "\033[33m!!!!!!!!!!!!!!! msi_multi test HOST BIOS MUST OPEN VTD!!!!!!!!!!!!!!!!!\033[0m",
    NULL,
    NULL,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_int_queue_mask, "tests all type interrupt mask feature"),
    SUB_CASE(sub_case_pending_bit, "tests all pending bit"),
};

int case_queue_cq_int_all_mask(void)
{
    uint32_t round_idx = 0;

    test_loop = 10;

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
    nvme_reinit(g_fd, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSIX, g_nvme_dev.max_sq_num + 1);
    return test_flag;
}

static int sub_case_int_queue_mask(void)
{
    uint32_t index = 0;

    create_all_io_queue(0);

    uint8_t queue_num = BYTE_RAND() % g_nvme_dev.max_sq_num + 1;

    wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
    wr_nlb = WORD_RAND() % 255 + 1;
    for (uint16_t i = 0; i < queue_num; i++)
    {
        io_sq_id = g_ctrl_sq_info[i].sqid;
        g_ctrl_sq_info[i].cmd_cnt = 0;

        for (index = 0; index < 200; index++)
        {
            if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
            {
                test_flag |= nvme_send_iocmd(g_fd, true, io_sq_id, wr_nsid, wr_slba, wr_nlb, g_write_buf);
                g_ctrl_sq_info[i].cmd_cnt++;
            }
        }
    }
    for (uint16_t i = 0; i < queue_num; i++)
    {
        io_sq_id = g_ctrl_sq_info[i].sqid;
        io_cq_id = g_ctrl_sq_info[i].cqid;
        test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
        nvme_mask_irq(g_fd, io_cq_id);  /////////////////////////////////////////////////nvme_mask_irq
    }
    usleep(10000);
    for (uint16_t i = 0; i < queue_num; i++)
    {
        io_cq_id = g_ctrl_sq_info[i].cqid;
        nvme_unmask_irq(g_fd, io_cq_id); /////////////////////////////////////////////////nvme_unmask_irq
        test_flag |= cq_gain(io_cq_id, g_ctrl_sq_info[i].cmd_cnt, &reap_num);
    }
    delete_all_io_queue();
    return test_flag;
}

int msi_cap_access(void)
{
    int ret_val = FAILED;
    struct pcie_msi_cap msi_cap;

    ret_val = pci_read_config_data(g_fd, g_nvme_dev.msicap_ofst, sizeof(struct pcie_msi_cap), &msi_cap);

    pr_info("\nmsi_cap_id: %#x\n",msi_cap.cap_id);
    pr_info("msg_ctrl: %#x\n",msi_cap.msg_ctrl);
    pr_info("msg_adrl: %#x\n",msi_cap.msg_adrl);
    pr_info("msg_dat: %#x\n",msi_cap.msg_dat);
    pr_info("mask_bit: %#x\n",msi_cap.mask_bit);
    pr_info("pending_bit: %#x\n",msi_cap.pending_bit);

    return ret_val;
}


static int sub_case_pending_bit(void)
{
    create_all_io_queue(0);
    
    uint8_t queue_num = BYTE_RAND() % g_nvme_dev.max_sq_num + 1;

    wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
    wr_nlb = WORD_RAND() % 255 + 1;
    for (uint16_t i = 0; i < queue_num; i++)
    {
        io_sq_id = g_ctrl_sq_info[i].sqid;
        g_ctrl_sq_info[i].cmd_cnt = 0;

        for (uint32_t index = 0; index < 200; index++)
        {
            if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
            {
                test_flag |= nvme_send_iocmd(g_fd, true, io_sq_id, wr_nsid, wr_slba, wr_nlb, g_write_buf);
                g_ctrl_sq_info[i].cmd_cnt++;
            }
        }
    }
    for (uint16_t i = 0; i < queue_num; i++)
    {
        io_sq_id = g_ctrl_sq_info[i].sqid;
        test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
        nvme_mask_irq(g_fd, io_cq_id);  /////////////////////////////////////////////////nvme_mask_irq
    }
    sleep(10);
    for (uint16_t i = 0; i < queue_num; i++)
    {
        io_cq_id = g_ctrl_sq_info[i].cqid;
        if( (g_nvme_dev.irq_type == NVME_INT_MSI_SINGLE)||(g_nvme_dev.irq_type == NVME_INT_MSI_MULTI) )
        {
            msi_cap_access();
        }
        nvme_unmask_irq(g_fd, io_cq_id); /////////////////////////////////////////////////nvme_unmask_irq
        test_flag |= cq_gain(io_cq_id, g_ctrl_sq_info[i].cmd_cnt, &reap_num);
    }
    delete_all_io_queue();
    return test_flag;
}
