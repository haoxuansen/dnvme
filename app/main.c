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

#include "overview.h"
#include "common.h"
#include "unittest.h"

#include "test_irq.h"
#include "test_send_cmd.h"
#include "test_init.h"
#include "test_cq_gain.h"
#include "case_all.h"
#include "test_metrics.h"
#include "test_bug_trace.h"

#define RANDOM_VAL 0

int file_desc = 0;
void *read_buffer;
void *write_buffer;
void *discontg_sq_buf;
void *discontg_cq_buf;
char *device_file_name = "/dev/nvme0";

struct nvme_ctrl g_nvme_dev;
struct nvme_sq_info *ctrl_sq_info;
struct nvme_ns *g_nvme_ns_info;

void test_mem_alloc(void)
{
    // allocate for cq entry buffer
    buffer_cq_entry = malloc(BUFFER_CQ_ENTRY_SIZE);
    if (buffer_cq_entry == NULL)
    {
        pr_err("Malloc Failed\n");
    }

    /* Allocating buffer for Discontiguous IOSQ/IOCQ and setting to 0 */
    if ((posix_memalign(&discontg_sq_buf, 4096, DISCONTIG_IO_SQ_SIZE)) ||
        (posix_memalign(&discontg_cq_buf, 4096, DISCONTIG_IO_CQ_SIZE)))
    {
        printf("Memalign Failed");
    }

/* Allocating buffer for Read & write*/
#ifdef RW_BUF_4K_ALN_EN
    if ((posix_memalign(&read_buffer, 4096, RW_BUFFER_SIZE)) ||
        (posix_memalign(&write_buffer, 4096, RW_BUFFER_SIZE)))
    {
        pr_err("Memalign Failed\n");
    }
#else
    read_buffer = malloc(RW_BUFFER_SIZE);
    write_buffer = malloc(RW_BUFFER_SIZE);
    if ((write_buffer == NULL) || (read_buffer == NULL))
    {
        pr_err("Malloc Failed\n");
    }
#endif
    // this malloc in test_init when runing 
    // ctrl_sq_info = (struct nvme_sq_info *)(malloc(g_nvme_dev.max_sq_num * sizeof(struct nvme_sq_info)));
    // g_nvme_ns_info = (struct nvme_ns *)malloc(g_nvme_dev.id_ctrl.nn * sizeof(struct nvme_ns));

}

void test_mem_free(void)
{
    free(read_buffer);
    free(write_buffer);
    free(discontg_sq_buf);
    free(discontg_cq_buf);
    free(buffer_cq_entry);
    free(ctrl_sq_info);
    free(g_nvme_ns_info);
}

/**
 * @brief main
 * 
 * @param argc 统计程序运行时发送给main函数的命令行参数的个数
 * @param argv 字符串数组，用来存放指向的字符串参数的指针数组，每一个元素指向一个参数。
 *  argv[0]指向程序运行的全路径名 
 *  argv[1]指向在DOS命令行中执行程序名后的第一个字符串 
 *  argv[2]指向执行程序名后的第二个字符串 
 *  argv[3]指向执行程序名后的第三个字符串 
 *  ...
 *  argv[argc]为NULL 
 * @return int 
 */
int main(int argc, char *argv[])
{
    char *closefile = "/tmp/closefile.txt";
    uint32_t i = 0;

    if ((argc > 2) || (argc == 1))
    {
        pr_err("You need specified a nvme device\r\n");
        exit(-1);
    }
    device_file_name = argv[1];
    file_desc = open(device_file_name, 0);
    if (file_desc < 0)
    {
        pr_err("Can't open device file:\033[31m %s \033[0m\n", device_file_name);
        exit(-1);
    }
    pr_color(LOG_COLOR_GREEN, "\nOpen device:%s OK!\n", device_file_name);
    test_mem_alloc();

    memset(buffer_cq_entry, 0, BUFFER_CQ_ENTRY_SIZE);
    memset(discontg_sq_buf, 0, DISCONTIG_IO_SQ_SIZE);
    memset(discontg_cq_buf, 0, DISCONTIG_IO_CQ_SIZE);

    // memset(read_buffer, 0xff, RW_BUFFER_SIZE);
    // memset(write_buffer, 0x55, RW_BUFFER_SIZE);
    for (i = 0; i < RW_BUFFER_SIZE; i += 4)
    {
        // *((uint32_t *)(write_buffer + i)) = DWORD_RAND();
        *((uint32_t *)(write_buffer + i)) = i;
    }
    memset(&g_nvme_dev, 0xff, sizeof(struct nvme_ctrl));

    RAND_INIT();

    // test_dev_metrics(file_desc);
    // test_drv_metrics(file_desc);

    test_init(file_desc);

    // random_sq_cq_info();
    case_display_case_list();
    nvme_select_case_to_execute();

    pr_info("\nCalling Dump Metrics to closefile\n");
    ioctl_dump(file_desc, closefile);

    /* Exit gracefully */
    pr_info("\nNow Exiting gracefully....\n");
    ioctl_disable_ctrl(file_desc, NVME_ST_DISABLE_COMPLETE);
    set_irqs(file_desc, NVME_INT_NONE, 0);
    test_mem_free();
    pr_info("\n\n****** END OF TEST ******\n\n");
    close(file_desc);
    return 0;
}

