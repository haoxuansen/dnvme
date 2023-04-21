# !/bin/bash
TOP_DIR=`pwd`/..

EVENTS="dnvme_proc_write \
	dnvme_sgl_set_seg \
	dnvme_sgl_set_data \
	dnvme_setup_prps \
	dnvme_submit_64b_cmd \
	handle_cmd_completion"

source $TOP_DIR/scripts/log.sh

# --------------------------------------------------------------------------- #
# Function
# --------------------------------------------------------------------------- #

# $@: event list
trace_enable_events()
{
	NOTICE "Enable events...$"	
	for evt in $@
	do
		echo "$evt" >> /sys/kernel/debug/tracing/set_event
		INFO "enable $evt ok"
	done
}

# --------------------------------------------------------------------------- #
# Execute
# --------------------------------------------------------------------------- #

trace_enable_events $EVENTS

