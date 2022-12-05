/*
 * NVM Express Compliance Suite
 * Copyright (c) 2011, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _DNVME_CMDS_H_
#define _DNVME_CMDS_H_

/* Enum specifying Writes/Reads to mapped pages and other general enums */
enum {
    PERSIST_QID_0 = 0, /* Default value of Persist queue ID */
    CDW11_PC = 1, /* Mask for checking CDW11.PC of create IO Q cmds */
    CDW11_IEN = 2, /* Mask to check if CDW11.IEN is set */
};

enum {
    PRP_PRESENT = 1, /* Specifies to generate PRP's for a particular command */
    PRP_ABSENT = 0   /* Specifies not to generate PRP's per command */
};

/* Enum specifying PRP1,PRP2 or List */
enum prp_type {
	NO_PRP = 0,
	PRP1 = (1 << 0),
	PRP2 = (1 << 1),
	PRP_List = (1 << 2),
};

/* Enum specifying type of data buffer */
enum data_buf_type {
	DATA_BUF,
	NVME_IO_QUEUE_CONTIG,
	NVME_IO_QUEUE_DISCONTIG
};

void dnvme_release_prps(struct nvme_device *nvme_device, struct nvme_prps *prps);

void dnvme_delete_cmd_list(struct nvme_device *ndev, struct nvme_sq *sq);

#endif
