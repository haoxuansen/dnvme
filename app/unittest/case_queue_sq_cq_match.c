#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include "dnvme.h"
#include "libnvme.h"

#include "common.h"
#include "test.h"
#include "test_metrics.h"
#include "test_cq_gain.h"
#include "test_send_cmd.h"

static int test_flag = SUCCEED;

static char *disp_this_case = ".1 one to one test: cq 1 <---> sq 1, from 1 to 8 \n"
                              ".1.1: after delete sq3/cq3 - sq5/cq5, run io cmd on the remain sq\n"
                              ".1.2: after run io cmd on the sq1-sq2,sq6-sq8 delete it\n"
                              ".2 sq in order test: cq 1 <---> sq 1~8\n"
                              ".3 sq reverse order Test: cq 1 <---> sq 8~1\n"
                              ".4 cq/sq random match test: cq 3 <---> sq 5 | cq 1 <---> sq 3,2 | cq 6 <---> sq 7,6,4 | cq 5 <---> sq 8,1\n"
                              ".4.1: after delete sq 2-7, run io cmd on sq 8,1\n"
                              ".4.2: after run io cmd on sq 1,8, delete it\n"
                              ".5 sq/cq random match\n"
                              "each sq will send 2 io cmd(1 write and 1 read)\n"
                              "default parameter: write/read slba=0, nlb = 8"
                              "\n";

static void test_sub(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t index = 0;
    uint32_t cmd_cnt = 0;
    uint32_t io_sq_id = 1;
    uint32_t io_cq_id = 1;
    uint32_t cq_size = 4096;
    uint32_t sq_size = 4096;
    uint64_t wr_slba = 0;
    uint16_t wr_nlb = 8;
    uint32_t wr_nsid = 1;

    struct create_cq_parameter cq_parameter = {0};
    struct create_sq_parameter sq_parameter = {0};

    uint32_t reap_num = 0;

    /*******************************************************************************************************************************/
    pr_info(".1 one to one test: cq 1 <---> sq 1, from 1 to max_sq_num\n");

    cq_size = 16384;
    sq_size = 16384;

    pr_color(LOG_COLOR_PURPLE, "  Create contig sq assoc cq 1-max_sq_num, sq_size = %d\n", sq_size);
    for (index = 1; index <= ndev->max_sq_num; index++)
    {
        io_cq_id = io_sq_id = index;
        test_flag |= nvme_create_contig_iocq(ndev->fd, io_cq_id, cq_size, ENABLE, io_cq_id);
        test_flag |= nvme_create_contig_iosq(ndev->fd, io_sq_id, io_cq_id, sq_size, MEDIUM_PRIO);
    }

    /**********************************************************************/
    wr_slba = 0;
    wr_nlb = 8;
    pr_div("\n\033[35m  Sending IO Write && Read Command to SQ1-max \033[0m\n");
    for (index = 1; index <= ndev->max_sq_num; index++)
    {
        io_sq_id = index;
        test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
        test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
    }
    pr_div("\033[35m  Ringing Doorbell for SQ1-max IO cmd\033[0m\n");
    for (index = 1; index <= ndev->max_sq_num; index++)
    {
        io_sq_id = index;
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    }
    for (index = 1; index <= ndev->max_sq_num; index++)
    {
        io_cq_id = index;
        test_flag |= cq_gain(io_cq_id, 2, &reap_num);
        pr_div("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);
    }

    /**********************************************************************/
    pr_div("\033[35m  Deleting SQ1\033[0m\n");
    test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_sq, 1);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /**********************************************************************/
    pr_div("\033[35m  Deleting CQ1\033[0m\n");
    test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_cq, 1);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    pr_info(".1.1: after delete sq1/cq1, run io cmd on the remain sq\n");
    /*******************************************************************************************************************************/
    pr_div("\n\033[35m  Sending IO Write && Read Command to SQ2-max \033[0m\n");

    wr_slba = 0;
    wr_nlb = 8;

    for (index = 2; index <= ndev->max_sq_num; index++)
    {
        io_sq_id = index;
        test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
        test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
    }
    pr_div("\033[35m  Ringing Doorbell for SQ 2-max IO cmd\033[0m\n");
    for (index = 2; index <= ndev->max_sq_num; index++)
    {
        io_sq_id = index;
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    }

    for (index = 2; index <= ndev->max_sq_num; index++)
    {
        io_cq_id = index;
        test_flag |= cq_gain(io_cq_id, 2, &reap_num);
        pr_div("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);
    }

    pr_info(".1.2: after run io cmd on the  SQ 2-max delete it\n");
    /**********************************************************************/
    pr_div("\033[35m  Deleting SQ1-max\033[0m\n");
    cmd_cnt = 0;
    for (index = 2; index <= ndev->max_sq_num; index++)
    {
        io_sq_id = index;
        test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_sq, io_sq_id);
        cmd_cnt++;
    }
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, cmd_cnt, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /**********************************************************************/
    pr_div("\033[35m  Deleting CQ1-max\033[0m\n");
    cmd_cnt = 0;
    for (index = 2; index <= ndev->max_sq_num; index++)
    {
        io_cq_id = index;
        test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_cq, io_cq_id);
        cmd_cnt++;
    }
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, cmd_cnt, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /*******************************************************************************************************************************/

    io_cq_id = rand() % ndev->max_sq_num + 1;
    pr_color(LOG_COLOR_PURPLE,".2: cq %d <---> sq 1~ndev->max_sq_num\n", io_cq_id);
    io_sq_id = 1;

    cq_size = 16384;
    sq_size = 16384;

    test_flag |= nvme_create_contig_iocq(ndev->fd, io_cq_id, cq_size, ENABLE, io_cq_id);

    pr_div("\n\033[35m  Preparing io_sq_id 1-ndev->max_sq_num: io_cq_id = %d, sq_size = %d \033[0m\n", io_cq_id, sq_size);
    cmd_cnt = 0;
    sq_parameter.sq_size = sq_size;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;
    sq_parameter.cq_id = io_cq_id;
    for (index = 1; index <= ndev->max_sq_num; index++)
    {
        sq_parameter.sq_id = index;
        test_flag |= create_iosq(ndev->fd, &sq_parameter);
        cmd_cnt++;
    }
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, cmd_cnt, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /**********************************************************************/
    pr_div("\n\033[35m  Sending IO Write && Read Command to SQ1-max \033[0m\n");
    wr_slba = 0;
    wr_nlb = 8;
    cmd_cnt = 0;
    for (index = 1; index <= ndev->max_sq_num; index++)
    {
        io_sq_id = index;
        test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
        cmd_cnt++;
        test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
        cmd_cnt++;
        wr_slba += wr_nlb;
    }

    pr_div("\033[35m  Ringing Doorbell for SQ1-max IO cmd\033[0m\n");
    for (index = 1; index <= ndev->max_sq_num; index++)
    {
        io_sq_id = index;
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    }
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);

    /**********************************************************************/
    pr_div("\033[35m  Deleting SQ1-max\033[0m\n");
    cmd_cnt = 0;
    for (index = 1; index <= ndev->max_sq_num; index++)
    {
        io_sq_id = index;
        test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_sq, io_sq_id);
        cmd_cnt++;
    }
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, cmd_cnt, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    pr_div("\033[35m  Deleting CQ 1\033[0m\n");
    test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_cq, io_cq_id);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /*******************************************************************************************************************************/

    io_cq_id = rand() % ndev->max_sq_num + 1;
    pr_color(LOG_COLOR_PURPLE,".3: cq %d <---> sq ndev->max_sq_num~1\n", io_cq_id);
    io_sq_id = 1;

    cq_size = 16384;
    sq_size = 16384;

    test_flag |= nvme_create_contig_iocq(ndev->fd, io_cq_id, cq_size, ENABLE, io_cq_id);

    pr_div("\n\033[35m  Preparing io_sq_id ndev->max_sq_num-1: io_cq_id = %d, sq_size = %d \033[0m\n", io_cq_id, sq_size);
    cmd_cnt = 0;
    sq_parameter.sq_size = sq_size;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;
    sq_parameter.cq_id = io_cq_id;
    for (index = ndev->max_sq_num; index >= 1; index--)
    {
        sq_parameter.sq_id = index;
        test_flag |= create_iosq(ndev->fd, &sq_parameter);
        cmd_cnt++;
    }
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, cmd_cnt, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /**********************************************************************/
    pr_div("\n\033[35m  Sending IO Write && Read Command to SQ8-SQ1 \033[0m\n");
    cmd_cnt = 0;
    wr_nlb = 8;
    cmd_cnt = 0;
    for (index = ndev->max_sq_num; index >= 1; index--)
    {
        io_sq_id = index;
        test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
        cmd_cnt++;
        test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
        cmd_cnt++;
        wr_slba += wr_nlb;
    }

    pr_div("\033[35m  Ringing Doorbell for SQ8-SQ1 IO cmd\033[0m\n");
    for (index = ndev->max_sq_num; index >= 1; index--)
    {
        io_sq_id = index;
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    }
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);

    /**********************************************************************/
    pr_div("\033[35m  Deleting SQ8-SQ1\033[0m\n");
    cmd_cnt = 0;
    for (index = ndev->max_sq_num; index >= 1; index--)
    {
        io_sq_id = index;
        test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_sq, io_sq_id);
        cmd_cnt++;
    }
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, cmd_cnt, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    pr_div("\033[35m Deleting CQ 1\033[0m\n");
    test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_cq, io_cq_id);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /*******************************************************************************************************************************/
    //5, 3, 2, 7, 6, 4, 8, 1
    pr_color(LOG_COLOR_PURPLE,".4: cq 1 <---> sq 4 "
                  "| cq 2 <---> sq 3 "
                  "| cq 3 <---> sq 1,2\n");

    io_cq_id = 1;
    io_sq_id = 1;

    cq_size = 16384;
    sq_size = 16384;

    pr_div("\n\033[35m  Preparing io_cq_id 1,2,3; cq_size = %d \033[0m\n", cq_size);

    cq_parameter.cq_size = cq_size;
    cq_parameter.contig = 1;
    cq_parameter.irq_en = 1;

    io_cq_id = 1;
    cq_parameter.irq_no = io_cq_id;
    cq_parameter.cq_id = io_cq_id;
    test_flag |= create_iocq(ndev->fd, &cq_parameter);

    io_cq_id = 2;
    cq_parameter.irq_no = io_cq_id;
    cq_parameter.cq_id = io_cq_id;
    test_flag |= create_iocq(ndev->fd, &cq_parameter);

    io_cq_id = 3;
    cq_parameter.irq_no = io_cq_id;
    cq_parameter.cq_id = io_cq_id;
    test_flag |= create_iocq(ndev->fd, &cq_parameter);


    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 3, &reap_num);
    pr_div("  cq reaped ok! reap_num:%d\n", reap_num);

    pr_div("\n\033[35m  Preparing cq:sq 1: 4; 2: 3; 3: 1,2; sq_size = %d \033[0m\n", sq_size);

    sq_parameter.sq_size = sq_size;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;

    sq_parameter.cq_id = 1;
    sq_parameter.sq_id = 4;
    test_flag |= create_iosq(ndev->fd, &sq_parameter);

    sq_parameter.cq_id = 2;
    sq_parameter.sq_id = 3;
    test_flag |= create_iosq(ndev->fd, &sq_parameter);

    //cq 6 <---> sq 7,6,4
    sq_parameter.cq_id = 3;
    sq_parameter.sq_id = 1;
    test_flag |= create_iosq(ndev->fd, &sq_parameter);
    sq_parameter.sq_id = 2;
    test_flag |= create_iosq(ndev->fd, &sq_parameter);

    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 4, &reap_num);
    pr_div("  cq reaped ok! reap_num:%d\n", reap_num);

    /**********************************************************************/
    pr_div("\n\033[35m  Sending IO Write && Read Command to SQ1-SQ8 \033[0m\n");
    wr_slba = 0;
    wr_nlb = 8;
    //cmd_cnt = 0;
    for (index = 1; index <= 4; index++)
    {
        io_sq_id = index;
        test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
        //cmd_cnt++;
        test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
        //cmd_cnt++;
        wr_slba += wr_nlb;
    }

    pr_div("\033[35m  Ringing Doorbell for SQ1-SQ8 \033[0m\n");
    for (index = 1; index <= 4; index++)
    {
        io_sq_id = index;
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    }

    pr_div("\n\033[35m  Preparing cq:sq 1: 4; 2: 3; 3: 1,2; sq_size = %d \033[0m\n", sq_size);
    io_cq_id = 1; //
    test_flag |= cq_gain(io_cq_id, 1 * 2, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);

    io_cq_id = 2; //
    test_flag |= cq_gain(io_cq_id, 1 * 2, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);

    io_cq_id = 3; //cq 3 <---> sq 1,2
    test_flag |= cq_gain(io_cq_id, 2 * 2, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);

    /**********************************************************************/
    pr_div("\033[35m  Deleting SQ4\033[0m\n");
    cmd_cnt = 0;
    for (index = 4; index <= 4; index++)
    {
        io_sq_id = index;
        test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_sq, io_sq_id);
        cmd_cnt++;
    }
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, cmd_cnt, &reap_num);
    pr_div("  cq reaped ok! reap_num:%d\n", reap_num);

    pr_div("\033[35m  Deleting CQ 1\033[0m\n");
    io_cq_id = 1;
    test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_cq, io_cq_id);

    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);

    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq reaped ok! reap_num:%d\n", reap_num);

    pr_info(".4.1: after delete sq 4, run io cmd on sq 1-2\n");

    /**********************************************************************/
    //".4 cq/sq random match test: cq 3 <---> sq 5 | cq 1 <---> sq 3,2 | cq 6 <---> sq 7,6,4 | cq 5 <---> sq 8,1\n"
    // pr_div("\n\033[35m  Preparing cq:sq 1: 4; 2: 3; 3: 1,2; sq_size = %d \033[0m\n", sq_size);
    pr_div("\n\033[35m  Sending IO Write && Read Command to SQ1,SQ2 \033[0m\n");
    io_sq_id = 1;
    test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
    test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
    wr_slba += wr_nlb;

    io_sq_id = 2;
    test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
    test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
    wr_slba += wr_nlb;

    pr_div("\033[35m  Ringing Doorbell for SQ2,SQ3 \033[0m\n");
    io_sq_id = 1;
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    io_sq_id = 2;
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);

    io_cq_id = 3; //cq 
    test_flag |= cq_gain(io_cq_id, 2 * 2, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);

    pr_info(".4.2: after run io cmd on sq 1,2, delete it\n");
    test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_sq, 1);
    test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_sq, 2);
    test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_sq, 3);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 3, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);//

    test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_cq, 2);
    test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_cq, 3);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 2, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);//

    /*******************************************************************************************************************************/

    io_cq_id = rand() % ndev->max_sq_num + 1;
    io_sq_id = rand() % ndev->max_sq_num + 1;
    pr_color(LOG_COLOR_PURPLE,".5 random sq:%d/cq:%d  match\n", io_sq_id, io_cq_id);

    cq_size = 16384;
    sq_size = 16384;

    cq_parameter.cq_id = io_cq_id;
    cq_parameter.cq_size = cq_size;
    cq_parameter.contig = 1;
    cq_parameter.irq_en = 1;
    cq_parameter.irq_no = io_cq_id;
    test_flag |= create_iocq(ndev->fd, &cq_parameter);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq reaped ok! reap_num:%d\n", reap_num);

    sq_parameter.cq_id = io_cq_id;
    sq_parameter.sq_size = sq_size;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;

    sq_parameter.sq_id = io_sq_id;
    test_flag |= create_iosq(ndev->fd, &sq_parameter);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /**********************************************************************/
    pr_div("\n\033[35m  Sending IO Write && Read Command to SQ \033[0m\n");
    wr_slba = 0;
    wr_nlb = 8 * (rand() % 10 + 1);
    cmd_cnt = 0;
    test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
    cmd_cnt++;
    test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
    cmd_cnt++;

    pr_div("\033[35m  Ringing Doorbell for SQ1-SQ8 IO cmd\033[0m\n");
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", io_cq_id, reap_num);

    /**********************************************************************/
    pr_div("\033[35m  Deleting SQ\033[0m\n");
    cmd_cnt = 0;
    test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_sq, io_sq_id);
    cmd_cnt++;
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, cmd_cnt, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    pr_div("\033[35m  Deleting CQ 1\033[0m\n");
    test_flag |= ioctl_delete_ioq(ndev->fd, nvme_admin_delete_cq, io_cq_id);
    test_flag |= nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /*******************************************************************************************************************************/
}

int case_queue_sq_cq_match(struct nvme_tool *tool)
{
    int test_round = 0;
    pr_info("\n********************\t %s \t********************\n", __FUNCTION__);
    pr_info("%s", disp_this_case);

    for (test_round = 1; test_round <= 50; test_round++)
    {
        pr_info("\n\ntest_round: %d\n", test_round);
        test_sub();
    }

    nvme_display_test_result(test_flag != SUCCEED ? -EPERM : 0, __func__);
    return test_flag;
}
