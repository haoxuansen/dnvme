/**
 * @file test_rng_sqxdbl.c
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

#include "dnvme_interface.h"
#include "dnvme_ioctl.h"

#include "common.h"
#include "test_metrics.h"

int ioctl_tst_ring_dbl(int file_desc, int sq_id)
{
    int ret_val = -1;

    ret_val = ioctl(file_desc, NVME_IOCTL_RING_SQ_DOORBELL, sq_id);
    if (ret_val < 0)
    {
        pr_err("\t\tRing Doorbell of SQ ID = %d Failed!\n", sq_id);
        return FAILED;
    }
    else
    {
        pr_debug("\t\tRing Doorbell of SQ ID = %d success\n", sq_id);
    }
    return SUCCEED;
}
