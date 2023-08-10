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

#include <stdlib.h>
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
