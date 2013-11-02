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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __OV8825_I2C_H__
#define __OV8825_I2C_H__
#include <linux/i2c.h>

int ov8825_i2c_write_reg(struct i2c_client *badclient, u16 addr, u8 val);
int ov8825_i2c_read_reg(struct i2c_client *badclient, u16 addr, u8 *val);

#ifdef __KERNEL__
#endif /* __KERNEL__ */

#endif  /* __OV8820_I2C_H__ */

