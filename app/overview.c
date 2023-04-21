/**
 * @file overview.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-08
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "defs.h"
#include "log.h"
#include "pci_regs_ext.h"

#include "overview.h"
#include "pci.h"
#include "pcie.h"
#include "meta.h"
#include "queue.h"
#include "test.h"
#include "test_queue.h"
#include "test_cmd.h"
#include "test_pm.h"

#include "common_define.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_bug_trace.h"
#include "unittest.h"
#include "case_all.h"

#define INIT_CASE(id, _func, _desc) \
	[id] = {.name = #_func, .desc = _desc, .func = _func}

typedef int (*case_func_t)(struct nvme_tool *tool);

struct nvme_case {
	const char	*name;
	const char	*desc;
	case_func_t	func;
};

enum {
	CASE_CMD = 101,
	CASE_QUEUE = CASE_CMD + 20,
	CASE_PM = CASE_QUEUE + 20,
	CASE_META = CASE_PM + 20,
};

static TestCase_t TestCaseList[] = {
	TCD(case_register_test),//case_54
	TCD(case_queue_admin),//case_52
	TCD(case_queue_delete_q),//case_31
	TCD(test_0_full_disk_wr),//case_55
	TCD(test_2_mix_case),//case_57
	TCD(test_3_adm_wr_cache_fua),//case_58
	TCD(test_5_fua_wr_rd_cmp),//case_59
	TCD(case_queue_create_q_size),//case_30
	TCD(case_iocmd_write_read),//case_33
	TCD(case_queue_abort),//case_36
	TCD(case_resets_random_all),//case_50
	TCD(case_queue_sq_cq_match),//case_32
	TCD(case_command_arbitration),//case_48
	TCD(case_queue_cq_int_all),//case_40
	TCD(case_queue_cq_int_all_mask),//case_41
	TCD(case_queue_cq_int_coalescing),//case_47
	TCD(case_nvme_boot_partition),//case_53
	TCD(test_6_all_ns_lbads_test),//case_60
};

static int case_disable_ctrl_complete(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;

	return nvme_disable_controller_complete(ndev->fd);
}

static int case_reinit_device(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;

	return nvme_reinit(ndev, ndev->asq.size, ndev->acq.size, ndev->irq_type);
}

static int case_encrypt_decrypt(struct nvme_tool *tool)
{
	test_encrypt_decrypt();
	return 0;
}

static int case_write_fwdma(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	uint32_t i = 0;
	uint32_t index = 0;
	uint32_t reap_num;
	struct fwdma_parameter fwdma_parameter = {0};

	i = 10;
	memset((uint8_t *)&fwdma_parameter, 0, sizeof(fwdma_parameter));
	// pr_color(LOG_COLOR_CYAN,"pls enter loop cnt:");
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

static int case_unknown5(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	uint32_t index = 0;
	uint32_t reap_num;
	uint16_t wr_nlb = 8;
	struct fwdma_parameter fwdma_parameter = {0};

	for (index = 0; index < 400; index++)
	{
		pr_info("send_fwdma_wr/rd test cnt:%d\n", index);
		wr_nlb = (BYTE_RAND() % 8) + 1;
		pr_info("nlb:%d\n", wr_nlb);
		memset((uint8_t *)tool->wbuf, rand() % 0xff, wr_nlb * LBA_DAT_SIZE);
		fwdma_parameter.cdw10 = wr_nlb * LBA_DAT_SIZE; //data_len
		fwdma_parameter.cdw11 = 0x40754C0;             //axi_addr
		fwdma_parameter.cdw12 |= (1 << 0);             //flag bit[0] crc chk,
		fwdma_parameter.cdw12 |= (1 << 1);             //flag bit[1] hw data chk(only read)
		fwdma_parameter.cdw12 |= (1 << 2);             //flag bit[2] enc/dec chk,

		fwdma_parameter.addr = tool->wbuf;
		nvme_maxio_fwdma_wr(ndev->fd, &fwdma_parameter);
		nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
		cq_gain(NVME_AQ_ID, 1, &reap_num);

		fwdma_parameter.addr = tool->rbuf;
		nvme_maxio_fwdma_rd(ndev->fd, &fwdma_parameter);
		nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
		cq_gain(NVME_AQ_ID, 1, &reap_num);

		if (FAILED == dw_cmp(tool->wbuf, tool->rbuf, wr_nlb * LBA_DAT_SIZE))
		{
			pr_info("\nwrite_buffer Data:\n");
			mem_disp(tool->wbuf, wr_nlb * LBA_DAT_SIZE);
			pr_info("\nRead_buffer Data:\n");
			mem_disp(tool->rbuf, wr_nlb * LBA_DAT_SIZE);
			break;
		}
	}
	return 0;
}

static int case_test_full_disk_wr(struct nvme_tool *tool)
{
	uint32_t cmd_cnt = 0;

	pr_color(LOG_COLOR_CYAN, "test_0_full_disk_wr.pls enter loop cnt:");
	fflush(stdout);
	scanf("%d", &cmd_cnt);
	while (cmd_cnt--)
	{
		test_0_full_disk_wr(tool);
	}
	return 0;
}

static int case_disable_ltr(struct nvme_tool *tool)
{
	/* !FIXME: The BDF of NVMe device cannot be fixed. It's better to use
	 * device ID and vendor ID instead! 
	 */
	system("setpci -s 2:0.0  99.b=00");
	return 0;
}

static int case_unknown6(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	uint32_t test_loop = 1;

	pr_color(LOG_COLOR_CYAN, "pcie_set_width:");
	fflush(stdout);
	scanf("%d", &test_loop);
	pcie_set_width(test_loop);
	pcie_retrain_link();
	ndev->link_width = test_loop;
	return 0;
}

static int case_unknown7(struct nvme_tool *tool)
{
	uint32_t test_loop = 1;

	pr_color(LOG_COLOR_CYAN, "pls enter loop cnt:");
	fflush(stdout);
	scanf("%d", &test_loop);
	while (test_loop--)
	{
		pcie_random_speed_width();
	}
	return 0;
}

static int case_unknown8(struct nvme_tool *tool)
{
	iocmd_cstc_rdy_test();
	return 0;
}

static int case_unknown9(struct nvme_tool *tool)
{
	reg_bug_trace();
	return 0;
}

static int case_all_cases(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	int loop = 0;
	int ret;
	uint32_t test_loop = 1;
	uint32_t u32_tmp_data = 0;

	pr_color(LOG_COLOR_CYAN, "pls enter auto loop cnt:");
	fflush(stdout);
	scanf("%d", &test_loop);
	while (test_loop--)
	{
		loop++;
		pr_color(LOG_COLOR_CYAN, "auto_case_loop_cnt:%d\r\n",loop);
		ret = pci_read_config_word(ndev->fd, ndev->express.offset + 0x12, (uint16_t *)&u32_tmp_data);
		if (ret < 0)
			exit(-1);

		pr_color(LOG_COLOR_CYAN, "\nCurrent link status: Gen%d, X%d\n", u32_tmp_data & 0x0F, (u32_tmp_data >> 4) & 0x3F);
		if(test_list_exe(TestCaseList, ARRAY_SIZE(TestCaseList)))
		{
			break;
		}
		random_list(TestCaseList, ARRAY_SIZE(TestCaseList));
		// pcie_random_speed_width();
	}
	pr_color(LOG_COLOR_CYAN, "auto_case_run_loop:%d\r\n",loop);
	return 0;
}

static struct nvme_case g_case_table[] = {
	INIT_CASE(0, case_display_case_list, "Display all supported cases"),
	INIT_CASE(1, case_disable_ctrl_complete, 
		"Disable the controller completely"),
	INIT_CASE(2, case_reinit_device, 
		"Reinitialize the device for running others later"),

	INIT_CASE(20, case_write_fwdma, "maxio_cmd_fwdma_write"),
	INIT_CASE(21, case_read_fwdma, "maxio_cmd_fwdma_read"),
	INIT_CASE(29, test_4_peak_power, "hc peak power test"),
	INIT_CASE(30, case_queue_create_q_size, "case_queue_create_q_size"),
	INIT_CASE(31, case_queue_delete_q, "case_queue_delete_q"),
	INIT_CASE(32, case_queue_sq_cq_match, "case_queue_sq_cq_match"),
	INIT_CASE(33, case_iocmd_write_read, "case_iocmd_write_read"),
	INIT_CASE(36, case_queue_abort, "case_queue_abort"),
	INIT_CASE(37, case_iocmd_fw_io_cmd, "case_fw_io_cmd"),
	INIT_CASE(40, case_queue_cq_int_all, "case_queue_cq_int_all"),
	INIT_CASE(41, case_queue_cq_int_all_mask, "case_queue_cq_int_all_mask"),
	INIT_CASE(42, case_queue_cq_int_msi_multi_mask, "case_queue_cq_int_msi_multi_mask"),
	INIT_CASE(43, case_queue_cq_int_msix_mask, "case_queue_cq_int_msix_mask"),
	INIT_CASE(47, case_queue_cq_int_coalescing, "case_queue_cq_int_coalescing"),
	INIT_CASE(48, case_command_arbitration, "case_command_arbitration"),
	INIT_CASE(49, case_resets_link_down, "case_resets_link_down test"),
	INIT_CASE(50, case_resets_random_all, "case_resets_random_all test"),
	INIT_CASE(52, case_queue_admin, "case_queue_admin"),
	INIT_CASE(53, case_nvme_boot_partition, "case_nvme_boot_partition"),
	INIT_CASE(54, case_register_test, "register test"),
	INIT_CASE(55, case_test_full_disk_wr, "test_0_full_disk_wr test"),
	INIT_CASE(56, test_1_fused, "test_1_fused loop test"),
	INIT_CASE(57, test_2_mix_case, "test_2_mix_case"),
	INIT_CASE(58, test_3_adm_wr_cache_fua, "test_3_adm_wr_cache_fua test"),
	INIT_CASE(59, test_5_fua_wr_rd_cmp, "test_5_fua_wr_rd_cmp test"),
	INIT_CASE(60, test_6_all_ns_lbads_test, "test_6_all_ns_lbads_test"),
	INIT_CASE(77, case_disable_ltr, "set LTR disable"),
	INIT_CASE(80, case_pcie_link_speed_width_step, "pcie_link_speed_width step"),
	INIT_CASE(81, case_pcie_link_speed_width_cyc, "pcie_link_speed_width cyc"),
	INIT_CASE(82, case_pcie_reset_single, "pcie_reset single"),
	INIT_CASE(83, case_pcie_reset_cyc, "pcie_reset cycle"),
	INIT_CASE(84, case_pcie_low_power_L0s_L1_step, "pcie_low_power L0s&L1 step"),
	INIT_CASE(85, case_pcie_low_power_L0s_L1_cyc, "pcie_low_power L0s&L1 cycle"),
	INIT_CASE(86, case_pcie_low_power_L1sub_step, "pcie_low_power L1sub step"),
	INIT_CASE(87, case_pcie_PM, "pcie_PM"),
	INIT_CASE(88, case_pcie_low_power_measure, "pcie_low_power_measure"),
	INIT_CASE(89, case_pcie_low_power_pcipm_l1sub, "pcie_low_power_pcipm_l1sub"),
	INIT_CASE(90, case_pcie_MPS, "pcie_MPS"),
	INIT_CASE(91, case_pcie_MRRS, "pcie_MRRS"),

	INIT_CASE(CASE_CMD, case_cmd_io_write, 
		"Send IO write cmd to IOSQ"),
	INIT_CASE(CASE_CMD + 1, case_cmd_io_read, 
		"Send IO read cmd to IOSQ"),
	INIT_CASE(CASE_CMD + 2, case_cmd_io_compare, 
		"Send IO compare cmd to IOSQ"),
	INIT_CASE(CASE_CMD + 3, case_cmd_io_write_with_fua, 
		"Send IO write cmd with FUA to IOSQ"),
	INIT_CASE(CASE_CMD + 4, case_cmd_io_read_with_fua, 
		"Send IO read cmd with FUA to IOSQ"),

	INIT_CASE(CASE_QUEUE, case_queue_iocmd_to_asq, 
		"Submit IO command to Admin SQ"),
	INIT_CASE(CASE_QUEUE + 1, case_queue_contiguous, 
		"Test contiguous I/O queue"),
	INIT_CASE(CASE_QUEUE + 2, case_queue_discontiguous, 
		"Test discontiguous I/O queue"),

	INIT_CASE(CASE_PM, case_pm_switch_power_state, 
		"Switch NVMe power state randomly"),
	INIT_CASE(CASE_PM + 1, case_pm_set_d0_state, 
		"Set PCIe power state to D0"),
	INIT_CASE(CASE_PM + 2, case_pm_set_d3hot_state, 
		"Set PCIe power state to D3 hot"),

	/* Test meta data */
	INIT_CASE(CASE_META, case_meta_node_contiguous,
		"Create a contiguous meta node and delete it later"),
	INIT_CASE(CASE_META + 1, case_meta_xfer_sgl,
		"Meta data transferred as SGL"),
	INIT_CASE(CASE_META + 2, case_meta_xfer_separate, 
		"Meta data transferred as separate buffer"),
	INIT_CASE(CASE_META + 3, case_meta_xfer_extlba,
		"Meta data transferred contiguous with LBA data"),

#if 1 // Obsolete?
	INIT_CASE(211, case_encrypt_decrypt, 
		"Encrypt and decrypt (Obsolete?)"),
	INIT_CASE(222, case_unknown5, "Unknown5 (Obsolete?)"),
	INIT_CASE(223, case_unknown6, "Unknown6 (Obsolete?)"),
	INIT_CASE(224, case_unknown7, "Unknown7 (Obsolete?)"),
	INIT_CASE(225, case_unknown8, "Unknown8 (Obsolete?)"),
	INIT_CASE(226, case_unknown9, "Unknown9 (Obsolete?)"),
#endif
	INIT_CASE(255, case_all_cases, "test case list exe"),
};

int case_display_case_list(struct nvme_tool *tool)
{
	int i;

	pr_info("\n******  Maxio NVMe Unit Test Case List  ******\n");

	for (i = 0; i < ARRAY_SIZE(g_case_table); i++) {
		if (g_case_table[i].func) {
			printf("case %4d: %s\n", i, g_case_table[i].desc);
		}
	}
	return 0;
}

int nvme_select_case_to_execute(struct nvme_tool *tool)
{
	int ret;
	int select;

	while (1) {
		fflush(stdout);
		pr_notice("Please enter the case ID that is ready to run: ");
		scanf("%d", &select);

		if (select >= ARRAY_SIZE(g_case_table)) {
			pr_warn("The selected case number is out of range(%lu)!"
				"Now will exit...\n", 
				ARRAY_SIZE(g_case_table));
			break;
		}

		if (!g_case_table[select].func) {
			pr_warn("Case %d is empty! please try again\n", select);
			continue;
		}

		ret = g_case_table[select].func(tool);
		if (ret < 0) {
			pr_err("Failed to execute %s!(%d)\n", 
				g_case_table[select].name, ret);
		} else {
			pr_info("'%s' success!\n", g_case_table[select].desc);
		}
	} 

	return 0;
}
