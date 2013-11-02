/*
 * Copyright (C) 2011 NVIDIA Corporation.
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

#ifndef __OV8825_FOCUSER_H__
#define __OV8825_FOCUSER_H__

#include <media/nvc_focus.h>


struct ov8825_focuser_platform_data {
	int cfg;
	int num;
	int sync;
	const char *dev_name;
	struct nvc_focus_nvc (*nvc);
	struct nvc_focus_cap (*cap);
	struct ov8825_focuser_pdata_info (*info);
	__u8 i2c_addr_rom;
	unsigned gpio_reset;
/* Due to a Linux limitation, a GPIO is defined to "enable" the device.  This
 * workaround is for when the device's power GPIO's are behind an I2C expander.
 * The Linux limitation doesn't allow the I2C GPIO expander to be ready for
 * use when this device is probed.
 * When this problem is solved, this driver needs to hard-code the regulator
 * names (vreg_vdd & vreg_i2c) and remove the gpio_en WAR.
 */
	unsigned gpio_en;
	int (*power_on)(void);
	int (*power_off)(void);
};

struct ov8825_focuser_pdata_info {
	__s16 pos_low;
	__s16 pos_high;
	__s16 limit_low;
	__s16 limit_high;
	int move_timeoutms;
	__u32 focus_hyper_ratio;
	__u32 focus_hyper_div;
};


/* Register Definition  : *** ** ** */
/* ******* addresses */


/*Macro define*/
#if !defined(abs)
#define abs(a)		(((a) > 0) ? (a) : -(a))
#endif

#endif
/* __OV8820_FOCUSER_H__ */

