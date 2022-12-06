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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/msi.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/version.h>

#include "core.h"
#include "pci.h"
#include "io.h"
#include "queue.h"
#include "irq.h"

/**
 * @ts: Table Size
 * @msix_tbl: MSI-X Table structure position
 * @pba_tbl: MSI-X PBA(Pending Bit Array) structure position
 */
struct msix_info {
	u16		ts;
	u8 __iomem	*msix_tbl;
	u8 __iomem	*pba_tbl;
};

/* Static function declarations used for setting interrupt schemes. */
static int validate_irq_inputs(struct nvme_context
    *pmetrics_device_elem, struct nvme_interrupt *irq_new,
        struct msix_info *pmsix_tbl_info);
static int update_msixptr(struct  nvme_context
    *pmetrics_device_elem, u16 offset, struct msix_info *pmsix_tbl_info);

/**
 * @brief This property is used to mask nvme_interrupt when using pin-based
 *  interrupts, single message MSI, or multiple message MSI.
 * @attention Host software shall not access this property when configured
 *  for MSI-X.
 */
static void set_int_mask(void __iomem *bar0, u32 val)
{
	dnvme_writel(bar0, NVME_REG_INTMS, val);
#if IS_ENABLED(CONFIG_DNVME_FLUSH_REG_WRITE)
	dnvme_readl(bar0, NVME_REG_INTMS);
#endif
}

/**
 * @brief This property is used to unmask interrupts when using pin-based
 *  interrupts, single message MSI, or multiple message MSI.
 * @attention Host software shall not access this property when configured
 *  for MSI-X.
 */
static void clear_int_mask(void __iomem *bar0, u32 val)
{
	dnvme_writel(bar0, NVME_REG_INTMC, val);
#if IS_ENABLED(CONFIG_DNVME_FLUSH_REG_WRITE)
	dnvme_readl(bar0, NVME_REG_INTMC);
#endif
}

static void set_msix_single_mask(void __iomem *msix_tbl, u32 entry)
{
	void __iomem *msix_entry = msix_tbl + entry * PCI_MSIX_ENTRY_SIZE;
	u32 vec;

	vec = dnvme_readl(msix_entry, PCI_MSIX_ENTRY_VECTOR_CTRL);
	vec |= PCI_MSIX_ENTRY_CTRL_MASKBIT;
	dnvme_writel(msix_entry, PCI_MSIX_ENTRY_VECTOR_CTRL, vec);
}

static void clear_msix_single_mask(void __iomem *msix_tbl, u32 entry)
{
	void __iomem *msix_entry = msix_tbl + entry * PCI_MSIX_ENTRY_SIZE;
	u32 vec;

	vec = dnvme_readl(msix_entry, PCI_MSIX_ENTRY_VECTOR_CTRL);
	vec &= ~PCI_MSIX_ENTRY_CTRL_MASKBIT;
	dnvme_writel(msix_entry, PCI_MSIX_ENTRY_VECTOR_CTRL, vec);
}

static void set_msix_mask(void __iomem *msix_tbl, struct msix_entry *arr, 
	u32 count)
{
	int i;

	for (i = 0; i < count; i++)
		set_msix_single_mask(msix_tbl, arr[i].entry);
}

static void clear_msix_mask(void __iomem *msix_tbl, struct msix_entry *arr,
	u32 count)
{
	u32 i;

	for (i = 0; i < count; i++)
		clear_msix_single_mask(msix_tbl, arr[i].entry);
}

static bool pba_bit_is_set(void __iomem *pba_tbl, u32 entry)
{
	u32 bit = entry & 0x1f; /* entry % 32 */
	u32 pba;

	pba = dnvme_readl(pba_tbl, 4 * (entry >> 5));

	return (pba & (1 << bit)) ? true : false;
}

static bool pba_bits_is_set(void __iomem *pba_tbl, struct msix_entry *arr,
	u32 count)
{
	u32 i;

	for (i = 0; i < count; i++) {
		if (pba_bit_is_set(pba_tbl, arr[i].entry))
			return true;
	}
	return false;
}

/**
 * @brief Mask the specified irq.
 *
 * @note Determine the type of masking required based on the interrupt
 *  scheme active.
 */
int dnvme_mask_interrupt(struct nvme_irq_set *irq, u16 irq_no)
{
	struct nvme_context *ctx = dnvme_irq_to_context(irq);
	void __iomem *bar0 = ctx->dev->priv.bar0;

	switch (irq->irq_type) {
	case NVME_INT_PIN:
	case NVME_INT_MSI_SINGLE:
	case NVME_INT_MSI_MULTI:
		set_int_mask(bar0, 0x1 << irq_no);
		break;

	case NVME_INT_MSIX:
		set_msix_single_mask(irq->mask_ptr, irq_no);
		break;

	case NVME_INT_NONE:
		break;

	default:
		dnvme_err("irq type(%u) is unknown!\n", irq->irq_type);
		return -EINVAL;
	}
	return 0;
}

/**
 * @brief Unmask the specified irq.
 *
 * @note Determine the type of masking required based on the interrupt
 *  scheme active.
 */
int dnvme_unmask_interrupt(struct nvme_irq_set *irq, u16 irq_no)
{
	struct nvme_context *ctx = dnvme_irq_to_context(irq);
	void __iomem *bar0 = ctx->dev->priv.bar0;

	switch (irq->irq_type) {
	case NVME_INT_PIN:
	case NVME_INT_MSI_SINGLE:
	case NVME_INT_MSI_MULTI:
		clear_int_mask(bar0, 0x1 << irq_no);
		break;

	case NVME_INT_MSIX:
		clear_msix_single_mask(irq->mask_ptr, irq_no);
		break;

	case NVME_INT_NONE:
		break;

	default:
		dnvme_err("irq type(%u) is unknown!\n", irq->irq_type);
		return -EINVAL;
	}
	return 0;
}

/**
 * @brief Find icq node within the given irq track list node.
 *
 * @return pointer to the icq node on success. Otherwise returns NULL.
 */
static struct nvme_icq *find_icq_node(struct nvme_irq *irq, u16 cq_id)
{
	struct nvme_icq *node;

	list_for_each_entry(node, &irq->icq_list, entry) {
		if (cq_id == node->cq_id)
			return node;
	}
	return NULL;
}

static struct nvme_irq *find_irq_node_by_vec(struct nvme_irq_set *irq_set,
	u32 int_vec)
{
	struct nvme_irq *node;

	list_for_each_entry(node, &irq_set->irq_list, irq_entry) {
		if (int_vec == node->int_vec)
			return node;
	}
	return NULL;
}

/**
 * @brief Find the irq node in the given irq list
 * 
 * @return pointer to the irq node on success. Otherwise returns NULL.
 */
static struct nvme_irq *find_irq_node_by_id(struct nvme_irq_set *irq_set, 
	u16 irq_id)
{
	struct nvme_irq *node;

	list_for_each_entry(node, &irq_set->irq_list, irq_entry) {
		if (irq_id == node->irq_id)
			return node;
	}
	return NULL;
}

/**
 * @brief Find the work node in the given work list
 * 
 * @return pointer to the work node on success. Otherwise returns NULL.
 */
static struct nvme_work *find_work_node_by_vec(struct nvme_irq_set *irq_set,
	 u32 int_vec)
{
	struct nvme_work *node;

	list_for_each_entry(node, &irq_set->work_list, entry) {
		if (int_vec == node->int_vec)
			return node;
	}
	return NULL;
}

static struct nvme_work *find_work_node_by_id(struct nvme_irq_set *irq_set,
	u16 irq_id)
{
	struct nvme_work *node;

	list_for_each_entry(node, &irq_set->work_list, entry) {
		if (irq_id == node->irq_id)
			return node;
	}
	return NULL;
}

/**
 * @brief Create a icq node and add it to the specified linked list
 * 
 * @param cq_id The CQ ID associated with the irq
 * @return 0 on success, otherwise a negative errno.
 */
static int create_icq_node(struct nvme_irq *irq, u16 cq_id)
{
	struct nvme_icq *icq;

	icq = find_icq_node(irq, cq_id);
	if (icq) {
		dnvme_warn("icq(%u) already exist in irq(%u) list!\n", 
			cq_id, irq->irq_id);
		return 0;
	}

	icq = kmalloc(sizeof(*icq), GFP_KERNEL);
	if (!icq) {
		dnvme_err("failed to alloc icq node!\n");
		return -ENOMEM;
	}
	icq->cq_id = cq_id;

	list_add_tail(&icq->entry, &irq->icq_list);
	return 0;
}

int dnvme_create_icq_node(struct nvme_irq_set *irq_set, u16 cq_id, u16 irq_no)
{
	int ret = 0;
	struct nvme_irq *irq;

	mutex_lock(&irq_set->mtx_lock);

	irq = find_irq_node_by_id(irq_set, irq_no);
	if (!irq) {
		dnvme_err("failed to find irq node %u!\n", irq_no);
		ret = -EINVAL;
		goto unlock;
	}

	ret = create_icq_node(irq, cq_id);
	if (ret < 0)
		goto unlock;

unlock:
	mutex_unlock(&irq_set->mtx_lock);
	return ret;
}

static void delete_icq_node(struct nvme_icq *icq)
{
	if (unlikely(!icq))
		return;

	list_del(&icq->entry);
	kfree(icq);
}

void dnvme_delete_icq_node(struct nvme_irq_set *irq_set, u16 cq_id, u16 irq_no)
{
	struct nvme_irq *irq;
	struct nvme_icq *icq;

	irq = find_irq_node_by_id(irq_set, irq_no);
	if (!irq)
		return;

	icq = find_icq_node(irq, cq_id);
	if (!icq)
		return;

	delete_icq_node(icq);
}

/**
 * @brief Delete all icq nodes in the icq list and free memory.
 */
static void delete_icq_list(struct nvme_irq *irq)
{
	struct nvme_icq *icq;
	struct nvme_icq *tmp;

	list_for_each_entry_safe(icq, tmp, &irq->icq_list, entry) {
		delete_icq_node(icq);
	}
}

/**
 * @brief Create a irq node and add to irq list.
 * @attention Hold irq_set mutex lock before calling this function
 *
 * @return 0 on success, otherwise a negative errno. 
 */
static int create_irq_node(struct nvme_irq_set *irq_set, u32 int_vec, u16 irq_id)
{
	struct nvme_irq *irq;

	irq = find_irq_node_by_id(irq_set, irq_id);
	if (irq) {
		if (irq->int_vec == int_vec) {
			dnvme_warn("irq node(%u:%u) already exist! skip\n",
				irq_id, int_vec);
			return 0;
		}
		dnvme_err("irq node already exist! int_vec mismatch!\n");
		return -EINVAL;
	}

	irq = find_irq_node_by_vec(irq_set, int_vec);
	if (irq) {
		dnvme_err("irq node already exist! irq_id mismatch!\n");
		return -EINVAL;
	}

	irq = kzalloc(sizeof(*irq), GFP_KERNEL);
	if (irq == NULL) {
		dnvme_err("failed to alloc irq node!\n");
		return -ENOMEM;
	}

	irq->int_vec = int_vec; /* int vector number   */
	irq->irq_id = irq_id;
	irq->isr_fired = 0;
	irq->isr_count = 0;
	INIT_LIST_HEAD(&irq->icq_list);

	list_add_tail(&irq->irq_entry, &irq_set->irq_list);
	return 0;
}

static void delete_irq_node(struct nvme_irq *irq)
{
	if (unlikely(!irq))
		return;

	delete_icq_list(irq);
	list_del(&irq->irq_entry);	
	kfree(irq);
}

/**
 * @brief Delete all irq node in irq list and free memory.
 * @attention Hold irq_set mutex lock before calling this function
 */
static void delete_irq_list(struct nvme_irq_set *irq_set)
{
	struct nvme_irq *irq;
	struct nvme_irq *tmp;

	list_for_each_entry_safe(irq, tmp, &irq_set->irq_list, irq_entry) {
		delete_irq_node(irq);
	}
}

static void inc_isr_count(struct nvme_irq_set *irq_set, u16 irq_id)
{
	struct nvme_irq *node;

	node = find_irq_node_by_id(irq_set, irq_id);
	if (!node) {
		pr_err("ERROR: irq node(ID:%u) not found!\n", irq_id);
		return;
	}

	/* !TODO: isr_fired & isr_count are redundant? */
	node->isr_fired = 1;
	node->isr_count++;
	dnvme_vdbg("irq node(ID:%u) count is %u\n", node->irq_id, 
		node->isr_count);
}

/**
 * @brief ISR bottom half
 */
static void update_irq_flag(struct work_struct *wk)
{
	struct nvme_work *pwork = container_of(wk, struct nvme_work, work);

	mutex_lock(&pwork->irq_set->mtx_lock);
	inc_isr_count(pwork->irq_set, pwork->irq_id);
	mutex_unlock(&pwork->irq_set->mtx_lock);
}

/**
 * @brief Create a work node and add to list.
 * 
 * @return 0 on success, otherwise a negative errno.
 */
static int create_work_node(struct nvme_irq_set *irq_set, u32 int_vec, 
	u16 irq_id)
{
	struct nvme_work *node;

	/* Sanity check */
	node = find_work_node_by_id(irq_set, irq_id);
	if (node) {
		if (node->int_vec == int_vec) {
			dnvme_warn("work node(%u:%u) already exist! skip\n", 
				irq_id, int_vec);
			return 0;
		}
		dnvme_err("work node already exist! int_vec mismatch!\n");
		return -EINVAL;
	}

	node = find_work_node_by_vec(irq_set, int_vec);
	if (node) {
		dnvme_err("work node already exist! irq_id mismatch!\n");
		return -EINVAL;
	}

	node = kzalloc(sizeof(struct nvme_work), GFP_KERNEL);
	if (!node) {
		dnvme_err("failed to alloc work node!\n");
		return -ENOMEM;
	}

	/* Initialize the bottom half call back with the work struct */
	INIT_WORK(&node->work, update_irq_flag);
	node->irq_set = irq_set;
	node->int_vec = int_vec;
	node->irq_id = irq_id;
	/* Add work item to work items list */
	list_add_tail(&node->entry, &irq_set->work_list);
	return 0;
}

static void delete_work_node(struct nvme_work *wk)
{
	if (unlikely(!wk))
		return;

	list_del(&wk->entry);
	kfree(wk);
}

/**
 * @brief Delete all the work nodes in the list and free memory.
 */
static void delete_work_list(struct nvme_irq_set *irq_set)
{
	struct nvme_work *node;
	struct nvme_work *tmp;

	list_for_each_entry_safe(node, tmp, &irq_set->work_list, entry) {
		delete_work_node(node);
	}
}

static int create_irq_work_queue(struct nvme_irq_set *irq_set)
{
	if (irq_set->wq) {
		dnvme_warn("work queue already exist! skip\n");
		return 0;
	}

	irq_set->wq = create_workqueue("irq_wq");
	if (!irq_set->wq) {
		dnvme_err("failed to create work queue!\n");
		return -ENOMEM;
	}

	return 0;
}

static void destroy_irq_work_queue(struct nvme_irq_set *irq_set)
{
	if (!irq_set->wq)
		return;

	/* Flush the WQ and wait till all work's are executed */
	flush_workqueue(irq_set->wq);
	destroy_workqueue(irq_set->wq);
	irq_set->wq = NULL;
}

/**
 * @brief Create irq and work node. If necessary, create icq node at the
 *  same time.
 * 
 * @return 0 on success, otherwise a negative errno.
 * @note if irq_id = 0, create an icq node and bind to the irq node.
 *  Otherwise, it won't to create an icq node.
 */
static int create_irq_work_pair(struct nvme_irq_set *irq_set, u32 int_vec,
	u16 irq_id)
{
	int ret;

	ret = create_irq_node(irq_set, int_vec, irq_id);
	if (ret < 0)
		return ret;

	ret = create_work_node(irq_set, int_vec, irq_id);
	if (ret < 0)
		goto out;
	
	if (irq_id == 0) {
		ret = create_icq_node(find_irq_node_by_id(irq_set, irq_id), 0);
		if (ret < 0)
			goto out2;
	}
	return 0;
out2:
	delete_work_node(find_work_node_by_id(irq_set, irq_id));
out:
	delete_irq_node(find_irq_node_by_id(irq_set, irq_id));
	return ret;
}

static void destroy_irq_work_pair(struct nvme_irq_set *irq_set, u32 int_vec, 
	u16 irq_id)
{
	delete_work_node(find_work_node_by_id(irq_set, irq_id));
	delete_irq_node(find_irq_node_by_id(irq_set, irq_id));
}

/**
 * @brief Register irq handler, Create irq node and Enable irq.
 *
 * @return 0 on success, otherwise a negative errno.
 */
static int set_int_pin(struct nvme_context *ctx)
{
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->priv.pdev;
	void __iomem *bar0 = ndev->priv.bar0;
	int ret;

	ret = request_irq(pdev->irq, dnvme_interrupt, IRQF_SHARED, "pin-base",
		&ctx->irq_set);
	if (ret < 0) {
		dnvme_err("failed to request irq(%u)!\n", pdev->irq);
		return ret;
	}
	dnvme_vdbg("Pin-Base Interrupt Vector(%u)\n", pdev->irq);

	ret = create_irq_work_pair(&ctx->irq_set, pdev->irq, 0);
	if (ret < 0) {
		dnvme_err("failed to create irq work pair!\n");
		goto out;
	}
	clear_int_mask(bar0, UINT_MAX);
	pci_enable_int_pin(pdev);
	return 0;
out:
	free_irq(pdev->irq, &ctx->irq_set);
	return ret;
}

/**
 * @brief Enable msi irq, Register irq handler, Create irq node.
 *
 * @return 0 on success, otherwise a negative errno.
 */
static int set_int_msi_single(struct nvme_context *ctx)
{
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->priv.pdev;
	void __iomem *bar0 = ndev->priv.bar0;
	int ret;

	clear_int_mask(bar0, UINT_MAX);

	/*
	 * Allocate one interrupt to this device. A successful call will switch the
	 * device to msi single mode and pdev->irq is changed to a new number which
	 * represents msi interrupt vector consequently.
	 */
	ret = pci_enable_msi(pdev);
	if (ret) {
		dnvme_err("Can't enable MSI-single!\n");
		goto out;
	}

	ret = request_irq(pdev->irq, dnvme_interrupt, IRQF_SHARED,
		"msi-single", &ctx->irq_set);
	if (ret < 0) {
		dnvme_err("failed to request irq(%u)!\n", pdev->irq);
		goto out2;
	}
	dnvme_vdbg("MSI-Single Interrupt Vector(%u)\n", pdev->irq);

	ret = create_irq_work_pair(&ctx->irq_set, pdev->irq, 0);
	if (ret < 0) {
		dnvme_err("failed to create irq work pair!\n");
		goto out3;
	}

	return 0;
out3:
	free_irq(pdev->irq, &ctx->irq_set);
out2:
	pci_disable_msi(pdev);
out:
	set_int_mask(bar0, UINT_MAX);
	return ret;
}

/**
 * @brief Alloc and enable msi irq, Register irq handler, Create irq node.
 *
 * @return 0 on success, otherwise a negative errno.
 */
static int set_int_msi_multi(struct nvme_context *ctx, u16 num_irqs)
{
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->priv.pdev;
	void __iomem *bar0 = ndev->priv.bar0;
	int ret, i, j;

	clear_int_mask(bar0, UINT_MAX);

	/*
	 * Enable msi-block interrupts for this device. The pdev->irq will
	 * be the lowest of the new interrupts assigned to this device.
	 */
	ret = pci_alloc_irq_vectors(pdev, num_irqs, num_irqs, PCI_IRQ_MSI);
	if (ret != num_irqs) {
		dnvme_err("Can't enable MSI-Multi with num_irq=%u\n", num_irqs);
		goto out;
	}

	/* Request irq on each interrupt vector */
	for (i = 0; i < num_irqs; i++) {
		ret = request_irq(pdev->irq + i, dnvme_interrupt, IRQF_SHARED,
			"msi-multi", &ctx->irq_set);
		if (ret < 0) {
			dnvme_err("failed to request irq(%u)!\n", pdev->irq + i);
			goto out2;
		}
	}

	for (j = 0; j < num_irqs; j++) {
		ret = create_irq_work_pair(&ctx->irq_set, pdev->irq + j, j);
		if (ret < 0) {
			dnvme_err("failed to create irq work pair(%d)!\n", j);
			goto out3;
		}
	}

	return 0;
out3:
	for (j--; j >= 0; j--)
		destroy_irq_work_pair(&ctx->irq_set, pdev->irq + j, j);
out2:
	for (i--; i >= 0; i--)
		free_irq(pdev->irq + i, &ctx->irq_set);
out:
	set_int_mask(bar0, UINT_MAX);
	return ret;
}

/**
 * @brief Alloc and enable msi irq, Register irq handler, Create irq node.
 *
 * @details Sets up the active IRQ scheme to MSI-X. It gets the number of irqs
 *  requested and loops from 0 to n -1 irqs, enables the active irq
 *  scheme to MSI-X. Calls request_irq for each irq no and gets the OS
 *  allocated interrupt vector. This function add each of this irq node
 *  in the irq track linked list with int_vec and irq no. At any point
 *  if the adding of node fails it cleans up and exits with invalid return
 *  code.
 *
 * @return 0 on success, otherwise a negative errno
 */
static int set_int_msix(struct nvme_context *ctx, u16 num_irqs, 
	struct msix_info *msix)
{
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->priv.pdev;
	struct msix_entry *entries;
	int ret, i, j;

	entries = kcalloc(num_irqs, sizeof(*entries), GFP_KERNEL);
	if (!entries) {
		dnvme_err("failed to alloc msix_entry!\n");
		return -ENOMEM;
	}

	for (i = 0; i < num_irqs; i++)
		entries[i].entry = i;

	ret = pci_enable_msix_range(pdev, entries, num_irqs, num_irqs);
	if (ret != num_irqs) {
		dnvme_err("failed to enable msix!(%d)\n", ret);
		ret = -EPERM;
		goto out;
	}

	for (i = 0; i < num_irqs; i++) {
		ret = request_irq(entries[i].vector, dnvme_interrupt, 
			IRQF_SHARED, "msi-x", &ctx->irq_set);
		if (ret < 0) {
			dnvme_err("failed to request vec(%u)!\n", 
				entries[i].vector);
			goto out2;
		}
	}

	for (j = 0; j < num_irqs; j++) {
		ret = create_irq_work_pair(&ctx->irq_set, entries[j].vector, 
			entries[j].entry);
		if (ret < 0) {
			dnvme_err("failed to create irq work pair(%u)!\n", 
				entries[j].entry);
			goto out3;
		}
	}

	if (pba_bits_is_set(msix->pba_tbl, entries, num_irqs)) {
		dnvme_err("PBA bit is set at IRQ init, shall set none!\n");
		ret = -EINVAL;
		goto out3;
	}

	set_msix_mask(msix->msix_tbl, entries, num_irqs);

	kfree(entries);
	return 0;
out3:
	for (j--; j >= 0; j--)
		destroy_irq_work_pair(&ctx->irq_set, entries[j].vector, 
			entries[j].entry);
out2:
	for (i--; i >= 0; i--)
		free_irq(entries[i].vector, &ctx->irq_set);

	pci_disable_msix(pdev);
out:
	kfree(entries);
	return ret;
}

void dnvme_clear_interrupt(struct nvme_context *ctx)
{
	struct nvme_irq *irq;
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->priv.pdev;
	enum nvme_irq_type irq_type = ndev->pub.irq_active.irq_type;

#if IS_ENABLED(CONFIG_DNVME_CHECK_MUTEX_LOCK)
	if (!mutex_is_locked(&ctx->irq_set.mtx_lock))
		dnvme_warn("Mutex should have been locked before this...\n");
#endif

	list_for_each_entry(irq, &ctx->irq_set.irq_list, irq_entry) {
		free_irq(irq->int_vec, &ctx->irq_set);
	}

	switch (irq_type) {
	case NVME_INT_MSI_SINGLE:
	case NVME_INT_MSI_MULTI:
		pci_disable_msi(pdev);
		break;

	case NVME_INT_MSIX:
		pci_disable_msix(pdev);
		break;

	case NVME_INT_PIN:
		pci_disable_int_pin(pdev);
		break;

	case NVME_INT_NONE:
	default:
		break; /* Nothing to do */
	}

	destroy_irq_work_queue(&ctx->irq_set);

	delete_irq_list(&ctx->irq_set);
	delete_work_list(&ctx->irq_set);

	/* update active irq info */
	ctx->dev->pub.irq_active.irq_type = NVME_INT_NONE;
	ctx->dev->pub.irq_active.num_irqs = 0;
	ctx->irq_set.irq_type = NVME_INT_NONE;
}


/**
 * @brief Set new interrupt scheme for this device
 *
 * @attention The controller should be disabled before setting up new irq.
 *
 * @details It will set the new interrupt scheme for this device regardless
 *  of the current irq scheme that is active for this device. It also validates
 *  if the inputs given for setting up new scheme are within bounds.
 */
int dnvme_set_interrupt(struct nvme_context *ctx, struct nvme_interrupt __user *uirq)
{
	struct msix_info msix;
	struct nvme_device *ndev = ctx->dev;
	struct nvme_irq_set *irq_set = &ctx->irq_set;
	struct nvme_interrupt irq;
	enum nvme_irq_type act_irq = ndev->pub.irq_active.irq_type;
	int ret;

	memset(&msix, 0, sizeof(struct msix_info));

	if (copy_from_user(&irq, uirq, sizeof(irq))) {
		dnvme_err("failed to copy from user space!\n");
		return -EFAULT;
	}
	dnvme_vdbg("Set irq_type:%d, num_irqs:%u\n", irq.irq_type, irq.num_irqs);

	/* First validate if the inputs given are correct */
	ret = validate_irq_inputs(ctx, &irq, &msix);
	if (ret < 0) {
		dnvme_err("Invalid inputs set or device is not disabled");
		return ret;
	}

	/* lock onto IRQ linked list mutex as we would access the IRQ list */
	mutex_lock(&irq_set->mtx_lock);

	if (act_irq != NVME_INT_NONE)
		dnvme_clear_interrupt(ctx);

	ret = create_irq_work_queue(irq_set);
	if (ret < 0) {
		dnvme_err("Failed to initialize resources for work queue/items");
		goto out;
	}

	switch (irq.irq_type) {
	case NVME_INT_MSI_SINGLE:
		ret = set_int_msi_single(ctx);
		break;
	case NVME_INT_MSI_MULTI:
		ret = set_int_msi_multi(ctx, irq.num_irqs);
		break;
	case NVME_INT_MSIX:
		ret = set_int_msix(ctx, irq.num_irqs, &msix);
		break;
	case NVME_INT_PIN:
		ret = set_int_pin(ctx);
		break;
	case NVME_INT_NONE:
		break;
	default:
		dnvme_err("irq_type(%d) is unknown!\n", irq.irq_type);
		ret = -EBADRQC;
		break;
	}

	if (ret < 0) {
		dnvme_err("failed to set irq_type:%d!\n",irq.irq_type);
		goto out2;
	}

	/* update active irq info */
	ndev->pub.irq_active.irq_type = irq.irq_type;
	ndev->pub.irq_active.num_irqs = irq.num_irqs;
	irq_set->irq_type = irq.irq_type;

	mutex_unlock(&irq_set->mtx_lock);
	return 0;
out2:
	destroy_irq_work_queue(irq_set);
out:
	mutex_unlock(&irq_set->mtx_lock);
	return ret;
}

/*
 * Check if the controller supports the interrupt type requested. If it
 * supports returns the offset, otherwise it will return invalid for the
 * caller to indicate that the controller does not support the capability
 * type.
 */
static int check_cntlr_cap(struct pci_dev *pdev, enum nvme_irq_type cap_type,
    u16 *offset)
{
	u16 val = 0;
	u16 pci_offset = 0;
	int ret_val = -EINVAL;

	if (pci_read_config_word(pdev, PCI_STATUS, &val) < 0) {
		dnvme_err("pci_read_config failed...");
		return -EINVAL;
	}
	dnvme_vdbg("PCI_DEVICE_STATUS = 0x%X", val);
	if (!(val & PCI_STATUS_CAP_LIST)) {
		dnvme_err("Controller does not support Capability list...");
		return -EINVAL;
	} else {
		if (pci_read_config_word(pdev, PCI_CAPABILITY_LIST, &pci_offset) < 0) {
			dnvme_err("pci_read_config failed...");
			return -EINVAL;
		}
	}
	/* Interrupt Type MSI-X*/
	if (cap_type == NVME_INT_MSIX) {
		/* Loop through Capability list */
		while (pci_offset) {
			if (pci_read_config_word(pdev, pci_offset, &val) < 0) {
				dnvme_err("pci_read_config failed...");
				return -EINVAL;
			}
			/* exit when we find MSIX_capbility offset */
			if ((val & PCI_CAP_ID_MASK) == PCI_CAP_ID_MSIX) {
				/* write msix cap offset */
				*offset = pci_offset;
				ret_val = 0;
				/* break from while loop */
				break;
			}
			/* Next Capability offset. */
			pci_offset = (val & PCI_CAP_NEXT_MASK) >> 8;
		} /* end of while loop */

	} else if (cap_type == NVME_INT_MSI_SINGLE || cap_type == NVME_INT_MSI_MULTI) {
		/* Loop through Capability list */
		while (pci_offset) {
			if (pci_read_config_word(pdev, pci_offset, &val) < 0) {
				dnvme_err("pci_read_config failed...");
				return -EINVAL;
			}
			/* exit when we find MSIX_capbility offset */
			if ((val & PCI_CAP_ID_MASK) == PCI_CAP_ID_MSI) {
				/* write the msi offset */
				*offset = pci_offset;
				ret_val = 0;
				/* break from while loop */
				break;
			}
			/* Next Capability offset. */
			pci_offset = (val & PCI_CAP_NEXT_MASK) >> 8;
		} /* end of while loop */

	} else {
		dnvme_err("Invalid capability type specified...");
		ret_val = -EINVAL;
	}

	return ret_val;
}


/*
 * Validates the IRQ inputs for MSI-X, MSI-Single and MSI-Mutli.
 * If the CC.EN bit is set or the number of irqs are invalid then
 * return failure otherwise success.
 */
static int validate_irq_inputs(struct nvme_context *ctx, 
	struct nvme_interrupt *irq_new,
	struct msix_info *pmsix_tbl_info)
{
	int ret_val = 0;
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ctx->dev->priv.pdev;
	void __iomem *bar0 = ndev->priv.bar0;
	u16 msi_offset;
	u16 mc_val;

	/* Check if the EN bit is set and return failure if set */
	if (dnvme_readl(bar0, NVME_REG_CC) & NVME_CC_ENABLE) {
		dnvme_err("IRQ Scheme cannot change when CC.EN bit is set!!");
		dnvme_err("Call Disable or Disable completely first...");
		return -EINVAL;
	}

	/* Switch based on new irq type desired */
	switch (irq_new->irq_type) {

	case NVME_INT_MSI_SINGLE: /* MSI Single interrupt settings */
		if (irq_new->num_irqs != 1) {
			dnvme_err("MSI Single: num_irqs(%u) = 1\n", irq_new->num_irqs);
			return -EINVAL;
		}
		/* Check if the card Supports MSI capability */
		if (check_cntlr_cap(pdev, NVME_INT_MSI_SINGLE, &msi_offset) < 0) {
			dnvme_err("Controller does not support for MSI capability!\n");
			return -EINVAL;
		}
		/* Update interrupt vector Mask Set and Mask Clear offsets */
		ctx->irq_set.mask_ptr = ctx->dev->priv.bar0 + NVME_REG_INTMS;
		break;

	case NVME_INT_MSI_MULTI: /* MSI Multi interrupt settings */
		if (irq_new->num_irqs > PCI_MSI_VEC_MAX || irq_new->num_irqs == 0) {
			dnvme_err("MSI Multi: num_irqs(%u) <= %u\n", 
				irq_new->num_irqs, PCI_MSI_VEC_MAX);
			return -EINVAL;
		}
		/* Check if the card Supports MSI capability */
		if (check_cntlr_cap(pdev, NVME_INT_MSI_MULTI, &msi_offset) < 0) {
			dnvme_err("Controller does not support for MSI capability!\n");
			return -EINVAL;
		}
		/* compute MSI MC offset if MSI is supported */
		msi_offset += 2;
		/* Read MSI-MC value */
		pci_read_config_word(pdev, msi_offset, &mc_val);

		if(irq_new->num_irqs > (1 << ((mc_val & PCI_MSI_FLAGS_QMASK) >> 1))) { 
			dnvme_err("IRQs = %d exceed MSI MMC = %d", irq_new->num_irqs,
				(1 << ((mc_val & PCI_MSI_FLAGS_QMASK) >> 1)));
			return -EINVAL;
		}
		/* Update interrupt vector Mask Set and Mask Clear offsets */
		ctx->irq_set.mask_ptr = ctx->dev->priv.bar0 + NVME_REG_INTMS;
		break;

	case NVME_INT_PIN: 	/* NVME_INT_PIN  */
		/* Update interrupt vector Mask Set and Mask Clear offsets */
		ctx->irq_set.mask_ptr = ctx->dev->priv.bar0 + NVME_REG_INTMS;
		break;

	case NVME_INT_MSIX: /* MSI-X interrupt settings */
		/* First check if num irqs req are greater than MAX MSIX SUPPORTED */
		if (irq_new->num_irqs > PCI_MSIX_VEC_MAX || irq_new->num_irqs == 0) {
			dnvme_err("MSI-X: num_irqs(%u) <= %u\n", 
				irq_new->num_irqs, PCI_MSIX_VEC_MAX);
			return -EINVAL;
		}
		/* Check if the card Supports MSIX capability */
		if (check_cntlr_cap(pdev, NVME_INT_MSIX, &msi_offset) < 0) {
			dnvme_err("Controller does not support for MSI-X capability!!");
			return -EINVAL;
		}
		/* if msix exists then update the msix pointer for this device */
		if (update_msixptr(ctx, msi_offset, pmsix_tbl_info) < 0) {
			return -EINVAL;
		}
		/* compute MSI-X MXC offset if MSI-X is supported */
		msi_offset += 2;
		/* Read MSIX-MXC value */
		pci_read_config_word(pdev, msi_offset, &mc_val);
		pmsix_tbl_info->ts = (mc_val & PCI_MSIX_FLAGS_QSIZE);
		/* check if Table size of MSIXCAP supports requested irqs.
		* as TS is 0 based and num_irq is 1 based, so we add 1 */
		if (irq_new->num_irqs > (pmsix_tbl_info->ts + 1)) {
			dnvme_err("IRQs = %d exceed MSI-X table size = %d", 
				irq_new->num_irqs, pmsix_tbl_info->ts);
			/* does not support the requested irq's*/
			return -EINVAL;
		} /* if table size */
		break;
	case NVME_INT_NONE: /* NVME_INT_NONE validation always returns success */
		/* no validation for NVME_INT_NONE schemes return success. */
		break;
	default:
		/* invalidate other type of IRQ schemes */
		dnvme_err("No validation for default case..");
		ret_val = -EINVAL;
		break;
	}
	return ret_val;
}

// 2021/05/15 meng_yu https://github.com/nvmecompliance/dnvme/pull/11
#if 0
/**
 * Calls a pci_enable_msi function.  Support for pci_enable_msi_block was
 * dropped in 3.16 in favor of pci_enable_msi_range which was implemented in
 * 3.13.x kernels (but not 3.13.0).  Calls pci_enable_msi_block for 3.13.x
 * kernels and earlier or otherwise calls pci_enable_msi_range.
 * @param dev the pci device structure
 * @param nvec the number of interrupt vectors to allocate
 * @return 0 on success, <0 on error, >0 if fewer than nvec interrupts could
 * be allocated.
 */
static int dnvme_pci_enable_msi(struct pci_dev * dev, unsigned int nvec)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,14,0)
    return pci_enable_msi_block(dev, nvec);
#else 
    int ret;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,11,0)
    ret = pci_enable_msi_range(dev, nvec, nvec);
#else
    ret = pci_enable_msi(dev);
#endif
    if (ret < nvec)
        return ret;
    else
        return 0;
#endif
}

static int dnvme_pci_enable_msi(struct pci_dev * dev, unsigned int nvec)
{
	int ret;

	ret = pci_alloc_irq_vectors(dev, nvec, nvec, PCI_IRQ_MSI);

	if(ret < nvec)
		return ret;
	else
		return 0;
}
#endif


/*
 * Update MSIX pointer in the irq process structure.
 */
static int update_msixptr(struct nvme_context *ctx,
    u16 offset, struct msix_info *pmsix_tbl_info)
{
	u8 __iomem *msix_ptr = NULL;
	u8 __iomem *pba_ptr = NULL;
	u32 msix_mtab;          /* MSIXCAP.MTAB register */
	u32 msix_to;            /* MSIXCAP.MTAB.TO field */
	u32 msix_tbir;          /* MSIXCAP.MTAB.TBIR field */
	u32 msix_mpba;          /* MSIXCAP.MPBA register */
	u32 msix_pbir;          /* MSIXCAP.MPBA.PBIR field */
	u32 msix_pbao;          /* MSIXCAP.MPBA.PBAO field */
	struct nvme_device *dev = ctx->dev;
	struct pci_dev *pdev = dev->priv.pdev;


	/* Compute & read offset for MSIXCAP.MTAB register */
	offset += 0x4;
	pci_read_config_dword(pdev, offset, &msix_mtab);
	msix_tbir = (msix_mtab & PCI_MSIX_TABLE_BIR);
	msix_to =   (msix_mtab & ~PCI_MSIX_TABLE_BIR);

	/* Compute & read offset for MSIXCAP.MPBA register */
	offset += 0x4;
	pci_read_config_dword(pdev, offset, &msix_mpba);
	msix_pbir = (msix_mpba & PCI_MSIX_PBA_BIR);
	msix_pbao = (msix_mpba & ~PCI_MSIX_PBA_BIR);

	switch (msix_tbir) {
	case 0x00:  /* BAR0 (64-bit) */
		msix_ptr = (dev->priv.bar0 + msix_to);
		break;
	case 0x04:  /* BAR2 (64-bit) */
		if (dev->priv.bar2 == NULL) {
			dnvme_err("BAR2 not implemented by DUT");
			return -EINVAL;
		}
		msix_ptr = (dev->priv.bar2 + msix_to);
		break;
	case 0x05:
		dnvme_err("BAR5 not supported, implies 32-bit, TBIR requiring 64-bit");
		return -EINVAL;
	default:
		dnvme_err("BAR? not supported, check value in MSIXCAP.MTAB.TBIR");
		return -EINVAL;
	}

	switch (msix_pbir) {
	case 0x00:  /* BAR0 (64-bit) */
		pba_ptr = (dev->priv.bar0 + msix_pbao);
		break;
	case 0x04:  /* BAR2 (64-bit) */
		if (dev->priv.bar2 == NULL) {
			dnvme_err("BAR2 not implemented by DUT");
			return -EINVAL;
		}
		pba_ptr = (dev->priv.bar2 + msix_pbao);
		break;
	case 0x05:
		dnvme_err("BAR5 not supported, implies 32-bit, MPBA requiring 64-bit");
		return -EINVAL;
	default:
		dnvme_err("BAR? not supported, check value in MSIXCAP.MPBA.PBIR");
		return -EINVAL;
	}

	/* Update the msix pointer in the device metrics */
	ctx->irq_set.mask_ptr = msix_ptr;
	pmsix_tbl_info->msix_tbl = msix_ptr;
	pmsix_tbl_info->pba_tbl = pba_ptr;
	return 0;
}

/*
 * dnvme_inquiry_cqe_with_isr - will process reap inquiry for the given cq using irq_vec
 * and isr_fired flags from two nodes, public cq node and nvme_irq list node.
 * NOTE: This function should be called with irq mutex locked otherwise it
 * will error out.
 */
int dnvme_inquiry_cqe_with_isr(struct nvme_cq *cq, u32 *num_remaining, u32 *isr_count)
{
	u16 irq_no = cq->pub.irq_no; /* irq_no for CQ   */
	struct nvme_irq *irq;
	struct nvme_context *ctx = cq->ctx;

#if IS_ENABLED(CONFIG_DNVME_CHECK_MUTEX_LOCK)
	if (!mutex_is_locked(&ctx->irq_set.mtx_lock)) {
		dnvme_err("Mutex should have been locked before this...\n");
		return -EPERM;
	}
#endif

	/* Get the Irq node for given irq vector */
	irq = find_irq_node_by_id(&ctx->irq_set, irq_no);
	if (irq == NULL) {
		dnvme_err("Node for IRQ No = %d does not exist in IRQ list!", irq_no);
		return -EINVAL;
	}

	/* Check if ISR is really fired for this CQ */
	if (irq->isr_fired != 0) {
		/* process reap inquiry for isr fired case */
		*num_remaining = dnvme_get_cqe_remain(cq, &ctx->dev->priv.pdev->dev);
	} else {
		/* To deal with ISR's aggregation, not supposed to notify CE's yet */
		*num_remaining = 0;
	}

	/* return the isr_count flag */
	*isr_count = irq->isr_count;
	return 0;
}

/**
 * @brief Loop through all CQ's associated with irq_no and check whehter
 *  they are empty and if empty reset the isr_flag for that particular irq_no
 */
int dnvme_reset_isr_flag(struct nvme_context *ctx, u16 irq_no)
{
	struct pci_dev *pdev = ctx->dev->priv.pdev;
	struct nvme_irq *irq;
	struct nvme_icq *icq;
	struct nvme_cq *cq;
	u32 remain = 0;

	irq = find_irq_node_by_id(&ctx->irq_set, irq_no);
	if (!irq) {
		dnvme_err("failed to find irq node(ID:%u)!\n", irq_no);
		return -EINVAL;
	}

	list_for_each_entry(icq, &irq->icq_list, entry) {
		cq = dnvme_find_cq(ctx, icq->cq_id);
		if (!cq) {
			dnvme_err("CQ ID = %d not found", icq->cq_id);
			return -EBADSLT;
		}

		remain = dnvme_get_cqe_remain(cq, &pdev->dev);
		if (remain)
			break;
	}

	/* reset the isr flag */
	if (remain == 0) {
		irq->isr_fired = 0;
	}
	return 0;
}

irqreturn_t dnvme_interrupt(int int_vec, void *data)
{
	struct nvme_irq_set *irq_set = (struct  nvme_irq_set *)data;
	struct nvme_work *wk_node;

	wk_node = find_work_node_by_vec(irq_set, int_vec);
	if (!wk_node) {
		dnvme_err("spurious irq with int_vec(%d)", int_vec);
		return IRQ_NONE;
	}

	/* To resolve contention between ISR's getting fired on different cores */
	spin_lock(&irq_set->spin_lock);
	dnvme_vdbg("TH:IRQNO = %d is serviced", wk_node->irq_no);
	/* Mask the interrupts which was fired till BH */
	//MengYu add. if tests interrupt, should commit this line
	dnvme_mask_interrupt(irq_set, wk_node->irq_id);

	if (queue_work(irq_set->wq, &wk_node->work) == 0)
		dnvme_vdbg("work node(%u:%u) already in queue!\n",
			wk_node->irq_id, wk_node->int_vec);

	spin_unlock(&irq_set->spin_lock);
	return IRQ_HANDLED;
}
