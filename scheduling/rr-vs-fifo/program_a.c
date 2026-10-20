
#include "sched_common.h"
#include "workload_cpu.h"


int main(int argc, char *argv[])
{
    return run_experiment("Program-A (CPU-Bound)", cpu_task, want_header(argc, argv));
}
