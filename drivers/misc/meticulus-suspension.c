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

#define SLEEP_FREQ	(800 * 1000) /* Use 800MHz when entering sleep */

static bool debug = true;

static bool enable_screenoff_freqs = false;

static bool sleep_freq_is_set = false;

/* Placeholder for policy values during suspend */
static int max,min,usermax,usermin = 0;

static int screenoff_freq_min, screenoff_freq_max = SLEEP_FREQ;

enum cpufreq_access {
	DISABLE_FURTHER_CPUFREQ = 0x10,
	ENABLE_FURTHER_CPUFREQ = 0x20,
};

static void meticulus_wait_for_target_freq(void)
{
	while(cpufreq_quick_get(0) < SLEEP_FREQ) {

		if(debug)
			printk("[MET_SUS]: CPU FREQ = %d\n",cpufreq_quick_get(0));
		
		mdelay(10);
	}
	if(debug)
		printk("[MET_SUS]: CPU FREQ = %d\n",cpufreq_quick_get(0));
}

static void meticulus_set_screenoff_freq(void)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	policy->max = screenoff_freq_max;
	policy->min = screenoff_freq_min;
	policy->user_policy.min = policy->min;
	policy->user_policy.max = policy->max;
	cpufreq_update_policy(0);
}

static void meticulus_set_sleep_freq(void)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	int ret = -1;
	int retries = 0;

	policy->max = SLEEP_FREQ;
	policy->min = SLEEP_FREQ;
	policy->user_policy.min = policy->min;
	policy->user_policy.max = policy->max;

	while(retries < 10) {
		ret = cpufreq_driver_target(policy, SLEEP_FREQ,
				DISABLE_FURTHER_CPUFREQ);
		if(debug)		
			printk("[MET_SUS]: Trying to set policy - tries=%d\n", retries +1);

		if (ret >= 0) 
			break;

		mdelay(10);
		retries += 1;
	}
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

	cpufreq_driver_target(policy, SLEEP_FREQ,
			ENABLE_FURTHER_CPUFREQ);
	policy->max = max;
	policy->user_policy.max = usermax;
	policy->min = min;
	policy->user_policy.min = usermin;
	if(debug)
		printk("[MET_SUS]: Restoring policy values min=%d max=%d usermin=%d usermax=%d\n",
			min,max,usermin,usermax);

	sleep_freq_is_set = false;
}
static void meticulus_early_suspend_enter(struct early_suspend *h)
{
	meticulus_store_freqs();
	if(enable_screenoff_freqs)
		meticulus_set_screenoff_freq();
}
static void meticulus_suspend_enter(void)
{
	/* This function can get fired several times 
	before suspend completes. */
	if(!sleep_freq_is_set)
		meticulus_set_sleep_freq();
}
static void meticulus_resume_complete(void)
{
	/* This function can get fired several times 
	before resume completes. */
}
static void meticulus_late_resume_complete(struct early_suspend *h)
{
	meticulus_restore_freqs();
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
module_param_named(enable_screenoff_freqs, enable_screenoff_freqs, bool, S_IRUGO | S_IWUSR);
module_param_named(screenoff_freq_min, screenoff_freq_min, int, S_IRUGO | S_IWUSR);
module_param_named(screenoff_freq_max, screenoff_freq_max, int, S_IRUGO | S_IWUSR);

module_init(meticulus_suspend_init);

MODULE_LICENSE("GPL");
