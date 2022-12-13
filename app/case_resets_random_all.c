#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "dnvme_ioctl.h"

#include "common.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_irq.h"
#include "test_init.h"

static int test_flag = SUCCEED;
static uint8_t test_type = 1; //
static char *disp_this_case = "this case will random tests: random disable / subsystem / flr reset reset\n"
                              "this case will send random IO(w/r) cmd, wait random cmds back,\n"
                              "then controller disable / subsystem / flr reset,and re-init\n";
static void *fwdma_wr_buffer;
static void *fwdma_rd_buffer;

static void test_sub(void)
{
    uint32_t cmd_cnt = 0;
    uint32_t io_sq_id = 1;
    uint32_t io_cq_id = 1;
    uint32_t cq_size = g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes;
    uint32_t sq_size = g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes;
    uint64_t wr_slba = 0;
    uint16_t wr_nlb = 0;
    uint32_t wr_nsid = 1;
    uint16_t control = 0;
    struct create_cq_parameter cq_parameter = {0};
    struct create_sq_parameter sq_parameter = {0};
    uint32_t reap_num = 0;

/**********************************************************************/
#ifdef FWDMA_RST_OPEN
    uint32_t data_len = 4096;
    uint32_t axi_addr = 0x4070000;
    struct fwdma_parameter fwdma_parameter = {0};
#endif
    /**********************************************************************/
    io_sq_id = BYTE_RAND() % g_nvme_dev.max_sq_num + 1;
    pr_info("create SQ %d\n", io_sq_id);
    /**********************************************************************/
    cq_parameter.cq_id = io_cq_id;
    cq_parameter.cq_size = cq_size;
    cq_parameter.contig = 1;
    cq_parameter.irq_en = 1;
    if (g_nvme_dev.id_ctrl.vid == SAMSUNG_CTRL_VID)
        cq_parameter.irq_no = 0;
    else
        cq_parameter.irq_no = io_cq_id;
    test_flag |= create_iocq(g_fd, &cq_parameter);
    test_flag |= ioctl_tst_ring_dbl(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_debug("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    sq_parameter.cq_id = io_cq_id;
    sq_parameter.sq_id = io_sq_id;
    sq_parameter.sq_size = sq_size;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;
    test_flag |= create_iosq(g_fd, &sq_parameter);
    test_flag |= ioctl_tst_ring_dbl(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_debug("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
    /**********************************************************************/
    wr_slba = 0;
    wr_nlb = 8;
    cmd_cnt = 0;
    /**********************************************************************/
    for (int index = 0; index < (rand() % (512)) / 2 + 100; index++)
    {
        wr_slba = rand() % (g_nvme_ns_info[0].nsze / 2);
        wr_nlb = 256; //WORD_RAND() % 255 + 1;
        if (wr_slba % 2)
        {
            control = NVME_RW_FUA;
        }
        else
        {
            control = 0;
        }

        if (wr_slba + wr_nlb < g_nvme_ns_info[0].nsze)
        {
            test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, control, g_write_buf);
            cmd_cnt++;
            test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_read_buf);
            cmd_cnt++;
            test_flag |= nvme_io_compare_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, control, g_write_buf);
            cmd_cnt++;
        }
    }
    #ifdef FWDMA_RST_OPEN
    for (int index = 0; index < (rand() % 7) + 5; index++)
    {
        fwdma_parameter.cdw10 = data_len; //data_len
        fwdma_parameter.cdw11 = axi_addr; //axi_addr
        // fwdma_parameter.cdw12 |= (1 << 0); //flag bit[0] crc chk,
        fwdma_parameter.cdw12 |= (1 << 1); //flag bit[1] hw data chk(only read)
        fwdma_parameter.cdw12 |= (1 << 2); //flag bit[2] enc/dec chk,
        fwdma_parameter.addr = fwdma_wr_buffer;
        test_flag |= nvme_maxio_fwdma_wr(g_fd, &fwdma_parameter);
        fwdma_parameter.addr = fwdma_rd_buffer;
        test_flag |= nvme_maxio_fwdma_rd(g_fd, &fwdma_parameter);
    }
    #endif

    /**********************************************************************/
    test_flag |= ioctl_tst_ring_dbl(g_fd, io_sq_id);
#ifdef FWDMA_RST_OPEN
    test_flag |= ioctl_tst_ring_dbl(g_fd, NVME_ADMIN_SQ);
#endif
    /**********************************************************************/
    //reap cq
    // test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    // test_flag |= cq_gain(io_cq_id, (rand() % (cmd_cnt-10) + 10), &reap_num);
    // pr_info("send:%d reaped:%d\n", cmd_cnt, reap_num);
#if 0 //def FWDMA_RST_OPEN
    test_flag |= cq_gain(NVME_ADMIN_CQ, 2, &reap_num);
#endif
    test_type = BYTE_RAND() % 6 + 1;
    if (test_type == 1)
    {
        pr_color(LOG_COLOR_YELLOW, "controller disable Reset ...\n");
        test_flag |= ctrl_disable();
        pr_color(LOG_COLOR_YELLOW, "controller disable Reset Done\n");
    }
    else if (test_type == 2)
    {
        pr_color(LOG_COLOR_YELLOW, "NVM Subsystem Reset ...\n");
        test_flag |= subsys_reset();
        pr_color(LOG_COLOR_YELLOW, "NVM Subsystem Reset Done\n");
    }
    else if (test_type == 3)
    {
        pr_color(LOG_COLOR_YELLOW, "controller flr Reset ...\n");
        test_flag |= ctrl_pci_flr();
        pr_color(LOG_COLOR_YELLOW, "controller flr Reset Done\n");
    }
    else if (test_type == 4)
    {
        pr_color(LOG_COLOR_YELLOW, "controller d0d3 Reset ...\n");
        test_flag |= set_power_state(g_nvme_dev.pmcap_ofst, D3hot);
        assert(test_flag == SUCCEED);
        test_flag |= set_power_state(g_nvme_dev.pmcap_ofst, D0);
        assert(test_flag == SUCCEED);
        pr_color(LOG_COLOR_YELLOW, "controller d0d3 Reset Done\n");
    }
    else if (test_type == 5)
    {
        pr_color(LOG_COLOR_YELLOW, "controller hot Reset ...\n");
        pcie_hot_reset();
        pr_color(LOG_COLOR_YELLOW, "controller hot Reset Done\n");
    }
    else if (test_type == 6)
    {
        pr_color(LOG_COLOR_YELLOW, "controller linkdown Reset ...\n");
        pcie_link_down();
        pr_color(LOG_COLOR_YELLOW, "controller linkdown Reset Done\n");
    }

    usleep(10000);
    if (g_nvme_dev.id_ctrl.vid == SAMSUNG_CTRL_VID)
        test_change_init(g_fd, MAX_ADMIN_QUEUE_SIZE, MAX_ADMIN_QUEUE_SIZE, NVME_INT_PIN, 1);
    else
        test_change_init(g_fd, MAX_ADMIN_QUEUE_SIZE, MAX_ADMIN_QUEUE_SIZE, NVME_INT_MSIX, g_nvme_dev.max_sq_num + 1);
}

int case_resets_random_all(void)
{
    int test_round = 0;
    pr_info("\n********************\t %s \t********************\n", __FUNCTION__);
    pr_info("%s\n", disp_this_case);

    /* !FIXME: release memory after alloc fail! */
    if ((posix_memalign(&fwdma_wr_buffer, CONFIG_UNVME_RW_BUF_ALIGN, 8192)) ||
        (posix_memalign(&fwdma_rd_buffer, CONFIG_UNVME_RW_BUF_ALIGN, 8192)))
    {
        pr_err("Memalign Failed\n");
        return FAILED;
    }

    memset((uint8_t *)fwdma_wr_buffer, rand() % 0xff, 8192);
    memset((uint8_t *)fwdma_rd_buffer, 0, 8192);

    /**********************************************************************/
    for (test_round = 1; test_round <= 50; test_round++)
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
    free(fwdma_wr_buffer);
    free(fwdma_rd_buffer);
    return test_flag;
}
