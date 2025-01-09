#!/bin/sh
set -e

IGNORE=100000
NMEMB=1100000
SIZE=10000

subtract_flt() {
    echo $(awk "BEGIN {printf \"%.2f\", ${1}-${2}; exit(0)}")
}

echo "Benchmarking ${NMEMB} allocations of ${SIZE} bytes. Ignoring first ${IGNORE} calls."

malloc_time1=$(env time -f "%e" ./benchmark.out "malloc" $NMEMB $SIZE 2>&1)
malloc_time2=$(env time -f "%e" ./benchmark.out "malloc" $IGNORE $SIZE 2>&1)
malloc_time=$(subtract_flt "$malloc_time1" "$malloc_time2")
libpool_time1=$(env time -f "%e" ./benchmark.out "libpool" $NMEMB $SIZE 2>&1)
libpool_time2=$(env time -f "%e" ./benchmark.out "libpool" $IGNORE $SIZE 2>&1)
libpool_time=$(subtract_flt "$libpool_time1" "$libpool_time2")

echo "Time when using 'malloc'....: ${malloc_time1} - ${malloc_time2} = ${malloc_time} seconds"
echo "Time when using 'libpool'...: ${libpool_time1} - ${libpool_time2} = ${libpool_time} seconds"
