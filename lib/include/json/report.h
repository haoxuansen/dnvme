/**
 * @file report.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-10-16
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UAPI_LIB_JSON_REPORT_H_
#define _UAPI_LIB_JSON_REPORT_H_

#include <stdbool.h>
#include "libjson.h"

struct json_node *json_create_root_node(const char *version);

static inline void json_destroy_root_node(struct json_node *root)
{
	cJSON_Delete(root);
}

struct json_node *json_add_case_node(struct json_node *parent, 
	const char *name, bool subcase);

static inline struct json_node *json_add_result_node(struct json_node *parent, 
	int result)
{
	return cJSON_AddNumberToObject(parent, "result", (double)result);
}

static inline struct json_node *json_add_speed_node(struct json_node *parent, 
	double speed)
{
	return cJSON_AddNumberToObject(parent, "speed", speed);
}

static inline struct json_node *json_add_step_node(struct json_node *parent)
{
	return cJSON_AddArrayToObject(parent, "step");
}

static inline struct json_node *json_add_subcase_node(struct json_node *parent, 
	const char *name)
{
	return cJSON_AddObjectToObject(parent, name);
}

static inline struct json_node *json_add_time_node(struct json_node *parent, int ms)
{
	return cJSON_AddNumberToObject(parent, "time", ms);
}

static inline struct json_node *json_add_width_node(struct json_node *parent, int lane)
{
	return cJSON_AddNumberToObject(parent, "width", lane);
}

static inline struct json_node *json_get_case_set(struct json_node *parent)
{
	return cJSON_GetObjectItem(parent, "case");
}

static inline struct json_node *json_get_subcase_set(struct json_node *parent)
{
	return cJSON_GetObjectItem(parent, "subcase");
}

static inline struct json_node *json_get_case_node(struct json_node *parent, 
	const char *name)
{
	return cJSON_GetObjectItem(parent, name);
}

int json_add_content_to_step_node(struct json_node *parent, const char *step);

#endif /* !_UAPI_LIB_JSON_REPORT_H_ */
