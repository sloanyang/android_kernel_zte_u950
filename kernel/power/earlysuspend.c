/* kernel/power/earlysuspend.c
 *
 * Copyright (C) 2005-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/earlysuspend.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rtc.h>
#include <linux/syscalls.h> /* sys_sync */
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos_params.h>

#include "power.h"

enum {
	DEBUG_USER_STATE = 1U << 0,
	DEBUG_SUSPEND = 1U << 2,
	DEBUG_VERBOSE = 1U << 3,
	DEBUG_CALLTIME = 1U << 4,
};
static int debug_mask = DEBUG_USER_STATE;
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

static DEFINE_MUTEX(early_suspend_lock);
static LIST_HEAD(early_suspend_handlers);
static void sync_system(struct work_struct *work);
static void early_suspend(struct work_struct *work);
static void late_resume(struct work_struct *work);
static DECLARE_WORK(sync_system_work, sync_system);
static DECLARE_WORK(early_suspend_work, early_suspend);
static DECLARE_WORK(late_resume_work, late_resume);
static DEFINE_SPINLOCK(state_lock);

enum {
	SUSPEND_REQUESTED = 0x1,
	SUSPENDED = 0x2,
	SUSPEND_REQUESTED_AND_SUSPENDED = SUSPEND_REQUESTED | SUSPENDED,
};

static int state;

#define BOOST_CPU_FREQ_MIN	1500000
static struct pm_qos_request_list boost_cpu_freq_req;

static ktime_t earlysuspend_debug_start(void)
{
	ktime_t calltime = ktime_set(0, 0);

	if (debug_mask & DEBUG_CALLTIME) {
		calltime = ktime_get();
	}

	return calltime;
}

static void earlysuspend_debug_report(void *fn, ktime_t calltime)
{
	ktime_t delta, rettime;

	if (debug_mask & DEBUG_CALLTIME) {
		rettime = ktime_get();
		delta = ktime_sub(rettime, calltime);
		pr_info("ES: function %ps returned after %Ld usecs\n", fn,
			(unsigned long long)ktime_to_ns(delta) >> 10);
	}
}

static void earlysuspend_show_time(const char *label, ktime_t calltime)
{
	ktime_t delta, rettime;

	if (debug_mask & DEBUG_CALLTIME) {
		rettime = ktime_get();
		delta = ktime_sub(rettime, calltime);
		pr_info("ES: %s complete after %Ld usecs\n", label,
			(unsigned long long)ktime_to_ns(delta) >> 10);
	}
}

void register_early_suspend(struct early_suspend *handler)
{
	struct list_head *pos;

	mutex_lock(&early_suspend_lock);
	list_for_each(pos, &early_suspend_handlers) {
		struct early_suspend *e;
		e = list_entry(pos, struct early_suspend, link);
		if (e->level > handler->level)
			break;
	}
	list_add_tail(&handler->link, pos);
	if ((state & SUSPENDED) && handler->suspend)
		handler->suspend(handler);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(register_early_suspend);

void unregister_early_suspend(struct early_suspend *handler)
{
	mutex_lock(&early_suspend_lock);
	list_del(&handler->link);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(unregister_early_suspend);

static void sync_system(struct work_struct *work){
	sys_sync();
	wake_unlock(&sync_wake_lock);

	if (debug_mask & DEBUG_SUSPEND)
		pr_info("early_suspend: sync done\n");
}

static void early_suspend(struct work_struct *work)
{
	struct early_suspend *pos;
	unsigned long irqflags;
	int abort = 0;
	ktime_t calltime;
	ktime_t starttime = ktime_set(0, 0);

	mutex_lock(&early_suspend_lock);
	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPEND_REQUESTED)
		state |= SUSPENDED;
	else
		abort = 1;
	spin_unlock_irqrestore(&state_lock, irqflags);

	if (abort) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("early_suspend: abort, state %d\n", state);
		mutex_unlock(&early_suspend_lock);
		goto abort;
	}

	if (debug_mask & DEBUG_SUSPEND)
		pr_info("early_suspend: call handlers\n");
	if (debug_mask & DEBUG_CALLTIME)
		starttime = ktime_get();
	list_for_each_entry(pos, &early_suspend_handlers, link) {
		if (pos->suspend != NULL) {
			if (debug_mask & DEBUG_VERBOSE)
				pr_info("early_suspend: calling %pf\n", pos->suspend);

			calltime = earlysuspend_debug_start();
			pos->suspend(pos);
			earlysuspend_debug_report(pos->suspend, calltime);
		}
	}
	if (debug_mask & DEBUG_CALLTIME)
		earlysuspend_show_time("call early_suspend handlers", starttime);
	mutex_unlock(&early_suspend_lock);

	if (debug_mask & DEBUG_SUSPEND)
		pr_info("early_suspend: sync\n");

	wake_lock(&sync_wake_lock);
	queue_work(sync_work_queue, &sync_system_work);
abort:
	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPEND_REQUESTED_AND_SUSPENDED)
		wake_unlock(&main_wake_lock);
	spin_unlock_irqrestore(&state_lock, irqflags);
}

static void late_resume(struct work_struct *work)
{
	struct early_suspend *pos;
	unsigned long irqflags;
	int abort = 0;
	ktime_t calltime;
	ktime_t starttime = ktime_set(0, 0);

	if (debug_mask & DEBUG_CALLTIME)
		pr_info("ES: cpu info(CPU %d Freq %d MHz)\n",
			raw_smp_processor_id(), cpufreq_get(raw_smp_processor_id())/1000);

	pm_qos_update_request(&boost_cpu_freq_req, (s32)BOOST_CPU_FREQ_MIN);

	mutex_lock(&early_suspend_lock);
	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPENDED)
		state &= ~SUSPENDED;
	else
		abort = 1;
	spin_unlock_irqrestore(&state_lock, irqflags);

	if (abort) {
		if (debug_mask & DEBUG_SUSPEND)
			pr_info("late_resume: abort, state %d\n", state);
		goto abort;
	}

	if (debug_mask & DEBUG_SUSPEND)
		pr_info("late_resume: call handlers\n");

	if (debug_mask & DEBUG_CALLTIME)
		starttime = ktime_get();
	list_for_each_entry_reverse(pos, &early_suspend_handlers, link) {
		if (pos->resume != NULL) {
			if (debug_mask & DEBUG_VERBOSE)
				pr_info("late_resume: calling %pf\n", pos->resume);

			calltime = earlysuspend_debug_start();
			pos->resume(pos);
			earlysuspend_debug_report(pos->resume, calltime);
		}
	}

	if (debug_mask & DEBUG_CALLTIME)
		earlysuspend_show_time("call late_resume handlers", starttime);

	if (debug_mask & DEBUG_SUSPEND)
		pr_info("late_resume: done\n");
abort:
	mutex_unlock(&early_suspend_lock);
	pm_qos_update_request(&boost_cpu_freq_req, PM_QOS_DEFAULT_VALUE);
}

void request_suspend_state(suspend_state_t new_state)
{
	unsigned long irqflags;
	int old_sleep;

	spin_lock_irqsave(&state_lock, irqflags);
	old_sleep = state & SUSPEND_REQUESTED;
	if (debug_mask & DEBUG_USER_STATE) {
		struct timespec ts;
		struct rtc_time tm;
		getnstimeofday(&ts);
		rtc_time_to_tm(ts.tv_sec, &tm);
		pr_info("request_suspend_state: %s (%d->%d) at %lld "
			"(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n",
			new_state != PM_SUSPEND_ON ? "sleep" : "wakeup",
			requested_suspend_state, new_state,
			ktime_to_ns(ktime_get()),
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
	}

	if (!old_sleep && new_state != PM_SUSPEND_ON) {
		state |= SUSPEND_REQUESTED;
		queue_work(suspend_work_queue, &early_suspend_work);
	} else if (old_sleep && new_state == PM_SUSPEND_ON) {
		state &= ~SUSPEND_REQUESTED;
		wake_lock(&main_wake_lock);
		queue_work(suspend_work_queue, &late_resume_work);
	}
	requested_suspend_state = new_state;
	spin_unlock_irqrestore(&state_lock, irqflags);
}

suspend_state_t get_suspend_state(void)
{
	return requested_suspend_state;
}

static int __init earlysuspend_pm_qos_init(void)
{
	pm_qos_add_request(&boost_cpu_freq_req, PM_QOS_CPU_FREQ_MIN,
			   PM_QOS_DEFAULT_VALUE);
}

late_initcall(earlysuspend_pm_qos_init);
