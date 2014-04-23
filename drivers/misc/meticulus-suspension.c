/* linux/driver/misc/meticulus-suspension.c
 *
 * Copyright (c) 2014 Jonathan Jason Dennis [Meticulus]
 *		theonejohnnyd@gmail.com
 *
 * Management of suspend/resume proccess
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/suspend.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/earlysuspend.h>
#include <linux/module.h>
#include <linux/wakelock.h>

/* print debug messages */
static bool debug = true;

/* flag to insure that somethings only happen */
/* once in the suspend resume process */
static bool sleep_freq_is_set = false;

/* storage for policy values during suspend */
static int max,min,usermax,usermin = 0;

/* frequencies to use for suspend */
static int sleep_freq_min = (400 * 1000);
static int sleep_freq_max = (1000 * 1000);

/* time to let new sleep freqs to settle */
static long defer_sleep_timeout = 20000;

static struct wake_lock defer_suspend;


static void meticulus_init_wakelock(void)
{	
	wake_lock_init(&defer_suspend,WAKE_LOCK_SUSPEND,"defer_suspend");
}

static void meticulus_lock_wakelock(void)
{
	wake_lock_timeout(&defer_suspend, defer_sleep_timeout);
}
static void meticulus_release_wakelock(void)
{
	wake_unlock(&defer_suspend);
	wake_lock_destroy(&defer_suspend);
}

static void meticulus_set_sleep_freq(void)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	policy->max = sleep_freq_max;
	policy->min = sleep_freq_min;
	policy->user_policy.min = policy->min;
	policy->user_policy.max = policy->max;
	cpufreq_update_policy(0);
	sleep_freq_is_set = true;
}

static void meticulus_store_freqs(void)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);

	max = policy->max;
	min = policy->min;
	usermax = policy->user_policy.max;
	usermin = policy->user_policy.min;
	if(debug)
		printk("[MET_SUS]: Storing policy values min=%d max=%d usermin=%d usermax=%d\n",
			min,max,usermin,usermax);
}

static void meticulus_restore_freqs(void)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);

	policy->max = max;
	policy->user_policy.max = usermax;
	policy->min = min;
	policy->user_policy.min = usermin;
	cpufreq_update_policy(0);
	if(debug)
		printk("[MET_SUS]: Restoring policy values min=%d max=%d usermin=%d usermax=%d\n",
			min,max,usermin,usermax);

	sleep_freq_is_set = false;
}
static void meticulus_early_suspend_enter(struct early_suspend *h)
{
	meticulus_store_freqs();
	meticulus_init_wakelock();
}
static void meticulus_suspend_enter(void)
{
	/* This function can get fired several times 
	before suspend completes. */
	if(!sleep_freq_is_set || cpufreq_quick_get(0) < sleep_freq_min)
	{
		meticulus_set_sleep_freq();
		/* Activating a wake lock at this point 
		causes suspend to abort. We are doing 
		this to give some time for sleep freqs
		to settle. Suspend will try again when
		wakelock expires */
		meticulus_lock_wakelock();
	}
}
static void meticulus_resume_complete(void)
{
	/* This function can get fired several times 
	before resume completes. */
}
static void meticulus_late_resume_complete(struct early_suspend *h)
{
	meticulus_restore_freqs();
	meticulus_release_wakelock();
}

static int meticulus_suspend_notifier_event(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	switch (event)
	{
		case PM_SUSPEND_PREPARE:
			meticulus_suspend_enter();
			return NOTIFY_OK;
		case PM_POST_SUSPEND:
			meticulus_resume_complete();
			return NOTIFY_OK;			
	}
	return NOTIFY_DONE;
}

static struct early_suspend early_suspend_handler = {
	.level = 0,
	.suspend = meticulus_early_suspend_enter,
	.resume	= meticulus_late_resume_complete,
};

static struct notifier_block meticulus_suspend_notifier = {
	.notifier_call = meticulus_suspend_notifier_event,
};

int __init meticulus_suspend_init(void)
{
	struct early_suspend *handler = &early_suspend_handler;

	/* register suspend notifier */
	register_pm_notifier(&meticulus_suspend_notifier);

	/* register earlysuspend handler */
	register_early_suspend(handler);

	return 0;
}

module_param_named(debug, debug, bool, S_IRUGO | S_IWUSR);

module_init(meticulus_suspend_init);

MODULE_LICENSE("GPL");
