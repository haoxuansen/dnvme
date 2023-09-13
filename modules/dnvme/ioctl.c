/**
 * @file ioctl.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-04-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include "pci_ids_ext.h"
#include "dnvme.h"
#include "io.h"
#include "core.h"
#include "pci.h"
#include "queue.h"
#include "irq.h"

static int dnvme_wait_ready(struct nvme_device *ndev, bool enabled)
{
	u64 cap;
	unsigned long timeout;
	void __iomem *bar0 = ndev->bar[0];
	u32 bit = enabled ? NVME_CSTS_RDY : 0;
	u32 csts;

	cap = dnvme_readq(bar0, NVME_REG_CAP);
	timeout = ((NVME_CAP_TIMEOUT(cap) + 1) * HZ / 2) + jiffies;

	while (1) {
		csts = dnvme_readl(bar0, NVME_REG_CSTS);
		if (csts == ~0) {
			dnvme_err(ndev, "csts = 0x%x, dev not exist?\n", csts);
			return -ENODEV;
		}
		if ((csts & NVME_CSTS_RDY) == bit)
			break;
		
		usleep_range(1000, 2000);

		if (time_after(jiffies, timeout)) {
			dnvme_err(ndev, "Device not ready; aborting %s, CSTS=0x%x\n",
				enabled ? "init" : "reset", csts);
			return -ETIME;
		}
	}

	return 0;
}

/**
 * @brief Set controller state
 * 
 * @param ctx NVMe context
 * @return 0 on success, otherwise a negative errno.
 */
static int dnvme_set_ctrl_state(struct nvme_device *ndev, bool enabled)
{
	void __iomem *bar0 = ndev->bar[0];
	u32 cc;

	cc = dnvme_readl(bar0, NVME_REG_CC);
	if (enabled) {
		cc &= ~(NVME_CC_IOSQES_MASK | NVME_CC_IOCQES_MASK);
		cc |= NVME_CC_FOR_IOSQES(NVME_NVM_IOSQES);
		cc |= NVME_CC_FOR_IOCQES(NVME_NVM_IOCQES);
		cc |= NVME_CC_ENABLE;
	} else {
		cc &= ~NVME_CC_ENABLE;
	}
	dnvme_writel(bar0, NVME_REG_CC, cc);

	return dnvme_wait_ready(ndev, enabled);
}

static int dnvme_reset_subsystem(struct nvme_device *ndev)
{
	struct pci_dev *pdev = ndev->pdev;
	void __iomem *bar0 = ndev->bar[0];
	u32 rstval = 0x4e564d65; /* "NVMe" */

	dnvme_writel(bar0, NVME_REG_NSSR, rstval);

	if (pdev->vendor == PCI_VENDOR_ID_MAXIO && 
			pdev->device == PCI_DEVICE_ID_FALCON_LITE)
		return dnvme_wait_ready(ndev, false);

	return 0;
}

int dnvme_set_device_state(struct nvme_device *ndev, enum nvme_state state)
{
	int ret;

	switch (state) {
	case NVME_ST_ENABLE:
		ret =  dnvme_set_ctrl_state(ndev, true);
		break;

	case NVME_ST_DISABLE:
	case NVME_ST_DISABLE_COMPLETE:
		ret = dnvme_set_ctrl_state(ndev, false);
		if (ret < 0) {
			dnvme_err(ndev, "failed to set ctrl state:%d!(%d)\n", 
				state, ret);
		} else {
			dnvme_cleanup_device(ndev, state);
		}
		break;

	case NVME_ST_SUBSYSTEM_RESET:
		ret = dnvme_reset_subsystem(ndev);
		/* !NOTICE: It's necessary to clean device here? */
		break;

	case NVME_ST_PCIE_FLR_RESET:
		ret = pcie_do_flr_reset(ndev->pdev);
		break;

	case NVME_ST_PCIE_HOT_RESET:
		ret = pcie_do_hot_reset(ndev->pdev);
		break;

	case NVME_ST_PCIE_LINKDOWN_RESET:
		ret = pcie_do_linkdown_reset(ndev->pdev);
		break;

	default:
		dnvme_err(ndev, "nvme state(%d) is unkonw!\n", state);
		ret = -EINVAL;
	}

	return ret;
}

/**
 * @brief Check access offset and size is align with access type.
 * 
 * @return 0 if check success, otherwise a negative errno.
 */
static int dnvme_check_access_align(struct nvme_device *ndev, 
	struct nvme_access *access)
{
	switch (access->type) {
	case NVME_ACCESS_QWORD:
		if ((access->bytes % 8) != 0 || (access->offset % 4) != 0) {
			dnvme_err(ndev, "offset(%u) shall dword-align, "
				"bytes(%u) shall qword-align!\n", 
				access->offset, access->bytes);
			return -EINVAL;
		}
		break;

	case NVME_ACCESS_DWORD:
		if ((access->bytes % 4) != 0 || (access->offset % 4) != 0) {
			dnvme_err(ndev, "offset(%u) or bytes(%u) shall dword-align!\n",
				access->offset, access->bytes);
			return -EINVAL;
		}
		break;
	
	case NVME_ACCESS_WORD:
		if ((access->bytes % 2) != 0 || (access->offset % 2) != 0) {
			dnvme_err(ndev, "offset(%u) or bytes(%u) shall word-align!\n",
				access->offset, access->bytes);
			return -EINVAL;
		}
		break;
	
	case NVME_ACCESS_BYTE:
	default:
		break;
	}

	return 0;
}

/**
 * @brief Read data from NVMe device
 * 
 * @param ctx NVMe device context
 * @return 0 on success, otherwise a negative errno.
 */
int dnvme_generic_read(struct nvme_device *ndev, struct nvme_access __user *uaccess)
{
	int ret = 0;
	void *buf;
	struct nvme_access access;
	struct pci_dev *pdev = ndev->pdev;

	if (copy_from_user(&access, uaccess, sizeof(struct nvme_access))) {
		dnvme_err(ndev, "Failed to copy from user space!\n");
		return -EFAULT;
	}

	if (!access.bytes) {
		dnvme_dbg(ndev, "Access size is zero!\n");
		return 0;
	}

	ret = dnvme_check_access_align(ndev, &access);
	if (ret < 0)
		return ret;

	buf = kzalloc(access.bytes, GFP_KERNEL);
	if (!buf) {
		dnvme_err(ndev, "Failed to alloc %ubytes!\n", access.bytes);
		return -ENOMEM;
	}

	switch (access.region) {
	case NVME_PCI_CONFIG:
		dnvme_dbg(ndev, "READ PCI Header Space: 0x%x+0x%x\n", 
			access.offset, access.bytes);
		ret = dnvme_read_from_config(pdev, &access, buf);
		if (ret < 0)
			goto out;
		break;

	case NVME_BAR0_BAR1:
		dnvme_dbg(ndev, "READ NVMe BAR0~1: 0x%x+0x%x\n", 
			access.offset, access.bytes);
		ret = dnvme_read_from_bar(ndev->bar[0], &access, buf);
		if (ret < 0)
			goto out;
		break;

	default:
		dnvme_err(ndev, "Access region(%d) is unkonwn!\n", access.region);
		ret = -EINVAL;
		goto out;
	}

	if (copy_to_user(access.buffer, buf, access.bytes)) {
		dnvme_err(ndev, "Failed to copy to user space!\n");
		ret = -EFAULT;
		goto out;
	}

out:
	kfree(buf);
	return ret;
}

/**
 * @brief Write data to NVMe device
 * 
 * @param ctx NVMe device context
 * @return 0 on success, otherwise a negative errno.
 */
int dnvme_generic_write(struct nvme_device *ndev, struct nvme_access __user *uaccess)
{
	int ret = 0;
	void *buf;
	struct nvme_access access;
	struct pci_dev *pdev = ndev->pdev;

	if (copy_from_user(&access, uaccess, sizeof(struct nvme_access))) {
		dnvme_err(ndev, "Failed to copy from user space!\n");
		return -EFAULT;
	}

	if (!access.bytes) {
		dnvme_dbg(ndev, "Access size is zero!\n");
		return 0;
	}

	ret = dnvme_check_access_align(ndev, &access);
	if (ret < 0)
		return ret;

	buf = kzalloc(access.bytes, GFP_KERNEL);
	if (!buf) {
		dnvme_err(ndev, "Failed to alloc %ubytes!\n", access.bytes);
		return -ENOMEM;
	}

	if (copy_from_user(buf, access.buffer, access.bytes)) {
		dnvme_err(ndev, "Failed to copy from user space!\n");
		ret = -EFAULT;
		goto out;
	}

	switch (access.region) {
	case NVME_PCI_CONFIG:
		dnvme_dbg(ndev, "WRITE PCI Header Space: 0x%x+0x%x\n",
			access.offset, access.bytes);
		ret = dnvme_write_to_config(pdev, &access, buf);
		if (ret < 0)
			goto out;
		break;
	
	case NVME_BAR0_BAR1:
		dnvme_dbg(ndev, "WRITE NVMe BAR0~1: 0x%x+0x%x\n",
			access.offset, access.bytes);
		ret = dnvme_write_to_bar(ndev->bar[0], &access, buf);
		if (ret < 0)
			goto out;
		break;

	default:
		dnvme_err(ndev, "Access region(%d) is unkonwn!\n", access.region);
		ret = -EINVAL;
		goto out;
	}

out:
	kfree(buf);
	return ret;
}

/**
 * @brief Create admin queue
 * 
 * @return 0 on success, otherwise a negative errno.
 */
int dnvme_create_admin_queue(struct nvme_device *ndev, 
	struct nvme_admin_queue __user *uaq)
{
	int ret;
	struct nvme_admin_queue aq;

	if (copy_from_user(&aq, uaq, sizeof(aq))) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		return -EFAULT;
	}

	if (aq.type == NVME_ADMIN_SQ) {
		ret = dnvme_create_asq(ndev, aq.elements);
	} else if (aq.type == NVME_ADMIN_CQ) {
		ret = dnvme_create_acq(ndev, aq.elements);
	} else {
		dnvme_err(ndev, "queue type(%d) is unknown!\n", aq.type);
		return -EINVAL;
	}

	return ret;
}

int dnvme_prepare_sq(struct nvme_device *ndev, struct nvme_prep_sq __user *uprep)
{
	struct nvme_sq *sq;
	struct nvme_prep_sq prep;
	int ret;

	if (copy_from_user(&prep, uprep, sizeof(prep))) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		return -EFAULT;
	}

	if (!prep.elements || prep.elements > ndev->q_depth) {
		dnvme_err(ndev, "SQ elements(%u) is invalid!\n", prep.elements);
		return -EINVAL;
	}

	ret = dnvme_check_qid_unique(ndev, NVME_SQ, prep.sq_id);
	if (ret < 0)
		return ret;

	if (NVME_CAP_CQR(ndev->reg_cap) && !prep.contig) {
		dnvme_err(ndev, "SQ shall be contig!\n");
		return -EINVAL;
	}

	sq = dnvme_alloc_sq(ndev, &prep, NVME_NVM_IOSQES);
	if (!sq)
		return -ENOMEM;

	return 0;
}

int dnvme_prepare_cq(struct nvme_device *ndev, struct nvme_prep_cq __user *uprep)
{
	struct nvme_cq *cq;
	struct nvme_prep_cq prep;
	int ret;

	if (copy_from_user(&prep, uprep, sizeof(prep))) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		return -EFAULT;
	}

	if (!prep.elements || prep.elements > ndev->q_depth) {
		dnvme_err(ndev, "CQ elements(%u) is invalid!\n", prep.elements);
		return -EINVAL;
	}

	ret = dnvme_check_qid_unique(ndev, NVME_CQ, prep.cq_id);
	if (ret < 0)
		return ret;

	if (NVME_CAP_CQR(ndev->reg_cap) && !prep.contig) {
		dnvme_err(ndev, "CQ shall be contig!\n");
		return -EINVAL;
	}

	cq = dnvme_alloc_cq(ndev, &prep, NVME_NVM_IOCQES);
	if (!cq)
		return -ENOMEM;

	if (cq->pub.irq_enabled) {
		ret = dnvme_create_icq_node(&ndev->irq_set, cq->pub.q_id, cq->pub.irq_no);
		if (ret < 0) {
			dnvme_err(ndev, "failed to create icq node!(%d)\n", ret);
			goto out;
		}
	}

	return 0;
out:
	dnvme_release_cq(ndev, cq);
	return ret;
}

int dnvme_get_sq_info(struct nvme_device *ndev, struct nvme_sq_public __user *usqp)
{
	struct nvme_sq_public sqp;
	struct nvme_sq *sq;

	if (copy_from_user(&sqp, usqp, sizeof(sqp))) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		return -EFAULT;
	}

	sq = dnvme_find_sq(ndev, sqp.sq_id);
	if (!sq) {
		dnvme_err(ndev, "SQ(%u) doesn't exist!\n", sqp.sq_id);
		return -EBADSLT;
	}

	if (copy_to_user(usqp, &sq->pub, sizeof(struct nvme_sq_public))) {
		dnvme_err(ndev, "failed to copy to user space!\n");
		return -EFAULT;
	}

	return 0;
}

int dnvme_get_cq_info(struct nvme_device *ndev, struct nvme_cq_public __user *ucqp)
{
	struct nvme_cq_public cqp;
	struct nvme_cq *cq;

	if (copy_from_user(&cqp, ucqp, sizeof(cqp))) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		return -EFAULT;
	}

	cq = dnvme_find_cq(ndev, cqp.q_id);
	if (!cq) {
		dnvme_err(ndev, "CQ(%u) doesn't exist!\n", cqp.q_id);
		return -EBADSLT;
	}

	if (copy_to_user(ucqp, &cq->pub, sizeof(struct nvme_cq_public))) {
		dnvme_err(ndev, "failed to copy to user space!\n");
		return -EFAULT;
	}

	return 0;
}

static int dnvme_get_pci_capability(struct nvme_device *ndev, 
	struct nvme_get_cap *gcap)
{
	struct nvme_capability *cap = &ndev->cap;
	struct pci_dev *pdev = ndev->pdev;
	u8 offset;

	switch (gcap->id) {
	case PCI_CAP_ID_PM:
		if (unlikely(!cap->pm))
			return -ENOENT;
		if (gcap->size < sizeof(*cap->pm))
			return -ENOSPC;
		if (copy_to_user(gcap->buf, cap->pm, sizeof(*cap->pm)))
			return -EFAULT;
		break;
	case PCI_CAP_ID_MSI:
		if (unlikely(!cap->msi))
			return -ENOENT;
		if (gcap->size < sizeof(*cap->msi))
			return -ENOSPC;
		if (copy_to_user(gcap->buf, cap->msi, sizeof(*cap->msi)))
			return -EFAULT;
		break;
	case PCI_CAP_ID_EXP:
		if (unlikely(!cap->express))
			return -ENOENT;
		if (gcap->size < sizeof(*cap->express))
			return -ENOSPC;
		if (copy_to_user(gcap->buf, cap->express, sizeof(*cap->express)))
			return -EFAULT;
		break;
	case PCI_CAP_ID_MSIX:
		if (unlikely(!cap->msix))
			return -ENOENT;
		if (gcap->size < sizeof(*cap->msix))
			return -ENOSPC;
		if (copy_to_user(gcap->buf, cap->msix, sizeof(*cap->msix)))
			return -EFAULT;
		break;
	default:
		offset = pci_find_capability(pdev, gcap->id);
		if (!offset) {
			dnvme_err(ndev, "ID:%u is not support!\n", gcap->id);
			return -EINVAL;
		}
		if (gcap->size < sizeof(offset))
			return -ENOSPC;
		if (copy_to_user(gcap->buf, &offset, sizeof(offset)))
			return -EFAULT;
		break;
	}
	return 0;
}

static int dnvme_get_pcie_capability(struct nvme_device *ndev, 
	struct nvme_get_cap *gcap)
{
	struct pci_dev *pdev = ndev->pdev;
	u16 offset;

	switch (gcap->id) {
	default:
		offset = pci_find_ext_capability(pdev, gcap->id);
		if (!offset) {
			dnvme_err(ndev, "ID:%u is not support!\n", gcap->id);
			return -EINVAL;
		}
		if (gcap->size < sizeof(offset))
			return -ENOSPC;
		if (copy_to_user(gcap->buf, &offset, sizeof(offset)))
			return -EFAULT;
		break;
	}
	return 0;
}

int dnvme_get_capability(struct nvme_device *ndev, struct nvme_get_cap __user *ucap)
{
	struct nvme_get_cap gcap;
	int ret;

	if (copy_from_user(&gcap, ucap, sizeof(gcap))) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		return -EFAULT;
	}

	if (gcap.type == NVME_CAP_TYPE_PCI)
		ret = dnvme_get_pci_capability(ndev, &gcap);
	else if (gcap.type == NVME_CAP_TYPE_PCIE)
		ret = dnvme_get_pcie_capability(ndev, &gcap);
	else
		ret = -EINVAL;

	return ret;
}

int dnvme_get_dev_info(struct nvme_device *ndev, struct nvme_dev_public __user *udevp)
{
	struct nvme_dev_public pub;

	pub.devno = ndev->instance;
	pub.family = nvme_gnl_id;

	if (copy_to_user(udevp, &pub, sizeof(pub))) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		return -EFAULT;
	}

	return 0;
}

static int dnvme_fill_hmb_descriptor(struct nvme_device *ndev, 
	struct nvme_hmb *hmb, struct nvme_hmb_alloc *alloc)
{
	unsigned int i;
	size_t offset = 0;

	for (i = 0; i < alloc->nr_desc; i++) {
		hmb->desc[i].addr = cpu_to_le64(hmb->buf_addr + offset);
		hmb->desc[i].size = cpu_to_le32(alloc->bsize[i]);
		offset += alloc->bsize[i];

		dnvme_info(ndev, "desc idx: %u, addr: 0x%llx, size: 0x%x\n", i, 
			le64_to_cpu(hmb->desc[i].addr), 
			le32_to_cpu(hmb->desc[i].size));
	}
	hmb->desc_num = alloc->nr_desc;
	return 0;
}

int dnvme_alloc_hmb(struct nvme_device *ndev, struct nvme_hmb_alloc __user *uhmb)
{
	struct pci_dev *pdev = ndev->pdev;
	struct nvme_hmb_alloc head;
	struct nvme_hmb_alloc *copy;
	struct nvme_hmb *hmb;
	size_t size;
	size_t buf_size = 0;
	u32 i;
	int ret;

	if (ndev->hmb) {
		dnvme_err(ndev, "hmb already exist! please release it first!\n");
		return -EPERM;
	}

	if (copy_from_user(&head, uhmb, sizeof(head))) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		return -EFAULT;
	}

	if (head.nr_desc > (PAGE_SIZE / sizeof(struct nvme_feat_hmb_descriptor))) {
		dnvme_err(ndev, "The number(%u) of descriptors exceeded the limit!\n",
			head.nr_desc);
		return -EINVAL;
	}

	size = sizeof(head) + sizeof(head.nr_desc) * head.nr_desc;

	copy = kzalloc(size, GFP_KERNEL);
	if (!copy) {
		dnvme_err(ndev, "failed to alloc for %u desc!\n", head.nr_desc);
		return -ENOMEM;
	}

	if (copy_from_user(copy, uhmb, size)) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		ret = -EFAULT;
		goto out;
	}

	hmb = kzalloc(sizeof(*hmb), GFP_KERNEL);
	if (!hmb) {
		dnvme_err(ndev, "failed to alloc memory!\n");
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < copy->nr_desc; i++)
		buf_size += copy->bsize[i];

	hmb->buf = dma_alloc_coherent(&pdev->dev, buf_size, &hmb->buf_addr, 
		GFP_KERNEL);
	if (!hmb->buf) {
		dnvme_err(ndev, "failed to alloc host memory buffer with "
			"size 0x%lx!\n", buf_size);
		ret = -ENOMEM;
		goto out2;
	}
	memset(hmb->buf, 0, buf_size);
	hmb->buf_size = buf_size;
	dnvme_info(ndev, "HMB buffer addr: 0x%llx, size: 0x%lx\n",
		hmb->buf_addr, hmb->buf_size);

	hmb->desc = dma_alloc_coherent(&pdev->dev, PAGE_SIZE, 
		&hmb->desc_addr, GFP_KERNEL);
	if (!hmb->desc) {
		dnvme_err(ndev, "failed to alloc host memory descriptor with "
			"size 0x%lx!\n", PAGE_SIZE);
		ret = -ENOMEM;
		goto free_hmb;
	}
	memset(hmb->desc, 0, PAGE_SIZE);
	hmb->desc_size = PAGE_SIZE;
	dnvme_info(ndev, "HMB desc addr: 0x%llx, size: 0x%lx\n", 
		hmb->desc_addr, PAGE_SIZE);

	dnvme_fill_hmb_descriptor(ndev, hmb, copy);

	head.desc_list = hmb->desc_addr;
	if (copy_to_user(uhmb, &head, sizeof(head))) {
		dnvme_err(ndev, "failed to copy to user space!\n");
		ret = -EFAULT;
		goto free_desc;
	}
	ndev->hmb = hmb;

	kfree(copy);
	return 0;
free_desc:
	dma_free_coherent(&pdev->dev, hmb->desc_size, hmb->desc, hmb->desc_addr);
free_hmb:
	dma_free_coherent(&pdev->dev, hmb->buf_size, hmb->buf, hmb->buf_addr);
out2:
	kfree(hmb);
out:
	kfree(copy);
	return ret;
}

int dnvme_release_hmb(struct nvme_device *ndev)
{
	struct pci_dev *pdev = ndev->pdev;
	struct nvme_hmb *hmb;

	if (!ndev->hmb)
		return 0;

	hmb = ndev->hmb;
	dma_free_coherent(&pdev->dev, hmb->desc_size, hmb->desc, hmb->desc_addr);
	dma_free_coherent(&pdev->dev, hmb->buf_size, hmb->buf, hmb->buf_addr);
	kfree(hmb);
	ndev->hmb = NULL;

	return 0;
}

int dnvme_access_hmb(struct nvme_device *ndev, struct nvme_hmb_access __user *uhmb)
{
	struct nvme_hmb_access access;
	struct nvme_hmb *hmb = ndev->hmb;

	if (!hmb) {
		dnvme_err(ndev, "host memory buffer not exist!\n");
		return -EPERM;
	}

	if (copy_from_user(&access, uhmb, sizeof(struct nvme_hmb_access))) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		return -EFAULT;
	}

	if (access.offset > hmb->buf_size || access.length > hmb->buf_size ||
		(access.offset + access.length) > hmb->buf_size) {
		dnvme_err(ndev, "access offset:0x%x, len:0x%x, HMB len:0x%lx!\n",
			access.offset, access.length, hmb->buf_size);
		return -EINVAL;
	}

	switch (access.option) {
	case NVME_HMB_OPT_READ:
		if (copy_to_user(access.buf, hmb->buf + access.offset, 
				access.length)) {
			dnvme_err(ndev, "failed to copy to user space!\n");
			return -EFAULT;
		}
		break;

	case NVME_HMB_OPT_WRITE:
		if (copy_from_user(hmb->buf + access.offset, access.buf, 
				access.length)) {
			dnvme_err(ndev, "failed to copy from user space!\n");
			return -EFAULT;
		}
		break;

	default:
		dnvme_err(ndev, "access option(%u) is unknown!\n", access.option);
		return -EINVAL;
	}

	return 0;
}

