#ifndef _CASE_ALL_H_
#define _CASE_ALL_H_

//***old unittest framework**************************************
int case_queue_sq_cq_match(void);
int case_iocmd_write_read(void);
int case_queue_create_q_size(void);
int case_queue_delete_q(void);
int case_queue_abort(void);

int case_iocmd_fw_io_cmd(void);

int case_queue_cq_int_all(void);

int case_queue_cq_int_coalescing(void);
int case_queue_cq_int_all_mask(void);
int case_queue_cq_int_msi_multi_mask(void);
int case_queue_cq_int_msix_mask(void);

int case_command_arbitration(void);

int case_resets_random_all(void);
int case_resets_link_down(void);

int case_pcie_link_speed_width_step(void);
int case_pcie_link_speed_width_cyc(void);
int case_pcie_reset_single(void);
int case_pcie_reset_cyc(void);
int case_pcie_low_power_L0s_L1_step(void);
int case_pcie_low_power_L0s_L1_cyc(void);
int case_pcie_low_power_L1sub_step(void);
int case_pcie_PM(void);

int case_pcie_low_power_measure(void);
int case_pcie_low_power_pcipm_l1sub(void);
int case_pcie_MPS(void);
int case_pcie_MRRS(void);

int case_register_test(void);
int case_queue_admin(void);
int case_nvme_boot_partition(void);
//***new unittest framework**************************************
int test_0_full_disk_wr(void);
int test_1_fused(void);
int test_2_mix_case(void);
int test_3_adm_wr_cache_fua(void);
int test_4_peak_power(void);
int test_5_fua_wr_rd_cmp(void);
int test_6_all_ns_lbads_test(void);

#endif

