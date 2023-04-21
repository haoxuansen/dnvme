/**
 * @file test_cmd.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _APP_TEST_CMD_H_
#define _APP_TEST_CMD_H_

int case_cmd_io_read(struct nvme_tool *tool);
int case_cmd_io_write(struct nvme_tool *tool);
int case_cmd_io_compare(struct nvme_tool *tool);

int case_cmd_io_read_with_fua(struct nvme_tool *tool);
int case_cmd_io_write_with_fua(struct nvme_tool *tool);

#endif /* !_APP_TEST_CMD_H_ */
