#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "dnvme_ioctl.h"
#include "queue.h"
#include "irq.h"

#include "common.h"
#include "unittest.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_irq.h"
#include "test_init.h"

static int test_flag = SUCCEED;
static uint32_t test_loop = 0;
static uint32_t cmd_cnt = 0;
static uint32_t io_sq_id = 1;
static uint32_t io_cq_id = 1;
static uint32_t cq_size = 1024;
static uint32_t sq_size = 1024;
static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint32_t reap_num = 0;

static int sub_case_int_pin(void);
static int sub_case_int_msi_single(void);
static int sub_case_int_msi_multi(void);
static int sub_case_int_msix(void);
static int sub_case_int_n_queue(void);
static int sub_case_multi_cq_map_one_int_vct(void);

static SubCaseHeader_t sub_case_header = {
    "case_queue_cq_int_all",
    "This case will tests all cq interrupt type,pin/msi_single/msi_multi/msix\r\n"
    "\033[33m!!!!!!!!!!!!!!! msi_multi test HOST BIOS MUST OPEN VTD!!!!!!!!!!!!!!!!!\033[0m",
    NULL,
    NULL,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_int_pin, "tests all queue'pin interrupt, send admin and io cmd"),
    SUB_CASE(sub_case_int_msi_single, "tests all queue'msi_single interrupt, send admin and io cmd"),
    SUB_CASE(sub_case_int_msi_multi, "tests all queue'msi_multi interrupt, send admin and io cmd"),
    SUB_CASE(sub_case_int_msix, "tests all queue'msix interrupt, send admin and io cmd"),
    SUB_CASE(sub_case_int_n_queue, "tests all queue interrupt"),
    SUB_CASE(sub_case_multi_cq_map_one_int_vct, "tests multi_cq_map_one_int_vct"),
};

int case_queue_cq_int_all(void)
{
    struct nvme_dev_info *ndev = &g_nvme_dev;
    uint32_t round_idx = 0;

    test_loop = 10;

    pr_info("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        pr_info("\ntest cnt: %d\n", round_idx);
        sub_case_list_exe(&sub_case_header, sub_case_list, ARRAY_SIZE(sub_case_list));
        if (FAILED == test_flag)
        {
            pr_err("test_flag == FAILED\n");
            break;
        }
    }
    nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSIX);
    return test_flag;
}

static int sub_case_int_pin(void)
{
    struct nvme_dev_info *ndev = &g_nvme_dev;
    uint32_t index = 0;
    struct create_cq_parameter cq_parameter = {0};
    struct create_sq_parameter sq_parameter = {0};

    nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_PIN);

    cq_parameter.cq_id = io_cq_id;
    cq_parameter.cq_size = cq_size;
    cq_parameter.contig = 1;
    cq_parameter.irq_en = 1;
    cq_parameter.irq_no = 0; // 0's based
    pr_info("create cq: %d, irq_no:%d\n", io_cq_id, cq_parameter.irq_no);
    test_flag |= create_iocq(g_fd, &cq_parameter);
    test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    pr_info("create sq: %d, assoc cq: %d\n", io_sq_id, io_cq_id);
    sq_parameter.cq_id = io_cq_id;
    sq_parameter.sq_id = io_sq_id;
    sq_parameter.sq_size = sq_size;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;
    test_flag |= create_iosq(g_fd, &sq_parameter);
    test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /**********************************************************************/
    wr_slba = 0;
    wr_nlb = 128;
    cmd_cnt = 0;
    for (index = 0; index < 100; index++)
    {
        wr_slba = DWORD_RAND() % (ndev->nss[0].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        if ((wr_slba + wr_nlb) < ndev->nss[0].nsze)
        {
            test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf);
            cmd_cnt++;
            test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_read_buf);
            cmd_cnt++;
        }
    }
    test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
    test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);

    /**********************************************************************/
    test_flag |= ioctl_delete_ioq(g_fd, nvme_admin_delete_sq, io_sq_id);
    test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    test_flag |= ioctl_delete_ioq(g_fd, nvme_admin_delete_cq, io_cq_id);
    test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
    /**********************************************************************/
    return test_flag;
}

static int sub_case_int_msi_single(void)
{
    struct nvme_dev_info *ndev = &g_nvme_dev;
    uint32_t index = 0;
    struct create_cq_parameter cq_parameter = {0};
    struct create_sq_parameter sq_parameter = {0};

    nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSI_SINGLE);

    cq_parameter.cq_id = io_cq_id;
    cq_parameter.cq_size = cq_size;
    cq_parameter.contig = 1;
    cq_parameter.irq_en = 1;
    cq_parameter.irq_no = 0; // 0's based
    pr_info("create cq: %d, irq_no:%d\n", io_cq_id, cq_parameter.irq_no);
    test_flag |= create_iocq(g_fd, &cq_parameter);
    test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    pr_info("create sq: %d, assoc cq: %d\n", io_sq_id, io_cq_id);
    sq_parameter.cq_id = io_cq_id;
    sq_parameter.sq_id = io_sq_id;
    sq_parameter.sq_size = sq_size;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;
    test_flag |= create_iosq(g_fd, &sq_parameter);
    test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    /**********************************************************************/
    wr_slba = 0;
    wr_nlb = 128;
    cmd_cnt = 0;
    for (index = 0; index < 100; index++)
    {
        wr_slba = DWORD_RAND() % (ndev->nss[0].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        if ((wr_slba + wr_nlb) < ndev->nss[0].nsze)
        {
            test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf);
            cmd_cnt++;
            test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_read_buf);
            cmd_cnt++;
        }
    }
    if (cmd_cnt > 0)
    {
        test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
        test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    }

    /**********************************************************************/
    test_flag |= ioctl_delete_ioq(g_fd, nvme_admin_delete_sq, io_sq_id);
    test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    test_flag |= ioctl_delete_ioq(g_fd, nvme_admin_delete_cq, io_cq_id);
    test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
    /**********************************************************************/
    return test_flag;
}

static int sub_case_int_msi_multi(void)
{
    struct nvme_dev_info *ndev = &g_nvme_dev;
    uint32_t index = 0;
    #ifdef AMD_MB_EN
        return SKIPED;
    #endif
    struct create_cq_parameter cq_parameter = {0};
    struct create_sq_parameter sq_parameter = {0};
    nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSI_MULTI);

    /**********************************************************************/
    cq_parameter.cq_id = io_cq_id;
    cq_parameter.cq_size = cq_size;
    cq_parameter.contig = 1;
    cq_parameter.irq_en = 1;
    cq_parameter.irq_no = io_cq_id;
    pr_info("create cq: %d, irq_no:%d\n", io_cq_id, cq_parameter.irq_no);
    test_flag |= create_iocq(g_fd, &cq_parameter);
    test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    pr_info("create sq: %d, assoc cq: %d\n", io_sq_id, io_cq_id);
    sq_parameter.cq_id = io_cq_id;
    sq_parameter.sq_id = io_sq_id;
    sq_parameter.sq_size = sq_size;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;
    test_flag |= create_iosq(g_fd, &sq_parameter);
    test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
    /**********************************************************************/
    wr_slba = 0;
    wr_nlb = 128;
    cmd_cnt = 0;
    for (index = 0; index < 100; index++)
    {
        wr_slba = DWORD_RAND() % (ndev->nss[0].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        if ((wr_slba + wr_nlb) < ndev->nss[0].nsze)
        {
            test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf);
            cmd_cnt++;
            test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_read_buf);
            cmd_cnt++;
        }
    }
    if (cmd_cnt > 0)
    {
        test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
        test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    }
    //pr_info("  cq:%d io cmd reaped ok! reap_num:%d\n", io_cq_id, reap_num);

    /**********************************************************************/
    test_flag |= ioctl_delete_ioq(g_fd, nvme_admin_delete_sq, io_sq_id);
    test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    test_flag |= ioctl_delete_ioq(g_fd, nvme_admin_delete_cq, io_cq_id);
    test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
    return test_flag;
}

static int sub_case_int_msix(void)
{
    struct nvme_dev_info *ndev = &g_nvme_dev;
    uint32_t index = 0;
    struct create_cq_parameter cq_parameter = {0};
    struct create_sq_parameter sq_parameter = {0};

    nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSIX);

    /**********************************************************************/
    cq_parameter.cq_id = io_cq_id;
    cq_parameter.cq_size = cq_size;
    cq_parameter.contig = 1;
    cq_parameter.irq_en = 1;
    cq_parameter.irq_no = io_cq_id;
    pr_info("create cq: %d, irq_no:%d\n", io_cq_id, cq_parameter.irq_no);
    test_flag |= create_iocq(g_fd, &cq_parameter);
    test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    pr_info("create sq: %d, assoc cq: %d\n", io_sq_id, io_cq_id);
    sq_parameter.cq_id = io_cq_id;
    sq_parameter.sq_id = io_sq_id;
    sq_parameter.sq_size = sq_size;
    sq_parameter.contig = 1;
    sq_parameter.sq_prio = MEDIUM_PRIO;
    test_flag |= create_iosq(g_fd, &sq_parameter);
    test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
    /**********************************************************************/
    wr_slba = 0;
    wr_nlb = 128;
    cmd_cnt = 0;
    for (index = 0; index < 100; index++)
    {
        wr_slba = DWORD_RAND() % (ndev->nss[0].nsze / 2);
        wr_nlb = WORD_RAND() % 255 + 1;
        if ((wr_slba + wr_nlb) < ndev->nss[0].nsze)
        {
            test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf);
            cmd_cnt++;
            test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_read_buf);
            cmd_cnt++;
        }
    }
    if (cmd_cnt > 0)
    {
        test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
        test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
    }
    /**********************************************************************/
    test_flag |= ioctl_delete_ioq(g_fd, nvme_admin_delete_sq, io_sq_id);
    test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);

    test_flag |= ioctl_delete_ioq(g_fd, nvme_admin_delete_cq, io_cq_id);
    test_flag |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    test_flag |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    pr_div("  cq:%d reaped ok! reap_num:%d\n", NVME_AQ_ID, reap_num);
    return test_flag;
}

static int sub_case_int_n_queue(void)
{
    struct nvme_dev_info *ndev = &g_nvme_dev;
    struct nvme_sq_info *sqs = ndev->iosqs;
    uint32_t index = 0;
	enum nvme_irq_type type;
	int ret;

	type = nvme_select_irq_type_random();
	ret = nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, type);
	if (ret < 0)
		return ret;

    ret = nvme_create_all_ioq(ndev, 0);
    if (ret < 0)
        return ret;

    uint8_t queue_num = BYTE_RAND() % g_nvme_dev.max_sq_num + 1;

    wr_slba = DWORD_RAND() % (ndev->nss[0].nsze / 2);
    wr_nlb = WORD_RAND() % 255 + 1;
    for (uint16_t i = 0; i < queue_num; i++)
    {
        io_sq_id = sqs[i].sqid;
        sqs[i].cmd_cnt = 0;

        for (index = 0; index < 100; index++)
        {
            if ((wr_slba + wr_nlb) < ndev->nss[0].nsze)
            {
                test_flag |= nvme_send_iocmd(g_fd, true, io_sq_id, wr_nsid, wr_slba, wr_nlb, g_write_buf);
                sqs[i].cmd_cnt++;
            }
        }
    }
    for (uint16_t i = 0; i < queue_num; i++)
    {
        io_sq_id = sqs[i].sqid;
        test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
    }
    for (uint16_t i = 0; i < queue_num; i++)
    {
        io_cq_id = sqs[i].cqid;
        test_flag |= cq_gain(io_cq_id, sqs[i].cmd_cnt, &reap_num);
    }

    nvme_delete_all_ioq(ndev);
    return test_flag;
}

static int sub_case_multi_cq_map_one_int_vct(void)
{
    struct nvme_dev_info *ndev = &g_nvme_dev;
    struct nvme_sq_info *sqs = ndev->iosqs;
    uint32_t index = 0;
	enum nvme_irq_type type;
	int ret;

	type = nvme_select_irq_type_random();
	ret = nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, type);
	if (ret < 0)
		return ret;

    ret = nvme_create_all_ioq(ndev, NVME_CIOQ_F_CQS_BIND_SINGLE_IRQ);
    if (ret < 0)
        return ret;
    
    uint8_t queue_num = BYTE_RAND() % g_nvme_dev.max_sq_num + 1;

    wr_slba = DWORD_RAND() % (ndev->nss[0].nsze / 2);
    wr_nlb = WORD_RAND() % 255 + 1;
    for (uint16_t i = 0; i < queue_num; i++)
    {
        io_sq_id = sqs[i].sqid;
        sqs[i].cmd_cnt = 0;

        for (index = 0; index < 200; index++)
        {
            if ((wr_slba + wr_nlb) < ndev->nss[0].nsze)
            {
                test_flag |= nvme_send_iocmd(g_fd, true, io_sq_id, wr_nsid, wr_slba, wr_nlb, g_write_buf);
                sqs[i].cmd_cnt++;
            }
        }
    }
    for (uint16_t i = 0; i < queue_num; i++)
    {
        io_sq_id = sqs[i].sqid;
        test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
    }
    for (uint16_t i = 0; i < queue_num; i++)
    {
        io_cq_id = sqs[i].cqid;
        test_flag |= cq_gain(io_cq_id, sqs[i].cmd_cnt, &reap_num);
    }

    nvme_delete_all_ioq(ndev);
    return test_flag;
}
