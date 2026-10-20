# Round-Robin vs FIFO scheduling

An experiment measuring how Linux's two real-time policies, `SCHED_RR` and `SCHED_FIFO`, treat CPU-bound and I/O-bound workloads, and how the RR time slice changes the result.

## Setup

- `program_a` forks 8 **CPU-bound** children, each running a 150M-iteration floating-point loop.
- `program_b` forks 8 **I/O-bound** children, each doing 2,000 × 4 KB `write` + `fsync`.
- Every child records its start and end with `CLOCK_MONOTONIC` and sends them to the parent over a pipe. The parent reads context-switch counts from `getrusage(RUSAGE_CHILDREN)`.
- `run_experiments.sh` pins everything to CPU 0 (`taskset -c 0`) at real-time priority 50 (`chrt`). It sweeps `kernel.sched_rr_timeslice_ms` over 1, 10, 50, 150, 300 and 500 ms, runs FIFO once, and restores the original sysctl value on exit.

Metrics: response time (fork → child first runs), turnaround time (fork → child finishes), voluntary and involuntary context switches, throughput.

```bash
cd rr-vs-fifo
sudo ./run_experiments.sh 50     # needs root to set real-time policy and the sysctl
cat results/results.txt
```

## Results (kernel 7.0.0, 1 core)

### CPU-bound (`program_a`)

| Policy | Involuntary CS | Avg response | Max response | Avg turnaround | Throughput |
|---|---:|---:|---:|---:|---:|
| RR 1 ms | 2,281 | 3.36 ms | 6.79 ms | 2,270.72 ms | 3.52/s |
| RR 10 ms | 245 | 34.38 ms | 69.26 ms | 2,258.91 ms | 3.52/s |
| RR 50 ms | 60 | 174.76 ms | 349.69 ms | 2,114.18 ms | 3.63/s |
| RR 150 ms | 31 | 524.96 ms | 1,049.92 ms | 1,843.59 ms | 3.59/s |
| RR 300 ms | 22 | 963.58 ms | 1,928.42 ms | 1,239.16 ms | 3.63/s |
| RR 500 ms | 24 | 965.65 ms | 1,929.33 ms | 1,241.08 ms | 3.63/s |
| FIFO | 24 | 967.15 ms | 1,936.16 ms | 1,243.72 ms | 3.61/s |

### I/O-bound (`program_b`)

| Policy | Involuntary CS | Voluntary CS | Avg response | Avg turnaround | Throughput |
|---|---:|---:|---:|---:|---:|
| RR 1 ms | 326 | 50,793 | 0.46 ms | 1,423.11 ms | 5.57/s |
| RR 10 ms | 41 | 55,007 | 0.51 ms | 1,465.94 ms | 5.45/s |
| RR 50 ms | 11 | 55,254 | 0.49 ms | 1,431.63 ms | 5.57/s |
| RR 500 ms | 8 | 49,614 | 0.51 ms | 1,469.90 ms | 5.43/s |
| FIFO | 8 | 54,435 | 0.46 ms | 1,437.89 ms | 5.55/s |

## What the numbers show

- **The time slice is a trade-off between response and overhead.** For CPU-bound work, going from a 1 ms to a 50 ms slice cuts involuntary context switches by about 38× (2,281 → 60), but average response time rises about 52× (3.36 → 174.76 ms).
- **Short slices hurt turnaround.** With 1 ms slices every child finishes near the very end, so average turnaround is close to the total run time. Long slices let early children finish first, so average turnaround drops by about 45%.
- **RR turns into FIFO.** Each child needs about 275 ms of CPU. Once the slice is longer than that (300 and 500 ms), no child is ever preempted, and RR's numbers match FIFO's.
- **I/O-bound work doesn't care about the policy.** Children block on `fsync` long before any slice runs out, so almost every switch is voluntary (around 50,000 vs 8–326 involuntary), and response and turnaround stay flat across all settings.
- **Throughput is set by total work**, not by policy: about 3.6/s for CPU-bound and 5.5/s for I/O-bound in every configuration.
