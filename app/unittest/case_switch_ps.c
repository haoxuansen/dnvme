#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"

#include "test.h"

static int send_io_cmd(struct case_data *priv, struct nvme_sq_info *sq)
{
	struct nvme_dev_info *ndev = priv->tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	struct case_config_effect effect = {0};
	int ret;

	priv->cfg.effect = &effect;
	effect.nsid = le32_to_cpu(ns_grp->act_list[0]);

	ret = ut_send_io_write_cmd_random_data(priv, sq, 0, 8);
	if (ret < 0)
		return ret;

	ret = ut_send_io_read_cmd(priv, sq, 0, 8);
	if (ret < 0)
		return ret;

	pr_debug("R/W ok\n");
	return 0;
}

static int case_switch_power_state(struct nvme_tool *tool, struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_sq_info *sq = &ndev->iosqs[0];
	int ret;

	ret = ut_create_pair_io_queue(priv, sq, NULL);
	if (ret < 0)
		return ret;
	
	while (ret >= 0) {
		ret |= send_io_cmd(priv, sq);
		ret |= nvme_set_feat_power_mgmt(ndev, 2, 0);
		msleep(50);	
		ret |= nvme_set_feat_power_mgmt(ndev, 0, 0);
		msleep(50);	

		ret |= send_io_cmd(priv, sq);
		ret |= nvme_set_feat_power_mgmt(ndev, 3, 0);
		msleep(50);	
		ret |= nvme_set_feat_power_mgmt(ndev, 0, 0);
		msleep(50);	

		ret |= send_io_cmd(priv, sq);
		ret |= nvme_set_feat_power_mgmt(ndev, 4, 0);
		msleep(50);	
		ret |= nvme_set_feat_power_mgmt(ndev, 0, 0);
		msleep(50);	
	}

	if (ret < 0)
		pr_err("io cmd or switch ps failed!(%d)\n", ret);

	ret |= ut_delete_pair_io_queue(priv, sq, NULL);
	return ret;
}
NVME_CASE_SYMBOL(case_switch_power_state, "?");
