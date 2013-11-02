/* 
 * drivers/input/touchscreen/ft5x0x_ts.c
 *
 * FocalTech ft5x0x TouchScreen driver. 
 *
 * Copyright (c) 2010  Focal tech Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * VERSION          DATE            AUTHOR        Note
 *    1.0         2010-01-05            WenFS    only support mulititouch   Wenfs 2010-10-01
 *    2.0          2011-09-05                   Duxx      Add touch key, and project setting update, auto CLB command
 *    3.0         2011-09-09            Luowj   
 *
 */

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/ft5x06_ts.h>
#include <linux/earlysuspend.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>   
#include <mach/irqs.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/module.h>

#include <linux/syscalls.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/input/ft5x06_ex_fun.h>


static struct i2c_client *this_client;

#define CONFIG_FT5X0X_MULTITOUCH 1
#if 0
struct ts_event {
    u16 au16_x[CFG_MAX_TOUCH_POINTS];              //x coordinate
    u16 au16_y[CFG_MAX_TOUCH_POINTS];              //y coordinate
    u8  au8_touch_event[CFG_MAX_TOUCH_POINTS];     //touch event:  0 -- down; 1-- contact; 2 -- contact
    u8  au8_finger_id[CFG_MAX_TOUCH_POINTS];       //touch ID
    u16 pressure;
    u8  touch_point;
};

struct ft5x0x_ts_data {
    struct input_dev    *input_dev;
    struct ts_event     event;
    struct work_struct  pen_event_work;
    struct workqueue_struct *ts_workqueue;
//  struct early_suspend    early_suspend;
//  struct mutex device_mode_mutex;   /* Ensures that only one function can specify the Device Mode at a time. */
};
#endif
static int suspend_flag_for_usb = 0;
static int suspend_flag = 1;
static int probe_success_flag = 0;
static atomic_t irq_enabled;
#if POLLING_OR_INTERRUPT
static void ft5x0x_polling(unsigned long data);
static struct timer_list test_timer;
#define POLLING_CYCLE               10
#define POLLING_CHECK_TOUCH         0x01
#define POLLING_CHECK_NOTOUCH       0x00
#endif
static int debug = 0;
module_param(debug, int, 0600);
#if CFG_SUPPORT_TOUCH_KEY
int tsp_keycodes[CFG_NUMOFKEYS] ={
        KEY_MENU,
        //KEY_HOME,
        KEY_HOMEPAGE,
        KEY_BACK,
        KEY_SEARCH
};
char *tsp_keyname[CFG_NUMOFKEYS] ={
        "Menu",
        "Home",
        "Back",
        "Search"
};
static bool tsp_keystatus[CFG_NUMOFKEYS];
#endif
#ifdef FTS_FW_DOWNLOAD
#define FTS_PACKET_LENGTH           128
#define FT5x0x_TX_NUM               28
#define FT5x0x_RX_NUM               16
static unsigned char CTPM_FW[]=
{

    #include <linux/input/ft_app.i>
};
#endif
/*virtual key support begin*/
static ssize_t tsp_vkeys_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{

	/*key_type : key_value : center_x : cener_y : area_width : area_height*/
	return sprintf(buf,
	__stringify(EV_KEY) ":" __stringify(KEY_MENU) ":70:900:100:200"
	":" __stringify(EV_KEY) ":" __stringify(KEY_HOMEPAGE) ":193:900:100:200"
	":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":309:900:100:200"
	":" __stringify(EV_KEY) ":" __stringify(KEY_SEARCH) ":415:900:100:200"
	"\n");
}
static struct kobj_attribute tsp_vkeys_attr = {
	.attr = {
		.mode = S_IRUGO,
	},
	.show = &tsp_vkeys_show,
};

static struct attribute *tsp_properties_attrs[] = {
	&tsp_vkeys_attr.attr,
	NULL
};

static struct attribute_group tsp_properties_attr_group = {
	.attrs = tsp_properties_attrs,
};
 
struct kobject *kobj_tsp;
static void ts_key_report_tsp_init(void)
{
	int rc = -EINVAL;

	/* virtual keys */
	tsp_vkeys_attr.attr.name = "virtualkeys.Ft5x_dev";
	kobj_tsp = kobject_create_and_add("board_properties",
				NULL);
	if (kobj_tsp)
		rc = sysfs_create_group(kobj_tsp,
			&tsp_properties_attr_group);
	if (!kobj_tsp || rc)
		pr_err("%s: failed to create board_properties\n",
				__func__);
}
/*virtual key support end*/
/***********************************************************************************************
Name    :   ft5x0x_i2c_rxdata 

Input   :   *rxdata
                     *length

Output  :   ret

function    :   

***********************************************************************************************/
int ft5x0x_i2c_Read(char * writebuf, int writelen, char *readbuf, int readlen)
{
    int ret;

    if(writelen > 0)
    {
        struct i2c_msg msgs[] = {
            {
                .addr   = this_client->addr,
                .flags  = 0,
                .len    = writelen,
                .buf    = writebuf,
            },
            {
                .addr   = this_client->addr,
                .flags  = I2C_M_RD,
                .len    = readlen,
                .buf    = readbuf,
            },
        };
        ret = i2c_transfer(this_client->adapter, msgs, 2);
        if (ret < 0)
            DbgPrintk("[FTS] msg %s i2c read error: %d\n", __func__, ret);
    }
    else
    {
        struct i2c_msg msgs[] = {
            {
                .addr   = this_client->addr,
                .flags  = I2C_M_RD,
                .len    = readlen,
                .buf    = readbuf,
            },
        };
        ret = i2c_transfer(this_client->adapter, msgs, 1);
        if (ret < 0)
            DbgPrintk("[FTS] msg %s i2c read error: %d\n", __func__, ret);
    }
    return ret;
}EXPORT_SYMBOL(ft5x0x_i2c_Read);
/***********************************************************************************************
Name    :    ft5x0x_i2c_Write

Input   :   
                     

Output  :0-write success    
        other-error code    
function    :   write data by i2c 

***********************************************************************************************/
int ft5x0x_i2c_Write(char *writebuf, int writelen)
{
    int ret;

    struct i2c_msg msg[] = {
        {
            .addr   = this_client->addr,
            .flags  = 0,
            .len    = writelen,
            .buf    = writebuf,
        },
    };

    ret = i2c_transfer(this_client->adapter, msg, 1);
    if (ret < 0)
        DbgPrintk("[FTS] %s i2c write error: %d\n", __func__, ret);

    return ret;
}EXPORT_SYMBOL(ft5x0x_i2c_Write);
#ifdef FTS_FW_DOWNLOAD
void delay_qt_ms(unsigned long  w_ms)
{
    unsigned long i;
    unsigned long j;

    for (i = 0; i < w_ms; i++)
    {
        for (j = 0; j < 1000; j++)
        {
            udelay(1);
        }
    }
}
int ft5x0x_write_reg(unsigned char regaddr, unsigned char regvalue)
{
    unsigned char buf[2];
    buf[0] = regaddr;
    buf[1] = regvalue;
    return ft5x0x_i2c_Write(buf, sizeof(buf));
}

int ft5x0x_read_reg(unsigned char regaddr, unsigned char * regvalue)
{
    return ft5x0x_i2c_Read(&regaddr, 1, regvalue, 1);
}
int fts_ctpm_auto_clb(void)
{
    unsigned char uc_temp;
    unsigned char i ;

    printk("[FTS] start auto CLB.\n");
    msleep(200);
    ft5x0x_write_reg(0, 0x40);  
    delay_qt_ms(100);                       //make sure already enter factory mode
    ft5x0x_write_reg(2, 0x4);               //write command to start calibration
    delay_qt_ms(300);
    for(i=0;i<100;i++)
    {
        ft5x0x_read_reg(0,&uc_temp);
        if ( ((uc_temp&0x70)>>4) == 0x0)    //return to normal mode, calibration finish
        {
            break;
        }
        delay_qt_ms(200);
        printk("[FTS] waiting calibration %d\n",i);
    }
    
    printk("[FTS] calibration OK.\n");
    
    msleep(300);
    ft5x0x_write_reg(0, 0x40);              //goto factory mode
    delay_qt_ms(100);                       //make sure already enter factory mode
    ft5x0x_write_reg(2, 0x5);               //store CLB result
    delay_qt_ms(300);
    ft5x0x_write_reg(0, 0x0);               //return to normal mode 
    msleep(300);
    printk("[FTS] store CLB result OK.\n");
    return 0;
}
unsigned char fts_ctpm_get_i_file_ver(void)
{
    unsigned int ui_sz;
    ui_sz = sizeof(CTPM_FW);
    if (ui_sz > 2)
    {
        return CTPM_FW[ui_sz - 2];
    }
    else
    {
        //TBD, error handling?
        return 0xff; //default value
    }
}
E_UPGRADE_ERR_TYPE  fts_ctpm_fw_upgrade(FTS_BYTE* pbt_buf, FTS_DWRD dw_lenth)
{
    
        FTS_BYTE reg_val[2] = {0};
        FTS_DWRD i = 0;

        FTS_DWRD  packet_number;
        FTS_DWRD  j;
    FTS_DWRD  temp;
        FTS_DWRD  lenght;
        FTS_BYTE  packet_buf[FTS_PACKET_LENGTH + 6];
        FTS_BYTE  auc_i2c_write_buf[10];
        FTS_BYTE bt_ecc;
        int      i_ret;

        /*********Step 1:Reset  CTPM *****/
        /*write 0xaa to register 0xfc*/
    ft5x0x_write_reg(0xfc,0xaa);
        delay_qt_ms(50);
         /*write 0x55 to register 0xfc*/
        ft5x0x_write_reg(0xfc,0x55);
        printk("[FTS] Step 1: Reset CTPM test\n");
   
        delay_qt_ms(30);   


        /*********Step 2:Enter upgrade mode *****/
        auc_i2c_write_buf[0] = 0x55;
        auc_i2c_write_buf[1] = 0xaa;
        do
        {
            i ++;
            i_ret = ft5x0x_i2c_Write(auc_i2c_write_buf, 2);
            delay_qt_ms(5);
        }while(i_ret <= 0 && i < 5 );

        /*********Step 3:check READ-ID***********************/        
    auc_i2c_write_buf[0] = 0x90; 
    auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
    //ft5x0x_i2c_Write(auc_i2c_write_buf, 4);

        ft5x0x_i2c_Read(auc_i2c_write_buf, 4, reg_val, 2);
        if (reg_val[0] == 0x79 && reg_val[1] == 0x3)
        {
            printk("[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
        }
        else
        {
            return ERR_READID;
        }
    auc_i2c_write_buf[0] = 0xcd;
    ft5x0x_i2c_Read(auc_i2c_write_buf, 1, reg_val, 1);
    printk("[FTS] bootloader version = 0x%x\n", reg_val[0]);

        /*********Step 4:erase app and panel paramenter area ********************/
    auc_i2c_write_buf[0] = 0x61;
    ft5x0x_i2c_Write(auc_i2c_write_buf, 1); //erase app area    
        delay_qt_ms(1500); 

    auc_i2c_write_buf[0] = 0x63;
    ft5x0x_i2c_Write(auc_i2c_write_buf, 1); //erase panel parameter area
        delay_qt_ms(100);
        printk("[FTS] Step 4: erase. \n");

    /*********Step 5:write firmware(FW) to ctpm flash*********/
    bt_ecc = 0;
    printk("[FTS] Step 5: start upgrade. \n");
    dw_lenth = dw_lenth - 8;
    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
    packet_buf[0] = 0xbf;
    packet_buf[1] = 0x00;
    for (j=0;j<packet_number;j++)
    {
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(lenght>>8);
        packet_buf[5] = (FTS_BYTE)lenght;

        for (i=0;i<FTS_PACKET_LENGTH;i++)
        {
            packet_buf[6+i] = pbt_buf[j*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }

        ft5x0x_i2c_Write(packet_buf, FTS_PACKET_LENGTH+6);
        delay_qt_ms(FTS_PACKET_LENGTH/6 + 1);
        if ((j * FTS_PACKET_LENGTH % 1024) == 0)
        {
              printk("[FTS] upgrade the 0x%x th byte.\n", ((unsigned int)j) * FTS_PACKET_LENGTH);
        }
    }

    if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
    {
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;

        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;

        for (i=0;i<temp;i++)
        {
            packet_buf[6+i] = pbt_buf[ packet_number*FTS_PACKET_LENGTH + i]; 
            bt_ecc ^= packet_buf[6+i];
        }
  
        ft5x0x_i2c_Write(packet_buf, temp+6);
        delay_qt_ms(20);
    }

    //send the last six byte
    for (i = 0; i<6; i++)
    {
        temp = 0x6ffa + i;
        packet_buf[2] = (FTS_BYTE)(temp>>8);
        packet_buf[3] = (FTS_BYTE)temp;
        temp =1;
        packet_buf[4] = (FTS_BYTE)(temp>>8);
        packet_buf[5] = (FTS_BYTE)temp;
        packet_buf[6] = pbt_buf[ dw_lenth + i]; 
        bt_ecc ^= packet_buf[6];
  
        ft5x0x_i2c_Write(packet_buf, 7);
        delay_qt_ms(20);
    }

    /*********Step 6: read out checksum***********************/
    /*send the opration head*/
    auc_i2c_write_buf[0] = 0xcc;
    ft5x0x_i2c_Read(auc_i2c_write_buf, 1, reg_val, 1); 

    printk("[FTS] Step 6:  ecc read 0x%x, new firmware 0x%x. \n", reg_val[0], bt_ecc);
    if(reg_val[0] != bt_ecc)
    {
            return ERR_ECC;
    }

    /*********Step 7: reset the new FW***********************/
    auc_i2c_write_buf[0] = 0x07;
    ft5x0x_i2c_Write(auc_i2c_write_buf, 1);
    msleep(300);  //make sure CTP startup normally

    return ERR_OK;
}

int fts_ctpm_fw_upgrade_with_i_file(void)
{
   FTS_BYTE*     pbt_buf = NULL;
   int i_ret;
    
    //=========FW upgrade========================*/
   pbt_buf = CTPM_FW;
   /*call the upgrade function*/
   i_ret =  fts_ctpm_fw_upgrade(pbt_buf,sizeof(CTPM_FW));
   if (i_ret != 0)
   {
       printk("[FTS] upgrade failed i_ret = %d.\n", i_ret);
       //error handling ...
       //TBD
   }
   else
   {
       printk("[FTS] upgrade successfully.\n");
       fts_ctpm_auto_clb();  //start auto CLB
   }

   return i_ret;
}
int fts_ctpm_auto_upg(void)
{
    unsigned char uc_host_fm_ver=FT5x0x_REG_FW_VER;
    unsigned char uc_tp_fm_ver;
    int           i_ret;
    
    ft5x0x_read_reg(FT5x0x_REG_FW_VER, &uc_tp_fm_ver);
    uc_host_fm_ver = fts_ctpm_get_i_file_ver();
    if ( uc_tp_fm_ver == FT5x0x_REG_FW_VER  ||   //the firmware in touch panel maybe corrupted
         uc_tp_fm_ver < uc_host_fm_ver //the firmware in host flash is new, need upgrade
        )
    {
        msleep(100);
        printk("[FTS] uc_tp_fm_ver = 0x%x, uc_host_fm_ver = 0x%x\n",
                    uc_tp_fm_ver, uc_host_fm_ver);
        i_ret = fts_ctpm_fw_upgrade_with_i_file();    
        if (i_ret == 0)
        {
                msleep(300);
                uc_host_fm_ver = fts_ctpm_get_i_file_ver();
                printk("[FTS] upgrade to new version 0x%x\n", uc_host_fm_ver);
        }
        else
        {
                printk("[FTS] upgrade failed ret=%d.\n", i_ret);
        }
    }

    return 0;
}

#endif

/***********************************************************************************************
Name    :    

Input   :   
                     

Output  :   

function    :   

***********************************************************************************************/
static void ft5x0x_ts_release(void)
{
    struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
 /*   int i;
    for(i = 0; i <CFG_NUMOFKEYS; i++ )
    {
            if(tsp_keystatus[i])
            {
                  input_report_key(data->input_dev, tsp_keycodes[i], 0);
      
            //      DbgPrintk("[FTS] %s key is release. Keycode : %d\n", tsp_keyname[i], tsp_keycodes[i]);

                  tsp_keystatus[i] = KEY_RELEASE;      
            }
    }*/
    input_report_key(data->input_dev, BTN_TOUCH, 0);
    input_mt_sync(data->input_dev);
    input_sync(data->input_dev);
}


//read touch point information
static int ft5x0x_read_Touchdata(void)
{
    struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
    struct ts_event *event = &data->event;
    u8 buf[CFG_POINT_READ_BUF] = {0};
    int ret = -1;
    int i;

    ret = ft5x0x_i2c_Read(buf, 1, buf, CFG_POINT_READ_BUF);
    if (ret < 0) {
        DbgPrintk("[FTS] %s read_data i2c_rxdata failed: %d\n", __func__, ret);
        return ret;
    }
    memset(event, 0, sizeof(struct ts_event));
#if USE_EVENT_POINT
    event->touch_point = buf[2] & 0x07; 
#else
    event->touch_point = buf[2] >>4;
#endif
    if (event->touch_point > CFG_MAX_TOUCH_POINTS)
    {
        event->touch_point = CFG_MAX_TOUCH_POINTS;
    }
//    DbgPrintk("[FTS] %s event->touch_point: %d\n", __func__, event->touch_point);
    for (i = 0; i < event->touch_point; i++)
    {
        event->au16_x[i] = (u16)(buf[3 + 6*i] & 0x0F)<<8 | (u16)buf[4 + 6*i];
        event->au16_y[i] = (u16)(buf[5 + 6*i] & 0x0F)<<8 | (u16)buf[6 + 6*i];
        event->au8_touch_event[i] = buf[3 + 6*i] >> 6;
         event->pressure[i] = buf[7 + 6*i] ;     
        event->au8_finger_id[i]   = buf[5 + 6*i] >> 4;
    if(debug)
       DbgPrintk("[FTS] %s  %d %d %d %d .\n", __func__, event->au16_x[i], event->au16_y[i], event->au8_finger_id[i], event->pressure[i]);

    }

  

    return 0;
}

/***********************************************************************************************
Name    :    

Input   :   
                     

Output  :   

function    :   

***********************************************************************************************/


#if CFG_SUPPORT_TOUCH_KEY
int ft5x0x_touch_key_process(struct input_dev *dev, int x, int y, int touch_event)
{
    int i;
    int key_id;
    DbgPrintk("[FTS] %s x %d  y %d touch_event %d\n", __func__, x, y, touch_event);   
    if ( x < 70)
    {
        key_id = 0;
    }
    else if ( x > 160 && x < 210)
    {
        key_id = 1;
    }
    
    else if ( x > 280 && x < 360)
    {
        key_id = 2;
    }  
    else if (x > 410)
    {
        key_id = 3;
    }
    else
    {
        key_id = 0xf;
    }
    
    for(i = 0; i <CFG_NUMOFKEYS; i++ )
    {
        if( key_id == i )
        {
            if( touch_event == 0)                                  // detect
            {

                if(!tsp_keystatus[i])
                {
                    input_report_key(dev, tsp_keycodes[i], 1);
                    DbgPrintk( "[FTS] %s key is pressed. Keycode : %d\n", tsp_keyname[i], tsp_keycodes[i]);
                    tsp_keystatus[i] = KEY_PRESS;
               }
            }
        }
    }
    return 0;
    
}    
#endif
static int ft5x0x_oldx = 0, ft5x0x_oldy = 0, ft5x0x_oldid = 0;
static void ft5x0x_report_value(void)
{
    struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
    struct ts_event *event = &data->event;
    int i;
    //   DbgPrintk("[FTS] ft5x0x_report_value event->touch_point %d.\n", event->touch_point);
    input_report_key(data->input_dev, BTN_TOUCH, 1);

    for (i  = 0; i < event->touch_point; i++)
    {
        // LCD view area
        //   DbgPrintk("[FTS] ft5x0x_report_value %d %d %d %d .\n", event->au16_x[i], event->au16_y[i], event->au8_finger_id[i], event->pressure);
   //     if (event->au16_x[i] < SCREEN_MAX_X && event->au16_y[i] < SCREEN_MAX_Y)
        {
            input_report_abs(data->input_dev, ABS_X, event->au16_x[i]);
            input_report_abs(data->input_dev, ABS_Y, event->au16_y[i]);
            input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->au16_x[i]);
            input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->au16_y[i]);
            input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, 1);
            input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, event->au8_finger_id[i]);



            if (event->au8_touch_event[i]== 0 || event->au8_touch_event[i] == 2)
            {
                input_report_abs(data->input_dev, ABS_MT_PRESSURE, event->pressure[i]);
            }
            else
            {
                input_report_abs(data->input_dev, ABS_MT_PRESSURE, 0);
            }
        }
 /*       else //maybe the touch key area
        {
#if CFG_SUPPORT_TOUCH_KEY
            if (event->au16_y[i] >= (SCREEN_MAX_Y+20))
            {
                ft5x0x_touch_key_process(data->input_dev, event->au16_x[i], event->au16_y[i], event->au8_touch_event[i]);
            }
#endif
        }*/
        input_mt_sync(data->input_dev);
    }
    ft5x0x_oldx = event->au16_x[0];
    ft5x0x_oldy = event->au16_y[0];
    ft5x0x_oldid = event->au8_finger_id[0];
    input_sync(data->input_dev);
    if (event->touch_point == 0) {
        ft5x0x_ts_release();
    }
}   /*end ft5x0x_report_value*/


/***********************************************************************************************
Name    :    

Input   :   
                     

Output  :   

function    :   

***********************************************************************************************/
static void ft5x0x_ts_pen_irq_work(struct work_struct *work)
{

    int ret = -1;
   if(suspend_flag)
    {
        ret = ft5x0x_read_Touchdata();
        suspend_flag = 0;
    }
   else
   {
        struct ft5x0x_ts_data *data = i2c_get_clientdata(this_client);
        struct ts_event *event = &data->event;
    ret = ft5x0x_read_Touchdata();  
        if(ft5x0x_oldx == event->au16_x[0] && ft5x0x_oldy == event->au16_y[0] && ft5x0x_oldid == event->au8_finger_id[0])
        {
        }
        else
        {

    if (ret == 0 && probe_success_flag==1) { 
        ft5x0x_report_value();
    }
        }

   }
#if POLLING_OR_INTERRUPT
    del_timer(&test_timer);
    add_timer(&test_timer);
#else
	if (atomic_cmpxchg(&irq_enabled, 0, 1) == 0)
		enable_irq(this_client->irq);
#endif

}

#if POLLING_OR_INTERRUPT
static void ft5x0x_polling(unsigned long data)
{
    struct ft5x0x_ts_data *ft5x0x_ts = i2c_get_clientdata(this_client);

    if (!work_pending(&ft5x0x_ts->pen_event_work)) {
        queue_work(ft5x0x_ts->ts_workqueue, &ft5x0x_ts->pen_event_work);
    }
}
#endif
/***********************************************************************************************
Name    :    

Input   :   
                     

Output  :   

function    :   

***********************************************************************************************/
static irqreturn_t ft5x0x_ts_interrupt(int irq, void *dev_id)
{
    struct ft5x0x_ts_data *ft5x0x_ts = dev_id;

  	//printk("[FTS] ts ft5x0x_ts_interrupt\n");
	if (atomic_cmpxchg(&irq_enabled, 1, 0) == 1)
		disable_irq_nosync(this_client->irq);
    if (!work_pending(&ft5x0x_ts->pen_event_work)) {
        queue_work(ft5x0x_ts->ts_workqueue, &ft5x0x_ts->pen_event_work);
    }

    return IRQ_HANDLED;
}
static struct mutex ft5x0x_ts_lock;
static int ft5x0x_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
        unsigned char uc_reg_value; 
        unsigned char uc_reg_addr; 
        unsigned char  i2c_write_buf[10]; 

    	printk("[FTS] ts suspend\n");
	mutex_lock(&ft5x0x_ts_lock);
      suspend_flag_for_usb = 1;
      if (atomic_cmpxchg(&irq_enabled, 1, 0) == 1)
     	disable_irq(client->irq); 
     i2c_write_buf[0] = FT5x0x_REG_PW_MODE;
      i2c_write_buf[1] = 3;
	ft5x0x_i2c_Write(i2c_write_buf, 2);    
      uc_reg_addr = FT5x0x_REG_PW_MODE;
        ft5x0x_i2c_Read(&uc_reg_addr, 1, &uc_reg_value, 1);
     DbgPrintk("[FTS] suspend touch PWR_MODE is %d.\n", uc_reg_value);
      suspend_flag= 1;
	mutex_unlock(&ft5x0x_ts_lock);
	return 0;
}
extern int  ft_touch_wake(void);
extern int  ft_touch_reset(void);
static int ft5x0x_ts_resume(struct i2c_client *client)
{
        unsigned char uc_reg_value; 
        unsigned char uc_reg_addr; 
 //       unsigned char  i2c_write_buf[10]; 
    	printk("[FTS] ts resume\n");
	mutex_lock(&ft5x0x_ts_lock);
       ft_touch_wake();     
       uc_reg_addr = FT5x0x_REG_PW_MODE;
        ft5x0x_i2c_Read(&uc_reg_addr, 1, &uc_reg_value, 1);
        DbgPrintk("[FTS] resume touch PWR_MODE is %d.\n", uc_reg_value);   
        if(uc_reg_value != 0)
        {
            ft_touch_reset();
         ft5x0x_i2c_Read(&uc_reg_addr, 1, &uc_reg_value, 1);
        DbgPrintk("[FTS] resume touch read status after reset PWR_MODE is %d.\n", uc_reg_value);   
       }
        ft5x0x_ts_release();
		if (atomic_cmpxchg(&irq_enabled, 0, 1) == 0)
			enable_irq(client->irq);  //or remove this
        suspend_flag_for_usb = 0;
	mutex_unlock(&ft5x0x_ts_lock);

      	return 0;
}
static void ft5x0x_ts_early_suspend(struct early_suspend *h)
{
	struct ft5x0x_ts_data *ft5x0x_ts;
       int ret;
    	printk("[FTS] ts ft5x0x_ts_early_suspend\n");
	ft5x0x_ts = container_of(h, struct ft5x0x_ts_data, early_suspend);
    	ret = cancel_work_sync(&ft5x0x_ts->pen_event_work);
    	printk("[FTS] ts ft5x0x_ts_early_suspend cancel_work_sync ret %d\n", ret);

	ft5x0x_ts_suspend(ft5x0x_ts->client, PMSG_SUSPEND);
}

static void ft5x0x_ts_later_resume(struct early_suspend *h)
{
	struct ft5x0x_ts_data *ft5x0x_ts;
    	printk("[FTS] ts ft5x0x_ts_later_resume\n");
	ft5x0x_ts = container_of(h, struct ft5x0x_ts_data, early_suspend);

	ft5x0x_ts_resume(ft5x0x_ts->client);

      queue_work(ft5x0x_ts->ts_workqueue, &ft5x0x_ts->pen_event_work);

}
extern int prop_add( struct device *dev, char *item, char *value);

/*zte lipeng10094834 add for usb detect for ft5x06 at 2012629*/
extern int tps80031_get_vbus_status(void);
static int g_usbstate = 0;
static int old_usbstate = 0;

struct timer_list			ft5x0x_timer;
struct work_struct              ft5x0x_timer_work;
struct workqueue_struct     *ft5x0x_timer_workqueue;
#define ENABLE_FT5x0x_TIMEOUT (msecs_to_jiffies(3000))
static void ft5x0x_timer_work_fun(struct work_struct *work)
{
    unsigned char  i2c_write_buf[10];
    unsigned char uc_reg_value;
    unsigned char uc_reg_addr;

    if(g_usbstate != old_usbstate && suspend_flag_for_usb==0)
    {
         i2c_write_buf[0] = 0x86;
        i2c_write_buf[1] = g_usbstate;
        ft5x0x_i2c_Write(i2c_write_buf, 2);
        uc_reg_addr = 0x86;
        ft5x0x_i2c_Read(&uc_reg_addr, 1, &uc_reg_value, 1);
        //DbgPrintk("[FTS] ft5x0x_timer_work_fun 0x86 is %d.\n", uc_reg_value);

        old_usbstate = g_usbstate;
    }
    mod_timer(&ft5x0x_timer, jiffies + ENABLE_FT5x0x_TIMEOUT);
}

void ft5x0x_usb_detect(void)
{
     if(tps80031_get_vbus_status() != 0)
        g_usbstate = 3;
     else
        g_usbstate = 1;

     DbgPrintk("[FTS] ft5x0x_usb_detect g_usbstate %d old_usbstate %d\n", g_usbstate, old_usbstate);
}

static void ft5x0x_timer_fun(unsigned long handle)
{
    if (!work_pending(&ft5x0x_timer_work)) {
        queue_work(ft5x0x_timer_workqueue, &ft5x0x_timer_work);
    }
    return;
}
/*zte lipeng10094834 add for usb detect for ft5x06 at 2012629 end*/

/***********************************************************************************************
Name    :    

Input   :   
                     

Output  :   

function    :   

***********************************************************************************************/
static int 
ft5x0x_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct ft5x0x_ts_data *ft5x0x_ts;
    struct input_dev *input_dev;
    int err = 0;
    unsigned char  i2c_write_buf[10];
    unsigned char uc_reg_value; 
    unsigned char uc_reg_addr;
    char versionbuf[5];
#if CFG_SUPPORT_TOUCH_KEY
        int i;
#endif
      struct Ft5x06_ts_platform_data *pdata = pdata = client->dev.platform_data;

     	if (pdata->init_platform_hw)
		pdata->init_platform_hw();   
    DbgPrintk("[FTS] ft5x0x_ts_probe, driver version is %s.\n", CFG_FTS_CTP_DRIVER_VERSION);

    
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        err = -ENODEV;
        goto exit_check_functionality_failed;
    }

    ft5x0x_ts = kzalloc(sizeof(struct ft5x0x_ts_data), GFP_KERNEL);

    if (!ft5x0x_ts) {
        err = -ENOMEM;
        goto exit_alloc_data_failed;
    }

    this_client = client;
    ft5x0x_ts->client = client;
    i2c_set_clientdata(client, ft5x0x_ts);
    this_client->irq = client->irq;
    DbgPrintk("[FTS] INT irq=%d\n", client->irq);
//  mutex_init(&ft5x0x_ts->device_mode_mutex);
    INIT_WORK(&ft5x0x_ts->pen_event_work, ft5x0x_ts_pen_irq_work);

    ft5x0x_ts->ts_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
    if (!ft5x0x_ts->ts_workqueue) {
        err = -ESRCH;
        goto exit_create_singlethread;
    }
#if POLLING_OR_INTERRUPT
    DbgPrintk("[FTS] Read TouchData by Polling\n");
#else
    err = request_irq(this_client->irq, ft5x0x_ts_interrupt, IRQF_TRIGGER_FALLING, "ft5x0x_ts", ft5x0x_ts);
    if (err < 0) {
        dev_err(&client->dev, "ft5x0x_probe: request irq failed\n");
        goto exit_irq_request_failed;
    }
    disable_irq(this_client->irq);
#endif
    input_dev = input_allocate_device();
    if (!input_dev) {
        err = -ENOMEM;
        dev_err(&client->dev, "failed to allocate input device\n");
        goto exit_input_dev_alloc_failed;
    }
    
    ft5x0x_ts->input_dev = input_dev;

    __set_bit(EV_ABS, input_dev->evbit);
    __set_bit(EV_SYN, input_dev->evbit);
    __set_bit(EV_KEY, input_dev->evbit);
    __set_bit(BTN_TOUCH, input_dev->keybit);
    __set_bit(BTN_2, input_dev->keybit);
    input_dev->mscbit[0] = BIT_MASK(MSC_GESTURE);
    /* x-axis acceleration */
    input_set_abs_params(input_dev, ABS_X, 0, SCREEN_MAX_X, 0, 0);
    /* y-axis acceleration */
    input_set_abs_params(input_dev, ABS_Y,  0, SCREEN_MAX_Y, 0, 0);
    input_set_abs_params(input_dev,
            ABS_MT_POSITION_X,  0, SCREEN_MAX_X, 0, 0);
    input_set_abs_params(input_dev,
    ABS_MT_POSITION_Y,  0, SCREEN_MAX_Y, 0, 0);
    input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);   
    input_set_abs_params(input_dev,
            ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);
    input_set_abs_params(input_dev,
            ABS_MT_TRACKING_ID, 0, 5, 0, 0);

#if CFG_SUPPORT_TOUCH_KEY
    //setup key code area
    input_dev->keycode = tsp_keycodes;
    for(i = 0; i < CFG_NUMOFKEYS; i++)
    {
        input_set_capability(input_dev, EV_KEY, ((int*)input_dev->keycode)[i]);
        tsp_keystatus[i] = KEY_RELEASE;
    }
#endif
    input_dev->name     = "Ft5x_dev";      //dev_name(&client->dev)
    err = input_register_device(input_dev);
    if (err) {
        dev_err(&client->dev,
                "ft5x0x_ts_probe: failed to register input device: %s\n",
                dev_name(&client->dev));
        goto exit_input_register_device_failed;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    DbgPrintk("[FTS] ==register_early_suspend =\n");
    ft5x0x_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    ft5x0x_ts->early_suspend.suspend = ft5x0x_ts_early_suspend;
    ft5x0x_ts->early_suspend.resume   = ft5x0x_ts_later_resume;
    register_early_suspend(&ft5x0x_ts->early_suspend);
#endif
     mutex_init(&ft5x0x_ts_lock);

     msleep(150);  //make sure CTP already finish startup process
     #ifdef FTS_FW_DOWNLOAD
  //   fts_ctpm_auto_upg();   
  #endif
    //get some register information
    uc_reg_addr = FT5x0x_REG_FW_VER;
    ft5x0x_i2c_Read(&uc_reg_addr, 1, &uc_reg_value, 1);
    DbgPrintk("[FTS] Firmware version = 0x%x\n", uc_reg_value);
    sprintf(versionbuf, "0x%x", uc_reg_value);
    prop_add(&input_dev->dev, "fw_version", versionbuf);


    i2c_write_buf[0] = FT5x0x_REG_POINT_RATE;
    i2c_write_buf[1] = 14;
    ft5x0x_i2c_Write(i2c_write_buf, 2);     
    uc_reg_addr = FT5x0x_REG_POINT_RATE;
    ft5x0x_i2c_Read(&uc_reg_addr, 1, &uc_reg_value, 1);
    DbgPrintk("[FTS] report rate is %dHz.\n", uc_reg_value * 10);

    uc_reg_addr = FT5X0X_REG_THGROUP;
    ft5x0x_i2c_Read(&uc_reg_addr, 1, &uc_reg_value, 1);
     DbgPrintk("[FTS] touch threshold is %d.\n", uc_reg_value * 4);
     
    uc_reg_addr = FT5x0x_REG_PW_MODE;
    ft5x0x_i2c_Read(&uc_reg_addr, 1, &uc_reg_value, 1);
     DbgPrintk("[FTS] touch PWR_MODE is %d.\n", uc_reg_value);
     #if POLLING_OR_INTERRUPT
    test_timer.function = ft5x06_polling;
    test_timer.expires = jiffies + HZ*2;//POLLING_CYCLE*100; // 100/10
    test_timer.data = 1;
    init_timer(&test_timer);
    add_timer(&test_timer);
#else
    enable_irq(this_client->irq);
	atomic_set(&irq_enabled, 1);
#endif
    ts_key_report_tsp_init();
    //you can add sysfs for test
    //ft5x0x_create_sysfs(client);
    probe_success_flag= 1;
    //  mutex_init(&ft5x0x_ts->device_mode_mutex);
/*zte lipeng10094834 add for usb detect for ft5x06 at 2012629*/
    INIT_WORK(&ft5x0x_timer_work, ft5x0x_timer_work_fun);
    ft5x0x_timer_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
    if (!ft5x0x_timer_workqueue) {
       return -1;
    }
    setup_timer(&ft5x0x_timer, ft5x0x_timer_fun, (unsigned long)this_client);
    mod_timer(&ft5x0x_timer, jiffies + ENABLE_FT5x0x_TIMEOUT);
/*zte lipeng10094834 add for usb detect for ft5x06 at 2012629 end*/
    DbgPrintk("[FTS] ==probe over =\n");
    return 0;

exit_input_register_device_failed:
    input_free_device(input_dev);
    
exit_input_dev_alloc_failed:
    free_irq(this_client->irq, ft5x0x_ts);
    
exit_irq_request_failed:
    cancel_work_sync(&ft5x0x_ts->pen_event_work);
    destroy_workqueue(ft5x0x_ts->ts_workqueue);
    
exit_create_singlethread:
    DbgPrintk("[FTS] ==singlethread error =\n");
    i2c_set_clientdata(client, NULL);
    kfree(ft5x0x_ts);
    
exit_alloc_data_failed:
exit_check_functionality_failed:
    return err;
}
/***********************************************************************************************
Name    :    

Input   :   
                     

Output  :   

function    :   

***********************************************************************************************/
static int __devexit ft5x0x_ts_remove(struct i2c_client *client)
{
    struct ft5x0x_ts_data *ft5x0x_ts;
    DbgPrintk("[FTS] ==ft5x0x_ts_remove=\n");
    ft5x0x_ts = i2c_get_clientdata(client);
    //unregister_early_suspend(&ft5x0x_ts->early_suspend);
    //mutex_destroy(&ft5x0x_ts->device_mode_mutex); 
    input_unregister_device(ft5x0x_ts->input_dev);
    kfree(ft5x0x_ts);
    cancel_work_sync(&ft5x0x_ts->pen_event_work);
    destroy_workqueue(ft5x0x_ts->ts_workqueue);
    i2c_set_clientdata(client, NULL); 
#if POLLING_OR_INTERRUPT
    del_timer(&test_timer);
#else
    free_irq(client->irq, ft5x0x_ts);
#endif

    return 0;
}

static const struct i2c_device_id ft5x0x_ts_id[] = {
    { FT5X0X_NAME, 0 },{ }
};


MODULE_DEVICE_TABLE(i2c, ft5x0x_ts_id);

static struct i2c_driver ft5x0x_ts_driver = {
    .probe      = ft5x0x_ts_probe,
    .remove     = __devexit_p(ft5x0x_ts_remove),
       .id_table   = ft5x0x_ts_id,
    .driver = {
        .name   = FT5X0X_NAME,
        .owner  = THIS_MODULE,
    },
};

/***********************************************************************************************
Name    :    

Input   :   
                     

Output  :   

function    :   

***********************************************************************************************/
static int __init ft5x0x_ts_init(void)
{
    int ret;
    DbgPrintk("[FTS] ==ft5x0x_ts_init==\n");
    ret = i2c_add_driver(&ft5x0x_ts_driver);
    DbgPrintk("[FTS] ret=%d\n",ret);
    return ret;
}

/***********************************************************************************************
Name    :    

Input   :   
                     

Output  :   

function    :   

***********************************************************************************************/
static void __exit ft5x0x_ts_exit(void)
{
    DbgPrintk("[FTS] ==ft5x0x_ts_exit==\n");
    i2c_del_driver(&ft5x0x_ts_driver);
}

module_init(ft5x0x_ts_init);
module_exit(ft5x0x_ts_exit);

MODULE_AUTHOR("<luowj@Focaltech-systems.com>");
MODULE_DESCRIPTION("FocalTech ft5x0x TouchScreen driver");
MODULE_LICENSE("GPL");
