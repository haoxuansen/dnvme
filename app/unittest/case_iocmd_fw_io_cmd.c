#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "dnvme.h"
#include "libnvme.h"

#include "common.h"
#include "test.h"
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
static uint32_t cq_size = 1024;
static uint32_t sq_size = 1024;
static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint32_t reap_num = 0;

static int sub_case_pre(void);
static int sub_case_end(void);

static int sub_case_fwio_flush(void);
static int sub_case_fwio_write_unc(void);
static int sub_case_fwio_write_zero(void);
static int sub_case_trim_cmd(void);

static SubCaseHeader_t sub_case_header = {
    "case_iocmd_fw_io_cmd",
    "This case will tests nvme fw io cmd",
    sub_case_pre,
    sub_case_end,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_fwio_flush, "tests fwio cmd: flush"),
    SUB_CASE(sub_case_fwio_write_unc, "tests fwio cmd: write_unc"),
    SUB_CASE(sub_case_fwio_write_zero, "tests fwio cmd: write_zero"),
    SUB_CASE(sub_case_trim_cmd, "test trim cmd"),
};

int case_iocmd_fw_io_cmd(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t round_idx = 0;

    test_loop = 2;
    pr_info("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        pr_info("\ntest cnt: %d\n", round_idx);
        for (uint32_t index = 1; index <= ndev->max_sq_num; index++)
        {
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

static int sub_case_pre(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;

    pr_info("==>QID:%d\n", io_sq_id);
    pr_color(LOG_COLOR_PURPLE, "  Create contig cq_id:%d, cq_size = %d\n", io_cq_id, cq_size);
    test_flag |= nvme_create_contig_iocq(ndev->fd, io_cq_id, cq_size, ENABLE, io_cq_id);

    pr_color(LOG_COLOR_PURPLE, "  Create contig sq_id:%d, assoc cq_id = %d, sq_size = %d\n", io_sq_id, io_cq_id, sq_size);
    test_flag |= nvme_create_contig_iosq(ndev->fd, io_sq_id, io_cq_id, sq_size, MEDIUM_PRIO);
    return test_flag;
}

static int sub_case_end(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;

    pr_color(LOG_COLOR_PURPLE, "  Deleting SQID:%d,CQID:%d\n", io_sq_id, io_cq_id);
    test_flag |= nvme_delete_ioq(ndev->fd, nvme_admin_delete_sq, io_sq_id);
    test_flag |= nvme_delete_ioq(ndev->fd, nvme_admin_delete_cq, io_cq_id);
    return test_flag;
}
static int sub_case_trim_cmd(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t index;

    pr_info("1.1 host send io_write cmd\n");

    wr_slba = 0;
    wr_nlb = 8;
    cmd_cnt = 0;
    pr_info("LBA_DATA_SIZE:%dB\n", LBA_DAT_SIZE);
    memset(tool->wbuf, 0x11, tool->wbuf_size);
    mem_disp(tool->wbuf, wr_nlb * LBA_DAT_SIZE);
    //memset(tool->rbuf, 0xff, tool->rbuf_size);
    //mem_disp(tool->rbuf,wr_nlb*LBA_DAT_SIZE);
    for (index = 0; index < 1; index++)
    {
        nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
        cmd_cnt++;
        wr_slba += wr_nlb;
    }
    pr_info("host prepare %d io_write cmd\n", cmd_cnt);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);

    pr_info("1.2 host send io_read cmd\n");
    wr_slba = 0;
    wr_nlb = 8;
    cmd_cnt = 0;
    for (index = 0; index < 1; index++)
    {
        nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
        cmd_cnt++;
        wr_slba += wr_nlb;
    }
    pr_info("host prepare %d io_read cmd\n", cmd_cnt);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    mem_disp(tool->rbuf, wr_nlb * LBA_DAT_SIZE);

    pr_info("2.host send io_write_zero cmd\n");
    wr_slba = 3;
    wr_nlb = 1;
    cmd_cnt = 0;
    for (index = 0; index < 1; index++)
    {
        ioctl_send_write_zero(ndev->fd, io_sq_id, wr_slba, wr_nlb, 0);
        cmd_cnt++;
        wr_slba += wr_nlb;
    }
    pr_info("host prepare how manay:%d io_write_zero cmd\n", cmd_cnt);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);

    pr_info("3.host send io_read cmd\n");
    wr_slba = 0;
    wr_nlb = 8;
    cmd_cnt = 0;
    for (index = 0; index < 1; index++)
    {
        nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
        cmd_cnt++;
        wr_slba += wr_nlb;
    }
    pr_info("host prepare %d io_read cmd\n", cmd_cnt);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    mem_disp(tool->rbuf, wr_nlb * LBA_DAT_SIZE);
    return test_flag;
}
static int sub_case_fwio_flush(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t index = 0;

    pr_info("send io cmd\n");
    wr_slba = 0;
    wr_nlb = 10;
    cmd_cnt = 0;
    for (index = 0; index < 200; index++)
    {
        test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
        cmd_cnt++;
        //wr_slba += wr_nlb;
    }
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d io cmd reaped ok! reap_num:%d\n", io_cq_id, reap_num);

    pr_info("send flush cmd\n");
    ioctl_send_flush(ndev->fd, io_sq_id);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, 1, &reap_num);
    pr_info("  cq:%d flush cmd reaped ok! reap_num:%d\n", io_cq_id, reap_num);

    return test_flag;
}

static int sub_case_fwio_write_unc(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;

    pr_info("write start...");
    wr_slba = 0;
    cmd_cnt = 0;
    test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
    cmd_cnt++;
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d write reaped ok! reap_num:%d\n", io_cq_id, reap_num);

    /**********************************************************************/
    pr_info("read start...");
    wr_slba = 0;
    cmd_cnt = 0;
    test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
    cmd_cnt++;
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d read reaped ok! reap_num:%d\n", io_cq_id, reap_num);

    /**********************************************************************/
    pr_info("write uncorrectable start...");
    wr_slba = 0;
    cmd_cnt = 0;
    test_flag |= ioctl_send_write_unc(ndev->fd, io_sq_id, wr_slba, NVME_TOOL_RW_BUF_SIZE / 512);
    cmd_cnt++;
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d write uncorrectable reaped ok! reap_num:%d\n", io_cq_id, reap_num);

    //("After write_unc read the same blk")
    /**********************************************************************/
    pr_info("After write_unc read the same blk\n");
    wr_slba = 0;
    cmd_cnt = 0;
    test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
    cmd_cnt++;
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    /*test_flag |= */ cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d After write_unc read reaped ok! reap_num:%d\n", io_cq_id, reap_num); //add test_flag=0

    /**********************************************************************/
    pr_info("After write_unc write the same blk");
    wr_slba = 0;
    cmd_cnt = 0;
    test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
    cmd_cnt++;
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d After write_unc write reaped ok! reap_num:%d\n", io_cq_id, reap_num);

    return test_flag;
}

static int sub_case_fwio_write_zero(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t index = 0;

    pr_info("write start...\n");
    wr_slba = 0;
    wr_nlb = 8;
    cmd_cnt = 0;
    for (index = 0; index < 2; index++)
    {
        test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
        cmd_cnt++;
        wr_slba += wr_nlb;
    }
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d write reaped ok! reap_num:%d\n", io_cq_id, reap_num);

    /**********************************************************************/
    pr_info("write_zero start...\n");
    wr_slba = 0;
    wr_nlb = 8;
    cmd_cnt = 0;
    test_flag |= ioctl_send_write_zero(ndev->fd, io_sq_id, wr_slba, wr_nlb, 0);
    cmd_cnt++;

    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d write_zero reaped ok! reap_num:%d\n", io_cq_id, reap_num);

    /**********************************************************************/
    pr_info("read start...\n");
    wr_slba = 0;
    wr_nlb = 8;
    cmd_cnt = 0;
    test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
    cmd_cnt++;
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d read reaped ok! reap_num:%d\n", io_cq_id, reap_num);
    //mem_disp(tool->rbuf,wr_nlb*LBA_DAT_SIZE);
    wr_slba += wr_nlb;
    cmd_cnt = 0;
    test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
    cmd_cnt++;
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_info("  cq:%d read reaped ok! reap_num:%d\n", io_cq_id, reap_num);
    //mem_disp(tool->rbuf,wr_nlb*LBA_DAT_SIZE);
    return test_flag;
}
