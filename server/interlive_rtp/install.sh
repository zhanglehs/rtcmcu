#!/bin/sh

basepath=/opt/interlive
role=$1

mkdir -p $basepath/$role/logs
cp -f $role $role.xml $basepath/$role
cp -f scripts/$role /etc/init.d
chown interlive $basepath -R
chgrp interlive $basepath -R
chmod +x $basepath/$role/$role
chmod +x /etc/init.d/$role
