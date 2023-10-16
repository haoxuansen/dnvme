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

cJSON *json_create_root_node(const char *version);

static inline void json_destroy_root_node(cJSON *root)
{
	cJSON_Delete(root);
}

cJSON *json_add_case_node(cJSON *parent, const char *name, bool subcase);

static inline cJSON *json_add_result_node(cJSON *parent, int result)
{
	return cJSON_AddNumberToObject(parent, "result", (double)result);
}

static inline cJSON *json_add_speed_node(cJSON *parent, double speed)
{
	return cJSON_AddNumberToObject(parent, "speed", speed);
}

static inline cJSON *json_add_step_node(cJSON *parent)
{
	return cJSON_AddArrayToObject(parent, "step");
}

static inline cJSON *json_add_subcase_node(cJSON *parent, const char *name)
{
	return cJSON_AddObjectToObject(parent, name);
}

static inline cJSON *json_add_time_node(cJSON *parent, int ms)
{
	return cJSON_AddNumberToObject(parent, "time", ms);
}

static inline cJSON *json_add_width_node(cJSON *parent, int lane)
{
	return cJSON_AddNumberToObject(parent, "width", lane);
}

static inline cJSON *json_get_case_set(cJSON *parent)
{
	return cJSON_GetObjectItemCaseSensitive(parent, "case");
}

static inline cJSON *json_get_subcase_set(cJSON *parent)
{
	return cJSON_GetObjectItemCaseSensitive(parent, "subcase");
}

static inline cJSON *json_get_case_node(cJSON *parent, const char *name)
{
	return cJSON_GetObjectItemCaseSensitive(parent, name);
}

int json_add_content_to_step_node(cJSON *parent, const char *step);

#endif /* !_UAPI_LIB_JSON_REPORT_H_ */
