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
    u32 cmd_specifc;       /* DW 0 all 32 bits     */
    u32 reserved;          /* DW 1 all 32 bits     */
    u16 sq_head_ptr;       /* DW 2 lower 16 bits   */
    u16 sq_identifier;     /* DW 2 higher 16 bits  */
    u16 cmd_identifier;    /* Cmd identifier       */
    u8  phase_bit:1;       /* Phase bit            */
    u16 status_field:15;   /* Status field         */
};

/**
 * nvme_ctrl_enable - NVME controller enable function.This will set the CAP.EN
 * flag and this function which call the timer handler and check for the timer
 * expiration. It returns success if the ctrl in rdy before timeout.
 * @param  pmetrics_device
 * @return 0 or -1
 */
//int nvme_ctrl_enable(struct  nvme_context *pmetrics_device);

/*
 * iol_nvme_ctrl_enable - NVME controller enable function. This will set the
 * CAP.EN flag and this function which call the timer handler and check for the
 * timer expiration. It returns success if the ctrl in rdy before timeout.
 *
 * Modified from nvme_ctrl_enable to call different wait function to conform
 * to test plan.
 * @param  pmetrics_device
 * @return 0 or -1
 */
//int iol_nvme_ctrl_enable(struct nvme_context *pmetrics_device);

/**
 * nvme_ctrl_disable - NVME controller disable function.This will reset the
 * CAP.EN flag and this function which call the timer handler and check for
 * the timer expiration. It returns success if the ctrl in rdy before timeout.
 * @param pmetrics_device
 * @return 0 or -1
 */
//int nvme_ctrl_disable(struct  nvme_context *pmetrics_device);

/**
 * device_cleanup - Will clean up all the existing data structs used by driver
 * @param pmetrics_device
 * @param new_state
 */
void device_cleanup(struct  nvme_context *pmetrics_device,
    enum nvme_state new_state);

/**
 * nvme_prepare_sq - NVME controller prepare sq function. This will check
 * if q is allocated and then create a memory for the IO SQ.
 * @param pmetrics_sq_list
 * @param pnvme_dev
 * @return 0 or -1
 */
int nvme_prepare_sq(struct  nvme_sq  *pmetrics_sq_list,
            struct nvme_device *pnvme_dev);

/**
 * nvme_prepare_cq - NVME controller prepare cq function. This will check
 * if q is allocated and then create a memory for the IO SQ.
 * @param pmetrics_cq_list
 * @param pnvme_dev
 * @return 0 or -1
 */
int nvme_prepare_cq(struct  nvme_cq  *pmetrics_cq_list,
            struct nvme_device *pnvme_dev);

/**
 * nvme_ring_sqx_dbl - NVME controller function to ring the appropriate
 * SQ doorbell.
 * @param ring_sqx
 * @param pmetrics_device
 * @return 0 or -1
 */
int nvme_ring_sqx_dbl(u16 ring_sqx, struct  nvme_context
        *pmetrics_device);


struct nvme_sq *dnvme_find_sq(struct nvme_context *ctx, u16 id);
struct nvme_cq *dnvme_find_cq(struct nvme_context *ctx, u16 id);

int dnvme_check_qid_unique(struct nvme_context *ctx, 
	enum nvme_queue_type type, u16 id);

struct nvme_cmd *dnvme_find_cmd(struct nvme_sq *sq, u16 id);
struct nvme_meta *dnvme_find_meta(struct nvme_context *ctx, u32 id);

int dnvme_create_asq(struct nvme_context *ctx, u32 elements);
int dnvme_create_acq(struct nvme_context *ctx, u32 elements);

/**
 *  reap_inquiry - This generic function will try to inquire the number of
 *  CE entries in the Completion Queue that are waiting to be reaped for any
 *  given q_id.
 *  @param pmetrics_cq_node
 *  @param dev
 *  @return number of CE's remaining
  */
u32 reap_inquiry(struct nvme_cq  *pmetrics_cq_node, struct device *dev);

#endif
