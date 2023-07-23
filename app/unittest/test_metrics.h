/**
 * @file test_metrics.h
 * @author yumeng (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2019-10-08
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef _TEST_METRICS_H_
#define _TEST_METRICS_H_

#include "dnvme.h"
#include "libnvme.h"

#define DEFAULT_IO_QUEUE_SIZE (65535)
#define NSIDX(n) (n - 1)

#define LBA_DAT_SIZE (512)

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

#pragma pack(push)
#pragma pack(4)

//--------------------------------------------------------------------------------
struct create_cq_parameter
{
    uint16_t cq_id;
    uint16_t cq_size;
    uint8_t contig;
    uint8_t irq_en;
    uint16_t irq_no;
};

struct create_sq_parameter
{
    uint16_t sq_id;
    uint16_t cq_id;
    uint16_t sq_size;
    uint8_t contig;
    enum nvme_sq_prio sq_prio;
};

enum fua_sts
{
    FUA_DISABLE,
    FUA_ENABLE
};

struct fwdma_parameter
{
    uint8_t *addr;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
};

#pragma pack(pop)

//--------------------------------------------------------------------------------

int ioctl_delete_ioq(int g_fd, uint8_t opcode, uint16_t qid);

int ioctl_send_nvme_write(int g_fd, uint16_t sq_id, uint64_t slba, uint16_t nlb,
                          enum fua_sts fua_sts, void *addr, uint32_t buf_size);
int ioctl_send_nvme_read(int g_fd, uint16_t sq_id, uint64_t slba, uint16_t nlb,
                         enum fua_sts fua_sts, void *addr, uint32_t buf_size);
int ioctl_send_nvme_compare(int g_fd, uint16_t sq_id, uint64_t slba, uint16_t nlb,
                            enum fua_sts fua_sts, void *addr, uint32_t buf_size);

int ioctl_reap_cq(int g_fd, int cq_id, int elements, int size, int display);

int display_cq_data(unsigned char *cq_buffer, int reap_ele, int display);

uint32_t crc32_mpeg_2(uint8_t *data, uint32_t length);

int test_create_contig_iocq(int g_fd, uint16_t io_cq_id, uint16_t cq_size);
int test_create_contig_iosq(int g_fd, uint16_t io_sq_id, uint16_t io_cq_id, uint16_t sq_size);
int test_reap_cq(int g_fd, int cq_id, uint32_t cmd_cnt, int disp_flag);

struct nvme_completion *send_get_feature(int g_fd, uint8_t feature_id);

int create_iocq(int g_fd, struct create_cq_parameter *cq_parameter);
int create_iosq(int g_fd, struct create_sq_parameter *sq_parameter);
int admin_illegal_opcode_cmd(int g_fd, uint8_t opcode);

int nvme_maxio_fwdma_rd(int g_fd, struct fwdma_parameter *fwdma_parameter);
int nvme_maxio_fwdma_wr(int g_fd, struct fwdma_parameter *fwdma_parameter);

int ioctl_send_abort(int g_fd, uint16_t sq_id, uint16_t cmd_id);
int ioctl_send_flush(int g_fd, uint16_t sq_id);
int ioctl_send_write_zero(int g_fd, uint16_t sq_id, uint64_t slba, uint16_t nlb, uint16_t control);
int ioctl_send_write_unc(int g_fd, uint16_t sq_id, uint64_t slba, uint16_t nlb);
int ioctl_send_format(int g_fd, uint8_t lbaf);

uint8_t pci_find_cap_ofst(int g_fd, uint8_t cap_id);

void pcie_retrain_link(void);
void pcie_RC_cfg_speed(int speed);
void pcie_set_width(int width);
void pcie_random_speed_width(void);

void test_encrypt_decrypt(void);

#endif
