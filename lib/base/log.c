/**
 * @file log.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-08-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

#include "libbase.h"

const char *log_color(uint8_t level)
{
	switch (level) {
	case LOG_LEVEL_EMERG:
		return LOG_B_PURPLE;
	case LOG_LEVEL_ALERT:
		return LOG_N_PURPLE;
	case LOG_LEVEL_CRIT:
		return LOG_B_RED;
	case LOG_LEVEL_ERR:
		return LOG_N_RED;
	case LOG_LEVEL_WARN:
		return LOG_B_YELLOW;
	case LOG_LEVEL_NOTICE:
		return LOG_B_BLUE;
	case LOG_LEVEL_INFO:
		return LOG_B_GREEN;
	default:
		return "";
	}
}

static int log_vprint(uint8_t level, const char *fmt, va_list args)
{
	int ret;
	static char textbuf[4096];
	char *text = textbuf;

	ret = vsnprintf(text, sizeof(textbuf), fmt, args);
	printf("%s%s[%d]%s", LOG_N, log_color(level), level, text);

	return ret;
}

int log_print(uint8_t level, const char *fmt, ...)
{
	int ret;
	va_list args;

	if (level > LOG_LEVEL_DEFAULT)
		return 0;

	va_start(args, fmt);
	ret = log_vprint(level, fmt, args);
	va_end(args);

	return ret;
}
