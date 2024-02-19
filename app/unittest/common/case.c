/**
 * @file case.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2024-02-19
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <stdlib.h>
#include <errno.h>
#include "libbase.h"
#include "test.h"

int ut_case_alloc_config_effect(struct case_config *cfg)
{
	struct case_config_effect *effect;

	effect = zalloc(sizeof(struct case_config_effect));
	if (!effect) {
		pr_err("failed to alloc memory!\n");
		return -ENOMEM;
	}
	cfg->effect = effect;
	return 0;
}

void ut_case_release_config_effect(struct case_config *cfg)
{
	free(cfg->effect);
	cfg->effect = NULL;
}
