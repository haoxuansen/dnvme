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
#include "dnvme_ioctl.h"

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

static uint32_t io_sq_id = 1;
static uint32_t io_cq_id = 1;
static uint32_t reap_num;
static uint32_t cmd_cnt = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint64_t wr_slba = 0;
static uint32_t test_loop = 1;

struct nvme_ctrl g_nvme_dev;
struct nvme_sq_info *ctrl_sq_info;
struct nvme_ns *g_nvme_ns_info;

static TestCase_t TestCaseList[] = {
    TCD(case_register_test),//case_54
    TCD(case_queue_admin),//case_52
    TCD(case_queue_delete_q),//case_31
    TCD(test_0_full_disk_wr),//case_55
    TCD(test_2_mix_case),//case_57
    TCD(test_3_adm_wr_cache_fua),//case_58
    TCD(test_5_fua_wr_rd_cmp),//case_59
    TCD(case_queue_create_q_size),//case_30
    TCD(case_iocmd_write_read),//case_33
    TCD(case_queue_abort),//case_36
    TCD(case_resets_random_all),//case_50
    TCD(case_queue_sq_cq_match),//case_32
    TCD(case_command_arbitration),//case_48
    TCD(case_queue_cq_int_all),//case_40
    TCD(case_queue_cq_int_all_mask),//case_41
    TCD(case_queue_cq_int_coalescing),//case_47
    TCD(case_nvme_boot_partition),//case_53
    TCD(test_6_all_ns_lbads_test),//case_60
};

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

    int test_case;
    char *closefile = "/tmp/closefile.txt";

    /* Maximum possible entries */
    uint32_t index = 0, i = 0;
    uint32_t u32_tmp_data = 0;

    uint32_t cq_size = DEFAULT_IO_QUEUE_SIZE;
    uint32_t sq_size = DEFAULT_IO_QUEUE_SIZE;

    struct fwdma_parameter fwdma_parameter = {0};

    uint32_t data_len = 0;
    uint8_t ret = 0;

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
        // *((dword_t *)(write_buffer + i)) = DWORD_RAND();
        *((dword_t *)(write_buffer + i)) = i;
    }
    memset(&g_nvme_dev, 0xff, sizeof(struct nvme_ctrl));

    RAND_INIT();

    // test_dev_metrics(file_desc);
    // test_drv_metrics(file_desc);

    test_init(file_desc);

    sq_size = g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes;
    cq_size = g_nvme_dev.ctrl_reg.nvme_cap0.bits.cap_mqes;

    // random_sq_cq_info();

    pr_info("%s", DISP_HELP);
    // LOG(pr_warn,"pr_warn\n");
    // pr_err("pr_err\n");
    // pr_info("pr_info\n");
    // pr_debug("pr_debug\n");
    do
    {
        pr_color(LOG_COLOR_CYAN, "%s >", device_file_name);
        fflush(stdout);
        scanf("%d", &test_case);
        switch (test_case)
        {
        case 0:
            pr_info("%s", DISP_HELP);
            break;
        case 1:
            pr_info("\nTest: Disabling the controller completely\n");
            ioctl_disable_ctrl(file_desc, NVME_ST_DISABLE_COMPLETE);
            break;
        case 2:
            test_init(file_desc);
            break;
        case 3:
            io_sq_id = 1;
            io_cq_id = 1;
            cq_size = 65280;
            sq_size = 65472;

            pr_color(LOG_COLOR_PURPLE, "  Create discontig cq_id:%d, cq_size = %d\n", io_cq_id, cq_size);
            nvme_create_discontig_iocq(file_desc, io_cq_id, cq_size, ENABLE, io_cq_id, discontg_cq_buf, DISCONTIG_IO_CQ_SIZE);

            pr_color(LOG_COLOR_PURPLE, "  Create discontig sq_id:%d, assoc cq_id = %d, sq_size = %d\n", io_sq_id, io_cq_id, sq_size);
            nvme_create_discontig_iosq(file_desc, io_sq_id, io_cq_id, sq_size, MEDIUM_PRIO, discontg_sq_buf, DISCONTIG_IO_SQ_SIZE);

            break;
        case 4:
            io_sq_id = 1;
            io_cq_id = 1;

            pr_color(LOG_COLOR_PURPLE, "  Create contig cq_id:%d, cq_size = %d\n", io_cq_id, cq_size);
            nvme_create_contig_iocq(file_desc, io_cq_id, cq_size, ENABLE, io_cq_id);

            pr_color(LOG_COLOR_PURPLE, "  Create contig sq_id:%d, assoc cq_id = %d, sq_size = %d\n", io_sq_id, io_cq_id, sq_size);
            nvme_create_contig_iosq(file_desc, io_sq_id, io_cq_id, sq_size, MEDIUM_PRIO);
            break;
        case 5: /* Delete the Queues */
            pr_color(LOG_COLOR_PURPLE, "  Deleting SQID:%d,CQID:%d\n", io_sq_id, io_cq_id);
            nvme_delete_ioq(file_desc, nvme_admin_delete_sq, io_sq_id);
            nvme_delete_ioq(file_desc, nvme_admin_delete_cq, io_cq_id);
            break;
        case 6:
            pr_info("\nTest: Sending IO Write Command through sq_id %d\n", io_sq_id);
            for (dword_t i = 0; i < RW_BUFFER_SIZE / 4; i += 4)
            {
                // *(char *)(write_buffer + i) = BYTE_RAND();
                *(dword_t *)(write_buffer + i) = i;
            }

            ret = FAILED;
            //wr_slba = rand()%100;
            //wr_nlb = 8*(rand()%10 + 1);
            //buf_size = 4096*(rand()%10 + 1);
            wr_slba = 0; //0x40;
            wr_nlb = 8;
            pr_info("slba:%ld nlb:%d\n", wr_slba, wr_nlb);
            cmd_cnt = 0;
            // for (index = 0; index < (sq_size-1); index++)
            {
                ret = nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
                cmd_cnt++;
                //nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
                //cmd_cnt++;
            }
            if (ret == SUCCEED)
            {
                ioctl_tst_ring_dbl(file_desc, io_sq_id);
                pr_info("Ringing Doorbell for sq_id %d\n", io_sq_id);
                cq_gain(io_cq_id, cmd_cnt, &reap_num);
                // ioctl_tst_ring_dbl(file_desc, 1);
                // cq_gain(io_cq_id, cmd_cnt, &reap_num);
                pr_info("  cq reaped ok! reap_num:%d\n", reap_num);
            }
            break;
        case 7:
            pr_info("\nTest: Sending IO Read Command through sq_id %d\n", io_sq_id);
            wr_slba = 0; //0x40;
            wr_nlb = 32;
            memset(read_buffer, 0, wr_nlb * LBA_DAT_SIZE);
            // pr_info("slba:%ld nlb:%d\n", wr_slba, wr_nlb);
            cmd_cnt = 0;
            // for (index = 0; index < 150; index++)
            {
                // wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
                // wr_nlb = WORD_RAND() % 255 + 1;
                if ((wr_slba + wr_nlb) < g_nvme_ns_info[0].nsze)
                {
                    // nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
                    // cmd_cnt++;
                    ret = nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
                    //nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
                    cmd_cnt++;
                }
            }
            pr_info("Ringing Doorbell for sq_id %d\n", io_sq_id);
            ioctl_tst_ring_dbl(file_desc, io_sq_id);
            cq_gain(io_cq_id, cmd_cnt, &reap_num);
            pr_info("  cq reaped ok! reap_num:%d\n", reap_num);
            // ioctl_tst_ring_dbl(file_desc, 0);
            break;
        case 8:
            //WARNING!
            pr_info("Send IO cmp cmd slba=%ld, this shouldn't be output warning!(FPGA-06 may not work!)\n", wr_slba);
            ioctl_send_nvme_compare(file_desc, io_sq_id, wr_slba, wr_nlb, FUA_DISABLE, read_buffer, wr_nlb * LBA_DAT_SIZE);

            ioctl_tst_ring_dbl(file_desc, io_sq_id);
            cq_gain(io_cq_id, 1, &reap_num);
            pr_info("  cq reaped ok! reap_num:%d\n", reap_num);

            pr_info("Send IO cmp cmd slba=%ld, this should be output warning!\n", wr_slba + 3);
            ioctl_send_nvme_compare(file_desc, io_sq_id, wr_slba + 3, wr_nlb, FUA_DISABLE, read_buffer, wr_nlb * LBA_DAT_SIZE);
            ioctl_tst_ring_dbl(file_desc, io_sq_id);
            cq_gain(io_cq_id, 1, &reap_num);
            pr_info("  cq reaped ok! reap_num:%d\n", reap_num);
            break;
        case 9:
            pr_info("\nwrite_buffer Data:\n");
            mem_disp(write_buffer, wr_nlb * LBA_DAT_SIZE);
            pr_info("\nRead_buffer Data:\n");
            mem_disp(read_buffer, wr_nlb * LBA_DAT_SIZE);
            break;
        case 10:
            dw_cmp(write_buffer, read_buffer, wr_nlb * LBA_DAT_SIZE);
            break;
        case 11:
            test_encrypt_decrypt();
            break;
        case 12:
            test_meta(file_desc);
            break;
        case 13:
            break;
        case 14:
            break;
        case 15:
            io_sq_id = 1;
            io_cq_id = 1;
            pr_info("\nPreparing contig cq_id = %d, cq_size = %d\n", io_cq_id, cq_size);
            if (SUCCEED == nvme_create_contig_iocq(file_desc, io_cq_id, cq_size, ENABLE, io_cq_id))
            {
                pr_info("create succeed\n");
            }

            pr_info("\nPreparing contig sq_id = %d, assoc cq_id = %d, sq_size = %d\n", io_sq_id, io_cq_id, sq_size);
            if (SUCCEED == nvme_create_contig_iosq(file_desc, io_sq_id, io_cq_id, sq_size, MEDIUM_PRIO))
            {
                pr_info("create succeed\n");
            }
            break;
        case 16:
            pr_color(LOG_COLOR_CYAN, "pls enter wr_slba:");
            fflush(stdout);
            scanf("%d", (int *)&wr_slba);
            pr_color(LOG_COLOR_CYAN, "pls enter wr_nlb:");
            fflush(stdout);
            scanf("%d", (int *)&wr_nlb);
            memset(write_buffer, BYTE_RAND(), wr_nlb * LBA_DATA_SIZE(wr_nsid));
            create_meta_buf(file_desc, 0);
            // if (SUCCEED == nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer))
            if(SUCCEED == send_nvme_write_using_metabuff(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, 0,write_buffer))
            {
                if (SUCCEED == nvme_ring_dbl_and_reap_cq(file_desc, io_sq_id, io_cq_id, 1))
                {
                    pr_info("io write succeed\n");
                }
            }
            ioctl_meta_buf_delete(file_desc, 0);
            break;
        case 17:
            memset(read_buffer, 0, wr_nlb * LBA_DATA_SIZE(wr_nsid));
            create_meta_buf(file_desc, 0);
            // if (SUCCEED == nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer))
            if(SUCCEED == send_nvme_read_using_metabuff(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, 0,read_buffer))
            {
                if (SUCCEED == nvme_ring_dbl_and_reap_cq(file_desc, io_sq_id, io_cq_id, 1))
                {
                    pr_info("io read succeed\n");
                }
            }
            ioctl_meta_buf_delete(file_desc, 0);
            break;
        case 18:
            pr_color(LOG_COLOR_CYAN, "pls enter loop cnt:");
            fflush(stdout);
            scanf("%d", &test_loop);
            while (test_loop--)
            {
                for (uint32_t ns_idx = 0; ns_idx < g_nvme_dev.id_ctrl.nn; ns_idx++)
                {
                    wr_nsid = ns_idx + 1;
                    wr_slba = 0;
                    wr_nlb = BYTE_RAND() % 32;
                    memset(read_buffer, 0, RW_BUFFER_SIZE);
                    memset(write_buffer, BYTE_RAND(), wr_nlb * LBA_DATA_SIZE(wr_nsid));
                    pr_info("sq_id:%d nsid:%d lbads:%d slba:%ld nlb:%d\n", io_sq_id, wr_nsid, LBA_DATA_SIZE(wr_nsid), wr_slba, wr_nlb);
                    cmd_cnt = 0;
                    ret = nvme_io_write_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, write_buffer);
                    cmd_cnt++;
                    if (ret == SUCCEED)
                    {
                        ioctl_tst_ring_dbl(file_desc, io_sq_id);
                        cq_gain(io_cq_id, cmd_cnt, &reap_num);
                        pr_info("  cq reaped ok! reap_num:%d\n", reap_num);
                    }
                    cmd_cnt = 0;
                    
                    data_len = 40 * 4;
                    pr_info("send_maxio_fwdma_wr\n");
                    //memset((uint8_t *)write_buffer, rand() % 0xff, data_len);
                    fwdma_parameter.addr = write_buffer;
                    fwdma_parameter.cdw10 = data_len;  //data_len
                    fwdma_parameter.cdw11 = 0x40754C0; //axi_addr
                    nvme_maxio_fwdma_wr(file_desc, &fwdma_parameter);
                    ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
                    cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
                    pr_info("\nfwdma wr cmd send done!\n");

                    ret = nvme_io_read_cmd(file_desc, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, read_buffer);
                    cmd_cnt++;
                    if (ret == SUCCEED)
                    {
                        ioctl_tst_ring_dbl(file_desc, io_sq_id);
                        cq_gain(io_cq_id, cmd_cnt, &reap_num);
                        pr_info("  cq reaped ok! reap_num:%d\n", reap_num);
                    }
                    if (SUCCEED == dw_cmp(write_buffer, read_buffer, wr_nlb * LBA_DATA_SIZE(wr_nsid)))
                    {
                        pr_color(LOG_COLOR_GREEN, "dw_cmp pass!\n");
                    }
                }
            }
            break;
        case 19:
            pr_info("host2reg tets send_maxio_fwdma_rd\n");
            data_len = 4 * 4;
            fwdma_parameter.addr = read_buffer;
            fwdma_parameter.cdw10 = data_len;  //data_len
            fwdma_parameter.cdw11 = 0x4055500; //axi_addr
            //fwdma_parameter.cdw12 |= (1<<0);               //flag bit[0] crc chk,
            //fwdma_parameter.cdw12 |= (1<<1);               //flag bit[1] hw data chk(only read)
            fwdma_parameter.cdw12 |= (1 << 2); //flag bit[2] dec chk,
            nvme_maxio_fwdma_rd(file_desc, &fwdma_parameter);
            ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
            cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
            pr_info("\tfwdma wr cmd send done!\n");
            pr_info("host2reg tets send_maxio_fwdma_rd\n");
            //memset((uint8_t *)write_buffer, rand() % 0xff, data_len);
            fwdma_parameter.addr = read_buffer;
            fwdma_parameter.cdw10 = data_len;  //data_len
            fwdma_parameter.cdw11 = 0x4055500; //axi_addr
            //fwdma_parameter.cdw12 |= (1<<0);              //flag bit[0] crc chk,
            fwdma_parameter.cdw12 &= ~(1 << 2); //flag bit[2] enc chk,

            nvme_maxio_fwdma_wr(file_desc, &fwdma_parameter);
            ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
            cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
            pr_info("\tfwdma wr cmd send done!\n");
            break;
        case 20:
            i = 10;
            memset((uint8_t *)&fwdma_parameter, 0, sizeof(fwdma_parameter));
            // pr_color(LOG_COLOR_CYAN,"pls enter loop cnt:");
            // fflush(stdout);
            // scanf("%d", &i);
            while (i--)
            {
                for (index = 0; index < 3000; index++)
                {
                    fwdma_parameter.addr = write_buffer;
                    fwdma_parameter.cdw10 = 4096; //data_len
                    // fwdma_parameter.cdw11 = 0x4078000;              //axi_addr
                    // fwdma_parameter.cdw12 |= (1<<0);                //flag bit[0] crc chk,
                    // fwdma_parameter.cdw12 &= ~(1<<1);             //flag bit[1] hw data chk(only read)
                    // fwdma_parameter.cdw12 |= (1<<2);                //flag bit[2] enc chk,
                    nvme_maxio_fwdma_wr(file_desc, &fwdma_parameter);
                }
                ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
                cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
                pr_info("\nfwdma wr cmd send done!\n");
            }

            break;
        case 21:
            pr_info("send_maxio_fwdma_rd\n");
            data_len = 40 * 4;
            fwdma_parameter.addr = read_buffer;
            fwdma_parameter.cdw10 = data_len;  //data_len
            fwdma_parameter.cdw11 = 0x40754C0; //axi_addr
            //fwdma_parameter.cdw12 |= (1<<0);               //flag bit[0] crc chk,
            //fwdma_parameter.cdw12 |= (1<<1);               //flag bit[1] hw data chk(only read)
            // fwdma_parameter.cdw12 |= (1<<2);                 //flag bit[2] dec chk,
            nvme_maxio_fwdma_rd(file_desc, &fwdma_parameter);
            ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
            cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);
            pr_info("\nfwdma wr cmd send done!\n");

            dw_cmp(write_buffer, read_buffer, data_len);
            break;
        case 22:
            for (index = 0; index < 400; index++)
            {
                pr_info("send_fwdma_wr/rd test cnt:%d\n", index);
                wr_nlb = (BYTE_RAND() % 8) + 1;
                pr_info(" nlb:%d\n", wr_nlb);
                memset((uint8_t *)write_buffer, rand() % 0xff, wr_nlb * LBA_DAT_SIZE);
                fwdma_parameter.cdw10 = wr_nlb * LBA_DAT_SIZE; //data_len
                fwdma_parameter.cdw11 = 0x40754C0;             //axi_addr
                fwdma_parameter.cdw12 |= (1 << 0);             //flag bit[0] crc chk,
                fwdma_parameter.cdw12 |= (1 << 1);             //flag bit[1] hw data chk(only read)
                fwdma_parameter.cdw12 |= (1 << 2);             //flag bit[2] enc/dec chk,

                fwdma_parameter.addr = write_buffer;
                nvme_maxio_fwdma_wr(file_desc, &fwdma_parameter);
                ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
                cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);

                fwdma_parameter.addr = read_buffer;
                nvme_maxio_fwdma_rd(file_desc, &fwdma_parameter);
                ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
                cq_gain(ADMIN_QUEUE_ID, 1, &reap_num);

                if (FAILED == dw_cmp(write_buffer, read_buffer, wr_nlb * LBA_DAT_SIZE))
                {
                    pr_info("\nwrite_buffer Data:\n");
                    mem_disp(write_buffer, wr_nlb * LBA_DAT_SIZE);
                    pr_info("\nRead_buffer Data:\n");
                    mem_disp(read_buffer, wr_nlb * LBA_DAT_SIZE);
                    break;
                }
            }
            break;
        case 29:
            test_4_peak_power();
            break;
        /****************************************************************/
        case 30:
            case_queue_create_q_size();
            break;
        case 31:
            case_queue_delete_q();
            break;
        case 32:
            case_queue_sq_cq_match();
            break;
        case 33:
            case_iocmd_write_read();
            break;
        case 34:
            break;
        case 35:
            break;
        case 36:
            case_queue_abort();
            break;
        case 37:
            case_iocmd_fw_io_cmd();
            break;
        case 38:
            break;
        case 39:
            break;
        case 40:
            case_queue_cq_int_all();
            break;
        case 41:
            case_queue_cq_int_all_mask();
            break;
        case 42:
            case_queue_cq_int_msi_multi_mask();
            break;
        case 43:
            case_queue_cq_int_msix_mask();
            break;
        case 44:
            break;
        case 45:
            break;
        case 46:
            break;
        case 47:
            case_queue_cq_int_coalescing();
            break;
        case 48:
            case_command_arbitration();
            break;
        case 49:
            case_resets_link_down();
            break;
        case 50:
            case_resets_random_all();
            break;
        case 51:
            break;
        case 52:
            case_queue_admin();
            break;
        case 53:
            case_nvme_boot_partition();
            break;
        case 54:
            case_register_test();
            break;
        case 55:
            pr_color(LOG_COLOR_CYAN, "test_0_full_disk_wr.pls enter loop cnt:");
            fflush(stdout);
            scanf("%d", &cmd_cnt);
            while (cmd_cnt--)
            {
                test_0_full_disk_wr();
            }
            break;
        case 56:
            test_1_fused();
            break;
        case 57:
            test_2_mix_case();
            break;
        case 58:
            test_3_adm_wr_cache_fua();
            break;
        case 59:
            test_5_fua_wr_rd_cmp();
            break;
        case 60:
            test_6_all_ns_lbads_test();
            break;

            /*--------------------------------------------------------------------------------------------*/
        case 77:
            // set LTR disable
            system("setpci -s 2:0.0  99.b=00");
            break;

        case 78:
            pr_info("set to D0 state\n");
            set_power_state(g_nvme_dev.pmcap_ofst, D0);
            break;

        case 79:
            pr_info("set to D3 state\n");
            set_power_state(g_nvme_dev.pmcap_ofst, D3hot);
            break;

        case 80:
            case_pcie_link_speed_width_step();
            break;

        case 81:
            case_pcie_link_speed_width_cyc();
            break;

        case 82:
            case_pcie_reset_single();
            break;

        case 83:
            case_pcie_reset_cyc();
            break;

        case 84:
            case_pcie_low_power_L0s_L1_step();
            break;

        case 85:
            case_pcie_low_power_L0s_L1_cyc();
            break;

        case 86:
            case_pcie_low_power_L1sub_step();
            break;

        case 87:
            case_pcie_PM();
            break;

        case 88:
            case_pcie_low_power_measure();
            break;
        case 89:
            case_pcie_low_power_pcipm_l1sub();
            break;
        case 90:
            case_pcie_MPS();
            break;
        case 91:
            case_pcie_MRRS();
            break;

        case 98:
            pr_color(LOG_COLOR_CYAN, "pcie_set_width:");
            fflush(stdout);
            scanf("%d", &test_loop);
            pcie_set_width(test_loop);
            pcie_retrain_link();
            g_nvme_dev.link_width = test_loop;
            break;
        case 99:
            pr_color(LOG_COLOR_CYAN, "pls enter loop cnt:");
            fflush(stdout);
            scanf("%d", &test_loop);
            while (test_loop--)
            {
                pcie_random_speed_width();
            }
            break;
        case 100:
            iocmd_cstc_rdy_test();
            break;
        case 101:
            reg_bug_trace();
            break;

        case 255:
            pr_color(LOG_COLOR_CYAN, "pls enter auto loop cnt:");
            fflush(stdout);
            int loop = 0;
            scanf("%d", &test_loop);
            while (test_loop--)
            {
                loop++;
                pr_color(LOG_COLOR_CYAN, "auto_case_loop_cnt:%d\r\n",loop);
                u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
                pr_color(LOG_COLOR_CYAN, "\nCurrent link status: Gen%d, X%d\n", u32_tmp_data & 0x0F, (u32_tmp_data >> 4) & 0x3F);
                if(test_list_exe(TestCaseList, ARRAY_SIZE(TestCaseList)))
                {
                    break;
                }
                random_list(TestCaseList, ARRAY_SIZE(TestCaseList));
                // pcie_random_speed_width();
            }
            pr_color(LOG_COLOR_CYAN, "auto_case_run_loop:%d\r\n",loop);
            
            break;
        default:
            if (test_case < 256)
                pr_err("Error case number! please try again:\n %s", DISP_HELP);
            break;
        }
    } while (test_case < 256);
    pr_err("test_case num should < 256, Now test will Exiting...\n");
    pr_info("\nCalling Dump Metrics to closefile\n");
    ioctl_dump(file_desc, closefile);

    /* Exit gracefully */
    pr_info("\nNow Exiting gracefully....\n");
    ioctl_disable_ctrl(file_desc, NVME_ST_DISABLE_COMPLETE);
    set_irqs(file_desc, INT_NONE, 0);
    test_mem_free();
    pr_info("\n\n****** END OF TEST ******\n\n");
    close(file_desc);
    return 0;
}

