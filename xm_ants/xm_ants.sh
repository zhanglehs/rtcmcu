#!/bin/sh

PWD=`pwd`
$PWD/xm_ants --config-file $PWD/xm_ants.conf -d
echo $?
