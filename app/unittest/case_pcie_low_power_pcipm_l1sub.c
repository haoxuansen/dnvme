
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
static uint32_t reg_value;
static uint32_t pmcap = 0;
static uint8_t speed, width;

static char *disp_this_case = "this case for PCIe PCIPM low power l1.1/l1.2 flow \n";

// #define BURN_IN


static void pcipm_l11_enable(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t u32_tmp_data = 0;
    int ret;

    // // RC enable PCIPML1.1
    system("setpci -s 0:1b.4 208.b=02");
    // EP enalbe PCIPM L1.1
    system("setpci -s 2:0.0 188.b=02");

    /************************** D0 to D3 *********************/
    pr_info("\nD0 to D3\n");

    ret = pci_read_config_dword(ndev->fd, pmcap + 0x4, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data |= 0x03;
    pci_write_config_data(ndev->fd, pmcap + 0x4, 4, (uint8_t *)&u32_tmp_data);
    usleep(100000); // 100 ms

    // check status
    ret = pci_read_config_dword(ndev->fd, pmcap + 0x4, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0x03;
    if (u32_tmp_data == 3)
    {
        pr_info("-->D3 OK\n");
    }
    else
    {
        pr_err("-->failed: D%d\n", u32_tmp_data);
        test_flag = -1;
    }
}

static void pcipm_l12_enable(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t u32_tmp_data = 0;
    int ret;

    // RC enable PCIPML1.1
    system("setpci -s 0:1b.4 208.b=01");
    // EP enalbe PCIPM L1.1
    system("setpci -s 2:0.0 188.b=01");

    /************************** D0 to D3 *********************/
    pr_info("\nD0 to D3\n");

    ret = pci_read_config_dword(ndev->fd, pmcap + 0x4, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data |= 0x03;
    pci_write_config_data(ndev->fd, pmcap + 0x4, 4, (uint8_t *)&u32_tmp_data);
    usleep(100000); // 100 ms

    // check status
    ret = pci_read_config_dword(ndev->fd, pmcap + 0x4, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0x03;
    if (u32_tmp_data == 3)
    {
        pr_info("-->D3 OK\n");
    }
    else
    {
        pr_err("-->failed: D%d\n", u32_tmp_data);
        test_flag = -1;
    }
}

static void pcipm_l1sub_disable(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t u32_tmp_data = 0;
    int ret;

    // EP disable L1.1
    system("setpci -s 2:0.0 188.b=00");
    // // RC disable L1.1
    system("setpci -s 0:1b.4 208.b=00");

    /************************** D3 to D0 *********************/
    pr_info("\nD3 to D0\n");

    ret = pci_read_config_dword(ndev->fd, pmcap + 0x4, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0xFFFFFFFC;
    pci_write_config_data(ndev->fd, pmcap + 0x4, 4, (uint8_t *)&u32_tmp_data);
    usleep(100000); // 100 ms

    // check status
    ret = pci_read_config_dword(ndev->fd, pmcap + 0x4, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0x03;
    if (u32_tmp_data == 0)
    {
        pr_info("-->D0 OK\n");
    }
    else
    {
        pr_err("-->D0 failed: D%d\n", u32_tmp_data);
        test_flag = -1;
    }
}

static void test_sub(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
	uint8_t exp_oft = pdev->express.offset;
    int cmds = 0;
    int ret;
    uint32_t u32_tmp_data = 0;
    uint8_t set_speed = 0, set_width = 0, cur_speed = 0, cur_width = 0;

    set_speed = 3; // gen1 gen2 gen3
    set_width = 4; // x1 x2 x4

    pr_color(LOG_N_RED, "\n .......... Set PCIe Gen%d, lane width X%d ..........\n", set_speed, set_width);

    // cfg speed (RC)
    pcie_RC_cfg_speed(set_speed);
    // cfg width (device)
    pcie_set_width(set_width);

    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);

    // check Link status register
    ret = pci_read_config_word(ndev->fd, exp_oft + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    cur_speed = u32_tmp_data & 0x0F;
    cur_width = (u32_tmp_data >> 4) & 0x3F;
    if (cur_speed == set_speed && cur_width == set_width)
    {
        pr_info("Current linked at Gen%d, X%d\n", cur_speed, cur_width);
    }
    else
    {
        pr_err("Error: linked speed: Gen%d, width: X%d\n", cur_speed, cur_width);
        test_flag = -1;
    }

    scanf("%d", &cmds);

    pr_color(LOG_N_RED, "\n .......... Change low power state: ..........\n");

    //get register value
    ret = pci_read_config_dword(ndev->fd, exp_oft + 0x10, &reg_value);
    if (ret < 0)
    	exit(-1);

    reg_value &= 0xFFFFFFFC;

    pr_info("\nenable PCIPM L1.1\n");
    pcipm_l11_enable();
    scanf("%d", &cmds);

    // pr_info("\nL1 --> L0 --> L1\n");
    // u32_tmp_data = pci_read_dword(g_fd, ndev->express.offset+0x10);       //access EP
    // scanf("%d", &cmds);

    pr_info("\ndisable PCIPM L1.1\n");
    pcipm_l1sub_disable();
    scanf("%d", &cmds);

    pr_info("\nenable PCIPM L1.2\n");
    pcipm_l12_enable();
    scanf("%d", &cmds);

    // pr_info("\nL1 --> L0 --> L1\n");
    // u32_tmp_data = pci_read_dword(g_fd, ndev->express.offset+0x10);       //access EP
    // scanf("%d", &cmds);

    pr_info("\ndisable PCIPM L1.2\n");
    pcipm_l1sub_disable();
    scanf("%d", &cmds);
}

static int case_pcie_low_power_pcipm_l1sub(struct nvme_tool *tool, struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
    int test_round = 0;
    uint32_t offset, u32_tmp_data = 0;
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

    //find PM CAP
    ret = pci_read_config_dword(ndev->fd, 0x34, &offset);
    if (ret < 0)
    	exit(-1);

    offset &= 0xFF;

    ret = pci_read_config_dword(ndev->fd, offset, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    if ((u32_tmp_data & 0xFF) == 0x01)
    {
        pmcap = offset;
    }
    else
    {
        while (0x01 != (u32_tmp_data & 0xFF))
        {
            pmcap = (u32_tmp_data >> 8) & 0xFF;
	    ret = pci_read_config_dword(ndev->fd, pmcap, &u32_tmp_data);
	    if (ret < 0)
	    	exit(-1);
        }
    }

    // first displaly power up PM status
    ret = pci_read_config_dword(ndev->fd, pmcap + 0x4, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0x03;
    pr_info("\nPower up PM status: D%d\n", u32_tmp_data);
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
NVME_CASE_SYMBOL(case_pcie_low_power_pcipm_l1sub, "?");
