/**
 * @file test_2_mix_case.c
 * @author yumeng (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2019-10-08
 * 
 * @copyright Copyright (c) 2019
 * 
 */
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

static void *fwdma_wr_buffer;
static void *fwdma_rd_buffer;

static uint32_t cmd_cnt = 0;
static uint32_t io_sq_id = 1;
static uint32_t io_cq_id = 1;
static uint32_t cq_size = 4096;
static uint32_t sq_size = 4096;
static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint32_t reap_num = 0;

static dword_t sub_case_pre(void);
static dword_t sub_case_end(void);

static dword_t sub_case_io_cmd(void);
static dword_t sub_case_fwdma_cmd(void);

static SubCaseHeader_t sub_case_header = {
    "test_2_mix_case",
    "This case will tests io/admin/fwdma cmd ",
    sub_case_pre,
    sub_case_end,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_io_cmd, "send io cmd and check data"),
    SUB_CASE(sub_case_fwdma_cmd, "send fwdma wr/rd cmd and check data"),
};

int test_2_mix_case(void)
{
    uint32_t round_idx = 0;

    test_loop = 2;

    pr_info("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        pr_info("\ntest cnt: %d\n", round_idx);
        for (uint32_t index = 1; index <= g_nvme_dev.max_sq_num; index++)
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

static dword_t sub_case_pre(void)
{
    pr_info("==>QID:%d\n", io_sq_id);
    pr_color(LOG_COLOR_PURPLE, "  Create contig cq_id:%d, cq_size = %d\n", io_cq_id, cq_size);
    test_flag |= nvme_create_contig_iocq(file_desc, io_cq_id, cq_size, ENABLE, io_cq_id);

    pr_color(LOG_COLOR_PURPLE, "  Create contig sq_id:%d, assoc cq_id = %d, sq_size = %d\n", io_sq_id, io_cq_id, sq_size);
    test_flag |= nvme_create_contig_iosq(file_desc, io_sq_id, io_cq_id, sq_size, MEDIUM_PRIO);
    return test_flag;
}

static dword_t sub_case_end(void)
{
    pr_color(LOG_COLOR_PURPLE, "  Deleting SQID:%d,CQID:%d\n", io_sq_id, io_cq_id);
    test_flag |= nvme_delete_ioq(file_desc, nvme_admin_delete_sq, io_sq_id);
    test_flag |= nvme_delete_ioq(file_desc, nvme_admin_delete_cq, io_cq_id);
    return test_flag;
}

static dword_t sub_case_io_cmd(void)
{
    byte_t tmp_fg = 0;

    for (uint32_t ns_idx = 0; ns_idx < g_nvme_dev.id_ctrl.nn; ns_idx++)
    {
        wr_nsid = ns_idx + 1;
        // wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
        // wr_nlb = WORD_RAND() % 32 + 1;
        wr_slba = 0;
        wr_nlb = 32;

        pr_info("sq_id:%d nsid:%d lbads:%d slba:%ld nlb:%d\n", io_sq_id, wr_nsid, LBA_DATA_SIZE(wr_nsid), wr_slba, wr_nlb);

        for (uint32_t i = 0; i < (((g_nvme_ns_info[0].nsze / wr_nlb) > (sq_size - 1)) ? (sq_size - 1) : (g_nvme_ns_info[0].nsze / wr_nlb)); i++)
        {

            if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
            {
                mem_set(write_buffer, DWORD_RAND(), wr_nlb * LBA_DATA_SIZE(wr_nsid));
                mem_set(read_buffer, 0, wr_nlb * LBA_DATA_SIZE(wr_nsid));

                cmd_cnt = 0;
                test_flag |= nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
                if (test_flag == SUCCEED)
                {
                    cmd_cnt++;
                    test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
                    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
                }
                else
                {
                    goto out;
                }

                cmd_cnt = 0;
                test_flag |= nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
                if (test_flag == SUCCEED)
                {
                    cmd_cnt++;
                    test_flag |= ioctl_tst_ring_dbl(file_desc, io_sq_id);
                    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
                }
                else
                {
                    goto out;
                }
                tmp_fg = dw_cmp(write_buffer, read_buffer, wr_nlb * LBA_DATA_SIZE(wr_nsid));
                test_flag |= tmp_fg;
                if (tmp_fg != SUCCEED)
                {
                    pr_info("[E] i:%d,wr_slba:%lx,wr_nlb:%x\n", i, wr_slba, wr_nlb);
                    pr_info("\nwrite_buffer Data:\n");
                    mem_disp(write_buffer, wr_nlb * LBA_DATA_SIZE(wr_nsid));
                    pr_info("\nRead_buffer Data:\n");
                    mem_disp(read_buffer, wr_nlb * LBA_DATA_SIZE(wr_nsid));
                    break;
                }
                wr_slba += wr_nlb;
            }
        }
    }
out:
    return test_flag;
}

static dword_t sub_case_fwdma_cmd(void)
{
    uint32_t data_len = 0;
    uint32_t reap_num = 0;
    // uint32_t wr_nlb = 0;
    uint32_t axi_addr = 0x4070000;
    struct fwdma_parameter fwdma_parameter = {0};

#if 0
    if ((posix_memalign(&fwdma_wr_buffer, 4096, 8192)) ||
        (posix_memalign(&fwdma_rd_buffer, 4096, 8192)))
    {
        pr_err("Memalign Failed\n");
        return FAILED;
    }
#else
    fwdma_wr_buffer = malloc(8192);
    fwdma_rd_buffer = malloc(8192);
    if ((write_buffer == NULL) || (read_buffer == NULL))
    {
        pr_err("Malloc Failed\n");
        return FAILED;
    }
#endif
    /**********************************************************************/
    pr_info("send_fwdma_wr/rd test crc enc-dec wr/rd\n");
    data_len = 4096; //((WORD_RAND() % 255) + 1) * 16;
    pr_info(" data_len:%d\n", data_len);
    memset((uint8_t *)fwdma_wr_buffer, rand() % 0xff, data_len);
    memset((uint8_t *)fwdma_rd_buffer, 0, data_len);
    fwdma_parameter.cdw10 = data_len;  //data_len
    fwdma_parameter.cdw11 = axi_addr;  //axi_addr
    // fwdma_parameter.cdw12 |= (1 << 0); //flag bit[0] crc chk,
    fwdma_parameter.cdw12 |= (1 << 1); //flag bit[1] hw data chk(only read)
    fwdma_parameter.cdw12 |= (1 << 2); //flag bit[2] enc/dec chk,

    fwdma_parameter.addr = fwdma_wr_buffer;
    test_flag |= nvme_maxio_fwdma_wr(file_desc, &fwdma_parameter);
    if (SUCCEED == test_flag)
    {
        test_flag |= ioctl_tst_ring_dbl(file_desc, 0);
        test_flag |= cq_gain(0, 1, &reap_num);
    }

    fwdma_parameter.addr = fwdma_rd_buffer;
    test_flag |= nvme_maxio_fwdma_rd(file_desc, &fwdma_parameter);
    if (SUCCEED == test_flag)
    {
        test_flag |= ioctl_tst_ring_dbl(file_desc, 0);
        test_flag |= cq_gain(0, 1, &reap_num);
    }

    if (mem_cmp(fwdma_wr_buffer, fwdma_rd_buffer, data_len))
    {
        pr_info("\nwrite_buffer Data:\n");
        mem_disp(fwdma_wr_buffer, data_len);
        pr_info("\nRead_buffer Data:\n");
        mem_disp(fwdma_rd_buffer, data_len);
        test_flag |= 1;
    }
    /**********************************************************************/

    free(fwdma_wr_buffer);
    free(fwdma_rd_buffer);
    return test_flag;
}
