/**
 * @file test_meta.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-03-23
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _APP_TEST_META_H_
#define _APP_TEST_META_H_

struct nvme_tool;

int case_meta_create_pool(struct nvme_tool *tool);
int case_meta_destroy_pool(struct nvme_tool *tool);
int case_meta_create_node(struct nvme_tool *tool);
int case_meta_delete_node(struct nvme_tool *tool);

int case_meta_xfer_separate(struct nvme_tool *tool);
int case_meta_xfer_extlba(struct nvme_tool *tool);

#endif /* !_APP_TEST_META_H_ */
