/*
 * arch/arm/mach-tegra/board-zteenterprise-kbc.c
 * Keys configuration for Nvidia tegra3 zteenterprise platform.
 *
 * Copyright (C) 2011 NVIDIA, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/mfd/tps6591x.h>
#include <linux/mfd/max77663-core.h>
#include <linux/gpio_scrollwheel.h>

#include <mach/irqs.h>
#include <mach/io.h>
#include <mach/iomap.h>
#include <mach/kbc.h>
#include "board.h"
#include "board-enterprise.h"

#include "gpio-names.h"
#include "devices.h"
#include <asm/io.h>  
#include "wakeups-t3.h" 


#define PMC_WAKE2_STATUS	0x168


#define GPIO_KEY(_id, _gpio, _iswake)		\
	{					\
		.code = _id,			\
		.gpio = TEGRA_GPIO_##_gpio,	\
		.active_low = 1,		\
		.desc = #_id,			\
		.type = EV_KEY,			\
		.wakeup = _iswake,		\
		.debounce_interval = 10,	\
	}

#define GPIO_IKEY(_id, _irq, _iswake, _deb)	\
	{					\
		.code = _id,			\
		.gpio = -1,			\
		.irq = _irq,			\
		.desc = #_id,			\
		.type = EV_KEY,			\
		.wakeup = _iswake,		\
		.debounce_interval = _deb,	\
	}

static struct gpio_keys_button zteenterprise_keys[] = {
    [0] = GPIO_KEY(KEY_POWER, PI6, 1),
    [1] = GPIO_KEY(KEY_VOLUMEUP, PQ1, 1),
    [2] = GPIO_KEY(KEY_VOLUMEDOWN, PQ0, 1),
};
static int zteenterprise_wakeup_key(void)
{
    u64 status3 = ((u64)readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_WAKE2_STATUS))<<32;  
    
    return status3 & TEGRA_WAKE_GPIO_PI6 ? KEY_POWER : KEY_RESERVED;
}

static struct gpio_keys_platform_data zteenterprise_keys_pdata = {
	.buttons	= zteenterprise_keys,
	.nbuttons	= ARRAY_SIZE(zteenterprise_keys),
	.wakeup_key     = zteenterprise_wakeup_key,   

};

static struct platform_device zteenterprise_keys_device = {
	.name   = "gpio-keys",
	.id     = 0,
	.dev    = {
		.platform_data  = &zteenterprise_keys_pdata,
	},
};


int __init zteenterprise_keys_init(void)
{
	int i;
	int gpio_nr;


	    pr_info("Registering gpio keys\n");

		/* Enable gpio mode for other pins */
		for (i = 0; i < zteenterprise_keys_pdata.nbuttons; i++) {
			gpio_nr = zteenterprise_keys_pdata.buttons[i].gpio;
			if (gpio_nr < 0) {
				if (get_tegra_image_type() == rck_image)
					zteenterprise_keys_pdata.buttons[i].code
							= KEY_ENTER;
			} else {
				tegra_gpio_enable(gpio_nr);
			}
		}

		platform_device_register(&zteenterprise_keys_device);
	return 0;
}
