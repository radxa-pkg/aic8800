#!/bin/bash

case "$1" in
suspend | hibernate)
	/sbin/rfkill block bluetooth
	;;
resume | thaw)
	/sbin/rfkill unblock bluetooth
	;;
*) ;;
esac
