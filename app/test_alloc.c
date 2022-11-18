/**
 * @file test_alloc.c
 * @author yumeng (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2019-10-08
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <ctype.h>
#include <malloc.h>
#include <stdint.h>
#include <sys/mman.h>

#include "dnvme_interface.h"
#include "dnvme_ioctls.h"

#include "common.h"
#include "unittest.h"
#include "test_metrics.h"
#include "test_cq_gain.h"

int ioctl_prep_sq(int file_desc, uint16_t sq_id, uint16_t cq_id, uint16_t elem, uint8_t contig)
{
    int ret_val = -1;
    struct nvme_prep_sq prep_sq = {0};

    prep_sq.sq_id = sq_id;
    prep_sq.cq_id = cq_id;
    prep_sq.elements = elem;
    prep_sq.contig = contig;
    prep_sq.sq_prio = MEDIUM_PRIO;

    LOG_DBUG("Calling Prepare SQ Creation...");
    LOG_DBUG("  SQ ID = %d", prep_sq.sq_id);
    LOG_DBUG("  Assoc CQ ID = %d", prep_sq.cq_id);
    LOG_DBUG("  No. of Elem = %d", prep_sq.elements);
    LOG_DBUG("  Contig(Y|N=(1|0)) = %d\n", prep_sq.contig);

    ret_val = ioctl(file_desc, NVME_IOCTL_PREPARE_SQ_CREATION, &prep_sq);

    if (ret_val < 0)
    {
        LOG_ERROR("\tSQ ID = %d Preparation Failed!\n", prep_sq.sq_id);
    }
    else
    {
        LOG_DBUG("\tSQ ID = %d Preparation success\n", prep_sq.sq_id);
    }
    return ret_val;
}

int ioctl_prep_cq(int file_desc, uint16_t cq_id, uint16_t elem, uint8_t contig)
{
    int ret_val = -1;
    struct nvme_prep_cq prep_cq = {0};

    prep_cq.cq_id = cq_id;
    prep_cq.elements = elem;
    prep_cq.contig = contig;
    /***************************/
    prep_cq.cq_irq_en = 1;
    prep_cq.cq_irq_no = 0;
    /***************************/

    LOG_DBUG("Calling Prepare CQ Creation...");
    LOG_DBUG("  CQ ID = %d", prep_cq.cq_id);
    LOG_DBUG("  No. of Elem = %d", prep_cq.elements);
    LOG_DBUG("  Contig(Y|N=(1|0)) = %d\n", prep_cq.contig);

    ret_val = ioctl(file_desc, NVME_IOCTL_PREPARE_CQ_CREATION, &prep_cq);

    if (ret_val < 0)
    {
        LOG_ERROR("\tCQ ID = %d Preparation Failed!\n", prep_cq.cq_id);
    }
    else
    {
        LOG_DBUG("\tCQ ID = %d Preparation success\n", prep_cq.cq_id);
    }
    return ret_val;
}

uint32_t ioctl_reap_inquiry(int file_desc, int cq_id)
{
    int ret_val = -1;
    struct nvme_reap_inquiry rp_inq = {0};

    rp_inq.q_id = cq_id;

    ret_val = ioctl(file_desc, NVME_IOCTL_REAP_INQUIRY, &rp_inq);
    if (ret_val < 0)
    {
        LOG_ERROR("reap inquiry Failed! ret_val:%d\n", ret_val);
        exit(-1);
    }
    // else
    // {
    //     if (rp_inq.num_remaining)
    //         LOG_INFO("\tReap Inquiry on CQ ID = %d, Num_Remaining = %d, ISR_count = %d\n",
    //                rp_inq.q_id, rp_inq.num_remaining, rp_inq.isr_count);
    // }
    return rp_inq.num_remaining;
}

int display_cq_data(unsigned char *cq_buffer, int reap_ele, int display)
{
    int ret_val = -1;
    struct cq_completion *cq_entry = NULL;
    while (reap_ele)
    {
        cq_entry = (struct cq_completion *)cq_buffer;
        if (cq_entry->status_field) // status != 0 !!!!! force display
        {
            LOG_WARN("  Reaped:cmd_id=%d, dw0=%#x, phase_bit=%d, sq_head_ptr=%#x, sq_id=%d, sts=%#x\n",
                cq_entry->cmd_identifier, cq_entry->cmd_specifc, cq_entry->phase_bit, cq_entry->sq_head_ptr,
                cq_entry->sq_identifier, cq_entry->status_field);
            ret_val = -1;
        }
        else
        {
            if (display)
            {
                LOG_INFO("  Reaped:cmd_id=%d, dw0=%#x, phase_bit=%d, sq_head_ptr=%#x, sq_id=%d, sts=%#x\n",
                    cq_entry->cmd_identifier, cq_entry->cmd_specifc, cq_entry->phase_bit, cq_entry->sq_head_ptr,
                    cq_entry->sq_identifier, cq_entry->status_field);
            }
            ret_val = 0;
        }
        reap_ele--;
        cq_buffer += sizeof(struct cq_completion);
    }
    return ret_val;
}

int ioctl_reap_cq(int file_desc, int cq_id, int elements, int size, int display)
{
    struct nvme_reap rp_cq = {0};

    int ret_val = 0;
    rp_cq.q_id = cq_id;
    rp_cq.elements = elements;
    rp_cq.size = (size * elements);
    rp_cq.buffer = malloc(sizeof(char) * rp_cq.size);
    if (rp_cq.buffer == NULL)
    {
        LOG_ERROR("Malloc Failed");
        return -1;
    }
    ret_val = ioctl(file_desc, NVME_IOCTL_REAP, &rp_cq);
    if (ret_val < 0)
    {
        LOG_ERROR("reap cq Failed! ret_val:%d\n", ret_val);
    }
    else
    {
        LOG_DBUG("  Reaped on CQ ID = %d, No Request = %d, No Reaped = %d, No Rem = %d, ISR_count = %d\n",
            rp_cq.q_id, rp_cq.elements, rp_cq.num_reaped, rp_cq.num_remaining, rp_cq.isr_count);
        if (display_cq_data(rp_cq.buffer, rp_cq.num_reaped, display))
            ret_val = -1;
    }
    free(rp_cq.buffer);
    return ret_val;
}

int ioctl_meta_buf_create(int file_desc, int size)
{
    int ret_val;
    ret_val = ioctl(file_desc, NVME_IOCTL_METABUF_CREATE, size);
    if (ret_val < 0)
    {
        LOG_ERROR("Meta data creation failed!\n");
    }
    else
    {
        LOG_DBUG("Meta Data creation success!!\n");
    }
    return ret_val;
}

int ioctl_meta_buf_alloc(int file_desc, uint32_t meta_id)
{
    int ret_val;
    ret_val = ioctl(file_desc, NVME_IOCTL_METABUF_ALLOC, meta_id);
    if (ret_val < 0)
    {
        LOG_ERROR("\nMeta Id = %d allocation failed!\n", meta_id);
    }
    else
    {
        LOG_DBUG("Meta Id = %d allocation success!!\n", meta_id);
    }
    return ret_val;
}

int ioctl_meta_buf_delete(int file_desc, uint32_t meta_id)
{
    int ret_val;
    ret_val = ioctl(file_desc, NVME_IOCTL_METABUF_DELETE, meta_id);
    if (ret_val < 0)
    {
        LOG_ERROR("\nMeta Id = %d allocation failed!\n", meta_id);
    }
    else
    {
        LOG_DBUG("Meta Id = %d allocation success!!\n", meta_id);
    }
    return ret_val;
}

uint32_t create_meta_buf(int file_desc, uint32_t meta_id)
{
    int ret_val;
    ret_val = ioctl_meta_buf_create(file_desc, 4096);
    ret_val = ioctl_meta_buf_alloc(file_desc, meta_id);
    return ret_val;
}

void test_meta(int file_desc)
{
    int size;
    int ret_val;
    int meta_id;
    uint64_t *kadr;

    size = 4096;
    ret_val = ioctl_meta_buf_create(file_desc, size);

    for (meta_id = 0; meta_id < 20; meta_id++)
    {
        ret_val = ioctl_meta_buf_alloc(file_desc, meta_id);
    }

    meta_id = 5;
    ret_val = ioctl_meta_buf_delete(file_desc, meta_id);
    meta_id = 6;
    ret_val = ioctl_meta_buf_delete(file_desc, meta_id);
    meta_id = 6;
    ret_val = ioctl_meta_buf_delete(file_desc, meta_id);
    meta_id = 5;
    ret_val = ioctl_meta_buf_delete(file_desc, meta_id);
    meta_id = 6;
    ret_val = ioctl_meta_buf_alloc(file_desc, meta_id);

    meta_id = 0x80004;
    LOG_INFO("\nTEST 3.1: Call to Mmap encoded Meta Id = 0x%x\n", meta_id);
    kadr = mmap(0, 4096, PROT_READ, MAP_SHARED, file_desc, 4096 * meta_id);
    if (!kadr)
    {
        LOG_ERROR("mapping failed\n");
        exit(-1);
    }
    mem_disp(kadr,4096);
    munmap(kadr, 4096);
    if(ret_val<0)
    return;
}
