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

#define ADMIN_QUEUE_ID (0)
#define MAX_ADMIN_QUEUE_SIZE (4096)
#define DEFAULT_IO_QUEUE_SIZE (65535)
#define NSIDX(n) (n - 1)

#define SAMSUNG_CTRL_VID (0x144d)

#define MAXIO_CTRL_VID (0x1E4B)
#define MAXIO_CTRL_SSVID (0x1E4B)

#define IDENTIFY_DATA_SIZE (4096)
#define RW_BUFFER_SIZE (2 * 1024 * 1024)
#define LBA_DAT_SIZE (512)

// #define RW_BUF_4K_ALN_EN

#define DISCONTIG_IO_SQ_SIZE (1023 * 4096)
#define DISCONTIG_IO_CQ_SIZE (255 * 4096)

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

#pragma pack(push)
#pragma pack(4)

//--------------------------------------------------------------------------------
struct cq_completion
{
    uint32_t cmd_specifc;       /* DW 0 all 32 bits     */
    uint32_t reserved;          /* DW 1 all 32 bits     */
    uint16_t sq_head_ptr;       /* DW 2 lower 16 bits   */
    uint16_t sq_identifier;     /* DW 2 higher 16 bits  */
    uint16_t cmd_identifier;    /* Cmd identifier       */
    uint8_t phase_bit : 1;      /* Phase bit            */
    uint16_t status_field : 15; /* Status field         */
};

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
	dword_t max_sq_num; // 1'base
	dword_t max_cq_num; // 1'base
	dword_t link_speed; //
	dword_t link_width; //
	dword_t irq_type;   //
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

void ioctl_get_q_metrics(int file_desc, int q_id, int q_type, int size);
void test_drv_metrics(int file_desc);
int ioctl_prep_sq(int file_desc, uint16_t sq_id, uint16_t cq_id, uint16_t elem, uint8_t contig);
int ioctl_prep_cq(int file_desc, uint16_t cq_id, uint16_t elem, uint8_t contig);
int ioctl_tst_ring_dbl(int file_desc, int sq_id);

int ioctl_delete_ioq(int file_desc, uint8_t opcode, uint16_t qid);

int ioctl_send_nvme_write(int file_desc, uint16_t sq_id, uint64_t slba, uint16_t nlb,
                          enum fua_sts fua_sts, void *addr, uint32_t buf_size);
int ioctl_send_nvme_read(int file_desc, uint16_t sq_id, uint64_t slba, uint16_t nlb,
                         enum fua_sts fua_sts, void *addr, uint32_t buf_size);
int ioctl_send_nvme_compare(int file_desc, uint16_t sq_id, uint64_t slba, uint16_t nlb,
                            enum fua_sts fua_sts, void *addr, uint32_t buf_size);

uint32_t ioctl_reap_inquiry(int file_desc, int cq_id);
int ioctl_reap_cq(int file_desc, int cq_id, int elements, int size, int display);
void ioctl_enable_ctrl(int file_desc);
int ioctl_disable_ctrl(int file_desc, enum nvme_state new_state);

void ioctl_create_acq(int file_desc, uint32_t queue_size);
void ioctl_create_asq(int file_desc, uint32_t queue_size);
void test_meta(int file_desc);
uint32_t create_meta_buf(int file_desc, uint32_t id);
int ioctl_meta_buf_delete(int file_desc, uint32_t id);

void ioctl_dump(int file_desc, char *tmpfile);
int display_cq_data(unsigned char *cq_buffer, int reap_ele, int display);
void admin_queue_config(int file_desc);
void test_irq_review568(int fd);
void test_dev_metrics(int file_desc);

uint32_t ioctl_read_data(int file_desc, uint32_t offset, uint32_t bytes);
int read_nvme_register(int file_desc, uint32_t offset, uint32_t bytes, uint8_t *byte_buffer);
int ioctl_write_data(int file_desc, uint32_t offset, uint32_t bytes, uint8_t *byte_buffer);

uint32_t crc32_mpeg_2(uint8_t *data, uint32_t length);

int test_create_contig_iocq(int file_desc, uint16_t io_cq_id, uint16_t cq_size);
int test_create_contig_iosq(int file_desc, uint16_t io_sq_id, uint16_t io_cq_id, uint16_t sq_size);
int test_reap_cq(int file_desc, int cq_id, uint32_t cmd_cnt, int disp_flag);

struct cq_completion *send_get_feature(int file_desc, uint8_t feature_id);

int create_iocq(int file_desc, struct create_cq_parameter *cq_parameter);
int create_iosq(int file_desc, struct create_sq_parameter *sq_parameter);
int keep_alive_cmd(int file_desc);
int admin_illegal_opcode_cmd(int file_desc, uint8_t opcode);

int nvme_maxio_fwdma_rd(int file_desc, struct fwdma_parameter *fwdma_parameter);
int nvme_maxio_fwdma_wr(int file_desc, struct fwdma_parameter *fwdma_parameter);

int ioctl_send_abort(int file_desc, uint16_t sq_id, uint16_t cmd_id);
int ioctl_send_flush(int file_desc, uint16_t sq_id);
int ioctl_send_write_zero(int file_desc, uint16_t sq_id, uint64_t slba, uint16_t nlb, uint16_t control);
int ioctl_send_write_unc(int file_desc, uint16_t sq_id, uint64_t slba, uint16_t nlb);
int ioctl_send_format(int file_desc, uint8_t lbaf);

uint32_t pci_read_dword(int file_desc, uint32_t offset);
uint16_t pci_read_word(int file_desc, uint32_t offset);
uint8_t pci_read_byte(int file_desc, uint32_t offset);

int read_pcie_register(int file_desc, uint32_t offset, uint32_t bytes, enum nvme_access_type acc_type, uint8_t *byte_buffer);
int ioctl_pci_write_data(int file_desc, uint32_t offset, uint32_t bytes, uint8_t *byte_buffer);

uint8_t pci_find_cap_ofst(int file_desc, uint8_t cap_id);

int subsys_reset(void);
int ctrl_disable(void);
int ctrl_pci_flr(void);
int set_power_state(uint8_t pmcap, uint8_t dstate);

void pcie_retrain_link(void);
void pcie_RC_cfg_speed(int speed);
void pcie_set_width(int width);
void pcie_random_speed_width(void);
dword_t pcie_link_down(void);
dword_t pcie_hot_reset(void);

void test_encrypt_decrypt(void);

extern int file_desc;
extern void *read_buffer;
extern void *write_buffer;
extern void *discontg_sq_buf;
extern void *discontg_cq_buf;
extern struct nvme_ctrl g_nvme_dev;
extern struct nvme_sq_info *ctrl_sq_info;
extern struct nvme_ns *g_nvme_ns_info;

#define DISP_HELP "\n******  Maxio dnvme unittest test  ******\n"                 \
                  "\033[32mtest case list:\033[0m\n"                              \
                  "case  0: Display this help\n"                                  \
                  "case  1: Disabling the controller completely\n"                \
                  "case  2: Initializing the device to Run tests.\n"              \
                  "case  3: Create discontiguous IOSQ and IOCQ \n"                \
                  "case  4: Create contiguous IOSQ and IOCQ \n"                   \
                  "case  5: Delete IOSQ && IOCQ which is create in case 4\n"      \
                  "case  6: Send IO Write cmd which create in case 4\n"           \
                  "case  7: Send IO Read cmd which create in case 4\n"            \
                  "case  8: Send IO Compare cmd which process in case 6 case 7\n" \
                  "case  9: Disp Write_buffer Read_buffer Data\n"                 \
                  "case 10: Compare Write_buffer Read_buffer Data\n"              \
                  "case 20: maxio_cmd_fwdma_write\n"                              \
                  "case 21: maxio_cmd_fwdma_read\n"                               \
                  "case 29: hc peak power test\n"                                 \
                  "case 30: case_queue_create_q_size\n"                           \
                  "case 31: case_queue_delete_q\n"                                \
                  "case 32: case_queue_sq_cq_match\n"                             \
                  "case 33: case_iocmd_write_read\n"                              \
                  "case 36: case_queue_abort\n"                                   \
                  "case 37: case_fw_io_cmd\n"                                     \
                  "case 40: case_queue_cq_int_all\n"                              \
                  "case 41: case_queue_cq_int_all mask\n"                         \
                  "case 42: case_queue_cq_int_msi_multi_mask\n"                   \
                  "case 43: case_queue_cq_int_msix mask\n"                        \
                  "case 47: case_queue_cq_int_coalescing\n"                       \
                  "case 48: case_queue_command_arbitration\n"                     \
                  "case 49: case_resets_link_down test\n"                         \
                  "case 50: case_resets_random_all test\n"                        \
                  "case 52: case_queue_admin\n"                                   \
                  "case 53: case_nvme_boot_partition\n"                           \
                  "case 54: register test\n"                                      \
                  "case 55: test_0_full_disk_wr test\n"                           \
                  "case 56: test_1_fused loop test\n"                             \
                  "case 57: test_2_mix_case\n"                                    \
                  "case 58: test_3_adm_wr_cache_fua test\n"                       \
                  "case 59: test_5_fua_wr_rd_cmp test\n"                          \
                  "case 60: test_6_all_ns_lbads_test\n"                           \
                  "case 78: set LTR disable\n"                                    \
                  "case 78: set to D0 state\n"                                    \
                  "case 79: set to D3 state\n"                                    \
                  "case 80: pcie_link_speed_width step\n"                         \
                  "case 81: pcie_link_speed_width cyc\n"                          \
                  "case 82: pcie_reset single\n"                                  \
                  "case 83: pcie_reset cycle\n"                                   \
                  "case 84: pcie_low_power L0s&L1 step\n"                         \
                  "case 85: pcie_low_power L0s&L1 cycle\n"                        \
                  "case 86: pcie_low_power L1sub step\n"                          \
                  "case 87: pcie_PM\n"                                            \
                  "case 88: pcie_low_power_measure\n"                             \
                  "case 89: pcie_low_power_pcipm_l1sub\n"                         \
                  "case 90: pcie_MPS\n"                                           \
                  "case 91: pcie_MRRS\n"                                          \
                  "case255: test case list exe\n"

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
