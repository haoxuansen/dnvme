/**
 * @file base.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-08-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "libbase.h"

/**
 * @return 0 on success, otherwise a negative errno.
 */
int call_system(const char *command)
{
	int status;

	status = system(command);
	if (-1 == status) {
		pr_err("system err!\n");
		return -EPERM;
	}

	if (!WIFEXITED(status)) {
		pr_err("abort with status:%d!\n", WEXITSTATUS(status));
		return -EPERM;
	}

	if (WEXITSTATUS(status)) {
		pr_err("failed to execute '%s'!(%d)\n", command, 
			WEXITSTATUS(status));
		return -EPERM;
	}

	return 0;
}

int dump_data_to_console(void *buf, uint32_t size)
{
	uint32_t oft = 0;
	char string[256];
	char *str = string;
	unsigned char *ptr = buf;
	int ret;

	while (size >= 0x10) {
		snprintf(str, sizeof(string), 
			"%x: %02x %02x %02x %02x %02x %02x %02x %02x "
			"%02x %02x %02x %02x %02x %02x %02x %02x", oft, 
			ptr[oft], ptr[oft + 1], ptr[oft + 2], ptr[oft + 3],
			ptr[oft + 4], ptr[oft + 5], ptr[oft + 6], ptr[oft + 7],
			ptr[oft + 8], ptr[oft + 9], ptr[oft + 10], ptr[oft + 11],
			ptr[oft + 12], ptr[oft + 13], ptr[oft + 14], ptr[oft + 15]);
		pr_debug("%s\n", str);
		size -= 0x10;
		oft += 0x10;
	}

	if (!size)
		return 0;

	ret = snprintf(str, sizeof(string), "%x:", oft);

	if (size >= 8) {
		ret += snprintf(str + ret, sizeof(string) - ret, 
			" %02x %02x %02x %02x %02x %02x %02x %02x", 
			ptr[oft], ptr[oft + 1], ptr[oft + 2], ptr[oft + 3],
			ptr[oft + 4], ptr[oft + 5], ptr[oft + 6], ptr[oft + 7]);
		size -= 8;
		oft += 8;
	}

	if (size >= 4) {
		ret += snprintf(str + ret, sizeof(string) - ret, 
			" %02x %02x %02x %02x", 
			ptr[oft], ptr[oft + 1], ptr[oft + 2], ptr[oft + 3]);
		size -= 4;
		oft += 4;
	}

	if (size >= 2) {
		ret += snprintf(str + ret, sizeof(string) - ret, 
			" %02x %02x", ptr[oft], ptr[oft + 1]);
		size -= 2;
		oft += 2;
	}

	if (size >= 1) {
		ret += snprintf(str + ret, sizeof(string) - ret, 
			" %02x", ptr[oft]);
		size -= 1;
		oft += 1;
	}

	pr_debug("%s\n", str);
	return 0;
}

int dump_data_to_file(void *buf, uint32_t size, const char *file)
{
        int fd = -1;
        ssize_t ret;
 
        fd = open(file, O_CREAT | O_RDWR);
        if (fd < 0) {
                pr_err("failed to open %s: %s!\n", file, strerror(errno));
                return fd; 
        }   
 
        ret = write(fd, buf, size);
        if (ret < 0 || ret != size) {
                pr_err("failed to write %s: %s\n", file, strerror(errno));
                ret = -EIO;
                goto out;
        }   
        pr_debug("write data to %s ok!\n", file);

out:
        close(fd);
        return 0;
}

