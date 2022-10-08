
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "../dnvme_interface.h"

#include "common.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_irq.h"

static int test_flag = SUCCEED;
static uint32_t reg_value = 0;
static uint8_t speed, width;

static char *disp_this_case = "this case will tests PCIe low power L1sub step\n";

void pcie_L11_enable(void)
{
    uint32_t u32_tmp_data = 0;

    // RC enable L1
    system("setpci -s 0:1b.4 50.b=42");

    // RC enable L1.1
    system("setpci -s 0:1b.4 208.b=08");//L1 PM substates control 1 resister(offset 08h)

    // EP enalbe L1.1
    system("setpci -s 2:0.0 188.b=08");

    // EP enable L1
    u32_tmp_data = reg_value | 0x02;
    ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst + 0x10, 4, (uint8_t *)&u32_tmp_data);
}

void pcie_L11_disable(void)
{
    uint32_t u32_tmp_data = 0;

    // EP disable L1
    u32_tmp_data = reg_value;
    ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst + 0x10, 4, (uint8_t *)&u32_tmp_data);

    // EP disable L1.1
    system("setpci -s 2:0.0 1d8.b=00");

    // RC disable L1
    system("setpci -s 0:1b.4 50.b=40");

    // RC disable L1.1
    system("setpci -s 0:1b.4 208.b=00");//L1 PM substates control 1 resister(offset 08h)
}

void pcie_L12_enable(void)
{
    uint32_t u32_tmp_data = 0;
    uint32_t reg_data = 0;
    // get register value
    reg_data = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst + 0x28);
    u32_tmp_data = reg_data | 0x400; // bit[10] LTR enable
    ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst + 0x28, 4, (uint8_t *)&u32_tmp_data);

    // RC enable L1
    system("setpci -s 0:1b.4 50.b=42");

    // RC enable L1.2
    system("setpci -s 0:1b.4 208.b=04");//L1 PM substates control 1 resister(offset 08h)

    // EP enalbe L1.2
    system("setpci -s 2:0.0 1e4.b=04");

    // EP enable L1
    u32_tmp_data = reg_value | 0x02;
    ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst + 0x10, 4, (uint8_t *)&u32_tmp_data);
}

void pcie_L12_disable(void)
{
    uint32_t u32_tmp_data = 0;

    // EP disable L1
    u32_tmp_data = reg_value;
    ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst + 0x10, 4, (uint8_t *)&u32_tmp_data);

    // EP disable L1.2
    system("setpci -s 2:0.0 188.b=00");

    // RC disable L1
    system("setpci -s 0:1b.4 50.b=40");

    // RC disable L1.2
    system("setpci -s 0:1b.4 208.b=00");//L1 PM substates control 1 resister(offset 08h)
}

static void test_sub(void)
{
    int cmds;
    #if 0
    uint32_t u32_tmp_data = 0;
    uint8_t set_speed, set_width, cur_speed, cur_width;

    set_speed = 3; // gen1 gen2 gen3
    set_width = 4; // x1 x2 x4

    LOG_COLOR(RED_LOG, "\n .......... Set PCIe Gen%d, lane width X%d ..........\n", set_speed, set_width);

    // cfg speed (RC)
    pcie_RC_cfg_speed(set_speed);
    // cfg width (device)
    pcie_set_width(set_width);

    pcie_retrain_link();

    // check Link status register
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    cur_speed = u32_tmp_data & 0x0F;
    cur_width = (u32_tmp_data >> 4) & 0x3F;
    if (cur_speed == set_speed && cur_width == set_width)
    {
        //LOG_INFO("Successful linked\n");
    }
    else
    {
        LOG_ERROR("Error: linked speed: Gen%d, width: X%d\n", cur_speed, cur_width);
        test_flag = FAILED;
    }
    #endif

    // scanf("%d", &cmds);
    // LOG_INFO("\n/************************** L0 --> L1 --> L1sub --> L1.1 *********************/\n");

    // LOG_INFO("\nEnable L1.1\n");
    // pcie_L11_enable();

    // scanf("%d", &cmds);

    // //clkreq# rise
    // LOG_INFO("\nL1.1 exit then enter\n");
    //pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst+0x10);       //access EP

    // scanf("%d", &cmds);
    // //clkreq# fall

    // LOG_INFO("\nDisable L1.1\n");
    // pcie_L11_disable();

    scanf("%d", &cmds);

    LOG_INFO("\n/************************** L0 --> L1 --> L1sub --> L1.2 *********************/\n");

    LOG_INFO("\nEnable L1.2\n");
    pcie_L12_enable();
    
    scanf("%d", &cmds);

    //clkreq# rise
    LOG_INFO("\nL1.2 exit then enter\n");
    pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst + 0x10); //access EP
    scanf("%d", &cmds);
    //clkreq# fall

    LOG_INFO("\nDisable L1.2\n");
    pcie_L12_disable();
}

int case_pcie_low_power_L1sub_step(void)
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

    // get register value
    reg_value = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst + 0x10);
    reg_value &= 0xFFFFFFFC;

    usleep(200000);

    /**********************************************************************/
    for (test_round = 1; test_round <= 1; test_round++)
    {
        //LOG_INFO("\nlink status: %d\n", test_round);
        test_sub();
        if (test_flag)
        {
            break;
        }
    }

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
