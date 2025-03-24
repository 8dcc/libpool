#!/bin/bash
#
# Copyright 2025 8dcc. All Rights Reserved.
#
# This file is part of libpool.
#
# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program.  If not, see <https://www.gnu.org/licenses/>.
set -e

IGNORE=100000   # Number of initial allocations that should be ignored.
NMEMB=1000000   # Number of total allocations.
ELEM_SIZE=10000 # Size of each element.
STEP=10000      # Number of allocations to be increased each iteration.
OUTPUT_FILE="benchmark.svg"

# float subtract_flt(float a, float b);
subtract_flt() {
    awk "BEGIN {printf \"%.2f\", ${1}-${2}; exit(0)}"
}

# int add_int(int a, int b);
add_int() {
    echo "$(( "$1" + "$2" ))"
}

time_of() {
    env time -f "%e" "${@}"
}

# const char* benchmark(int nmemb);
benchmark() {
    local nmemb="$1"
    local malloc_time
    local libpool_time

    # If we were going to benchmark 100 allocations, but we want to ignore 5,
    # start by benchmarking 105, then benchmark 5, and then subtract the
    # results.
    if [ -n "$IGNORE" ] && [ "$IGNORE" -gt 0 ]; then
        nmemb="$(add_int "$nmemb" "$IGNORE")"
    fi

    libpool_time=$(time_of ./benchmark.out "libpool" "$nmemb" "$ELEM_SIZE" 2>&1)
    malloc_time=$(time_of ./benchmark.out "malloc" "$nmemb" "$ELEM_SIZE" 2>&1)

    if [ -n "$IGNORE" ] && [ "$IGNORE" -gt 0 ]; then
        libpool_time_ignored=$(time_of ./benchmark.out "libpool" "$IGNORE" "$ELEM_SIZE" 2>&1)
        libpool_time=$(subtract_flt "$libpool_time" "$libpool_time_ignored")
        malloc_time_ignored=$(time_of ./benchmark.out "malloc" "$IGNORE" "$ELEM_SIZE" 2>&1)
        malloc_time=$(subtract_flt "$malloc_time" "$malloc_time_ignored")
    fi

    echo "${nmemb} ${libpool_time} ${malloc_time}"
}

# ------------------------------------------------------------------------------

printf 'Benchmarking [%d..%d] allocations of %d bytes.' "$STEP" "$NMEMB" "$ELEM_SIZE"
if [ -n "$IGNORE" ]; then
    printf ' Ignoring first %d calls.' "$IGNORE"
fi
printf '\n'

tmp_file="$(mktemp --tmpdir "libpool-benchmark.XXX.log")"
for (( i="$STEP"; i<="$NMEMB"; i+="$STEP" )); do
    printf '\rCurrently benchmarking %d allocations...' "$i"
    benchmark "$i" >> "$tmp_file"
done
printf '\nDone.\n'


graph_title='Performance of libpool vs. malloc'
if [ -n "$IGNORE" ]; then
    graph_title="${graph_title} after ${IGNORE} allocations"
fi

gnuplot -e "
set terminal svg;
set output '${OUTPUT_FILE}';
set title '${graph_title}';
set xlabel 'N. of allocations';
set ylabel 'Time (seconds)';
plot '${tmp_file}' using 1:2 smooth bezier with lines title 'libpool',
     '${tmp_file}' using 1:3 smooth bezier with lines title 'malloc';
"

if [ -f "$tmp_file" ]; then
   rm "$tmp_file"
fi
