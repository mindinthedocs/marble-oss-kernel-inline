#include <linux/swap.h>
#include <linux/module.h>
#include <trace/hooks/binder.h>
#include <uapi/linux/android/binder.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched/prio.h>
#include <../../android/binder_internal.h>
#include <../../../kernel/sched/sched.h>
#include <linux/string.h>

static const char * const task_name[] = {
        "com.miui.home",
        "ndroid.systemui",
        "surfaceflinger",
        "cameraserver",
        "rsonalassistant",
        "droid.launcher3",
};
static const char *RenderThread = "RenderThread";
static const char *passBlur = "passBlur";
static const char *cameraserver_C3Dev = "C3Dev-";
static const char *cameraserver_ReqQ = "-ReqQ";

// 内核优先级与java层优先级转换
static int to_userspace_prio(int policy, int kernel_priority) {
        if (fair_policy(policy))
                return PRIO_TO_NICE(kernel_priority);
        else
                return MAX_RT_PRIO - 1 - kernel_priority;
}

//判断发起端task的状态
// 1. 是否为上述task中进程的主线程
// 2. 此task当前是否为RT
// 3. 此task发起的binder是否为非oneway
static bool set_binder_rt_task(struct binder_transaction *t) {
        int i;
 	if (!t || !t->from || !t->from->task || !t->to_proc || !t->to_proc->tsk) {
		return false;
	}

	if (t->flags & TF_ONE_WAY) {
		return false;
	}

	if (!rt_policy(t->from->task->policy)) {
		return false;
	}

	if ((strncmp(t->from->task->group_leader->comm, task_name[0], strlen(task_name[0])) == 0)
		&& (strncmp(t->from->task->comm, RenderThread, strlen(RenderThread)) == 0)
		&& (strncmp(t->to_proc->tsk->comm, task_name[2], strlen(task_name[2])) == 0)) {
		return true;
	}

	if ((strncmp(t->from->task->group_leader->comm, task_name[2], strlen(task_name[2])) == 0)
		&& (strncmp(t->from->task->comm, passBlur, strlen(passBlur)) == 0)) {
		return true;
	}

        if ((strncmp(t->from->task->group_leader->comm, task_name[3], strlen(task_name[3])) == 0)
                && (strncmp(t->from->task->comm, cameraserver_C3Dev, strlen(cameraserver_C3Dev)) == 0)
                && (strstr(t->from->task->comm, cameraserver_ReqQ) != NULL)) {
                return true;
        }

        if (t->from->task->pid == t->from->task->tgid) {
                for(i = 0; i < sizeof(task_name)/sizeof(task_name[0]); i++) {
                        if (strncmp(t->from->task->comm, task_name[i], strlen(task_name[i])) == 0) {
                                return true;
                        }
                }
                return false;
        }
        return false;
}

static void extend_surfacefinger_binder_set_priority_handler(void *data, struct binder_transaction *t, struct task_struct *task) {
        struct sched_param params;
        struct binder_priority desired;
        unsigned int policy;
        struct binder_node *target_node = t->buffer->target_node;

        desired.prio = target_node->min_priority;
        desired.sched_policy = target_node->sched_policy;
        policy = desired.sched_policy;
        // 判断当前task是否满足状态
        if (set_binder_rt_task(t)) {
                desired.sched_policy = SCHED_FIFO;
                desired.prio = 98;
                policy = desired.sched_policy;
        }
        // 如果通过上面的条件，则立即设置优先级
        if (rt_policy(policy) && task->policy != policy) {
                params.sched_priority = to_userspace_prio(policy, desired.prio);
                sched_setscheduler_nocheck(task, policy | SCHED_RESET_ON_FORK, &params);
        }
}

static void extend_surfacefinger_binder_trans_handler(void *data, struct binder_proc *target_proc,
    struct binder_proc *proc,struct binder_thread *thread, struct binder_transaction_data *tr) {
        // 判断当前binder的状态
        // 1. 对端是否为surfaceflinger
        // 2. 是否为非oneway
        if (target_proc && target_proc->tsk && strncmp(target_proc->tsk->comm, "surfaceflinger",
                strlen("surfaceflinger")) == 0) {
                if (thread && proc && tr && thread->transaction_stack
                        && (!(thread->transaction_stack->flags & TF_ONE_WAY))) {
                        target_proc->default_priority.sched_policy = SCHED_FIFO;
                        target_proc->default_priority.prio = 98;
                }
        }
}

int __init binder_prio_init(void)
{
    pr_info("binder_prio: module init!");
    // extend_surfacefinger_binder_set_priority_handler 函数对应
    // trace_android_vh_binder_set_priority hook 点
    register_trace_android_vh_binder_set_priority(extend_surfacefinger_binder_set_priority_handler, NULL);
    // extend_surfacefinger_binder_trans_handler 函数对应
    // trace_android_vh_binder_trans hook 点
    register_trace_android_vh_binder_trans(extend_surfacefinger_binder_trans_handler, NULL);

    return 0;
}

void __exit binder_prio_exit(void)
{
    unregister_trace_android_vh_binder_set_priority(extend_surfacefinger_binder_set_priority_handler, NULL);
    unregister_trace_android_vh_binder_trans(extend_surfacefinger_binder_trans_handler, NULL);

    pr_info("binder_prio: module exit!");
}

module_init(binder_prio_init);
module_exit(binder_prio_exit);
MODULE_LICENSE("GPL");