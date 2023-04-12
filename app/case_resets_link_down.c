#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "dnvme.h"

#include "queue.h"
#include "pcie.h"

#include "common.h"
#include "test.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_irq.h"

static int test_flag = SUCCEED;

static char *disp_this_case = "this case will tests link down reset\n"
                              "this case will send 100 IO cmd, wait 500 cmds back, then link down\n";

static void test_sub(void)
{
    uint32_t index = 0;
    uint32_t cmd_cnt = 0;
    uint32_t io_sq_id = 1;
    uint32_t io_cq_id = 1;
    uint32_t cq_size = 4096;
    uint32_t sq_size = 4096;
    uint64_t wr_slba = 0;
    uint16_t wr_nlb = 0;
    uint32_t wr_nsid = 1;

	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    struct create_cq_parameter cq_parameter = {0};
    struct create_sq_parameter sq_parameter = {0};
    uint32_t reap_num = 0;

    /**********************************************************************/
    io_sq_id = BYTE_RAND() % ndev->max_sq_num + 1;
    pr_info("create SQ %d\n", io_sq_id);
    /**********************************************************************/
    cq_parameter.cq_id = io_cq_id;
    cq_parameter.cq_size = cq_size;
    cq_parameter.contig = 1;
    cq_parameter.irq_en = 1;
    cq_parameter.irq_no = io_cq_id;
    test_flag |= create_iocq(ndev->fd, &cq_parameter);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    sq_parameter.cq_id = io_cq_id;
    sq_parameter.sq_id = io_sq_id;
    sq_parameter.sq_size = sq_size;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;
    test_flag |= create_iosq(ndev->fd, &sq_parameter);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
    /**********************************************************************/
    wr_slba = 0;
    wr_nlb = 8;
    cmd_cnt = 0;
    /**********************************************************************/
    for (index = 0; index < (DWORD_RAND() % (sq_size - 2)) / 2 + 100; index++)
    {
        wr_slba = DWORD_RAND() % (ndev->nss[0].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        if (wr_slba + wr_nlb < ndev->nss[0].nsze)
        {
            test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
            cmd_cnt++;
            test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
            cmd_cnt++;
        }
    }
    pr_info("send cmd num: %d\n", cmd_cnt);
    /**********************************************************************/
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    /**********************************************************************/
    //reap cq
    test_flag |= cq_gain(io_cq_id, (DWORD_RAND() % (cmd_cnt + 1)), &reap_num);
    pr_info("reaped cq num: %d\n", reap_num);

    /************************** Issue link down *********************/
    pr_info("\nIssue link down\n");

    pcie_do_link_down(ndev->fd);

    sleep(1);

    nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSIX);

    /*******************************************************************************************************************************/
}

int case_resets_link_down(struct nvme_tool *tool)
{
    int test_round = 0;
    pr_info("\n********************\t %s \t********************\n", __FUNCTION__);
    pr_info("%s\n", disp_this_case);
    /**********************************************************************/
    for (test_round = 1; test_round <= 2; test_round++) //reset just can run 1 times!!!!!!
    {
        pr_info("\ntest_round: %d\n", test_round);
        test_sub();
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
