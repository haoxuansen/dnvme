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

static int case_disable_ctrl_complete(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;

	return nvme_disable_controller_complete(ndev->fd);
}
NVME_CASE_SYMBOL(case_disable_ctrl_complete, "Disable the controller completely");

static int case_reinit_device(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;

	return nvme_reinit(ndev, ndev->asq.size, ndev->acq.size, ndev->irq_type);
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
