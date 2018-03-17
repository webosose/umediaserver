#!/bin/bash

if [[ -z "$1" ]] ; then
    echo "usage: ./test.sh <path_to_media_file>"
    exit -1
fi

export PYTHONPATH=/usr/local/webos/usr/lib/python2.7/site-packages

succeeded=0

for i in {1..1000} ; do
    python crash.py $1 2>&1 > /dev/null
    if [[ $? == 0 ]] ; then
	succeeded=$((succeeded+1))
	echo -n '.'
    else
	echo -n 'F'
    fi
done

echo "Succeded $succeeded out of 1000 tests."
