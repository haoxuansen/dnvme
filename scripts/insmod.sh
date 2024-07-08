#!/bin/bash

TOP_DIR=`pwd`/..

# nvme ----------+--> nvme_core --> nvme_common
# nvme_fabrics --+
MOD_NVME=nvme
MOD_NVME_FABRICS=nvme_fabrics
MOD_NVME_CORE=nvme_core
MOD_NVME_COMMON=nvme_common
MOD_DNVME=dnvme

MOD_NVME_EXIST=yes
MOD_NVME_FABRICS_EXIST=yes
MOD_NVME_CORE_EXIST=yes
MOD_NVME_COMMON_EXIST=yes
MOD_DNVME_EXIST=yes

source $TOP_DIR/scripts/log.sh

# --------------------------------------------------------------------------- #
# Function
# --------------------------------------------------------------------------- #

# $1: module name
# $2: remove or not
nvme_remove_module()
{
	if [[ "$2" = "yes" ]]; then
		NOTICE "Remove modules |$1|...$"
		sudo rmmod $1
		if [[ "$?" -ne 0 ]]; then
			ERR "\tfailed to rmmod $1"
			exit 1
		else
			INFO "\trmmod $1 ok"
		fi
	fi
}

# $1: module name
nvme_insmod_module()
{
	NOTICE "Insmod modules |$1|...$"
	sudo insmod $1.ko
	if [[ "$?" -ne 0 ]]; then
		ERR "\tfailed to insmod $1"
		exit 1
	else
		INFO "\tinsmod $1 ok"
	fi
}

# $1: module name
# $2: method
nvme_check_mod_exist()
{
	local find

	NOTICE "Check whether the module named \"$1\" exist?"
	if [ $# -eq 2 ]; then
		find=`lsmod | egrep "^$1[^_]" | awk '{print $1}'`
	else
		find=`lsmod | grep "$1" | awk '{print $1}'`
	fi

	if [ "$find" == "" ]; then
		INFO "\"$1\" not exist!"
		return 0
	else
		INFO "\"$1\" exist!"
		return 1
	fi
}

nvme_check_mods_exist()
{
	nvme_check_mod_exist $MOD_NVME 1
	[ $? -eq 0 ] && MOD_NVME_EXIST=no
	nvme_check_mod_exist $MOD_NVME_FABRICS
	[ $? -eq 0 ] && MOD_NVME_FABRICS_EXIST=no
	nvme_check_mod_exist $MOD_NVME_CORE
	[ $? -eq 0 ] && MOD_NVME_CORE_EXIST=no
	nvme_check_mod_exist $MOD_NVME_COMMON
	[ $? -eq 0 ] && MOD_NVME_COMMON_EXIST=no
	nvme_check_mod_exist $MOD_DNVME
	[ $? -eq 0 ] && MOD_DNVME_EXIST=no
}

# --------------------------------------------------------------------------- #
# Execute
# --------------------------------------------------------------------------- #
nvme_check_mods_exist
nvme_remove_module $MOD_NVME $MOD_NVME_EXIST
nvme_remove_module $MOD_NVME_FABRICS $MOD_NVME_FABRICS_EXIST
nvme_remove_module $MOD_NVME_CORE $MOD_NVME_CORE_EXIST
nvme_remove_module $MOD_NVME_COMMON $MOD_NVME_COMMON_EXIST
nvme_remove_module $MOD_DNVME $MOD_DNVME_EXIST
nvme_insmod_module $MOD_DNVME
