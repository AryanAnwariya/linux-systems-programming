# fork-guard: fork-storm containment and CPU tiers (work in progress)

> **Status: written but not yet verified.** The patches have not been applied to a kernel tree, built or tested. `fg_init()`, which registers the `/proc` files and sysctls, is not yet called during boot. Treat this as design and implementation code, not a working feature.

## Idea

**Fork-storm containment.** The kernel counts forks per user ID within a sliding window (default: more than 200 forks per 1,000 ms). A user who goes over the limit is escalated in stages:

| Stage | Action |
|---|---|
| THROTTLE | The forking task sleeps `fg_throttle_ms` (40 ms) and the fork fails with `-EAGAIN` |
| FREEZE | `SIGSTOP` to the offending process group |
| REAP | `SIGCONT` then `SIGKILL` to the process group |

A user drops back to OK after `fg_cooldown_s` (10 s) with no further violations. Root (UID 0) is never limited.

**Per-user CPU tiers.** Each user is GOLD (nice -7), SILVER (nice 0, the default) or BRONZE (nice 5). Only one user can be GOLD at a time; promoting someone else demotes the previous GOLD user to SILVER. Changing a tier re-nices every process that user is running, and `sched_fork` gives new processes their user's tier priority.

## Files

```
include/linux/fork_guard.h   shared types, stage/tier constants, API
kernel/fork_guard.c          per-UID tracking, escalation, tiers, /proc + sysctl interface
kernel/fork.c.patch          calls fg_check_fork() at the start of kernel_clone()
kernel/sched_core.c.patch    sets new tasks' static_prio from the user's tier in sched_fork()
kernel/Makefile.patch        adds fork_guard.o to the build
userspace/tier.c             `tier set <user> <gold|silver|bronze>`, `tier reset`, `tier show`
```

Interfaces: `/proc/fork_guard` (stats), `/proc/sched_tier` (tiers), `/proc/fork_guard_ctl` (commands).

## Still to do

- [ ] Call `fg_init()` during boot (for example with `late_initcall`)
- [ ] Clean up `fork.c.patch` so it applies cleanly to the target tree
- [ ] Check that sleeping (`msleep`) in the fork path is safe in every calling context
- [ ] Build, boot and test with a controlled fork bomb under a non-root user
