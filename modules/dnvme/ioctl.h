/**
 * @file ioctl.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-06
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _DNVME_IOCTL_H_
#define _DNVME_IOCTL_H_

int dnvme_set_device_state(struct nvme_context *ctx, enum nvme_state state);

int dnvme_generic_read(struct nvme_context *ctx, struct nvme_access __user *uaccess);
int dnvme_generic_write(struct nvme_context *ctx, struct nvme_access __user *uaccess);

int dnvme_get_sq_info(struct nvme_context *ctx, struct nvme_sq_public __user *usqp);
int dnvme_get_cq_info(struct nvme_context *ctx, struct nvme_cq_public __user *ucqp);

int dnvme_get_capability(struct nvme_device *ndev, struct nvme_get_cap __user *ucap);

int dnvme_create_admin_queue(struct nvme_context *ctx, 
	struct nvme_admin_queue __user *uaq);

int dnvme_prepare_sq(struct nvme_context *ctx, struct nvme_prep_sq __user *uprep);
int dnvme_prepare_cq(struct nvme_context *ctx, struct nvme_prep_cq __user *uprep);

int dnvme_create_meta_pool(struct nvme_context *ctx, u32 size);
void dnvme_destroy_meta_pool(struct nvme_context *ctx);
int dnvme_create_meta_node(struct nvme_context *ctx, u32 id);
void dnvme_delete_meta_node(struct nvme_context *ctx, u32 id);
int dnvme_compare_meta_node(struct nvme_context *ctx, 
	struct nvme_cmp_meta __user *ucmp);

#endif /* !_DNVME_IOCTL_H_ */
