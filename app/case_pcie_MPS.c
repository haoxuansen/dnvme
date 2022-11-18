
#include <stdio.h>
#include <stdlib.h>
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

static int test_flag = SUCCEED;

static uint32_t io_sq_id = 1;
static uint32_t io_cq_id = 1;
static uint32_t cq_size = DEFAULT_IO_QUEUE_SIZE;
static uint32_t sq_size = DEFAULT_IO_QUEUE_SIZE;
static uint32_t reap_num;
static uint32_t cmd_cnt = 0;

static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint32_t i = 0;
static struct create_cq_parameter cq_parameter = {0};
static struct create_sq_parameter sq_parameter = {0};

static char *disp_this_case = "this case will tests PCIe Max Payload Size\n";

static void set_pcie_mps_128(void)
{
    uint32_t u32_tmp_data = 0;

    // RC set MPS 128
    //system("setpci -s 0:1b.4 48.b=0f");

    // EP set MPS 128
    u32_tmp_data = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst + 0x8);
    u32_tmp_data &= 0xFFFFFF1F;
    ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst + 0x8, 4, (uint8_t *)&u32_tmp_data);
    pcie_retrain_link();
    u32_tmp_data = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst + 0x8);
    u32_tmp_data = (u32_tmp_data & 0xE0) >> 5;
    //pr_info("\nread g_nvme_dev.pxcap_ofst+0x8 0x%x\n", u32_tmp_data);
    //u32_tmp_data = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst+0x4);
    //u32_tmp_data &= 0x07;
    pr_info("\nEP Max Payload Size support 128 byte, 0x%x\n", u32_tmp_data);
}

static void set_pcie_mps_256(void)
{
    uint32_t u32_tmp_data = 0;

    // RC set MPS 256
    //system("setpci -s 0:1b.4 48.b=2f");

    // EP set MPS 256
    u32_tmp_data = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst + 0x8);
    u32_tmp_data &= 0xFFFFFF1F;
    u32_tmp_data |= 0x20;
    ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst + 0x8, 4, (uint8_t *)&u32_tmp_data);
    pcie_retrain_link();
    u32_tmp_data = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst + 0x8);
    u32_tmp_data = (u32_tmp_data & 0xE0) >> 5;
    pr_info("\nEP Max Payload Size support 256 byte, 0x%x\n", u32_tmp_data);
}

static void pcie_packet(void)
{
    pr_info("\nTest: Sending IO Read Command through sq_id %d\n", io_sq_id);
    wr_slba = 0;
    wr_nlb = 64;
    cmd_cnt = 0;
    for (i = 0; i < 10; i++)
    {
        nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
        cmd_cnt++;
    }
    pr_info("Ringing Doorbell for sq_id %d\n", io_sq_id);
    ioctl_tst_ring_dbl(file_desc, io_sq_id);
    cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq reaped ok! reap_num:%d\n", reap_num);
}

static void test_sub(void)
{
    int cmds;

    /************************** 128 byte *********************/
    pr_info("\nMPS: 128 byte\n");
    set_pcie_mps_128();
    pcie_packet();
    scanf("%d", &cmds);

    /************************** 256 byte *********************/
    pr_info("\nMPS: 256 byte\n");
    set_pcie_mps_256();
    pcie_packet();
    scanf("%d", &cmds);
}

int case_pcie_MPS(void)
{
    int test_round = 0;
    uint32_t u32_tmp_data = 0;
    cq_size = g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes;
    sq_size = g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes;
    pr_info("\n********************\t %s \t********************\n", __FUNCTION__);
    pr_info("%s\n", disp_this_case);
    /**********************************************************************/

    pr_info("\nPreparing contig cq_id = %d, cq_size = %d\n", io_cq_id, cq_size);
    cq_parameter.cq_id = io_cq_id;
    cq_parameter.cq_size = cq_size;
    cq_parameter.contig = 1;
    cq_parameter.irq_en = 1;
    cq_parameter.irq_no = io_cq_id;
    test_flag |= create_iocq(file_desc, &cq_parameter);
    pr_info("Ringing Doorbell for ADMIN_QUEUE_ID\n");
    ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
    pr_info("  cq reaped ok! reap_num:%d\n", reap_num);

    pr_info("\nPreparing contig sq_id = %d, assoc cq_id = %d, sq_size = %d\n", io_sq_id, io_cq_id, sq_size);
    sq_parameter.cq_id = io_cq_id;
    sq_parameter.sq_id = io_sq_id;
    sq_parameter.sq_size = sq_size;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;
    test_flag |= create_iosq(file_desc, &sq_parameter);
    pr_info("Ringing Doorbell for ADMIN_QUEUE_ID\n");
    ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
    pr_info("  cq reaped ok! reap_num:%d\n", reap_num);

    // first displaly EP Max Payload Size support
    u32_tmp_data = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst + 0x4);
    u32_tmp_data &= 0x07;
    if (u32_tmp_data == 0)
    {
        pr_info("\nEP Max Payload Size support 128 byte\n");
    }
    else if (u32_tmp_data == 1)
    {
        pr_info("\nEP Max Payload Size support 256 byte\n");
    }
    else if (u32_tmp_data == 2)
    {
        pr_info("\nEP Max Payload Size support 512 byte\n");
    }
    usleep(200000);

    for (test_round = 1; test_round <= 1; test_round++)
    {
        //pr_info("\nlink status: %d\n", test_round);
        test_sub();
        if (test_flag)
        {
            break;
        }
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