/**
 * @file test_mess.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief The test cases in this file need to be further classified and sorted out
 * @version 0.1
 * @date 2023-08-04
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"
#include "test.h"
#include "test_cq_gain.h"
#include "test_metrics.h"
#include "unittest.h"

struct test_data {
	uint32_t	nsid;
	uint64_t	nsze;
	uint32_t	lbads;
};

static struct test_data g_test = {0};

/**
 * @note May re-initialized? ignore...We shall to update this if data changed. 
 */
static int init_test_data(struct nvme_dev_info *ndev, struct test_data *data)
{
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	int ret;

	/* use first active namespace as default */
	data->nsid = le32_to_cpu(ns_grp->act_list[0]);

	ret = nvme_id_ns_lbads(ns_grp, data->nsid, &data->lbads);
	if (ret < 0) {
		pr_err("failed to get lbads!(%d)\n", ret);
		return ret;
	}
	/* we have checked once, skip the check below */
	nvme_id_ns_nsze(ns_grp, data->nsid, &data->nsze);

	return 0;
}

static int case_disable_ctrl_complete(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;

	return nvme_disable_controller_complete(ndev->fd);
}
NVME_CASE_SYMBOL(case_disable_ctrl_complete, "Disable the controller completely");

static int case_reinit_device(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;

	return nvme_reinit(ndev, ndev->asq.nr_entry, ndev->acq.nr_entry, 
		ndev->irq_type);
}
NVME_CASE_SYMBOL(case_reinit_device, 
	"Reinitialize the device for running others later");

static int case_write_fwdma(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	uint32_t i = 0;
	uint32_t index = 0;
	uint32_t reap_num;
	struct fwdma_parameter fwdma_parameter = {0};

	i = 10;
	memset((uint8_t *)&fwdma_parameter, 0, sizeof(fwdma_parameter));
	// pr_color(LOG_N_CYAN,"pls enter loop cnt:");
	// fflush(stdout);
	// scanf("%d", &i);
	while (i--)
	{
		for (index = 0; index < 3000; index++)
		{
			fwdma_parameter.addr = tool->wbuf;
			fwdma_parameter.cdw10 = 4096; //data_len
			// fwdma_parameter.cdw11 = 0x4078000;              //axi_addr
			// fwdma_parameter.cdw12 |= (1<<0);                //flag bit[0] crc chk,
			// fwdma_parameter.cdw12 &= ~(1<<1);             //flag bit[1] hw data chk(only read)
			// fwdma_parameter.cdw12 |= (1<<2);                //flag bit[2] enc chk,
			nvme_maxio_fwdma_wr(ndev->fd, &fwdma_parameter);
		}
		nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
		cq_gain(NVME_AQ_ID, 1, &reap_num);
		pr_info("\nfwdma wr cmd send done!\n");
	}
	return 0;
}
NVME_CASE_SYMBOL(case_write_fwdma, "maxio_cmd_fwdma_write");

static int case_read_fwdma(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	uint32_t data_len = 0;
	uint32_t reap_num;
	struct fwdma_parameter fwdma_parameter = {0};

	pr_info("send_maxio_fwdma_rd\n");
	data_len = 40 * 4;
	fwdma_parameter.addr = tool->rbuf;
	fwdma_parameter.cdw10 = data_len;  //data_len
	fwdma_parameter.cdw11 = 0x40754C0; //axi_addr
	//fwdma_parameter.cdw12 |= (1<<0);               //flag bit[0] crc chk,
	//fwdma_parameter.cdw12 |= (1<<1);               //flag bit[1] hw data chk(only read)
	// fwdma_parameter.cdw12 |= (1<<2);                 //flag bit[2] dec chk,
	nvme_maxio_fwdma_rd(ndev->fd, &fwdma_parameter);
	nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	cq_gain(NVME_AQ_ID, 1, &reap_num);
	pr_info("\nfwdma wr cmd send done!\n");

	dw_cmp(tool->wbuf, tool->rbuf, data_len);
	return 0;
}
NVME_CASE_SYMBOL(case_read_fwdma, "maxio_cmd_fwdma_read");

static int case_disable_ltr(struct nvme_tool *tool)
{
	/* !FIXME: The BDF of NVMe device cannot be fixed. It's better to use
	 * device ID and vendor ID instead! 
	 */
	system("setpci -s 2:0.0  99.b=00");
	return 0;
}
NVME_CASE_SYMBOL(case_disable_ltr, "set LTR disable");

struct nvme_maxio_fwdma {
	__u8			opcode;
	__u8			flags;
	__u16			command_id;
	__le32			nsid;
	__le32			option;
	__le32			param;
	__le64			metadata;
	union nvme_data_ptr	dptr;
	__le32			bitmask;
	__le32			cdw11[2];
	__le64			slba;
	__le32			len;
};

static int nvme_cmd_maxio_fwdma_write(struct nvme_tool *tool, uint64_t slba, 
	uint32_t nlb)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	struct nvme_maxio_fwdma fwdma = {0};
	struct nvme_64b_cmd cmd = {0};

	fwdma.opcode = 0xf0;
	fwdma.nsid = cpu_to_le32(test->nsid);
	fwdma.option = cpu_to_le32(1); /* set */
	fwdma.bitmask = cpu_to_le32(1); /* bit0 */
	fwdma.slba = cpu_to_le64(slba);
	fwdma.len = cpu_to_le32(nlb);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &fwdma;
	cmd.bit_mask = NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST |
		NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST;
	cmd.data_buf_ptr = tool->wbuf;
	cmd.data_buf_size = nlb * test->lbads;
	cmd.data_dir = DMA_BIDIRECTIONAL;
	
	BUG_ON(tool->wbuf_size < cmd.data_buf_size);

	fill_data_with_random(cmd.data_buf_ptr, cmd.data_buf_size);

	return nvme_submit_64b_cmd(ndev->fd, &cmd);
}

static int nvme_cmd_maxio_fwdma_read(struct nvme_tool *tool, uint64_t slba, 
	uint32_t nlb)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	struct nvme_maxio_fwdma fwdma = {0};
	struct nvme_64b_cmd cmd = {0};

	fwdma.opcode = 0xf0;
	fwdma.nsid = cpu_to_le32(test->nsid);
	fwdma.option = cpu_to_le32(0); /* get */
	fwdma.bitmask = cpu_to_le32(1); /* bit0 */
	fwdma.slba = cpu_to_le64(slba);
	fwdma.len = cpu_to_le32(nlb);

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &fwdma;
	cmd.bit_mask = NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST |
		NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST;
	cmd.data_buf_ptr = tool->rbuf;
	cmd.data_buf_size = nlb * test->lbads;
	cmd.data_dir = DMA_BIDIRECTIONAL;

	BUG_ON(tool->rbuf_size < cmd.data_buf_size);

	return nvme_submit_64b_cmd(ndev->fd, &cmd);
}

static int nvme_maxio_fwdma_rwdata(struct nvme_tool *tool, uint64_t slba, 
	uint32_t nlb, int write)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	if (write)
		ret = nvme_cmd_maxio_fwdma_write(tool, slba, nlb);
	else
		ret = nvme_cmd_maxio_fwdma_read(tool, slba, nlb);

	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, NVME_SC_SUCCESS);
	if (ret < 0)
		return ret;
	
	return 0;
}

static int case_cmd_vendor_test(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct test_data *test = &g_test;
	uint64_t slba = 0;
	uint32_t size = SZ_64K;
	uint32_t nlb = size / test->lbads;
	int ret;

	ret = init_test_data(ndev, test);
	if (ret < 0)
		return ret;

	ret = nvme_maxio_fwdma_rwdata(tool, slba, nlb, 1);
	if (ret < 0)
		return ret;

	ret = nvme_maxio_fwdma_rwdata(tool, slba, nlb, 0);
	if (ret < 0)
		return ret;

	if (memcmp(tool->rbuf, tool->wbuf, nlb * test->lbads)) {
		pr_err("rbuf vs wbuf is different!\n");
		dump_data_to_file(tool->rbuf, nlb * test->lbads, "./rbuf.bin");
		dump_data_to_file(tool->wbuf, nlb * test->lbads, "./wbuf.bin");
	}

	return 0;
}
NVME_CASE_CMD_SYMBOL(case_cmd_vendor_test, "Test vendor command");
