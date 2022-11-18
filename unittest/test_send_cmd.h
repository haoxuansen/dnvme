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

// struct nvme_smart_log
// {
//     __u8 critical_warning;
//     __u8 temperature[2];
//     __u8 avail_spare;
//     __u8 spare_thresh;
//     __u8 percent_used;
//     __u8 endu_grp_crit_warn_sumry;
//     __u8 rsvd7[25];
//     __u8 data_units_read[16];
//     __u8 data_units_written[16];
//     __u8 host_reads[16];
//     __u8 host_writes[16];
//     __u8 ctrl_busy_time[16];
//     __u8 power_cycles[16];
//     __u8 power_on_hours[16];
//     __u8 unsafe_shutdowns[16];
//     __u8 media_errors[16];
//     __u8 num_err_log_entries[16];
//     __le32 warning_temp_time;
//     __le32 critical_comp_time;
//     __le16 temp_sensor[8];
//     __le32 thm_temp1_trans_count;
//     __le32 thm_temp2_trans_count;
//     __le32 thm_temp1_total_time;
//     __le32 thm_temp2_total_time;
//     __u8 rsvd232[280];
// };

/*
tips: opcode low 2bit Indicates the data transfer direction of the command:
      00b = no data transfer; 01b = host to controller; 10b = controller to host; 11b = bidirectional.
*/
// enum nvme_admin_opcode
// {
//     nvme_admin_delete_sq = 0x00,
//     nvme_admin_create_sq = 0x01,
//     nvme_admin_get_log_page = 0x02,
//     nvme_admin_delete_cq = 0x04,
//     nvme_admin_create_cq = 0x05,
//     nvme_admin_identify = 0x06,
//     nvme_admin_abort_cmd = 0x08,
//     nvme_admin_set_features = 0x09,
//     nvme_admin_get_features = 0x0a,
//     nvme_admin_async_event = 0x0c,
//     nvme_admin_ns_mgmt = 0x0d,
//     nvme_admin_activate_fw = 0x10, //firmware_commit
//     nvme_admin_download_fw = 0x11, //firmware_image_download
//     nvme_admin_dev_self_test = 0x14,
//     nvme_admin_ns_attach = 0x15,
//     nvme_admin_keep_alive = 0x18,
//     nvme_admin_directive_send = 0x19,
//     nvme_admin_directive_recv = 0x1a,
//     nvme_admin_virtual_mgmt = 0x1c,
//     nvme_admin_nvme_mi_send = 0x1d,
//     nvme_admin_nvme_mi_recv = 0x1e,
//     nvme_admin_dbbuf = 0x7C,
//     nvme_admin_format_nvm = 0x80,
//     nvme_admin_security_send = 0x81,
//     nvme_admin_security_recv = 0x82,
//     nvme_admin_sanitize_nvm = 0x84,
//     nvme_admin_get_lba_status = 0x86,

//     nvme_admin_vendor_write = 0xc1,
//     nvme_admin_vendor_read = 0xc2,
//     nvme_admin_vendor_para_set = 0xc5,
//     nvme_admin_fwdma_write_test = 0xc3,
//     nvme_admin_fwdma_read_test = 0xc4,
// };

/* I/O commands */
// enum nvme_opcode
// {
//     nvme_cmd_flush = 0x00,
//     nvme_cmd_write = 0x01,
//     nvme_cmd_read = 0x02,
//     nvme_cmd_write_uncor = 0x04,
//     nvme_cmd_compare = 0x05,
//     nvme_cmd_write_zeroes = 0x08,
//     nvme_cmd_dsm = 0x09,
//     nvme_cmd_verify = 0x0c,
//     nvme_cmd_resv_register = 0x0d,
//     nvme_cmd_resv_report = 0x0e,
//     nvme_cmd_resv_acquire = 0x11,
//     nvme_cmd_resv_release = 0x15,
// };

// enum
// {
//     NVME_ID_CNS_NS = 0x00,
//     NVME_ID_CNS_CTRL = 0x01,
//     NVME_ID_CNS_NS_ACTIVE_LIST = 0x02,
//     NVME_ID_CNS_NS_DESC_LIST = 0x03,
//     NVME_ID_CNS_NVMSET_LIST = 0x04,
//     NVME_ID_CNS_NS_PRESENT_LIST = 0x10,
//     NVME_ID_CNS_NS_PRESENT = 0x11,
//     NVME_ID_CNS_CTRL_NS_LIST = 0x12,
//     NVME_ID_CNS_CTRL_LIST = 0x13,
//     NVME_ID_CNS_SCNDRY_CTRL_LIST = 0x15,
//     NVME_ID_CNS_NS_GRANULARITY = 0x16,
//     NVME_ID_CNS_UUID_LIST = 0x17,
// };

// enum
// {
//     NVME_QUEUE_PHYS_CONTIG = (1 << 0),
//     NVME_CQ_IRQ_ENABLED = (1 << 1),
//     NVME_SQ_PRIO_URGENT = (0 << 1),
//     NVME_SQ_PRIO_HIGH = (1 << 1),
//     NVME_SQ_PRIO_MEDIUM = (2 << 1),
//     NVME_SQ_PRIO_LOW = (3 << 1),

//     NVME_FEAT_ARBITRATION = 0x01,
//     NVME_FEAT_POWER_MGMT = 0x02,
//     NVME_FEAT_LBA_RANGE = 0x03,
//     NVME_FEAT_TEMP_THRESH = 0x04,
//     NVME_FEAT_ERR_RECOVERY = 0x05,
//     NVME_FEAT_VOLATILE_WC = 0x06,
//     NVME_FEAT_NUM_QUEUES = 0x07,
//     NVME_FEAT_IRQ_COALESCE = 0x08,
//     NVME_FEAT_IRQ_CONFIG = 0x09,
//     NVME_FEAT_WRITE_ATOMIC = 0x0a,
//     NVME_FEAT_ASYNC_EVENT = 0x0b,
//     NVME_FEAT_AUTO_PST = 0x0c,
//     NVME_FEAT_HOST_MEM_BUF = 0x0d,
//     NVME_FEAT_TIMESTAMP = 0x0e,
//     NVME_FEAT_KATO = 0x0f,
//     NVME_FEAT_HCTM = 0x10,
//     NVME_FEAT_NOPSC = 0x11,
//     NVME_FEAT_RRL = 0x12,
//     NVME_FEAT_PLM_CONFIG = 0x13,
//     NVME_FEAT_PLM_WINDOW = 0x14,
//     NVME_FEAT_HOST_BEHAVIOR = 0x16,
//     NVME_FEAT_SANITIZE = 0x17,
//     NVME_FEAT_SW_PROGRESS = 0x80,
//     NVME_FEAT_HOST_ID = 0x81,
//     NVME_FEAT_RESV_MASK = 0x82,
//     NVME_FEAT_RESV_PERSIST = 0x83,
//     NVME_FEAT_WRITE_PROTECT = 0x84,
//     NVME_FEAT_VENDOR_START = 0xC0,
//     NVME_FEAT_VENDOR_END = 0xFF,

//     NVME_LOG_ERROR = 0x01,
//     NVME_LOG_SMART = 0x02,
//     NVME_LOG_FW_SLOT = 0x03,
//     NVME_LOG_CHANGED_NS = 0x04,
//     NVME_LOG_CMD_EFFECTS = 0x05,
//     NVME_LOG_DEVICE_SELF_TEST = 0x06,
//     NVME_LOG_TELEMETRY_HOST = 0x07,
//     NVME_LOG_TELEMETRY_CTRL = 0x08,
//     NVME_LOG_ENDURANCE_GROUP = 0x09,
//     NVME_LOG_ANA = 0x0c,
//     NVME_LOG_DISC = 0x70,
//     NVME_LOG_RESERVATION = 0x80,

//     NVME_FWACT_REPL = (0 << 3),
//     NVME_FWACT_REPL_ACTV = (1 << 3),
//     NVME_FWACT_ACTV = (2 << 3),
// };

// enum
// {
//     NVME_RW_LR = 1 << 15,
//     NVME_RW_FUA = 1 << 14,
//     NVME_RW_APPEND_PIREMAP = 1 << 9,

//     NVME_RW_DSM_FREQ_UNSPEC = 0,
//     NVME_RW_DSM_FREQ_TYPICAL = 1,
//     NVME_RW_DSM_FREQ_RARE = 2,
//     NVME_RW_DSM_FREQ_READS = 3,
//     NVME_RW_DSM_FREQ_WRITES = 4,
//     NVME_RW_DSM_FREQ_RW = 5,
//     NVME_RW_DSM_FREQ_ONCE = 6,
//     NVME_RW_DSM_FREQ_PREFETCH = 7,
//     NVME_RW_DSM_FREQ_TEMP = 8,
//     NVME_RW_DSM_LATENCY_NONE = 0 << 4,
//     NVME_RW_DSM_LATENCY_IDLE = 1 << 4,
//     NVME_RW_DSM_LATENCY_NORM = 2 << 4,
//     NVME_RW_DSM_LATENCY_LOW = 3 << 4,
//     NVME_RW_DSM_SEQ_REQ = 1 << 6,
//     NVME_RW_DSM_COMPRESSED = 1 << 7,

//     NVME_RW_PRINFO_PRCHK_REF = 1 << 10,
//     NVME_RW_PRINFO_PRCHK_APP = 1 << 11,
//     NVME_RW_PRINFO_PRCHK_GUARD = 1 << 12,
//     NVME_RW_PRINFO_PRACT = 1 << 13,
//     NVME_RW_DTYPE_STREAMS = 1 << 4,
// };

// struct nvme_common_command
// {
//     __u8 opcode;
//     __u8 flags;
//     __u16 command_id;
//     __le32 nsid;
//     __le32 cdw2[2];
//     __le64 metadata;
//     union nvme_data_ptr dptr;
//     __le32 cdw10;
//     __le32 cdw11;
//     __le32 cdw12;
//     __le32 cdw13;
//     __le32 cdw14;
//     __le32 cdw15;
// };

typedef struct _ADMIN_FORMAT_COMMAND_DW10
{
    uint32_t LBAF : 4;
    uint32_t MSET : 1;
    uint32_t PI : 3;
    uint32_t PIL : 1;
    uint32_t SES : 3;
    uint32_t Reserved : 20;
} ADMIN_FORMAT_COMMAND_DW10, *PADMIN_FORMAT_COMMAND_DW10;

// struct nvme_rw_command
// {
//     __u8 opcode;
//     __u8 flags; //bit[7:6]PSDT bit[1:0]FUSE
//     __u16 command_id;
//     __le32 nsid;
//     __u64 rsvd2;
//     __le64 metadata;
//     union nvme_data_ptr dptr;
//     __le64 slba;
//     __le16 length;  //0'base
//     __le16 control; //bit[15] lr; bit[14] fua; bit[13:10] prinfo; bit[7:4] dtype(w cmd);
//     __le32 dsmgmt;  //bit[7:0] dms; bit[31:16] dspec;
//     __le32 reftag;
//     __le16 apptag;
//     __le16 appmask;
// };

// struct nvme_identify
// {
//     __u8 opcode;
//     __u8 flags;
//     __u16 command_id;
//     __le32 nsid;
//     __u64 rsvd2[2];
//     union nvme_data_ptr dptr;
//     __u8 cns;
//     __u8 rsvd3;
//     __le16 ctrlid;
//     __u8 rsvd11[3];
//     __u8 csi;
//     __u32 rsvd12[4];
// };

// struct nvme_features
// {
//     __u8 opcode;
//     __u8 flags;
//     __u16 command_id;
//     __le32 nsid;
//     __u64 rsvd2[2];
//     union nvme_data_ptr dptr;
//     __le32 fid;
//     __le32 dword11;
//     __le32 dword12;
//     __le32 dword13;
//     __le32 dword14;
//     __le32 dword15;
// };

// struct nvme_dsm_cmd
// {
//     __u8 opcode;
//     __u8 flags;
//     __u16 command_id;
//     __le32 nsid;
//     __u64 rsvd2[2];
//     union nvme_data_ptr dptr;
//     __le32 nr;
//     __le32 attributes;
//     __u32 rsvd12[4];
// };

// struct nvme_abort_cmd
// {
//     __u8 opcode;
//     __u8 flags;
//     __u16 command_id;
//     __u32 rsvd1[9];
//     __le16 sqid;
//     __u16 cid;
//     __u32 rsvd11[5];
// };

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

//* 0=none; 1=to_device, 2=from_device, 3=bidirectional, others illegal */
enum dma_data_direction
{
    DMA_BIDIRECTIONAL = 0,
    DMA_TO_DEVICE = 1,
    DMA_FROM_DEVICE = 2,
    DMA_NONE = 3,
};

#define LBA_DATA_SIZE(nsid) (g_nvme_ns_info[nsid - 1].lbads)

int nvme_64b_cmd(int file_desc, struct nvme_64b_send *user_cmd);

int nvme_io_write_cmd(int file_desc, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                      uint16_t control, void *data_addr);

int nvme_io_read_cmd(int file_desc, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                     uint16_t control, void *data_addr);

int nvme_io_compare_cmd(int file_desc, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                     uint16_t control, void *data_addr);

int send_nvme_write_using_metabuff(int file_desc, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                     uint16_t control, uint32_t meta_id, void *data_addr);
int send_nvme_read_using_metabuff(int file_desc, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                     uint16_t control, uint32_t meta_id, void *data_addr);

int nvme_ring_dbl_and_reap_cq(int file_desc, uint16_t sq_id, uint16_t cq_id, uint32_t expect_num);

int nvme_idfy_ctrl(int file_desc, void *data);
int nvme_idfy_ns(int fd, uint32_t nsid, uint8_t present, void *data);
int nvme_idfy_ns_list(int fd, uint32_t nsid, uint8_t all, void *data);
int nvme_idfy_ctrl_list(int fd, uint32_t nsid, uint16_t cntid, void *data);
int nvme_idfy_secondary_ctrl_list(int fd, uint32_t nsid, uint16_t cntid, void *data);
int nvme_idfy_ns_descs(int fd, uint32_t nsid, void *data);
int nvme_set_feature_cmd(int file_desc, uint32_t nsid, uint8_t feat_id, uint16_t dw11l, uint16_t dw11h);
int nvme_set_feature_hmb_cmd(int file_desc, uint32_t nsid, uint16_t ehm, uint32_t hsize,
                             uint32_t hmdlla, uint32_t hmdlua, uint32_t hmdlec);
int nvme_get_feature_cmd(int file_desc, uint32_t nsid, uint8_t feat_id);

int nvme_admin_ring_dbl_reap_cq(int file_desc);
int nvme_create_contig_iocq(int file_desc, uint16_t cq_id, uint32_t cq_size, uint8_t irq_en, uint16_t irq_no);
int nvme_create_discontig_iocq(int file_desc, uint16_t cq_id, uint32_t cq_size, uint8_t irq_en, uint16_t irq_no,
                               uint8_t const *discontg_cq_buf, uint32_t discontig_cq_size);
int nvme_create_contig_iosq(int file_desc, uint16_t sq_id, uint16_t cq_id, uint32_t sq_size, uint8_t sq_prio);
int nvme_create_discontig_iosq(int file_desc, uint16_t sq_id, uint16_t cq_id, uint32_t sq_size, uint8_t sq_prio,
                               uint8_t const *discontg_sq_buf, uint32_t discontig_sq_size);
int nvme_delete_ioq(int file_desc, uint8_t opcode, uint16_t qid);

int nvme_firmware_commit(int file_desc, byte_t bpid, byte_t ca, byte_t fs);
int nvme_firmware_download(int file_desc, dword_t numd, dword_t ofst, byte_t *addr);

dword_t nvme_msi_register_test(void);
dword_t create_all_io_queue(byte_t flags);
dword_t delete_all_io_queue(void);
int nvme_send_iocmd(int file_desc, uint8_t cmp_dis, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb, void *data_addr);


#endif
