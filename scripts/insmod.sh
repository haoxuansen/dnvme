#!/bin/bash

TOP_DIR=`pwd`/..

DNVME=dnvme
NVME=nvme
NVME_CORE=nvme_core

source $TOP_DIR/scripts/log.sh

# --------------------------------------------------------------------------- #
# Function
# --------------------------------------------------------------------------- #

nvme_install_module()
{
	local find

	NOTICE "Install modules...$"
	for mod in $@
	do
		find=`lsmod | grep "$mod" | awk '{print $1}'`
		if [ "$find" == "" ]; then
			sudo insmod $mod.ko
			if [[ "$?" -ne 0 ]]; then
				ERR "\tfailed to insmod $mod"
				exit 1
			else
				INFO "\tinsmod $mod ok"
			fi
		fi
	done
}

nvme_remove_module()
{
	local find

	NOTICE "Remove modules...$"
	for mod in $@
	do
		find=`lsmod | grep "$mod" | awk '{print $1}'`
		if [ ! "$find" == "" ]; then
			sudo rmmod $mod
			if [[ "$?" -ne 0 ]]; then
				ERR "\tfailed to rmmod $mod"
				exit 1
			else
				INFO "\trmmod $mod ok"
			fi
		fi
	done
}

# $1: module name
nvme_check_mod_exist()
{
	local find

	NOTICE "Check whether the module named \"$1\" exist?"
	find=`lsmod | grep "$1" | awk '{print $1}'`
	if [ "$find" == "" ]; then
		return 0 # mod not exist
	else
		return 1 # mod exist
	fi
}

# --------------------------------------------------------------------------- #
# Execute
# --------------------------------------------------------------------------- #
nvme_check_mod_exist $DNVME
if [ "$?" == "0" ]; then
	INFO "The driver is loaded for the first time."
	nvme_remove_module $NVME $NVME_CORE
else
	INFO "Reload the driver."
	nvme_remove_module $DNVME
fi

nvme_install_module $DNVME
