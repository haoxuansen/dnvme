/**
 * @file libnvme.h
 * @author yeqiang_xu <yeqiang_xu@maxio-tech.com>
 * @brief 
 * @version 0.1
 * @date 2023-07-21
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _UAPI_LIBNVME_H_
#define _UAPI_LIBNVME_H_

#include <stdint.h> /* for "uint32_t" ... */
#include <sys/mman.h> /** for "@mmap" */

#include "byteorder.h"
#include "pci_caps.h"
#include "pci_ids_ext.h"
#include "pci_regs_ext.h"
#include "dnvme.h"

#include "nvme/core.h"
#include "nvme/cmd/cmd.h"
#include "nvme/cmd/directive.h"
#include "nvme/cmd/feature.h"
#include "nvme/cmd/identify.h"
#include "nvme/cmd/vendor.h"
#include "nvme/cmd/zns.h"
#include "nvme/ioctl.h"
#include "nvme/irq.h"
#include "nvme/meta.h"
#include "nvme/netlink.h"
#include "nvme/pcie.h"
#include "nvme/pci.h"
#include "nvme/property.h"
#include "nvme/queue.h"

#endif /* !_UAPI_LIBNVME_H_ */
