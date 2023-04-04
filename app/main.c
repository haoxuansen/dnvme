/**
 * @file test.c
 * @author yumeng (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2019-10-08
 * 
 * @copyright Copyright (c) 2018-2019 Maxio-Tech
 * 
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <ctype.h>
#include <malloc.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "dnvme_ioctl.h"
#include "ioctl.h"
#include "irq.h"

#include "overview.h"
#include "common.h"
#include "test.h"
#include "unittest.h"
#include "test_irq.h"

static struct nvme_tool s_nvme_tool = {0};
struct nvme_tool *g_nvme_tool = &s_nvme_tool;

static int alloc_buffer(struct nvme_tool *tool)
{
	int ret;

	tool->entry = calloc(1, NVME_TOOL_CQ_ENTRY_SIZE);
	if (!tool->entry) {
		pr_err("failed to alloc for CQ entry!\n");
		return -ENOMEM;
	}
	tool->entry_size = NVME_TOOL_CQ_ENTRY_SIZE;

	ret = posix_memalign(&tool->sq_buf, 4096, NVME_TOOL_SQ_BUF_SIZE);
	if (ret) {
		pr_err("failed to alloc for SQ!\n");
		goto out;
	}
	memset(tool->sq_buf, 0, NVME_TOOL_SQ_BUF_SIZE);
	tool->sq_buf_size = NVME_TOOL_SQ_BUF_SIZE;

	ret = posix_memalign(&tool->cq_buf, 4096, NVME_TOOL_CQ_BUF_SIZE);
	if (ret) {
		pr_err("failed to alloc for CQ!\n");
		goto out2;
	}
	memset(tool->cq_buf, 0, NVME_TOOL_CQ_BUF_SIZE);
	tool->cq_buf_size = NVME_TOOL_CQ_BUF_SIZE;

	ret = posix_memalign(&tool->rbuf, CONFIG_UNVME_RW_BUF_ALIGN, 
		NVME_TOOL_RW_BUF_SIZE);
	if (ret) {
		pr_err("failed to alloc read buf!\n");
		goto out3;
	}
	memset(tool->rbuf, 0, NVME_TOOL_RW_BUF_SIZE);
	tool->rbuf_size = NVME_TOOL_RW_BUF_SIZE;

	ret = posix_memalign(&tool->wbuf, CONFIG_UNVME_RW_BUF_ALIGN, 
		NVME_TOOL_RW_BUF_SIZE);
	if (ret) {
		pr_err("failed to alloc write buf!\n");
		goto out4;
	}
	memset(tool->wbuf, 0, NVME_TOOL_RW_BUF_SIZE);
	tool->wbuf_size = NVME_TOOL_RW_BUF_SIZE;

	tool->meta_rbuf = calloc(1, NVME_TOOL_RW_META_SIZE);
	if (!tool->meta_rbuf) {
		pr_err("failed to alloc meta rbuf!\n");
		goto out5;
	}
	tool->meta_rbuf_size = NVME_TOOL_RW_META_SIZE;

	tool->meta_wbuf = calloc(1, NVME_TOOL_RW_META_SIZE);
	if (!tool->meta_wbuf) {
		pr_err("failed to alloc meta wbuf!\n");
		goto out6;
	}
	tool->meta_wbuf_size = NVME_TOOL_RW_META_SIZE;

	return 0;
out6:
	free(tool->meta_rbuf);
	tool->meta_rbuf = NULL;
out5:
	free(tool->wbuf);
	tool->wbuf = NULL;
out4:
	free(tool->rbuf);
	tool->rbuf = NULL;
out3:
	free(tool->cq_buf);
	tool->cq_buf = NULL;
out2:
	free(tool->sq_buf);
	tool->sq_buf = NULL;
out:
	free(tool->entry);
	tool->entry = NULL;
	return -ENOMEM;
}

void release_buffer(struct nvme_tool *tool)
{
	free(tool->meta_rbuf);
	tool->meta_rbuf = NULL;
	free(tool->meta_wbuf);
	tool->meta_wbuf = NULL;
	free(tool->rbuf);
	tool->rbuf = NULL;
	free(tool->wbuf);
	tool->wbuf = NULL;
	free(tool->sq_buf);
	tool->sq_buf = NULL;
	free(tool->cq_buf);
	tool->cq_buf = NULL;
	free(tool->entry);
	tool->entry = NULL;
}

/**
 * @brief NVMe unit test entry
 * 
 * @param argc The number of parameters entered on the command line.
 * @param argv Array of pointers, each element point to a string.
 *  argv[1]: NVMe device path, eg: /dev/nvme0
 * @return 0 on success, otherwise a negative errno.
 */
int main(int argc, char *argv[])
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev;
	int ret;

	if (argc < 2) {
		pr_err("Please specify a nvme device!\n");
		return -EINVAL;
	}

	srand(time(NULL));

	ndev = nvme_init(argv[1]);
	if (!ndev)
		return -EPERM;

	tool->ndev = ndev;

	ret = alloc_buffer(tool);
	if (ret < 0)
		goto out;

	case_display_case_list(tool);
	nvme_select_case_to_execute(tool);

	nvme_disable_controller_complete(ndev->fd);
	nvme_deinit(ndev);
	release_buffer(tool);

	pr_notice("********** END OF TEST **********\n\n");
	return 0;
out:
	nvme_deinit(ndev);
	return ret;
}

