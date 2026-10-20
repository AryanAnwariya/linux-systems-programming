
#ifndef SCHED_COMMON_H
#define SCHED_COMMON_H

#define _GNU_SOURCE

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define NUM_CHILDREN 8

typedef struct {
    struct timespec start;
    struct timespec end;
} child_times;

static double ts_diff_ms(struct timespec from, struct timespec to)
{
    return (double)(to.tv_sec - from.tv_sec) * 1000.0 +
           (double)(to.tv_nsec - from.tv_nsec) / 1e6;
}


static int write_all(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static int read_all(int fd, void *buf, size_t len)
{
    char *p = buf;
    while (len > 0) {
        ssize_t n = read(fd, p, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;      
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static long read_rr_timeslice_sysctl(void)
{
    long val = -1;
    FILE *fp = fopen("/proc/sys/kernel/sched_rr_timeslice_ms", "r");
    if (fp) {
        if (fscanf(fp, "%ld", &val) != 1) val = -1;
        fclose(fp);
    }
    return val;
}

static void print_policy_line(void)
{
    int policy = sched_getscheduler(0);

    if (policy == SCHED_RR) {
        long cfg = read_rr_timeslice_sysctl();
        struct timespec q;
        double eff = -1;
        if (sched_rr_get_interval(0, &q) == 0)
            eff = q.tv_sec * 1000.0 + q.tv_nsec / 1e6;

        if (cfg > 0)
            printf("Time Quantum: %ldms", cfg);
        else
            printf("Time Quantum: %.0fms", eff);
      
        if (eff > 0 && cfg > 0 && (eff < cfg - 0.5 || eff > cfg + 0.5))
            printf(" (effective %.0fms after jiffy rounding)", eff);
        printf("\n");
    } else if (policy == SCHED_FIFO) {
        printf("Scheduling Policy: SCHED_FIFO\n");
    } else {
        printf("Scheduling Policy: SCHED_OTHER "
               "(WARNING: not real-time; run with chrt -r / chrt -f)\n");
    }
}


static int run_experiment(const char *title, void (*task)(int), int print_header)
{
    int fds[NUM_CHILDREN][2];
    pid_t pids[NUM_CHILDREN];
    child_times times[NUM_CHILDREN];
    struct timespec ref, finish;

    clock_gettime(CLOCK_MONOTONIC, &ref);       

    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (pipe(fds[i]) == -1) {
            perror("pipe");
            return EXIT_FAILURE;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return EXIT_FAILURE;
        }

        if (pid == 0) {
            /* ---- child ---- */
            child_times t;
            clock_gettime(CLOCK_MONOTONIC, &t.start);  

            close(fds[i][0]);
            task(i);                            

            clock_gettime(CLOCK_MONOTONIC, &t.end);     
            if (write_all(fds[i][1], &t, sizeof t) != 0)
                _exit(EXIT_FAILURE);
            close(fds[i][1]);
            _exit(EXIT_SUCCESS);
        }

        /* ---- parent ---- */
        pids[i] = pid;
        close(fds[i][1]);
    }

    int failures = 0;
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (read_all(fds[i][0], &times[i], sizeof times[i]) != 0) {
            fprintf(stderr, "child %d (pid %d) did not report its timestamps\n",
                    i, (int)pids[i]);
            failures++;
            memset(&times[i], 0, sizeof times[i]);
        }
        close(fds[i][0]);
    }

    for (int i = 0; i < NUM_CHILDREN; i++) {
        int st;
        while (waitpid(pids[i], &st, 0) == -1 && errno == EINTR)
            ;
    }
    clock_gettime(CLOCK_MONOTONIC, &finish);

    struct rusage usage;
    if (getrusage(RUSAGE_CHILDREN, &usage) != 0) {
        perror("getrusage");
        return EXIT_FAILURE;
    }

    double sum_resp = 0, max_resp = 0, sum_tat = 0, max_tat = 0;
    int counted = 0;
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (times[i].start.tv_sec == 0 && times[i].start.tv_nsec == 0)
            continue;
        double resp = ts_diff_ms(ref, times[i].start);
        double tat  = ts_diff_ms(ref, times[i].end);
        sum_resp += resp;
        sum_tat  += tat;
        if (resp > max_resp) max_resp = resp;
        if (tat  > max_tat)  max_tat  = tat;
        counted++;
    }

    double elapsed_s = ts_diff_ms(ref, finish) / 1000.0;

    if (print_header) {
        printf("%s\n", title);
        printf("--------------------------------------------------\n");
    }
    print_policy_line();
    printf(" Involuntary Context Switches: %ld\n", usage.ru_nivcsw);
    printf(" Voluntary Context Switches: %ld\n", usage.ru_nvcsw);
    printf(" Average Response Time: %.2fms\n", counted ? sum_resp / counted : 0.0);
    printf(" Maximum Response Time: %.2fms\n", max_resp);
    printf(" Average Turnaround Time: %.2fms\n", counted ? sum_tat / counted : 0.0);
    printf(" Maximum Turnaround Time: %.2fms\n", max_tat);
    printf(" Throughput: %.2f children/s\n", elapsed_s > 0 ? counted / elapsed_s : 0.0);
    printf(" Total Elapsed Time: %.2fs\n", elapsed_s);
    printf("\n");
    fflush(stdout);

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}


static int want_header(int argc, char *argv[])
{
    return !(argc > 1 && strcmp(argv[1], "-n") == 0);
}

#endif
