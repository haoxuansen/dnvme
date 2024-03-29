/**
 * @file test_send_cmd.c
 * @author yumeng (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2019-10-08
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <ctype.h>
#include <malloc.h>
#include <stdint.h>
#include <stdlib.h>

#include "libbase.h"
#include "libnvme.h"

#include "test.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"

/* CMD to delete IO Queue */
int ioctl_delete_ioq(int g_fd, uint8_t opcode, uint16_t qid)
{
	int ret_val = -1;
	struct nvme_delete_queue del_q_cmd = {
		.opcode = opcode,
		.qid = qid,
		.rsvd1[0] = 0x00,
	};

	struct nvme_64b_cmd user_cmd = {
		.sqid = 0,
		.cmd_buf_ptr = (u_int8_t *)&del_q_cmd,
		.data_buf_ptr = NULL,
		.data_dir = DMA_FROM_DEVICE,
	};
	ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
	if (ret_val < 0)
	{
		if (opcode == nvme_admin_delete_sq)
		{
			pr_err("Delete_SQ:%d failed %d\n", qid, ret_val);
		}
		else if (opcode == nvme_admin_delete_cq)
		{
			pr_err("Delete_CQ:%d failed %d\n", qid, ret_val);
		}
		return ret_val;
	}
	return 0;
}

//******************************************************************************************
/**
 * @brief   CMD to send nvme_compare command
 * @note   
 * @param  g_fd: 
 * @param  sq_id: 
 * @param  slba: 
 * @param  nlb: 
 * @param  fua_sts: 
 * @param  *addr: 
 * @param  buf_size: 
 * @retval 
 */
int ioctl_send_nvme_compare(int g_fd, uint16_t sq_id, uint64_t slba, uint16_t nlb,
                            enum fua_sts fua_sts, void *addr, uint32_t buf_size)
{
    int ret_val = -1;
    struct nvme_64b_cmd user_cmd = {0};
    struct nvme_rw_command nvme_compare = {0};

    /* Fill the command for nvme_compare*/
    nvme_compare.opcode = nvme_cmd_compare;
    nvme_compare.flags = 0;
    nvme_compare.nsid = 1;
    nvme_compare.metadata = 0;
    nvme_compare.slba = slba;
    nlb = nlb - 1; //0'base
    nvme_compare.length = nlb;
    if (fua_sts == FUA_ENABLE)
    {
        nvme_compare.control |= NVME_RW_FUA;
    }

    /* Fill the user command */
    user_cmd.sqid = sq_id;
    user_cmd.bit_mask = (NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST |
                         NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST);
    user_cmd.cmd_buf_ptr = (u_int8_t *)&nvme_compare;
    user_cmd.data_buf_size = buf_size;
    user_cmd.data_buf_ptr = addr;
    user_cmd.data_dir = 0;

    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("command sq_id: %d, slba: 0x%x, nlb: %d Sending Command Failed!\n",
                  sq_id, (uint32_t)slba, nlb + 1);
        return -1;
    }
    else
    {
        pr_div("command sq_id: %d, slba: 0x%x, nlb: %d Command sent succesfully\n\n",
                 sq_id, (uint32_t)slba, nlb + 1);
    }
    return 0;
}

/**
 * @brief  send maxio vendor cmd: fw dma read
 * @note   
 * @param  g_fd: 
 * @param  *addr: 
 * @param  size: 
 * @param  crcc_len: 
 * @param  axi_addr: 
 * @retval 
 */
int nvme_maxio_fwdma_rd(int g_fd, struct fwdma_parameter *fwdma_parameter)
{
    int ret_val = -1;
    /* Fill the command for nvme_compare*/
    struct nvme_common_command maxio_fwdma_rd = {
        .opcode = nvme_admin_maxio_fwdma_read, //nvme_admin_maxio_read,
        .nsid = 1,
        .cdw10 = fwdma_parameter->cdw10,
        .cdw11 = fwdma_parameter->cdw11,
        .cdw12 = fwdma_parameter->cdw12,
        .cdw13 = fwdma_parameter->cdw13,
        .cdw14 = fwdma_parameter->cdw14,
        .cdw15 = fwdma_parameter->cdw15,
    };
    /* Fill the user command */
    struct nvme_64b_cmd user_cmd = {
        .sqid = 0,
        .bit_mask = (NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST |
                     NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST),
        .cmd_buf_ptr = (u_int8_t *)&maxio_fwdma_rd,
        .data_buf_size = fwdma_parameter->cdw10,
        .data_buf_ptr = fwdma_parameter->addr,
        .data_dir = 0,
    };

    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("NVME_VENDOR_FWDMA_RD Sending Command Failed!\n");
        return -1;
    }
    else
    {
        pr_div("NVME_VENDOR_FWDMA_RD Sending Command Succesfully!\n");
    }
    return 0;
}
/**
 * @brief  send maxio vendor cmd: fw dma write
 * @note   
 * @param  g_fd: 
 * @param  fwdma_parameter: 
 * @retval 
 */
int nvme_maxio_fwdma_wr(int g_fd, struct fwdma_parameter *fwdma_parameter)
{
    int ret_val = -1;
    /* Fill the command for nvme_compare*/
    struct nvme_common_command maxio_fwdma_wr = {
        .opcode = nvme_admin_maxio_fwdma_write, //nvme_admin_maxio_write,
        .nsid = 1,
        .cdw10 = fwdma_parameter->cdw10,
        .cdw11 = fwdma_parameter->cdw11,
        .cdw12 = fwdma_parameter->cdw12,
        .cdw13 = fwdma_parameter->cdw13,
        .cdw14 = fwdma_parameter->cdw14,
        .cdw15 = fwdma_parameter->cdw15,
    };

    /* Fill the user command */
    struct nvme_64b_cmd user_cmd = {
        .sqid = 0,
        .bit_mask = (NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST | NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST),
        .cmd_buf_ptr = (u_int8_t *)&maxio_fwdma_wr,
        .data_buf_size = fwdma_parameter->cdw10,
        .data_buf_ptr = fwdma_parameter->addr,
        .data_dir = 2,
    };

    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("NVME_VENDOR_FWDMA_WR Sending Command Failed!\n");
        return -1;
    }
    else
    {
        pr_div("NVME_VENDOR_FWDMA_WR Sending Command Succesfully!\n");
    }
    return 0;
}

/**
 * @brief send nvme_firmware_commit cmd
 * 
 * @param g_fd 
 * @param bpid Boot Partition ID (BPID):
    Specifies the Boot Partition that shall be used for the Commit Action, if applicable.
 * @param ca Commit Action:
    Value Definition
    000b    Downloaded image replaces the existing image, if any, in the specified
                Firmware Slot. The newly placed image is not activated.
    001b    Downloaded image replaces the existing image, if any, in the specified
                Firmware Slot. The newly placed image is activated at the next Controller Level Reset.
    010b    The existing image in the specified Firmware Slot is activated at the next Controller Level Reset.
    011b    Downloaded image replaces the existing image, if any, in the specified
                Firmware Slot and is then activated immediately. If there is not a newly
                downloaded image, then the existing image in the specified firmware slot is activated immediately.
    100b to 101b Reserved
    110b    Downloaded image replaces the Boot Partition specified by the Boot Partition ID field.
    111b    Mark the Boot Partition specified in the BPID field as active and update BPINFO.ABPID.
 * @param fs Firmware Slot (FS):
     Specifies the firmware slot that shall be used for the Commit Action, if
    applicable. If the value specified is 0h, then the controller shall choose the firmware slot (i.e., slot
    1 to slot 7) to use for the operation.
 * @return int 
 */
int nvme_firmware_commit(int g_fd, uint8_t bpid, uint8_t ca, uint8_t fs)
{
    int ret_val = -1;
    /* Fill the command for nvme_compare*/
    struct nvme_common_command firmware_commit = {
        .opcode = nvme_admin_activate_fw,
        .nsid = 1,
        .cdw10 = ((uint32_t)bpid << 31) | ((uint32_t)((ca & 0x7) << 3)) | ((uint32_t)(fs & 0x7)),
    };

    /* Fill the user command */
    struct nvme_64b_cmd user_cmd = {
        .sqid = 0,
        .bit_mask = (NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST | NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST),
        .cmd_buf_ptr = (u_int8_t *)&firmware_commit,
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = 2,
    };

    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("NVME_VENDOR_FWDMA_WR Sending Command Failed!\n");
        return -1;
    }
    else
    {
        pr_div("NVME_VENDOR_FWDMA_WR Sending Command Succesfully!\n");
    }
    return 0;
}

/**
 * @brief send nvme_firmware_download cmd
 * 
 * @param g_fd 
 * @param numd Number of Dwords (NUMD):0'base This field specifies the number of dwords to transfer for this portion of the firmware.
 * @param ofst Offset (OFST): This field specifies the number of dwords offset from the 
 *             start of the firmware image being downloaded to the controller.
 * @param dptr Data Pointer (DPTR): This field specifies the location where data should be transferred from.
 * @return int 
 */
int nvme_firmware_download(int g_fd, uint32_t numd, uint32_t ofst, uint8_t *dptr)
{
    int ret_val = -1;
    /* Fill the command for nvme_compare*/
    struct nvme_common_command firmware_download = {
        .opcode = nvme_admin_download_fw,
        .nsid = 1,
        .cdw10 = numd,
        .cdw11 = ofst,
    };

    /* Fill the user command */
    struct nvme_64b_cmd user_cmd = {
        .sqid = 0,
        .bit_mask = (NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST | NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST),
        .cmd_buf_ptr = (u_int8_t *)&firmware_download,
        .data_buf_size = numd << 2, //number of dwords
        .data_buf_ptr = dptr,
        .data_dir = 2,
    };

    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("firmware_download Sending Command Failed!\n");
        return -1;
    }
    else
    {
        pr_div("firmware_download Sending Command Succesfully!\n");
    }
    return 0;
}

/**
 * @brief  
 * @note   
 * @param  g_fd: 
 * @param  cq_parameter: 
 * @retval 
 */
int create_iocq(int g_fd, struct create_cq_parameter *cq_parameter)
{
    struct nvme_tool *tool = g_nvme_tool;
    int ret_val = -1;

    struct nvme_64b_cmd user_cmd = {0};
    struct nvme_create_cq create_cq_cmd = {0};
    struct nvme_prep_cq pcq = {0};

    nvme_fill_prep_cq(&pcq, cq_parameter->cq_id, cq_parameter->cq_size,
    	cq_parameter->contig, cq_parameter->irq_en, cq_parameter->irq_no);
    ret_val = nvme_prepare_iocq(g_fd, &pcq);
    if (ret_val < 0)
    {
        pr_err("\tCQ ID = %d Preparation Failed! %d\n", cq_parameter->cq_id, ret_val);
        return -1;
    }
    else
    {
        pr_div("\tCQ ID = %d Preparation success\n", cq_parameter->cq_id);
    }

    /* Fill the command for create IOSQ*/
    create_cq_cmd.opcode = nvme_admin_create_cq;
    create_cq_cmd.cqid = cq_parameter->cq_id;
    create_cq_cmd.qsize = cq_parameter->cq_size - 1;
    create_cq_cmd.cq_flags = ((cq_parameter->irq_en << 1) | cq_parameter->contig);
    create_cq_cmd.irq_vector = cq_parameter->irq_no;
    create_cq_cmd.rsvd1[0] = 0x00;

    /* Fill the user command */
    user_cmd.sqid = 0;
    user_cmd.cmd_buf_ptr = (u_int8_t *)&create_cq_cmd;
    user_cmd.data_dir = 0;
    if (cq_parameter->contig)
    {
        user_cmd.bit_mask = (NVME_MASK_PRP1_PAGE);
        user_cmd.data_buf_size = 0;
        user_cmd.data_buf_ptr = NULL;
    }
    else
    {
        user_cmd.bit_mask = (NVME_MASK_PRP1_LIST);
        user_cmd.data_buf_size = tool->cq_buf_size;
        user_cmd.data_buf_ptr = tool->cq_buf;
    }

    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending of Command Failed!\n");
        return -1;
    }
    else
    {
        pr_div("Command sent succesfully\n\n");
    }
    return 0;
}
/**
 * @brief  
 * @note   
 * @param  g_fd: 
 * @param  sq_parameter: 
 * @retval 
 */
int create_iosq(int g_fd, struct create_sq_parameter *sq_parameter)
{
    struct nvme_tool *tool = g_nvme_tool;
    int ret_val = -1;
    struct nvme_64b_cmd user_cmd = {0};
    struct nvme_create_sq create_sq_cmd = {0};
    struct nvme_prep_sq psq = {0};

    nvme_fill_prep_sq(&psq, sq_parameter->sq_id, sq_parameter->cq_id, 
    	sq_parameter->sq_size, sq_parameter->contig);
    ret_val = nvme_prepare_iosq(g_fd, &psq);
    if (ret_val < 0)
    {
        pr_err("\tSQ ID = %d Preparation Failed!\n", sq_parameter->sq_id);
        return -1;
    }
    else
    {
        pr_div("\tSQ ID = %d Preparation success\n", sq_parameter->sq_id);
    }

    /* Fill the command for create IOSQ*/
    create_sq_cmd.opcode = nvme_admin_create_sq;
    create_sq_cmd.sqid = sq_parameter->sq_id;
    create_sq_cmd.qsize = sq_parameter->sq_size - 1;
    create_sq_cmd.cqid = sq_parameter->cq_id;
    create_sq_cmd.sq_flags = ((((uint16_t)sq_parameter->sq_prio) << 1) | sq_parameter->contig);
    //pr_err("!!!!!!!!!!  create_sq_cmd.sq_flags: 0x%x\n",   create_sq_cmd.sq_flags);
    //  create_sq_cmd.sq_flags = 0x03;
    create_sq_cmd.rsvd1[0] = 0x00;

    /* Fill the user command */
    user_cmd.sqid = 0;
    user_cmd.cmd_buf_ptr = (u_int8_t *)&create_sq_cmd;
    if (sq_parameter->contig)
    {
        user_cmd.bit_mask = (NVME_MASK_PRP1_PAGE);
        user_cmd.data_buf_size = 0;
        user_cmd.data_buf_ptr = NULL;
        user_cmd.data_dir = 0;
    }
    else
    {
        user_cmd.bit_mask = NVME_MASK_PRP1_LIST;
        user_cmd.data_buf_size = tool->sq_buf_size;
        user_cmd.data_buf_ptr = tool->sq_buf;
        user_cmd.data_dir = 2;
    }

    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending of Command Failed!\n");
        return -1;
    }
    else
    {
        pr_div("Command sent succesfully\n\n");
    }
    return 0;
}

/**
 * @brief  
 * @note   
 * @param  g_fd: 
 * @param  opcode: 
 * @retval 
 */
int admin_illegal_opcode_cmd(int g_fd, uint8_t opcode)
{
    int ret_val = -1;
    struct nvme_common_command nvme_cmd = {
        .opcode = opcode,
    };

    struct nvme_64b_cmd user_cmd = {
        .sqid = 0,
        .bit_mask = 0,
        .cmd_buf_ptr = (u_int8_t *)(&nvme_cmd),
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = 0,
    };

    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending cmd Failed! ret_val:%d\n", ret_val);
        return -1;
    }
    else
    {
        pr_div("%s sent succesfully\n\n", __FUNCTION__);
    }
    return 0;
}

/**
 * @brief  
 * @note   
 * @param  g_fd: 
 * @param  sq_id: 
 * @param  cmd_id: 
 * @retval 
 */
int ioctl_send_abort(int g_fd, uint16_t sq_id, uint16_t cmd_id)
{
    int ret_val = -1;
    struct nvme_abort_cmd abort_cmd = {
        .opcode = nvme_admin_abort_cmd,
        .sqid = sq_id,
        .cid = cmd_id,
    };

    struct nvme_64b_cmd user_cmd = {
        .sqid = 0,
        .bit_mask = (NVME_MASK_PRP1_PAGE),
        .cmd_buf_ptr = (u_int8_t *)&abort_cmd,
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = 0,
    };

    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending of Command Failed!\n");
        return -1;
    }
    else
    {
        pr_div("Command sent succesfully\n\n");
    }

    return 0;
}
/**
 * @brief  
 * @note   
 * @param  g_fd: 
 * @param  sq_id: 
 * @retval 
 */
int ioctl_send_flush(int g_fd, uint16_t sq_id)
{
    int ret_val = -1;
    struct nvme_common_command flush_cmd = {
        .opcode = nvme_cmd_flush,
        .nsid = 1,
    };

    /* Fill the user command */
    struct nvme_64b_cmd user_cmd = {
        .sqid = sq_id,
        .bit_mask = (NVME_MASK_PRP1_PAGE),
        .cmd_buf_ptr = (u_int8_t *)&flush_cmd,
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = 0,
    };

    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending of Command Failed!\n");
        return -1;
    }
    else
    {
        pr_div("Command sent succesfully\n\n");
    }

    return 0;
}
/**
 * @brief  
 * @note   
 * @param  g_fd: 
 * @param  sq_id: 
 * @param  slba: 
 * @param  nlb: 
 * @param  fua_sts: 
 * @retval 
 */
int ioctl_send_write_zero(int g_fd, uint16_t sq_id, uint64_t slba, uint16_t nlb, uint16_t control)
{
    int ret_val = -1;

    /* Fill the command for write_zero*/
    struct nvme_rw_command write_zero = {
        .opcode = nvme_cmd_write_zeroes,
        .flags = 0,
        .nsid = 1,
        .metadata = 0,
        .slba = slba,
        .length = nlb - 1,
        .control = control,
    };

    /* Fill the user command */
    struct nvme_64b_cmd user_cmd = {
        .sqid = sq_id,
        .bit_mask = (NVME_MASK_PRP1_PAGE |
                     NVME_MASK_PRP1_LIST |
                     NVME_MASK_PRP2_PAGE |
                     NVME_MASK_PRP2_LIST),
        .cmd_buf_ptr = (u_int8_t *)&write_zero,
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = 2,
    };

    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("command sq_id: %d, slba: 0x%x, nlb: %d Sending Command Failed!\n",
                  sq_id, (uint32_t)slba, nlb);
        return -1;
    }
    else
    {
        pr_div("command sq_id: %d, slba: 0x%x, nlb: %d Command sent succesfully\n\n",
                 sq_id, (uint32_t)slba, nlb);
    }
    return 0;
}
/**
 * @brief  
 * @note   
 * @param  g_fd: 
 * @param  sq_id: 
 * @param  slba: 
 * @param  nlb: 
 * @retval 
 */
int ioctl_send_write_unc(int g_fd, uint16_t sq_id, uint64_t slba, uint16_t nlb)
{
    int ret_val = -1;

    /* Fill the command for write_unc*/
    struct nvme_rw_command write_unc = {
        .opcode = nvme_cmd_write_uncor,
        .flags = 0,
        .nsid = 1,
        .slba = slba,
        .length = nlb - 1, //0'base
    };

    /* Fill the user command */
    struct nvme_64b_cmd user_cmd = {
        .sqid = sq_id,
        .bit_mask = (NVME_MASK_PRP1_PAGE |
                     NVME_MASK_PRP1_LIST |
                     NVME_MASK_PRP2_PAGE |
                     NVME_MASK_PRP2_LIST),
        .cmd_buf_ptr = (u_int8_t *)&write_unc,
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = 2,
    };

    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("command sq_id: %d, slba: 0x%x, nlb: %d Sending Command Failed!\n",
                  sq_id, (uint32_t)slba, nlb);
        return -1;
    }
    else
    {
        pr_div("command sq_id: %d, slba: 0x%x, nlb: %d Command sent succesfully\n\n",
                 sq_id, (uint32_t)slba, nlb + 1);
    }
    return 0;
}

/**
 * @brief  
 * @note   
 * @param  g_fd: 
 * @param  lbaf: 
 * @retval 
 */
int ioctl_send_format(int g_fd, uint8_t lbaf)
{
    int ret_val = -1;
    struct nvme_64b_cmd user_cmd = {0};
    struct nvme_common_command format_cmd = {0};
    PADMIN_FORMAT_COMMAND_DW10 cdw10 = NULL;
    memset(&format_cmd, 0, sizeof(struct nvme_common_command));

    format_cmd.opcode = nvme_admin_format_nvm;
    format_cmd.nsid = 1;
    cdw10 = (PADMIN_FORMAT_COMMAND_DW10)&format_cmd.cdw10;
    cdw10->SES = 1;
    cdw10->LBAF = lbaf; //lba data size:0-->512;1-->4096;
    /* Fill the user command */
    user_cmd.sqid = 0;
    user_cmd.bit_mask = (NVME_MASK_PRP1_PAGE);
    user_cmd.cmd_buf_ptr = (u_int8_t *)&format_cmd;
    user_cmd.data_buf_size = 0;
    user_cmd.data_buf_ptr = NULL;
    user_cmd.data_dir = 0;

    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending of Command Failed!\n");
        return -1;
    }
    else
    {
        pr_div("Command sent succesfully\n\n");
    }

    return 0;
}

//########################################################################################
// new unittest arch
//########################################################################################
/**
 * @brief io cmds use this common interface send to device
 * @note not include meta data 
 * @param g_fd 
 * @param sq_id 
 * @param data_addr 
 * @param buf_size 
 * @param data_dir 
 * @param io_cmd 
 * @return int 
 */
static int nvme_io_cmd(int g_fd, uint16_t sq_id, uint8_t *data_addr, 
	uint32_t buf_size, uint8_t data_dir, struct nvme_rw_command *io_cmd)
{
	int ret;

	struct nvme_64b_cmd user_cmd = {
		.sqid = sq_id,
		.bit_mask = (NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST | 		
			NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST),
		.cmd_buf_ptr = (u_int8_t *)io_cmd,
		.data_buf_size = buf_size,
		.data_buf_ptr = data_addr,
		.data_dir = data_dir,
	};

	ret = nvme_submit_64b_cmd(g_fd, &user_cmd);
	if (ret < 0)
		return ret;

	return 0;
}
/**
 * @brief can refer struct nvme_rw_command for detail
 * 
 * @param g_fd 
 * @param flags NVME_CMD_FUSE_FIRST;NVME_CMD_FUSE_SECOND;
 * @param sq_id 
 * @param nsid 
 * @param slba 
 * @param nlb 
 * @param control NVME_RW_LR;NVME_RW_FUA
 * @param data_addr 
 * @return int 
 */
int nvme_io_write_cmd(int g_fd, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                      uint16_t control, void *data_addr)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint32_t data_size;
	uint32_t lbads;
	int ret;
	struct nvme_rw_command io_cmd = {
		.opcode = nvme_cmd_write,
		.flags = flags,
		.nsid = nsid,
		.slba = slba,
		.length = (nlb - 1),
		.control = control,
	};

	ret = nvme_id_ns_lbads(ns_grp, nsid, &lbads);
	if (ret < 0) {
		pr_err("failed to get lbads!(%d)\n", ret);
		return ret;
	}

	data_size = nlb * lbads;
	return nvme_io_cmd(g_fd, sq_id, data_addr, data_size, DMA_FROM_DEVICE, &io_cmd);
}

/**
 * @brief can refer struct nvme_rw_command for detail
 * 
 * @param g_fd 
 * @param flags NVME_CMD_FUSE_FIRST;NVME_CMD_FUSE_SECOND;
 * @param sq_id 
 * @param nsid 
 * @param slba 
 * @param nlb 
 * @param control NVME_RW_LR;NVME_RW_FUA
 * @param data_addr 
 * @return int 
 */
int nvme_io_read_cmd(int g_fd, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                     uint16_t control, void *data_addr)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint32_t data_size;
	uint32_t lbads;
	int ret;
	struct nvme_rw_command io_cmd = {
		.opcode = nvme_cmd_read,
		.flags = flags,
		.nsid = nsid,
		.slba = slba,
		.length = (nlb - 1),
		.control = control,
	};

	ret = nvme_id_ns_lbads(ns_grp, nsid, &lbads);
	if (ret < 0) {
		pr_err("failed to get lbads!(%d)\n", ret);
		return ret;
	}

	data_size = nlb * lbads;
	// data_size = nlb * LBA_DAT_SIZE;
	return nvme_io_cmd(g_fd, sq_id, data_addr, data_size, DMA_BIDIRECTIONAL, &io_cmd);
}

/**
 * @brief can refer struct nvme_rw_command for detail
 * 
 * @param g_fd 
 * @param flags NVME_CMD_FUSE_FIRST;NVME_CMD_FUSE_SECOND;
 * @param sq_id 
 * @param nsid 
 * @param slba 
 * @param nlb 
 * @param control NVME_RW_LR;NVME_RW_FUA
 * @param data_addr 
 * @return int 
 */
int nvme_io_compare_cmd(int g_fd, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                        uint16_t control, void *data_addr)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct nvme_ns_group *ns_grp = ndev->ns_grp;
	uint32_t data_size;
	uint32_t lbads;
	int ret;
	struct nvme_rw_command io_cmd = {
		.opcode = nvme_cmd_compare,
		.flags = flags,
		.nsid = nsid,
		.slba = slba,
		.length = (nlb - 1),
		.control = control,
	};

	ret = nvme_id_ns_lbads(ns_grp, nsid, &lbads);
	if (ret < 0) {
		pr_err("failed to get lbads!(%d)\n", ret);
		return ret;
	}

	data_size = nlb * lbads;
	return nvme_io_cmd(g_fd, sq_id, data_addr, data_size, DMA_FROM_DEVICE, &io_cmd);
}

int nvme_ring_dbl_and_reap_cq(int g_fd, uint16_t sq_id, uint16_t cq_id, uint32_t expect_num)
{
    uint32_t reap_num = 0;
    int ret_val = 0;
    ret_val |= nvme_ring_sq_doorbell(g_fd, sq_id);
    ret_val |= cq_gain(cq_id, expect_num, &reap_num);
    return ret_val;
}

/**
 * @brief 
 * 
 * @param g_fd 
 * @param nsid 
 * @param feat_id 
 * @param dw11l 
 * @param dw11h 
 * @return int 
 */
int nvme_set_feature_cmd(int g_fd, uint32_t nsid, uint8_t feat_id, 
	uint16_t dw11l, uint16_t dw11h)
{
	int ret;
	struct nvme_features feat_cmd = {
		.opcode = nvme_admin_set_features,
		.nsid = nsid,
		.fid = feat_id,
		.dword11 = ((__le32)dw11h << 16) | dw11l,
	};
	struct nvme_64b_cmd user_cmd = {
		.sqid = 0,
		.bit_mask = 0,
		.cmd_buf_ptr = (u_int8_t *)&feat_cmd,
		.data_buf_size = 0,
		.data_buf_ptr = NULL,
		.data_dir = DMA_BIDIRECTIONAL,
	};

	ret = nvme_submit_64b_cmd(g_fd, &user_cmd);
	if (ret < 0)
		return ret;
	
	return 0;
}
/**
 * @brief 
 * 
 * @param g_fd 
 * @param nsid 
 * @param ehm 
 * @param hsize 
 * @param hmdlla 
 * @param hmdlua 
 * @param hmdlec 
 * @return int 
 */
int nvme_set_feature_hmb_cmd(int g_fd, uint32_t nsid, uint16_t ehm, 
	uint32_t hsize, uint32_t hmdlla, uint32_t hmdlua, uint32_t hmdlec)
{
	int ret;
	struct nvme_features feat_cmd = {
		.opcode = nvme_admin_set_features,
		.nsid = nsid,
		.fid = NVME_FEAT_HOST_MEM_BUF,
		.dword11 = ehm,
		.dword12 = hsize,
		.dword13 = hmdlla,
		.dword14 = hmdlua,
		.dword15 = hmdlec,
	};
	struct nvme_64b_cmd user_cmd = {
		.sqid = 0,
		.bit_mask = 0,
		.cmd_buf_ptr = (u_int8_t *)&feat_cmd,
		.data_buf_size = 0,
		.data_buf_ptr = NULL,
		.data_dir = DMA_BIDIRECTIONAL,
	};

	ret = nvme_submit_64b_cmd(g_fd, &user_cmd);
	if (ret < 0)
		return ret;
	
	return 0;
}
/**
 * @brief 
 * 
 * @param g_fd 
 * @param nsid 
 * @param feat_id 
 * @return int 
 */
int nvme_get_feature_cmd(int g_fd, uint32_t nsid, uint8_t feat_id)
{
	int ret;
	struct nvme_features feat_cmd = {
		.opcode = nvme_admin_get_features,
		.nsid = nsid,
		.fid = feat_id,
	};
	struct nvme_64b_cmd user_cmd = {
		.sqid = 0,
		.bit_mask = 0,
		.cmd_buf_ptr = (u_int8_t *)&feat_cmd,
		.data_buf_size = 0,
		.data_buf_ptr = NULL,
		.data_dir = DMA_BIDIRECTIONAL,
	};

	ret = nvme_submit_64b_cmd(g_fd, &user_cmd);
	if (ret < 0)
		return ret;
	
	return 0;
}

/**
 * @brief 
 * 
 * @return int 
 */
int nvme_admin_ring_dbl_reap_cq(int g_fd)
{
    uint32_t reap_num = 0;
    int ret_val = 0;
    ret_val |= nvme_ring_sq_doorbell(g_fd, NVME_AQ_ID);
    ret_val |= cq_gain(NVME_AQ_ID, 1, &reap_num);
    return ret_val;
}
/**
 * @brief 
 * 
 * @param g_fd 
 * @param cq_id 
 * @param cq_size 
 * @param irq_en 
 * @param irq_no 
 * @return int 
 */
int nvme_create_contig_iocq(int g_fd, uint16_t cq_id, uint32_t cq_size, uint8_t irq_en, uint16_t irq_no)
{
    int ret_val = 0;
    struct nvme_prep_cq pcq = {0};

    nvme_fill_prep_cq(&pcq, cq_id, cq_size, 1, irq_en, irq_no);
    ret_val = nvme_prepare_iocq(g_fd, &pcq);
    if (0 != ret_val)
    {
        pr_err("\tCQ ID = %d Preparation Failed! %d\n", cq_id, ret_val);
        return -1;
    }

    struct nvme_create_cq create_cq_cmd = {
        .opcode = nvme_admin_create_cq,
        .cqid = cq_id,
        .qsize = cq_size - 1,
        .cq_flags = ((irq_en << 1) | 1),
        .irq_vector = irq_no,
        .rsvd1[0] = 0,
    };
    struct nvme_64b_cmd user_cmd = {
        .sqid = 0,
        .bit_mask = (NVME_MASK_PRP1_PAGE),
        .cmd_buf_ptr = (u_int8_t *)&create_cq_cmd,
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = DMA_BIDIRECTIONAL,
    };
    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0) {
	pr_err("failed to submit command!(%d)\n", ret_val);
        return -1;
    }

    return nvme_admin_ring_dbl_reap_cq(g_fd);
}

/**
 * @brief 
 * 
 * @param g_fd 
 * @param cq_id 
 * @param cq_size 
 * @param irq_en 
 * @param irq_no 
 * @param g_discontig_cq_buf 
 * @param discontig_cq_size 
 * @return int 
 */
int nvme_create_discontig_iocq(int g_fd, uint16_t cq_id, uint32_t cq_size, uint8_t irq_en, uint16_t irq_no,
                               uint8_t *g_discontig_cq_buf, uint32_t discontig_cq_size)
{
    int ret_val = -1;
    struct nvme_prep_cq pcq = {0};

    nvme_fill_prep_cq(&pcq, cq_id, cq_size, 0, irq_en, irq_no);
    ret_val = nvme_prepare_iocq(g_fd, &pcq);
    if (0 != ret_val)
    {
        pr_err("\tCQ ID = %d Preparation Failed! %d\n", cq_id, ret_val);
        return -1;
    }

    struct nvme_create_cq create_cq_cmd = {
        .opcode = nvme_admin_create_cq,
        .cqid = cq_id,
        .qsize = cq_size - 1,
        .cq_flags = ((irq_en << 1) | 0),
        .irq_vector = irq_no,
    };

    struct nvme_64b_cmd user_cmd = {
        .sqid = 0,
        .bit_mask = (NVME_MASK_PRP1_LIST),
        .cmd_buf_ptr = (u_int8_t *)&create_cq_cmd,
        .data_buf_size = discontig_cq_size,
        .data_buf_ptr = g_discontig_cq_buf,
        .data_dir = DMA_BIDIRECTIONAL,
    };
    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0) {
        pr_err("failed to submit command!(%d)\n", ret_val);
        return -1;
    }

    return nvme_admin_ring_dbl_reap_cq(g_fd);
}

/**
 * @brief 
 * 
 * @param g_fd 
 * @param sq_id Existing or non-existing SQ ID.
 * @param cq_id assoc CQ ID.
 * @param sq_size Total number of entries that need kernal mem
 * @param sq_prio 
 * @return int 
 */
int nvme_create_contig_iosq(int g_fd, uint16_t sq_id, uint16_t cq_id, uint32_t sq_size, uint8_t sq_prio)
{
    int ret_val = -1;
    struct nvme_prep_sq psq = {0};

    nvme_fill_prep_sq(&psq, sq_id, cq_id, sq_size, 1);
    ret_val = nvme_prepare_iosq(g_fd, &psq);
    if (0 != ret_val)
    {
        pr_err("\tSQ ID = %d Preparation Failed! %d\n", cq_id, ret_val);
        return -1;
    }

    struct nvme_create_sq create_sq_cmd = {
        .opcode = nvme_admin_create_sq,
        .sqid = sq_id,
        .qsize = sq_size - 1,
        .cqid = cq_id,
        .sq_flags = ((((uint16_t)sq_prio) << 1) | 1),
    };

    struct nvme_64b_cmd user_cmd = {
        .sqid = 0,
        .cmd_buf_ptr = (u_int8_t *)&create_sq_cmd,
        .bit_mask = (NVME_MASK_PRP1_PAGE),
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = DMA_BIDIRECTIONAL,
    };
    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0) {
        pr_err("failed to submit command!(%d)\n", ret_val);
        return -1;
    }

    return nvme_admin_ring_dbl_reap_cq(g_fd);
}

/**
 * @brief 
 * 
 * @param g_fd 
 * @param sq_id Existing or non-existing SQ ID.
 * @param cq_id assoc CQ ID.
 * @param sq_size Total number of entries that need kernal mem
 * @param sq_prio 
 * @param g_discontig_sq_buf 
 * @param discontig_sq_size 
 * @return int 
 */
int nvme_create_discontig_iosq(int g_fd, uint16_t sq_id, uint16_t cq_id, uint32_t sq_size, uint8_t sq_prio,
                               uint8_t *g_discontig_sq_buf, uint32_t discontig_sq_size)
{
    int ret_val = -1;
    struct nvme_prep_sq psq = {0};

    nvme_fill_prep_sq(&psq, sq_id, cq_id, sq_size, 0);
    ret_val = nvme_prepare_iosq(g_fd, &psq);
    if (0 != ret_val)
    {
        pr_err("\tSQ ID = %d Preparation Failed! %d\n", cq_id, ret_val);
        return -1;
    }
    struct nvme_create_sq create_sq_cmd = {
        .opcode = nvme_admin_create_sq,
        .sqid = sq_id,
        .qsize = sq_size - 1,
        .cqid = cq_id,
        .sq_flags = ((((uint16_t)sq_prio) << 1) | 0),
    };

    struct nvme_64b_cmd user_cmd = {
        .sqid = 0,
        .bit_mask = (NVME_MASK_PRP1_LIST),
        .cmd_buf_ptr = (u_int8_t *)&create_sq_cmd,
        .data_buf_size = discontig_sq_size,
        .data_buf_ptr = g_discontig_sq_buf,
        .data_dir = DMA_FROM_DEVICE,
    };
    ret_val = nvme_submit_64b_cmd(g_fd, &user_cmd);
    if (ret_val < 0) {
	pr_err("failed to submit command!(%d)\n", ret_val);
	return -1;
    }

    return nvme_admin_ring_dbl_reap_cq(g_fd);
}

int nvme_delete_ioq(int g_fd, uint8_t opcode, uint16_t qid)
{
    int ret_val = -1;
    ret_val = ioctl_delete_ioq(g_fd, opcode, qid);
    if (0 != ret_val)
    {
        pr_err("\tdel qid:%d Failed! %d\n", qid, ret_val);
        return -1;
    }
    return nvme_admin_ring_dbl_reap_cq(g_fd);
}

/********** PCIe RC cfg speed **********/
/**
 * @brief pcie_set_width
 * 
 * @param width 1; 2; 3
 */
void pcie_RC_cfg_speed(int speed)
{
    if (!((1 == speed) || (2 == speed) || (3 == speed)))
    {
        pr_err("speed error\r\n");
        assert(0);
    }
    char spd[3][8] = {"1:0F", "2:0F", "3:0F"};
    char cmd[64] = "setpci -s " RC_PCI_EXP_REG_LINK_CONTROL2 ".b=";
    strcat(cmd, spd[speed - 1]);
    system(cmd);
}
/********** PCIe set width **********/

/**
 * @brief pcie_set_width
 * 
 * @param width 1; 2; 4
 */
void pcie_set_width(int width)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    int ret;
    uint32_t u32_tmp_data;
    
    if (!((1 == width) || (2 == width) || (4 == width)))
    {
        pr_err("width error\r\n");
        assert(0);
    }
    //beagle;cougar;eagle;falcon
    ret = pci_read_config_dword(ndev->fd, 0x8C0, &u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    u32_tmp_data &= 0xFFFFFF80;
    u32_tmp_data |= (0x000000040 + width);
    pci_write_config_data(ndev->fd, 0x8C0, 4, (uint8_t *)&u32_tmp_data);
}

void pcie_random_speed_width(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
	struct pci_dev_instance *pdev = ndev->pdev;
    uint32_t u32_tmp_data = 0;
    uint8_t set_speed, set_width, cur_speed, cur_width;
    uint8_t speed_arr[] = {1, 2, 3};
    uint8_t width_arr[] = {1, 2, 4};
    int ret;

    // get speed and width random
    set_speed = speed_arr[(uint8_t)rand() % ARRAY_SIZE(speed_arr)];
    set_width = width_arr[(uint8_t)rand() % ARRAY_SIZE(width_arr)];
    if (pdev->link_speed < set_speed)
    {
        set_speed = pdev->link_speed;
    }
    if (pdev->link_width < set_width)
    {
        set_width = pdev->link_width;
    }
    pr_info("Set_PCIe_Gen%d, lane width X%d\n", set_speed, set_width);
    // fflush(stdout);
    // cfg speed (RC)
    pcie_RC_cfg_speed(set_speed);
    // cfg width (device)
    pcie_set_width(set_width);

    pcie_retrain_link(RC_PCI_EXP_REG_LINK_CONTROL);
    // check Link status register
    ret = pci_read_config_word(ndev->fd, pdev->express.offset + 0x12, (uint16_t *)&u32_tmp_data);
    if (ret < 0)
    	exit(-1);
    
    cur_speed = u32_tmp_data & 0x0F;
    cur_width = (u32_tmp_data >> 4) & 0x3F;
    if (cur_speed == set_speed && cur_width == set_width)
    {
        //pr_info("Successful linked\n");
    }
    else
    {
        pr_err("Error: linked speed: Gen%d, width: X%d\n", cur_speed, cur_width);
        assert(0);
    }
}

/**
 * @brief INTMS/INTMC
 * 
 * @return uint32_t 
 */
uint32_t nvme_msi_register_test(void)
{
	struct nvme_tool *tool = g_nvme_tool;
	struct nvme_dev_info *ndev = tool->ndev;
    int ret;
    uint32_t u32_tmp_data = 0;

    ret = nvme_read_ctrl_property(ndev->fd, NVME_REG_INTMS, 4, &u32_tmp_data);
    if (ret < 0)
    	return -1;

    pr_info("NVME_REG_INTMS:%#x\n", u32_tmp_data);
    u32_tmp_data = U32_MAX;
    if (nvme_write_ctrl_property(ndev->fd, NVME_REG_INTMS, 4, (uint8_t *)&u32_tmp_data))
        return -1;

    ret = nvme_read_ctrl_property(ndev->fd, NVME_REG_INTMS, 4, &u32_tmp_data);
    if (ret < 0)
    	return -1;
    pr_info("set NVME_REG_INTMS = U32_MAX, read register:%#x\n", u32_tmp_data);

    ret = nvme_read_ctrl_property(ndev->fd, NVME_REG_INTMC, 4, &u32_tmp_data);
    if (ret < 0)
    	return -1;

    pr_info("NVME_REG_INTMC:%#x\n", u32_tmp_data);
    u32_tmp_data = U32_MAX;
    if (nvme_write_ctrl_property(ndev->fd, NVME_REG_INTMC, 4, (uint8_t *)&u32_tmp_data))
        return -1;

    ret = nvme_read_ctrl_property(ndev->fd, NVME_REG_INTMC, 4, &u32_tmp_data);
    if (ret < 0)
    	return -1;

    pr_info("set NVME_REG_INTMC = U32_MAX, read register:%#x\n", u32_tmp_data);

    return 0;
}

int nvme_send_iocmd(int g_fd, uint8_t cmp_dis, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb, void *data_addr)
{
    uint8_t iocmd_type = (uint8_t)rand() % 3;
    uint16_t fua_en = (((uint8_t)rand() % 2) ? NVME_RW_FUA : 0);
    if (cmp_dis)
    {
        iocmd_type = 1;
    }
    if (iocmd_type == 0)
    {
        return nvme_io_read_cmd(g_fd, 0, sq_id, nsid, slba, nlb, fua_en, data_addr);
    }
    else if (iocmd_type == 1)
    {
        return nvme_io_write_cmd(g_fd, 0, sq_id, nsid, slba, nlb, fua_en, data_addr);
    }
    else
    {
        return nvme_io_compare_cmd(g_fd, 0, sq_id, nsid, slba, nlb, fua_en, data_addr);
    }
}
