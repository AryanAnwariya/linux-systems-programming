# setnice_logged: a custom Linux system call

A new system call added to **Linux 6.14** (x86-64), which was then rebuilt and booted.

```c
long setnice_logged(int nice_val);
```

1. Rejects values outside `[-20, 19]` with `-EINVAL`, without changing anything.
2. Sets the calling process's nice value using the kernel's `set_user_nice()`.
3. Logs the PID, process name, old nice value and new nice value with `printk`, so they show up in `dmesg`.

## Files

```
kernel/setnice_logged.c        the syscall itself (SYSCALL_DEFINE1)
patches/syscall_64.tbl.snippet registers syscall number 470
patches/syscalls.h.snippet     prototype declaration
patches/Makefile.snippet       adds setnice_logged.o to the kernel build
test/test_setnice.c            user-space test using syscall(470, ...)
```

## Adding it to a kernel tree

```bash
cd linux-6.14
cp <repo>/kernel/setnice-logged/kernel/setnice_logged.c kernel/
# then apply the three one-line edits in patches/ by hand

make olddefconfig
make -j"$(nproc)"
sudo make modules_install && sudo make install
sudo reboot            # boot into the new kernel
uname -r               # confirm
```

## Testing

```bash
make -C ../.. build/test_setnice
../../build/test_setnice           # calls setnice_logged(5)
dmesg | grep setnice_logged
```

```
setnice_logged: pid=1234 comm=test_setnice old=0 new=5
```

We tested a valid value (5) and an invalid one (25). The invalid call returns -1 with `errno = EINVAL` and logs nothing.

## Notes

- `set_user_nice()` handles run-queue locking and recalculates the task's load weight, so the syscall doesn't touch `task_struct` priority fields itself.
- The log prints `task_nice(current)` after the change rather than the value that was asked for, so it shows what the kernel actually applied.
- The number 470 was the first free slot in this kernel's `syscall_64.tbl`. It may be different on another kernel version.
