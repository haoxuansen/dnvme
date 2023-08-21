/**
 * @file meta.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-12-16
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef _UAPI_LIB_NVME_META_H_
#define _UAPI_LIB_NVME_META_H_

int nvme_create_meta_node(int fd, struct nvme_meta_create *mc);
int nvme_delete_meta_node(int fd, uint16_t id);

/**
 * @brief Map contiguous meta node to user space
 * 
 * @param id meta node identifier
 * @param size meta node size
 * @return meta buffer pointer if success, otherwise returns NULL.
 */
static inline void *nvme_map_meta_node(int fd, uint16_t id, uint32_t size)
{
	return nvme_mmap(fd, id, size, NVME_VMPGOFF_TYPE_META);
}

static inline int nvme_unmap_meta_node(void *meta, uint32_t size)
{
	return munmap(meta, size);
}

#endif /* !_UAPI_LIB_NVME_META_H_ */
