#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "dnvme_ioctl.h"

#include "common.h"
#include "unittest.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "case_all.h"

static int test_flag = SUCCEED;
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

static uint32_t sub_case_pre(void);
static uint32_t sub_case_end(void);

static uint32_t sub_case_abort_1_wrd_cmd(void);
static uint32_t sub_case_random_abort_1_wrd_cmd(void);
static uint32_t sub_case_abort_2_wrd_cmd(void);
static uint32_t sub_case_abort_3_wrd_cmd(void);
static uint32_t sub_case_abort_4_wrd_cmd(void);

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

int case_queue_abort(void)
{
    uint32_t round_idx = 0;

    test_loop = 1;

    pr_info("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        pr_info("\ntest cnt: %d\n", round_idx);
        for (uint32_t index = 1; index <= g_nvme_dev.max_sq_num; index++)
        {
            test_flag = SUCCEED;
            io_sq_id = index;
            io_cq_id = index;
            sub_case_list_exe(&sub_case_header, sub_case_list, ARRAY_SIZE(sub_case_list));
        }
        if (FAILED == test_flag)
        {
            pr_err("test_flag == FAILED\n");
            break;
        }
    }
    return test_flag;
}

static uint32_t sub_case_pre(void)
{
    pr_info("==>QID:%d\n", io_sq_id);
    pr_color(LOG_COLOR_PURPLE, "  Create contig cq_id:%d, cq_size = %d\n", io_cq_id, cq_size);
    test_flag |= nvme_create_contig_iocq(g_fd, io_cq_id, cq_size, ENABLE, io_cq_id);

    pr_color(LOG_COLOR_PURPLE, "  Create contig sq_id:%d, assoc cq_id = %d, sq_size = %d\n", io_sq_id, io_cq_id, sq_size);
    test_flag |= nvme_create_contig_iosq(g_fd, io_sq_id, io_cq_id, sq_size, MEDIUM_PRIO);
    return test_flag;
}
static uint32_t sub_case_end(void)
{
    pr_color(LOG_COLOR_PURPLE, "  Deleting SQID:%d,CQID:%d\n", io_sq_id, io_cq_id);
    test_flag |= nvme_delete_ioq(g_fd, nvme_admin_delete_sq, io_sq_id);
    test_flag |= nvme_delete_ioq(g_fd, nvme_admin_delete_cq, io_cq_id);
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
static uint32_t sub_case_abort_1_wrd_cmd(void)
{
    wr_slba = 0;
    wr_nlb = WORD_RAND() % 255 + 1;
    cmd_cnt = 0;
    test_flag |= nvme_send_iocmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, g_write_buf);
    cmd_cnt++;
    // send abort cmd
    ioctl_send_abort(g_fd, io_sq_id, 0);

    test_flag |= ioctl_tst_ring_dbl(g_fd, io_sq_id);
    test_flag |= ioctl_tst_ring_dbl(g_fd, NVME_AQ_ID);

    /*test_flag |= */ cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num); // ststus shouldn't be 0!

    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
    return test_flag;
}

static uint32_t sub_case_random_abort_1_wrd_cmd(void)
{
    wr_slba = 0;
    wr_nlb = WORD_RAND() % 255 + 1;
    cmd_cnt = 0;
    for (uint32_t index = 0; index < 20; index++)
    {
        test_flag |= nvme_send_iocmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, g_write_buf);
        cmd_cnt++;
    }
    //random select 1 cmd (read or write) to abort
    test_flag |= ioctl_send_abort(g_fd, io_sq_id, WORD_RAND() % 4 + 9);

    test_flag |= ioctl_tst_ring_dbl(g_fd, io_sq_id);
    test_flag |= ioctl_tst_ring_dbl(g_fd, NVME_AQ_ID);

    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /*test_flag |= */ cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num); // ststus shouldn't be 0!

    return test_flag;
}

static uint32_t sub_case_abort_2_wrd_cmd(void)
{
    wr_slba = 0;
    wr_nlb = WORD_RAND() % 255 + 1;
    cmd_cnt = 0;
    send_num = ((WORD_RAND() % 300) + 100);
    for (uint32_t index = 0; index < send_num; index++)
    {
        test_flag |= nvme_send_iocmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, g_write_buf);
        cmd_cnt++;
        if (index == 25)
            ioctl_send_abort(g_fd, io_sq_id, index);
        if (index == 30)
            ioctl_send_abort(g_fd, io_sq_id, index);
    }
    test_flag |= ioctl_tst_ring_dbl(g_fd, io_sq_id);
    test_flag |= ioctl_tst_ring_dbl(g_fd, NVME_AQ_ID);

    test_flag |= cq_gain(NVME_AQ_ID, 2, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /*test_flag |=*/cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num); // ststus shouldn't be 0!
    return test_flag;
}

static uint32_t sub_case_abort_3_wrd_cmd(void)
{
    wr_slba = 0;
    wr_nlb = WORD_RAND() % 255 + 1;
    cmd_cnt = 0;
    send_num = ((WORD_RAND() % 300) + 100);
    for (uint32_t index = 0; index < send_num; index++)
    {
        test_flag |= nvme_send_iocmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, g_write_buf);
        cmd_cnt++;
    }
    // send abort cmd
    send_num = ((send_num) / 10) * 9;
    for (uint32_t index = send_num; index < send_num + 3; index++)
    {
        ioctl_send_abort(g_fd, io_sq_id, index);
    }
    test_flag |= ioctl_tst_ring_dbl(g_fd, io_sq_id);
    test_flag |= ioctl_tst_ring_dbl(g_fd, NVME_AQ_ID);

    test_flag |= cq_gain(NVME_AQ_ID, 3, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /*test_flag |=*/cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num); // ststus shouldn't be 0!
    return test_flag;
}

static uint32_t sub_case_abort_4_wrd_cmd(void)
{
    wr_slba = 0;
    wr_nlb = WORD_RAND() % 255 + 1;
    cmd_cnt = 0;
    send_num = ((WORD_RAND() % 300) + 100);
    for (uint32_t index = 0; index < send_num; index++)
    {
        test_flag |= nvme_send_iocmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, g_write_buf);
        cmd_cnt++;
    }
    // send abort cmd
    send_num = ((send_num) / 10) * 9;
    for (uint32_t index = send_num; index < send_num + 4; index++)
    {
        ioctl_send_abort(g_fd, io_sq_id, index);
    }
    test_flag |= ioctl_tst_ring_dbl(g_fd, io_sq_id);
    test_flag |= ioctl_tst_ring_dbl(g_fd, NVME_AQ_ID);

    test_flag |= cq_gain(NVME_AQ_ID, 4, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /*test_flag |=*/cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num); // ststus shouldn't be 0!
    return test_flag;
}
