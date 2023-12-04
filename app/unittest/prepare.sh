#!/bin/bash

CUR_DIR=`pwd`
if [[ "$TOP_DIR" = "" ]]; then
	TOP_DIR=$CUR_DIR/../..
fi

source $TOP_DIR/scripts/log.sh

AUTO_HEADER="${CUR_DIR}/auto_header.h"

DEV_BDF_PATCH=

#
# Device Link Partner: May be a Switch or Root Complex
#

RC_BDF_PATCH=
# PCI Configuration Space Header
RC_PCI_HDR_REG_BRIDGE_CONTROL="3E"

# PCI express capability offset
RC_PCI_CAP_OFFSET_EXP=
# Device Control Register
RC_PCI_EXP_REG_DEVICE_CONTROL=
# Link Capabilities Register
RC_PCI_EXP_REG_LINK_CAPABILITY=
# Link Control Register
RC_PCI_EXP_REG_LINK_CONTROL=
# Link Control 2 Register
RC_PCI_EXP_REG_LINK_CONTROL2=

# --------------------------------------------------------------------------- #
# Function
# --------------------------------------------------------------------------- #

app_acquire_devinfo()
{
	local nvme_dev=`lspci -PP | grep "Non-Volatile memory controller"`
	local nvme_path
	local pci_cap

	if [ ! -n "$nvme_dev" ]; then
		WARN "Non-Volatile memory controller not exist! skip update \"auto_header.h\""
		exit 0
	fi
	NOTICE "Find NVMe device, try to acquire devinfo...$"

	nvme_path=${nvme_dev%% *}
	RC_BDF_PATCH=${nvme_path%%/*}
	DEV_BDF_PATCH=${nvme_path#*/}

	pci_cap=`sudo lspci -s "${RC_BDF_PATCH}" -vvv | grep " Express (v2)"`
	pci_cap=${pci_cap#*[}
	RC_PCI_CAP_OFFSET_EXP=${pci_cap%%]*}
	printf -v RC_PCI_EXP_REG_DEVICE_CONTROL "%x" $((16#${RC_PCI_CAP_OFFSET_EXP}+0x8))
	printf -v RC_PCI_EXP_REG_LINK_CAPABILITY "%x" $((16#${RC_PCI_CAP_OFFSET_EXP}+0xc))
	printf -v RC_PCI_EXP_REG_LINK_CONTROL "%x" $((16#${RC_PCI_CAP_OFFSET_EXP}+0x10))
	printf -v RC_PCI_EXP_REG_LINK_CONTROL2 "%x" $((16#${RC_PCI_CAP_OFFSET_EXP}+0x30))

	INFO "\tParse devinfo ok!"
}

app_generate_autoheader()
{
	local cpu=`sudo dmidecode -t 4 | grep "Advanced Micro Devices, Inc."`

	NOTICE "Auto generate header file...$"
	rm -f $AUTO_HEADER
	echo "#ifndef __AUTO_HEADER_H__">>${AUTO_HEADER}
	echo "#define __AUTO_HEADER_H__">>${AUTO_HEADER}
	echo "">>${AUTO_HEADER}
	if [ -n "$cpu" ] ; then
		echo "#define AMD_MB_EN">>${AUTO_HEADER}
	fi
	echo "#define RC_BDF_PATCH \"${RC_BDF_PATCH}\"">>${AUTO_HEADER}
	echo "#define DEV_BDF_PATCH \"${DEV_BDF_PATCH}\"">>${AUTO_HEADER}
	echo "#define RC_PCI_CAP_OFFSET_EXP 0x${RC_PCI_CAP_OFFSET_EXP}">>${AUTO_HEADER}
	echo "#define RC_PCI_EXP_REG_DEVICE_CONTROL \"${RC_BDF_PATCH} ${RC_PCI_EXP_REG_DEVICE_CONTROL}\"">>${AUTO_HEADER}
	echo "#define RC_PCI_EXP_REG_LINK_CAPABILITY \"${RC_BDF_PATCH} ${RC_PCI_EXP_REG_LINK_CAPABILITY}\"">>${AUTO_HEADER}
	echo "#define RC_PCI_EXP_REG_LINK_CONTROL \"${RC_BDF_PATCH} ${RC_PCI_EXP_REG_LINK_CONTROL}\"">>${AUTO_HEADER}
	echo "#define RC_PCI_EXP_REG_LINK_CONTROL2 \"${RC_BDF_PATCH} ${RC_PCI_EXP_REG_LINK_CONTROL2}\"">>${AUTO_HEADER}
	echo "#define RC_PCI_HDR_REG_BRIDGE_CONTROL \"${RC_BDF_PATCH} ${RC_PCI_HDR_REG_BRIDGE_CONTROL}\"">>${AUTO_HEADER}
	echo "">>${AUTO_HEADER}
	echo "#endif /* __AUTO_HEADER_H_ */">>${AUTO_HEADER}

	INFO "\tGenerate header file ok!"
}

# --------------------------------------------------------------------------- #
# Execute
# --------------------------------------------------------------------------- #
app_acquire_devinfo
app_generate_autoheader
