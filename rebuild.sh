#!/bin/bash

# UNH-IOL NVMe Consortium
# Mike Bogochow
# rebuild.sh
#
# Rebuilds dnvme using make and then reloads dnvme with the newly compiled 
# driver file (dnvme.ko).

make clean && make

. reload.sh
