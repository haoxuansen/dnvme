/**
 * @file extension.h
 * @author yeqiang_xu (yeqiang_xu@maxio-tech.com)
 * @brief 
 * @version 0.1
 * @date 2023-11-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

int json_update_number_value(struct json_node *item, double val);

struct json_node *json_add_string_to_array(struct json_node *arr, 
	const char *str);
