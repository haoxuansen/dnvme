/**
 * @file io.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-11-17
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>

#include "io.h"
#include "core.h"
#include "dnvme_ioctl.h"

/**
 * @brief Read data from PCI config space.
 * @attention Before calling this function, you must ensure that the address
 *  and lengths accessed are aligned.
 * 
 * @param pdev PCI device
 * @param access Access request information
 * @return 0 on success, otherwise a negative errno.
 */
int dnvme_read_from_config(struct pci_dev *pdev, struct nvme_access *access, 
	void *buf)
{
	struct device *dev = &pdev->dev;
	int ret;
	u32 idx = 0;

	if ((access->offset + access->bytes) > PCI_CFG_SPACE_EXP_SIZE) {
		dev_err(dev, "Access region(%u+%u) out of PCI Config Space(%u)!\n",
			access->offset, access->bytes, PCI_CFG_SPACE_EXP_SIZE);
		return -EINVAL;
	}

	do {
		switch (access->type) {
		case NVME_ACCESS_DWORD:
			ret= pci_read_config_dword(pdev, access->offset + idx, 
				buf + idx);
			dev_dbg(dev, "READ CFG offset:0x%08x data:0x%08x\n",
				(access->offset + idx), *(u32 *)(buf + idx));
			idx += 4;
			break;
		
		case NVME_ACCESS_WORD:
			ret = pci_read_config_word(pdev, access->offset + idx, 
				buf + idx);
			dev_dbg(dev, "READ CFG offset:0x%08x data:0x%08x\n",
				(access->offset + idx), *(u16 *)(buf + idx));
			idx += 2;
			break;
		
		case NVME_ACCESS_BYTE:
			ret = pci_read_config_byte(pdev, access->offset + idx, 
				buf + idx);
			dev_dbg(dev, "READ CFG offset:0x%08x data:0x%08x\n",
				(access->offset + idx), *(u8 *)(buf + idx));
			idx++;
			break;
		
		default:
			dev_err(dev, "Access type(%u) is unknown!\n", access->type);
			return -EINVAL;
		}

		ret = pcibios_err_to_errno(ret);
		if (ret < 0) {
			dev_err(dev, "READ something err!(%d)\n", ret);
			return ret;
		}

	} while (idx < access->bytes);

	return 0;
}

/**
 * @brief Write data to PCI config space.
 * @attention Before calling this function, you must ensure that the address
 *  and lengths accessed are aligned.
 * 
 * @param pdev PCI device
 * @param access Access request information
 * @return 0 on success, otherwise a negative errno.
 */
int dnvme_write_to_config(struct pci_dev *pdev, struct nvme_access *access, 
	void *buf)
{
	struct device *dev = &pdev->dev;
	int ret;
	u32 idx = 0;

	if ((access->offset + access->bytes) > PCI_CFG_SPACE_EXP_SIZE) {
		dev_err(dev, "Access region(%u+%u) out of PCI Config Space(%u)!\n",
			access->offset, access->bytes, PCI_CFG_SPACE_EXP_SIZE);
		return -EINVAL;
	}

	do {
		switch (access->type) {
		case NVME_ACCESS_DWORD:
			ret = pci_write_config_dword(pdev, access->offset + idx, 
				*(u32 *)(buf + idx));
			dev_dbg(dev, "WRITE CFG offset:0x%08x data:0x%08x\n",
				(access->offset + idx), *(u32 *)(buf + idx));
			idx += 4;
			break;
		
		case NVME_ACCESS_WORD:
			ret = pci_write_config_word(pdev, access->offset + idx, 
				*(u16 *)(buf + idx));
			dev_dbg(dev, "WRITE CFG offset:0x%08x data:0x%08x\n",
				(access->offset + idx), *(u16 *)(buf + idx));
			idx += 2;
			break;
		
		case NVME_ACCESS_BYTE:
			ret = pci_write_config_byte(pdev, access->offset + idx, 
				*(u8 *)(buf + idx));
			dev_dbg(dev, "WRITE CFG offset:0x%08x data:0x%08x\n",
				(access->offset + idx), *(u8 *)(buf + idx));
			idx++;
			break;
		
		default:
			dev_err(dev, "Access type(%u) is unknown!\n", access->type);
			return -EINVAL;
		}

		ret = pcibios_err_to_errno(ret);
		if (ret < 0) {
			dev_err(dev, "WRITE something err!(%d)\n", ret);
			return ret;
		}

	} while (idx < access->bytes);

	return 0;
}

/**
 * @brief Read data from NVMe BAR space.
 * @attention Before calling this function, you must ensure that the address
 *  and lengths accessed are aligned.
 * 
 * @param access Access request information
 * @return 0 on success, otherwise a negative errno.
 */
int dnvme_read_from_bar(void __iomem *bar, struct nvme_access *access, void *buf)
{
	u32 idx = 0;

	/* !TODO: It's better to check whether the accessed region is out of
	 * range!
	 */

	do {
		switch (access->type) {
		case NVME_ACCESS_QWORD:
			*(u64 *)(buf + idx) = dnvme_readq(bar, access->offset + idx);
			idx += 8;
			break;

		case NVME_ACCESS_DWORD:
			*(u32 *)(buf + idx) = dnvme_readl(bar, access->offset + idx);
			idx += 4;
			break;
		
		case NVME_ACCESS_WORD:
			*(u16 *)(buf + idx) = dnvme_readw(bar, access->offset + idx);
			idx += 2;
			break;

		case NVME_ACCESS_BYTE:
			*(u8 *)(buf + idx) = dnvme_readb(bar, access->offset + idx);
			idx++;
			break;
		
		default:
			pr_err("Access type(%u) is unknown!\n", access->type);
			return -EINVAL;
		}
	} while (idx < access->bytes);

	return 0;
}

/**
 * @brief Write data to NVMe BAR space.
 * @attention Before calling this function, you must ensure that the address
 *  and lengths accessed are aligned.
 * 
 * @param access Access request information
 * @return 0 on success, otherwise a negative errno.
 */
int dnvme_write_to_bar(void __iomem *bar, struct nvme_access *access, void *buf)
{
	u32 idx = 0;

	/* !TODO: It's better to check whether the accessed region is out of
	 * range!
	 */

	do {
		switch (access->type) {
		case NVME_ACCESS_QWORD:
			dnvme_writeq(bar, access->offset + idx, *(u64 *)(buf + idx));
			idx += 8;
			break;

		case NVME_ACCESS_DWORD:
			dnvme_writel(bar, access->offset + idx, *(u32 *)(buf + idx));
			idx += 4;
			break;
		
		case NVME_ACCESS_WORD:
			dnvme_writew(bar, access->offset + idx, *(u16 *)(buf + idx));
			idx += 2;
			break;

		case NVME_ACCESS_BYTE:
			dnvme_writeb(bar, access->offset + idx, *(u8 *)(buf + idx));
			idx++;
			break;
		
		default:
			pr_err("Access type(%u) is unknown!\n", access->type);
			return -EINVAL;
		}
	} while (idx < access->bytes);

	return 0;
}
