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
#include "core.h"

#define TEST_PASS "\n" \
	"ppppppppppp     aaaaaaaaaaa     sssssssssss     sssssssssss \n" \
	"pp       pp     aa       aa     ss              ss          \n" \
	"pp       pp     aa       aa     ss              ss          \n" \
	"pp       pp     aa       aa     ss              ss          \n" \
	"ppppppppppp     aaaaaaaaaaa     sssssssssss     sssssssssss \n" \
	"pp              aa       aa              ss              ss \n" \
	"pp              aa       aa              ss              ss \n" \
	"pp              aa       aa              ss              ss \n" \
	"pp              aa       aa     sssssssssss     sssssssssss \n"

#define TEST_FAIL "\n" \
	"fffffffffff     aaaaaaaaaaa     iiiiiiiiiii     ll          \n" \
	"ff              aa       aa         iii         ll          \n" \
	"ff              aa       aa         iii         ll          \n" \
	"ff              aa       aa         iii         ll          \n" \
	"fffffffffff     aaaaaaaaaaa         iii         ll          \n" \
	"ff              aa       aa         iii         ll          \n" \
	"ff              aa       aa         iii         ll          \n" \
	"ff              aa       aa         iii         ll          \n" \
	"ff              aa       aa     iiiiiiiiiii     lllllllllll \n"

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


/* ==================== Related to "test_meta.c" ==================== */

int case_meta_node_contiguous(struct nvme_tool *tool);

int case_meta_xfer_sgl(struct nvme_tool *tool);
int case_meta_xfer_separate(struct nvme_tool *tool);
int case_meta_xfer_extlba(struct nvme_tool *tool);

#endif /* !_APP_TEST_H_ */

