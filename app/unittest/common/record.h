/**
 * @file record.h
 * @author yeqiang_xu (yeqiang_xu@maxio-tech.com)
 * @brief 
 * @version 0.1
 * @date 2023-11-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

struct json_node *ut_rpt_create_root(struct nvme_tool *tool);
void ut_rpt_destroy_root(struct nvme_tool *tool);
int ut_rpt_init_root(struct nvme_tool *tool, const char *version);
int ut_rpt_save_to_file(struct nvme_tool *tool, const char *filepath);

struct json_node *ut_rpt_create_case(struct nvme_tool *tool, 
	struct case_report *rpt, const char *name);
struct json_node *ut_rpt_create_subcase(struct case_report *rpt, 
	const char *name);

int ut_rpt_record_case_cost(struct case_report *rpt, int time);
int ut_rpt_record_case_result(struct case_report *rpt, int result);
int ut_rpt_record_case_speed(struct case_report *rpt, double speed);
int ut_rpt_record_case_step(struct case_report *rpt, const char *fmt, ...);
int ut_rpt_record_case_width(struct case_report *rpt, int width);
