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

#ifndef _UAPI_LIB_NVME_IDENTIFY_H_
#define _UAPI_LIB_NVME_IDENTIFY_H_

int nvme_ctrl_support_copy_cmd(struct nvme_ctrl_instance *ctrl);
int nvme_ctrl_support_write_protect(struct nvme_ctrl_instance *ctrl);

int nvme_ctrl_support_sgl(struct nvme_ctrl_instance *ctrl);
int nvme_ctrl_support_keyed_sgl_data_block(struct nvme_ctrl_instance *ctrl);
int nvme_ctrl_support_sgl_bit_bucket(struct nvme_ctrl_instance *ctrl);
int nvme_ctrl_support_sgl_data_block(struct nvme_ctrl_instance *ctrl);

int nvme_ns_support_zns_command_set(struct nvme_ns_instance *ns);

int nvme_id_ctrl_npss(struct nvme_ctrl_instance *ctrl);
int nvme_id_ctrl_vid(struct nvme_ctrl_instance *ctrl);
int nvme_id_ctrl_hmminds(struct nvme_ctrl_instance *ctrl, uint32_t *hmminds);
int nvme_id_ctrl_hmmaxd(struct nvme_ctrl_instance *ctrl);
int nvme_id_ctrl_nn(struct nvme_ctrl_instance *ctrl, uint32_t *nn);
int nvme_id_ctrl_sgls(struct nvme_ctrl_instance *ctrl, uint32_t *sgls);

int nvme_id_ns_nsze(struct nvme_ns_group *grp, uint32_t nsid, uint64_t *nsze);
int nvme_id_ns_flbas(struct nvme_ns_group *grp, uint32_t nsid);
int nvme_id_ns_mc(struct nvme_ns_group *grp, uint32_t nsid);
int nvme_id_ns_mssrl(struct nvme_ns_group *grp, uint32_t nsid);
int nvme_id_ns_mcl(struct nvme_ns_group *grp, uint32_t nsid, uint32_t *mcl);
int nvme_id_ns_msrc(struct nvme_ns_group *grp, uint32_t nsid);
int nvme_id_ns_ms(struct nvme_ns_group *grp, uint32_t nsid);
int nvme_id_ns_lbads(struct nvme_ns_group *grp, uint32_t nsid, uint32_t *lbads);

#endif /* _UAPI_LIB_NVME_IDENTIFY_H_ */
