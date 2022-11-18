
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
static uint8_t speed, width;

static char *disp_this_case = "this case will tests PCIe low power step\n";

static void test_sub(void)
{
    uint32_t reg_value, u32_tmp_data = 0;
    int cmds;

    // get register value
    reg_value = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst + 0x10);
    reg_value &= 0xFFFFFFFC;

    // pr_info("\n/************************** L0 --> L0s --> L0 *********************/\n");
    // pr_info("\nL0 --> L0s\n");
    // //EP enable L0s
    // u32_tmp_data = reg_value | 0x01;
    // ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst+0x10, 4, (uint8_t *)&u32_tmp_data);
    // scanf("%d", &cmds);

    // pr_info("\nDisable L0s\n");
    // //EP disable L0s
    // u32_tmp_data = reg_value;
    // ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst+0x10, 4, (uint8_t *)&u32_tmp_data);
    // scanf("%d", &cmds);

    pr_info("\n/************************** L0 --> L1 --> L0 --> L1 *********************/\n");

    pr_info("\nL0 --> L1\n");
    system("setpci -s 0:1.1 b0.b=42"); //RC enable L1
    //EP enable L1
    u32_tmp_data = reg_value | 0x02;
    ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst + 0x10, 4, (uint8_t *)&u32_tmp_data);
    scanf("%d", &cmds);

    pr_info("\nL1 --> L0 --> L1\n");
    u32_tmp_data = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst + 0x10); //access EP
    scanf("%d", &cmds);

    pr_info("\nDisable L1\n");
    system("setpci -s 0:1.1 b0.b=40"); //RC disable L1
    //EP disable L1
    u32_tmp_data = reg_value;
    ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst + 0x10, 4, (uint8_t *)&u32_tmp_data);
    scanf("%d", &cmds);

    pr_info("\n/************************** L0 --> L0s&L1 --> L0 --> L0s&L1 *********************/\n");

    pr_info("\nL0 --> L0s&L1\n");
    system("setpci -s 0:1.1 b0.b=43"); //RC enable L0s&L1
    //EP enable L0s&L1
    u32_tmp_data = reg_value | 0x03;
    ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst + 0x10, 4, (uint8_t *)&u32_tmp_data);
    scanf("%d", &cmds);

    pr_info("\nL1 --> L0 --> L0s&L1\n");
    u32_tmp_data = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst + 0x10); //access EP
    scanf("%d", &cmds);

    pr_info("\nDisable L0s&L1\n");
    system("setpci -s 0:1.1 b0.b=40"); //RC disable L0s&L1
    //EP disable L0s&L1
    u32_tmp_data = reg_value;
    ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst + 0x10, 4, (uint8_t *)&u32_tmp_data);
    scanf("%d", &cmds);
}

int case_pcie_low_power_L0s_L1_step(void)
{
    int test_round = 0;
    uint32_t u32_tmp_data = 0;

    pr_info("\n********************\t %s \t********************\n", __FUNCTION__);
    pr_info("%s\n", disp_this_case);

    // first displaly power up link status
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
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
