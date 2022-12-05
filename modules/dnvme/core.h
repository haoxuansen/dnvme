/**
 * @file core.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-11-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef _DNVME_CORE_H_
#define _DNVME_CORE_H_

#include <linux/limits.h>

#include "dnvme_ds.h"
#include "dnvme_ioctl.h"

#define NVME_ASQ_ENTRY_MAX		4096
#define NVME_ACQ_ENTRY_MAX		4096

#define NVME_SQ_ID_MAX			U16_MAX
#define NVME_CQ_ID_MAX			U16_MAX
#define NVME_META_ID_MAX		((0x1 << 18) - 1)

#define NVME_META_BUF_ALIGN		4

#define NVME_PRP_ENTRY_SIZE		8 /* in bytes */

#undef pr_fmt
#define pr_fmt(fmt)			"[%s,%d]" fmt, __func__, __LINE__

#if !IS_ENABLED(CONFIG_PRINTK_COLOR)
#include "log/color.h"

#define dnvme_err(fmt, ...) \
	pr_err(LOG_COLOR_RED fmt, ##__VA_ARGS__)
#define dnvme_warn(fmt, ...) \
	pr_warn(LOG_COLOR_YELLOW fmt, ##__VA_ARGS__)
#define dnvme_notice(fmt, ...) \
	pr_notice(LOG_COLOR_BLUE fmt, ##__VA_ARGS__)
#define dnvme_info(fmt, ...) \
	pr_info(LOG_COLOR_GREEN fmt, ##__VA_ARGS__)
#define dnvme_dbg(fmt, ...) \
	pr_debug(LOG_COLOR_NONE fmt, ##__VA_ARGS__)
#ifdef VERBOSE_DEBUG
#define dnvme_vdbg(fmt, ...) \
	pr_debug(LOG_COLOR_NONE fmt, ##__VA_ARGS__)
#else
#define dnvme_vdbg(fmt, ...)
#endif /* !VERBOSE_DEBUG */
#endif

static inline struct nvme_context *dnvme_irq_to_context(struct nvme_irq_set *irq_set)
{
	return container_of(irq_set, struct nvme_context, irq_set);
}

void dnvme_cleanup_context(struct nvme_context *ctx, enum nvme_state state);

#endif /* !_DNVME_CORE_H_ */
