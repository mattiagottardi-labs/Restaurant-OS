#!/bin/bash

file=/tmp/program.pid

# check if file exists and is redable
if [[ -e $file && -r $file ]]; then
    echo "File: $file exist and is readable"
else
    echo "File: $file is not existing/readable"
    exit 1
fi

# read the pid from the file
pid=$(< $file)
echo "PID: $pid"

# send signal to process with PID = $pid
kill -SIGUSR1 $pid