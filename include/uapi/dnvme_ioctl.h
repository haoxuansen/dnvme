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

#ifndef _DNVME_IOCTLS_H_
#define _DNVME_IOCTLS_H_

#include "bitops.h"
#include "dnvme_interface.h"

/**
 * Enumeration types which provide common interface between kernel driver and
 * user app layer ioctl functions. dnvme is using letter 'N' to designate the
 * first param to _IO macros because of "Nvme" designation. Since
 * drivers/usb/scanner.h is officially designated to use this letter, but
 * only for the 2nd param values ranging from 0x00-0x1f, dnmve will therefore
 * use higher values for the 2nd param as indicated by the value of the
 * following enum.
 */
enum {
	NVME_READ_GENERIC = 0xB0,
	NVME_WRITE_GENERIC,
	NVME_GET_CAPABILITY,
	NVME_CREATE_ADMN_SQ,
	NVME_CREATE_ADMN_CQ,
	NVME_SET_DEV_STATE,
	NVME_SEND_64B_CMD,
	NVME_TOXIC_64B_DWORD,
	NVME_GET_Q_METRICS,
	NVME_CREATE_ADMN_Q,
	NVME_PREPARE_SQ_CREATION,
	NVME_PREPARE_CQ_CREATION,
	NVME_RING_SQ_DOORBELL,
	NVME_DUMP_METRICS,
	NVME_REAP_INQUIRY,
	NVME_REAP,
	NVME_GET_DRIVER_METRICS,
	NVME_METABUF_ALLOC,
	NVME_METABUF_CREAT,
	NVME_METABUF_DEL,
	NVME_SET_IRQ,
	NVME_MASK_IRQ,
	NVME_UNMASK_IRQ,
	NVME_GET_DEVICE_METRICS,
	NVME_MARK_SYSLOG,
	// NVME_GET_BP_MEM,
	// NVME_GET_BP_MEM_ADDR,
};

enum nvme_region {
	NVME_PCI_HEADER,
	NVME_BAR0_BAR1,
};

/**
 * @brief The required access width of register or memory space.
 */
enum nvme_access_type {
	NVME_ACCESS_BYTE,
	NVME_ACCESS_WORD,
	NVME_ACCESS_DWORD,
	NVME_ACCESS_QWORD,
};

/**
 * @brief Parameters for the generic read or write.
 */
struct nvme_access {
	enum nvme_region	region;
	enum nvme_access_type	type;
	uint8_t			*buffer;
	uint32_t		bytes;
	uint32_t		offset;
};

struct nvme_capability {
	unsigned long	pci_cap_support[BITS_TO_LONGS(32)];
/* PCI Power Management Capability */
#define PCI_CAP_SUPPORT_PM		0
/* Message Signaled Interrupts Capability */
#define PCI_CAP_SUPPORT_MSI		1
#define PCI_CAP_SUPPORT_MSIX		2
#define PCI_CAP_SUPPORT_PCIE		3
	uint8_t		pci_cap_offset[32];
};

#define NVME_IOCTL_READ_GENERIC \
	_IOWR('N', NVME_READ_GENERIC, struct nvme_access)
#define NVME_IOCTL_WRITE_GENERIC \
	_IOWR('N', NVME_WRITE_GENERIC, struct nvme_access)

#define NVME_IOCTL_GET_CAPABILITY \
	_IOWR('N', NVME_GET_CAPABILITY, struct nvme_capability)

#define NVME_IOCTL_SET_DEV_STATE \
	_IOW('N', NVME_SET_DEV_STATE, enum nvme_state)

/**
 * @def NVME_IOCTL_CREATE_ADMN_SQ
 * define a unique value for creating admin submission queue.
 */
#define NVME_IOCTL_CREATE_ADMN_SQ _IOWR('N', NVME_CREATE_ADMN_SQ, \
    struct nvme_asq_gen)

/**
 * @def NVME_IOCTL_CREATE_ADMN_CQ
 * define a unique value for creating admin completion queue.
 */
#define NVME_IOCTL_CREATE_ADMN_CQ _IOWR('N', NVME_CREATE_ADMN_CQ, \
    struct nvme_acq_gen)

/**
 * @def NVME_IOCTL_GET_Q_METRICS
 * define a unique value for getting the q metrics. The metrics is either for
 * Submission queue including Admin SQ or Completion Queue including Admin CQ.
 */
#define NVME_IOCTL_GET_Q_METRICS _IOWR('N', NVME_GET_Q_METRICS, \
    struct nvme_get_q_metrics)

/**
 * @def NVME_IOCTL_CREATE_ADMN_Q
 * define a unique value for creating admin queues.
 */
#define NVME_IOCTL_CREATE_ADMN_Q _IOWR('N', NVME_CREATE_ADMN_Q, \
    struct nvme_create_admn_q)

/**
 * @def NVME_IOCTL_SEND_64B_CMD
 * For sending any 64 Byte command, supporing meta data, IRQ's, PRP user
 * payloads, etc. Simply any 64B cmd should be supported.
 */
#define NVME_IOCTL_SEND_64B_CMD _IOWR('N', NVME_SEND_64B_CMD, \
    struct nvme_64b_send)

/**
 * @def NVME_IOCTL_TOXIC_64B_CMD
 * After Utilizing NVME_IOCTL_SEND_64B_CMD to issue a cmd into any SQ, but
 * before utilizing NVME_IOCTL_RING_SQ_DOORBELL on that same cmd, one is allowed
 * issue this IOCTL. This IOCTL will effectively bypass all safety checking,
 * those things which prevent an erroneous IOCTL from bringing down the kernel,
 * to inject bad/illegal data bits into a cmd. This is HIGHLY VOLATILE if you
 * are NOT intimately aware of the logic of this driver. Assumptions are made,
 * albeit a minimal set of, which require proper setup of all cmds when they
 * are send via NVME_IOCTL_SEND_64B_CMD and subsequently reaped via
 * NVME_IOCTL_REAP, and thus bypassing these assumptions is what will crash
 * the kernel. However, if you thoroughly understand these assumption, this
 * IOCTL will allow to modify the cmd bits after safety checking is performed.
 */
#define NVME_IOCTL_TOXIC_64B_DWORD _IOWR('N', NVME_TOXIC_64B_DWORD, \
    struct backdoor_inject)

/**
 * @def NVME_IOCTL_PREPARE_SQ_CREATION
 * define a unique value for allocating contiguous memory for SQ.
 */
#define NVME_IOCTL_PREPARE_SQ_CREATION _IOWR('N', NVME_PREPARE_SQ_CREATION, \
    struct nvme_prep_sq)

/**
 * @def NVME_IOCTL_PREPARE_CQ_CREATION
 * define a unique value for allocating contiguous memory for CQ.
 */
#define NVME_IOCTL_PREPARE_CQ_CREATION _IOWR('N', NVME_PREPARE_CQ_CREATION, \
    struct nvme_prep_sq)

/**
 * @def NVME_IOCTL_RING_SQ_DOORBELL
 * define a unique value to ring SQ doorbell.
 */
#define NVME_IOCTL_RING_SQ_DOORBELL _IOWR('N', NVME_RING_SQ_DOORBELL, uint16_t)

/**
 * @def NVME_IOCTL_DUMP_METRICS
 * define a unique value to Dump Q metrics.
 */
#define NVME_IOCTL_DUMP_METRICS _IOWR('N', NVME_DUMP_METRICS, struct nvme_file)

/**
 * @def NVME_IOCTL_REAP_INQUIRY
 * define a unique value to reap inquiry ioctl.
 */
#define NVME_IOCTL_REAP_INQUIRY _IOWR('N', NVME_REAP_INQUIRY, \
    struct nvme_reap_inquiry)

/**
 * @def NVME_IOCTL_REAP
 * define a unique value to reap ioctl.
 */
#define NVME_IOCTL_REAP _IOWR('N', NVME_REAP, struct nvme_reap)

/**
 * @def NVME_IOCTL_GET_DRIVER_METRICS
 * define a unique value to return driver metrics ioctl.
 */
#define NVME_IOCTL_GET_DRIVER_METRICS _IOWR('N', NVME_GET_DRIVER_METRICS, \
    struct nvme_driver)

/**
 * @def NVME_IOCTL_METABUF_ALLOC
 * define a unique value to meta buffer allocation. The third parameter give the
 * size of data and type of data passed to this ioctl from user to kernel.
 */
#define NVME_IOCTL_METABUF_ALLOC _IOWR('N', NVME_METABUF_ALLOC, uint32_t)

/**
 * @def NVME_IOCTL_METABUF_CREATE
 * define a unique value to meta buffer creation.
 */
#define NVME_IOCTL_METABUF_CREATE _IOWR('N', NVME_METABUF_CREAT, uint16_t)

/**
 * @def NVME_IOCTL_METABUF_DELETE
 * define a unique value meta buffer deletion.
 */
#define NVME_IOCTL_METABUF_DELETE _IOWR('N', NVME_METABUF_DEL, uint32_t)

/**
 * @def NVME_IOCTL_SET_IRQ
 * define a unique value for IRQ setting scheme.
 */
#define NVME_IOCTL_SET_IRQ _IOWR('N', NVME_SET_IRQ, struct interrupts)

#define NVME_IOCTL_MASK_IRQ _IOWR('N', NVME_MASK_IRQ, uint16_t)

#define NVME_IOCTL_UNMASK_IRQ _IOWR('N', NVME_UNMASK_IRQ, uint16_t)

/**
 * @def NVME_IOCTL_GET_DEVICE_METRICS
 * define a unique value for returning the device metrics.
 */
#define NVME_IOCTL_GET_DEVICE_METRICS _IOWR('N', NVME_GET_DEVICE_METRICS, \
    struct nvme_dev_public)

/**
 * @def NVME_IOCTL_MARK_SYSLOG
 * Write a unique string, i.e. mark, into the system log to mark a point in time
 */
#define NVME_IOCTL_MARK_SYSLOG _IOW('N', NVME_MARK_SYSLOG, struct nvme_logstr)

/**
 * @def NVME_GET_BP_MEM
 * boot part kernel memory alloc
 */
//#define NVME_IOCTL_WRITE_BP_BUF _IOW('N', NVME_GET_BP_MEM, struct nvme_write_bp_buf)

/**
 * @def NVME_GET_BP_MEM_ADDR
 * boot part kernel memory alloc
 */
//#define NVME_IOCTL_GET_BP_MEM_ADDR _IOW('N', NVME_GET_BP_MEM_ADDR, struct nvme_write_bp_buf)

#endif
