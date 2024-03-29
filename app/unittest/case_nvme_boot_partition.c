#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

#include "libbase.h"
#include "libnvme.h"

#include "test.h"
#include "unittest.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"

static int test_flag = 0;
static uint32_t test_loop = 0;
static void *boot_buffer;

static int rd_wr_boot_part_ccen_0(void);
static int rd_wr_boot_part_ccen_1(void);

static SubCaseHeader_t sub_case_header = {
    "case_nvme_boot_partition",
    "this case will tests Boot Partitions feature\r\n",
    NULL,
    NULL,
};

static SubCase_t sub_case_list[] = {
    SUB_CASE(rd_wr_boot_part_ccen_0, "ccen=0: Reading/Writing to a Boot Partition"),
    SUB_CASE(rd_wr_boot_part_ccen_1, "ccen=1: Reading/Writing to a Boot Partition"),
};

static int case_nvme_boot_partition(struct nvme_tool *tool, struct case_data *priv)
{
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t round_idx = 0;
    test_loop = 1;
    pr_info("\ntest will loop number: %d\n", test_loop);
    for (round_idx = 1; round_idx <= test_loop; round_idx++)
    {
        pr_info("\ntest cnt: %d\n", round_idx);
        sub_case_list_exe(&sub_case_header, sub_case_list, ARRAY_SIZE(sub_case_list));
        if (-1 == test_flag)
        {
            pr_err("test_flag == -1\n");
            break;
        }
    }

    nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSIX);

    return test_flag;
}
NVME_CASE_SYMBOL(case_nvme_boot_partition, "?");
NVME_AUTOCASE_SYMBOL(case_nvme_boot_partition);

static int read_one_boot_part(uint32_t bpid, uint32_t bprof, uint32_t bprsz)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    int ret_val = 0;
    uint32_t u32_tmp_data = 0;
    uint32_t try_cnt = 0, try_max;
    u32_tmp_data = 0;
    u32_tmp_data |= bpid << 31;  //Boot Partition Identifier (BPID)
    u32_tmp_data |= bprof << 10; //Boot Partition Read Offset (BPROF)
    u32_tmp_data |= bprsz << 0;  //Boot Partition Read Size (BPRSZ) in multiples of 4KB
    ret_val = nvme_write_ctrl_property(ndev->fd, NVME_REG_BPRSEL, sizeof(uint32_t), (uint8_t *)&u32_tmp_data);
    if (ret_val < 0)
    {
        pr_err("[E] NVME_REG_BPRSEL ret_val:%d!\n", ret_val);
        goto error_out;
    }
    //7. writing to the Boot Partition Read Select(BPRSEL) register.
    ret_val = nvme_read_ctrl_property(ndev->fd, NVME_REG_BPINFO, sizeof(uint32_t), (uint8_t *)&u32_tmp_data);
    if (ret_val < 0)
    {
        pr_err("[E] NVME_REG_BPINFO_OFST1 ret_val:%d! %x\n", ret_val, u32_tmp_data);
        goto error_out;
    }
    try_cnt = 0;
    try_max = (U16_MAX << 4);
    while (((u32_tmp_data >> 24) & 0x3) != 0x2)
    {
        ret_val = nvme_read_ctrl_property(ndev->fd, NVME_REG_BPINFO, sizeof(uint32_t), (uint8_t *)&u32_tmp_data);
        if (ret_val < 0)
        {
            pr_err("[E] NVME_REG_BPINFO_OFST2 ret_val:%d! %x\n", ret_val, u32_tmp_data);
            goto error_out;
        }
        try_cnt++;
        if (try_cnt > try_max)
        {
            pr_err("[E] try_cnt > try_max %x\n", u32_tmp_data);
            goto error_out;
        }
    }
    return 0;
error_out:
    test_flag = -1;
    return -1;
}

int reading_boot_partition(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    int ret_val;
    int fd = 0;
    uint8_t ABPID = 0;
    uint16_t BPSZ = 0;
    uint32_t boot_read_cnt = 0;
    uint32_t bprsz_4k = 1; //4KB uints
    uint32_t idx = 0;
    uint32_t u32_tmp_data = 0;
    uint64_t BMBBA = 0;

    char attr[1024];
    unsigned long phys_addr;

    //3. Get BPINFO.ABPID BPINFO.BPSZ
    ret_val = nvme_read_ctrl_property(ndev->fd, NVME_REG_BPINFO, sizeof(uint32_t), (uint8_t *)&u32_tmp_data);
    if (ret_val < 0)
    {
        pr_err("[E] NVME_REG_BPINFO ret_val:%d!\n", ret_val);
        goto error_out;
    }
    ABPID = (u32_tmp_data & 0x80000000) >> 31;
    //This field defines the size of each Boot Partition in multiples of 128KB.Both Boot Partitions are the same size.
    BPSZ = u32_tmp_data & 0x7fff;
    pr_info("ABPID:%d,BPSZ:%d\n", ABPID, BPSZ);

    //4. Allocate a physically contiguous memory buffer
    if ((fd = open("/sys/class/u-dma-buf/udmabuf0/phys_addr", O_RDONLY)) != -1)
    {
        read(fd, attr, 1024);
        sscanf(attr, "%lx", &phys_addr);
        close(fd);
    }
    pr_info("phys_addr:0x%lx\n", phys_addr);
    bprsz_4k = 1;
    boot_read_cnt = (BPSZ * 128 * 1024) / (bprsz_4k * 4096);
    for (idx = 0; idx < boot_read_cnt; idx++)
    {
        BMBBA = phys_addr + idx * 4096;
        ret_val = nvme_write_ctrl_property(ndev->fd, NVME_REG_BPMBL, sizeof(uint64_t), (uint8_t *)&BMBBA);
        if (ret_val < 0)
        {
            pr_err("[E] NVME_REG_BPMBL ret_val:%d!\n", ret_val);
            goto error_out;
        }
        // pr_info("boot_read_cnt:%d!\n", idx);
        ret_val = read_one_boot_part(ABPID, idx, bprsz_4k);
        if (ret_val < 0)
        {
            pr_err("[E] read_one_boot_part ret_val:%d!\n", ret_val);
            goto error_out;
        }
    }

    pr_info("Boot partition Read done!\n");
    if ((fd = open("/dev/udmabuf0", O_RDWR)) != -1)
    {
        boot_buffer = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        /* Do some read/write access to buf */
        mem_disp((void *)boot_buffer, 256);
        close(fd);
    }
    test_flag = 0;
    return test_flag;
error_out:
    pr_err("[%s]ioctl ret_val:%d!\n", __FUNCTION__, ret_val);
    test_flag = -1;
    return test_flag;
}

int writeing_boot_partition(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    uint32_t reap_num = 0;

    nvme_reinit(ndev, NVME_AQ_MAX_SIZE, NVME_AQ_MAX_SIZE, NVME_INT_MSIX);
    // 1. The host issues a Firmware Image Download command to download the contents of the Boot
    // Partition to the controller. There may be multiple portions of the Boot Partition to download, thus
    // the offset for each portion of the Boot Partition being downloaded is specified in the Firmware Image
    // Download command. Host software shall send the Boot Partition image in order starting with the
    // beginning of the Boot Partition;
    // 4. Allocate a physically contiguous memory buffer
    if (posix_memalign(&boot_buffer, 4096, 128 * 1024))
    {
        pr_err("Memalign Failed\n");
        return test_flag;
    }

    memset((void *)boot_buffer, (uint8_t)rand(), 128 * 1024);
    pr_color(LOG_N_GREEN, "Boot Partition dl_fw,wr_buf_addr:0x%lx\n", (uint64_t)boot_buffer);

    if (0 == nvme_firmware_download(ndev->fd, (128 * 1024 / 4) - 1, 0, (uint8_t *)boot_buffer))
    {
        nvme_ring_sq_doorbell(ndev->fd, 0);
        cq_gain(0, 1, &reap_num);
    }

    // 2. Unlock Boot Partitions for writing (refer to section 8.13.3);

    // 3. The host submits a Firmware Commit command with a Commit Action of 110b which specifies that
    // the downloaded image replaces the contents of the Boot Partition specified in the Boot Partition ID field;
    // nvme_firmware_commit(g_fd, 0, 0x7, 1);

    // 4. The controller completes the Firmware Commit command. The following actions are taken in
    // certain error scenarios:
    // a. If the firmware activation was not successful because the Boot Partition could not be written,
    // then the controller reports an error of Boot Partition Write Prohibited;

    // 5. (Optional) The host reads the contents of the Boot Partition to verify they are correct (refer to section
    // 8.13.1). Host software updates the active Boot Partition ID by issuing a Firmware Commit command
    // with a Commit Action of 111b; and

    // 6. The host locks Boot Partition access to prevent further modification (refer to section 8.13.3).
    free(boot_buffer);
    return test_flag;
}

static int rd_wr_boot_part_ccen_0(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    int ret_val;
    uint64_t cap;
    // if(test_flag == -1)
    //     return test_flag;
    nvme_disable_controller_complete(ndev->fd);

    ret_val = nvme_read_ctrl_cap(ndev->fd, &cap);
    if (ret_val < 0)
    {
        pr_err("failed to read controller capability reg!(%d)\n", ret_val);
        goto error_out;
    }
    if (NVME_CAP_BPS(cap))
    {
        /************************************************************************************************/
        pr_info(".1 Reading from a Boot Partition\n");
        reading_boot_partition();
        /************************************************************************************************/
        /************************************************************************************************/
        pr_info(".2 Writing to a Boot Partition\n");
        writeing_boot_partition();
        reading_boot_partition();
    }
    else
    {
        pr_warn("This Device does not support Boot Partitions!\n");
        goto skip_out;
    }
    test_flag = 0;
    return test_flag;
error_out:
    test_flag = -1;
    return test_flag;
skip_out:
    test_flag = SKIPED;
    return test_flag;
}

static int rd_wr_boot_part_ccen_1(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    int ret_val;
    uint64_t cap;
    // if(test_flag == -1)
    //     return test_flag;
    nvme_enable_controller(ndev->fd);

    ret_val = nvme_read_ctrl_cap(ndev->fd, &cap);
    if (ret_val < 0)
    {
        pr_err("failed to read controller capability reg!(%d)\n", ret_val);
        goto error_out;
    }
    if (NVME_CAP_BPS(cap))
    {
        /************************************************************************************************/
        pr_info(".1 Reading from a Boot Partition\n");
        reading_boot_partition();
        /************************************************************************************************/
        /************************************************************************************************/
        pr_info(".2 Writing to a Boot Partition\n");
        writeing_boot_partition();
        reading_boot_partition();
    }
    else
    {
        pr_warn("This Device does not support Boot Partitions!\n");
        goto skip_out;
    }
    test_flag = 0;
    return test_flag;
error_out:
    test_flag = -1;
    return test_flag;
skip_out:
    test_flag = SKIPED;
    return test_flag;
}
