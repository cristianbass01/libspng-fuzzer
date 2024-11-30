#!/bin/bash

if [ "$#" -le 1 ]; then
  echo "Usage: $0 <fuzzer> <test> [options]"
  echo "Example: $0 zzuf decode_dev_zero"
#   echo " - Supported fuzzers: radamsa, zzuf, afl++, certfuzz, honggfuzz"
  echo " - Supported fuzzers: zzuf, afl"
#   echo " - Supported sanitizers: clang, valgrind"
  exit 1
fi

# Parse options
opt_num=0
declare -a opts
for i in $(seq 1 $#); do
    param=${!i}
    next_param=""
    if [ "$i" -ne $# ]; then
        next_i=$((i+1))
        next_param=${!next_i}
    fi
    if [[ $param == -* ]]; then
        opts[$opt_num]=$param
        if [[ $next_param != -* ]]; then
            opts[$((opt_num+1))]=$next_param
        fi
        opt_num=$((opt_num+2))
    fi
done

# Check if test exists
test_source="fuzz/$2.c"
if [ ! -f $test_source ]; then
    echo "Test \"$2\" not found"
    exit 1
fi

# Build test
test_file="fuzz/$2.fuzz"
make $test_file
if [ $? -ne 0 ]; then
    echo "Failed to build test"
    exit 1
fi

# Run fuzzer
mkdir output > /dev/null 2>&1 || true
# !!! Add or change fuzzer cases here
case "$1" in
zzuf)
    # Set by prepending NUM_RUNS to the script
    if [ -z ${NUM_RUNS+x} ]; then
        NUM_RUNS=1
    fi

    output_dir="output/zzuf"
    mkdir $output_dir > /dev/null 2>&1 || true
    echo "Fuzzing with zzuf..."

    num_files=$(ls -la $output_dir | grep -E "$2.*.out" | wc -l)
    output_file="$output_dir/$2$((num_files+1)).out"
    
    echo "" > $output_file
    for i in $(seq 1 $NUM_RUNS); do
        command="zzuf ${opts[@]} -s $RANDOM $test_file"
        echo $command >> $output_file
        LD_LIBRARY_PATH=libspng/build $command >> $output_file
        echo "" >> $output_file
    done
    ;;
afl)
    echo "Fuzzing with AFL..."
    # AFL_SKIP_CPUFREQ=1 AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 AFL_SKIP_BIN_CHECK=1 AFL_NO_AFFINITY=1 AFL_EXIT_WHEN_DONE
    # Probably need to do smth witn -in and -out idk how afl works
    LD_LIBRARY_PATH=libspng/build afl-fuzz ${opts[@]} $test_file
    ;;
radamsa)
    echo "Fuzzing with Radamsa..."
    LD_LIBRARY_PATH=libspng/build radamsa ${opts[@]} $test_file
    ;;
honggfuzz)
    echo "Fuzzing with Honggfuzz..."
    LD_LIBRARY_PATH=libspng/build honggfuzz ${opts[@]} -f $test_file
    ;;
*)  
    echo "Unknown fuzzer \"$1\""
    ;;
esac
