/**
 * @file test_irq.c
 * @author yumeng (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2019-10-08
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdlib.h>

#include "dnvme.h"
#include "ioctl.h"
#include "irq.h"
#include "queue.h"
#include "cmd.h"

#include "common.h"
#include "test.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_irq.h"
#include "test_cq_gain.h"

int irq_for_io_discontig(int g_fd, int cq_id, int irq_no, int cq_flags,
                         uint16_t elem, void *addr)
{
    int ret_val = -1;
    struct nvme_64b_cmd user_cmd = {0};
    struct nvme_create_cq create_cq_cmd = {0};

    /* Fill the command for create discontig IOSQ*/
    create_cq_cmd.opcode = 0x05;
    create_cq_cmd.cqid = cq_id;
    create_cq_cmd.qsize = elem;
    create_cq_cmd.cq_flags = cq_flags;
    create_cq_cmd.irq_vector = irq_no;
    create_cq_cmd.rsvd1[0] = 0x00;

    /* Fill the user command */
    user_cmd.sqid = 0;
    user_cmd.bit_mask = NVME_MASK_PRP1_LIST;
    user_cmd.cmd_buf_ptr = (u_int8_t *)&create_cq_cmd;
    user_cmd.data_buf_size = PAGE_SIZE_I * 16;
    user_cmd.data_buf_ptr = addr;
    user_cmd.data_dir = 0;

    pr_info("User Call to send command\n");

    ret_val = nvme_submit_64b_cmd_legacy(g_fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending of Command \033[31mfailed!\033[0m\n");
    }
    else
    {
        pr_div("Command sent \033[32msuccesfully\033[0m\n");
    }
    return ret_val;
}

int irq_for_io_contig(int g_fd, int cq_id, int irq_no,
                      int cq_flags, uint16_t elems)
{
    int ret_val = -1;
    struct nvme_64b_cmd user_cmd = {0};
    struct nvme_create_cq create_cq_cmd = {0};

    /* Fill the command for create contig IOSQ*/
    create_cq_cmd.opcode = 0x05;
    create_cq_cmd.cqid = cq_id;
    create_cq_cmd.qsize = elems;
    create_cq_cmd.cq_flags = cq_flags;
    create_cq_cmd.irq_vector = irq_no;
    create_cq_cmd.rsvd1[0] = 0x00;

    /* Fill the user command */
    user_cmd.sqid = 0;
    user_cmd.bit_mask = (NVME_MASK_PRP1_PAGE);
    user_cmd.cmd_buf_ptr = (u_int8_t *)&create_cq_cmd;
    user_cmd.data_buf_size = 0;
    user_cmd.data_buf_ptr = NULL;
    user_cmd.data_dir = 0;

    pr_info("User Call to send command\n");

    ret_val = nvme_submit_64b_cmd_legacy(g_fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending of Command \033[31mfailed!\033[0m\n");
    }
    else
    {
        pr_div("Command sent \033[32msuccesfully\033[0m\n");
    }
    return ret_val;
}

void test_irq_send_nvme_read(int g_fd, int sq_id, void *addr)
{
    int ret_val = -1;
    struct nvme_64b_cmd user_cmd = {0};
    struct nvme_rw_command nvme_read = {0};

    /* Fill the command for create discontig IOSQ*/
    nvme_read.opcode = 0x02;
    nvme_read.flags = 0;
    nvme_read.nsid = 1;
    nvme_read.metadata = 0;
    nvme_read.slba = 0;
    nvme_read.length = 15;

    /* Fill the user command */
    user_cmd.sqid = sq_id;
    user_cmd.bit_mask = (NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST |
                         NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST);
    user_cmd.cmd_buf_ptr = (u_int8_t *)&nvme_read;
    user_cmd.data_buf_size = NVME_TOOL_RW_BUF_SIZE;
    user_cmd.data_buf_ptr = addr;
    user_cmd.data_dir = 0;

    pr_info("User Call to send command\n");

    ret_val = nvme_submit_64b_cmd_legacy(g_fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending of Command \033[31mfailed!\033[0m\n");
    }
    else
    {
        pr_div("Command sent \033[32msuccesfully\033[0m\n");
    }
}

int admin_create_iocq_irq(int fd, int cq_id, int irq_no, int cq_flags)
{
    struct nvme_64b_cmd user_cmd = {0};
    struct nvme_create_cq create_cq_cmd = {0};
    int ret_val;

    //nvme_prepare_iocq(fd, cq_id, 20, 1, 1, 0);

    /* Fill the command for create contig IOSQ*/
    create_cq_cmd.opcode = 0x05;
    create_cq_cmd.cqid = cq_id;
    create_cq_cmd.qsize = 20;
    create_cq_cmd.cq_flags = cq_flags;
    create_cq_cmd.irq_vector = irq_no;

    /* Fill the user command */
    user_cmd.sqid = 0;
    user_cmd.bit_mask = (NVME_MASK_PRP1_PAGE);
    user_cmd.cmd_buf_ptr = (u_int8_t *)&create_cq_cmd;
    user_cmd.data_buf_size = 0;
    user_cmd.data_buf_ptr = NULL;
    user_cmd.data_dir = 0;

    ret_val = nvme_submit_64b_cmd_legacy(fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending Admin Command Create IO CQ ID:%d \033[31mfailed!\033[0m\n", cq_id);
    }
    else
    {
        pr_div("Admin Command Create IO CQ ID:%d success!\n\n", cq_id);
    }
    return ret_val;
}

void set_cq_irq(int fd, void *p_dcq_buf)
{
    int ret_val;
    int cq_id;
    int irq_no;
    int cq_flags;
    struct nvme_prep_cq pcq = {0};
    // int num;

    /* Discontig case */
    cq_id = 20;
    irq_no = 1;
    cq_flags = 0x2;
    // num = nvme_inquiry_cq_entries(fd, 0);
    nvme_inquiry_cq_entries(fd, 0);

    nvme_fill_prep_cq(&pcq, cq_id, PAGE_SIZE_I, 0, 1, 0);
    ret_val = nvme_prepare_iocq(fd, &pcq);
    if (ret_val < 0)
        exit(-1);
    ret_val = irq_for_io_discontig(fd, cq_id, irq_no, cq_flags,
                                   PAGE_SIZE_I, p_dcq_buf);
    nvme_ring_sq_doorbell(fd, 0);
    /* contig case */
    cq_id = 2;
    irq_no = 2;
    cq_flags = 0x3;

    nvme_fill_prep_cq(&pcq, cq_id, PAGE_SIZE_I, 1, 1, 0);
    ret_val = nvme_prepare_iocq(fd, &pcq);
    if (ret_val < 0)
        exit(-1);
    ret_val = irq_for_io_contig(fd, cq_id, irq_no, cq_flags, PAGE_SIZE_I);

    cq_id = 3;
    irq_no = 2;
    cq_flags = 0x3;

    nvme_fill_prep_cq(&pcq, cq_id, PAGE_SIZE_I, 1, 1, 0);
    ret_val = nvme_prepare_iocq(fd, &pcq);
    if (ret_val < 0)
        exit(-1);

    ret_val = irq_for_io_contig(fd, cq_id, irq_no, cq_flags, PAGE_SIZE_I);

    cq_id = 4;
    irq_no = 3;
    cq_flags = 0x3;

    nvme_fill_prep_cq(&pcq, cq_id, PAGE_SIZE_I, 1, 1, 0);
    ret_val = nvme_prepare_iocq(fd, &pcq);
    if (ret_val < 0)
        exit(-1);

    ret_val = irq_for_io_contig(fd, cq_id, irq_no, cq_flags, PAGE_SIZE_I);

    nvme_ring_sq_doorbell(fd, 0);

    test_reap_cq(fd, 0, 3, 1);
}

int irq_cr_contig_io_sq(int fd, int sq_id, int assoc_cq_id, uint16_t elems)
{
    int ret_val = -1;
    struct nvme_64b_cmd user_cmd = {0};
    struct nvme_create_sq create_sq_cmd = {0};

    /* Fill the command for create discontig IOSQ*/
    create_sq_cmd.opcode = 0x01;
    create_sq_cmd.sqid = sq_id;
    create_sq_cmd.qsize = elems;
    create_sq_cmd.cqid = assoc_cq_id;
    create_sq_cmd.sq_flags = 0x01;

    /* Fill the user command */
    user_cmd.sqid = 0;
    user_cmd.bit_mask = (NVME_MASK_PRP1_PAGE);
    user_cmd.cmd_buf_ptr = (u_int8_t *)&create_sq_cmd;
    user_cmd.data_buf_size = 0;
    user_cmd.data_buf_ptr = NULL;
    user_cmd.data_dir = 2;

    pr_info("User Call to send command\n");

    ret_val = nvme_submit_64b_cmd_legacy(fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending of Command \033[31mfailed!\033[0m\n");
    }
    else
    {
        pr_div("Command sent \033[32msuccesfully\033[0m\n");
    }
    return ret_val;
}

int irq_cr_disc_io_sq(int fd, void *addr, int sq_id,
                      int assoc_cq_id, uint16_t elems)
{
    int ret_val = -1;
    struct nvme_64b_cmd user_cmd = {0};
    struct nvme_create_sq create_sq_cmd = {0};

    /* Fill the command for create discontig IOSQ*/
    create_sq_cmd.opcode = 0x01;
    create_sq_cmd.sqid = sq_id;
    create_sq_cmd.qsize = elems;
    create_sq_cmd.cqid = assoc_cq_id;
    create_sq_cmd.sq_flags = 0x00;

    /* Fill the user command */
    user_cmd.sqid = 0;
    user_cmd.bit_mask = NVME_MASK_PRP1_LIST;
    user_cmd.cmd_buf_ptr = (u_int8_t *)&create_sq_cmd;
    user_cmd.data_buf_size = PAGE_SIZE_I * 64;
    user_cmd.data_buf_ptr = addr;
    user_cmd.data_dir = 2;

    ret_val = nvme_submit_64b_cmd_legacy(fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending of Command \033[31mfailed!\033[0m\n");
    }
    else
    {
        pr_div("Command sent \033[32msuccesfully\033[0m\n");
    }
    return ret_val;
}

void set_sq_irq(int fd, void *addr)
{
    int sq_id;
    int assoc_cq_id;
    // int num;
    int ret_val;
    struct nvme_prep_sq psq = {0};

    nvme_inquiry_cq_entries(fd, 0);
    sq_id = 31;
    assoc_cq_id = 2;
    nvme_fill_prep_sq(&psq, sq_id, assoc_cq_id, PAGE_SIZE_I, 0);
    ret_val = nvme_prepare_iosq(fd, &psq);
    if (ret_val < 0)
        return;
    ret_val = irq_cr_disc_io_sq(fd, addr, sq_id, assoc_cq_id, PAGE_SIZE_I);
    nvme_ring_sq_doorbell(fd, 0);
    /* Contig SQ */
    sq_id = 2;
    assoc_cq_id = 2;
    nvme_fill_prep_sq(&psq, sq_id, assoc_cq_id, PAGE_SIZE_I, 1);
    ret_val = nvme_prepare_iosq(fd, &psq);
    if (ret_val < 0)
        return;
    ret_val = irq_cr_contig_io_sq(fd, sq_id, assoc_cq_id, PAGE_SIZE_I);

    sq_id = 3;
    assoc_cq_id = 3;
    nvme_fill_prep_sq(&psq, sq_id, assoc_cq_id, PAGE_SIZE_I, 1);
    ret_val = nvme_prepare_iosq(fd, &psq);
    if (ret_val < 0)
        return;
    ret_val = irq_cr_contig_io_sq(fd, sq_id, assoc_cq_id, PAGE_SIZE_I);

    sq_id = 4;
    assoc_cq_id = 4;
    nvme_fill_prep_sq(&psq, sq_id, assoc_cq_id, PAGE_SIZE_I, 1);
    ret_val = nvme_prepare_iosq(fd, &psq);
    if (ret_val < 0)
        return;
    ret_val = irq_cr_contig_io_sq(fd, sq_id, assoc_cq_id, PAGE_SIZE_I);

    nvme_ring_sq_doorbell(fd, 0);
    test_reap_cq(fd, 0, 3, 1);
}

void test_discontig_io_irq(int fd, void *addr)
{
    int sq_id, assoc_cq_id;
    uint32_t num;
    /* SQ:CQ 31:20 */
    sq_id = 31;
    assoc_cq_id = 20;
    num = nvme_inquiry_cq_entries(fd, assoc_cq_id);
    test_irq_send_nvme_read(fd, sq_id, addr);
    nvme_ring_sq_doorbell(fd, sq_id);
    while (nvme_inquiry_cq_entries(fd, assoc_cq_id) != num + 1)
        ;
    ioctl_reap_cq(fd, assoc_cq_id, 1, 16, 0);
}

void test_irq_delete(int fd)
{
    int op_code;
    int q_id;
    uint32_t num;

    /*
     * Host software shall delete all associated Submission Queues
     * prior to deleting a Completion Queue.
     */

    /* SQ Case */
    op_code = 0x0;
    q_id = 32;
    num = nvme_inquiry_cq_entries(fd, 0);
    ioctl_delete_ioq(fd, op_code, q_id);
    nvme_ring_sq_doorbell(fd, 0);
    while (nvme_inquiry_cq_entries(fd, 0) != num + 1)
        ;

    /* SQ Case */
    op_code = 0x0;
    q_id = 31;
    num = nvme_inquiry_cq_entries(fd, 0);
    ioctl_delete_ioq(fd, op_code, q_id);
    nvme_ring_sq_doorbell(fd, 0);
    while (nvme_inquiry_cq_entries(fd, 0) != num + 1)
        ;
    /* SQ Case */
    op_code = 0x0;
    q_id = 33;
    num = nvme_inquiry_cq_entries(fd, 0);
    ioctl_delete_ioq(fd, op_code, q_id);
    nvme_ring_sq_doorbell(fd, 0);
    while (nvme_inquiry_cq_entries(fd, 0) != num + 1)
        ;

    /* SQ Case */
    op_code = 0x0;
    q_id = 34;
    num = nvme_inquiry_cq_entries(fd, 0);
    ioctl_delete_ioq(fd, op_code, q_id);
    nvme_ring_sq_doorbell(fd, 0);
    while (nvme_inquiry_cq_entries(fd, 0) != num + 1)
        ;

    /* CQ case */
    op_code = 0x4;
    q_id = 21;
    num = nvme_inquiry_cq_entries(fd, 0);
    ioctl_delete_ioq(fd, op_code, q_id);
    nvme_ring_sq_doorbell(fd, 0);
    while (nvme_inquiry_cq_entries(fd, 0) != num + 1)
        ;

    /* CQ case */
    op_code = 0x4;
    q_id = 22;
    num = nvme_inquiry_cq_entries(fd, 0);
    ioctl_delete_ioq(fd, op_code, q_id);
    nvme_ring_sq_doorbell(fd, 0);
    while (nvme_inquiry_cq_entries(fd, 0) != num + 1)
        ;
    /* CQ case */
    op_code = 0x4;
    q_id = 23;
    num = nvme_inquiry_cq_entries(fd, 0);
    ioctl_delete_ioq(fd, op_code, q_id);
    nvme_ring_sq_doorbell(fd, 0);
    while (nvme_inquiry_cq_entries(fd, 0) != num + 1)
        ;
    /* CQ case */
    op_code = 0x4;
    q_id = 20;
    num = nvme_inquiry_cq_entries(fd, 0);
    ioctl_delete_ioq(fd, op_code, q_id);
    nvme_ring_sq_doorbell(fd, 0);
    while (nvme_inquiry_cq_entries(fd, 0) != num + 1)
        ;
}
