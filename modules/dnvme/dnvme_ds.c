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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <asm/segment.h>
#include <linux/version.h>

#include "definitions.h"
#include "core.h"
#include "dnvme_ioctl.h"
#include "sysfuncproto.h"
#include "dnvme_queue.h"

#define IDNT_L1             "\n\t"
#define IDNT_L2             "\n\t\t"
#define IDNT_L3             "\n\t\t\t"
#define IDNT_L4             "\n\t\t\t\t"
#define IDNT_L5             "\n\t\t\t\t\t"
#define IDNT_L6             "\n\t\t\t\t\t\t"

#define SIZE_OF_WORK        256

/* local static functions */
static loff_t meta_nodes_log(struct file *file, loff_t pos,
    struct  nvme_context *pmetrics_device);
static loff_t irq_nodes_log(struct file *file, loff_t pos,
    struct  nvme_context *pmetrics_device_elem);


    int driver_logstr(struct nvme_logstr *logStr)
{
    u8 *fmtText = NULL;
    int err = 0;
    struct nvme_logstr *user_data = NULL;


    /* Allocating memory for user struct in kernel space */
    user_data = kmalloc(sizeof(struct nvme_logstr), GFP_KERNEL);
    if (user_data == NULL) {
        dnvme_err("Unable to alloc kernel memory to copy user data");
        err = -ENOMEM;
        goto fail_out;
    }
    if (copy_from_user(user_data, logStr, sizeof(struct nvme_logstr))) {
        dnvme_err("Unable to copy from user space");
        err = -EFAULT;
        goto fail_out;
    }

    /* Allocating memory for the data in kernel space, add 1 for a NULL term */
    fmtText = kmalloc(user_data->slen+1, (GFP_KERNEL | __GFP_ZERO));
    if (NULL == fmtText) {
        dnvme_err("Unable to allocate kernel memory");
        err = -ENOMEM;
        goto fail_out;
    }

    /* Copy userspace buffer to kernel memory */
    if (copy_from_user(fmtText, user_data->log_str, user_data->slen)) {
        dnvme_err("Unable to copy from user space");
        err = -EFAULT;
        goto fail_out;
    }

    /* If the user didn't provide a NULL term, we will to avoid problems */
    fmtText[user_data->slen] = '\0';
    dnvme_info("%s", fmtText);
    /* Fall through to label in intended */

fail_out:
    if (fmtText == NULL) {
        kfree(fmtText);
    }
    if (user_data != NULL) {
        kfree(user_data);
    }
    return err;
}


int driver_log(struct nvme_file *n_file)
{
    struct file *file;  /* File pointer where the data is written */
    loff_t pos = 0;     /* offset into the file */
    int dev = 0;        /* local var for tracking no. of devices */
    int i = 0;          /* local var to track no. of SQ's and CQ's */
    int cmd = 0;        /* Local variable to track no. of cmds */
    mm_segment_t oldfs; /* Old file segment to map between Kernel and usp */
    struct  nvme_sq  *pmetrics_sq_list;        /* SQ linked list */
    u8 work[SIZE_OF_WORK];
    struct  nvme_cq  *pmetrics_cq_list;        /* CQ linked list */
    struct  nvme_context *pmetrics_device; /* Metrics device list */
    struct  nvme_cmd_node  *pcmd_track_list;          /* cmd track linked list */
    u8 *filename = NULL;
    int err = 0;
    struct nvme_file *user_data = NULL;


    /* Allocating memory for user struct in kernel space */
    user_data = kmalloc(sizeof(struct nvme_file), GFP_KERNEL);
    if (user_data == NULL) {
        dnvme_err("Unable to alloc kernel memory to copy user data");
        err = -ENOMEM;
        goto fail_out;
    }
    if (copy_from_user(user_data, n_file, sizeof(struct nvme_file))) {
        dnvme_err("Unable to copy from user space");
        err = -EFAULT;
        goto fail_out;
    }

    /* Allocating memory for the data in kernel space, add 1 for a NULL term */
    filename = kmalloc(user_data->flen+1, (GFP_KERNEL | __GFP_ZERO));
    if (NULL == filename) {
        dnvme_err("Unable to allocate kernel memory");
        err = -ENOMEM;
        goto fail_out;
    }

    /* Copy userspace buffer to kernel memory */
    if (copy_from_user(filename, user_data->file_name, user_data->flen)) {
        dnvme_err("Unable to copy from user space");
        err = -EFAULT;
        goto fail_out;
    }

    /* If the user didn't provide a NULL term, we will to avoid problems */
    filename[user_data->flen] = '\0';

    dnvme_vdbg("Dumping dnvme metrics to output file: %s", filename);
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)//2021.05.14 meng_yu add
        oldfs = force_uaccess_begin();
    #else
        oldfs = get_fs();
        set_fs(KERNEL_DS);
    #endif
    file = filp_open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (file) {

        /* Loop through the devices */
        list_for_each_entry(pmetrics_device, &nvme_ctx_list,
            entry) {

            /* Get the variable from metrics structure and write to file */
            snprintf(work, SIZE_OF_WORK, "nvme_context[%d]\n", dev++);
            __kernel_write(file, work, strlen(work), &pos);
            snprintf(work, SIZE_OF_WORK, "Minor Number = %d\n",
                pmetrics_device->dev->priv.minor);
            __kernel_write(file, work, strlen(work), &pos);
            snprintf(work, SIZE_OF_WORK, "open_flag = %d\n",
                pmetrics_device->dev->priv.opened);
            __kernel_write(file, work, strlen(work), &pos);
            snprintf(work, SIZE_OF_WORK, "pdev = 0X%llX\n",
                (u64)pmetrics_device->dev->priv.pdev);
            __kernel_write(file, work, strlen(work), &pos);

            snprintf(work, SIZE_OF_WORK, "bar0 = 0X%llX\n",
                (u64)pmetrics_device->dev->priv.bar0);
            __kernel_write(file, work, strlen(work), &pos);
            snprintf(work, SIZE_OF_WORK, "bar1 = 0X%llX\n",
                (u64)pmetrics_device->dev->priv.bar1);
            __kernel_write(file, work, strlen(work), &pos);
            snprintf(work, SIZE_OF_WORK, "bar2 = 0X%llX\n",
                (u64)pmetrics_device->dev->priv.bar2);
            __kernel_write(file, work, strlen(work), &pos);
            snprintf(work, SIZE_OF_WORK, "ctrlr_regs = 0X%llX\n",
                (u64)pmetrics_device->dev->priv.ctrlr_regs);
            __kernel_write(file, work, strlen(work), &pos);
            snprintf(work, SIZE_OF_WORK, "dmadev = 0X%llX\n",
                (u64)pmetrics_device->dev->priv.dmadev);
            __kernel_write(file, work, strlen(work), &pos);
            snprintf(work, SIZE_OF_WORK, "prp_page_pool = 0X%llX\n", (u64)
                pmetrics_device->dev->priv.prp_page_pool);
            __kernel_write(file, work, strlen(work), &pos);
            snprintf(work, SIZE_OF_WORK, "spcl_dev = 0X%llX\n",
                (u64)pmetrics_device->dev->priv.spcl_dev);
            __kernel_write(file, work, strlen(work), &pos);
            snprintf(work, SIZE_OF_WORK,
                "Interrupts:Active Scheme (S=0/M=1/X=2/N=3) = %d\n",
                pmetrics_device->dev->pub.irq_active.
                irq_type);
            __kernel_write(file, work, strlen(work), &pos);
            snprintf(work, SIZE_OF_WORK, "Interrupts:num_irqs = %d\n",
                pmetrics_device->dev->pub.irq_active.
                num_irqs);
            __kernel_write(file, work, strlen(work), &pos);
            /* Looping through the available CQ list */
            list_for_each_entry(pmetrics_cq_list, &pmetrics_device->
                cq_list, cq_entry) {

                /* Get the variable from CQ strucute and write to file */
                snprintf(work, SIZE_OF_WORK,
                    IDNT_L1"pmetrics_cq_list->pub[%d]", i);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"q_id = %d",
                    pmetrics_cq_list->pub.q_id);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"tail_ptr = %d",
                    pmetrics_cq_list->pub.tail_ptr);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"head pointer = %d",
                    pmetrics_cq_list->pub.head_ptr);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"elements = %d",
                    pmetrics_cq_list->pub.elements);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"irq enabled = %d",
                    pmetrics_cq_list->pub.irq_enabled);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"irq_no = %d",
                    pmetrics_cq_list->pub.irq_no);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"pbit_new_entry = %d",
                    pmetrics_cq_list->pub.pbit_new_entry);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK,
                    IDNT_L1"pmetrics_cq_list->priv[%d]", i++);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"buf = 0X%llX",
                    (u64)pmetrics_cq_list->priv.buf);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"dma_addr_t = 0X%llX",
                    (u64)pmetrics_cq_list->priv.dma);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK,
                    IDNT_L2"contig (1 = Y/(0 = N) = %d",
                    pmetrics_cq_list->priv.contig);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"size = %d",
                    pmetrics_cq_list->priv.size);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"dbs = 0X%llX",
                    (u64)pmetrics_cq_list->priv.dbs);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L3"prp_persist:");
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"npages = %d",
                    pmetrics_cq_list->priv.prp_persist.npages);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"type = %d",
                    pmetrics_cq_list->priv.prp_persist.type);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"buf = 0X%llX",
                    (u64)pmetrics_cq_list->priv.prp_persist.
                    buf);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"vir_prp_list = 0X%llX",
                    (u64)pmetrics_cq_list->priv.prp_persist.vir_prp_list);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"prp1 = 0X%llX",
                    (u64)pmetrics_cq_list->priv.prp_persist.prp1);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"prp2 = 0X%llX",
                    (u64)pmetrics_cq_list->priv.prp_persist.prp2);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"first dma = 0X%llX",
                    (u64)pmetrics_cq_list->priv.prp_persist.first_dma);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"data_dir = %d",
                    pmetrics_cq_list->priv.prp_persist.data_dir);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"data_buf_addr = 0X%llX",
                    (u64)pmetrics_cq_list->
                    priv.prp_persist.data_buf_addr);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"data_buf_size = %d",
                    pmetrics_cq_list->priv.prp_persist.data_buf_size);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"sq = 0X%llX",
                    (u64)pmetrics_cq_list->priv.prp_persist.sg);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"num_map_pgs = 0X%llX",
                    (u64)pmetrics_cq_list->priv.prp_persist.num_map_pgs);
                __kernel_write(file, work, strlen(work), &pos);
            } /* End of CQ list */

            i = 0;  /* Reset Q cnt */

            /* looping through available sq list */
            list_for_each_entry(pmetrics_sq_list, &pmetrics_device->
                sq_list, sq_entry) {

                /* Get each member of SQ structure and write to file */
                snprintf(work, SIZE_OF_WORK,
                    IDNT_L1"pmetrics_sq_list->pub[%d]", i);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"sq_id = %d",
                    pmetrics_sq_list->pub.sq_id);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"Assoc cq_id = %d",
                    pmetrics_sq_list->pub.cq_id);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"elements = %d",
                    pmetrics_sq_list->pub.elements);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"head_ptr = %d",
                    pmetrics_sq_list->pub.head_ptr);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"tail_ptr_virt = %d",
                    pmetrics_sq_list->pub.tail_ptr_virt);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"tail_ptr = %d",
                    pmetrics_sq_list->pub.tail_ptr);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK,
                    IDNT_L1"pmetrics_sq_list->priv[%d]", i++);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"buf = 0X%llX",
                    (u64)pmetrics_sq_list->priv.buf);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK,
                    IDNT_L2"contig (1 = Y/ 0 = N) = %d",
                    pmetrics_sq_list->priv.contig);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"size = %d",
                    pmetrics_sq_list->priv.size);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"unique_cmd_id(Cnt) = %d",
                    pmetrics_sq_list->priv.unique_cmd_id);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L2"dbs = 0X%llX",
                    (u64)pmetrics_sq_list->priv.dbs);
                __kernel_write(file, work, strlen(work), &pos);

                snprintf(work, SIZE_OF_WORK, IDNT_L3"prp_persist:");
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"npages = %d",
                    pmetrics_sq_list->priv.prp_persist.npages);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"buf = 0X%llX",
                    (u64)pmetrics_sq_list->priv.prp_persist.
                    buf);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"type = %d",
                    pmetrics_sq_list->priv.prp_persist.type);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"vir_prp_list = 0X%llX",
                    (u64)pmetrics_sq_list->priv.prp_persist.vir_prp_list);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"prp1 = 0X%llX",
                    (u64)pmetrics_sq_list->priv.prp_persist.prp1);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"prp2 = 0X%llX",
                    (u64)pmetrics_sq_list->priv.prp_persist.prp2);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"first dma = 0X%llX",
                    (u64)pmetrics_sq_list->priv.prp_persist.first_dma);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"data_dir = %d",
                    pmetrics_sq_list->priv.prp_persist.data_dir);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"data_buf_addr = 0X%llX",
                    (u64)pmetrics_sq_list->priv.prp_persist.
                    data_buf_addr);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"data_buf_size = %d",
                    pmetrics_sq_list->priv.prp_persist.data_buf_size);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"sg = 0X%llX",
                    (u64)pmetrics_sq_list->priv.prp_persist.sg);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L4"num_map_pgs = 0X%llX",
                    (u64)pmetrics_sq_list->priv.prp_persist.num_map_pgs);
                __kernel_write(file, work, strlen(work), &pos);
                snprintf(work, SIZE_OF_WORK, IDNT_L3"cmd track list = ");
                __kernel_write(file, work, strlen(work), &pos);
                /* Looping through the cmds if any */
                list_for_each_entry(pcmd_track_list,
                    &(pmetrics_sq_list->priv.cmd_list),
                    entry) {

                    /* write to file if any cmds exist */
                    snprintf(work, SIZE_OF_WORK, IDNT_L4"cmd track no = %d",
                        cmd++);
                    __kernel_write(file, work, strlen(work), &pos);
                    snprintf(work, SIZE_OF_WORK, IDNT_L4"unique_id = %d",
                        pcmd_track_list->unique_id);
                    __kernel_write(file, work, strlen(work), &pos);
                    snprintf(work, SIZE_OF_WORK, IDNT_L4"persist_q_id = %d",
                        pcmd_track_list->persist_q_id);
                    __kernel_write(file, work, strlen(work), &pos);
                    snprintf(work, SIZE_OF_WORK, IDNT_L4"opcode = %d",
                        pcmd_track_list->opcode);
                    __kernel_write(file, work, strlen(work), &pos);
                    snprintf(work, SIZE_OF_WORK, IDNT_L5"prp_nonpersist:");
                    __kernel_write(file, work, strlen(work), &pos);
                    /* Printing prp_nonpersist memeber variables */
                    snprintf(work, SIZE_OF_WORK,
                        IDNT_L6"buf = 0X%llX", (u64)
                        pcmd_track_list->prp_nonpersist.buf);
                    __kernel_write(file, work, strlen(work), &pos);
                    snprintf(work, SIZE_OF_WORK, IDNT_L6"npages = %d",
                        pcmd_track_list->prp_nonpersist.npages);
                    __kernel_write(file, work, strlen(work), &pos);
                    snprintf(work, SIZE_OF_WORK, IDNT_L6"type = %d",
                        pcmd_track_list->prp_nonpersist.type);
                    __kernel_write(file, work, strlen(work), &pos);
                    snprintf(work, SIZE_OF_WORK, IDNT_L6"vir_prp_list = 0X%llX",
                        (u64)pcmd_track_list->prp_nonpersist.vir_prp_list);
                    __kernel_write(file, work, strlen(work), &pos);
                    snprintf(work, SIZE_OF_WORK, IDNT_L6"prp1 = 0X%llX",
                        (u64)pcmd_track_list->prp_nonpersist.prp1);
                    __kernel_write(file, work, strlen(work), &pos);
                    snprintf(work, SIZE_OF_WORK, IDNT_L6"prp2 = 0X%llX",
                        (u64)pcmd_track_list->prp_nonpersist.prp2);
                    __kernel_write(file, work, strlen(work), &pos);
                    snprintf(work, SIZE_OF_WORK, IDNT_L6"first dma = 0X%llX",
                         (u64)pcmd_track_list->prp_nonpersist.first_dma);
                    __kernel_write(file, work, strlen(work), &pos);
                    snprintf(work, SIZE_OF_WORK, IDNT_L6"data_dir = %d",
                         pcmd_track_list->prp_nonpersist.data_dir);
                    __kernel_write(file, work, strlen(work), &pos);
                    snprintf(work, SIZE_OF_WORK,
                        IDNT_L6"data_buf_addr = 0X%llX",
                         (u64)pcmd_track_list->prp_nonpersist.data_buf_addr);
                    __kernel_write(file, work, strlen(work), &pos);
                    snprintf(work, SIZE_OF_WORK, IDNT_L6"data_buf_size = %d",
                         pcmd_track_list->prp_nonpersist.data_buf_size);
                    __kernel_write(file, work, strlen(work), &pos);
                    snprintf(work, SIZE_OF_WORK, IDNT_L6"sg = 0X%llX",
                         (u64)pcmd_track_list->prp_nonpersist.sg);
                    __kernel_write(file, work, strlen(work), &pos);
                    snprintf(work, SIZE_OF_WORK, IDNT_L6"num_map_pgs = 0X%llX",
                         (u64)pcmd_track_list->prp_nonpersist.num_map_pgs);
                    __kernel_write(file, work, strlen(work), &pos);
                } /* End of cmd track list */
            } /* End of SQ metrics list */
            pos = meta_nodes_log(file, pos, pmetrics_device);
            pos = irq_nodes_log(file, pos, pmetrics_device);
        } /* End of file writing */

        fput(file);
        filp_close(file, NULL);
    }
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)//2021.05.14 meng_yu add
        force_uaccess_end(oldfs);
    #else
        set_fs(oldfs);
    #endif	
    /* Fall through to label in intended */

fail_out:
    if (filename == NULL) {
        kfree(filename);
    }
    if (user_data != NULL) {
        kfree(user_data);
    }
    return err;
}


/*
 * Logging Meta data nodes into user space file.
 */
static loff_t meta_nodes_log(struct file *file, loff_t pos,
     struct nvme_context *pmetrics_device)
{
    struct nvme_meta *pmetrics_meta;
    u8 work[SIZE_OF_WORK];
    int i = 0;


    if (pmetrics_device->meta_set.pool == NULL) {
        return pos;
    }
    snprintf(work, SIZE_OF_WORK,
        IDNT_L1"pmetrics_device->meta_set.pool = 0x%llx",
        (u64)pmetrics_device->meta_set.pool);
    __kernel_write(file, work, strlen(work), &pos);

    list_for_each_entry(pmetrics_meta, &pmetrics_device->meta_set.
            meta_list, entry) {
        /* Get each Meta buffer node and write to file */
        snprintf(work, SIZE_OF_WORK,
            IDNT_L2"pmetrics_device->pmetrics_meta[%d]", i++);
        __kernel_write(file, work, strlen(work), &pos);
        snprintf(work, SIZE_OF_WORK, IDNT_L3"pmetrics_meta->id = %d",
            pmetrics_meta->id);
        __kernel_write(file, work, strlen(work), &pos);
        snprintf(work, SIZE_OF_WORK,
            IDNT_L3"pmetrics_meta->dma = 0x%llx",
            (u64)pmetrics_meta->dma);
        __kernel_write(file, work, strlen(work), &pos);
        snprintf(work, SIZE_OF_WORK,
            IDNT_L3"pmetrics_meta->buf = 0x%llx",
            (u64)pmetrics_meta->buf);
        __kernel_write(file, work, strlen(work), &pos);
    }
    return pos;
}


/*
 * logging irq nodes into user space file.
 */
static loff_t irq_nodes_log(struct file *file, loff_t pos,
    struct nvme_context *pmetrics_device_elem)
{
    u8 work[SIZE_OF_WORK];
    int i = 0;
    struct irq_track *pirq_node;
    struct irq_cq_track *pirq_cq_node;
    struct work_container *pwk_item_curr;  /* Current wk item in the list */


    /* locking on IRQ MUTEX here for irq track ll access */
    mutex_lock(&pmetrics_device_elem->irq_process.irq_track_mtx);

    snprintf(work, SIZE_OF_WORK, "\nirq_process.mask_ptr = 0x%llX",
        (u64)pmetrics_device_elem->irq_process.mask_ptr);
    __kernel_write(file, work, strlen(work), &pos);
    snprintf(work, SIZE_OF_WORK, "\nirq_process.irq_type = %d",
        pmetrics_device_elem->irq_process.irq_type);
    __kernel_write(file, work, strlen(work), &pos);

    /* Loop for the first irq node in irq track list */
    list_for_each_entry(pirq_node, &pmetrics_device_elem->
        irq_process.irq_track_list, irq_list_hd) {

        snprintf(work, SIZE_OF_WORK, IDNT_L1"pirq_node[%d]", i++);
        __kernel_write(file, work, strlen(work), &pos);
        snprintf(work, SIZE_OF_WORK, IDNT_L2"pirq_node->irq_no = %d",
            pirq_node->irq_no);
        __kernel_write(file, work, strlen(work), &pos);
        snprintf(work, SIZE_OF_WORK, IDNT_L2"pirq_node->int_vec = %d",
            pirq_node->int_vec);
        __kernel_write(file, work, strlen(work), &pos);
        snprintf(work, SIZE_OF_WORK, IDNT_L2"pirq_node->isr_fired = %d",
            pirq_node->isr_fired);
        __kernel_write(file, work, strlen(work), &pos);
        snprintf(work, SIZE_OF_WORK, IDNT_L2"pirq_node->isr_count = %d",
            pirq_node->isr_count);
        __kernel_write(file, work, strlen(work), &pos);

        /* Loop for each cq node within this irq node */
        list_for_each_entry(pirq_cq_node, &pirq_node->irq_cq_track,
            irq_cq_head) {

            snprintf(work, SIZE_OF_WORK, IDNT_L3"pirq_cq_node->cq_id = %d",
                pirq_cq_node->cq_id);
            __kernel_write(file, work, strlen(work), &pos);
        }

    }

    i = 0;
    /* Loop for the work nodes */
    list_for_each_entry(pwk_item_curr, &pmetrics_device_elem->
        irq_process.wrk_item_list, wrk_list_hd) {

        snprintf(work, SIZE_OF_WORK, IDNT_L1"wk_node[%d]", i++);
        __kernel_write(file, work, strlen(work), &pos);
        snprintf(work, SIZE_OF_WORK, IDNT_L2"wk_node->irq_no = %d",
            pwk_item_curr->irq_no);
        __kernel_write(file, work, strlen(work), &pos);
        snprintf(work, SIZE_OF_WORK, IDNT_L2"wk_node->int_vec = %d",
            pwk_item_curr->int_vec);
        __kernel_write(file, work, strlen(work), &pos);
        snprintf(work, SIZE_OF_WORK, IDNT_L2"wk_node->pirq_process = 0x%llX",
            (u64)pwk_item_curr->pirq_process);
        __kernel_write(file, work, strlen(work), &pos);
    }

    /* unlock IRQ MUTEX here */
    mutex_unlock(&pmetrics_device_elem->irq_process.irq_track_mtx);
    return pos;
}
