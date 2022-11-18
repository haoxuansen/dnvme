/*
 * NVM Express Compliance Suite
 * Copyright (c) 2011, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _SYSDNVME_H_
#define _SYSDNVME_H_

#define APPNAME         "dnvme"
#define LEVEL           APPNAME

/* Debug flag for IOCT_SEND_64B module */
#define TEST_PRP_DEBUG

/**
 * Absract the differences in trying to make this driver run within QEMU and
 * also within real world 64 bit platforms agaisnt real hardware.
 */
inline u64 READQ(const volatile void __iomem *addr);
inline void WRITEQ(u64 val, volatile void __iomem *addr);


#endif /* sysdnvme.h */
