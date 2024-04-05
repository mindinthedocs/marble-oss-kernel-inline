// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <walt.h>
#include "trace.h"

#ifdef CONFIG_HOTPLUG_CPU

/* if a cpu is halting */
struct cpumask __cpu_halt_mask;

static DEFINE_MUTEX(halt_lock);

struct halt_cpu_state {
	u64		last_halt;
	int		ref_count;
};

static DEFINE_PER_CPU(struct halt_cpu_state, halt_state);

/* the amount of time allowed for enqueue operations that happen
 * just after a halt operation.
 */
#define WALT_HALT_CHECK_THRESHOLD_NS 400000

/*
 * Remove a task from the runqueue and pretend that it's migrating. This
 * should prevent migrations for the detached task and disallow further
 * changes to tsk_cpus_allowed.
 */
void
detach_one_task_core(struct task_struct *p, struct rq *rq,
		     struct list_head *tasks)
{
	lockdep_assert_held(&rq->__lock);

	p->on_rq = TASK_ON_RQ_MIGRATING;
	deactivate_task(rq, p, 0);
	list_add(&p->se.group_node, tasks);
}

void attach_tasks_core(struct list_head *tasks, struct rq *rq)
{
	struct task_struct *p;

	lockdep_assert_held(&rq->__lock);

	while (!list_empty(tasks)) {
		p = list_first_entry(tasks, struct task_struct, se.group_node);
		list_del_init(&p->se.group_node);

		BUG_ON(task_rq(p) != rq);
		activate_task(rq, p, 0);
		p->on_rq = TASK_ON_RQ_QUEUED;
	}
}

/*
 * Migrate all tasks from the rq, sleeping tasks will be migrated by
 * try_to_wake_up()->select_task_rq().
 *
 * Called with rq->__lock held even though we'er in stop_machine() and
 * there's no concurrency possible, we hold the required locks anyway
 * because of lock validation efforts.
 *
 * The function will skip CPU pinned kthreads.
 */
static void migrate_tasks(struct rq *dead_rq, struct rq_flags *rf)
{
	struct rq *rq = dead_rq;
	struct task_struct *next, *stop = rq->stop;
	LIST_HEAD(percpu_kthreads);
	unsigned int num_pinned_kthreads = 1;
	struct rq_flags orf = *rf;
	int dest_cpu;

	/*
	 * Fudge the rq selection such that the below task selection loop
	 * doesn't get stuck on the currently eligible stop task.
	 *
	 * We're currently inside stop_machine() and the rq is either stuck
	 * in the stop_machine_cpu_stop() loop, or we're executing this code,
	 * either way we should never end up calling schedule() until we're
	 * done here.
	 */
	rq->stop = NULL;

	/*
	 * put_prev_task() and pick_next_task() sched
	 * class method both need to have an up-to-date
	 * value of rq->clock[_task]
	 */
	update_rq_clock(rq);

#ifdef CONFIG_SCHED_DEBUG
	/* note the clock update in orf */
	orf.clock_update_flags |= RQCF_UPDATED;
#endif

	for (;;) {
		/*
		 * There's this thread running, bail when that's the only
		 * remaining thread:
		 */
		if (rq->nr_running == 1)
			break;

		next = pick_migrate_task(rq);

		/*
		 * Argh ... no iterator for tasks, we need to remove the
		 * kthread from the run-queue to continue.
		 */

		if (is_per_cpu_kthread(next)) {
			detach_one_task_core(next, rq, &percpu_kthreads);
			num_pinned_kthreads += 1;
			continue;
		}

		/*
		 * Rules for changing task_struct::cpus_mask are holding
		 * both pi_lock and rq->__lock, such that holding either
		 * stabilizes the mask.
		 *
		 * Drop rq->__lock is not quite as disastrous as it usually is
		 * because !cpu_active at this point, which means load-balance
		 * will not interfere. Also, stop-machine.
		 */
		rq_unlock(rq, rf);
		raw_spin_lock(&next->pi_lock);
		rq_relock(rq, rf);

		/*
		 * Since we're inside stop-machine, _nothing_ should have
		 * changed the task, WARN if weird stuff happened, because in
		 * that case the above rq->__lock drop is a fail too.
		 */
		if (task_rq(next) != rq || !task_on_rq_queued(next)) {
			raw_spin_unlock(&next->pi_lock);
			continue;
		}

		/* Find suitable destination for @next */
		dest_cpu = select_fallback_rq(dead_rq->cpu, next);
		rq = __migrate_task(rq, rf, next, dest_cpu);
		if (rq != dead_rq) {
			rq_unlock(rq, rf);
			rq = dead_rq;
			*rf = orf;
			rq_relock(rq, rf);
		}
		raw_spin_unlock(&next->pi_lock);
	}

	if (num_pinned_kthreads > 1)
		attach_tasks_core(&percpu_kthreads, rq);

	rq->stop = stop;
}

static int drain_rq_cpu_stop(void *data)
{
	struct rq *rq = this_rq();
	struct rq_flags rf;

	rq_lock_irqsave(rq, &rf);
	migrate_tasks(rq, &rf);
	rq_unlock_irqrestore(rq, &rf);

	return 0;
}

static int cpu_drain_rq(unsigned int cpu)
{
	if (available_idle_cpu(cpu))
		return 0;

	return stop_one_cpu(cpu, drain_rq_cpu_stop, NULL);
}

/*
 * returns true if last halt is within threshold
 * note: do not take halt_lock, called from atomic context
 */
bool walt_halt_check_last(int cpu)
{
	u64 last_halt = per_cpu_ptr(&halt_state, cpu)->last_halt;

	/* last_halt is valid, check it against sched_clock */
	if (last_halt != 0 && sched_clock() - last_halt >  WALT_HALT_CHECK_THRESHOLD_NS)
		return false;

	return true;
}

/*
 * 1) add the cpus to the halt mask
 * 2) migrate tasks off the cpu
 *
 */
static int halt_cpus(struct cpumask *cpus)
{
	int cpu;
	int ret = 0;
	u64 start_time = sched_clock();
	struct halt_cpu_state *halt_cpu_state;

	trace_halt_cpus_start(cpus, 1);

	for_each_cpu(cpu, cpus) {

		halt_cpu_state = per_cpu_ptr(&halt_state, cpu);

		/* set the cpu as halted */
		cpumask_set_cpu(cpu, cpu_halt_mask);

		/* guarantee mask written before updating last_halt */
		wmb();

		halt_cpu_state->last_halt = start_time;

		/* only drain online cpus */
		if (cpu_online(cpu)) {
			/* drain the online CPU */
			ret = cpu_drain_rq(cpu);
		}

		if (ret < 0) {
			/* cpu failed to drain, do not mark as halted */
			cpumask_clear_cpu(cpu, cpu_halt_mask);
			break;
		}
	}

	trace_halt_cpus(cpus, start_time, 1, ret);

	return ret;
}

/*
 * 1) remove the cpus from the halt mask
 *
 */
static int start_cpus(struct cpumask *cpus)
{
	u64 start_time = sched_clock();
	struct halt_cpu_state *halt_cpu_state;
	int cpu;

	trace_halt_cpus_start(cpus, 0);

	for_each_cpu(cpu, cpus) {
		halt_cpu_state = per_cpu_ptr(&halt_state, cpu);
		halt_cpu_state->last_halt = 0;

		/* guarantee zero'd last_halt before clearing from the mask */
		wmb();

		cpumask_clear_cpu(cpu, cpu_halt_mask);
	}

	trace_halt_cpus(cpus, start_time, 0, 0);

	return 0;
}

/* increment/decrement ref count for cpus in yield/halt mask */
static void update_ref_counts(struct cpumask *cpus, bool halt)
{
	int cpu;
	struct halt_cpu_state *halt_cpu_state;

	for_each_cpu(cpu, cpus) {
		halt_cpu_state = per_cpu_ptr(&halt_state, cpu);
		if (halt) {
			halt_cpu_state->ref_count++;
		} else {
			WARN_ON_ONCE(halt_cpu_state->ref_count == 0);
			halt_cpu_state->ref_count--;
		}
	}
}

/* remove cpus that are already halted */
static void update_halt_cpus(struct cpumask *cpus)
{
	int cpu;
	struct halt_cpu_state *halt_cpu_state;

	for_each_cpu(cpu, cpus) {
		halt_cpu_state = per_cpu_ptr(&halt_state, cpu);
		if (halt_cpu_state->ref_count)
			cpumask_clear_cpu(cpu, cpus);
	}
}

/* cpus will be modified */
int walt_halt_cpus(struct cpumask *cpus)
{
	int ret = 0;
	cpumask_t requested_cpus;

	mutex_lock(&halt_lock);

	cpumask_copy(&requested_cpus, cpus);

	/* remove cpus that are already halted */
	update_halt_cpus(cpus);

	if (cpumask_empty(cpus)) {
		update_ref_counts(&requested_cpus, true);
		goto unlock;
	}

	ret = halt_cpus(cpus);

	if (ret < 0)
		pr_debug("halt_cpus failure ret=%d cpus=%*pbl\n", ret,
			 cpumask_pr_args(&requested_cpus));
	else
		update_ref_counts(&requested_cpus, true);
unlock:
	mutex_unlock(&halt_lock);

	return ret;
}
EXPORT_SYMBOL(walt_halt_cpus);

int walt_pause_cpus(struct cpumask *cpus)
{
	return walt_halt_cpus(cpus);
}
EXPORT_SYMBOL(walt_pause_cpus);

/* cpus will be modified */
int walt_start_cpus(struct cpumask *cpus)
{
	int ret = 0;
	cpumask_t requested_cpus;

	mutex_lock(&halt_lock);
	cpumask_copy(&requested_cpus, cpus);
	update_ref_counts(&requested_cpus, false);

	/* remove cpus that should still be halted, due to ref-counts */
	update_halt_cpus(cpus);

	ret = start_cpus(cpus);

	if (ret < 0) {
		pr_debug("halt_cpus failure ret=%d cpus=%*pbl\n", ret,
			 cpumask_pr_args(&requested_cpus));
		/* restore/increment ref counts in case of error */
		update_ref_counts(&requested_cpus, true);
	}

	mutex_unlock(&halt_lock);

	return ret;
}
EXPORT_SYMBOL(walt_start_cpus);

int walt_resume_cpus(struct cpumask *cpus)
{
	return walt_start_cpus(cpus);
}
EXPORT_SYMBOL(walt_resume_cpus);

void walt_halt_init(void)
{
}

#endif /* CONFIG_HOTPLUG_CPU */
