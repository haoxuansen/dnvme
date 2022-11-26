/*
 * NVM Express Compliance Suite
 * Copyright (c) 2011, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/scatterlist.h>

#include "core.h"
#include "cmd.h"

#include "definitions.h"
#include "dnvme_reg.h"
#include "dnvme_ds.h"
#include "dnvme_cmds.h"
#include <linux/vmalloc.h>

/* Debug flag for IOCT_SEND_64B module */
#define TEST_PRP_DEBUG

/* Declaration of static functions belonging to Submitting 64Bytes Command */
static int data_buf_to_prp(struct nvme_device *nvme_dev,
    struct nvme_sq *pmetrics_sq, struct nvme_64b_cmd *nvme_64b_cmd,
    struct nvme_prps *prps, u8 opcode, u16 persist_q_id,
    enum data_buf_type data_buf_type, u16 cmd_id);
static int map_user_pg_to_dma(struct nvme_device *nvme_dev,
    enum dma_data_direction kernel_dir, unsigned long buf_addr,
    unsigned total_buf_len, struct scatterlist **sg_list,
    struct nvme_prps *prps, enum data_buf_type data_buf_type);
static int pages_to_sg(struct page **pages, int num_pages, int buf_offset,
    unsigned len, struct scatterlist **sg_list);
static int setup_prps(struct nvme_device *nvme_dev, struct scatterlist *sg,
    s32 buf_len, struct nvme_prps *prps, u8 cr_io_q,
    enum nvme_64b_cmd_mask prp_mask);
static void unmap_user_pg_to_dma(struct nvme_device *nvme_dev,
    struct nvme_prps *prps);
static void free_prp_pool(struct nvme_device *nvme_dev,
    struct nvme_prps *prps, u32 npages);

//****2020/10/24 meng_yu add start****
//refer linux nvme driver

#define SGES_PER_PAGE	(PAGE_SIZE / sizeof(struct nvme_sgl_desc))

/*
 * setup_sgls:
 * Sets up SGL'sfrom DMA'ed memory
 * Returns Error codes
 */
static int setup_sgls(struct nvme_device *nvme_dev, 
    struct scatterlist *sg,
    s32 buf_len, 
    struct nvme_prps *prps, 
    struct nvme_gen_cmd *nvme_gen_cmd,
    enum nvme_64b_cmd_mask prp_mask,
    int entries)
{
    dma_addr_t sgl_dma, dma_addr;
    s32 dma_len; /* Length of DMA'ed SG */
    struct nvme_sgl_desc *sg_list;
    u32 offset;
    u32 num_prps, num_pg, sgl_page = 0;
    struct dma_pool *sgl_page_pool;
    int i = 0;
    dnvme_vdbg("-->[%s]",__FUNCTION__);

    dma_addr = sg_dma_address(sg);
    dma_len = sg_dma_len(sg);
    offset = offset_in_page(dma_addr);

    if(entries == 1)
    {
        dnvme_sgl_set_data(&nvme_gen_cmd->dptr.sgl, prps->sg);
        return 0;
    }

    /* Generate PRP List */
    num_prps = DIV_ROUND_UP(offset + buf_len, PAGE_SIZE);
    /* Taking into account the last entry of PRP Page */
    num_pg = DIV_ROUND_UP(NVME_PRP_ENTRY_SIZE * num_prps, PAGE_SIZE - NVME_PRP_ENTRY_SIZE);

    prps->vir_prp_list = kmalloc(sizeof(__le64 *) * num_pg, GFP_ATOMIC);
    if (NULL == prps->vir_prp_list) {
        dnvme_err("Memory allocation for virtual list failed");
        return -ENOMEM;
    }

    sgl_page = 0;
    sgl_page_pool = nvme_dev->priv.prp_page_pool;

    sg_list = dma_pool_alloc(sgl_page_pool, GFP_ATOMIC, &sgl_dma);
    if (NULL == sg_list) {
        kfree(prps->vir_prp_list);
        dnvme_err("Memory allocation for sgl page failed");
        return -ENOMEM;
    }

    prps->vir_prp_list[sgl_page++] = (__le64 *)sg_list;
    prps->npages = sgl_page;
    prps->first_dma = sgl_dma;
    dnvme_sgl_set_seg(&nvme_gen_cmd->dptr.sgl, sgl_dma, entries);

    do{
        if(i == SGES_PER_PAGE)
        {
            struct nvme_sgl_desc *old_sg_desc = sg_list;
            struct nvme_sgl_desc *link = &old_sg_desc[i - 1];
            sg_list = dma_pool_alloc(sgl_page_pool, GFP_ATOMIC, &sgl_dma);
            if (!sg_list) {
                kfree(prps->vir_prp_list);
                dnvme_err("Memory allocation for sgl page failed");
                return -ENOMEM;
            }
            i = 0;
            sg_list[i++] = *link;
            prps->vir_prp_list[sgl_page++] = (__le64 *)sg_list;
            prps->npages = sgl_page;
            dnvme_sgl_set_seg(link, sgl_dma, entries);
        }
        dnvme_sgl_set_data(&sg_list[i++],sg);
        sg = sg_next(sg);
    }while(--entries > 0);
    return 0;
}

/*
 * data_buf_to_sgl:
 * Creates persist or non persist SGL's from data_buf_ptr memory
 * and addes a node inside cmd track list pointed by pmetrics_sq
 */
static int data_buf_to_sgl(struct nvme_device *nvme_dev,
    struct nvme_sq *pmetrics_sq, struct nvme_64b_cmd *nvme_64b_cmd,
    struct nvme_prps *prps, struct nvme_gen_cmd *nvme_gen_cmd, 
    u16 persist_q_id, enum data_buf_type data_buf_type)
{
    int err;
    unsigned long addr;
    struct scatterlist *sg_list = NULL;
    enum dma_data_direction kernel_dir;
    uint8_t opcode = nvme_gen_cmd->opcode; 
    uint16_t cmd_id = nvme_gen_cmd->command_id;
    dnvme_vdbg("-->[%s]",__FUNCTION__);

    /* Catch common mistakes */
    addr = (unsigned long)nvme_64b_cmd->data_buf_ptr;
    if ((addr & 3) || (addr == 0) ||
        (nvme_64b_cmd->data_buf_size == 0) || (nvme_dev == NULL)) {

        dnvme_err("Invalid Arguments");
        return -EINVAL;
    }

    /* Typecase is only possible because the kernel vs. user space contract
     * states the following which agrees with 'enum dma_data_direction'
     * 0=none; 1=to_device, 2=from_device, 3=bidirectional, others illegal */
    kernel_dir = (enum dma_data_direction)nvme_64b_cmd->data_dir;

    /* Mapping user pages to dma memory */
    err = map_user_pg_to_dma(nvme_dev, kernel_dir, addr,
        nvme_64b_cmd->data_buf_size, &sg_list, prps, data_buf_type);
    if (err < 0) {
        return err;
    }
    dnvme_vdbg("sgl_num_map_pgs:%#x",prps->num_map_pgs);
    err = setup_sgls(nvme_dev, sg_list, nvme_64b_cmd->data_buf_size, prps,
                    nvme_gen_cmd, nvme_64b_cmd->bit_mask, prps->num_map_pgs);
    if (err < 0) {
        unmap_user_pg_to_dma(nvme_dev, prps);
        return err;
    }

    /* Adding node inside nvme_cmd_node list for pmetrics_sq */
    err = add_cmd_track_node(pmetrics_sq, persist_q_id, prps, opcode, cmd_id);
    if (err < 0) {
        dnvme_err("Failure to add command track node");
        goto err_unmap_prp_pool;
    }

    dnvme_vdbg("<--[%s]",__FUNCTION__);
    return 0;

err_unmap_prp_pool:
    unmap_user_pg_to_dma(nvme_dev, prps);
    free_prp_pool(nvme_dev, prps, prps->npages);
    return err;
}

//****2020/10/24 meng_yu add end****

/* prep_send64b_cmd:
 * Prepares the 64 byte command to be sent
 * with PRP generation and addition of nodes
 * inside cmd track list
 */
int prep_send64b_cmd(struct nvme_device *nvme_dev, struct nvme_sq
    *pmetrics_sq, struct nvme_64b_cmd *nvme_64b_cmd, struct nvme_prps *prps,
    struct nvme_gen_cmd *nvme_gen_cmd, u16 persist_q_id,
    enum data_buf_type data_buf_type, u8 gen_prp)
{
    int ret_code;

    if (gen_prp) {
//****2020/10/23 meng_yu add end****
        if(dnvme_use_sgls(nvme_gen_cmd,nvme_64b_cmd))
        {
            //test
            ret_code = data_buf_to_sgl(nvme_dev, pmetrics_sq, nvme_64b_cmd, prps,
                nvme_gen_cmd, persist_q_id, data_buf_type);
            if (ret_code < 0) {
                dnvme_err("Data buffer to SGL generation failed");
                return ret_code;
            }
            dnvme_vdbg("sgl_dbg:%d: adr:%#llx,len:%#x,typ:%#x",__LINE__,
                nvme_gen_cmd->dptr.sgl.addr,
                nvme_gen_cmd->dptr.sgl.length,
                nvme_gen_cmd->dptr.sgl.type);
        }
        else
//****2020/10/23 meng_yu add end****
        {
            /* Create PRP and add the node inside the command track list */
            ret_code = data_buf_to_prp(nvme_dev, pmetrics_sq, nvme_64b_cmd, prps,
                nvme_gen_cmd->opcode, persist_q_id, data_buf_type,
                nvme_gen_cmd->command_id);
            if (ret_code < 0) {
                dnvme_err("Data buffer to PRP generation failed");
                return ret_code;
            }
            /* Update the PRP's in the command based on type */
            if ((prps->type == (PRP1 | PRP2)) ||
                (prps->type == (PRP2 | PRP_List))) {
                nvme_gen_cmd->dptr.prp1 = cpu_to_le64(prps->prp1);
                nvme_gen_cmd->dptr.prp2 = cpu_to_le64(prps->prp2);
            } else {
                nvme_gen_cmd->dptr.prp1 = cpu_to_le64(prps->prp1);
            }
        }
    } else {
        /* Adding node inside nvme_cmd_node list for pmetrics_sq */
        ret_code = add_cmd_track_node(pmetrics_sq, persist_q_id, prps,
            nvme_gen_cmd->opcode, nvme_gen_cmd->command_id);
        if (ret_code < 0) {
            dnvme_err("Failure to add command track node for"
                " Create Contig Queue Command");
            return ret_code;
        }
    }
    return 0;
}

/*
 * add_cmd_track_node:
 * Create and add the node inside command track list
 */
int add_cmd_track_node(struct nvme_sq *pmetrics_sq,
    u16 persist_q_id, struct nvme_prps *prps, u8 opcode, u16 cmd_id)
{
    /* pointer to cmd track linked list node */
    struct nvme_cmd_node  *pcmd_track_list;

    /* Fill the nvme_cmd_node structure */
    pcmd_track_list = kmalloc(sizeof(struct nvme_cmd_node),
        GFP_ATOMIC | __GFP_ZERO);
    if (pcmd_track_list == NULL) {
        dnvme_err("Failed to alloc memory for the command track list");
        return -ENOMEM;
    }

    /* Fill the node */
    pcmd_track_list->unique_id = cmd_id;
    pcmd_track_list->persist_q_id = persist_q_id;
    pcmd_track_list->opcode = opcode;
    /* non_persist PRP's not filled for create/delete contig/discontig IOQ */
    if (!persist_q_id) {
        memcpy(&pcmd_track_list->prp_nonpersist, prps,
            sizeof(struct nvme_prps));
    }

    /* Add an element to the end of the list */
    list_add_tail(&pcmd_track_list->entry,
        &pmetrics_sq->priv.cmd_list);
    dnvme_vdbg("Node created and added inside command track list");
    return 0;
}

/**
 * @brief Delete command list in the SQ
 * 
 * @param ndev NVMe device
 * @param sq the specified submission queue
 */
void dnvme_delete_cmd_list(struct nvme_device *ndev, struct nvme_sq *sq)
{
	struct nvme_cmd_node *cmd;
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &sq->priv.cmd_list) {
		cmd = list_entry(pos, struct nvme_cmd_node, entry);
		dnvme_delete_prps(ndev, &cmd->prp_nonpersist);
		list_del(pos);
		kfree(cmd);
	}
}

/*
 * dnvme_delete_prps:
 * Deletes the PRP structures of SQ/CQ or command track node
 */
void dnvme_delete_prps(struct nvme_device *nvme_device, struct nvme_prps *prps)
{
    /* First unmap the dma */
    unmap_user_pg_to_dma(nvme_device, prps);
    /* free prp list pointed by this non contig cq */
    free_prp_pool(nvme_device, prps, prps->npages);
}

/*
 * destroy_dma_pool:
 * Destroy's the dma pool
 * Returns void
 */
void destroy_dma_pool(struct nvme_device *nvme_dev)
{
    /* Destroy the DMA pool */
    dma_pool_destroy(nvme_dev->priv.prp_page_pool);
}

/*
 * data_buf_to_prp:
 * Creates persist or non persist PRP's from data_buf_ptr memory
 * and addes a node inside cmd track list pointed by pmetrics_sq
 */
static int data_buf_to_prp(struct nvme_device *nvme_dev,
    struct nvme_sq *pmetrics_sq, struct nvme_64b_cmd *nvme_64b_cmd,
    struct nvme_prps *prps, u8 opcode, u16 persist_q_id,
    enum data_buf_type data_buf_type, u16 cmd_id)
{
    int err;
    unsigned long addr;
    struct scatterlist *sg_list = NULL;
    enum dma_data_direction kernel_dir;
#ifdef TEST_PRP_DEBUG
    int last_prp, i, j;
    __le64 *prp_vlist;
    s32 num_prps;
#endif


    /* Catch common mistakes */
    addr = (unsigned long)nvme_64b_cmd->data_buf_ptr;
    if ((addr & 3) || (addr == 0) ||
        (nvme_64b_cmd->data_buf_size == 0) || (nvme_dev == NULL)) {

        dnvme_err("Invalid Arguments");
        return -EINVAL;
    }

    /* Typecase is only possible because the kernel vs. user space contract
     * states the following which agrees with 'enum dma_data_direction'
     * 0=none; 1=to_device, 2=from_device, 3=bidirectional, others illegal */
    kernel_dir = (enum dma_data_direction)nvme_64b_cmd->data_dir;

    /* Mapping user pages to dma memory */
    err = map_user_pg_to_dma(nvme_dev, kernel_dir, addr,
        nvme_64b_cmd->data_buf_size, &sg_list, prps, data_buf_type);
    if (err < 0) {
        return err;
    }

    err = setup_prps(nvme_dev, sg_list, nvme_64b_cmd->data_buf_size, prps,
        data_buf_type, nvme_64b_cmd->bit_mask);
    if (err < 0) {
        unmap_user_pg_to_dma(nvme_dev, prps);
        return err;
    }

#ifdef TEST_PRP_DEBUG
    last_prp = PAGE_SIZE / NVME_PRP_ENTRY_SIZE - 1;
    if (prps->type == (PRP1 | PRP_List)) {
        num_prps = DIV_ROUND_UP(nvme_64b_cmd->data_buf_size +
            offset_in_page(addr), PAGE_SIZE);
    } else {
        num_prps = DIV_ROUND_UP(nvme_64b_cmd->data_buf_size, PAGE_SIZE);
    }

    if (prps->type == (PRP1 | PRP_List) || prps->type == (PRP2 | PRP_List)) {
        if (!(prps->vir_prp_list)) {
            dnvme_err("Creation of PRP failed");
            err = -ENOMEM;
            goto err_unmap_prp_pool;
        }
        prp_vlist = prps->vir_prp_list[0];
        if (prps->type == (PRP2 | PRP_List)) {
            dnvme_vdbg("P1 Entry: %llx", (unsigned long long) prps->prp1);
        }
        for (i = 0, j = 0; i < num_prps; i++) {

            if (j < (prps->npages - 1) && i == last_prp) {
                j++;
                num_prps -= i;
                i = 0 ;
                prp_vlist = prps->vir_prp_list[j];
                dnvme_vdbg("Physical address of next PRP Page: %llx",
                    (__le64) prp_vlist);
            }

            dnvme_vdbg("PRP List: %llx", (unsigned long long) prp_vlist[i]);
        }

    } else if (prps->type == PRP1) {
        dnvme_vdbg("P1 Entry: %llx", (unsigned long long) prps->prp1);
    } else {
        dnvme_vdbg("P1 Entry: %llx", (unsigned long long) prps->prp1);
        dnvme_vdbg("P2 Entry: %llx", (unsigned long long) prps->prp2);
    }
#endif

    /* Adding node inside nvme_cmd_node list for pmetrics_sq */
    err = add_cmd_track_node(pmetrics_sq, persist_q_id, prps, opcode, cmd_id);
    if (err < 0) {
        dnvme_err("Failure to add command track node");
        goto err_unmap_prp_pool;
    }

    dnvme_vdbg("PRP Built and added to command track node successfully");
    return 0;

err_unmap_prp_pool:
    unmap_user_pg_to_dma(nvme_dev, prps);
    free_prp_pool(nvme_dev, prps, prps->npages);
    return err;
}

static int map_user_pg_to_dma(struct nvme_device *nvme_dev,
    enum dma_data_direction kernel_dir, unsigned long buf_addr,
    unsigned total_buf_len, struct scatterlist **sg_list,
    struct nvme_prps *prps, enum data_buf_type data_buf_type)
{
    int i, err, buf_pg_offset, buf_pg_count, num_sg_entries;
    struct page **pages;
    void *buf = NULL;


    buf_pg_offset = offset_in_page(buf_addr);
    buf_pg_count = DIV_ROUND_UP(buf_pg_offset + total_buf_len, PAGE_SIZE);
    dnvme_vdbg("User buf addr = 0x%016lx", buf_addr);
    dnvme_vdbg("User buf pg offset = 0x%08x", buf_pg_offset);
    dnvme_vdbg("User buf pg count = 0x%08x", buf_pg_count);
    dnvme_vdbg("User buf total length = 0x%08x", total_buf_len);

    pages = kcalloc(buf_pg_count, sizeof(*pages), GFP_KERNEL);
    if (pages  == NULL) {
        dnvme_err("Memory alloc for describing user pages failed");
        return -ENOMEM;
    }

    /* Pinning user pages in memory, always assuming writing in case user space
     * specifies an incorrect direction of data xfer */
    err = get_user_pages_fast(buf_addr, buf_pg_count, FOLL_WRITE, pages);
    if (err < buf_pg_count) {
        dnvme_err("\n@@@@@@@@@@@@@  buf_pg_count: %d, err: %d", buf_pg_count, err);
        dnvme_err("User buf addr = 0x%016lx", buf_addr);
        dnvme_err("User buf pg offset = 0x%08x", buf_pg_offset);
        dnvme_err("User buf pg count = 0x%08x", buf_pg_count);
        dnvme_err("User buf total length = 0x%08x", total_buf_len);
        buf_pg_count = err;
        err = -EFAULT;
        dnvme_err("Pinning down user pages failed");
        goto error;
    }

    /* Kernel needs direct access to all Q memory, so discontiguously backed */
    /* IOQ's must be mapped to allow the access to the memory */
    if (data_buf_type == NVME_IO_QUEUE_DISCONTIG) {
        /* Note: Not suitable for pages with offsets, but since discontig back'd
         *       Q's are required to be page aligned this isn't an issue */
        buf = vmap(pages, buf_pg_count, VM_MAP, PAGE_KERNEL);
        dnvme_vdbg("Map'd user space buf to vir Kernel Addr: %p", buf);
        if (buf == NULL) {
            err = -EFAULT;
            dnvme_err("Unable to map user space buffer to kernel space");
            goto error;
        }
    }

    /* Generate SG List from pinned down pages */
    err = pages_to_sg(pages, buf_pg_count, buf_pg_offset,
        total_buf_len, sg_list);
    if (err < 0) {
        dnvme_err("Generation of sg lists failed");
        goto error_unmap;
    }

    /* Mapping SG List to DMA; NOTE: The sg list could be coalesced by either
     * an IOMMU or the kernel, so checking whether or not the number of
     * mapped entries equate to the number given to the func is not warranted */
    num_sg_entries = dma_map_sg(&nvme_dev->priv.pdev->dev, *sg_list,
        buf_pg_count, kernel_dir);
    dnvme_vdbg("%d elements mapped out of %d sglist elements",
        buf_pg_count, num_sg_entries);
    if (num_sg_entries == 0) {
        dnvme_err("Unable to map the sg list into dma addr space");
        err = -ENOMEM;
        goto error_unmap;
    }
    kfree(pages);

#ifdef TEST_PRP_DEBUG
    {
        struct scatterlist *work;
        dnvme_vdbg("Dump sg list after DMA mapping");
        for (i = 0, work = *sg_list; i < num_sg_entries; i++,
            work = sg_next(work)) {

            dnvme_vdbg("  sg list: page_link=0x%016lx", work->page_link);
            dnvme_vdbg("  sg list: offset=0x%08x, length=0x%08x",
                work->offset, work->length);
            dnvme_vdbg("  sg list: dmaAddr=0x%016llx, dmaLen=0x%08x",
                (u64)work->dma_address, work->dma_length);
        }
    }
#endif

    /* Fill in nvme_prps */
    prps->sg = *sg_list;
    prps->num_map_pgs = num_sg_entries;
    prps->buf = buf;
    prps->data_dir = kernel_dir;
    prps->data_buf_addr = buf_addr;
    prps->data_buf_size = total_buf_len;
    return 0;

error_unmap:
    vunmap(buf);
error:
    for (i = 0; i < buf_pg_count; i++) {
        put_page(pages[i]);
    }
    kfree(pages);
    return err;
}

static int pages_to_sg(struct page **pages, int num_pages, int buf_offset,
    unsigned len, struct scatterlist **sg_list)
{
    int i;
    struct scatterlist *sg;

    *sg_list = NULL;
    sg = kmalloc((num_pages * sizeof(struct scatterlist)), GFP_KERNEL);
    if (sg == NULL) {
        dnvme_err("Memory alloc for sg list failed");
        return -ENOMEM;
    }

    /* Building the SG List */
    sg_init_table(sg, num_pages);
    for (i = 0; i < num_pages; i++) {
        if (pages[i] == NULL) {
            kfree(sg);
            return -EFAULT;
        }
        sg_set_page(&sg[i], pages[i],
            min_t(int, len, (PAGE_SIZE - buf_offset)), buf_offset);
        len -= (PAGE_SIZE - buf_offset);
        buf_offset = 0;
    }
    sg_mark_end(&sg[i - 1]);
    *sg_list = sg;
    return 0;
}

/*
 * setup_prps:
 * Sets up PRP'sfrom DMA'ed memory
 * Returns Error codes
 */
static int setup_prps(struct nvme_device *nvme_dev, struct scatterlist *sg,
    s32 buf_len, struct nvme_prps *prps, u8 cr_io_q,
    enum nvme_64b_cmd_mask prp_mask)
{
    dma_addr_t prp_dma, dma_addr;
    s32 dma_len; /* Length of DMA'ed SG */
    __le64 *prp_list; /* Pointer to PRP List */
    u32 offset;
    u32 num_prps, num_pg, prp_page = 0;
    int index, err;
    struct dma_pool *prp_page_pool;

    dma_addr = sg_dma_address(sg);
    dma_len = sg_dma_len(sg);
    offset = offset_in_page(dma_addr);

    /* Create IO CQ/SQ's */
    if (cr_io_q) {
        /* Checking for PRP1 mask */
        if (!(prp_mask & NVME_MASK_PRP1_LIST)) {
            dnvme_err("bit_mask does not support PRP1 list");
            return -EINVAL;
        }
        /* Specifies PRP1 entry is a PRP_List */
        prps->type = (PRP1 | PRP_List);
        goto prp_list;
    }

    dnvme_vdbg("PRP1 Entry: Buf_len %d", buf_len);
    dnvme_vdbg("PRP1 Entry: dma_len %u", dma_len);
    dnvme_vdbg("PRP1 Entry: PRP entry %llx", (unsigned long long) dma_addr);

    /* Checking for PRP1 mask */
    if (!(prp_mask & NVME_MASK_PRP1_PAGE)) {
        dnvme_err("bit_mask does not support PRP1 page");
        return -EINVAL;
    }
    prps->prp1 = cpu_to_le64(dma_addr);
    buf_len -= (PAGE_SIZE - offset);
    dma_len -= (PAGE_SIZE - offset);

    if (buf_len <= 0) {
        prps->type = PRP1;
        return 0;
    }

    /* If pages were contiguous in memory use same SG Entry */
    if (dma_len) {
        dma_addr += (PAGE_SIZE - offset);
    } else {
        sg = sg_next(sg);
        dma_addr = sg_dma_address(sg);
        dma_len = sg_dma_len(sg);
    }

    offset = 0;

    if (buf_len <= PAGE_SIZE) {
        /* Checking for PRP2 mask */
        if (!(prp_mask & NVME_MASK_PRP2_PAGE)) {
            dnvme_err("bit_mask does not support PRP2 page");
            return -EINVAL;
        }
        prps->prp2 = cpu_to_le64(dma_addr);
        prps->type = (PRP1 | PRP2);
        dnvme_vdbg("PRP2 Entry: Type %u", prps->type);
        dnvme_vdbg("PRP2 Entry: Buf_len %d", buf_len);
        dnvme_vdbg("PRP2 Entry: dma_len %u", dma_len);
        dnvme_vdbg("PRP2 Entry: PRP entry %llx", (unsigned long long) dma_addr);
        return 0;
    }

    /* Specifies PRP2 entry is a PRP_List */
    prps->type = (PRP2 | PRP_List);
    /* Checking for PRP2 mask */
    if (!(prp_mask & NVME_MASK_PRP2_LIST)) {
        dnvme_err("bit_mask does not support PRP2 list");
        return -EINVAL;
    }

prp_list:
    /* Generate PRP List */
    num_prps = DIV_ROUND_UP(offset + buf_len, PAGE_SIZE);
    /* Taking into account the last entry of PRP Page */
    num_pg = DIV_ROUND_UP(NVME_PRP_ENTRY_SIZE * num_prps, PAGE_SIZE - NVME_PRP_ENTRY_SIZE);

    prps->vir_prp_list = kmalloc(sizeof(__le64 *) * num_pg, GFP_ATOMIC);
    if (NULL == prps->vir_prp_list) {
        dnvme_err("Memory allocation for virtual list failed");
        return -ENOMEM;
    }

    dnvme_vdbg("No. of PRP Entries inside PRPList: %u", num_prps);

    prp_page = 0;
    prp_page_pool = nvme_dev->priv.prp_page_pool;

    prp_list = dma_pool_alloc(prp_page_pool, GFP_ATOMIC, &prp_dma);
    if (NULL == prp_list) {
        kfree(prps->vir_prp_list);
        dnvme_err("Memory allocation for prp page failed");
        return -ENOMEM;
    }
    prps->vir_prp_list[prp_page++] = prp_list;
    prps->npages = prp_page;
    prps->first_dma = prp_dma;
    if (prps->type == (PRP2 | PRP_List)) {
        prps->prp2 = cpu_to_le64(prp_dma);
        dnvme_vdbg("PRP2 Entry: %llx", (unsigned long long) prps->prp2);
    } else if (prps->type == (PRP1 | PRP_List)) {
        prps->prp1 = cpu_to_le64(prp_dma);
        prps->prp2 = 0;
        dnvme_vdbg("PRP1 Entry: %llx", (unsigned long long) prps->prp1);
    } else {
        dnvme_err("PRP cmd options don't allow proper description of buffer");
        err = -EFAULT;
        goto error;
    }

    index = 0;
    for (;;) {
        if ((index == PAGE_SIZE / NVME_PRP_ENTRY_SIZE - 1) && (buf_len > PAGE_SIZE)) {
            __le64 *old_prp_list = prp_list;
            prp_list = dma_pool_alloc(prp_page_pool, GFP_ATOMIC, &prp_dma);
            if (NULL == prp_list) {
                dnvme_err("Memory allocation for prp page failed");
                err = -ENOMEM;
                goto error;
            }
            prps->vir_prp_list[prp_page++] = prp_list;
            prps->npages = prp_page;
            old_prp_list[index] = cpu_to_le64(prp_dma);
            index = 0;
        }

        dnvme_vdbg("PRP List: dma_len %d", dma_len);
        dnvme_vdbg("PRP List: Buf_len %d", buf_len);
        dnvme_vdbg("PRP List: offset %d", offset);
        dnvme_vdbg("PRP List: PRP entry %llx", (unsigned long long)dma_addr);
		//iol_interact-12.0a srart
        // inject offset if flag is set and used for the 2nd entry
        //if ((prp_mask & NVME_MASK_PRP_ADDR_OFFSET_ERR) && (index == 1)){
        //	prp_list[index++] = cpu_to_le64(dma_addr | 0x200);
        //	dnvme_err("PRP offset injected to entry 1");
        //} else {
        	prp_list[index++] = cpu_to_le64(dma_addr);//old_dnvme_driver
        //}
		//iol_interact-12.0a end
        dma_len  -= (PAGE_SIZE - offset);
        dma_addr += (PAGE_SIZE - offset);
        buf_len  -= (PAGE_SIZE - offset);
        offset = 0;

        if (buf_len <= 0) {
            break;
        } else if (dma_len > 0) {
            continue;
        } else if (dma_len < 0) {
            dnvme_err("DMA data length is illegal");
            err = -EFAULT;
            goto error;
        } else {
            sg = sg_next(sg);
            dma_addr = sg_dma_address(sg);
            dma_len = sg_dma_len(sg);
        }
    }
    return 0;

error:
    dnvme_err("Error in setup_prps function: %d", err);
    free_prp_pool(nvme_dev, prps, prp_page);
    return err;
}


/*
 * unmap_user_pg_to_dma:
 * Unmaps mapped DMA pages and frees the pinned down pages
 */
static void unmap_user_pg_to_dma(struct nvme_device *nvme_dev,
    struct nvme_prps *prps)
{
    int i;
    struct page *pg;

    if (!prps) {
        return;
    }

    /* Unammping Kernel Virtual Address */
    if (prps->buf && prps->type != NO_PRP) {
        vunmap(prps->buf);
    }

    if (prps->type != NO_PRP) {
        dma_unmap_sg(&nvme_dev->priv.pdev->dev, prps->sg,
            prps->num_map_pgs, prps->data_dir);

        for (i = 0; i < prps->num_map_pgs; i++) {
            pg = sg_page(&prps->sg[i]);
            if ((prps->data_dir == DMA_FROM_DEVICE) ||
                (prps->data_dir == DMA_BIDIRECTIONAL)) {

                set_page_dirty_lock(pg);
            }
            put_page(pg);
        }
        kfree(prps->sg);
    }
}


/*
 * free_prp_pool:
 * Free's PRP List and virtual List
 */
static void free_prp_pool(struct nvme_device *nvme_dev,
    struct nvme_prps *prps, u32 npages)
{
    int i;
    __le64 *prp_vlist;
    const int last_prp = ((PAGE_SIZE / NVME_PRP_ENTRY_SIZE) - 1);
    dma_addr_t prp_dma, next_prp_dma = 0;


    if (prps == NULL) {
        return;
    }

    if (prps->type == (PRP1 | PRP_List) || prps->type == (PRP2 | PRP_List)) {

        prp_dma = prps->first_dma;
        for (i = 0; i < npages; i++) {

            prp_vlist = prps->vir_prp_list[i];
            if (i < (npages - 1)) {
                next_prp_dma = le64_to_cpu(prp_vlist[last_prp]);
            }
            dma_pool_free(
                nvme_dev->priv.prp_page_pool, prp_vlist, prp_dma);
            prp_dma = next_prp_dma;
        }
        kfree(prps->vir_prp_list);
    }
}
