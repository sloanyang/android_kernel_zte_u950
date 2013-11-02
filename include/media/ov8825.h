/*
 * Copyright (C) 2010 Motorola, Inc.
 * Copyright (C) 2011 NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __OV8825_H__
#define __OV8825_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

#define OV8825_IOCTL_SET_MODE		_IOW('o', 1, struct ov8825_mode)
#define OV8825_IOCTL_SET_FRAME_LENGTH	_IOW('o', 2, __u32)
#define OV8825_IOCTL_SET_COARSE_TIME	_IOW('o', 3, __u32)
#define OV8825_IOCTL_SET_GAIN		_IOW('o', 4, __u16)
#define OV8825_IOCTL_GET_STATUS		_IOR('o', 5, __u8)
#define OV8825_IOCTL_SET_GROUP_HOLD	    _IOW('o', 6, struct ov8825_ae)
#define OV8825_IOCTL_GET_SENSOR_ID      _IOR('o', 7, __u8 *)

/* OV8825 registers */
#define OV8825_OTP_READ_NWRITE          (0x3D81)
#define OV8825_OTP_BANK_SELECT          (0x3D84)
#define OV8825_OTP_DATA_START           (0x3D00)
#define OV8825_OTP_DATA_END             (0x3D1F)
#define OV8825_OTP_BANK_EN_BIT          (1 << 3)
#define OV8825_OTP_READ_BIT             (1 << 0)
#define OV8825_FUSEID_REG0              (0x3D00)
#define OV8825_FUSEID_REG1              (0x3D01)
#define OV8825_FUSEID_REG2              (0x3D02)
#define OV8825_FUSEID_REG3              (0x3D03)
#define OV8825_FUSEID_REG4              (0x3D04)

struct ov8825_mode {
	int xres;
	int yres;
	__u32 frame_length;
	__u32 coarse_time;
	__u16 gain;
};

struct ov8825_ae {
	__u32 frame_length;
	__u8 frame_length_enable;
	__u32 coarse_time;
	__u8 coarse_time_enable;
	__s32 gain;
	__u8 gain_enable;
};

#ifdef __KERNEL__
struct ov8825_platform_data {
	int (*power_on)(void);
	int (*power_off)(void);

};
#endif /* __KERNEL__ */

#endif  /* __OV8825_H__ */

