/**
 * @file report.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-10-16
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <errno.h>
#include <time.h>
#include "libbase.h"
#include "libjson.h"


struct json_node *json_create_root_node(const char *version)
{
	struct json_node *root;
	struct json_node *item;
	char buf[64];
	time_t now;
	struct tm *tmnow;

	root = cJSON_CreateObject();
	if (!root) {
		pr_err("failed to create root node!\n");
		return NULL;
	}

	item = cJSON_AddStringToObject(root, "version", version);
	if (!item) {
		pr_err("failed to create version node!\n");
		goto out;
	}

	now = time(NULL);
	tmnow = gmtime(&now);
	snprintf(buf, sizeof(buf), "%d/%02d/%02d %02d:%02d:%02d", 
		1900 + tmnow->tm_year, 1 + tmnow->tm_mon, tmnow->tm_mday,
		8 + tmnow->tm_hour, tmnow->tm_min, tmnow->tm_sec);

	item = cJSON_AddStringToObject(root, "date", buf);
	if (!item) {
		pr_err("failed to create date node!\n");
		goto out;
	}

	item = cJSON_AddObjectToObject(root, "case");
	if (!item) {
		pr_err("failed to create case set!\n");
		goto out;
	}

	return root;

out:
	cJSON_Delete(root);
	return NULL;
}

struct json_node *json_add_case_node(struct json_node *parent, const char *name, bool subcase)
{
	struct json_node *item;
	struct json_node *subitem;

	item = cJSON_AddObjectToObject(parent, name);
	if (!item)
		return NULL;

	if (subcase) {
		subitem = cJSON_AddObjectToObject(item, "subcase");
		if (!subitem)
			goto out;
	}

	return item;
out:
	cJSON_Delete(item);
	return NULL;
}

int json_add_content_to_step_node(struct json_node *parent, const char *step)
{
	struct json_node *item;

	item = cJSON_CreateString(step);
	if (!item)
		return -EPERM;

	return cJSON_AddItemToArray(parent, item) == true ? 0 : -EPERM;
}
