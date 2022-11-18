
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

static char *disp_this_case = "this case will tests PCIe Link Speed and Width step by step\n";

static void test_sub(void)
{
    uint32_t u32_tmp_data = 0;
    // int cmds;

    /************************** Set PCIe lane width: X1 *********************/
    LOG_INFO("\nSet PCIe lane width: X1\n");
    u32_tmp_data = pci_read_dword(file_desc, 0x8C0); // SPHY beagle spec 5.7.156(p366),
    u32_tmp_data &= 0xFFFFFF80;
    u32_tmp_data |= 0x000000041;
    ioctl_pci_write_data(file_desc, 0x8C0, 4, (uint8_t *)&u32_tmp_data);

    pcie_retrain_link();

    // check Link status register
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    u32_tmp_data = (u32_tmp_data >> 4) & 0x3F;
    if (u32_tmp_data == 1)
    {
        //LOG_INFO("Successful: linked width X1\n");
    }
    else
    {
        LOG_ERROR("Error: linked width: X%d\n", u32_tmp_data);
        test_flag = FAILED;
    }

    /************************** Set PCIe lane width: X2 *********************/
    LOG_INFO("\nSet PCIe lane width: X2\n");
    u32_tmp_data = pci_read_dword(file_desc, 0x8C0);
    u32_tmp_data &= 0xFFFFFF80;
    u32_tmp_data |= 0x000000042;
    ioctl_pci_write_data(file_desc, 0x8C0, 4, (uint8_t *)&u32_tmp_data);

    pcie_retrain_link();

    // check Link status register
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    u32_tmp_data = (u32_tmp_data >> 4) & 0x3F;
    if (u32_tmp_data == 2)
    {
        // LOG_INFO("Successful: linked width X2\n");
    }
    else
    {
        LOG_INFO("Error: linked width: X%d\n", u32_tmp_data);
        test_flag = FAILED;
    }

    /************************** Set PCIe lane width: X4 *********************/
    LOG_INFO("\nSet PCIe lane width: X4\n");
    u32_tmp_data = pci_read_dword(file_desc, 0x8C0);
    u32_tmp_data &= 0xFFFFFF80;
    u32_tmp_data |= 0x000000044;
    ioctl_pci_write_data(file_desc, 0x8C0, 4, (uint8_t *)&u32_tmp_data);

    pcie_retrain_link();

    // check Link status register
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    u32_tmp_data = (u32_tmp_data >> 4) & 0x3F;
    if (u32_tmp_data == 4)
    {
        // LOG_INFO("Successful: linked width X4\n");
    }
    else
    {
        LOG_INFO("Error: linked width: X%d\n", u32_tmp_data);
        test_flag = FAILED;
    }

    /************************** Set PCIe lane width: X2 *********************/
    LOG_INFO("\nSet PCIe lane width: X2\n");
    u32_tmp_data = pci_read_dword(file_desc, 0x8C0);
    u32_tmp_data &= 0xFFFFFF80;
    u32_tmp_data |= 0x000000042;
    ioctl_pci_write_data(file_desc, 0x8C0, 4, (uint8_t *)&u32_tmp_data);

    pcie_retrain_link();

    // check Link status register
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    u32_tmp_data = (u32_tmp_data >> 4) & 0x3F;
    if (u32_tmp_data == 2)
    {
        // LOG_INFO("Successful: linked width X2\n");
    }
    else
    {
        LOG_INFO("Error: linked width: X%d\n", u32_tmp_data);
        test_flag = FAILED;
    }

    /************************** Set PCIe lane width: X1 *********************/
    LOG_INFO("\nSet PCIe lane width: X1\n");
    u32_tmp_data = pci_read_dword(file_desc, 0x8C0);
    u32_tmp_data &= 0xFFFFFF80;
    u32_tmp_data |= 0x000000041;
    ioctl_pci_write_data(file_desc, 0x8C0, 4, (uint8_t *)&u32_tmp_data);

    pcie_retrain_link();

    // check Link status register
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    u32_tmp_data = (u32_tmp_data >> 4) & 0x3F;
    if (u32_tmp_data == 1)
    {
        // LOG_INFO("Successful: linked width X1\n");
    }
    else
    {
        LOG_INFO("Error: linked width: X%d\n", u32_tmp_data);
        test_flag = FAILED;
    }

    /************************** Set PCIe lane width: X4 *********************/
    LOG_INFO("\nSet PCIe lane width: X4\n");
    u32_tmp_data = pci_read_dword(file_desc, 0x8C0);
    u32_tmp_data &= 0xFFFFFF80;
    u32_tmp_data |= 0x000000044;
    ioctl_pci_write_data(file_desc, 0x8C0, 4, (uint8_t *)&u32_tmp_data);

    pcie_retrain_link();

    // check Link status register
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    u32_tmp_data = (u32_tmp_data >> 4) & 0x3F;
    if (u32_tmp_data == 4)
    {
        // LOG_INFO("Successful: linked width X4\n");
    }
    else
    {
        LOG_INFO("Error: linked width: X%d\n", u32_tmp_data);
        test_flag = FAILED;
    }

    /************************** Set PCIe link speed: Gen1 *********************/
    LOG_INFO("\nSet PCIe link speed: Gen1\n");
    // u32_tmp_data = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst+0x30);
    // u32_tmp_data &= 0xFFFFFFF0;
    // u32_tmp_data |= 0x00000001;
    // ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst+0x30, 4, (uint8_t *)&u32_tmp_data);

    pcie_RC_cfg_speed(1); // RC cfg gen1
    pcie_retrain_link();

    // check Link status register
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    u32_tmp_data = u32_tmp_data & 0x0F;
    if (u32_tmp_data == 1)
    {
        // LOG_INFO("Successful: linked speed Gen1\n");
    }
    else
    {
        LOG_INFO("Error: linked speed: Gen%d\n", u32_tmp_data);
        test_flag = FAILED;
    }

    /************************** Set PCIe link speed: Gen2 *********************/
    LOG_INFO("\nSet PCIe link speed: Gen2\n");
    // u32_tmp_data = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst+0x30);
    // u32_tmp_data &= 0xFFFFFFF0;
    // u32_tmp_data |= 0x00000002;
    // ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst+0x30, 4, (uint8_t *)&u32_tmp_data);

    pcie_RC_cfg_speed(2); // RC cfg gen2
    pcie_retrain_link();

    // check Link status register
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    u32_tmp_data = u32_tmp_data & 0x0F;
    if (u32_tmp_data == 2)
    {
        // LOG_INFO("Successful: linked speed Gen2\n");
    }
    else
    {
        LOG_INFO("Error: linked speed: Gen%d\n", u32_tmp_data);
        test_flag = FAILED;
    }

    /************************** Set PCIe link speed: Gen3 *********************/
    LOG_INFO("\nSet PCIe link speed: Gen3\n");
    // u32_tmp_data = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst+0x30);
    // u32_tmp_data &= 0xFFFFFFF0;
    // u32_tmp_data |= 0x00000003;
    // ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst+0x30, 4, (uint8_t *)&u32_tmp_data);

    pcie_RC_cfg_speed(3); // RC cfg gen3
    pcie_retrain_link();

    // check Link status register
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    u32_tmp_data = u32_tmp_data & 0x0F;
    if (u32_tmp_data == 3)
    {
        // LOG_INFO("Successful: linked speed Gen3\n");
    }
    else
    {
        LOG_INFO("Error: linked speed: Gen%d\n", u32_tmp_data);
        test_flag = FAILED;
    }

    /************************** Set PCIe link speed: Gen2 *********************/
    LOG_INFO("\nSet PCIe link speed: Gen2\n");
    // u32_tmp_data = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst+0x30);
    // u32_tmp_data &= 0xFFFFFFF0;
    // u32_tmp_data |= 0x00000002;
    // ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst+0x30, 4, (uint8_t *)&u32_tmp_data);

    pcie_RC_cfg_speed(2); // RC cfg gen2
    pcie_retrain_link();

    // check Link status register
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    u32_tmp_data = u32_tmp_data & 0x0F;
    if (u32_tmp_data == 2)
    {
        // LOG_INFO("Successful: linked speed Gen2\n");
    }
    else
    {
        LOG_INFO("Error: linked speed: Gen%d\n", u32_tmp_data);
        test_flag = FAILED;
    }

    /************************** Set PCIe link speed: Gen1 *********************/
    LOG_INFO("\nSet PCIe link speed: Gen1\n");
    // u32_tmp_data = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst+0x30);
    // u32_tmp_data &= 0xFFFFFFF0;
    // u32_tmp_data |= 0x00000001;
    // ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst+0x30, 4, (uint8_t *)&u32_tmp_data);

    pcie_RC_cfg_speed(1); // RC cfg gen1
    pcie_retrain_link();

    // check Link status register
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    u32_tmp_data = u32_tmp_data & 0x0F;
    if (u32_tmp_data == 1)
    {
        // LOG_INFO("Successful: linked speed Gen1\n");
    }
    else
    {
        LOG_INFO("Error: linked speed: Gen%d\n", u32_tmp_data);
        test_flag = FAILED;
    }

    /************************** Set PCIe link speed: Gen3 *********************/
    LOG_INFO("\nSet PCIe link speed: Gen3\n");
    // u32_tmp_data = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst+0x30);
    // u32_tmp_data &= 0xFFFFFFF0;
    // u32_tmp_data |= 0x00000003;
    // ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst+0x30, 4, (uint8_t *)&u32_tmp_data);

    pcie_RC_cfg_speed(3); // RC cfg gen3
    pcie_retrain_link();

    // check Link status register
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    u32_tmp_data = u32_tmp_data & 0x0F;
    if (u32_tmp_data == 3)
    {
        // LOG_INFO("Successful: linked speed Gen3\n");
    }
    else
    {
        LOG_INFO("Error: linked speed: Gen%d\n", u32_tmp_data);
        test_flag = FAILED;
    }
}

int case_pcie_link_speed_width_step(void)
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
