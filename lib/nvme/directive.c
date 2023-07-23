/**
 * @file directive.c
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-04-27
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <errno.h>

#include "byteorder.h"
#include "libbase.h"
#include "libnvme.h"

static int nvme_cmd_directive(int fd, struct nvme_directive_cmd *dir, 
	void *buf, uint32_t size)
{
	struct nvme_64b_cmd cmd = {0};

	cmd.sqid = NVME_AQ_ID;
	cmd.cmd_buf_ptr = dir;
	cmd.bit_mask = NVME_MASK_PRP1_PAGE | NVME_MASK_PRP2_PAGE;
	cmd.data_dir = DMA_BIDIRECTIONAL;
	cmd.data_buf_ptr = buf;
	cmd.data_buf_size = size;

	return nvme_submit_64b_cmd(fd, &cmd);
}

static int nvme_cmd_dir_receive(int fd, struct nvme_directive_cmd *dir, 
	void *buf, uint32_t size)
{
	dir->opcode = nvme_admin_directive_recv;
	return nvme_cmd_directive(fd, dir, buf, size);
}

static int nvme_cmd_dir_send(int fd, struct nvme_directive_cmd *dir, 
	void *buf, uint32_t size)
{
	dir->opcode = nvme_admin_directive_send;
	return nvme_cmd_directive(fd, dir, buf, size);
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_dir_rcv_id_param(struct nvme_dev_info *ndev, 
	struct nvme_dir_identify_params *param, uint32_t nsid)
{
	struct nvme_directive_cmd dir = {0};

	dir.nsid = cpu_to_le32(nsid);
	dir.numd = cpu_to_le32(sizeof(*param) / 4 - 1);
	dir.doper = NVME_DIR_RCV_ID_OP_PARAM;
	dir.dtype = NVME_DIR_IDENTIFY;

	return nvme_cmd_dir_receive(ndev->fd, &dir, param, sizeof(*param));
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_dir_snd_id_enable(struct nvme_dev_info *ndev, 
	uint32_t nsid, uint8_t type, bool enable)
{
	struct nvme_directive_cmd dir = {0};

	/* The Identify Directive is always enabled. */
	WARN_ON(NVME_DIR_IDENTIFY == type);

	dir.nsid = cpu_to_le32(nsid);
	dir.doper = NVME_DIR_SND_ID_OP_ENABLE;
	dir.dtype = NVME_DIR_IDENTIFY;
	if (enable)
		dir.dw12.endir = NVME_DIR_ENDIR;
	dir.dw12.tdtype = type;

	return nvme_cmd_dir_send(ndev->fd, &dir, NULL, 0);
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_dir_rcv_st_param(struct nvme_dev_info *ndev,
	struct nvme_dir_streams_params *param, uint32_t nsid)
{
	struct nvme_directive_cmd dir = {0};

	dir.nsid = cpu_to_le32(nsid);
	dir.numd = cpu_to_le32(sizeof(*param) / 4 - 1);
	dir.doper = NVME_DIR_RCV_ST_OP_PARAM;
	dir.dtype = NVME_DIR_STREAMS;

	return nvme_cmd_dir_receive(ndev->fd, &dir, param, sizeof(*param));
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_dir_rcv_st_status(struct nvme_dev_info *ndev,
	void *buf, uint32_t size, uint32_t nsid)
{
	struct nvme_directive_cmd dir = {0};

	if (!IS_ALIGNED(size, 4)) {
		pr_err("size:0x%x shall align to 4!\n", size);
		return -EINVAL;
	}

	dir.nsid = cpu_to_le32(nsid);
	dir.numd = cpu_to_le32(size / 4 - 1);
	dir.doper = NVME_DIR_RCV_ST_OP_STATUS;
	dir.dtype = NVME_DIR_STREAMS;

	return nvme_cmd_dir_receive(ndev->fd, &dir, buf, size);
}

/**
 * @param req specify the number of stream resources the host is requesting
 *  be allocated for exclusive use by the namespace specified.
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_dir_rcv_st_resource(struct nvme_dev_info *ndev,
	uint32_t nsid, uint16_t req)
{
	struct nvme_directive_cmd dir = {0};

	dir.nsid = cpu_to_le32(nsid);
	dir.doper = NVME_DIR_RCV_ST_OP_RESOURCE;
	dir.dtype = NVME_DIR_STREAMS;
	dir.dw12.nsr = cpu_to_le16(req);

	return nvme_cmd_dir_receive(ndev->fd, &dir, NULL, 0);
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_dir_snd_st_rel_id(struct nvme_dev_info *ndev,
	uint32_t nsid, uint16_t id)
{
	struct nvme_directive_cmd dir = {0};

	dir.nsid = cpu_to_le32(nsid);
	dir.doper = NVME_DIR_SND_ST_OP_REL_ID;
	dir.dtype = NVME_DIR_STREAMS;
	dir.dspec = cpu_to_le16(id);

	return nvme_cmd_dir_send(ndev->fd, &dir, NULL, 0);
}

/**
 * @return The assigned command identifier if success, otherwise a negative
 *  errno.
 */
int nvme_cmd_dir_snd_st_rel_rsc(struct nvme_dev_info *ndev,
	uint32_t nsid)
{
	struct nvme_directive_cmd dir = {0};

	dir.nsid = cpu_to_le32(nsid);
	dir.doper = NVME_DIR_SND_ST_OP_REL_RSC;
	dir.dtype = NVME_DIR_STREAMS;

	return nvme_cmd_dir_send(ndev->fd, &dir, NULL, 0);
}

/**
 * @brief Get identify directive parameters
 * 
 * @details Get a data structure that contains a bit vector specifying the
 *  Directive Types supported by the controller and a bit vector specifying
 *  the Directive Types enabled for the namespace.
 * 
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_dir_rcv_id_param(struct nvme_dev_info *ndev, 
	struct nvme_dir_identify_params *param, uint32_t nsid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_dir_rcv_id_param(ndev, param, nsid);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
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
 * @brief Enable or disable the specified directive type.
 * 
 * @param enable Enable directive if true, disable directive if false.
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_dir_snd_id_enable(struct nvme_dev_info *ndev, 
	uint32_t nsid, uint8_t type, bool enable)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_dir_snd_id_enable(ndev, nsid, type, enable);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
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
 * @brief Get a data structure that specifies the features and capabilities
 *  supported by the Streams Directive, including namespace specific values.
 * 
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_dir_rcv_st_param(struct nvme_dev_info *ndev,
	struct nvme_dir_streams_params *param, uint32_t nsid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_dir_rcv_st_param(ndev, param, nsid);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
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
 * @brief Get information about the status of currently open streams for 
 *  the specified namespace.
 * 
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_dir_rcv_st_status(struct nvme_dev_info *ndev,
	void *buf, uint32_t size, uint32_t nsid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_dir_rcv_st_status(ndev, buf, size, nsid);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
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
 * @brief Allocaate resources for the exclusive use for the specified namespace
 * 
 * @return the nubmer of streams resources that have been allocated for
 *  exclusive use by the namespace specified if success, otherwise a 
 *  negative errno.
 * 
 * @note To allocate additional streams resources, the host should release
 *  resources and request a complete set of resources.
 */
int nvme_dir_rcv_st_resource(struct nvme_dev_info *ndev,
	uint32_t nsid, uint16_t req)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_dir_rcv_st_resource(ndev, nsid, req);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;

	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, 0);
	if (ret < 0)
		return ret;
	
	return (int)le16_to_cpu(entry.result.u16);
}

/**
 * @brief Release identifier
 * 
 * @param id steam identifier
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_dir_snd_st_rel_id(struct nvme_dev_info *ndev,
	uint32_t nsid, uint16_t id)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_dir_snd_st_rel_id(ndev, nsid, id);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
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
 * @brief Release all streams resources allocated for the exclusive use
 *  of the namespace attached to all controllers.
 * 
 * @return 0 on success, otherwise a negative errno.
 */
int nvme_dir_snd_st_rel_rsc(struct nvme_dev_info *ndev,
	uint32_t nsid)
{
	struct nvme_completion entry = {0};
	uint16_t cid;
	int ret;

	ret = nvme_cmd_dir_snd_st_rel_rsc(ndev, nsid);
	if (ret < 0)
		return ret;
	cid = ret;

	ret = nvme_ring_sq_doorbell(ndev->fd, NVME_AQ_ID);
	if (ret < 0)
		return ret;
	
	ret = nvme_gnl_cmd_reap_cqe(ndev, NVME_AQ_ID, 1, &entry, sizeof(entry));
	if (ret != 1) {
		pr_err("expect reap 1, actual reaped %d!\n", ret);
		return ret < 0 ? ret : -ETIME;
	}

	ret = nvme_valid_cq_entry(&entry, NVME_AQ_ID, cid, 0);
	if (ret < 0)
		return ret;
	
	return 0;
}
