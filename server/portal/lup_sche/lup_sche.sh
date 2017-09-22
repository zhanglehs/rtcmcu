#!/bin/sh
#
# lup_sche.sh - this script starts and stops the lup_sche daemon
#
# Source function library.
. "/etc/rc.d/init.d/functions"

# Source networking configuration.
. "/etc/sysconfig/network"

# Check that networking is up.
[ "$NETWORKING" = "no" ] && exit 0

python_bin="/usr/bin/python"
prog_path=`pwd`
prog="$prog_path/lup_sche.py"
pidfile="$prog_path/lup_sche.pid"
lockfile="$prog_path/lup_sche.lock"

rh_status() {
    status -p $pidfile $prog
}

rh_status_q() {
    rh_status >/dev/null 2>&1
}

start() {
    echo -n $"Starting $prog: "
    $python_bin $prog start

	sleep 5

	rh_status
	retval=$?

    echo
    [ $retval -eq 0 ] && touch $lockfile

    return $retval
}

stop() {
    echo -n $"Stopping $prog... "
	$python_bin $prog stop
    retval=$?
    echo

	while : ; do
		sleep 1
		if [ ! -f $pidfile ]; then
			break
		fi
	done

    [ $retval -eq 0 ] && rm -f $lockfile

	rh_status
    return $retval
}

force_stop() {
    echo -n $"Stopping $prog: "
	$python_bin $prog force_stop
    retval=$?
    echo
    [ $retval -eq 0 ] && rm -f $lockfile

	rh_status
    return $retval
}

restart() {
    configtest || return $?
    stop
    start
}

reload_conf() {
    configtest || return $?
    echo -n $"Reloading $prog: "
	$python_bin $prog reload_conf
    RETVAL=$?
    echo
}

configtest() {
	# do nothing
	:
}

case "$1" in
    start)
        rh_status_q && exit 0
        $1
        ;;
    stop)
        rh_status_q || exit 0
        $1
        ;;
    status)
        rh_status
        ;;
    reload_conf)
        rh_status_q || exit 7
        $1
        ;;
    force_stop)
		$1
        ;;
    *)
        echo $"Usage: $0 {start|stop|status|force_stop}"
        exit 2
esac
