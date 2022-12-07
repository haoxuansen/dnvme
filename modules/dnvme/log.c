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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include "core.h"

#define LOG_BUF_SIZE			1024

static char log_buf[LOG_BUF_SIZE];

static const char *log_indent_level(int level)
{
	switch (level) {
	case 0: return "";
	case 1: return "\t";
	case 2: return "\t\t";
	case 3: return "\t\t\t";
	case 4: return "\t\t\t\t";
	case 5: return "\t\t\t\t\t";
	case 6: return "\t\t\t\t\t\t";
	default:
		return "\t";
	}
}

static int log_prps(struct nvme_prps *prps, struct file *fp, 
	loff_t *pos, int indent)
{
	int oft;
	char *buf = log_buf;

	oft = snprintf(buf, LOG_BUF_SIZE, 
		"%s prps:\n", log_indent_level(indent));
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, 
		"%s nr_pages: %u\n", log_indent_level(indent + 1), prps->nr_pages);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft,
		"%s prp_list: 0x%lx\n", log_indent_level(indent + 1), 
		(unsigned long)prps->prp_list);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s dma: 0x%lx\n", 
		log_indent_level(indent + 1), (unsigned long)prps->pg_addr);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s buf: 0x%lx\n", 
		log_indent_level(indent + 1), (unsigned long)prps->buf);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft,
		"%s scatterlist: 0x%lx\n", log_indent_level(indent + 1), 
		(unsigned long)prps->sg);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft,
		"%s num_map_pgs: %u\n", log_indent_level(indent + 1), 
		prps->num_map_pgs);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft,
		"%s data_buf_size: %u\n", log_indent_level(indent + 1), 
		prps->data_buf_size);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft,
		"%s data_buf_addr: 0x%llx\n", log_indent_level(indent + 1), 
		prps->data_buf_addr);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft,
		"%s data_dir: %u\n", log_indent_level(indent + 1),
		prps->data_dir);

	__kernel_write(fp, buf, oft, pos);
	return 0;
}

static int log_cmd_node(struct nvme_cmd *node, int idx, 
	struct file *fp, loff_t *pos, int indent)
{
	int oft;
	char *buf = log_buf;

	oft = snprintf(buf, LOG_BUF_SIZE, "%s cmd_node[%d]:\n", 
		log_indent_level(indent), idx);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s sqid: %u\n", 
		log_indent_level(indent + 1), node->sqid);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s id: %u\n", 
		log_indent_level(indent + 1), node->id);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s opcode: %u\n", 
		log_indent_level(indent + 1), node->opcode);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s target_qid: %u\n", 
		log_indent_level(indent + 1), node->target_qid);

	__kernel_write(fp, buf, oft, pos);

	log_prps(&node->prps, fp, pos, indent + 1);

	return 0;
}

static int log_sq(struct nvme_sq *sq, int idx, struct file *fp, loff_t *pos,
	int indent)
{
	int oft, i;
	char *buf = log_buf;
	struct nvme_cmd *cmd_node;

	/* SQ public info */
	oft = snprintf(buf, LOG_BUF_SIZE, "%s sq->pub[%d]\n", 
		log_indent_level(indent), idx);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s sq_id: %u\n", 
		log_indent_level(indent + 1), sq->pub.sq_id);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s cq_id: %u\n", 
		log_indent_level(indent + 1), sq->pub.cq_id);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s elements: %u\n", 
		log_indent_level(indent + 1), sq->pub.elements);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s sqes: %u\n", 
		log_indent_level(indent + 1), sq->pub.sqes);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s head_ptr: %u\n", 
		log_indent_level(indent + 1), sq->pub.head_ptr);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s tail_ptr: %u\n", 
		log_indent_level(indent + 1), sq->pub.tail_ptr);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s tail_ptr_virt: %u\n", 
		log_indent_level(indent + 1), sq->pub.tail_ptr_virt);

	/* SQ private info */
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s sq->priv[%d]\n",
		log_indent_level(indent), idx);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s buf: 0x%lx\n", 
		log_indent_level(indent + 1), (unsigned long)sq->priv.buf);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s dma: 0x%llx\n", 
		log_indent_level(indent + 1), sq->priv.dma);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s size: 0x%x(%u)\n", 
		log_indent_level(indent + 1), sq->priv.size, sq->priv.size);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s dbs: 0x%lx\n", 
		log_indent_level(indent + 1), (unsigned long)sq->priv.dbs);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s unique_cmd_id: %u\n", 
		log_indent_level(indent + 1), sq->priv.unique_cmd_id);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s contig: %u\n", 
		log_indent_level(indent + 1), sq->priv.contig);
	__kernel_write(fp, buf, oft, pos);

	log_prps(&sq->priv.prps, fp, pos, indent + 1);

	/* SQ cmd list */
	oft = snprintf(buf, LOG_BUF_SIZE, "%s cmd_list:\n", 
		log_indent_level(indent + 1));
	__kernel_write(fp, buf, oft, pos);

	i = 0;
	list_for_each_entry(cmd_node, &(sq->priv.cmd_list), entry) {
		log_cmd_node(cmd_node, i, fp, pos, indent + 2);
		i++;
	}

	return 0;
}

static int log_cq(struct nvme_cq *cq, int idx, struct file *fp, loff_t *pos,
	int indent)
{
	int oft;
	char *buf = log_buf;

	/* CQ public info */
	oft = snprintf(buf, LOG_BUF_SIZE, "%s cq->pub[%d]\n", 
		log_indent_level(indent), idx);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s cq_id: %u\n", 
		log_indent_level(indent + 1), cq->pub.q_id);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s tail_ptr: %u\n", 
		log_indent_level(indent + 1), cq->pub.tail_ptr);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s head_ptr: %u\n", 
		log_indent_level(indent + 1), cq->pub.head_ptr);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s elements: %u\n", 
		log_indent_level(indent + 1), cq->pub.elements);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s irq_enabled: %u\n", 
		log_indent_level(indent + 1), cq->pub.irq_enabled);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s irq_no: %u\n", 
		log_indent_level(indent + 1), cq->pub.irq_no);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s pbit_new_entry: %u\n", 
		log_indent_level(indent + 1), cq->pub.pbit_new_entry);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s cqes: %u\n", 
		log_indent_level(indent + 1), cq->pub.cqes);

	/* CQ private info */
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s cq->priv[%d]\n", 
		log_indent_level(indent), idx);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s buf: 0x%lx\n", 
		log_indent_level(indent + 1), (unsigned long)cq->priv.buf);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s dma: 0x%llx\n", 
		log_indent_level(indent + 1), cq->priv.dma);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s size: 0x%x\n", 
		log_indent_level(indent + 1), cq->priv.size);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s dbs: 0x%lx\n", 
		log_indent_level(indent + 1), (unsigned long)cq->priv.dbs);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s contig: %u\n", 
		log_indent_level(indent + 1), cq->priv.contig);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s bit_mask: 0x%x\n", 
		log_indent_level(indent + 1), cq->priv.bit_mask);
	__kernel_write(fp, buf, oft, pos);

	log_prps(&cq->priv.prps, fp, pos, indent + 1);
	return 0;
}

static int log_meta_node(struct nvme_meta *meta, int idx, struct file *fp, 
	loff_t *pos, int indent)
{
	int oft;
	char *buf = log_buf;

	oft = snprintf(buf, LOG_BUF_SIZE, "%s meta_node[%d]:\n",
		log_indent_level(indent), idx);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s id: %u\n", 
		log_indent_level(indent + 1), meta->id);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s dma: 0x%llx\n", 
		log_indent_level(indent + 1), meta->dma);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s buf: 0x%lx\n", 
		log_indent_level(indent + 1), (unsigned long)meta->buf);

	__kernel_write(fp, buf, oft, pos);
	return 0;
}

static int log_meta_list(struct nvme_context *ctx, struct file *fp,
	loff_t *pos, int indent)
{
	char *buf = log_buf;
	struct nvme_meta *meta;
	int i, oft;

	if (!ctx->meta_set.pool)
		return 0;

	oft = snprintf(buf, LOG_BUF_SIZE, "%s ctx->meta_set.pool: 0x%lx\n", 
		log_indent_level(indent), (unsigned long)ctx->meta_set.pool);
	__kernel_write(fp, buf, oft, pos);

	i = 0;
	list_for_each_entry(meta, &ctx->meta_set.meta_list, entry) {
		log_meta_node(meta, i, fp, pos, indent + 1);
		i++;
	}
	return 0;
}

static int log_irq_sub_node(struct nvme_icq *node, int idx, struct file *fp,
	loff_t *pos, int indent)
{
	char *buf = log_buf;
	int oft;

	oft = snprintf(buf, LOG_BUF_SIZE, "%s irq_sub_node[%d]:\n",
		log_indent_level(indent), idx);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s cq_id: %u\n", 
		log_indent_level(indent + 1), node->cq_id);
	__kernel_write(fp, buf, oft, pos);
	return 0;
}

static int log_irq_node(struct nvme_irq *irq, int idx, struct file *fp, 
	loff_t *pos, int indent)
{
	struct nvme_icq *node;
	char *buf = log_buf;
	int oft, i;

	oft = snprintf(buf, LOG_BUF_SIZE, "%s irq_node[%d]\n", 
		log_indent_level(indent), idx);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s irq_id: %u\n", 
		log_indent_level(indent + 1), irq->irq_id);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s int_vec: %u\n", 
		log_indent_level(indent + 1), irq->int_vec);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s isr_fired: %u\n", 
		log_indent_level(indent + 1), irq->isr_fired);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s isr_count: %u\n", 
		log_indent_level(indent + 1), irq->isr_count);
	__kernel_write(fp, buf, oft, pos);

	i = 0;
	list_for_each_entry(node, &irq->icq_list, entry) {
		log_irq_sub_node(node, i, fp, pos, indent + 1);
		i++;
	}
	return 0;
}

static int log_irq_work_node(struct nvme_work *node, int idx,
	struct file *fp, loff_t *pos, int indent)
{
	char *buf = log_buf;
	int oft;

	oft = snprintf(buf, LOG_BUF_SIZE, "%s work_node[%d]\n",
		log_indent_level(indent), idx);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s irq_id: %u\n", 
		log_indent_level(indent + 1), node->irq_id);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s int_vec: %u\n", 
		log_indent_level(indent + 1), node->int_vec);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s irq_set: 0x%lx\n", 
		log_indent_level(indent + 1), (unsigned long)node->irq_set);
	__kernel_write(fp, buf, oft, pos);
	return 0;
}

static int log_irq_process(struct nvme_context *ctx, struct file *fp, 
	loff_t *pos, int indent)
{
	struct nvme_irq *irq_node;
	struct nvme_irq_set *irq_proc = &ctx->irq_set;
	struct nvme_work *work_node;  /* Current wk item in the list */
	char *buf = log_buf;
	int oft, i;

	mutex_lock(&irq_proc->mtx_lock);

	oft = snprintf(buf, LOG_BUF_SIZE, "%s irq_set->mask_ptr: 0x%lx\n", 
		log_indent_level(indent), (unsigned long)irq_proc->mask_ptr);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, 
		"%s irq_set->irq_type: %u\n", 
		log_indent_level(indent), irq_proc->irq_type);
	__kernel_write(fp, buf, oft, pos);

	i = 0;
	list_for_each_entry(irq_node, &irq_proc->irq_list, irq_entry) {
		log_irq_node(irq_node, i, fp, pos, indent + 1);
		i++;
	}

	i = 0;
	list_for_each_entry(work_node, &irq_proc->work_list, entry) {
		log_irq_work_node(work_node, i, fp, pos, indent + 1);
		i++;
	}

	mutex_unlock(&irq_proc->mtx_lock);
	return 0;
}

static int log_context(struct nvme_context *ctx, int idx, struct file *fp,
	loff_t *pos, int indent)
{
	char *buf = log_buf;
	int oft, i;
	struct nvme_device *ndev = ctx->dev;
	struct nvme_sq *sq;
	struct nvme_cq *cq;

	/* NVMe device private info */
	oft = snprintf(buf, LOG_BUF_SIZE, "%s ndev->pub[%d]:\n", 
		log_indent_level(indent), idx);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s minor: %d\n", 
		log_indent_level(indent + 1), ndev->priv.minor);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s opened: %u\n", 
		log_indent_level(indent + 1), ndev->priv.opened);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s pdev: 0x%lx\n", 
		log_indent_level(indent + 1), (unsigned long)ndev->priv.pdev);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s bar0: 0x%lx\n", 
		log_indent_level(indent + 1), (unsigned long)ndev->priv.bar0);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s bar1: 0x%lx\n", 
		log_indent_level(indent + 1), (unsigned long)ndev->priv.bar1);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s bar2: 0x%lx\n", 
		log_indent_level(indent + 1), (unsigned long)ndev->priv.bar2);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s dbs: 0x%lx\n", 
		log_indent_level(indent + 1), (unsigned long)ndev->priv.dbs);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s prp_page_pool: 0x%lx\n", 
		log_indent_level(indent + 1), (unsigned long)ndev->priv.prp_page_pool);

	/* NVMe device public info */
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s ndev->priv[%d]:\n", 
		log_indent_level(indent), idx);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s irq_type: %d\n", 
		log_indent_level(indent + 1), ndev->pub.irq_active.irq_type);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s (S:%u/M:%u/X:%u/P:%u)\n", 
		log_indent_level(indent + 2), NVME_INT_MSI_SINGLE, NVME_INT_MSI_MULTI,
		NVME_INT_MSIX, NVME_INT_PIN);
	oft += snprintf(buf + oft, LOG_BUF_SIZE - oft, "%s num_irqs: %u\n", 
		log_indent_level(indent + 1), ndev->pub.irq_active.num_irqs);

	__kernel_write(fp, buf, oft, pos);

	i = 0;
	list_for_each_entry(sq, &ctx->sq_list, sq_entry) {
		log_sq(sq, i, fp, pos, indent + 1);
		i++;
	}

	i = 0;
	list_for_each_entry(cq, &ctx->cq_list, cq_entry) {
		log_cq(cq, i, fp, pos, indent + 1);
		i++;
	}

	log_meta_list(ctx, fp, pos, indent);
	log_irq_process(ctx, fp, pos, indent);
	return 0;
}

int dnvme_dump_log_file(struct nvme_log_file __user *ulog_file)
{
	struct nvme_log_file log_file;
	struct nvme_context *ctx;
	struct file *fp;
	loff_t pos = 0;
	mm_segment_t fs; /* file segment to map between Kernel and user */
	char *name;
	int ret = 0;
	int i;

	if (copy_from_user(&log_file, ulog_file, sizeof(log_file))) {
		dnvme_err("failed to copy from user space!\n");
		return -EFAULT;
	}

	/* Alloc memory for the data in kernel space, add 1 for a NULL term */
	name = kzalloc(log_file.len + 1, GFP_KERNEL);
	if (!name) {
		dnvme_err("failed to alloc for file name!\n");
		return -ENOMEM;
	}

	if (copy_from_user(name, log_file.name, log_file.len)) {
		dnvme_err("failed to copy from user space!\n");
		ret = -EFAULT;
		goto out;
	}

	/* If the user didn't provide a NULL term, we will to avoid problems */
	name[log_file.len] = '\0';

	dnvme_vdbg("dump log file to %s!\n", name);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	fs = force_uaccess_begin();
#else
	fs = get_fs();
	set_fs(KERNEL_DS);
#endif
	fp = filp_open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (!fp) {
		dnvme_err("failed to create log file(%s)!\n", name);
		ret = -EPERM;
		goto out2;
	}

	i = 0;
	list_for_each_entry(ctx, &nvme_ctx_list, entry) {
		log_context(ctx, i, fp, &pos, 0);
		i++;
	}

	fput(fp);
	filp_close(fp, NULL);
out2:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
        force_uaccess_end(fs);
#else
        set_fs(fs);
#endif	
out:
	kfree(name);
	return ret;
}

