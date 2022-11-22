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

#define NVME_SQ_ID_MAX			U16_MAX
#define NVME_CQ_ID_MAX			U16_MAX
#define NVME_META_ID_MAX		((0x1 << 18) - 1)

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
#define dnvme_debug(fmt, ...) \
	pr_debug(LOG_COLOR_NONE fmt, ##__VA_ARGS__)
#endif

#endif /* !_DNVME_CORE_H_ */
