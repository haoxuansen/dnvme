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
#include "ioctl.h"
#include "queue.h"

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


void test_encrypt_decrypt(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t io_sq_id = 1;
    uint32_t io_cq_id = 1;
    uint32_t reap_num;
    uint32_t cmd_cnt = 0;
    uint16_t wr_nlb = 8;
    uint32_t wr_nsid = 1;
    uint64_t wr_slba = 0;
    uint8_t err_flg = 0;

    uint32_t i = 0, j = 0, k = 0;

    //XTS-AES-128/256 Encrypt 512 bytes Verctor 4 plaintext
    #if 1
        for (i = 0, j = 0, k = 0; i < 16; i++)
        {
            k = i * 4 + 3;
            *((uint32_t *)tool->wbuf + k) = (((j) << 24) | ((j + 1) << 16) | ((j + 2) << 8) | ((j + 3) << 0));
            *((uint32_t *)tool->wbuf + 64 + k) = *((uint32_t *)tool->wbuf + k);
            j += 4;
            *((uint32_t *)tool->wbuf + k - 1) = (((j) << 24) | ((j + 1) << 16) | ((j + 2) << 8) | ((j + 3) << 0));
            *((uint32_t *)tool->wbuf + 64 + k - 1) = *((uint32_t *)tool->wbuf + k - 1);
            j += 4;
            *((uint32_t *)tool->wbuf + k - 2) = (((j) << 24) | ((j + 1) << 16) | ((j + 2) << 8) | ((j + 3) << 0));
            *((uint32_t *)tool->wbuf + 64 + k - 2) = *((uint32_t *)tool->wbuf + k - 2);
            j += 4;
            *((uint32_t *)tool->wbuf + k - 3) = (((j) << 24) | ((j + 1) << 16) | ((j + 2) << 8) | ((j + 3) << 0));
            *((uint32_t *)tool->wbuf + 64 + k - 3) = *((uint32_t *)tool->wbuf + k - 3);
            j += 4;
            //pr_info("k:%d,j:%d,i:%d,tool->wbuf[%d]:0x%08x\n",k,j,i,i,((uint32_t *)tool->wbuf)[i]);
        }
    // for(i=0; i<128; i++)
    // {
    //     pr_info("tool->wbuf[%d]:0x%08x\n",i,((uint32_t *)tool->wbuf)[i]);
    // }
    #else
    // //ECB-AES128/256
    // *((uint32_t *)tool->wbuf) = 0x7393172a;
    // *((uint32_t *)tool->wbuf+1) = 0xe93d7e11;
    // *((uint32_t *)tool->wbuf+2) = 0x2e409f96;
    // *((uint32_t *)tool->wbuf+3) = 0x6bc1bee2;

    // //SM4-ECB128
    // *((uint32_t *)tool->wbuf) = 0xd468cdc9;
    // *((uint32_t *)tool->wbuf+1) = 0xc2d02d76;
    // *((uint32_t *)tool->wbuf+2) = 0x598a2797;
    // *((uint32_t *)tool->wbuf+3) = 0x115960c5;
    #endif

    wr_slba = 0xff;
    wr_nlb = 1;
    for (i = 0; i < (wr_nlb * LBA_DAT_SIZE); i++)
    {
        // *(char *)(tool->wbuf + i) = 0;
        *(char *)(tool->rbuf + i) = 0;
    }
    cmd_cnt = 0;
    nvme_io_write_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->wbuf);
    cmd_cnt++;
    nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    cq_gain(io_cq_id, cmd_cnt, &reap_num);
    cmd_cnt = 0;
    nvme_io_read_cmd(ndev->fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, tool->rbuf);
    cmd_cnt++;
    nvme_ring_sq_doorbell(ndev->fd, io_sq_id);
    cq_gain(io_cq_id, cmd_cnt, &reap_num);

    err_flg = 0;
#if 0
    // // ECB-AES128.Encrypt
    // if(*((uint32_t *)tool->rbuf) != 0x2466ef97)
    //     err_flg = 1;
    // if(*((uint32_t *)tool->rbuf+1) != 0xa89ecaf3)
    //     err_flg = 1;
    // if(*((uint32_t *)tool->rbuf+2) != 0x0d7a3660)
    //     err_flg = 1;
    // if(*((uint32_t *)tool->rbuf+3) != 0x3ad77bb4)
    //     err_flg = 1;

    // //SM4-ECB128.Encrypt
    // if(*((uint32_t *)tool->rbuf) != 0x4dd7c50b)
    //     err_flg = 1;
    // if(*((uint32_t *)tool->rbuf+1) != 0x00ce6c9a)
    //     err_flg = 1;
    // if(*((uint32_t *)tool->rbuf+2) != 0x9b327396)
    //     err_flg = 1;
    // if(*((uint32_t *)tool->rbuf+3) != 0xa52485ce)
    //     err_flg = 1;

    // //ECB-AES256.Encrypt
    // if(*((uint32_t *)tool->rbuf) != 0x3db181f8)
    //     err_flg = 1;
    // if(*((uint32_t *)tool->rbuf+1) != 0x064b5a7e)
    //     err_flg = 1;
    // if(*((uint32_t *)tool->rbuf+2) != 0xb5d2a03c)
    //     err_flg = 1;
    // if(*((uint32_t *)tool->rbuf+3) != 0xf3eed1bd)
    //     err_flg = 1;

    // XTS-AES128.Encrypt
    // if (*((uint32_t *)tool->rbuf) != 0xd4cfa6e2)
    //     err_flg = 1;
    // if (*((uint32_t *)tool->rbuf + 1) != 0x489f308c)
    //     err_flg = 1;
    // if (*((uint32_t *)tool->rbuf + 2) != 0xefa1d476)
    //     err_flg = 1;
    // if (*((uint32_t *)tool->rbuf + 3) != 0x27a7479b)
    //     err_flg = 1;
    // if (*((uint32_t *)tool->rbuf + 124) != 0x319d0568)
    //     err_flg = 1;
    // if (*((uint32_t *)tool->rbuf + 125) != 0xbe421ee5)
    //     err_flg = 1;
    // if (*((uint32_t *)tool->rbuf + 126) != 0x20147bea)
    //     err_flg = 1;
    // if (*((uint32_t *)tool->rbuf + 127) != 0x0a282df9)
    //     err_flg = 1;

    // XTS-AES256.Encrypt
    if (*((uint32_t *)tool->rbuf) != 0xe370cf9b)
        err_flg = 1;
    if (*((uint32_t *)tool->rbuf + 1) != 0xe4836c99)
        err_flg = 1;
    if (*((uint32_t *)tool->rbuf + 2) != 0x2f770386)
        err_flg = 1;
    if (*((uint32_t *)tool->rbuf + 3) != 0x1c3b3a10)
        err_flg = 1;
    if (*((uint32_t *)tool->rbuf + 124) != 0xe148c151)
        err_flg = 1;
    if (*((uint32_t *)tool->rbuf + 125) != 0xb9c6e693)
        err_flg = 1;
    if (*((uint32_t *)tool->rbuf + 126) != 0xa9fcea70)
        err_flg = 1;
    if (*((uint32_t *)tool->rbuf + 127) != 0xc4f36ffd)
        err_flg = 1;

#else
    if (FAILED == dw_cmp(tool->wbuf, tool->rbuf, wr_nlb * LBA_DAT_SIZE))
    {
        pr_info("\nwrite_buffer Data:\n");
        mem_disp(tool->wbuf, LBA_DAT_SIZE);
        pr_info("\nRead_buffer Data:\n");
        mem_disp(tool->rbuf, LBA_DAT_SIZE);
    }
#endif
    if (err_flg)
    {
        pr_err("Data enc ERROR!!!\n");
    }
    else
    {
        pr_info("Data enc PASS!!!\n");
    }
}
