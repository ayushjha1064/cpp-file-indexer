#!/usr/bin/env bash
set -euo pipefail

if (( $# < 2 || $# > 5 )); then
    echo "Usage: $0 <file-indexer-binary> <log-file> [reader-mode] [workers] [buffer-kb]" >&2
    exit 2
fi

indexer=$1
log_file=$2
reader_mode=${3:-mmap}
workers=${4:-1}
buffer_kb=${5:-1024}

if [[ ! -x "$indexer" ]]; then
    echo "Indexer binary is not executable: $indexer" >&2
    exit 2
fi
if [[ ! -f "$log_file" ]]; then
    echo "Log file is not a regular file: $log_file" >&2
    exit 2
fi
if [[ "$reader_mode" != read && "$reader_mode" != mmap ]]; then
    echo "Reader mode must be read or mmap." >&2
    exit 2
fi
if (( workers < 1 )); then
    echo "Workers must be at least 1." >&2
    exit 2
fi

stdout_file=$(mktemp "${TMPDIR:-/tmp}/file-indexer-memory-stdout.XXXXXX")
stderr_file=$(mktemp "${TMPDIR:-/tmp}/file-indexer-memory-stderr.XXXXXX")
trap 'rm -f "$stdout_file" "$stderr_file"' EXIT

case "$(uname -s)" in
    Darwin)
        set +e
        /usr/bin/time -l "$indexer" --query word --reader "$reader_mode" --workers "$workers" \
            --file "$log_file" --version benchmark --buffer "$buffer_kb" \
            --word __benchmark_missing_word__ >"$stdout_file" 2>"$stderr_file"
        time_status=$?
        set -e
        peak_memory=$(awk '/maximum resident set size/ { print $1; exit }' "$stderr_file")
        memory_unit=bytes
        ;;
    Linux)
        set +e
        /usr/bin/time -v "$indexer" --query word --reader "$reader_mode" --workers "$workers" \
            --file "$log_file" --version benchmark --buffer "$buffer_kb" \
            --word __benchmark_missing_word__ >"$stdout_file" 2>"$stderr_file"
        time_status=$?
        set -e
        peak_memory=$(awk -F: '/Maximum resident set size/ { gsub(/^[[:space:]]+/, "", $2); print $2; exit }' "$stderr_file")
        memory_unit=KiB
        ;;
    *)
        echo "Peak-memory measurement is supported only on Darwin and Linux." >&2
        exit 2
        ;;
esac

latency=$(awk '/^Execution Time \(s\): / { print $4 }' "$stdout_file")
if [[ -z "$latency" ]]; then
    cat "$stderr_file" >&2
    echo "Could not read benchmark latency." >&2
    exit 1
fi

file_bytes=$(wc -c < "$log_file")
throughput=$(awk -v bytes="$file_bytes" -v seconds="$latency" 'BEGIN { printf "%.2f", bytes / seconds / (1024 * 1024) }')
printf 'reader: %s; workers: %s; buffer: %s KiB\n' "$reader_mode" "$workers" "$buffer_kb"
printf 'latency: %ss\n' "$latency"
printf 'throughput: %s MiB/s\n' "$throughput"
if [[ -n "$peak_memory" ]]; then
    printf 'peak resident memory: %s %s\n' "$peak_memory" "$memory_unit"
else
    printf 'peak resident memory: unavailable (time exited %s)\n' "$time_status"
fi
