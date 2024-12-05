#!/bin/bash

# Directory containing the initial seed files
SEED_DIR="./images"  # Directory with initial seed files (e.g., PNG files)
EXECUTABLE="./fuzz/generic_test.fuzz"

# Directories to save mutated files and crash logs
RADAMSA_DIR="./output/radamsa"
MUTATED_DIR="$RADAMSA_DIR/mutated"
TMP_LOG_FILE="$RADAMSA_DIR/tmp_log.txt"
ERROR_LOG_DIR="$RADAMSA_DIR/error_logs"
SEGM_FAULT_LOG_DIR="$RADAMSA_DIR/seg_fault_logs"

mkdir -p $MUTATED_DIR
mkdir -p $ERROR_LOG_DIR
mkdir -p $SEGM_FAULT_LOG_DIR

rm -f $TMP_LOG_FILE
rm -f $MUTATED_DIR/*
rm -f $ERROR_LOG_DIR/*
rm -f $SEGM_FAULT_LOG_DIR/*

# Initialize mutation count
COUNTER=0
SEED_NUMBER=0
MUTATIONS_PER_SEED_FILE=10  # Number of mutations per seed file
COUNTER_ERRORS=0
COUNTER_SEG_FAULTS=0

# Infinite loop for continuous fuzzing
while true; do
    # Loop over each seed file in the seed directory
    for SEED_FILE in $SEED_DIR/*.png; do
        SEED_NAME=$(basename "$SEED_FILE")  # Get the filename without the path
        SEED_NAME="${SEED_NAME%.*}"  # Remove the file extension

        # Generate mutated versions from seed file
        SEED_NUMBER=$((SEED_NUMBER + 1))

        # Mutate the seed file using Radamsa
        MUTATED_FILES="$MUTATED_DIR/${SEED_NAME}_%n.png"

        radamsa -o $MUTATED_FILES -s $SEED_NUMBER -n $MUTATIONS_PER_SEED_FILE $SEED_FILE

        for MUTATED_FILE in $MUTATED_DIR/*; do
            # Run the mutated file through the program that uses libspng
            # Capture the exit status of the program
            $EXECUTABLE $MUTATED_FILE >> $TMP_LOG_FILE 2>&1
            EXIT_STATUS=$?

            # Print the current mutation count on the same line
            COUNTER=$((COUNTER + 1))
            
            if [ $EXIT_STATUS -eq 139 ]; then
                # Increment the segmentation fault count
                COUNTER_SEG_FAULTS=$((COUNTER_SEG_FAULTS + 1))

                # Log the segmentation fault and the mutated file
                echo "Segmentation fault detected on seed number $SEED_NUMBER for seed file: $SEED_NAME!" >> $SEGM_FAULT_LOG_DIR/crash_report.txt
                echo "Counter at: $COUNTER" >> $SEGM_FAULT_LOG_DIR/crash_report.txt
                echo "Mutated File: $MUTATED_FILE" >> $SEGM_FAULT_LOG_DIR/crash_report.txt
                echo "Exit Status: $EXIT_STATUS" >> $SEGM_FAULT_LOG_DIR/crash_report.txt
                echo "Segmentation Output:" >> $SEGM_FAULT_LOG_DIR/crash_report.txt
                cat $TMP_LOG_FILE >> $SEGM_FAULT_LOG_DIR/crash_report.txt
                echo "----------------------------------------------------" >> $SEGM_FAULT_LOG_DIR/crash_report.txt

                # Save the file with a unique name based on the mutation count
                cp $MUTATED_FILE $SEGM_FAULT_LOG_DIR/crash_$COUNTER.png

            elif [ $EXIT_STATUS -ne 0 ]; then
                # Increment the error count
                COUNTER_ERRORS=$((COUNTER_ERRORS + 1))

                # Log the crash or other errors and the mutated file
                echo "Error detected on seed number $SEED_NUMBER for seed file: $SEED_NAME!" >> $ERROR_LOG_DIR/crash_report.txt
                echo "Counter at: $COUNTER" >> $ERROR_LOG_DIR/crash_report.txt
                echo "Mutated File: $MUTATED_FILE" >> $ERROR_LOG_DIR/crash_report.txt
                echo "Exit Status: $EXIT_STATUS" >> $ERROR_LOG_DIR/crash_report.txt
                echo "Error Output:" >> $ERROR_LOG_DIR/crash_report.txt
                cat $TMP_LOG_FILE >> $ERROR_LOG_DIR/crash_report.txt
                echo "----------------------------------------------------" >> $ERROR_LOG_DIR/crash_report.txt

                # Save the file with a unique name based on the mutation count
                cp $MUTATED_FILE $ERROR_LOG_DIR/crash_$COUNTER.png
            fi

            # Print the current mutation count on the same line
            echo -ne "Counter $COUNTER - Mutating $SEED_NAME - Error count $COUNTER_ERRORS - Segmentation fault count $COUNTER_SEG_FAULTS\r"

            # Add a line to separate the output for each log
            echo "----------------------------------------------------\n" >> $TMP_LOG_FILE
        done  # Inner for loop for all mutated files
        
        # Delete the mutated files to save space
        rm -f $MUTATED_DIR/*

    done  # Outer for loop for all seed files
done  # Outer while loop for continuous fuzzing across all seed files
