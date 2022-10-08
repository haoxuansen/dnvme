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

#include "../dnvme_interface.h"
#include "../dnvme_ioctls.h"

#include "common.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_irq.h"
#include "test_cq_gain.h"

static char * int_type[] = {
    "INT_MSI_SINGLE",
    "INT_MSI_MULTI",
    "INT_MSIX",
    "INT_PIN",
    "INT_NONE",
    "INT_FENCE"};

void set_irqs(int fd, enum nvme_irq_type irq_type, uint16_t num_irqs)
{
    int ret_val;
    struct interrupts new_irq = {0};

    LOG_INFO("Set interrupts, Type: %s, num_irqs:%d\n", int_type[irq_type], num_irqs);
    #ifdef AMD_MB_EN
    //Warning: AMD MB may not support msi-multi
    if (irq_type == INT_MSI_MULTI)
    {
        irq_type = INT_MSIX;
        LOG_WARN("AMD MB may not support msi-multi, use msi-x replace\n");
    }
    #endif

    g_nvme_dev.irq_type = irq_type;

    new_irq.irq_type = irq_type;
    new_irq.num_irqs = num_irqs;

    ret_val = ioctl(fd, NVME_IOCTL_SET_IRQ, &new_irq);
    if (ret_val < 0)
    {
        LOG_ERROR("Set interrupts ioctl \033[31mfailed!\033[0m\n");
    }
    else
    {
        LOG_DBUG("Set interrupts ioctl success!\n");
    }
}

void mask_irqs(int fd, uint16_t irq_no)
{
    int ret_val;

    ret_val = ioctl(fd, NVME_IOCTL_MASK_IRQ, irq_no);
    if (ret_val < 0)
    {
        LOG_ERROR("mask interrupts ioctl \033[31mfailed!\033[0m\n");
    }
    else
    {
        LOG_DBUG("mask interrupts ioctl success!\n");
    }
}

void umask_irqs(int fd, uint16_t irq_no)
{
    int ret_val;

    ret_val = ioctl(fd, NVME_IOCTL_UNMASK_IRQ, irq_no);
    if (ret_val < 0)
    {
        LOG_ERROR("unmask interrupts ioctl \033[31mfailed!\033[0m\n");
    }
    else
    {
        LOG_DBUG("unmask interrupts ioctl success!\n");
    }
}

void test_irq_review568(int fd)
{
    int i;
    i = 10000;
    while (i)
    {
        LOG_INFO("\nIRQ Loop Test = %d\n", i + 1);
        set_irqs(fd, INT_MSIX, 2);
        i--;
    }
    set_irqs(file_desc, INT_NONE, 0);
    LOG_INFO("\nCalling Dump Metrics to irq_loop_test\n");
    ioctl_dump(fd, "/tmp/test_rev568.txt");
    LOG_INFO("\nPressAny key..\n");
    getchar();
}

void test_loop_irq(int fd)
{
    int i, cmds;
    uint32_t num;
    void *rd_buffer;
    if (posix_memalign(&rd_buffer, 4096, RW_BUFFER_SIZE))
    {
        LOG_ERROR("Memalign Failed");
        return;
    }
    num = ioctl_reap_inquiry(fd, 0);
    /* Submit 10 cmds */
    LOG_INFO("\nEnter no of commands in ACQ:");
    fflush(stdout);
    scanf("%d", &cmds);
    for (i = 0; i < cmds; i++)
    {
        nvme_idfy_ctrl(fd, rd_buffer);
    }

    ioctl_tst_ring_dbl(fd, 0); /* Ring Admin Q Doorbell */
    while (ioctl_reap_inquiry(fd, 0) != (num + cmds))
        ;

    ioctl_dump(fd, "/tmp/irq_loop_test.txt");
    free(rd_buffer);
}

int irq_for_io_discontig(int file_desc, int cq_id, int irq_no, int cq_flags,
                         uint16_t elem, void *addr)
{
    int ret_val = -1;
    struct nvme_64b_send user_cmd = {0};
    struct nvme_create_cq create_cq_cmd = {0};

    /* Fill the command for create discontig IOSQ*/
    create_cq_cmd.opcode = 0x05;
    create_cq_cmd.cqid = cq_id;
    create_cq_cmd.qsize = elem;
    create_cq_cmd.cq_flags = cq_flags;
    create_cq_cmd.irq_vector = irq_no;
    create_cq_cmd.rsvd1[0] = 0x00;

    /* Fill the user command */
    user_cmd.q_id = 0;
    user_cmd.bit_mask = MASK_PRP1_LIST;
    user_cmd.cmd_buf_ptr = (u_int8_t *)&create_cq_cmd;
    user_cmd.data_buf_size = PAGE_SIZE_I * 16;
    user_cmd.data_buf_ptr = addr;
    user_cmd.data_dir = 0;

    LOG_INFO("User Call to send command\n");

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        LOG_ERROR("Sending of Command \033[31mfailed!\033[0m\n");
    }
    else
    {
        LOG_DBUG("Command sent \033[32msuccesfully\033[0m\n");
    }
    return ret_val;
}

int irq_for_io_contig(int file_desc, int cq_id, int irq_no,
                      int cq_flags, uint16_t elems)
{
    int ret_val = -1;
    struct nvme_64b_send user_cmd = {0};
    struct nvme_create_cq create_cq_cmd = {0};

    /* Fill the command for create contig IOSQ*/
    create_cq_cmd.opcode = 0x05;
    create_cq_cmd.cqid = cq_id;
    create_cq_cmd.qsize = elems;
    create_cq_cmd.cq_flags = cq_flags;
    create_cq_cmd.irq_vector = irq_no;
    create_cq_cmd.rsvd1[0] = 0x00;

    /* Fill the user command */
    user_cmd.q_id = 0;
    user_cmd.bit_mask = (MASK_PRP1_PAGE);
    user_cmd.cmd_buf_ptr = (u_int8_t *)&create_cq_cmd;
    user_cmd.data_buf_size = 0;
    user_cmd.data_buf_ptr = NULL;
    user_cmd.data_dir = 0;

    LOG_INFO("User Call to send command\n");

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        LOG_ERROR("Sending of Command \033[31mfailed!\033[0m\n");
    }
    else
    {
        LOG_DBUG("Command sent \033[32msuccesfully\033[0m\n");
    }
    return ret_val;
}

void test_irq_send_nvme_read(int file_desc, int sq_id, void *addr)
{
    int ret_val = -1;
    struct nvme_64b_send user_cmd = {0};
    struct nvme_rw_command nvme_read = {0};

    /* Fill the command for create discontig IOSQ*/
    nvme_read.opcode = 0x02;
    nvme_read.flags = 0;
    nvme_read.nsid = 1;
    nvme_read.metadata = 0;
    nvme_read.slba = 0;
    nvme_read.length = 15;

    /* Fill the user command */
    user_cmd.q_id = sq_id;
    user_cmd.bit_mask = (MASK_PRP1_PAGE | MASK_PRP1_LIST |
                         MASK_PRP2_PAGE | MASK_PRP2_LIST);
    user_cmd.cmd_buf_ptr = (u_int8_t *)&nvme_read;
    user_cmd.data_buf_size = RW_BUFFER_SIZE;
    user_cmd.data_buf_ptr = addr;
    user_cmd.data_dir = 0;

    LOG_INFO("User Call to send command\n");

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        LOG_ERROR("Sending of Command \033[31mfailed!\033[0m\n");
    }
    else
    {
        LOG_DBUG("Command sent \033[32msuccesfully\033[0m\n");
    }
}

void send_nvme_read_mb(int file_desc, int sq_id, void *addr, uint32_t meta_id)
{
    int ret_val = -1;
    struct nvme_64b_send user_cmd = {0};
    struct nvme_rw_command nvme_read = {0};

    /* Fill the command for create discontig IOSQ*/
    nvme_read.opcode = 0x02;
    nvme_read.flags = 0;
    nvme_read.nsid = 1;
    nvme_read.metadata = 0;
    nvme_read.slba = 0;
    nvme_read.length = 15;

    /* Fill the user command */
    user_cmd.q_id = sq_id; /* Contig SQ ID */
    user_cmd.bit_mask = (MASK_PRP1_PAGE | MASK_PRP1_LIST |
                         MASK_PRP2_PAGE | MASK_PRP2_LIST);
    user_cmd.cmd_buf_ptr = (u_int8_t *)&nvme_read;
    user_cmd.data_buf_size = RW_BUFFER_SIZE;
    user_cmd.data_buf_ptr = addr;
    user_cmd.meta_buf_id = meta_id;
    user_cmd.data_dir = 0;

    LOG_INFO("User Call to send command\n");

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        LOG_ERROR("Sending of Command \033[31mfailed!\033[0m\n");
    }
    else
    {
        LOG_DBUG("Command sent \033[32msuccesfully\033[0m\n");
    }
}

int admin_create_iocq_irq(int fd, int cq_id, int irq_no, int cq_flags)
{
    struct nvme_64b_send user_cmd = {0};
    struct nvme_create_cq create_cq_cmd = {0};
    int ret_val;

    //ioctl_prep_cq(fd, cq_id, 20, 1);

    /* Fill the command for create contig IOSQ*/
    create_cq_cmd.opcode = 0x05;
    create_cq_cmd.cqid = cq_id;
    create_cq_cmd.qsize = 20;
    create_cq_cmd.cq_flags = cq_flags;
    create_cq_cmd.irq_vector = irq_no;

    /* Fill the user command */
    user_cmd.q_id = 0;
    user_cmd.bit_mask = (MASK_PRP1_PAGE);
    user_cmd.cmd_buf_ptr = (u_int8_t *)&create_cq_cmd;
    user_cmd.data_buf_size = 0;
    user_cmd.data_buf_ptr = NULL;
    user_cmd.data_dir = 0;

    ret_val = nvme_64b_cmd(fd, &user_cmd);
    if (ret_val < 0)
    {
        LOG_ERROR("Sending Admin Command Create IO CQ ID:%d \033[31mfailed!\033[0m\n", cq_id);
    }
    else
    {
        LOG_DBUG("Admin Command Create IO CQ ID:%d success!\n\n", cq_id);
    }
    return ret_val;
}

void set_cq_irq(int fd, void *p_dcq_buf)
{
    int ret_val;
    int cq_id;
    int irq_no;
    int cq_flags;
    // int num;

    /* Discontig case */
    cq_id = 20;
    irq_no = 1;
    cq_flags = 0x2;
    // num = ioctl_reap_inquiry(fd, 0);
    ioctl_reap_inquiry(fd, 0);

    ret_val = ioctl_prep_cq(fd, cq_id, PAGE_SIZE_I, 0);
    if (ret_val < 0)
        exit(-1);
    ret_val = irq_for_io_discontig(fd, cq_id, irq_no, cq_flags,
                                   PAGE_SIZE_I, p_dcq_buf);
    ioctl_tst_ring_dbl(fd, 0);
    /* contig case */
    cq_id = 2;
    irq_no = 2;
    cq_flags = 0x3;

    ret_val = ioctl_prep_cq(fd, cq_id, PAGE_SIZE_I, 1);
    if (ret_val < 0)
        exit(-1);
    ret_val = irq_for_io_contig(fd, cq_id, irq_no, cq_flags, PAGE_SIZE_I);

    cq_id = 3;
    irq_no = 2;
    cq_flags = 0x3;

    ret_val = ioctl_prep_cq(fd, cq_id, PAGE_SIZE_I, 1);
    if (ret_val < 0)
        exit(-1);

    ret_val = irq_for_io_contig(fd, cq_id, irq_no, cq_flags, PAGE_SIZE_I);

    cq_id = 4;
    irq_no = 3;
    cq_flags = 0x3;

    ret_val = ioctl_prep_cq(fd, cq_id, PAGE_SIZE_I, 1);
    if (ret_val < 0)
        exit(-1);

    ret_val = irq_for_io_contig(fd, cq_id, irq_no, cq_flags, PAGE_SIZE_I);

    ioctl_tst_ring_dbl(fd, 0);

    test_reap_cq(fd, 0, 3, 1);
}

int irq_cr_contig_io_sq(int fd, int sq_id, int assoc_cq_id, uint16_t elems)
{
    int ret_val = -1;
    struct nvme_64b_send user_cmd = {0};
    struct nvme_create_sq create_sq_cmd = {0};

    /* Fill the command for create discontig IOSQ*/
    create_sq_cmd.opcode = 0x01;
    create_sq_cmd.sqid = sq_id;
    create_sq_cmd.qsize = elems;
    create_sq_cmd.cqid = assoc_cq_id;
    create_sq_cmd.sq_flags = 0x01;

    /* Fill the user command */
    user_cmd.q_id = 0;
    user_cmd.bit_mask = (MASK_PRP1_PAGE);
    user_cmd.cmd_buf_ptr = (u_int8_t *)&create_sq_cmd;
    user_cmd.data_buf_size = 0;
    user_cmd.data_buf_ptr = NULL;
    user_cmd.data_dir = 2;

    LOG_INFO("User Call to send command\n");

    ret_val = nvme_64b_cmd(fd, &user_cmd);
    if (ret_val < 0)
    {
        LOG_ERROR("Sending of Command \033[31mfailed!\033[0m\n");
    }
    else
    {
        LOG_DBUG("Command sent \033[32msuccesfully\033[0m\n");
    }
    return ret_val;
}

int irq_cr_disc_io_sq(int fd, void *addr, int sq_id,
                      int assoc_cq_id, uint16_t elems)
{
    int ret_val = -1;
    struct nvme_64b_send user_cmd = {0};
    struct nvme_create_sq create_sq_cmd = {0};

    /* Fill the command for create discontig IOSQ*/
    create_sq_cmd.opcode = 0x01;
    create_sq_cmd.sqid = sq_id;
    create_sq_cmd.qsize = elems;
    create_sq_cmd.cqid = assoc_cq_id;
    create_sq_cmd.sq_flags = 0x00;

    /* Fill the user command */
    user_cmd.q_id = 0;
    user_cmd.bit_mask = MASK_PRP1_LIST;
    user_cmd.cmd_buf_ptr = (u_int8_t *)&create_sq_cmd;
    user_cmd.data_buf_size = PAGE_SIZE_I * 64;
    user_cmd.data_buf_ptr = addr;
    user_cmd.data_dir = 2;

    ret_val = nvme_64b_cmd(fd, &user_cmd);
    if (ret_val < 0)
    {
        LOG_ERROR("Sending of Command \033[31mfailed!\033[0m\n");
    }
    else
    {
        LOG_DBUG("Command sent \033[32msuccesfully\033[0m\n");
    }
    return ret_val;
}

void set_sq_irq(int fd, void *addr)
{
    int sq_id;
    int assoc_cq_id;
    // int num;
    int ret_val;

    ioctl_reap_inquiry(fd, 0);
    sq_id = 31;
    assoc_cq_id = 2;
    ret_val = ioctl_prep_sq(fd, sq_id, assoc_cq_id, PAGE_SIZE_I, 0);
    if (ret_val < 0)
        return;
    ret_val = irq_cr_disc_io_sq(fd, addr, sq_id, assoc_cq_id, PAGE_SIZE_I);
    ioctl_tst_ring_dbl(fd, 0);
    /* Contig SQ */
    sq_id = 2;
    assoc_cq_id = 2;
    ret_val = ioctl_prep_sq(fd, sq_id, assoc_cq_id, PAGE_SIZE_I, 1);
    if (ret_val < 0)
        return;
    ret_val = irq_cr_contig_io_sq(fd, sq_id, assoc_cq_id, PAGE_SIZE_I);

    sq_id = 3;
    assoc_cq_id = 3;
    ret_val = ioctl_prep_sq(fd, sq_id, assoc_cq_id, PAGE_SIZE_I, 1);
    if (ret_val < 0)
        return;
    ret_val = irq_cr_contig_io_sq(fd, sq_id, assoc_cq_id, PAGE_SIZE_I);

    sq_id = 4;
    assoc_cq_id = 4;
    ret_val = ioctl_prep_sq(fd, sq_id, assoc_cq_id, PAGE_SIZE_I, 1);
    if (ret_val < 0)
        return;
    ret_val = irq_cr_contig_io_sq(fd, sq_id, assoc_cq_id, PAGE_SIZE_I);

    ioctl_tst_ring_dbl(fd, 0);
    test_reap_cq(fd, 0, 3, 1);
}

void test_contig_threeio_irq(int fd, void *addr0, void *addr1, void *addr2)
{
    static uint32_t meta_index = 10;
    int sq_id, assoc_cq_id = {0};
    uint32_t num;

    /* Send read command through 3 IO SQ's */

    /* SQ:CQ 32:21 */
    sq_id = 32;
    assoc_cq_id = 21;
    create_meta_buf(fd, meta_index);
    send_nvme_read_mb(fd, sq_id, addr0, meta_index);
    ioctl_tst_ring_dbl(fd, sq_id);

    /* SQ:CQ 33:22 */
    sq_id = 33;
    assoc_cq_id = 22;
    test_irq_send_nvme_read(fd, sq_id, addr1);
    ioctl_tst_ring_dbl(fd, sq_id);

    /* SQ:CQ 34:23 */

    sq_id = 34;
    assoc_cq_id = 23;
    num = ioctl_reap_inquiry(fd, assoc_cq_id);
    test_irq_send_nvme_read(fd, sq_id, addr2);
    ioctl_tst_ring_dbl(fd, sq_id);
    while (ioctl_reap_inquiry(fd, assoc_cq_id) != num + 1)
        ;
    ioctl_reap_cq(fd, assoc_cq_id, 1, 16, 0);

    meta_index++;
}

void test_discontig_io_irq(int fd, void *addr)
{
    int sq_id, assoc_cq_id;
    uint32_t num;
    /* SQ:CQ 31:20 */
    sq_id = 31;
    assoc_cq_id = 20;
    num = ioctl_reap_inquiry(fd, assoc_cq_id);
    test_irq_send_nvme_read(fd, sq_id, addr);
    ioctl_tst_ring_dbl(fd, sq_id);
    while (ioctl_reap_inquiry(fd, assoc_cq_id) != num + 1)
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
    num = ioctl_reap_inquiry(fd, 0);
    ioctl_delete_ioq(fd, op_code, q_id);
    ioctl_tst_ring_dbl(fd, 0);
    while (ioctl_reap_inquiry(fd, 0) != num + 1)
        ;

    /* SQ Case */
    op_code = 0x0;
    q_id = 31;
    num = ioctl_reap_inquiry(fd, 0);
    ioctl_delete_ioq(fd, op_code, q_id);
    ioctl_tst_ring_dbl(fd, 0);
    while (ioctl_reap_inquiry(fd, 0) != num + 1)
        ;
    /* SQ Case */
    op_code = 0x0;
    q_id = 33;
    num = ioctl_reap_inquiry(fd, 0);
    ioctl_delete_ioq(fd, op_code, q_id);
    ioctl_tst_ring_dbl(fd, 0);
    while (ioctl_reap_inquiry(fd, 0) != num + 1)
        ;

    /* SQ Case */
    op_code = 0x0;
    q_id = 34;
    num = ioctl_reap_inquiry(fd, 0);
    ioctl_delete_ioq(fd, op_code, q_id);
    ioctl_tst_ring_dbl(fd, 0);
    while (ioctl_reap_inquiry(fd, 0) != num + 1)
        ;

    /* CQ case */
    op_code = 0x4;
    q_id = 21;
    num = ioctl_reap_inquiry(fd, 0);
    ioctl_delete_ioq(fd, op_code, q_id);
    ioctl_tst_ring_dbl(fd, 0);
    while (ioctl_reap_inquiry(fd, 0) != num + 1)
        ;

    /* CQ case */
    op_code = 0x4;
    q_id = 22;
    num = ioctl_reap_inquiry(fd, 0);
    ioctl_delete_ioq(fd, op_code, q_id);
    ioctl_tst_ring_dbl(fd, 0);
    while (ioctl_reap_inquiry(fd, 0) != num + 1)
        ;
    /* CQ case */
    op_code = 0x4;
    q_id = 23;
    num = ioctl_reap_inquiry(fd, 0);
    ioctl_delete_ioq(fd, op_code, q_id);
    ioctl_tst_ring_dbl(fd, 0);
    while (ioctl_reap_inquiry(fd, 0) != num + 1)
        ;
    /* CQ case */
    op_code = 0x4;
    q_id = 20;
    num = ioctl_reap_inquiry(fd, 0);
    ioctl_delete_ioq(fd, op_code, q_id);
    ioctl_tst_ring_dbl(fd, 0);
    while (ioctl_reap_inquiry(fd, 0) != num + 1)
        ;
}
