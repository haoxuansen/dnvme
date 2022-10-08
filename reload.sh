#!/bin/bash

# UNH-IOL NVMe Consortium
# Mike Bogochow
# reload.sh
#
# Reloads dnvme with the newly compiled driver file (dnvme.ko).

if [ -z "$(lsmod | grep '^dnvme')" ]; then
  sudo insmod dnvme.ko
else
  sudo rmmod dnvme
  sudo insmod dnvme.ko
fi
