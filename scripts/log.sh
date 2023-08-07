
LOG_N="\033[0m"
LOG_N_BLACK="\033[1;30m"
LOG_B_BLACK="\033[0;30m"
LOG_N_RED="\033[1;31m"
LOG_B_RED="\033[0;31m"
LOG_N_GREEN="\033[1;32m"
LOG_B_GREEN="\033[0;32m"
LOG_N_YELLOW="\033[1;33m"
LOG_B_YELLOW="\033[0;33m"
LOG_N_BLUE="\033[1;34m"
LOG_B_BLUE="\033[0;34m"
LOG_N_PURPLE="\033[1;35m"
LOG_B_PURPLE="\033[0;35m"
LOG_N_CYAN="\033[1;36m"
LOG_B_CYAN="\033[0;36m"
LOG_N_WHITE="\033[1;37m"

# --------------------------------------------------------------------------- #
# Function
# --------------------------------------------------------------------------- #

DEBUG()
{
	echo $ECHO_E "$1"
}

INFO()
{
	echo $ECHO_E "${LOG_N_GREEN}$1${LOG_N}"
}

NOTICE()
{
	echo $ECHO_E "${LOG_N_BLUE}$1${LOG_N}"
}

WARN()
{
	echo $ECHO_E "${LOG_N_YELLOW}$1${LOG_N}"
}

ERR()
{
	echo $ECHO_E "${LOG_N_RED}$1${LOG_N}"
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
