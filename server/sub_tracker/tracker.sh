#!/bin/sh
#
# tracker.sh Startup script for the tracker server of Youku Tudou Interlive System
#
# processname: tracker 
# config: /opt/interlive/tracker/tracker.xml
# pidfile: /opt/interlive/tracker/tracker.pid
#

# Source function library
. /etc/rc.d/init.d/functions

#TRACKER_DIR="/opt/interlive/tracker/"
TRACKER_DIR="/home/interlive/interlive/trunk/server/tracker/"
TTRACKER_VER="2.0.130807.2"
if [ -z "$TRACKER_CONF_PATH" ]; then
	TRACKER_CONF_PATH=${TRACKER_DIR}tracker.xml
fi

prog="tracker"
tracker=${TRACKER_DIR}${prog}
RETVAL=0

start() {
	echo -n $"Starting $prog: "
	daemon $tracker -c $TRACKER_CONF_PATH
	RETVAL=$?
	echo
	[ $RETVAL -eq 0 ] && touch /var/lock/subsys/$prog
	return $RETVAL
}

stop() {
	echo -n $"Stopping $prog: "
	killproc $tracker
	RETVAL=$?
	echo
	[ $RETVAL -eq 0 ] && rm -f /var/lock/subsys/$prog
	return $RETVAL
}

reload() {
	echo -n $"Reloading $prog: "
	killproc $tracker -HUP
	RETVAL=$?
	echo
	return $RETVAL
}

case "$1" in
	start)
		start
		;;
	stop)
		stop
		;;
	restart)
		stop
		start
		;;
	reload)
		reload
		;;
	status)
		status $tracker
		RETVAL=$?
		;;
	*)
		echo $"Usage: $0 {start|stop|restart|reload|status}"
		RETVAL=1
esac

exit $RETVAL
