#!/bin/bash

INPUT_DIR="unique_images/"

# This script is used to run the fuzzing process with AFL.

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <run_number>"
    exit 1
fi

RUN_NUMBER=$1

mkdir -p afl_output_dir$RUN_NUMBER

afl-fuzz -M main-$HOSTNAME  -i $INPUT_DIR/ -o afl_output_dir$RUN_NUMBER/ ./fuzz/afl_generic_test_nosan.fuzz @@ > afl_output_dir$RUN_NUMBER/main-$HOSTNAME.log 2>&1 &
# Run the other afl-fuzz instances in the background
afl-fuzz -S slave-1 -i $INPUT_DIR/ -o afl_output_dir$RUN_NUMBER/ ./fuzz/afl_generic_test_asan.fuzz @@ > afl_output_dir$RUN_NUMBER/slave-1.log 2>&1 &
afl-fuzz -S slave-2 -i $INPUT_DIR/ -o afl_output_dir$RUN_NUMBER/ ./fuzz/afl_generic_test_msan.fuzz @@ > afl_output_dir$RUN_NUMBER/slave-2.log 2>&1 &
afl-fuzz -S slave-3 -i $INPUT_DIR/ -o afl_output_dir$RUN_NUMBER/ ./fuzz/afl_generic_test_nosan.fuzz @@ > afl_output_dir$RUN_NUMBER/slave-3.log 2>&1 &
afl-fuzz -S slave-4 -i $INPUT_DIR/ -o afl_output_dir$RUN_NUMBER/ ./fuzz/afl_generic_test_nosan.fuzz @@ > afl_output_dir$RUN_NUMBER/slave-4.log 2>&1 &