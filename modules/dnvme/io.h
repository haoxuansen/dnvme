/**
 * @file io.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-11-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _DNVME_IO_H_
#define _DNVME_IO_H_

#include <linux/io.h>
#include <linux/io-64-nonatomic-lo-hi.h>

struct pci_dev;
struct nvme_access;

static inline u8 dnvme_readb(void __iomem *base, u32 oft)
{
	return readb(base + oft);
}

static inline void dnvme_writeb(void __iomem *base, u32 oft, u8 val)
{
	writeb(val, base + oft);
}

static inline u16 dnvme_readw(void __iomem *base, u32 oft)
{
	return readw(base + oft);
}

static inline void dnvme_writew(void __iomem *base, u32 oft, u16 val)
{
	writew(val, base + oft);
}

static inline u32 dnvme_readl(void __iomem *base, u32 oft)
{
	return readl(base + oft);
}

static inline void dnvme_writel(void __iomem *base, u32 oft, u32 val)
{
	writel(val, base + oft);
}

static inline u64 dnvme_readq(void __iomem *base, u32 oft)
{
	return lo_hi_readq(base + oft);
}

static inline void dnvme_writeq(void __iomem *base, u32 oft, u64 val)
{
	lo_hi_writeq(val, base + oft);
}

int dnvme_read_from_config(struct pci_dev *pdev, struct nvme_access *access, 
	void *buf);
int dnvme_write_to_config(struct pci_dev *pdev, struct nvme_access *access, 
	void *buf);

int dnvme_read_from_bar(void __iomem *bar, struct nvme_access *access, void *buf);
int dnvme_write_to_bar(void __iomem *bar, struct nvme_access *access, void *buf);

#endif /* !_DNVME_IO_H_ */
