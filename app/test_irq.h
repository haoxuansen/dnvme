/**
 * @file test_irq.h
 * @author yumeng (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2019-10-08
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef _TEST_IRQ_H_
#define _TEST_IRQ_H_

// #include "test_send_cmd.h"

#define PAGE_SIZE_I 4096

int admin_create_iocq_irq(int fd, int cq_id, int irq_vec, int cq_flags);
void set_cq_irq(int fd, void *p_dcq_buf);
void set_sq_irq(int fd, void *addr);
int irq_for_io_discontig(int g_fd, int cq_id, int irq_no, int cq_flags, uint16_t elem, void *addr);
int irq_for_io_contig(int g_fd, int cq_id, int irq_no, int cq_flags, uint16_t elems);
void test_discontig_io_irq(int fd, void *addr);
void test_irq_delete(int fd);
int irq_cr_contig_io_sq(int fd, int sq_id, int assoc_cq_id, uint16_t elems);
int irq_cr_disc_io_sq(int fd, void *addr, int sq_id, int assoc_cq_id, uint16_t elems);

#endif
