

#ifndef _LINUX_FORK_GUARD_H
#define _LINUX_FORK_GUARD_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/uidgid.h>



#define FG_MAX_UID      65536 

#define FG_STAGE_OK       0   
#define FG_STAGE_THROTTLE 1    
#define FG_STAGE_FREEZE   2   
#define FG_STAGE_REAP     3   


#define FG_TIER_SILVER  0      
#define FG_TIER_GOLD    1      
#define FG_TIER_BRONZE  2      


#define FG_NICE_GOLD    (-7)
#define FG_NICE_SILVER  ( 0)
#define FG_NICE_BRONZE  ( 5)



struct fg_user_info {
    spinlock_t  lock;

    unsigned long   window_start;   
    unsigned int    fork_count;     
    int             stage;          
    unsigned long   last_over_ts;   


    unsigned long   cnt_throttle;
    unsigned long   cnt_freeze;
    unsigned long   cnt_reap;
    int             tier;           
    bool            active;
};


extern struct fg_user_info fg_users[FG_MAX_UID];
extern spinlock_t          fg_global_lock;   
extern uid_t               fg_gold_uid;      


extern unsigned int fg_enable;          
extern unsigned int fg_threshold;       
extern unsigned int fg_window_ms;       
extern unsigned int fg_throttle_ms;     
extern unsigned int fg_cooldown_s;      

// API called from kernel/fork.c and kernel/sched/core.c            



int  fg_check_fork(struct task_struct *tsk);

int  fg_get_nice(uid_t uid);

int  fg_set_tier(uid_t uid, int tier);


void fg_reset_all_tiers(void);


void fg_init(void);

#endif
