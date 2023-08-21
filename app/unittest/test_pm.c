/**
 * @file test_pm.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-12
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "libbase.h"
#include "byteorder.h"
#include "pci_ids_ext.h"
#include "pci_regs_ext.h"

#include "libnvme.h"
#include "test.h"

static int get_power_state(struct nvme_dev_info *ndev, uint8_t *ps)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_get_feat_power_mgmt(ndev->fd, NVME_FEAT_SEL_CUR);
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

	*ps = NVME_POWER_MGMT_TO_PS(le32_to_cpu(entry.result.u32));
	return 0;
}

static int set_power_state(struct nvme_dev_info *ndev, uint8_t ps)
{
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	struct nvme_ctrl_property *prop = ctrl->prop;
	struct nvme_completion entry = {0};
	uint16_t cid;
	uint8_t ps_new;
	int ret;

	ret = nvme_cmd_set_feat_power_mgmt(ndev->fd, ps, 0);
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

	if (prop->vs >= NVME_VS(1, 4, 0)) {
		ps_new = NVME_POWER_MGMT_TO_PS(le32_to_cpu(entry.result.u32));
		if (ps_new != ps) {
			pr_err("expect ps:%u, actual ps:%u\n", ps, ps_new);
			return -EPERM;
		}
		pr_debug("set power state to %u ok!\n", ps);
	}

	return 0;
}

static int select_next_power_state(struct nvme_ctrl_instance *ctrl, uint8_t *sel)
{
	uint8_t npss;
	int ret;

	ret = nvme_id_ctrl_npss(ctrl);
	if (ret < 0) {
		pr_err("failed to get NPSS!(%d)\n", ret);
		return ret;
	}
	npss = ret;

	if (nvme_id_ctrl_vid(ctrl) == PCI_VENDOR_ID_MAXIO) {
		switch (rand() % 3) {
		case 1:
			return 3;
		case 2:
			return 4;
		case 0:
		default:
			return 0;
		}
	} else {
		return rand() % npss;
	}
}

static int case_pm_switch_power_state(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_property *prop = ndev->ctrl->prop;
	uint8_t ps, ps_new;
	uint32_t loop = 100;
	int ret;

	do {
		ret = select_next_power_state(ndev->ctrl, &ps);
		if (ret < 0)
			return ret;

		ret = set_power_state(ndev, ps);
		if (ret < 0)
			return ret;

		if (prop->vs < NVME_VS(1, 4, 0)) {
			ret = get_power_state(ndev, &ps_new);
			if (ret < 0)
				return ret;

			if (ps != ps_new) {
				pr_err("expect ps:%u, actual ps:%u\n", ps, ps_new);
				return -EPERM;
			}
			pr_debug("set power state to %u ok!\n", ps);
		}

	} while (--loop);

	return 0;
}
NVME_CASE_PM_SYMBOL(case_pm_switch_power_state, 
	"Randomly switch NVMe power state and repeat N times");

static int case_pm_set_d0_state(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	int ret;

	ret = pcie_set_power_state(ndev->fd,  ndev->pm.offset, 
		PCI_PM_CTRL_STATE_D0);
	if (ret < 0) {
		pr_err("failed to set PCIe power state to D0!(%d)\n", ret);
		return ret;
	}
	return 0;
}
NVME_CASE_PM_SYMBOL(case_pm_set_d0_state, "Set PCIe power state to D0");

static int case_pm_set_d3hot_state(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	int ret;

	ret = pcie_set_power_state(ndev->fd,  ndev->pm.offset, 
		PCI_PM_CTRL_STATE_D3HOT);
	if (ret < 0) {
		pr_err("failed to set PCIe power state to D3 hot!(%d)\n", ret);
		return ret;
	}
	return 0;
}
NVME_CASE_PM_SYMBOL(case_pm_set_d3hot_state, "Set PCIe power state to D3 hot");
