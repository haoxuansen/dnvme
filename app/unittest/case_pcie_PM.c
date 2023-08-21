
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
static uint32_t pmcap = 0;

static char *disp_this_case = "this case will tests PCIe PM\n";

static void test_sub(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t u32_tmp_data = 0;
    int cmds;
    int ret;

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
        pr_info("D0 to D3 OK\n");
    }
    else
    {
        pr_err("D0 to D3 failed: D%d\n", u32_tmp_data);
        test_flag = -1;
    }

    //usleep(500000);
    scanf("%d", &cmds);

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
        pr_info("D3 to D0 OK\n");
    }
    else
    {
        pr_err("D3 to D0 failed: D%d\n", u32_tmp_data);
        test_flag = -1;
    }

    usleep(500000);
    //scanf("%d", &cmds);
}

static int case_pcie_PM(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
    int test_round = 0;
    uint32_t offset, u32_tmp_data = 0;
    int ret;

    pr_info("\n********************\t %s \t********************\n", __FUNCTION__);
    pr_info("%s\n", disp_this_case);
    /**********************************************************************/

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
NVME_CASE_SYMBOL(case_pcie_PM, "?");
