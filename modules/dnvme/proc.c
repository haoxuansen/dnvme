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

#include "proc.h"
#include "pci.h"
#include "io.h"

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

static void print_bar_data(struct nvme_device *ndev)
{
	u32 id;
	u16 sts;

	id = dnvme_readl(ndev->bar0, 0);
	dnvme_info(ndev, "ID: 0x%08x\n", id);

	sts = dnvme_readl(ndev->bar0, 6);
	dnvme_info(ndev, "STS: 0x%04x\n", sts);
}

static ssize_t dnvme_proc_read(struct file *file, char __user *buf, 
	size_t size, loff_t *fpos)
{
	struct inode *inode = file->f_path.dentry->d_inode;
	struct nvme_device *ndev = PDE_DATA(inode);
	struct device *dev = &ndev->dev;

	dnvme_info(ndev, "echo \"dump cfg\" > /proc/nvme/%s "
		":dump configuration space data\n", dev_name(dev));
	dnvme_info(ndev, "echo \"dump bar\" > /proc/nvme/%s "
		":dump bar0 space data\n", dev_name(dev));

	return 0;
}

static ssize_t dnvme_proc_write(struct file *file, const char __user *buf,
	size_t size, loff_t *fpos)
{
	struct inode *inode = file->f_path.dentry->d_inode;
	struct nvme_device *ndev = PDE_DATA(inode);
	char *cmd;

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

	if (!strncmp(cmd, "dump cfg", strlen("dump cfg"))) {
		print_cfg_data(ndev);
	} else if (!strncmp(cmd, "dump bar", strlen("dump bar"))) {
		print_bar_data(ndev);
	} else if (!strncmp(cmd, "do pcie hot reset", strlen("do pcie hot reset"))) {
		pcie_do_hot_reset(ndev->pdev);
	} else if (!strncmp(cmd, "do pcie linkdown reset", strlen("do pcie linkdown reset"))) {
		pcie_do_linkdown_reset(ndev->pdev);
	} else {
		dnvme_warn(ndev, "cmd(%s) is unknown!\n", cmd);
	}

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
