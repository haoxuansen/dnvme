/**
 * @file cmd.h
 * @author yeqiang_xu (yeqiang_xu@maxio-tech.com)
 * @brief 
 * @version 0.1
 * @date 2023-11-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#define UT_CMD_SEL_ALL			U32_MAX
#define UT_CMD_SEL_IO_READ		BIT(0)
#define UT_CMD_SEL_IO_WRITE		BIT(1)
#define UT_CMD_SEL_IO_COMPARE		BIT(2)
#define UT_CMD_SEL_IO_VERIFY		BIT(3)
#define UT_CMD_SEL_IO_COPY		BIT(4)
#define UT_CMD_SEL_IO_FUSED_CW		BIT(5) /**< Fused<Compare, Write> */
#define UT_CMD_SEL_NUM			6

struct copy_range {
	uint64_t	slba;
	uint32_t	nlb;
};

struct copy_resource {
	void			*desc;
	uint32_t		size;
	uint8_t			format;

	uint64_t		slba; /**< target */

	uint32_t		nr_entry;
	struct copy_range	entry[0];
};

struct copy_resource *ut_alloc_copy_resource(uint32_t nr_desc, uint8_t format);
void ut_release_copy_resource(struct copy_resource *copy);

int ut_submit_io_read_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb);
int ut_submit_io_read_cmds(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, int nr_cmd);
int ut_submit_io_read_cmd_random_region(struct case_data *priv, 
	struct nvme_sq_info *sq);
int ut_submit_io_read_cmds_random_region(struct case_data *priv,
	struct nvme_sq_info *sq, int nr_cmd);

int ut_submit_io_write_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb);
int ut_submit_io_write_cmds(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, int nr_cmd);
int ut_submit_io_write_cmd_random_data(struct case_data *priv, 
	struct nvme_sq_info *sq, uint64_t slba, uint32_t nlb);
int ut_submit_io_write_cmd_random_d_r(struct case_data *priv,
	struct nvme_sq_info *sq);
int ut_submit_io_write_cmds_random_d_r(struct case_data *priv,
	struct nvme_sq_info *sq, int nr_cmd);

int ut_submit_io_compare_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb);
int ut_submit_io_compare_cmds(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, int nr_cmd);
int ut_submit_io_compare_cmd_random_region(struct case_data *priv, 
	struct nvme_sq_info *sq);
int ut_submit_io_compare_cmds_random_region(struct case_data *priv,
	struct nvme_sq_info *sq, int nr_cmd);

int ut_submit_io_verify_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb);
int ut_submit_io_verify_cmds(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, int nr_cmd);
int ut_submit_io_verify_cmd_random_region(struct case_data *priv, 
	struct nvme_sq_info *sq);

int ut_submit_io_copy_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	struct copy_resource *copy);
int ut_submit_io_copy_cmds(struct case_data *priv, struct nvme_sq_info *sq,
	struct copy_resource *copy, int nr_cmd);

int ut_submit_io_fused_cw_cmd(struct case_data *priv, struct nvme_sq_info *sq, 
	uint64_t slba, uint32_t nlb);
int ut_submit_io_fused_cw_cmds(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb, int nr_cmd);
int ut_submit_io_fused_cw_cmd_random_region(
	struct case_data *priv, struct nvme_sq_info *sq);

int ut_submit_io_zone_append_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t zslba, uint32_t nlb);

int ut_submit_io_random_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint32_t select);
int ut_submit_io_random_cmds(struct case_data *priv, struct nvme_sq_info *sq,
	uint32_t select, int nr_cmd);
int ut_submit_io_random_cmds_to_sqs(struct case_data *priv, 
	struct nvme_sq_info **sq, uint32_t select, int nr_cmd, int nr_sq);

int ut_send_io_read_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb);
int ut_send_io_read_cmd_random_region(struct case_data *priv, 
	struct nvme_sq_info *sq);
int ut_send_io_read_cmds_random_region(struct case_data *priv,
	struct nvme_sq_info *sq, int nr_cmd);

int ut_send_io_write_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb);
int ut_send_io_write_cmd_random_data(struct case_data *priv, 
	struct nvme_sq_info *sq, uint64_t slba, uint32_t nlb);
int ut_send_io_write_cmd_random_d_r(struct case_data *priv,
	struct nvme_sq_info *sq);
int ut_send_io_write_cmds_random_d_r(struct case_data *priv,
	struct nvme_sq_info *sq, int nr_cmd);

int ut_send_io_compare_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb);
int ut_send_io_compare_cmd_random_region(struct case_data *priv, 
	struct nvme_sq_info *sq);

int ut_send_io_verify_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t slba, uint32_t nlb);
int ut_send_io_verify_cmd_random_region(struct case_data *priv, 
	struct nvme_sq_info *sq);

int ut_send_io_copy_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	struct copy_resource *copy);
int ut_send_io_copy_cmd_random_region(struct case_data *priv, 
	struct nvme_sq_info *sq);

int ut_send_io_fused_cw_cmd(struct case_data *priv, 
	struct nvme_sq_info *sq, uint64_t slba, uint32_t nlb);
int ut_send_io_fused_cw_cmd_random_region(
	struct case_data *priv, struct nvme_sq_info *sq);

int ut_send_io_zone_append_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint64_t zslba, uint32_t nlb);

int ut_send_io_random_cmd(struct case_data *priv, struct nvme_sq_info *sq,
	uint32_t select);
int ut_send_io_random_cmds(struct case_data *priv, struct nvme_sq_info *sq,
	uint32_t select, int nr_cmd);
int ut_send_io_random_cmds_to_sqs(struct case_data *priv, 
	struct nvme_sq_info **sq, uint32_t select, int nr_cmd, int nr_sq);

