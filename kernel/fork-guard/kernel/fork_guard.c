

#define pr_fmt(fmt) "fork_guard: " fmt

#include <linux/fork_guard.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/cred.h>
#include <linux/uidgid.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/signal.h>
#include <linux/pid.h>
#include <linux/rcupdate.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sysctl.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/kstrtox.h>


struct fg_user_info fg_users[FG_MAX_UID];
EXPORT_SYMBOL(fg_users);

spinlock_t fg_global_lock;
EXPORT_SYMBOL(fg_global_lock);

uid_t fg_gold_uid = (uid_t)UINT_MAX;   
EXPORT_SYMBOL(fg_gold_uid);



unsigned int fg_enable      = 1;    
unsigned int fg_threshold   = 200;  
unsigned int fg_window_ms   = 1000;
unsigned int fg_throttle_ms = 40;   
unsigned int fg_cooldown_s  = 10;  

EXPORT_SYMBOL(fg_enable);
EXPORT_SYMBOL(fg_threshold);
EXPORT_SYMBOL(fg_window_ms);
EXPORT_SYMBOL(fg_throttle_ms);
EXPORT_SYMBOL(fg_cooldown_s);


static unsigned int sysctl_zero  = 0;
static unsigned int sysctl_one   = 1;
static unsigned int sysctl_max_w = 60000;  
static unsigned int sysctl_max_c = 300;    


static int tier_to_nice(int tier)
{
    switch (tier) {
    case FG_TIER_GOLD:   return FG_NICE_GOLD;
    case FG_TIER_BRONZE: return FG_NICE_BRONZE;
    default:             return FG_NICE_SILVER;
    }
}

static const char *tier_name(int tier)
{
    switch (tier) {
    case FG_TIER_GOLD:   return "GOLD";
    case FG_TIER_BRONZE: return "BRONZE";
    default:             return "SILVER";
    }
}


static const char *stage_name(int stage)
{
    switch (stage) {
    case FG_STAGE_THROTTLE: return "THROTTLE";
    case FG_STAGE_FREEZE:   return "FREEZE";
    case FG_STAGE_REAP:     return "REAP";
    default:                return "OK";
    }
}


static void re_nice_user(uid_t uid, int nice_val)
{
    struct task_struct *p;

    rcu_read_lock();
    for_each_process(p) {
        const struct cred *cred;

        cred = __task_cred(p);
        if (!uid_eq(cred->uid, make_kuid(&init_user_ns, uid)))
            continue;

        
        get_task_struct(p);
        rcu_read_unlock();

        set_user_nice(p, nice_val);

        rcu_read_lock();
        put_task_struct(p);
    }
    rcu_read_unlock();
}


static int do_escalate(struct fg_user_info *info, uid_t uid,
                       struct pid *pgrp, unsigned long *flags)
{
    int new_stage = info->stage + 1;
    if (new_stage > FG_STAGE_REAP)
        new_stage = FG_STAGE_REAP;

    info->stage        = new_stage;
    info->last_over_ts = jiffies;

    switch (new_stage) {

    case FG_STAGE_THROTTLE:
        info->cnt_throttle++;
        spin_unlock_irqrestore(&info->lock, *flags);

        pr_warn("THROTTLE uid=%u pgid=%d (>%u forks in %u ms)\n",
                uid, pid_nr(pgrp), fg_threshold, fg_window_ms);

        
        msleep(fg_throttle_ms);

        spin_lock_irqsave(&info->lock, *flags);
        break;

    case FG_STAGE_FREEZE:
        info->cnt_freeze++;
        get_pid(pgrp);         
        spin_unlock_irqrestore(&info->lock, *flags);

        pr_warn("FREEZE  uid=%u pgid=%d (SIGSTOP process group)\n",
                uid, pid_nr(pgrp));

        kill_pgrp(pgrp, SIGSTOP, 1);
        put_pid(pgrp);

        spin_lock_irqsave(&info->lock, *flags);
        break;

    case FG_STAGE_REAP:
        info->cnt_reap++;
        get_pid(pgrp);
        spin_unlock_irqrestore(&info->lock, *flags);

        pr_alert("REAP    uid=%u pgid=%d (SIGKILL process group)\n",
                 uid, pid_nr(pgrp));

       
        kill_pgrp(pgrp, SIGCONT, 1);
        kill_pgrp(pgrp, SIGKILL, 1);
        put_pid(pgrp);

        spin_lock_irqsave(&info->lock, *flags);
        break;
    }

    return new_stage;
}


int fg_check_fork(struct task_struct *tsk)
{
    uid_t uid;
    struct fg_user_info *info;
    unsigned long flags;
    unsigned long now;
    unsigned long window_jiffies;
    struct pid *pgrp;
    int over;

  
    if (!fg_enable)
        return 0;

    uid = from_kuid(&init_user_ns, task_uid(tsk));

    
    if (uid == 0)
        return 0;

    if (uid >= FG_MAX_UID)
        return 0;

    info = &fg_users[uid];
    window_jiffies = msecs_to_jiffies(fg_window_ms);
    now = jiffies;

    spin_lock_irqsave(&info->lock, flags);
    info->active = true;


    if (time_after(now, info->window_start + window_jiffies)) {
        info->window_start = now;
        info->fork_count   = 0;

        
        if (info->stage > FG_STAGE_OK &&
            time_after(now, info->last_over_ts +
                             (unsigned long)fg_cooldown_s * HZ)) {
            info->stage = FG_STAGE_OK;
        }
    }

    info->fork_count++;
    over = (info->fork_count > fg_threshold);

    if (!over) {
        spin_unlock_irqrestore(&info->lock, flags);
        return 0; 
    }

    
    pgrp = get_pid(task_pgrp(tsk));

    do_escalate(info, uid, pgrp, &flags);
    /* lock re-held after do_escalate */

    put_pid(pgrp);
    spin_unlock_irqrestore(&info->lock, flags);

   
    return -EAGAIN;
}
EXPORT_SYMBOL(fg_check_fork);



int fg_get_nice(uid_t uid)
{
    int tier;
    unsigned long flags;

    if (uid >= FG_MAX_UID)
        return FG_NICE_SILVER;

    spin_lock_irqsave(&fg_users[uid].lock, flags);
    tier = fg_users[uid].tier;
    spin_unlock_irqrestore(&fg_users[uid].lock, flags);

    return tier_to_nice(tier);
}
EXPORT_SYMBOL(fg_get_nice);

int fg_set_tier(uid_t uid, int tier)
{
    unsigned long flags, gflags;
    uid_t old_gold = (uid_t)UINT_MAX;

    if (uid == 0 || uid >= FG_MAX_UID)
        return -EINVAL;
    if (tier < FG_TIER_SILVER || tier > FG_TIER_BRONZE)
        return -EINVAL;

    
    if (tier == FG_TIER_GOLD) {
        spin_lock_irqsave(&fg_global_lock, gflags);
        old_gold    = fg_gold_uid;
        fg_gold_uid = uid;
        spin_unlock_irqrestore(&fg_global_lock, gflags);

       
        if (old_gold != (uid_t)UINT_MAX && old_gold != uid) {
            spin_lock_irqsave(&fg_users[old_gold].lock, flags);
            fg_users[old_gold].tier = FG_TIER_SILVER;
            spin_unlock_irqrestore(&fg_users[old_gold].lock, flags);

            pr_warn("tier: uid=%u demoted GOLD→SILVER (new GOLD is uid=%u)\n",
                    old_gold, uid);
            re_nice_user(old_gold, FG_NICE_SILVER);
        }
    } else {
      
        spin_lock_irqsave(&fg_global_lock, gflags);
        if (fg_gold_uid == uid)
            fg_gold_uid = (uid_t)UINT_MAX;
        spin_unlock_irqrestore(&fg_global_lock, gflags);
    }

    
    spin_lock_irqsave(&fg_users[uid].lock, flags);
    fg_users[uid].tier   = tier;
    fg_users[uid].active = true;
    spin_unlock_irqrestore(&fg_users[uid].lock, flags);

    pr_warn("tier: uid=%u -> %s (nice %d)\n",
            uid, tier_name(tier), tier_to_nice(tier));

   
    re_nice_user(uid, tier_to_nice(tier));

    return 0;
}
EXPORT_SYMBOL(fg_set_tier);

void fg_reset_all_tiers(void)
{
    unsigned long flags, gflags;
    uid_t uid;

    spin_lock_irqsave(&fg_global_lock, gflags);
    fg_gold_uid = (uid_t)UINT_MAX;
    spin_unlock_irqrestore(&fg_global_lock, gflags);

    for (uid = 1; uid < FG_MAX_UID; uid++) {
        int was;

        spin_lock_irqsave(&fg_users[uid].lock, flags);
        was = fg_users[uid].tier;
        fg_users[uid].tier = FG_TIER_SILVER;
        spin_unlock_irqrestore(&fg_users[uid].lock, flags);

        if (was != FG_TIER_SILVER)
            re_nice_user(uid, FG_NICE_SILVER);
    }

    pr_warn("tier: all users reset to SILVER\n");
}
EXPORT_SYMBOL(fg_reset_all_tiers);



static int fg_proc_show(struct seq_file *m, void *v)
{
    uid_t uid;

    /* Header line with live tunable values */
    seq_printf(m,
        "enabled=%u threshold=%u window_ms=%u throttle_ms=%u cooldown_s=%u\n",
        fg_enable, fg_threshold, fg_window_ms,
        fg_throttle_ms, fg_cooldown_s);

    /* Column headers */
    seq_printf(m, "%-6s %-8s %-7s %-7s %-10s %-8s %-6s\n",
               "UID", "TIER", "FORKS", "STAGE",
               "THROTTLE", "FREEZE", "REAP");

    for (uid = 1; uid < FG_MAX_UID; uid++) {
        struct fg_user_info *info = &fg_users[uid];
        unsigned long flags;
        bool active;
        int tier, stage;
        unsigned int forks;
        unsigned long ct, cf, cr;

        spin_lock_irqsave(&info->lock, flags);
        active = info->active;
        tier   = info->tier;
        forks  = info->fork_count;
        stage  = info->stage;
        ct     = info->cnt_throttle;
        cf     = info->cnt_freeze;
        cr     = info->cnt_reap;
        spin_unlock_irqrestore(&info->lock, flags);

        if (!active)
            continue;

        seq_printf(m, "%-6u %-8s %-7u %-7d %-10lu %-8lu %-6lu\n",
                   uid, tier_name(tier), forks,
                   stage, ct, cf, cr);
    }

    return 0;
}

static int fg_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, fg_proc_show, NULL);
}

static const struct proc_ops fg_proc_ops = {
    .proc_open    = fg_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};


static int fg_tier_show(struct seq_file *m, void *v)
{
    uid_t uid;

    seq_printf(m, "%-6s %-8s %-6s\n", "UID", "TIER", "NICE");

    for (uid = 1; uid < FG_MAX_UID; uid++) {
        struct fg_user_info *info = &fg_users[uid];
        unsigned long flags;
        int tier;
        bool active;

        spin_lock_irqsave(&info->lock, flags);
        tier   = info->tier;
        active = info->active;
        spin_unlock_irqrestore(&info->lock, flags);

       
        if (!active && tier == FG_TIER_SILVER)
            continue;
        if (!active)
            continue;
        if (tier == FG_TIER_SILVER)
            continue;  

        seq_printf(m, "%-6u %-8s %-6d\n",
                   uid, tier_name(tier), tier_to_nice(tier));
    }

    return 0;
}

static int fg_tier_open(struct inode *inode, struct file *file)
{
    return single_open(file, fg_tier_show, NULL);
}

static const struct proc_ops fg_tier_ops = {
    .proc_open    = fg_tier_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static ssize_t fg_ctl_write(struct file *file, const char __user *ubuf,
                            size_t count, loff_t *ppos)
{
    char buf[64];
    size_t len;
    int ret = 0;

    if (count == 0)
        return 0;

    len = min(count, sizeof(buf) - 1);
    if (copy_from_user(buf, ubuf, len))
        return -EFAULT;
    buf[len] = '\0';

    if (len > 0 && buf[len - 1] == '\n')
        buf[--len] = '\0';

    if (strncmp(buf, "reset", 5) == 0) {
        fg_reset_all_tiers();
        goto out;
    }

    if (strncmp(buf, "set ", 4) == 0) {
        unsigned int uid_val, tier_val;

        if (sscanf(buf + 4, "%u %u", &uid_val, &tier_val) != 2) {
            pr_warn("ctl: bad 'set' syntax, expected: set <uid> <0|1|2>\n");
            return -EINVAL;
        }
        ret = fg_set_tier((uid_t)uid_val, (int)tier_val);
        if (ret < 0)
            return ret;
        goto out;
    }

    pr_warn("ctl: unknown command '%s'\n", buf);
    return -EINVAL;

out:
    return (ssize_t)count;
}

static const struct proc_ops fg_ctl_ops = {
    .proc_write   = fg_ctl_write,
    .proc_lseek   = noop_llseek,
};



static struct ctl_table fg_sysctl_table[] = {
    {
        .procname       = "fg_enable",
        .data           = &fg_enable,
        .maxlen         = sizeof(fg_enable),
        .mode           = 0644,
        .proc_handler   = proc_douintvec_minmax,
        .extra1         = &sysctl_zero,
        .extra2         = &sysctl_one,
    },
    {
        .procname       = "fg_threshold",
        .data           = &fg_threshold,
        .maxlen         = sizeof(fg_threshold),
        .mode           = 0644,
        .proc_handler   = proc_douintvec_minmax,
        .extra1         = &sysctl_one,
        .extra2         = NULL,
    },
    {
        .procname       = "fg_window_ms",
        .data           = &fg_window_ms,
        .maxlen         = sizeof(fg_window_ms),
        .mode           = 0644,
        .proc_handler   = proc_douintvec_minmax,
        .extra1         = &sysctl_one,
        .extra2         = &sysctl_max_w,
    },
    {
        .procname       = "fg_throttle_ms",
        .data           = &fg_throttle_ms,
        .maxlen         = sizeof(fg_throttle_ms),
        .mode           = 0644,
        .proc_handler   = proc_douintvec_minmax,
        .extra1         = &sysctl_zero,
        .extra2         = &sysctl_max_w,
    },
    {
        .procname       = "fg_cooldown_s",
        .data           = &fg_cooldown_s,
        .maxlen         = sizeof(fg_cooldown_s),
        .mode           = 0644,
        .proc_handler   = proc_douintvec_minmax,
        .extra1         = &sysctl_one,
        .extra2         = &sysctl_max_c,
    },
    { }   
};

static struct ctl_table_header *fg_sysctl_header;



void __init fg_init(void)
{
    uid_t uid;

    spin_lock_init(&fg_global_lock);

    for (uid = 0; uid < FG_MAX_UID; uid++) {
        struct fg_user_info *info = &fg_users[uid];
        spin_lock_init(&info->lock);
        info->window_start  = jiffies;
        info->fork_count    = 0;
        info->stage         = FG_STAGE_OK;
        info->last_over_ts  = 0;
        info->cnt_throttle  = 0;
        info->cnt_freeze    = 0;
        info->cnt_reap      = 0;
        info->tier          = FG_TIER_SILVER;
        info->active        = false;
    }

   
    fg_sysctl_header = register_sysctl("kernel", fg_sysctl_table);
    if (!fg_sysctl_header)
        pr_warn("failed to register sysctl table\n");

    
    if (!proc_create("fork_guard", 0444, NULL, &fg_proc_ops))
        pr_warn("failed to create /proc/fork_guard\n");

  
    if (!proc_create("sched_tier", 0444, NULL, &fg_tier_ops))
        pr_warn("failed to create /proc/sched_tier\n");

    
    if (!proc_create("fork_guard_ctl", 0200, NULL, &fg_ctl_ops))
        pr_warn("failed to create /proc/fork_guard_ctl\n");

    pr_warn("initialised (threshold=%u/%ums, gold nice=%d)\n",
            fg_threshold, fg_window_ms, FG_NICE_GOLD);
}
