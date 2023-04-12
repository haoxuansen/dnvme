
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "dnvme.h"

#include "pci.h"
#include "pcie.h"

#include "common.h"
#include "test.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_irq.h"

static int test_flag = SUCCEED;
static uint8_t speed, width;

static char *disp_this_case = "this case will tests PCIe Reset single\n";

static void test_sub(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t u32_tmp_data = 0;
    uint8_t cur_speed, cur_width;
    int cmds;
    int ret;

    /************************** Issue hot reset *********************/
    pr_info("\nIssue hot reset\n");
    pcie_do_hot_reset(ndev->fd);

    // check status
    ret = pci_read_config_word(ndev->fd, ndev->express.offset + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
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
    pcie_do_link_down(ndev->fd);

    // check status
    ret = pci_read_config_word(ndev->fd, ndev->express.offset + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
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
    ret = pci_read_config_dword(ndev->fd, ndev->express.offset + 0x8, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data |= 0x00008000;
    pci_write_config_data(ndev->fd, ndev->express.offset + 0x8, 4, (uint8_t *)&u32_tmp_data);
    usleep(100000); // 100 ms

    ret = pci_read_config_dword(ndev->fd, 0x4, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    u32_tmp_data |= 0x06; // bus master and memory space enable
    pci_write_config_data(ndev->fd, 0x4, 4, (uint8_t *)&u32_tmp_data);
    usleep(100000); // 100 ms

    // check status
    ret = pci_read_config_word(ndev->fd, ndev->express.offset + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
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

int case_pcie_reset_single(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
    int test_round = 0;
    uint32_t u32_tmp_data = 0;
    int ret;

    pr_info("\n********************\t %s \t********************\n", __FUNCTION__);
    pr_info("%s\n", disp_this_case);
    /**********************************************************************/

    // first displaly power up link status
    ret = pci_read_config_word(ndev->fd, ndev->express.offset + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
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
