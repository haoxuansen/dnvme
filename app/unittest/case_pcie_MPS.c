
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

static uint32_t io_sq_id = 1;
static uint32_t io_cq_id = 1;
static uint32_t cq_size = DEFAULT_IO_QUEUE_SIZE;
static uint32_t sq_size = DEFAULT_IO_QUEUE_SIZE;
static uint32_t reap_num;
static uint32_t cmd_cnt = 0;

static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint32_t i = 0;
static struct create_cq_parameter cq_parameter = {0};
static struct create_sq_parameter sq_parameter = {0};

static char *disp_this_case = "this case will tests PCIe Max Payload Size\n";

static void set_pcie_mps_128(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
	uint8_t exp_oft = pdev->express.offset;
    uint32_t u32_tmp_data = 0;
    int ret;

    // RC set MPS 128
    //system("setpci -s 0:1b.4 48.b=0f");

    // EP set MPS 128
    ret = pci_read_config_dword(ndev->fd, exp_oft + 0x8, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0xFFFFFF1F;
    pci_write_config_data(ndev->fd, exp_oft + 0x8, 4, (uint8_t *)&u32_tmp_data);
    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);

    ret = pci_read_config_dword(ndev->fd, exp_oft + 0x8, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data = (u32_tmp_data & 0xE0) >> 5;
    //pr_info("\nread ndev->express.offset+0x8 0x%x\n", u32_tmp_data);
    //u32_tmp_data = pci_read_dword(g_fd, ndev->express.offset+0x4);
    //u32_tmp_data &= 0x07;
    pr_info("\nEP Max Payload Size support 128 byte, 0x%x\n", u32_tmp_data);
}

static void set_pcie_mps_256(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
	uint8_t exp_oft = pdev->express.offset;
	uint32_t u32_tmp_data = 0;
	int ret;

    // RC set MPS 256
    //system("setpci -s 0:1b.4 48.b=2f");

    // EP set MPS 256
    ret = pci_read_config_dword(ndev->fd, exp_oft + 0x8, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0xFFFFFF1F;
    u32_tmp_data |= 0x20;
    pci_write_config_data(ndev->fd, exp_oft + 0x8, 4, (uint8_t *)&u32_tmp_data);
    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);

    ret = pci_read_config_dword(ndev->fd, exp_oft + 0x8, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data = (u32_tmp_data & 0xE0) >> 5;
    pr_info("\nEP Max Payload Size support 256 byte, 0x%x\n", u32_tmp_data);
}

static void pcie_packet(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;

    pr_info("\nTest: Sending IO Read Command through sq_id %d\n", io_sq_id);
    wr_slba = 0;
    wr_nlb = 64;
    cmd_cnt = 0;
    for (i = 0; i < 10; i++)
    {
        nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
        cmd_cnt++;
    }
    pr_info("Ringing Doorbell for sq_id %d\n", io_sq_id);
    nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq reaped ok! reap_num:%d\n", reap_num);
}

static void test_sub(void)
{
    int cmds;

    /************************** 128 byte *********************/
    pr_info("\nMPS: 128 byte\n");
    set_pcie_mps_128();
    pcie_packet();
    scanf("%d", &cmds);

    /************************** 256 byte *********************/
    pr_info("\nMPS: 256 byte\n");
    set_pcie_mps_256();
    pcie_packet();
    scanf("%d", &cmds);
}

static int case_pcie_MPS(struct nvme_tool *tool, struct case_data *priv)
{
	int test_round = 0;
	uint32_t u32_tmp_data = 0;
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ctrl_property *prop = ctrl->prop;
	int ret;

    cq_size = NVME_CAP_MQES(prop->cap);
    sq_size = NVME_CAP_MQES(prop->cap);
    pr_info("\n********************\t %s \t********************\n", __FUNCTION__);
    pr_info("%s\n", disp_this_case);
    /**********************************************************************/

    pr_info("\nPreparing contig cq_id = %d, cq_size = %d\n", io_cq_id, cq_size);
    cq_parameter.cq_id = io_cq_id;
    cq_parameter.cq_size = cq_size;
    cq_parameter.contig = 1;
    cq_parameter.irq_en = 1;
    cq_parameter.irq_no = io_cq_id;
    test_flag |= create_iocq(ndev->fd, &cq_parameter);
    DBG_ON(test_flag < 0);
    pr_info("Ringing Doorbell for NVME_AQ_ID\n");
    nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_info("  cq reaped ok! reap_num:%d\n", reap_num);

    pr_info("\nPreparing contig sq_id = %d, assoc cq_id = %d, sq_size = %d\n", io_sq_id, io_cq_id, sq_size);
    sq_parameter.cq_id = io_cq_id;
    sq_parameter.sq_id = io_sq_id;
    sq_parameter.sq_size = sq_size;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;
    test_flag |= create_iosq(ndev->fd, &sq_parameter);
    DBG_ON(test_flag < 0);
    pr_info("Ringing Doorbell for NVME_AQ_ID\n");
    nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_info("  cq reaped ok! reap_num:%d\n", reap_num);

    // first displaly EP Max Payload Size support
    ret = pci_read_config_dword(ndev->fd, pdev->express.offset + 0x4, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0x07;
    if (u32_tmp_data == 0)
    {
        pr_info("\nEP Max Payload Size support 128 byte\n");
    }
    else if (u32_tmp_data == 1)
    {
        pr_info("\nEP Max Payload Size support 256 byte\n");
    }
    else if (u32_tmp_data == 2)
    {
        pr_info("\nEP Max Payload Size support 512 byte\n");
    }
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
NVME_CASE_SYMBOL(case_pcie_MPS, "?");
