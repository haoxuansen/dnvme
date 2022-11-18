/**
 * @file core.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2022-11-09
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef _DNVME_CORE_H_
#define _DNVME_CORE_H_

/* Debug flag for IOCT_SEND_64B module */
#define TEST_PRP_DEBUG

/**
 * Absract the differences in trying to make this driver run within QEMU and
 * also within real world 64 bit platforms agaisnt real hardware.
 */
inline u64 READQ(const volatile void __iomem *addr);
inline void WRITEQ(u64 val, volatile void __iomem *addr);


#endif /* !_DNVME_CORE_H_ */
