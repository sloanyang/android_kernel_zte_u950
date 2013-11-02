/*
 * drivers/misc/max1749.c
 *
 * Driver for MAX1749, vibrator motor driver.
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/regulator/consumer.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/wakelock.h>
#include <linux/suspend.h>
#include <linux/notifier.h>

#include "../staging/android/timed_output.h"

/*Interface shoulde be realize recording HW design*/
extern int gpio_greenlight_ctrl(int onoff);
extern int tps8003x_redlight_ctrl(int onoff, int dimming);
extern int tps8003x_offred_verify(void);
static void led_start(void);
static void led_stop(void);
static int led_red_notify_sys(struct notifier_block *this, unsigned long code,void *unused);
static int led_red_pm_notify(struct notifier_block *nb,	unsigned long event, void *nouse);
static int red_led_timeout;
static void timed_control_init(void);
static void timed_control_remove(void);
static void led_on_timer_fun(unsigned long);
static void led_off_timer_fun(unsigned long);

static int timed_control_work = 0;

#define LOW_BAT_WARN_ON  500
#define LOW_BAT_WARN_OFF  500
#define red_led_default_dimming 170

static struct timer_list  led_on_timer;
static struct timer_list  led_off_timer;
static struct work_struct led_on_work;
static struct work_struct led_off_work;

struct wake_lock led_charge_wakelock;

static struct notifier_block led_red_notifier = {
	.notifier_call = led_red_notify_sys,
};

static struct notifier_block led_red_notify = {
	.notifier_call = led_red_pm_notify,
};

static int led_red_pm_notify(struct notifier_block *nb,
			unsigned long event, void *nouse)
{

	switch (event) {
		case PM_SUSPEND_PREPARE:
			if(1 == timed_control_work) {
				tps8003x_redlight_ctrl(0, 0);
				del_timer(&led_on_timer);
				del_timer(&led_off_timer);
				printk("===suspend prepare===\n");
			}
			break;

		case PM_POST_SUSPEND:
			if(1 == timed_control_work) {
				tps8003x_redlight_ctrl(1, red_led_default_dimming);
				setup_timer(&led_on_timer, led_on_timer_fun,0);
				setup_timer(&led_off_timer, led_off_timer_fun,0);
				mod_timer(&led_on_timer,jiffies + msecs_to_jiffies(LOW_BAT_WARN_ON));
				printk("===suspend post===\n");
			}
		break;

		default:
			break;
	}

	return NOTIFY_OK;
}

static int led_red_notify_sys(struct notifier_block *this, unsigned long code,
			  void *unused)
{
	if (code == SYS_DOWN || code == SYS_POWER_OFF)
	{
		/* Off led */
		timed_control_remove();
		led_stop();
	}
	return NOTIFY_DONE;
}

static void timed_control_init(void)
{
    if(0 == timed_control_work)
    {
        printk("enter %s ,set_up timer!!\n", __func__);
        setup_timer(&led_on_timer, led_on_timer_fun,0);
        setup_timer(&led_off_timer, led_off_timer_fun,0);
        timed_control_work = 1;
    }
}

static void timed_control_remove(void)
{
    if(1 == timed_control_work)
    {
        printk("enter %s ,del timer!!\n", __func__);
        del_timer(&led_on_timer);
        del_timer(&led_off_timer);
        timed_control_work = 0;
    }
}

static void led_on_timer_fun(unsigned long data)
{
    //printk("enter %s !!\n", __func__);
    mod_timer(&led_off_timer,jiffies + msecs_to_jiffies(LOW_BAT_WARN_OFF));
    schedule_work(&led_off_work);
}

static void led_off_timer_fun(unsigned long data)
{
    //printk("enter %s !!\n", __func__);
    mod_timer(&led_on_timer,jiffies + msecs_to_jiffies(LOW_BAT_WARN_ON));
    schedule_work(&led_on_work);
}

static void led_on_work_fun(struct work_struct *work)
{
    led_start();
}
static void led_off_work_fun(struct work_struct *work)
{
    led_stop();
}

static void led_start(void)
{
	int ret = -1;

	//printk("start RED led \n");

	ret = tps8003x_redlight_ctrl(1, red_led_default_dimming);
	if(ret < 0)
		printk("fail to start RED led !!!\n");
}

static void led_stop(void)
{
	int ret = -1;

	ret = tps8003x_redlight_ctrl(0, red_led_default_dimming);
	if(ret < 0)
	{
		printk("fail to close RED led !!!\n");
		tps8003x_redlight_ctrl(0, red_led_default_dimming);
	}
	else if(tps8003x_offred_verify() < 0)
	{
		printk("Verify fail to close RED led !!!\n");
		tps8003x_redlight_ctrl(0, red_led_default_dimming);
	}
}

/*
 * Timeout value can be changed from sysfs entry
 * created by timed_output_dev.
 * echo 100 > /sys/class/timed_output/led_red_zte/enable
 */
static void led_enable(struct timed_output_dev *dev, int value)
{
    int ret;

    printk("enter into %s,led red on_time is %d\n",__func__,value);
    red_led_timeout = value;

    if (-1 == value)
    {
	if( !wake_lock_active(&led_charge_wakelock) )
		wake_lock(&led_charge_wakelock);

	led_stop();
	timed_control_remove();
	led_start();
    }
    else if(0 == value)
    {
	timed_control_remove();
	led_stop();

	if( wake_lock_active(&led_charge_wakelock) )
		wake_unlock(&led_charge_wakelock);
    }
    else
    {
	if( wake_lock_active(&led_charge_wakelock) )
		wake_unlock(&led_charge_wakelock);

	timed_control_remove();
	led_stop();
	led_start();
	timed_control_init();
	mod_timer(&led_on_timer,jiffies + msecs_to_jiffies(LOW_BAT_WARN_ON));
    }

}

/*
 * Timeout value can be read from sysfs entry
 * created by timed_output_dev.
 * cat /sys/class/timed_output/led_red_zte/enable
 */
static int led_get_time(struct timed_output_dev *dev)
{
	return red_led_timeout;
}

static struct timed_output_dev led_red_dev = {
	.name		= "led_red_zte",
	.get_time	= led_get_time,
	.enable		= led_enable,
};

static int __init led_red_init(void)
{
	int status;

	INIT_WORK(&led_on_work, led_on_work_fun);
	INIT_WORK(&led_off_work, led_off_work_fun);

	status = timed_output_dev_register(&led_red_dev);
	status = register_reboot_notifier(&led_red_notifier);
	status = register_pm_notifier(&led_red_notify);

	wake_lock_init(&led_charge_wakelock, WAKE_LOCK_SUSPEND, "Red_led");
	led_stop();

	return status;
}

static void __exit led_red_exit(void)
{
	cancel_work_sync(&led_on_work);
	cancel_work_sync(&led_off_work);
	timed_output_dev_unregister(&led_red_dev);
	unregister_reboot_notifier(&led_red_notifier);
	unregister_pm_notifier(&led_red_notify);

	wake_lock_destroy(&led_charge_wakelock);
}

MODULE_DESCRIPTION("timed output led red device");
MODULE_AUTHOR("GPL");

module_init(led_red_init);
module_exit(led_red_exit);
