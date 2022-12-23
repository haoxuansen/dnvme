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
#include "unittest.h"
#include "test_metrics.h"
#include "test_cq_gain.h"
#include "test_init.h"
#include "test_irq.h"

int g_fd = -1;
void *g_read_buf;
void *g_write_buf;
void *g_cq_entry_buf;
void *g_discontig_sq_buf;
void *g_discontig_cq_buf;

struct nvme_dev_info g_nvme_dev = {0};
struct nvme_sq_info *g_ctrl_sq_info;
struct nvme_ns *g_nvme_ns_info;

static int test_mem_alloc(void)
{
	int ret;

	/* !TODO: It's better to alloc this in "test_cq_gain.c"? */
	g_cq_entry_buf = calloc(1, BUFFER_CQ_ENTRY_SIZE);
	if (!g_cq_entry_buf) {
		pr_err("failed to alloc cq_entry_buf(0x%x)!\n", 
			BUFFER_CQ_ENTRY_SIZE);
		return -ENOMEM;
	}

	ret = posix_memalign(&g_discontig_sq_buf, 4096, DISCONTIG_IO_SQ_SIZE);
	if (ret) {
		pr_err("failed to alloc discontig_sq_buf(0x%x)!\n", 
			DISCONTIG_IO_SQ_SIZE);
		goto out;
	}
	memset(g_discontig_sq_buf, 0, DISCONTIG_IO_SQ_SIZE);

	ret = posix_memalign(&g_discontig_cq_buf, 4096, DISCONTIG_IO_CQ_SIZE);
	if (ret) {
		pr_err("failed to alloc discontig_cq_buf(0x%x)!\n", 
			DISCONTIG_IO_CQ_SIZE);
		goto out2;
	}
	memset(g_discontig_cq_buf, 0, DISCONTIG_IO_CQ_SIZE);

	ret = posix_memalign(&g_read_buf, CONFIG_UNVME_RW_BUF_ALIGN, 
		RW_BUFFER_SIZE);
	if (ret) {
		pr_err("failed to alloc read buf(0x%x) with align 0x%x!\n",
			RW_BUFFER_SIZE, CONFIG_UNVME_RW_BUF_ALIGN);
		goto out3;
	}
	memset(g_read_buf, 0, RW_BUFFER_SIZE);

	ret = posix_memalign(&g_write_buf, CONFIG_UNVME_RW_BUF_ALIGN, 
		RW_BUFFER_SIZE);
	if (ret) {
		pr_err("failed to alloc write buf(0x%x) with align 0x%x!\n",
			RW_BUFFER_SIZE, CONFIG_UNVME_RW_BUF_ALIGN);
		goto out4;
	}
	memset(g_write_buf, 0, RW_BUFFER_SIZE);

	return 0;
out4:
	free(g_read_buf);
	g_read_buf = NULL;
out3:
	free(g_discontig_cq_buf);
	g_discontig_cq_buf = NULL;
out2:
	free(g_discontig_sq_buf);
	g_discontig_sq_buf = NULL;
out:
	free(g_cq_entry_buf);
	g_cq_entry_buf = NULL;
	return -ENOMEM;
}

void test_mem_free(void)
{
	free(g_read_buf);
	g_read_buf = NULL;
	free(g_write_buf);
	g_write_buf = NULL;
	free(g_discontig_sq_buf);
	g_discontig_sq_buf = NULL;
	free(g_discontig_cq_buf);
	g_discontig_cq_buf = NULL;
	free(g_cq_entry_buf);
	g_cq_entry_buf = NULL;

	/* !TODO: It's better to free memory in "test_exit"? */
	free(g_ctrl_sq_info);
	g_ctrl_sq_info = NULL;
	free(g_nvme_ns_info);
	g_nvme_ns_info = NULL;
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
	int ret;
	char *log_file = "/tmp/nvme_log.txt";
	uint32_t i = 0;

	if (argc < 2) {
		pr_err("Please specify a nvme device!\n");
		return -EINVAL;
	}

	g_fd = open(argv[1], 0);
	if (g_fd < 0) {
		pr_err("failed to open %s: %s!\n", argv[1], strerror(errno));
		return errno;
	}
	pr_info("Open %s OK!\n", argv[1]);

	ret = test_mem_alloc();
	if (ret < 0)
		return ret;

	for (i = 0; i < RW_BUFFER_SIZE; i += 4)
	{
		// *((uint32_t *)(g_write_buf + i)) = DWORD_RAND();
		*((uint32_t *)(g_write_buf + i)) = i;
	}
	memset(&g_nvme_dev, 0xff, sizeof(struct nvme_dev_info));

	srand(time(NULL));

	test_init(g_fd, &g_nvme_dev);

	case_display_case_list();
	nvme_select_case_to_execute();
	nvme_dump_log(g_fd, log_file);

	/* Exit gracefully */
	pr_info("\nNow Exiting gracefully....\n");
	nvme_disable_controller_complete(g_fd);
	nvme_set_irq(g_fd, NVME_INT_NONE, 0);
	g_nvme_dev.irq_type = NVME_INT_NONE;
	test_mem_free();
	pr_info("\n\n****** END OF TEST ******\n\n");
	close(g_fd);
	return 0;
}

