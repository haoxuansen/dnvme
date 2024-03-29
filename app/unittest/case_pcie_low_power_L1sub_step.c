
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
static uint32_t reg_value = 0;
static uint8_t speed, width;

static char *disp_this_case = "this case will tests PCIe low power L1sub step\n";

void pcie_L11_enable(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
    uint32_t u32_tmp_data = 0;

    // RC enable L1
    system("setpci -s 0:1b.4 50.b=42");

    // RC enable L1.1
    system("setpci -s 0:1b.4 208.b=08");//L1 PM substates control 1 resister(offset 08h)

    // EP enalbe L1.1
    system("setpci -s 2:0.0 188.b=08");

    // EP enable L1
    u32_tmp_data = reg_value | 0x02;
    pci_write_config_data(ndev->fd, pdev->express.offset + 0x10, 4, (uint8_t *)&u32_tmp_data);
}

void pcie_L11_disable(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
    uint32_t u32_tmp_data = 0;

    // EP disable L1
    u32_tmp_data = reg_value;
    pci_write_config_data(ndev->fd, pdev->express.offset + 0x10, 4, (uint8_t *)&u32_tmp_data);

    // EP disable L1.1
    system("setpci -s 2:0.0 1d8.b=00");

    // RC disable L1
    system("setpci -s 0:1b.4 50.b=40");

    // RC disable L1.1
    system("setpci -s 0:1b.4 208.b=00");//L1 PM substates control 1 resister(offset 08h)
}

void pcie_L12_enable(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
	uint8_t exp_oft = pdev->express.offset;
    int ret;
    uint32_t u32_tmp_data = 0;
    uint32_t reg_data = 0;
    // get register value
    ret = pci_read_config_dword(ndev->fd, exp_oft + 0x28, &reg_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data = reg_data | 0x400; // bit[10] LTR enable
    pci_write_config_data(ndev->fd, exp_oft + 0x28, 4, (uint8_t *)&u32_tmp_data);

    // RC enable L1
    system("setpci -s 0:1b.4 50.b=42");

    // RC enable L1.2
    system("setpci -s 0:1b.4 208.b=04");//L1 PM substates control 1 resister(offset 08h)

    // EP enalbe L1.2
    system("setpci -s 2:0.0 1e4.b=04");

    // EP enable L1
    u32_tmp_data = reg_value | 0x02;
    pci_write_config_data(ndev->fd, exp_oft + 0x10, 4, (uint8_t *)&u32_tmp_data);
}

void pcie_L12_disable(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
    uint32_t u32_tmp_data = 0;

    // EP disable L1
    u32_tmp_data = reg_value;
    pci_write_config_data(ndev->fd, pdev->express.offset + 0x10, 4, (uint8_t *)&u32_tmp_data);

    // EP disable L1.2
    system("setpci -s 2:0.0 188.b=00");

    // RC disable L1
    system("setpci -s 0:1b.4 50.b=40");

    // RC disable L1.2
    system("setpci -s 0:1b.4 208.b=00");//L1 PM substates control 1 resister(offset 08h)
}

static void test_sub(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
    int cmds;
    int ret;
    uint32_t tmp;
    #if 0
    uint32_t u32_tmp_data = 0;
    uint8_t set_speed, set_width, cur_speed, cur_width;

    set_speed = 3; // gen1 gen2 gen3
    set_width = 4; // x1 x2 x4

    pr_color(LOG_N_RED, "\n .......... Set PCIe Gen%d, lane width X%d ..........\n", set_speed, set_width);

    // cfg speed (RC)
    pcie_RC_cfg_speed(set_speed);
    // cfg width (device)
    pcie_set_width(set_width);

    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);

    // check Link status register
    u32_tmp_data = pci_read_word(g_fd, ndev->express.offset + 0x12);
    cur_speed = u32_tmp_data & 0x0F;
    cur_width = (u32_tmp_data >> 4) & 0x3F;
    if (cur_speed == set_speed && cur_width == set_width)
    {
        //pr_info("Successful linked\n");
    }
    else
    {
        pr_err("Error: linked speed: Gen%d, width: X%d\n", cur_speed, cur_width);
        test_flag = -1;
    }
    #endif

    // scanf("%d", &cmds);
    // pr_info("\n/************************** L0 --> L1 --> L1sub --> L1.1 *********************/\n");

    // pr_info("\nEnable L1.1\n");
    // pcie_L11_enable();

    // scanf("%d", &cmds);

    // //clkreq# rise
    // pr_info("\nL1.1 exit then enter\n");
    //pci_read_dword(g_fd, ndev->express.offset+0x10);       //access EP

    // scanf("%d", &cmds);
    // //clkreq# fall

    // pr_info("\nDisable L1.1\n");
    // pcie_L11_disable();

    scanf("%d", &cmds);

    pr_info("\n/************************** L0 --> L1 --> L1sub --> L1.2 *********************/\n");

    pr_info("\nEnable L1.2\n");
    pcie_L12_enable();
    
    scanf("%d", &cmds);

    //clkreq# rise
    pr_info("\nL1.2 exit then enter\n");

    ret = pci_read_config_dword(ndev->fd, pdev->express.offset + 0x10, &tmp);
    if (ret < 0)
    	exit(-1);

    scanf("%d", &cmds);
    //clkreq# fall

    pr_info("\nDisable L1.2\n");
    pcie_L12_disable();
}

static int case_pcie_low_power_L1sub_step(struct nvme_tool *tool, struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
	uint8_t exp_oft = pdev->express.offset;
    int test_round = 0;
    uint32_t u32_tmp_data = 0;
    int ret;

    pr_info("\n********************\t %s \t********************\n", __FUNCTION__);
    pr_info("%s\n", disp_this_case);

    // first displaly power up link status
    ret = pci_read_config_word(ndev->fd, exp_oft + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    speed = u32_tmp_data & 0x0F;
    width = (u32_tmp_data >> 4) & 0x3F;
    pr_info("\nPower up linked status: Gen%d, X%d\n", speed, width);

    // get register value
    ret = pci_read_config_dword(ndev->fd, exp_oft + 0x10, &reg_value);
    if (ret < 0)
    	exit(-1);

    reg_value &= 0xFFFFFFFC;

    usleep(200000);

    /**********************************************************************/
    for (test_round = 1; test_round <= 1; test_round++)
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
NVME_CASE_SYMBOL(case_pcie_low_power_L1sub_step, "?");
