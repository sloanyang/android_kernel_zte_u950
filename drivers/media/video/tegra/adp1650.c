/*
 * adp1650.c - adp1650 flash/torch kernel driver
 *
 * Copyright (C) 2011 NVIDIA Corp.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */


#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <media/adp1650.h>

#include <mach/gpio.h>
#include "../../../../arch/arm/mach-tegra/gpio-names.h"
#include <linux/proc_fs.h>

#define ADP1650_I2C_REG_AMP		0x00
#define ADP1650_I2C_REG_TMR                 0x02
#define ADP1650_I2C_REG_CUR                0x03
#define ADP1650_I2C_REG_FLASH               0x04
/* ZTE: add by yaoling for flash reg val level set 20110902 ++ */
/* ZTE: modified by yangchunni to change flash current and timeout reg values 20120908 ++ */
/*
#define FLASH_TIME_LEVEL_ONE   0x0f
#define FLASH_TIME_LEVEL_TWO   0x0e
#define FLASH_TIME_LEVEL_THREE   0x0d

#define FLASH_CURRENT_LEVEL_ONE  0x69
#define FLASH_CURRENT_LEVEL_TWO 0x59
#define FLASH_CURRENT_LEVEL_THREE  0x49
*/
#define FLASH_TIME_LEVEL_ONE   0x08
#define FLASH_TIME_LEVEL_TWO   0x07
#define FLASH_TIME_LEVEL_THREE   0x06

#define FLASH_CURRENT_LEVEL_ONE  0x77 //0x59   //0x51
#define FLASH_CURRENT_LEVEL_TWO 0x67   //0x41
#define FLASH_CURRENT_LEVEL_THREE  0x57 //0x31
/* ZTE: modified by yangchunni to change flash current and timeout reg values 20120908 ++ */
/* ZTE: add by yaoling for flash reg val level set 20110902 ++ */
static struct proc_dir_entry *adp_proc_file;
#define ADP_PROC_FILE "driver/adp1650"
#define CAM_FLASH_EN TEGRA_GPIO_PBB3
#define CAM_IO_1V8_EN TEGRA_GPIO_PM2

enum {
	
	ADP1650_GPIO_EN,
	ADP1650_GPIO_STRB,
	ADP1650_GPIO_TORCH,
};

struct adp1650_info {
	struct i2c_client *i2c_client;
	struct adp1650_platform_data *pdata;
};

static struct adp1650_info *info;

struct timer_list			adp1650_timer;
#define ENABLE_ADP1650_TIMEOUT (msecs_to_jiffies(1100))
static int adp1650_gpio(u8 gpio, u8 val)
{
	int prev_val;

	switch (gpio) {

	case ADP1650_GPIO_EN:
		if (info->pdata && info->pdata->gpio_en) {
			info->pdata->gpio_en(val);
			return 0;
		}
		return -1;

	case ADP1650_GPIO_STRB:
		if (info->pdata && info->pdata->gpio_strb) {
			info->pdata->gpio_strb(val);
			return 0;
		}
        /* ZTE: add by yaoling for adp1650 20110711 ++ */
        case ADP1650_GPIO_TORCH:
        if (info->pdata && info->pdata->gpio_tor) {
        info->pdata->gpio_tor(val);
        return 0;
        }
          /* ZTE: add by yaoling for adp1650 20110711 -- */
	default:
		return -1;
	}
}

static int adp1650_get_reg(u8 addr, u8 *val)
{
	struct i2c_client *client = info->i2c_client;
	struct i2c_msg msg[2];
	unsigned char data[2];

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = data;

	data[0] = (u8) (addr);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data + 1;

	*val = 0;

	if (i2c_transfer(client->adapter, msg, 2) == 2) {
		*val = data[1];
		return 0;
	} else {
		return -1;
	}
}

static int adp1650_set_reg(u8 addr, u8 val)
{
	struct i2c_client *client = info->i2c_client;
	struct i2c_msg msg;
	unsigned char data[2];

	data[0] = (u8) (addr);
	data[1] = (u8) (val);
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = data;

	if (i2c_transfer(client->adapter, &msg, 1) == 1)
		return 0;
	else
		return -1;
}
#ifdef  CONFIG_VIDEO_OV5640
extern u8 ov5640_get_expstatus();
#endif
static long adp1650_ioctl(
	struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	u8 val = (u8)arg;
	u8 reg;
        u8  time_reg, current_reg ;
      #ifdef  CONFIG_VIDEO_OV5640
	static u8 expstatus = 0;
     #endif
      printk("adp1650_ioctl cmd=%d\n",cmd);
	switch (cmd) {
	
	case ADP1650_IOCTL_MODE_SHUTDOWN:
		 printk("ADP1650_IOCTL_MODE_SHUTDOWN\n");
		#ifdef  CONFIG_VIDEO_OV5640
		//adp1650_gpio(ADP1650_GPIO_ACT, 1);       
	//	adp1650_gpio(ADP1650_GPIO_TORCH, 0); 
   
		  mod_timer(&adp1650_timer, jiffies + msecs_to_jiffies(500));
             #endif
	     #ifdef CONFIG_VIDEO_OV8825
              //adp1650_gpio(ADP1650_GPIO_ACT, 1); 
                adp1650_gpio(ADP1650_GPIO_TORCH, 0); 	
             #endif
	      return 0;
	case ADP1650_IOCTL_MODE_STANDBY:
              printk("ADP1650_IOCTL_MODE_STANDBY\n");
			return 0;
			
	case ADP1650_IOCTL_MODE_PREFLASH:
		#ifdef CONFIG_VIDEO_OV5640
		printk("ADP1650_IOCTL_MODE_PREFLASH\n");
		expstatus = ov5640_get_expstatus(); 		
		mod_timer(&adp1650_timer, jiffies + msecs_to_jiffies(4000));
		if(expstatus > 0x20)
			adp1650_set_reg(ADP1650_I2C_REG_CUR, 0x01);
		else if(expstatus > 0x10)
			adp1650_set_reg(ADP1650_I2C_REG_CUR, 0x03);
		else if(expstatus > 0x5)
			adp1650_set_reg(ADP1650_I2C_REG_CUR, 0x05);
		else 
			adp1650_set_reg(ADP1650_I2C_REG_CUR, 0x07);
		adp1650_set_reg(ADP1650_I2C_REG_FLASH, 0x08);            
		adp1650_set_reg(ADP1650_I2C_REG_TMR,0x1f);  // set gpio 1 as gpio mode        
		adp1650_gpio(ADP1650_GPIO_TORCH, 1);  // hardware tor to high

		#endif
		break;
     
/* Amp limit for torch, flash, and LED is controlled by external circuitry in
 * GPIO mode.  In I2C mode amp limit is controlled by chip registers and the
 * limit values are in the board-sensors file.
 */
	case ADP1650_IOTCL_MODE_TORCH:
		//adp1650_gpio(ADP1650_GPIO_ACT, 1);

		  //printk("ADP1650_IOTCL_MODE_TORCH %d g_expstatus %d expstatus%d \n", val, g_expstatus, expstatus);
            #ifdef  CONFIG_VIDEO_OV5640
             if(1==val)
            {
                adp1650_set_reg(ADP1650_I2C_REG_FLASH, 0x08);            
                adp1650_set_reg(ADP1650_I2C_REG_TMR,0x1f);  // set gpio 1 as gpio mode        
                adp1650_gpio(ADP1650_GPIO_TORCH, 1);  // hardware tor to high
            }
             else
            {
            	//  g_expstatus = 0;
            	  adp1650_gpio(ADP1650_GPIO_TORCH, 0); 
                adp1650_set_reg(ADP1650_I2C_REG_FLASH, 0x00);            
                adp1650_set_reg(ADP1650_I2C_REG_TMR,0x0f);          
                 // hardware tor to high
            }
        #endif
        #ifdef CONFIG_VIDEO_OV8825
           if (0!=val)
            {
			adp1650_get_reg(ADP1650_I2C_REG_FLASH,&reg); 
			reg = reg |0x08;
			adp1650_set_reg(ADP1650_I2C_REG_FLASH, reg); 
			adp1650_set_reg(ADP1650_I2C_REG_TMR,0x12);  // 300ms
			adp1650_gpio(ADP1650_GPIO_TORCH, 1);  // hardware tor to high
            }
           else
            {
			adp1650_gpio(ADP1650_GPIO_TORCH, 0);  // hardware tor to low
			adp1650_set_reg(ADP1650_I2C_REG_FLASH, 0x00);            
			adp1650_set_reg(ADP1650_I2C_REG_TMR,0x02);          
			adp1650_gpio(ADP1650_GPIO_TORCH, 0);  // hardware tor to low
            }
        #endif
            return 0;

	case ADP1650_IOCTL_MODE_FLASH:
                printk("ADP1650_IOCTL_MODE_FLASH val=%d\n",val);
               #ifdef CONFIG_VIDEO_OV8825 
			adp1650_gpio(ADP1650_GPIO_TORCH, 0);  
			adp1650_set_reg(ADP1650_I2C_REG_TMR,0x06);   
			adp1650_set_reg(ADP1650_I2C_REG_FLASH, 0x00); 
               if(val!=0)
                {
			/*adp1650_gpio(ADP1650_GPIO_TORCH, 0);  
			adp1650_set_reg(ADP1650_I2C_REG_TMR,0x06);   
			adp1650_set_reg(ADP1650_I2C_REG_FLASH, 0x00); */
			adp1650_set_reg(0x03,0x33); 
			adp1650_get_reg(ADP1650_I2C_REG_FLASH,&reg); 
			//reg = reg |0x2f;
			reg = reg |0x0f;
			adp1650_set_reg(ADP1650_I2C_REG_FLASH,reg); 
			/*adp1650_get_reg(ADP1650_I2C_REG_FLASH,&reg); 	 
			printk("ADP1650_IOCTL_MODE_FLASH04register  reg=%d\n",reg);
			adp1650_get_reg(ADP1650_I2C_REG_CUR,&reg); 	
			printk("ADP1650_IOCTL_MODE_FLASH03register  reg=%d\n",reg);
			adp1650_get_reg(0x05,&reg); 	
			printk("ADP1650_IOCTL_MODE_FLASH05register  reg=%d\n",reg);*/
                }
               else
                {
                    adp1650_set_reg(ADP1650_I2C_REG_FLASH,0x00);
                }
              #endif
              #ifdef  CONFIG_VIDEO_OV5640
              if(val!=0)
                {
                     adp1650_set_reg(ADP1650_I2C_REG_FLASH,0xbb); 
                }
               else
                {
                    adp1650_set_reg(ADP1650_I2C_REG_FLASH,0x00);
                }
              #endif
                return 0;

	case ADP1650_IOCTL_MODE_LED:
               printk("ADP1650_IOCTL_MODE_LED\n");
               return 0;
	case ADP1650_IOCTL_STRB:
            printk("ADP1650_IOCTL_STRB\n");
          
             pr_info("ADP1650_IOCTL_STRB val=%d\n",val);
             
           #ifdef  CONFIG_VIDEO_OV5640
            {
                pr_info(" if (!info->pdata->config & 0x01) val=%d\n",val); 
                if(val == 0x01 ||val == 0x02||val == 0x03||val == 0x04)
                {
                    time_reg = FLASH_TIME_LEVEL_ONE;
                    current_reg = FLASH_CURRENT_LEVEL_ONE;
                }
                else if(val == 0x05 ||val == 0x06||val == 0x07||val == 0x08||val ==0x09)
                {
                    time_reg = FLASH_TIME_LEVEL_TWO;
                    current_reg = FLASH_CURRENT_LEVEL_TWO;
                }
                else
                {
                    time_reg = FLASH_TIME_LEVEL_THREE;
                    current_reg = FLASH_CURRENT_LEVEL_THREE; 
                }
                pr_info(" ADP1650_IOCTL_STRB time_reg=%x,current_reg=%x \n",time_reg,current_reg); 
                adp1650_set_reg(ADP1650_I2C_REG_TMR, time_reg);
                adp1650_set_reg(ADP1650_I2C_REG_CUR, current_reg);
			
            }
	
            #endif
            #ifdef CONFIG_VIDEO_OV8825
            if(val!=0)
                {
                  
                    adp1650_gpio(ADP1650_GPIO_STRB, 1); 
                }
            else
                {
                    adp1650_gpio(ADP1650_GPIO_STRB, 0); 
                }
            #endif
           return 0;//

	case ADP1650_IOCTL_TIMER:
              printk("ADP1650_IOCTL_TIMER=%d\n",val);
            #ifdef  CONFIG_VIDEO_OV5640
            {
                pr_info(" if (!info->pdata->config & 0x01) val=%d\n",val); 
                if(val == 0x0 || val == 0x01 ||val == 0x02||val == 0x03||val == 0x04)
                {
		 	time_reg = FLASH_TIME_LEVEL_ONE;
                  }
                else if(val == 0x05 ||val == 0x06||val == 0x07||val == 0x08||val ==0x09)
                {
                    time_reg = FLASH_TIME_LEVEL_TWO;
                }
                else
                {
                    time_reg = FLASH_TIME_LEVEL_THREE; 
                }
                pr_info(" ADP1650_IOCTL_STRB time_reg=%x,current_reg=%x \n",time_reg,current_reg); 
                adp1650_set_reg(ADP1650_I2C_REG_TMR, time_reg);
                
            }
             #endif
             #ifdef CONFIG_VIDEO_OV8825
             adp1650_get_reg(ADP1650_I2C_REG_TMR,&reg);
             reg = val & reg;
             adp1650_set_reg(ADP1650_I2C_REG_TMR, reg);
             #endif
		return 0;
       case ADP1650_IOCTL_CURRENT:
              printk("ADP1650_IOCTL_CURRENT=%d\n",val);
            #ifdef  CONFIG_VIDEO_OV5640
            {
            pr_info(" if (!info->pdata->config & 0x01) val=%d\n",val); 
            if(val == 0x00  || val == 0x01 ||val == 0x02||val == 0x03||val == 0x04)
            {
	      	current_reg = FLASH_CURRENT_LEVEL_ONE;
            }
            else if(val == 0x05 ||val == 0x06||val == 0x07||val == 0x08||val ==0x09)
            {
                current_reg = FLASH_CURRENT_LEVEL_ONE;
            }
            else
            {
                current_reg = FLASH_CURRENT_LEVEL_THREE; 
            }
            pr_info(" ADP1650_IOCTL_STRB time_reg=%x,current_reg=%x \n",time_reg,current_reg); 

                adp1650_set_reg(ADP1650_I2C_REG_CUR, current_reg);	
            }

            #endif
             #ifdef CONFIG_VIDEO_OV8825
              adp1650_get_reg(ADP1650_I2C_REG_CUR, &reg);	
              reg = (val<<3) & reg ;
               adp1650_set_reg(ADP1650_I2C_REG_CUR,reg);	
             #endif
		return 0;

	default:
		return -1;
	}
}


static int adp1650_open(struct inode *inode, struct file *file)
{
	int err;
	//u8 reg;
	file->private_data = info;

	printk("%s\n", __func__);
	if (info->pdata && info->pdata->init) {
		err = info->pdata->init();
		if (err)
			pr_err("adp1650_open: Board init failed\n");
	}
	
	return 0;
}

int adp1650_release(struct inode *inode, struct file *file)
{
	if (info->pdata && info->pdata->exit)
		info->pdata->exit();
	file->private_data = NULL;
	return 0;
}
int flash_adp1650_init(void)
{
    gpio_set_value(CAM_FLASH_EN, 0);
    mdelay(5);
    gpio_set_value(CAM_FLASH_EN, 1);
    return 0;
}

void flash_adp1650_exit(void)
{
    gpio_set_value(CAM_FLASH_EN, 0);
    return 0;  
}

static ssize_t adp1650_proc_write(struct file *filp,
				    const char *buff, size_t len,
				    loff_t * off)
{
	int err;
	if(strncmp(buff, "FlashOpen", 9) == 0)
	{
		printk("the order is %s...\n", buff);
       	err = adp1650_set_reg(ADP1650_I2C_REG_FLASH,0xBB); 
		if (-1 == err)
			printk("adp1650 set reg failed!");
	}
	else if(strncmp(buff, "FlashOff", 8) == 0)
	{
		printk("the order is %s...\n", buff);
       	adp1650_set_reg(ADP1650_I2C_REG_FLASH,0x00);
		flash_adp1650_exit();
		gpio_set_value(CAM_IO_1V8_EN, 0);
	}
	else
	{
		printk("wrong order...\n");
	}
	return len;
}

static ssize_t adp1650_proc_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	
	return 0;
}

static struct file_operations adp1650_proc_ops = 
{
	.write = adp1650_proc_write,
	.read = adp1650_proc_read,
};

void create_adp1650_proc_file(void)
{
	int err;
       printk("create_adp1650_proc_file\n");
	adp_proc_file =
	    create_proc_entry(ADP_PROC_FILE, 0660, NULL);
	if (adp_proc_file) {
		adp_proc_file->proc_fops = &adp1650_proc_ops;
	} 
	else	{
		printk("proc file create failed!");
	}
	err = info->pdata->init();
		if (err)
			pr_err("adp1650_open: Board init failed\n");

	/* gpio_set_value(CAM_IO_1V8_EN, 1);
	 mdelay(5);
	 flash_adp1650_init(); */
	 
}

static void remove_adp1650_proc_file(void)
{
	remove_proc_entry(ADP_PROC_FILE, NULL);
}


static const struct file_operations adp1650_fileops = {
	.owner = THIS_MODULE,
	.open = adp1650_open,
	.unlocked_ioctl = adp1650_ioctl,
	.release = adp1650_release,
};

static struct miscdevice adp1650_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "adp1650",
	.fops = &adp1650_fileops,
};
static void adp1650_timer_fun(unsigned long handle)
{
	printk("adp1650_timer_fun\n");
	adp1650_gpio(ADP1650_GPIO_TORCH, 0); 
	return;
}
static int adp1650_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int err;
    printk("adp1650_probe");
	info = kzalloc(sizeof(struct adp1650_info), GFP_KERNEL);
	if (!info) {
		pr_err("adp1650: Unable to allocate memory!\n");
		return -ENOMEM;
	}
	err = misc_register(&adp1650_device);
	if (err) {
		pr_err("adp1650: Unable to register misc device!\n");
		kfree(info);
		return err;
	}
	info->pdata = client->dev.platform_data;
	info->i2c_client = client;
	i2c_set_clientdata(client, info);
    	setup_timer(&adp1650_timer, adp1650_timer_fun, (unsigned long)client);	
	create_adp1650_proc_file();
	return 0;
}

static int adp1650_remove(struct i2c_client *client)
{
	info = i2c_get_clientdata(client);
	misc_deregister(&adp1650_device);
	kfree(info);
	return 0;
}

static const struct i2c_device_id adp1650_id[] = {
	{ "adp1650", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, adp1650_id);

static struct i2c_driver adp1650_i2c_driver = {
	.driver = {
		.name = "adp1650",
		.owner = THIS_MODULE,
	},
	.probe = adp1650_probe,
	.remove = adp1650_remove,
	.id_table = adp1650_id,
};

static int __init adp1650_init(void)
{
	return i2c_add_driver(&adp1650_i2c_driver);
}

static void __exit adp1650_exit(void)
{
	i2c_del_driver(&adp1650_i2c_driver);
}

module_init(adp1650_init);
module_exit(adp1650_exit);

