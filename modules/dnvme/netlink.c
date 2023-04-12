/**
 * @file netlink.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-04-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/limits.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <net/genetlink.h>
#include <net/sock.h>

#include "dnvme.h"
#include "core.h"
#include "queue.h"

static struct genl_family nvme_gnl_family;

static int dnvme_gnl_cmd_reap_cqe(struct sk_buff *skb, struct genl_info *info)
{
	struct nvme_device *ndev;
	struct pci_dev *pdev;
	struct nvme_cq *cq;
	struct sk_buff *msg;
	void *hdr;
	u16 cqid;
	u32 expect, actual;
	unsigned long buf_ptr;
	u32 buf_size;
	int timeout = 0; /* default */
	int instance;
	int status = 0;

	if (!info->attrs[NVME_GNL_ATTR_DEVNO] || 
			!info->attrs[NVME_GNL_ATTR_CQID] ||
			!info->attrs[NVME_GNL_ATTR_OPT_NUM] ||
			!info->attrs[NVME_GNL_ATTR_OPT_BUF_PTR] ||
			!info->attrs[NVME_GNL_ATTR_OPT_BUF_SIZE]) {
		pr_err("some attr not exist!\n");
		status = -EINVAL;
		goto out_response;
	}
	instance = nla_get_s32(info->attrs[NVME_GNL_ATTR_DEVNO]);
	cqid = nla_get_u16(info->attrs[NVME_GNL_ATTR_CQID]);
	expect = nla_get_u32(info->attrs[NVME_GNL_ATTR_OPT_NUM]);
	buf_ptr = (unsigned long)nla_get_u64(info->attrs[NVME_GNL_ATTR_OPT_BUF_PTR]);
	buf_size = nla_get_u32(info->attrs[NVME_GNL_ATTR_OPT_BUF_SIZE]);
	pr_debug("devno:%d, cqid:%u, expect:%u, ptr:0x%lx, size:0x%x\n", 
		instance, cqid, expect, buf_ptr, buf_size);

	if (info->attrs[NVME_GNL_ATTR_TIMEOUT]) {
		timeout = nla_get_s32(info->attrs[NVME_GNL_ATTR_TIMEOUT]);
		if (timeout < 0)
			timeout = S32_MAX;
	}

	ndev = dnvme_lock_device(instance);
	if (IS_ERR(ndev)) {
		status = PTR_ERR(ndev);
		goto out_response;
	}
	pdev = ndev->pdev;

	cq = dnvme_find_cq(ndev, cqid);
	if (!cq) {
		dnvme_err(ndev, "failed to find CQ(%u)!\n", cqid);
		status = -ENOENT;
		goto out_unlock;
	}

	do {
		actual = dnvme_get_cqe_remain(cq, &pdev->dev);
		if (actual >= expect)
			break;

		msleep(1);
		timeout--;
	} while (timeout > 0);

	if (actual >= expect) {
		status = dnvme_reap_cqe(cq, expect, (void *)buf_ptr, buf_size);
		if (status > 0) {
			actual = status;
			status = 0;
		}
	} else {
		status = -ETIMEDOUT;
	}

out_unlock:
	dnvme_unlock_device(ndev);

out_response:
	msg = genlmsg_new(GENLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg) {
		dnvme_err(ndev, "failed to alloc gnelmsg!\n");
		return -ENOMEM;
	}

	hdr = genlmsg_put(msg, info->snd_portid, info->snd_seq + 1,
		&nvme_gnl_family, 0, NVME_GNL_CMD_REAP_CQE);
	if (!hdr) {
		nlmsg_free(msg);
		return -EMSGSIZE;
	}
	nla_put_s32(msg, NVME_GNL_ATTR_OPT_STATUS, status);
	nla_put_u32(msg, NVME_GNL_ATTR_OPT_NUM, actual);

	genlmsg_end(msg, hdr);

	return genlmsg_unicast(genl_info_net(info), msg, info->snd_portid);
}

static const struct nla_policy nvme_gnl_policy[] = {
	[NVME_GNL_ATTR_UNSPEC]		= { .type = NLA_UNSPEC },
	[NVME_GNL_ATTR_TIMEOUT]		= { .type = NLA_S32 },
	[NVME_GNL_ATTR_DEVNO]		= { .type = NLA_S32 },
	[NVME_GNL_ATTR_SQID]		= { .type = NLA_U16 },
	[NVME_GNL_ATTR_CQID]		= { .type = NLA_U16 },
	[NVME_GNL_ATTR_OPT_NUM]		= { .type = NLA_U32 },
	[NVME_GNL_ATTR_OPT_DATA]	= { .type = NLA_BINARY },
	[NVME_GNL_ATTR_OPT_BUF_PTR]	= { .type = NLA_U64 },
	[NVME_GNL_ATTR_OPT_BUF_SIZE]	= { .type = NLA_U32 },
	[NVME_GNL_ATTR_OPT_STATUS]	= { .type = NLA_S32 },
};

static const struct genl_ops nvme_gnl_ops[] = {
	{
		.cmd	= NVME_GNL_CMD_REAP_CQE,
		.doit	= dnvme_gnl_cmd_reap_cqe,
		.flags	= GENL_CMD_CAP_DO,
	}
};

static struct genl_family nvme_gnl_family = {
	.name		= "nvme",
	.version	= 1,
	.ops		= nvme_gnl_ops,
	.n_ops		= ARRAY_SIZE(nvme_gnl_ops),
	.policy		= nvme_gnl_policy,
	.maxattr	= NVME_GNL_ATTR_MAX,
	.module		= THIS_MODULE,
	.netnsok	= true,
};

int dnvme_gnl_init(void)
{
	int ret;

	ret = genl_register_family(&nvme_gnl_family);
	if (ret < 0) {
		pr_err("failed to register generic netlink family!(%d)\n", ret);
		return ret;
	}
	pr_debug("family ID: %d\n", nvme_gnl_family.id);
	return nvme_gnl_family.id;
}

void dnvme_gnl_exit(void)
{
	genl_unregister_family(&nvme_gnl_family);
}

