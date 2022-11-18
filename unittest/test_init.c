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

#include "../dnvme_interface.h"
#include "../dnvme_ioctls.h"

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
		LOG_ERROR("set queue num failed!\n");
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
		LOG_ERROR("acq_num or sq_id is wrong!!!\n");
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
		LOG_ERROR("[E]nvme_idfy_ctrl\n");
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
		LOG_ERROR("[E]nvme_idfy_ns\n");
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

		// LOG_INFO("random_map:sq_id:%#x, cq_id:%#x, cq_vct:%#x\n", ctrl_sq_info[i].sq_id, ctrl_sq_info[i].cq_id, ctrl_sq_info[i].cq_int_vct);
	}
	for (dword_t i = 1; i <= g_nvme_dev.max_sq_num; i++)
	{
		ctrl_sq_info[i - 1].sq_size = rand() % (g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes - 512) + 512;
		ctrl_sq_info[i - 1].cq_size = rand() % (g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes - 512) + 512;
		// LOG_INFO("init_sq_size:%#x,cq_size:%#x\n", ctrl_sq_info[i - 1].sq_size, ctrl_sq_info[i - 1].cq_size);
	}
	LOG_INFO("\n");
}

void test_init(int file_desc)
{
	int ret_val = FAILED;
	uint16_t iosq_num, iocq_num;
	uint8_t flbas;
	uint32_t u32_tmp_data = 0;
	uint8_t sqes = 0;
	uint8_t cqes = 0;
	LOG_INFO("--->[%s]\n", __FUNCTION__);
	int ret = FAILED;
	ret = ioctl_disable_ctrl(file_desc, ST_DISABLE_COMPLETELY);
	ASSERT(ret == SUCCEED);
	ioctl_create_acq(file_desc, MAX_ADMIN_QUEUE_SIZE);
	ioctl_create_asq(file_desc, MAX_ADMIN_QUEUE_SIZE);
	set_irqs(file_desc, INT_PIN, 1);
	ioctl_enable_ctrl(file_desc);

	//step4: send get feature cmd (get queue number)
	LOG_INFO("Send set feature cmd (get queue number)\n");
	set_queue_num(file_desc, &iosq_num, &iocq_num);
	LOG_INFO("\tDevice support: %d iosq, %d iocq\n", iosq_num + 1, iocq_num + 1);

	g_nvme_dev.max_sq_num = (iosq_num + 1);
	g_nvme_dev.max_cq_num = (iocq_num + 1);
	LOG_INFO("\tWe use %d iosq, %d iocq\n", iosq_num + 1, iocq_num + 1);
	/**********************************************************************/
	ctrl_sq_info = (struct nvme_sq_info *)(malloc(g_nvme_dev.max_sq_num * sizeof(struct nvme_sq_info)));
	if (ctrl_sq_info == NULL)
	{
		LOG_ERROR("Malloc Failed\n");
	}
	for (dword_t i = 1; i <= g_nvme_dev.max_sq_num; i++)
	{
		ctrl_sq_info[i - 1].sq_id = i;
		ctrl_sq_info[i - 1].cq_id = i;
		ctrl_sq_info[i - 1].cq_int_vct = i;
		LOG_INFO("init_map sq_id:%#x,cq_id:%#x\n", ctrl_sq_info[i - 1].sq_id, ctrl_sq_info[i - 1].cq_id);
	}
	/**********************************************************************/
	// step6: send identify cmd
	identify_control(file_desc, &g_nvme_dev.id_ctrl);

	LOG_COLOR(GREEN_LOG, "identify ctrl info:\n");
	LOG_INFO("PCI Vendor ID (VID): %#x\n", g_nvme_dev.id_ctrl.vid);
	LOG_INFO("PCI Subsystem Vendor ID (SSVID): %#x\n", g_nvme_dev.id_ctrl.ssvid);
	LOG_INFO("Model Name(MN): %s\n", g_nvme_dev.id_ctrl.mn);
	LOG_INFO("Serial Number (SN): %s\n", g_nvme_dev.id_ctrl.sn);
	LOG_INFO("Maximum Data Transfer Size 2^n (MDTS): %#x\n", g_nvme_dev.id_ctrl.mdts);
	LOG_INFO("Controller ID (CNTLID): %#x\n", g_nvme_dev.id_ctrl.cntlid);
	LOG_INFO("Version (VER): %#x\n", g_nvme_dev.id_ctrl.ver);
	LOG_INFO("Submission Queue Entry Size: %#x\n", g_nvme_dev.id_ctrl.sqes);
	LOG_INFO("Completion Queue Entry Size: %#x\n", g_nvme_dev.id_ctrl.cqes);
	LOG_INFO("Number of Namespaces (NN): %d\n", g_nvme_dev.id_ctrl.nn);
	LOG_INFO("SGL support (SGLS): %#x\n", g_nvme_dev.id_ctrl.sgls);

	//step1: disable control
	ioctl_disable_ctrl(file_desc, ST_DISABLE_COMPLETELY);
	//step 2.1: configure Admin queue
	LOG_INFO("Init Admin cq, qsize:%d\n", MAX_ADMIN_QUEUE_SIZE);
	ioctl_create_acq(file_desc, MAX_ADMIN_QUEUE_SIZE);
	LOG_INFO("Init Admin sq, qsize:%d\n", MAX_ADMIN_QUEUE_SIZE);
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
		LOG_ERROR("Malloc Failed\n");
	}
	/**********************************************************************/

	for (uint32_t ns_idx = 0; ns_idx < g_nvme_dev.id_ctrl.nn; ns_idx++)
	{
		LOG_COLOR(GREEN_LOG, "identify nsid:%d info:\n", ns_idx + 1);
		identify_ns(file_desc, (ns_idx + 1), (void *)&g_nvme_ns_info[ns_idx].id_ns);
		flbas = g_nvme_ns_info[ns_idx].id_ns.flbas & 0xf;
		g_nvme_ns_info[ns_idx].nsze = g_nvme_ns_info[ns_idx].id_ns.nsze;
		g_nvme_ns_info[ns_idx].lbads = (1 << g_nvme_ns_info[ns_idx].id_ns.lbaf[flbas].ds);
		LOG_INFO("nlbaf:%#x\n", g_nvme_ns_info[ns_idx].id_ns.nlbaf);
		LOG_INFO("flbas:%#x\n", flbas);
		LOG_INFO("lbaf.ds(2^n):%d\n", g_nvme_ns_info[ns_idx].id_ns.lbaf[flbas].ds);
		LOG_INFO("nsze:%#lx\n", g_nvme_ns_info[ns_idx].nsze);
		LOG_INFO("lbads:%d\n", g_nvme_ns_info[ns_idx].lbads);

		ASSERT(g_nvme_ns_info[ns_idx].id_ns.lbaf[flbas].ds);
		ASSERT(g_nvme_ns_info[ns_idx].nsze);
	}

	LOG_COLOR(GREEN_LOG, "ns 0 disk_max_lba: %#lx \n", g_nvme_ns_info[0].nsze);

	g_nvme_dev.pmcap_ofst = pci_find_cap_ofst(file_desc, PCI_PMCAP_ID);
	g_nvme_dev.msicap_ofst = pci_find_cap_ofst(file_desc, PCI_MSICAP_ID);
	g_nvme_dev.pxcap_ofst = pci_find_cap_ofst(file_desc, PCI_PXCAP_ID);
	LOG_COLOR(GREEN_LOG, "pcie_regs_info:\n");
	LOG_INFO("pmcap_ofst: %#x\n", g_nvme_dev.pmcap_ofst);
	LOG_INFO("msicap_ofst: %#x\n", g_nvme_dev.msicap_ofst);
	LOG_INFO("pxcap_ofst: %#x\n", g_nvme_dev.pxcap_ofst);

	ret_val = read_nvme_register(file_desc, 0, sizeof(struct reg_nvme_ctrl), (uint8_t *)&g_nvme_dev.ctrl_reg);
	if (ret_val < 0)
	{
		LOG_ERROR("[E] read ctrlr register ret_val:%d!\n", ret_val);
	}
	for (dword_t i = 1; i <= g_nvme_dev.max_sq_num; i++)
	{
		ctrl_sq_info[i - 1].sq_size = WORD_RAND() % (g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes - 512) + 512;
		ctrl_sq_info[i - 1].cq_size = WORD_RAND() % (g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes - 512) + 512;
		LOG_INFO("init sq_size:%#x,cq_size:%#x\n", ctrl_sq_info[i - 1].sq_size, ctrl_sq_info[i - 1].cq_size);
	}

	LOG_COLOR(GREEN_LOG, "ctrl_regs_info:\n");
	LOG_INFO("CAP Maximum Queue Entries Supported: %d\n", g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes);
	LOG_INFO("CAP Timeout:%d\n", g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_to);
	LOG_INFO("CAP Memory Page Size Maximum: %d\n", g_nvme_dev.ctrl_reg.nvme_cap1.bits.cap_mpsmax);
	LOG_INFO("CAP Memory Page Size Minimum: %d\n", g_nvme_dev.ctrl_reg.nvme_cap1.bits.cap_mpsmin);

	LOG_INFO("CC I/O Completion Queue Entry Size: %d\n", g_nvme_dev.ctrl_reg.nvme_cc.bits.cc_iocqes);
	LOG_INFO("CC I/O Submission Queue Entry Size: %d\n", g_nvme_dev.ctrl_reg.nvme_cc.bits.cc_iosqes);

	LOG_INFO("CC Memory Page Size : %d\n", g_nvme_dev.ctrl_reg.nvme_cc.bits.cc_mps);

	LOG_INFO("Arbitration Mechanism Supported : %d\n", g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_ams);
	LOG_INFO("Arbitration Mechanism Selected : %d\n", g_nvme_dev.ctrl_reg.nvme_cc.bits.cc_ams);

	LOG_INFO("<---[%s]\n", __FUNCTION__);

    //displaly power up link status
    u32_tmp_data = pci_read_byte(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
    g_nvme_dev.link_speed = u32_tmp_data & 0x0F;
    g_nvme_dev.link_width = (u32_tmp_data >> 4) & 0x3F;
    LOG_COLOR(SKBLU_LOG, "\nCurrent link status: Gen%d, X%d\n", g_nvme_dev.link_speed, g_nvme_dev.link_width);
}

void test_change_init(int file_desc, uint32_t asqsz, uint32_t acqsz, enum nvme_irq_type irq_type, uint16_t num_irqs)
{
	uint32_t u32_tmp_data = 0;
	int ret = FAILED;
	ret = ioctl_disable_ctrl(file_desc, ST_DISABLE_COMPLETELY);
	ASSERT(ret == SUCCEED);
	ioctl_create_asq(file_desc, asqsz);
	ioctl_create_acq(file_desc, acqsz);
	set_irqs(file_desc, irq_type, num_irqs);
	ioctl_enable_ctrl(file_desc);

	u32_tmp_data = 0x00460001;
	ioctl_write_data(file_desc, NVME_REG_CC_OFST, 4, (uint8_t *)&u32_tmp_data);
}

void test_change_irqs(int file_desc, enum nvme_irq_type irq_type, uint16_t num_irqs)
{
	ioctl_disable_ctrl(file_desc, ST_DISABLE);
	set_irqs(file_desc, irq_type, num_irqs);
	ioctl_enable_ctrl(file_desc);
}

void test_set_admn(int file_desc, uint32_t asqsz, uint32_t acqsz)
{
	ioctl_disable_ctrl(file_desc, ST_DISABLE);
	ioctl_create_acq(file_desc, acqsz);
	ioctl_create_asq(file_desc, asqsz);
	ioctl_enable_ctrl(file_desc);
}
