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

# Sanitizer
if [ -z ${SANITIZER+x} ]; then
        SANITIZER=""
fi

# Build test
if [ -z ${FUZZ_READ+x} ]; then
        FUZZ_READ=1
fi

if [ -z ${TEST_TYPE+x} ]; then
        TEST_TYPE=2
fi

case "$SANITIZER" in
asan)
    test_file="fuzz/${2}_asan.fuzz"
    ;;
msan)
    test_file="fuzz/${2}_msan.fuzz"
    ;;
*)
    test_file="fuzz/${2}.fuzz"
    ;;
esac

# rm $test_file > /dev/null 2>&1 || true
make fuzz/in_out.fuzz
touch "fuzz/${2}.c"
make CPPFLAGS="-DFUZZ_READ=$FUZZ_READ -DTEST_TYPE=$TEST_TYPE" $test_file
if [ $? -ne 0 ]; then
    echo "Failed to build test"
    exit 1
fi

# Run fuzzer

if [ -z ${SINGLE_IMAGE+x} ]; then
        SINGLE_IMAGE=0
fi


mkdir output > /dev/null 2>&1 || true
# !!! Add or change fuzzer cases here
case "$1" in
zzuf)
    echo "Fuzzing with zzuf..."
    output_dir="output/zzuf"
    mkdir $output_dir > /dev/null 2>&1 || true

    # Set by prepending NUM_RUNS to the script
    if [ -z ${NUM_RUNS+x} ]; then
        NUM_RUNS=1
    fi

    num_files=$(ls -la $output_dir | grep -E "$2.*.out" | wc -l)
    output_file="$output_dir/$2$((num_files+1)).out"
    echo "" > $output_file

    START=$(cat /dev/random | head -c 4 | xxd -p)
    START=$((16#$START))

    if [ $SINGLE_IMAGE = 1 ]; then
        for i in $(seq $START $((START + NUM_RUNS - 1))); do
            command="zzuf ${opts[@]} -s $i $test_file"
            echo $command >> $output_file
            LD_LIBRARY_PATH=libspng/build $command >> $output_file 2>&1
            echo "" >> $output_file
        done
    else
        IMAGES_DIR="images/"
        total_images=$(ls -la $IMAGES_DIR | grep -E ".*.png" | wc -l)

        ind=1
        for img_path in $IMAGES_DIR*; do
            ind=$((ind+1))

            img_name=$(basename $img_path)
            for i in $(seq $START $((START + NUM_RUNS - 1))); do
                echo -ne "                                                                       \r"
                echo -ne "Image $ind/$total_images, Run $((i - START))/$NUM_RUNS\r"
                command="zzuf ${opts[@]} -s $i $test_file $img_path"
                if [ "$SANITIZER" = "valgrind" ]; then
                    OUTPUT=$(LD_LIBRARY_PATH=libspng/build valgrind --leak-check=full --error-exitcode=1 --trace-children=yes --show-leak-kinds=all $command 2>&1)
                elif [ "$SANITIZER" = "asan" ]; then
                    zzuf ${opts[@]} -s $i fuzz/in_out.fuzz $img_path "${img_path}_TMP" > /dev/null 2>&1
                    OUTPUT=$(LD_LIBRARY_PATH=libspng/build $test_file "${img_path}_TMP" 2>&1)
                    rm "${img_path}_TMP"
                else
                    OUTPUT=$(LD_LIBRARY_PATH=libspng/build $command 2>&1)
                fi
                # if [ $? -ne 0 ]; then
                echo $command >> $output_file
                echo $OUTPUT >> $output_file
                # fi
            done    
        done
    fi
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
