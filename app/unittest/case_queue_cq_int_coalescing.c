#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "dnvme.h"
#include "libnvme.h"

#include "common.h"
#include "unittest.h"
#include "test.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_irq.h"

static int test_flag = SUCCEED;
static uint32_t test_loop = 0;
static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint32_t reap_num = 0;

static int sub_case_cq_int_coalescing(void);

static SubCaseHeader_t sub_case_header = {
    "this case will tests CQ interrupt int_coalescing",
    "This case will tests all cq interrupt type,pin/msi_single/msi_multi/msix\r\n"
    "\033[33m!!!!!!!!!!!!!!! msi_multi test HOST BIOS MUST OPEN VTD!!!!!!!!!!!!!!!!!\033[0m",
    NULL,
    NULL,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_cq_int_coalescing, "tests all queue interrupt coalescing"),
};

uint8_t aggr_time = 0;
uint8_t aggr_thr = 0;

uint16_t int_vertor = 0;
uint16_t coals_disable = 0;


static int case_queue_cq_int_coalescing(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t round_idx = 0;

    test_loop = 10;

    // struct nvme_completion *cq_entry;
    // uint32_t reap_num;
    // /**********************************************************************/
    // cq_entry = send_get_feature(g_fd, NVME_FEAT_IRQ_COALESCE);
    // // must be admin queue
    // if (0 == cq_entry->sq_identifier)
    // {
    // 	aggr_thr = (uint8_t)(cq_entry->cmd_specifc & 0xFF);
    // 	aggr_time = (uint8_t)(cq_entry->cmd_specifc >> 8);
    // }
    // else
    // {
    // 	pr_err("acq_num or sq_id is wrong!!!\n");
    // }

    // /**********************************************************************/
    // cq_entry = send_get_feature(g_fd, NVME_FEAT_IRQ_CONFIG);
    // // must be admin queue
    // if (0 == cq_entry->sq_identifier)
    // {
    // 	int_vertor = (uint16_t)(cq_entry->cmd_specifc & 0xFFFF);
    // 	coals_disable = (uint16_t)((cq_entry->cmd_specifc >> 16)&0x1);
    // }
    // else
    // {
    // 	pr_err("acq_num or sq_id is wrong!!!\n");
    // }
    // /**********************************************************************/

    // pr_info("aggr_time: %d\n", aggr_time);
    // pr_info("aggr_thr: %d\n", aggr_thr);
    // pr_info("int_vertor: %d\n", int_vertor);
    // pr_info("coals_disable: %d\n", coals_disable);

    pr_info("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        pr_info("\ntest cnt: %d\n", round_idx);
        sub_case_list_exe(&sub_case_header, sub_case_list, ARRAY_SIZE(sub_case_list));
        if (FAILED == test_flag)
        {
            pr_err("test_flag == FAILED\n");
            break;
        }
    }
    nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSIX);
    return test_flag;
}
NVME_CASE_SYMBOL(case_queue_cq_int_coalescing, "?");
NVME_AUTOCASE_SYMBOL(case_queue_cq_int_coalescing);

static int sub_case_cq_int_coalescing(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    struct nvme_sq_info *sqs = ndev->iosqs;
	enum nvme_irq_type type;
	int ret;
    uint32_t index = 0;
    uint8_t queue_num = BYTE_RAND() % ndev->max_sq_num + 1;
    wr_slba = DWORD_RAND() % (ndev->nss[0].nsze / 2);
    wr_nlb = WORD_RAND() % 255 + 1;

	type = nvme_select_irq_type_random();
	ret = nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, type);
	if (ret < 0)
		return ret;

    ret = nvme_create_all_ioq(ndev, 0);
    if (ret < 0)
        return ret;

    /**********************************************************************/
    aggr_time = 255; // max
    aggr_thr = 5;    // 0' beaed
    test_flag |= nvme_set_feature_cmd(ndev->fd, wr_nsid, NVME_FEAT_IRQ_COALESCE, ((aggr_time << 8) | aggr_thr), 0);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, 0);
    test_flag |= cq_gain(0, 1, &reap_num);
    /**********************************************************************/
    for (uint16_t i = 0; i < queue_num; i++)
    {
        int_vertor = sqs[i].cqid;
        coals_disable = 0;
        test_flag |= nvme_set_feature_cmd(ndev->fd, wr_nsid, NVME_FEAT_IRQ_CONFIG, int_vertor, coals_disable);
    }
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, 0);
    test_flag |= cq_gain(0, queue_num, &reap_num);
    /**********************************************************************/

    for (uint16_t i = 0; i < queue_num; i++)
    {
        sqs[i].cmd_cnt = 0;

        for (index = 0; index < 200; index++)
        {
            if ((wr_slba + wr_nlb) < ndev->nss[0].nsze)
            {
                test_flag |= nvme_send_iocmd(ndev->fd, 0, sqs[i].sqid, wr_nsid, wr_slba, wr_nlb, tool->wbuf);
                sqs[i].cmd_cnt++;
            }
        }
    }
    for (uint16_t i = 0; i < queue_num; i++)
    {
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, sqs[i].sqid);
    }
    for (uint16_t i = 0; i < queue_num; i++)
    {
        // io_cq_id = sqs[i].cq_id;
        // test_flag |= cq_gain(io_cq_id, sqs[i].cmd_cnt, &reap_num);
        test_flag |= cq_gain_disp_cq(sqs[i].cqid, sqs[i].cmd_cnt, &reap_num, false);
    }

    nvme_delete_all_ioq(ndev);
    return test_flag;
}
