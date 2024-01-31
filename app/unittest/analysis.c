/**
 * @file analysis.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2024-01-11
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <errno.h>

#include "libbase.h"
#include "libnvme.h"
#include "test.h"

// static const char *s_file = "analysis.log";
static char *s_buf = NULL;

static int alloc_buffer(void)
{
	if (s_buf)
		return 0;
	
	s_buf = zalloc(SZ_1M);
	if (!s_buf) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
	}
	return 0;
}

static void release_buffer(void)
{
	if (!s_buf)
		return;
	
	free(s_buf);
	s_buf = NULL;
}

static int analysis_identify_controller_data_structure(struct nvme_id_ctrl *ctrl)
{
	/* !TODO: */
	return 0;
}

int ut_analysis_entry(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ctrl_instance *ctrl = ndev->ctrl;
	int ret;

	ret = alloc_buffer();
	if (ret < 0)
		return ret;

	analysis_identify_controller_data_structure(ctrl->id_ctrl);

	release_buffer();
	return ret;
}
