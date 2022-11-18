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
#include "test_init.h"

static int test_flag = SUCCEED;
static uint8_t speed, width;

static char *disp_this_case = "this case will tests PCIe Reset cycle\n";

static void test_sub(void)
{
    uint32_t u32_tmp_data = 0;
    uint8_t cur_speed, cur_width;
    // int cmds;

    /************************** Issue hot reset *********************/
    LOG_INFO("\nIssue hot reset\n");
    pcie_hot_reset();

    // u32_tmp_data = pci_read_dword(file_desc, 0);
    // if ((u32_tmp_data >> 16) != 0x100)
    // {
    //     LOG_ERROR("device id error: %x\n", u32_tmp_data >> 16);
    // }
    // u32_tmp_data = pci_read_dword(file_desc, 0x2c);
    // if ((u32_tmp_data) != 0x01020304)
    // {
    //     LOG_ERROR("sub_sys id error: %x\n", u32_tmp_data);
    // }
    // u32_tmp_data = pci_read_dword(file_desc, 0x8);
    // if ((u32_tmp_data) != 0x01080200)
    // {
    //     LOG_ERROR("class code id error: %d\n", u32_tmp_data);
    // }

    // check status after re-link
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    cur_speed = u32_tmp_data & 0x0F;
    cur_width = (u32_tmp_data >> 4) & 0x3F;
    if (cur_speed == speed && cur_width == width)
    {
        //LOG_INFO("linked status OK\n");
    }
    else
    {
        LOG_ERROR("linked status error: Gen%d, X%d\n", cur_speed, cur_width);
        test_flag = FAILED;
    }

    /************************** Issue link down *********************/
    LOG_INFO("\nIssue link down\n");
    pcie_link_down();

    // check status after re-link
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    cur_speed = u32_tmp_data & 0x0F;
    cur_width = (u32_tmp_data >> 4) & 0x3F;
    if (cur_speed == speed && cur_width == width)
    {
        //LOG_INFO("linked status OK\n");
    }
    else
    {
        LOG_INFO("linked status error: Gen%d, X%d\n", cur_speed, cur_width);
        test_flag = 1;
    }
}

int case_pcie_reset_cyc(void)
{
    int test_round = 0;
    uint32_t u32_tmp_data = 0;

    LOG_INFO("\n********************\t %s \t********************\n", __FUNCTION__);
    LOG_INFO("%s\n", disp_this_case);

    // first displaly power up link status
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    speed = u32_tmp_data & 0x0F;
    width = (u32_tmp_data >> 4) & 0x3F;
    LOG_INFO("\nPower up linked status: Gen%d, X%d\n", speed, width);
    usleep(200000);
    test_flag = 0;
    /**********************************************************************/
    for (test_round = 1; test_round <= 2; test_round++)
    {
        LOG_INFO("test_round: %d\n", test_round);
        test_sub();
        if (test_flag)
        {
            break;
        }
    }
    sleep(1);
    
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
