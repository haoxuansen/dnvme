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
	void __iomem *bar0 = ctx->dev->bar0;

	switch (irq->irq_type) {
	case NVME_INT_PIN:
	case NVME_INT_MSI_SINGLE:
	case NVME_INT_MSI_MULTI:
		set_int_mask(bar0, 0x1 << irq_no);
		break;

	case NVME_INT_MSIX:
		set_msix_single_mask(irq->msix.tb, irq_no);
		break;

	case NVME_INT_NONE:
		break;

	default:
		dnvme_err(ctx->dev, "irq type(%u) is unknown!\n", irq->irq_type);
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
	void __iomem *bar0 = ctx->dev->bar0;

	switch (irq->irq_type) {
	case NVME_INT_PIN:
	case NVME_INT_MSI_SINGLE:
	case NVME_INT_MSI_MULTI:
		clear_int_mask(bar0, 0x1 << irq_no);
		break;

	case NVME_INT_MSIX:
		clear_msix_single_mask(irq->msix.tb, irq_no);
		break;

	case NVME_INT_NONE:
		break;

	default:
		dnvme_err(ctx->dev, "irq type(%u) is unknown!\n", irq->irq_type);
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
		pr_warn("icq(%u) already exist in irq(%u) list!\n", 
			cq_id, irq->irq_id);
		return 0;
	}

	icq = kmalloc(sizeof(*icq), GFP_KERNEL);
	if (!icq) {
		pr_err("failed to alloc icq node!\n");
		return -ENOMEM;
	}
	icq->cq_id = cq_id;

	list_add_tail(&icq->entry, &irq->icq_list);
	return 0;
}

int dnvme_create_icq_node(struct nvme_irq_set *irq_set, u16 cq_id, u16 irq_no)
{
	struct nvme_context *ctx = dnvme_irq_to_context(irq_set);
	int ret;
	struct nvme_irq *irq;

	irq = find_irq_node_by_id(irq_set, irq_no);
	if (!irq) {
		dnvme_err(ctx->dev, "failed to find irq node %u!\n", irq_no);
		return -EINVAL;
	}

	ret = create_icq_node(irq, cq_id);
	if (ret < 0)
		return ret;

	return 0;
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
	struct nvme_context *ctx = dnvme_irq_to_context(irq_set);
	struct nvme_device *ndev = ctx->dev;
	struct nvme_irq *irq;

	irq = find_irq_node_by_id(irq_set, irq_id);
	if (irq) {
		if (irq->int_vec == int_vec) {
			dnvme_warn(ndev, "irq node(%u:%u) already exist! skip\n",
				irq_id, int_vec);
			return 0;
		}
		dnvme_err(ndev, "irq node already exist! int_vec mismatch!\n");
		return -EINVAL;
	}

	irq = find_irq_node_by_vec(irq_set, int_vec);
	if (irq) {
		dnvme_err(ndev, "irq node already exist! irq_id mismatch!\n");
		return -EINVAL;
	}

	irq = kzalloc(sizeof(*irq), GFP_KERNEL);
	if (irq == NULL) {
		dnvme_err(ndev, "failed to alloc irq node!\n");
		return -ENOMEM;
	}

	irq->int_vec = int_vec; /* int vector number   */
	irq->irq_id = irq_id;
	atomic_set(&irq->isr_fired, 0);
	atomic_set(&irq->isr_count, 0);
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
	struct nvme_context *ctx = dnvme_irq_to_context(irq_set);
	struct nvme_irq *node;

	node = find_irq_node_by_id(irq_set, irq_id);
	if (!node) {
		dnvme_err(ctx->dev, "irq node(ID:%u) not found!\n", irq_id);
		return;
	}

	atomic_set(&node->isr_fired, 1);
	atomic_add(1, &node->isr_count);

	dnvme_vdbg(ctx->dev, "irq node(ID:%u) count is %u\n", node->irq_id, 
		atomic_read(&node->isr_count));
}

/**
 * @brief ISR bottom half
 */
static void update_irq_flag(struct work_struct *wk)
{
	struct nvme_work *pwork = container_of(wk, struct nvme_work, work);

	inc_isr_count(pwork->irq_set, pwork->irq_id);
}

/**
 * @brief Create a work node and add to list.
 * 
 * @return 0 on success, otherwise a negative errno.
 */
static int create_work_node(struct nvme_irq_set *irq_set, u32 int_vec, 
	u16 irq_id)
{
	struct nvme_context *ctx = dnvme_irq_to_context(irq_set);
	struct nvme_device *ndev = ctx->dev;
	struct nvme_work *node;

	/* Sanity check */
	node = find_work_node_by_id(irq_set, irq_id);
	if (node) {
		if (node->int_vec == int_vec) {
			dnvme_warn(ndev, "work node(%u:%u) already exist! skip\n", 
				irq_id, int_vec);
			return 0;
		}
		dnvme_err(ndev, "work node already exist! int_vec mismatch!\n");
		return -EINVAL;
	}

	node = find_work_node_by_vec(irq_set, int_vec);
	if (node) {
		dnvme_err(ndev, "work node already exist! irq_id mismatch!\n");
		return -EINVAL;
	}

	node = kzalloc(sizeof(struct nvme_work), GFP_KERNEL);
	if (!node) {
		dnvme_err(ndev, "failed to alloc work node!\n");
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
	struct nvme_context *ctx = dnvme_irq_to_context(irq_set);

	if (irq_set->wq) {
		dnvme_warn(ctx->dev, "work queue already exist! skip\n");
		return 0;
	}

	irq_set->wq = create_workqueue("irq_wq");
	if (!irq_set->wq) {
		dnvme_err(ctx->dev, "failed to create work queue!\n");
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
	struct pci_dev *pdev = ndev->pdev;
	void __iomem *bar0 = ndev->bar0;
	int ret;

	ret = request_irq(pdev->irq, dnvme_interrupt, IRQF_SHARED, "pin-base",
		&ctx->irq_set);
	if (ret < 0) {
		dnvme_err(ndev, "failed to request irq(%u)!\n", pdev->irq);
		return ret;
	}
	dnvme_vdbg(ndev, "Pin-Base Interrupt Vector(%u)\n", pdev->irq);

	ret = create_irq_work_pair(&ctx->irq_set, pdev->irq, 0);
	if (ret < 0) {
		dnvme_err(ndev, "failed to create irq work pair!\n");
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
	struct pci_dev *pdev = ndev->pdev;
	void __iomem *bar0 = ndev->bar0;
	int ret;

	clear_int_mask(bar0, UINT_MAX);

	/*
	 * Allocate one interrupt to this device. A successful call will switch the
	 * device to msi single mode and pdev->irq is changed to a new number which
	 * represents msi interrupt vector consequently.
	 */
	ret = pci_enable_msi(pdev);
	if (ret) {
		dnvme_err(ndev, "Can't enable MSI-single!\n");
		goto out;
	}

	ret = request_irq(pdev->irq, dnvme_interrupt, IRQF_SHARED,
		"msi-single", &ctx->irq_set);
	if (ret < 0) {
		dnvme_err(ndev, "failed to request irq(%u)!\n", pdev->irq);
		goto out2;
	}
	dnvme_vdbg(ndev, "MSI-Single Interrupt Vector(%u)\n", pdev->irq);

	ret = create_irq_work_pair(&ctx->irq_set, pdev->irq, 0);
	if (ret < 0) {
		dnvme_err(ndev, "failed to create irq work pair!\n");
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
	struct pci_dev *pdev = ndev->pdev;
	void __iomem *bar0 = ndev->bar0;
	int ret, i, j;

	clear_int_mask(bar0, UINT_MAX);

	/*
	 * Enable msi-block interrupts for this device. The pdev->irq will
	 * be the lowest of the new interrupts assigned to this device.
	 */
	ret = pci_alloc_irq_vectors(pdev, num_irqs, num_irqs, PCI_IRQ_MSI);
	if (ret != num_irqs) {
		dnvme_err(ndev, "Can't enable MSI-Multi with num_irq=%u\n", num_irqs);
		goto out;
	}

	/* Request irq on each interrupt vector */
	for (i = 0; i < num_irqs; i++) {
		ret = request_irq(pdev->irq + i, dnvme_interrupt, IRQF_SHARED,
			"msi-multi", &ctx->irq_set);
		if (ret < 0) {
			dnvme_err(ndev, "failed to request irq(%u)!\n", pdev->irq + i);
			goto out2;
		}
	}

	for (j = 0; j < num_irqs; j++) {
		ret = create_irq_work_pair(&ctx->irq_set, pdev->irq + j, j);
		if (ret < 0) {
			dnvme_err(ndev, "failed to create irq work pair(%d)!\n", j);
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
static int set_int_msix(struct nvme_context *ctx, u16 num_irqs)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_irq_set *irq = &ctx->irq_set;
	struct pci_dev *pdev = ndev->pdev;
	struct msix_entry *entries;
	int ret, i, j;

	entries = kcalloc(num_irqs, sizeof(*entries), GFP_KERNEL);
	if (!entries) {
		dnvme_err(ndev, "failed to alloc msix_entry!\n");
		return -ENOMEM;
	}

	for (i = 0; i < num_irqs; i++)
		entries[i].entry = i;

	ret = pci_enable_msix_range(pdev, entries, num_irqs, num_irqs);
	if (ret != num_irqs) {
		dnvme_err(ndev, "failed to enable msix!(%d)\n", ret);
		ret = -EPERM;
		goto out;
	}

	for (i = 0; i < num_irqs; i++) {
		ret = request_irq(entries[i].vector, dnvme_interrupt, 
			IRQF_SHARED, "msi-x", &ctx->irq_set);
		if (ret < 0) {
			dnvme_err(ndev, "failed to request vec(%u)!\n", 
				entries[i].vector);
			goto out2;
		}
	}

	for (j = 0; j < num_irqs; j++) {
		ret = create_irq_work_pair(&ctx->irq_set, entries[j].vector, 
			entries[j].entry);
		if (ret < 0) {
			dnvme_err(ndev, "failed to create irq work pair(%u)!\n", 
				entries[j].entry);
			goto out3;
		}
	}

	if (pba_bits_is_set(irq->msix.pba, entries, num_irqs)) {
		dnvme_err(ndev, "PBA bit is set at IRQ init, shall set none!\n");
		ret = -EINVAL;
		goto out3;
	}

	set_msix_mask(irq->msix.tb, entries, num_irqs);

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

static int init_msix_ptr(struct nvme_context *ctx, struct pci_cap_msix *cap)
{
	struct nvme_device *ndev = ctx->dev;
	struct nvme_irq_set *irq_set = &ctx->irq_set;
	u32 msix_to; /* MSIXCAP.MTAB.TO */
	u32 msix_tbir; /* MSIXCAP.MTAB.TBIR */
	u32 msix_pbir; /* MSIXCAP.MPBA.PBIR */
	u32 msix_pbao; /* MSIXCAP.MPBA.PBAO */

	if (irq_set->msix.irqs)
		return 0; /* already initialized */

	/* Table Size is 0's based */
	irq_set->msix.irqs = (cap->mc & PCI_MSIX_FLAGS_QSIZE) + 1;

	msix_tbir = cap->table & PCI_MSIX_TABLE_BIR;
	msix_to = cap->table & PCI_MSIX_TABLE_OFFSET;
	msix_pbir = cap->pba & PCI_MSIX_PBA_BIR;
	msix_pbao = cap->pba & PCI_MSIX_PBA_OFFSET;

	switch (msix_tbir) {
	case 0x00:  /* BAR0 (64-bit) */
		irq_set->msix.tb = ndev->bar0 + msix_to;
		break;
	default:
		dnvme_err(ndev, "BAR%u is not supported!\n", msix_tbir);
		return -EINVAL;
	}

	switch (msix_pbir) {
	case 0x00:  /* BAR0 (64-bit) */
		irq_set->msix.pba = ndev->bar0 + msix_pbao;
		break;

	default:
		dnvme_err(ndev, "BAR%u is not supported!\n", msix_pbir);
		return -EINVAL;
	}
	
	return 0;
}

/**
 * @brief Check whether nvme_interrupt is prperly populated
 * 
 * @param ctx NVMe device context
 * @return 0 on success, otherwise a negative errno.
 */
static int check_interrupt(struct nvme_context *ctx, struct nvme_interrupt *irq)
{
	int ret = 0;
	struct nvme_device *ndev = ctx->dev;
	struct nvme_capability *cap = &ndev->cap;
	struct nvme_irq_set *irq_set = &ctx->irq_set;
	struct pci_dev *pdev = ctx->dev->pdev;
	void __iomem *bar0 = ndev->bar0;
	u32 offset;
	u16 mc; /* Message Control */

	if (dnvme_readl(bar0, NVME_REG_CC) & NVME_CC_ENABLE) {
		dnvme_err(ndev, "CC.EN is set! Please set irq before CC.EN set!\n");
		return -EINVAL;
	}

	switch (irq->irq_type) {
	case NVME_INT_MSI_SINGLE:
		if (irq->num_irqs != 1) {
			dnvme_err(ndev, "MSI Single: num_irqs(%u) = 1\n", irq->num_irqs);
			return -EINVAL;
		}

		if (!cap->msi || !cap->msi->offset) {
			dnvme_err(ndev, "Not support MSI capability!\n");
			return -EINVAL;
		}
		break;

	case NVME_INT_MSI_MULTI:
		if (irq->num_irqs > PCI_MSI_VEC_MAX || irq->num_irqs == 0) {
			dnvme_err(ndev, "MSI Multi: num_irqs(%u) <= %u\n", 
				irq->num_irqs, PCI_MSI_VEC_MAX);
			return -EINVAL;
		}

		if (!cap->msi || !cap->msi->offset) {
			dnvme_err(ndev, "Not support MSI capability!\n");
			return -EINVAL;
		}
		offset = cap->msi->offset;

		ret = pci_msi_read_mc(pdev, offset, &mc);
		if (ret < 0) {
			dnvme_err(ndev, "failed to read msi mc!(%d)\n", ret);
			return ret;
		}

		if(irq->num_irqs > (1 << ((mc & PCI_MSI_FLAGS_QMASK) >> 1))) { 
			dnvme_err(ndev, "IRQs(%u) > MSI MMC(%u)\n", irq->num_irqs,
				(1 << ((mc & PCI_MSI_FLAGS_QMASK) >> 1)));
			return -EINVAL;
		}
		break;

	case NVME_INT_MSIX:
		if (irq->num_irqs > PCI_MSIX_VEC_MAX || irq->num_irqs == 0) {
			dnvme_err(ndev, "MSI-X: num_irqs(%u) <= %u\n", 
				irq->num_irqs, PCI_MSIX_VEC_MAX);
			return -EINVAL;
		}

		if (!cap->msix || !cap->msix->offset) {
			dnvme_err(ndev, "Not support MSI-X capability!\n");
			return -EINVAL;
		}

		ret = init_msix_ptr(ctx, cap->msix);
		if (ret < 0)
			return ret;
		
		if (irq->num_irqs > irq_set->msix.irqs) {
			dnvme_err(ndev, "IRQ(%u) > MSI-X irqs(%u)\n", 
				irq->num_irqs, irq_set->msix.irqs);
			return -EINVAL;
		}
		break;

	case NVME_INT_PIN:
	case NVME_INT_NONE:
		/* nothing required to do */
		break;
	default:
		dnvme_err(ndev, "irq_type(%d) is unknonw!\n", irq->irq_type);
		return -EINVAL;
	}
	return 0;
}


void dnvme_clean_interrupt(struct nvme_context *ctx)
{
	struct nvme_irq *irq;
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ndev->pdev;
	enum nvme_irq_type irq_type = ndev->pub.irq_active.irq_type;

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
	struct nvme_device *ndev = ctx->dev;
	struct nvme_irq_set *irq_set = &ctx->irq_set;
	struct nvme_interrupt irq;
	enum nvme_irq_type act_irq = ndev->pub.irq_active.irq_type;
	int ret;

	if (copy_from_user(&irq, uirq, sizeof(irq))) {
		dnvme_err(ndev, "failed to copy from user space!\n");
		return -EFAULT;
	}
	dnvme_vdbg(ndev, "Set irq_type:%d, num_irqs:%u\n", irq.irq_type, irq.num_irqs);

	ret = check_interrupt(ctx, &irq);
	if (ret < 0) {
		dnvme_err(ndev, "Invalid inputs set or device is not disabled");
		return ret;
	}

	if (act_irq != NVME_INT_NONE)
		dnvme_clean_interrupt(ctx);

	ret = create_irq_work_queue(irq_set);
	if (ret < 0) {
		dnvme_err(ndev, "Failed to initialize resources for work queue/items");
		return ret;
	}

	switch (irq.irq_type) {
	case NVME_INT_MSI_SINGLE:
		ret = set_int_msi_single(ctx);
		break;
	case NVME_INT_MSI_MULTI:
		ret = set_int_msi_multi(ctx, irq.num_irqs);
		break;
	case NVME_INT_MSIX:
		ret = set_int_msix(ctx, irq.num_irqs);
		break;
	case NVME_INT_PIN:
		ret = set_int_pin(ctx);
		break;
	case NVME_INT_NONE:
		break;
	default:
		dnvme_err(ndev, "irq_type(%d) is unknown!\n", irq.irq_type);
		ret = -EBADRQC;
		break;
	}

	if (ret < 0) {
		dnvme_err(ndev, "failed to set irq_type:%d!\n",irq.irq_type);
		goto out_destroy_wq;
	}

	/* update active irq info */
	ndev->pub.irq_active.irq_type = irq.irq_type;
	ndev->pub.irq_active.num_irqs = irq.num_irqs;
	irq_set->irq_type = irq.irq_type;

	return 0;

out_destroy_wq:
	destroy_irq_work_queue(irq_set);
	return ret;
}

/**
 * @brief Loop through all CQ's associated with irq_no and check whehter
 *  they are empty and if empty reset the isr_flag for that particular irq_no
 */
int dnvme_reset_isr_flag(struct nvme_context *ctx, u16 irq_no)
{
	struct nvme_device *ndev = ctx->dev;
	struct pci_dev *pdev = ctx->dev->pdev;
	struct nvme_irq *irq;
	struct nvme_icq *icq;
	struct nvme_cq *cq;
	u32 remain = 0;

	irq = find_irq_node_by_id(&ctx->irq_set, irq_no);
	if (!irq) {
		dnvme_err(ndev, "failed to find irq node(ID:%u)!\n", irq_no);
		return -EINVAL;
	}

	list_for_each_entry(icq, &irq->icq_list, entry) {
		cq = dnvme_find_cq(ndev, icq->cq_id);
		if (!cq) {
			dnvme_err(ndev, "CQ ID = %d not found", icq->cq_id);
			return -EBADSLT;
		}

		remain = dnvme_get_cqe_remain(cq, &pdev->dev);
		if (remain)
			break;
	}

	/* reset the isr flag */
	if (remain == 0) {
		atomic_set(&irq->isr_fired, 0);
	}
	return 0;
}

irqreturn_t dnvme_interrupt(int int_vec, void *data)
{
	struct nvme_irq_set *irq_set = (struct nvme_irq_set *)data;
	struct nvme_context *ctx = dnvme_irq_to_context(irq_set);
	struct nvme_device *ndev = ctx->dev;
	struct nvme_work *wk_node;

	wk_node = find_work_node_by_vec(irq_set, int_vec);
	if (!wk_node) {
		dnvme_err(ndev, "spurious irq with int_vec(%d)", int_vec);
		return IRQ_NONE;
	}

	/* To resolve contention between ISR's getting fired on different cores */
	spin_lock(&irq_set->spin_lock);
	dnvme_vdbg(ndev, "TH:IRQNO = %d is serviced", wk_node->irq_id);

	/* Mask this interrupt until we have reaped all CQ entries */
	dnvme_mask_interrupt(irq_set, wk_node->irq_id);

	if (queue_work(irq_set->wq, &wk_node->work) == 0)
		dnvme_vdbg(ndev, "work node(%u:%u) already in queue!\n",
			wk_node->irq_id, wk_node->int_vec);

	spin_unlock(&irq_set->spin_lock);
	return IRQ_HANDLED;
}

