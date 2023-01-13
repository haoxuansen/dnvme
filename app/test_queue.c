/**
 * @file test_queue.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-06
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <errno.h>

#include "log.h"
#include "dnvme_ioctl.h"

#include "core.h"
#include "cmd.h"
#include "test.h"
#include "test_metrics.h"
#include "test_queue.h"

static int send_io_read_to_asq(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_info *nss = ndev->nss;
	struct nvme_rwc_wrapper wrap = {0};
	int ret;

	wrap.sqid = NVME_AQ_ID;
	wrap.cqid = NVME_AQ_ID;
	wrap.nsid = nss[0].nsid;
	wrap.slba = 0;
	wrap.nlb = 8;
	wrap.buf = tool->rbuf;
	wrap.size = wrap.nlb * nss[0].lbads;

	ret = nvme_io_read(ndev->fd, &wrap);
	if (ret < 0) {
		pr_debug("ASQ failed to execute IO read cmd!\n");
		return 0;
	} else {
		pr_err("ASQ execute IO read cmd success?\n");
		return -EPERM;
	}
}

static int send_io_write_to_asq(struct nvme_tool *tool)
{
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_info *nss = ndev->nss;
	struct nvme_rwc_wrapper wrap = {0};
	int ret;

	wrap.sqid = NVME_AQ_ID;
	wrap.cqid = NVME_AQ_ID;
	wrap.nsid = nss[0].nsid;
	wrap.slba = 0;
	wrap.nlb = 8;
	wrap.buf = tool->wbuf;
	wrap.size = wrap.nlb * nss[0].lbads;

	ret = nvme_io_write(ndev->fd, &wrap);
	if (ret < 0) {
		pr_debug("ASQ failed to execute IO write cmd!\n");
		return 0;
	} else {
		pr_err("ASQ execute IO write cmd success?\n");
		return -EPERM;
	}
}

int case_queue_iocmd_to_asq(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	int ret;

	ret = send_io_read_to_asq(tool);
	if (ret < 0)
		return ret;

	ret = send_io_write_to_asq(tool);
	if (ret < 0)
		return ret;

	return 0;
}
