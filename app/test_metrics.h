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

#include "auto_header.h"
#include "reg_nvme_ctrl.h"
#include "dnvme_ioctl.h"

// #define AMD_MB_EN //Warning: AMD MB may not support msi-multi, create by shell script in auto_heater.h
// #define FWDMA_RST_OPEN

#define DEFAULT_IO_QUEUE_SIZE (65535)
#define NSIDX(n) (n - 1)

#define SAMSUNG_CTRL_VID (0x144d)

#define MAXIO_CTRL_VID (0x1E4B)
#define MAXIO_CTRL_SSVID (0x1E4B)

#define IDENTIFY_DATA_SIZE (4096)
#define RW_BUFFER_SIZE (2 * 1024 * 1024)
#define LBA_DAT_SIZE (512)

#define DISCONTIG_IO_SQ_SIZE (1023 * 4096)
#define DISCONTIG_IO_CQ_SIZE (255 * 4096)

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

struct nvme_ctrl
{
	uint32_t max_sq_num; // 1'base
	uint32_t max_cq_num; // 1'base
	uint32_t link_speed; //
	uint32_t link_width; //
	uint32_t irq_type;   //
	uint8_t pmcap_ofst;
	uint8_t msicap_ofst;
	uint8_t pxcap_ofst;

	struct nvme_id_ctrl id_ctrl;
	struct reg_nvme_ctrl ctrl_reg;
};

struct nvme_ns
{
	uint32_t lbads;
	uint64_t nsze;
	struct nvme_id_ns id_ns;
};

struct nvme_sq_info
{
	uint16_t sq_id;
	uint16_t cq_id;
	uint16_t cq_int_vct;
	uint16_t cmd_cnt;
	uint32_t cq_size;
	uint32_t sq_size;
};

struct nvme_cq_info
{
    uint16_t sq_id;
    uint16_t cq_id;
};

#pragma pack(pop)

/* nvme register definition offset*/
#define NVME_REG_CAP_OFST 0x00
#define NVME_REG_CAP_OFST_H 0x04
#define NVME_REG_VS_OFST 0x08
#define NVME_REG_INTMS_OFST 0x0C
#define NVME_REG_INTMC_OFST 0x10
#define NVME_REG_CC_OFST 0x14
#define NVME_REG_CC_OFST_H 0x18
#define NVME_REG_CSTS_OFST 0x1C
#define NVME_REG_NSSR_OFST 0x20
#define NVME_REG_AQA_OFST 0x24
#define NVME_REG_ASQ_OFST 0x28
#define NVME_REG_ASQ_OFST_H 0x2C
#define NVME_REG_ACQ_OFST 0x30
#define NVME_REG_ACQ_OFST_H 0x34
#define NVME_REG_CMBLOC_OFST 0x38
#define NVME_REG_CMBSZ_OFST 0x3C
#define NVME_REG_BPINFO_OFST 0x40
#define NVME_REG_BPRSEL_OFST 0x44
#define NVME_REG_BPMBL_OFST 0x48

#define PCI_PMCAP_ID 0x01
#define PCI_MSICAP_ID 0x05
#define PCI_PXCAP_ID 0x10

//--------------------------------------------------------------------------------

int ioctl_delete_ioq(int g_fd, uint8_t opcode, uint16_t qid);

int ioctl_send_nvme_write(int g_fd, uint16_t sq_id, uint64_t slba, uint16_t nlb,
                          enum fua_sts fua_sts, void *addr, uint32_t buf_size);
int ioctl_send_nvme_read(int g_fd, uint16_t sq_id, uint64_t slba, uint16_t nlb,
                         enum fua_sts fua_sts, void *addr, uint32_t buf_size);
int ioctl_send_nvme_compare(int g_fd, uint16_t sq_id, uint64_t slba, uint16_t nlb,
                            enum fua_sts fua_sts, void *addr, uint32_t buf_size);

int ioctl_reap_cq(int g_fd, int cq_id, int elements, int size, int display);

void test_meta(int g_fd);
uint32_t create_meta_buf(int g_fd, uint32_t id);

int display_cq_data(unsigned char *cq_buffer, int reap_ele, int display);
void admin_queue_config(int g_fd);
void test_irq_review568(int fd);

uint32_t crc32_mpeg_2(uint8_t *data, uint32_t length);

int test_create_contig_iocq(int g_fd, uint16_t io_cq_id, uint16_t cq_size);
int test_create_contig_iosq(int g_fd, uint16_t io_sq_id, uint16_t io_cq_id, uint16_t sq_size);
int test_reap_cq(int g_fd, int cq_id, uint32_t cmd_cnt, int disp_flag);

struct nvme_completion *send_get_feature(int g_fd, uint8_t feature_id);

int create_iocq(int g_fd, struct create_cq_parameter *cq_parameter);
int create_iosq(int g_fd, struct create_sq_parameter *sq_parameter);
int keep_alive_cmd(int g_fd);
int admin_illegal_opcode_cmd(int g_fd, uint8_t opcode);

int nvme_maxio_fwdma_rd(int g_fd, struct fwdma_parameter *fwdma_parameter);
int nvme_maxio_fwdma_wr(int g_fd, struct fwdma_parameter *fwdma_parameter);

int ioctl_send_abort(int g_fd, uint16_t sq_id, uint16_t cmd_id);
int ioctl_send_flush(int g_fd, uint16_t sq_id);
int ioctl_send_write_zero(int g_fd, uint16_t sq_id, uint64_t slba, uint16_t nlb, uint16_t control);
int ioctl_send_write_unc(int g_fd, uint16_t sq_id, uint64_t slba, uint16_t nlb);
int ioctl_send_format(int g_fd, uint8_t lbaf);

uint8_t pci_find_cap_ofst(int g_fd, uint8_t cap_id);

int ctrl_pci_flr(void);
int set_power_state(uint8_t pmcap, uint8_t dstate);

void pcie_retrain_link(void);
void pcie_RC_cfg_speed(int speed);
void pcie_set_width(int width);
void pcie_random_speed_width(void);
uint32_t pcie_link_down(void);
uint32_t pcie_hot_reset(void);

void test_encrypt_decrypt(void);

extern int g_fd;
extern void *g_cq_entry_buf;
extern void *g_read_buf;
extern void *g_write_buf;
extern void *g_discontig_sq_buf;
extern void *g_discontig_cq_buf;
extern struct nvme_ctrl g_nvme_dev;
extern struct nvme_sq_info *ctrl_sq_info;
extern struct nvme_ns *g_nvme_ns_info;

#define TEST_PASS "\nppppppppppp     aaaaaaaaaaa     sssssssssss     sssssssssss \n" \
                  "pp       pp     aa       aa     ss              ss          \n"   \
                  "pp       pp     aa       aa     ss              ss          \n"   \
                  "pp       pp     aa       aa     ss              ss          \n"   \
                  "ppppppppppp     aaaaaaaaaaa     sssssssssss     sssssssssss \n"   \
                  "pp              aa       aa              ss              ss \n"   \
                  "pp              aa       aa              ss              ss \n"   \
                  "pp              aa       aa              ss              ss \n"   \
                  "pp              aa       aa     sssssssssss     sssssssssss \n"

#define TEST_FAIL "\nfffffffffff     aaaaaaaaaaa     iiiiiiiiiii     ll          \n" \
                  "ff              aa       aa         iii         ll          \n"   \
                  "ff              aa       aa         iii         ll          \n"   \
                  "ff              aa       aa         iii         ll          \n"   \
                  "fffffffffff     aaaaaaaaaaa         iii         ll          \n"   \
                  "ff              aa       aa         iii         ll          \n"   \
                  "ff              aa       aa         iii         ll          \n"   \
                  "ff              aa       aa         iii         ll          \n"   \
                  "ff              aa       aa     iiiiiiiiiii     lllllllllll \n"

#endif
