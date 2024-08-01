
if [ -z $TOP_DIR ]; then
	TOP_DIR=`pwd`/..
fi

if [ -z $SHELL_TYPE ]; then
	# defualt cfg for "/bin/bash"
	ECHO_E=-e
else
	# cfg for "/bin/sh"
	ECHO_E=
fi

. $TOP_DIR/scripts/color.sh

# --------------------------------------------------------------------------- #
# Function
# --------------------------------------------------------------------------- #

pr_err()
{
	echo $ECHO_E "${LOG_B_RED}${FUNCNAME[1]}:$1${LOG_N}"
}

pr_warn()
{
	echo $ECHO_E "${LOG_B_YELLOW}${FUNCNAME[1]}:$1${LOG_N}"
}

pr_notice()
{
	echo $ECHO_E "${LOG_B_BLUE}$1${LOG_N}"
}

pr_info()
{
	echo $ECHO_E "${LOG_B_GREEN}$1${LOG_N}"
}

pr_debug()
{
	echo $ECHO_E "$1"
}

# obsolete
DEBUG()
{
	pr_debug $1
}

# obsolete
INFO()
{
	pr_info $1
}

# obsolete
NOTICE()
{
	pr_notice $1
}

# obsolete
WARN()
{
	pr_warn $1
}

# obsolete
ERR()
{
	pr_err $1
}

