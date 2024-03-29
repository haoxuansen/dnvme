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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <malloc.h>

#include "libbase.h"
#include "libnvme.h"

#include "test.h"
#include "unittest.h"
#include "test_irq.h"

static struct nvme_tool s_nvme_tool = {0};
struct nvme_tool *g_nvme_tool = &s_nvme_tool;

static int alloc_buffer(struct nvme_tool *tool)
{
	int ret;

	tool->entry = zalloc(NVME_TOOL_CQ_ENTRY_SIZE);
	if (!tool->entry) {
		pr_err("failed to alloc for CQ entry!\n");
		return -ENOMEM;
	}
	tool->entry_size = NVME_TOOL_CQ_ENTRY_SIZE;

	/* !TODO: It's better to align pages rather than a constant of 4096. */
	ret = posix_memalign(&tool->sq_buf, 4096, NVME_TOOL_SQ_BUF_SIZE);
	if (ret) {
		pr_err("failed to alloc for SQ!\n");
		goto free_entry;
	}
	memset(tool->sq_buf, 0, NVME_TOOL_SQ_BUF_SIZE);
	tool->sq_buf_size = NVME_TOOL_SQ_BUF_SIZE;

	ret = posix_memalign(&tool->cq_buf, 4096, NVME_TOOL_CQ_BUF_SIZE);
	if (ret) {
		pr_err("failed to alloc for CQ!\n");
		goto free_sq_buf;
	}
	memset(tool->cq_buf, 0, NVME_TOOL_CQ_BUF_SIZE);
	tool->cq_buf_size = NVME_TOOL_CQ_BUF_SIZE;

	ret = posix_memalign(&tool->rbuf, NVME_TOOL_RW_BUF_ALIGN, 
		NVME_TOOL_RW_BUF_SIZE);
	if (ret) {
		pr_err("failed to alloc read buf!\n");
		goto free_cq_buf;
	}
	memset(tool->rbuf, 0, NVME_TOOL_RW_BUF_SIZE);
	tool->rbuf_size = NVME_TOOL_RW_BUF_SIZE;

	ret = posix_memalign(&tool->wbuf, NVME_TOOL_RW_BUF_ALIGN, 
		NVME_TOOL_RW_BUF_SIZE);
	if (ret) {
		pr_err("failed to alloc write buf!\n");
		goto free_rbuf;
	}
	memset(tool->wbuf, 0, NVME_TOOL_RW_BUF_SIZE);
	tool->wbuf_size = NVME_TOOL_RW_BUF_SIZE;

	tool->meta_rbuf = zalloc(NVME_TOOL_RW_META_SIZE);
	if (!tool->meta_rbuf) {
		pr_err("failed to alloc meta rbuf!\n");
		goto free_wbuf;
	}
	tool->meta_rbuf_size = NVME_TOOL_RW_META_SIZE;

	tool->meta_wbuf = zalloc(NVME_TOOL_RW_META_SIZE);
	if (!tool->meta_wbuf) {
		pr_err("failed to alloc meta wbuf!\n");
		goto free_meta_rbuf;
	}
	tool->meta_wbuf_size = NVME_TOOL_RW_META_SIZE;

	return 0;

free_meta_rbuf:
	free(tool->meta_rbuf);
	tool->meta_rbuf = NULL;
free_wbuf:
	free(tool->wbuf);
	tool->wbuf = NULL;
free_rbuf:
	free(tool->rbuf);
	tool->rbuf = NULL;
free_cq_buf:
	free(tool->cq_buf);
	tool->cq_buf = NULL;
free_sq_buf:
	free(tool->sq_buf);
	tool->sq_buf = NULL;
free_entry:
	free(tool->entry);
	tool->entry = NULL;
	return -ENOMEM;
}

static void release_buffer(struct nvme_tool *tool)
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

static int case_display_case_list(struct nvme_tool *tool, struct case_data *priv)
{
	struct nvme_case *start = __start_nvme_case;
	struct nvme_case *end = __stop_nvme_case;
	struct nvme_case *entry;
	unsigned int idx;

	pr_color(LOG_B_CYAN, "==================== "
		"NVMe Unit Test Case List ====================\n");

	for (entry = start, idx = 0; entry < end; entry++, idx++) {
		pr_color(LOG_N_CYAN, "Case %3u: %s", idx, entry->name);
		pr_color(LOG_N, "\t%s\n", entry->desc);
	}

	return 0;
}
NVME_CASE_HEAD_SYMBOL(case_display_case_list, "Display all supported cases");

static int case_collection(struct nvme_tool *tool, struct case_data *priv)
{
	unsigned long *start = __start_nvme_autocase;
	unsigned long *end = __stop_nvme_autocase;
	struct nvme_case *cur;
	unsigned int total;
	unsigned int *array;
	unsigned int loop = 1;
	unsigned int i, j, k;
	int ret;

	BUG_ON(start > end);

	if (start == end) {
		pr_warn("autocase list is empty!\n");
		return -EOPNOTSUPP;
	}

	total = ((unsigned long)end - (unsigned long)start) / 
		sizeof(unsigned long);

	array = zalloc(sizeof(unsigned int) * total);
	if (!array) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
	}

	for (i = 0; i < total; i++)
		array[i] = i;

	fflush(stdout);
	pr_notice("Please enter the number of loop to run: ");
	scanf("%u", &loop);

	for (k = 1; k <= loop; k++) {
		pr_notice("Start execute autocase list %utimes...\n", k);

		for (i = 0; i < total; i++) {
			cur = (struct nvme_case *)start[array[i]];
			ret = cur->func(tool, cur->data);
			nvme_record_case_result(cur->name, ret);
		}
		ret = nvme_display_case_report();

		/* Shuffle the order of test cases */
		for (i = 0; i < (total - 1); i++) {
			j = i + rand() % (total - i);
			swap(array[i], array[j]);
		}
	}

	free(array);
	return ret;
}
NVME_CASE_HEAD_SYMBOL(case_collection, "Execute cases in aotocase list");

static int select_case_to_execute(struct nvme_tool *tool)
{
	struct nvme_case *start = __start_nvme_case;
	struct nvme_case *end = __stop_nvme_case;
	struct case_data *priv = NULL;
	unsigned int total;
	unsigned int select;
	int ret;

	BUG_ON(start >= end);

	total = ((unsigned long)end - (unsigned long)start) / 
		sizeof(struct nvme_case);
	while (1) {
		fflush(stdout);
		pr_notice("Please enter the case ID that is ready to run: ");
		scanf("%u", &select);

		if (select >= total) {
			pr_warn("The selected case number(%u) is out of range(%u)!"
				"Now will exit...\n", select, total - 1);
			break;
		}

		if (!start[select].func) {
			pr_warn("Case %d is empty! please try again\n", select);
			continue;
		}
		priv = start[select].data;
		priv->tool = tool;

		ut_rpt_create_case(tool, &priv->rpt, start[select].name);

		ret = start[select].func(tool, start[select].data);

		ut_rpt_record_case_result(&priv->rpt, ret);
		nvme_display_test_result(ret, start[select].name);
	} 

	return 0;
}


#if IS_ENABLED(CONFIG_DEBUG_NO_DEVICE)
int main(int argc, char *argv[])
{
	struct nvme_tool *tool = g_nvme_tool;
	int i;

	for (i = 0; i < argc; i++) {
		pr_info("argv[%d]: %s\n", i, argv[i]);
	}

	if (!ut_rpt_create_root(tool) || ut_rpt_init_root(tool, "1.0.0"))
		return -EPERM;

	case_display_case_list(tool, NULL);
	select_case_to_execute(tool);

	pr_notice("********** END OF TEST **********\n\n");
	ut_rpt_save_to_file(tool, "./test_report.json");

	ut_rpt_destroy_root(tool);
	return 0;
}

#else

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
	int ret = -EPERM;

	if (argc < 2) {
		pr_err("Please specify a nvme device!\n");
		return -EINVAL;
	}

	if (!ut_rpt_create_root(tool) || ut_rpt_init_root(tool, "1.0.0"))
		return -EPERM;

	ndev = nvme_init(argv[1]);
	if (!ndev)
		goto out;

	tool->ndev = ndev;

	ret = alloc_buffer(tool);
	if (ret < 0)
		goto out2;

	case_display_case_list(tool, NULL);
	select_case_to_execute(tool);

	pr_notice("********** END OF TEST **********\n\n");
	ut_rpt_save_to_file(tool, "./test_report.json");

	nvme_disable_controller_complete(ndev->fd);

	release_buffer(tool);
	ret = 0;
out2:
	nvme_deinit(ndev);
out:
	ut_rpt_destroy_root(tool);
	return ret;
}
#endif /* !IS_ENABLED(CONFIG_DEBUG_NO_DEVICE) */

