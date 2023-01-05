
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

#include "dnvme_ioctl.h"
#include "ioctl.h"
#include "pci.h"
#include "queue.h"

#include "common.h"
#include "unittest.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_irq.h"
#include "test_init.h"
#include "case_all.h"

typedef enum _reg_type
{
    RW,
    RO,
    WO,
    W1C,
    W1S,
    RW1S,
    RW1C,
} reg_type;

typedef struct _reg_desc
{
    const uint32_t addr;
    const uint32_t mark;
    uint32_t def_val;
    const uint8_t type;
} reg_desc;

// typedef struct _reg_tab_list
// {
//     reg_desc *ptr;
//     uint32_t size;
// } reg_tab_list;

#define Q_NUM 8
#define CAP_DSTRD 0

//nvme registers
reg_desc nvme_ctrl_reg[] =
    {
        {.addr = 0x0000, .mark = 0xffffffff, .type = RO},
        {.addr = 0x0004, .mark = 0xffffffff, .type = RO},
        {.addr = 0x0008, .mark = 0xffffffff, .type = RO},
        {.addr = 0x000c, .mark = 0xffffffff, .type = RW1S},
        {.addr = 0x0010, .mark = 0xffffffff, .type = RW1C},
        {.addr = 0x0014, .mark = 0xff00000e, .type = RO},
        {.addr = 0x0014, .mark = 0x00fffff1, .type = RW},
        {.addr = 0x0018, .mark = 0xffffffff, .type = RO},
        {.addr = 0x001c, .mark = 0xffffffef, .type = RO},
        {.addr = 0x001c, .mark = 0x00000010, .type = RW1C},
        {.addr = 0x0020, .mark = 0xffffffff, .type = RW},
        {.addr = 0x0024, .mark = 0xf000f000, .type = RO},
        {.addr = 0x0024, .mark = 0x0fff0fff, .type = RW},
        {.addr = 0x0028, .mark = 0x00000fff, .type = RO},
        {.addr = 0x0028, .mark = 0xfffff000, .type = RW},
        {.addr = 0x002c, .mark = 0xffffffff, .type = RW},
        {.addr = 0x0030, .mark = 0x00000fff, .type = RO},
        {.addr = 0x0030, .mark = 0xfffff000, .type = RW},
        {.addr = 0x0034, .mark = 0xffffffff, .type = RW},
        {.addr = 0x0038, .mark = 0xffffffff, .type = RO},
        {.addr = 0x003c, .mark = 0xffffffff, .type = RO},
        {.addr = 0x050, .mark = 0xffffffff, .type = RO},
        {.addr = 0x054, .mark = 0xffffffff, .type = RO},
        {.addr = 0x058, .mark = 0xffffffff, .type = RO},
        {.addr = 0x0e00, .mark = 0xffffffff, .type = RO},
        {.addr = 0x0e04, .mark = 0xffffffff, .type = RO},
        {.addr = 0x0e08, .mark = 0xffffffff, .type = RO},
        {.addr = 0x0e0c, .mark = 0xffffffff, .type = RO},
        {.addr = 0x0e10, .mark = 0xffffffff, .type = RO},
        {.addr = 0x0e14, .mark = 0xffffffff, .type = RO},
        {.addr = 0x0e18, .mark = 0xffffffff, .type = RO},
        {.addr = 0x1000, .mark = 0x0000ffff, .type = RW},
        {.addr = 0x1000, .mark = 0xffff0000, .type = RO},
#if 0
        {0x1000 + (1 * (4 << CAP_DSTRD)), 0, 0, RO},
        {0x1000 + (2 * (4 << CAP_DSTRD)), 0, 0, RO},
        {0x1000 + (3 * (4 << CAP_DSTRD)), 0, 0, RO},
        {0x1000 + (4 * (4 << CAP_DSTRD)), 0, 0, RO},
        {0x1000 + (5 * (4 << CAP_DSTRD)), 0, 0, RO},
        {0x1000 + (6 * (4 << CAP_DSTRD)), 0, 0, RO},
        {0x1000 + (7 * (4 << CAP_DSTRD)), 0, 0, RO},
        {0x1000 + (8 * (4 << CAP_DSTRD)), 0, 0, RO},
#endif
};
//pcie ids registers
reg_desc pcie_ids_reg[] =
    {
        {.addr = 0x0000, .mark = 0xffffffff, .type = RO},
        // {.addr = 0x0004, .mark = 0x0efffab8, .type = RO},
        {.addr = 0x0008, .mark = 0xffffffff, .type = RO},
        // {.addr = 0x000c, .mark = 0xbfffff00, .type = RO},
        // {.addr = 0x0010, .mark = 0x00003fff, .type = RO},
        {.addr = 0x0014, .mark = 0x00fffff1, .type = RW},
        // {.addr = 0x0018, .mark = 0x00000007, .type = RO},
        {.addr = 0x0028, .mark = 0xffffffff, .type = RO},
        {.addr = 0x002c, .mark = 0xffffffff, .type = RO},
};

static int test_flag = SUCCEED;
static uint32_t test_loop = 0;
static uint32_t cmd_cnt = 0;
static uint32_t io_sq_id = 1;
static uint32_t io_cq_id = 1;
static uint32_t cq_size = 4;
static uint32_t sq_size = 64;
static uint64_t wr_slba = 0;
static uint16_t wr_nlb = 8;
static uint32_t wr_nsid = 1;
static uint32_t reap_num = 0;

static int sub_case_nvme_reg_normal(void);
static int sub_case_pcie_reg_normal(void);
static int sub_case_iocmd_nvme_reg(void);
static int sub_case_iocmd_nvme_reg_normal(void);

static SubCaseHeader_t sub_case_header = {
    "case_register_test",
    "this case will tests NVMe register",
    NULL,
    NULL,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(sub_case_nvme_reg_normal, "this case will tests nvme ctrl registers"),
    SUB_CASE(sub_case_pcie_reg_normal, "this case will tests pcie header registers"),
    SUB_CASE(sub_case_iocmd_nvme_reg, "this case tests iocmd and nvme msi register"),
    SUB_CASE(sub_case_iocmd_nvme_reg_normal, "this case tests iocmd and nvme msi register qsize=1024"),
};

void scan_control_reister(void)
{
    int ret;

    for (uint32_t i = 0; i < ARRAY_SIZE(nvme_ctrl_reg); i++)
    {
    	ret = nvme_read_ctrl_property(g_fd, nvme_ctrl_reg[i].addr, 4, 
		&nvme_ctrl_reg[i].def_val);
	if (ret < 0)
		exit(-1);
    }
    for (uint32_t i = 0; i < ARRAY_SIZE(pcie_ids_reg); i++)
    {
        ret = pci_read_config_dword(g_fd, pcie_ids_reg[i].addr, &pcie_ids_reg[i].def_val);
	if (ret < 0)
		exit(-1);
    }
}

int case_register_test(void)
{
    uint32_t round_idx = 0;

    test_loop = 2;
    scan_control_reister();
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
    return test_flag;
}

static uint32_t sub_case_pre(void)
{
    pr_info("==>QID:%d\n", io_sq_id);
    pr_color(LOG_COLOR_PURPLE, "  Create contig cq_id:%d, cq_size = %d\n", io_cq_id, cq_size);
    test_flag |= nvme_create_contig_iocq(g_fd, io_cq_id, cq_size, ENABLE, io_cq_id);

    pr_color(LOG_COLOR_PURPLE, "  Create contig sq_id:%d, assoc cq_id = %d, sq_size = %d\n", io_sq_id, io_cq_id, sq_size);
    test_flag |= nvme_create_contig_iosq(g_fd, io_sq_id, io_cq_id, sq_size, MEDIUM_PRIO);

    return test_flag;
}

static uint32_t sub_case_end(void)
{
    pr_color(LOG_COLOR_PURPLE, "  Deleting SQID:%d,CQID:%d\n", io_sq_id, io_cq_id);
    test_flag |= nvme_delete_ioq(g_fd, nvme_admin_delete_sq, io_sq_id);
    test_flag |= nvme_delete_ioq(g_fd, nvme_admin_delete_cq, io_cq_id);
    return test_flag;
}

static int sub_case_nvme_reg_normal(void)
{
    struct nvme_dev_info *ndev = &g_nvme_dev;
    uint32_t u32_tmp_data = 0, rand_data = 0;
    uint32_t i = 0;
    int ret;
    //nvme_ctrl_reg
    for (i = 0; i < sizeof(nvme_ctrl_reg) / sizeof(reg_desc); i++)
    {
        if (nvme_ctrl_reg[i].type == RO)
        {
            ret = nvme_read_ctrl_property(g_fd, nvme_ctrl_reg[i].addr, 4, &u32_tmp_data);
	    if (ret < 0)
	    	exit(-1);

            u32_tmp_data &= nvme_ctrl_reg[i].mark;
            if ((nvme_ctrl_reg[i].def_val&nvme_ctrl_reg[i].mark) != u32_tmp_data)
            {
                pr_err("addr:0x%08x, mark:0x%08x, read reg_val(0x%08x) != def_val(0x%08x)\n",
                          nvme_ctrl_reg[i].addr,
                          nvme_ctrl_reg[i].mark,
                          u32_tmp_data,
                          nvme_ctrl_reg[i].def_val);
                test_flag = FAILED;
            }
#if 1
            rand_data = rand();
            u32_tmp_data = (rand_data & nvme_ctrl_reg[i].mark);
            // pr_info("addr:0x%08x, rand_data:0x%08x, mark:0x%08x, rand_data&mark:0x%08x\n",
            //          nvme_ctrl_reg[i].addr,rand_data, nvme_ctrl_reg[i].mark, u32_tmp_data);
            nvme_write_ctrl_property(g_fd, nvme_ctrl_reg[i].addr, 4, (uint8_t *)&u32_tmp_data);

            ret = nvme_read_ctrl_property(g_fd, nvme_ctrl_reg[i].addr, 4, &u32_tmp_data);
	    if (ret < 0)
	    	exit(-1);

            u32_tmp_data &= nvme_ctrl_reg[i].mark;
            if ((nvme_ctrl_reg[i].def_val&nvme_ctrl_reg[i].mark) != u32_tmp_data)
            {
                pr_err("\taddr:0x%08x, mark:0x%08x, read reg_val(0x%08x) != def_val(0x%08x)\n",
                          nvme_ctrl_reg[i].addr,
                          nvme_ctrl_reg[i].mark,
                          u32_tmp_data,
                          nvme_ctrl_reg[i].def_val);
                test_flag = FAILED;
            }
#endif
        }
    }
    pr_color(LOG_COLOR_GREEN, "nvme ctrl reg tests done!\n\n");

    nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSIX);

    return test_flag;
}

static int sub_case_pcie_reg_normal(void)
{
    struct nvme_dev_info *ndev = &g_nvme_dev;
    uint32_t u32_tmp_data = 0, rand_data = 0;
    uint32_t i = 0;
    int ret;
    //pcie_ids_reg
    for (i = 0; i < sizeof(pcie_ids_reg) / sizeof(reg_desc); i++)
    {
        if (pcie_ids_reg[i].type == RO)
        {
            ret = pci_read_config_dword(g_fd, pcie_ids_reg[i].addr, &u32_tmp_data);
	    if (ret < 0)
	    	exit(-1);
	    
            u32_tmp_data &= pcie_ids_reg[i].mark;
            if (pcie_ids_reg[i].def_val != u32_tmp_data)
            {
                pr_err("addr:0x%08x, mark:0x%08x, read reg_val(0x%08x) != def_val(0x%08x)\n",
                          pcie_ids_reg[i].addr,
                          pcie_ids_reg[i].mark,
                          u32_tmp_data,
                          pcie_ids_reg[i].def_val);
                test_flag = FAILED;
            }
#if 1
            rand_data = rand();
            u32_tmp_data = (rand_data & pcie_ids_reg[i].mark);
            // pr_info("addr:0x%08x, rand_data:0x%08x, mark:0x%08x, rand_data&mark:0x%08x\n",
            //          pcie_ids_reg[i].addr,rand_data, pcie_ids_reg[i].mark, u32_tmp_data);
            pci_write_config_data(g_fd, pcie_ids_reg[i].addr, 4, (uint8_t *)&u32_tmp_data);

            ret = pci_read_config_dword(g_fd, pcie_ids_reg[i].addr, &u32_tmp_data);
	    if (ret < 0)
	    	exit(-1);
	    
            u32_tmp_data &= pcie_ids_reg[i].mark;
            if (pcie_ids_reg[i].def_val != u32_tmp_data)
            {
                pr_err("\taddr:0x%08x, mark:0x%08x, read reg_val(0x%08x) != def_val(0x%08x)\n",
                          pcie_ids_reg[i].addr,
                          pcie_ids_reg[i].mark,
                          u32_tmp_data,
                          pcie_ids_reg[i].def_val);
                test_flag = FAILED;
            }
#endif
        }
    }
    pr_color(LOG_COLOR_GREEN, "pcie ids reg tests done!\n\n");

    nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSIX);
    return test_flag;
}

static int sub_case_iocmd_nvme_reg(void)
{
    // if(strstr(g_nvme_dev.id_ctrl.mn, "Cougar"))
    // {
    //     return SKIPED;
    // }
    //for (uint32_t index = 1; index <= g_nvme_dev.max_sq_num; index++)
    {
        uint32_t index = 1;
        cq_size = 4;
        sq_size = 64;

        io_sq_id = index;
        io_cq_id = index;

        sub_case_pre();

        wr_nsid = 1;
        wr_slba = 0;
        // wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
        wr_nlb = WORD_RAND() % 32 + 1;

        mem_set(g_write_buf, DWORD_RAND(), wr_nlb * LBA_DATA_SIZE(wr_nsid));
        mem_set(g_read_buf, 0, wr_nlb * LBA_DATA_SIZE(wr_nsid));

        pr_info("sq_id:%d nsid:%d lbads:%d slba:%ld nlb:%d\n", io_sq_id, wr_nsid, LBA_DATA_SIZE(wr_nsid), wr_slba, wr_nlb);

        cmd_cnt = 0;
        for (index = 0; index < (sq_size - 1); index++)
        {
            test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf);
            cmd_cnt++;
        }
        if (test_flag == SUCCEED)
        {
            test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
            sleep(1);
            nvme_msi_register_test();
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
        }
        else
        {
            pr_err("nvme_io_write_cmd");
            goto out;
        }

        // nvme_msi_register_test();
        cmd_cnt = 0;
        for (index = 0; index < (sq_size - 1); index++)
        {
            test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_read_buf);
            cmd_cnt++;
        }
        if (test_flag == SUCCEED)
        {
            test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
            sleep(1);
            nvme_msi_register_test();
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
        }
        else
        {
            pr_err("nvme_io_read_cmd");
            goto out;
        }
        if (dw_cmp(g_write_buf, g_read_buf, wr_nlb * LBA_DATA_SIZE(wr_nsid)))
        {
            test_flag |= FAILED;
            pr_err("dw_cmp");
            goto out;
        }
        sub_case_end();
    }
out:
    return test_flag;
}

static int sub_case_iocmd_nvme_reg_normal(void)
{
    //for (uint32_t index = 1; index <= g_nvme_dev.max_sq_num; index++)
    {
        uint32_t index = 1;

        cq_size = 1024;
        sq_size = 1024;

        io_sq_id = index;
        io_cq_id = index;

        sub_case_pre();

        wr_nsid = 1;
        wr_slba = 0;
        // wr_slba = DWORD_RAND() % (g_nvme_ns_info[0].nsze / 2);
        wr_nlb = WORD_RAND() % 32 + 1;

        mem_set(g_write_buf, DWORD_RAND(), wr_nlb * LBA_DATA_SIZE(wr_nsid));
        mem_set(g_read_buf, 0, wr_nlb * LBA_DATA_SIZE(wr_nsid));

        pr_info("sq_id:%d nsid:%d lbads:%d slba:%ld nlb:%d\n", io_sq_id, wr_nsid, LBA_DATA_SIZE(wr_nsid), wr_slba, wr_nlb);

        cmd_cnt = 0;
        for (index = 0; index < (sq_size - 1); index++)
        {
            test_flag |= nvme_io_write_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_write_buf);
            cmd_cnt++;
        }
        if (test_flag == SUCCEED)
        {
            test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
            sleep(1);
            nvme_msi_register_test();
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
        }
        else
        {
            pr_err("nvme_io_write_cmd");
            goto out;
        }

        // nvme_msi_register_test();
        cmd_cnt = 0;
        for (index = 0; index < (sq_size - 1); index++)
        {
            test_flag |= nvme_io_read_cmd(g_fd, 0, io_sq_id, wr_nsid, wr_slba, wr_nlb, 0, g_read_buf);
            cmd_cnt++;
        }
        if (test_flag == SUCCEED)
        {
            test_flag |= nvme_ring_sq_doorbell(g_fd, io_sq_id);
            sleep(1);
            nvme_msi_register_test();
            test_flag |= cq_gain(io_cq_id, cmd_cnt, &reap_num);
        }
        else
        {
            pr_err("nvme_io_read_cmd");
            goto out;
        }
        if (dw_cmp(g_write_buf, g_read_buf, wr_nlb * LBA_DATA_SIZE(wr_nsid)))
        {
            test_flag |= FAILED;
            pr_err("dw_cmp");
            goto out;
        }
        sub_case_end();
    }
out:
    return test_flag;
}
