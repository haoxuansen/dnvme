
LOG_COLOR_NONE="\033[0m"
LOG_COLOR_BLACK="\033[1;30m"
LOG_COLOR_DBLACK="\033[0;30m"
LOG_COLOR_RED="\033[1;31m"
LOG_COLOR_DRED="\033[0;31m"
LOG_COLOR_GREEN="\033[1;32m"
LOG_COLOR_DGREEN="\033[0;32m"
LOG_COLOR_YELLOW="\033[1;33m"
LOG_COLOR_DYELLOW="\033[0;33m"
LOG_COLOR_BLUE="\033[1;34m"
LOG_COLOR_DBLUE="\033[0;34m"
LOG_COLOR_PURPLE="\033[1;35m"
LOG_COLOR_DPURPLE="\033[0;35m"
LOG_COLOR_CYAN="\033[1;36m"
LOG_COLOR_DCYAN="\033[0;36m"
LOG_COLOR_WHITE="\033[1;37m"

# --------------------------------------------------------------------------- #
# Function
# --------------------------------------------------------------------------- #

DEBUG()
{
	echo $ECHO_E "$1"
}

INFO()
{
	echo $ECHO_E "${LOG_COLOR_GREEN}$1${LOG_COLOR_NONE}"
}

NOTICE()
{
	echo $ECHO_E "${LOG_COLOR_BLUE}$1${LOG_COLOR_NONE}"
}

WARN()
{
	echo $ECHO_E "${LOG_COLOR_YELLOW}$1${LOG_COLOR_NONE}"
}

ERR()
{
	echo $ECHO_E "${LOG_COLOR_RED}$1${LOG_COLOR_NONE}"
}

# --------------------------------------------------------------------------- #
# Configuration
# --------------------------------------------------------------------------- #

if [[ $CONFIG_SHELL_TYPE = "sh" ]]; then
	# /bin/sh
	ECHO_E=
else
	# /bin/bash
	ECHO_E=-e
fi
