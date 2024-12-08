#!/bin/bash

# Copyright (C) 2021-2024 Bytedance Ltd. and/or its affiliates

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

daemon_pids=$(ps -ef | grep "$1/sbin" | grep -v grep | awk '{print $2}')
if [ ! -z $daemon_pids ]; then
    ps -ef | grep "$1/sbin" | grep -v grep | awk '{print $2}' | xargs kill -9
fi

chkconfig volclava off
chkconfig --del volclava

if rpm -qa | grep volclava-1.0* > /dev/null 2>&1; then
    rpm -e volclava-1.0*
fi

rm -f /etc/init.d/volclava*
rm -f /etc/profile.d/volclava.*
rm -rf $1

echo "volclava has been uninstalled."
