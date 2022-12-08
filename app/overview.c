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

#include "common_define.h"
#include "overview.h"
#include "test_init.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_bug_trace.h"
#include "unittest.h"
#include "case_all.h"

#define INIT_CASE(id, _func, _desc) \
	[id] = {.name = #_func, .desc = _desc, .func = _func}

typedef int (*case_func_t)(void);

struct nvme_case {
	const char	*name;
	const char	*desc;
	case_func_t	func;
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

static int case_disable_ctrl_complete(void)
{
	return ioctl_disable_ctrl(file_desc, NVME_ST_DISABLE_COMPLETE);
}

static int case_reinit_device(void)
{
	/* !TODO: Check return value! */
	test_init(file_desc);
	return 0;
}

static int case_create_discontig_queue(void)
{
	uint16_t sq_id = 1;
	uint16_t cq_id = 1;
	uint32_t sq_size = 65472; /* 64 * 1023 */
	uint32_t cq_size = 65280; /* 16 * 4080 */

	/* !TODO: Check return value! */
	pr_notice("Create discontig cq_id:%d, cq_size = %d\n", cq_id, cq_size);
	nvme_create_discontig_iocq(file_desc, cq_id, cq_size, true, 
		cq_id, discontg_cq_buf, DISCONTIG_IO_CQ_SIZE);

	pr_notice("Create discontig sq_id:%d, assoc cq_id = %d, sq_size = %d\n", 
		sq_id, cq_id, sq_size);
	nvme_create_discontig_iosq(file_desc, sq_id, cq_id, sq_size, 
		MEDIUM_PRIO, discontg_sq_buf, DISCONTIG_IO_SQ_SIZE);
	return 0;
}

static int case_create_contig_queue(void)
{
	uint16_t sq_id = 1;
	uint16_t cq_id = 1;
	uint32_t sq_size = g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes;
	uint32_t cq_size = g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes;

	/* !TODO: Check return value! */
	pr_notice("Create contig cq_id:%d, cq_size = %d\n", cq_id, cq_size);
	nvme_create_contig_iocq(file_desc, cq_id, cq_size, true, cq_id);

	pr_notice("Create contig sq_id:%d, assoc cq_id = %d, sq_size = %d\n", 
		sq_id, cq_id, sq_size);
	nvme_create_contig_iosq(file_desc, sq_id, cq_id, sq_size, MEDIUM_PRIO);
	return 0;
}

static int case_delete_queue(void)
{
	uint16_t sq_id = 1;
	uint16_t cq_id = 1;

	/* !TODO: Check return value! */
	pr_notice("Deleting SQID:%d,CQID:%d\n", sq_id, cq_id);
	nvme_delete_ioq(file_desc, nvme_admin_delete_sq, sq_id);
	nvme_delete_ioq(file_desc, nvme_admin_delete_cq, cq_id);
	return 0;
}

static int case_send_io_write_cmd(void)
{
	int ret;
	uint16_t sq_id = 1;
	uint16_t cq_id = 1;
	uint32_t i;
	uint64_t wr_slba = 0;
	uint32_t wr_nsid = 1;
	uint16_t wr_nlb = 8;
	uint32_t cmd_cnt = 0;
	uint32_t reap_num;

	pr_notice("\nTest: Sending IO Write Command through sq_id %u\n", sq_id);
	for (i = 0; i < RW_BUFFER_SIZE / 4; i += 4) {
		*(uint32_t *)(write_buffer + i) = i;
	}

	pr_info("slba:%ld nlb:%d\n", wr_slba, wr_nlb);

	ret = nvme_io_write_cmd(file_desc, 0, sq_id, wr_nsid, wr_slba, wr_nlb, 
		0, write_buffer);
	cmd_cnt++;
	if (ret == 0) {
		/* !TODO: Check return value! */
		ioctl_tst_ring_dbl(file_desc, sq_id);
		pr_info("Ringing Doorbell for sq_id %d\n", sq_id);
		cq_gain(cq_id, cmd_cnt, &reap_num);
		pr_info("cq reaped ok! reap_num:%d\n", reap_num);
	}
	return 0;
}

/* !TODO: case_send_io_read_cmd */
static int case_send_io_read_cmd(void)
{
	uint16_t sq_id = 1;
	uint16_t cq_id = 1;
	uint64_t wr_slba = 0;
	uint32_t wr_nsid = 1;
	uint16_t wr_nlb = 32; /* !NOTE: why different from case_send_io_write_cmd */
	uint32_t cmd_cnt = 0;
	uint32_t reap_num;

	pr_info("\nTest: Sending IO Read Command through sq_id %u\n", sq_id);

	memset(read_buffer, 0, wr_nlb * LBA_DAT_SIZE);
	// for (index = 0; index < 150; index++)
	{
		// wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
		// wr_nlb = WORD_RAND() % 255 + 1;
		if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
		{
			// nvme_io_write_cmd(file_desc, 0, sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
			// cmd_cnt++;
			nvme_io_read_cmd(file_desc, 0, sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
			//nvme_io_read_cmd(file_desc, 0, sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
			cmd_cnt++;
		}
	}
	pr_info("Ringing Doorbell for sq_id %u\n", sq_id);
	ioctl_tst_ring_dbl(file_desc, sq_id);
	cq_gain(cq_id, cmd_cnt, &reap_num);
	pr_info("cq reaped ok! reap_num:%d\n", reap_num);
	// ioctl_tst_ring_dbl(file_desc, 0);
	return 0;
}

/* !TODO: case_send_io_compare_cmd */
static int case_send_io_compare_cmd(void)
{
	uint16_t io_sq_id = 1;
	uint16_t io_cq_id = 1;
	uint64_t wr_slba = 0;
	uint16_t wr_nlb = 32;
	uint32_t reap_num;

	pr_info("Send IO cmp cmd slba=%ld, this shouldn't be output warning!(FPGA-06 may not work!)\n", wr_slba);
	ioctl_send_nvme_compare(file_desc, io_sq_id, wr_slba, wr_nlb, FUA_DISABLE, read_buffer, wr_nlb * LBA_DAT_SIZE);

	ioctl_tst_ring_dbl(file_desc, io_sq_id);
	cq_gain(io_cq_id, 1, &reap_num);
	pr_info("  cq reaped ok! reap_num:%d\n", reap_num);

	pr_info("Send IO cmp cmd slba=%ld, this should be output warning!\n", wr_slba + 3);
	ioctl_send_nvme_compare(file_desc, io_sq_id, wr_slba + 3, wr_nlb, FUA_DISABLE, read_buffer, wr_nlb * LBA_DAT_SIZE);
	ioctl_tst_ring_dbl(file_desc, io_sq_id);
	cq_gain(io_cq_id, 1, &reap_num);
	pr_info("cq reaped ok! reap_num:%d\n", reap_num);
	return 0;
}

static int case_display_rw_buffer(void)
{
	uint16_t wr_nlb = 8;

	pr_info("\nwrite_buffer Data:\n");
	mem_disp(write_buffer, wr_nlb * LBA_DAT_SIZE);
	pr_info("\nRead_buffer Data:\n");
	mem_disp(read_buffer, wr_nlb * LBA_DAT_SIZE);
	return 0;
}

static int case_compare_rw_buffer(void)
{
	uint16_t wr_nlb = 8;

	dw_cmp(write_buffer, read_buffer, wr_nlb * LBA_DAT_SIZE);
	return 0;
}

static int case_encrypt_decrypt(void)
{
	test_encrypt_decrypt();
	return 0;
}

static int case_test_meta(void)
{
	test_meta(file_desc);
	return 0;
}

static int case_unknown1(void)
{
	uint32_t io_sq_id = 1;
	uint32_t io_cq_id = 1;
	uint64_t wr_slba = 0;
	uint32_t wr_nsid = 1;
	uint16_t wr_nlb = 8;

	pr_color(LOG_COLOR_CYAN, "pls enter wr_slba:");
	fflush(stdout);
	scanf("%d", (int *)&wr_slba);
	pr_color(LOG_COLOR_CYAN, "pls enter wr_nlb:");
	fflush(stdout);
	scanf("%d", (int *)&wr_nlb);
	memset(write_buffer, BYTE_RAND(), wr_nlb * LBA_DATA_SIZE(wr_nsid));
	create_meta_buf(file_desc, 0);

	if(SUCCEED == send_nvme_write_using_metabuff(file_desc, 0, io_sq_id, 
		wr_nsid, wr_slba, wr_nlb, 0, 0, write_buffer))
	{
		if (SUCCEED == nvme_ring_dbl_and_reap_cq(file_desc, 
			io_sq_id, io_cq_id, 1))
		{
			pr_info("io write succeed\n");
		}
	}
	ioctl_meta_node_delete(file_desc, 0);
	return 0;
}

static int case_unknown2(void)
{
	uint32_t io_sq_id = 1;
	uint32_t io_cq_id = 1;
	uint64_t wr_slba = 0;
	uint32_t wr_nsid = 1;
	uint16_t wr_nlb = 8;

	memset(read_buffer, 0, wr_nlb * LBA_DATA_SIZE(wr_nsid));
	create_meta_buf(file_desc, 0);

	if (SUCCEED == send_nvme_read_using_metabuff(file_desc, 0, io_sq_id, 
		wr_nsid, wr_slba, wr_nlb, 0, 0,read_buffer))
	{
		if (SUCCEED == nvme_ring_dbl_and_reap_cq(file_desc, io_sq_id,
			io_cq_id, 1))
		{
			pr_info("io read succeed\n");
		}
	}
	ioctl_meta_node_delete(file_desc, 0);
	return 0;
}

static int case_unknown3(void)
{
	int ret;
	uint32_t io_sq_id = 1;
	uint32_t io_cq_id = 1;
	uint64_t wr_slba = 0;
	uint32_t wr_nsid = 1;
	uint16_t wr_nlb = 8;
	uint32_t test_loop = 1;
	uint32_t cmd_cnt = 0;
	uint32_t reap_num;
	uint32_t data_len = 0;
	struct fwdma_parameter fwdma_parameter = {0};

	pr_color(LOG_COLOR_CYAN, "pls enter loop cnt:");
	fflush(stdout);
	scanf("%d", &test_loop);
	while (test_loop--)
	{
		for (uint32_t ns_idx = 0; ns_idx < g_nvme_dev.id_ctrl.nn; ns_idx++)
		{
			wr_nsid = ns_idx + 1;
			wr_slba = 0;
			wr_nlb = BYTE_RAND() % 32;
			memset(read_buffer, 0, RW_BUFFER_SIZE);
			memset(write_buffer, BYTE_RAND(), wr_nlb * LBA_DATA_SIZE(wr_nsid));
			pr_info("sq_id:%d nsid:%d lbads:%d slba:%ld nlb:%d\n", io_sq_id, wr_nsid, LBA_DATA_SIZE(wr_nsid), wr_slba, wr_nlb);
			cmd_cnt = 0;
			ret = nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
			cmd_cnt++;
			if (ret == SUCCEED)
			{
				ioctl_tst_ring_dbl(file_desc, io_sq_id);
				cq_gain(io_cq_id, cmd_cnt, &reap_num);
				pr_info("  cq reaped ok! reap_num:%d\n", reap_num);
			}
			cmd_cnt = 0;
			
			data_len = 40 * 4;
			pr_info("send_maxio_fwdma_wr\n");
			//memset((uint8_t *)write_buffer, rand() % 0xff, data_len);
			fwdma_parameter.addr = write_buffer;
			fwdma_parameter.cdw10 = data_len;  //data_len
			fwdma_parameter.cdw11 = 0x40754C0; //axi_addr
			nvme_maxio_fwdma_wr(file_desc, &fwdma_parameter);
			ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
			cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
			pr_info("\nfwdma wr cmd send done!\n");

			ret = nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
			cmd_cnt++;
			if (ret == SUCCEED)
			{
				ioctl_tst_ring_dbl(file_desc, io_sq_id);
				cq_gain(io_cq_id, cmd_cnt, &reap_num);
				pr_info("  cq reaped ok! reap_num:%d\n", reap_num);
			}
			if (SUCCEED == dw_cmp(write_buffer, read_buffer, wr_nlb * LBA_DATA_SIZE(wr_nsid)))
			{
				pr_color(LOG_COLOR_GREEN, "dw_cmp pass!\n");
			}
		}
	}
	return 0;
}

static int case_unknown4(void)
{
	uint32_t data_len = 0;
	uint32_t reap_num;
	struct fwdma_parameter fwdma_parameter = {0};

	pr_info("host2reg tets send_maxio_fwdma_rd\n");
	data_len = 4 * 4;
	fwdma_parameter.addr = read_buffer;
	fwdma_parameter.cdw10 = data_len;  //data_len
	fwdma_parameter.cdw11 = 0x4055500; //axi_addr
	//fwdma_parameter.cdw12 |= (1<<0);               //flag bit[0] crc chk,
	//fwdma_parameter.cdw12 |= (1<<1);               //flag bit[1] hw data chk(only read)
	fwdma_parameter.cdw12 |= (1 << 2); //flag bit[2] dec chk,
	nvme_maxio_fwdma_rd(file_desc, &fwdma_parameter);
	ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
	cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
	pr_info("\tfwdma wr cmd send done!\n");
	pr_info("host2reg tets send_maxio_fwdma_rd\n");
	//memset((uint8_t *)write_buffer, rand() % 0xff, data_len);
	fwdma_parameter.addr = read_buffer;
	fwdma_parameter.cdw10 = data_len;  //data_len
	fwdma_parameter.cdw11 = 0x4055500; //axi_addr
	//fwdma_parameter.cdw12 |= (1<<0);              //flag bit[0] crc chk,
	fwdma_parameter.cdw12 &= ~(1 << 2); //flag bit[2] enc chk,

	nvme_maxio_fwdma_wr(file_desc, &fwdma_parameter);
	ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
	cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
	pr_info("\tfwdma wr cmd send done!\n");
	return 0;
}

static int case_write_fwdma(void)
{
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
			fwdma_parameter.addr = write_buffer;
			fwdma_parameter.cdw10 = 4096; //data_len
			// fwdma_parameter.cdw11 = 0x4078000;              //axi_addr
			// fwdma_parameter.cdw12 |= (1<<0);                //flag bit[0] crc chk,
			// fwdma_parameter.cdw12 &= ~(1<<1);             //flag bit[1] hw data chk(only read)
			// fwdma_parameter.cdw12 |= (1<<2);                //flag bit[2] enc chk,
			nvme_maxio_fwdma_wr(file_desc, &fwdma_parameter);
		}
		ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
		cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
		pr_info("\nfwdma wr cmd send done!\n");
	}
	return 0;
}

static int case_read_fwdma(void)
{
	uint32_t data_len = 0;
	uint32_t reap_num;
	struct fwdma_parameter fwdma_parameter = {0};

	pr_info("send_maxio_fwdma_rd\n");
	data_len = 40 * 4;
	fwdma_parameter.addr = read_buffer;
	fwdma_parameter.cdw10 = data_len;  //data_len
	fwdma_parameter.cdw11 = 0x40754C0; //axi_addr
	//fwdma_parameter.cdw12 |= (1<<0);               //flag bit[0] crc chk,
	//fwdma_parameter.cdw12 |= (1<<1);               //flag bit[1] hw data chk(only read)
	// fwdma_parameter.cdw12 |= (1<<2);                 //flag bit[2] dec chk,
	nvme_maxio_fwdma_rd(file_desc, &fwdma_parameter);
	ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
	cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
	pr_info("\nfwdma wr cmd send done!\n");

	dw_cmp(write_buffer, read_buffer, data_len);
	return 0;
}

static int case_unknown5(void)
{
	uint32_t index = 0;
	uint32_t reap_num;
	uint16_t wr_nlb = 8;
	struct fwdma_parameter fwdma_parameter = {0};

	for (index = 0; index < 400; index++)
	{
		pr_info("send_fwdma_wr/rd test cnt:%d\n", index);
		wr_nlb = (BYTE_RAND() % 8) + 1;
		pr_info("nlb:%d\n", wr_nlb);
		memset((uint8_t *)write_buffer, rand() % 0xff, wr_nlb * LBA_DAT_SIZE);
		fwdma_parameter.cdw10 = wr_nlb * LBA_DAT_SIZE; //data_len
		fwdma_parameter.cdw11 = 0x40754C0;             //axi_addr
		fwdma_parameter.cdw12 |= (1 << 0);             //flag bit[0] crc chk,
		fwdma_parameter.cdw12 |= (1 << 1);             //flag bit[1] hw data chk(only read)
		fwdma_parameter.cdw12 |= (1 << 2);             //flag bit[2] enc/dec chk,

		fwdma_parameter.addr = write_buffer;
		nvme_maxio_fwdma_wr(file_desc, &fwdma_parameter);
		ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
		cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);

		fwdma_parameter.addr = read_buffer;
		nvme_maxio_fwdma_rd(file_desc, &fwdma_parameter);
		ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
		cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);

		if (FAILED == dw_cmp(write_buffer, read_buffer, wr_nlb * LBA_DAT_SIZE))
		{
			pr_info("\nwrite_buffer Data:\n");
			mem_disp(write_buffer, wr_nlb * LBA_DAT_SIZE);
			pr_info("\nRead_buffer Data:\n");
			mem_disp(read_buffer, wr_nlb * LBA_DAT_SIZE);
			break;
		}
	}
	return 0;
}

static int case_test_full_disk_wr(void)
{
	uint32_t cmd_cnt = 0;

	pr_color(LOG_COLOR_CYAN, "test_0_full_disk_wr.pls enter loop cnt:");
	fflush(stdout);
	scanf("%d", &cmd_cnt);
	while (cmd_cnt--)
	{
		test_0_full_disk_wr();
	}
	return 0;
}

static int case_disable_ltr(void)
{
	system("setpci -s 2:0.0  99.b=00");
	return 0;
}

static int case_set_d0_state(void)
{
	pr_info("set to D0 state\n");
	set_power_state(g_nvme_dev.pmcap_ofst, D0);
	return 0;
}

static int case_set_d3_state(void)
{
	pr_info("set to D3 state\n");
	set_power_state(g_nvme_dev.pmcap_ofst, D3hot);
	return 0;
}

static int case_unknown6(void)
{
	uint32_t test_loop = 1;

	pr_color(LOG_COLOR_CYAN, "pcie_set_width:");
	fflush(stdout);
	scanf("%d", &test_loop);
	pcie_set_width(test_loop);
	pcie_retrain_link();
	g_nvme_dev.link_width = test_loop;
	return 0;
}

static int case_unknown7(void)
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

static int case_unknown8(void)
{
	iocmd_cstc_rdy_test();
	return 0;
}

static int case_unknown9(void)
{
	reg_bug_trace();
	return 0;
}

static int case_all_cases(void)
{
	int loop = 0;
	uint32_t test_loop = 1;
	uint32_t u32_tmp_data = 0;

	pr_color(LOG_COLOR_CYAN, "pls enter auto loop cnt:");
	fflush(stdout);
	scanf("%d", &test_loop);
	while (test_loop--)
	{
		loop++;
		pr_color(LOG_COLOR_CYAN, "auto_case_loop_cnt:%d\r\n",loop);
		u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
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
	INIT_CASE(3, case_create_discontig_queue, 
		"Create discontiguous IOSQ and IOCQ"),
	INIT_CASE(4, case_create_contig_queue, 
		"Create contiguous IOSQ and IOCQ"),
	INIT_CASE(5, case_delete_queue, 
		"Delete IOSQ and IOCQ which is created in case 4"),
	INIT_CASE(6, case_send_io_write_cmd, 
		"Send IO write cmd to SQ which is create in case 4"),
	INIT_CASE(7, case_send_io_read_cmd, 
		"Send IO read cmd to SQ which is create in case 4"),
	INIT_CASE(8, case_send_io_compare_cmd, 
		"Send IO Compare cmd which process in case 6 case 7"),
	INIT_CASE(9, case_display_rw_buffer, 
		"Display Write_buffer and Read_buffer Data"),
	INIT_CASE(10, case_compare_rw_buffer, 
		"Compare Write_buffer and Read_buffer Data"),
	INIT_CASE(11, case_encrypt_decrypt, 
		"Encrypt and decrypt (Obsolete?)"),
	INIT_CASE(12, case_test_meta, "Test meta data (Obsolete?)"),
	INIT_CASE(16, case_unknown1, "Unknown1 (Obsolete?)"),
	INIT_CASE(17, case_unknown2, "Unknown2 (Obsolete?)"),
	INIT_CASE(18, case_unknown3, "Unknown3 (Obsolete?)"),
	INIT_CASE(19, case_unknown4, "Unknown4 (Obsolete?)"),
	INIT_CASE(20, case_write_fwdma, "maxio_cmd_fwdma_write"),
	INIT_CASE(21, case_read_fwdma, "maxio_cmd_fwdma_read"),
	INIT_CASE(22, case_unknown5, "Unknown5 (Obsolete?)"),
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
	INIT_CASE(78, case_set_d0_state, "set to D0 state"),
	INIT_CASE(79, case_set_d3_state, "set to D3 state"),
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
	INIT_CASE(98, case_unknown6, "Unknown6 (Obsolete?)"),
	INIT_CASE(99, case_unknown7, "Unknown7 (Obsolete?)"),
	INIT_CASE(100, case_unknown8, "Unknown8 (Obsolete?)"),
	INIT_CASE(101, case_unknown9, "Unknown9 (Obsolete?)"),
	INIT_CASE(255, case_all_cases, "test case list exe"),
};

int case_display_case_list(void)
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

int nvme_select_case_to_execute(void)
{
	int ret;
	int select;

	while (1) {
		fflush(stdout);
		pr_notice("Please enter the case ID that is ready to run: ");
		scanf("%d", &select);

		if (select >= ARRAY_SIZE(g_case_table)) {
			pr_err("The selected case number is out of range(%lu)!"
				"Now will exit...\n", 
				ARRAY_SIZE(g_case_table));
			break;
		}

		if (!g_case_table[select].func) {
			pr_warn("Case %d is empty! please try again\n", select);
			continue;
		}

		ret = g_case_table[select].func();
		if (ret < 0) {
			pr_err("Failed to execute %s!(%d)\n", 
				g_case_table[select].name, ret);
			break;
		}
	} 

	return 0;
}
