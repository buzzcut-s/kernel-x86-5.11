#ifndef ALT_SCHED_H
#define ALT_SCHED_H

#include <linux/sched.h>

#include <linux/sched/clock.h>
#include <linux/sched/cpufreq.h>
#include <linux/sched/cputime.h>
#include <linux/sched/debug.h>
#include <linux/sched/init.h>
#include <linux/sched/isolation.h>
#include <linux/sched/loadavg.h>
#include <linux/sched/mm.h>
#include <linux/sched/nohz.h>
#include <linux/sched/signal.h>
#include <linux/sched/stat.h>
#include <linux/sched/sysctl.h>
#include <linux/sched/task.h>
#include <linux/sched/topology.h>
#include <linux/sched/wake_q.h>

#include <uapi/linux/sched/types.h>

#include <linux/cgroup.h>
#include <linux/cpufreq.h>
#include <linux/cpuidle.h>
#include <linux/cpuset.h>
#include <linux/ctype.h>
#include <linux/kthread.h>
#include <linux/livepatch.h>
#include <linux/membarrier.h>
#include <linux/proc_fs.h>
#include <linux/psi.h>
#include <linux/slab.h>
#include <linux/stop_machine.h>
#include <linux/suspend.h>
#include <linux/swait.h>
#include <linux/syscalls.h>
#include <linux/tsacct_kern.h>

#include <asm/tlb.h>

#ifdef CONFIG_PARAVIRT
# include <asm/paravirt.h>
#endif

#include "cpupri.h"

#include <trace/events/sched.h>

#ifdef CONFIG_SCHED_BMQ
#include "bmq.h"
#endif
#ifdef CONFIG_SCHED_PDS
#include "pds.h"
#endif

#ifdef CONFIG_SCHED_DEBUG
# define SCHED_WARN_ON(x)	WARN_ONCE(x, #x)
#else
# define SCHED_WARN_ON(x)	({ (void)(x), 0; })
#endif

/*
 * Increase resolution of nice-level calculations for 64-bit architectures.
 * The extra resolution improves shares distribution and load balancing of
 * low-weight task groups (eg. nice +19 on an autogroup), deeper taskgroup
 * hierarchies, especially on larger systems. This is not a user-visible change
 * and does not change the user-interface for setting shares/weights.
 *
 * We increase resolution only if we have enough bits to allow this increased
 * resolution (i.e. 64-bit). The costs for increasing resolution when 32-bit
 * are pretty high and the returns do not justify the increased costs.
 *
 * Really only required when CONFIG_FAIR_GROUP_SCHED=y is also set, but to
 * increase coverage and consistency always enable it on 64-bit platforms.
 */
#ifdef CONFIG_64BIT
# define NICE_0_LOAD_SHIFT	(SCHED_FIXEDPOINT_SHIFT + SCHED_FIXEDPOINT_SHIFT)
# define scale_load(w)		((w) << SCHED_FIXEDPOINT_SHIFT)
# define scale_load_down(w) \
({ \
	unsigned long __w = (w); \
	if (__w) \
		__w = max(2UL, __w >> SCHED_FIXEDPOINT_SHIFT); \
	__w; \
})
#else
# define NICE_0_LOAD_SHIFT	(SCHED_FIXEDPOINT_SHIFT)
# define scale_load(w)		(w)
# define scale_load_down(w)	(w)
#endif

#ifdef CONFIG_FAIR_GROUP_SCHED
#define ROOT_TASK_GROUP_LOAD	NICE_0_LOAD

/*
 * A weight of 0 or 1 can cause arithmetics problems.
 * A weight of a cfs_rq is the sum of weights of which entities
 * are queued on this cfs_rq, so a weight of a entity should not be
 * too large, so as the shares value of a task group.
 * (The default weight is 1024 - so there's no practical
 *  limitation from this.)
 */
#define MIN_SHARES		(1UL <<  1)
#define MAX_SHARES		(1UL << 18)
#endif

/* task_struct::on_rq states: */
#define TASK_ON_RQ_QUEUED	1
#define TASK_ON_RQ_MIGRATING	2

static inline int task_on_rq_queued(struct task_struct *p)
{
	return p->on_rq == TASK_ON_RQ_QUEUED;
}

static inline int task_on_rq_migrating(struct task_struct *p)
{
	return READ_ONCE(p->on_rq) == TASK_ON_RQ_MIGRATING;
}

/*
 * wake flags
 */
#define WF_SYNC		0x01		/* waker goes to sleep after wakeup */
#define WF_FORK		0x02		/* child wakeup after fork */
#define WF_MIGRATED	0x04		/* internal use, task got migrated */
#define WF_ON_CPU	0x08		/* Wakee is on_rq */

/*
 * This is the main, per-CPU runqueue data structure.
 * This data should only be modified by the local cpu.
 */
struct rq {
	/* runqueue lock: */
	raw_spinlock_t lock;

	struct task_struct __rcu *curr;
	struct task_struct *idle, *stop, *skip;
	struct mm_struct *prev_mm;

#ifdef CONFIG_SCHED_BMQ
	struct bmq queue;
#endif
#ifdef CONFIG_SCHED_PDS
	struct skiplist_node sl_header;
#endif
	unsigned long watermark;

	/* switch count */
	u64 nr_switches;

	atomic_t nr_iowait;

#ifdef CONFIG_MEMBARRIER
	int membarrier_state;
#endif

#ifdef CONFIG_SMP
	int cpu;		/* cpu of this runqueue */
	bool online;

	unsigned int		ttwu_pending;
	unsigned char		nohz_idle_balance;
	unsigned char		idle_balance;

#ifdef CONFIG_HAVE_SCHED_AVG_IRQ
	struct sched_avg	avg_irq;
#endif

#ifdef CONFIG_SCHED_SMT
	int active_balance;
	struct cpu_stop_work	active_balance_work;
#endif
	struct callback_head	*balance_callback;
	unsigned char		balance_push;
#ifdef CONFIG_HOTPLUG_CPU
	struct rcuwait		hotplug_wait;
#endif
	unsigned int		nr_pinned;
#endif /* CONFIG_SMP */
#ifdef CONFIG_IRQ_TIME_ACCOUNTING
	u64 prev_irq_time;
#endif /* CONFIG_IRQ_TIME_ACCOUNTING */
#ifdef CONFIG_PARAVIRT
	u64 prev_steal_time;
#endif /* CONFIG_PARAVIRT */
#ifdef CONFIG_PARAVIRT_TIME_ACCOUNTING
	u64 prev_steal_time_rq;
#endif /* CONFIG_PARAVIRT_TIME_ACCOUNTING */

	/* calc_load related fields */
	unsigned long calc_load_update;
	long calc_load_active;

	u64 clock, last_tick;
	u64 last_ts_switch;
	u64 clock_task;

	unsigned int  nr_running;
	unsigned long nr_uninterruptible;

#ifdef CONFIG_SCHED_HRTICK
#ifdef CONFIG_SMP
	call_single_data_t hrtick_csd;
#endif
	struct hrtimer hrtick_timer;
#endif

#ifdef CONFIG_SCHEDSTATS

	/* latency stats */
	struct sched_info rq_sched_info;
	unsigned long long rq_cpu_time;
	/* could above be rq->cfs_rq.exec_clock + rq->rt_rq.rt_runtime ? */

	/* sys_sched_yield() stats */
	unsigned int yld_count;

	/* schedule() stats */
	unsigned int sched_switch;
	unsigned int sched_count;
	unsigned int sched_goidle;

	/* try_to_wake_up() stats */
	unsigned int ttwu_count;
	unsigned int ttwu_local;
#endif /* CONFIG_SCHEDSTATS */

#ifdef CONFIG_CPU_IDLE
	/* Must be inspected within a rcu lock section */
	struct cpuidle_state *idle_state;
#endif

#ifdef CONFIG_NO_HZ_COMMON
#ifdef CONFIG_SMP
	call_single_data_t	nohz_csd;
#endif
	atomic_t		nohz_flags;
#endif /* CONFIG_NO_HZ_COMMON */
};

extern unsigned long calc_load_update;
extern atomic_long_t calc_load_tasks;

extern void calc_global_load_tick(struct rq *this_rq);
extern long calc_load_fold_active(struct rq *this_rq, long adjust);

DECLARE_PER_CPU_SHARED_ALIGNED(struct rq, runqueues);
#define cpu_rq(cpu)		(&per_cpu(runqueues, (cpu)))
#define this_rq()		this_cpu_ptr(&runqueues)
#define task_rq(p)		cpu_rq(task_cpu(p))
#define cpu_curr(cpu)		(cpu_rq(cpu)->curr)
#define raw_rq()		raw_cpu_ptr(&runqueues)

#ifdef CONFIG_SMP
#if defined(CONFIG_SCHED_DEBUG) && defined(CONFIG_SYSCTL)
void register_sched_domain_sysctl(void);
void unregister_sched_domain_sysctl(void);
#else
static inline void register_sched_domain_sysctl(void)
{
}
static inline void unregister_sched_domain_sysctl(void)
{
}
#endif

extern bool sched_smp_initialized;

enum {
	ITSELF_LEVEL_SPACE_HOLDER,
#ifdef CONFIG_SCHED_SMT
	SMT_LEVEL_SPACE_HOLDER,
#endif
	COREGROUP_LEVEL_SPACE_HOLDER,
	CORE_LEVEL_SPACE_HOLDER,
	OTHER_LEVEL_SPACE_HOLDER,
	NR_CPU_AFFINITY_LEVELS
};

DECLARE_PER_CPU(cpumask_t [NR_CPU_AFFINITY_LEVELS], sched_cpu_topo_masks);
DECLARE_PER_CPU(cpumask_t *, sched_cpu_llc_mask);

static inline int __best_mask_cpu(int cpu, const cpumask_t *cpumask,
				  const cpumask_t *mask)
{
#if NR_CPUS <= 64
	unsigned long t;

	while ((t = cpumask->bits[0] & mask->bits[0]) == 0UL)
		mask++;

	return __ffs(t);
#else
	while ((cpu = cpumask_any_and(cpumask, mask)) >= nr_cpu_ids)
		mask++;
	return cpu;
#endif
}

static inline int best_mask_cpu(int cpu, const cpumask_t *mask)
{
#if NR_CPUS <= 64
	unsigned long llc_match;
	cpumask_t *chk = per_cpu(sched_cpu_llc_mask, cpu);

	if ((llc_match = mask->bits[0] & chk->bits[0])) {
		unsigned long match;

		chk = per_cpu(sched_cpu_topo_masks, cpu);
		if (mask->bits[0] & chk->bits[0])
			return cpu;

#ifdef CONFIG_SCHED_SMT
		chk++;
		if ((match = mask->bits[0] & chk->bits[0]))
			return __ffs(match);
#endif

		return __ffs(llc_match);
	}

	return __best_mask_cpu(cpu, mask, chk + 1);
#else
	cpumask_t llc_match;
	cpumask_t *chk = per_cpu(sched_cpu_llc_mask, cpu);

	if (cpumask_and(&llc_match, mask, chk)) {
		cpumask_t tmp;

		chk = per_cpu(sched_cpu_topo_masks, cpu);
		if (cpumask_test_cpu(cpu, mask))
			return cpu;

#ifdef CONFIG_SCHED_SMT
		chk++;
		if (cpumask_and(&tmp, mask, chk))
			return cpumask_any(&tmp);
#endif

		return cpumask_any(&llc_match);
	}

	return __best_mask_cpu(cpu, mask, chk + 1);
#endif
}

extern void flush_smp_call_function_from_idle(void);

#else  /* !CONFIG_SMP */
static inline void flush_smp_call_function_from_idle(void) { }
#endif

#ifndef arch_scale_freq_tick
static __always_inline
void arch_scale_freq_tick(void)
{
}
#endif

#ifndef arch_scale_freq_capacity
static __always_inline
unsigned long arch_scale_freq_capacity(int cpu)
{
	return SCHED_CAPACITY_SCALE;
}
#endif

static inline u64 __rq_clock_broken(struct rq *rq)
{
	return READ_ONCE(rq->clock);
}

static inline u64 rq_clock(struct rq *rq)
{
	/*
	 * Relax lockdep_assert_held() checking as in VRQ, call to
	 * sched_info_xxxx() may not held rq->lock
	 * lockdep_assert_held(&rq->lock);
	 */
	return rq->clock;
}

static inline u64 rq_clock_task(struct rq *rq)
{
	/*
	 * Relax lockdep_assert_held() checking as in VRQ, call to
	 * sched_info_xxxx() may not held rq->lock
	 * lockdep_assert_held(&rq->lock);
	 */
	return rq->clock_task;
}

/*
 * {de,en}queue flags:
 *
 * DEQUEUE_SLEEP  - task is no longer runnable
 * ENQUEUE_WAKEUP - task just became runnable
 *
 */

#define DEQUEUE_SLEEP		0x01

#define ENQUEUE_WAKEUP		0x01


/*
 * Below are scheduler API which using in other kernel code
 * It use the dummy rq_flags
 * ToDo : BMQ need to support these APIs for compatibility with mainline
 * scheduler code.
 */
struct rq_flags {
	unsigned long flags;
};

struct rq *__task_rq_lock(struct task_struct *p, struct rq_flags *rf)
	__acquires(rq->lock);

struct rq *task_rq_lock(struct task_struct *p, struct rq_flags *rf)
	__acquires(p->pi_lock)
	__acquires(rq->lock);

static inline void __task_rq_unlock(struct rq *rq, struct rq_flags *rf)
	__releases(rq->lock)
{
	raw_spin_unlock(&rq->lock);
}

static inline void
task_rq_unlock(struct rq *rq, struct task_struct *p, struct rq_flags *rf)
	__releases(rq->lock)
	__releases(p->pi_lock)
{
	raw_spin_unlock(&rq->lock);
	raw_spin_unlock_irqrestore(&p->pi_lock, rf->flags);
}

static inline void
rq_lock(struct rq *rq, struct rq_flags *rf)
	__acquires(rq->lock)
{
	raw_spin_lock(&rq->lock);
}

static inline void
rq_unlock_irq(struct rq *rq, struct rq_flags *rf)
	__releases(rq->lock)
{
	raw_spin_unlock_irq(&rq->lock);
}

static inline void
rq_unlock(struct rq *rq, struct rq_flags *rf)
	__releases(rq->lock)
{
	raw_spin_unlock(&rq->lock);
}

static inline struct rq *
this_rq_lock_irq(struct rq_flags *rf)
	__acquires(rq->lock)
{
	struct rq *rq;

	local_irq_disable();
	rq = this_rq();
	raw_spin_lock(&rq->lock);

	return rq;
}

static inline int task_current(struct rq *rq, struct task_struct *p)
{
	return rq->curr == p;
}

static inline bool task_running(struct task_struct *p)
{
	return p->on_cpu;
}

extern int task_running_nice(struct task_struct *p);

extern struct static_key_false sched_schedstats;

#ifdef CONFIG_CPU_IDLE
static inline void idle_set_state(struct rq *rq,
				  struct cpuidle_state *idle_state)
{
	rq->idle_state = idle_state;
}

static inline struct cpuidle_state *idle_get_state(struct rq *rq)
{
	WARN_ON(!rcu_read_lock_held());
	return rq->idle_state;
}
#else
static inline void idle_set_state(struct rq *rq,
				  struct cpuidle_state *idle_state)
{
}

static inline struct cpuidle_state *idle_get_state(struct rq *rq)
{
	return NULL;
}
#endif

static inline int cpu_of(const struct rq *rq)
{
#ifdef CONFIG_SMP
	return rq->cpu;
#else
	return 0;
#endif
}

#include "stats.h"

#ifdef CONFIG_NO_HZ_COMMON
#define NOHZ_BALANCE_KICK_BIT	0
#define NOHZ_STATS_KICK_BIT	1

#define NOHZ_BALANCE_KICK	BIT(NOHZ_BALANCE_KICK_BIT)
#define NOHZ_STATS_KICK		BIT(NOHZ_STATS_KICK_BIT)

#define NOHZ_KICK_MASK	(NOHZ_BALANCE_KICK | NOHZ_STATS_KICK)

#define nohz_flags(cpu)	(&cpu_rq(cpu)->nohz_flags)

/* TODO: needed?
extern void nohz_balance_exit_idle(struct rq *rq);
#else
static inline void nohz_balance_exit_idle(struct rq *rq) { }
*/
#endif

#ifdef CONFIG_IRQ_TIME_ACCOUNTING
struct irqtime {
	u64			total;
	u64			tick_delta;
	u64			irq_start_time;
	struct u64_stats_sync	sync;
};

DECLARE_PER_CPU(struct irqtime, cpu_irqtime);

/*
 * Returns the irqtime minus the softirq time computed by ksoftirqd.
 * Otherwise ksoftirqd's sum_exec_runtime is substracted its own runtime
 * and never move forward.
 */
static inline u64 irq_time_read(int cpu)
{
	struct irqtime *irqtime = &per_cpu(cpu_irqtime, cpu);
	unsigned int seq;
	u64 total;

	do {
		seq = __u64_stats_fetch_begin(&irqtime->sync);
		total = irqtime->total;
	} while (__u64_stats_fetch_retry(&irqtime->sync, seq));

	return total;
}
#endif /* CONFIG_IRQ_TIME_ACCOUNTING */

#ifdef CONFIG_CPU_FREQ
DECLARE_PER_CPU(struct update_util_data __rcu *, cpufreq_update_util_data);

/**
 * cpufreq_update_util - Take a note about CPU utilization changes.
 * @rq: Runqueue to carry out the update for.
 * @flags: Update reason flags.
 *
 * This function is called by the scheduler on the CPU whose utilization is
 * being updated.
 *
 * It can only be called from RCU-sched read-side critical sections.
 *
 * The way cpufreq is currently arranged requires it to evaluate the CPU
 * performance state (frequency/voltage) on a regular basis to prevent it from
 * being stuck in a completely inadequate performance level for too long.
 * That is not guaranteed to happen if the updates are only triggered from CFS
 * and DL, though, because they may not be coming in if only RT tasks are
 * active all the time (or there are RT tasks only).
 *
 * As a workaround for that issue, this function is called periodically by the
 * RT sched class to trigger extra cpufreq updates to prevent it from stalling,
 * but that really is a band-aid.  Going forward it should be replaced with
 * solutions targeted more specifically at RT tasks.
 */
static inline void cpufreq_update_util(struct rq *rq, unsigned int flags)
{
	struct update_util_data *data;

	data = rcu_dereference_sched(*per_cpu_ptr(&cpufreq_update_util_data,
						  cpu_of(rq)));
	if (data)
		data->func(data, rq_clock(rq), flags);
}
#else
static inline void cpufreq_update_util(struct rq *rq, unsigned int flags) {}
#endif /* CONFIG_CPU_FREQ */

#ifdef CONFIG_NO_HZ_FULL
extern int __init sched_tick_offload_init(void);
#else
static inline int sched_tick_offload_init(void) { return 0; }
#endif

#ifdef arch_scale_freq_capacity
#ifndef arch_scale_freq_invariant
#define arch_scale_freq_invariant()	(true)
#endif
#else /* arch_scale_freq_capacity */
#define arch_scale_freq_invariant()	(false)
#endif

extern void schedule_idle(void);

#define cap_scale(v, s) ((v)*(s) >> SCHED_CAPACITY_SHIFT)

/*
 * !! For sched_setattr_nocheck() (kernel) only !!
 *
 * This is actually gross. :(
 *
 * It is used to make schedutil kworker(s) higher priority than SCHED_DEADLINE
 * tasks, but still be able to sleep. We need this on platforms that cannot
 * atomically change clock frequency. Remove once fast switching will be
 * available on such platforms.
 *
 * SUGOV stands for SchedUtil GOVernor.
 */
#define SCHED_FLAG_SUGOV	0x10000000

#ifdef CONFIG_MEMBARRIER
/*
 * The scheduler provides memory barriers required by membarrier between:
 * - prior user-space memory accesses and store to rq->membarrier_state,
 * - store to rq->membarrier_state and following user-space memory accesses.
 * In the same way it provides those guarantees around store to rq->curr.
 */
static inline void membarrier_switch_mm(struct rq *rq,
					struct mm_struct *prev_mm,
					struct mm_struct *next_mm)
{
	int membarrier_state;

	if (prev_mm == next_mm)
		return;

	membarrier_state = atomic_read(&next_mm->membarrier_state);
	if (READ_ONCE(rq->membarrier_state) == membarrier_state)
		return;

	WRITE_ONCE(rq->membarrier_state, membarrier_state);
}
#else
static inline void membarrier_switch_mm(struct rq *rq,
					struct mm_struct *prev_mm,
					struct mm_struct *next_mm)
{
}
#endif

#ifdef CONFIG_NUMA
extern int sched_numa_find_closest(const struct cpumask *cpus, int cpu);
#else
static inline int sched_numa_find_closest(const struct cpumask *cpus, int cpu)
{
	return nr_cpu_ids;
}
#endif

void swake_up_all_locked(struct swait_queue_head *q);
void __prepare_to_swait(struct swait_queue_head *q, struct swait_queue *wait);

#endif /* ALT_SCHED_H */
