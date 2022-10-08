/**
 * @file dnvme_cmb.h
 * @author yumeng (imyumeng@qq.com)
 * @brief 
 * @version 0.1
 * @date 2021-05-11
 * 
 * @copyright Copyright (c) 2021
 * 
 */


#ifndef _DNVME_CMB_H_
#define _DNVME_CMB_H_

// #define DNVME_CMB_EN

#pragma pack(push)
#pragma pack(4)

#pragma pack(pop)



#ifdef DNVME_CMB_EN
void nvme_map_cmb(struct nvme_device *dev);
void nvme_release_cmb(struct nvme_device *dev);
#else
static inline void nvme_map_cmb(struct nvme_device *dev)
{
    return;
}
static inline void nvme_release_cmb(struct nvme_device *dev)
{
    return;
}

#endif


#endif
