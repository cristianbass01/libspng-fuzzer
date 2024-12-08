#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <executable>"
    exit 1
fi

EXECUTABLE="$1"

if [[ "$EXECUTABLE" == *asan* ]]; then
    SANITIZER="asan"
elif [[ "$EXECUTABLE" == *msan* ]]; then
    SANITIZER="msan"
else
    SANITIZER=""
fi


# Directory containing the initial seed files
SEED_DIR="./images"  # Directory with initial seed files (e.g., PNG files)
echo "Using executable: $EXECUTABLE"

SAVE_ALL_LOGS=0  # Set to 1 to save all logs, including successful runs
SAVE_IMAGES=0  # Set to 1 to save all mutated images

# Directories to save mutated files and crash logs
RADAMSA_DIR="./output/radamsa_$SANITIZER" 
MUTATED_DIR="$RADAMSA_DIR/mutated"
TMP_LOG_FILE="$RADAMSA_DIR/tmp_log.txt"
LOG_FILE="$RADAMSA_DIR/logs.txt"
ERROR_LOG_DIR="$RADAMSA_DIR/error_logs"
SEGM_FAULT_LOG_DIR="$RADAMSA_DIR/seg_fault_logs"

ERROR_CRASH_REPORT="$ERROR_LOG_DIR/_crash_report.txt"
SEGM_FAULT_CRASH_REPORT="$SEGM_FAULT_LOG_DIR/_crash_report.txt"

rm -rf $RADAMSA_DIR

mkdir -p $MUTATED_DIR
mkdir -p $ERROR_LOG_DIR
mkdir -p $SEGM_FAULT_LOG_DIR

# Initialize mutation count
COUNTER=0
SEED_NUMBER=0
MUTATIONS_PER_SEED_FILE=1000  # Number of mutations per seed file
COUNTER_ERRORS=0
COUNTER_SEG_FAULTS=0

START_TIME=$(date +%s)

# Infinite loop for continuous fuzzing
while true; do
    # Loop over each seed file in the seed directory
    for SEED_FILE in $SEED_DIR/*.png; do
        SEED_NAME=$(basename "$SEED_FILE")  # Get the filename without the path
        SEED_NAME="${SEED_NAME%.*}"  # Remove the file extension

        # Msan crashes on some seed files
        # if the seed name starts with ct and ends with n0g04, skip it (but not if the char in the middle is 0)
        #if [[ "$SEED_NAME" =~ ^ct.*n0g04$ && ! "$SEED_NAME" =~ ^ct0.*n0g04$ ]]; then
            #echo "Skipping seed file: $SEED_NAME"
        #    continue
        #fi

        # Generate mutated versions from seed file
        SEED_NUMBER=$((SEED_NUMBER + 1))

        # Mutate the seed file using Radamsa
        MUTATED_FILES="$MUTATED_DIR/${SEED_NAME}_%n.png"

        radamsa -o $MUTATED_FILES -s $SEED_NUMBER -n $MUTATIONS_PER_SEED_FILE $SEED_FILE
        EXIT_STATUS=$?

        if [ $EXIT_STATUS -ne 0 ]; then
            echo "Radamsa failed with exit status $EXIT_STATUS: "
            echo "Command radamsa -o $MUTATED_FILES -s $SEED_NUMBER -n $MUTATIONS_PER_SEED_FILE $SEED_FILE"
            continue
        fi

        for MUTATED_FILE in $MUTATED_DIR/*; do
            # Run the mutated file through the program that uses libspng
            # Capture the exit status of the program
            #strace -e trace=write $EXECUTABLE $MUTATED_FILE > $TMP_LOG_FILE 2>&1
            timeout 4s $EXECUTABLE $MUTATED_FILE > $TMP_LOG_FILE 2>&1
            EXIT_STATUS=$?

            if [ $SAVE_ALL_LOGS -eq 1 ]; then
                # Save all logs
                echo "Seed number $SEED_NUMBER for seed file: $SEED_NAME!" >> $LOG_FILE
                echo "Counter at: $COUNTER" >> $LOG_FILE
                echo "Mutated File: $MUTATED_FILE" >> $LOG_FILE
                echo "Exit Status: $EXIT_STATUS" >> $LOG_FILE
                echo "Output:" >> $LOG_FILE
                cat $TMP_LOG_FILE >> $LOG_FILE
                echo "----------------------------------------------------" >> $LOG_FILE
            fi
            
            # Print the current mutation count on the same line
            COUNTER=$((COUNTER + 1))
            
            if [ $EXIT_STATUS -eq 139 ]; then
                # Increment the segmentation fault count
                COUNTER_SEG_FAULTS=$((COUNTER_SEG_FAULTS + 1))

                # Log the segmentation fault and the mutated file
                echo "Segmentation fault detected on seed number $SEED_NUMBER for seed file: $SEED_NAME!" >> $SEGM_FAULT_CRASH_REPORT
                echo "Counter at: $COUNTER" >> $SEGM_FAULT_CRASH_REPORT
                echo "Mutated File: $MUTATED_FILE" >> $SEGM_FAULT_CRASH_REPORT
                echo "Exit Status: $EXIT_STATUS" >> $SEGM_FAULT_CRASH_REPORT
                echo "Error Output:" >> $SEGM_FAULT_CRASH_REPORT
                cat $TMP_LOG_FILE >> $SEGM_FAULT_CRASH_REPORT
                echo "----------------------------------------------------" >> $SEGM_FAULT_CRASH_REPORT

                if [ $SAVE_IMAGES -eq 1 ]; then
                    # Save the file with a unique name based on the mutation count
                    cp $MUTATED_FILE $SEGM_FAULT_LOG_DIR/${SEED_NAME}_$COUNTER.png
                fi

            elif [ $EXIT_STATUS -ne 0 ]; then
                # Increment the error count
                COUNTER_ERRORS=$((COUNTER_ERRORS + 1))

                if [ $EXIT_STATUS -eq 124 ]; then
                    echo "Timeout reached for file: $MUTATED_FILE" >> $LOG_FILE
                fi

                # Log the crash or other errors and the mutated file
                echo "Error detected on seed number $SEED_NUMBER for seed file: $SEED_NAME!" >> $ERROR_CRASH_REPORT
                echo "Counter at: $COUNTER" >> $ERROR_CRASH_REPORT
                echo "Mutated File: $MUTATED_FILE" >> $ERROR_CRASH_REPORT
                echo "Exit Status: $EXIT_STATUS" >> $ERROR_CRASH_REPORT
                echo "Error Output:" >> $ERROR_CRASH_REPORT
                cat $TMP_LOG_FILE >> $ERROR_CRASH_REPORT
                echo "----------------------------------------------------" >> $ERROR_CRASH_REPORT

                if [ $SAVE_IMAGES -eq 1 ]; then
                    # Save the file with a unique name based on the mutation count
                    cp $MUTATED_FILE $ERROR_LOG_DIR/${SEED_NAME}_$COUNTER.png
                fi
            fi

            # Print the current mutation count on the same line
            echo -ne "Counter $COUNTER - Mutating $SEED_NAME - Errors $COUNTER_ERRORS - Segm. faults $COUNTER_SEG_FAULTS - time: $(( $(date +%s) - $START_TIME ))\r"
            
        done  # Inner for loop for all mutated files
        
        # Delete the mutated files to save space
        rm -f $MUTATED_DIR/*

    done  # Outer for loop for all seed files
done  # Outer while loop for continuous fuzzing across all seed files
