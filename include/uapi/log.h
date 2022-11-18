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

#ifndef _UAPI_LOG_H_
#define _UAPI_LOG_H_

#include <stdarg.h>

#define LOG_COLOR_NONE			"\033[0m"
#define LOG_COLOR_BLACK			"\033[1;30m"
#define LOG_COLOR_DBLACK		"\033[0;30m"
#define LOG_COLOR_RED			"\033[1;31m"
#define LOG_COLOR_DRED			"\033[0;31m"
#define LOG_COLOR_GREEN			"\033[1;32m"
#define LOG_COLOR_DGREEN		"\033[0;32m"
#define LOG_COLOR_YELLOW		"\033[1;33m"
#define LOG_COLOR_DYELLOW		"\033[0;33m"
#define LOG_COLOR_BLUE			"\033[1;34m"
#define LOG_COLOR_DBLUE			"\033[0;34m"
#define LOG_COLOR_PURPLE		"\033[1;35m"
#define LOG_COLOR_DPURPLE		"\033[0;35m"
#define LOG_COLOR_CYAN			"\033[1;36m"
#define LOG_COLOR_DCYAN			"\033[0;36m"
#define LOG_COLOR_WHITE			"\033[1;37m"

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

#define pr_fmt(fmt)			"[%s,%d]" fmt, __func__, __LINE__
#ifndef pr_fmt
#define pr_fmt(fmt)			fmt
#endif

#define pr_emerg(fmt, ...) \
	do { \
		if (LOG_LEVEL_DEFAULT >= LOG_LEVEL_EMERG) \
			printf(LOG_COLOR_PURPLE "[0]" pr_fmt(fmt),  ##__VA_ARGS__); \
	} while (0)

#define pr_alert(fmt, ...) \
	do { \
		if (LOG_LEVEL_DEFAULT >= LOG_LEVEL_ALERT) \
			printf(LOG_COLOR_DPURPLE "[1]" pr_fmt(fmt),  ##__VA_ARGS__); \
	} while (0)

#define pr_crit(fmt, ...) \
	do { \
		if (LOG_LEVEL_DEFAULT >= LOG_LEVEL_CRIT) \
			printf(LOG_COLOR_RED "[2]" pr_fmt(fmt),  ##__VA_ARGS__); \
	} while (0)

#define pr_err(fmt, ...) \
	do { \
		if (LOG_LEVEL_DEFAULT >= LOG_LEVEL_ERR) \
			printf(LOG_COLOR_DRED "[3]" pr_fmt(fmt),  ##__VA_ARGS__); \
	} while (0)

#define pr_warn(fmt, ...) \
	do { \
		if (LOG_LEVEL_DEFAULT >= LOG_LEVEL_WARN) \
			printf(LOG_COLOR_YELLOW "[4]" pr_fmt(fmt),  ##__VA_ARGS__); \
	} while (0)

#define pr_notice(fmt, ...) \
	do { \
		if (LOG_LEVEL_DEFAULT >= LOG_LEVEL_NOTICE) \
			printf(LOG_COLOR_BLUE "[5]" pr_fmt(fmt),  ##__VA_ARGS__); \
	} while (0)

#define pr_info(fmt, ...) \
	do { \
		if (LOG_LEVEL_DEFAULT >= LOG_LEVEL_INFO) \
			printf(LOG_COLOR_GREEN "[6]" pr_fmt(fmt),  ##__VA_ARGS__); \
	} while (0)

#define pr_debug(fmt, ...) \
	do { \
		if (LOG_LEVEL_DEFAULT >= LOG_LEVEL_DEBUG) \
			printf(LOG_COLOR_NONE "[7]" pr_fmt(fmt),  ##__VA_ARGS__); \
	} while (0)

#define pr_bub(fmt, ...) \
	do { \
		if (LOG_LEVEL_DEFAULT >= LOG_LEVEL_BUBBLING) \
			printf(LOG_COLOR_NONE "[8]" pr_fmt(fmt),  ##__VA_ARGS__); \
	} while (0)

#define pr_div(fmt, ...) \
	do { \
		if (LOG_LEVEL_DEFAULT >= LOG_LEVEL_DIVING) \
			printf(LOG_COLOR_NONE "[9]" pr_fmt(fmt),  ##__VA_ARGS__); \
	} while (0)

#define pr_color(color, fmt, ...) \
	printf(color pr_fmt(fmt), ##__VA_ARGS__)

#endif /* !_UAPI_LOG_H_ */
