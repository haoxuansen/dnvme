/**
 * @file test_send_cmd.h
 * @author yumeng (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2019-10-08
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef _TEST_SEND_CMD_H_
#define _TEST_SEND_CMD_H_
#include <linux/types.h>

typedef struct _ADMIN_FORMAT_COMMAND_DW10
{
    uint32_t LBAF : 4;
    uint32_t MSET : 1;
    uint32_t PI : 3;
    uint32_t PIL : 1;
    uint32_t SES : 3;
    uint32_t Reserved : 20;
} ADMIN_FORMAT_COMMAND_DW10, *PADMIN_FORMAT_COMMAND_DW10;

enum power_states_
{
    D0 = 0,
    D1 = 1,
    D2 = 2,
    D3hot = 3,
};

struct pcie_msi_cap
{
    __u8 cap_id;
    __u8 nxt_cap;
    __u16 msg_ctrl;
    __u32 msg_adrl;
    __u32 msg_adrh;
    __u16 msg_dat;
    __u16 msg_dat_ext;
    __u32 mask_bit;
    __u32 pending_bit;
};

//########################################################################################
// new unittest arch
//########################################################################################

#define LBA_DATA_SIZE(nsid) (g_nvme_ns_info[nsid - 1].lbads)

int nvme_io_write_cmd(int g_fd, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                      uint16_t control, void *data_addr);

int nvme_io_read_cmd(int g_fd, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                     uint16_t control, void *data_addr);

int nvme_io_compare_cmd(int g_fd, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                     uint16_t control, void *data_addr);

int send_nvme_write_using_metabuff(int g_fd, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                     uint16_t control, uint32_t id, void *data_addr);
int send_nvme_read_using_metabuff(int g_fd, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                     uint16_t control, uint32_t id, void *data_addr);

int nvme_ring_dbl_and_reap_cq(int g_fd, uint16_t sq_id, uint16_t cq_id, uint32_t expect_num);

int nvme_idfy_ctrl(int g_fd, void *data);
int nvme_idfy_ns(int fd, uint32_t nsid, uint8_t present, void *data);
int nvme_idfy_ns_list(int fd, uint32_t nsid, uint8_t all, void *data);
int nvme_idfy_ctrl_list(int fd, uint32_t nsid, uint16_t cntid, void *data);
int nvme_idfy_secondary_ctrl_list(int fd, uint32_t nsid, uint16_t cntid, void *data);
int nvme_idfy_ns_descs(int fd, uint32_t nsid, void *data);
int nvme_set_feature_cmd(int g_fd, uint32_t nsid, uint8_t feat_id, uint16_t dw11l, uint16_t dw11h);
int nvme_set_feature_hmb_cmd(int g_fd, uint32_t nsid, uint16_t ehm, uint32_t hsize,
                             uint32_t hmdlla, uint32_t hmdlua, uint32_t hmdlec);
int nvme_get_feature_cmd(int g_fd, uint32_t nsid, uint8_t feat_id);

int nvme_admin_ring_dbl_reap_cq(int g_fd);
int nvme_create_contig_iocq(int g_fd, uint16_t cq_id, uint32_t cq_size, uint8_t irq_en, uint16_t irq_no);
int nvme_create_discontig_iocq(int g_fd, uint16_t cq_id, uint32_t cq_size, uint8_t irq_en, uint16_t irq_no,
                               uint8_t *g_discontig_cq_buf, uint32_t discontig_cq_size);
int nvme_create_contig_iosq(int g_fd, uint16_t sq_id, uint16_t cq_id, uint32_t sq_size, uint8_t sq_prio);
int nvme_create_discontig_iosq(int g_fd, uint16_t sq_id, uint16_t cq_id, uint32_t sq_size, uint8_t sq_prio,
                               uint8_t *g_discontig_sq_buf, uint32_t discontig_sq_size);
int nvme_delete_ioq(int g_fd, uint8_t opcode, uint16_t qid);

int nvme_firmware_commit(int g_fd, uint8_t bpid, uint8_t ca, uint8_t fs);
int nvme_firmware_download(int g_fd, uint32_t numd, uint32_t ofst, uint8_t *addr);

uint32_t nvme_msi_register_test(void);
uint32_t create_all_io_queue(uint8_t flags);
uint32_t delete_all_io_queue(void);
int nvme_send_iocmd(int g_fd, uint8_t cmp_dis, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb, void *data_addr);


#endif
