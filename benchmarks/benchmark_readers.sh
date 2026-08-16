#!/usr/bin/env bash
set -euo pipefail

if (( $# < 2 || $# > 4 )); then
    echo "Usage: $0 <file-indexer-binary> <log-file> [buffer-kb] [iterations]" >&2
    exit 2
fi

indexer=$1
log_file=$2
buffer_kb=${3:-1024}
iterations=${4:-5}

if [[ ! -x "$indexer" ]]; then
    echo "Indexer binary is not executable: $indexer" >&2
    exit 2
fi
if [[ ! -f "$log_file" ]]; then
    echo "Log file is not a regular file: $log_file" >&2
    exit 2
fi
if (( iterations < 1 )); then
    echo "Iterations must be at least 1." >&2
    exit 2
fi

file_bytes=$(wc -c < "$log_file")
printf 'file size: %s bytes; buffer: %s KiB; workers: 1\n' "$file_bytes" "$buffer_kb"
for reader in read mmap; do
    total=0
    echo "$reader:"

    for ((iteration = 1; iteration <= iterations; ++iteration)); do
        output=$("$indexer" --query word --reader "$reader" --file "$log_file" \
            --version benchmark --buffer "$buffer_kb" --word __benchmark_missing_word__)
        elapsed=$(awk '/^Execution Time \(s\): / { print $4 }' <<< "$output")

        if [[ -z "$elapsed" ]]; then
            echo "Could not read execution time from indexer output." >&2
            exit 1
        fi

        total=$(awk -v total="$total" -v elapsed="$elapsed" 'BEGIN { print total + elapsed }')
        printf '  run %d: %ss\n' "$iteration" "$elapsed"
    done

    average=$(awk -v total="$total" -v iterations="$iterations" 'BEGIN { printf "%.5f", total / iterations }')
    throughput=$(awk -v bytes="$file_bytes" -v seconds="$average" 'BEGIN { printf "%.2f", bytes / seconds / (1024 * 1024) }')
    printf '  average: %ss\n' "$average"
    printf '  throughput: %s MiB/s\n' "$throughput"
done
