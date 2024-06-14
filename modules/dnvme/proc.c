/**
 * @file proc.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-03-22
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#define DEBUG

#include <linux/version.h>

#include "trace.h"
#include "proc.h"
#include "pci.h"
#include "io.h"
#include "queue.h"

#define CMD_SEG_MAX		8

static void dnvme_dump_data(void *buf, u32 size, u32 flag)
{
	u32 oft = 0;
	u8 string[256];
	u8 *str = string;
	u8 *ptr = buf;
	int ret;

	while (size >= 0x10) {
		snprintf(str, sizeof(string), 
			"%x: %02x %02x %02x %02x %02x %02x %02x %02x "
			"%02x %02x %02x %02x %02x %02x %02x %02x", oft, 
			ptr[oft], ptr[oft + 1], ptr[oft + 2], ptr[oft + 3],
			ptr[oft + 4], ptr[oft + 5], ptr[oft + 6], ptr[oft + 7],
			ptr[oft + 8], ptr[oft + 9], ptr[oft + 10], ptr[oft + 11],
			ptr[oft + 12], ptr[oft + 13], ptr[oft + 14], ptr[oft + 15]);
		pr_info("%s\n", str);
		size -= 0x10;
		oft += 0x10;
	}

	if (!size)
		return;

	ret = snprintf(str, sizeof(string), "%x:", oft);

	if (size >= 8) {
		ret += snprintf(str + ret, sizeof(string) - ret, 
			" %02x %02x %02x %02x %02x %02x %02x %02x", 
			ptr[oft], ptr[oft + 1], ptr[oft + 2], ptr[oft + 3],
			ptr[oft + 4], ptr[oft + 5], ptr[oft + 6], ptr[oft + 7]);
		size -= 8;
		oft += 8;
	}

	if (size >= 4) {
		ret += snprintf(str + ret, sizeof(string) - ret, 
			" %02x %02x %02x %02x", 
			ptr[oft], ptr[oft + 1], ptr[oft + 2], ptr[oft + 3]);
		size -= 4;
		oft += 4;
	}

	if (size >= 2) {
		ret += snprintf(str + ret, sizeof(string) - ret, 
			" %02x %02x", ptr[oft], ptr[oft + 1]);
		size -= 2;
		oft += 2;
	}

	if (size >= 1) {
		ret += snprintf(str + ret, sizeof(string) - ret, 
			" %02x", ptr[oft]);
		size -= 1;
		oft += 1;
	}

	pr_info("%s\n", str);
}

static void print_cfg_data(struct nvme_device *ndev)
{
	void *buf;
	int ret;

	buf = kmalloc(PCI_CFG_SPACE_EXP_SIZE, GFP_KERNEL);
	if (!buf) {
		dnvme_err(ndev, "failed to alloc buf!\n");
		return;
	}

	ret = pci_read_cfg_data(ndev->pdev, buf, PCI_CFG_SPACE_EXP_SIZE);
	if (ret < 0)
		dnvme_warn(ndev, "something read is wrong?(%d)\n", ret);

	dnvme_info(ndev, "~~~~~ Configuration Space Data ~~~~~\n");
	dnvme_dump_data(buf, PCI_CFG_SPACE_EXP_SIZE, 0);

	kfree(buf);
}

static int is_align(u32 offset, u32 len)
{
	if (len >= 4) {
		if (!(offset % 4))
			return 1;
	} else if (len >= 2) {
		if (!(offset % 2))
			return 1;
	} else if (len == 1) {
		return 1;
	}

	return 0;
}

static int cmd_dump_metadata(struct nvme_device *ndev, char *argv[], int argc)
{
	struct nvme_meta *meta;
	u32 id;
	int ret;

	if (argc < 1)
		return -EINVAL;

	ret = kstrtouint(argv[0], 0, &id);
	if (ret < 0) {
		dnvme_err(ndev, "arg:%s is invalid!\n", argv[0]);
		return -EINVAL;
	}

	meta = dnvme_find_meta(ndev, id);
	if (!meta) {
		dnvme_err(ndev, "failed to find meta(0x%x) node!\n", id);
		return -EFAULT;
	}

	if (meta->contig) {
		dnvme_info(ndev, "meta data (ID:0x%x, DMA addr:0x%llx):\n", id, meta->dma);
		dnvme_dump_data(meta->buf, meta->size, 0);
		return 0;
	}

	dnvme_warn(ndev, "Not support to dump SGL meta data!\n");
	return 0;
}

static int cmd_dump_queue(struct nvme_device *ndev)
{
	struct nvme_sq *sq;
	struct nvme_cq *cq;
	unsigned long i;

	xa_for_each(&ndev->sqs, i, sq) {
		if (sq->contig) {
			dnvme_info(ndev, "SQ%u bind CQ%u, addr:0x%16llx, size:0x%8x\n", 
				sq->pub.sq_id, sq->pub.cq_id, sq->dma, sq->size);
		} else {
			dnvme_warn(ndev, "SQ%u is discontigous!\n", sq->pub.sq_id);
		}
	}

	xa_for_each(&ndev->cqs, i, cq) {
		if (cq->contig) {
			dnvme_info(ndev, "CQ%u addr:0x%16llx, size:0x%8x\n",
				cq->pub.q_id, cq->dma, cq->size);
		} else {
			dnvme_warn(ndev, "CQ%u is discontigous!\n", cq->pub.q_id);
		}
	}
	return 0;
}

static int cmd_dump(struct nvme_device *ndev, char *argv[], int argc)
{
	if (argc < 1)
		return -EINVAL;

	if (!strncmp(argv[0], "cfg", strlen("cfg")))
		print_cfg_data(ndev);
	else if (!strncmp(argv[0], "meta", strlen("meta")))
		cmd_dump_metadata(ndev, &argv[1], argc - 1);
	else if (!strncmp(argv[0], "queue", strlen("queue")))
		cmd_dump_queue(ndev);

	return 0;
}

static int cmd_read_bar(struct nvme_device *ndev, char *argv[], int argc)
{
	void __iomem *ptr = ndev->bar[0];
	u8 bar = 0;
	u32 offset = 0;
	u32 len = 0;
	int ret;

	if (argc < 1)
		return -EINVAL;

	switch (argc) {
	default:
		dnvme_warn(ndev, "The number of args are out of limit!\n");
		fallthrough;
	case 3:
		ret = kstrtouint(argv[2], 0, &len);
		if (ret < 0) {
			dnvme_err(ndev, "arg:%s is invalid!\n", argv[0]);
			return -EINVAL;
		}
		fallthrough;
	case 2:
		ret = kstrtouint(argv[1], 0, &offset);
		if (ret < 0) {
			dnvme_err(ndev, "arg:%s is invalid!\n", argv[0]);
			return -EINVAL;
		}
		fallthrough;
	case 1:
		ret = kstrtou8(argv[0], 0, &bar);
		if (ret < 0) {
			dnvme_err(ndev, "arg:%s is invalid!\n", argv[0]);
			return -EINVAL;
		}
		break;
	}

	if (!len)
		len = (offset % 4) ? ((offset % 2) ? 1 : 2) : 4;

	if (!is_align(offset, len)) {
		dnvme_err(ndev, "offset:0x%x or len:0x%x is not align!\n",
			offset, len);
		return -EINVAL;
	}

	if (bar > 0) {
		dnvme_err(ndev, "Not support bar%u!\n", bar);
		return -EINVAL;
	}

	while (len > 0) {
		if (len >= 4) {
			dnvme_dbg(ndev, "0x%08x: 0x%08x\n", offset, 
				dnvme_readl(ptr, offset));
			offset += 4;
			len -= 4;
		} else if (len >= 2) {
			dnvme_dbg(ndev, "0x%08x: 0x%04x\n", offset,
				dnvme_readw(ptr, offset));
			offset += 2;
			len -= 2;
		} else {
			dnvme_dbg(ndev, "0x%08x: 0x%02x\n", offset,
				dnvme_readb(ptr, offset));
			offset++;
			len--;
		}
	}
	return 0;
}

static int cmd_read(struct nvme_device *ndev, char *argv[], int argc)
{
	if (argc < 1)
		return -EINVAL;

	if (!strncmp(argv[0], "bar", strlen("bar")))
		cmd_read_bar(ndev, &argv[1], argc - 1);
	else
		dnvme_warn(ndev, "arg:%s is unknown!\n", argv[0]);

	return 0;
}

static int cmd_write(struct nvme_device *ndev, char *argv[], int argc)
{
	if (argc < 1)
		return -EINVAL;

	return 0;
}

static int execute_command(struct nvme_device *ndev, char *argv[], int argc)
{
	if (argc < 1)
		return -EINVAL;

	if (!strncmp(argv[0], "dump", strlen("dump"))) {
		cmd_dump(ndev, &argv[1], argc - 1);
	} else if (!strncmp(argv[0], "read", strlen("read"))) {
		cmd_read(ndev, &argv[1], argc - 1);
	} else if (!strncmp(argv[0], "write", strlen("write"))) {
		cmd_write(ndev, &argv[1], argc - 1);
	} else {
		dnvme_warn(ndev, "arg:%s is unknown!\n", argv[0]);
	}

	return 0;
}

static ssize_t dnvme_proc_read(struct file *file, char __user *buf, 
	size_t size, loff_t *fpos)
{
	struct inode *inode = file->f_path.dentry->d_inode;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
	struct nvme_device *ndev = pde_data(inode);
#else
	struct nvme_device *ndev = PDE_DATA(inode);
#endif
	struct device *dev = &ndev->dev;

	dnvme_info(ndev, "echo \"dump cfg\" > /proc/nvme/%s "
		":dump configuration space data\n", dev_name(dev));
	dnvme_info(ndev, "echo \"dump meta [meta_id]\" > /proc/nvme/%s "
		":dump meta data\n", dev_name(dev));
	dnvme_info(ndev, "echo \"dump queue\" > /proc/nvme/%s "
		":dump queue info in a concise manner", dev_name(dev));
	dnvme_info(ndev, "echo \"read bar [bar] [offset] [len]\" > /proc/nvme/%s "
		":read bar space data\n", dev_name(dev));

	return 0;
}

static ssize_t dnvme_proc_write(struct file *file, const char __user *buf,
	size_t size, loff_t *fpos)
{
	struct inode *inode = file->f_path.dentry->d_inode;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
	struct nvme_device *ndev = pde_data(inode);
#else
	struct nvme_device *ndev = PDE_DATA(inode);
#endif
	char *args[CMD_SEG_MAX] = {NULL};
	char *cmd;
	char *tmp;
	int i;

	cmd = kmalloc(size, GFP_KERNEL);
	if (!cmd) {
		dnvme_err(ndev, "failed to alloc for cmd!\n");
		return -ENOMEM;
	}

	if (copy_from_user(cmd, buf, size)) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		kfree(cmd);
		return -EFAULT;
	}
	trace_dnvme_proc_write(&ndev->dev, cmd);

	tmp = cmd;
	for (i = 0; i < CMD_SEG_MAX; i++) {
		args[i] = strsep(&tmp, " ");
		if (!args[i])
			break;
	}
	execute_command(ndev, args, i);

	kfree(cmd);
	return size;
}

static const struct proc_ops dnvme_proc_ops = {
	.proc_read	= dnvme_proc_read,
	.proc_write	= dnvme_proc_write,
};

int dnvme_create_proc_entry(struct nvme_device *ndev, 
	struct proc_dir_entry *parent)
{
	struct device *dev = &ndev->dev;
	struct proc_dir_entry *entry;

	if (!parent) {
		dnvme_err(ndev, "required to craete parent first!\n");
		return -EINVAL;
	}

	entry = proc_create_data(dev_name(dev), S_IRUSR | S_IWUSR, 
		parent, &dnvme_proc_ops, ndev);
	if (!entry) {
		dnvme_err(ndev, "failed to create proc entry!\n");
		return -EFAULT;
	}

	ndev->proc = entry;
	return 0;
}

void dnvme_destroy_proc_entry(struct nvme_device *ndev)
{
	proc_remove(ndev->proc);
	ndev->proc = NULL;
}
