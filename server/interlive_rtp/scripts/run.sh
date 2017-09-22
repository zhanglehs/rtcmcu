#!/bin/sh

program=$1
configfile=$2

#ulimit -c unlimited
#ulimit -c 0
#$program -c $configfile

cd $(dirname $configfile)
#run_user=`head -1 scripts/username`
run_user=interlive
if [ "`whoami`" == "$run_user" ]; then
        echo "run by user: `whoami`"
	cd $(dirname $configfile);
        $program -c $configfile;
else    
        su $run_user -l -c "whoami"
        su $run_user -l -c "cd $(dirname $configfile)";
        su $run_user -l -c "$program -c $configfile";
fi  
