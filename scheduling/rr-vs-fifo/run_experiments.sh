#!/usr/bin/env bash

set -u

PRIO="${1:-50}"
QUANTA=(1 10 50 150 300 500)
OUT="results/results.txt"
SYSCTL_FILE=/proc/sys/kernel/sched_rr_timeslice_ms

if [[ $EUID -ne 0 ]]; then
    echo "Run as root: sudo $0" >&2
    exit 1
fi

cd "$(dirname "$0")"
gcc -Wall -Wextra -O2 -o program_a program_a.c || exit 1
gcc -Wall -Wextra -O2 -o program_b program_b.c || exit 1

ORIGINAL=$(cat "$SYSCTL_FILE")
trap 'echo "$ORIGINAL" > "$SYSCTL_FILE"' EXIT

{
    echo "CS F372 Problem 2 - Round-Robin vs FIFO"
    echo "Kernel: $(uname -r)   Date: $(date)"
    echo "Pinned to CPU 0, real-time priority $PRIO, 8 children per run"
    echo
} > "$OUT"

run_program() {
    local prog="$1" title="$2"
    {
        echo "$title"
        echo "--------------------------------------------------"
    } >> "$OUT"

    for q in "${QUANTA[@]}"; do
        sysctl -w kernel.sched_rr_timeslice_ms="$q" > /dev/null
        echo ">> $title, SCHED_RR, quantum ${q}ms"
        taskset -c 0 chrt -r "$PRIO" "./$prog" -n >> "$OUT"
    done

    echo "$ORIGINAL" > "$SYSCTL_FILE"
    echo ">> $title, SCHED_FIFO"
    if ! timeout 30 taskset -c 0 chrt -f "$PRIO" "./$prog" -n >> "$OUT"; then
        echo "Scheduling Policy: SCHED_FIFO" >> "$OUT"
        echo " (run did not finish within the 30 s timeout)" >> "$OUT"
        echo >> "$OUT"
    fi
}

run_program program_a "Program-A (CPU-Bound)"
run_program program_b "Program-B (I/O-Bound)"

echo "Done. Results written to $(pwd)/$OUT"
