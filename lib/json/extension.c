/**
 * @file extension.c
 * @author yeqiang_xu (yeqiang_xu@maxio-tech.com)
 * @brief 
 * @version 0.1
 * @date 2023-11-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <errno.h>

#include "libbase.h"
#include "libjson.h"


int json_update_number_value(struct json_node *item, double val)
{
	if (!cJSON_IsNumber(item)) {
		pr_err("json type isn't a number!\n");
		return -EINVAL;
	}
	item->valuedouble = val;
	return 0;
}

struct json_node *json_add_string_to_array(struct json_node *arr, 
	const char *str)
{
	struct json_node *item;

	if (!cJSON_IsArray(arr)) {
		pr_err("json type isn't an array!\n");
		return NULL;
	}

	item = cJSON_CreateString(str);
	if (!item) {
		pr_err("failed to create string node!\n");
		return NULL;
	}

	if (cJSON_AddItemToArray(arr, item) == false)
		goto out;

	return item;
out:
	cJSON_Delete(item);
	return NULL;
}

