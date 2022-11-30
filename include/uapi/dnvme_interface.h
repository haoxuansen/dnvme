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

#ifndef _DNVME_INTERFACE_H_
#define _DNVME_INTERFACE_H_

#include "nvme.h"

/**
 * This structure defines the parameters required for creating any CQ.
 * It supports both Admin CQ and IO CQ.
 */
struct nvme_cq_public {
	uint16_t	q_id; /* even admin q's are supported here q_id = 0 */
	uint16_t	tail_ptr; /* The value calculated for respective tail_ptr */
	uint16_t	head_ptr; /* Actual value in CQxTDBL for this q_id */
	uint32_t	elements; /* pass the actual elements in this q */
	uint8_t		irq_enabled; /* sets when the irq scheme is active */
	uint16_t	irq_no; /* idx in list; always 0 based */
	uint8_t		pbit_new_entry; /* Indicates if a new entry is in CQ */
	uint8_t		cqes;
};

/**
 * @sqes: SQ entry size, in bytes and specified as a power of two (2^n)
 */
struct nvme_sq_public {
	uint16_t	sq_id; /* Admin SQ are supported with q_id = 0 */
	uint16_t	cq_id; /* The CQ ID to which this SQ is associated */
	uint16_t	tail_ptr; /* Actual value in SQxTDBL for this SQ id */
	/* future SQxTDBL write value based on no. of new cmds copied to SQ */
	uint16_t	tail_ptr_virt; 
	uint16_t	head_ptr; /* Calculate this value based on cmds reaped */
	uint32_t	elements; /* total number of elements in this Q */
	uint8_t		sqes;
};

/**
 * Interface structure for reap ioctl. Admin Q and all IO Q's are supported.
 */
struct nvme_reap {
	uint16_t q_id;          /* CQ ID to reap commands for */
	uint32_t elements;      /* Get the no. of elements to be reaped */
	uint32_t num_remaining; /* return no. of cmds waiting for this cq */
	uint32_t num_reaped;    /* Return no. of elements reaped */
	uint8_t  *buffer;       /* Buffer to copy reaped data */
	/* no of times isr was fired which is associated with cq reaped on */
	uint32_t isr_count;
	uint32_t size;          /* Size of buffer to fill data to */
};

/**
 * Format of general purpose nvme command DW0-DW9
 */
struct nvme_gen_cmd {
	uint8_t  opcode;
	uint8_t  flags;
	uint16_t command_id;
	uint32_t nsid;
	uint64_t rsvd2;
	uint64_t metadata;
	// uint64_t prp1;
	// uint64_t prp2;
	union nvme_data_ptr dptr;
};

// /**
//  * Specific structure for Create CQ command
//  */
// struct nvme_create_cq {
//     uint8_t  opcode;
//     uint8_t  flags;
//     uint16_t command_id;
//     uint32_t rsvd1[5];
//     uint64_t prp1;
//     uint64_t rsvd8;
//     uint16_t cqid;
//     uint16_t qsize;
//     uint16_t cq_flags;
//     uint16_t irq_no;
//     uint32_t rsvd12[4];
// };

// /**
//  * Specific structure for Create SQ command
//  */
// struct nvme_create_sq {
//     uint8_t  opcode;
//     uint8_t  flags;
//     uint16_t command_id;
//     uint32_t rsvd1[5];
//     uint64_t prp1;
//     uint64_t rsvd8;
//     uint16_t sqid;
//     uint16_t qsize;
//     uint16_t sq_flags;
//     uint16_t cqid;
//     uint32_t rsvd12[4];
// };

/**
 * Specific structure for Delete Q command
 */
struct nvme_del_q {
    uint8_t  opcode;
    uint8_t  flags;
    uint16_t command_id;
    uint32_t rsvd1[9];
    uint16_t qid;
    uint16_t rsvd10;
    uint32_t rsvd11[5];
};

/**
 * Describes bits/bytes within an existing SQ indicating a new value for any
 * cmd dword. This is only allowed for those cmds for which the doorbell hasn't
 * already rung.
 */
struct backdoor_inject {
    uint16_t q_id;        /* SQ ID where the cmd is residing */
    uint16_t cmd_ptr;     /* [0 -> (CreateIOSQ.DW10.SIZE-1)] which cmd in SQ? */
    uint8_t  dword;       /* [0 -> (CC.IOSQES-1)] which DWORD in the cmd */
    uint32_t value_mask;  /* Bitmask indicates which 'value' bits to use */
    uint32_t value;       /* Extract spec'd bits; overwrite those exact bits */
};

// struct nvme_write_bp_buf
// {
//     enum nvme_region type;
//     uint32_t offset;
//     uint32_t bytes;
//     enum nvme_access_type acc_type;

//     /* Data buffer user space address */
//     uint8_t *bp_buf;
//     uint32_t bp_buf_size; /* Size of Data Buffer */
// };

#endif
