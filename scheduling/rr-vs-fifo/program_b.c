
#include "sched_common.h"
#include "workload_io.h"


int main(int argc, char *argv[])
{
    return run_experiment("Program-B (I/O-Bound)", io_task, want_header(argc, argv));
}
