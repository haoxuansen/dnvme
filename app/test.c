/**
 * @file test.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-03-31
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <errno.h>

#include "log.h"

#include "test.h"

#define TEST_REPORT_ARRAY_SIZE		64

struct test_report {
	unsigned int	idx;

	struct {
		const char	*name;
		int		result;
	} arr[TEST_REPORT_ARRAY_SIZE];
};

static struct test_report subcase = {0};

void nvme_record_subcase_result(const char *name, int result)
{
	struct test_report *report = &subcase;

	if (report->idx >= TEST_REPORT_ARRAY_SIZE) {
		pr_warn("Array is full!\n");
		return;
	}

	report->arr[report->idx].name = name;
	report->arr[report->idx].result = result;
	report->idx++;
}

void nvme_display_subcase_result(void)
{
	struct test_report *report = &subcase;
	uint32_t skip = 0;
	uint32_t pass = 0;
	uint32_t fail = 0;
	uint32_t i;

	pr_info("-----+----------------------------------+--------\n");
	pr_info(" Idx |                 Name             | Result \n");
	pr_info("-----+----------------------------------+--------\n");

	for (i = 0; i < report->idx; i++) {
		if (report->arr[i].result == -EOPNOTSUPP) {
			pr_notice("%4u | %-32s | SKIP \n", i, report->arr[i].name);
			skip++;
		} else if (report->arr[i].result < 0) {
			pr_err("%4u | %-32s | FAIL \n", i, report->arr[i].name);
			fail++;
		} else {
			pr_info("%4u | %-32s | PASS \n", i, report->arr[i].name);
			pass++;
		}
	}

	pr_info("Total: %u, Pass: %u, Fail: %u, Skip: %u\n", report->idx,
		pass, fail, skip);
	report->idx = 0; /* clean for next using */
}
