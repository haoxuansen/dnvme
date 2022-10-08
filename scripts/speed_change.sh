#!/bin/bash

#targate_link_speed bit[3:0]
setpci -s 2:0.0 a0.b=02
#direct_speed_change bit:[2]
setpci -s 2:0.0 80e.b=3

#target_link_width bit[5:0]
#direct_link_width_change bit:[6]
setpci -s 2:0.0 8c0.b=c4


