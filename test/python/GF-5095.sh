#!/bin/bash

export PYTHONPATH=/usr/local/webos/usr/lib/python2.7/site-packages

succeeded=0

for i in {1..1000} ; do
    python hang.py 2>&1 > /dev/null
    if [[ $? == 0 ]] ; then
	succeeded=$((succeeded+1))
	echo -n '.'
    else
	echo -n 'F'
    fi
done

echo "Succeded $succeeded out of 1000 tests."
