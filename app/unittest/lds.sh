#!/bin/bash
CUR_DIR=`pwd`
if [[ "$TOP_DIR" = "" ]]; then
	TOP_DIR=$CUR_DIR/../..
fi

source $TOP_DIR/scripts/log.sh

LDS_FILE=test.lds
TMP_FILE=tmp.txt

# --------------------------------------------------------------------------- #
# Function
# --------------------------------------------------------------------------- #

# lds_delete_head - Deletes the contents of the file before this line,
#	include this line number.
#
# $1: Linker script file
# $2: Line number
lds_delete_head()
{
	sed -i "1,${2}d" $1
}

# lds_delete_tail - Deletes the contents of the file after this line,
#	include this line number.
#
# $1: Linker script file
# $2: Line number
lds_delete_tail()
{
	sed -i "${2},\$d" $1
}

# $1: Linker script file
lds_trim_context()
{
	local lines=`grep -rn "===" $1 | awk -F: '{print $1}'`
	local num=`echo $lines | awk '{print NF}'`
	local head=`echo $lines | awk '{print $1}'`
	local tail=`echo $lines | awk '{print $2}'`
	
	if [[ $num = 2 ]]; then
		lds_delete_tail $1 $tail
		lds_delete_head $1 $head
	fi
}

# $1: Linker script file
lds_add_section()
{
	local line

	line=`grep -rn "(\.rela\.data" $1 | awk -F: '{print $1}'`
	echo "      *(.rela.nvme .rela.nvme.*)" > $TMP_FILE
	sed -i "${line}r ${TMP_FILE}" $1

	line=`grep -rn "\.data1" $1 | awk -F: '{print $1}'`
	echo "  .nvme           :" > $TMP_FILE
	echo "  {" >> $TMP_FILE
	echo "    __start_nvme_case = .;" >> $TMP_FILE
	echo "    KEEP (*(.nvme.case.0))" >> $TMP_FILE
	echo "    KEEP (*(.nvme.case.1))" >> $TMP_FILE
	echo "    KEEP (*(.nvme.case.2))" >> $TMP_FILE
	echo "    KEEP (*(.nvme.case.3))" >> $TMP_FILE
	echo "    KEEP (*(.nvme.case.4))" >> $TMP_FILE
	echo "    KEEP (*(.nvme.case.5))" >> $TMP_FILE
	echo "    KEEP (*(.nvme.case.6))" >> $TMP_FILE
	echo "    KEEP (*(.nvme.case.7))" >> $TMP_FILE
	echo "    KEEP (*(.nvme.case.8))" >> $TMP_FILE
	echo "    KEEP (*(.nvme.case.9))" >> $TMP_FILE
	echo "    __stop_nvme_case = .;" >> $TMP_FILE
	echo "    __start_nvme_autocase = .;" >> $TMP_FILE
	echo "    KEEP (*(.nvme.autocase.0))" >> $TMP_FILE
	echo "    KEEP (*(.nvme.autocase.1))" >> $TMP_FILE
	echo "    __stop_nvme_autocase = .;" >> $TMP_FILE
	echo "  }" >> $TMP_FILE

	sed -i "${line}r ${TMP_FILE}" $1
	rm $TMP_FILE
}

# --------------------------------------------------------------------------- #
# Execute
# --------------------------------------------------------------------------- #
CMD=$1

if [[ "$CMD" = "build" ]] || [[ "$CMD" = "" ]]; then
	NOTICE "Auto generate link script file...$"
	ld --verbose > $LDS_FILE

	lds_trim_context $LDS_FILE
	lds_add_section $LDS_FILE
elif [[ "$CMD" = "clean" ]] || [[ "$CMD" = "distclean" ]]; then
	NOTICE "Delete link script file...$"
	rm -f $LDS_FILE
fi

