#!/bin/sh
# run_all.sh - Launch all ATC_System components in the correct order

rm -f /dev/shm/atc_*

# Launch each component in the background
./launcher &
sleep 1
./aircraft &
./radar &
./computer &
./operator_display &
./communication &

# Wait for all background processes to finish
wait