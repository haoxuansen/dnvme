/**
 * @file case_switch_link_speed_width.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2024-07-19
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"
#include "test.h"

#define PCIE_INTERNAL_REG1		0x8c0

static int set_link_width(struct case_data *priv, int width)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	uint32_t reg;
	int ret;

	ret = pci_read_config_dword(ndev->fd, PCIE_INTERNAL_REG1, &reg);
	if (ret < 0)
		return ret;
	
	reg &= 0xffffff80;
	reg |= (0x40 + width);
	ret = pci_write_config_dword(ndev->fd, PCIE_INTERNAL_REG1, reg);
	if (ret < 0)
		return ret;
	
	return 0;
}

static int switch_link_speed_width(struct case_data *priv, int speed, int width)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
	int ret;

	pr_debug("Set PCIe Link Gen%dx%d ...\n", speed, width);
	ret = pcie_set_link_speed(RC_PCI_EXP_REG_LINK_CONTROL2, speed);
	ret |= set_link_width(priv, width);
	ret |= pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);
	if (ret < 0)
		return ret;
	
	ret = pcie_check_link_speed_width(ndev->fd, pdev->express.offset, speed, width);
	if (ret < 0)
		return ret;

	return 0;
}

static int case_switch_link_width_order(struct nvme_tool *tool,
	struct case_data *priv)
{
	int speed = 1;
	int width[] = {1, 2, 4, 2, 1, 4, 1};
	int result[ARRAY_SIZE(width)] = {0};
	int i;
	int ret;
	int loop = 1;
	int err = 0;

	pr_notice("Please enter link speed: ");
	scanf("%d", &speed);
	pr_notice("Please enter loop times: ");
	scanf("%d", &loop);
	pr_info("Set link speed %d, loop %d\n", speed, loop);

	while (loop--) {
		for (i = 0; i < ARRAY_SIZE(width); i++) {
			ret = switch_link_speed_width(priv, speed, width[i]);
			if (ret < 0) {
				result[i]++;
				err++;
			}
		}
	}

	if (err) {
		pr_err("Error count: %d\n", err);

		pr_blue("S\\W \t");
		for (i = 0; i < ARRAY_SIZE(width); i++) {
			pr_blue("%d \t", width[i]);
		}
		pr_blue("\n");

		pr_blue("%d \t", speed);
		for (i = 0; i < ARRAY_SIZE(width); i++) {
			if (result[i])
				pr_red("%d \t", result[i]);
			else
				pr_white("%d \t", result[i]);
		}
		pr_white("\n");
	}

	return -err;
}
NVME_CASE_SYMBOL(case_switch_link_width_order, "?");

static int case_switch_link_speed_order(struct nvme_tool *tool,
	struct case_data *priv)
{
	int speed[] = {1, 3, 1, 4, 3, 4, 1, 5, 3, 5, 4, 5, 1};
	int width = 1;
	int result[ARRAY_SIZE(speed)] = {0};
	int i;
	int ret;
	int loop = 1;
	int err = 0;

	pr_notice("Please enter link width: ");
	scanf("%d", &width);
	pr_notice("Please enter loop times: ");
	scanf("%d", &loop);
	pr_info("Set link width %d, loop %d\n", width, loop);

	while (loop--) {
		for (i = 0; i < ARRAY_SIZE(speed); i++) {
			ret = switch_link_speed_width(priv, speed[i], width);
			if (ret < 0) {
				result[i]++;
				err++;
			}
		}
	}

	if (err) {
		pr_err("Error count: %d\n", err);

		pr_blue("W\\S \t");
		for (i = 0; i < ARRAY_SIZE(speed); i++) {
			pr_blue("%d \t", speed[i]);
		}
		pr_blue("\n");

		pr_blue("%d \t", width);
		for (i = 0; i < ARRAY_SIZE(speed); i++) {
			if (result[i])
				pr_red("%d \t", result[i]);
			else
				pr_white("%d \t", result[i]);
		}
		pr_white("\n");
	}

	return -err;
}
NVME_CASE_SYMBOL(case_switch_link_speed_order, "?");

static int case_switch_link_speed_width_order(struct nvme_tool *tool,
	struct case_data *priv)
{
	int speed[] = {1, 3, 1, 4, 3, 4, 1, 5, 3, 5, 4, 5, 1};
	int width[] = {1, 2, 4, 2, 1, 4, 1};
	int result[ARRAY_SIZE(speed)][ARRAY_SIZE(width)] = {0};
	int i, j;
	int ret = 0;
	int loop = 1;
	int err = 0;

	pr_notice("Please enter loop times: ");
	scanf("%d", &loop);

	while (loop--) {
		for (i = 0; i < ARRAY_SIZE(speed); i++) {
			for (j = 0; j < ARRAY_SIZE(width); j++) {
				ret = switch_link_speed_width(priv, speed[i], width[j]);
				if (ret < 0) {
					result[i][j]++;
					err++;
				}
			}
		}
	}

	if (err) {
		pr_err("Error count: %d\n", err);

		pr_blue("S\\W \t");
		for (i = 0; i < ARRAY_SIZE(width); i++) {
			pr_blue("%d \t", width[i]);
		}
		pr_blue("\n");

		for (i = 0; i < ARRAY_SIZE(speed); i++) {
			pr_blue("%d \t", speed[i]);
			for (j = 0; j < ARRAY_SIZE(width); j++) {
				if (result[i][j])
					pr_red("%d \t", result[i][j]);
				else
					pr_white("%d \t", result[i][j]);
			}
			pr_white("\n");
		}
	}

	return -err;
}
NVME_CASE_SYMBOL(case_switch_link_speed_width_order, "?");

static int case_switch_link_speed_width_random(struct nvme_tool *tool, 
	struct case_data *priv)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
	int ret;
	int loop = 1;
	int err = 0;
	int i;
	uint32_t speed;
	uint32_t width;
	uint32_t save[1024] = {0};
	/*
	 * bit[31:24] = last speed, bit[23:16] = last width
	 * bit[15:8] = current speed, bit[7:0] = current width
	 */
	uint32_t cfg = 0;

	pr_notice("Please enter loop times: ");
	scanf("%d", &loop);

	while (loop--) {
		speed = rand() % 5 + 1;
		do {
			width = rand() % 4 + 1;
		} while (width == 3);

		ret = switch_link_speed_width(priv, speed, width);
		if (ret < 0) {
			cfg |= ((speed & 0xff) << 8) | (width & 0xff);
			if (err < ARRAY_SIZE(save))
				save[err] = cfg;
			else
				pr_warn("WARN: buf is too small to save!\n");
			err++;

			ret = pcie_get_link_speed_width(ndev->fd, 
				pdev->express.offset, 
				(int *)&speed, (int *)&width);
			if (ret < 0)
				return ret;
		}
		/* record the speed & width after last switch */
		cfg = ((speed & 0xff) << 24) | ((width & 0xff) << 16);
	}

	if (err) {
		pr_err("Error count: %d\n", err);
		for (i = 0; i < err && i < ARRAY_SIZE(save); i++) {
			pr_white("id:%d, data:0x%08x\n", i, save[i]);
		}
	}

	return -err;
}
NVME_CASE_SYMBOL(case_switch_link_speed_width_random, "?");
