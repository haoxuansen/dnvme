/**
 * @file record.c
 * @author yeqiang_xu (yeqiang_xu@maxio-tech.com)
 * @brief 
 * @version 0.1
 * @date 2023-11-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "sizes.h"
#include "libbase.h"
#include "libjson.h"
#include "test.h"

static inline struct json_node *ut_rpt_get_case_entry(struct nvme_tool *tool)
{
	return cJSON_GetObjectItem(tool->report, "case");
}

static inline struct json_node *ut_rpt_get_subcase_entry(struct case_report *rpt)
{
	return cJSON_GetObjectItem(rpt->root, "subcase");
}

static struct json_node *ut_rpt_get_context_node(struct case_report *rpt)
{
	switch (rpt->ctx) {
	case UT_RPT_CTX_CASE:
		return rpt->root;
	case UT_RPT_CTX_SUBCASE:
		return rpt->child;
	default:
		pr_err("invalid context:%u!\n", rpt->ctx);
		return NULL;
	}
}

struct json_node *ut_rpt_create_root(struct nvme_tool *tool)
{
	struct json_node *root;

	root = cJSON_CreateObject();
	if (!root) {
		pr_err("failed to create root node!\n");
		return NULL;
	}
	tool->report = root;
	return root;
}

void ut_rpt_destroy_root(struct nvme_tool *tool)
{
	if (!tool->report) {
		pr_warn("double free?");
		return;
	}
	cJSON_Delete(tool->report);
	tool->report = NULL;
}

int ut_rpt_init_root(struct nvme_tool *tool, const char *version)
{
	struct json_node *item;
	char buf[64];
	time_t now;
	struct tm *tmnow;

	item = cJSON_AddStringToObject(tool->report, "version", version);
	if (!item) {
		pr_err("failed to create version node!");
		return -EPERM;
	}

	now = time(NULL);
	tmnow = gmtime(&now);
	snprintf(buf, sizeof(buf), "%d/%02d/%02d %02d:%02d:%02d", 
		1900 + tmnow->tm_year, 1 + tmnow->tm_mon, tmnow->tm_mday,
		8 + tmnow->tm_hour, tmnow->tm_min, tmnow->tm_sec);
	item = cJSON_AddStringToObject(tool->report, "date", buf);
	if (!item) {
		pr_err("failed to create date node!\n");
		return -EPERM;
	}

	item = cJSON_AddObjectToObject(tool->report, "case");
	if (!item) {
		pr_err("failed to create case entry!\n");
		return -EPERM;
	}

	return 0;
}

int ut_rpt_save_to_file(struct nvme_tool *tool, const char *filepath)
{
	char *str;
	FILE *fp;
	int ret = -EPERM;

	str = cJSON_Print(tool->report);
	if (!str) {
		pr_err("failed to convert json data to string!\n");
		return -EPERM;
	}

	fp = fopen(filepath, "w+");
	if (!fp) {
		pr_err("failed to open %s!\n", filepath);
		goto out;
	}

	fwrite(str, strlen(str), 1, fp);
	fclose(fp);
	ret = 0;
out:
	free(str);
	return ret;
}

struct json_node *ut_rpt_create_case(struct nvme_tool *tool, 
	struct case_report *rpt, const char *name)
{
	struct json_node *current;
	struct json_node *item;

	current = cJSON_AddObjectToObject(ut_rpt_get_case_entry(tool), name);
	if (!current) {
		pr_err("failed to create case node!\n");
		return NULL;
	}

	item = cJSON_AddObjectToObject(current, "subcase");
	if (!item) {
		pr_err("failed to create subcase entry!\n");
		return NULL;
	}

	rpt->root = current;
	return current;
}

struct json_node *ut_rpt_create_subcase(struct case_report *rpt, 
	const char *name)
{
	struct json_node *current;

	current = cJSON_AddObjectToObject(ut_rpt_get_subcase_entry(rpt), name);
	if (!current) {
		pr_err("failed to create subcase node!\n");
		return NULL;
	}
	rpt->child = current;
	rpt->ctx = UT_RPT_CTX_SUBCASE;
	return current;
}

int ut_rpt_record_case_cost(struct case_report *rpt, int time)
{
	struct json_node *parent;
	struct json_node *item;

	parent = ut_rpt_get_context_node(rpt);
	item = cJSON_GetObjectItem(parent, "time");
	if (item)
		return json_update_number_value(item, (double)time);

	item = cJSON_AddNumberToObject(parent, "time", (double)time);
	if (!item) {
		pr_err("failed to create time node!\n");
		return -EPERM;
	}

	return 0;
}

int ut_rpt_record_case_result(struct case_report *rpt, int result)
{
	struct json_node *parent;
	struct json_node *item;

	parent = ut_rpt_get_context_node(rpt);
	item = cJSON_GetObjectItem(parent, "result");
	if (item)
		return json_update_number_value(item, (double)result);

	item = cJSON_AddNumberToObject(parent, "result", (double)result);
	if (!item) {
		pr_err("failed to create result node!\n");
		return -EPERM;
	}

	return 0;
}

int ut_rpt_record_case_speed(struct case_report *rpt, double speed)
{
	struct json_node *parent;
	struct json_node *item;

	parent = ut_rpt_get_context_node(rpt);
	item = cJSON_GetObjectItem(parent, "speed");
	if (item)
		return json_update_number_value(item, speed);

	item = cJSON_AddNumberToObject(parent, "speed", speed);
	if (!item) {
		pr_err("failed to create speed node!\n");
		return -EPERM;
	}

	return 0;
}

int ut_rpt_record_case_step(struct case_report *rpt, const char *fmt, ...)
{
	struct json_node *parent;
	struct json_node *arr;
	struct json_node *item;
	va_list args;
	char *buf;
	int ret = -EPERM;

	buf = zalloc(SZ_1K);
	if (!buf)
		return -ENOMEM;

	va_start(args, fmt);
	vsnprintf(buf, SZ_1K, fmt, args);
	va_end(args);

	parent = ut_rpt_get_context_node(rpt);
	arr = cJSON_GetObjectItem(parent, "step");
	if (!arr) {
		arr = cJSON_AddArrayToObject(parent, "step");
		if (!arr) {
			pr_err("failed to create step node!");
			goto out;
		}
	}

	item = json_add_string_to_array(arr, buf);
	if (!item)
		goto out;

	ret = 0;
out:
	free(buf);
	return ret;
}

int ut_rpt_record_case_width(struct case_report *rpt, int width)
{
	struct json_node *parent;
	struct json_node *item;

	parent = ut_rpt_get_context_node(rpt);
	item = cJSON_GetObjectItem(parent, "width");
	if (item)
		return json_update_number_value(item, (double)width);

	item = cJSON_AddNumberToObject(parent, "width", (double)width);
	if (!item) {
		pr_err("failed to create width node!\n");
		return -EPERM;
	}

	return 0;
}
