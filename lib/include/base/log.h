/**
 * @file log.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-08-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _UAPI_LIB_BASE_LOG_H_
#define _UAPI_LIB_BASE_LOG_H_

#include <stdio.h> /* for "@printf" */
#include <stdint.h> /* for "uint8_t" */
#include <stdarg.h>
#include <assert.h> /* for "@assert" */
#include "color.h"

#define LOG_LEVEL_EMERG			0
#define LOG_LEVEL_ALERT			1
#define LOG_LEVEL_CRIT			2
#define LOG_LEVEL_ERR			3
#define LOG_LEVEL_WARN			4
#define LOG_LEVEL_NOTICE		5
#define LOG_LEVEL_INFO			6
#define LOG_LEVEL_DEBUG			7
#define LOG_LEVEL_BUBBLING		8
#define LOG_LEVEL_DIVING		9
#define LOG_LEVEL_DEFAULT		LOG_LEVEL_DEBUG

#ifndef pr_fmt
#define pr_fmt(fmt)			"[%s,%d]" fmt, __func__, __LINE__
// #define pr_fmt(fmt)			fmt
#endif

#define pr_emerg(fmt, ...) \
	log_print(LOG_LEVEL_EMERG, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_alert(fmt, ...) \
	log_print(LOG_LEVEL_ALERT, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_crit(fmt, ...) \
	log_print(LOG_LEVEL_CRIT, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err(fmt, ...) \
	log_print(LOG_LEVEL_ERR, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn(fmt, ...) \
	log_print(LOG_LEVEL_WARN, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_notice(fmt, ...) \
	log_print(LOG_LEVEL_NOTICE, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info(fmt, ...) \
	log_print(LOG_LEVEL_INFO, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_debug(fmt, ...) \
	log_print(LOG_LEVEL_DEBUG, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_bub(fmt, ...) \
	log_print(LOG_LEVEL_BUBBLING, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_div(fmt, ...) \
	log_print(LOG_LEVEL_DIVING, pr_fmt(fmt), ##__VA_ARGS__)

int log_print(uint8_t level, const char *fmt, ...);

#define pr_color(color, fmt, ...) \
	printf(LOG_N color fmt, ##__VA_ARGS__)

#define pr_red(fmt, ...) \
	printf(LOG_N LOG_B_RED fmt, ##__VA_ARGS__)
#define pr_green(fmt, ...) \
	printf(LOG_N LOG_B_GREEN fmt, ##__VA_ARGS__)
#define pr_yellow(fmt, ...) \
	printf(LOG_N LOG_B_YELLOW fmt, ##__VA_ARGS__)
#define pr_blue(fmt, ...) \
	printf(LOG_N LOG_B_BLUE fmt, ##__VA_ARGS__)
#define pr_cyan(fmt, ...) \
	printf(LOG_N LOG_B_CYAN fmt, ##__VA_ARGS__)
#define pr_white(fmt, ...) \
	printf(LOG_N LOG_B_WHITE fmt, ##__VA_ARGS__)

#define BUG_ON(cond)			assert(!(cond))
#define WARN_ON(cond) \
	((cond) ? pr_warn("%s:%d: %s: Assertion '!(%s)' failed.\n", __FILE__, \
		__LINE__, __func__, #cond) : 0)
#define DBG_ON(cond) \
	((cond) ? dump_stack(0) : 0)

#endif /* !_UAPI_LIB_BASE_LOG_H_ */
