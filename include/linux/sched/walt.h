/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _LINUX_SCHED_WALT_H
#define _LINUX_SCHED_WALT_H

#include <linux/types.h>

#if IS_ENABLED(CONFIG_SCHED_WALT)
extern int sched_lpm_disallowed_time(int cpu, u64 *timeout);
extern int set_task_boost(int boost, u64 period);
#else
static inline int sched_lpm_disallowed_time(int cpu, u64 *timeout)
{
	return INT_MAX;
}
static inline int set_task_boost(int boost, u64 period)
{
	return 0;
}
#endif

#endif /* _LINUX_SCHED_WALT_H */
