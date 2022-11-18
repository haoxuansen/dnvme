#!/bin/bash
#########################################################################
#
# Name:     Load
# Purpose:  Load dnvme Driver 
##########################################################################

rmmodule=u-dma-buf
insmodule=u-dma-buf

echo -e "\033[32m----- clean! -> make! -----\033[0m"    
make -f Makefile clean
make -f Makefile all

echo "list all udmabuf devices:"
sh_lsdev=`ls /dev/ |grep udmabuf`
echo -e "\033[32m$sh_lsdev\033[0m "

echo "sudo rmmod $rmmodule"

sudo rmmod $rmmodule 

#Insmod module
echo "sudo insmod $insmodule.ko || exit 1"
sudo insmod $insmodule.ko udmabuf0=1048576 || exit 1
echo -e "successful load driver: \033[41;37m$insmodule.ko\033[0m ^_^"

make clean

##########################################################################
# 
#END:  Load script 
#
#########################################################################




