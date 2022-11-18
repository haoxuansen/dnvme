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
			sudo rmmod $find
			if [[ "$?" -ne 0 ]]; then
				ERR "\tfailed to rmmod $find"
			else
				INFO "\trmmod $find ok"
			fi
		fi
	done
}

nvme_remove_module $NVME_CORE $NVME $DNVME
nvme_install_module $DNVME
