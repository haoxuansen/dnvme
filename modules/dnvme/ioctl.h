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

int dnvme_set_device_state(struct nvme_device *ndev, enum nvme_state state);

int dnvme_generic_read(struct nvme_device *ndev, struct nvme_access __user *uaccess);
int dnvme_generic_write(struct nvme_device *ndev, struct nvme_access __user *uaccess);

int dnvme_get_sq_info(struct nvme_device *ndev, struct nvme_sq_public __user *usqp);
int dnvme_get_cq_info(struct nvme_device *ndev, struct nvme_cq_public __user *ucqp);
int dnvme_get_dev_info(struct nvme_device *ndev, struct nvme_dev_public __user *udevp);

int dnvme_get_capability(struct nvme_device *ndev, struct nvme_get_cap __user *ucap);

int dnvme_create_admin_queue(struct nvme_device *ndev, 
	struct nvme_admin_queue __user *uaq);

int dnvme_prepare_sq(struct nvme_device *ndev, struct nvme_prep_sq __user *uprep);
int dnvme_prepare_cq(struct nvme_device *ndev, struct nvme_prep_cq __user *uprep);

int dnvme_alloc_hmb(struct nvme_device *ndev, struct nvme_hmb_alloc __user *uhmb);
int dnvme_release_hmb(struct nvme_device *ndev);

#endif /* !_DNVME_IOCTL_H_ */
