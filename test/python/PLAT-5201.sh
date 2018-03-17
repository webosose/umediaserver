#!/bin/sh

PID=""
cleanup () {
    kill $PID
}

CONCURRENCY=${1:-1}

trap cleanup INT

cd "$(dirname $(readlink -f $0))"

for i in $(seq 1 $CONCURRENCY); do
    PYTHONPATH="/usr/local/webos/usr/lib/python2.7/site-packages" python2 load.py -c 10 -d 5000 dummy sim &
    PID="$PID $!"
done

wait
