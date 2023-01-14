/**
 * @file overview.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-08
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _OVERVIEW_H_
#define _OVERVIEW_H_

struct nvme_tool;

int case_display_case_list(struct nvme_tool *tool);

int nvme_select_case_to_execute(struct nvme_tool *tool);

#endif /* !_OVERVIEW_H_ */
