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

#include "byteorder.h"
#include "dnvme_ioctl.h"
#include "meta.h"
#include "queue.h"

#include "common.h"
#include "unittest.h"
#include "test_metrics.h"
#include "test_cq_gain.h"

int display_cq_data(unsigned char *cq_buffer, int reap_ele, int display)
{
    int ret_val = -1;
    struct nvme_completion *cq_entry = NULL;
    while (reap_ele)
    {
        cq_entry = (struct nvme_completion *)cq_buffer;
        if (NVME_CQE_STATUS_TO_STATE(cq_entry->status)) // status != 0 !!!!! force display
        {
            pr_warn("  Reaped:cmd_id=%d, dw0=%#x, phase_bit=%d, sq_head_ptr=%#x, sq_id=%d, sts=%#x\n",
                cq_entry->command_id, cq_entry->result.u32, 
                NVME_CQE_STATUS_TO_PHASE(cq_entry->status), cq_entry->sq_head,
                cq_entry->sq_id, NVME_CQE_STATUS_TO_STATE(cq_entry->status));
            ret_val = -1;
        }
        else
        {
            if (display)
            {
                pr_info("  Reaped:cmd_id=%d, dw0=%#x, phase_bit=%d, sq_head_ptr=%#x, sq_id=%d, sts=%#x\n",
                    cq_entry->command_id, cq_entry->result.u32, 
                    NVME_CQE_STATUS_TO_PHASE(cq_entry->status), cq_entry->sq_head,
                    cq_entry->sq_id, NVME_CQE_STATUS_TO_STATE(cq_entry->status));
            }
            ret_val = 0;
        }
        reap_ele--;
        cq_buffer += sizeof(struct nvme_completion);
    }
    return ret_val;
}

int ioctl_reap_cq(int g_fd, int cq_id, int elements, int size, int display)
{
    struct nvme_reap rp_cq = {0};

    int ret_val = 0;
    rp_cq.q_id = cq_id;
    rp_cq.elements = elements;
    rp_cq.size = (size * elements);
    rp_cq.buffer = malloc(sizeof(char) * rp_cq.size);
    if (rp_cq.buffer == NULL)
    {
        pr_err("Malloc Failed");
        return -1;
    }
    ret_val = nvme_reap_cq_entries(g_fd, &rp_cq);
    if (ret_val < 0)
    {
        pr_err("reap cq Failed! ret_val:%d\n", ret_val);
    }
    else
    {
        pr_div("  Reaped on CQ ID = %d, No Request = %d, No Reaped = %d, No Rem = %d, ISR_count = %d\n",
            rp_cq.q_id, rp_cq.elements, rp_cq.num_reaped, rp_cq.num_remaining, rp_cq.isr_count);
        if (display_cq_data(rp_cq.buffer, rp_cq.num_reaped, display))
            ret_val = -1;
    }
    free(rp_cq.buffer);
    return ret_val;
}

uint32_t create_meta_buf(int g_fd, uint32_t id)
{
    int ret_val;
    ret_val = nvme_create_meta_pool(g_fd, 4096);
    ret_val = nvme_create_meta_node(g_fd, id);
    return ret_val;
}

void test_meta(int g_fd)
{
    uint32_t size;
    int ret_val;
    int id;
    uint64_t *kadr;

    size = 4096;
    ret_val = nvme_create_meta_pool(g_fd, size);

    for (id = 0; id < 20; id++)
    {
        ret_val = nvme_create_meta_node(g_fd, id);
    }

    id = 5;
    ret_val = nvme_delete_meta_node(g_fd, id);
    id = 6;
    ret_val = nvme_delete_meta_node(g_fd, id);
    id = 6;
    ret_val = nvme_delete_meta_node(g_fd, id);
    id = 5;
    ret_val = nvme_delete_meta_node(g_fd, id);
    id = 6;
    ret_val = nvme_create_meta_node(g_fd, id);

    id = 0x80004;
    pr_info("\nTEST 3.1: Call to Mmap encoded Meta Id = 0x%x\n", id);
    kadr = mmap(0, 4096, PROT_READ, MAP_SHARED, g_fd, 4096 * id);
    if (!kadr)
    {
        pr_err("mapping failed\n");
        exit(-1);
    }
    mem_disp(kadr,4096);
    munmap(kadr, 4096);
    if(ret_val<0)
    return;
}
