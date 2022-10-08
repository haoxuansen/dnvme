#!/bin/bash
#########################################################################
#
# Name:     Load
# Purpose:  Load clean, make, run  
##########################################################################


cd ${PWD}/dnvme
sudo ./load_driver.sh
cd udmabuf/
sudo ./load_udmabuf.sh
sudo dmesg |grep 'phys address'
cd ../unittest/
./load_make.sh

##########################################################################
# 
#END:  Load script 
#
#########################################################################




