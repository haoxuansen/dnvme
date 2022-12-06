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

#ifndef _DNVME_CMD_H_
#define _DNVME_CMD_H_

void dnvme_release_prps(struct nvme_device *nvme_device, struct nvme_prps *prps);

void dnvme_delete_cmd_list(struct nvme_device *ndev, struct nvme_sq *sq);

int dnvme_send_64b_cmd(struct nvme_context *ctx, struct nvme_64b_cmd __user *ucmd);

#endif /* !_DNVME_CMD_H_ */
