
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
static uint32_t pmcap = 0;

static char *disp_this_case = "this case will tests PCIe PM\n";

static void test_sub(void)
{
    uint32_t u32_tmp_data = 0;
    int cmds;

    /************************** D0 to D3 *********************/
    pr_info("\nD0 to D3\n");

    u32_tmp_data = pci_read_dword(file_desc, pmcap + 0x4);
    u32_tmp_data |= 0x03;
    ioctl_pci_write_data(file_desc, pmcap + 0x4, 4, (uint8_t *)&u32_tmp_data);
    usleep(100000); // 100 ms

    // check status
    u32_tmp_data = pci_read_dword(file_desc, pmcap + 0x4);
    u32_tmp_data &= 0x03;
    if (u32_tmp_data == 3)
    {
        pr_info("D0 to D3 OK\n");
    }
    else
    {
        pr_err("D0 to D3 failed: D%d\n", u32_tmp_data);
        test_flag = FAILED;
    }

    //usleep(500000);
    scanf("%d", &cmds);

    /************************** D3 to D0 *********************/
    pr_info("\nD3 to D0\n");

    u32_tmp_data = pci_read_dword(file_desc, pmcap + 0x4);
    u32_tmp_data &= 0xFFFFFFFC;
    ioctl_pci_write_data(file_desc, pmcap + 0x4, 4, (uint8_t *)&u32_tmp_data);
    usleep(100000); // 100 ms

    // check status
    u32_tmp_data = pci_read_dword(file_desc, pmcap + 0x4);
    u32_tmp_data &= 0x03;
    if (u32_tmp_data == 0)
    {
        pr_info("D3 to D0 OK\n");
    }
    else
    {
        pr_err("D3 to D0 failed: D%d\n", u32_tmp_data);
        test_flag = FAILED;
    }

    usleep(500000);
    //scanf("%d", &cmds);
}

int case_pcie_PM(void)
{
    int test_round = 0;
    uint32_t offset, u32_tmp_data = 0;

    pr_info("\n********************\t %s \t********************\n", __FUNCTION__);
    pr_info("%s\n", disp_this_case);
    /**********************************************************************/

    //find PM CAP
    offset = pci_read_dword(file_desc, 0x34);
    offset &= 0xFF;
    u32_tmp_data = pci_read_dword(file_desc, offset);
    if ((u32_tmp_data & 0xFF) == 0x01)
    {
        pmcap = offset;
    }
    else
    {
        while (0x01 != (u32_tmp_data & 0xFF))
        {
            pmcap = (u32_tmp_data >> 8) & 0xFF;
            u32_tmp_data = pci_read_dword(file_desc, pmcap);
        }
    }

    // first displaly power up PM status
    u32_tmp_data = pci_read_dword(file_desc, pmcap + 0x4);
    u32_tmp_data &= 0x03;
    pr_info("\nPower up PM status: D%d\n", u32_tmp_data);
    usleep(200000);

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