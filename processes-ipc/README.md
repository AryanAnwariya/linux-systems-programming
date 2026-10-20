# Processes & IPC

Three programs built on `fork`, pipes, message queues and `exec`.

## gcd-pipes

The parent repeatedly picks two random numbers from an array (removing each one as it's picked) and sends the pair to a child over one pipe. The child computes their GCD, prints it, sleeps for `time(NULL) % gcd` ms and sends the GCD back over a second pipe. The parent prints it and sleeps for `gcd` ms. This runs for n/2 rounds or until Ctrl+C.

```bash
../build/gcd_pipes
```

Concepts: `fork`, two one-way pipes for two-way communication, closing unused pipe ends so `read` sees EOF, `wait`.

## resource-monitor

A child process runs `ps aux` every *n* seconds, scores each process as `3·CPU% + 2·MEM%` and prints the top *k*. Every *r* rounds it blocks on a System V message queue until the parent sends it a decision typed by the user:

- a PID: show that process's details and kill it with `SIGKILL`
- `-1`: skip
- `-2`: delete the queue and quit

```bash
../build/resource_monitor     # then enter n, k, r when prompted
```

Concepts: `msgget`/`msgsnd`/`msgrcv`/`msgctl`, creating the queue before `fork` so both processes share it, `popen`, parsing with `sscanf` and `%n`, `qsort`.

## logtop

Prints the 5 most frequent values in a given column of a log file. It does this by building the shell pipeline `tr -s ' \t' ' ' | cut -d' ' -f<col> | sort | uniq -c | sort -rn | head -5` by hand: six child processes linked by five pipes.

```bash
../build/logtop logtop/access.log 1
```

```
    155 192.168.1.10
    124 192.168.1.14
     93 10.0.0.5
     90 10.0.0.9
     27 172.16.0.7
```

Concepts: `pipe`, `dup2` to rewire stdin/stdout, closing every unused pipe end in every process (otherwise `sort` never sees EOF), `execvp`, reaping each child with `waitpid`.
