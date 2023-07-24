#ifndef _CASE_ALL_H_
#define _CASE_ALL_H_

struct nvme_tool;

//***old unittest framework**************************************
int case_queue_sq_cq_match(struct nvme_tool *tool);
int case_iocmd_write_read(struct nvme_tool *tool);
int case_queue_create_q_size(struct nvme_tool *tool);
int case_queue_delete_q(struct nvme_tool *tool);
int case_queue_abort(struct nvme_tool *tool);

int case_iocmd_fw_io_cmd(struct nvme_tool *tool);

int case_queue_cq_int_all(struct nvme_tool *tool);

int case_queue_cq_int_coalescing(struct nvme_tool *tool);
int case_queue_cq_int_all_mask(struct nvme_tool *tool);
int case_queue_cq_int_msi_multi_mask(struct nvme_tool *tool);
int case_queue_cq_int_msix_mask(struct nvme_tool *tool);

int case_command_arbitration(struct nvme_tool *tool);

int case_resets_random_all(struct nvme_tool *tool);
int case_resets_link_down(struct nvme_tool *tool);

int case_pcie_link_speed_width_step(struct nvme_tool *tool);
int case_pcie_link_speed_width_cyc(struct nvme_tool *tool);
int case_pcie_reset_single(struct nvme_tool *tool);
int case_pcie_reset_cyc(struct nvme_tool *tool);
int case_pcie_low_power_L0s_L1_step(struct nvme_tool *tool);
int case_pcie_low_power_L0s_L1_cyc(struct nvme_tool *tool);
int case_pcie_low_power_L1sub_step(struct nvme_tool *tool);
int case_pcie_PM(struct nvme_tool *tool);

int case_pcie_low_power_measure(struct nvme_tool *tool);
int case_pcie_low_power_pcipm_l1sub(struct nvme_tool *tool);
int case_pcie_MPS(struct nvme_tool *tool);
int case_pcie_MRRS(struct nvme_tool *tool);

int case_register_test(struct nvme_tool *tool);
int case_queue_admin(struct nvme_tool *tool);
int case_nvme_boot_partition(struct nvme_tool *tool);
//***new unittest framework**************************************
int test_0_full_disk_wr(struct nvme_tool *tool);
int test_2_mix_case(struct nvme_tool *tool);
int test_3_adm_wr_cache_fua(struct nvme_tool *tool);
int test_4_peak_power(struct nvme_tool *tool);
int test_5_fua_wr_rd_cmp(struct nvme_tool *tool);
int test_6_all_ns_lbads_test(struct nvme_tool *tool);

#endif

