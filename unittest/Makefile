CC=gcc
# CC=clang
CFLAGS=-g -W -Wall
APP_NAME := test

SOURCES :=  test.c \
			unittest.c \
			test_metrics.c \
			test_alloc.c \
			test_rng_sqxdbl.c \
			test_send_cmd.c test_irq.c \
			test_init.c \
			test_cq_gain.c \
			case_queue_sq_cq_match.c\
			case_iocmd_write_read.c\
			case_queue_create_q_size.c\
			case_queue_delete_q.c\
			case_queue_abort.c\
			case_iocmd_fw_io_cmd.c\
			case_queue_cq_int_all.c\
			case_queue_cq_int_all_mask.c\
			case_queue_cq_int_msi_multi_mask.c\
			case_queue_cq_int_msix_mask.c\
			case_queue_cq_int_coalescing.c\
			case_command_arbitration.c\
			case_resets_random_all.c\
			case_pcie_link_speed_width_step.c\
			case_pcie_link_speed_width_cyc.c\
			case_pcie_reset_single.c\
			case_pcie_reset_cyc.c\
			case_pcie_low_power_L0s_L1_step.c\
			case_pcie_low_power_L0s_L1_cyc.c\
			case_pcie_low_power_L1sub_step.c\
			case_pcie_PM.c\
			case_pcie_low_power_measure.c\
			case_pcie_low_power_pcipm_l1sub.c\
			case_pcie_MPS.c\
			case_pcie_MRRS.c\
			case_register_test.c\
			case_queue_admin.c\
			case_nvme_boot_partition.c\
			test_0_full_disk_wr.c\
			test_1_fused.c\
			test_2_mix_case.c\
			case_resets_link_down.c\
			test_3_adm_wr_cache_fua.c\
			test_4_peak_power.c\
			test_5_fua_wr_rd_cmp.c\
			test_6_all_ns_lbads_test.c\
			test_bug_trace.c


INCLUDE :=

all: $(APP_NAME)

$(APP_NAME): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(APP_NAME) $(LDFLAGS)

clean:
	rm -f *.o
	rm -f $(APP_NAME)

.PHONY: all clean doc

