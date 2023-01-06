/**
 * @file cmd.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-19
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "log.h"
#include "cmd.h"
#include "queue.h"

/**
 * @brief Submit the specified command
 * 
 * @param fd NVMe device file descriptor
 * @param cmd The command to be submitted
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_submit_64b_cmd(int fd, struct nvme_64b_cmd *cmd)
{
	struct nvme_common_command *ccmd;
	int ret;

	if (!cmd->cmd_buf_ptr) {
		pr_err("cmd buf pointer is NULL!\n");
		return -EINVAL;
	}
	ccmd = cmd->cmd_buf_ptr;

	ret = ioctl(fd, NVME_IOCTL_SUBMIT_64B_CMD, cmd);
	if (ret < 0) {
		pr_err("failed to submit cmd(0x%x) to SQ(%u)!(%d)\n", 
			ccmd->opcode, cmd->q_id, ret);
		return ret;
	}
	return (int)cmd->unique_id;
}

/* !TODO: delete it later */
int nvme_submit_64b_cmd_legacy(int fd, struct nvme_64b_cmd *cmd)
{
	struct nvme_common_command *ccmd;
	int ret;

	if (!cmd->cmd_buf_ptr) {
		pr_err("cmd buf pointer is NULL!\n");
		return -EINVAL;
	}
	ccmd = cmd->cmd_buf_ptr;

	ret = ioctl(fd, NVME_IOCTL_SUBMIT_64B_CMD, cmd);
	if (ret < 0) {
		pr_err("failed to submit cmd(0x%x) to SQ(%u)!(%d)\n", 
			ccmd->opcode, cmd->q_id, ret);
		return ret;
	}
	return 0;
}

/**
 * @brief Submit command to create a I/O submission queue
 * 
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_create_iosq(int fd, struct nvme_create_sq *csq, uint8_t contig,
	void *buf, uint32_t size)
{
	struct nvme_64b_cmd cmd = {0};

	cmd.q_id = NVME_AQ_ID;
	cmd.cmd_buf_ptr = csq;
	/* !TODO: Although it is always right to use bidirection, it is
	 * better to choose the right direction */
	cmd.data_dir = DMA_BIDIRECTIONAL;

	if (contig)
		cmd.bit_mask = NVME_MASK_PRP1_PAGE;
	else
		cmd.bit_mask = NVME_MASK_PRP1_LIST;

	cmd.data_buf_ptr = buf;
	cmd.data_buf_size = size;

	return nvme_submit_64b_cmd(fd, &cmd);
}

/**
 * @brief Submit command to create a I/O completion queue
 * 
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_create_iocq(int fd, struct nvme_create_cq *ccq, uint8_t contig,
	void *buf, uint32_t size)
{
	struct nvme_64b_cmd cmd = {0};

	cmd.q_id = NVME_AQ_ID;
	cmd.cmd_buf_ptr = ccq;
	/* !TODO: Although it is always right to use bidirection, it is
	 * better to choose the right direction */
	cmd.data_dir = DMA_BIDIRECTIONAL;

	if (contig)
		cmd.bit_mask = NVME_MASK_PRP1_PAGE;
	else
		cmd.bit_mask = NVME_MASK_PRP1_LIST;

	cmd.data_buf_ptr = buf;
	cmd.data_buf_size = size;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_cmd_delete_iosq(int fd, uint16_t sqid)
{
	struct nvme_64b_cmd cmd = {0};
	struct nvme_delete_queue dq = {0};

	dq.opcode = nvme_admin_delete_sq;
	dq.qid = cpu_to_le16(sqid);

	cmd.q_id = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &dq;

	return nvme_submit_64b_cmd(fd, &cmd);
}

int nvme_cmd_delete_iocq(int fd, uint16_t cqid)
{
	struct nvme_64b_cmd cmd = {0};
	struct nvme_delete_queue dq = {0};

	dq.opcode = nvme_admin_delete_cq;
	dq.qid = cpu_to_le16(cqid);

	cmd.q_id = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &dq;

	return nvme_submit_64b_cmd(fd, &cmd);
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_set_feature(int fd, uint32_t nsid, uint32_t fid, uint32_t dw11)
{
	struct nvme_64b_cmd cmd = {0};
	struct nvme_features feat = {0};

	feat.opcode = nvme_admin_set_features;
	feat.nsid = cpu_to_le32(nsid);
	feat.fid = cpu_to_le32(fid);
	feat.dword11 = cpu_to_le32(dw11);

	cmd.q_id = NVME_AQ_ID;
	cmd.cmd_buf_ptr = &feat;

	return nvme_submit_64b_cmd(fd, &cmd);
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_identify(int fd, struct nvme_identify *identify, void *buf, 
	uint32_t size)
{
	struct nvme_64b_cmd cmd = {0};
	
	BUG_ON(size != NVME_IDENTIFY_DATA_SIZE);

	cmd.q_id = NVME_AQ_ID;
	cmd.cmd_buf_ptr = identify;
	cmd.bit_mask = NVME_MASK_PRP1_PAGE | NVME_MASK_PRP2_PAGE;
	cmd.data_dir = DMA_BIDIRECTIONAL,
	cmd.data_buf_ptr = buf;
	cmd.data_buf_size = size;

	return nvme_submit_64b_cmd(fd, &cmd);
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_identify_ctrl_list(int fd, void *buf, uint32_t size, 
	uint16_t cntid)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.cns = NVME_ID_CNS_CTRL_LIST;
	identify.ctrlid = cpu_to_le16(cntid);

	return nvme_cmd_identify(fd, &identify, buf, size);
}

/**
 * @brief Get Controller list.
 * 
 * @param cntid A controller list is returned containing a controller 
 *  identifier greater than or equal to the value specified in this field.
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_identify_ctrl_list(int fd, void *buf, uint32_t size, uint16_t cntid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ctrl_list(fd, buf, size, cntid);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_reap_expect_cqe(fd, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, 0);
	if (ret < 0)
		return ret;
	
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_identify_ns_attached_ctrl_list(int fd, void *buf, uint32_t size,
	uint32_t nsid, uint16_t cntid)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.cns = NVME_ID_CNS_CTRL_NS_LIST;
	identify.nsid = cpu_to_le32(nsid);
	identify.ctrlid = cpu_to_le16(cntid);

	return nvme_cmd_identify(fd, &identify, buf, size);
}

/**
 * @brief Get Namespace Attached Controller list.
 * 
 * @param cntid A controller list is returned containing a controller 
 *  identifier greater than or equal to the value specified in this field.
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_identify_ns_attached_ctrl_list(int fd, void *buf, uint32_t size, 
	uint32_t nsid, uint16_t cntid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ns_attached_ctrl_list(fd, buf, size, nsid, cntid);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_reap_expect_cqe(fd, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, 0);
	if (ret < 0)
		return ret;
	
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_identify_ns_desc_list(int fd, void *buf, uint32_t size,
	uint32_t nsid)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.cns = NVME_ID_CNS_NS_DESC_LIST;
	identify.nsid = cpu_to_le32(nsid);

	return nvme_cmd_identify(fd, &identify, buf, size);
}

/**
 * @brief Get Namespace Identification Descriptor list.
 * 
 * @param nsid An active NSID
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_identify_ns_desc_list(int fd, void *buf, uint32_t size, uint32_t nsid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ns_desc_list(fd, buf, size, nsid);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_reap_expect_cqe(fd, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, 0);
	if (ret < 0)
		return ret;
	
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_identify_ns_list_active(int fd, void *buf, uint32_t size, 
	uint32_t nsid)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.cns = NVME_ID_CNS_NS_ACTIVE_LIST;
	identify.nsid = cpu_to_le32(nsid);

	return nvme_cmd_identify(fd, &identify, buf, size);
}

/**
 * @brief Get Active Namespace ID List
 * 
 * @param nsid This field may be cleared to 0h to retrive a Namespace List
 *  including the namespace starting with NSID of 1h.
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_identify_ns_list_active(int fd, void *buf, uint32_t size, 
	uint32_t nsid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ns_list_active(fd, buf, size, nsid);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_reap_expect_cqe(fd, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, 0);
	if (ret < 0)
		return ret;
	
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_identify_ns_list_allocated(int fd, void *buf, uint32_t size, 
	uint32_t nsid)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.cns = NVME_ID_CNS_NS_PRESENT_LIST;
	identify.nsid = cpu_to_le32(nsid);

	return nvme_cmd_identify(fd, &identify, buf, size);
}

/**
 * @brief Get Allocated Namespace ID List
 * 
 * @param nsid This field may be cleared to 0h to retrive a Namespace List
 *  including the namespace starting with NSID of 1h.
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_identify_ns_list_allocated(int fd, void *buf, uint32_t size, 
	uint32_t nsid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ns_list_allocated(fd, buf, size, nsid);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_reap_expect_cqe(fd, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, 0);
	if (ret < 0)
		return ret;
	
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_identify_ctrl(int fd, struct nvme_id_ctrl *ctrl)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.cns = NVME_ID_CNS_CTRL;

	return nvme_cmd_identify(fd, &identify, ctrl, sizeof(*ctrl));
}

/**
 * @brief Get Identify Controller data structure for the controller 
 *  processing the command.
 * 
 * @param fd NVMe device file descriptor
 * @param ctrl point to the identify controller data structure
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_identify_ctrl(int fd, struct nvme_id_ctrl *ctrl)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ctrl(fd, ctrl);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_reap_expect_cqe(fd, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, 0);
	if (ret < 0)
		return ret;
	
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 * 
 * @note It that specified namespace is an inactive NSID, then the controller
 *  returns a zero filled data structure.
 */
int nvme_cmd_identify_ns_active(int fd, struct nvme_id_ns *ns, uint32_t nsid)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.nsid = cpu_to_le32(nsid);
	identify.cns = NVME_ID_CNS_NS;

	return nvme_cmd_identify(fd, &identify, ns, sizeof(*ns));
}

/**
 * @brief Get Identify Namespace data structure for the specified NSID or
 *  the common namespace capabilities for the NVM Command Set
 * 
 * @param fd NVMe device file descriptor
 * @param ns point to the identify namespace data structure
 * @param nsid An active NSID
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_identify_ns_active(int fd, struct nvme_id_ns *ns, uint32_t nsid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ns_active(fd, ns, nsid);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_reap_expect_cqe(fd, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, 0);
	if (ret < 0)
		return ret;
	
	return 0;
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 * 
 * @note It that specified namespace is an unallocated NSID, then the controller
 *  returns a zero filled data structure.
 */
int nvme_cmd_identify_ns_allocated(int fd, struct nvme_id_ns *ns, uint32_t nsid)
{
	struct nvme_identify identify = {0};

	identify.opcode = nvme_admin_identify;
	identify.nsid = cpu_to_le32(nsid);
	identify.cns = NVME_ID_CNS_NS_PRESENT;

	return nvme_cmd_identify(fd, &identify, ns, sizeof(*ns));
}

/**
 * @brief Get Identify Namespace data structure for the specified NSID or
 *  the common namespace capabilities for the NVM Command Set
 * 
 * @param fd NVMe device file descriptor
 * @param ns point to the identify namespace data structure
 * @param nsid An allocated NSID
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_identify_ns_allocated(int fd, struct nvme_id_ns *ns, uint32_t nsid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_identify_ns_allocated(fd, ns, nsid);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_reap_expect_cqe(fd, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, 0);
	if (ret < 0)
		return ret;
	
	return 0;
}
