/**
 * @file test_json.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-09-25
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "compiler.h"
#include "libbase.h"
#include "libjson.h"

static char *get_file_context(const char *file)
{
	FILE *fp = NULL;
	long len = 0;
	char *ctx = NULL;
	size_t read_chars = 0;
	int ret;

	fp = fopen(file, "rb");
	if (!fp) {
		pr_err("failed to fopen %s: %s!\n", file, strerror(errno));
		return NULL;
	}

	ret = fseek(fp, 0, SEEK_END);
	if (ret < 0) {
		pr_err("failed to fseek: %s!\n", strerror(errno));
		goto close_file;
	}

	len = ftell(fp);
	if (len < 0)
		goto close_file;
	
	ret = fseek(fp, 0, SEEK_SET);
	if (ret < 0)
		goto close_file;
	
	ctx = (char *)malloc((size_t)len + sizeof(""));
	if (!ctx)
		goto close_file;

	read_chars = fread(ctx, sizeof(char), (size_t)len, fp);
	if ((long)read_chars != len) {
		free(ctx);
		ctx = NULL;
		goto close_file;
	}
	ctx[read_chars] = '\0';

close_file:
	fclose(fp);
	return ctx;
}

static void put_file_context(char *ctx)
{
	free(ctx);
}

static int __unused test_create_object(void)
{
	struct json_node *root;
	struct json_node *ut_case;
	char *txt;
	int ret = -ENOMEM;

	root = cJSON_CreateObject();
	if (!root) {
		pr_err("failed to crate root object!\n");
		return -ENOMEM;
	}

	ut_case = cJSON_CreateObject();
	if (!ut_case) {
		pr_err("failed to create case object!\n");
		goto del_root;
	}


	cJSON_AddStringToObject(root, "version", "1.0.0");
	cJSON_AddItemToObject(root, "case", ut_case);
	cJSON_AddStringToObject(ut_case, "date", "2023-09-25 12:00:00");
	cJSON_AddNumberToObject(ut_case, "time", 100); /* us */
	cJSON_AddStringToObject(ut_case, "result", "PASS");

	txt = cJSON_Print(root);
	printf("%s\n", txt);

	ret = 0;

	free(txt);
del_root:
	cJSON_Delete(root);
	return ret;
}

static int __unused test_read_file(int argc, char *argv[])
{
	struct json_node *parsed = NULL;
	char *ctx = NULL;
	char *txt = NULL;
	int ret = 0;

	if (argc < 2) {
		pr_err("argc:%d is invalid!\n", argc);
		return -EINVAL;
	}

	ctx = get_file_context(argv[1]);
	if (!ctx) {
		pr_err("fail to read file context!\n");
		return -EPERM;
	}

	parsed = cJSON_Parse(ctx);
	if (!parsed) {
		pr_err("failed to parse json!\n");
		ret = -EPERM;
		goto out;
	}

	txt = cJSON_Print(parsed);
	printf("%s\n", txt);

	free(txt);
out:
	put_file_context(ctx);
	return ret;
}

static int __unused test_generate_report(void)
{
	struct json_node *root;
	struct json_node *case_set, *subcase_set;
	struct json_node *item_case, *item_subcase;
	struct json_node *step;
	char *str;
	FILE *fp;
	int ret = -EPERM;

	root = json_create_root_node("1.0.0");
	if (!root) {
		pr_err("failed to create root node!\n");
		return -EPERM;
	}
	case_set = json_get_case_set(root);

	item_case = json_add_case_node(case_set, "case_cmd_io_read", false);
	json_add_result_node(item_case, -95);
	json_add_speed_node(item_case, 2.5);
	json_add_time_node(item_case, 100);

	item_case = json_add_case_node(case_set, "case_cmd_io_write", true);
	json_add_result_node(item_case, 0);
	json_add_speed_node(item_case, 8);
	json_add_width_node(item_case, 4);

	subcase_set = json_get_subcase_set(item_case);
	item_subcase = json_add_subcase_node(subcase_set, "subcase_write_success");
	json_add_result_node(item_subcase, 0);
	step = json_add_step_node(item_subcase);
	json_add_content_to_step_node(step, 
		"Send io write cmd with slba(0x100) and nlb(0x10)");
	json_add_content_to_step_node(step, "Record return value");
	item_subcase = json_add_subcase_node(subcase_set, "subcase_write_fua_success");
	json_add_result_node(item_subcase, 0);
	item_subcase = json_add_subcase_node(subcase_set, "subcase_write_invlid_cid");
	json_add_result_node(item_subcase, 0);
	item_subcase = json_add_subcase_node(subcase_set, 
		"subcase_write_use_sgl_bit_bucket");
	json_add_result_node(item_subcase, 0);

	item_case = json_add_case_node(case_set, "case_cmd_io_copy", true);
	json_add_result_node(item_case, -1);
	json_add_speed_node(item_case, 5);
	json_add_width_node(item_case, 4);
	step = json_add_step_node(item_case);
	json_add_content_to_step_node(step, "subcase_copy_success");
	json_add_content_to_step_node(step, "subcase_copy_to_read_only");
	json_add_content_to_step_node(step, "subcase_copy_invalid_desc_format");
	subcase_set = json_get_subcase_set(item_case);
	item_subcase = json_add_subcase_node(subcase_set, "subcase_copy_success");
	json_add_result_node(item_subcase, 0);
	item_subcase = json_add_subcase_node(subcase_set, "subcase_copy_to_read_only");
	json_add_result_node(item_subcase, -1);
	item_subcase = json_add_subcase_node(subcase_set, 
		"subcase_copy_invalid_desc_format");
	json_add_result_node(item_subcase, -95);

	str = cJSON_Print(root);

	fp = fopen("./sample.json", "w+");
	if (!fp) {
		pr_err("failed to open file!\n");
		goto out;
	}

	fwrite(str, strlen(str), 1, fp);

	ret = 0;

	fclose(fp);
	free(str);
out:
	json_destroy_root_node(root);
	return ret;
}

int main(int argc, char *argv[])
{
	int ret = 0;

	ret |= test_create_object();
	// ret |= test_read_file(argc, argv);
	// ret |= test_generate_report();

	return ret;
}

