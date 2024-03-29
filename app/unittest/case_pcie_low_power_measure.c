
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

static char *disp_this_case = "this case for PCIe low power measure\n";

// #define BURN_IN

static void test_sub(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
	uint8_t exp_oft = pdev->express.offset;
    int cmds = 0;
    int ret;
    uint32_t reg_value = 0;
    uint32_t u32_tmp_data = 0;
    uint8_t set_speed = 0, set_width = 0, cur_speed = 0, cur_width = 0;

    uint32_t cmd_cnt = 0;
    uint32_t io_sq_id = 1;
    uint32_t io_cq_id = 1;
    uint32_t sq_size = 128;
    uint32_t cq_size = 128;
    uint64_t wr_slba = 0;
    uint16_t wr_nlb = 8;
    uint32_t wr_nsid = 1;

    struct create_cq_parameter cq_parameter = {0};
    struct create_sq_parameter sq_parameter = {0};
    uint32_t reap_num = 0;

    pr_info("\tPreparing io_cq_id %d, cq_size = %d\n", io_cq_id, cq_size);
    cq_parameter.cq_size = cq_size;
    cq_parameter.irq_en = 1;
    cq_parameter.contig = 1;
    cq_parameter.irq_no = io_cq_id;
    cq_parameter.cq_id = io_cq_id;
    test_flag |= create_iocq(ndev->fd, &cq_parameter);
    DBG_ON(test_flag < 0);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    DBG_ON(test_flag < 0);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    DBG_ON(test_flag < 0);
        
    pr_info("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
    /**********************************************************************/
    pr_info("\tPreparing io_sq_id %d, sq_size = %d\n", io_sq_id, sq_size);
    sq_parameter.sq_size = sq_size;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;
    sq_parameter.cq_id = io_cq_id;
    sq_parameter.sq_id = io_sq_id;
    test_flag |= create_iosq(ndev->fd, &sq_parameter);
    DBG_ON(test_flag < 0);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    DBG_ON(test_flag < 0);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    DBG_ON(test_flag < 0);
    pr_info("\tcq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

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
        //pr_info("Successful linked\n");
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

    pr_info("\n/************************** L0 --> L1 --> L0 --> L1 *********************/\n");

    scanf("%d", &cmds);
    pr_info("\nL0 --> L1\n");
    system("setpci -s 0:1b.4 50.b=42"); //RC enable L1
    // system("setpci -s 0:1.1 b0.b=42");          //RC enable L1
    //EP enable L1
    u32_tmp_data = reg_value | 0x02;
    pci_write_config_data(ndev->fd, exp_oft + 0x10, 4, (uint8_t *)&u32_tmp_data);

    scanf("%d", &cmds);
    pr_info("\nL1 --> L0 --> L1\n");
    // u32_tmp_data = pci_read_dword(g_fd, ndev->express.offset+0x10);       //access EP

    /**********************************************************************/
    cmd_cnt = 0;
    //for (uint32_t index = 1; index < (sq_size/2); index++)
    {
        test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
	DBG_ON(test_flag < 0);
        cmd_cnt++;
        test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
	DBG_ON(test_flag < 0);
        cmd_cnt++;
    }
    /**********************************************************************/
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    DBG_ON(test_flag < 0);
        
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    DBG_ON(test_flag < 0);
        
    pr_color(LOG_N_PURPLE, "\tcq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);
    /**********************************************************************/

    scanf("%d", &cmds);
    pr_info("\nDisable L1\n");
    system("setpci -s 0:1b.4 50.b=40"); //RC disable L1
    // system("setpci -s 0:1.1 b0.b=40");          //RC disable L1
    //EP disable L1
    u32_tmp_data = reg_value;
    pci_write_config_data(ndev->fd, exp_oft + 0x10, 4, (uint8_t *)&u32_tmp_data);

    scanf("%d", &cmds);
    pr_div("\nTest: Delete sq_id %d, cq_id %d\n", io_sq_id, io_cq_id);
    ioctl_delete_ioq(ndev->fd, nvme_admin_delete_sq, io_sq_id);
    ioctl_delete_ioq(ndev->fd, nvme_admin_delete_cq, io_cq_id);
    pr_div("Ringing Doorbell for NVME_AQ_ID\n");
    nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    cq_gain(NVME_AQ_ID, 2, &reap_num);
    pr_div("\tcq reaped ok! reap_num:%d\n", reap_num);
}

static int case_pcie_low_power_measure(struct nvme_tool *tool, struct case_data *priv)
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
    for (test_round = 1; test_round <= 100; test_round++)
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
NVME_CASE_SYMBOL(case_pcie_low_power_measure, "?");
