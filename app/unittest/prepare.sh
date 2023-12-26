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

# --------------------------------------------------------------------------- #
# Function
# --------------------------------------------------------------------------- #

# $1: BDF, eg: "00:08.0"
# $2: capability name, eg: "Power Management"
lspci_get_pci_cap_offset()
{
	local bdf=$1
	local name=$2
	local desc
	local offset
	local tmp

	# desc format example:
	#   Capabilities: [70] Express (v2) Endpoint, MSI 1f
	desc=`sudo lspci -s "${bdf}" -vvv | grep "${name}"`
	if [ ! "$desc" ]; then
		WARN "Failed to find '$name' capability from '$bdf'!"
		return 1
	fi
	tmp=${desc#*[}
	offset=${tmp%%]*}
	RET_VAL=$offset
	return 0
}

# $1: BDF, eg: "00:08.0"
# $2: capability name, eg: "Latency Tolerance Reporting"
lspci_get_pcie_cap_offset()
{
	local bdf=$1
	local name=$2
	local desc
	local offset
	local tmp

	# desc format example:
	#   Capabilities: [1e8 v1] Latency Tolerance Reporting
	desc=`sudo lspci -s "${bdf}" -vvv | grep "${name}"`
	if [ ! "$desc" ]; then
		WARN "Failed to find '$name' capability from '$bdf'!"
		return 1
	fi
	tmp=${desc#*[}
	offset=${tmp%% *}
	RET_VAL=$offset
	return 0
}

# $1: BDF, eg: "00:08.0"
init_pci_pm_cap()
{
	local bdf=$1
	local offset

	lspci_get_pci_cap_offset $bdf "Power Management"
	if [ $? -ne 0 ]; then
		return 1
	fi
	offset=$RET_VAL
	return 0
}

# $1: BDF, eg: "00:08.0"
init_pci_exp_cap()
{
	local bdf=$1
	local offset

	lspci_get_pci_cap_offset $bdf "Express (v2)"
	if [ $? -ne 0 ]; then
		return 1
	fi
	offset=$RET_VAL

	RC_PCI_EXP_CAP_OFFSET=$offset
	printf -v RC_PCI_EXP_REG_DEVICE_CONTROL "%x" $((16#${offset}+0x8))
	printf -v RC_PCI_EXP_REG_LINK_CAPABILITY "%x" $((16#${offset}+0xc))
	printf -v RC_PCI_EXP_REG_LINK_CONTROL "%x" $((16#${offset}+0x10))
	printf -v RC_PCI_EXP_REG_LINK_CONTROL2 "%x" $((16#${offset}+0x30))
	INFO "Init PCI Express Capability OK!"
	return 0
}

# $1: BDF, eg: "00:08.0"
init_pcie_ltr_cap()
{
	local bdf=$1
	local name="Latency Tolerance Reporting"
	local offset

	lspci_get_pcie_cap_offset $bdf "$name"
	if [ $? -ne 0 ]; then
		return 1
	fi
	offset=$RET_VAL

	RC_PCIE_LTR_CAP_OFFSET=$offset
	INFO "Init PCIe $name Capability OK!"
	return 0
}

# $1: BDF, eg: "00:08.0"
init_pcie_l1ss_cap()
{
	local bdf=$1
	local name="L1 PM Substates"
	local offset

	lspci_get_pcie_cap_offset $bdf "$name"
	if [ $? -ne 0 ]; then
		return 1
	fi
	offset=$RET_VAL

	RC_PCIE_L1SS_CAP_OFFSET=$offset
	printf -v RC_PCIE_L1SS_REG_CAP "%x" $((16#${offset}+0x4))
	printf -v RC_PCIE_L1SS_REG_CTRL1 "%x" $((16#${offset}+0x8))
	printf -v RC_PCIE_L1SS_REG_CTRL2 "%x" $((16#${offset}+0xc))
	printf -v RC_PCIE_L1SS_REG_STATUS "%x" $((16#${offset}+0x10))
	INFO "Init PCIe $name Capability OK!"
	return 0
}

# $1: BDF, eg: "00:08.0"
init_pci_caps()
{
	local bdf=$1

	init_pci_pm_cap $bdf
	init_pci_exp_cap $bdf
	return 0
}

# $1: BDF, eg: "00:08.0"
init_pcie_caps()
{
	local bdf=$1

	init_pcie_ltr_cap $bdf
	init_pcie_l1ss_cap $bdf
	return 0
}

init_pci_bdf()
{
	local desc=`lspci -PP | grep "Non-Volatile memory controller"`
	local path=${desc%% *}

	if [ ! -n "$desc" ]; then
		WARN "Non-Volatile memory controller not exist! skip update \"auto_header.h\""
		# Exit with 0 to avoid makefile reporting error!
		exit 0
	fi
	NOTICE "Find NVMe device, try to acquire devinfo...$"

	RC_BDF_PATCH=${path%%/*}
	DEV_BDF_PATCH=${path#*/}
	return 0
}

# $1: The file name that contains the path.
gen_header_file()
{
	local file=$1
	local cpu=`sudo dmidecode -t 4 | grep "Advanced Micro Devices, Inc."`

	NOTICE "Start to generate header file...$"
	rm -f $file
	echo "#ifndef __AUTO_HEADER_H__">>${file}
	echo "#define __AUTO_HEADER_H__">>${file}
	echo "">>${file}
	if [ -n "$cpu" ] ; then
		echo "#define AMD_MB_EN">>${file}
	fi
	echo "#define RC_BDF_PATCH \"${RC_BDF_PATCH}\"">>${file}
	echo "#define DEV_BDF_PATCH \"${DEV_BDF_PATCH}\"">>${file}

	# Record PCI express capability
	echo "#define RC_PCI_EXP_CAP_OFFSET 0x${RC_PCI_EXP_CAP_OFFSET}">>${file}
	echo "#define RC_PCI_EXP_REG_DEVICE_CONTROL \"${RC_BDF_PATCH} ${RC_PCI_EXP_REG_DEVICE_CONTROL}\"">>${file}
	echo "#define RC_PCI_EXP_REG_LINK_CAPABILITY \"${RC_BDF_PATCH} ${RC_PCI_EXP_REG_LINK_CAPABILITY}\"">>${file}
	echo "#define RC_PCI_EXP_REG_LINK_CONTROL \"${RC_BDF_PATCH} ${RC_PCI_EXP_REG_LINK_CONTROL}\"">>${file}
	echo "#define RC_PCI_EXP_REG_LINK_CONTROL2 \"${RC_BDF_PATCH} ${RC_PCI_EXP_REG_LINK_CONTROL2}\"">>${file}
	echo "#define RC_PCI_HDR_REG_BRIDGE_CONTROL \"${RC_BDF_PATCH} ${RC_PCI_HDR_REG_BRIDGE_CONTROL}\"">>${file}

	# Record PCIe Latency Tolerance Reporting capability
	echo "#define RC_PCIE_LTR_CAP_OFFSET 0x${RC_PCIE_LTR_CAP_OFFSET}">>${file}

	# Record PCIe L1 PM Substates capability
	echo "#define RC_PCIE_L1SS_CAP_OFFSET 0x${RC_PCIE_L1SS_CAP_OFFSET}">>${file}
	echo "#define RC_PCIE_L1SS_REG_CAP \"${RC_BDF_PATCH} ${RC_PCIE_L1SS_REG_CAP}\"">>${file}
	echo "#define RC_PCIE_L1SS_REG_CTRL1 \"${RC_BDF_PATCH} ${RC_PCIE_L1SS_REG_CTRL1}\"">>${file}
	echo "#define RC_PCIE_L1SS_REG_CTRL2 \"${RC_BDF_PATCH} ${RC_PCIE_L1SS_REG_CTRL2}\"">>${file}
	echo "#define RC_PCIE_L1SS_REG_STATUS \"${RC_BDF_PATCH} ${RC_PCIE_L1SS_REG_STATUS}\"">>${file}

	echo "">>${file}
	echo "#endif /* __AUTO_HEADER_H_ */">>${file}

	INFO "Generate header file ok!"
}

# --------------------------------------------------------------------------- #
# Default Setting
# --------------------------------------------------------------------------- #
RET_VAL=

#
# PCI Express Capability
#
RC_PCI_EXP_CAP_OFFSET="0"
# Device Control Register
RC_PCI_EXP_REG_DEVICE_CONTROL="0"
# Link Capabilities Register
RC_PCI_EXP_REG_LINK_CAPABILITY="0"
# Link Control Register
RC_PCI_EXP_REG_LINK_CONTROL="0"
# Link Control 2 Register
RC_PCI_EXP_REG_LINK_CONTROL2="0"

#
# PCIe Latency Tolerance Reporting capability
#
RC_PCIE_LTR_CAP_OFFSET="0"

#
# PCIe L1 PM Substates capability
#
RC_PCIE_L1SS_CAP_OFFSET="0"
# L1 PM Substates Capabilities
RC_PCIE_L1SS_REG_CAP="0"
# L1 PM Substates Control 1
RC_PCIE_L1SS_REG_CTRL1="0"
# L1 PM Substates Control 2
RC_PCIE_L1SS_REG_CTRL2="0"
# L1 PM Substates Status
RC_PCIE_L1SS_REG_STATUS="0"

# --------------------------------------------------------------------------- #
# Execute
# --------------------------------------------------------------------------- #
init_pci_bdf
init_pci_caps $RC_BDF_PATCH
init_pcie_caps $RC_BDF_PATCH
gen_header_file $AUTO_HEADER
