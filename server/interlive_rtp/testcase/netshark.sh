#!/bin/sh

function shark()
{
    for((i=1;i<$1;i++))
    do
        /sbin/iptables -A OUTPUT -d 10.10.161.168 -m limit --limit 5/s -j ACCEPT
        /sbin/iptables -A OUTPUT -d 10.10.161.168 -j DROP

        sleep $2

        /sbin/iptables -F OUTPUT

        sleep $2
    done
}

shark 100 5
