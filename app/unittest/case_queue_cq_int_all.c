#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>

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
static uint32_t cmd_cnt = 0;
static uint32_t io_sq_id = 1;
static uint32_t io_cq_id = 1;
static uint32_t cq_size = 1024;
static uint32_t sq_size = 1024;
static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint32_t reap_num = 0;

static int sub_case_int_pin(void);
static int sub_case_int_msi_single(void);
static int sub_case_int_msi_multi(void);
static int sub_case_int_msix(void);
static int sub_case_int_n_queue(void);
static int sub_case_multi_cq_map_one_int_vct(void);

static SubCaseHeader_t sub_case_header = {
    "case_queue_cq_int_all",
    "This case will tests all cq interrupt type,pin/msi_single/msi_multi/msix\r\n"
    "\033[33m!!!!!!!!!!!!!!! msi_multi test HOST BIOS MUST OPEN VTD!!!!!!!!!!!!!!!!!\033[0m",
    NULL,
    NULL,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_int_pin, "tests all queue'pin interrupt, send admin and io cmd"),
    SUB_CASE(sub_case_int_msi_single, "tests all queue'msi_single interrupt, send admin and io cmd"),
    SUB_CASE(sub_case_int_msi_multi, "tests all queue'msi_multi interrupt, send admin and io cmd"),
    SUB_CASE(sub_case_int_msix, "tests all queue'msix interrupt, send admin and io cmd"),
    SUB_CASE(sub_case_int_n_queue, "tests all queue interrupt"),
    SUB_CASE(sub_case_multi_cq_map_one_int_vct, "tests multi_cq_map_one_int_vct"),
};

static int case_queue_cq_int_all(struct nvme_tool *tool, struct case_data *priv)
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
NVME_CASE_SYMBOL(case_queue_cq_int_all, "?");
NVME_AUTOCASE_SYMBOL(case_queue_cq_int_all);

static int sub_case_int_pin(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint32_t index = 0;
	struct create_cq_parameter cq_parameter = {0};
	struct create_sq_parameter sq_parameter = {0};
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0)
		return ret;

	nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_PIN);

	cq_parameter.cq_id = io_cq_id;
	cq_parameter.cq_size = cq_size;
	cq_parameter.contig = 1;
	cq_parameter.irq_en = 1;
	cq_parameter.irq_no = 0; // 0's based
	pr_info("create cq: %d, irq_no:%d\n", io_cq_id, cq_parameter.irq_no);
	test_flag |= create_iocq(ndev->fd, &cq_parameter);
	DBG_ON(test_flag < 0);
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
	DBG_ON(test_flag < 0);
	pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

	pr_info("create sq: %d, assoc cq: %d\n", io_sq_id, io_cq_id);
	sq_parameter.cq_id = io_cq_id;
	sq_parameter.sq_id = io_sq_id;
	sq_parameter.sq_size = sq_size;
	sq_parameter.contig = 1;
	sq_parameter.sq_prio = MEDIUM_PRIO;
	test_flag |= create_iosq(ndev->fd, &sq_parameter);
	DBG_ON(test_flag < 0);
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
	DBG_ON(test_flag < 0);
	pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

	/**********************************************************************/
	wr_slba = 0;
	wr_nlb = 128;
	cmd_cnt = 0;
	for (index = 0; index < 100; index++)
	{
	wr_slba = (uint32_t)rand() % (nsze / 2);
	wr_nlb = (uint16_t)rand() % 255 + 1;
	if ((wr_slba + wr_nlb) < nsze)
	{
		test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
		DBG_ON(test_flag < 0);
		cmd_cnt++;
		test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
		DBG_ON(test_flag < 0);
		cmd_cnt++;
	}
	}
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
	DBG_ON(test_flag < 0);

	/**********************************************************************/
	test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_sq, io_sq_id);
	DBG_ON(test_flag < 0);
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
	DBG_ON(test_flag < 0);
	pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

	test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_cq, io_cq_id);
	DBG_ON(test_flag < 0);
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
	DBG_ON(test_flag < 0);
	pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
	/**********************************************************************/
	return test_flag;
}

static int sub_case_int_msi_single(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint32_t index = 0;
	struct create_cq_parameter cq_parameter = {0};
	struct create_sq_parameter sq_parameter = {0};
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0)
		return ret;

	nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSI_SINGLE);

	cq_parameter.cq_id = io_cq_id;
	cq_parameter.cq_size = cq_size;
	cq_parameter.contig = 1;
	cq_parameter.irq_en = 1;
	cq_parameter.irq_no = 0; // 0's based
	pr_info("create cq: %d, irq_no:%d\n", io_cq_id, cq_parameter.irq_no);
	test_flag |= create_iocq(ndev->fd, &cq_parameter);
	DBG_ON(test_flag < 0);
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
	DBG_ON(test_flag < 0);
	pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

	pr_info("create sq: %d, assoc cq: %d\n", io_sq_id, io_cq_id);
	sq_parameter.cq_id = io_cq_id;
	sq_parameter.sq_id = io_sq_id;
	sq_parameter.sq_size = sq_size;
	sq_parameter.contig = 1;
	sq_parameter.sq_prio = MEDIUM_PRIO;
	test_flag |= create_iosq(ndev->fd, &sq_parameter);
	DBG_ON(test_flag < 0);
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
	DBG_ON(test_flag < 0);
	pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

	/**********************************************************************/
	wr_slba = 0;
	wr_nlb = 128;
	cmd_cnt = 0;
	for (index = 0; index < 100; index++)
	{
		wr_slba = (uint32_t)rand() % (nsze / 2);
		wr_nlb = (uint16_t)rand() % 255 + 1;
		if ((wr_slba + wr_nlb) < nsze)
		{
			test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
			DBG_ON(test_flag < 0);
			cmd_cnt++;
			test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
			DBG_ON(test_flag < 0);
			cmd_cnt++;
		}
	}
	if (cmd_cnt > 0)
	{
		test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
		DBG_ON(test_flag < 0);
		test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
		DBG_ON(test_flag < 0);
	}

	/**********************************************************************/
	test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_sq, io_sq_id);
	DBG_ON(test_flag < 0);
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
	DBG_ON(test_flag < 0);
	pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

	test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_cq, io_cq_id);
	DBG_ON(test_flag < 0);
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
	DBG_ON(test_flag < 0);
	pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
	/**********************************************************************/
	return test_flag;
}

static int sub_case_int_msi_multi(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint32_t index = 0;
#ifdef AMD_MB_EN
	return SKIPED;
#endif
	struct create_cq_parameter cq_parameter = {0};
	struct create_sq_parameter sq_parameter = {0};
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0)
		return ret;

	nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSI_MULTI);

	/**********************************************************************/
	cq_parameter.cq_id = io_cq_id;
	cq_parameter.cq_size = cq_size;
	cq_parameter.contig = 1;
	cq_parameter.irq_en = 1;
	cq_parameter.irq_no = io_cq_id;
	pr_info("create cq: %d, irq_no:%d\n", io_cq_id, cq_parameter.irq_no);
	test_flag |= create_iocq(ndev->fd, &cq_parameter);
	DBG_ON(test_flag < 0);
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
	DBG_ON(test_flag < 0);
	pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

	pr_info("create sq: %d, assoc cq: %d\n", io_sq_id, io_cq_id);
	sq_parameter.cq_id = io_cq_id;
	sq_parameter.sq_id = io_sq_id;
	sq_parameter.sq_size = sq_size;
	sq_parameter.contig = 1;
	sq_parameter.sq_prio = MEDIUM_PRIO;
	test_flag |= create_iosq(ndev->fd, &sq_parameter);
	DBG_ON(test_flag < 0);
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
	DBG_ON(test_flag < 0);
	pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
	/**********************************************************************/
	wr_slba = 0;
	wr_nlb = 128;
	cmd_cnt = 0;
	for (index = 0; index < 100; index++)
	{
		wr_slba = (uint32_t)rand() % (nsze / 2);
		wr_nlb = (uint16_t)rand() % 255 + 1;
		if ((wr_slba + wr_nlb) < nsze)
		{
			test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
			DBG_ON(test_flag < 0);
			cmd_cnt++;
			test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
			DBG_ON(test_flag < 0);
			cmd_cnt++;
		}
	}
	if (cmd_cnt > 0)
	{
		test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
		DBG_ON(test_flag < 0);
		test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
		DBG_ON(test_flag < 0);
	}
	//pr_info("  cq:%d io cmd reaped ok! reap_num:%d\n", io_cq_id, reap_num);

	/**********************************************************************/
	test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_sq, io_sq_id);
	DBG_ON(test_flag < 0);
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
	DBG_ON(test_flag < 0);
	pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

	test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_cq, io_cq_id);
	DBG_ON(test_flag < 0);
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
	DBG_ON(test_flag < 0);
	pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
	return test_flag;
}

static int sub_case_int_msix(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint32_t index = 0;
	struct create_cq_parameter cq_parameter = {0};
	struct create_sq_parameter sq_parameter = {0};
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0)
		return ret;

	nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSIX);

	/**********************************************************************/
	cq_parameter.cq_id = io_cq_id;
	cq_parameter.cq_size = cq_size;
	cq_parameter.contig = 1;
	cq_parameter.irq_en = 1;
	cq_parameter.irq_no = io_cq_id;
	pr_info("create cq: %d, irq_no:%d\n", io_cq_id, cq_parameter.irq_no);
	test_flag |= create_iocq(ndev->fd, &cq_parameter);
	DBG_ON(test_flag < 0);
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
	DBG_ON(test_flag < 0);
	pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

	pr_info("create sq: %d, assoc cq: %d\n", io_sq_id, io_cq_id);
	sq_parameter.cq_id = io_cq_id;
	sq_parameter.sq_id = io_sq_id;
	sq_parameter.sq_size = sq_size;
	sq_parameter.contig = 1;
	sq_parameter.sq_prio = MEDIUM_PRIO;
	test_flag |= create_iosq(ndev->fd, &sq_parameter);
	DBG_ON(test_flag < 0);
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
	DBG_ON(test_flag < 0);
	pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
	/**********************************************************************/
	wr_slba = 0;
	wr_nlb = 128;
	cmd_cnt = 0;
	for (index = 0; index < 100; index++)
	{
		wr_slba = (uint32_t)rand() % (nsze / 2);
		wr_nlb = (uint16_t)rand() % 255 + 1;
		if ((wr_slba + wr_nlb) < nsze)
		{
			test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
			DBG_ON(test_flag < 0);
			cmd_cnt++;
			test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
			DBG_ON(test_flag < 0);
			cmd_cnt++;
		}
	}
	if (cmd_cnt > 0)
	{
		test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
		DBG_ON(test_flag < 0);
		test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
		DBG_ON(test_flag < 0);
	}
	/**********************************************************************/
	test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_sq, io_sq_id);
	DBG_ON(test_flag < 0);
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
	DBG_ON(test_flag < 0);
	pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

	test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_cq, io_cq_id);
	DBG_ON(test_flag < 0);
	test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	DBG_ON(test_flag < 0);
	test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
	DBG_ON(test_flag < 0);
	pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
	return test_flag;
}

static int sub_case_int_n_queue(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sqs = ndev->iosqs;
	uint32_t index = 0;
	enum nvme_irq_type type;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0)
		return ret;

	type = nvme_select_irq_type_random();
	ret = nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, type);
	if (ret < 0)
		return ret;

	ret = nvme_create_all_ioq(ndev, 0);
	if (ret < 0)
		return ret;

	uint8_t queue_num = (uint8_t)rand() % ctrl->nr_sq + 1;

	wr_slba = (uint32_t)rand() % (nsze / 2);
	wr_nlb = (uint16_t)rand() % 255 + 1;
	for (uint16_t i = 0; i < queue_num; i++)
	{
		io_sq_id = sqs[i].sqid;
		sqs[i].cmd_cnt = 0;

		for (index = 0; index < 100; index++)
		{
			if ((wr_slba + wr_nlb) < nsze)
			{
				test_flag |= nvme_send_iocmd(ndev->fd, true, io_sq_id, wr_nsid, wr_slba, wr_nlb, tool->wbuf);
				DBG_ON(test_flag < 0);
				sqs[i].cmd_cnt++;
			}
		}
	}
	for (uint16_t i = 0; i < queue_num; i++)
	{
		io_sq_id = sqs[i].sqid;
		test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
		DBG_ON(test_flag < 0);
	}
	for (uint16_t i = 0; i < queue_num; i++)
	{
		io_cq_id = sqs[i].cqid;
		test_flag |= cq_gain(io_cq_id, sqs[i].cmd_cnt, &reap_num);
		DBG_ON(test_flag < 0);
	}

	nvme_delete_all_ioq(ndev);
	return test_flag;
}

static int sub_case_multi_cq_map_one_int_vct(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct nvme_sq_info *sqs = ndev->iosqs;
	uint32_t index = 0;
	enum nvme_irq_type type;
	uint64_t nsze;
	int ret;

	ret = nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
	if (ret < 0)
		return ret;

	type = nvme_select_irq_type_random();
	ret = nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, type);
	if (ret < 0)
		return ret;

	ret = nvme_create_all_ioq(ndev, NVME_CIOQ_F_CQS_BIND_SINGLE_IRQ);
	if (ret < 0)
		return ret;

	uint8_t queue_num = (uint8_t)rand() % ctrl->nr_sq + 1;

	wr_slba = (uint32_t)rand() % (nsze / 2);
	wr_nlb = (uint16_t)rand() % 255 + 1;
	for (uint16_t i = 0; i < queue_num; i++)
	{
		io_sq_id = sqs[i].sqid;
		sqs[i].cmd_cnt = 0;

		for (index = 0; index < 200; index++)
		{
			if ((wr_slba + wr_nlb) < nsze)
			{
				test_flag |= nvme_send_iocmd(ndev->fd, true, io_sq_id, wr_nsid, wr_slba, wr_nlb, tool->wbuf);
				DBG_ON(test_flag < 0);
				sqs[i].cmd_cnt++;
			}
		}
	}
	for (uint16_t i = 0; i < queue_num; i++)
	{
		io_sq_id = sqs[i].sqid;
		test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
		DBG_ON(test_flag < 0);
	}
	for (uint16_t i = 0; i < queue_num; i++)
	{
		io_cq_id = sqs[i].cqid;
		test_flag |= cq_gain(io_cq_id, sqs[i].cmd_cnt, &reap_num);
		DBG_ON(test_flag < 0);
	}

	nvme_delete_all_ioq(ndev);
	return test_flag;
}
