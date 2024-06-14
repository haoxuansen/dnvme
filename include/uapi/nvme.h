/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Definitions for the NVM Express interface
 * Copyright (c) 2011-2014, Intel Corporation.
 */

#ifndef _LINUX_NVME_H
#define _LINUX_NVME_H

#include <linux/types.h>
#include <linux/uuid.h>

#include "compiler.h"
#include "nvme/property.h"

/* NQN names in commands fields specified one size */
#define NVMF_NQN_FIELD_LEN	256

/* However the max length of a qualified name is another size */
#define NVMF_NQN_SIZE		223

#define NVME_DISC_SUBSYS_NAME	"nqn.2014-08.org.nvmexpress.discovery"

#define NVME_RDMA_IP_PORT	4420

#define NVME_NSID_ALL		0xffffffff

enum nvme_subsys_type {
	NVME_NQN_DISC	= 1,		/* Discovery type target subsystem */
	NVME_NQN_NVME	= 2,		/* NVME type target subsystem */
};


#define NVME_AQ_ID		0
#define NVME_AQ_DEPTH		32
#define NVME_NR_AEN_COMMANDS	1
#define NVME_AQ_BLK_MQ_DEPTH	(NVME_AQ_DEPTH - NVME_NR_AEN_COMMANDS)

/*
 * Subtract one to leave an empty queue entry for 'Full Queue' condition. See
 * NVM-Express 1.2 specification, section 4.1.2.
 */
#define NVME_AQ_MQ_TAG_DEPTH	(NVME_AQ_BLK_MQ_DEPTH - 1)


/*
 * Submission and Completion Queue Entry Sizes for the NVM command set.
 * (In bytes and specified as a power of two (2^n)).
 */
#define NVME_ADM_SQES		6
#define NVME_ADM_CQES		4
#define NVME_NVM_IOSQES		6
#define NVME_NVM_IOCQES		4

#include "nvme/command.h"

static inline int nvme_is_fabrics(struct nvme_command *cmd)
{
	return cmd->common.opcode == nvme_admin_fabrics;
}

static inline int nvme_is_write(struct nvme_command *cmd)
{
	/*
	 * What a mess...
	 *
	 * Why can't we simply have a Fabrics In and Fabrics out command?
	 */
	if (unlikely(nvme_is_fabrics(cmd)))
		return cmd->fabrics.fctype & 1;
	return cmd->common.opcode & 1;
}

#endif /* _LINUX_NVME_H */
