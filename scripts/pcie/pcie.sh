#!/bin/bash

TOP_DIR=`pwd`/../..

source $TOP_DIR/scripts/log.sh

# --------------------------------------------------------------------------- #
# Variable
# --------------------------------------------------------------------------- #
REG_VAL=0

# --------------------------------------------------------------------------- #
# Function
# --------------------------------------------------------------------------- #

# $1: BDF of PCIe device
# $2: offset
pcie_read_config_byte()
{
	local regval=`setpci -s $1 $2.b`
	REG_VAL=$((16#${regval}))
}

# $1: BDF of PCIe device
# $2: offset
pcie_read_config_word()
{
	local regval=`setpci -s $1 $2.w`
	REG_VAL=$((16#${regval}))
}

# $1: BDF of PCIe device
# $2: offset
pcie_read_config_dword()
{
	local regval=`setpci -s $1 $2.l`
	REG_VAL=$((16#${regval}))
}

# $1: BDF of PCIe device
# $2: offset
# $3: value
# $4: mask
pcie_write_config_byte()
{
	local val=`printf "%x" "$3"`
	local mask=`printf "%x" "$4"`
	setpci -s $1 $2.b=$val:$mask
}

# $1: BDF of PCIe device
# $2: offset
# $3: value
# $4: mask
pcie_write_config_word()
{
	local val=`printf "%x" "$3"`
	local mask=`printf "%x" "$4"`
	setpci -s $1 $2.w=$val:$mask
}

# $1: BDF of PCIe device
# $2: offset
# $3: value
# $4: mask
pcie_write_config_dword()
{
	local val=`printf "%x" "$3"`
	local mask=`printf "%x" "$4"`
	setpci -s $1 $2.l=$val:$mask
}



# $1: The BDF of Upstream Component like "00:01.2"
pcie_retrain_link()
{
	setpci -s $1 CAP_EXP+10.w=20:20
	sleep 0.01
}

# $1: Target BDF
# $2: Speed
pcie_set_target_link_speed()
{
	local speed=`printf "%x" "$2"`
	# Link Control2
	setpci -s $1 CAP_EXP+30.w=$speed:f
}

# $1: target
# $2: speed
# $3: width
# $?: return 0 if check success, otherwise greater than zero
pcie_check_cur_link_speed_width()
{
	local expect_speed=$2
	local expect_width=$3
	local regstr regval cur_speed cur_width

	# Link Status
	regstr=`setpci -s $1 CAP_EXP+12.w`
	if [[ "$?" -ne 0 ]]; then
		pr_err "ERR: read link status!"
		return 1
	fi
	regval=$((16#${regstr}))
	# bit[3:0] Current link speed
	cur_speed=$((regval & 0xf))
	# bit[9:4] Negotiated Link Width
	cur_width=$((regval / 16))
	cur_width=$((cur_width & 0x3f))

	if [ $expect_speed -ne 0 ] && [ $expect_speed -ne $cur_speed ]; then
		pr_err "ERR: Cur link Gen${cur_speed}x${cur_width}, Expect Gen${expect_speed}x${expect_width}"
		return 1
	fi

	if [ $expect_width -ne 0 ] && [ $expect_width -ne $cur_width ]; then
		pr_err "ERR: Cur link Gen${cur_speed}x${cur_width}, Expect Gen${expect_speed}x${expect_width}"
		return 1
	fi

	return 0
}

# $1: The BDF of Upstream Component
# $2: EP BDF
# $3: Speed
# $?: return 0 if switch success, otherwise greater than zero
pcie_switch_link_speed()
{
	pcie_set_target_link_speed $1 $3
	pcie_retrain_link $1
	pcie_check_cur_link_speed_width $2 $3 0
	if [[ "$?" -eq 0 ]]; then
		return 0
	fi

	return 1
}
