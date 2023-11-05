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
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "libbase.h"
#include "test.h"

#define TEST_PASS "\n" \
	"\t ppppppppppp     aaaaaaaaaaa     sssssssssss     sssssssssss \n" \
	"\t pp       pp     aa       aa     ss              ss          \n" \
	"\t pp       pp     aa       aa     ss              ss          \n" \
	"\t pp       pp     aa       aa     ss              ss          \n" \
	"\t ppppppppppp     aaaaaaaaaaa     sssssssssss     sssssssssss \n" \
	"\t pp              aa       aa              ss              ss \n" \
	"\t pp              aa       aa              ss              ss \n" \
	"\t pp              aa       aa              ss              ss \n" \
	"\t pp              aa       aa     sssssssssss     sssssssssss \n"

#define TEST_SKIP "\n" \
	"\t sssssssssss     kk       kk     iiiiiiiiiii     ppppppppppp \n" \
	"\t ss              kk     kk           iii         pp       pp \n" \
	"\t ss              kk   kk             iii         pp       pp \n" \
	"\t ss              kk kk               iii         pp       pp \n" \
	"\t sssssssssss     kkk                 iii         ppppppppppp \n" \
	"\t          ss     kk kk               iii         pp          \n" \
	"\t          ss     kk   kk             iii         pp          \n" \
	"\t          ss     kk     kk           iii         pp          \n" \
	"\t sssssssssss     kk       kk     iiiiiiiiiii     pp          \n"

#define TEST_FAIL "\n" \
	"\t fffffffffff     aaaaaaaaaaa     iiiiiiiiiii     ll          \n" \
	"\t ff              aa       aa         iii         ll          \n" \
	"\t ff              aa       aa         iii         ll          \n" \
	"\t ff              aa       aa         iii         ll          \n" \
	"\t fffffffffff     aaaaaaaaaaa         iii         ll          \n" \
	"\t ff              aa       aa         iii         ll          \n" \
	"\t ff              aa       aa         iii         ll          \n" \
	"\t ff              aa       aa         iii         ll          \n" \
	"\t ff              aa       aa     iiiiiiiiiii     lllllllllll \n"

#define TEST_REPORT_ARRAY_SIZE		64

struct test_report {
	unsigned int	idx;

	struct {
		const char	*name;
		int		result;
	} arr[TEST_REPORT_ARRAY_SIZE];
};

static struct test_report rpt_subcase = {0};
static struct test_report rpt_case = {0};

static void record_test_result(struct test_report *report, 
	const char *name, int result)
{
	if (report->idx >= TEST_REPORT_ARRAY_SIZE) {
		pr_warn("Array is full!\n");
		return;
	}

	report->arr[report->idx].name = name;
	report->arr[report->idx].result = result;
	report->idx++;
}

/**
 * @brief Record test case result info
 */
void nvme_record_case_result(const char *name, int result)
{
	record_test_result(&rpt_case, name, result);
}

void nvme_record_subcase_result(const char *name, int result)
{
	record_test_result(&rpt_subcase, name, result);
}

static int display_test_report(struct test_report *report)
{
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

	return fail == 0 ? 0 : -EPERM; 
}

int nvme_display_case_report(void)
{
	return display_test_report(&rpt_case);
}

int nvme_display_subcase_report(void)
{
	return display_test_report(&rpt_subcase);
}

void nvme_display_test_result(int result, const char *desc)
{
	if (result < 0) {
		if (result == -EOPNOTSUPP) {
			pr_color(LOG_U_YELLOW, "%s\n", desc);
			pr_color(LOG_F_YELLOW, "%s\n", TEST_SKIP);
		} else {
			pr_color(LOG_U_RED, "%s\n", desc);
			pr_color(LOG_F_RED, "%s\n", TEST_FAIL);
		}
		return;
	}

	pr_color(LOG_U_GREEN, "%s\n", desc);
	pr_color(LOG_F_GREEN, "%s\n", TEST_PASS);

}
