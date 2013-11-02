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
#include <linux/gpio.h>
#include <linux/suspend.h>
#include <linux/notifier.h>

#include "../../arch/arm/mach-tegra/gpio-names.h"
#include "../staging/android/timed_output.h"

extern int tps8003x_redlight_ctrl(int onoff, int dimming);
int gpio_greenlight_ctrl(int onoff);
static int led_get_time(struct timed_output_dev *dev);
static int led_green_notify_sys(struct notifier_block *this, unsigned long code,void *unused);
static int led_green_pm_notify(struct notifier_block *nb,unsigned long event, void *nouse);
static void led_start(void);
static void led_stop(void);
static void orange_led_start(void);
static void timed_control_init(void);
static void timed_control_remove(void);
static void led_on_timer_fun(unsigned long);
static void led_off_timer_fun(unsigned long);

static int timed_control_work = 0;
static int red_led_timeout = 0;

#ifdef CONFIG_PROJECT_U950
#define led_green_gpio TEGRA_GPIO_PA0
#else
#define led_green_gpio TEGRA_GPIO_PM5
#endif

#define MSG_WARN_ON  500
#define MSG_WARN_OFF  500
#define CALL_WARN_ON  500
#define CALL_WARN_OFF  2500

static struct timer_list  led_on_timer;
static struct timer_list  led_off_timer;
static struct work_struct led_on_work;
static struct work_struct led_off_work;

struct wake_lock led_indicator_wakelock;
static struct notifier_block led_green_notifier = {
	.notifier_call = led_green_notify_sys,
};

static struct notifier_block led_green_notify = {
	.notifier_call = led_green_pm_notify,
};

static int led_green_pm_notify(struct notifier_block *nb,
			unsigned long event, void *nouse)
{

	switch (event) {
		case PM_SUSPEND_PREPARE:
			if(1 == timed_control_work) {
				gpio_greenlight_ctrl(0);
				del_timer(&led_on_timer);
				del_timer(&led_off_timer);
				printk("===suspend prepare===\n");
			}
			break;

		case PM_POST_SUSPEND:
			if(1 == timed_control_work) {
				gpio_greenlight_ctrl(1);
			        setup_timer(&led_on_timer, led_on_timer_fun,0);
			        setup_timer(&led_off_timer, led_off_timer_fun,0);
			        mod_timer(&led_on_timer,jiffies + msecs_to_jiffies(MSG_WARN_ON));
				printk("===suspend post===\n");
			}
		break;

		default:
			break;
	}

	return NOTIFY_OK;
}

int gpio_greenlight_ctrl(int onoff)
{
	if(onoff)
		gpio_set_value(led_green_gpio, 1);
	else
		gpio_set_value(led_green_gpio, 0);

	return 0;
}


static int led_green_notify_sys(struct notifier_block *this, unsigned long code,
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

    mod_timer(&led_off_timer,jiffies + msecs_to_jiffies(MSG_WARN_OFF));
    schedule_work(&led_off_work);

}

static void led_off_timer_fun(unsigned long data)
{
    //printk("enter %s !!\n", __func__);

    mod_timer(&led_on_timer,jiffies + msecs_to_jiffies(MSG_WARN_ON));
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

	//printk("start GREEN led \n");

	ret = gpio_greenlight_ctrl(1);
	if(ret < 0)
		printk("fail to start GREEN led !!!\n");
}

static void led_stop(void)
{
    int ret = -1;

    ret = gpio_greenlight_ctrl(0);
    if(ret < 0)
        printk("fail to close GREEN led !!!\n");
}

/*
 * Timeout value can be changed from sysfs entry
 * created by timed_output_dev.
 * echo 100 > /sys/class/timed_output/led_green_zte/enable
 */
static void led_enable(struct timed_output_dev *dev, int value)
{
    printk("enter into %s,led green on_time is %d\n",__func__,value);
    red_led_timeout = value;

    if (-1 == value)
    {
	if( !wake_lock_active(&led_indicator_wakelock) )
		wake_lock(&led_indicator_wakelock);

	led_stop();
	timed_control_remove();
	led_start();
    }
    else if(0 == value)
    {
	timed_control_remove();
	led_stop();

	if( wake_lock_active(&led_indicator_wakelock) )
		wake_unlock(&led_indicator_wakelock);
    }
    else
    {
	if( wake_lock_active(&led_indicator_wakelock) )
		wake_unlock(&led_indicator_wakelock);

        timed_control_remove();
        led_stop();
        led_start();
        timed_control_init();
        mod_timer(&led_on_timer,jiffies + msecs_to_jiffies(MSG_WARN_ON));
	}
}

/*
 * Timeout value can be read from sysfs entry
 * created by timed_output_dev.
 * cat /sys/class/timed_output/led_green_zte/enable
 */
static int led_get_time(struct timed_output_dev *dev)
{
	return red_led_timeout;
}

static struct timed_output_dev led_green_dev = {
	.name		= "led_green_zte",
	.get_time	      = led_get_time,
	.enable		= led_enable,
};

static int __init led_green_init(void)
{
	int status;

	INIT_WORK(&led_on_work, led_on_work_fun);
	INIT_WORK(&led_off_work, led_off_work_fun);

	//gpio_free(led_green_gpio);
	status = gpio_request(led_green_gpio, "green_led");
	tegra_gpio_enable(led_green_gpio);
	gpio_direction_output(led_green_gpio, 0);
	tegra_gpio_set_tristate(led_green_gpio,TEGRA_TRI_NORMAL);

	status = timed_output_dev_register(&led_green_dev);
	status = register_reboot_notifier(&led_green_notifier);
	status = register_pm_notifier(&led_green_notify);

	wake_lock_init(&led_indicator_wakelock, WAKE_LOCK_SUSPEND, "Green_led");
	led_stop();

	return status;
}

static void __exit led_green_exit(void)
{
	cancel_work_sync(&led_on_work);
	cancel_work_sync(&led_off_work);
	timed_output_dev_unregister(&led_green_dev);
	unregister_reboot_notifier(&led_green_notifier);
	unregister_pm_notifier(&led_green_notify);

	wake_lock_destroy(&led_indicator_wakelock);

	gpio_set_value(led_green_gpio, 0);
	gpio_free(led_green_gpio);
	tegra_gpio_disable(led_green_gpio);
}

MODULE_DESCRIPTION("timed output led green device");
MODULE_AUTHOR("GPL");

module_init(led_green_init);
module_exit(led_green_exit);
