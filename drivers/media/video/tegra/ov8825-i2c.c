/*
 * ov8825.c - ov8825 sensor driver
 *
 * Copyright (C) 2011 Google Inc.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <media/ov8825-i2c.h>
#include <media/ov8825.h>
#include <linux/proc_fs.h>

#define OV8825_MAX_RETRIES 3
#define OV8825_TABLE_WAIT_MS 0
#define OV8825_TABLE_END 1
static u8 module_id = 0;

struct ov8825_i2c_info {
	int mode;
	struct i2c_client *i2c_client;
	struct ov8825_platform_data *pdata;
};

struct ov8825_reg_m {
	u16 addr;
	u16 val;
};

struct otp_test_struct {
	u8 module_integrator_id;
	int lens_id;
	int rg_ratio;
	int bg_ratio;
	int user_data[3];
	int light_rg;
	int light_bg;
	int lenc[62];
};
static struct proc_dir_entry *sensor_proc_file;
#define SENSOR_PROC_FILE "driver/camera"

static struct ov8825_i2c_info *info;

static struct ov8825_reg_m mode_init[] = {

{0x0103, 0x01},
{OV8825_TABLE_WAIT_MS, 5},
{0x3000, 0x16},
{0x3001, 0x00},
{0x3002, 0x6c},
{0x3003, 0xce},
{0x3004, 0xd4},
{0x3005, 0x00},
{0x3006, 0x10},
{0x3007, 0x3b},
{0x300d, 0x00},
{0x301f, 0x09},
{0x3010, 0x00},
{0x3011, 0x01},
{0x3012, 0x80},
{0x3013, 0x39},
{0x3018, 0x00},
{0x3104, 0x20},
{0x3106, 0x15},
{0x3300, 0x00},
{0x3500, 0x00},
{0x3501, 0x4e},
{0x3502, 0xa0},
{0x3503, 0x07},
{0x3509, 0x00}, /* gain stuff - 350[a/b]|[4/5] */
{0x350b, 0x1f},
{0x3600, 0x07},
{0x3601, 0x34},
{0x3602, 0x42},
{0x3603, 0x5c},
{0x3604, 0x98},
{0x3605, 0xf5},
{0x3609, 0xb4},
{0x360a, 0x7c},
{0x360b, 0xc9},
{0x360c, 0x0b},
{0x3612, 0x00},
{0x3613, 0x02},
{0x3614, 0x0f},
{0x3615, 0x00},
{0x3616, 0x03},
{0x3617, 0xa1},
{0x361a, 0xb0},
{0x361b, 0x04},
{0x3700, 0x20},
{0x3701, 0x44},
{0x3702, 0x50},
{0x3703, 0xcc},
{0x3704, 0x19},
{0x3705, 0x14},
{0x3706, 0x4b},
{0x3707, 0x63},
{0x3708, 0x84},
{0x3709, 0x40},
{0x370a, 0x33},
{0x370b, 0x01},
{0x370c, 0x50},
{0x370d, 0x00},
{0x370e, 0x08},
{0x3711, 0x0f},
{0x3712, 0x9c},
{0x3724, 0x01},
{0x3725, 0x92},
{0x3726, 0x01},
{0x3727, 0xa9},
{0x3800, 0x00},
{0x3801, 0x00},
{0x3802, 0x00},
{0x3803, 0x00},
{0x3804, 0x0c},
{0x3805, 0xdf},
{0x3806, 0x09},
{0x3807, 0x9b},
{0x3808, 0x06},
{0x3809, 0x60},
{0x380a, 0x04},
{0x380b, 0xc8},
{0x380c, 0x0d},
{0x380d, 0xbc},
{0x380e, 0x04},
{0x380f, 0xf0},
{0x3810, 0x00},
{0x3811, 0x08},
{0x3812, 0x00},
{0x3813, 0x04},
{0x3814, 0x31},
{0x3815, 0x31},
{0x3816, 0x02},
{0x3817, 0x40},
{0x3818, 0x00},
{0x3819, 0x40},
{0x3820, 0x80},
{0x3821, 0x17},
{0x3b1f, 0x00},
{0x3d00, 0x00},
{0x3d01, 0x00},
{0x3d02, 0x00},
{0x3d03, 0x00},
{0x3d04, 0x00},
{0x3d05, 0x00},
{0x3d06, 0x00},
{0x3d07, 0x00},
{0x3d08, 0x00},
{0x3d09, 0x00},
{0x3d0a, 0x00},
{0x3d0b, 0x00},
{0x3d0c, 0x00},
{0x3d0d, 0x00},
{0x3d0e, 0x00},
{0x3d0f, 0x00},
{0x3d10, 0x00},
{0x3d11, 0x00},
{0x3d12, 0x00},
{0x3d13, 0x00},
{0x3d14, 0x00},
{0x3d15, 0x00},
{0x3d16, 0x00},
{0x3d17, 0x00},
{0x3d18, 0x00},
{0x3d19, 0x00},
{0x3d1a, 0x00},
{0x3d1b, 0x00},
{0x3d1c, 0x00},
{0x3d1d, 0x00},
{0x3d1e, 0x00},
{0x3d1f, 0x00},
{0x3d80, 0x00},
{0x3d81, 0x00},
{0x3d84, 0x00},
{0x3f00, 0x00},
{0x3f01, 0xfc},
{0x3f05, 0x10},
{0x3f06, 0x00},
{0x3f07, 0x00},
{0x4000, 0x29},
{0x4001, 0x02},
{0x4002, 0x45},
{0x4003, 0x08},
{0x4004, 0x04},
{0x4005, 0x18},
{0x4300, 0xff},
{0x4303, 0x00},
{0x4304, 0x08},
{0x4307, 0x00},
{0x4600, 0x04},
{0x4601, 0x00},
{0x4602, 0x30},
{0x4800, 0x04},//continue bit 5 = 0;
{0x4801, 0x0f},
{0x4837, 0x28},
{0x4843, 0x02},
{0x5000, 0x06},
{0x5001, 0x00},
{0x5002, 0x00},
{0x5068, 0x00},
{0x506a, 0x00},
{0x501f, 0x00},
{0x5780, 0xfc},
{0x5c00, 0x80},
{0x5c01, 0x00},
{0x5c02, 0x00},
{0x5c03, 0x00},
{0x5c04, 0x00},
{0x5c05, 0x00},
{0x5c06, 0x00},
{0x5c07, 0x80},
{0x5c08, 0x10},
{0x6700, 0x05},
{0x6701, 0x19},
{0x6702, 0xfd},
{0x6703, 0xd7},
{0x6704, 0xff},
{0x6705, 0xff},
{0x6800, 0x10},
{0x6801, 0x02},
{0x6802, 0x90},
{0x6803, 0x10},
{0x6804, 0x59},
{0x6900, 0x60},
{0x6901, 0x05},
//{0x0100, 0x01},
{0x5800, 0x0f},
{0x5801, 0x0d},
{0x5802, 0x09},
{0x5803, 0x0a},
{0x5804, 0x0d},
{0x5805, 0x14},
{0x5806, 0x0a},
{0x5807, 0x04},
{0x5808, 0x03},
{0x5809, 0x03},
{0x580a, 0x05},
{0x580b, 0x0a},
{0x580c, 0x05},
{0x580d, 0x02},
{0x580e, 0x00},
{0x580f, 0x00},
{0x5810, 0x03},
{0x5811, 0x05},
{0x5812, 0x09},
{0x5813, 0x03},
{0x5814, 0x01},
{0x5815, 0x01},
{0x5816, 0x04},
{0x5817, 0x09},
{0x5818, 0x09},
{0x5819, 0x08},
{0x581a, 0x06},
{0x581b, 0x06},
{0x581c, 0x08},
{0x581d, 0x06},
{0x581e, 0x33},
{0x581f, 0x11},
{0x5820, 0x0e},
{0x5821, 0x0f},
{0x5822, 0x11},
{0x5823, 0x3f},
{0x5824, 0x08},
{0x5825, 0x46},
{0x5826, 0x46},
{0x5827, 0x46},
{0x5828, 0x46},
{0x5829, 0x46},
{0x582a, 0x42},
{0x582b, 0x42},
{0x582c, 0x44},
{0x582d, 0x46},
{0x582e, 0x46},
{0x582f, 0x60},
{0x5830, 0x62},
{0x5831, 0x42},
{0x5832, 0x46},
{0x5833, 0x46},
{0x5834, 0x44},
{0x5835, 0x44},
{0x5836, 0x44},
{0x5837, 0x48},
{0x5838, 0x28},
{0x5839, 0x46},
{0x583a, 0x48},
{0x583b, 0x68},
{0x583c, 0x28},
{0x583d, 0xae},
{0x5842, 0x00},
{0x5843, 0xef},
{0x5844, 0x01},
{0x5845, 0x3f},
{0x5846, 0x01},
{0x5847, 0x3f},
{0x5848, 0x00},
{0x5849, 0xd5},
//{0x0100, 0x01},
    {OV8825_TABLE_WAIT_MS, 5},
{OV8825_TABLE_END, 0x0000}
};

int ov8825_i2c_write_reg(struct i2c_client *badclient, u16 addr, u8 val)
{
    struct i2c_client * client = info->i2c_client;
	int err;
	struct i2c_msg msg;
	unsigned char data[3];
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);;
	data[1] = (u8) (addr & 0xff);
	data[2] = (u8) (val & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;

	do {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			return 0;
		retry++;
		pr_err("ov8825: i2c transfer failed, retrying %x %x\n",
			addr, val);
		usleep_range(3000, 3250);
	} while (retry <= OV8825_MAX_RETRIES);

	return err;
}
EXPORT_SYMBOL(ov8825_i2c_write_reg);

int ov8825_i2c_read_reg(struct i2c_client *badclient, u16 addr, u8 *val)
{
    struct i2c_client * client = info->i2c_client;
	int err;
	struct i2c_msg msg[2];
	unsigned char data[3];
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;

	/* high byte goes out first */
	data[0] = (u8) (addr >> 8);;
	data[1] = (u8) (addr & 0xff);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data + 2;

	do {
		err = i2c_transfer(client->adapter, msg, 2);
		if (err == 2)
			break;
		retry++;
		pr_err("ov8825: i2c transfer failed, retrying %x %x\n",
			addr, val);
		usleep_range(3000, 3250);
	} while (retry <= OV8825_MAX_RETRIES);

	*val = data[2];

	if (err != 2)
		return -EINVAL;

	return 0;
}

EXPORT_SYMBOL(ov8825_i2c_read_reg);


static int write_table(struct i2c_client *client,
				const struct ov8825_reg_m table[],
				const struct ov8825_reg_m override_list[],
				int num_override_regs)
{
	int err;
	const struct ov8825_reg_m *next;
	int i;
	u16 val;

	for (next = table; next->addr != OV8825_TABLE_END; next++) {
		if (next->addr == OV8825_TABLE_WAIT_MS) {
			msleep(next->val);
			continue;
		}

		val = next->val;

		/* When an override list is passed in, replace the reg */
		/* value to write if the reg is in the list            */
		if (override_list) {
			for (i = 0; i < num_override_regs; i++) {
				if (next->addr == override_list[i].addr) {
					val = override_list[i].val;
					break;
				}
			}
		}

		err = ov8825_i2c_write_reg(client, next->addr, val);
		if (err)
			return err;
	}
	return 0;
}

int check_otp_wb_m(struct i2c_client *client,int index)
{
	int  i;
	u8 flag;
	//int 
	 u16 address;
	int ret;

	// select bank 0
	ov8825_i2c_write_reg(client,0x3d84, 0x08);
	
	// read otp into buffer
	ov8825_i2c_write_reg(client,0x3d81, 0x01);
	
       mdelay(1);
	// read flag
	address = 0x3d05 + index*9;
	//flag = 
	#if 1
	printk("check_otp_wb   address=%x\n",address);
	#endif
	ret = ov8825_i2c_read_reg(client,address,&flag);
	#if 1
	printk("check_otp_wb  ret=%d\n",ret);
	printk("check_otp_wb  flag=%d\n",flag);
	#endif
	// disable otp read
	ov8825_i2c_write_reg(client,0x3d81, 0x00);

	// clear otp buffer
	for (i=0;i<32;i++) {
		ov8825_i2c_write_reg(client,0x3d00 + i, 0x00);
	}

	if (!flag) {
		printk("check_otp_wb  if (!flag) \n");
		return 0;
	}
	else if ((!(flag & 0x80)) && (flag & 0x7f)) {
		printk("check_otp_wb  else if ((!(flag & 0x80)) && (flag & 0x7f))\n");
		return 2;
	}
	else {
		printk("check_otp_wb  else \n");
		return 1;
	}
}
int read_otp_wb_m(struct i2c_client *client,int index, struct otp_test_struct * otp_ptr)
{
	int i;
	int address;

	// select bank 0
	ov8825_i2c_write_reg(client,0x3d84, 0x08);

	// read otp into buffer
	ov8825_i2c_write_reg(client,0x3d81, 0x01);

	address = 0x3d05 + index*9;
	  ov8825_i2c_read_reg(client,address,&(*otp_ptr).module_integrator_id);
	  printk("shanying:module id read_reg %x\n",(*otp_ptr).module_integrator_id);
	  module_id = (*otp_ptr).module_integrator_id;

	  
	/*  ov8825_i2c_read_reg(NULL,address + 1,&(*otp_ptr).lens_id);
	 ov8825_i2c_read_reg(NULL,address + 2,&(*otp_ptr).rg_ratio);
	ov8825_i2c_read_reg(NULL,address + 3,&(*otp_ptr).bg_ratio);
	 ov8825_i2c_read_reg(NULL,address + 4,&(*otp_ptr).user_data[0]);
	 ov8825_i2c_read_reg(NULL,address + 5,&(*otp_ptr).user_data[1] );
	 ov8825_i2c_read_reg(NULL,address + 6,&(*otp_ptr).user_data[2]);
	ov8825_i2c_read_reg(NULL,address + 7,&(*otp_ptr).light_rg);
	ov8825_i2c_read_reg(NULL,address + 8,&(*otp_ptr).light_bg);*/
	
	// disable otp read
	ov8825_i2c_write_reg(client,0x3d81, 0x00);

	// clear otp buffer
	for (i=0;i<32;i++) {
		ov8825_i2c_write_reg(client,0x3d00 + i, 0x00);
	}

	return 0;	
}

int update_otp_wb_m(struct i2c_client *client)
{
	struct otp_test_struct current_otp;
	int i;
	int otp_index;
	int temp;
	int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	int rg,bg;


	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	for(i=0;i<3;i++) {
		temp = check_otp_wb_m(client,i);
		if (temp == 2) {
			otp_index = i;
			break;
		}
	}

	if (i==3) {
		// no valid wb OTP data
		printk(" no valid wb OTP data\n");
		return 1;
	}

	read_otp_wb_m(client,otp_index, &current_otp);
}

static ssize_t sensor_proc_write(struct file *filp,
				    const char *buff, size_t len,
				    loff_t * off)
{
   return 0;
}

static ssize_t sensor_proc_read(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	u8 camera_id = 0;
	int len = 0;
	camera_id = module_id;
	switch(camera_id)
	{
	case 0x31:
		printk("shanying:mcnex!");
		snprintf(page, 5,"mcne");
		len = 5;
		break;
	case 0x07:
		printk("shanying:qtec!");
		snprintf(page, 5,"qtec");
		len = 6;
		break;
	default:
		printk("shanying:err!");
		snprintf(page ,4,"err");
		len = 3;
		break;
	}
	return len;
}



void create_camera_module_proc_file(void)
{
	int err;
       printk("shanying:create_camera_module_proc_file\n");
	sensor_proc_file =
	    create_proc_entry(SENSOR_PROC_FILE, 0666, NULL);
	#if 0
	if (sensor_proc_file) {
		printk("shanying:sensor_proc_file enter\n");
		sensor_proc_file->proc_fops = &sensor_proc_ops;
	} 
	#endif

	if (sensor_proc_file) {
		printk("shanying:create_camera_module_proc_file!");
		sensor_proc_file->read_proc = sensor_proc_read;
		sensor_proc_file->write_proc = NULL;
	}


	
	else	{
		printk("proc file create failed!");
	}
	 
}

extern int tegra_camera_clk_onoff(int on);

static int ov8825_i2c_probe(struct i2c_client *client,
            const struct i2c_device_id *id)
{
	int err;
	u8 reg1 = 0;
	u8 reg2 = 0;
	uint16_t sensor_id = 0; 
	
	printk("%s: probing sensor.\n", __func__);

	info = kzalloc(sizeof(struct ov8825_i2c_info), GFP_KERNEL);
	if (!info) {
		pr_err("ov8825: Unable to allocate memory!\n");
		return -ENOMEM;
	}

	info->pdata = client->dev.platform_data;
	info->i2c_client = client;
    /* 0503 */
    if (info->pdata && info->pdata->power_on)
        {
        printk("8825 poweron\n ");
        info->pdata->power_on();

		msleep(100);
		tegra_camera_clk_onoff(1);
		
		msleep(100);
        }
	ov8825_i2c_read_reg(info->i2c_client, 0x300a, &reg1);
	printk("%s ov5640_read_reg 1%x\n", __func__, reg1);
	ov8825_i2c_read_reg(info->i2c_client, 0x300b, &reg2);
	printk("%s ov5640_read_reg 2 %x\n", __func__, reg2);
	sensor_id=(reg1<<8)|reg2;
	printk("shanying:sensor_id %x\n",	sensor_id);


	write_table(info->i2c_client, mode_init, NULL, 0);

    ov8825_i2c_write_reg(info->i2c_client, 0x0100, 0x01); 
	/* otp */

	msleep(100);

	update_otp_wb_m(info->i2c_client);

	create_camera_module_proc_file();
    if (info->pdata && info->pdata->power_off)
        {
		printk("8825 poweroff\n ");
		info->pdata->power_off();
		tegra_camera_clk_onoff(0);
        }
	i2c_set_clientdata(client, info);
	return 0;
}

static int ov8825_i2c_remove(struct i2c_client *client)
{
printk("OV8825 %s\n", __func__);
	struct ov8825_i2c_info *info;
	info = i2c_get_clientdata(client);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ov8825_i2c_id[] = {
	{ "ov8825-i2c", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, ov8825_i2c_id);

static struct i2c_driver ov8825_i2c_driver = {
	.driver = {
		.name = "ov8825-i2c",
		.owner = THIS_MODULE,
	},
	.probe = ov8825_i2c_probe,
	.remove = ov8825_i2c_remove,
	.id_table = ov8825_i2c_id,
};

static struct miscdevice ov8825_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ov8825-i2c",
	.fops = NULL,
};//

static int __init ov8825_i2c_init(void)
{
printk("OV8825 %s\n", __func__);
	misc_register(&ov8825_device);
       printk("after misc_register(&ov8825_device)\n");
	return i2c_add_driver(&ov8825_i2c_driver);
}

static void __exit ov8825_i2c_exit(void)
{
printk("OV8825 %s\n", __func__);
	i2c_del_driver(&ov8825_i2c_driver);
}

module_init(ov8825_i2c_init);
module_exit(ov8825_i2c_exit);

