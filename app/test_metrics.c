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

#include "dnvme_interface.h"
#include "dnvme_ioctls.h"

#include "common.h"
#include "test_send_cmd.h"
#include "test_metrics.h"
#include "test_cq_gain.h"
#include "test_irq.h"
#include "unittest.h"

/*
 * Functions for the ioctl calls
*/
void ioctl_get_q_metrics(int file_desc, int q_id, int q_type, int size)
{
    int ret_val = -1;
    uint16_t tmp;
    struct nvme_get_q_metrics get_q_metrics = {0};

    pr_debug("User App Calling Get Q Metrics...\n");

    get_q_metrics.q_id = q_id;
    get_q_metrics.type = q_type;
    get_q_metrics.nBytes = size;

    if (q_type == 1)
    {
        get_q_metrics.buffer = malloc(sizeof(uint8_t) *
                                      sizeof(struct nvme_gen_sq));
    }
    else
    {
        get_q_metrics.buffer = malloc(sizeof(uint8_t) *
                                      sizeof(struct nvme_gen_cq));
    }
    if (get_q_metrics.buffer == NULL)
    {
        pr_err("Malloc Failed");
        return;
    }
    ret_val = ioctl(file_desc, NVME_IOCTL_GET_Q_METRICS, &get_q_metrics);

    if (ret_val < 0)
    {
        pr_err("\tQ metrics could not be checked!\n");
    }
    else
    {
        if (q_type == 1)
        {
            memcpy(&tmp, &get_q_metrics.buffer[0], sizeof(uint16_t));
            pr_debug("\nMetrics for SQ Id = %d\n", tmp);
            memcpy(&tmp, &get_q_metrics.buffer[2], sizeof(uint16_t));
            pr_debug("\tCQ Id = %d\n", tmp);
            memcpy(&tmp, &get_q_metrics.buffer[4], sizeof(uint16_t));
            pr_debug("\tTail Ptr = %d\n", tmp);
            memcpy(&tmp, &get_q_metrics.buffer[6], sizeof(uint16_t));
            pr_debug("\tTail_Ptr_Virt = %d\n", tmp);
            memcpy(&tmp, &get_q_metrics.buffer[8], sizeof(uint16_t));
            pr_debug("\tHead Ptr = %d\n", tmp);
            memcpy(&tmp, &get_q_metrics.buffer[10], sizeof(uint16_t));
            pr_debug("\tElements = %d\n", tmp);
        }
        else
        {
            memcpy(&tmp, &get_q_metrics.buffer[0], sizeof(uint16_t));
            pr_debug("\nMetrics for CQ Id = %d\n", tmp);
            memcpy(&tmp, &get_q_metrics.buffer[2], sizeof(uint16_t));
            pr_debug("\tTail_Ptr = %d\n", tmp);
            memcpy(&tmp, &get_q_metrics.buffer[4], sizeof(uint16_t));
            pr_debug("\tHead Ptr = %d\n", tmp);
            memcpy(&tmp, &get_q_metrics.buffer[6], sizeof(uint16_t));
            pr_debug("\tElements = %d\n", tmp);
            memcpy(&tmp, &get_q_metrics.buffer[8], sizeof(uint16_t));
            pr_debug("\tIrq Enabled = %d\n", tmp);
        }
    }
    free(get_q_metrics.buffer);
}

void admin_queue_config(int file_desc)
{
    ioctl_create_acq(file_desc, MAX_ADMIN_QUEUE_SIZE);
    ioctl_create_asq(file_desc, MAX_ADMIN_QUEUE_SIZE);
}

void test_drv_metrics(int file_desc)
{
    struct metrics_driver get_drv_metrics = {0};
    int ret_val = -1;

    ret_val = ioctl(file_desc, NVME_IOCTL_GET_DRIVER_METRICS, &get_drv_metrics);
    if (ret_val < 0)
    {
        pr_err("\tDrv metrics Failed!\n");
    }
    pr_debug("Drv Version = 0x%X\n", get_drv_metrics.driver_version);
    pr_debug("Api Version = 0x%X\n", get_drv_metrics.api_version);
}

void test_dev_metrics(int file_desc)
{
    struct public_metrics_dev get_dev_metrics = {0};
    int ret_val = -1;

    ret_val = ioctl(file_desc, NVME_IOCTL_GET_DEVICE_METRICS, &get_dev_metrics);
    if (ret_val < 0)
    {
        pr_err("\tDev metrics Failed!\n");
    }
    pr_debug("IRQ Type = %d (0=S/1=M/2=X/3=N)\n", get_dev_metrics.irq_active.irq_type);
    pr_debug("IRQ No's = %d\n", get_dev_metrics.irq_active.num_irqs);
}

/**
 * @brief ioctl_read_data, nvme layer
 * 
 * @param file_desc 
 * @param offset 
 * @param nBytes 
 * @return uint32_t 
 */
uint32_t ioctl_read_data(int file_desc, uint32_t offset, uint32_t nBytes)
{
    int ret_val = 0;
    uint32_t u32_data = 0;
    struct rw_generic test_data = {0};

    test_data.type = NVMEIO_BAR01;
    test_data.offset = offset;
    test_data.nBytes = nBytes;
    test_data.acc_type = BYTE_LEN;

    test_data.buffer = malloc(sizeof(char) * test_data.nBytes);
    if (test_data.buffer == NULL)
    {
        pr_err("Malloc Failed");
        exit(-1);
    }
    ret_val = ioctl(file_desc, NVME_IOCTL_READ_GENERIC, &test_data);
    if (ret_val < 0)
    {
        pr_err("ioctl_set_msg failed:%d\n", ret_val);
        exit(-1);
    }
    u32_data = *(uint32_t *)test_data.buffer;
#if 0
    if (nBytes / 4 == 0)
        pr_info("NVME Reading DWORD at offset:0x%02x Val:0x%08x\n", test_data.offset, *(uint32_t *)test_data.buffer);
    else if (nBytes % 4 != 0)
    {
        for (uint32_t i = 0; i < (nBytes / 4 + 1); i++)
        {
            pr_info("NVME Reading DWORD at offset:0x%02x Val:0x%08x\n", test_data.offset + i * 4, *(uint32_t *)(test_data.buffer + i * 4));
        }
    }
    else
    {
        for (uint32_t i = 0; i < (nBytes / 4); i++)
        {
            pr_info("NVME Reading DWORD at offset:0x%02x Val:0x%08x\n", test_data.offset + i * 4, *(uint32_t *)(test_data.buffer + i * 4));
        }
    }
#endif
    free(test_data.buffer);
    return u32_data; //DEBUG: just return first u32 data !!!
}
/*
 * @brief read_nvme_register nvme layer
 * @param[in] file_desc  
 * @param[in] offset 
 * @param[in] nBytes 
 * @param[out] buffer output register data
 * @return depends on REQUEST. Usually -1 indicates error.
*/
int read_nvme_register(int file_desc, uint32_t offset, uint32_t nBytes, uint8_t *byte_buffer)
{
    int ret_val = 0;
    struct rw_generic test_data = {0};

    test_data.type = NVMEIO_BAR01; //nvme register
    test_data.offset = offset;
    test_data.nBytes = nBytes;
    test_data.acc_type = BYTE_LEN;
    test_data.buffer = byte_buffer;
    ret_val = ioctl(file_desc, NVME_IOCTL_READ_GENERIC, &test_data);
    if (ret_val < 0)
    {
        pr_err("ioctl_set_msg failed:%d\n", ret_val);
        exit(-1);
    }
    return ret_val;
}

/**
 * @brief ioctl_write_data, nvme layer
 * 
 * @param file_desc 
 * @param offset 
 * @param nBytes 
 * @param byte_buffer 
 * @return int 
 */
int ioctl_write_data(int file_desc, uint32_t offset, uint32_t nBytes, uint8_t *byte_buffer)
{
    int ret_val = 0;
    //int message;
    struct rw_generic test_data = {0};

    test_data.type = NVMEIO_BAR01;
    test_data.offset = offset;
    test_data.nBytes = nBytes;
    test_data.acc_type = DWORD_LEN;
    test_data.buffer = NULL;

    test_data.buffer = malloc(test_data.nBytes);
    if (test_data.buffer == NULL)
    {
        pr_err("malloc failed!");
        goto err;
    }

    memcpy(test_data.buffer, byte_buffer, nBytes);
#if 0
    pr_info("NVME Writing DWORD at offset:0x%02x Val:0x%08x\n", test_data.offset, *(uint32_t *)test_data.buffer);
#endif
    ret_val = ioctl(file_desc, NVME_IOCTL_WRITE_GENERIC, &test_data);

    if (ret_val < 0)
    {
        pr_err("ioctl_set_msg failed:%d\n", ret_val);
        exit(-1);
    }

    free(test_data.buffer);
    return ret_val;
err:
    return -1;
}
/**
 * @brief ioctl_create_acq
 * 
 * @param file_desc 
 * @param queue_size 
 */
void ioctl_create_acq(int file_desc, uint32_t queue_size)
{
    int ret_val = -1;
    struct nvme_create_admn_q aq_data = {0};

    // check queue size
    if (queue_size > MAX_ADMIN_QUEUE_SIZE)
    {
        pr_err("\tadmin cq size \033[32mexceed\033[0m\n");
    }

    aq_data.elements = queue_size;
    aq_data.type = ADMIN_CQ;

    pr_debug("\tAdmin CQ No. of Elements = %d\n", aq_data.elements);

    ret_val = ioctl(file_desc, NVME_IOCTL_CREATE_ADMN_Q, &aq_data);
    if (ret_val < 0)
    {
        pr_err("\tCreation of ACQ Failed!  ret_val=%d\n", ret_val);
    }
    else
    {
        pr_debug("\tACQ Creation success\n");
    }
}
/**
 * @brief ioctl_create_asq
 * 
 * @param file_desc 
 * @param queue_size 
 */
void ioctl_create_asq(int file_desc, uint32_t queue_size)
{
    int ret_val = -1;
    struct nvme_create_admn_q aq_data = {0};

    // check queue size
    if (queue_size > MAX_ADMIN_QUEUE_SIZE)
    {
        pr_err("\tadmin sq size \033[32mexceed\033[0m\n");
    }

    aq_data.elements = queue_size;
    aq_data.type = ADMIN_SQ;

    pr_debug("\tAdmin SQ No. of Elements = %d\n", aq_data.elements);

    ret_val = ioctl(file_desc, NVME_IOCTL_CREATE_ADMN_Q, &aq_data);
    if (ret_val < 0)
    {
        pr_err("\tCreation of ASQ Failed! ret_val=%d\n", ret_val);
    }
    else
    {
        pr_debug("\tASQ Creation success\n");
    }
}
/**
 * @brief ioctl_enable_ctrl
 * 
 * @param file_desc 
 */
void ioctl_enable_ctrl(int file_desc)
{
    int ret_val = -1;
    enum nvme_state new_state = ST_ENABLE;

    ret_val = ioctl(file_desc, NVME_IOCTL_DEVICE_STATE, new_state);
    if (ret_val < 0)
    {
        pr_err("Enable Ctrlr: Failed!\n");
    }
    else
    {
        pr_debug("Enable Ctrlr: success\n");
    }
}
/**
 * @brief ioctl_disable_ctrl
 * 
 * @param file_desc 
 * @param new_state 
 */
int ioctl_disable_ctrl(int file_desc, enum nvme_state new_state)
{
    int ret_val = -1;

    ret_val = ioctl(file_desc, NVME_IOCTL_DEVICE_STATE, new_state);
    if (ret_val < 0)
    {
        pr_err("User Call to Disable Ctrlr: Failed!\n");
    }
    else
    {
        pr_debug("User Call to Disable Ctrlr: success\n");
    }
    return ret_val;
}

void ioctl_dump(int file_desc, char *tmpfile)
{
    int ret_val = -1;
    struct nvme_file pfile = {0};

    pfile.flen = strlen(tmpfile);
    //pr_info("size = %d\n", pfile.flen);
    pfile.file_name = malloc(pfile.flen);
    strcpy((char *)pfile.file_name, tmpfile);

    pr_info("File name = %s\n", pfile.file_name);

    ret_val = ioctl(file_desc, NVME_IOCTL_DUMP_METRICS, &pfile);
    if (ret_val < 0)
    {
        pr_err("Dump Metrics Failed!\n");
    }
    else
    {
        pr_debug("Dump Metrics success\n");
    }
}

int test_create_contig_iocq(int file_desc, uint16_t io_cq_id, uint16_t cq_size)
{
    return nvme_create_contig_iocq(file_desc, io_cq_id, cq_size, 1, 1);
}

int test_create_contig_iosq(int file_desc, uint16_t io_sq_id, uint16_t io_cq_id, uint16_t sq_size)
{
    return nvme_create_contig_iosq(file_desc, io_sq_id, io_cq_id, sq_size, 1);
}

int test_reap_cq(int file_desc, int cq_id, uint32_t cmd_cnt, int disp_flag)
{
    while (ioctl_reap_inquiry(file_desc, cq_id) != cmd_cnt)
        usleep(50);
    return (ioctl_reap_cq(file_desc, cq_id, cmd_cnt, 16, disp_flag));
}

struct cq_completion *send_get_feature(int file_desc, uint8_t feature_id)
{
    uint32_t reap_num = 0;
    //send command
    if (nvme_get_feature_cmd(file_desc, 1, feature_id))
    {
        pr_err("send get feature cmd Failed!\n");
    }
    // ring doorbell
    if (ioctl_tst_ring_dbl(file_desc, 0))
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
/**
 * @brief pci_read_dword, pcie layer
 * 
 * @param file_desc 
 * @param offset 
 * @return uint32_t 
 */
uint32_t pci_read_dword(int file_desc, uint32_t offset)
{
    int ret_val = FAILED;
    uint32_t data = 0;
    struct rw_generic test_data;
    if(offset%4 != 0)
        assert(0);
    test_data.type = NVMEIO_PCI_HDR;
    test_data.offset = offset;
    test_data.nBytes = sizeof(data);
    test_data.acc_type = DWORD_LEN;
    test_data.buffer = (uint8_t *)&data;
    ret_val = ioctl(file_desc, NVME_IOCTL_READ_GENERIC, &test_data);
    if (ret_val < 0)
    {
        pr_err("ioctl_set_msg failed:%d\n", ret_val);
        exit(-1);
    }
    return data;
}

/**
 * @brief pci_read_word, pcie layer
 * 
 * @param file_desc 
 * @param offset 
 * @return uint16_t 
 */
uint16_t pci_read_word(int file_desc, uint32_t offset)
{
    int ret_val = FAILED;
    uint16_t data = 0;
    struct rw_generic test_data;
    if(offset%2 != 0)
        assert(0);
    test_data.type = NVMEIO_PCI_HDR;
    test_data.offset = offset;
    test_data.nBytes = sizeof(data);
    test_data.acc_type = WORD_LEN;
    test_data.buffer = (uint8_t *)&data;
    ret_val = ioctl(file_desc, NVME_IOCTL_READ_GENERIC, &test_data);
    if (ret_val < 0)
    {
        pr_err("ioctl_set_msg failed:%d\n", ret_val);
        exit(-1);
    }
    return data;
}

/**
 * @brief pci_read_byte, pcie layer
 * 
 * @param file_desc 
 * @param offset 
 * @return uint8_t 
 */
uint8_t pci_read_byte(int file_desc, uint32_t offset)
{
    int ret_val = FAILED;
    uint8_t data = 0;
    struct rw_generic test_data;
    test_data.type = NVMEIO_PCI_HDR;
    test_data.offset = offset;
    test_data.nBytes = sizeof(data);
    test_data.acc_type = BYTE_LEN;
    test_data.buffer = (uint8_t *)&data;
    ret_val = ioctl(file_desc, NVME_IOCTL_READ_GENERIC, &test_data);
    if (ret_val < 0)
    {
        pr_err("ioctl_set_msg failed:%d\n", ret_val);
        exit(-1);
    }
    return data;
}

/*
 * @brief read_pcie_register layer
 * @param[in] file_desc  
 * @param[in] offset 
 * @param[in] nBytes 
 * @param[in] acc_type
 * @param[out] buffer output register data
 * @return depends on REQUEST. Usually -1 indicates error.
*/
int read_pcie_register(int file_desc, uint32_t offset, uint32_t nBytes, enum nvme_acc_type acc_type, uint8_t *byte_buffer)
{
    int ret_val = FAILED;
    struct rw_generic test_data = {0};

    test_data.type = NVMEIO_PCI_HDR;
    test_data.offset = offset;
    test_data.nBytes = nBytes;
    test_data.acc_type = acc_type;
    test_data.buffer = byte_buffer;
    ret_val = ioctl(file_desc, NVME_IOCTL_READ_GENERIC, &test_data);
    if (ret_val < 0)
    {
        pr_err("ioctl_set_msg failed:%d\n", ret_val);
        exit(-1);
    }
    return ret_val;
}

/**
 * @brief ioctl_pci_write_data, pcie layer
 * 
 * @param file_desc 
 * @param offset 
 * @param nBytes 
 * @param byte_buffer 
 * @return uint32_t 
 */
int ioctl_pci_write_data(int file_desc, uint32_t offset, uint32_t nBytes, uint8_t *byte_buffer)
{
    int ret_val = FAILED;
    struct rw_generic test_data = {0};

    test_data.type = NVMEIO_PCI_HDR;
    test_data.offset = offset;
    test_data.nBytes = nBytes;
    test_data.acc_type = DWORD_LEN;
    test_data.buffer = NULL;

    test_data.buffer = malloc(test_data.nBytes);
    if (test_data.buffer == NULL)
    {
        pr_err("malloc failed!");
        return FAILED;
    }
    memcpy(test_data.buffer, byte_buffer, nBytes);
#if 0
    pr_info("PCIe Writing DWORD at offset:0x%02x Val:0x%08x\n", test_data.offset, *(uint32_t *)test_data.buffer);
#endif
    ret_val = ioctl(file_desc, NVME_IOCTL_WRITE_GENERIC, &test_data);

    if (ret_val < 0)
    {
        pr_err("NVME_IOCTL_WRITE_GENERIC failed:%d\n", ret_val);
    }

    free(test_data.buffer);
    return ret_val;
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
            *((uint32_t *)write_buffer + k) = (((j) << 24) | ((j + 1) << 16) | ((j + 2) << 8) | ((j + 3) << 0));
            *((uint32_t *)write_buffer + 64 + k) = *((uint32_t *)write_buffer + k);
            j += 4;
            *((uint32_t *)write_buffer + k - 1) = (((j) << 24) | ((j + 1) << 16) | ((j + 2) << 8) | ((j + 3) << 0));
            *((uint32_t *)write_buffer + 64 + k - 1) = *((uint32_t *)write_buffer + k - 1);
            j += 4;
            *((uint32_t *)write_buffer + k - 2) = (((j) << 24) | ((j + 1) << 16) | ((j + 2) << 8) | ((j + 3) << 0));
            *((uint32_t *)write_buffer + 64 + k - 2) = *((uint32_t *)write_buffer + k - 2);
            j += 4;
            *((uint32_t *)write_buffer + k - 3) = (((j) << 24) | ((j + 1) << 16) | ((j + 2) << 8) | ((j + 3) << 0));
            *((uint32_t *)write_buffer + 64 + k - 3) = *((uint32_t *)write_buffer + k - 3);
            j += 4;
            //pr_info("k:%d,j:%d,i:%d,write_buffer[%d]:0x%08x\n",k,j,i,i,((uint32_t *)write_buffer)[i]);
        }
    // for(i=0; i<128; i++)
    // {
    //     pr_info("write_buffer[%d]:0x%08x\n",i,((uint32_t *)write_buffer)[i]);
    // }
    #else
    // //ECB-AES128/256
    // *((uint32_t *)write_buffer) = 0x7393172a;
    // *((uint32_t *)write_buffer+1) = 0xe93d7e11;
    // *((uint32_t *)write_buffer+2) = 0x2e409f96;
    // *((uint32_t *)write_buffer+3) = 0x6bc1bee2;

    // //SM4-ECB128
    // *((uint32_t *)write_buffer) = 0xd468cdc9;
    // *((uint32_t *)write_buffer+1) = 0xc2d02d76;
    // *((uint32_t *)write_buffer+2) = 0x598a2797;
    // *((uint32_t *)write_buffer+3) = 0x115960c5;
    #endif

    wr_slba = 0xff;
    wr_nlb = 1;
    for (i = 0; i < (wr_nlb * LBA_DAT_SIZE); i++)
    {
        // *(char *)(write_buffer + i) = 0;
        *(char *)(read_buffer + i) = 0;
    }
    cmd_cnt = 0;
    nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
    cmd_cnt++;
    ioctl_tst_ring_dbl(file_desc, io_sq_id);
    cq_gain(io_cq_id, cmd_cnt, &reap_num);
    cmd_cnt = 0;
    nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
    cmd_cnt++;
    ioctl_tst_ring_dbl(file_desc, io_sq_id);
    cq_gain(io_cq_id, cmd_cnt, &reap_num);

    err_flg = 0;
#if 0
    // // ECB-AES128.Encrypt
    // if(*((uint32_t *)read_buffer) != 0x2466ef97)
    //     err_flg = 1;
    // if(*((uint32_t *)read_buffer+1) != 0xa89ecaf3)
    //     err_flg = 1;
    // if(*((uint32_t *)read_buffer+2) != 0x0d7a3660)
    //     err_flg = 1;
    // if(*((uint32_t *)read_buffer+3) != 0x3ad77bb4)
    //     err_flg = 1;

    // //SM4-ECB128.Encrypt
    // if(*((uint32_t *)read_buffer) != 0x4dd7c50b)
    //     err_flg = 1;
    // if(*((uint32_t *)read_buffer+1) != 0x00ce6c9a)
    //     err_flg = 1;
    // if(*((uint32_t *)read_buffer+2) != 0x9b327396)
    //     err_flg = 1;
    // if(*((uint32_t *)read_buffer+3) != 0xa52485ce)
    //     err_flg = 1;

    // //ECB-AES256.Encrypt
    // if(*((uint32_t *)read_buffer) != 0x3db181f8)
    //     err_flg = 1;
    // if(*((uint32_t *)read_buffer+1) != 0x064b5a7e)
    //     err_flg = 1;
    // if(*((uint32_t *)read_buffer+2) != 0xb5d2a03c)
    //     err_flg = 1;
    // if(*((uint32_t *)read_buffer+3) != 0xf3eed1bd)
    //     err_flg = 1;

    // XTS-AES128.Encrypt
    // if (*((uint32_t *)read_buffer) != 0xd4cfa6e2)
    //     err_flg = 1;
    // if (*((uint32_t *)read_buffer + 1) != 0x489f308c)
    //     err_flg = 1;
    // if (*((uint32_t *)read_buffer + 2) != 0xefa1d476)
    //     err_flg = 1;
    // if (*((uint32_t *)read_buffer + 3) != 0x27a7479b)
    //     err_flg = 1;
    // if (*((uint32_t *)read_buffer + 124) != 0x319d0568)
    //     err_flg = 1;
    // if (*((uint32_t *)read_buffer + 125) != 0xbe421ee5)
    //     err_flg = 1;
    // if (*((uint32_t *)read_buffer + 126) != 0x20147bea)
    //     err_flg = 1;
    // if (*((uint32_t *)read_buffer + 127) != 0x0a282df9)
    //     err_flg = 1;

    // XTS-AES256.Encrypt
    if (*((uint32_t *)read_buffer) != 0xe370cf9b)
        err_flg = 1;
    if (*((uint32_t *)read_buffer + 1) != 0xe4836c99)
        err_flg = 1;
    if (*((uint32_t *)read_buffer + 2) != 0x2f770386)
        err_flg = 1;
    if (*((uint32_t *)read_buffer + 3) != 0x1c3b3a10)
        err_flg = 1;
    if (*((uint32_t *)read_buffer + 124) != 0xe148c151)
        err_flg = 1;
    if (*((uint32_t *)read_buffer + 125) != 0xb9c6e693)
        err_flg = 1;
    if (*((uint32_t *)read_buffer + 126) != 0xa9fcea70)
        err_flg = 1;
    if (*((uint32_t *)read_buffer + 127) != 0xc4f36ffd)
        err_flg = 1;

#else
    if (FAILED == dw_cmp(write_buffer, read_buffer, wr_nlb * LBA_DAT_SIZE))
    {
        pr_info("\nwrite_buffer Data:\n");
        mem_disp(write_buffer, LBA_DAT_SIZE);
        pr_info("\nRead_buffer Data:\n");
        mem_disp(read_buffer, LBA_DAT_SIZE);
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
