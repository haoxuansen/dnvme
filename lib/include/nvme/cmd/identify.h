/**
 * @file identify.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-08-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

int nvme_ctrl_support_ext_lba_formats(struct nvme_ctrl_instance *ctrl);

int nvme_ctrl_support_copy_cmd(struct nvme_ctrl_instance *ctrl);
int nvme_ctrl_support_write_protect(struct nvme_ctrl_instance *ctrl);

int nvme_ctrl_support_sgl(struct nvme_ctrl_instance *ctrl);
int nvme_ctrl_support_keyed_sgl_data_block(struct nvme_ctrl_instance *ctrl);
int nvme_ctrl_support_sgl_bit_bucket(struct nvme_ctrl_instance *ctrl);
int nvme_ctrl_support_sgl_mptr_sgl(struct nvme_ctrl_instance *ctrl);
int nvme_ctrl_support_sgl_data_block(struct nvme_ctrl_instance *ctrl);

int nvme_ns_support_zns_command_set(struct nvme_ns_instance *ns);

int nvme_id_ctrl_npss(struct nvme_ctrl_instance *ctrl);
int nvme_id_ctrl_vid(struct nvme_ctrl_instance *ctrl);
int nvme_id_ctrl_hmminds(struct nvme_ctrl_instance *ctrl, uint32_t *hmminds);
int nvme_id_ctrl_hmmaxd(struct nvme_ctrl_instance *ctrl);
int nvme_id_ctrl_nn(struct nvme_ctrl_instance *ctrl, uint32_t *nn);
int nvme_id_ctrl_sgls(struct nvme_ctrl_instance *ctrl, uint32_t *sgls);

int nvme_id_ctrl_nvm_vsl(struct nvme_ctrl_instance *ctrl);

int nvme_id_ns_nsze(struct nvme_ns_group *grp, uint32_t nsid, uint64_t *nsze);
int nvme_id_ns_flbas(struct nvme_ns_group *grp, uint32_t nsid);
int nvme_id_ns_mc(struct nvme_ns_group *grp, uint32_t nsid);
int nvme_id_ns_dpc(struct nvme_ns_group *grp, uint32_t nsid);
int nvme_id_ns_dps(struct nvme_ns_group *grp, uint32_t nsid);
int nvme_id_ns_mssrl(struct nvme_ns_group *grp, uint32_t nsid);
int nvme_id_ns_mcl(struct nvme_ns_group *grp, uint32_t nsid, uint32_t *mcl);
int nvme_id_ns_msrc(struct nvme_ns_group *grp, uint32_t nsid);
int nvme_id_ns_ms(struct nvme_ns_group *grp, uint32_t nsid);
int nvme_id_ns_lbads(struct nvme_ns_group *grp, uint32_t nsid, uint32_t *lbads);

int nvme_id_ns_nvm_lbstm(struct nvme_ns_group *grp, uint32_t nsid, uint64_t *lbstm);

int nvme_cmd_identify(int fd, struct nvme_identify *identify, void *buf, 
	uint32_t size);

int nvme_cmd_identify_ns_active(int fd, struct nvme_id_ns *ns, uint32_t nsid);
int nvme_identify_ns_active(struct nvme_dev_info *ndev, struct nvme_id_ns *ns, 
	uint32_t nsid);

int nvme_cmd_identify_ctrl(int fd, struct nvme_id_ctrl *ctrl);
int nvme_identify_ctrl(struct nvme_dev_info *ndev, struct nvme_id_ctrl *ctrl);

int nvme_cmd_identify_ns_list_active(int fd, void *buf, uint32_t size, 
	uint32_t nsid);
int nvme_identify_ns_list_active(struct nvme_dev_info *ndev, void *buf, 
	uint32_t size, uint32_t nsid);

int nvme_cmd_identify_ns_desc_list(int fd, void *buf, uint32_t size,
	uint32_t nsid);
int nvme_identify_ns_desc_list(struct nvme_dev_info *ndev, void *buf, 
	uint32_t size, uint32_t nsid);

int nvme_cmd_identify_cs_ns(int fd, void *buf, uint32_t size, uint32_t nsid,
	uint8_t csi);
int nvme_identify_cs_ns(struct nvme_dev_info *ndev, void *buf, uint32_t size, 
	uint32_t nsid, uint8_t csi);

int nvme_cmd_identify_cs_ctrl(int fd, void *buf, uint32_t size, uint8_t csi);
int nvme_identify_cs_ctrl(struct nvme_dev_info *ndev, void *buf, uint32_t size,
	uint8_t csi);

int nvme_cmd_identify_ns_list_allocated(int fd, void *buf, uint32_t size, 
	uint32_t nsid);
int nvme_identify_ns_list_allocated(struct nvme_dev_info *ndev, void *buf, 
	uint32_t size, uint32_t nsid);

int nvme_cmd_identify_ns_allocated(int fd, struct nvme_id_ns *ns, uint32_t nsid);
int nvme_identify_ns_allocated(struct nvme_dev_info *ndev, 
	struct nvme_id_ns *ns, uint32_t nsid);

int nvme_cmd_identify_ns_attached_ctrl_list(int fd, void *buf, uint32_t size,
	uint32_t nsid, uint16_t cntid);
int nvme_identify_ns_attached_ctrl_list(struct nvme_dev_info *ndev, void *buf, 
	uint32_t size, uint32_t nsid, uint16_t cntid);

int nvme_cmd_identify_ctrl_list(int fd, void *buf, uint32_t size, 
	uint16_t cntid);
int nvme_identify_ctrl_list(struct nvme_dev_info *ndev, void *buf, 
	uint32_t size, uint16_t cntid);

int nvme_cmd_identify_ctrl_csc_list(int fd, struct nvme_id_ctrl_csc *csc, 
	uint16_t cntid);
int nvme_identify_ctrl_csc_list(struct nvme_dev_info *ndev, 
	struct nvme_id_ctrl_csc *csc, uint16_t cntid);
