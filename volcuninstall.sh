#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: volcuninstall.sh /volclava_top"
    exit 1
fi

if [ ! -e $1 ]; then
    echo "$1 not exsit"
    exit 1
fi

if [ ! -d $1 ]; then
    echo "$1 is not directory"
    exit 1
fi

service volclava stop
chkconfig volclava off
chkconfig --del volclava

if rpm -qa | grep volclava-1.0* > /dev/null 2>&1; then
    rpm -e volclava-1.0*
fi

rm -f /etc/init.d/volclava
rm -f /etc/profile.d/volclava.*
rm -rf $1
echo "volclava has been uninstalled."
