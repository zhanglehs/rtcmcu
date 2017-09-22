#!/bin/sh

default_user=interlive
user=$default_user
password=$user

#add an user for run interlive service
#if id -u $user >/dev/null 2>&1; then
#	echo "user $user exists"
#else
#	echo "user $user does not exist, add user"
#	/usr/sbin/useradd $user 
#	echo $user:$password | /usr/sbin/chpasswd
#fi

# normal user can use tcpdump (only works on youku's server)
if [ -f "/usr/sbin/tcpdump" ]; then
	if getent group tcpdump >/dev/null 2>&1; then
		usermod -a -G tcpdump $user
	fi
fi

