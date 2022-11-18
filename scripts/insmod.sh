#!/bin/bash

TOP_DIR=`pwd`/..

DNVME=dnvme
NVME=nvme
NVME_CORE=nvme_core

source $TOP_DIR/scripts/log.sh

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

nvme_remove_module $NVME $NVME_CORE $DNVME
nvme_install_module $DNVME
