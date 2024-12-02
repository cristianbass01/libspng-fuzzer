#!/bin/bash

# Directory containing the initial seed files
SEED_DIR="./images"  # Directory with initial seed files (e.g., PNG files)
EXECUTABLE="/path/to/your/test_program_using_libspng"

# Directories to save mutated files and crash logs
MUTATED_DIR="./tmp_mutated_radamsa"
TMP_LOG_FILE="./test_radamsa.log"
CRASH_LOG_DIR="./crash_logs_radamsa"
mkdir -p $MUTATED_DIR
mkdir -p $CRASH_LOG_DIR

# Initialize mutation count
COUNTER=0
SEED_NUMBER=0
MUTATIONS_PER_SEED_FILE=100  # Number of mutations per seed file
COUNTER_ERRORS=0

# Infinite loop for continuous fuzzing
while true; do
    # Loop over each seed file in the seed directory
    for SEED_FILE in $SEED_DIR/*.png; do
        SEED_NAME=$(basename "$SEED_FILE")  # Get the filename without the path

        # Generate mutated versions from seed file
        SEED_NUMBER=$((SEED_NUMBER + 1))

        # Mutate the seed file using Radamsa
        MUTATED_FILES="$MUTATED_DIR/mutated_%n.png"

        radamsa -o $MUTATED_FILES -s $SEED_NUMBER -n $MUTATIONS_PER_SEED_FILE $SEED_FILE

        for MUTATED_FILE in $MUTATED_DIR/*; do
            # Run the mutated file through the program that uses libspng
            # Capture the exit status of the program
            $EXECUTABLE $MUTATED_FILE &> $TMP_LOG_FILE
            EXIT_STATUS=$?

            # Print the current mutation count on the same line
            COUNTER=$((COUNTER + 1))
            echo -ne "Counter $COUNTER - Mutating $SEED_NAME - Error count $COUNTER_ERRORS\r"

            # Check if the program crashed (non-zero exit code)
            if [ $EXIT_STATUS -ne 0 ]; then
                # Increment the error count
                COUNTER_ERRORS=$((COUNTER_ERRORS + 1))

                # If the program crashed, log the crash and the mutated file
                echo "Crash detected on seed number $SEED_NUMBER for seed file: $SEED_NAME!" >> $CRASH_LOG_DIR/crash_report.txt
                echo "Counter at: $COUNTER" >> $CRASH_LOG_DIR/crash_report.txt
                echo "Mutated File: $MUTATED_FILE" >> $CRASH_LOG_DIR/crash_report.txt
                echo "Exit Status: $EXIT_STATUS" >> $CRASH_LOG_DIR/crash_report.txt
                echo "Crash Output:" >> $CRASH_LOG_DIR/crash_report.txt
                cat $TMP_LOG_FILE >> $CRASH_LOG_DIR/crash_report.txt
                echo "----------------------------------------------------" >> $CRASH_LOG_DIR/crash_report.txt

                # Save the file with a unique name based on the mutation count
                cp $MUTATED_FILE $CRASH_LOG_DIR/crash_$COUNTER.png
            fi
        done  # Inner for loop for all mutated files
    done  # Outer for loop for all seed files
done  # Outer while loop for continuous fuzzing across all seed files
