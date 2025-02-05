
#!/bin/bash

NUM_WORKERS=16
RUNS_PER_IMAGE=10000
PROGRAM_NAME="generic_test"

# Array to store PIDs of background jobs
declare -a JOB_PIDS=()
declare -a FILENAMES=()

# Output managing
output_dir="output/zzuf"
num_files=$(ls -la $output_dir | grep -E "$PROGRAM_NAME.*.out" | wc -l)

# Function to clean up background jobs on exit
cleanup() {
    echo "Caught signal, terminating background jobs..."
    for pid in "${JOB_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            echo "Killing process $pid"
            kill -TERM "$pid" 2>/dev/null
        fi
    done
    wait
    echo "All jobs terminated."
    # echo ${FILENAMES[@]}
    cat "${FILENAMES[@]}" > "$output_dir/TMP_FILE.out"
    rm "${FILENAMES[@]}"
    mv "$output_dir/TMP_FILE.out" "$output_dir/$PROGRAM_NAME$((num_files+1)).out"
    exit 0
}

# Trap signals (Ctrl+C or termination signals)
trap cleanup SIGINT SIGTERM

# Start fuzzing jobs in the background


for i in $(seq 1 $NUM_WORKERS); do
    FILENAMES+=("$output_dir/$PROGRAM_NAME$((num_files+i)).out")
done

for i in $(seq 1 "$NUM_WORKERS"); do
    if [ "$i" -eq 1 ]; then
        time NUM_RUNS=$RUNS_PER_IMAGE SANITIZER="" ./run_fuzzer.sh zzuf $PROGRAM_NAME -r 0.01 &
    else
        NUM_RUNS=$RUNS_PER_IMAGE SANITIZER="" ./run_fuzzer.sh zzuf $PROGRAM_NAME -r 0.01 > /dev/null 2>&1&
    fi
    sleep 1
    JOB_PIDS+=("$!")           # Store the PID of the background job
    echo "Started fuzzing job $i with PID $!"
done

# Wait for all jobs to finish
for pid in "${JOB_PIDS[@]}"; do
    wait "$pid"
done

echo "All jobs completed successfully."

cat "${FILENAMES[@]}" > "$output_dir/TMP_FILE.out"
rm "${FILENAMES[@]}"
mv "$output_dir/TMP_FILE.out" "$output_dir/$PROGRAM_NAME$((num_files+1)).out"