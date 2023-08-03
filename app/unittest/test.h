/**
 * @file test.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-13
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _APP_TEST_H_
#define _APP_TEST_H_

#include <stdint.h>

#include "sizes.h"
#include "libnvme.h"

#define NVME_TOOL_CQ_ENTRY_SIZE		SZ_1M /* CQES(16) * elements(64K) */
#define NVME_TOOL_SQ_BUF_SIZE		SZ_4M /* SQES(64) * elements(64K) */
#define NVME_TOOL_CQ_BUF_SIZE		SZ_1M /* CQES(16) * elements(64K) */
#define NVME_TOOL_RW_BUF_SIZE		SZ_2M
#define NVME_TOOL_RW_META_SIZE		SZ_1M

struct nvme_usr_param {
	const char	*devpath;
};

struct nvme_tool {
	struct nvme_usr_param	param;
	struct nvme_dev_info	*ndev;

	struct nvme_completion	*entry;
	uint32_t		entry_size;

	void		*rbuf;
	uint32_t	rbuf_size;
	void		*wbuf;
	uint32_t	wbuf_size;

	/* For discontiguous queue */
	void		*sq_buf;
	uint32_t	sq_buf_size;
	void		*cq_buf;
	uint32_t	cq_buf_size;

	/* For SGL metadata */
	void		*meta_rbuf;
	uint32_t	meta_rbuf_size;
	void		*meta_wbuf;
	uint32_t	meta_wbuf_size;
};

extern struct nvme_tool *g_nvme_tool;

void nvme_record_subcase_result(const char *name, int result);
void nvme_display_subcase_result(void);
void nvme_display_test_result(int result, const char *desc);

/* ==================== Related to "test_cmd.c" ==================== */

int case_cmd_io_read(struct nvme_tool *tool);
int case_cmd_io_write(struct nvme_tool *tool);
int case_cmd_io_compare(struct nvme_tool *tool);

int case_cmd_io_read_with_fua(struct nvme_tool *tool);
int case_cmd_io_write_with_fua(struct nvme_tool *tool);

int case_cmd_io_copy(struct nvme_tool *tool);


/* ==================== Related to "test_fused.c" ==================== */

int case_fused_operation(struct nvme_tool *tool);


/* ==================== Related to "test_meta.c" ==================== */

int case_meta_node_contiguous(struct nvme_tool *tool);

int case_meta_xfer_separate_sgl(struct nvme_tool *tool);
int case_meta_xfer_separate_prp(struct nvme_tool *tool);
int case_meta_xfer_contig_lba(struct nvme_tool *tool);


/* ==================== Related to "test_mix.c" ==================== */

int case_disable_bus_master(struct nvme_tool *tool);


/* ==================== Related to "test_pm.c" ==================== */

int case_pm_switch_power_state(struct nvme_tool *tool);

int case_pm_set_d0_state(struct nvme_tool *tool);
int case_pm_set_d3hot_state(struct nvme_tool *tool);


/* ==================== Related to "test_queue.c" ==================== */

int case_queue_iocmd_to_asq(struct nvme_tool *tool);

int case_queue_contiguous(struct nvme_tool *tool);
int case_queue_discontiguous(struct nvme_tool *tool);

#endif /* !_APP_TEST_H_ */

