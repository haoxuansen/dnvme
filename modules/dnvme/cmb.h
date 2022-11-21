/**
 * @file cmb.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief controller memory buffer
 * @version 0.1
 * @date 2022-11-22
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _DNVME_CMB_H_
#define _DNVME_CMB_H_

struct nvme_device;

#if IS_ENABLED(CONFIG_DNVME_CMB)
int dnvme_map_cmb(struct nvme_device *ndev);
void dnvme_unmap_cmb(struct nvme_device *ndev);
#else
static inline int dnvme_map_cmb(struct nvme_device *ndev) { return 0; }
static inline void dnvme_unmap_cmb(struct nvme_device *ndev) {}
#endif

#endif /* !_DNVME_CMB_H_ */
