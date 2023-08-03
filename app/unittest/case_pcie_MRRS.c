
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>

#include "dnvme.h"
#include "libnvme.h"

#include "common.h"
#include "test.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_irq.h"

static int test_flag = SUCCEED;

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

static char *disp_this_case = "this case will tests PCIe Max Read Request Size\n";

static void set_pcie_mrrs_128(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t u32_tmp_data = 0;
    int ret;
    // EP set MRRS 128
    ret = pci_read_config_dword(ndev->fd, ndev->express.offset + 0x8, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0xFFFF8FFF;
    pci_write_config_data(ndev->fd, ndev->express.offset + 0x8, 4, (uint8_t *)&u32_tmp_data);
    pcie_retrain_link(RC_CAP_LINK_CONTROL);
}

static void set_pcie_mrrs_256(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t u32_tmp_data = 0;
    int ret;
    // EP set MRRS 256
    ret = pci_read_config_dword(ndev->fd, ndev->express.offset + 0x8, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0xFFFF8FFF;
    u32_tmp_data |= 0x1000;
    pci_write_config_data(ndev->fd, ndev->express.offset + 0x8, 4, (uint8_t *)&u32_tmp_data);
    pcie_retrain_link(RC_CAP_LINK_CONTROL);
}

static void set_pcie_mrrs_512(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t u32_tmp_data = 0;
    int ret;
    // EP set MRRS 512
    ret = pci_read_config_dword(ndev->fd, ndev->express.offset + 0x8, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0xFFFF8FFF;
    u32_tmp_data |= 0x2000;
    pci_write_config_data(ndev->fd, ndev->express.offset + 0x8, 4, (uint8_t *)&u32_tmp_data);
    pcie_retrain_link(RC_CAP_LINK_CONTROL);
}

static void pcie_packet(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;

    pr_info("\nTest: Sending IO Write Command through sq_id %d\n", io_sq_id);
    wr_slba = 0;
    wr_nlb = 64;
    cmd_cnt = 0;
    for (i = 0; i < 1; i++)
    {
        nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
        cmd_cnt++;
        //nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
        //cmd_cnt++;
    }
    pr_info("Ringing Doorbell for sq_id %d\n", io_sq_id);
    nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq reaped ok! reap_num:%d\n", reap_num);
}

static void set_pcie_rcb_64(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t u32_tmp_data = 0;
    int ret;

    // RC set RCB 64
    //system("setpci -s 0:1b.4 50.b=40");

    // EP set RCB 64
    ret = pci_read_config_dword(ndev->fd, ndev->express.offset + 0x10, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0xFFFFFFF7;
    pci_write_config_data(ndev->fd, ndev->express.offset + 0x10, 4, (uint8_t *)&u32_tmp_data);
    pcie_retrain_link(RC_CAP_LINK_CONTROL);

    ret = pci_read_config_dword(ndev->fd, ndev->express.offset + 0x10, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data = (u32_tmp_data & 0x8) >> 3;
    pr_info("\nRCB: 64 boundary 0x%x\n", u32_tmp_data);
}

static void set_pcie_rcb_128(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t u32_tmp_data = 0;
    int ret;

    // RC set RCB 128
    //system("setpci -s 0:1b.4 50.b=48");

    // EP set RCB 128
    ret = pci_read_config_dword(ndev->fd, ndev->express.offset + 0x10, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);

    u32_tmp_data &= 0xFFFFFFF7;
    u32_tmp_data |= 0x08;
    pci_write_config_data(ndev->fd, ndev->express.offset + 0x10, 4, (uint8_t *)&u32_tmp_data);
    pcie_retrain_link(RC_CAP_LINK_CONTROL);
    
    ret = pci_read_config_dword(ndev->fd, ndev->express.offset + 0x10, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    u32_tmp_data = (u32_tmp_data & 0x8) >> 3;
    pr_info("\nRCB: 128 boundary 0x%x\n", u32_tmp_data);
}

static void test_sub(void)
{
    int cmds;
    //set_pcie_rcb_128();
    /************************** 128 byte *********************/
    pr_info("\nMRRS: 128 byte\n");
    set_pcie_mrrs_128();
    pcie_packet();
    scanf("%d", &cmds);
    /************************** 256 byte *********************/
    pr_info("\nMRRS: 256 byte\n");
    set_pcie_mrrs_256();
    pcie_packet();
    scanf("%d", &cmds);
    /************************** 512 byte *********************/

    pr_info("\nMRRS: 512 byte\n");
    set_pcie_mrrs_512();
    pcie_packet();
    scanf("%d", &cmds);

    pr_info("\nre MRRS: 256 byte\n");
    set_pcie_mrrs_256();
    pcie_packet();
    scanf("%d", &cmds);
    /************************** RCB 64 *********************/
    pr_info("\nRCB: 64 boundary\n");
    set_pcie_rcb_64();
    pcie_packet();
    scanf("%d", &cmds);

    /************************** RCB 128 *********************/
    pr_info("\nRCB: 128 boundary\n");
    set_pcie_rcb_128();
    pcie_packet();
    scanf("%d", &cmds);
}

int case_pcie_MRRS(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
    struct nvme_ctrl_property *prop = &ndev->prop;
    int ret;
    int test_round = 0;
    uint32_t u32_tmp_data = 0;

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
    pr_info("Ringing Doorbell for NVME_AQ_ID\n");
    nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_info("  cq reaped ok! reap_num:%d\n", reap_num);

    // first displaly EP Max Read Request Size
    ret = pci_read_config_dword(ndev->fd, ndev->express.offset + 0x8, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    u32_tmp_data = (u32_tmp_data >> 12) & 0x7;
    if (u32_tmp_data == 0)
    {
        pr_info("\nEP Max Read Request Size  128 byte\n");
    }
    else if (u32_tmp_data == 1)
    {
        pr_info("\nEP Max Read Request Size  256 byte\n");
    }
    else if (u32_tmp_data == 2)
    {
        pr_info("\nEP Max Read Request Size  512 byte\n");
    }
    usleep(200000);

    for (test_round = 1; test_round <= 10000; test_round++)
    {
        //pr_info("\nlink status: %d\n", test_round);
        test_sub();
        if (test_flag)
        {
            break;
        }
    }

    nvme_display_test_result(test_flag != SUCCEED ? -EPERM : 0, __func__);
    return test_flag;
}
