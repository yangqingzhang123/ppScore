#!/bin/bash

export LD_LIBRARY_PATH=./lib:./lib64:/usr/local/lib64:/usr/local/lib:/usr/local/mioji/lib:$LD_LIBRARY_PATH

CURR_PATH=`cd $(dirname $0);pwd;`
cd $CURR_PATH

kill -9 `netstat -nlap | grep 8899| grep LISTEN | awk '{print $7}' | cut -d / -f 1`
sleep 1
ulimit -c unlimited
nohup ./ppScore ../conf/server.cfg 2>&1 | nohup /usr/sbin/cronolog ~/logs/ppscore/%Y%m%d/%Y%m%d_%H.log &
sleep 10
