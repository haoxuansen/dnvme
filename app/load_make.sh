#!/bin/bash
#########################################################################
#
# Name:     Load
# Purpose:  Load make & run  
##########################################################################


sh_lsdev=`lspci -PP|grep "Non-Volatile memory controller"`
sh_cpu=`sudo dmidecode -t 4 |grep "Advanced Micro Devices, Inc."`

if [ -n "$sh_lsdev" ] ; then
    echo "Find device!"
else
    echo -e "\033[41;37mWARNING: NOT FIND Non-Volatile memory controller!!!\033[0m"  
    exit 0
fi

AUTO_HEADER="${PWD}/auto_header.h"
echo ${AUTO_HEADER}

rm -f ${AUTO_HEADER}
echo "#ifndef __AUTO_HEADER_H__">>${AUTO_HEADER}
echo "#define __AUTO_HEADER_H__">>${AUTO_HEADER}
echo "">>${AUTO_HEADER}

if [ -n "$sh_cpu" ] ; then
    echo "#define AMD_MB_EN">>${AUTO_HEADER}
fi

echo -e "${sh_lsdev}"
# sh_lsdev=${sh_lsdev##*+-}
# sh_lspci=${sh_lsdev##* }
# sh_lspci=${sh_lsdev% *}
sh_lspci=${sh_lsdev%% *}
echo "${sh_lspci}"

rc_bdf_path="${sh_lspci:0-15:7}"
echo "#define RC_BDF_PATCH \"${rc_bdf_path}\"">>${AUTO_HEADER}
ep_bdf_path="${sh_lspci:0-7}"
echo "#define DEV_BDF_PATCH \"${ep_bdf_path}\"">>${AUTO_HEADER}

sh_lspci_rc=`sudo lspci -s "${rc_bdf_path}" -vvv |grep " Express (v2)"`
echo -e "${sh_lspci_rc}"
sh_lspci_rc=${sh_lspci_rc#*[}
sh_lspci_rc=${sh_lspci_rc%]*}
echo "0x${sh_lspci_rc}"
echo "#define RC_PCIE_CAP_OFST 0x${sh_lspci_rc}">>${AUTO_HEADER}

CAP_LINK_CONTROL=`printf "%x" $((16#${sh_lspci_rc}+0x10))`
CAP_LINK_CONTROL2=`printf "%x" $((16#${sh_lspci_rc}+0x30))`

echo "#define RC_CAP_LINK_CONTROL \"${rc_bdf_path} ${CAP_LINK_CONTROL}\"">>${AUTO_HEADER}
echo "#define RC_CAP_LINK_CONTROL2 \"${rc_bdf_path} ${CAP_LINK_CONTROL2}\"">>${AUTO_HEADER}
echo "#define RC_BRIDGE_CONTROL \"${rc_bdf_path} 3E\"">>${AUTO_HEADER}

echo "">>${AUTO_HEADER}
echo "#endif /* __AUTO_HEADER_H_ */">>${AUTO_HEADER}

echo -e "\033[32m----- make! -----\033[0m"    
sudo make clean 
make -j
echo -e "\n\033[32mrun test!\033[0m" 
sudo ./test /dev/nvme0

##########################################################################
# 
#END:  Load script 
#
#########################################################################




