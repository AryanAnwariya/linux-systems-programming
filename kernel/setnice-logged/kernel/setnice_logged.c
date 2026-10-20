#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/errno.h>

SYSCALL_DEFINE1(setnice_logged, int, nice_val)
{
    int previous;

    if (nice_val < -20 || nice_val > 19)
        return -EINVAL;

    previous = task_nice(current);

    set_user_nice(current, nice_val);

    printk(KERN_INFO
           "setnice_logged: pid=%d comm=%s old=%d new=%d\n",
           current->pid,
           current->comm,
           previous,
           task_nice(current));

    return 0;
}
