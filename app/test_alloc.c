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
    rp_cq.cqid = cq_id;
    rp_cq.expect = elements;
    rp_cq.size = (size * elements);
    rp_cq.buf = malloc(sizeof(char) * rp_cq.size);
    if (rp_cq.buf == NULL)
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
        pr_div("  Reaped on CQ ID = %d, No Request = %d, No Reaped = %d, No Rem = %d\n",
            rp_cq.cqid, rp_cq.expect, rp_cq.reaped, rp_cq.remained);
        if (display_cq_data(rp_cq.buf, rp_cq.reaped, display))
            ret_val = -1;
    }
    free(rp_cq.buf);
    return ret_val;
}

