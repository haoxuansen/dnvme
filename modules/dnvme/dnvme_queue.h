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

#ifndef _DNVME_QUEUE_H_
#define _DNVME_QUEUE_H_

#include "dnvme_reg.h"
#include "dnvme_ds.h"
#include "sysfuncproto.h"

/* Admin SQ tail Door bell offset */
#define NVME_SQ0TBDL    0x1000

/* Admin SQ size Mask bits 0-11 in AQA */
#define ASQS_MASK       0xFFF

/* Admin Completion Q Mask Bits 16-21 in ADA */
#define ACQS_MASK       0x0FFF0000

/* As Time Out is in lower 32 bits of 64 bit CAP */
#define NVME_TO_SHIFT_MASK 24

/* CAP.TO field units */
#define CAP_TO_UNIT 500

/*
 * Maximum AQ entries allowed.
 */
#define MAX_AQ_ENTRIES   4096

#define NVME_CSTS_SHUTDOWN_BIT_MASK		0x0000000C
#define NVME_CSTS_CFS_BIT_MASK			0x00000002
#define NVME_CSTS_RDY_BIT_MASK			0x00000001

/*
 * completion q entry structure.
 */
struct cq_completion {
	u32	cmd_specifc;       /* DW 0 all 32 bits     */
	u32	reserved;          /* DW 1 all 32 bits     */
	u16	sq_head_ptr;       /* DW 2 lower 16 bits   */
	u16	sq_identifier;     /* DW 2 higher 16 bits  */
	u16	cmd_identifier;    /* Cmd identifier       */
	u8	phase_bit:1;       /* Phase bit            */
	u16	status_field:15;   /* Status field         */
};

/**
 * @breif Check whether the SQ is full
 *
 * @return 1 if SQ is full, otherwise return 0.
 */
static inline int dnvme_sq_is_full(struct nvme_sq *sq)
{
	return (((u32)sq->pub.tail_ptr_virt + 1) % sq->pub.elements) ==
		sq->pub.head_ptr ? 1 : 0;
}

/**
 * @breif Check whether the SQ is empty
 *
 * @return 1 if SQ is empty, otherwise return 0.
 */
static inline int dnvme_sq_is_empty(struct nvme_sq *sq)
{
	return sq->pub.tail_ptr_virt == sq->pub.head_ptr ? 1 : 0;
}

struct nvme_sq *dnvme_find_sq(struct nvme_context *ctx, u16 id);
struct nvme_cq *dnvme_find_cq(struct nvme_context *ctx, u16 id);

int dnvme_check_qid_unique(struct nvme_context *ctx, 
	enum nvme_queue_type type, u16 id);

struct nvme_cmd *dnvme_find_cmd(struct nvme_sq *sq, u16 id);
struct nvme_meta *dnvme_find_meta(struct nvme_context *ctx, u32 id);

struct nvme_sq *dnvme_alloc_sq(struct nvme_context *ctx, 
	struct nvme_prep_sq *prep, u8 sqes);
void dnvme_release_sq(struct nvme_context *ctx, struct nvme_sq *sq);

struct nvme_cq *dnvme_alloc_cq(struct nvme_context *ctx, 
	struct nvme_prep_cq *prep, u8 cqes);
void dnvme_release_cq(struct nvme_context *ctx, struct nvme_cq *sq);

int dnvme_create_asq(struct nvme_context *ctx, u32 elements);
int dnvme_create_acq(struct nvme_context *ctx, u32 elements);

void dnvme_delete_all_queues(struct nvme_context *ctx, enum nvme_state state);

int dnvme_ring_sq_doorbell(struct nvme_context *ctx, u16 sq_id);

u32 dnvme_get_cqe_remain(struct nvme_cq *cq, struct device *dev);
int dnvme_inquiry_cqe(struct nvme_context *ctx, struct nvme_inquiry __user *uinq);
int dnvme_reap_cqe(struct nvme_context *ctx, struct nvme_reap __user *ureap);

#endif
