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

#ifndef _SYSFUNCPROTO_H_
#define _SYSFUNCPROTO_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>

#include "dnvme_ds.h"
#include "dnvme_reg.h"
#include "core.h"
#include "dnvme_ioctl.h"

int dnvme_reset_subsystem(struct nvme_context *ctx);
int dnvme_set_ctrl_state(struct nvme_context *ctx, bool enabled);

int dnvme_generic_read(struct nvme_context *ctx, struct nvme_access __user *uaccess);
int dnvme_generic_write(struct nvme_context *ctx, struct nvme_access __user *uaccess);

int dnvme_get_capability(struct nvme_context *ctx, struct nvme_capability __user *ucap);
int dnvme_get_queue(struct nvme_context *ctx, struct nvme_get_queue __user *uq);

int dnvme_create_admin_queue(struct nvme_context *ctx, 
	struct nvme_admin_queue __user *uaq);

int dnvme_prepare_sq(struct nvme_context *ctx, struct nvme_prep_sq __user *uprep);
int dnvme_prepare_cq(struct nvme_context *ctx, struct nvme_prep_cq __user *uprep);

int dnvme_send_64b_cmd(struct nvme_context *ctx, struct nvme_64b_cmd __user *ucmd);

/**
 * driver_toxic_dword - Please refer to the header file comment for
 * NVME_IOCTL_TOXIC_64B_CMD.
 * @param pmetrics_device
 * @param err_inject Pass ptr to the user space buffer describing the error
 *        to inject.
 * @return Error Codes
 */
int driver_toxic_dword(struct nvme_context *pmetrics_device,
    struct backdoor_inject *err_inject);

/**
 * driver_log - Driver routine to log data into file from metrics
 * @param n_file
 * @return allocation of contig mem 0 or -1.
 */
int driver_log(struct nvme_file *n_file);

/**
 * driver_logstr - Driver routine to log a custom string to the system log
 * @param logStr
 * @return 0 or -1.
 */
int driver_logstr(struct nvme_logstr *logStr);

/**
 * deallocate_all_queues - This function will start freeing up the memory for
 * the queues (SQ and CQ) allocated during the prepare queues. This function
 * takes a parameter, NVME_ST_DISABLE or NVME_ST_DISABLE_COMPLETE, which identifies if
 * you need to clear Admin or not. Also while driver exit call this function
 * with NVME_ST_DISABLE_COMPLETE.
 * @param pmetrics_device
 * @param nstate
 */
void deallocate_all_queues(struct nvme_context *pmetrics_device,
    enum nvme_state nstate);

/**
 * driver_reap_inquiry - This function will traverse the metrics device list
 * for the given cq_id and return the number of commands that are to be reaped.
 * This is only function apart from initializations, that will modify tail_ptr
 * for the corresponding CQ.
 * @param pmetrics_device
 * @param usr_reap_inq
 * @return success or failure based on reap_inquiry
 */
int driver_reap_inquiry(struct nvme_context *pmetrics_device,
    struct nvme_reap_inquiry *usr_reap_inq);

/**
 * dnvme_device_open - This operation is always the first operation performed
 * on the device file.
 * @param inode
 * @param filp
 * @return success or failure based on device open
 */
int dnvme_device_open(struct inode *inode, struct file *filp);

/**
 * dnvme_device_release - This operation is invoked when the file structure
 * is being released.
 * @param inode
 * @param filp
 * @return success or failure based on device clean up.
 */
int dnvme_device_release(struct inode *inode, struct file *filp);

/**
 * dnvme_device_mmap - This mmap will do the linear mapping to device memory
 * into user space.
 * The parameter vma holds all the required mapping and return the caller with
 * virtual address.
 * @param filp
 * @param vma
 * @return success or failure depending on mapping.
 */
int dnvme_device_mmap(struct file *filp, struct vm_area_struct *vma);

/**
 * driver_reap_cq - Reap the number of elements specified for the given CQ id.
 * Return the CQ entry data in the buffer specified.
 * @param pmetrics_device
 * @param usr_reap_data
 * @return Success of Failure based on Reap Success or failure.
 */
int driver_reap_cq(struct nvme_context *pmetrics_device,
    struct nvme_reap *usr_reap_data);

/**
 * Create a dma pool for the requested size. Initialize the DMA pool pointer
 * with DWORD alignment and associate it with the active device.
 * @param pmetrics_device
 * @param alloc_size
 * @return 0 or -1 based on dma pool creation.
 */
int metabuff_create(struct nvme_context *pmetrics_device,
    u32 alloc_size);

/**
 * Create a meta buffer node when user request and allocate a consistent
 * dma memory from the meta dma pool. Add this node into the meta data
 * linked list.
 * @param pmetrics_device
 * @param id
 * @return Success of Failure based on dma alloc Success or failure.
 */
int metabuff_alloc(struct nvme_context *pmetrics_device,
    u32 id);

/**
 * Delete a meta buffer node when user requests and deallocate a consistent
 * dma memory. Delete this node from the meta data linked list.
 * @param pmetrics_device
 * @param id
 * @return Success of Failure based on metabuff delete
 */
int metabuff_del(struct nvme_context *pmetrics_device,
    u32 id);

/*
 * deallocate_mb will free up the memory and nodes for the meta buffers
 * that were allocated during the alloc and create meta. Finally
 * destroys the dma pool and free up the metrics meta node.
 * @param pmetrics_device
 */
void deallocate_mb(struct nvme_context *pmetrics_device);

int check_cntlr_cap(struct pci_dev *pdev, enum nvme_irq_type cap_type,
    u16 *offset);

// int driver_nvme_write_bp_buf(struct nvme_write_bp_buf *nvme_data, 
//     struct nvme_context *pmetrics_device);


#endif
