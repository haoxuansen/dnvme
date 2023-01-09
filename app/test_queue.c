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
#include "test_metrics.h"
#include "test_queue.h"

int case_queue_iocmd_to_asq(void)
{
	struct nvme_dev_info *ndev = &g_nvme_dev;
	struct nvme_ns_info *nss = ndev->nss;
	struct nvme_rwc_wrapper wrap = {0};
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	wrap.sqid = NVME_AQ_ID;
	wrap.cqid = NVME_AQ_ID;
	wrap.nsid = nss[0].nsid;
	wrap.slba = 0;
	wrap.nlb = 8;
	wrap.buf = g_read_buf;
	wrap.size = wrap.nlb * nss[0].lbads;

	ret = nvme_cmd_io_read(ndev->fd, &wrap);
	if (ret < 0) {
		pr_err("failed to submit io read cmd!(%d)\n", ret);
		return ret;
	}
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, wrap.sqid);
	if (ret < 0)
		return ret;

	ret = nvme_reap_expect_cqe(ndev->fd, wrap.cqid, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	if (le16_to_cpu(entry.sq_id) != wrap.sqid) {
		pr_err("SQ:%u vs %u mismatch!\n", wrap.sqid, 
			le16_to_cpu(entry.sq_id));
		return -EPERM;
	}

	if (entry.command_id != cid) {
		pr_err("CID:%u vs %u mismatch!\n", cid, entry.command_id);
		return -EPERM;
	}

	if (NVME_CQE_STATUS_TO_STATE(entry.status) == 0) {
		pr_err("ASQ execute IO CMD success?\n");
		return -EPERM;
	}
	
	pr_info("ASQ failed to execute IO CMD! status:0x%x\n",
		NVME_CQE_STATUS_TO_STATE(entry.status));
	return 0;
}
