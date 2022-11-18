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

static inline int dnvme_readb(void __iomem *base, u32 oft, u8 *val)
{
	*val = readb(base + oft);
	return 0;
}

static inline int dnvme_writeb(void __iomem *base, u32 oft, u8 val)
{
	writeb(val, base + oft);
	return 0;
}

static inline int dnvme_readw(void __iomem *base, u32 oft, u16 *val)
{
	*val = readw(base + oft);
	return 0;
}

static inline int dnvme_writew(void __iomem *base, u32 oft, u16 val)
{
	writew(val, base + oft);
	return 0;
}

static inline int dnvme_readl(void __iomem *base, u32 oft, u32 *val)
{
	*val = readl(base + oft);
	return 0;
}

static inline int dnvme_writel(void __iomem *base, u32 oft, u32 val)
{
	writel(val, base + oft);
	return 0;
}

static inline int dnvme_readq(void __iomem *base, u32 oft, u64 *val)
{
	*val = lo_hi_readq(base + oft);
	return 0;
}

static inline int dnvme_writeq(void __iomem *base, u32 oft, u64 val)
{
	lo_hi_writeq(val, base + oft);
	return 0;
}

int dnvme_read_from_config(struct pci_dev *pdev, struct nvme_access *access, 
	void *buf);
int dnvme_write_to_config(struct pci_dev *pdev, struct nvme_access *access, 
	void *buf);

int dnvme_read_from_bar(void __iomem *bar, struct nvme_access *access, void *buf);
int dnvme_write_to_bar(void __iomem *bar, struct nvme_access *access, void *buf);

#endif /* !_DNVME_IO_H_ */
