#/bin/bash

CUR_DIR=`pwd`
TOP_DIR=$CUR_DIR/..

source $TOP_DIR/scripts/log.sh

AUTO_HEADER="${CUR_DIR}/auto_header.h"

RC_BDF_PATCH=
DEV_BDF_PATCH=
RC_PCIE_CAP_OFST=
RC_CAP_LINK_CONTROL=
RC_CAP_LINK_CONTROL2=

app_acquire_devinfo()
{
	local nvme_dev=`lspci -PP | grep "Non-Volatile memory controller"`
	local nvme_path
	local pci_cap

	if [ ! -n "$nvme_dev" ]; then
		ERR "failed to find Non-Volatile memory controller"
		exit 1
	fi
	NOTICE "Find NVMe device, try to acquire devinfo...$"

	nvme_path=${nvme_dev%% *}
	RC_BDF_PATCH=${nvme_path%%/*}
	DEV_BDF_PATCH=${nvme_path#*/}

	pci_cap=`sudo lspci -s "${RC_BDF_PATCH}" -vvv | grep " Express (v2)"`
	pci_cap=${pci_cap#*[}
	RC_PCIE_CAP_OFST=${pci_cap%%]*}
	RC_CAP_LINK_CONTROL=`printf "%x" $((16#${RC_PCIE_CAP_OFST}+0x10))`
	RC_CAP_LINK_CONTROL2=`printf "%x" $((16#${RC_PCIE_CAP_OFST}+0x30))`

	INFO "\tParse devinfo ok!"
}

app_generate_autoheader()
{
	NOTICE "Auto generate header file...$"
	rm -f $AUTO_HEADER
	echo "#ifndef __AUTO_HEADER_H__">>${AUTO_HEADER}
	echo "#define __AUTO_HEADER_H__">>${AUTO_HEADER}
	echo "">>${AUTO_HEADER}
	echo "#define RC_BDF_PATCH \"${RC_BDF_PATCH}\"">>${AUTO_HEADER}
	echo "#define DEV_BDF_PATCH \"${DEV_BDF_PATCH}\"">>${AUTO_HEADER}
	echo "#define RC_PCIE_CAP_OFST 0x${RC_PCIE_CAP_OFST}">>${AUTO_HEADER}
	echo "#define RC_CAP_LINK_CONTROL \"${RC_BDF_PATCH} ${RC_CAP_LINK_CONTROL}\"">>${AUTO_HEADER}
	echo "#define RC_CAP_LINK_CONTROL2 \"${RC_BDF_PATCH} ${RC_CAP_LINK_CONTROL2}\"">>${AUTO_HEADER}
	echo "#define RC_BRIDGE_CONTROL \"${RC_BDF_PATCH} 3E\"">>${AUTO_HEADER}
	echo "">>${AUTO_HEADER}
	echo "#endif /* __AUTO_HEADER_H_ */">>${AUTO_HEADER}

	INFO "\tGenerate header file ok!"
}

app_acquire_devinfo
app_generate_autoheader
