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
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "libbase.h"
#include "libnvme.h"
#include "test.h"
#include "unittest.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"

static int test_flag = 0;
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

static int sub_case_pre(void);
static int sub_case_end(void);

static int sub_case_io_cmd(void);
static int sub_case_fwdma_cmd(void);

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

static int test_2_mix_case(struct nvme_tool *tool, struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
    struct nvme_ctrl_instance *ctrl = ndev->ctrl;
    uint32_t round_idx = 0;

    test_loop = 2;

    pr_info("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        pr_info("\ntest cnt: %d\n", round_idx);
        for (uint32_t index = 1; index <= ctrl->nr_sq; index++)
        {
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
NVME_CASE_SYMBOL(test_2_mix_case, "?");
NVME_AUTOCASE_SYMBOL(test_2_mix_case);

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

static int sub_case_io_cmd(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint8_t tmp_fg = 0;
	uint32_t nn = 0;
	uint32_t lbads;
	uint64_t nsze;
	uint32_t i;
	int ret;

	nvme_id_ctrl_nn(ctrl, &nn);

	for (uint32_t ns_idx = 0; ns_idx < nn; ns_idx++)
	{
		wr_nsid = ns_idx + 1;
		// wr_slba = (uint32_t)rand() % (ndev->nss[0].nsze / 2);
		// wr_nlb = (uint16_t)rand() % 32 + 1;
		wr_slba = 0;
		wr_nlb = 32;

		ret = nvme_id_ns_lbads(ns_grp, wr_nsid, &lbads);
		ret |= nvme_id_ns_nsze(ns_grp, wr_nsid, &nsze);
		if (ret < 0) {
			test_flag |= ret;
			DBG_ON(test_flag < 0);
			return ret;
		}

		pr_info("sq_id:%d nsid:%d lbads:%d slba:%ld nlb:%d\n", 
			io_sq_id, wr_nsid, lbads, wr_slba, wr_nlb);

		for (i = 0; i < (((nsze / wr_nlb) > (sq_size - 1)) ? (sq_size - 1) : (nsze / wr_nlb)); i++)
		{

			if ((wr_slba + wr_nlb) < nsze)
			{
				mem_set(tool->wbuf, (uint32_t)rand(), wr_nlb * lbads);
				mem_set(tool->rbuf, 0, wr_nlb * lbads);

				cmd_cnt = 0;
				test_flag |= nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
				DBG_ON(test_flag < 0);
				if (test_flag == 0)
				{
					cmd_cnt++;
					test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
					DBG_ON(test_flag < 0);
					test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
					DBG_ON(test_flag < 0);
				}
				else
				{
					goto out;
				}

				cmd_cnt = 0;
				test_flag |= nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
				DBG_ON(test_flag < 0);
				if (test_flag == 0)
				{
					cmd_cnt++;
					test_flag |= nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
					DBG_ON(test_flag < 0);
					test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
					DBG_ON(test_flag < 0);
				}
				else
				{
					goto out;
				}
				tmp_fg = dw_cmp(tool->wbuf, tool->rbuf, wr_nlb * lbads);
				test_flag |= tmp_fg;
				if (tmp_fg != 0)
				{
					pr_info("[E] i:%d,wr_slba:%lx,wr_nlb:%x\n", i, wr_slba, wr_nlb);
					pr_info("\nwrite_buffer Data:\n");
					mem_disp(tool->wbuf, wr_nlb * lbads);
					pr_info("\nRead_buffer Data:\n");
					mem_disp(tool->rbuf, wr_nlb * lbads);
					break;
				}
				wr_slba += wr_nlb;
			}
		}
	}
out:
	return test_flag;
}

static int sub_case_fwdma_cmd(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
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
        return -1;
    }
#else
    fwdma_wr_buffer = malloc(8192);
    fwdma_rd_buffer = malloc(8192);
    if ((tool->wbuf == NULL) || (tool->rbuf == NULL))
    {
        pr_err("Malloc Failed\n");
        return -1;
    }
#endif
    /**********************************************************************/
    pr_info("send_fwdma_wr/rd test crc enc-dec wr/rd\n");
    data_len = 4096; //(((uint16_t)rand() % 255) + 1) * 16;
    pr_info(" data_len:%d\n", data_len);
    memset((uint8_t *)fwdma_wr_buffer, rand() % 0xff, data_len);
    memset((uint8_t *)fwdma_rd_buffer, 0, data_len);
    fwdma_parameter.cdw10 = data_len;  //data_len
    fwdma_parameter.cdw11 = axi_addr;  //axi_addr
    // fwdma_parameter.cdw12 |= (1 << 0); //flag bit[0] crc chk,
    fwdma_parameter.cdw12 |= (1 << 1); //flag bit[1] hw data chk(only read)
    fwdma_parameter.cdw12 |= (1 << 2); //flag bit[2] enc/dec chk,

    fwdma_parameter.addr = fwdma_wr_buffer;
    test_flag |= nvme_maxio_fwdma_wr(ndev->fd, &fwdma_parameter);
    DBG_ON(test_flag < 0);
    if (0 == test_flag)
    {
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, 0);
	DBG_ON(test_flag < 0);
        test_flag |= cq_gain(0, 1, &reap_num);
	DBG_ON(test_flag < 0);
    }

    fwdma_parameter.addr = fwdma_rd_buffer;
    test_flag |= nvme_maxio_fwdma_rd(ndev->fd, &fwdma_parameter);
    DBG_ON(test_flag < 0);
    if (0 == test_flag)
    {
        test_flag |= nvme_ring_sq_doorbell(ndev->fd, 0);
	DBG_ON(test_flag < 0);
        test_flag |= cq_gain(0, 1, &reap_num);
	DBG_ON(test_flag < 0);
    }

    if (memcmp(fwdma_wr_buffer, fwdma_rd_buffer, data_len))
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
