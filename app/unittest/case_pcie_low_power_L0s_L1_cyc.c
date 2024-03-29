
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

static char *disp_this_case = "this case will tests PCIe low power cycle\n";

static uint8_t cur_speed = 0, cur_width = 0;

static void test_sub(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
    int ret;
    uint32_t reg_value, u32_tmp_data = 0;
    // int cmds;

    // check Link status register
    ret = pci_read_config_word(ndev->fd, pdev->express.offset + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    cur_speed = u32_tmp_data & 0x0F;
    cur_width = (u32_tmp_data >> 4) & 0x3F;
    if (cur_speed == speed && cur_width == width)
    {
        //pr_info("Successful linked\n");
    }
    else
    {
        pr_err("Error: linked speed: Gen%d, width: X%d\n", cur_speed, cur_width);
        test_flag = -1;
    }

    // get register value
    ret = pci_read_config_word(ndev->fd, pdev->express.offset + 0x10, (uint16_t *)&reg_value);
    if (ret < 0)
    	exit(-1);
    
    reg_value &= 0xFFFFFFFC;

    pr_info("\n/************************** L0 --> L0s --> L0 *********************/\n");
    pr_info("\nL0 --> L0s\n");
    //EP enable L0s
    u32_tmp_data = reg_value | 0x01;
    pci_write_config_data(ndev->fd, pdev->express.offset + 0x10, 4, (uint8_t *)&u32_tmp_data);
    //scanf("%d", &cmds);
    usleep(10000);

    pr_info("\nDisable L0s\n");
    //EP disable L0s
    u32_tmp_data = reg_value;
    pci_write_config_data(ndev->fd, pdev->express.offset + 0x10, 4, (uint8_t *)&u32_tmp_data);
    //scanf("%d", &cmds);
    usleep(10000);

    // check Link status register
    ret = pci_read_config_word(ndev->fd, pdev->express.offset + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    cur_speed = u32_tmp_data & 0x0F;
    cur_width = (u32_tmp_data >> 4) & 0x3F;
    if (cur_speed == speed && cur_width == width)
    {
        //pr_info("Successful linked\n");
    }
    else
    {
        pr_err("Error: linked speed: Gen%d, width: X%d\n", cur_speed, cur_width);
        test_flag = -1;
    }

    pr_info("\n/************************** L0 --> L1 --> L0 --> L1 *********************/\n");

    pr_info("\nL0 --> L1\n");
    system("setpci -s 0:1.1 b0.b=42"); //RC enable L1
    //EP enable L1
    u32_tmp_data = reg_value | 0x02;
    pci_write_config_data(ndev->fd, pdev->express.offset + 0x10, 4, (uint8_t *)&u32_tmp_data);
    //scanf("%d", &cmds);
    usleep(10000);

    pr_info("\nL1 --> L0 --> L1\n");
    ret = pci_read_config_dword(ndev->fd, pdev->express.offset + 0x10, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    //scanf("%d", &cmds);
    usleep(10000);

    pr_info("\nDisable L1\n");
    system("setpci -s 0:1.1 b0.b=40"); //RC disable L1
    //EP disable L1
    u32_tmp_data = reg_value;
    pci_write_config_data(ndev->fd, pdev->express.offset + 0x10, 4, (uint8_t *)&u32_tmp_data);
    //scanf("%d", &cmds);
    usleep(10000);

    // check Link status register
    ret = pci_read_config_word(ndev->fd, pdev->express.offset + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    cur_speed = u32_tmp_data & 0x0F;
    cur_width = (u32_tmp_data >> 4) & 0x3F;
    if (cur_speed == speed && cur_width == width)
    {
        //pr_info("Successful linked\n");
    }
    else
    {
        pr_err("Error: linked speed: Gen%d, width: X%d\n", cur_speed, cur_width);
        test_flag = -1;
    }

    // pr_info("\n/************************** L0 --> L0s&L1 --> L0 --> L0s&L1 *********************/\n");

    // pr_info("\nL0 --> L0s&L1\n");
    // system("setpci -s 0:1.1 b0.b=43"); //RC enable L0s&L1
    // //EP enable L0s&L1
    // u32_tmp_data = reg_value | 0x03;
    // pci_write_config_data(g_fd, ndev->express.offset + 0x10, 4, (uint8_t *)&u32_tmp_data);
    // //scanf("%d", &cmds);
    // usleep(10000);

    // pr_info("\nL1 --> L0 --> L0s&L1\n");//access EP
    // u32_tmp_data = pci_read_dword(g_fd, ndev->express.offset + 0x10);
    //                                                                 //scanf("%d", &cmds);
    // usleep(10000);

    // pr_info("\nDisable L0s&L1\n");
    // system("setpci -s 0:1.1 b0.b=40"); //RC disable L0s&L1
    // //EP disable L0s&L1
    // u32_tmp_data = reg_value;
    // pci_write_config_data(g_fd, ndev->express.offset + 0x10, 4, (uint8_t *)&u32_tmp_data);
    // //scanf("%d", &cmds);
    // usleep(10000);
}

static int case_pcie_low_power_L0s_L1_cyc(struct nvme_tool *tool, struct case_data *priv)
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

    /**********************************************************************/
    for (test_round = 1; test_round <= 1000000; test_round++)
    {
        pr_info("\ntest_round: %d\n", test_round);
        test_sub();
        if (test_flag)
        {
            break;
        }
    }

    return test_flag != 0 ? -EPERM : 0;
}
NVME_CASE_SYMBOL(case_pcie_low_power_L0s_L1_cyc, "?");
