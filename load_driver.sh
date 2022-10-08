#!/bin/bash
#########################################################################
#
# Name:     Load
# Purpose:  Load dnvme Driver 
##########################################################################

rmmodule=nvme
insmodule=dnvme

#  
#下面是字体输出颜色及终端格式控制  
# 字体色范围：30-37  
# echo -e "\033[30m 黑色字 \033[0m"  
# echo -e "\033[31m 红色字 \033[0m"  
# echo -e "\033[32m 绿色字 \033[0m"  
# echo -e "\033[33m 黄色字 \033[0m"  
# echo -e "\033[34m 蓝色字 \033[0m"  
# echo -e "\033[35m 紫色字 \033[0m"  
# echo -e "\033[36m 天蓝字 \033[0m"  
# echo -e "\033[37m 白色字 \033[0m"  
# #字背景颜色范围：40-47  
# echo -e "\033[40;37m 黑底白字 \033[0m"  
# echo -e "\033[41;37m 红底白字 \033[0m"  
# echo -e "\033[42;34m 绿底蓝字 \033[0m"  
# echo -e "\033[43;34m 黄底蓝字 \033[0m"  
# echo -e "\033[44;37m 蓝底白字 \033[0m"  
# echo -e "\033[45;30m 紫底黑字 \033[0m"  
# echo -e "\033[46;30m 天蓝底黑字 \033[0m"  
# echo -e "\033[47;34m 白底蓝字 \033[0m"  

# echo -e "\033[4;31m 下划线红字 \033[0m"  

echo -e "\033[32m----- make! -----\033[0m"    
make clean
make -j

# Remove module
echo "list all nvme drivers:"
sh_lsmod=`sudo lsmod |grep nvme`
echo -e "\033[32m$sh_lsmod\033[0m "

echo "list all nvme devices:"
sh_lsdev=`ls /dev/ |grep nvme`
echo -e "\033[32m$sh_lsdev\033[0m "

echo "sudo rmmod $rmmodule & nvme_core & $insmodule"

sudo rmmod $rmmodule 
sudo rmmod nvme_core
sudo rmmod $insmodule

#Insmod module
echo "sudo insmod $insmodule.ko || exit 1"
sudo insmod $insmodule.ko || exit 1
echo -e "successful load driver: \033[41;37m$insmodule.ko\033[0m ^_^"

make clean

##########################################################################
# 
#END:  Load script 
#
#########################################################################




