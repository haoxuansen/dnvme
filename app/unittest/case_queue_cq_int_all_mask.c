#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>

#include "dnvme.h"
#include "libbase.h"
#include "libnvme.h"

#include "test.h"
#include "unittest.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_irq.h"

static int test_flag = 0;
static uint32_t test_loop = 0;
static uint32_t io_sq_id = 1;
static uint32_t io_cq_id = 1;
static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint32_t reap_num = 0;

static int sub_case_int_queue_mask(void);
static int sub_case_pending_bit(void);

static SubCaseHeader_t sub_case_header = {
    "case_queue_cq_int_all",
    "This case will tests all cq interrupt (pin/msi_single/msi_multi/msix) mask\r\n"
    "\033[33m!!!!!!!!!!!!!!! msi_multi test HOST BIOS MUST OPEN VTD!!!!!!!!!!!!!!!!!\033[0m",
    NULL,
    NULL,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_int_queue_mask, "tests all type interrupt mask feature"),
    SUB_CASE(sub_case_pending_bit, "tests all pending bit"),
};

static int case_queue_cq_int_all_mask(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t round_idx = 0;

    test_loop = 10;

    pr_info("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        pr_info("\ntest cnt: %d\n", round_idx);
        sub_case_list_exe(&sub_case_header, sub_case_list, ARRAY_SIZE(sub_case_list));
        if (-1 == test_flag)
        {
            pr_err("test_flag == -1\n");
            break;
        }
    }
    nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSIX);
    return test_flag;
}
NVME_CASE_SYMBOL(case_queue_cq_int_all_mask, "?");
NVME_AUTOCASE_SYMBOL(case_queue_cq_int_all_mask);

static int sub_case_int_queue_mask(void)
{
	uint32_t index = 0;
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sqs = ndev->iosqs;
	enum nvme_irq_type type;
	uint64_t nsze;
	uint16_t i;
	uint8_t queue_num;
	int ret;

	type = nvme_select_irq_type_random();
	ret = nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, type);
	if (ret < 0)
		return ret;

	ret = nvme_create_all_ioq(ndev, 0);
	if (ret < 0)
		return ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0)
		return ret;

	queue_num = (uint8_t)rand() % ctrl->nr_sq + 1;

	wr_slba = (uint32_t)rand() % (nsze / 2);
	wr_nlb = (uint16_t)rand() % 255 + 1;
	for (i = 0; i < queue_num; i++)
	{
		io_sq_id = sqs[i].sqid;
		sqs[i].cmd_cnt = 0;

		for (index = 0; index < 200; index++)
		{
			if ((wr_slba + wr_nlb) < nsze)
			{
				test_flag |= nvme_send_iocmd(ndev->fd, true, io_sq_id, wr_nsid, wr_slba, wr_nlb, tool->wbuf);
				sqs[i].cmd_cnt++;
			}
		}
	}
	for (i = 0; i < queue_num; i++)
	{
		io_sq_id = sqs[i].sqid;
		io_cq_id = sqs[i].cqid;
		test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
		nvme_mask_irq(ndev->fd, io_cq_id);  /////////////////////////////////////////////////nvme_mask_irq
	}
	usleep(10000);
	for (i = 0; i < queue_num; i++)
	{
		io_cq_id = sqs[i].cqid;
		nvme_unmask_irq(ndev->fd, io_cq_id); /////////////////////////////////////////////////nvme_unmask_irq
		test_flag |= cq_gain(io_cq_id, sqs[i].cmd_cnt, &reap_num);
	}
	nvme_delete_all_ioq(ndev);
	return test_flag;
}

int msi_cap_access(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    int ret_val = -1;
    struct pcie_msi_cap msi_cap;

    ret_val = pci_read_config_data(ndev->fd, ndev->msi.offset, sizeof(struct pcie_msi_cap), &msi_cap);

    pr_info("\nmsi_cap_id: %#x\n",msi_cap.cap_id);
    pr_info("msg_ctrl: %#x\n",msi_cap.msg_ctrl);
    pr_info("msg_adrl: %#x\n",msi_cap.msg_adrl);
    pr_info("msg_dat: %#x\n",msi_cap.msg_dat);
    pr_info("mask_bit: %#x\n",msi_cap.mask_bit);
    pr_info("pending_bit: %#x\n",msi_cap.pending_bit);

    return ret_val;
}


static int sub_case_pending_bit(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sqs = ndev->iosqs;
	enum nvme_irq_type type;
	uint64_t nsze;
	uint8_t queue_num;
	int ret;

	type = nvme_select_irq_type_random();
	ret = nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, type);
	if (ret < 0)
		return ret;

	ret = nvme_create_all_ioq(ndev, 0);
	if (ret < 0)
		return ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0)
		return ret;

	queue_num = (uint8_t)rand() % ctrl->nr_sq + 1;

	wr_slba = (uint32_t)rand() % (nsze / 2);
	wr_nlb = (uint16_t)rand() % 255 + 1;
	for (uint16_t i = 0; i < queue_num; i++)
	{
		io_sq_id = sqs[i].sqid;
		sqs[i].cmd_cnt = 0;

		for (uint32_t index = 0; index < 200; index++)
		{
			if ((wr_slba + wr_nlb) < nsze)
			{
				test_flag |= nvme_send_iocmd(ndev->fd, true, io_sq_id, wr_nsid, wr_slba, wr_nlb, tool->wbuf);
				sqs[i].cmd_cnt++;
			}
		}
	}
	for (uint16_t i = 0; i < queue_num; i++)
	{
		io_sq_id = sqs[i].sqid;
		test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
		nvme_mask_irq(ndev->fd, io_cq_id);  /////////////////////////////////////////////////nvme_mask_irq
	}
	sleep(10);
	for (uint16_t i = 0; i < queue_num; i++)
	{
		io_cq_id = sqs[i].cqid;
		if( (ndev->irq_type == NVME_INT_MSI_SINGLE)||(ndev->irq_type == NVME_INT_MSI_MULTI) )
		{
			msi_cap_access();
		}
		nvme_unmask_irq(ndev->fd, io_cq_id); /////////////////////////////////////////////////nvme_unmask_irq
		test_flag |= cq_gain(io_cq_id, sqs[i].cmd_cnt, &reap_num);
	}

	nvme_delete_all_ioq(ndev);
	return test_flag;
}
