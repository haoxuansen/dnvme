
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "dnvme_ioctl.h"

#include "common.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_irq.h"

static int test_flag = SUCCEED;
static uint8_t speed, width;

static char *disp_this_case = "this case will tests PCIe Reset single\n";

static void test_sub(void)
{
    uint32_t u32_tmp_data = 0;
    uint8_t cur_speed, cur_width;
    int cmds;

    /************************** Issue hot reset *********************/
    pr_info("\nIssue hot reset\n");
    pcie_hot_reset();

    // check status
    u32_tmp_data = pci_read_word(g_fd, g_nvme_dev.pxcap_ofst + 0x12);
    cur_speed = u32_tmp_data & 0x0F;
    cur_width = (u32_tmp_data >> 4) & 0x3F;
    if (cur_speed == speed && cur_width == width)
    {
        //pr_info("linked status OK\n");
    }
    else
    {
        pr_err("linked status error: Gen%d, X%d\n", cur_speed, cur_width);
        test_flag = FAILED;
    }
    scanf("%d", &cmds);

    /************************** Issue link down *********************/
    pr_info("\nIssue link down\n");
    pcie_link_down();

    // check status
    u32_tmp_data = pci_read_word(g_fd, g_nvme_dev.pxcap_ofst + 0x12);
    cur_speed = u32_tmp_data & 0x0F;
    cur_width = (u32_tmp_data >> 4) & 0x3F;
    if (cur_speed == speed && cur_width == width)
    {
        //pr_info("linked status OK\n");
    }
    else
    {
        pr_err("linked status error: Gen%d, X%d\n", cur_speed, cur_width);
        test_flag = FAILED;
    }
    scanf("%d", &cmds);

    /************************** Issue FLR reset *********************/
    pr_info("\nIssue FLR reset\n");
    u32_tmp_data = pci_read_dword(g_fd, g_nvme_dev.pxcap_ofst + 0x8);
    u32_tmp_data |= 0x00008000;
    ioctl_pci_write_data(g_fd, g_nvme_dev.pxcap_ofst + 0x8, 4, (uint8_t *)&u32_tmp_data);
    usleep(100000); // 100 ms

    u32_tmp_data = pci_read_dword(g_fd, 0x4);
    u32_tmp_data |= 0x06; // bus master and memory space enable
    ioctl_pci_write_data(g_fd, 0x4, 4, (uint8_t *)&u32_tmp_data);
    usleep(100000); // 100 ms

    // check status
    u32_tmp_data = pci_read_word(g_fd, g_nvme_dev.pxcap_ofst + 0x12);
    cur_speed = u32_tmp_data & 0x0F;
    cur_width = (u32_tmp_data >> 4) & 0x3F;
    if (cur_speed == speed && cur_width == width)
    {
        //pr_info("linked status OK\n");
    }
    else
    {
        pr_err("linked status error: Gen%d, X%d\n", cur_speed, cur_width);
        test_flag = FAILED;
    }
}

int case_pcie_reset_single(void)
{
    int test_round = 0;
    uint32_t u32_tmp_data = 0;

    pr_info("\n********************\t %s \t********************\n", __FUNCTION__);
    pr_info("%s\n", disp_this_case);
    /**********************************************************************/

    // first displaly power up link status
    u32_tmp_data = pci_read_word(g_fd, g_nvme_dev.pxcap_ofst + 0x12);
    speed = u32_tmp_data & 0x0F;
    width = (u32_tmp_data >> 4) & 0x3F;
    pr_info("\nPower up linked status: Gen%d, X%d\n", speed, width);
    usleep(200000);

    for (test_round = 1; test_round <= 1; test_round++)
    {
        pr_info("\nlink status: %d\n", test_round);
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
