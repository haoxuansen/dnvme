
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

static char *disp_this_case = "this case will tests PCIe Link Speed and Width step by step\n";

static void test_sub(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
	uint8_t exp_oft = pdev->express.offset;
    int ret;
    uint32_t u32_tmp_data = 0;
    // int cmds;

    /************************** Set PCIe lane width: X1 *********************/
    pr_info("\nSet PCIe lane width: X1\n");

    ret = pci_read_config_dword(ndev->fd, 0x8C0, &u32_tmp_data);// SPHY beagle spec 5.7.156(p366),
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0xFFFFFF80;
    u32_tmp_data |= 0x000000041;
    pci_write_config_data(ndev->fd, 0x8C0, 4, (uint8_t *)&u32_tmp_data);

    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);

    // check Link status register
    ret = pci_read_config_word(ndev->fd, exp_oft + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    u32_tmp_data = (u32_tmp_data >> 4) & 0x3F;
    if (u32_tmp_data == 1)
    {
        //pr_info("Successful: linked width X1\n");
    }
    else
    {
        pr_err("Error: linked width: X%d\n", u32_tmp_data);
        test_flag = -1;
    }

    /************************** Set PCIe lane width: X2 *********************/
    pr_info("\nSet PCIe lane width: X2\n");

    ret = pci_read_config_dword(ndev->fd, 0x8C0, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0xFFFFFF80;
    u32_tmp_data |= 0x000000042;
    pci_write_config_data(ndev->fd, 0x8C0, 4, (uint8_t *)&u32_tmp_data);

    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);

    // check Link status register
    ret = pci_read_config_word(ndev->fd, exp_oft + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    u32_tmp_data = (u32_tmp_data >> 4) & 0x3F;
    if (u32_tmp_data == 2)
    {
        // pr_info("Successful: linked width X2\n");
    }
    else
    {
        pr_info("Error: linked width: X%d\n", u32_tmp_data);
        test_flag = -1;
    }

    /************************** Set PCIe lane width: X4 *********************/
    pr_info("\nSet PCIe lane width: X4\n");

    ret = pci_read_config_dword(ndev->fd, 0x8C0, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0xFFFFFF80;
    u32_tmp_data |= 0x000000044;
    pci_write_config_data(ndev->fd, 0x8C0, 4, (uint8_t *)&u32_tmp_data);

    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);

    // check Link status register
    ret = pci_read_config_word(ndev->fd, exp_oft + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    u32_tmp_data = (u32_tmp_data >> 4) & 0x3F;
    if (u32_tmp_data == 4)
    {
        // pr_info("Successful: linked width X4\n");
    }
    else
    {
        pr_info("Error: linked width: X%d\n", u32_tmp_data);
        test_flag = -1;
    }

    /************************** Set PCIe lane width: X2 *********************/
    pr_info("\nSet PCIe lane width: X2\n");

    ret = pci_read_config_dword(ndev->fd, 0x8C0, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0xFFFFFF80;
    u32_tmp_data |= 0x000000042;
    pci_write_config_data(ndev->fd, 0x8C0, 4, (uint8_t *)&u32_tmp_data);

    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);

    // check Link status register
    ret = pci_read_config_word(ndev->fd, exp_oft + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data = (u32_tmp_data >> 4) & 0x3F;
    if (u32_tmp_data == 2)
    {
        // pr_info("Successful: linked width X2\n");
    }
    else
    {
        pr_info("Error: linked width: X%d\n", u32_tmp_data);
        test_flag = -1;
    }

    /************************** Set PCIe lane width: X1 *********************/
    pr_info("\nSet PCIe lane width: X1\n");
    ret = pci_read_config_dword(ndev->fd, 0x8C0, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0xFFFFFF80;
    u32_tmp_data |= 0x000000041;
    pci_write_config_data(ndev->fd, 0x8C0, 4, (uint8_t *)&u32_tmp_data);

    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);

    // check Link status register
    ret = pci_read_config_word(ndev->fd, exp_oft + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data = (u32_tmp_data >> 4) & 0x3F;
    if (u32_tmp_data == 1)
    {
        // pr_info("Successful: linked width X1\n");
    }
    else
    {
        pr_info("Error: linked width: X%d\n", u32_tmp_data);
        test_flag = -1;
    }

    /************************** Set PCIe lane width: X4 *********************/
    pr_info("\nSet PCIe lane width: X4\n");

    ret = pci_read_config_dword(ndev->fd, 0x8C0, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0xFFFFFF80;
    u32_tmp_data |= 0x000000044;
    pci_write_config_data(ndev->fd, 0x8C0, 4, (uint8_t *)&u32_tmp_data);

    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);

    // check Link status register
    ret = pci_read_config_word(ndev->fd, exp_oft + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data = (u32_tmp_data >> 4) & 0x3F;
    if (u32_tmp_data == 4)
    {
        // pr_info("Successful: linked width X4\n");
    }
    else
    {
        pr_info("Error: linked width: X%d\n", u32_tmp_data);
        test_flag = -1;
    }

    /************************** Set PCIe link speed: Gen1 *********************/
    pr_info("\nSet PCIe link speed: Gen1\n");
    // u32_tmp_data = pci_read_dword(g_fd, ndev->express.offset+0x30);
    // u32_tmp_data &= 0xFFFFFFF0;
    // u32_tmp_data |= 0x00000001;
    // pci_write_config_data(g_fd, ndev->express.offset+0x30, 4, (uint8_t *)&u32_tmp_data);

    pcie_RC_cfg_speed(1); // RC cfg gen1
    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);

    // check Link status register
    ret = pci_read_config_word(ndev->fd, exp_oft + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    u32_tmp_data = u32_tmp_data & 0x0F;
    if (u32_tmp_data == 1)
    {
        // pr_info("Successful: linked speed Gen1\n");
    }
    else
    {
        pr_info("Error: linked speed: Gen%d\n", u32_tmp_data);
        test_flag = -1;
    }

    /************************** Set PCIe link speed: Gen2 *********************/
    pr_info("\nSet PCIe link speed: Gen2\n");
    // u32_tmp_data = pci_read_dword(g_fd, ndev->express.offset+0x30);
    // u32_tmp_data &= 0xFFFFFFF0;
    // u32_tmp_data |= 0x00000002;
    // pci_write_config_data(g_fd, ndev->express.offset+0x30, 4, (uint8_t *)&u32_tmp_data);

    pcie_RC_cfg_speed(2); // RC cfg gen2
    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);

    // check Link status register
    ret = pci_read_config_word(ndev->fd, exp_oft + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    u32_tmp_data = u32_tmp_data & 0x0F;
    if (u32_tmp_data == 2)
    {
        // pr_info("Successful: linked speed Gen2\n");
    }
    else
    {
        pr_info("Error: linked speed: Gen%d\n", u32_tmp_data);
        test_flag = -1;
    }

    /************************** Set PCIe link speed: Gen3 *********************/
    pr_info("\nSet PCIe link speed: Gen3\n");
    // u32_tmp_data = pci_read_dword(g_fd, ndev->express.offset+0x30);
    // u32_tmp_data &= 0xFFFFFFF0;
    // u32_tmp_data |= 0x00000003;
    // pci_write_config_data(g_fd, ndev->express.offset+0x30, 4, (uint8_t *)&u32_tmp_data);

    pcie_RC_cfg_speed(3); // RC cfg gen3
    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);

    // check Link status register
    ret = pci_read_config_word(ndev->fd, exp_oft + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    u32_tmp_data = u32_tmp_data & 0x0F;
    if (u32_tmp_data == 3)
    {
        // pr_info("Successful: linked speed Gen3\n");
    }
    else
    {
        pr_info("Error: linked speed: Gen%d\n", u32_tmp_data);
        test_flag = -1;
    }

    /************************** Set PCIe link speed: Gen2 *********************/
    pr_info("\nSet PCIe link speed: Gen2\n");
    // u32_tmp_data = pci_read_dword(g_fd, ndev->express.offset+0x30);
    // u32_tmp_data &= 0xFFFFFFF0;
    // u32_tmp_data |= 0x00000002;
    // pci_write_config_data(g_fd, ndev->express.offset+0x30, 4, (uint8_t *)&u32_tmp_data);

    pcie_RC_cfg_speed(2); // RC cfg gen2
    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);

    // check Link status register
    ret = pci_read_config_word(ndev->fd, exp_oft + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    u32_tmp_data = u32_tmp_data & 0x0F;
    if (u32_tmp_data == 2)
    {
        // pr_info("Successful: linked speed Gen2\n");
    }
    else
    {
        pr_info("Error: linked speed: Gen%d\n", u32_tmp_data);
        test_flag = -1;
    }

    /************************** Set PCIe link speed: Gen1 *********************/
    pr_info("\nSet PCIe link speed: Gen1\n");
    // u32_tmp_data = pci_read_dword(g_fd, ndev->express.offset+0x30);
    // u32_tmp_data &= 0xFFFFFFF0;
    // u32_tmp_data |= 0x00000001;
    // pci_write_config_data(g_fd, ndev->express.offset+0x30, 4, (uint8_t *)&u32_tmp_data);

    pcie_RC_cfg_speed(1); // RC cfg gen1
    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);

    // check Link status register
    ret = pci_read_config_word(ndev->fd, exp_oft + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    u32_tmp_data = u32_tmp_data & 0x0F;
    if (u32_tmp_data == 1)
    {
        // pr_info("Successful: linked speed Gen1\n");
    }
    else
    {
        pr_info("Error: linked speed: Gen%d\n", u32_tmp_data);
        test_flag = -1;
    }

    /************************** Set PCIe link speed: Gen3 *********************/
    pr_info("\nSet PCIe link speed: Gen3\n");
    // u32_tmp_data = pci_read_dword(g_fd, ndev->express.offset+0x30);
    // u32_tmp_data &= 0xFFFFFFF0;
    // u32_tmp_data |= 0x00000003;
    // pci_write_config_data(g_fd, ndev->express.offset+0x30, 4, (uint8_t *)&u32_tmp_data);

    pcie_RC_cfg_speed(3); // RC cfg gen3
    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);

    // check Link status register
    ret = pci_read_config_word(ndev->fd, exp_oft + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    u32_tmp_data = u32_tmp_data & 0x0F;
    if (u32_tmp_data == 3)
    {
        // pr_info("Successful: linked speed Gen3\n");
    }
    else
    {
        pr_info("Error: linked speed: Gen%d\n", u32_tmp_data);
        test_flag = -1;
    }
}

static int case_pcie_link_speed_width_step(struct nvme_tool *tool, struct case_data *priv)
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
NVME_CASE_SYMBOL(case_pcie_link_speed_width_step, "?");
