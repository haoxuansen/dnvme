/**
 * @file test_queue.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-01-06
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _APP_TEST_QUEUE_H_
#define _APP_TEST_QUEUE_H_

int case_queue_iocmd_to_asq(struct nvme_tool *tool);

int case_queue_create_and_delete_contig_queue(struct nvme_tool *tool);
int case_queue_create_and_delete_discontig_queue(struct nvme_tool *tool);

#endif /* !_APP_TEST_QUEUE_H_ */
