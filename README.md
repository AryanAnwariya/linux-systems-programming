# Linux Systems Programming & Kernel Development

A collection of Linux systems programs in C, covering processes, inter-process communication, signals, a custom shell, POSIX threads, real-time scheduling experiments and Linux kernel changes (a new system call and a fork-rate limiter).

Built as coursework for **CS F372 Operating Systems** at BITS Pilani, Hyderabad (2026). Tested on Ubuntu running in Oracle VirtualBox; kernel work was done against Linux 6.14.

## What's inside

| Area | Project | Key concepts |
|---|---|---|
| Processes & IPC | [`processes-ipc/gcd-pipes`](processes-ipc/gcd-pipes) | `fork`, two unnamed pipes for parent↔child talk |
| Processes & IPC | [`processes-ipc/resource-monitor`](processes-ipc/resource-monitor) | System V message queues, `popen("ps aux")`, `kill(SIGKILL)` |
| Processes & IPC | [`processes-ipc/logtop`](processes-ipc/logtop) | 6-stage `tr \| cut \| sort \| uniq \| sort \| head` pipeline built by hand with `pipe`, `dup2`, `execvp` |
| Shell | [`shell/belt-shell`](shell/belt-shell) | REPL, built-in vs external commands, `fork`/`exec`/`wait`, `SIGINT` handling with `sigaction` |
| Threads | [`threads/report-card`](threads/report-card) | `pthread` per student, parallel insertion sort over *k* segments, k-way merge |
| Threads | [`threads/sudoku-analyzer`](threads/sudoku-analyzer) | 27 validator threads + 9 analysis threads, bitmasks, conflict heatmap |
| Scheduling | [`scheduling/rr-vs-fifo`](scheduling/rr-vs-fifo) | `SCHED_RR` vs `SCHED_FIFO`, time-slice sweep, `getrusage`, `taskset`, `chrt` |
| Kernel | [`kernel/setnice-logged`](kernel/setnice-logged) | New syscall in Linux 6.14: `SYSCALL_DEFINE1`, syscall table, `set_user_nice`, `printk` |
| Kernel | [`kernel/fork-guard`](kernel/fork-guard) | *Work in progress:* fork-storm detection and per-user CPU tiers inside the kernel |

## Highlight: Round-Robin vs FIFO

Eight CPU-bound children pinned to one core at real-time priority 50:

| Policy / quantum | Involuntary context switches | Avg response time | Avg turnaround time |
|---|---:|---:|---:|
| RR, 1 ms | 2,281 | 3.36 ms | 2,270.72 ms |
| RR, 10 ms | 245 | 34.38 ms | 2,258.91 ms |
| RR, 50 ms | 60 | 174.76 ms | 2,114.18 ms |
| RR, 150 ms | 31 | 524.96 ms | 1,843.59 ms |
| RR, 300 ms | 22 | 963.58 ms | 1,239.16 ms |
| FIFO | 24 | 967.15 ms | 1,243.72 ms |

Small time slices give fast response but many context switches. Once the slice is longer than a child's whole CPU burst (about 275 ms here), RR behaves exactly like FIFO. Full results, including the I/O-bound workload, are in [`scheduling/rr-vs-fifo`](scheduling/rr-vs-fifo).

## Building

Requires `gcc` and `make` on Linux.

```bash
make            # builds every user-space program into ./build
make clean
```

Kernel projects are built as part of a kernel tree. See each project's README.

## Team

Group project by Aryan Anwariya, Dhruv Kothari, Mellimpudi Tejas and Aarati Dhamele.
