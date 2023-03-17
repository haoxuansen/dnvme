/**
 * @file test_pm.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-12
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _APP_TEST_PM_H_
#define _APP_TEST_PM_H_

int case_pm_switch_power_state(struct nvme_tool *tool);

int case_pm_set_d0_state(struct nvme_tool *tool);
int case_pm_set_d3hot_state(struct nvme_tool *tool);

#endif /* !_APP_TEST_PM_H_ */