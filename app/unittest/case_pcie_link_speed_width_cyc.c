
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
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

static char *disp_this_case = "this case will tests PCIe Link Speed and Width cycle\n";

static void test_sub(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint16_t data;
    uint8_t set_speed, set_width, cur_speed, cur_width;
    int ret;
    // int cmds;

    // get speed and width random
    set_speed = rand() % 3 + 1;
    set_width = rand() % 4 + 1;
    if (set_width == 3)
    {
        set_width = 4;
    }
    pr_info("\nSet PCIe Gen%d, lane width X%d\n", set_speed, set_width);

    // cfg speed (RC)
    pcie_RC_cfg_speed(set_speed);
    // cfg width (device)
    pcie_set_width(set_width);

    pcie_retrain_link(RC_CAP_LINK_CONTROL);

    // check Link status register
    ret = pci_read_config_word(ndev->fd, ndev->express.offset + 0x12, &data);
    if (ret < 0)
    	exit(-1);
    
    cur_speed = data & 0x0F;
    cur_width = (data >> 4) & 0x3F;
    if (cur_speed == set_speed && cur_width == set_width)
    {
        //pr_info("Successful linked\n");
    }
    else
    {
        pr_err("Error: linked speed: Gen%d, width: X%d\n", cur_speed, cur_width);
        test_flag = -1;
    }
}

static int case_pcie_link_speed_width_cyc(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
    int test_round = 0;
    uint16_t u32_tmp_data = 0;
    int ret;
    srand(time(0));

    pr_info("\n********************\t %s \t********************\n", __FUNCTION__);
    pr_info("%s\n", disp_this_case);

    // first displaly power up link status
    ret = pci_read_config_word(ndev->fd, ndev->express.offset + 0x12, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    speed = u32_tmp_data & 0x0F;
    width = (u32_tmp_data >> 4) & 0x3F;
    pr_info("\nPower up linked status: Gen%d, X%d\n", speed, width);
    usleep(200000);
    /**********************************************************************/
    for (test_round = 1; test_round <= 10; test_round++)
    {
        //pr_info("\nlink status: %d\n", test_round);
        test_sub();
        if (test_flag)
        {
            break;
        }
    }

    return test_flag != 0 ? -EPERM : 0;
}
NVME_CASE_SYMBOL(case_pcie_link_speed_width_cyc, "?");
