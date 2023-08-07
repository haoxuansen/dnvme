/**
 * @file test_metrics.c
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

#include "dnvme.h"
#include "libnvme.h"

#include "common.h"
#include "test.h"
#include "test_send_cmd.h"
#include "test_metrics.h"
#include "test_cq_gain.h"
#include "test_irq.h"
#include "unittest.h"

int test_create_contig_iocq(int g_fd, uint16_t io_cq_id, uint16_t cq_size)
{
    return nvme_create_contig_iocq(g_fd, io_cq_id, cq_size, 1, 1);
}

int test_create_contig_iosq(int g_fd, uint16_t io_sq_id, uint16_t io_cq_id, uint16_t sq_size)
{
    return nvme_create_contig_iosq(g_fd, io_sq_id, io_cq_id, sq_size, 1);
}

int test_reap_cq(int g_fd, int cq_id, uint32_t cmd_cnt, int disp_flag)
{
    while (nvme_inquiry_cq_entries(g_fd, cq_id) != cmd_cnt)
        usleep(50);
    return (ioctl_reap_cq(g_fd, cq_id, cmd_cnt, 16, disp_flag));
}

struct nvme_completion *send_get_feature(int g_fd, uint8_t feature_id)
{
    uint32_t reap_num = 0;
    //send command
    if (nvme_get_feature_cmd(g_fd, 1, feature_id))
    {
        pr_err("send get feature cmd Failed!\n");
    }
    // ring doorbell
    if (nvme_ring_sq_doorbell(g_fd, 0))
    {
        pr_err("DBL ERR!");
    }
    //get 1 cq from admin cq
    if (cq_gain(0, 1, &reap_num))
    {
        pr_err("cq_gain failed!!!\n");
    }
    //get cq entry
    return get_cq_entry();
}

/******************************************************************************
 * Name:    CRC-32/MPEG-2  x32+x26+x23+x22+x16+x12+x11+x10+x8+x7+x5+x4+x2+x+1
 * Poly:    0x4C11DB7
 * Init:    0xFFFFFFF
 * Refin:   False
 * Refout:  False
 * Xorout:  0x0000000
 * Note:
 *****************************************************************************/
uint32_t crc32_mpeg_2(uint8_t *data, uint32_t length)
{
    uint8_t i;
    uint32_t crc = 0xffffffff; // Initial value
    while (length--)
    {
        crc ^= (uint32_t)(*data++) << 24; // crc ^=(uint32_t)(*data)<<24; data++;
        for (i = 0; i < 8; ++i)
        {
            if (crc & 0x80000000)
                crc = (crc << 1) ^ 0x04C11DB7;
            else
                crc <<= 1;
        }
    }
    return crc;
}

