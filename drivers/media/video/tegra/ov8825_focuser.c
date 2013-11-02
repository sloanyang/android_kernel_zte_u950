/*
 * OV8825 focuser driver.
 *
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

/* Implementation
 * --------------
 * The board level details about the device need to be provided in the board
 * file with the ov8825_focuser_platform_data structure.
 * Standard among NVODM kernel drivers in this structure is:
 * .cfg = Use the NVODM_CFG_ defines that are in nvodm.h.
 *        Descriptions of the configuration options are with the defines.
 *        This value is typically 0.
 * .num = The number of the instance of the device.  This should start at 1 and
 *        and increment for each device on the board.  This number will be
 *        appended to the MISC driver name, Example: /dev/focuser.1
 *        If not used or 0, then nothing is appended to the name.
 * .sync = If there is a need to synchronize two devices, then this value is
 *         the number of the device instance (.num above) this device is to
 *         sync to.  For example:
 *         Device 1 platform entries =
 *         .num = 1,
 *         .sync = 2,
 *         Device 2 platfrom entries =
 *         .num = 2,
 *         .sync = 1,
 *         The above example sync's device 1 and 2.
 *         This is typically used for stereo applications.
 * .dev_name = The MISC driver name the device registers as.  If not used,
 *             then the part number of the device is used for the driver name.
 *             If using the ODM user driver then use the name found in this
 *             driver under _default_pdata.
 *
 * The following is specific to NVODM kernel focus drivers:
 * .odm = Pointer to the nvodm_focus_odm structure.  This structure needs to
 *        be defined and populated if overriding the driver defaults.
 * .cap = Pointer to the nvodm_focus_cap structure.  This structure needs to
 *        be defined and populated if overriding the driver defaults.
 *
 * The following is specific to only this NVODM kernel focus driver:
 * .info = Pointer to the ov8825_focuser_pdata_info structure.  This structure does
 *         not need to be defined and populated unless overriding ROM data.
 * .gpio_reset = The GPIO connected to the devices reset.  If not used then
 *               leave blank.
 * .gpio_en = Due to a Linux limitation, a GPIO is defined to "enable" the
 *            device.  This workaround is for when the device's power GPIO's
 *            are behind an I2C expander.  The Linux limitation doesn't allow
 *            the I2C GPIO expander to be ready for use when this device is
 *            probed.  When this problem is solved, this driver needs to
 *            hard-code the regulator names (vreg_vdd & vreg_i2c) and remove
 *            the gpio_en WAR.
 * .vreg_vdd = This is the name of the power regulator for the device's power.
 * .vreg_i2c = This is the name of the power regulator for I2C power.
.* .i2c_addr_rom = The I2C address of the onboard ROM.
 *
 * The above values should be all that is needed to use the device with this
 * driver.  Modifications of this driver should not be needed.
 */


#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/gpio.h>
#include <media/nvc.h>
#include <media/ov8825-i2c.h>
#include <media/ov8825_focuser.h>

#define OV8825_FOCUSER_ID		0xF0
/* defaults if no ROM data */
#define OV8825_FOCUSER_POS_LOW_DEFAULT		(0x000)
#define OV8825_FOCUSER_POS_HIGH_DEFAULT		(0x3FF)
#define OV8825_FOCUSER_HYPERFOCAL_RATIO 1836 /* 41.2f/224.4f Ratio source: SEMCO */ /* outdated */
/* _HYPERFOCAL_RATIO is multiplied and _HYPERFOCAL_DIV divides for float */
#define OV8825_FOCUSER_HYPERFOCAL_DIV 	10000
#define OV8825_FOCUSER_FOCAL_LENGTH	0x308D70A4 /* outdated */
#define OV8825_FOCAL_LENGTH	 4
#define OV8825_FOCUSER_FNUMBER		0x30333333 /* outdated */
#define OV8825_FOCUSER_MAX_APERATURE	0x3FCA0EA1 /* outdated */
/* OV8825_FOCUSER_CAPS_VER = 0: invalid value */
/* OV8825_FOCUSER_CAPS_VER = 1: added NVODM_PARAM_STS */
/* OV8825_FOCUSER_CAPS_VER = 2: expanded nvodm_focus_cap */
#define OV8825_FOCUSER_CAPS_VER		2
#define OV8825_FOCUSER_ACTUATOR_RANGE	1000
#define OV8825_FOCUSER_SETTLETIME	200
#define OV8825_FOCUSER_FOCUS_MACRO	1000
#define OV8825_FOCUSER_FOCUS_HYPER	50
#define OV8825_FOCUSER_FOCUS_INFINITY	10
#define OV8825_FOCUSER_TIMEOUT_MS	30


struct ov8825_focuser_info {
	atomic_t in_use;
	struct platform_device *platform_device;
	struct ov8825_focuser_platform_data *pdata;
	struct miscdevice miscdev;
	struct list_head list;
	int pwr_api;
	int pwr_dev;
	struct nvc_regulator vreg_vdd;
	struct nvc_regulator vreg_i2c;
	u8 s_mode;
	struct ov8825_focuser_info *s_info;
	unsigned i2c_addr_rom;
	struct nvc_focus_nvc nvc;
	struct nvc_focus_cap cap;
	enum nvc_focus_sts sts;
	struct ov8825_focuser_pdata_info cfg;
	bool gpio_flag_reset;
	bool init_cal_flag;
	s16 abs_base;
	u32 abs_range;
	u32 pos_rel_old;
	u32 pos_rel;
	s16 pos_abs;
	long pos_time_wr;
};

static struct ov8825_focuser_pdata_info ov8825_focuser_default_info = {
	.move_timeoutms	= OV8825_FOCUSER_TIMEOUT_MS,
	.focus_hyper_ratio = OV8825_FOCUSER_HYPERFOCAL_RATIO,
	.focus_hyper_div = OV8825_FOCUSER_HYPERFOCAL_DIV,
};

static struct nvc_focus_cap ov8825_focuser_default_cap = {
	.version	= OV8825_FOCUSER_CAPS_VER,
	.actuator_range = OV8825_FOCUSER_ACTUATOR_RANGE,
	.settle_time	= OV8825_FOCUSER_SETTLETIME,
	.focus_macro	= OV8825_FOCUSER_FOCUS_MACRO,
	.focus_hyper	= OV8825_FOCUSER_FOCUS_HYPER,
	.focus_infinity	= OV8825_FOCUSER_FOCUS_INFINITY,
};

static struct nvc_focus_nvc ov8825_focuser_default_nvc = {
	.focal_length	= OV8825_FOCUSER_FOCAL_LENGTH,
	.fnumber	= OV8825_FOCUSER_FNUMBER,
	.max_aperature	= OV8825_FOCUSER_MAX_APERATURE,
};

static struct ov8825_focuser_platform_data ov8825_focuser_default_pdata = {
	.cfg		= 0,
	.num		= 0,
	.sync		= 0,
	.dev_name	= "focuser",
	.i2c_addr_rom	= 0x50,
};

static LIST_HEAD(ov8825_focuser_info_list);
static DEFINE_SPINLOCK(ov8825_focuser_spinlock);

static int ov8825_focuser_pm_wr(struct ov8825_focuser_info *info, int pwr)
{
	int err = 0;
#if 1
    return 0;
#else
	if (pwr == info->pwr_dev)
		return 0;

	switch (pwr) {
	case NVC_PWR_OFF:
	case NVC_PWR_STDBY_OFF:
	case NVC_PWR_STDBY:
	case NVC_PWR_COMM:
	case NVC_PWR_ON:
		break;

	default:
		err = -EINVAL;
		break;
	}

	if (err < 0) {
		dev_err(&info->platform_device->dev, "%s pwr err: %d\n",
				__func__, pwr);
		pwr = NVC_PWR_ERR;
	}
	info->pwr_dev = pwr;
	if (err > 0)
		return 0;
	return err;
#endif
}

static int ov8825_focuser_pm_api_wr(struct ov8825_focuser_info *info, int pwr)
{
	int err1 = 0;
	int err2 = 0;

	if (!pwr || (pwr > NVC_PWR_ON))
		return 0;

	if ((info->s_mode == NVC_SYNC_OFF) ||
			(info->s_mode == NVC_SYNC_MASTER) ||
			(info->s_mode == NVC_SYNC_STEREO)) {
		err1 = ov8825_focuser_pm_wr(info, pwr);
		if (!err1)
			info->pwr_api = pwr;
		else
			info->pwr_api = NVC_PWR_ERR;
	}
	if ((info->s_mode == NVC_SYNC_SLAVE) ||
			(info->s_mode == NVC_SYNC_STEREO)) {
		err2 = ov8825_focuser_pm_wr(info->s_info, pwr);
		if (!err2)
			info->s_info->pwr_api = pwr;
		else
			info->s_info->pwr_api = NVC_PWR_ERR;
	}
	if (info->pdata->cfg & NVC_CFG_NOERR)
		return 0;

	return err1 | err2;
}

static int ov8825_focuser_pm_dev_wr(struct ov8825_focuser_info *info, int pwr)
{
	if (pwr < info->pwr_api)
		pwr = info->pwr_api;
	return ov8825_focuser_pm_wr(info, pwr);
}

static void ov8825_focuser_pm_exit(struct ov8825_focuser_info *info)
{
	ov8825_focuser_pm_api_wr(info, NVC_PWR_OFF);
}

static int ov8825_focuser_dev_id(struct ov8825_focuser_info *info)
{
	u8 val;
	int err;

	err = ov8825_i2c_read_reg(NULL, 0x300A, &val);
	if (!err) {
		return 0;
    }

	return -ENODEV;
}


static s16 ov8825_focuser_rel2abs(struct ov8825_focuser_info *info, u32 rel_position)
{
	s16 abs_pos;
     //  printk("info->abs_range=%d,info->cfg.limit_low=%d,info->cfg.limit_high=%d\n",info->abs_range,info->cfg.limit_low,info->cfg.limit_high);
  // printk("info->cap.actuator_range=%d,info->abs_base=%d\n",info->cap.actuator_range,info->abs_base );
  // printk("info->cap.focus_macro=%d,info->cap.focus_hyper=%d,info->cap.focus_infinity=%d\n",info->cap.focus_macro,info->cap.focus_hyper,info->cap.focus_infinity);
    if (rel_position > info->cap.actuator_range)
		rel_position = info->cap.actuator_range;
	rel_position = info->cap.actuator_range - rel_position;
	if (rel_position) {
		rel_position *= info->abs_range;
		rel_position /= info->cap.actuator_range;
	}
	abs_pos = (s16)(info->abs_base + rel_position);
	if (abs_pos < info->cfg.limit_low)
		abs_pos = info->cfg.limit_low;
	if (abs_pos > info->cfg.limit_high)
		abs_pos = info->cfg.limit_high;
	return abs_pos;
}

static u32 ov8825_focuser_abs2rel(struct ov8825_focuser_info *info, s16 abs_position)
{
	u32 rel_pos;

	if (abs_position > info->cfg.limit_high)
		abs_position = info->cfg.limit_high;
	if (abs_position < info->abs_base)
		abs_position = info->abs_base;
	rel_pos = (u32)(abs_position - info->abs_base);
	rel_pos *= info->cap.actuator_range;
	rel_pos /= info->abs_range;
	if (rel_pos > info->cap.actuator_range)
		rel_pos = info->cap.actuator_range;
	rel_pos = info->cap.actuator_range - rel_pos;
	return rel_pos;
}

static int ov8825_focuser_abs_pos_rd(struct ov8825_focuser_info *info, s16 *position)
{
	int err;
	u8 pos1;
	u8 pos2;
   // printk("ov8825_focuser_abs_pos_rd");
    err = ov8825_i2c_read_reg(NULL, 0x3618, &pos1);
    err |= ov8825_i2c_read_reg(NULL, 0x3619, &pos2);
    if(!err) {
        *position = ((pos2 & 0x3F) << 4) | ((pos1 & 0xF0) >> 4);
    }

	return err;
}

static int ov8825_focuser_rel_pos_rd(struct ov8825_focuser_info *info, u32 *position)
{
	s16 abs_pos;
	long msec;
	int pos;
	int err;

	err = ov8825_focuser_abs_pos_rd(info, &abs_pos);
	if (err)
		return -EINVAL;

    msec = jiffies;
    msec -= info->pos_time_wr;
    msec = msec * 1000 / HZ;
    if (msec > info->cfg.move_timeoutms) {
        pos = (int)ov8825_focuser_abs2rel(info, abs_pos);
	    if ((pos == (info->pos_rel - 1)) ||
			(pos == (info->pos_rel + 1))) {
    		pos = (int)info->pos_rel;
    	}
    } else {
        pos = (int)info->pos_rel_old;
    }

	if (pos < 0)
		pos = 0;
	*position = (u32)pos;
	return 0;
}

static int ov8825_focuser_calibration(struct ov8825_focuser_info *info, bool use_defaults)
{
	u8 reg;
	s16 abs_top;
	u32 rel_range;
	u32 rel_lo;
	u32 rel_hi;
	u32 step;
	u32 loop_limit;
	u32 i;
	int err;
	int ret = 0;

	if (info->init_cal_flag)
		return 0;

	/* set defaults */
	memcpy(&info->cfg, &ov8825_focuser_default_info, sizeof(info->cfg));
	memcpy(&info->nvc, &ov8825_focuser_default_nvc, sizeof(info->nvc));
	memcpy(&info->cap, &ov8825_focuser_default_cap, sizeof(info->cap));
	if (info->pdata->i2c_addr_rom)
		info->i2c_addr_rom = info->pdata->i2c_addr_rom;
	else
		info->i2c_addr_rom = ov8825_focuser_default_pdata.i2c_addr_rom;
	/* set overrides if any */
	if (info->pdata->nvc) {
		if (info->pdata->nvc->fnumber)
			info->nvc.fnumber = info->pdata->nvc->fnumber;
		if (info->pdata->nvc->focal_length)
			info->nvc.focal_length =
					info->pdata->nvc->focal_length;
		if (info->pdata->nvc->max_aperature)
			info->nvc.max_aperature =
					info->pdata->nvc->max_aperature;
	}
	if (info->pdata->cap) {
		if (info->pdata->cap->actuator_range)
			info->cap.actuator_range =
					info->pdata->cap->actuator_range;
		if (info->pdata->cap->settle_time)
			info->cap.settle_time = info->pdata->cap->settle_time;
		if (info->pdata->cap->focus_macro)
			info->cap.focus_macro = info->pdata->cap->focus_macro;
		if (info->pdata->cap->focus_hyper)
			info->cap.focus_hyper = info->pdata->cap->focus_hyper;
		if (info->pdata->cap->focus_infinity)
			info->cap.focus_infinity =
					info->pdata->cap->focus_infinity;
	}

	/* set overrides */
	if (info->pdata->info) {
		if (info->pdata->info->pos_low)
			info->cfg.pos_low = info->pdata->info->pos_low;
		if (info->pdata->info->pos_high)
			info->cfg.pos_high = info->pdata->info->pos_high;
		if (info->pdata->info->limit_low)
			info->cfg.limit_low = info->pdata->info->limit_low;
		if (info->pdata->info->limit_high)
			info->cfg.limit_high = info->pdata->info->limit_high;
		if (info->pdata->info->move_timeoutms)
			info->cfg.move_timeoutms =
					info->pdata->info->move_timeoutms;
		if (info->pdata->info->focus_hyper_ratio)
			info->cfg.focus_hyper_ratio =
					info->pdata->info->focus_hyper_ratio;
		if (info->pdata->info->focus_hyper_div)
			info->cfg.focus_hyper_div =
					info->pdata->info->focus_hyper_div;
	}

	if (!info->cfg.pos_low || !info->cfg.pos_high ||
			!info->cfg.limit_low || !info->cfg.limit_high)
		err = 1;
	else
		err = 0;
	/* Exit with -EIO */
	if (!use_defaults && ret && err) {
		dev_err(&info->platform_device->dev, "%s ERR\n", __func__);
		return -EIO;
	}

	/* Use defaults */
	if (err) {
		info->cfg.pos_low = OV8825_FOCUSER_POS_LOW_DEFAULT;
		info->cfg.pos_high = OV8825_FOCUSER_POS_HIGH_DEFAULT;
		info->cfg.limit_low = OV8825_FOCUSER_POS_LOW_DEFAULT;
		info->cfg.limit_high = OV8825_FOCUSER_POS_HIGH_DEFAULT;
		dev_err(&info->platform_device->dev, "%s ERR: ERPOM data is void!  "
			    "Focuser will use defaults that will cause "
			    "reduced functionality!\n", __func__);
	}
	if (info->cfg.pos_low < info->cfg.limit_low)
		info->cfg.pos_low = info->cfg.limit_low;
	if (info->cfg.pos_high > info->cfg.limit_high)
		info->cfg.pos_high = info->cfg.limit_high;
	//dev_dbg(&info->platform_device->dev, "%s pos_low=%d\n", __func__,
	//			(int)info->cfg.pos_low);
	//dev_dbg(&info->platform_device->dev, "%s pos_high=%d\n", __func__,
	//			(int)info->cfg.pos_high);
	//dev_dbg(&info->platform_device->dev, "%s limit_low=%d\n", __func__,
	//			(int)info->cfg.limit_low);
	//dev_dbg(&info->platform_device->dev, "%s limit_high=%d\n", __func__,
	//			(int)info->cfg.limit_high);
	/*
	 * calculate relative and absolute positions
	 * Note that relative values, what upper SW uses, are the
	 * abstraction of HW (absolute) values.
	 * |<--limit_low                                  limit_high-->|
	 * | |<-------------------_ACTUATOR_RANGE------------------->| |
	 *              -focus_inf                        -focus_mac
	 *   |<---RI--->|                                 |<---RM--->|
	 *   -abs_base  -pos_low                          -pos_high  -abs_top
	 *
	 * The pos_low and pos_high are fixed absolute positions and correspond
	 * to the relative focus_infinity and focus_macro, respectively.  We'd
	 * like to have "wiggle" room (RI and RM) around these relative
	 * positions so the loop below finds the best fit for RI and RM without
	 * passing the absolute limits.
	 * We want our _ACTUATOR_RANGE to be infinity on the 0 end and macro
	 * on the max end.  However, the focuser HW is opposite this.
	 * Therefore we use the rel(ative)_lo/hi variables in the calculation
	 * loop and assign them the focus_infinity and focus_macro values.
	 */
	rel_lo = (info->cap.actuator_range - info->cap.focus_macro);
	rel_hi = info->cap.focus_infinity;
	info->abs_range = (u32)(info->cfg.pos_high - info->cfg.pos_low);
	loop_limit = (rel_lo > rel_hi) ? rel_lo : rel_hi;
	for (i = 0; i <= loop_limit; i++) {
		rel_range = info->cap.actuator_range - (rel_lo + rel_hi);
		step = info->abs_range / rel_range;
		info->abs_base = info->cfg.pos_low - (step * rel_lo);
		abs_top = info->cfg.pos_high + (step * rel_hi);
		if (info->abs_base < info->cfg.limit_low) {
			if (rel_lo > 0)
				rel_lo--;
		}
		if (abs_top > info->cfg.limit_high) {
			if (rel_hi > 0)
				rel_hi--;
		}
		if (info->abs_base >= info->cfg.limit_low &&
					abs_top <= info->cfg.limit_high)
			break;
	}
	info->cap.focus_hyper = info->abs_range;
	info->abs_range = (u32)(abs_top - info->abs_base);
	/* calculate absolute hyperfocus position */
	info->cap.focus_hyper *= info->cfg.focus_hyper_ratio;
	info->cap.focus_hyper /= info->cfg.focus_hyper_div;
	abs_top = (s16)(info->cfg.pos_high - info->cap.focus_hyper);
	/* update actual relative positions */
	info->cap.focus_hyper = ov8825_focuser_abs2rel(info, abs_top);
	info->cap.focus_infinity = ov8825_focuser_abs2rel(info, info->cfg.pos_high);
	info->cap.focus_macro = ov8825_focuser_abs2rel(info, info->cfg.pos_low);
	//dev_dbg(&info->platform_device->dev, "%s focus_macro=%u\n", __func__,
	//				info->cap.focus_macro);
	//dev_dbg(&info->platform_device->dev, "%s focus_infinity=%u\n", __func__,
	//				info->cap.focus_infinity);
	//dev_dbg(&info->platform_device->dev, "%s focus_hyper=%u\n", __func__,
	//				info->cap.focus_hyper);
	info->init_cal_flag = 1;
	//dev_dbg(&info->platform_device->dev, "%s complete\n", __func__);
	return 0;
}

static int ov8825_focuser_dev_init(struct ov8825_focuser_info *info)
{
	int err;

	if (info->init_cal_flag)
		return 0;

	err = ov8825_focuser_calibration(info, false);
	return err;
}

static int ov8825_focuser_move_lens(struct ov8825_focuser_info *info, s16 tar_pos)
{
    int err;
	u8 modeBits;
    //printk("ov8825_focuser_move_lens");
    err = ov8825_i2c_read_reg(NULL, 0x3618, &modeBits);
	if (err)
		return err;
    modeBits &= 0x0F;
	/* Set Position */
	err = ov8825_i2c_write_reg(NULL, 0x3619, ((tar_pos & 0x3F0) >> 4));
	err |= ov8825_i2c_write_reg(NULL, 0x3618, (((tar_pos & 0x00F) << 4)|modeBits));

    s16 rd_pos = 0;
    ov8825_focuser_abs_pos_rd(info, &rd_pos);
	return err;
}

static int ov8825_focuser_pos_abs_wr(struct ov8825_focuser_info *info, s16 position)
{
	if (position > info->cfg.limit_high || position < info->cfg.limit_low) {
		dev_err(&info->platform_device->dev, "%s target position %u out of bounds - "
            "LOW: %d, HIGH: %d\n", __func__, position, info->cfg.limit_low, info->cfg.limit_high);
		return -EINVAL;
    }

	return ov8825_focuser_move_lens(info, position);
}

static int ov8825_focuser_pos_rel_wr(struct ov8825_focuser_info *info, u32 position)
{
	s16 abs_pos;

	if (position > info->cap.actuator_range) {
		dev_err(&info->platform_device->dev, "%s invalid position %u\n",
				__func__, position);
		return -EINVAL;
	}

	abs_pos = ov8825_focuser_rel2abs(info, position);
	info->pos_rel_old = info->pos_rel;
	info->pos_rel = position;
	info->pos_abs = abs_pos;
	info->pos_time_wr = jiffies;
	return ov8825_focuser_pos_abs_wr(info, abs_pos);
}


static int ov8825_focuser_param_rd(struct ov8825_focuser_info *info, unsigned long arg)
{
	struct nvc_param params;
	const void *data_ptr;
	u32 data_size = 0;
	u32 position;
	int err;

	if (copy_from_user(&params,
			(const void __user *)arg,
			sizeof(struct nvc_param))) {
		dev_err(&info->platform_device->dev, "%s %d copy_from_user err\n",
				__func__, __LINE__);
		return -EFAULT;
	}
	
	if (info->s_mode == NVC_SYNC_SLAVE)
		info = info->s_info;
	
	//printk("ov8825_focuser_param_rd params.param=%d",params.param);
	switch (params.param) {
	case NVC_PARAM_LOCUS:
		//ov8825_focuser_pm_dev_wr(info, NVODM_PWR_COMM);
		err = ov8825_focuser_rel_pos_rd(info, &position);
               printk("read locus [ %u ]\n", position);
		if (err && !(info->pdata->cfg & NVC_CFG_NOERR)) {
			return -EINVAL;
        }

		data_ptr = &position;
		data_size = sizeof(position);
		//ov8825_focuser_pm_dev_wr(info, NVODM_PWR_OFF);
		//dev_dbg(&info->platform_device->dev, "%s LOCUS: %d\n",
			//	__func__, position);
		break;

	case NVC_PARAM_FOCAL_LEN:
              info->nvc.focal_length = OV8825_FOCAL_LENGTH;
			
		data_ptr = &info->nvc.focal_length;
		data_size =sizeof(info->nvc.focal_length);
		//dev_dbg(&info->platform_device->dev, "%s FOCAL_LEN: %x\n",
		//		__func__, info->nvc.focal_length);
		//printk("NVC_PARAM_FOCAL_LEN data_ptr =%d\n",*(int*)data_ptr);
		break;

	case NVC_PARAM_MAX_APERTURE:
		data_ptr = &info->nvc.max_aperature;
		data_size = sizeof(info->nvc.max_aperature);
		//dev_dbg(&info->platform_device->dev, "%s MAX_APERTURE: %x\n",
		//		__func__, info->nvc.max_aperature);
		break;

	case NVC_PARAM_FNUMBER:
		data_ptr = &info->nvc.fnumber;
		data_size = sizeof(info->nvc.fnumber);
		//dev_dbg(&info->platform_device->dev, "%s FNUMBER: %x\n",
		//		__func__, info->nvc.fnumber);
		break;

	case NVC_PARAM_CAPS:
		//ov8825_focuser_pm_dev_wr(info, NVC_PWR_COMM);
		err = ov8825_focuser_calibration(info, true);
		//ov8825_focuser_pm_dev_wr(info, NVC_PWR_STDBY);
		if (err)
			return -EIO;

		data_ptr = &info->cap;
		/* there are different sizes depending on the version */
		/* send back just what's requested or our max size */
		if (params.sizeofvalue < sizeof(info->cap))
			data_size = params.sizeofvalue;
		else
			data_size = sizeof(info->cap);
		//dev_dbg(&info->platform_device->dev, "%s CAPS\n",
		//		__func__);
		break;

	case NVC_PARAM_STS:
		data_ptr = &info->sts;
		data_size = sizeof(info->sts);
		//dev_dbg(&info->platform_device->dev, "%s STS: %d\n",
		//		__func__, info->sts);
		break;

	case NVC_PARAM_STEREO:
		data_ptr = &info->s_mode;
		data_size = sizeof(info->s_mode);
		//dev_dbg(&info->platform_device->dev, "%s STEREO: %d\n",
		//		__func__, info->s_mode);
		break;

	default:
		dev_err(&info->platform_device->dev,
				"%s unsupported parameter: %d\n",
				__func__, params.param);
		return -EINVAL;
	}

	if (params.sizeofvalue < data_size) {
		dev_err(&info->platform_device->dev, "%s %d data size err\n",
				__func__, __LINE__);
		return -EINVAL;
	}
	

	//printk("NVC_PARAM_FOCAL_LEN 11 data_ptr =%d",*(int*)data_ptr);

	if (copy_to_user((void __user *)params.p_value,
			 data_ptr,
			 data_size)) {
		dev_err(&info->platform_device->dev, "%s %d copy_to_user err\n",
				__func__, __LINE__);
		return -EFAULT;
	}
	

	//printk("NVC_PARAM_FOCAL_LEN 12 data_ptr =%d\n",*(int*)data_ptr);
	//printk("NVC_PARAM_FOCAL_LEN 12 params.p_value =%d\n",*(int*)params.p_value);

	return 0;
}

static int ov8825_focuser_param_wr_s(struct ov8825_focuser_info *info,
			     struct nvc_param *params,
			     u32 u32_val)
{
	int err ;

	switch (params->param) {
	case NVC_PARAM_LOCUS:
		//dev_dbg(&info->platform_device->dev, "%s LOCUS: %d\n",
		//		__func__, (int)u32_val);
#if 0
		ov8825_focuser_pm_dev_wr(info, NVODM_PWR_ON);
#endif
    ov8825_i2c_write_reg(NULL, 0x361A, 0xb0);
    ov8825_i2c_write_reg(NULL, 0x361B, 0x04);
		err = ov8825_focuser_pos_rel_wr(info, u32_val);
		return err;

	case NVC_PARAM_RESET:
#if 0
		err = ov8825_focuser_pm_wr(info, NVODM_PWR_OFF);
		err |= ov8825_focuser_pm_wr(info, NVODM_PWR_ON);
		err |= ov8825_focuser_pm_wr(info, info->pwr_api);
#endif
		//dev_dbg(&info->platform_device->dev, "%s RESET: %d\n",
		//		__func__, err);
		return err;

	case NVC_PARAM_SELF_TEST:
#if 0
		ov8825_focuser_pm_dev_wr(info, NVODM_PWR_ON);
#endif
		//dev_dbg(&info->platform_device->dev, "%s SELF_TEST: %d\n",
		//		__func__, err);
		return err;

	default:
		dev_err(&info->platform_device->dev,
				"%s unsupported parameter: %d\n",
				__func__, params->param);
		return -EINVAL;
	}
}

static int ov8825_focuser_param_wr(struct ov8825_focuser_info *info, unsigned long arg)
{
	struct nvc_param params;
	u8 val;
	u32 u32_val;
	int err = 0;

	if (copy_from_user(&params,
				(const void __user *)arg,
				sizeof(struct nvc_param))) {
		dev_err(&info->platform_device->dev, "%s %d copy_from_user err\n",
				__func__, __LINE__);
		return -EFAULT;
	}

	if (copy_from_user(&u32_val, (const void __user *)params.p_value,
			   sizeof(u32_val))) {
		dev_err(&info->platform_device->dev, "%s %d copy_from_user err\n",
				__func__, __LINE__);
		return -EFAULT;
	}

	/* parameters independent of sync mode */
        //printk("wr params.param=%d,u32_val=%d",params.param,u32_val);
	switch (params.param) {
	case NVC_PARAM_STEREO:
		dev_dbg(&info->platform_device->dev, "%s STEREO: %d\n",
				__func__, (int)u32_val);
		val = (u8)u32_val;
		if (val == info->s_mode)
			return 0;

		switch (val) {
		case NVC_SYNC_OFF:
		case NVC_SYNC_MASTER:
			info->s_mode = val;
			if (info->s_info != NULL)
				info->s_info->s_mode = val;
			break;

		case NVC_SYNC_SLAVE:
		case NVC_SYNC_STEREO:
			if (info->s_info != NULL) {
				/* sync power */
				err = ov8825_focuser_pm_api_wr(info->s_info,
						       info->pwr_api);
				if (!err) {
					info->s_mode = val;
					info->s_info->s_mode = val;
				}
			}
			break;
		}
		if (info->pdata->cfg & NVC_CFG_NOERR)
			return 0;

		return err;

	default:
	/* parameters dependent on sync mode */
             // printk("wr info->s_mode=%d",info->s_mode);
		switch(info->s_mode) {
		case NVC_SYNC_OFF:
		case NVC_SYNC_MASTER:
			return ov8825_focuser_param_wr_s(info, &params, u32_val);

		case NVC_SYNC_SLAVE:
			return ov8825_focuser_param_wr_s(info->s_info,
						 &params,
						 u32_val);

		case NVC_SYNC_STEREO:
			err = ov8825_focuser_param_wr_s(info, &params, u32_val);
			if (!(info->pdata->cfg & NVC_CFG_SYNC_I2C_MUX))
				err |= ov8825_focuser_param_wr_s(info->s_info,
							 &params,
							 u32_val);
			return err;

		default:
			dev_err(&info->platform_device->dev, "%s %d internal err\n",
					__func__, __LINE__);
			return -EINVAL;
		}
	}
}

static long ov8825_focuser_ioctl(struct file *file,
		unsigned int cmd,
		unsigned long arg)
{
	struct ov8825_focuser_info *info = file->private_data;

	int pwr;

	switch (cmd) {
	case NVC_IOCTL_PARAM_WR:
		return ov8825_focuser_param_wr(info, arg);

	case NVC_IOCTL_PARAM_RD:
		return ov8825_focuser_param_rd(info, arg);

	case NVC_IOCTL_PWR_WR:
		/* This should just be a power hint - we shouldn't need it */
		pwr = (int)arg * 2;
		//dev_dbg(&info->platform_device->dev, "%s PWR: %d\n",
		//		__func__, pwr);
		return ov8825_focuser_pm_api_wr(info, pwr);

	case NVC_IOCTL_PWR_RD:
		if (info->s_mode == NVC_SYNC_SLAVE)
			pwr = info->s_info->pwr_api / 2;
		else
			pwr = info->pwr_api / 2;
		dev_err(&info->platform_device->dev, "%s PWR_RD: %d\n",
				__func__, pwr);
		if (copy_to_user((void __user *)arg, (const void *)&pwr,
				 sizeof(pwr))) {
			dev_err(&info->platform_device->dev,
					"%s copy_to_user err line %d\n",
					__func__, __LINE__);
			return -EFAULT;
		}

		return 0;

	default:
		dev_err(&info->platform_device->dev, "%s unsupported ioctl: %x\n",
				__func__, cmd);
		return -EINVAL;
	}

}

static int ov8825_focuser_sync_en(int dev1, int dev2)
{
	struct ov8825_focuser_info *sync1 = NULL;
	struct ov8825_focuser_info *sync2 = NULL;
	struct ov8825_focuser_info *pos = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(pos, &ov8825_focuser_info_list, list) {
		if (pos->pdata->num == dev1) {
			sync1 = pos;
			break;
		}
	}
	pos = NULL;
	list_for_each_entry_rcu(pos, &ov8825_focuser_info_list, list) {
		if (pos->pdata->num == dev2) {
			sync2 = pos;
			break;
		}
	}
	rcu_read_unlock();
	if (sync1 != NULL)
		sync1->s_info = NULL;
	if (sync2 != NULL)
		sync2->s_info = NULL;
	if (!dev1 && !dev2)
		return 0; /* no err if default instance 0's used */

	if (dev1 == dev2)
		return -EINVAL; /* err if sync instance is itself */

	if ((sync1 != NULL) && (sync2 != NULL)) {
		sync1->s_info = sync2;
		sync2->s_info = sync1;
	}
	return 0;
}

static int ov8825_focuser_sync_dis(struct ov8825_focuser_info *info)
{
	if (info->s_info != NULL) {
		info->s_info->s_mode = 0;
		info->s_info->s_info = NULL;
		info->s_mode = 0;
		info->s_info = NULL;
		return 0;
	}

	return -EINVAL;
}

static int ov8825_focuser_open(struct inode *inode, struct file *file)
{
	struct ov8825_focuser_info *info = NULL;
	struct ov8825_focuser_info *pos = NULL;
	int err;

	printk("%s\n", __func__);
	rcu_read_lock();
	list_for_each_entry_rcu(pos, &ov8825_focuser_info_list, list) {
		if (pos->miscdev.minor == iminor(inode)) {
			info = pos;
			break;
		}
	}
	rcu_read_unlock();
	if (!info)
		return -ENODEV;

	err = ov8825_focuser_sync_en(info->pdata->num, info->pdata->sync);
	if (err == -EINVAL)
		dev_err(&info->platform_device->dev,
			 "%s err: invalid num (%u) and sync (%u) instance\n",
			 __func__, info->pdata->num, info->pdata->sync);
	if (atomic_xchg(&info->in_use, 1))
		return -EBUSY;

	if (info->s_info != NULL) {
		if (atomic_xchg(&info->s_info->in_use, 1))
			return -EBUSY;
	}

	file->private_data = info;

 // if (info->pdata && info->pdata->power_on)
        {
       //  printk("focus poweron\n");
        info->pdata->power_on();
        }
 mdelay(50);
      //  printk("after poweron\n");
	ov8825_focuser_calibration(info, true);

    
    //ov8825_i2c_write_reg(NULL, 0x361A, 0xb0);
   // ov8825_i2c_write_reg(NULL, 0x361B, 0x04);

	file->private_data = info;
	dev_dbg(&info->platform_device->dev, "%s\n", __func__);

	return 0;
}

int ov8825_focuser_release(struct inode *inode, struct file *file)
{
	struct ov8825_focuser_info *info = file->private_data;

	//dev_dbg(&info->platform_device->dev, "%s\n", __func__);
  //  printk("ov8825_focuser_release");
	//ov8825_focuser_pm_wr_s(info, NVC_PWR_OFF);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	if (info->s_info != NULL)
		WARN_ON(!atomic_xchg(&info->s_info->in_use, 0));
	ov8825_focuser_sync_dis(info);
    /* 0503 */
    //  if (info->pdata && info->pdata->power_off)
                info->pdata->power_off();
	return 0;
}

static const struct file_operations ov8825_focuser_fileops = {
	.owner = THIS_MODULE,
	.open = ov8825_focuser_open,
	.unlocked_ioctl = ov8825_focuser_ioctl,
	.release = ov8825_focuser_release,
};

static void ov8825_focuser_del(struct ov8825_focuser_info *info)
{
	//ov8825_focuser_pm_exit(info);
	ov8825_focuser_sync_dis(info);
	spin_lock(&ov8825_focuser_spinlock);
	list_del_rcu(&info->list);
	spin_unlock(&ov8825_focuser_spinlock);
	synchronize_rcu();
}

static int ov8825_focuser_remove(struct platform_device *client)
{
	struct ov8825_focuser_info *info = i2c_get_clientdata(client);

	dev_dbg(&info->platform_device->dev, "%s\n", __func__);//
	misc_deregister(&info->miscdev);
	ov8825_focuser_del(info);
	return 0;
}

static int ov8825_focuser_probe(
    struct platform_device *client,
    const struct platform_device_id *id)
{
  
	struct ov8825_focuser_info *info = NULL;
	char dname[15];
	int err;
          printk("%s\n", __func__);
	dev_dbg(&client->dev, "%s\n", __func__);
	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	info->platform_device = client;
	if (client->dev.platform_data) {
		info->pdata = client->dev.platform_data;
	} else {
		info->pdata = &ov8825_focuser_default_pdata;
		dev_dbg(&client->dev,
				"%s No platform data.  Using defaults.\n",
				__func__);
	}
	INIT_LIST_HEAD(&info->list);
	spin_lock(&ov8825_focuser_spinlock);
	list_add_rcu(&info->list, &ov8825_focuser_info_list);
	spin_unlock(&ov8825_focuser_spinlock);

    ov8825_focuser_calibration(info, false);

	if (info->pdata->dev_name != 0)
		strcpy(dname, info->pdata->dev_name);
	else
		strcpy(dname, "ov8825_focuser");
	if (info->pdata->num)
		snprintf(dname, sizeof(dname), "%s.%u",
			 dname, info->pdata->num);
	info->miscdev.name = dname;
	info->miscdev.fops = &ov8825_focuser_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&info->miscdev)) {
		dev_err(&client->dev, "%s unable to register misc device %s\n",
				__func__, dname);
		ov8825_focuser_del(info);
		return -ENODEV;
	}

	return 0;
}

static const struct platform_device_id ov8825_focuser_id[] = {
	{ "ov8825_focuser", 0 },
	{ },
};

MODULE_DEVICE_TABLE(platform, ov8825_focuser_id);

static struct platform_driver ov8825_focuser_driver = {
	.driver = {
		.name = "ov8825_focuser",
		.owner = THIS_MODULE,
	},
	.id_table = ov8825_focuser_id,
	.probe = ov8825_focuser_probe,
	.remove = ov8825_focuser_remove,
};

static int __init ov8825_focuser_init(void)
{
         printk("%s\n", __func__);
	return platform_driver_register(&ov8825_focuser_driver);
}

static void __exit ov8825_focuser_exit(void)
{
	return platform_driver_unregister(&ov8825_focuser_driver);
}

module_init(ov8825_focuser_init);
module_exit(ov8825_focuser_exit);

