/**
 * @file test_init.c
 * @author yumeng (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2019-10-08
 * 
 * @copyright Copyright (c) 2018-2019 Maxio-Tech
 * 
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <malloc.h>

#include "dnvme_interface.h"
#include "dnvme_ioctl.h"

#include "common.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_irq.h"
#include "test_cq_gain.h"

#include "test_init.h"

/**
 * @brief Set the queue num object
 * 
 * @param file_desc 
 * @param iosq_num 
 * @param iocq_num 
 * @return int 
 */
static int set_queue_num(int file_desc, uint16_t *iosq_num, uint16_t *iocq_num)
{
	struct cq_completion *cq_entry = NULL;
	int ret_val = nvme_set_feature_cmd(file_desc, 1, NVME_FEAT_NUM_QUEUES, 0xFF, 0xFF);
	if (SUCCEED != ret_val)
	{
		pr_err("set queue num failed!\n");
		return ret_val;
	}
	ret_val |= nvme_admin_ring_dbl_reap_cq(file_desc);

	//get cq entry
	cq_entry = get_cq_entry();

	// must be admin queue
	if (0 == cq_entry->sq_identifier)
	{
		*iosq_num = (uint16_t)(cq_entry->cmd_specifc & 0xFFFF);
		*iocq_num = (uint16_t)(cq_entry->cmd_specifc >> 16);
	}
	else
	{
		pr_err("acq_num or sq_id is wrong!!!\n");
		ret_val |= FAILED;
	}
	return ret_val;
}

/**
 * @brief identify control
 * 
 * @param file_desc 
 * @param addr 
 * @return int 
 */
int identify_control(int file_desc, void *addr)
{
	int ret_val = nvme_idfy_ctrl(file_desc, addr);
	if (ret_val != SUCCEED)
	{
		pr_err("[E]nvme_idfy_ctrl\n");
		return ret_val;
	}
	return nvme_admin_ring_dbl_reap_cq(file_desc);
}

/**
 * @brief identify namespace
 * 
 * @param file_desc 
 * @param nsid 
 * @param addr 
 * @return int 
 */
int identify_ns(int file_desc, uint32_t nsid, void *addr)
{
	int ret_val = nvme_idfy_ns(file_desc, nsid, FALSE, addr);
	if (ret_val != SUCCEED)
	{
		pr_err("[E]nvme_idfy_ns\n");
		return ret_val;
	}
	return nvme_admin_ring_dbl_reap_cq(file_desc);
}

/**
 * @brief make sq_cq_map_arr random.
 * 
 */
void random_sq_cq_info(void)
{
	dword_t num = 0;
	struct nvme_sq_info temp;
	dword_t cnt = g_nvme_dev.max_sq_num;
	srand((dword_t)time(NULL));
	for (dword_t i = 0; i < (cnt - 1); i++)
	{
		num = i + rand() % (cnt - i);
		temp.cq_id = ctrl_sq_info[i].cq_id;
		ctrl_sq_info[i].cq_id = ctrl_sq_info[num].cq_id;
		ctrl_sq_info[num].cq_id = temp.cq_id;

		num = i + rand() % (cnt - i);
		temp.cq_int_vct = ctrl_sq_info[i].cq_int_vct;
		ctrl_sq_info[i].cq_int_vct = ctrl_sq_info[num].cq_int_vct;
		ctrl_sq_info[num].cq_int_vct = temp.cq_int_vct;

		// pr_info("random_map:sq_id:%#x, cq_id:%#x, cq_vct:%#x\n", ctrl_sq_info[i].sq_id, ctrl_sq_info[i].cq_id, ctrl_sq_info[i].cq_int_vct);
	}
	for (dword_t i = 1; i <= g_nvme_dev.max_sq_num; i++)
	{
		ctrl_sq_info[i - 1].sq_size = rand() % (g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes - 512) + 512;
		ctrl_sq_info[i - 1].cq_size = rand() % (g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes - 512) + 512;
		// pr_info("init_sq_size:%#x,cq_size:%#x\n", ctrl_sq_info[i - 1].sq_size, ctrl_sq_info[i - 1].cq_size);
	}
	pr_info("\n");
}

void test_init(int file_desc)
{
	int ret_val = FAILED;
	uint16_t iosq_num, iocq_num;
	uint8_t flbas;
	uint32_t u32_tmp_data = 0;
	uint8_t sqes = 0;
	uint8_t cqes = 0;
	pr_info("--->[%s]\n", __FUNCTION__);
	int ret = FAILED;
	ret = ioctl_disable_ctrl(file_desc, NVME_ST_DISABLE_COMPLETE);
	assert(ret == SUCCEED);
	ioctl_create_acq(file_desc, MAX_ADMIN_QUEUE_SIZE);
	ioctl_create_asq(file_desc, MAX_ADMIN_QUEUE_SIZE);
	set_irqs(file_desc, INT_PIN, 1);
	ioctl_enable_ctrl(file_desc);

	//step4: send get feature cmd (get queue number)
	pr_info("Send set feature cmd (get queue number)\n");
	set_queue_num(file_desc, &iosq_num, &iocq_num);
	pr_info("\tDevice support: %d iosq, %d iocq\n", iosq_num + 1, iocq_num + 1);

	g_nvme_dev.max_sq_num = (iosq_num + 1);
	g_nvme_dev.max_cq_num = (iocq_num + 1);
	pr_info("\tWe use %d iosq, %d iocq\n", iosq_num + 1, iocq_num + 1);

	// !TODO: It's better to check IOSQs ==IOCQs ?

	/**********************************************************************/
	ctrl_sq_info = (struct nvme_sq_info *)(malloc(g_nvme_dev.max_sq_num * sizeof(struct nvme_sq_info)));
	if (ctrl_sq_info == NULL)
	{
		pr_err("Malloc Failed\n");
	}
	for (dword_t i = 1; i <= g_nvme_dev.max_sq_num; i++)
	{
		ctrl_sq_info[i - 1].sq_id = i;
		ctrl_sq_info[i - 1].cq_id = i;
		ctrl_sq_info[i - 1].cq_int_vct = i;
		pr_info("init_map sq_id:%#x,cq_id:%#x\n", ctrl_sq_info[i - 1].sq_id, ctrl_sq_info[i - 1].cq_id);
	}
	/**********************************************************************/
	// step6: send identify cmd
	identify_control(file_desc, &g_nvme_dev.id_ctrl);

	pr_color(LOG_COLOR_GREEN, "identify ctrl info:\n");
	pr_info("PCI Vendor ID (VID): %#x\n", g_nvme_dev.id_ctrl.vid);
	pr_info("PCI Subsystem Vendor ID (SSVID): %#x\n", g_nvme_dev.id_ctrl.ssvid);
	pr_info("Model Name(MN): %s\n", g_nvme_dev.id_ctrl.mn);
	pr_info("Serial Number (SN): %s\n", g_nvme_dev.id_ctrl.sn);
	pr_info("Maximum Data Transfer Size 2^n (MDTS): %#x\n", g_nvme_dev.id_ctrl.mdts);
	pr_info("Controller ID (CNTLID): %#x\n", g_nvme_dev.id_ctrl.cntlid);
	pr_info("Version (VER): %#x\n", g_nvme_dev.id_ctrl.ver);
	pr_info("Submission Queue Entry Size: %#x\n", g_nvme_dev.id_ctrl.sqes);
	pr_info("Completion Queue Entry Size: %#x\n", g_nvme_dev.id_ctrl.cqes);
	pr_info("Number of Namespaces (NN): %d\n", g_nvme_dev.id_ctrl.nn);
	pr_info("SGL support (SGLS): %#x\n", g_nvme_dev.id_ctrl.sgls);

	//step1: disable control
	ioctl_disable_ctrl(file_desc, NVME_ST_DISABLE_COMPLETE);
	//step 2.1: configure Admin queue
	pr_info("Init Admin cq, qsize:%d\n", MAX_ADMIN_QUEUE_SIZE);
	ioctl_create_acq(file_desc, MAX_ADMIN_QUEUE_SIZE);
	pr_info("Init Admin sq, qsize:%d\n", MAX_ADMIN_QUEUE_SIZE);
	ioctl_create_asq(file_desc, MAX_ADMIN_QUEUE_SIZE);

	//step 2.2: configure Admin queue
	//set_irqs(file_desc, INT_NONE, 0);
	// set_irqs(file_desc, INT_PIN, 1);
	// set_irqs(file_desc, INT_MSI_SINGLE, 1);
	if (g_nvme_dev.id_ctrl.vid == SAMSUNG_CTRL_VID)
		set_irqs(file_desc, INT_PIN, 1);
	else
		set_irqs(file_desc, INT_MSIX, g_nvme_dev.max_sq_num + 1); // min 1, max g_nvme_dev.max_sq_num

	//step3: enable control
	ioctl_enable_ctrl(file_desc);

	//step7: set cq/sq entry size
	sqes = g_nvme_dev.id_ctrl.sqes & 0x0f;
	cqes = g_nvme_dev.id_ctrl.cqes & 0x0f;
	u32_tmp_data = ioctl_read_data(file_desc, NVME_REG_CC_OFST, 4);
	u32_tmp_data &= ~((0xf << 20) | (0xf << 16) | (1 << 0));
	u32_tmp_data |= ((cqes << 20) | sqes << 16) | (1 << 0);
	ioctl_write_data(file_desc, NVME_REG_CC_OFST, 4, (uint8_t *)&u32_tmp_data);

	/**********************************************************************/
	g_nvme_ns_info = (struct nvme_ns *)malloc(g_nvme_dev.id_ctrl.nn * sizeof(struct nvme_ns));
	if (g_nvme_ns_info == NULL)
	{
		pr_err("Malloc Failed\n");
	}
	/**********************************************************************/

	for (uint32_t ns_idx = 0; ns_idx < g_nvme_dev.id_ctrl.nn; ns_idx++)
	{
		pr_color(LOG_COLOR_GREEN, "identify nsid:%d info:\n", ns_idx + 1);
		identify_ns(file_desc, (ns_idx + 1), (void *)&g_nvme_ns_info[ns_idx].id_ns);
		flbas = g_nvme_ns_info[ns_idx].id_ns.flbas & 0xf;
		g_nvme_ns_info[ns_idx].nsze = g_nvme_ns_info[ns_idx].id_ns.nsze;
		g_nvme_ns_info[ns_idx].lbads = (1 << g_nvme_ns_info[ns_idx].id_ns.lbaf[flbas].ds);
		pr_info("nlbaf:%#x\n", g_nvme_ns_info[ns_idx].id_ns.nlbaf);
		pr_info("flbas:%#x\n", flbas);
		pr_info("lbaf.ds(2^n):%d\n", g_nvme_ns_info[ns_idx].id_ns.lbaf[flbas].ds);
		pr_info("nsze:%#lx\n", g_nvme_ns_info[ns_idx].nsze);
		pr_info("lbads:%d\n", g_nvme_ns_info[ns_idx].lbads);

		assert(g_nvme_ns_info[ns_idx].id_ns.lbaf[flbas].ds);
		assert(g_nvme_ns_info[ns_idx].nsze);
	}

	pr_color(LOG_COLOR_GREEN, "ns 0 disk_max_lba: %#lx \n", g_nvme_ns_info[0].nsze);

	g_nvme_dev.pmcap_ofst = pci_find_cap_ofst(file_desc, PCI_PMCAP_ID);
	g_nvme_dev.msicap_ofst = pci_find_cap_ofst(file_desc, PCI_MSICAP_ID);
	g_nvme_dev.pxcap_ofst = pci_find_cap_ofst(file_desc, PCI_PXCAP_ID);
	pr_color(LOG_COLOR_GREEN, "pcie_regs_info:\n");
	pr_info("pmcap_ofst: %#x\n", g_nvme_dev.pmcap_ofst);
	pr_info("msicap_ofst: %#x\n", g_nvme_dev.msicap_ofst);
	pr_info("pxcap_ofst: %#x\n", g_nvme_dev.pxcap_ofst);

	ret_val = read_nvme_register(file_desc, 0, sizeof(struct reg_nvme_ctrl), (uint8_t *)&g_nvme_dev.ctrl_reg);
	if (ret_val < 0)
	{
		pr_err("[E] read ctrlr register ret_val:%d!\n", ret_val);
	}
	for (dword_t i = 1; i <= g_nvme_dev.max_sq_num; i++)
	{
		ctrl_sq_info[i - 1].sq_size = WORD_RAND() % (g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes - 512) + 512;
		ctrl_sq_info[i - 1].cq_size = WORD_RAND() % (g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes - 512) + 512;
		pr_info("init sq_size:%#x,cq_size:%#x\n", ctrl_sq_info[i - 1].sq_size, ctrl_sq_info[i - 1].cq_size);
	}

	pr_color(LOG_COLOR_GREEN, "ctrl_regs_info:\n");
	pr_info("CAP Maximum Queue Entries Supported: %d\n", g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes);
	pr_info("CAP Timeout:%d\n", g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_to);
	pr_info("CAP Memory Page Size Maximum: %d\n", g_nvme_dev.ctrl_reg.nvme_cap1.bits.cap_mpsmax);
	pr_info("CAP Memory Page Size Minimum: %d\n", g_nvme_dev.ctrl_reg.nvme_cap1.bits.cap_mpsmin);

	pr_info("CC I/O Completion Queue Entry Size: %d\n", g_nvme_dev.ctrl_reg.nvme_cc.bits.cc_iocqes);
	pr_info("CC I/O Submission Queue Entry Size: %d\n", g_nvme_dev.ctrl_reg.nvme_cc.bits.cc_iosqes);

	pr_info("CC Memory Page Size : %d\n", g_nvme_dev.ctrl_reg.nvme_cc.bits.cc_mps);

	pr_info("Arbitration Mechanism Supported : %d\n", g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_ams);
	pr_info("Arbitration Mechanism Selected : %d\n", g_nvme_dev.ctrl_reg.nvme_cc.bits.cc_ams);

	pr_info("<---[%s]\n", __FUNCTION__);

    //displaly power up link status
    u32_tmp_data = pci_read_byte(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    g_nvme_dev.link_speed = u32_tmp_data & 0x0F;
    g_nvme_dev.link_width = (u32_tmp_data >> 4) & 0x3F;
    pr_color(LOG_COLOR_CYAN, "\nCurrent link status: Gen%d, X%d\n", g_nvme_dev.link_speed, g_nvme_dev.link_width);
}

void test_change_init(int file_desc, uint32_t asqsz, uint32_t acqsz, enum nvme_irq_type irq_type, uint16_t num_irqs)
{
	uint32_t u32_tmp_data = 0;
	int ret = FAILED;
	ret = ioctl_disable_ctrl(file_desc, NVME_ST_DISABLE_COMPLETE);
	assert(ret == SUCCEED);
	ioctl_create_asq(file_desc, asqsz);
	ioctl_create_acq(file_desc, acqsz);
	set_irqs(file_desc, irq_type, num_irqs);
	ioctl_enable_ctrl(file_desc);

	u32_tmp_data = 0x00460001;
	ioctl_write_data(file_desc, NVME_REG_CC_OFST, 4, (uint8_t *)&u32_tmp_data);
}

void test_change_irqs(int file_desc, enum nvme_irq_type irq_type, uint16_t num_irqs)
{
	ioctl_disable_ctrl(file_desc, NVME_ST_DISABLE);
	set_irqs(file_desc, irq_type, num_irqs);
	ioctl_enable_ctrl(file_desc);
}

void test_set_admn(int file_desc, uint32_t asqsz, uint32_t acqsz)
{
	ioctl_disable_ctrl(file_desc, NVME_ST_DISABLE);
	ioctl_create_acq(file_desc, acqsz);
	ioctl_create_asq(file_desc, asqsz);
	ioctl_enable_ctrl(file_desc);
}
