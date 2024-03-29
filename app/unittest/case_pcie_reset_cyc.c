#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"

#include "test.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_irq.h"

static int test_flag = 0;
static uint8_t speed, width;

static char *disp_this_case = "this case will tests PCIe Reset cycle\n";

static void test_sub(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
    uint32_t u32_tmp_data = 0;
    uint8_t cur_speed, cur_width;
    int ret;
    // int cmds;

    /************************** Issue hot reset *********************/
    pr_info("\nIssue hot reset\n");
    pcie_do_hot_reset(ndev->fd);

    // u32_tmp_data = pci_read_dword(g_fd, 0);
    // if ((u32_tmp_data >> 16) != 0x100)
    // {
    //     pr_err("device id error: %x\n", u32_tmp_data >> 16);
    // }
    // u32_tmp_data = pci_read_dword(g_fd, 0x2c);
    // if ((u32_tmp_data) != 0x01020304)
    // {
    //     pr_err("sub_sys id error: %x\n", u32_tmp_data);
    // }
    // u32_tmp_data = pci_read_dword(g_fd, 0x8);
    // if ((u32_tmp_data) != 0x01080200)
    // {
    //     pr_err("class code id error: %d\n", u32_tmp_data);
    // }

    // check status after re-link
    ret = pci_read_config_word(ndev->fd, pdev->express.offset + 0x12, (uint16_t *)&u32_tmp_data);
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
        test_flag = -1;
    }

    /************************** Issue link down *********************/
    pr_info("\nIssue link down\n");
    pcie_do_link_down(ndev->fd);

    // check status after re-link
    ret = pci_read_config_word(ndev->fd, pdev->express.offset + 0x12, (uint16_t *)&u32_tmp_data);
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
        test_flag = -1;
    }
}

static int case_pcie_reset_cyc(struct nvme_tool *tool, struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
    int test_round = 0;
    uint32_t u32_tmp_data = 0;
    int ret;

    pr_info("\n********************\t %s \t********************\n", __FUNCTION__);
    pr_info("%s\n", disp_this_case);

    // first displaly power up link status
    ret = pci_read_config_word(ndev->fd, pdev->express.offset + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    speed = u32_tmp_data & 0x0F;
    width = (u32_tmp_data >> 4) & 0x3F;
    pr_info("\nPower up linked status: Gen%d, X%d\n", speed, width);
    usleep(200000);
    test_flag = 0;
    /**********************************************************************/
    for (test_round = 1; test_round <= 2; test_round++)
    {
        pr_info("test_round: %d\n", test_round);
        test_sub();
        if (test_flag)
        {
            break;
        }
    }
    sleep(1);
    
    nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSIX);

    return test_flag != 0 ? -EPERM : 0;
}
NVME_CASE_SYMBOL(case_pcie_reset_cyc, "?");
