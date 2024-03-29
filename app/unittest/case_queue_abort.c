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

static int test_flag = 0;
static uint32_t test_loop = 0;
static uint32_t cmd_cnt = 0;
static uint32_t io_sq_id = 1;
static uint32_t io_cq_id = 1;
static uint32_t cq_size = 1020;
static uint32_t sq_size = 1024;
static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint32_t reap_num = 0;
static uint32_t send_num = 0;

static int sub_case_pre(void);
static int sub_case_end(void);

static int sub_case_abort_1_wrd_cmd(void);
static int sub_case_random_abort_1_wrd_cmd(void);
static int sub_case_abort_2_wrd_cmd(void);
static int sub_case_abort_3_wrd_cmd(void);
static int sub_case_abort_4_wrd_cmd(void);

static SubCaseHeader_t sub_case_header = {
    "case_queue_abort",
    "This case will tests abort command",
    sub_case_pre,
    sub_case_end,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_abort_1_wrd_cmd, "send 1 io cmd, then abort it"),
    SUB_CASE(sub_case_random_abort_1_wrd_cmd, "send 10*2 io(write/read) cmd, then random abort one cmd"),
    SUB_CASE(sub_case_abort_2_wrd_cmd, "send random cnt io cmd, then abort 2 cmd"),
    SUB_CASE(sub_case_abort_3_wrd_cmd, "send random cnt io cmd, then abort 3 cmd"),
    SUB_CASE(sub_case_abort_4_wrd_cmd, "send random cnt io cmd, then abort 4 cmd"),
};

static int case_queue_abort(struct nvme_tool *tool, struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
    uint32_t round_idx = 0;

    test_loop = 1;

    pr_info("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        pr_info("\ntest cnt: %d\n", round_idx);
        for (uint32_t index = 1; index <= ctrl->nr_sq; index++)
        {
            test_flag = 0;
            io_sq_id = index;
            io_cq_id = index;
            sub_case_list_exe(&sub_case_header, sub_case_list, ARRAY_SIZE(sub_case_list));
        }
        if (-1 == test_flag)
        {
            pr_err("test_flag == -1\n");
            break;
        }
    }
    return test_flag;
}
NVME_CASE_SYMBOL(case_queue_abort, "?");
NVME_AUTOCASE_SYMBOL(case_queue_abort);

static int sub_case_pre(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;

    pr_info("==>QID:%d\n", io_sq_id);
    pr_color(LOG_N_PURPLE, "  Create contig cq_id:%d, cq_size = %d\n", io_cq_id, cq_size);
    test_flag |= nvme_create_contig_iocq(ndev->fd, io_cq_id, cq_size, 1, io_cq_id);
    DBG_ON(test_flag < 0);

    pr_color(LOG_N_PURPLE, "  Create contig sq_id:%d, assoc cq_id = %d, sq_size = %d\n", io_sq_id, io_cq_id, sq_size);
    test_flag |= nvme_create_contig_iosq(ndev->fd, io_sq_id, io_cq_id, sq_size, MEDIUM_PRIO);
    DBG_ON(test_flag < 0);
    return test_flag;
}
static int sub_case_end(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;

    pr_color(LOG_N_PURPLE, "  Deleting SQID:%d,CQID:%d\n", io_sq_id, io_cq_id);
    test_flag |= nvme_delete_ioq(ndev->fd, nvme_admin_delete_sq, io_sq_id);
    DBG_ON(test_flag < 0);
    test_flag |= nvme_delete_ioq(ndev->fd, nvme_admin_delete_cq, io_cq_id);
    DBG_ON(test_flag < 0);
    return test_flag;
}

/* !FIXME: The command ID assigned by the driver is not used when sending 
 * abort command! In addition, unless the SQ is recreated, the command ID
 * is always increamented!
 *
 * Required to fix this bug in function: sub_case_abort_1_wrd_cmd,
 * sub_case_random_abort_1_wrd_cmd, sub_case_abort_2_wrd_cmd,
 * sub_case_abort_3_wrd_cmd, sub_case_abort_4_wrd_cmd
 */
static int sub_case_abort_1_wrd_cmd(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;

    wr_slba = 0;
    wr_nlb = (uint16_t)rand() % 255 + 1;
    cmd_cnt = 0;
    test_flag |= nvme_send_iocmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, tool->wbuf);
    DBG_ON(test_flag < 0);
    cmd_cnt++;
    // send abort cmd
    ioctl_send_abort(ndev->fd, io_sq_id, 0);

    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    DBG_ON(test_flag < 0);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    DBG_ON(test_flag < 0);

    /*test_flag |= */ cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num); // ststus shouldn't be 0!

    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    DBG_ON(test_flag < 0);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
    return test_flag;
}

static int sub_case_random_abort_1_wrd_cmd(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;

    wr_slba = 0;
    wr_nlb = (uint16_t)rand() % 255 + 1;
    cmd_cnt = 0;
    for (uint32_t index = 0; index < 20; index++)
    {
        test_flag |= nvme_send_iocmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, tool->wbuf);
	DBG_ON(test_flag < 0);
        cmd_cnt++;
    }
    //random select 1 cmd (read or write) to abort
    test_flag |= ioctl_send_abort(ndev->fd, io_sq_id, (uint16_t)rand() % 4 + 9);
    DBG_ON(test_flag < 0);

    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    DBG_ON(test_flag < 0);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    DBG_ON(test_flag < 0);

    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    DBG_ON(test_flag < 0);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /*test_flag |= */ cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num); // ststus shouldn't be 0!

    return test_flag;
}

static int sub_case_abort_2_wrd_cmd(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;

    wr_slba = 0;
    wr_nlb = (uint16_t)rand() % 255 + 1;
    cmd_cnt = 0;
    send_num = (((uint16_t)rand() % 300) + 100);
    for (uint32_t index = 0; index < send_num; index++)
    {
        test_flag |= nvme_send_iocmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, tool->wbuf);
	DBG_ON(test_flag < 0);
        cmd_cnt++;
        if (index == 25)
            ioctl_send_abort(ndev->fd, io_sq_id, index);
        if (index == 30)
            ioctl_send_abort(ndev->fd, io_sq_id, index);
    }
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    DBG_ON(test_flag < 0);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    DBG_ON(test_flag < 0);

    test_flag |= cq_gain(NVME_AQ_ID, 2, &reap_num);
    DBG_ON(test_flag < 0);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /*test_flag |=*/cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num); // ststus shouldn't be 0!
    return test_flag;
}

static int sub_case_abort_3_wrd_cmd(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;

    wr_slba = 0;
    wr_nlb = (uint16_t)rand() % 255 + 1;
    cmd_cnt = 0;
    send_num = (((uint16_t)rand() % 300) + 100);
    for (uint32_t index = 0; index < send_num; index++)
    {
        test_flag |= nvme_send_iocmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, tool->wbuf);
	DBG_ON(test_flag < 0);
        cmd_cnt++;
    }
    // send abort cmd
    send_num = ((send_num) / 10) * 9;
    for (uint32_t index = send_num; index < send_num + 3; index++)
    {
        ioctl_send_abort(ndev->fd, io_sq_id, index);
    }
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    DBG_ON(test_flag < 0);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    DBG_ON(test_flag < 0);

    test_flag |= cq_gain(NVME_AQ_ID, 3, &reap_num);
    DBG_ON(test_flag < 0);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /*test_flag |=*/cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num); // ststus shouldn't be 0!
    return test_flag;
}

static int sub_case_abort_4_wrd_cmd(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;

    wr_slba = 0;
    wr_nlb = (uint16_t)rand() % 255 + 1;
    cmd_cnt = 0;
    send_num = (((uint16_t)rand() % 300) + 100);
    for (uint32_t index = 0; index < send_num; index++)
    {
        test_flag |= nvme_send_iocmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, tool->wbuf);
	DBG_ON(test_flag < 0);
        cmd_cnt++;
    }
    // send abort cmd
    send_num = ((send_num) / 10) * 9;
    for (uint32_t index = send_num; index < send_num + 4; index++)
    {
        ioctl_send_abort(ndev->fd, io_sq_id, index);
    }
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    DBG_ON(test_flag < 0);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    DBG_ON(test_flag < 0);

    test_flag |= cq_gain(NVME_AQ_ID, 4, &reap_num);
    DBG_ON(test_flag < 0);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /*test_flag |=*/cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num); // ststus shouldn't be 0!
    return test_flag;
}
