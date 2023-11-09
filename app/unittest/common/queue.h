/**
 * @file queue.h
 * @author yeqiang_xu (yeqiang_xu@maxio-tech.com)
 * @brief 
 * @version 0.1
 * @date 2023-11-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

int ut_create_pair_io_queue(struct case_data *priv, struct nvme_sq_info *sq,
	struct nvme_cq_info *cq);
int ut_create_pair_io_queues(struct case_data *priv, struct nvme_sq_info **sq,
	struct nvme_cq_info **cq, int nr_pair);

int ut_delete_pair_io_queue(struct case_data *priv, struct nvme_sq_info *sq,
	struct nvme_cq_info *cq);
int ut_delete_pair_io_queues(struct case_data *priv, struct nvme_sq_info **sq,
	struct nvme_cq_info **cq, int nr_pair);

int ut_ring_sq_doorbell(struct case_data *priv, struct nvme_sq_info *sq);
int ut_ring_sqs_doorbell(struct case_data *priv, struct nvme_sq_info **sq,
	int nr_sq);

int ut_reap_cq_entry_check_status(struct case_data *priv, 
	struct nvme_cq_info *cq, int nr_entry);
int ut_reap_cq_entry_check_status_by_id(struct case_data *priv, 
	uint16_t cqid, int nr_entry);
int ut_reap_cq_entry_no_check(struct case_data *priv, 
	struct nvme_cq_info *cq, int nr_entry);
int ut_reap_cq_entry_no_check_by_id(struct case_data *priv, uint16_t cqid,
	int nr_entry);

