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

#include "dnvme_ioctl.h"
#include "dnvme_ioctl.h"
#include "reg_nvme_ctrl.h"

#include "common.h"
#include "test_metrics.h"
#include "test_send_cmd.h"
#include "test_cq_gain.h"
#include "test_init.h"

#include "auto_header.h"
/**
 * @brief nvme admin & io cmds use this api send to device
 * 
 * @param file_desc 
 * @param user_cmd 
 * @return int 
 */
int nvme_64b_cmd(int file_desc, struct nvme_64b_cmd *user_cmd)
{
    return ioctl(file_desc, NVME_IOCTL_SEND_64B_CMD, user_cmd);
}

/* CMD to delete IO Queue */
int ioctl_delete_ioq(int file_desc, uint8_t opcode, uint16_t qid)
{

    int ret_val = FAILED;
    struct nvme_delete_queue del_q_cmd = {
        .opcode = opcode,
        .qid = qid,
        .rsvd1[0] = 0x00,
    };

    struct nvme_64b_cmd user_cmd = {
        .q_id = 0,
        .cmd_buf_ptr = (u_int8_t *)&del_q_cmd,
        .data_buf_ptr = NULL,
        .data_dir = DMA_FROM_DEVICE,
    };
    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (SUCCEED != ret_val)
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
    return ret_val;
}

/* CMD to send NVME IO write command */
int ioctl_send_nvme_write(int file_desc, uint16_t sq_id, uint64_t slba, uint16_t nlb,
                          enum fua_sts fua_sts, void *addr, uint32_t buf_size)
{
    int ret_val = FAILED;
    struct nvme_64b_cmd user_cmd = {0};
    struct nvme_rw_command nvme_write = {0};

    /* Fill the command for nvme_write*/
    nvme_write.opcode = nvme_cmd_write;
    nvme_write.flags = 0;
    nvme_write.nsid = 1;
    nvme_write.slba = slba;
    nlb = nlb - 1; //0'base
    nvme_write.length = nlb;
    if (fua_sts == FUA_ENABLE)
    {
        nvme_write.control |= NVME_RW_FUA;
    }

    /* Fill the user command */
    user_cmd.q_id = sq_id;
    user_cmd.bit_mask = (NVME_MASK_PRP1_PAGE |
                         NVME_MASK_PRP1_LIST |
                         NVME_MASK_PRP2_PAGE |
                         NVME_MASK_PRP2_LIST);
    user_cmd.cmd_buf_ptr = (u_int8_t *)&nvme_write;
    user_cmd.data_buf_size = buf_size;
    user_cmd.data_buf_ptr = addr;
    user_cmd.data_dir = 2;

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("command sq_id: %d, slba: 0x%x, nlb: %d Sending Command Failed!\n",
                  sq_id, (uint32_t)slba, nlb + 1);
        return FAILED;
    }
    else
    {
        pr_debug("command sq_id: %d, slba: 0x%x, nlb: %d Command sent succesfully\n\n",
                 sq_id, (uint32_t)slba, nlb + 1);
    }
    return SUCCEED;
}

/* CMD to send nvme_read command */
int ioctl_send_nvme_read(int file_desc, uint16_t sq_id, uint64_t slba, uint16_t nlb,
                         enum fua_sts fua_sts, void *addr, uint32_t buf_size)
{
    int ret_val = FAILED;
    struct nvme_64b_cmd user_cmd = {0};
    struct nvme_rw_command nvme_read = {0};

    /* Fill the command for nvme_read*/
    nvme_read.opcode = nvme_cmd_read;
    nvme_read.flags = 0;
    nvme_read.nsid = 1;
    nvme_read.metadata = 0;
    nvme_read.slba = slba;
    nlb = nlb - 1; //0'base
    nvme_read.length = nlb;
    if (fua_sts == FUA_ENABLE)
    {
        nvme_read.control |= NVME_RW_FUA;
    }

    /* Fill the user command */
    user_cmd.q_id = sq_id;
    user_cmd.bit_mask = (NVME_MASK_PRP1_PAGE |
                         NVME_MASK_PRP1_LIST |
                         NVME_MASK_PRP2_PAGE |
                         NVME_MASK_PRP2_LIST);
    user_cmd.cmd_buf_ptr = (u_int8_t *)&nvme_read;
    user_cmd.data_buf_size = buf_size;
    user_cmd.data_buf_ptr = addr;
    user_cmd.data_dir = 0;

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("command sq_id: %d, slba: 0x%x, nlb: %d Sending Command Failed!\n",
                  sq_id, (uint32_t)slba, nlb + 1);
        return FAILED;
    }
    else
    {
        pr_debug("command sq_id: %d, slba: 0x%x, nlb: %d Command sent succesfully\n\n",
                 sq_id, (uint32_t)slba, nlb + 1);
    }
    return SUCCEED;
}

//******************************************************************************************
/**
 * @brief   CMD to send nvme_compare command
 * @note   
 * @param  file_desc: 
 * @param  sq_id: 
 * @param  slba: 
 * @param  nlb: 
 * @param  fua_sts: 
 * @param  *addr: 
 * @param  buf_size: 
 * @retval 
 */
int ioctl_send_nvme_compare(int file_desc, uint16_t sq_id, uint64_t slba, uint16_t nlb,
                            enum fua_sts fua_sts, void *addr, uint32_t buf_size)
{
    int ret_val = FAILED;
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
    user_cmd.q_id = sq_id;
    user_cmd.bit_mask = (NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST |
                         NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST);
    user_cmd.cmd_buf_ptr = (u_int8_t *)&nvme_compare;
    user_cmd.data_buf_size = buf_size;
    user_cmd.data_buf_ptr = addr;
    user_cmd.data_dir = 0;

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("command sq_id: %d, slba: 0x%x, nlb: %d Sending Command Failed!\n",
                  sq_id, (uint32_t)slba, nlb + 1);
        return FAILED;
    }
    else
    {
        pr_debug("command sq_id: %d, slba: 0x%x, nlb: %d Command sent succesfully\n\n",
                 sq_id, (uint32_t)slba, nlb + 1);
    }
    return SUCCEED;
}

/**
 * @brief  send maxio vendor cmd: fw dma read
 * @note   
 * @param  file_desc: 
 * @param  *addr: 
 * @param  size: 
 * @param  crcc_len: 
 * @param  axi_addr: 
 * @retval 
 */
int nvme_maxio_fwdma_rd(int file_desc, struct fwdma_parameter *fwdma_parameter)
{
    int ret_val = FAILED;
    /* Fill the command for nvme_compare*/
    struct nvme_common_command maxio_fwdma_rd = {
        .opcode = nvme_admin_vendor_fwdma_read, //nvme_admin_vendor_read,
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
        .q_id = 0,
        .bit_mask = (NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST |
                     NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST),
        .cmd_buf_ptr = (u_int8_t *)&maxio_fwdma_rd,
        .data_buf_size = fwdma_parameter->cdw10,
        .data_buf_ptr = fwdma_parameter->addr,
        .data_dir = 0,
    };

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("NVME_VENDOR_FWDMA_RD Sending Command Failed!\n");
        return FAILED;
    }
    else
    {
        pr_debug("NVME_VENDOR_FWDMA_RD Sending Command Succesfully!\n");
    }
    return SUCCEED;
}
/**
 * @brief  send maxio vendor cmd: fw dma write
 * @note   
 * @param  file_desc: 
 * @param  fwdma_parameter: 
 * @retval 
 */
int nvme_maxio_fwdma_wr(int file_desc, struct fwdma_parameter *fwdma_parameter)
{
    int ret_val = FAILED;
    /* Fill the command for nvme_compare*/
    struct nvme_common_command maxio_fwdma_wr = {
        .opcode = nvme_admin_vendor_fwdma_write, //nvme_admin_vendor_write,
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
        .q_id = 0,
        .bit_mask = (NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST | NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST),
        .cmd_buf_ptr = (u_int8_t *)&maxio_fwdma_wr,
        .data_buf_size = fwdma_parameter->cdw10,
        .data_buf_ptr = fwdma_parameter->addr,
        .data_dir = 2,
    };

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("NVME_VENDOR_FWDMA_WR Sending Command Failed!\n");
        return FAILED;
    }
    else
    {
        pr_debug("NVME_VENDOR_FWDMA_WR Sending Command Succesfully!\n");
    }
    return SUCCEED;
}

/**
 * @brief send nvme_firmware_commit cmd
 * 
 * @param file_desc 
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
int nvme_firmware_commit(int file_desc, byte_t bpid, byte_t ca, byte_t fs)
{
    int ret_val = FAILED;
    /* Fill the command for nvme_compare*/
    struct nvme_common_command firmware_commit = {
        .opcode = nvme_admin_activate_fw,
        .nsid = 1,
        .cdw10 = ((dword_t)bpid << 31) | ((dword_t)((ca & 0x7) << 3)) | ((dword_t)(fs & 0x7)),
    };

    /* Fill the user command */
    struct nvme_64b_cmd user_cmd = {
        .q_id = 0,
        .bit_mask = (NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST | NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST),
        .cmd_buf_ptr = (u_int8_t *)&firmware_commit,
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = 2,
    };

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("NVME_VENDOR_FWDMA_WR Sending Command Failed!\n");
        return FAILED;
    }
    else
    {
        pr_debug("NVME_VENDOR_FWDMA_WR Sending Command Succesfully!\n");
    }
    return SUCCEED;
}

/**
 * @brief send nvme_firmware_download cmd
 * 
 * @param file_desc 
 * @param numd Number of Dwords (NUMD):0'base This field specifies the number of dwords to transfer for this portion of the firmware.
 * @param ofst Offset (OFST): This field specifies the number of dwords offset from the 
 *             start of the firmware image being downloaded to the controller.
 * @param dptr Data Pointer (DPTR): This field specifies the location where data should be transferred from.
 * @return int 
 */
int nvme_firmware_download(int file_desc, dword_t numd, dword_t ofst, byte_t *dptr)
{
    int ret_val = FAILED;
    /* Fill the command for nvme_compare*/
    struct nvme_common_command firmware_download = {
        .opcode = nvme_admin_download_fw,
        .nsid = 1,
        .cdw10 = numd,
        .cdw11 = ofst,
    };

    /* Fill the user command */
    struct nvme_64b_cmd user_cmd = {
        .q_id = 0,
        .bit_mask = (NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST | NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST),
        .cmd_buf_ptr = (u_int8_t *)&firmware_download,
        .data_buf_size = numd << 2, //number of dwords
        .data_buf_ptr = dptr,
        .data_dir = 2,
    };

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("firmware_download Sending Command Failed!\n");
        return FAILED;
    }
    else
    {
        pr_debug("firmware_download Sending Command Succesfully!\n");
    }
    return SUCCEED;
}

/**
 * @brief  
 * @note   
 * @param  file_desc: 
 * @param  cq_parameter: 
 * @retval 
 */
int create_iocq(int file_desc, struct create_cq_parameter *cq_parameter)
{
    int ret_val = FAILED;

    struct nvme_64b_cmd user_cmd = {0};
    struct nvme_create_cq create_cq_cmd = {0};

    /* prep cq para */
    struct nvme_prep_cq prep_cq = {
        .cq_id = cq_parameter->cq_id,
        .elements = cq_parameter->cq_size,
        .contig = cq_parameter->contig,
        .cq_irq_en = cq_parameter->irq_en,
        .cq_irq_no = cq_parameter->irq_no,
    };

    ret_val = ioctl(file_desc, NVME_IOCTL_PREPARE_CQ_CREATION, &prep_cq);
    if (ret_val < 0)
    {
        pr_err("\tCQ ID = %d Preparation Failed! %d\n", prep_cq.cq_id, ret_val);
        return FAILED;
    }
    else
    {
        pr_debug("\tCQ ID = %d Preparation success\n", prep_cq.cq_id);
    }

    /* Fill the command for create IOSQ*/
    create_cq_cmd.opcode = nvme_admin_create_cq;
    create_cq_cmd.cqid = cq_parameter->cq_id;
    create_cq_cmd.qsize = cq_parameter->cq_size - 1;
    create_cq_cmd.cq_flags = ((cq_parameter->irq_en << 1) | cq_parameter->contig);
    create_cq_cmd.irq_vector = cq_parameter->irq_no;
    create_cq_cmd.rsvd1[0] = 0x00;

    /* Fill the user command */
    user_cmd.q_id = 0;
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
        user_cmd.data_buf_size = DISCONTIG_IO_CQ_SIZE;
        user_cmd.data_buf_ptr = discontg_cq_buf;
    }

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending of Command Failed!\n");
        return FAILED;
    }
    else
    {
        pr_debug("Command sent succesfully\n\n");
    }
    return SUCCEED;
}
/**
 * @brief  
 * @note   
 * @param  file_desc: 
 * @param  sq_parameter: 
 * @retval 
 */
int create_iosq(int file_desc, struct create_sq_parameter *sq_parameter)
{
    int ret_val = FAILED;
    struct nvme_prep_sq prep_sq = {0};

    struct nvme_64b_cmd user_cmd = {0};
    struct nvme_create_sq create_sq_cmd = {0};

    /* prep cq para */
    prep_sq.sq_id = sq_parameter->sq_id;
    prep_sq.cq_id = sq_parameter->cq_id;
    prep_sq.elements = sq_parameter->sq_size;
    prep_sq.contig = sq_parameter->contig;
    prep_sq.sq_prio = sq_parameter->sq_prio;

    ret_val = ioctl(file_desc, NVME_IOCTL_PREPARE_SQ_CREATION, &prep_sq);

    if (ret_val < 0)
    {
        pr_err("\tSQ ID = %d Preparation Failed!\n", prep_sq.sq_id);
        return FAILED;
    }
    else
    {
        pr_debug("\tSQ ID = %d Preparation success\n", prep_sq.sq_id);
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
    user_cmd.q_id = 0;
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
        user_cmd.data_buf_size = DISCONTIG_IO_SQ_SIZE;
        user_cmd.data_buf_ptr = discontg_sq_buf;
        user_cmd.data_dir = 2;
    }

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending of Command Failed!\n");
        return FAILED;
    }
    else
    {
        pr_debug("Command sent succesfully\n\n");
    }
    return SUCCEED;
}

/**
 * @brief  
 * @note   
 * @param  file_desc: 
 * @retval 
 */
int keep_alive_cmd(int file_desc)
{
    int ret_val = FAILED;
    struct nvme_common_command keep_alive_cmd = {
        .opcode = nvme_admin_keep_alive,
    };

    struct nvme_64b_cmd user_cmd = {
        .q_id = 0,
        .bit_mask = 0,
        .cmd_buf_ptr = (u_int8_t *)(&keep_alive_cmd),
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = 0,
    };

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending cmd Failed! ret_val:%d\n", ret_val);
        return FAILED;
    }
    else
    {
        pr_debug("%s sent succesfully\n\n", __FUNCTION__);
    }
    return SUCCEED;
}
/**
 * @brief  
 * @note   
 * @param  file_desc: 
 * @param  opcode: 
 * @retval 
 */
int admin_illegal_opcode_cmd(int file_desc, uint8_t opcode)
{
    int ret_val = FAILED;
    struct nvme_common_command nvme_cmd = {
        .opcode = opcode,
    };

    struct nvme_64b_cmd user_cmd = {
        .q_id = 0,
        .bit_mask = 0,
        .cmd_buf_ptr = (u_int8_t *)(&nvme_cmd),
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = 0,
    };

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending cmd Failed! ret_val:%d\n", ret_val);
        return FAILED;
    }
    else
    {
        pr_debug("%s sent succesfully\n\n", __FUNCTION__);
    }
    return SUCCEED;
}

/**
 * @brief  
 * @note   
 * @param  file_desc: 
 * @param  sq_id: 
 * @param  cmd_id: 
 * @retval 
 */
int ioctl_send_abort(int file_desc, uint16_t sq_id, uint16_t cmd_id)
{
    int ret_val = FAILED;
    struct nvme_abort_cmd abort_cmd = {
        .opcode = nvme_admin_abort_cmd,
        .sqid = sq_id,
        .cid = cmd_id,
    };

    struct nvme_64b_cmd user_cmd = {
        .q_id = 0,
        .bit_mask = (NVME_MASK_PRP1_PAGE),
        .cmd_buf_ptr = (u_int8_t *)&abort_cmd,
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = 0,
    };

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending of Command Failed!\n");
        return FAILED;
    }
    else
    {
        pr_debug("Command sent succesfully\n\n");
    }

    return SUCCEED;
}
/**
 * @brief  
 * @note   
 * @param  file_desc: 
 * @param  sq_id: 
 * @retval 
 */
int ioctl_send_flush(int file_desc, uint16_t sq_id)
{
    int ret_val = FAILED;
    struct nvme_common_command flush_cmd = {
        .opcode = nvme_cmd_flush,
        .nsid = 1,
    };

    /* Fill the user command */
    struct nvme_64b_cmd user_cmd = {
        .q_id = sq_id,
        .bit_mask = (NVME_MASK_PRP1_PAGE),
        .cmd_buf_ptr = (u_int8_t *)&flush_cmd,
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = 0,
    };

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending of Command Failed!\n");
        return FAILED;
    }
    else
    {
        pr_debug("Command sent succesfully\n\n");
    }

    return SUCCEED;
}
/**
 * @brief  
 * @note   
 * @param  file_desc: 
 * @param  sq_id: 
 * @param  slba: 
 * @param  nlb: 
 * @param  fua_sts: 
 * @retval 
 */
int ioctl_send_write_zero(int file_desc, uint16_t sq_id, uint64_t slba, uint16_t nlb, uint16_t control)
{
    int ret_val = FAILED;

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
        .q_id = sq_id,
        .bit_mask = (NVME_MASK_PRP1_PAGE |
                     NVME_MASK_PRP1_LIST |
                     NVME_MASK_PRP2_PAGE |
                     NVME_MASK_PRP2_LIST),
        .cmd_buf_ptr = (u_int8_t *)&write_zero,
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = 2,
    };

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("command sq_id: %d, slba: 0x%x, nlb: %d Sending Command Failed!\n",
                  sq_id, (uint32_t)slba, nlb);
        return FAILED;
    }
    else
    {
        pr_debug("command sq_id: %d, slba: 0x%x, nlb: %d Command sent succesfully\n\n",
                 sq_id, (uint32_t)slba, nlb);
    }
    return SUCCEED;
}
/**
 * @brief  
 * @note   
 * @param  file_desc: 
 * @param  sq_id: 
 * @param  slba: 
 * @param  nlb: 
 * @retval 
 */
int ioctl_send_write_unc(int file_desc, uint16_t sq_id, uint64_t slba, uint16_t nlb)
{
    int ret_val = FAILED;

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
        .q_id = sq_id,
        .bit_mask = (NVME_MASK_PRP1_PAGE |
                     NVME_MASK_PRP1_LIST |
                     NVME_MASK_PRP2_PAGE |
                     NVME_MASK_PRP2_LIST),
        .cmd_buf_ptr = (u_int8_t *)&write_unc,
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = 2,
    };

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("command sq_id: %d, slba: 0x%x, nlb: %d Sending Command Failed!\n",
                  sq_id, (uint32_t)slba, nlb);
        return FAILED;
    }
    else
    {
        pr_debug("command sq_id: %d, slba: 0x%x, nlb: %d Command sent succesfully\n\n",
                 sq_id, (uint32_t)slba, nlb + 1);
    }
    return SUCCEED;
}

int subsys_reset(void)
{
    int ret_val = -1;
    ret_val = ioctl(file_desc, NVME_IOCTL_SET_DEV_STATE, NVME_ST_RESET_SUBSYSTEM);
    if (ret_val < 0)
    {
        pr_err("User Call to subsys_reset: Failed!\n");
        return FAILED;
    }
    else
    {
        pr_debug("User Call to subsys_reset: SUCCESS\n");
        return SUCCEED;
    }

    // uint32_t u32_tmp_data = 0;
    // u32_tmp_data = 0x4E564D65; //“NVMe”
    // if (ioctl_write_data(file_desc, NVME_REG_NSSR_OFST, 4, (uint8_t *)&u32_tmp_data))
    //     return FAILED;
    // else
    //     return SUCCEED;
}

int ctrl_disable(void)
{
    int ret_val = -1;
    ret_val = ioctl(file_desc, NVME_IOCTL_SET_DEV_STATE, NVME_ST_DISABLE_COMPLETE);
    if (ret_val < 0)
    {
        pr_err("User Call to Disable Ctrlr: Failed!\n");
        return FAILED;
    }
    else
    {
        pr_debug("User Call to Disable Ctrlr: success\n");
        return SUCCEED;
    }

    // uint32_t u32_tmp_data = 0;
    // u32_tmp_data = ioctl_read_data(file_desc, 0x14, 4);
    // u32_tmp_data &= ~0x1; //CC.EN = 0
    // if (ioctl_write_data(file_desc, NVME_REG_CC_OFST, 4, (uint8_t *)&u32_tmp_data))
    //     return FAILED;
    // else
    //     return SUCCEED;
}

uint8_t pci_find_cap_ofst(int file_desc, uint8_t cap_id)
{
    uint16_t data = 0;
    uint8_t pci_cap = 0;
    uint32_t to_cnt = 0;
    data = pci_read_byte(file_desc, 0x34);
    pci_cap = (uint8_t)data;
    data = pci_read_word(file_desc, data);
    while (cap_id != (data & 0xFF))
    {
        pci_cap = (uint8_t)(data >> 8);
        data = pci_read_word(file_desc, pci_cap);
        if (++to_cnt > 100)
        {
            pr_err("can't find cap_id:%x\n", cap_id);
            return BYTE_MASK;
        }
    }
    return (uint8_t)pci_cap;
}

int ctrl_pci_flr(void)
{
    uint32_t u32_tmp_data = 0;
    uint32_t bar_reg[6];
    uint8_t idx = 0;
    int ret_val = FAILED;
    for (idx = 0; idx < 6; idx++)
    {
        bar_reg[idx] = pci_read_dword(file_desc, 0x10 + idx * 4);
    }
    u32_tmp_data = pci_read_dword(file_desc, g_nvme_dev.pxcap_ofst + 0x8);
    u32_tmp_data |= 0x1 << 15; //Initiate Function Level Reset
    ret_val = ioctl_pci_write_data(file_desc, g_nvme_dev.pxcap_ofst + 0x8, 4, (uint8_t *)&u32_tmp_data);
    if (ret_val < 0)
        return FAILED;

    usleep(2000);

    u32_tmp_data = ((uint32_t)(7)); // bus master/memory space/io space enable
    ret_val = ioctl_pci_write_data(file_desc, 0x4, 4, (uint8_t *)&u32_tmp_data);
    if (ret_val < 0)
        return FAILED;

    for (idx = 0; idx < 6; idx++)
    {
        ioctl_pci_write_data(file_desc, 0x10 + idx * 4, 4, (uint8_t *)&bar_reg[idx]);
    }

    return SUCCEED;
}

int set_power_state(uint8_t pmcap, uint8_t dstate)
{
    uint32_t u32_tmp_data = 0;
    int ret_val = FAILED;
    uint32_t retry_cnt = 0, retry_cnt1 = 0;
    // u32_tmp_data = pci_read_dword(file_desc, pmcap + 0x4);
    // if ((u32_tmp_data & 0x3) == dstate)
    //     return SUCCEED;
    u32_tmp_data &= ~0x3;
    u32_tmp_data |= dstate;
write_again:
    ret_val = ioctl_pci_write_data(file_desc, pmcap + 0x4, 4, (uint8_t *)&u32_tmp_data);
    if (ret_val < 0)
    {
        pr_err("ioctl_pci_write_data failed:%d\n", ret_val);
        usleep(10000);
        if (++retry_cnt < 10)
            goto write_again;
        else
            return FAILED;
    }
    usleep(10000); //spec define min 10ms wait ctrlr ready
    u32_tmp_data = pci_read_dword(file_desc, pmcap + 0x4);
    if ((u32_tmp_data & 0x3) != dstate)
    {
        pr_err("set_state:%#x,but read:%#x\r\n", dstate, u32_tmp_data & 0x3);
        if (++retry_cnt1 < 10)
            goto write_again;    
    }
    assert((u32_tmp_data & 0x3) == dstate);
    return SUCCEED;
}

//set_link_disable()

/**
 * @brief  
 * @note   
 * @param  file_desc: 
 * @param  lbaf: 
 * @retval 
 */
int ioctl_send_format(int file_desc, uint8_t lbaf)
{
    int ret_val = FAILED;
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
    user_cmd.q_id = 0;
    user_cmd.bit_mask = (NVME_MASK_PRP1_PAGE);
    user_cmd.cmd_buf_ptr = (u_int8_t *)&format_cmd;
    user_cmd.data_buf_size = 0;
    user_cmd.data_buf_ptr = NULL;
    user_cmd.data_dir = 0;

    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (ret_val < 0)
    {
        pr_err("Sending of Command Failed!\n");
        return FAILED;
    }
    else
    {
        pr_debug("Command sent succesfully\n\n");
    }

    return SUCCEED;
}

//########################################################################################
// new unittest arch
//########################################################################################
/**
 * @brief io cmds use this common interface send to device
 * @note not include meta data 
 * @param file_desc 
 * @param sq_id 
 * @param data_addr 
 * @param buf_size 
 * @param data_dir 
 * @param io_cmd 
 * @return int 
 */
static int nvme_io_cmd(int file_desc, uint16_t sq_id, uint8_t const *data_addr, uint32_t buf_size,
                       uint8_t data_dir, struct nvme_rw_command *io_cmd)
{
    struct nvme_64b_cmd user_cmd = {
        .q_id = sq_id,
        .bit_mask = (NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST | NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST),
        .cmd_buf_ptr = (u_int8_t *)io_cmd,
        .data_buf_size = buf_size,
        .data_buf_ptr = data_addr,
        .data_dir = data_dir,
    };
    return nvme_64b_cmd(file_desc, &user_cmd);
}
/**
 * @brief can refer struct nvme_rw_command for detail
 * 
 * @param file_desc 
 * @param flags NVME_CMD_FUSE_FIRST;NVME_CMD_FUSE_SECOND;
 * @param sq_id 
 * @param nsid 
 * @param slba 
 * @param nlb 
 * @param control NVME_RW_LR;NVME_RW_FUA
 * @param data_addr 
 * @return int 
 */
int nvme_io_write_cmd(int file_desc, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                      uint16_t control, void *data_addr)
{
    uint32_t data_size;
    struct nvme_rw_command io_cmd = {
        .opcode = nvme_cmd_write,
        .flags = flags,
        .nsid = nsid,
        .slba = slba,
        .length = (nlb - 1),
        .control = control,
    };
    data_size = nlb * LBA_DATA_SIZE(nsid);
    return nvme_io_cmd(file_desc, sq_id, data_addr, data_size, DMA_FROM_DEVICE, &io_cmd);
}

/**
 * @brief can refer struct nvme_rw_command for detail
 * 
 * @param file_desc 
 * @param flags NVME_CMD_FUSE_FIRST;NVME_CMD_FUSE_SECOND;
 * @param sq_id 
 * @param nsid 
 * @param slba 
 * @param nlb 
 * @param control NVME_RW_LR;NVME_RW_FUA
 * @param data_addr 
 * @return int 
 */
int nvme_io_read_cmd(int file_desc, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                     uint16_t control, void *data_addr)
{
    uint32_t data_size;
    struct nvme_rw_command io_cmd = {
        .opcode = nvme_cmd_read,
        .flags = flags,
        .nsid = nsid,
        .slba = slba,
        .length = (nlb - 1),
        .control = control,
    };
    data_size = nlb * LBA_DATA_SIZE(nsid);
    // data_size = nlb * LBA_DAT_SIZE;
    //pr_info("### %d,%d %d,%d\n", nlb * LBA_DAT_SIZE, nlb * LBA_DATA_SIZE(nsid), LBA_DAT_SIZE, g_nvme_ns_info[nsid - 1].lbads);
    return nvme_io_cmd(file_desc, sq_id, data_addr, data_size, DMA_BIDIRECTIONAL, &io_cmd);
}

/**
 * @brief can refer struct nvme_rw_command for detail
 * 
 * @param file_desc 
 * @param flags NVME_CMD_FUSE_FIRST;NVME_CMD_FUSE_SECOND;
 * @param sq_id 
 * @param nsid 
 * @param slba 
 * @param nlb 
 * @param control NVME_RW_LR;NVME_RW_FUA
 * @param data_addr 
 * @return int 
 */
int nvme_io_compare_cmd(int file_desc, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                        uint16_t control, void *data_addr)
{
    uint32_t data_size;
    struct nvme_rw_command io_cmd = {
        .opcode = nvme_cmd_compare,
        .flags = flags,
        .nsid = nsid,
        .slba = slba,
        .length = (nlb - 1),
        .control = control,
    };
    data_size = nlb * LBA_DATA_SIZE(nsid);
    return nvme_io_cmd(file_desc, sq_id, data_addr, data_size, DMA_FROM_DEVICE, &io_cmd);
}

/* CMD to send NVME IO write command using metabuff*/
int send_nvme_write_using_metabuff(int file_desc, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                                   uint16_t control, uint32_t id, void *data_addr)
{
    uint32_t data_size;
    struct nvme_rw_command io_cmd = {
        .opcode = nvme_cmd_write,
        .flags = flags,
        .nsid = nsid,
        .slba = slba,
        .length = (nlb - 1), //0'base
        .control = control,
    };
    data_size = nlb * LBA_DATA_SIZE(nsid);

    struct nvme_64b_cmd user_cmd = {
        .q_id = sq_id,
        .bit_mask = (NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST | NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST | NVME_MASK_MPTR),
        .cmd_buf_ptr = (u_int8_t *)&io_cmd,
        .data_buf_size = data_size,
        .data_buf_ptr = data_addr,
        .meta_buf_id = id,
        .data_dir = DMA_FROM_DEVICE,
    };
    return nvme_64b_cmd(file_desc, &user_cmd);
}

/* CMD to send NVME IO read command using metabuff through contig Queue */
int send_nvme_read_using_metabuff(int file_desc, uint8_t flags, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb,
                                  uint16_t control, uint32_t id, void *data_addr)
{
    uint32_t data_size;
    struct nvme_rw_command io_cmd = {
        .opcode = nvme_cmd_read,
        .flags = flags,
        .nsid = nsid,
        .slba = slba,
        .length = (nlb - 1), //0'base
        .control = control,
    };
    data_size = nlb * LBA_DATA_SIZE(nsid);

    struct nvme_64b_cmd user_cmd = {
        .q_id = sq_id,
        .bit_mask = (NVME_MASK_PRP1_PAGE | NVME_MASK_PRP1_LIST | NVME_MASK_PRP2_PAGE | NVME_MASK_PRP2_LIST),
        .cmd_buf_ptr = (u_int8_t *)&io_cmd,
        .data_buf_size = data_size,
        .data_buf_ptr = data_addr,
        .meta_buf_id = id,
        .data_dir = DMA_BIDIRECTIONAL,
    };
    return nvme_64b_cmd(file_desc, &user_cmd);
}

int nvme_ring_dbl_and_reap_cq(int file_desc, uint16_t sq_id, uint16_t cq_id, uint32_t expect_num)
{
    uint32_t reap_num = 0;
    int ret_val = SUCCEED;
    ret_val |= ioctl_tst_ring_dbl(file_desc, sq_id);
    ret_val |= cq_gain(cq_id, expect_num, &reap_num);
    return ret_val;
}

/**
 * @brief identify cmds use this common interface send to device
 * 
 * @param file_desc 
 * @param nsid 
 * @param data_addr 
 * @param idfy_cmd 
 * @return int 
 */
static int nvme_idfy_cmd(int file_desc, uint32_t nsid, uint32_t cns, uint8_t const *data_addr)
{
    struct nvme_identify idfy_cmd = {
        .opcode = nvme_admin_identify,
        .nsid = nsid,
        .cns = cns,
    };
    struct nvme_64b_cmd user_cmd = {
        .q_id = 0,
        .bit_mask = (NVME_MASK_PRP1_PAGE | NVME_MASK_PRP2_PAGE),
        .cmd_buf_ptr = (u_int8_t *)&idfy_cmd,
        .data_buf_size = IDENTIFY_DATA_SIZE,
        .data_buf_ptr = data_addr,
        .data_dir = DMA_BIDIRECTIONAL,
    };
    return nvme_64b_cmd(file_desc, &user_cmd);
}

int nvme_idfy_ctrl(int file_desc, void *data)
{
    memset(data, 0, IDENTIFY_DATA_SIZE);
    return nvme_idfy_cmd(file_desc, 0, NVME_ID_CNS_CTRL, data);
}

int nvme_idfy_ns(int fd, uint32_t nsid, uint8_t present, void *data)
{
    uint32_t cns = present ? NVME_ID_CNS_NS_PRESENT : NVME_ID_CNS_NS;
    memset(data, 0, IDENTIFY_DATA_SIZE);
    return nvme_idfy_cmd(fd, nsid, cns, data);
}

int nvme_idfy_ns_list(int fd, uint32_t nsid, uint8_t all, void *data)
{
    uint32_t cns = all ? NVME_ID_CNS_NS_PRESENT_LIST : NVME_ID_CNS_NS_ACTIVE_LIST;
    memset(data, 0, IDENTIFY_DATA_SIZE);
    return nvme_idfy_cmd(fd, nsid, cns, data);
}

int nvme_idfy_ctrl_list(int fd, uint32_t nsid, uint16_t cntid, void *data)
{
    uint32_t cns = nsid ? NVME_ID_CNS_CTRL_NS_LIST : NVME_ID_CNS_CTRL_LIST;
    memset(data, 0, IDENTIFY_DATA_SIZE);
    return nvme_idfy_cmd(fd, nsid, (cntid << 16) | cns, data);
}

int nvme_idfy_secondary_ctrl_list(int fd, uint32_t nsid, uint16_t cntid, void *data)
{
    memset(data, 0, IDENTIFY_DATA_SIZE);
    return nvme_idfy_cmd(fd, nsid, (cntid << 16) | NVME_ID_CNS_SCNDRY_CTRL_LIST, data);
}

int nvme_idfy_ns_descs(int fd, uint32_t nsid, void *data)
{
    memset(data, 0, IDENTIFY_DATA_SIZE);
    return nvme_idfy_cmd(fd, nsid, NVME_ID_CNS_NS_DESC_LIST, data);
}
/**
 * @brief 
 * 
 * @param file_desc 
 * @param nsid 
 * @param feat_id 
 * @param dw11l 
 * @param dw11h 
 * @return int 
 */
int nvme_set_feature_cmd(int file_desc, uint32_t nsid, uint8_t feat_id, uint16_t dw11l, uint16_t dw11h)
{
    struct nvme_features feat_cmd = {
        .opcode = nvme_admin_set_features,
        .nsid = nsid,
        .fid = feat_id,
        .dword11 = ((__le32)dw11h << 16) | dw11l,
    };
    struct nvme_64b_cmd user_cmd = {
        .q_id = 0,
        .bit_mask = 0,
        .cmd_buf_ptr = (u_int8_t *)&feat_cmd,
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = DMA_BIDIRECTIONAL,
    };
    return nvme_64b_cmd(file_desc, &user_cmd);
}
/**
 * @brief 
 * 
 * @param file_desc 
 * @param nsid 
 * @param ehm 
 * @param hsize 
 * @param hmdlla 
 * @param hmdlua 
 * @param hmdlec 
 * @return int 
 */
int nvme_set_feature_hmb_cmd(int file_desc, uint32_t nsid, uint16_t ehm, uint32_t hsize,
                             uint32_t hmdlla, uint32_t hmdlua, uint32_t hmdlec)
{
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
        .q_id = 0,
        .bit_mask = 0,
        .cmd_buf_ptr = (u_int8_t *)&feat_cmd,
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = DMA_BIDIRECTIONAL,
    };
    return nvme_64b_cmd(file_desc, &user_cmd);
}
/**
 * @brief 
 * 
 * @param file_desc 
 * @param nsid 
 * @param feat_id 
 * @return int 
 */
int nvme_get_feature_cmd(int file_desc, uint32_t nsid, uint8_t feat_id)
{
    struct nvme_features feat_cmd = {
        .opcode = nvme_admin_get_features,
        .nsid = nsid,
        .fid = feat_id,
    };
    struct nvme_64b_cmd user_cmd = {
        .q_id = 0,
        .bit_mask = 0,
        .cmd_buf_ptr = (u_int8_t *)&feat_cmd,
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = DMA_BIDIRECTIONAL,
    };
    return nvme_64b_cmd(file_desc, &user_cmd);
}

/**
 * @brief 
 * 
 * @param file_desc 
 * @param cq_id Existing or non-existing CQ ID.
 * @param cq_size Total number of entries that need kernal mem
 * @param contig Indicates if SQ is contig or not, 1 = contig
 * @param irq_en 
 * @param irq_no 
 * @return int 
 */
static int nvme_prep_cq(int file_desc, uint16_t cq_id, uint32_t cq_size, uint8_t contig,
                        uint8_t irq_en, uint16_t irq_no)
{
    struct nvme_prep_cq prep_cq = {
        .cq_id = cq_id,
        .elements = cq_size,
        .contig = contig,
        .cq_irq_en = irq_en,
        .cq_irq_no = irq_no,
    };
    return ioctl(file_desc, NVME_IOCTL_PREPARE_CQ_CREATION, &prep_cq);
}
/**
 * @brief 
 * 
 * @return int 
 */
int nvme_admin_ring_dbl_reap_cq(int file_desc)
{
    uint32_t reap_num = 0;
    int ret_val = SUCCEED;
    ret_val |= ioctl_tst_ring_dbl(file_desc, 0);
    ret_val |= cq_gain(0, 1, &reap_num);
    return ret_val;
}
/**
 * @brief 
 * 
 * @param file_desc 
 * @param cq_id 
 * @param cq_size 
 * @param irq_en 
 * @param irq_no 
 * @return int 
 */
int nvme_create_contig_iocq(int file_desc, uint16_t cq_id, uint32_t cq_size, uint8_t irq_en, uint16_t irq_no)
{
    int ret_val = SUCCEED;
    ret_val = nvme_prep_cq(file_desc, cq_id, cq_size, 1, irq_en, irq_no);
    if (SUCCEED != ret_val)
    {
        pr_err("\tCQ ID = %d Preparation Failed! %d\n", cq_id, ret_val);
        return FAILED;
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
        .q_id = 0,
        .bit_mask = (NVME_MASK_PRP1_PAGE),
        .cmd_buf_ptr = (u_int8_t *)&create_cq_cmd,
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = DMA_BIDIRECTIONAL,
    };
    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (SUCCEED != ret_val)
    {
        pr_err("\tcq_id %d nvme_64b_cmd failed %d\n", cq_id, ret_val);
        return FAILED;
    }
    return nvme_admin_ring_dbl_reap_cq(file_desc);
}

/**
 * @brief 
 * 
 * @param file_desc 
 * @param cq_id 
 * @param cq_size 
 * @param irq_en 
 * @param irq_no 
 * @param discontg_cq_buf 
 * @param discontig_cq_size 
 * @return int 
 */
int nvme_create_discontig_iocq(int file_desc, uint16_t cq_id, uint32_t cq_size, uint8_t irq_en, uint16_t irq_no,
                               uint8_t const *discontg_cq_buf, uint32_t discontig_cq_size)
{
    int ret_val = FAILED;
    ret_val = nvme_prep_cq(file_desc, cq_id, cq_size, 0, irq_en, irq_no);
    if (SUCCEED != ret_val)
    {
        pr_err("\tCQ ID = %d Preparation Failed! %d\n", cq_id, ret_val);
        return FAILED;
    }

    struct nvme_create_cq create_cq_cmd = {
        .opcode = nvme_admin_create_cq,
        .cqid = cq_id,
        .qsize = cq_size - 1,
        .cq_flags = ((irq_en << 1) | 0),
        .irq_vector = irq_no,
    };

    struct nvme_64b_cmd user_cmd = {
        .q_id = 0,
        .bit_mask = (NVME_MASK_PRP1_LIST),
        .cmd_buf_ptr = (u_int8_t *)&create_cq_cmd,
        .data_buf_size = discontig_cq_size,
        .data_buf_ptr = discontg_cq_buf,
        .data_dir = DMA_BIDIRECTIONAL,
    };
    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (SUCCEED != ret_val)
    {
        pr_err("\tcq_id %d nvme_64b_cmd failed %d\n", cq_id, ret_val);
        return FAILED;
    }
    return nvme_admin_ring_dbl_reap_cq(file_desc);
}

/**
 * @brief 
 * 
 * @param file_desc 
 * @param cq_id Existing or non-existing SQ ID.
 * @param cq_size Total number of entries that need kernal mem
 * @param contig Indicates if SQ is contig or not, 1 = contig
 * @param irq_en 
 * @param irq_no 
 * @return int 
 */
static int nvme_prep_sq(int file_desc, uint16_t sq_id, uint16_t cq_id, uint32_t sq_size, uint8_t contig, uint8_t sq_prio)
{
    struct nvme_prep_sq prep_sq = {
        .sq_id = sq_id,
        .cq_id = cq_id,
        .elements = sq_size,
        .contig = contig,
        .sq_prio = sq_prio,
    };
    return ioctl(file_desc, NVME_IOCTL_PREPARE_SQ_CREATION, &prep_sq);
}

/**
 * @brief 
 * 
 * @param file_desc 
 * @param sq_id Existing or non-existing SQ ID.
 * @param cq_id assoc CQ ID.
 * @param sq_size Total number of entries that need kernal mem
 * @param sq_prio 
 * @return int 
 */
int nvme_create_contig_iosq(int file_desc, uint16_t sq_id, uint16_t cq_id, uint32_t sq_size, uint8_t sq_prio)
{
    int ret_val = FAILED;
    ret_val = nvme_prep_sq(file_desc, sq_id, cq_id, sq_size, 1, sq_prio);
    if (SUCCEED != ret_val)
    {
        pr_err("\tSQ ID = %d Preparation Failed! %d\n", cq_id, ret_val);
        return FAILED;
    }

    struct nvme_create_sq create_sq_cmd = {
        .opcode = nvme_admin_create_sq,
        .sqid = sq_id,
        .qsize = sq_size - 1,
        .cqid = cq_id,
        .sq_flags = ((((uint16_t)sq_prio) << 1) | 1),
    };

    struct nvme_64b_cmd user_cmd = {
        .q_id = 0,
        .cmd_buf_ptr = (u_int8_t *)&create_sq_cmd,
        .bit_mask = (NVME_MASK_PRP1_PAGE),
        .data_buf_size = 0,
        .data_buf_ptr = NULL,
        .data_dir = DMA_BIDIRECTIONAL,
    };
    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (SUCCEED != ret_val)
    {
        pr_err("\tcq_id %d nvme_64b_cmd failed %d\n", cq_id, ret_val);
        return FAILED;
    }
    return nvme_admin_ring_dbl_reap_cq(file_desc);
}

/**
 * @brief 
 * 
 * @param file_desc 
 * @param sq_id Existing or non-existing SQ ID.
 * @param cq_id assoc CQ ID.
 * @param sq_size Total number of entries that need kernal mem
 * @param sq_prio 
 * @param discontg_sq_buf 
 * @param discontig_sq_size 
 * @return int 
 */
int nvme_create_discontig_iosq(int file_desc, uint16_t sq_id, uint16_t cq_id, uint32_t sq_size, uint8_t sq_prio,
                               uint8_t const *discontg_sq_buf, uint32_t discontig_sq_size)
{
    int ret_val = FAILED;
    ret_val = nvme_prep_sq(file_desc, sq_id, cq_id, sq_size, 0, sq_prio);
    if (SUCCEED != ret_val)
    {
        pr_err("\tSQ ID = %d Preparation Failed! %d\n", cq_id, ret_val);
        return FAILED;
    }
    struct nvme_create_sq create_sq_cmd = {
        .opcode = nvme_admin_create_sq,
        .sqid = sq_id,
        .qsize = sq_size - 1,
        .cqid = cq_id,
        .sq_flags = ((((uint16_t)sq_prio) << 1) | 0),
    };

    struct nvme_64b_cmd user_cmd = {
        .q_id = 0,
        .bit_mask = (NVME_MASK_PRP1_LIST),
        .cmd_buf_ptr = (u_int8_t *)&create_sq_cmd,
        .data_buf_size = discontig_sq_size,
        .data_buf_ptr = discontg_sq_buf,
        .data_dir = DMA_FROM_DEVICE,
    };
    ret_val = nvme_64b_cmd(file_desc, &user_cmd);
    if (SUCCEED != ret_val)
    {
        pr_err("\tcq_id %d nvme_64b_cmd failed %d\n", cq_id, ret_val);
        return FAILED;
    }
    return nvme_admin_ring_dbl_reap_cq(file_desc);
}

int nvme_delete_ioq(int file_desc, uint8_t opcode, uint16_t qid)
{
    int ret_val = FAILED;
    ret_val = ioctl_delete_ioq(file_desc, opcode, qid);
    if (SUCCEED != ret_val)
    {
        pr_err("\tdel qid:%d Failed! %d\n", qid, ret_val);
        return FAILED;
    }
    return nvme_admin_ring_dbl_reap_cq(file_desc);
}

/********** PCIe RC retrain link **********/
void pcie_retrain_link(void)
{
    system("setpci -s " RC_CAP_LINK_CONTROL ".b=20:20");
    usleep(100000); // 100 ms
}

/********** PCIe hot reset **********/
dword_t pcie_hot_reset(void)
{
    int ret_val = FAILED;
    uint8_t idx = 0;
    uint32_t u32_tmp_data = 0;
    uint32_t bar_reg[6];

    for (idx = 0; idx < 6; idx++)
    {
        bar_reg[idx] = pci_read_dword(file_desc, 0x10 + idx * 4);
    }

    system("setpci -s " RC_BRIDGE_CONTROL ".b=40:40");
    usleep(50000); // 100 ms
    system("setpci -s " RC_BRIDGE_CONTROL ".b=00:40");

    usleep(50000); // 100 ms

    u32_tmp_data = ((uint32_t)(0x7)); // bus master/memory space/io space enable
    ret_val = ioctl_pci_write_data(file_desc, 0x4, 4, (uint8_t *)&u32_tmp_data);
    if (ret_val < 0)
        return FAILED;

    for (idx = 0; idx < 6; idx++)
    {
        ioctl_pci_write_data(file_desc, 0x10 + idx * 4, 4, (uint8_t *)&bar_reg[idx]);
    }

    pcie_retrain_link();
    return SUCCEED;
}

/********** PCIe link down **********/
dword_t pcie_link_down(void)
{
    uint32_t u32_tmp_data = 0;
    uint32_t bar_reg[6];
    uint8_t idx = 0;
    int ret_val = FAILED;
    for (idx = 0; idx < 6; idx++)
    {
        bar_reg[idx] = pci_read_dword(file_desc, 0x10 + idx * 4);
    }

    system("setpci -s " RC_CAP_LINK_CONTROL ".b=10:10");
    usleep(100000); // 100 ms
    system("setpci -s " RC_CAP_LINK_CONTROL ".b=00:10");
    usleep(100000); // 100 ms

    u32_tmp_data = ((uint32_t)(0x7)); // bus master/memory space/io space enable
    ret_val = ioctl_pci_write_data(file_desc, 0x4, 4, (uint8_t *)&u32_tmp_data);
    if (ret_val < 0)
        return FAILED;

    for (idx = 0; idx < 6; idx++)
    {
        ioctl_pci_write_data(file_desc, 0x10 + idx * 4, 4, (uint8_t *)&bar_reg[idx]);
    }

    pcie_retrain_link();
    return SUCCEED;
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
    char cmd[64] = "setpci -s " RC_CAP_LINK_CONTROL2 ".b=";
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
    if (!((1 == width) || (2 == width) || (4 == width)))
    {
        pr_err("width error\r\n");
        assert(0);
    }
    //beagle;cougar;eagle;falcon
    uint32_t u32_tmp_data = pci_read_dword(file_desc, 0x8C0);
    u32_tmp_data &= 0xFFFFFF80;
    u32_tmp_data |= (0x000000040 + width);
    ioctl_pci_write_data(file_desc, 0x8C0, 4, (uint8_t *)&u32_tmp_data);
}

void pcie_random_speed_width(void)
{
    uint32_t u32_tmp_data = 0;
    uint8_t set_speed, set_width, cur_speed, cur_width;
    uint8_t speed_arr[] = {1, 2, 3};
    uint8_t width_arr[] = {1, 2, 4};

    // get speed and width random
    set_speed = speed_arr[BYTE_RAND() % ARRAY_SIZE(speed_arr)];
    set_width = width_arr[BYTE_RAND() % ARRAY_SIZE(width_arr)];
    if (g_nvme_dev.link_speed < set_speed)
    {
        set_speed = g_nvme_dev.link_speed;
    }
    if (g_nvme_dev.link_width < set_width)
    {
        set_width = g_nvme_dev.link_width;
    }
    pr_info("Set_PCIe_Gen%d, lane width X%d\n", set_speed, set_width);
    // fflush(stdout);
    // cfg speed (RC)
    pcie_RC_cfg_speed(set_speed);
    // cfg width (device)
    pcie_set_width(set_width);

    pcie_retrain_link();
    // check Link status register
    u32_tmp_data = pci_read_word(file_desc, g_nvme_dev.pxcap_ofst + 0x12);
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
 * @return dword_t 
 */
dword_t nvme_msi_register_test(void)
{
    uint32_t u32_tmp_data = 0;

    u32_tmp_data = ioctl_read_data(file_desc, NVME_REG_INTMS_OFST, 4);
    pr_info("NVME_REG_INTMS_OFST:%#x\n", u32_tmp_data);
    u32_tmp_data = DWORD_MASK;
    if (ioctl_write_data(file_desc, NVME_REG_INTMS_OFST, 4, (uint8_t *)&u32_tmp_data))
        return FAILED;
    u32_tmp_data = ioctl_read_data(file_desc, NVME_REG_INTMS_OFST, 4);
    pr_info("set NVME_REG_INTMS_OFST = DWORD_MASK, read register:%#x\n", u32_tmp_data);

    u32_tmp_data = ioctl_read_data(file_desc, NVME_REG_INTMC_OFST, 4);
    pr_info("NVME_REG_INTMC_OFST:%#x\n", u32_tmp_data);
    u32_tmp_data = DWORD_MASK;
    if (ioctl_write_data(file_desc, NVME_REG_INTMC_OFST, 4, (uint8_t *)&u32_tmp_data))
        return FAILED;
    u32_tmp_data = ioctl_read_data(file_desc, NVME_REG_INTMC_OFST, 4);
    pr_info("set NVME_REG_INTMC_OFST = DWORD_MASK, read register:%#x\n", u32_tmp_data);

    return SUCCEED;
}
/**
 * @brief Create a all io queue object
 * 
 * @param flags 
 * @return dword_t 
 */
dword_t create_all_io_queue(byte_t flags)
{
    byte_t int_type = 0;
    byte_t rdm_irq_no = 0;
    uint16_t num_irqs;
    uint32_t reap_num = 0;

    struct create_cq_parameter cq_parameter = {0};
    struct create_sq_parameter sq_parameter = {0};
    if (g_nvme_dev.id_ctrl.vid == SAMSUNG_CTRL_VID)
        return FAILED;

    int_type = BYTE_RAND() % 4;

    if (int_type == NVME_INT_PIN || int_type == NVME_INT_MSI_SINGLE)
    {
        num_irqs = 1;
    }
    else
    {
        num_irqs = g_nvme_dev.max_sq_num + 1;
    }

    rdm_irq_no = BYTE_RAND() % num_irqs;
    pr_debug("create queue int_type:%d num_irqs:%d\n", int_type, num_irqs);

    test_change_init(file_desc, MAX_ADMIN_QUEUE_SIZE, MAX_ADMIN_QUEUE_SIZE, int_type, num_irqs);

    random_sq_cq_info();
    /**********************************************************************/
    for (dword_t sqidx = 0; sqidx < g_nvme_dev.max_sq_num; sqidx++)
    {
        cq_parameter.cq_id = ctrl_sq_info[sqidx].cq_id;
        cq_parameter.cq_size = ctrl_sq_info[sqidx].cq_size;
        cq_parameter.contig = 1;
        cq_parameter.irq_en = 1;
        if (int_type == NVME_INT_PIN || int_type == NVME_INT_MSI_SINGLE)
        {
            cq_parameter.irq_no = 0;
        }
        else
        {
            if (1 == flags) //for test all_io_queue_with_one_irq_no
            {
                cq_parameter.irq_no = rdm_irq_no;
            }
            else
            {
                cq_parameter.irq_no = ctrl_sq_info[sqidx].cq_int_vct;
            }
        }
        pr_debug("create cq: %d\n", cq_parameter.cq_id);
        create_iocq(file_desc, &cq_parameter);
    }
    ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    cq_gain(ADMIN_QUEUE_ID, g_nvme_dev.max_sq_num, &reap_num);
    pr_debug("  cq reaped ok! reap_num:%d\n", reap_num);
    /**********************************************************************/
    for (dword_t sqidx = 0; sqidx < g_nvme_dev.max_sq_num; sqidx++)
    {
        sq_parameter.cq_id = ctrl_sq_info[sqidx].cq_id;
        sq_parameter.sq_id = ctrl_sq_info[sqidx].sq_id;
        sq_parameter.sq_size = ctrl_sq_info[sqidx].sq_size;
        sq_parameter.contig = 1;
        sq_parameter.sq_prio = MEDIUM_PRIO;
        pr_debug("create sq: %d, assoc cq: %d\n", sq_parameter.sq_id, 
            sq_parameter.cq_id);
        create_iosq(file_desc, &sq_parameter);
    }
    ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    cq_gain(ADMIN_QUEUE_ID, g_nvme_dev.max_sq_num, &reap_num);
    pr_debug("  cq reaped ok! reap_num:%d\n", reap_num);
    return SUCCEED;
}

dword_t delete_all_io_queue(void)
{
    uint32_t reap_num = 0;
    for (dword_t sqidx = 0; sqidx < g_nvme_dev.max_sq_num; sqidx++)
    {
        ioctl_delete_ioq(file_desc, nvme_admin_delete_sq, ctrl_sq_info[sqidx].sq_id);
    }
    ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    cq_gain(ADMIN_QUEUE_ID, g_nvme_dev.max_sq_num, &reap_num);
    pr_debug("  cq reaped ok! reap_num:%d\n", reap_num);

    for (dword_t sqidx = 0; sqidx < g_nvme_dev.max_sq_num; sqidx++)
    {
        ioctl_delete_ioq(file_desc, nvme_admin_delete_cq, ctrl_sq_info[sqidx].cq_id);
    }
    ioctl_tst_ring_dbl(file_desc, ADMIN_QUEUE_ID);
    cq_gain(ADMIN_QUEUE_ID, g_nvme_dev.max_sq_num, &reap_num);
    pr_debug("  cq reaped ok! reap_num:%d\n", reap_num);
    return SUCCEED;
}

int nvme_send_iocmd(int file_desc, uint8_t cmp_dis, uint16_t sq_id, uint32_t nsid, uint64_t slba, uint16_t nlb, void *data_addr)
{
    byte_t iocmd_type = BYTE_RAND() % 3;
    word_t fua_en = ((BYTE_RAND() % 2) ? NVME_RW_FUA : 0);
    if (cmp_dis)
    {
        iocmd_type = 1;
    }
    if (iocmd_type == 0)
    {
        return nvme_io_read_cmd(file_desc, 0, sq_id, nsid, slba, nlb, fua_en, data_addr);
    }
    else if (iocmd_type == 1)
    {
        return nvme_io_write_cmd(file_desc, 0, sq_id, nsid, slba, nlb, fua_en, data_addr);
    }
    else
    {
        return nvme_io_compare_cmd(file_desc, 0, sq_id, nsid, slba, nlb, fua_en, data_addr);
    }
}
