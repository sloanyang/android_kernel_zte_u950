/*
 * mt9m114.c - mt9m114 sensor driver
 *
 * Copyright (c) 2011, NVIDIA, All Rights Reserved.
 *
 * Contributors:
 *      erik lilliebjerg <elilliebjerg@nvidia.com>
 *
 * Leverage OV5650.c
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <media/mt9m114.h>

#define MT9M114_PRINT 0
enum mt9m114_width {
	WORD_LEN,
	BYTE_LEN
};

struct mt9m114_reg {
	u16 addr;
	u16 val;
       enum mt9m114_width width;
};

struct mt9m114_info {
	int mode;
	struct i2c_client *i2c_client;
	struct mt9m114_platform_data *pdata;
};

#define MT9M114_TABLE_WAIT_MS 0
#define MT9M114_TABLE_END 1
#define MT9M114_MAX_RETRIES 3
#define SENSOR_WIDTH_REG      0x2703
#define SENSOR_640_WIDTH_VAL  0x280
#define SENSOR_720_WIDTH_VAL  0x500
#define SENSOR_1600_WIDTH_VAL 0x640
static struct mt9m114_reg  mt9m114_brightness_level1[] = {

	{0x098E, 0xC87A ,WORD_LEN},
	{0xC87A, 0x2B ,	BYTE_LEN},
	{MT9M114_TABLE_END, 0x0000}

};
static  struct mt9m114_reg  mt9m114_brightness_level2[] = {
    
	{0x098E, 0xC87A ,WORD_LEN},
	{0xC87A, 0x37 ,	BYTE_LEN},
	{MT9M114_TABLE_END, 0x0000}
     
};
static  struct mt9m114_reg  mt9m114_brightness_level3[] = {
	{0x098E, 0xC87A ,WORD_LEN},
	{0xC87A, 0x43 ,	BYTE_LEN},
	{MT9M114_TABLE_END, 0x0000}
};
static  struct mt9m114_reg  mt9m114_brightness_level4[] = {
	{0x098E, 0xC87A ,WORD_LEN},
	{0xC87A, 0x4F ,	BYTE_LEN},
	{MT9M114_TABLE_END, 0x0000}
     
};
static  struct mt9m114_reg   mt9m114_brightness_level5[] = {
  
	{0x098E, 0xC87A ,WORD_LEN},
	{0xC87A, 0x5B ,	BYTE_LEN},
	{MT9M114_TABLE_END, 0x0000}
};
static  struct mt9m114_reg   mt9m114_brightness_level6[] = {
	{0x098E, 0xC87A ,WORD_LEN},
	{0xC87A, 0x67 ,	BYTE_LEN},

	{MT9M114_TABLE_END, 0x0000}
};
static struct mt9m114_reg Whitebalance_Auto[] = {

	{0x098E, 0x0000 ,WORD_LEN},		// LOGICAL_ADDRESS_ACCESS  
	{0xC909, 0x03,BYTE_LEN}	, 	// CAM_AWB_AWBMODE           

	{MT9M114_TABLE_END, 0x00}
};
//白炽
static struct mt9m114_reg Whitebalance_Incandescent[] = {
	{0x098E, 0x0000 ,WORD_LEN},	// LOGICAL_ADDRESS_ACCESS       
	{0xC909, 0x01 ,BYTE_LEN}	,// CAM_AWB_AWBMODE               
	{0xC8F0, 0x0AF0 ,	WORD_LEN},// CAM_AWB_COLOR_TEMPERATURE    

	{MT9M114_TABLE_END, 0x00}
};
// 日光 
static struct mt9m114_reg Whitebalance_Daylight[] = {
	{0x098E, 0x0000 ,WORD_LEN},	// LOGICAL_ADDRESS_ACCESS        
	{0xC909, 0x01 ,BYTE_LEN}	,	// CAM_AWB_AWBMODE                 
	{0xC8F0, 0x113D,	WORD_LEN}, 	// CAM_AWB_COLOR_TEMPERATURE     

{MT9M114_TABLE_END, 0x00}
};
// 荧光
static struct mt9m114_reg Whitebalance_Fluorescent[] = {
	{0x098E, 0x0000 ,WORD_LEN},	// LOGICAL_ADDRESS_ACCESS     
	{0xC909, 0x01 ,BYTE_LEN}	,	// CAM_AWB_AWBMODE              
	{0xC8F0, 0x1964 ,	WORD_LEN}, 	// CAM_AWB_COLOR_TEMPERATURE  

	{MT9M114_TABLE_END, 0x00}
};

//阴天
static struct mt9m114_reg Whitebalance_Cloudy[]={

	{0x098E, 0x0000,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS      
	{0xC909, 0x01 ,BYTE_LEN}	,	// CAM_AWB_AWBMODE               
	{0xC8F0, 0x1770,	WORD_LEN}, 	 	// CAM_AWB_COLOR_TEMPERATURE   

	{MT9M114_TABLE_END, 0x00} 
};
/* contrast level register setting*/
	static struct mt9m114_reg mt9m114_contrast_level1[] = {

	{0x098E, 0x4940 ,WORD_LEN},	// LOGICAL_ADDRESS_ACCESS [CAM_LL_GAMMA] 
	{0xC940, 0x012C ,WORD_LEN},	// CAM_LL_GAMMA                          

     {MT9M114_TABLE_END, 0x00} 
    };

    static struct mt9m114_reg mt9m114_contrast_level2[] = {

	{0x098E, 0x4940,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS [CAM_LL_GAMMA] 
	{0xC940, 0x00FE,WORD_LEN}, 	// CAM_LL_GAMMA                          

     
       {MT9M114_TABLE_END, 0x00} 
    };

    static struct mt9m114_reg mt9m114_contrast_level3[] = {

	{0x098E, 0x4940,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS [CAM_LL_GAMMA] 
	{0xC940, 0x00DC,WORD_LEN}, 	// CAM_LL_GAMMA                          
	                   
	{MT9M114_TABLE_END, 0x00} 
    };

    static struct mt9m114_reg mt9m114_contrast_level4[] = {

	{0x098E, 0x4940,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS [CAM_LL_GAMMA] 
	{0xC940, 0x00B4,WORD_LEN}, 	// CAM_LL_GAMMA                          
	           
	{MT9M114_TABLE_END, 0x00} 
    };

    static struct mt9m114_reg  mt9m114_contrast_level5[] = {
        
	{0x098E, 0x4940 ,WORD_LEN},	// LOGICAL_ADDRESS_ACCESS [CAM_LL_GAMMA] 
	{0xC940, 0x008C ,WORD_LEN},	// CAM_LL_GAMMA                          
	               
	{MT9M114_TABLE_END, 0x00} 
    };

static struct mt9m114_reg ColorEffect_None[] = { 

	{0x098E, 0xC874,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL] 
	{0xC874, 0x00,BYTE_LEN}, 	// CAM_SFX_CONTROL                            
	{0xDC00, 0x28 ,BYTE_LEN},	// SYSMGR_NEXT_STATE                          
	{0x0080, 0x8004 ,WORD_LEN},	// COMMAND_REGISTER                         

	{MT9M114_TABLE_END, 0x00} 
};

static struct mt9m114_reg ColorEffect_Mono[] = {
      
	{0x098E, 0xC874 ,WORD_LEN},	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]  
	{0xC874, 0x01 ,BYTE_LEN},	// CAM_SFX_CONTROL                             
	{0xDC00, 0x28 ,BYTE_LEN},	// SYSMGR_NEXT_STATE                           
	{0x0080, 0x8004 ,WORD_LEN},	// COMMAND_REGISTER                          

	{MT9M114_TABLE_END, 0x00} 
};

static struct mt9m114_reg ColorEffect_Sepia[] = {
        {0x098E, 0xC876 ,WORD_LEN},	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_SEPIA_CB]   
	{0xC876, 0x23 ,BYTE_LEN},	// CAM_SFX_SEPIA_CR                              
	{0xC877, 0xB2 ,BYTE_LEN},	// CAM_SFX_SEPIA_CB     
	  
	//{0x098E, 0xC874 ,WORD_LEN},	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]
	{0xC874, 0x02,BYTE_LEN}, 	// CAM_SFX_CONTROL                           
	{0xDC00, 0x28 ,BYTE_LEN},	// SYSMGR_NEXT_STATE                         
	{0x0080, 0x8004,WORD_LEN}, 	// COMMAND_REGISTER                        

	{MT9M114_TABLE_END, 0x00} 
};

static struct mt9m114_reg ColorEffect_Negative[] = {
	{0x098E, 0xC874,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL] 
	{0xC874, 0x03,BYTE_LEN}, 	// CAM_SFX_CONTROL                            
	{0xDC00, 0x28 ,BYTE_LEN},	// SYSMGR_NEXT_STATE                          
	{0x0080, 0x8004,WORD_LEN}, 	// COMMAND_REGISTER                         

	{MT9M114_TABLE_END, 0x00} 
};

static struct mt9m114_reg ColorEffect_Bluish[] = {

	{0x098E, 0xC876 ,WORD_LEN},	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_SEPIA_CB]   
	{0xC876, 0xF1 ,BYTE_LEN},	// CAM_SFX_SEPIA_CR                              
	{0xC877, 0x2C ,BYTE_LEN},	// CAM_SFX_SEPIA_CB                              
	{0xC874, 0x02 ,BYTE_LEN},	// CAM_SFX_CONTROL                               
	{0xDC00, 0x28, BYTE_LEN},	// SYSMGR_NEXT_STATE                             
	{0x0080, 0x8004,WORD_LEN}, 	// COMMAND_REGISTER                            
                     
	{MT9M114_TABLE_END, 0x00} 
};
#if 0
static struct mt9m114_reg mode_1280x960[]=
{
    //DELAY=10 
    {MT9M114_TABLE_WAIT_MS, 10},
    {0x3C40, 0x003C,WORD_LEN },	// MIPI_CONTROL
    {0x301A, 0x0230,WORD_LEN },	// RESET_REGISTER 
    {0x098E, 0x1000,WORD_LEN },	// LOGICAL_ADDRESS_ACCESS 
    {0xC97E, 0x01 ,BYTE_LEN	},// CAM_SYSCTL_PLL_ENABLE 
    //REG= 0xC980, 0x0225 	
    // CAM_SYSCTL_PLL_DIVIDER_M_N 
    {0xC980, 0x0120,WORD_LEN}, 	// CAM_SYSCTL_PLL_DIVIDER_M_N 
    {0xC982, 0x0700,WORD_LEN}, 	// CAM_SYSCTL_PLL_DIVIDER_P //DELAY=10 
    {MT9M114_TABLE_WAIT_MS, 10}, 
    // select output size, YUV format 
    {0xC800, 0x0004,WORD_LEN},		//cam_sensor_cfg_y_addr_start = 4 
    {0xC802, 0x0004,WORD_LEN},		//cam_sensor_cfg_x_addr_start = 4
    {0xC804, 0x03CB,WORD_LEN},		//cam_sensor_cfg_y_addr_end = 971 
    {0xC806, 0x050B,WORD_LEN},		//cam_sensor_cfg_x_addr_end = 1291
   // {0xC808, 0x2DC6,WORD_LEN}, //C00		//cam_sensor_cfg_pixclk = 48000000
    {0xC808, 0x02DC, WORD_LEN},//C00		//cam_sensor_cfg_pixclk = 48000000
    {0xC80A, 0x6C00, WORD_LEN},//C00		//cam_sensor_cfg_pixclk = 48000000
    {0xC80C, 0x0001,WORD_LEN},		//cam_sensor_cfg_row_speed = 1 
    {0xC80E, 0x00DB,WORD_LEN},		//cam_sensor_cfg_fine_integ_time_min = 219
    {0xC810, 0x05C8,WORD_LEN},		//cam_sensor_cfg_fine_integ_time_max = 1480 
    {0xC812, 0x03EE,WORD_LEN},		//cam_sensor_cfg_frame_length_lines = 1006
    {0xC814, 0x064B,WORD_LEN},		//cam_sensor_cfg_line_length_pck = 1611 
    {0xC816, 0x0060,WORD_LEN},		//cam_sensor_cfg_fine_correction = 96 
    {0xC818, 0x03C3,WORD_LEN},		//cam_sensor_cfg_cpipe_last_row = 963 
    {0xC826, 0x0020,WORD_LEN},		//cam_sensor_cfg_reg_0_data = 32 
    {0xC834, 0x0001,WORD_LEN},		//cam_sensor_control_read_mode = 0 
    {0xC854, 0x0000,WORD_LEN},		//cam_crop_window_xoffset = 0 
    {0xC856, 0x0000,WORD_LEN},		//cam_crop_window_yoffset = 0 
    {0xC858, 0x0500,WORD_LEN},		//cam_crop_window_width = 1280 
    {0xC85A, 0x03C0,WORD_LEN},		//cam_crop_window_height = 960
    {0xC85C, 0x03,BYTE_LEN},		//cam_crop_cropmode = 3 
    {0xC868, 0x0500,WORD_LEN},		//cam_output_width = 1280 
    {0xC86A, 0x03C0,WORD_LEN},		//cam_output_height = 960
    {0xC878, 0x00,BYTE_LEN		},//cam_aet_aemode = 0 
    {0xC88C, 0x1D9E,WORD_LEN},		//cam_aet_max_frame_rate = 7582 
    {0xC88E, 0x1800,WORD_LEN},		//cam_aet_min_frame_rate = 6144 
    {0xC914, 0x0000,WORD_LEN},		//cam_stat_awb_clip_window_xstart = 0
    {0xC916, 0x0000,WORD_LEN},		//cam_stat_awb_clip_window_ystart = 0 
    {0xC918, 0x04FF,WORD_LEN},		//cam_stat_awb_clip_window_xend = 1279 
    {0xC91A, 0x03BF,WORD_LEN},		//cam_stat_awb_clip_window_yend = 959 
    {0xC91C, 0x0000,WORD_LEN},		//cam_stat_ae_initial_window_xstart = 0 
    {0xC91E, 0x0000,WORD_LEN},		//cam_stat_ae_initial_window_ystart = 0 
    {0xC920, 0x00FF,WORD_LEN},		//cam_stat_ae_initial_window_xend = 255
    {0xC922, 0x00BF,WORD_LEN},		//cam_stat_ae_initial_window_yend = 191
    //[Step3-Recommended] 
    {0x316A, 0x8270,WORD_LEN}, 	// DAC_TXLO_ROW 
    {0x316C, 0x8270,WORD_LEN}, 	// DAC_TXLO 
    {0x3ED0, 0x2305,WORD_LEN}, 	// DAC_LD_4_5
    {0x3ED2, 0x77CF,WORD_LEN}, 	// DAC_LD_6_7
    {0x316E, 0x8202,WORD_LEN}, 	// DAC_ECL 
    {0x3180, 0x87FF,WORD_LEN}, 	// DELTA_DK_CONTROL 
    {0x30D4, 0x6080,WORD_LEN}, 	// COLUMN_CORRECTION
    {0xA802, 0x0008,WORD_LEN}, 	// AE_TRACK_MODE
    {0x3E14, 0xFF39,WORD_LEN}, 	// SAMP_COL_PUP2
    //[patch 1204]for 720P 
    {0x0982, 0x0001,WORD_LEN}, 	// ACCESS_CTL_STAT 
    {0x098A, 0x60BC,WORD_LEN}, 	// PHYSICAL_ADDRESS_ACCESS
    {0xE0BC, 0xC0F1,WORD_LEN}, 
    {0xE0BE, 0x082A,WORD_LEN}, 
    {0xE0C0, 0x05A0,WORD_LEN}, 
    {0xE0C2, 0xD800,WORD_LEN}, 
    {0xE0C4, 0x71CF,WORD_LEN}, 
    {0xE0C6, 0xFFFF,WORD_LEN}, 
    {0xE0C8, 0xC344,WORD_LEN}, 
    {0xE0CA, 0x77CF,WORD_LEN}, 
    {0xE0CC, 0xFFFF,WORD_LEN}, 
    {0xE0CE, 0xC7C0,WORD_LEN}, 
    {0xE0D0, 0xB104,WORD_LEN}, 
    {0xE0D2, 0x8F1F,WORD_LEN}, 
    {0xE0D4, 0x75CF,WORD_LEN}, 
    {0xE0D6, 0xFFFF,WORD_LEN}, 
    {0xE0D8, 0xC84C,WORD_LEN},
    {0xE0DA, 0x0811,WORD_LEN}, 
    {0xE0DC, 0x005E,WORD_LEN}, 
    {0xE0DE, 0x70CF,WORD_LEN}, 
    {0xE0E0, 0x0000,WORD_LEN}, 
    {0xE0E2, 0x500E,WORD_LEN}, 
    {0xE0E4, 0x7840,WORD_LEN}, 
    {0xE0E6, 0xF019,WORD_LEN}, 
    {0xE0E8, 0x0CC6,WORD_LEN},
    {0xE0EA, 0x0340,WORD_LEN}, 
    {0xE0EC, 0x0E26,WORD_LEN}, 
    {0xE0EE, 0x0340,WORD_LEN}, 
    {0xE0F0, 0x95C2,WORD_LEN}, 
    {0xE0F2, 0x0E21,WORD_LEN}, 
    {0xE0F4, 0x101E,WORD_LEN},
    {0xE0F6, 0x0E0D,WORD_LEN},
    {0xE0F8, 0x119E,WORD_LEN}, 
    {0xE0FA, 0x0D56,WORD_LEN},
    {0xE0FC, 0x0340,WORD_LEN}, 
    {0xE0FE, 0xF008,WORD_LEN}, 
    {0xE100, 0x2650,WORD_LEN}, 
    {0xE102, 0x1040,WORD_LEN}, 
    {0xE104, 0x0AA2,WORD_LEN},
    {0xE106, 0x0360,WORD_LEN}, 
    {0xE108, 0xB502,WORD_LEN},
    {0xE10A, 0xB5C2,WORD_LEN},
    {0xE10C, 0x0B22,WORD_LEN}, 
    {0xE10E, 0x0400,WORD_LEN},
    {0xE110, 0x0CCE,WORD_LEN},
    {0xE112, 0x0320,WORD_LEN}, 
    {0xE114, 0xD800,WORD_LEN}, 
    {0xE116, 0x70CF,WORD_LEN}, 
    {0xE118, 0xFFFF,WORD_LEN}, 
    {0xE11A, 0xC5D4,WORD_LEN},
    {0xE11C, 0x902C,WORD_LEN}, 
    {0xE11E, 0x72CF,WORD_LEN}, 
    {0xE120, 0xFFFF,WORD_LEN},
    {0xE122, 0xE218,WORD_LEN},
    {0xE124, 0x9009,WORD_LEN},
    {0xE126, 0xE105,WORD_LEN}, 
    {0xE128, 0x73CF,WORD_LEN}, 
    {0xE12A, 0xFF00,WORD_LEN}, 
    {0xE12C, 0x2FD0,WORD_LEN}, 
    {0xE12E, 0x7822,WORD_LEN}, 
    {0xE130, 0x7910,WORD_LEN}, 
    {0xE132, 0xB202,WORD_LEN}, 
    {0xE134, 0x1382,WORD_LEN}, 
    {0xE136, 0x0700,WORD_LEN}, 
    {0xE138, 0x0815,WORD_LEN}, 
    {0xE13A, 0x03DE,WORD_LEN}, 
    {0xE13C, 0x1387,WORD_LEN}, 
    {0xE13E, 0x0700,WORD_LEN}, 
    {0xE140, 0x2102,WORD_LEN}, 
    {0xE142, 0x000A,WORD_LEN}, 
    {0xE144, 0x212F,WORD_LEN}, 
    {0xE146, 0x0288,WORD_LEN}, 
    {0xE148, 0x1A04,WORD_LEN}, 
    {0xE14A, 0x0284,WORD_LEN},
    {0xE14C, 0x13B9,WORD_LEN}, 
    {0xE14E, 0x0700,WORD_LEN}, 
    {0xE150, 0xB8C1,WORD_LEN}, 
    {0xE152, 0x0815,WORD_LEN}, 
    {0xE154, 0x0052,WORD_LEN}, 
    {0xE156, 0xDB00,WORD_LEN}, 
    {0xE158, 0x230F,WORD_LEN}, 
    {0xE15A, 0x0003,WORD_LEN},
    {0xE15C, 0x2102,WORD_LEN},
    {0xE15E, 0x00C0,WORD_LEN}, 
    {0xE160, 0x7910,WORD_LEN}, 
    {0xE162, 0xB202,WORD_LEN},
    {0xE164, 0x9507,WORD_LEN}, 
    {0xE166, 0x7822,WORD_LEN}, 
    {0xE168, 0xE080,WORD_LEN}, 
    {0xE16A, 0xD900,WORD_LEN},
    {0xE16C, 0x20CA,WORD_LEN}, 
    {0xE16E, 0x004B,WORD_LEN}, 
    {0xE170, 0xB805,WORD_LEN}, 
    {0xE172, 0x9533,WORD_LEN}, 
    {0xE174, 0x7815,WORD_LEN}, 
    {0xE176, 0x6038,WORD_LEN},
    {0xE178, 0x0FB2,WORD_LEN},
    {0xE17A, 0x0560,WORD_LEN},
    {0xE17C, 0xB861,WORD_LEN},
    {0xE17E, 0xB711,WORD_LEN},
    {0xE180, 0x0775,WORD_LEN}, 
    {0xE182, 0x0540,WORD_LEN}, 
    {0xE184, 0xD900,WORD_LEN},
    {0xE186, 0xF00A,WORD_LEN}, 
    {0xE188, 0x70CF,WORD_LEN},
    {0xE18A, 0xFFFF,WORD_LEN}, 
    {0xE18C, 0xE210,WORD_LEN},
    {0xE18E, 0x7835,WORD_LEN},
    {0xE190, 0x8041,WORD_LEN}, 
    {0xE192, 0x8000,WORD_LEN}, 
    {0xE194, 0xE102,WORD_LEN},
    {0xE196, 0xA040,WORD_LEN},
    {0xE198, 0x09F1,WORD_LEN}, 
    {0xE19A, 0x8094,WORD_LEN},
    {0xE19C, 0x7FE0,WORD_LEN},
    {0xE19E, 0xD800,WORD_LEN}, 
    {0xE1A0, 0xC0F1,WORD_LEN}, 
    {0xE1A2, 0xC5E1,WORD_LEN},
    {0xE1A4, 0x71CF,WORD_LEN}, 
    {0xE1A6, 0x0000,WORD_LEN}, 
    {0xE1A8, 0x45E6,WORD_LEN}, 
    {0xE1AA, 0x7960,WORD_LEN}, 
    {0xE1AC, 0x7508,WORD_LEN},
    {0xE1AE, 0x70CF,WORD_LEN},
    {0xE1B0, 0xFFFF,WORD_LEN}, 
    {0xE1B2, 0xC84C,WORD_LEN}, 
    {0xE1B4, 0x9002,WORD_LEN}, 
    {0xE1B6, 0x083D,WORD_LEN}, 
    {0xE1B8, 0x021E,WORD_LEN}, 
    {0xE1BA, 0x0D39,WORD_LEN},
    {0xE1BC, 0x10D1,WORD_LEN}, 
    {0xE1BE, 0x70CF,WORD_LEN}, 
    {0xE1C0, 0xFF00,WORD_LEN}, 
    {0xE1C2, 0x3354,WORD_LEN}, 
    {0xE1C4, 0x9055,WORD_LEN}, 
    {0xE1C6, 0x71CF,WORD_LEN}, 
    {0xE1C8, 0xFFFF,WORD_LEN}, 
    {0xE1CA, 0xC5D4,WORD_LEN}, 
    {0xE1CC, 0x116C,WORD_LEN}, 
    {0xE1CE, 0x0103,WORD_LEN}, 
    {0xE1D0, 0x1170,WORD_LEN}, 
    {0xE1D2, 0x00C1,WORD_LEN},
    {0xE1D4, 0xE381,WORD_LEN}, 
    {0xE1D6, 0x22C6,WORD_LEN},
    {0xE1D8, 0x0F81,WORD_LEN}, 
    {0xE1DA, 0x0000,WORD_LEN}, 
    {0xE1DC, 0x00FF,WORD_LEN}, 
    {0xE1DE, 0x22C4,WORD_LEN},
    {0xE1E0, 0x0F82,WORD_LEN}, 
    {0xE1E2, 0xFFFF,WORD_LEN}, 
    {0xE1E4, 0x00FF,WORD_LEN}, 
    {0xE1E6, 0x29C0,WORD_LEN}, 
    {0xE1E8, 0x0222,WORD_LEN}, 
    {0xE1EA, 0x7945,WORD_LEN}, 
    {0xE1EC, 0x7930,WORD_LEN}, 
    {0xE1EE, 0xB035,WORD_LEN}, 
    {0xE1F0, 0x0715,WORD_LEN},
    {0xE1F2, 0x0540,WORD_LEN}, 
    {0xE1F4, 0xD900,WORD_LEN},
    {0xE1F6, 0xF00A,WORD_LEN}, 
    {0xE1F8, 0x70CF,WORD_LEN}, 
    {0xE1FA, 0xFFFF,WORD_LEN}, 
    {0xE1FC, 0xE224,WORD_LEN},
    {0xE1FE, 0x7835,WORD_LEN}, 
    {0xE200, 0x8041,WORD_LEN}, 
    {0xE202, 0x8000,WORD_LEN}, 
    {0xE204, 0xE102,WORD_LEN}, 
    {0xE206, 0xA040,WORD_LEN}, 
    {0xE208, 0x09F1,WORD_LEN}, 
    {0xE20A, 0x8094,WORD_LEN}, 
    {0xE20C, 0x7FE0,WORD_LEN}, 
    {0xE20E, 0xD800,WORD_LEN}, 
    {0xE210, 0xFFFF,WORD_LEN},
    {0xE212, 0xCB40,WORD_LEN},
    {0xE214, 0xFFFF,WORD_LEN}, 
    {0xE216, 0xE0BC,WORD_LEN},
    {0xE218, 0x0000,WORD_LEN},
    {0xE21A, 0x0000,WORD_LEN},
    {0xE21C, 0x0000,WORD_LEN}, 
    {0xE21E, 0x0000,WORD_LEN}, 
    {0xE220, 0x0000,WORD_LEN},
    {0x098E, 0x0000,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS
    {0xE000, 0x1184,WORD_LEN}, 	// PATCHLDR_LOADER_ADDRESS
    {0xE002, 0x1204,WORD_LEN}, 	// PATCHLDR_PATCH_ID 
    {0xE004, 0x4103,WORD_LEN},   //0202 
    {0xE006, 0x0202,WORD_LEN}, 
    // PATCHLDR_FIRMWARE_ID //REG= 0xE004, 0x4103 //REG= 0xE006, 0x0202
    {0x0080, 0xFFF0,WORD_LEN}, 	
    // COMMAND_REGISTER //  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00
    //  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00
    //  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00 
    // 读Reg= 0x080， 判断其最低位是否为0， 如果不为0，则delay 5ms，然后继续读， // 直到为0或者   50ms以上
    //delay = 50 
    {MT9M114_TABLE_WAIT_MS, 50}, 
    {0x0080, 0xFFF1,WORD_LEN}, 	// COMMAND_REGISTER 
    //  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00 // 读Reg= 0x080， 判断其最低位是否为0， 如果不为0，则delay 5ms，然后继续读， // 直到为0或者   50ms以上
    //delay = 50 
    {MT9M114_TABLE_WAIT_MS, 50}, 
    // AWB Start point 
    {0x098E,0x2c12,WORD_LEN}, 
    {0xac12,0x008f,WORD_LEN}, 
    {0xac14,0x0105,WORD_LEN},
    //[Step4-APGA //LSC] // [APGA Settings 85% 2012/05/02 12:16:56] 
    {0x3640, 0x00B0,WORD_LEN}, 	//  P_G1_P0Q0 
    {0x3642, 0xC0EA,WORD_LEN}, 	//  P_G1_P0Q1
    {0x3644, 0x3E70,WORD_LEN}, 	//  P_G1_P0Q2
    {0x3646, 0x182B,WORD_LEN}, 	//  P_G1_P0Q3 
    {0x3648, 0xCC4D,WORD_LEN}, 	//  P_G1_P0Q4
    {0x364A, 0x0150,WORD_LEN}, 	//  P_R_P0Q0 
    {0x364C, 0x9C4A,WORD_LEN}, 	//  P_R_P0Q1 
    {0x364E, 0x6DF0,WORD_LEN}, 	//  P_R_P0Q2
    {0x3650, 0xE5CA,WORD_LEN}, 	//  P_R_P0Q3 
    {0x3652, 0xA22F,WORD_LEN}, 	//  P_R_P0Q4
    {0x3654, 0x0170,WORD_LEN}, 	//  P_B_P0Q0
    {0x3656, 0xC288,WORD_LEN}, 	//  P_B_P0Q1 
    {0x3658, 0x3010,WORD_LEN}, 	//  P_B_P0Q2
    {0x365A, 0xBA26,WORD_LEN}, 	//  P_B_P0Q3
    {0x365C, 0xE1AE,WORD_LEN}, 	//  P_B_P0Q4 
    {0x365E, 0x00B0,WORD_LEN}, 	//  P_G2_P0Q0 
    {0x3660, 0xC34A,WORD_LEN}, 	//  P_G2_P0Q1 
    {0x3662, 0x3E90,WORD_LEN}, 	//  P_G2_P0Q2 
    {0x3664, 0x1DAB,WORD_LEN}, 	//  P_G2_P0Q3 
    {0x3666, 0xD0CD,WORD_LEN}, 	//  P_G2_P0Q4 
    {0x3680, 0x440B,WORD_LEN}, 	//  P_G1_P1Q0 
    {0x3682, 0x1386,WORD_LEN}, 	//  P_G1_P1Q1
    {0x3684, 0xAB0C,WORD_LEN}, 	//  P_G1_P1Q2 
    {0x3686, 0x1FCD,WORD_LEN}, 	//  P_G1_P1Q3
    {0x3688, 0xEACB,WORD_LEN}, 	//  P_G1_P1Q4
    {0x368A, 0x63C7,WORD_LEN}, 	//  P_R_P1Q0 
    {0x368C, 0x1006,WORD_LEN}, 	//  P_R_P1Q1 
    {0x368E, 0xE64C,WORD_LEN}, 	//  P_R_P1Q2 
    {0x3690, 0x7B4D,WORD_LEN}, 	//  P_R_P1Q3
    {0x3692, 0x32AF,WORD_LEN}, 	//  P_R_P1Q4
    {0x3694, 0x24EB,WORD_LEN}, 	//  P_B_P1Q0
    {0x3696, 0xFB6A,WORD_LEN}, 	//  P_B_P1Q1 
    {0x3698, 0xB96C,WORD_LEN}, 	//  P_B_P1Q2 
    {0x369A, 0x9AEA,WORD_LEN}, 	//  P_B_P1Q3 
    {0x369C, 0x092E,WORD_LEN}, 	//  P_B_P1Q4
    {0x369E, 0x432B,WORD_LEN}, 	//  P_G2_P1Q0 
    {0x36A0, 0xC7C5,WORD_LEN}, 	//  P_G2_P1Q1
    {0x36A2, 0xABEC,WORD_LEN}, 	//  P_G2_P1Q2 
    {0x36A4, 0x222D,WORD_LEN}, 	//  P_G2_P1Q3 
    {0x36A6, 0xE68B,WORD_LEN}, 	//  P_G2_P1Q4 
    {0x36C0, 0x0631,WORD_LEN}, 	//  P_G1_P2Q0
    {0x36C2, 0x946D,WORD_LEN}, 	//  P_G1_P2Q1 
    {0x36C4, 0xB6B1,WORD_LEN}, 	//  P_G1_P2Q2 
    {0x36C6, 0x5ECF,WORD_LEN}, 	//  P_G1_P2Q3
    {0x36C8, 0x25D2,WORD_LEN}, 	//  P_G1_P2Q4
    {0x36CA, 0x05F1,WORD_LEN}, 	//  P_R_P2Q0 
    {0x36CC, 0x934D,WORD_LEN}, 	//  P_R_P2Q1 
    {0x36CE, 0xB9F1,WORD_LEN}, 	//  P_R_P2Q2
    {0x36D0, 0x05AF,WORD_LEN}, 	//  P_R_P2Q3
    {0x36D2, 0x4C92,WORD_LEN}, 	//  P_R_P2Q4 
    {0x36D4, 0x37B0,WORD_LEN}, 	//  P_B_P2Q0 
    {0x36D6, 0x938B,WORD_LEN}, 	//  P_B_P2Q1 
    {0x36D8, 0xB4B0,WORD_LEN}, 	//  P_B_P2Q2
    {0x36DA, 0x6A8D,WORD_LEN}, 	//  P_B_P2Q3
    {0x36DC, 0x3951,WORD_LEN}, 	//  P_B_P2Q4 
    {0x36DE, 0x0611,WORD_LEN}, 	//  P_G2_P2Q0
    {0x36E0, 0xA28D,WORD_LEN}, 	//  P_G2_P2Q1 
    {0x36E2, 0xB6B1,WORD_LEN}, 	//  P_G2_P2Q2 
    {0x36E4, 0x6D6F,WORD_LEN}, 	//  P_G2_P2Q3 
    {0x36E6, 0x2652,WORD_LEN}, 	//  P_G2_P2Q4 
    {0x3700, 0x75CA,WORD_LEN}, 	//  P_G1_P3Q0
    {0x3702, 0x42AD,WORD_LEN}, 	//  P_G1_P3Q1 
    {0x3704, 0xB9AA,WORD_LEN}, 	//  P_G1_P3Q2 
    {0x3706, 0xA0EF,WORD_LEN}, 	//  P_G1_P3Q3 
    {0x3708, 0x29B2,WORD_LEN}, 	//  P_G1_P3Q4
    {0x370A, 0x830B,WORD_LEN}, 	//  P_R_P3Q0 
    {0x370C, 0x05AE,WORD_LEN}, 	//  P_R_P3Q1
    {0x370E, 0x30B0,WORD_LEN}, 	//  P_R_P3Q2 
    {0x3710, 0x88D0,WORD_LEN}, 	//  P_R_P3Q3
    {0x3712, 0x1AF0,WORD_LEN}, 	//  P_R_P3Q4 
    {0x3714, 0x746B,WORD_LEN}, 	//  P_B_P3Q0 
    {0x3716, 0x376D,WORD_LEN}, 	//  P_B_P3Q1
    {0x3718, 0x2E8E,WORD_LEN}, 	//  P_B_P3Q2
    {0x371A, 0x8C2B,WORD_LEN}, 	//  P_B_P3Q3 
    {0x371C, 0x0030,WORD_LEN}, 	//  P_B_P3Q4
    {0x371E, 0x050B,WORD_LEN}, 	//  P_G2_P3Q0
    {0x3720, 0x5C0D,WORD_LEN}, 	//  P_G2_P3Q1 
    {0x3722, 0x1E88,WORD_LEN}, 	//  P_G2_P3Q2 
    {0x3724, 0xB22F,WORD_LEN}, 	//  P_G2_P3Q3 
    {0x3726, 0x26B2,WORD_LEN}, 	//  P_G2_P3Q4 
    {0x3740, 0xFC90,WORD_LEN}, 	//  P_G1_P4Q0 
    {0x3742, 0x430F,WORD_LEN}, 	//  P_G1_P4Q1 
    {0x3744, 0x9151,WORD_LEN}, 	//  P_G1_P4Q2
    {0x3746, 0xDF71,WORD_LEN}, 	//  P_G1_P4Q3 
    {0x3748, 0x4575,WORD_LEN}, 	//  P_G1_P4Q4
    {0x374A, 0xC1CF,WORD_LEN}, 	//  P_R_P4Q0 
    {0x374C, 0x414F,WORD_LEN}, 	//  P_R_P4Q1 
    {0x374E, 0xD551,WORD_LEN}, 	//  P_R_P4Q2 
    {0x3750, 0x9451,WORD_LEN}, 	//  P_R_P4Q3
    {0x3752, 0x5A35,WORD_LEN}, 	//  P_R_P4Q4 

    {0x3754, 0x4C6C,WORD_LEN}, 	//  P_B_P4Q0
    {0x3756, 0x300F,WORD_LEN}, 	//  P_B_P4Q1 
    {0x3758, 0xB232,WORD_LEN}, 	//  P_B_P4Q2 
    {0x375A, 0x96F0,WORD_LEN}, 	//  P_B_P4Q3
    {0x375C, 0x47B5,WORD_LEN}, 	//  P_B_P4Q4 
    {0x375E, 0xFBD0,WORD_LEN}, 	//  P_G2_P4Q0
    {0x3760, 0x568F,WORD_LEN
    }, 	//  P_G2_P4Q1 
    {0x3762, 0x8E51,WORD_LEN}, 	//  P_G2_P4Q2
    {0x3764, 0xECB1,WORD_LEN}, 	//  P_G2_P4Q3 
    {0x3766, 0x44B5,WORD_LEN}, 	//  P_G2_P4Q4 
    {0x3784, 0x0280,WORD_LEN}, 	//  CENTER_COLUMN 
    {0x3782, 0x01EC,WORD_LEN}, 	//  CENTER_ROW 
    {0x37C0, 0x83C7,WORD_LEN}, 	//  P_GR_Q5 
    {0x37C2, 0xEB89,WORD_LEN}, 	//  P_RD_Q5 
    {0x37C4, 0xD089,WORD_LEN}, 	//  P_BL_Q5 
    {0x37C6, 0x9187,WORD_LEN}, 	//  P_GB_Q5 
    {0x098E, 0x0000,WORD_LEN}, 	//  LOGICAL addressing
    {0xC960, 0x0A8C,WORD_LEN}, 	//  CAM_PGA_L_CONFIG_COLOUR_TEMP 
    {0xC962, 0x7584,WORD_LEN}, 	//  CAM_PGA_L_CONFIG_GREEN_RED_Q14 
    {0xC964, 0x5900,WORD_LEN}, 	//  CAM_PGA_L_CONFIG_RED_Q14
    {0xC966, 0x75AE,WORD_LEN}, 	//  CAM_PGA_L_CONFIG_GREEN_BLUE_Q14 
    {0xC968, 0x7274,WORD_LEN}, 	//  CAM_PGA_L_CONFIG_BLUE_Q14 
    {0xC96A, 0x0FD2,WORD_LEN}, 	//  CAM_PGA_M_CONFIG_COLOUR_TEMP 
    {0xC96C, 0x8155,WORD_LEN}, 	//  CAM_PGA_M_CONFIG_GREEN_RED_Q14 
    {0xC96E, 0x8018,WORD_LEN}, 	//  CAM_PGA_M_CONFIG_RED_Q14
    {0xC970, 0x814C,WORD_LEN}, 	//  CAM_PGA_M_CONFIG_GREEN_BLUE_Q14
    {0xC972, 0x836B,WORD_LEN}, 	//  CAM_PGA_M_CONFIG_BLUE_Q14
    {0xC974, 0x1964,WORD_LEN}, 	//  CAM_PGA_R_CONFIG_COLOUR_TEMP 
    {0xC976, 0x7FDF,WORD_LEN}, 	//  CAM_PGA_R_CONFIG_GREEN_RED_Q14
    {0xC978, 0x7F15,WORD_LEN}, 	//  CAM_PGA_R_CONFIG_RED_Q14 
    {0xC97A, 0x7FDC,WORD_LEN}, 	//  CAM_PGA_R_CONFIG_GREEN_BLUE_Q14
    {0xC97C, 0x7F30,WORD_LEN}, 	//  CAM_PGA_R_CONFIG_BLUE_Q14 
    {0xC95E, 0x0003,WORD_LEN}, 	//  CAM_PGA_PGA_CONTROL
    //[Step5-AWB_CCM]//default value 
    {0xC892, 0x0267,WORD_LEN}, 	// CAM_AWB_CCM_L_0
    {0xC894, 0xFF1A,WORD_LEN}, 	// CAM_AWB_CCM_L_1
    {0xC896, 0xFFB3,WORD_LEN}, 	// CAM_AWB_CCM_L_2 
    {0xC898, 0xFF80,WORD_LEN}, 	// CAM_AWB_CCM_L_3 
    {0xC89A, 0x0166,WORD_LEN}, 	// CAM_AWB_CCM_L_4 
    {0xC89C, 0x0003,WORD_LEN}, 	// CAM_AWB_CCM_L_5 
    {0xC89E, 0xFF9A,WORD_LEN}, 	// CAM_AWB_CCM_L_6 
    {0xC8A0, 0xFEB4,WORD_LEN}, 	// CAM_AWB_CCM_L_7 
    {0xC8A2, 0x024D,WORD_LEN}, 	// CAM_AWB_CCM_L_8 
    {0xC8A4, 0x01BF,WORD_LEN}, 	// CAM_AWB_CCM_M_0
    {0xC8A6, 0xFF01,WORD_LEN}, 	// CAM_AWB_CCM_M_1 
    {0xC8A8, 0xFFF3,WORD_LEN}, 	// CAM_AWB_CCM_M_2 
    {0xC8AA, 0xFF75,WORD_LEN}, 	// CAM_AWB_CCM_M_3 
    {0xC8AC, 0x0198,WORD_LEN}, 	// CAM_AWB_CCM_M_4 
    {0xC8AE, 0xFFFD,WORD_LEN}, 	// CAM_AWB_CCM_M_5 
    {0xC8B0, 0xFF9A,WORD_LEN}, 	// CAM_AWB_CCM_M_6
    {0xC8B2, 0xFEE7,WORD_LEN}, 	// CAM_AWB_CCM_M_7 
    {0xC8B4, 0x02A8,WORD_LEN}, 	// CAM_AWB_CCM_M_8 
    {0xC8B6, 0x01D9,WORD_LEN}, 	// CAM_AWB_CCM_R_0 
    {0xC8B8, 0xFF26,WORD_LEN}, 	// CAM_AWB_CCM_R_1 
    {0xC8BA, 0xFFF3,WORD_LEN}, 	// CAM_AWB_CCM_R_2 
    {0xC8BC, 0xFFB3,WORD_LEN}, 	// CAM_AWB_CCM_R_3 
    {0xC8BE, 0x0132,WORD_LEN}, 	// CAM_AWB_CCM_R_4 
    {0xC8C0, 0xFFE8,WORD_LEN}, 	// CAM_AWB_CCM_R_5 
    {0xC8C2, 0xFFDA,WORD_LEN}, 	// CAM_AWB_CCM_R_6
    {0xC8C4, 0xFECD,WORD_LEN}, 	// CAM_AWB_CCM_R_7 
    {0xC8C6, 0x02C2,WORD_LEN}, 	// CAM_AWB_CCM_R_8 
    {0xC8C8, 0x0075,WORD_LEN}, 	// CAM_AWB_CCM_L_RG_GAIN 
    {0xC8CA, 0x011C,WORD_LEN}, 	// CAM_AWB_CCM_L_BG_GAIN 
    {0xC8CC, 0x009A,WORD_LEN}, 	// CAM_AWB_CCM_M_RG_GAIN 
    {0xC8CE, 0x0105,WORD_LEN}, 	// CAM_AWB_CCM_M_BG_GAIN 
    {0xC8D0, 0x00A4,WORD_LEN}, 	// CAM_AWB_CCM_R_RG_GAIN 
    {0xC8D2, 0x00AC,WORD_LEN}, 	// CAM_AWB_CCM_R_BG_GAIN 
    {0xC8D4, 0x0A8C,WORD_LEN}, 	// CAM_AWB_CCM_L_CTEMP 
    {0xC8D6, 0x0F0A,WORD_LEN}, 	// CAM_AWB_CCM_M_CTEMP
    {0xC8D8, 0x1964,WORD_LEN}, 	// CAM_AWB_CCM_R_CTEMP 
    {0xC914, 0x0000,WORD_LEN}, 	// CAM_STAT_AWB_CLIP_WINDOW_XSTART
    {0xC916, 0x0000,WORD_LEN}, 	// CAM_STAT_AWB_CLIP_WINDOW_YSTART
    {0xC918, 0x04FF,WORD_LEN}, 	// CAM_STAT_AWB_CLIP_WINDOW_XEND 
    {0xC91A, 0x02CF,WORD_LEN}, 	// CAM_STAT_AWB_CLIP_WINDOW_YEND
    {0xC904, 0x0033,WORD_LEN}, 	// CAM_AWB_AWB_XSHIFT_PRE_ADJ
    {0xC906, 0x0040,WORD_LEN}, 	// CAM_AWB_AWB_YSHIFT_PRE_ADJ 
    {0xC8F2, 0x03,BYTE_LEN 	},// CAM_AWB_AWB_XSCALE
    {0xC8F3, 0x02,BYTE_LEN 	},// CAM_AWB_AWB_YSCALE 
    {0xC906, 0x003C,WORD_LEN}, 	// CAM_AWB_AWB_YSHIFT_PRE_ADJ 
    {0xC8F4, 0x0000,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_0
    {0xC8F6, 0x0000,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_1 
    {0xC8F8, 0x0000,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_2 
    {0xC8FA, 0xE724,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_3 
    {0xC8FC, 0x1583,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_4 
    {0xC8FE, 0x2045,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_5 
    {0xC900, 0x03FF,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_6 
    {0xC902, 0x007C,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_7 
    {0xC90C, 0x80,BYTE_LEN}, 	// CAM_AWB_K_R_L 
    {0xC90D, 0x80,BYTE_LEN}, 	// CAM_AWB_K_G_L 
    {0xC90E, 0x80,BYTE_LEN}, 	// CAM_AWB_K_B_L 
    {0xC90F, 0x88,BYTE_LEN}, 	// CAM_AWB_K_R_R 
    {0xC910, 0x80,BYTE_LEN}, 	// CAM_AWB_K_G_R 
    {0xC911, 0x80,BYTE_LEN}, 	// CAM_AWB_K_B_R
    //[Step7-CPIPE_Preference]
    {0x098E, 0x4926,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS [CAM_LL_START_BRIGHTNESS] 
    {0xC926, 0x0020,WORD_LEN}, 	// CAM_LL_START_BRIGHTNESS 
    {0xC928, 0x009A,WORD_LEN}, 	// CAM_LL_STOP_BRIGHTNESS
    {0xC946, 0x0070,WORD_LEN}, 	// CAM_LL_START_GAIN_METRIC 
    {0xC948, 0x00F3,WORD_LEN}, 	// CAM_LL_STOP_GAIN_METRIC 
    {0xC952, 0x0020,WORD_LEN}, 	// CAM_LL_START_TARGET_LUMA_BM 
    {0xC954, 0x009A,WORD_LEN}, 	// CAM_LL_STOP_TARGET_LUMA_BM 
    {0xC92A, 0x80,BYTE_LEN},	// CAM_LL_START_SATURATION
    {0xC92B, 0x4B,BYTE_LEN},	// CAM_LL_END_SATURATION
    {0xC92C, 0x00,BYTE_LEN},	// CAM_LL_START_DESATURATION 
    {0xC92D, 0xFF,BYTE_LEN},	// CAM_LL_END_DESATURATION 
    {0xC92E, 0x3C,BYTE_LEN},	// CAM_LL_START_DEMOSAIC 
    {0xC92F, 0x02,BYTE_LEN},	// CAM_LL_START_AP_GAIN 
    {0xC930, 0x06,BYTE_LEN},	// CAM_LL_START_AP_THRESH
    {0xC931, 0x64,BYTE_LEN},	// CAM_LL_STOP_DEMOSAIC 
    {0xC932, 0x01,BYTE_LEN},	// CAM_LL_STOP_AP_GAIN 
    {0xC933, 0x0C,BYTE_LEN},	// CAM_LL_STOP_AP_THRESH 
    {0xC934, 0x3C,BYTE_LEN},	// CAM_LL_START_NR_RED
    {0xC935, 0x3C,BYTE_LEN},	// CAM_LL_START_NR_GREEN 
    {0xC936, 0x3C,BYTE_LEN},	// CAM_LL_START_NR_BLUE 
    {0xC937, 0x0F,BYTE_LEN},	// CAM_LL_START_NR_THRESH 
    {0xC938, 0x64,BYTE_LEN},	// CAM_LL_STOP_NR_RED 
    {0xC939, 0x64,BYTE_LEN},	// CAM_LL_STOP_NR_GREEN
    {0xC93A, 0x64,BYTE_LEN} ,	// CAM_LL_STOP_NR_BLUE 
    {0xC93B, 0x32,BYTE_LEN},	// CAM_LL_STOP_NR_THRESH 
    {0xC93C, 0x0020,WORD_LEN}, 	// CAM_LL_START_CONTRAST_BM 
    {0xC93E, 0x009A,WORD_LEN}, 	// CAM_LL_STOP_CONTRAST_BM 
    {0xC940, 0x00DC,WORD_LEN}, 	// CAM_LL_GAMMA 
    {0xC942, 0x38,BYTE_LEN}, 	// CAM_LL_START_CONTRAST_GRADIENT 
    {0xC943, 0x30,BYTE_LEN}, 	// CAM_LL_STOP_CONTRAST_GRADIENT 
    {0xC944, 0x50,BYTE_LEN}, 	// CAM_LL_START_CONTRAST_LUMA_PERCENTAGE 
    {0xC945, 0x19,BYTE_LEN}, 	// CAM_LL_STOP_CONTRAST_LUMA_PERCENTAGE 
    {0xC94A, 0x0230,WORD_LEN}, 	// CAM_LL_START_FADE_TO_BLACK_LUMA 
    {0xC94C, 0x0010,WORD_LEN}, 	// CAM_LL_STOP_FADE_TO_BLACK_LUMA 
    {0xC94E, 0x01CD,WORD_LEN}, 	// CAM_LL_CLUSTER_DC_TH_BM 
    {0xC950, 0x05,BYTE_LEN },	// CAM_LL_CLUSTER_DC_GATE_PERCENTAGE
    {0xC951, 0x40,BYTE_LEN },	// CAM_LL_SUMMING_SENSITIVITY_FACTOR 
    {0xC87B, 0x1B,BYTE_LEN}, 	// CAM_AET_TARGET_AVERAGE_LUMA_DARK
    {0xC878, 0x0E,BYTE_LEN}, 	// CAM_AET_AEMODE 
    {0xC890, 0x0080,WORD_LEN}, 	// CAM_AET_TARGET_GAIN 
    {0xC886, 0x0100,WORD_LEN}, 	// CAM_AET_AE_MAX_VIRT_AGAIN 
    {0xC87C, 0x005A,WORD_LEN}, 	// CAM_AET_BLACK_CLIPPING_TARGET 
    {0xB42A, 0x05,BYTE_LEN}, 	// CCM_DELTA_GAIN
    {0xA80A, 0x20,BYTE_LEN}, 	// AE_TRACK_AE_TRACKING_DAMPENING_SPEED
    //[Step8-Features] // For Parallel,2012-5-16 14:33 
    {0x098E, 0x0000,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS
    {0xC984, 0x8040,WORD_LEN}, 	// CAM_PORT_OUTPUT_CONTROL 
    {0xC984, 0x8040,WORD_LEN},  	// CAM_PORT_OUTPUT_CONTROL, parallel, 2012-3-21 0:41 
    {0x001E, 0x0777,WORD_LEN}, 	// PAD_SLEW, for parallel, 2012-5-16 14:34 //DELAY=50 //add 2012-4-1 13:42 // end, 2012-5-16 14:33
    // For MIPI, 2012-5-16 14:33 
    {0x098E, 0x0000,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS
    {0xC984, 0x8041,WORD_LEN}, 	// CAM_PORT_OUTPUT_CONTROL 
    {0xC988, 0x0F00,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_T_HS_ZERO 
    {0xC98A, 0x0B07,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_T_HS_EXIT_HS_TRAIL 
    {0xC98C, 0x0D01,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_T_CLK_POST_CLK_PRE 
    {0xC98E, 0x071D,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_T_CLK_TRAIL_CLK_ZERO 
    {0xC990, 0x0006,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_T_LPX 
    {0xC992, 0x0A0C,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_INIT_TIMING
    {0x3C5A, 0x0009,WORD_LEN}, 	// MIPI_DELAY_TRIM
    //[Anti-Flicker for MT9M114][50Hz] 
    {0x098E, 0xC88B,WORD_LEN},  // LOGICAL_ADDRESS_ACCESS [CAM_AET_FLICKER_FREQ_HZ] 
    {0xC88B, 0x32,BYTE_LEN  },  // CAM_AET_FLICKER_FREQ_HZ
    // Saturation
    {0xC92A,0x84,BYTE_LEN},
    {0xC92B,0x46,BYTE_LEN},
    // AE //Reg = 0xC87A,0x48 
    {0xC87A,0x3C,BYTE_LEN},
    // Sharpness
    {0x098E,0xC92F,WORD_LEN}, 
    {0xC92F,0x01,BYTE_LEN  }, 
    {0xC932,0x00,BYTE_LEN  },
    // Target Gain
    {0x098E,0x4890,WORD_LEN},
    {0xC890,0x0040,WORD_LEN},
    { 0xc940,0x00C8,WORD_LEN}, // CAM_LL_GAMMA,2012-5-17 14:42 
    {0x3C40, 0x783C,WORD_LEN}, 	// MIPI_CONTROL
    //[Change-Config] 
    {0x098E, 0xDC00,WORD_LEN},  	// LOGICAL_ADDRESS_ACCESS [SYSMGR_NEXT_STATE] 
    {0xDC00, 0x28,BYTE_LEN 	},// SYSMGR_NEXT_STATE 
    {0x0080, 0x8002,WORD_LEN}, 	// COMMAND_REGISTER //  POLL  COMMAND_REGISTER::HOST_COMMAND_1 =>  0x00 ////DELAY=100 //DELAY=150 {MT9M114_TABLE_WAIT_MS, 150}, 

    {MT9M114_TABLE_WAIT_MS, 100}, 
    {MT9M114_TABLE_END, 0x00}
};
#endif
static struct mt9m114_reg mode_1280x960[]= 
{ 
    //DELAY=10 
    {MT9M114_TABLE_WAIT_MS, 20}, 
    {0x3C40, 0x003C,WORD_LEN },        // MIPI_CONTROL 
    {0x301A, 0x0230,WORD_LEN },        // RESET_REGISTER 
    {0x098E, 0x1000,WORD_LEN },        // LOGICAL_ADDRESS_ACCESS 
    {0xC97E, 0x01 ,BYTE_LEN        },// CAM_SYSCTL_PLL_ENABLE 
    //REG= 0xC980, 0x0225         
    // CAM_SYSCTL_PLL_DIVIDER_M_N 
    {0xC980, 0x0120,WORD_LEN},         // CAM_SYSCTL_PLL_DIVIDER_M_N 
    {0xC982, 0x0700,WORD_LEN},         // CAM_SYSCTL_PLL_DIVIDER_P //DELAY=10 
    {MT9M114_TABLE_WAIT_MS, 20}, 

    // select output size, YUV format 
    {0xC800, 0x0004,WORD_LEN},                //cam_sensor_cfg_y_addr_start = 4 
    {0xC802, 0x0004,WORD_LEN},                //cam_sensor_cfg_x_addr_start = 4 
    {0xC804, 0x03CB,WORD_LEN},                //cam_sensor_cfg_y_addr_end = 971 
    {0xC806, 0x050B,WORD_LEN},                //cam_sensor_cfg_x_addr_end = 1291 
    {0xC808, 0x02DC, WORD_LEN},//C00                //cam_sensor_cfg_pixclk = 48000000 
    {0xC80A, 0x6C00, WORD_LEN},//C00                //cam_sensor_cfg_pixclk = 48000000 
    {0xC80C, 0x0001,WORD_LEN},                //cam_sensor_cfg_row_speed = 1 
    {0xC80E, 0x00DB,WORD_LEN},                //cam_sensor_cfg_fine_integ_time_min = 219 
    {0xC810, 0x05B1,WORD_LEN},                //cam_sensor_cfg_fine_integ_time_max = 1457 
    {0xC812, 0x03FD,WORD_LEN},                //cam_sensor_cfg_frame_length_lines = 1021 
    {0xC814, 0x0634,WORD_LEN},                //cam_sensor_cfg_line_length_pck = 1588 
    {0xC816, 0x0060,WORD_LEN},                //cam_sensor_cfg_fine_correction = 96 
    {0xC818, 0x03C3,WORD_LEN},                //cam_sensor_cfg_cpipe_last_row = 963 
    {0xC826, 0x0020,WORD_LEN},                //cam_sensor_cfg_reg_0_data = 32 
    {0xC834, 0x0003,WORD_LEN},                //cam_sensor_control_read_mode = 0 
    {0xC854, 0x0000,WORD_LEN},                //cam_crop_window_xoffset = 0 
    {0xC856, 0x0000,WORD_LEN},                //cam_crop_window_yoffset = 0 
    {0xC858, 0x0500,WORD_LEN},                //cam_crop_window_width = 1280 
    {0xC85A, 0x03C0,WORD_LEN},                //cam_crop_window_height = 960 
    {0xC85C, 0x03,BYTE_LEN},                //cam_crop_cropmode = 3 
    {0xC868, 0x0500,WORD_LEN},                //cam_output_width = 1280 
    {0xC86A, 0x03C0,WORD_LEN},                //cam_output_height = 960 
    {0xC878, 0x00,BYTE_LEN},                //cam_aet_aemode = 0 
    {0xC88C, 0x1D9B,WORD_LEN},                //cam_aet_max_frame_rate = 7579 
    {0xC88E, 0x0ECD,WORD_LEN},                //cam_aet_min_frame_rate = 3789 
    {0xC914, 0x0000,WORD_LEN},                //cam_stat_awb_clip_window_xstart = 0 
    {0xC916, 0x0000,WORD_LEN},                //cam_stat_awb_clip_window_ystart = 0 
    {0xC918, 0x04FF,WORD_LEN},                //cam_stat_awb_clip_window_xend = 1279 
    {0xC91A, 0x03BF,WORD_LEN},                //cam_stat_awb_clip_window_yend = 959 
    {0xC91C, 0x0000,WORD_LEN},                //cam_stat_ae_initial_window_xstart = 0 
    {0xC91E, 0x0000,WORD_LEN},                //cam_stat_ae_initial_window_ystart = 0 
    {0xC920, 0x00FF,WORD_LEN},                //cam_stat_ae_initial_window_xend = 255 
    {0xC922, 0x00BF,WORD_LEN},   //1280960             //cam_stat_ae_initial_window_yend = 191 
 //  {0xC922, 0x008F,WORD_LEN},   //1280720
   // {0xC920, 0x007F,WORD_LEN},		//cam_stat_ae_initial_window_xend = 127
  //  {0xC922, 0x005F,WORD_LEN},		//cam_stat_ae_initial_window_yend = 95 //
   
    //[Step3-Recommended] 
    {0x316A, 0x8270,WORD_LEN},         // DAC_TXLO_ROW 
    {0x316C, 0x8270,WORD_LEN},         // DAC_TXLO 
    {0x3ED0, 0x2305,WORD_LEN},         // DAC_LD_4_5 
    {0x3ED2, 0x77CF,WORD_LEN},         // DAC_LD_6_7 
    {0x316E, 0x8202,WORD_LEN},         // DAC_ECL 
    {0x3180, 0x87FF,WORD_LEN},         // DELTA_DK_CONTROL 
    {0x30D4, 0x6080,WORD_LEN},         // COLUMN_CORRECTION 
    {0xA802, 0x0008,WORD_LEN},         // AE_TRACK_MODE 
    {0x3E14, 0xFF39,WORD_LEN},         // SAMP_COL_PUP2 
    {0x31E0, 0x0001,WORD_LEN},         // DAC_TXLO_ROW 

    //[patch 1204]for 720P 
    {0x0982, 0x0001,WORD_LEN},         // ACCESS_CTL_STAT 
    {0x098A, 0x60BC,WORD_LEN},         // PHYSICAL_ADDRESS_ACCESS 
    {0xE0BC, 0xC0F1,WORD_LEN}, 
    {0xE0BE, 0x082A,WORD_LEN}, 
    {0xE0C0, 0x05A0,WORD_LEN}, 
    {0xE0C2, 0xD800,WORD_LEN}, 
    {0xE0C4, 0x71CF,WORD_LEN}, 
    {0xE0C6, 0xFFFF,WORD_LEN}, 
    {0xE0C8, 0xC344,WORD_LEN}, 
    {0xE0CA, 0x77CF,WORD_LEN}, 
    {0xE0CC, 0xFFFF,WORD_LEN}, 
    {0xE0CE, 0xC7C0,WORD_LEN}, 
    {0xE0D0, 0xB104,WORD_LEN}, 
    {0xE0D2, 0x8F1F,WORD_LEN}, 
    {0xE0D4, 0x75CF,WORD_LEN}, 
    {0xE0D6, 0xFFFF,WORD_LEN}, 
    {0xE0D8, 0xC84C,WORD_LEN}, 
    {0xE0DA, 0x0811,WORD_LEN}, 
    {0xE0DC, 0x005E,WORD_LEN}, 
    {0xE0DE, 0x70CF,WORD_LEN}, 
    {0xE0E0, 0x0000,WORD_LEN}, 
    {0xE0E2, 0x500E,WORD_LEN}, 
    {0xE0E4, 0x7840,WORD_LEN}, 
    {0xE0E6, 0xF019,WORD_LEN}, 
    {0xE0E8, 0x0CC6,WORD_LEN}, 
    {0xE0EA, 0x0340,WORD_LEN}, 
    {0xE0EC, 0x0E26,WORD_LEN}, 
    {0xE0EE, 0x0340,WORD_LEN}, 
    {0xE0F0, 0x95C2,WORD_LEN}, 
    {0xE0F2, 0x0E21,WORD_LEN}, 
    {0xE0F4, 0x101E,WORD_LEN}, 
    {0xE0F6, 0x0E0D,WORD_LEN}, 
    {0xE0F8, 0x119E,WORD_LEN}, 
    {0xE0FA, 0x0D56,WORD_LEN}, 
    {0xE0FC, 0x0340,WORD_LEN}, 
    {0xE0FE, 0xF008,WORD_LEN}, 
    {0xE100, 0x2650,WORD_LEN}, 
    {0xE102, 0x1040,WORD_LEN}, 
    {0xE104, 0x0AA2,WORD_LEN}, 
    {0xE106, 0x0360,WORD_LEN}, 
    {0xE108, 0xB502,WORD_LEN}, 
    {0xE10A, 0xB5C2,WORD_LEN}, 
    {0xE10C, 0x0B22,WORD_LEN}, 
    {0xE10E, 0x0400,WORD_LEN}, 
    {0xE110, 0x0CCE,WORD_LEN}, 
    {0xE112, 0x0320,WORD_LEN}, 
    {0xE114, 0xD800,WORD_LEN}, 
    {0xE116, 0x70CF,WORD_LEN}, 
    {0xE118, 0xFFFF,WORD_LEN}, 
    {0xE11A, 0xC5D4,WORD_LEN}, 
    {0xE11C, 0x902C,WORD_LEN}, 
    {0xE11E, 0x72CF,WORD_LEN}, 
    {0xE120, 0xFFFF,WORD_LEN}, 
    {0xE122, 0xE218,WORD_LEN}, 
    {0xE124, 0x9009,WORD_LEN}, 
    {0xE126, 0xE105,WORD_LEN}, 
    {0xE128, 0x73CF,WORD_LEN}, 
    {0xE12A, 0xFF00,WORD_LEN}, 
    {0xE12C, 0x2FD0,WORD_LEN}, 
    {0xE12E, 0x7822,WORD_LEN}, 
    {0xE130, 0x7910,WORD_LEN}, 
    {0xE132, 0xB202,WORD_LEN}, 
    {0xE134, 0x1382,WORD_LEN}, 
    {0xE136, 0x0700,WORD_LEN}, 
    {0xE138, 0x0815,WORD_LEN}, 
    {0xE13A, 0x03DE,WORD_LEN}, 
    {0xE13C, 0x1387,WORD_LEN}, 
    {0xE13E, 0x0700,WORD_LEN}, 
    {0xE140, 0x2102,WORD_LEN}, 
    {0xE142, 0x000A,WORD_LEN}, 
    {0xE144, 0x212F,WORD_LEN}, 
    {0xE146, 0x0288,WORD_LEN}, 
    {0xE148, 0x1A04,WORD_LEN}, 
    {0xE14A, 0x0284,WORD_LEN}, 
    {0xE14C, 0x13B9,WORD_LEN}, 
    {0xE14E, 0x0700,WORD_LEN}, 
    {0xE150, 0xB8C1,WORD_LEN}, 
    {0xE152, 0x0815,WORD_LEN}, 
    {0xE154, 0x0052,WORD_LEN}, 
    {0xE156, 0xDB00,WORD_LEN}, 
    {0xE158, 0x230F,WORD_LEN}, 
    {0xE15A, 0x0003,WORD_LEN}, 
    {0xE15C, 0x2102,WORD_LEN}, 
    {0xE15E, 0x00C0,WORD_LEN}, 
    {0xE160, 0x7910,WORD_LEN}, 
    {0xE162, 0xB202,WORD_LEN}, 
    {0xE164, 0x9507,WORD_LEN}, 
    {0xE166, 0x7822,WORD_LEN}, 
    {0xE168, 0xE080,WORD_LEN}, 
    {0xE16A, 0xD900,WORD_LEN}, 
    {0xE16C, 0x20CA,WORD_LEN}, 
    {0xE16E, 0x004B,WORD_LEN}, 
    {0xE170, 0xB805,WORD_LEN}, 
    {0xE172, 0x9533,WORD_LEN}, 
    {0xE174, 0x7815,WORD_LEN}, 
    {0xE176, 0x6038,WORD_LEN}, 
    {0xE178, 0x0FB2,WORD_LEN}, 
    {0xE17A, 0x0560,WORD_LEN}, 
    {0xE17C, 0xB861,WORD_LEN}, 
    {0xE17E, 0xB711,WORD_LEN}, 
    {0xE180, 0x0775,WORD_LEN}, 
    {0xE182, 0x0540,WORD_LEN}, 
    {0xE184, 0xD900,WORD_LEN}, 
    {0xE186, 0xF00A,WORD_LEN}, 
    {0xE188, 0x70CF,WORD_LEN}, 
    {0xE18A, 0xFFFF,WORD_LEN}, 
    {0xE18C, 0xE210,WORD_LEN}, 
    {0xE18E, 0x7835,WORD_LEN}, 
    {0xE190, 0x8041,WORD_LEN}, 
    {0xE192, 0x8000,WORD_LEN}, 
    {0xE194, 0xE102,WORD_LEN}, 
    {0xE196, 0xA040,WORD_LEN}, 
    {0xE198, 0x09F1,WORD_LEN}, 
    {0xE19A, 0x8094,WORD_LEN}, 
    {0xE19C, 0x7FE0,WORD_LEN}, 
    {0xE19E, 0xD800,WORD_LEN}, 
    {0xE1A0, 0xC0F1,WORD_LEN}, 
    {0xE1A2, 0xC5E1,WORD_LEN}, 
    {0xE1A4, 0x71CF,WORD_LEN}, 
    {0xE1A6, 0x0000,WORD_LEN}, 
    {0xE1A8, 0x45E6,WORD_LEN}, 
    {0xE1AA, 0x7960,WORD_LEN}, 
    {0xE1AC, 0x7508,WORD_LEN}, 
    {0xE1AE, 0x70CF,WORD_LEN}, 
    {0xE1B0, 0xFFFF,WORD_LEN}, 
    {0xE1B2, 0xC84C,WORD_LEN}, 
    {0xE1B4, 0x9002,WORD_LEN}, 
    {0xE1B6, 0x083D,WORD_LEN}, 
    {0xE1B8, 0x021E,WORD_LEN}, 
    {0xE1BA, 0x0D39,WORD_LEN}, 
    {0xE1BC, 0x10D1,WORD_LEN}, 
    {0xE1BE, 0x70CF,WORD_LEN}, 
    {0xE1C0, 0xFF00,WORD_LEN}, 
    {0xE1C2, 0x3354,WORD_LEN}, 
    {0xE1C4, 0x9055,WORD_LEN}, 
    {0xE1C6, 0x71CF,WORD_LEN}, 
    {0xE1C8, 0xFFFF,WORD_LEN}, 
    {0xE1CA, 0xC5D4,WORD_LEN}, 
    {0xE1CC, 0x116C,WORD_LEN}, 
    {0xE1CE, 0x0103,WORD_LEN}, 
    {0xE1D0, 0x1170,WORD_LEN}, 
    {0xE1D2, 0x00C1,WORD_LEN}, 
    {0xE1D4, 0xE381,WORD_LEN}, 
    {0xE1D6, 0x22C6,WORD_LEN}, 
    {0xE1D8, 0x0F81,WORD_LEN}, 
    {0xE1DA, 0x0000,WORD_LEN}, 
    {0xE1DC, 0x00FF,WORD_LEN}, 
    {0xE1DE, 0x22C4,WORD_LEN}, 
    {0xE1E0, 0x0F82,WORD_LEN}, 
    {0xE1E2, 0xFFFF,WORD_LEN}, 
    {0xE1E4, 0x00FF,WORD_LEN}, 
    {0xE1E6, 0x29C0,WORD_LEN}, 
    {0xE1E8, 0x0222,WORD_LEN}, 
    {0xE1EA, 0x7945,WORD_LEN}, 
    {0xE1EC, 0x7930,WORD_LEN}, 
    {0xE1EE, 0xB035,WORD_LEN}, 
    {0xE1F0, 0x0715,WORD_LEN}, 
    {0xE1F2, 0x0540,WORD_LEN}, 
    {0xE1F4, 0xD900,WORD_LEN}, 
    {0xE1F6, 0xF00A,WORD_LEN}, 
    {0xE1F8, 0x70CF,WORD_LEN}, 
    {0xE1FA, 0xFFFF,WORD_LEN}, 
    {0xE1FC, 0xE224,WORD_LEN}, 
    {0xE1FE, 0x7835,WORD_LEN}, 
    {0xE200, 0x8041,WORD_LEN}, 
    {0xE202, 0x8000,WORD_LEN}, 
    {0xE204, 0xE102,WORD_LEN}, 
    {0xE206, 0xA040,WORD_LEN}, 
    {0xE208, 0x09F1,WORD_LEN}, 
    {0xE20A, 0x8094,WORD_LEN}, 
    {0xE20C, 0x7FE0,WORD_LEN}, 
    {0xE20E, 0xD800,WORD_LEN}, 
    {0xE210, 0xFFFF,WORD_LEN}, 
    {0xE212, 0xCB40,WORD_LEN}, 
    {0xE214, 0xFFFF,WORD_LEN}, 
    {0xE216, 0xE0BC,WORD_LEN}, 
    {0xE218, 0x0000,WORD_LEN}, 
    {0xE21A, 0x0000,WORD_LEN}, 
    {0xE21C, 0x0000,WORD_LEN}, 
    {0xE21E, 0x0000,WORD_LEN}, 
    {0xE220, 0x0000,WORD_LEN}, 
    {0x098E, 0x0000,WORD_LEN},         // LOGICAL_ADDRESS_ACCESS 
    {0xE000, 0x1184,WORD_LEN},         // PATCHLDR_LOADER_ADDRESS 
    {0xE002, 0x1204,WORD_LEN},         // PATCHLDR_PATCH_ID 
    {0xE004, 0x4103,WORD_LEN},   //0202 
    {0xE006, 0x0202,WORD_LEN}, 
    // PATCHLDR_FIRMWARE_ID //REG= 0xE004, 0x4103 //REG= 0xE006, 0x0202 
    {0x0080, 0xFFF0,WORD_LEN},         
    // COMMAND_REGISTER //  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00 
    //  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00 
    //  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00 
    // 读Reg= 0x080， 判断其最低位是否为0， 如果不为0，则delay 5ms，然后继续读， // 直到为0或者   50ms以上 
    //delay = 50 
    {MT9M114_TABLE_WAIT_MS, 100}, 
    {0x0080, 0xFFF1,WORD_LEN},         // COMMAND_REGISTER 
    //  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00 // 读Reg= 0x080， 判断其最低位是否为0， 如果不为0，则delay 5ms，然后继续读， // 直到为0或者   50ms以上 
    //delay = 50 
    {MT9M114_TABLE_WAIT_MS, 100}, 
    // AWB Start point 
    {0x098E,0x2c12,WORD_LEN}, 
    {0xac12,0x008f,WORD_LEN}, 
    {0xac14,0x0105,WORD_LEN}, 
    //[Step4-APGA //LSC] // [APGA Settings 85% 2012/05/02 12:16:56] 
    {0x3640, 0x00B0,WORD_LEN},         //  P_G1_P0Q0 
    {0x3642, 0xC0EA,WORD_LEN},         //  P_G1_P0Q1 
    {0x3644, 0x3E70,WORD_LEN},         //  P_G1_P0Q2 
    {0x3646, 0x182B,WORD_LEN},         //  P_G1_P0Q3 
    {0x3648, 0xCC4D,WORD_LEN},         //  P_G1_P0Q4 
    {0x364A, 0x0150,WORD_LEN},         //  P_R_P0Q0 
    {0x364C, 0x9C4A,WORD_LEN},         //  P_R_P0Q1 
    {0x364E, 0x6DF0,WORD_LEN},         //  P_R_P0Q2 
    {0x3650, 0xE5CA,WORD_LEN},         //  P_R_P0Q3 
    {0x3652, 0xA22F,WORD_LEN},         //  P_R_P0Q4 
    {0x3654, 0x0170,WORD_LEN},         //  P_B_P0Q0 
    {0x3656, 0xC288,WORD_LEN},         //  P_B_P0Q1 
    {0x3658, 0x3010,WORD_LEN},         //  P_B_P0Q2 
    {0x365A, 0xBA26,WORD_LEN},         //  P_B_P0Q3 
    {0x365C, 0xE1AE,WORD_LEN},         //  P_B_P0Q4 
    {0x365E, 0x00B0,WORD_LEN},         //  P_G2_P0Q0 
    {0x3660, 0xC34A,WORD_LEN},         //  P_G2_P0Q1 
    {0x3662, 0x3E90,WORD_LEN},         //  P_G2_P0Q2 
    {0x3664, 0x1DAB,WORD_LEN},         //  P_G2_P0Q3 
    {0x3666, 0xD0CD,WORD_LEN},         //  P_G2_P0Q4 
    {0x3680, 0x440B,WORD_LEN},         //  P_G1_P1Q0 
    {0x3682, 0x1386,WORD_LEN},         //  P_G1_P1Q1 
    {0x3684, 0xAB0C,WORD_LEN},         //  P_G1_P1Q2 
    {0x3686, 0x1FCD,WORD_LEN},         //  P_G1_P1Q3 
    {0x3688, 0xEACB,WORD_LEN},         //  P_G1_P1Q4 
    {0x368A, 0x63C7,WORD_LEN},         //  P_R_P1Q0 
    {0x368C, 0x1006,WORD_LEN},         //  P_R_P1Q1 
    {0x368E, 0xE64C,WORD_LEN},         //  P_R_P1Q2 
    {0x3690, 0x7B4D,WORD_LEN},         //  P_R_P1Q3 
    {0x3692, 0x32AF,WORD_LEN},         //  P_R_P1Q4 
    {0x3694, 0x24EB,WORD_LEN},         //  P_B_P1Q0 
    {0x3696, 0xFB6A,WORD_LEN},         //  P_B_P1Q1 
    {0x3698, 0xB96C,WORD_LEN},         //  P_B_P1Q2 
    {0x369A, 0x9AEA,WORD_LEN},         //  P_B_P1Q3 
    {0x369C, 0x092E,WORD_LEN},         //  P_B_P1Q4 
    {0x369E, 0x432B,WORD_LEN},         //  P_G2_P1Q0 
    {0x36A0, 0xC7C5,WORD_LEN},         //  P_G2_P1Q1 
    {0x36A2, 0xABEC,WORD_LEN},         //  P_G2_P1Q2 
    {0x36A4, 0x222D,WORD_LEN},         //  P_G2_P1Q3 
    {0x36A6, 0xE68B,WORD_LEN},         //  P_G2_P1Q4 
    {0x36C0, 0x0631,WORD_LEN},         //  P_G1_P2Q0 
    {0x36C2, 0x946D,WORD_LEN},         //  P_G1_P2Q1 
    {0x36C4, 0xB6B1,WORD_LEN},         //  P_G1_P2Q2 
    {0x36C6, 0x5ECF,WORD_LEN},         //  P_G1_P2Q3 
    {0x36C8, 0x25D2,WORD_LEN},         //  P_G1_P2Q4 
    {0x36CA, 0x05F1,WORD_LEN},         //  P_R_P2Q0 
    {0x36CC, 0x934D,WORD_LEN},         //  P_R_P2Q1 
    {0x36CE, 0xB9F1,WORD_LEN},         //  P_R_P2Q2 
    {0x36D0, 0x05AF,WORD_LEN},         //  P_R_P2Q3 
    {0x36D2, 0x4C92,WORD_LEN},         //  P_R_P2Q4 
    {0x36D4, 0x37B0,WORD_LEN},         //  P_B_P2Q0 
    {0x36D6, 0x938B,WORD_LEN},         //  P_B_P2Q1 
    {0x36D8, 0xB4B0,WORD_LEN},         //  P_B_P2Q2 
    {0x36DA, 0x6A8D,WORD_LEN},         //  P_B_P2Q3 
    {0x36DC, 0x3951,WORD_LEN},         //  P_B_P2Q4 
    {0x36DE, 0x0611,WORD_LEN},         //  P_G2_P2Q0 
    {0x36E0, 0xA28D,WORD_LEN},         //  P_G2_P2Q1 
    {0x36E2, 0xB6B1,WORD_LEN},         //  P_G2_P2Q2 
    {0x36E4, 0x6D6F,WORD_LEN},         //  P_G2_P2Q3 
    {0x36E6, 0x2652,WORD_LEN},         //  P_G2_P2Q4 
    {0x3700, 0x75CA,WORD_LEN},         //  P_G1_P3Q0 
    {0x3702, 0x42AD,WORD_LEN},         //  P_G1_P3Q1 
    {0x3704, 0xB9AA,WORD_LEN},         //  P_G1_P3Q2 
    {0x3706, 0xA0EF,WORD_LEN},         //  P_G1_P3Q3 
    {0x3708, 0x29B2,WORD_LEN},         //  P_G1_P3Q4 
    {0x370A, 0x830B,WORD_LEN},         //  P_R_P3Q0 
    {0x370C, 0x05AE,WORD_LEN},         //  P_R_P3Q1 
    {0x370E, 0x30B0,WORD_LEN},         //  P_R_P3Q2 
    {0x3710, 0x88D0,WORD_LEN},         //  P_R_P3Q3 
    {0x3712, 0x1AF0,WORD_LEN},         //  P_R_P3Q4 
    {0x3714, 0x746B,WORD_LEN},         //  P_B_P3Q0 
    {0x3716, 0x376D,WORD_LEN},         //  P_B_P3Q1 
    {0x3718, 0x2E8E,WORD_LEN},         //  P_B_P3Q2 
    {0x371A, 0x8C2B,WORD_LEN},         //  P_B_P3Q3 
    {0x371C, 0x0030,WORD_LEN},         //  P_B_P3Q4 
    {0x371E, 0x050B,WORD_LEN},         //  P_G2_P3Q0 
    {0x3720, 0x5C0D,WORD_LEN},         //  P_G2_P3Q1 
    {0x3722, 0x1E88,WORD_LEN},         //  P_G2_P3Q2 
    {0x3724, 0xB22F,WORD_LEN},         //  P_G2_P3Q3 
    {0x3726, 0x26B2,WORD_LEN},         //  P_G2_P3Q4 
    {0x3740, 0xFC90,WORD_LEN},         //  P_G1_P4Q0 
    {0x3742, 0x430F,WORD_LEN},         //  P_G1_P4Q1 
    {0x3744, 0x9151,WORD_LEN},         //  P_G1_P4Q2 
    {0x3746, 0xDF71,WORD_LEN},         //  P_G1_P4Q3 
    {0x3748, 0x4575,WORD_LEN},         //  P_G1_P4Q4 
    {0x374A, 0xC1CF,WORD_LEN},         //  P_R_P4Q0 
    {0x374C, 0x414F,WORD_LEN},         //  P_R_P4Q1 
    {0x374E, 0xD551,WORD_LEN},         //  P_R_P4Q2 
    {0x3750, 0x9451,WORD_LEN},         //  P_R_P4Q3 
    {0x3752, 0x5A35,WORD_LEN},         //  P_R_P4Q4 

    {0x3754, 0x4C6C,WORD_LEN},         //  P_B_P4Q0 
    {0x3756, 0x300F,WORD_LEN},         //  P_B_P4Q1 
    {0x3758, 0xB232,WORD_LEN},         //  P_B_P4Q2 
    {0x375A, 0x96F0,WORD_LEN},         //  P_B_P4Q3 
    {0x375C, 0x47B5,WORD_LEN},         //  P_B_P4Q4 
    {0x375E, 0xFBD0,WORD_LEN},         //  P_G2_P4Q0 
    {0x3760, 0x568F,WORD_LEN 
    },         //  P_G2_P4Q1 
    {0x3762, 0x8E51,WORD_LEN},         //  P_G2_P4Q2 
    {0x3764, 0xECB1,WORD_LEN},         //  P_G2_P4Q3 
    {0x3766, 0x44B5,WORD_LEN},         //  P_G2_P4Q4 
    {0x3784, 0x0280,WORD_LEN},         //  CENTER_COLUMN 
    {0x3782, 0x01EC,WORD_LEN},         //  CENTER_ROW 
    {0x37C0, 0x83C7,WORD_LEN},         //  P_GR_Q5 
    {0x37C2, 0xEB89,WORD_LEN},         //  P_RD_Q5 
    {0x37C4, 0xD089,WORD_LEN},         //  P_BL_Q5 
    {0x37C6, 0x9187,WORD_LEN},         //  P_GB_Q5 
    {0x098E, 0x0000,WORD_LEN},         //  LOGICAL addressing 
    {0xC960, 0x0A8C,WORD_LEN},         //  CAM_PGA_L_CONFIG_COLOUR_TEMP 
    {0xC962, 0x7584,WORD_LEN},         //  CAM_PGA_L_CONFIG_GREEN_RED_Q14 
    {0xC964, 0x5900,WORD_LEN},         //  CAM_PGA_L_CONFIG_RED_Q14 
    {0xC966, 0x75AE,WORD_LEN},         //  CAM_PGA_L_CONFIG_GREEN_BLUE_Q14 
    {0xC968, 0x7274,WORD_LEN},         //  CAM_PGA_L_CONFIG_BLUE_Q14 
    {0xC96A, 0x0FD2,WORD_LEN},         //  CAM_PGA_M_CONFIG_COLOUR_TEMP 
    {0xC96C, 0x8155,WORD_LEN},         //  CAM_PGA_M_CONFIG_GREEN_RED_Q14 
    {0xC96E, 0x8018,WORD_LEN},         //  CAM_PGA_M_CONFIG_RED_Q14 
    {0xC970, 0x814C,WORD_LEN},         //  CAM_PGA_M_CONFIG_GREEN_BLUE_Q14 
    {0xC972, 0x836B,WORD_LEN},         //  CAM_PGA_M_CONFIG_BLUE_Q14 
    {0xC974, 0x1964,WORD_LEN},         //  CAM_PGA_R_CONFIG_COLOUR_TEMP 
    {0xC976, 0x7FDF,WORD_LEN},         //  CAM_PGA_R_CONFIG_GREEN_RED_Q14 
    {0xC978, 0x7F15,WORD_LEN},         //  CAM_PGA_R_CONFIG_RED_Q14 
    {0xC97A, 0x7FDC,WORD_LEN},         //  CAM_PGA_R_CONFIG_GREEN_BLUE_Q14 
    {0xC97C, 0x7F30,WORD_LEN},         //  CAM_PGA_R_CONFIG_BLUE_Q14 
    {0xC95E, 0x0003,WORD_LEN},         //  CAM_PGA_PGA_CONTROL 
    //[Step5-AWB_CCM]//default value 
    {0xC892, 0x0267,WORD_LEN},         // CAM_AWB_CCM_L_0 
    {0xC894, 0xFF1A,WORD_LEN},         // CAM_AWB_CCM_L_1 
    {0xC896, 0xFFB3,WORD_LEN},         // CAM_AWB_CCM_L_2 
    {0xC898, 0xFF80,WORD_LEN},         // CAM_AWB_CCM_L_3 
    {0xC89A, 0x0166,WORD_LEN},         // CAM_AWB_CCM_L_4 
    {0xC89C, 0x0003,WORD_LEN},         // CAM_AWB_CCM_L_5 
    {0xC89E, 0xFF9A,WORD_LEN},         // CAM_AWB_CCM_L_6 
    {0xC8A0, 0xFEB4,WORD_LEN},         // CAM_AWB_CCM_L_7 
    {0xC8A2, 0x024D,WORD_LEN},         // CAM_AWB_CCM_L_8 
    {0xC8A4, 0x01BF,WORD_LEN},         // CAM_AWB_CCM_M_0 
    {0xC8A6, 0xFF01,WORD_LEN},         // CAM_AWB_CCM_M_1 
    {0xC8A8, 0xFFF3,WORD_LEN},         // CAM_AWB_CCM_M_2 
    {0xC8AA, 0xFF75,WORD_LEN},         // CAM_AWB_CCM_M_3 
    {0xC8AC, 0x0198,WORD_LEN},         // CAM_AWB_CCM_M_4 
    {0xC8AE, 0xFFFD,WORD_LEN},         // CAM_AWB_CCM_M_5 
    {0xC8B0, 0xFF9A,WORD_LEN},         // CAM_AWB_CCM_M_6 
    {0xC8B2, 0xFEE7,WORD_LEN},         // CAM_AWB_CCM_M_7 
    {0xC8B4, 0x02A8,WORD_LEN},         // CAM_AWB_CCM_M_8 
    {0xC8B6, 0x01D9,WORD_LEN},         // CAM_AWB_CCM_R_0 
    {0xC8B8, 0xFF26,WORD_LEN},         // CAM_AWB_CCM_R_1 
    {0xC8BA, 0xFFF3,WORD_LEN},         // CAM_AWB_CCM_R_2 
    {0xC8BC, 0xFFB3,WORD_LEN},         // CAM_AWB_CCM_R_3 
    {0xC8BE, 0x0132,WORD_LEN},         // CAM_AWB_CCM_R_4 
    {0xC8C0, 0xFFE8,WORD_LEN},         // CAM_AWB_CCM_R_5 
    {0xC8C2, 0xFFDA,WORD_LEN},         // CAM_AWB_CCM_R_6 
    {0xC8C4, 0xFECD,WORD_LEN},         // CAM_AWB_CCM_R_7 
    {0xC8C6, 0x02C2,WORD_LEN},         // CAM_AWB_CCM_R_8 
    {0xC8C8, 0x0075,WORD_LEN},         // CAM_AWB_CCM_L_RG_GAIN 
    {0xC8CA, 0x011C,WORD_LEN},         // CAM_AWB_CCM_L_BG_GAIN 
    {0xC8CC, 0x009A,WORD_LEN},         // CAM_AWB_CCM_M_RG_GAIN 
    {0xC8CE, 0x0105,WORD_LEN},         // CAM_AWB_CCM_M_BG_GAIN 
    {0xC8D0, 0x00A4,WORD_LEN},         // CAM_AWB_CCM_R_RG_GAIN 
    {0xC8D2, 0x00AC,WORD_LEN},         // CAM_AWB_CCM_R_BG_GAIN 
    {0xC8D4, 0x0A8C,WORD_LEN},         // CAM_AWB_CCM_L_CTEMP 
    {0xC8D6, 0x0F0A,WORD_LEN},         // CAM_AWB_CCM_M_CTEMP 
    {0xC8D8, 0x1964,WORD_LEN},         // CAM_AWB_CCM_R_CTEMP 
    {0xC914, 0x0000,WORD_LEN},         // CAM_STAT_AWB_CLIP_WINDOW_XSTART 
    {0xC916, 0x0000,WORD_LEN},         // CAM_STAT_AWB_CLIP_WINDOW_YSTART 
    {0xC918, 0x04FF,WORD_LEN},         // CAM_STAT_AWB_CLIP_WINDOW_XEND 
    {0xC91A, 0x02CF,WORD_LEN},         // CAM_STAT_AWB_CLIP_WINDOW_YEND 
    {0xC904, 0x0033,WORD_LEN},         // CAM_AWB_AWB_XSHIFT_PRE_ADJ 
    {0xC906, 0x0040,WORD_LEN},         // CAM_AWB_AWB_YSHIFT_PRE_ADJ 
    {0xC8F2, 0x03,BYTE_LEN         },// CAM_AWB_AWB_XSCALE 
    {0xC8F3, 0x02,BYTE_LEN         },// CAM_AWB_AWB_YSCALE 
    {0xC906, 0x003C,WORD_LEN},         // CAM_AWB_AWB_YSHIFT_PRE_ADJ 
    {0xC8F4, 0x0000,WORD_LEN},         // CAM_AWB_AWB_WEIGHTS_0 
    {0xC8F6, 0x0000,WORD_LEN},         // CAM_AWB_AWB_WEIGHTS_1 
    {0xC8F8, 0x0000,WORD_LEN},         // CAM_AWB_AWB_WEIGHTS_2 
    {0xC8FA, 0xE724,WORD_LEN},         // CAM_AWB_AWB_WEIGHTS_3 
    {0xC8FC, 0x1583,WORD_LEN},         // CAM_AWB_AWB_WEIGHTS_4 
    {0xC8FE, 0x2045,WORD_LEN},         // CAM_AWB_AWB_WEIGHTS_5 
    {0xC900, 0x03FF,WORD_LEN},         // CAM_AWB_AWB_WEIGHTS_6 
    {0xC902, 0x007C,WORD_LEN},         // CAM_AWB_AWB_WEIGHTS_7 
    {0xC90C, 0x80,BYTE_LEN},         // CAM_AWB_K_R_L 
    {0xC90D, 0x80,BYTE_LEN},         // CAM_AWB_K_G_L 
    {0xC90E, 0x80,BYTE_LEN},         // CAM_AWB_K_B_L 
    {0xC90F, 0x88,BYTE_LEN},         // CAM_AWB_K_R_R 
    {0xC910, 0x80,BYTE_LEN},         // CAM_AWB_K_G_R 
    {0xC911, 0x80,BYTE_LEN},         // CAM_AWB_K_B_R 
    //[Step7-CPIPE_Preference] 
    {0x098E, 0x4926,WORD_LEN},         // LOGICAL_ADDRESS_ACCESS [CAM_LL_START_BRIGHTNESS] 
    {0xC926, 0x0020,WORD_LEN},         // CAM_LL_START_BRIGHTNESS 
    {0xC928, 0x009A,WORD_LEN},         // CAM_LL_STOP_BRIGHTNESS 
    {0xC946, 0x0070,WORD_LEN},         // CAM_LL_START_GAIN_METRIC 
    {0xC948, 0x00F3,WORD_LEN},         // CAM_LL_STOP_GAIN_METRIC 
    {0xC952, 0x0020,WORD_LEN},         // CAM_LL_START_TARGET_LUMA_BM 
    {0xC954, 0x009A,WORD_LEN},         // CAM_LL_STOP_TARGET_LUMA_BM 
    {0xC92A, 0x80,BYTE_LEN},        // CAM_LL_START_SATURATION 
    {0xC92B, 0x4B,BYTE_LEN},        // CAM_LL_END_SATURATION 
    {0xC92C, 0x00,BYTE_LEN},        // CAM_LL_START_DESATURATION 
    {0xC92D, 0xFF,BYTE_LEN},        // CAM_LL_END_DESATURATION 
    {0xC92E, 0x3C,BYTE_LEN},        // CAM_LL_START_DEMOSAIC 
    {0xC92F, 0x02,BYTE_LEN},        // CAM_LL_START_AP_GAIN 
    {0xC930, 0x06,BYTE_LEN},        // CAM_LL_START_AP_THRESH 
    {0xC931, 0x64,BYTE_LEN},        // CAM_LL_STOP_DEMOSAIC 
    {0xC932, 0x01,BYTE_LEN},        // CAM_LL_STOP_AP_GAIN 
    {0xC933, 0x0C,BYTE_LEN},        // CAM_LL_STOP_AP_THRESH 
    {0xC934, 0x3C,BYTE_LEN},        // CAM_LL_START_NR_RED 
    {0xC935, 0x3C,BYTE_LEN},        // CAM_LL_START_NR_GREEN 
    {0xC936, 0x3C,BYTE_LEN},        // CAM_LL_START_NR_BLUE 
    {0xC937, 0x0F,BYTE_LEN},        // CAM_LL_START_NR_THRESH 
    {0xC938, 0x64,BYTE_LEN},        // CAM_LL_STOP_NR_RED 
    {0xC939, 0x64,BYTE_LEN},        // CAM_LL_STOP_NR_GREEN 
    {0xC93A, 0x64,BYTE_LEN} ,        // CAM_LL_STOP_NR_BLUE 
    {0xC93B, 0x32,BYTE_LEN},        // CAM_LL_STOP_NR_THRESH 
    {0xC93C, 0x0020,WORD_LEN},         // CAM_LL_START_CONTRAST_BM 
    {0xC93E, 0x009A,WORD_LEN},         // CAM_LL_STOP_CONTRAST_BM 
    {0xC940, 0x00DC,WORD_LEN},         // CAM_LL_GAMMA 
    {0xC942, 0x38,BYTE_LEN},         // CAM_LL_START_CONTRAST_GRADIENT 
    {0xC943, 0x30,BYTE_LEN},         // CAM_LL_STOP_CONTRAST_GRADIENT 
    {0xC944, 0x50,BYTE_LEN},         // CAM_LL_START_CONTRAST_LUMA_PERCENTAGE 
    {0xC945, 0x19,BYTE_LEN},         // CAM_LL_STOP_CONTRAST_LUMA_PERCENTAGE 
    {0xC94A, 0x0230,WORD_LEN},         // CAM_LL_START_FADE_TO_BLACK_LUMA 
    {0xC94C, 0x0010,WORD_LEN},         // CAM_LL_STOP_FADE_TO_BLACK_LUMA 
    {0xC94E, 0x01CD,WORD_LEN},         // CAM_LL_CLUSTER_DC_TH_BM 
    {0xC950, 0x05,BYTE_LEN },        // CAM_LL_CLUSTER_DC_GATE_PERCENTAGE 
    {0xC951, 0x40,BYTE_LEN },        // CAM_LL_SUMMING_SENSITIVITY_FACTOR 
    {0xC87B, 0x1B,BYTE_LEN},         // CAM_AET_TARGET_AVERAGE_LUMA_DARK 
    {0xC878, 0x0E,BYTE_LEN},         // CAM_AET_AEMODE 
    {0xC890, 0x0080,WORD_LEN},         // CAM_AET_TARGET_GAIN 
    {0xC886, 0x0100,WORD_LEN},         // CAM_AET_AE_MAX_VIRT_AGAIN 
    {0xC87C, 0x005A,WORD_LEN},         // CAM_AET_BLACK_CLIPPING_TARGET 
    {0xB42A, 0x05,BYTE_LEN},         // CCM_DELTA_GAIN 
    {0xA80A, 0x20,BYTE_LEN},         // AE_TRACK_AE_TRACKING_DAMPENING_SPEED 
    //[Step8-Features] // For Parallel,2012-5-16 14:33 
    {0x098E, 0x0000,WORD_LEN},         // LOGICAL_ADDRESS_ACCESS 
    {0xC984, 0x8040,WORD_LEN},         // CAM_PORT_OUTPUT_CONTROL 
    {0xC984, 0x8040,WORD_LEN},          // CAM_PORT_OUTPUT_CONTROL, parallel, 2012-3-21 0:41 
    {0x001E, 0x0777,WORD_LEN},         // PAD_SLEW, for parallel, 2012-5-16 14:34 //DELAY=50 //add 2012-4-1 13:42 // end, 2012-5-16 14:33 
    // For MIPI, 2012-5-16 14:33 
    {0x098E, 0x0000,WORD_LEN},         // LOGICAL_ADDRESS_ACCESS 
    {0xC984, 0x8041,WORD_LEN},         // CAM_PORT_OUTPUT_CONTROL 
    {0xC988, 0x0F00,WORD_LEN},         // CAM_PORT_MIPI_TIMING_T_HS_ZERO 
    {0xC98A, 0x0B07,WORD_LEN},         // CAM_PORT_MIPI_TIMING_T_HS_EXIT_HS_TRAIL 
    {0xC98C, 0x0D01,WORD_LEN},         // CAM_PORT_MIPI_TIMING_T_CLK_POST_CLK_PRE 
    {0xC98E, 0x071D,WORD_LEN},         // CAM_PORT_MIPI_TIMING_T_CLK_TRAIL_CLK_ZERO 
    {0xC990, 0x0006,WORD_LEN},         // CAM_PORT_MIPI_TIMING_T_LPX 
    {0xC992, 0x0A0C,WORD_LEN},         // CAM_PORT_MIPI_TIMING_INIT_TIMING 
    {0x3C5A, 0x0009,WORD_LEN},         // MIPI_DELAY_TRIM 
    //[Anti-Flicker for MT9M114][50Hz] 
    {0x098E, 0xC88B,WORD_LEN},  // LOGICAL_ADDRESS_ACCESS [CAM_AET_FLICKER_FREQ_HZ] 
    {0xC88B, 0x32,BYTE_LEN  },  // CAM_AET_FLICKER_FREQ_HZ 
    // Saturation 
    {0xC92A,0x84,BYTE_LEN}, 
    {0xC92B,0x46,BYTE_LEN}, 
    // AE //Reg = 0xC87A,0x48 
   // {0xC87A,0x3C,BYTE_LEN},//0x3C 
 //  {0xC87A,0x46,BYTE_LEN},//0x3C 
 {0xC87A,0x43,BYTE_LEN},//0x3C 
    // Sharpness 
    {0x098E,0xC92F,WORD_LEN}, 
    {0xC92F,0x01,BYTE_LEN  }, 
    {0xC932,0x00,BYTE_LEN  }, 
    // Target Gain 
    {0x098E,0x4890,WORD_LEN}, 
    {0xC890,0x0040,WORD_LEN}, 
    { 0xc940,0x00C8,WORD_LEN}, // CAM_LL_GAMMA,2012-5-17 14:42 
    {0x3C40, 0x783C,WORD_LEN},         // MIPI_CONTROL 
    //[Change-Config] 
    {0x098E, 0xDC00,WORD_LEN},          // LOGICAL_ADDRESS_ACCESS [SYSMGR_NEXT_STATE] 
    {0xDC00, 0x28,BYTE_LEN         },// SYSMGR_NEXT_STATE 
    {0x0080, 0x8002,WORD_LEN},         // COMMAND_REGISTER //  POLL  COMMAND_REGISTER::HOST_COMMAND_1 =>  0x00 ////DELAY=100 //DELAY=150 {MT9M114_TABLE_WAIT_MS, 150}, 
    
  // {MT9M114_TABLE_WAIT_MS, 200}, 
  {MT9M114_TABLE_WAIT_MS, 100}, 
   
    {MT9M114_TABLE_END, 0x00} 
}; 

static struct mt9m114_reg mode_1280x720[] =
{
    //DELAY=10 
    {MT9M114_TABLE_WAIT_MS, 10},
    {0x3C40, 0x003C,WORD_LEN },	// MIPI_CONTROL
    {0x301A, 0x0230,WORD_LEN },	// RESET_REGISTER 
    {0x098E, 0x1000,WORD_LEN },	// LOGICAL_ADDRESS_ACCESS 
    {0xC97E, 0x01 ,BYTE_LEN	},// CAM_SYSCTL_PLL_ENABLE 
    //REG= 0xC980, 0x0225 	
    // CAM_SYSCTL_PLL_DIVIDER_M_N 
    {0xC980, 0x0120,WORD_LEN}, 	// CAM_SYSCTL_PLL_DIVIDER_M_N 
    {0xC982, 0x0700,WORD_LEN}, 	// CAM_SYSCTL_PLL_DIVIDER_P //DELAY=10 
    {MT9M114_TABLE_WAIT_MS, 10}, 
 
      // select output size, YUV format
    {0xC800, 0x007C,WORD_LEN},		//cam_sensor_cfg_y_addr_start = 124
    {0xC802, 0x0004,WORD_LEN},		//cam_sensor_cfg_x_addr_start = 4
    {0xC804, 0x0353,WORD_LEN},		//cam_sensor_cfg_y_addr_end = 851
    {0xC806, 0x050B,WORD_LEN},		//cam_sensor_cfg_x_addr_end = 1291
   // {0xC808, 0x2DC6,WORD_LEN},	//C00	//cam_sensor_cfg_pixclk = 48000000
    {0xC808, 0x02DC, WORD_LEN},//C00		//cam_sensor_cfg_pixclk = 48000000
    {0xC80A, 0x6C00, WORD_LEN},//C00		//cam_sensor_cfg_pixclk = 48000000
    {0xC80C, 0x0001,WORD_LEN},		//cam_sensor_cfg_row_speed = 1
    {0xC80E, 0x00DB,WORD_LEN},		//cam_sensor_cfg_fine_integ_time_min = 219
    {0xC810, 0x0679,WORD_LEN},		//cam_sensor_cfg_fine_integ_time_max = 1657
    {0xC812, 0x037E,WORD_LEN},		//cam_sensor_cfg_frame_length_lines = 894
    {0xC814, 0x06FC,WORD_LEN},		//cam_sensor_cfg_line_length_pck = 1788
    {0xC816, 0x0060,WORD_LEN},		//cam_sensor_cfg_fine_correction = 96
    {0xC818, 0x02D3,WORD_LEN},		//cam_sensor_cfg_cpipe_last_row = 723
    {0xC826, 0x0020,WORD_LEN},		//cam_sensor_cfg_reg_0_data = 32
    {0xC834, 0x0003,WORD_LEN},		//cam_sensor_control_read_mode = 0
    {0xC854, 0x0000,WORD_LEN},		//cam_crop_window_xoffset = 0
    {0xC856, 0x0000,WORD_LEN},		//cam_crop_window_yoffset = 0
    {0xC858, 0x0500,WORD_LEN},		//cam_crop_window_width = 1280
    {0xC85A, 0x02D0,WORD_LEN},		//cam_crop_window_height = 720
    {0xC85C, 0x03,BYTE_LEN},		//cam_crop_cropmode = 3
    {0xC868, 0x0500,WORD_LEN},		//cam_output_width = 1280
    {0xC86A, 0x02D0,WORD_LEN},		//cam_output_height = 720
    {0xC878, 0x00,BYTE_LEN},		//cam_aet_aemode = 0
    {0xC88C, 0x1E07,WORD_LEN},		//cam_aet_max_frame_rate = 7687
    {0xC88E, 0x0F00,WORD_LEN},		//cam_aet_min_frame_rate = 3840
    {0xC914, 0x0000,WORD_LEN},		//cam_stat_awb_clip_window_xstart = 0
    {0xC916, 0x0000,WORD_LEN},		//cam_stat_awb_clip_window_ystart = 0
    {0xC918, 0x04FF,WORD_LEN},		//cam_stat_awb_clip_window_xend = 1279
    {0xC91A, 0x02CF,WORD_LEN},		//cam_stat_awb_clip_window_yend = 719
    {0xC91C, 0x0000,WORD_LEN},		//cam_stat_ae_initial_window_xstart = 0
    {0xC91E, 0x0000,WORD_LEN},		//cam_stat_ae_initial_window_ystart = 0
    {0xC920, 0x00FF,WORD_LEN},		//cam_stat_ae_initial_window_xend = 255
    {0xC922, 0x008F,WORD_LEN},		//cam_stat_ae_initial_window_yend = 143

    //[Step3-Recommended] 
    {0x316A, 0x8270,WORD_LEN}, 	// DAC_TXLO_ROW 
    {0x316C, 0x8270,WORD_LEN}, 	// DAC_TXLO 
    {0x3ED0, 0x2305,WORD_LEN}, 	// DAC_LD_4_5
    {0x3ED2, 0x77CF,WORD_LEN}, 	// DAC_LD_6_7
    {0x316E, 0x8202,WORD_LEN}, 	// DAC_ECL 
    {0x3180, 0x87FF,WORD_LEN}, 	// DELTA_DK_CONTROL 
    {0x30D4, 0x6080,WORD_LEN}, 	// COLUMN_CORRECTION
    {0xA802, 0x0008,WORD_LEN}, 	// AE_TRACK_MODE
    {0x3E14, 0xFF39,WORD_LEN}, 	// SAMP_COL_PUP2
    //[patch 1204]for 720P 
    {0x0982, 0x0001,WORD_LEN}, 	// ACCESS_CTL_STAT 
    {0x098A, 0x60BC,WORD_LEN}, 	// PHYSICAL_ADDRESS_ACCESS
    {0xE0BC, 0xC0F1,WORD_LEN}, 
    {0xE0BE, 0x082A,WORD_LEN}, 
    {0xE0C0, 0x05A0,WORD_LEN}, 
    {0xE0C2, 0xD800,WORD_LEN}, 
    {0xE0C4, 0x71CF,WORD_LEN}, 
    {0xE0C6, 0xFFFF,WORD_LEN}, 
    {0xE0C8, 0xC344,WORD_LEN}, 
    {0xE0CA, 0x77CF,WORD_LEN}, 
    {0xE0CC, 0xFFFF,WORD_LEN}, 
    {0xE0CE, 0xC7C0,WORD_LEN}, 
    {0xE0D0, 0xB104,WORD_LEN}, 
    {0xE0D2, 0x8F1F,WORD_LEN}, 
    {0xE0D4, 0x75CF,WORD_LEN}, 
    {0xE0D6, 0xFFFF,WORD_LEN}, 
    {0xE0D8, 0xC84C,WORD_LEN},
    {0xE0DA, 0x0811,WORD_LEN}, 
    {0xE0DC, 0x005E,WORD_LEN}, 
    {0xE0DE, 0x70CF,WORD_LEN}, 
    {0xE0E0, 0x0000,WORD_LEN}, 
    {0xE0E2, 0x500E,WORD_LEN}, 
    {0xE0E4, 0x7840,WORD_LEN}, 
    {0xE0E6, 0xF019,WORD_LEN}, 
    {0xE0E8, 0x0CC6,WORD_LEN},
    {0xE0EA, 0x0340,WORD_LEN}, 
    {0xE0EC, 0x0E26,WORD_LEN}, 
    {0xE0EE, 0x0340,WORD_LEN}, 
    {0xE0F0, 0x95C2,WORD_LEN}, 
    {0xE0F2, 0x0E21,WORD_LEN}, 
    {0xE0F4, 0x101E,WORD_LEN},
    {0xE0F6, 0x0E0D,WORD_LEN},
    {0xE0F8, 0x119E,WORD_LEN}, 
    {0xE0FA, 0x0D56,WORD_LEN},
    {0xE0FC, 0x0340,WORD_LEN}, 
    {0xE0FE, 0xF008,WORD_LEN}, 
    {0xE100, 0x2650,WORD_LEN}, 
    {0xE102, 0x1040,WORD_LEN}, 
    {0xE104, 0x0AA2,WORD_LEN},
    {0xE106, 0x0360,WORD_LEN}, 
    {0xE108, 0xB502,WORD_LEN},
    {0xE10A, 0xB5C2,WORD_LEN},
    {0xE10C, 0x0B22,WORD_LEN}, 
    {0xE10E, 0x0400,WORD_LEN},
    {0xE110, 0x0CCE,WORD_LEN},
    {0xE112, 0x0320,WORD_LEN}, 
    {0xE114, 0xD800,WORD_LEN}, 
    {0xE116, 0x70CF,WORD_LEN}, 
    {0xE118, 0xFFFF,WORD_LEN}, 
    {0xE11A, 0xC5D4,WORD_LEN},
    {0xE11C, 0x902C,WORD_LEN}, 
    {0xE11E, 0x72CF,WORD_LEN}, 
    {0xE120, 0xFFFF,WORD_LEN},
    {0xE122, 0xE218,WORD_LEN},
    {0xE124, 0x9009,WORD_LEN},
    {0xE126, 0xE105,WORD_LEN}, 
    {0xE128, 0x73CF,WORD_LEN}, 
    {0xE12A, 0xFF00,WORD_LEN}, 
    {0xE12C, 0x2FD0,WORD_LEN}, 
    {0xE12E, 0x7822,WORD_LEN}, 
    {0xE130, 0x7910,WORD_LEN}, 
    {0xE132, 0xB202,WORD_LEN}, 
    {0xE134, 0x1382,WORD_LEN}, 
    {0xE136, 0x0700,WORD_LEN}, 
    {0xE138, 0x0815,WORD_LEN}, 
    {0xE13A, 0x03DE,WORD_LEN}, 
    {0xE13C, 0x1387,WORD_LEN}, 
    {0xE13E, 0x0700,WORD_LEN}, 
    {0xE140, 0x2102,WORD_LEN}, 
    {0xE142, 0x000A,WORD_LEN}, 
    {0xE144, 0x212F,WORD_LEN}, 
    {0xE146, 0x0288,WORD_LEN}, 
    {0xE148, 0x1A04,WORD_LEN}, 
    {0xE14A, 0x0284,WORD_LEN},
    {0xE14C, 0x13B9,WORD_LEN}, 
    {0xE14E, 0x0700,WORD_LEN}, 
    {0xE150, 0xB8C1,WORD_LEN}, 
    {0xE152, 0x0815,WORD_LEN}, 
    {0xE154, 0x0052,WORD_LEN}, 
    {0xE156, 0xDB00,WORD_LEN}, 
    {0xE158, 0x230F,WORD_LEN}, 
    {0xE15A, 0x0003,WORD_LEN},
    {0xE15C, 0x2102,WORD_LEN},
    {0xE15E, 0x00C0,WORD_LEN}, 
    {0xE160, 0x7910,WORD_LEN}, 
    {0xE162, 0xB202,WORD_LEN},
    {0xE164, 0x9507,WORD_LEN}, 
    {0xE166, 0x7822,WORD_LEN}, 
    {0xE168, 0xE080,WORD_LEN}, 
    {0xE16A, 0xD900,WORD_LEN},
    {0xE16C, 0x20CA,WORD_LEN}, 
    {0xE16E, 0x004B,WORD_LEN}, 
    {0xE170, 0xB805,WORD_LEN}, 
    {0xE172, 0x9533,WORD_LEN}, 
    {0xE174, 0x7815,WORD_LEN}, 
    {0xE176, 0x6038,WORD_LEN},
    {0xE178, 0x0FB2,WORD_LEN},
    {0xE17A, 0x0560,WORD_LEN},
    {0xE17C, 0xB861,WORD_LEN},
    {0xE17E, 0xB711,WORD_LEN},
    {0xE180, 0x0775,WORD_LEN}, 
    {0xE182, 0x0540,WORD_LEN}, 
    {0xE184, 0xD900,WORD_LEN},
    {0xE186, 0xF00A,WORD_LEN}, 
    {0xE188, 0x70CF,WORD_LEN},
    {0xE18A, 0xFFFF,WORD_LEN}, 
    {0xE18C, 0xE210,WORD_LEN},
    {0xE18E, 0x7835,WORD_LEN},
    {0xE190, 0x8041,WORD_LEN}, 
    {0xE192, 0x8000,WORD_LEN}, 
    {0xE194, 0xE102,WORD_LEN},
    {0xE196, 0xA040,WORD_LEN},
    {0xE198, 0x09F1,WORD_LEN}, 
    {0xE19A, 0x8094,WORD_LEN},
    {0xE19C, 0x7FE0,WORD_LEN},
    {0xE19E, 0xD800,WORD_LEN}, 
    {0xE1A0, 0xC0F1,WORD_LEN}, 
    {0xE1A2, 0xC5E1,WORD_LEN},
    {0xE1A4, 0x71CF,WORD_LEN}, 
    {0xE1A6, 0x0000,WORD_LEN}, 
    {0xE1A8, 0x45E6,WORD_LEN}, 
    {0xE1AA, 0x7960,WORD_LEN}, 
    {0xE1AC, 0x7508,WORD_LEN},
    {0xE1AE, 0x70CF,WORD_LEN},
    {0xE1B0, 0xFFFF,WORD_LEN}, 
    {0xE1B2, 0xC84C,WORD_LEN}, 
    {0xE1B4, 0x9002,WORD_LEN}, 
    {0xE1B6, 0x083D,WORD_LEN}, 
    {0xE1B8, 0x021E,WORD_LEN}, 
    {0xE1BA, 0x0D39,WORD_LEN},
    {0xE1BC, 0x10D1,WORD_LEN}, 
    {0xE1BE, 0x70CF,WORD_LEN}, 
    {0xE1C0, 0xFF00,WORD_LEN}, 
    {0xE1C2, 0x3354,WORD_LEN}, 
    {0xE1C4, 0x9055,WORD_LEN}, 
    {0xE1C6, 0x71CF,WORD_LEN}, 
    {0xE1C8, 0xFFFF,WORD_LEN}, 
    {0xE1CA, 0xC5D4,WORD_LEN}, 
    {0xE1CC, 0x116C,WORD_LEN}, 
    {0xE1CE, 0x0103,WORD_LEN}, 
    {0xE1D0, 0x1170,WORD_LEN}, 
    {0xE1D2, 0x00C1,WORD_LEN},
    {0xE1D4, 0xE381,WORD_LEN}, 
    {0xE1D6, 0x22C6,WORD_LEN},
    {0xE1D8, 0x0F81,WORD_LEN}, 
    {0xE1DA, 0x0000,WORD_LEN}, 
    {0xE1DC, 0x00FF,WORD_LEN}, 
    {0xE1DE, 0x22C4,WORD_LEN},
    {0xE1E0, 0x0F82,WORD_LEN}, 
    {0xE1E2, 0xFFFF,WORD_LEN}, 
    {0xE1E4, 0x00FF,WORD_LEN}, 
    {0xE1E6, 0x29C0,WORD_LEN}, 
    {0xE1E8, 0x0222,WORD_LEN}, 
    {0xE1EA, 0x7945,WORD_LEN}, 
    {0xE1EC, 0x7930,WORD_LEN}, 
    {0xE1EE, 0xB035,WORD_LEN}, 
    {0xE1F0, 0x0715,WORD_LEN},
    {0xE1F2, 0x0540,WORD_LEN}, 
    {0xE1F4, 0xD900,WORD_LEN},
    {0xE1F6, 0xF00A,WORD_LEN}, 
    {0xE1F8, 0x70CF,WORD_LEN}, 
    {0xE1FA, 0xFFFF,WORD_LEN}, 
    {0xE1FC, 0xE224,WORD_LEN},
    {0xE1FE, 0x7835,WORD_LEN}, 
    {0xE200, 0x8041,WORD_LEN}, 
    {0xE202, 0x8000,WORD_LEN}, 
    {0xE204, 0xE102,WORD_LEN}, 
    {0xE206, 0xA040,WORD_LEN}, 
    {0xE208, 0x09F1,WORD_LEN}, 
    {0xE20A, 0x8094,WORD_LEN}, 
    {0xE20C, 0x7FE0,WORD_LEN}, 
    {0xE20E, 0xD800,WORD_LEN}, 
    {0xE210, 0xFFFF,WORD_LEN},
    {0xE212, 0xCB40,WORD_LEN},
    {0xE214, 0xFFFF,WORD_LEN}, 
    {0xE216, 0xE0BC,WORD_LEN},
    {0xE218, 0x0000,WORD_LEN},
    {0xE21A, 0x0000,WORD_LEN},
    {0xE21C, 0x0000,WORD_LEN}, 
    {0xE21E, 0x0000,WORD_LEN}, 
    {0xE220, 0x0000,WORD_LEN},
    {0x098E, 0x0000,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS
    {0xE000, 0x1184,WORD_LEN}, 	// PATCHLDR_LOADER_ADDRESS
    {0xE002, 0x1204,WORD_LEN}, 	// PATCHLDR_PATCH_ID 
    {0xE004, 0x4103,WORD_LEN},   //0202 
    {0xE006, 0x0202,WORD_LEN},  
    // PATCHLDR_FIRMWARE_ID //REG= 0xE004, 0x4103 //REG= 0xE006, 0x0202
    {0x0080, 0xFFF0,WORD_LEN}, 	
    // COMMAND_REGISTER //  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00
    //  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00
    //  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00 
    // 读Reg= 0x080， 判断其最低位是否为0， 如果不为0，则delay 5ms，然后继续读， // 直到为0或者   50ms以上
    //delay = 50 
    {MT9M114_TABLE_WAIT_MS, 50}, 
    {0x0080, 0xFFF1,WORD_LEN}, 	// COMMAND_REGISTER 
    //  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00 // 读Reg= 0x080， 判断其最低位是否为0， 如果不为0，则delay 5ms，然后继续读， // 直到为0或者   50ms以上
    //delay = 50 
    {MT9M114_TABLE_WAIT_MS, 50}, 
    // AWB Start point 
    {0x098E,0x2c12,WORD_LEN}, 
    {0xac12,0x008f,WORD_LEN}, 
    {0xac14,0x0105,WORD_LEN},
    //[Step4-APGA //LSC] // [APGA Settings 85% 2012/05/02 12:16:56] 
    {0x3640, 0x00B0,WORD_LEN}, 	//  P_G1_P0Q0 
    {0x3642, 0xC0EA,WORD_LEN}, 	//  P_G1_P0Q1
    {0x3644, 0x3E70,WORD_LEN}, 	//  P_G1_P0Q2
    {0x3646, 0x182B,WORD_LEN}, 	//  P_G1_P0Q3 
    {0x3648, 0xCC4D,WORD_LEN}, 	//  P_G1_P0Q4
    {0x364A, 0x0150,WORD_LEN}, 	//  P_R_P0Q0 
    {0x364C, 0x9C4A,WORD_LEN}, 	//  P_R_P0Q1 
    {0x364E, 0x6DF0,WORD_LEN}, 	//  P_R_P0Q2
    {0x3650, 0xE5CA,WORD_LEN}, 	//  P_R_P0Q3 
    {0x3652, 0xA22F,WORD_LEN}, 	//  P_R_P0Q4
    {0x3654, 0x0170,WORD_LEN}, 	//  P_B_P0Q0
    {0x3656, 0xC288,WORD_LEN}, 	//  P_B_P0Q1 
    {0x3658, 0x3010,WORD_LEN}, 	//  P_B_P0Q2
    {0x365A, 0xBA26,WORD_LEN}, 	//  P_B_P0Q3
    {0x365C, 0xE1AE,WORD_LEN}, 	//  P_B_P0Q4 
    {0x365E, 0x00B0,WORD_LEN}, 	//  P_G2_P0Q0 
    {0x3660, 0xC34A,WORD_LEN}, 	//  P_G2_P0Q1 
    {0x3662, 0x3E90,WORD_LEN}, 	//  P_G2_P0Q2 
    {0x3664, 0x1DAB,WORD_LEN}, 	//  P_G2_P0Q3 
    {0x3666, 0xD0CD,WORD_LEN}, 	//  P_G2_P0Q4 
    {0x3680, 0x440B,WORD_LEN}, 	//  P_G1_P1Q0 
    {0x3682, 0x1386,WORD_LEN}, 	//  P_G1_P1Q1
    {0x3684, 0xAB0C,WORD_LEN}, 	//  P_G1_P1Q2 
    {0x3686, 0x1FCD,WORD_LEN}, 	//  P_G1_P1Q3
    {0x3688, 0xEACB,WORD_LEN}, 	//  P_G1_P1Q4
    {0x368A, 0x63C7,WORD_LEN}, 	//  P_R_P1Q0 
    {0x368C, 0x1006,WORD_LEN}, 	//  P_R_P1Q1 
    {0x368E, 0xE64C,WORD_LEN}, 	//  P_R_P1Q2 
    {0x3690, 0x7B4D,WORD_LEN}, 	//  P_R_P1Q3
    {0x3692, 0x32AF,WORD_LEN}, 	//  P_R_P1Q4
    {0x3694, 0x24EB,WORD_LEN}, 	//  P_B_P1Q0
    {0x3696, 0xFB6A,WORD_LEN}, 	//  P_B_P1Q1 
    {0x3698, 0xB96C,WORD_LEN}, 	//  P_B_P1Q2 
    {0x369A, 0x9AEA,WORD_LEN}, 	//  P_B_P1Q3 
    {0x369C, 0x092E,WORD_LEN}, 	//  P_B_P1Q4
    {0x369E, 0x432B,WORD_LEN}, 	//  P_G2_P1Q0 
    {0x36A0, 0xC7C5,WORD_LEN}, 	//  P_G2_P1Q1
    {0x36A2, 0xABEC,WORD_LEN}, 	//  P_G2_P1Q2 
    {0x36A4, 0x222D,WORD_LEN}, 	//  P_G2_P1Q3 
    {0x36A6, 0xE68B,WORD_LEN}, 	//  P_G2_P1Q4 
    {0x36C0, 0x0631,WORD_LEN}, 	//  P_G1_P2Q0
    {0x36C2, 0x946D,WORD_LEN}, 	//  P_G1_P2Q1 
    {0x36C4, 0xB6B1,WORD_LEN}, 	//  P_G1_P2Q2 
    {0x36C6, 0x5ECF,WORD_LEN}, 	//  P_G1_P2Q3
    {0x36C8, 0x25D2,WORD_LEN}, 	//  P_G1_P2Q4
    {0x36CA, 0x05F1,WORD_LEN}, 	//  P_R_P2Q0 
    {0x36CC, 0x934D,WORD_LEN}, 	//  P_R_P2Q1 
    {0x36CE, 0xB9F1,WORD_LEN}, 	//  P_R_P2Q2
    {0x36D0, 0x05AF,WORD_LEN}, 	//  P_R_P2Q3
    {0x36D2, 0x4C92,WORD_LEN}, 	//  P_R_P2Q4 
    {0x36D4, 0x37B0,WORD_LEN}, 	//  P_B_P2Q0 
    {0x36D6, 0x938B,WORD_LEN}, 	//  P_B_P2Q1 
    {0x36D8, 0xB4B0,WORD_LEN}, 	//  P_B_P2Q2
    {0x36DA, 0x6A8D,WORD_LEN}, 	//  P_B_P2Q3
    {0x36DC, 0x3951,WORD_LEN}, 	//  P_B_P2Q4 
    {0x36DE, 0x0611,WORD_LEN}, 	//  P_G2_P2Q0
    {0x36E0, 0xA28D,WORD_LEN}, 	//  P_G2_P2Q1 
    {0x36E2, 0xB6B1,WORD_LEN}, 	//  P_G2_P2Q2 
    {0x36E4, 0x6D6F,WORD_LEN}, 	//  P_G2_P2Q3 
    {0x36E6, 0x2652,WORD_LEN}, 	//  P_G2_P2Q4 
    {0x3700, 0x75CA,WORD_LEN}, 	//  P_G1_P3Q0
    {0x3702, 0x42AD,WORD_LEN}, 	//  P_G1_P3Q1 
    {0x3704, 0xB9AA,WORD_LEN}, 	//  P_G1_P3Q2 
    {0x3706, 0xA0EF,WORD_LEN}, 	//  P_G1_P3Q3 
    {0x3708, 0x29B2,WORD_LEN}, 	//  P_G1_P3Q4
    {0x370A, 0x830B,WORD_LEN}, 	//  P_R_P3Q0 
    {0x370C, 0x05AE,WORD_LEN}, 	//  P_R_P3Q1
    {0x370E, 0x30B0,WORD_LEN}, 	//  P_R_P3Q2 
    {0x3710, 0x88D0,WORD_LEN}, 	//  P_R_P3Q3
    {0x3712, 0x1AF0,WORD_LEN}, 	//  P_R_P3Q4 
    {0x3714, 0x746B,WORD_LEN}, 	//  P_B_P3Q0 
    {0x3716, 0x376D,WORD_LEN}, 	//  P_B_P3Q1
    {0x3718, 0x2E8E,WORD_LEN}, 	//  P_B_P3Q2
    {0x371A, 0x8C2B,WORD_LEN}, 	//  P_B_P3Q3 
    {0x371C, 0x0030,WORD_LEN}, 	//  P_B_P3Q4
    {0x371E, 0x050B,WORD_LEN}, 	//  P_G2_P3Q0
    {0x3720, 0x5C0D,WORD_LEN}, 	//  P_G2_P3Q1 
    {0x3722, 0x1E88,WORD_LEN}, 	//  P_G2_P3Q2 
    {0x3724, 0xB22F,WORD_LEN}, 	//  P_G2_P3Q3 
    {0x3726, 0x26B2,WORD_LEN}, 	//  P_G2_P3Q4 
    {0x3740, 0xFC90,WORD_LEN}, 	//  P_G1_P4Q0 
    {0x3742, 0x430F,WORD_LEN}, 	//  P_G1_P4Q1 
    {0x3744, 0x9151,WORD_LEN}, 	//  P_G1_P4Q2
    {0x3746, 0xDF71,WORD_LEN}, 	//  P_G1_P4Q3 
    {0x3748, 0x4575,WORD_LEN}, 	//  P_G1_P4Q4
    {0x374A, 0xC1CF,WORD_LEN}, 	//  P_R_P4Q0 
    {0x374C, 0x414F,WORD_LEN}, 	//  P_R_P4Q1 
    {0x374E, 0xD551,WORD_LEN}, 	//  P_R_P4Q2 
    {0x3750, 0x9451,WORD_LEN}, 	//  P_R_P4Q3
    {0x3752, 0x5A35,WORD_LEN}, 	//  P_R_P4Q4 

    {0x3754, 0x4C6C,WORD_LEN}, 	//  P_B_P4Q0
    {0x3756, 0x300F,WORD_LEN}, 	//  P_B_P4Q1 
    {0x3758, 0xB232,WORD_LEN}, 	//  P_B_P4Q2 
    {0x375A, 0x96F0,WORD_LEN}, 	//  P_B_P4Q3
    {0x375C, 0x47B5,WORD_LEN}, 	//  P_B_P4Q4 
    {0x375E, 0xFBD0,WORD_LEN}, 	//  P_G2_P4Q0
    {0x3760, 0x568F,WORD_LEN
    }, 	//  P_G2_P4Q1 
    {0x3762, 0x8E51,WORD_LEN}, 	//  P_G2_P4Q2
    {0x3764, 0xECB1,WORD_LEN}, 	//  P_G2_P4Q3 
    {0x3766, 0x44B5,WORD_LEN}, 	//  P_G2_P4Q4 
    {0x3784, 0x0280,WORD_LEN}, 	//  CENTER_COLUMN 
    {0x3782, 0x01EC,WORD_LEN}, 	//  CENTER_ROW 
    {0x37C0, 0x83C7,WORD_LEN}, 	//  P_GR_Q5 
    {0x37C2, 0xEB89,WORD_LEN}, 	//  P_RD_Q5 
    {0x37C4, 0xD089,WORD_LEN}, 	//  P_BL_Q5 
    {0x37C6, 0x9187,WORD_LEN}, 	//  P_GB_Q5 
    {0x098E, 0x0000,WORD_LEN}, 	//  LOGICAL addressing
    {0xC960, 0x0A8C,WORD_LEN}, 	//  CAM_PGA_L_CONFIG_COLOUR_TEMP 
    {0xC962, 0x7584,WORD_LEN}, 	//  CAM_PGA_L_CONFIG_GREEN_RED_Q14 
    {0xC964, 0x5900,WORD_LEN}, 	//  CAM_PGA_L_CONFIG_RED_Q14
    {0xC966, 0x75AE,WORD_LEN}, 	//  CAM_PGA_L_CONFIG_GREEN_BLUE_Q14 
    {0xC968, 0x7274,WORD_LEN}, 	//  CAM_PGA_L_CONFIG_BLUE_Q14 
    {0xC96A, 0x0FD2,WORD_LEN}, 	//  CAM_PGA_M_CONFIG_COLOUR_TEMP 
    {0xC96C, 0x8155,WORD_LEN}, 	//  CAM_PGA_M_CONFIG_GREEN_RED_Q14 
    {0xC96E, 0x8018,WORD_LEN}, 	//  CAM_PGA_M_CONFIG_RED_Q14
    {0xC970, 0x814C,WORD_LEN}, 	//  CAM_PGA_M_CONFIG_GREEN_BLUE_Q14
    {0xC972, 0x836B,WORD_LEN}, 	//  CAM_PGA_M_CONFIG_BLUE_Q14
    {0xC974, 0x1964,WORD_LEN}, 	//  CAM_PGA_R_CONFIG_COLOUR_TEMP 
    {0xC976, 0x7FDF,WORD_LEN}, 	//  CAM_PGA_R_CONFIG_GREEN_RED_Q14
    {0xC978, 0x7F15,WORD_LEN}, 	//  CAM_PGA_R_CONFIG_RED_Q14 
    {0xC97A, 0x7FDC,WORD_LEN}, 	//  CAM_PGA_R_CONFIG_GREEN_BLUE_Q14
    {0xC97C, 0x7F30,WORD_LEN}, 	//  CAM_PGA_R_CONFIG_BLUE_Q14 
    {0xC95E, 0x0003,WORD_LEN}, 	//  CAM_PGA_PGA_CONTROL
    //[Step5-AWB_CCM]//default value 
    {0xC892, 0x0267,WORD_LEN}, 	// CAM_AWB_CCM_L_0
    {0xC894, 0xFF1A,WORD_LEN}, 	// CAM_AWB_CCM_L_1
    {0xC896, 0xFFB3,WORD_LEN}, 	// CAM_AWB_CCM_L_2 
    {0xC898, 0xFF80,WORD_LEN}, 	// CAM_AWB_CCM_L_3 
    {0xC89A, 0x0166,WORD_LEN}, 	// CAM_AWB_CCM_L_4 
    {0xC89C, 0x0003,WORD_LEN}, 	// CAM_AWB_CCM_L_5 
    {0xC89E, 0xFF9A,WORD_LEN}, 	// CAM_AWB_CCM_L_6 
    {0xC8A0, 0xFEB4,WORD_LEN}, 	// CAM_AWB_CCM_L_7 
    {0xC8A2, 0x024D,WORD_LEN}, 	// CAM_AWB_CCM_L_8 
    {0xC8A4, 0x01BF,WORD_LEN}, 	// CAM_AWB_CCM_M_0
    {0xC8A6, 0xFF01,WORD_LEN}, 	// CAM_AWB_CCM_M_1 
    {0xC8A8, 0xFFF3,WORD_LEN}, 	// CAM_AWB_CCM_M_2 
    {0xC8AA, 0xFF75,WORD_LEN}, 	// CAM_AWB_CCM_M_3 
    {0xC8AC, 0x0198,WORD_LEN}, 	// CAM_AWB_CCM_M_4 
    {0xC8AE, 0xFFFD,WORD_LEN}, 	// CAM_AWB_CCM_M_5 
    {0xC8B0, 0xFF9A,WORD_LEN}, 	// CAM_AWB_CCM_M_6
    {0xC8B2, 0xFEE7,WORD_LEN}, 	// CAM_AWB_CCM_M_7 
    {0xC8B4, 0x02A8,WORD_LEN}, 	// CAM_AWB_CCM_M_8 
    {0xC8B6, 0x01D9,WORD_LEN}, 	// CAM_AWB_CCM_R_0 
    {0xC8B8, 0xFF26,WORD_LEN}, 	// CAM_AWB_CCM_R_1 
    {0xC8BA, 0xFFF3,WORD_LEN}, 	// CAM_AWB_CCM_R_2 
    {0xC8BC, 0xFFB3,WORD_LEN}, 	// CAM_AWB_CCM_R_3 
    {0xC8BE, 0x0132,WORD_LEN}, 	// CAM_AWB_CCM_R_4 
    {0xC8C0, 0xFFE8,WORD_LEN}, 	// CAM_AWB_CCM_R_5 
    {0xC8C2, 0xFFDA,WORD_LEN}, 	// CAM_AWB_CCM_R_6
    {0xC8C4, 0xFECD,WORD_LEN}, 	// CAM_AWB_CCM_R_7 
    {0xC8C6, 0x02C2,WORD_LEN}, 	// CAM_AWB_CCM_R_8 
    {0xC8C8, 0x0075,WORD_LEN}, 	// CAM_AWB_CCM_L_RG_GAIN 
    {0xC8CA, 0x011C,WORD_LEN}, 	// CAM_AWB_CCM_L_BG_GAIN 
    {0xC8CC, 0x009A,WORD_LEN}, 	// CAM_AWB_CCM_M_RG_GAIN 
    {0xC8CE, 0x0105,WORD_LEN}, 	// CAM_AWB_CCM_M_BG_GAIN 
    {0xC8D0, 0x00A4,WORD_LEN}, 	// CAM_AWB_CCM_R_RG_GAIN 
    {0xC8D2, 0x00AC,WORD_LEN}, 	// CAM_AWB_CCM_R_BG_GAIN 
    {0xC8D4, 0x0A8C,WORD_LEN}, 	// CAM_AWB_CCM_L_CTEMP 
    {0xC8D6, 0x0F0A,WORD_LEN}, 	// CAM_AWB_CCM_M_CTEMP
    {0xC8D8, 0x1964,WORD_LEN}, 	// CAM_AWB_CCM_R_CTEMP 
    {0xC914, 0x0000,WORD_LEN}, 	// CAM_STAT_AWB_CLIP_WINDOW_XSTART
    {0xC916, 0x0000,WORD_LEN}, 	// CAM_STAT_AWB_CLIP_WINDOW_YSTART
    {0xC918, 0x04FF,WORD_LEN}, 	// CAM_STAT_AWB_CLIP_WINDOW_XEND 
    {0xC91A, 0x02CF,WORD_LEN}, 	// CAM_STAT_AWB_CLIP_WINDOW_YEND
    {0xC904, 0x0033,WORD_LEN}, 	// CAM_AWB_AWB_XSHIFT_PRE_ADJ
    {0xC906, 0x0040,WORD_LEN}, 	// CAM_AWB_AWB_YSHIFT_PRE_ADJ 
    {0xC8F2, 0x03,BYTE_LEN 	},// CAM_AWB_AWB_XSCALE
    {0xC8F3, 0x02,BYTE_LEN 	},// CAM_AWB_AWB_YSCALE 
    {0xC906, 0x003C,WORD_LEN}, 	// CAM_AWB_AWB_YSHIFT_PRE_ADJ 
    {0xC8F4, 0x0000,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_0
    {0xC8F6, 0x0000,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_1 
    {0xC8F8, 0x0000,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_2 
    {0xC8FA, 0xE724,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_3 
    {0xC8FC, 0x1583,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_4 
    {0xC8FE, 0x2045,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_5 
    {0xC900, 0x03FF,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_6 
    {0xC902, 0x007C,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_7 
    {0xC90C, 0x80,BYTE_LEN}, 	// CAM_AWB_K_R_L 
    {0xC90D, 0x80,BYTE_LEN}, 	// CAM_AWB_K_G_L 
    {0xC90E, 0x80,BYTE_LEN}, 	// CAM_AWB_K_B_L 
    {0xC90F, 0x88,BYTE_LEN}, 	// CAM_AWB_K_R_R 
    {0xC910, 0x80,BYTE_LEN}, 	// CAM_AWB_K_G_R 
    {0xC911, 0x80,BYTE_LEN}, 	// CAM_AWB_K_B_R
    //[Step7-CPIPE_Preference]
    {0x098E, 0x4926,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS [CAM_LL_START_BRIGHTNESS] 
    {0xC926, 0x0020,WORD_LEN}, 	// CAM_LL_START_BRIGHTNESS 
    {0xC928, 0x009A,WORD_LEN}, 	// CAM_LL_STOP_BRIGHTNESS
    {0xC946, 0x0070,WORD_LEN}, 	// CAM_LL_START_GAIN_METRIC 
    {0xC948, 0x00F3,WORD_LEN}, 	// CAM_LL_STOP_GAIN_METRIC 
    {0xC952, 0x0020,WORD_LEN}, 	// CAM_LL_START_TARGET_LUMA_BM 
    {0xC954, 0x009A,WORD_LEN}, 	// CAM_LL_STOP_TARGET_LUMA_BM 
    {0xC92A, 0x80,BYTE_LEN},	// CAM_LL_START_SATURATION
    {0xC92B, 0x4B,BYTE_LEN},	// CAM_LL_END_SATURATION
    {0xC92C, 0x00,BYTE_LEN},	// CAM_LL_START_DESATURATION 
    {0xC92D, 0xFF,BYTE_LEN},	// CAM_LL_END_DESATURATION 
    {0xC92E, 0x3C,BYTE_LEN},	// CAM_LL_START_DEMOSAIC 
    {0xC92F, 0x02,BYTE_LEN},	// CAM_LL_START_AP_GAIN 
    {0xC930, 0x06,BYTE_LEN},	// CAM_LL_START_AP_THRESH
    {0xC931, 0x64,BYTE_LEN},	// CAM_LL_STOP_DEMOSAIC 
    {0xC932, 0x01,BYTE_LEN},	// CAM_LL_STOP_AP_GAIN 
    {0xC933, 0x0C,BYTE_LEN},	// CAM_LL_STOP_AP_THRESH 
    {0xC934, 0x3C,BYTE_LEN},	// CAM_LL_START_NR_RED
    {0xC935, 0x3C,BYTE_LEN},	// CAM_LL_START_NR_GREEN 
    {0xC936, 0x3C,BYTE_LEN},	// CAM_LL_START_NR_BLUE 
    {0xC937, 0x0F,BYTE_LEN},	// CAM_LL_START_NR_THRESH 
    {0xC938, 0x64,BYTE_LEN},	// CAM_LL_STOP_NR_RED 
    {0xC939, 0x64,BYTE_LEN},	// CAM_LL_STOP_NR_GREEN
    {0xC93A, 0x64,BYTE_LEN} ,	// CAM_LL_STOP_NR_BLUE 
    {0xC93B, 0x32,BYTE_LEN},	// CAM_LL_STOP_NR_THRESH 
    {0xC93C, 0x0020,WORD_LEN}, 	// CAM_LL_START_CONTRAST_BM 
    {0xC93E, 0x009A,WORD_LEN}, 	// CAM_LL_STOP_CONTRAST_BM 
    {0xC940, 0x00DC,WORD_LEN}, 	// CAM_LL_GAMMA 
    {0xC942, 0x38,BYTE_LEN}, 	// CAM_LL_START_CONTRAST_GRADIENT 
    {0xC943, 0x30,BYTE_LEN}, 	// CAM_LL_STOP_CONTRAST_GRADIENT 
    {0xC944, 0x50,BYTE_LEN}, 	// CAM_LL_START_CONTRAST_LUMA_PERCENTAGE 
    {0xC945, 0x19,BYTE_LEN}, 	// CAM_LL_STOP_CONTRAST_LUMA_PERCENTAGE 
    {0xC94A, 0x0230,WORD_LEN}, 	// CAM_LL_START_FADE_TO_BLACK_LUMA 
    {0xC94C, 0x0010,WORD_LEN}, 	// CAM_LL_STOP_FADE_TO_BLACK_LUMA 
    {0xC94E, 0x01CD,WORD_LEN}, 	// CAM_LL_CLUSTER_DC_TH_BM 
    {0xC950, 0x05,BYTE_LEN },	// CAM_LL_CLUSTER_DC_GATE_PERCENTAGE
    {0xC951, 0x40,BYTE_LEN },	// CAM_LL_SUMMING_SENSITIVITY_FACTOR 
    {0xC87B, 0x1B,BYTE_LEN}, 	// CAM_AET_TARGET_AVERAGE_LUMA_DARK
    {0xC878, 0x0E,BYTE_LEN}, 	// CAM_AET_AEMODE 
    {0xC890, 0x0080,WORD_LEN}, 	// CAM_AET_TARGET_GAIN 
    {0xC886, 0x0100,WORD_LEN}, 	// CAM_AET_AE_MAX_VIRT_AGAIN 
    {0xC87C, 0x005A,WORD_LEN}, 	// CAM_AET_BLACK_CLIPPING_TARGET 
    {0xB42A, 0x05,BYTE_LEN}, 	// CCM_DELTA_GAIN
    {0xA80A, 0x20,BYTE_LEN}, 	// AE_TRACK_AE_TRACKING_DAMPENING_SPEED
    //[Step8-Features] // For Parallel,2012-5-16 14:33 
    {0x098E, 0x0000,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS
    {0xC984, 0x8040,WORD_LEN}, 	// CAM_PORT_OUTPUT_CONTROL 
    {0xC984, 0x8040,WORD_LEN},  	// CAM_PORT_OUTPUT_CONTROL, parallel, 2012-3-21 0:41 
    {0x001E, 0x0777,WORD_LEN}, 	// PAD_SLEW, for parallel, 2012-5-16 14:34 //DELAY=50 //add 2012-4-1 13:42 // end, 2012-5-16 14:33
    // For MIPI, 2012-5-16 14:33 
    {0x098E, 0x0000,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS
    {0xC984, 0x8041,WORD_LEN}, 	// CAM_PORT_OUTPUT_CONTROL 
    {0xC988, 0x0F00,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_T_HS_ZERO 
    {0xC98A, 0x0B07,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_T_HS_EXIT_HS_TRAIL 
    {0xC98C, 0x0D01,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_T_CLK_POST_CLK_PRE 
    {0xC98E, 0x071D,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_T_CLK_TRAIL_CLK_ZERO 
    {0xC990, 0x0006,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_T_LPX 
    {0xC992, 0x0A0C,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_INIT_TIMING
    {0x3C5A, 0x0009,WORD_LEN}, 	// MIPI_DELAY_TRIM
    //[Anti-Flicker for MT9M114][50Hz] 
    {0x098E, 0xC88B,WORD_LEN},  // LOGICAL_ADDRESS_ACCESS [CAM_AET_FLICKER_FREQ_HZ] 
    {0xC88B, 0x32,BYTE_LEN  },  // CAM_AET_FLICKER_FREQ_HZ
    // Saturation
    {0xC92A,0x84,BYTE_LEN},
    {0xC92B,0x46,BYTE_LEN},
    // AE //Reg = 0xC87A,0x48 
   // {0xC87A,0x3C,BYTE_LEN},
 //  {0xC87A,0x3C,BYTE_LEN},//0x3C 
  {0xC87A,0x43,BYTE_LEN},//0x3C 
    // Sharpness
    {0x098E,0xC92F,WORD_LEN}, 
    {0xC92F,0x01,BYTE_LEN  }, 
    {0xC932,0x00,BYTE_LEN  },
    // Target Gain
    {0x098E,0x4890,WORD_LEN},
    {0xC890,0x0040,WORD_LEN},
    { 0xc940,0x00C8,WORD_LEN}, // CAM_LL_GAMMA,2012-5-17 14:42 
    {0x3C40, 0x783C,WORD_LEN}, 	// MIPI_CONTROL
    //[Change-Config] 
    {0x098E, 0xDC00,WORD_LEN},  	// LOGICAL_ADDRESS_ACCESS [SYSMGR_NEXT_STATE] 
    {0xDC00, 0x28,BYTE_LEN 	},// SYSMGR_NEXT_STATE 
    {0x0080, 0x8002,WORD_LEN}, 	// COMMAND_REGISTER //  POLL  COMMAND_REGISTER::HOST_COMMAND_1 =>  0x00 ////DELAY=100 //DELAY=150 {MT9M114_TABLE_WAIT_MS, 150}, 
  
    {MT9M114_TABLE_WAIT_MS, 100}, 
    {MT9M114_TABLE_END, 0x00}

};

static struct mt9m114_reg mode_640x480[] = { 

    //DELAY=10 
    {MT9M114_TABLE_WAIT_MS, 10},
    {0x3C40, 0x003C,WORD_LEN },	// MIPI_CONTROL
    {0x301A, 0x0230,WORD_LEN },	// RESET_REGISTER 
    {0x098E, 0x1000,WORD_LEN },	// LOGICAL_ADDRESS_ACCESS 
    {0xC97E, 0x01 ,BYTE_LEN	},// CAM_SYSCTL_PLL_ENABLE 
    //REG= 0xC980, 0x0225 	
    // CAM_SYSCTL_PLL_DIVIDER_M_N 
    {0xC980, 0x0120,WORD_LEN}, 	// CAM_SYSCTL_PLL_DIVIDER_M_N 
    {0xC982, 0x0700,WORD_LEN}, 	// CAM_SYSCTL_PLL_DIVIDER_P //DELAY=10 
    {MT9M114_TABLE_WAIT_MS, 10}, 

    // select output size, YUV format
    {0xC800, 0x0004,WORD_LEN},		//cam_sensor_cfg_y_addr_start = 4
    {0xC802, 0x0004,WORD_LEN},		//cam_sensor_cfg_x_addr_start = 4
    {0xC804, 0x03CB,WORD_LEN},		//cam_sensor_cfg_y_addr_end = 971
    {0xC806, 0x050B,WORD_LEN},		//cam_sensor_cfg_x_addr_end = 1291
    {0xC808, 0x02DC, WORD_LEN},//C00		//cam_sensor_cfg_pixclk = 48000000
    {0xC80A, 0x6C00, WORD_LEN},//C00		//cam_sensor_cfg_pixclk = 48000000
    {0xC80C, 0x0001,WORD_LEN},		//cam_sensor_cfg_row_speed = 1
    {0xC80E, 0x00DB,WORD_LEN},		//cam_sensor_cfg_fine_integ_time_min = 219
    {0xC810, 0x05B1,WORD_LEN},		//cam_sensor_cfg_fine_integ_time_max = 1457
    {0xC812, 0x03FD,WORD_LEN},		//cam_sensor_cfg_frame_length_lines = 1021
    {0xC814, 0x0634,WORD_LEN},		//cam_sensor_cfg_line_length_pck = 1588
    {0xC816, 0x0060,WORD_LEN},		//cam_sensor_cfg_fine_correction = 96
    {0xC818, 0x03C3,WORD_LEN},		//cam_sensor_cfg_cpipe_last_row = 963
    {0xC826, 0x0020,WORD_LEN},		//cam_sensor_cfg_reg_0_data = 32
    {0xC834, 0x0003,WORD_LEN},		//cam_sensor_control_read_mode = 0
    {0xC854, 0x0000,WORD_LEN},		//cam_crop_window_xoffset = 0
    {0xC856, 0x0000,WORD_LEN},		//cam_crop_window_yoffset = 0
    {0xC858, 0x0500,WORD_LEN},		//cam_crop_window_width = 1280
    {0xC85A, 0x03C0,WORD_LEN},		//cam_crop_window_height = 960
    {0xC85C, 0x03,BYTE_LEN},		//cam_crop_cropmode = 3
    {0xC868, 0x0280,WORD_LEN},		//cam_output_width = 640
    {0xC86A, 0x01E0,WORD_LEN},		//cam_output_height = 480
    {0xC878, 0x00,BYTE_LEN},		//cam_aet_aemode = 0
    {0xC88C, 0x1D9B,WORD_LEN},		//cam_aet_max_frame_rate = 7579
    {0xC88E, 0x0ECD,WORD_LEN},		//cam_aet_min_frame_rate = 3789
    {0xC914, 0x0000,WORD_LEN},		//cam_stat_awb_clip_window_xstart = 0
    {0xC916, 0x0000,WORD_LEN},		//cam_stat_awb_clip_window_ystart = 0
    {0xC918, 0x027F,WORD_LEN},		//cam_stat_awb_clip_window_xend = 639
    {0xC91A, 0x01DF,WORD_LEN},		//cam_stat_awb_clip_window_yend = 479
    {0xC91C, 0x0000,WORD_LEN},		//cam_stat_ae_initial_window_xstart = 0
    {0xC91E, 0x0000,WORD_LEN},		//cam_stat_ae_initial_window_ystart = 0
    {0xC920, 0x007F,WORD_LEN},		//cam_stat_ae_initial_window_xend = 127
    {0xC922, 0x005F,WORD_LEN},		//cam_stat_ae_initial_window_yend = 95
    //[Step3-Recommended] 
    {0x316A, 0x8270,WORD_LEN}, 	// DAC_TXLO_ROW 
    {0x316C, 0x8270,WORD_LEN}, 	// DAC_TXLO 
    {0x3ED0, 0x2305,WORD_LEN}, 	// DAC_LD_4_5
    {0x3ED2, 0x77CF,WORD_LEN}, 	// DAC_LD_6_7
    {0x316E, 0x8202,WORD_LEN}, 	// DAC_ECL 
    {0x3180, 0x87FF,WORD_LEN}, 	// DELTA_DK_CONTROL 
    {0x30D4, 0x6080,WORD_LEN}, 	// COLUMN_CORRECTION
    {0xA802, 0x0008,WORD_LEN}, 	// AE_TRACK_MODE
    {0x3E14, 0xFF39,WORD_LEN}, 	// SAMP_COL_PUP2
    //[patch 1204]for 720P 
    {0x0982, 0x0001,WORD_LEN}, 	// ACCESS_CTL_STAT 
    {0x098A, 0x60BC,WORD_LEN}, 	// PHYSICAL_ADDRESS_ACCESS
    {0xE0BC, 0xC0F1,WORD_LEN}, 
    {0xE0BE, 0x082A,WORD_LEN}, 
    {0xE0C0, 0x05A0,WORD_LEN}, 
    {0xE0C2, 0xD800,WORD_LEN}, 
    {0xE0C4, 0x71CF,WORD_LEN}, 
    {0xE0C6, 0xFFFF,WORD_LEN}, 
    {0xE0C8, 0xC344,WORD_LEN}, 
    {0xE0CA, 0x77CF,WORD_LEN}, 
    {0xE0CC, 0xFFFF,WORD_LEN}, 
    {0xE0CE, 0xC7C0,WORD_LEN}, 
    {0xE0D0, 0xB104,WORD_LEN}, 
    {0xE0D2, 0x8F1F,WORD_LEN}, 
    {0xE0D4, 0x75CF,WORD_LEN}, 
    {0xE0D6, 0xFFFF,WORD_LEN}, 
    {0xE0D8, 0xC84C,WORD_LEN},
    {0xE0DA, 0x0811,WORD_LEN}, 
    {0xE0DC, 0x005E,WORD_LEN}, 
    {0xE0DE, 0x70CF,WORD_LEN}, 
    {0xE0E0, 0x0000,WORD_LEN}, 
    {0xE0E2, 0x500E,WORD_LEN}, 
    {0xE0E4, 0x7840,WORD_LEN}, 
    {0xE0E6, 0xF019,WORD_LEN}, 
    {0xE0E8, 0x0CC6,WORD_LEN},
    {0xE0EA, 0x0340,WORD_LEN}, 
    {0xE0EC, 0x0E26,WORD_LEN}, 
    {0xE0EE, 0x0340,WORD_LEN}, 
    {0xE0F0, 0x95C2,WORD_LEN}, 
    {0xE0F2, 0x0E21,WORD_LEN}, 
    {0xE0F4, 0x101E,WORD_LEN},
    {0xE0F6, 0x0E0D,WORD_LEN},
    {0xE0F8, 0x119E,WORD_LEN}, 
    {0xE0FA, 0x0D56,WORD_LEN},
    {0xE0FC, 0x0340,WORD_LEN}, 
    {0xE0FE, 0xF008,WORD_LEN}, 
    {0xE100, 0x2650,WORD_LEN}, 
    {0xE102, 0x1040,WORD_LEN}, 
    {0xE104, 0x0AA2,WORD_LEN},
    {0xE106, 0x0360,WORD_LEN}, 
    {0xE108, 0xB502,WORD_LEN},
    {0xE10A, 0xB5C2,WORD_LEN},
    {0xE10C, 0x0B22,WORD_LEN}, 
    {0xE10E, 0x0400,WORD_LEN},
    {0xE110, 0x0CCE,WORD_LEN},
    {0xE112, 0x0320,WORD_LEN}, 
    {0xE114, 0xD800,WORD_LEN}, 
    {0xE116, 0x70CF,WORD_LEN}, 
    {0xE118, 0xFFFF,WORD_LEN}, 
    {0xE11A, 0xC5D4,WORD_LEN},
    {0xE11C, 0x902C,WORD_LEN}, 
    {0xE11E, 0x72CF,WORD_LEN}, 
    {0xE120, 0xFFFF,WORD_LEN},
    {0xE122, 0xE218,WORD_LEN},
    {0xE124, 0x9009,WORD_LEN},
    {0xE126, 0xE105,WORD_LEN}, 
    {0xE128, 0x73CF,WORD_LEN}, 
    {0xE12A, 0xFF00,WORD_LEN}, 
    {0xE12C, 0x2FD0,WORD_LEN}, 
    {0xE12E, 0x7822,WORD_LEN}, 
    {0xE130, 0x7910,WORD_LEN}, 
    {0xE132, 0xB202,WORD_LEN}, 
    {0xE134, 0x1382,WORD_LEN}, 
    {0xE136, 0x0700,WORD_LEN}, 
    {0xE138, 0x0815,WORD_LEN}, 
    {0xE13A, 0x03DE,WORD_LEN}, 
    {0xE13C, 0x1387,WORD_LEN}, 
    {0xE13E, 0x0700,WORD_LEN}, 
    {0xE140, 0x2102,WORD_LEN}, 
    {0xE142, 0x000A,WORD_LEN}, 
    {0xE144, 0x212F,WORD_LEN}, 
    {0xE146, 0x0288,WORD_LEN}, 
    {0xE148, 0x1A04,WORD_LEN}, 
    {0xE14A, 0x0284,WORD_LEN},
    {0xE14C, 0x13B9,WORD_LEN}, 
    {0xE14E, 0x0700,WORD_LEN}, 
    {0xE150, 0xB8C1,WORD_LEN}, 
    {0xE152, 0x0815,WORD_LEN}, 
    {0xE154, 0x0052,WORD_LEN}, 
    {0xE156, 0xDB00,WORD_LEN}, 
    {0xE158, 0x230F,WORD_LEN}, 
    {0xE15A, 0x0003,WORD_LEN},
    {0xE15C, 0x2102,WORD_LEN},
    {0xE15E, 0x00C0,WORD_LEN}, 
    {0xE160, 0x7910,WORD_LEN}, 
    {0xE162, 0xB202,WORD_LEN},
    {0xE164, 0x9507,WORD_LEN}, 
    {0xE166, 0x7822,WORD_LEN}, 
    {0xE168, 0xE080,WORD_LEN}, 
    {0xE16A, 0xD900,WORD_LEN},
    {0xE16C, 0x20CA,WORD_LEN}, 
    {0xE16E, 0x004B,WORD_LEN}, 
    {0xE170, 0xB805,WORD_LEN}, 
    {0xE172, 0x9533,WORD_LEN}, 
    {0xE174, 0x7815,WORD_LEN}, 
    {0xE176, 0x6038,WORD_LEN},
    {0xE178, 0x0FB2,WORD_LEN},
    {0xE17A, 0x0560,WORD_LEN},
    {0xE17C, 0xB861,WORD_LEN},
    {0xE17E, 0xB711,WORD_LEN},
    {0xE180, 0x0775,WORD_LEN}, 
    {0xE182, 0x0540,WORD_LEN}, 
    {0xE184, 0xD900,WORD_LEN},
    {0xE186, 0xF00A,WORD_LEN}, 
    {0xE188, 0x70CF,WORD_LEN},
    {0xE18A, 0xFFFF,WORD_LEN}, 
    {0xE18C, 0xE210,WORD_LEN},
    {0xE18E, 0x7835,WORD_LEN},
    {0xE190, 0x8041,WORD_LEN}, 
    {0xE192, 0x8000,WORD_LEN}, 
    {0xE194, 0xE102,WORD_LEN},
    {0xE196, 0xA040,WORD_LEN},
    {0xE198, 0x09F1,WORD_LEN}, 
    {0xE19A, 0x8094,WORD_LEN},
    {0xE19C, 0x7FE0,WORD_LEN},
    {0xE19E, 0xD800,WORD_LEN}, 
    {0xE1A0, 0xC0F1,WORD_LEN}, 
    {0xE1A2, 0xC5E1,WORD_LEN},
    {0xE1A4, 0x71CF,WORD_LEN}, 
    {0xE1A6, 0x0000,WORD_LEN}, 
    {0xE1A8, 0x45E6,WORD_LEN}, 
    {0xE1AA, 0x7960,WORD_LEN}, 
    {0xE1AC, 0x7508,WORD_LEN},
    {0xE1AE, 0x70CF,WORD_LEN},
    {0xE1B0, 0xFFFF,WORD_LEN}, 
    {0xE1B2, 0xC84C,WORD_LEN}, 
    {0xE1B4, 0x9002,WORD_LEN}, 
    {0xE1B6, 0x083D,WORD_LEN}, 
    {0xE1B8, 0x021E,WORD_LEN}, 
    {0xE1BA, 0x0D39,WORD_LEN},
    {0xE1BC, 0x10D1,WORD_LEN}, 
    {0xE1BE, 0x70CF,WORD_LEN}, 
    {0xE1C0, 0xFF00,WORD_LEN}, 
    {0xE1C2, 0x3354,WORD_LEN}, 
    {0xE1C4, 0x9055,WORD_LEN}, 
    {0xE1C6, 0x71CF,WORD_LEN}, 
    {0xE1C8, 0xFFFF,WORD_LEN}, 
    {0xE1CA, 0xC5D4,WORD_LEN}, 
    {0xE1CC, 0x116C,WORD_LEN}, 
    {0xE1CE, 0x0103,WORD_LEN}, 
    {0xE1D0, 0x1170,WORD_LEN}, 
    {0xE1D2, 0x00C1,WORD_LEN},
    {0xE1D4, 0xE381,WORD_LEN}, 
    {0xE1D6, 0x22C6,WORD_LEN},
    {0xE1D8, 0x0F81,WORD_LEN}, 
    {0xE1DA, 0x0000,WORD_LEN}, 
    {0xE1DC, 0x00FF,WORD_LEN}, 
    {0xE1DE, 0x22C4,WORD_LEN},
    {0xE1E0, 0x0F82,WORD_LEN}, 
    {0xE1E2, 0xFFFF,WORD_LEN}, 
    {0xE1E4, 0x00FF,WORD_LEN}, 
    {0xE1E6, 0x29C0,WORD_LEN}, 
    {0xE1E8, 0x0222,WORD_LEN}, 
    {0xE1EA, 0x7945,WORD_LEN}, 
    {0xE1EC, 0x7930,WORD_LEN}, 
    {0xE1EE, 0xB035,WORD_LEN}, 
    {0xE1F0, 0x0715,WORD_LEN},
    {0xE1F2, 0x0540,WORD_LEN}, 
    {0xE1F4, 0xD900,WORD_LEN},
    {0xE1F6, 0xF00A,WORD_LEN}, 
    {0xE1F8, 0x70CF,WORD_LEN}, 
    {0xE1FA, 0xFFFF,WORD_LEN}, 
    {0xE1FC, 0xE224,WORD_LEN},
    {0xE1FE, 0x7835,WORD_LEN}, 
    {0xE200, 0x8041,WORD_LEN}, 
    {0xE202, 0x8000,WORD_LEN}, 
    {0xE204, 0xE102,WORD_LEN}, 
    {0xE206, 0xA040,WORD_LEN}, 
    {0xE208, 0x09F1,WORD_LEN}, 
    {0xE20A, 0x8094,WORD_LEN}, 
    {0xE20C, 0x7FE0,WORD_LEN}, 
    {0xE20E, 0xD800,WORD_LEN}, 
    {0xE210, 0xFFFF,WORD_LEN},
    {0xE212, 0xCB40,WORD_LEN},
    {0xE214, 0xFFFF,WORD_LEN}, 
    {0xE216, 0xE0BC,WORD_LEN},
    {0xE218, 0x0000,WORD_LEN},
    {0xE21A, 0x0000,WORD_LEN},
    {0xE21C, 0x0000,WORD_LEN}, 
    {0xE21E, 0x0000,WORD_LEN}, 
    {0xE220, 0x0000,WORD_LEN},
    {0x098E, 0x0000,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS
    {0xE000, 0x1184,WORD_LEN}, 	// PATCHLDR_LOADER_ADDRESS
    {0xE002, 0x1204,WORD_LEN}, 	// PATCHLDR_PATCH_ID 
    {0xE004, 0x4103,WORD_LEN},   //0202 
    {0xE006, 0x0202,WORD_LEN}, 
    // PATCHLDR_FIRMWARE_ID //REG= 0xE004, 0x4103 //REG= 0xE006, 0x0202
    {0x0080, 0xFFF0,WORD_LEN}, 	
    // COMMAND_REGISTER //  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00
    //  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00
    //  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00 
    // 读Reg= 0x080， 判断其最低位是否为0， 如果不为0，则delay 5ms，然后继续读， // 直到为0或者   50ms以上
    //delay = 50 
    {MT9M114_TABLE_WAIT_MS, 50}, 
    {0x0080, 0xFFF1,WORD_LEN}, 	// COMMAND_REGISTER 
    //  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00 // 读Reg= 0x080， 判断其最低位是否为0， 如果不为0，则delay 5ms，然后继续读， // 直到为0或者   50ms以上
    //delay = 50 
    {MT9M114_TABLE_WAIT_MS, 50}, 
    // AWB Start point 
    {0x098E,0x2c12,WORD_LEN}, 
    {0xac12,0x008f,WORD_LEN}, 
    {0xac14,0x0105,WORD_LEN},
    //[Step4-APGA //LSC] // [APGA Settings 85% 2012/05/02 12:16:56] 
    {0x3640, 0x00B0,WORD_LEN}, 	//  P_G1_P0Q0 
    {0x3642, 0xC0EA,WORD_LEN}, 	//  P_G1_P0Q1
    {0x3644, 0x3E70,WORD_LEN}, 	//  P_G1_P0Q2
    {0x3646, 0x182B,WORD_LEN}, 	//  P_G1_P0Q3 
    {0x3648, 0xCC4D,WORD_LEN}, 	//  P_G1_P0Q4
    {0x364A, 0x0150,WORD_LEN}, 	//  P_R_P0Q0 
    {0x364C, 0x9C4A,WORD_LEN}, 	//  P_R_P0Q1 
    {0x364E, 0x6DF0,WORD_LEN}, 	//  P_R_P0Q2
    {0x3650, 0xE5CA,WORD_LEN}, 	//  P_R_P0Q3 
    {0x3652, 0xA22F,WORD_LEN}, 	//  P_R_P0Q4
    {0x3654, 0x0170,WORD_LEN}, 	//  P_B_P0Q0
    {0x3656, 0xC288,WORD_LEN}, 	//  P_B_P0Q1 
    {0x3658, 0x3010,WORD_LEN}, 	//  P_B_P0Q2
    {0x365A, 0xBA26,WORD_LEN}, 	//  P_B_P0Q3
    {0x365C, 0xE1AE,WORD_LEN}, 	//  P_B_P0Q4 
    {0x365E, 0x00B0,WORD_LEN}, 	//  P_G2_P0Q0 
    {0x3660, 0xC34A,WORD_LEN}, 	//  P_G2_P0Q1 
    {0x3662, 0x3E90,WORD_LEN}, 	//  P_G2_P0Q2 
    {0x3664, 0x1DAB,WORD_LEN}, 	//  P_G2_P0Q3 
    {0x3666, 0xD0CD,WORD_LEN}, 	//  P_G2_P0Q4 
    {0x3680, 0x440B,WORD_LEN}, 	//  P_G1_P1Q0 
    {0x3682, 0x1386,WORD_LEN}, 	//  P_G1_P1Q1
    {0x3684, 0xAB0C,WORD_LEN}, 	//  P_G1_P1Q2 
    {0x3686, 0x1FCD,WORD_LEN}, 	//  P_G1_P1Q3
    {0x3688, 0xEACB,WORD_LEN}, 	//  P_G1_P1Q4
    {0x368A, 0x63C7,WORD_LEN}, 	//  P_R_P1Q0 
    {0x368C, 0x1006,WORD_LEN}, 	//  P_R_P1Q1 
    {0x368E, 0xE64C,WORD_LEN}, 	//  P_R_P1Q2 
    {0x3690, 0x7B4D,WORD_LEN}, 	//  P_R_P1Q3
    {0x3692, 0x32AF,WORD_LEN}, 	//  P_R_P1Q4
    {0x3694, 0x24EB,WORD_LEN}, 	//  P_B_P1Q0
    {0x3696, 0xFB6A,WORD_LEN}, 	//  P_B_P1Q1 
    {0x3698, 0xB96C,WORD_LEN}, 	//  P_B_P1Q2 
    {0x369A, 0x9AEA,WORD_LEN}, 	//  P_B_P1Q3 
    {0x369C, 0x092E,WORD_LEN}, 	//  P_B_P1Q4
    {0x369E, 0x432B,WORD_LEN}, 	//  P_G2_P1Q0 
    {0x36A0, 0xC7C5,WORD_LEN}, 	//  P_G2_P1Q1
    {0x36A2, 0xABEC,WORD_LEN}, 	//  P_G2_P1Q2 
    {0x36A4, 0x222D,WORD_LEN}, 	//  P_G2_P1Q3 
    {0x36A6, 0xE68B,WORD_LEN}, 	//  P_G2_P1Q4 
    {0x36C0, 0x0631,WORD_LEN}, 	//  P_G1_P2Q0
    {0x36C2, 0x946D,WORD_LEN}, 	//  P_G1_P2Q1 
    {0x36C4, 0xB6B1,WORD_LEN}, 	//  P_G1_P2Q2 
    {0x36C6, 0x5ECF,WORD_LEN}, 	//  P_G1_P2Q3
    {0x36C8, 0x25D2,WORD_LEN}, 	//  P_G1_P2Q4
    {0x36CA, 0x05F1,WORD_LEN}, 	//  P_R_P2Q0 
    {0x36CC, 0x934D,WORD_LEN}, 	//  P_R_P2Q1 
    {0x36CE, 0xB9F1,WORD_LEN}, 	//  P_R_P2Q2
    {0x36D0, 0x05AF,WORD_LEN}, 	//  P_R_P2Q3
    {0x36D2, 0x4C92,WORD_LEN}, 	//  P_R_P2Q4 
    {0x36D4, 0x37B0,WORD_LEN}, 	//  P_B_P2Q0 
    {0x36D6, 0x938B,WORD_LEN}, 	//  P_B_P2Q1 
    {0x36D8, 0xB4B0,WORD_LEN}, 	//  P_B_P2Q2
    {0x36DA, 0x6A8D,WORD_LEN}, 	//  P_B_P2Q3
    {0x36DC, 0x3951,WORD_LEN}, 	//  P_B_P2Q4 
    {0x36DE, 0x0611,WORD_LEN}, 	//  P_G2_P2Q0
    {0x36E0, 0xA28D,WORD_LEN}, 	//  P_G2_P2Q1 
    {0x36E2, 0xB6B1,WORD_LEN}, 	//  P_G2_P2Q2 
    {0x36E4, 0x6D6F,WORD_LEN}, 	//  P_G2_P2Q3 
    {0x36E6, 0x2652,WORD_LEN}, 	//  P_G2_P2Q4 
    {0x3700, 0x75CA,WORD_LEN}, 	//  P_G1_P3Q0
    {0x3702, 0x42AD,WORD_LEN}, 	//  P_G1_P3Q1 
    {0x3704, 0xB9AA,WORD_LEN}, 	//  P_G1_P3Q2 
    {0x3706, 0xA0EF,WORD_LEN}, 	//  P_G1_P3Q3 
    {0x3708, 0x29B2,WORD_LEN}, 	//  P_G1_P3Q4
    {0x370A, 0x830B,WORD_LEN}, 	//  P_R_P3Q0 
    {0x370C, 0x05AE,WORD_LEN}, 	//  P_R_P3Q1
    {0x370E, 0x30B0,WORD_LEN}, 	//  P_R_P3Q2 
    {0x3710, 0x88D0,WORD_LEN}, 	//  P_R_P3Q3
    {0x3712, 0x1AF0,WORD_LEN}, 	//  P_R_P3Q4 
    {0x3714, 0x746B,WORD_LEN}, 	//  P_B_P3Q0 
    {0x3716, 0x376D,WORD_LEN}, 	//  P_B_P3Q1
    {0x3718, 0x2E8E,WORD_LEN}, 	//  P_B_P3Q2
    {0x371A, 0x8C2B,WORD_LEN}, 	//  P_B_P3Q3 
    {0x371C, 0x0030,WORD_LEN}, 	//  P_B_P3Q4
    {0x371E, 0x050B,WORD_LEN}, 	//  P_G2_P3Q0
    {0x3720, 0x5C0D,WORD_LEN}, 	//  P_G2_P3Q1 
    {0x3722, 0x1E88,WORD_LEN}, 	//  P_G2_P3Q2 
    {0x3724, 0xB22F,WORD_LEN}, 	//  P_G2_P3Q3 
    {0x3726, 0x26B2,WORD_LEN}, 	//  P_G2_P3Q4 
    {0x3740, 0xFC90,WORD_LEN}, 	//  P_G1_P4Q0 
    {0x3742, 0x430F,WORD_LEN}, 	//  P_G1_P4Q1 
    {0x3744, 0x9151,WORD_LEN}, 	//  P_G1_P4Q2
    {0x3746, 0xDF71,WORD_LEN}, 	//  P_G1_P4Q3 
    {0x3748, 0x4575,WORD_LEN}, 	//  P_G1_P4Q4
    {0x374A, 0xC1CF,WORD_LEN}, 	//  P_R_P4Q0 
    {0x374C, 0x414F,WORD_LEN}, 	//  P_R_P4Q1 
    {0x374E, 0xD551,WORD_LEN}, 	//  P_R_P4Q2 
    {0x3750, 0x9451,WORD_LEN}, 	//  P_R_P4Q3
    {0x3752, 0x5A35,WORD_LEN}, 	//  P_R_P4Q4 

    {0x3754, 0x4C6C,WORD_LEN}, 	//  P_B_P4Q0
    {0x3756, 0x300F,WORD_LEN}, 	//  P_B_P4Q1 
    {0x3758, 0xB232,WORD_LEN}, 	//  P_B_P4Q2 
    {0x375A, 0x96F0,WORD_LEN}, 	//  P_B_P4Q3
    {0x375C, 0x47B5,WORD_LEN}, 	//  P_B_P4Q4 
    {0x375E, 0xFBD0,WORD_LEN}, 	//  P_G2_P4Q0
    {0x3760, 0x568F,WORD_LEN
    }, 	//  P_G2_P4Q1 
    {0x3762, 0x8E51,WORD_LEN}, 	//  P_G2_P4Q2
    {0x3764, 0xECB1,WORD_LEN}, 	//  P_G2_P4Q3 
    {0x3766, 0x44B5,WORD_LEN}, 	//  P_G2_P4Q4 
    {0x3784, 0x0280,WORD_LEN}, 	//  CENTER_COLUMN 
    {0x3782, 0x01EC,WORD_LEN}, 	//  CENTER_ROW 
    {0x37C0, 0x83C7,WORD_LEN}, 	//  P_GR_Q5 
    {0x37C2, 0xEB89,WORD_LEN}, 	//  P_RD_Q5 
    {0x37C4, 0xD089,WORD_LEN}, 	//  P_BL_Q5 
    {0x37C6, 0x9187,WORD_LEN}, 	//  P_GB_Q5 
    {0x098E, 0x0000,WORD_LEN}, 	//  LOGICAL addressing
    {0xC960, 0x0A8C,WORD_LEN}, 	//  CAM_PGA_L_CONFIG_COLOUR_TEMP 
    {0xC962, 0x7584,WORD_LEN}, 	//  CAM_PGA_L_CONFIG_GREEN_RED_Q14 
    {0xC964, 0x5900,WORD_LEN}, 	//  CAM_PGA_L_CONFIG_RED_Q14
    {0xC966, 0x75AE,WORD_LEN}, 	//  CAM_PGA_L_CONFIG_GREEN_BLUE_Q14 
    {0xC968, 0x7274,WORD_LEN}, 	//  CAM_PGA_L_CONFIG_BLUE_Q14 
    {0xC96A, 0x0FD2,WORD_LEN}, 	//  CAM_PGA_M_CONFIG_COLOUR_TEMP 
    {0xC96C, 0x8155,WORD_LEN}, 	//  CAM_PGA_M_CONFIG_GREEN_RED_Q14 
    {0xC96E, 0x8018,WORD_LEN}, 	//  CAM_PGA_M_CONFIG_RED_Q14
    {0xC970, 0x814C,WORD_LEN}, 	//  CAM_PGA_M_CONFIG_GREEN_BLUE_Q14
    {0xC972, 0x836B,WORD_LEN}, 	//  CAM_PGA_M_CONFIG_BLUE_Q14
    {0xC974, 0x1964,WORD_LEN}, 	//  CAM_PGA_R_CONFIG_COLOUR_TEMP 
    {0xC976, 0x7FDF,WORD_LEN}, 	//  CAM_PGA_R_CONFIG_GREEN_RED_Q14
    {0xC978, 0x7F15,WORD_LEN}, 	//  CAM_PGA_R_CONFIG_RED_Q14 
    {0xC97A, 0x7FDC,WORD_LEN}, 	//  CAM_PGA_R_CONFIG_GREEN_BLUE_Q14
    {0xC97C, 0x7F30,WORD_LEN}, 	//  CAM_PGA_R_CONFIG_BLUE_Q14 
    {0xC95E, 0x0003,WORD_LEN}, 	//  CAM_PGA_PGA_CONTROL
    //[Step5-AWB_CCM]//default value 
    {0xC892, 0x0267,WORD_LEN}, 	// CAM_AWB_CCM_L_0
    {0xC894, 0xFF1A,WORD_LEN}, 	// CAM_AWB_CCM_L_1
    {0xC896, 0xFFB3,WORD_LEN}, 	// CAM_AWB_CCM_L_2 
    {0xC898, 0xFF80,WORD_LEN}, 	// CAM_AWB_CCM_L_3 
    {0xC89A, 0x0166,WORD_LEN}, 	// CAM_AWB_CCM_L_4 
    {0xC89C, 0x0003,WORD_LEN}, 	// CAM_AWB_CCM_L_5 
    {0xC89E, 0xFF9A,WORD_LEN}, 	// CAM_AWB_CCM_L_6 
    {0xC8A0, 0xFEB4,WORD_LEN}, 	// CAM_AWB_CCM_L_7 
    {0xC8A2, 0x024D,WORD_LEN}, 	// CAM_AWB_CCM_L_8 
    {0xC8A4, 0x01BF,WORD_LEN}, 	// CAM_AWB_CCM_M_0
    {0xC8A6, 0xFF01,WORD_LEN}, 	// CAM_AWB_CCM_M_1 
    {0xC8A8, 0xFFF3,WORD_LEN}, 	// CAM_AWB_CCM_M_2 
    {0xC8AA, 0xFF75,WORD_LEN}, 	// CAM_AWB_CCM_M_3 
    {0xC8AC, 0x0198,WORD_LEN}, 	// CAM_AWB_CCM_M_4 
    {0xC8AE, 0xFFFD,WORD_LEN}, 	// CAM_AWB_CCM_M_5 
    {0xC8B0, 0xFF9A,WORD_LEN}, 	// CAM_AWB_CCM_M_6
    {0xC8B2, 0xFEE7,WORD_LEN}, 	// CAM_AWB_CCM_M_7 
    {0xC8B4, 0x02A8,WORD_LEN}, 	// CAM_AWB_CCM_M_8 
    {0xC8B6, 0x01D9,WORD_LEN}, 	// CAM_AWB_CCM_R_0 
    {0xC8B8, 0xFF26,WORD_LEN}, 	// CAM_AWB_CCM_R_1 
    {0xC8BA, 0xFFF3,WORD_LEN}, 	// CAM_AWB_CCM_R_2 
    {0xC8BC, 0xFFB3,WORD_LEN}, 	// CAM_AWB_CCM_R_3 
    {0xC8BE, 0x0132,WORD_LEN}, 	// CAM_AWB_CCM_R_4 
    {0xC8C0, 0xFFE8,WORD_LEN}, 	// CAM_AWB_CCM_R_5 
    {0xC8C2, 0xFFDA,WORD_LEN}, 	// CAM_AWB_CCM_R_6
    {0xC8C4, 0xFECD,WORD_LEN}, 	// CAM_AWB_CCM_R_7 
    {0xC8C6, 0x02C2,WORD_LEN}, 	// CAM_AWB_CCM_R_8 
    {0xC8C8, 0x0075,WORD_LEN}, 	// CAM_AWB_CCM_L_RG_GAIN 
    {0xC8CA, 0x011C,WORD_LEN}, 	// CAM_AWB_CCM_L_BG_GAIN 
    {0xC8CC, 0x009A,WORD_LEN}, 	// CAM_AWB_CCM_M_RG_GAIN 
    {0xC8CE, 0x0105,WORD_LEN}, 	// CAM_AWB_CCM_M_BG_GAIN 
    {0xC8D0, 0x00A4,WORD_LEN}, 	// CAM_AWB_CCM_R_RG_GAIN 
    {0xC8D2, 0x00AC,WORD_LEN}, 	// CAM_AWB_CCM_R_BG_GAIN 
    {0xC8D4, 0x0A8C,WORD_LEN}, 	// CAM_AWB_CCM_L_CTEMP 
    {0xC8D6, 0x0F0A,WORD_LEN}, 	// CAM_AWB_CCM_M_CTEMP
    {0xC8D8, 0x1964,WORD_LEN}, 	// CAM_AWB_CCM_R_CTEMP 
    {0xC914, 0x0000,WORD_LEN}, 	// CAM_STAT_AWB_CLIP_WINDOW_XSTART
    {0xC916, 0x0000,WORD_LEN}, 	// CAM_STAT_AWB_CLIP_WINDOW_YSTART
    {0xC918, 0x04FF,WORD_LEN}, 	// CAM_STAT_AWB_CLIP_WINDOW_XEND 
    {0xC91A, 0x02CF,WORD_LEN}, 	// CAM_STAT_AWB_CLIP_WINDOW_YEND
    {0xC904, 0x0033,WORD_LEN}, 	// CAM_AWB_AWB_XSHIFT_PRE_ADJ
    {0xC906, 0x0040,WORD_LEN}, 	// CAM_AWB_AWB_YSHIFT_PRE_ADJ 
    {0xC8F2, 0x03,BYTE_LEN 	},// CAM_AWB_AWB_XSCALE
    {0xC8F3, 0x02,BYTE_LEN 	},// CAM_AWB_AWB_YSCALE 
    {0xC906, 0x003C,WORD_LEN}, 	// CAM_AWB_AWB_YSHIFT_PRE_ADJ 
    {0xC8F4, 0x0000,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_0
    {0xC8F6, 0x0000,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_1 
    {0xC8F8, 0x0000,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_2 
    {0xC8FA, 0xE724,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_3 
    {0xC8FC, 0x1583,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_4 
    {0xC8FE, 0x2045,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_5 
    {0xC900, 0x03FF,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_6 
    {0xC902, 0x007C,WORD_LEN}, 	// CAM_AWB_AWB_WEIGHTS_7 
    {0xC90C, 0x80,BYTE_LEN}, 	// CAM_AWB_K_R_L 
    {0xC90D, 0x80,BYTE_LEN}, 	// CAM_AWB_K_G_L 
    {0xC90E, 0x80,BYTE_LEN}, 	// CAM_AWB_K_B_L 
    {0xC90F, 0x88,BYTE_LEN}, 	// CAM_AWB_K_R_R 
    {0xC910, 0x80,BYTE_LEN}, 	// CAM_AWB_K_G_R 
    {0xC911, 0x80,BYTE_LEN}, 	// CAM_AWB_K_B_R
    //[Step7-CPIPE_Preference]
    {0x098E, 0x4926,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS [CAM_LL_START_BRIGHTNESS] 
    {0xC926, 0x0020,WORD_LEN}, 	// CAM_LL_START_BRIGHTNESS 
    {0xC928, 0x009A,WORD_LEN}, 	// CAM_LL_STOP_BRIGHTNESS
    {0xC946, 0x0070,WORD_LEN}, 	// CAM_LL_START_GAIN_METRIC 
    {0xC948, 0x00F3,WORD_LEN}, 	// CAM_LL_STOP_GAIN_METRIC 
    {0xC952, 0x0020,WORD_LEN}, 	// CAM_LL_START_TARGET_LUMA_BM 
    {0xC954, 0x009A,WORD_LEN}, 	// CAM_LL_STOP_TARGET_LUMA_BM 
    {0xC92A, 0x80,BYTE_LEN},	// CAM_LL_START_SATURATION
    {0xC92B, 0x4B,BYTE_LEN},	// CAM_LL_END_SATURATION
    {0xC92C, 0x00,BYTE_LEN},	// CAM_LL_START_DESATURATION 
    {0xC92D, 0xFF,BYTE_LEN},	// CAM_LL_END_DESATURATION 
    {0xC92E, 0x3C,BYTE_LEN},	// CAM_LL_START_DEMOSAIC 
    {0xC92F, 0x02,BYTE_LEN},	// CAM_LL_START_AP_GAIN 
    {0xC930, 0x06,BYTE_LEN},	// CAM_LL_START_AP_THRESH
    {0xC931, 0x64,BYTE_LEN},	// CAM_LL_STOP_DEMOSAIC 
    {0xC932, 0x01,BYTE_LEN},	// CAM_LL_STOP_AP_GAIN 
    {0xC933, 0x0C,BYTE_LEN},	// CAM_LL_STOP_AP_THRESH 
    {0xC934, 0x3C,BYTE_LEN},	// CAM_LL_START_NR_RED
    {0xC935, 0x3C,BYTE_LEN},	// CAM_LL_START_NR_GREEN 
    {0xC936, 0x3C,BYTE_LEN},	// CAM_LL_START_NR_BLUE 
    {0xC937, 0x0F,BYTE_LEN},	// CAM_LL_START_NR_THRESH 
    {0xC938, 0x64,BYTE_LEN},	// CAM_LL_STOP_NR_RED 
    {0xC939, 0x64,BYTE_LEN},	// CAM_LL_STOP_NR_GREEN
    {0xC93A, 0x64,BYTE_LEN} ,	// CAM_LL_STOP_NR_BLUE 
    {0xC93B, 0x32,BYTE_LEN},	// CAM_LL_STOP_NR_THRESH 
    {0xC93C, 0x0020,WORD_LEN}, 	// CAM_LL_START_CONTRAST_BM 
    {0xC93E, 0x009A,WORD_LEN}, 	// CAM_LL_STOP_CONTRAST_BM 
    {0xC940, 0x00DC,WORD_LEN}, 	// CAM_LL_GAMMA 
    {0xC942, 0x38,BYTE_LEN}, 	// CAM_LL_START_CONTRAST_GRADIENT 
    {0xC943, 0x30,BYTE_LEN}, 	// CAM_LL_STOP_CONTRAST_GRADIENT 
    {0xC944, 0x50,BYTE_LEN}, 	// CAM_LL_START_CONTRAST_LUMA_PERCENTAGE 
    {0xC945, 0x19,BYTE_LEN}, 	// CAM_LL_STOP_CONTRAST_LUMA_PERCENTAGE 
    {0xC94A, 0x0230,WORD_LEN}, 	// CAM_LL_START_FADE_TO_BLACK_LUMA 
    {0xC94C, 0x0010,WORD_LEN}, 	// CAM_LL_STOP_FADE_TO_BLACK_LUMA 
    {0xC94E, 0x01CD,WORD_LEN}, 	// CAM_LL_CLUSTER_DC_TH_BM 
    {0xC950, 0x05,BYTE_LEN },	// CAM_LL_CLUSTER_DC_GATE_PERCENTAGE
    {0xC951, 0x40,BYTE_LEN },	// CAM_LL_SUMMING_SENSITIVITY_FACTOR 
    {0xC87B, 0x1B,BYTE_LEN}, 	// CAM_AET_TARGET_AVERAGE_LUMA_DARK
    {0xC878, 0x0E,BYTE_LEN}, 	// CAM_AET_AEMODE 
    {0xC890, 0x0080,WORD_LEN}, 	// CAM_AET_TARGET_GAIN 
    {0xC886, 0x0100,WORD_LEN}, 	// CAM_AET_AE_MAX_VIRT_AGAIN 
    {0xC87C, 0x005A,WORD_LEN}, 	// CAM_AET_BLACK_CLIPPING_TARGET 
    {0xB42A, 0x05,BYTE_LEN}, 	// CCM_DELTA_GAIN
    {0xA80A, 0x20,BYTE_LEN}, 	// AE_TRACK_AE_TRACKING_DAMPENING_SPEED
    //[Step8-Features] // For Parallel,2012-5-16 14:33 
    {0x098E, 0x0000,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS
    {0xC984, 0x8040,WORD_LEN}, 	// CAM_PORT_OUTPUT_CONTROL 
    {0xC984, 0x8040,WORD_LEN},  	// CAM_PORT_OUTPUT_CONTROL, parallel, 2012-3-21 0:41 
    {0x001E, 0x0777,WORD_LEN}, 	// PAD_SLEW, for parallel, 2012-5-16 14:34 //DELAY=50 //add 2012-4-1 13:42 // end, 2012-5-16 14:33
    // For MIPI, 2012-5-16 14:33 
    {0x098E, 0x0000,WORD_LEN}, 	// LOGICAL_ADDRESS_ACCESS
    {0xC984, 0x8041,WORD_LEN}, 	// CAM_PORT_OUTPUT_CONTROL 
    {0xC988, 0x0F00,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_T_HS_ZERO 
    {0xC98A, 0x0B07,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_T_HS_EXIT_HS_TRAIL 
    {0xC98C, 0x0D01,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_T_CLK_POST_CLK_PRE 
    {0xC98E, 0x071D,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_T_CLK_TRAIL_CLK_ZERO 
    {0xC990, 0x0006,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_T_LPX 
    {0xC992, 0x0A0C,WORD_LEN}, 	// CAM_PORT_MIPI_TIMING_INIT_TIMING
    {0x3C5A, 0x0009,WORD_LEN}, 	// MIPI_DELAY_TRIM
    //[Anti-Flicker for MT9M114][50Hz] 
    {0x098E, 0xC88B,WORD_LEN},  // LOGICAL_ADDRESS_ACCESS [CAM_AET_FLICKER_FREQ_HZ] 
    {0xC88B, 0x32,BYTE_LEN  },  // CAM_AET_FLICKER_FREQ_HZ
    // Saturation
    {0xC92A,0x84,BYTE_LEN},
    {0xC92B,0x46,BYTE_LEN},
    // AE //Reg = 0xC87A,0x48 
    //{0xC87A,0x3C,BYTE_LEN},//0x3C
   // {0xC87A,0x3C,BYTE_LEN},//0x3C 
    {0xC87A,0x43,BYTE_LEN},//0x3C 
    // Sharpness
    {0x098E,0xC92F,WORD_LEN}, 
    {0xC92F,0x01,BYTE_LEN  }, 
    {0xC932,0x00,BYTE_LEN  },
    // Target Gain
    {0x098E,0x4890,WORD_LEN},
    {0xC890,0x0040,WORD_LEN},
    { 0xc940,0x00C8,WORD_LEN}, // CAM_LL_GAMMA,2012-5-17 14:42 
    {0x3C40, 0x783C,WORD_LEN}, 	// MIPI_CONTROL
    //[Change-Config] 
    {0x098E, 0xDC00,WORD_LEN},  	// LOGICAL_ADDRESS_ACCESS [SYSMGR_NEXT_STATE] 
    {0xDC00, 0x28,BYTE_LEN 	},// SYSMGR_NEXT_STATE 
    {0x0080, 0x8002,WORD_LEN}, 	// COMMAND_REGISTER //  POLL  COMMAND_REGISTER::HOST_COMMAND_1 =>  0x00 ////DELAY=100 //DELAY=150 {MT9M114_TABLE_WAIT_MS, 150}, 
    
    {MT9M114_TABLE_WAIT_MS, 100}, 
    {MT9M114_TABLE_END, 0x00}
}; 


enum {
	MT9M114_MODE_1280x960,
       MT9M114_MODE_1280x720,
	//MT9M114_MODE_640x480,
};

static struct mt9m114_reg *mode_table[] = {
	[MT9M114_MODE_1280x960] = mode_1280x960,
       [MT9M114_MODE_1280x720] = mode_1280x720,
	///[MT9M114_MODE_640x480] = mode_640x480,
};


#if 0  // do not delete ,for test 
static int mt9m114_read_reg(struct i2c_client *client, u16 addr, u16 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[4];

	if (!client->adapter)
		return -ENODEV;
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = data + 2;
	err = i2c_transfer(client->adapter, msg, 2);

	if (err != 2)
		return -EINVAL;

	*val = data[2] << 8 | data[3];

	return 0;
}
#endif
static int mt9m114_write_reg(struct i2c_client *client, u16 addr, u16 val,u8 width)
{
	int err;
	struct i2c_msg msg;
	unsigned char data[4];
	int retry = 0;
	#if MT9M114_PRINT
       printk("mt9m114_write_reg");
	#endif
	if (!client->adapter)
		return -ENODEV;

        switch (width) {
	case WORD_LEN: {
	
                data[0] = (u8) (addr >> 8);
                data[1] = (u8) (addr & 0xff);
                data[2] = (u8) (val >> 8);
                data[3] = (u8) (val & 0xff);
		//rc = mt9v114_i2c_txdata(saddr, buf, 4);
		msg.len = 4;
	}
		break;

	case BYTE_LEN: {
		
                data[0] = (u8) (addr >> 8);
                data[1] = (u8) (addr & 0xff);
                data[2] =(u8) (val & 0xff);
		//rc = mt9v114_i2c_txdata(saddr, buf, 3);
		msg.len = 3;
	}
		break;

	default:
		break;
	}

	msg.addr = client->addr;
	msg.flags = 0;
	msg.buf = data;

	do {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			return 0;
		retry++;
		pr_err("mt9m114: i2c transfer failed, retrying %x %x\n",
		       addr, val);
		msleep(3);
	} while (retry <= MT9M114_MAX_RETRIES);

	return err;
}
static int mt9m114_write_table(struct i2c_client *client,
			      const struct mt9m114_reg table[],
			      const struct mt9m114_reg override_list[],
			      int num_override_regs)
{
	int err;
	const struct mt9m114_reg *next;
	int i;
	u16 val;
       u8 width ;
	for (next = table; next->addr != MT9M114_TABLE_END; next++) {
		if (next->addr == MT9M114_TABLE_WAIT_MS) {
			msleep(next->val);
			continue;
		}

		val = next->val;
               width = next->width;

		/* When an override list is passed in, replace the reg */
		/* value to write if the reg is in the list            */
		if (override_list) {
			for (i = 0; i < num_override_regs; i++) {
				if (next->addr == override_list[i].addr) {
					val = override_list[i].val;
                                   width =override_list[i].width;
					break;
				}
			}
		}

		err = mt9m114_write_reg(client, next->addr, val,width);
		if (err)
			return err;
	}
	return 0;
}


static int mt9m114_set_mode(struct mt9m114_info *info, struct mt9m114_mode *mode)
{
	int sensor_mode;
	int err;

     	#if MT9M114_PRINT
	printk("%s: resolution supplied to set mode %d %d\n", __func__, mode->xres, mode->yres);
	#endif
	if (mode->xres == 1280 && mode->yres == 960)
		sensor_mode = MT9M114_MODE_1280x960;
	else if (mode->xres == 1280 && mode->yres == 720)
		sensor_mode = MT9M114_MODE_1280x720;
	//else if (mode->xres == 640 && mode->yres == 480)
	//	sensor_mode = MT9M114_MODE_640x480;
	else {msleep(5);
		pr_err("%s: invalid resolution supplied to set mode %d %d\n",
		       __func__, mode->xres, mode->yres);
		return -EINVAL;
	}
        err = mt9m114_write_table(info->i2c_client, mode_table[sensor_mode],NULL, 0);

        msleep(100);

        info->mode = sensor_mode;
	return 0;
}

static int mt9m114_get_status(struct mt9m114_info *info, u8 *status)
{
	return 0;
}


static long mt9m114_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct mt9m114_info *info = file->private_data;

	switch (cmd) {
	case MT9M114_IOCTL_SET_MODE:
	{
		struct mt9m114_mode mode;
		if (copy_from_user(&mode,
				   (const void __user *)arg,
				   sizeof(struct mt9m114_mode))) {
			return -EFAULT;
		}

		return mt9m114_set_mode(info, &mode);
	}
	case MT9M114_IOCTL_GET_STATUS:
	{
		u8 status;
              err = 0;
		err = mt9m114_get_status(info, &status);
		if (err)
			return err;
		if (copy_to_user((void __user *)arg, &status,
				 2)) {
			return -EFAULT;
		}
		return 0;
	}
	 case MT9M114_IOCTL_SET_BRIGHTNESS:
        {
            u8 brightness;
            if (copy_from_user(&brightness,
                (const void __user *)arg,
                sizeof(brightness))) {
                return -EFAULT;
            }
	            switch(brightness)
	            {
	                case mt9m114_Brightness_Level1:
	                    #if MT9M114_PRINT
	                    printk("yuv SET_Brightness 0\n");
	                    #endif
	                    err = mt9m114_write_table(info->i2c_client, mt9m114_brightness_level1,NULL, 0);
	                    break;
	                   
	                case mt9m114_Brightness_Level2:
	                     #if MT9M114_PRINT
	                    printk("yuv SET_Brightness 1\n");
	                     #endif
	                    err = mt9m114_write_table(info->i2c_client, mt9m114_brightness_level2,NULL, 0);
	                    break;
	                   
	                case mt9m114_Brightness_Level3:
	                     #if MT9M114_PRINT
	                    	printk("yuv SET_Brightness 2\n");
	                     #endif
	                    	err = mt9m114_write_table(info->i2c_client, mt9m114_brightness_level3,NULL, 0);
	                   	break;
	                   
	                case mt9m114_Brightness_Level4:
	                     #if MT9M114_PRINT
	                    	printk("yuv SET_Brightness 3\n");
	                     #endif
	                    	err = mt9m114_write_table(info->i2c_client, mt9m114_brightness_level4,NULL, 0);
				break;
	                    
	                case mt9m114_Brightness_Level5:
	                     #if MT9M114_PRINT
	                    	printk("yuv SET_Brightness 4\n");
	                     #endif
	                    	err = mt9m114_write_table(info->i2c_client, mt9m114_brightness_level5,NULL, 0);
				break;
	                    
	                case mt9m114_Brightness_Level6:
	                 	#if MT9M114_PRINT
	                	printk("yuv SET_Brightness 4\n");
	                 	#endif
	                	err = mt9m114_write_table(info->i2c_client, mt9m114_brightness_level6,NULL, 0);
			  	break;
			  default:
	                         break;
	            	}
                if(err)
                    return err;
		  return 0;
              		
            	}
	case MT9M114_IOCTL_SET_WHITE_BALANCE:
	        {
	                u8 whitebalance;

		        pr_info("yuv whitebalance %lu\n",arg);
			if (copy_from_user(&whitebalance,
					   (const void __user *)arg,
					   sizeof(whitebalance))) {
				return -EFAULT;
			}
	                // #if MT9M114_PRINT
		       // printk("7692 yuv whitebalance %d\n",whitebalance);
	               //  #endif
	             
	                switch(whitebalance)
	                {
	                    case MT9M114_Whitebalance_Auto:
		                 err = mt9m114_write_table(info->i2c_client, Whitebalance_Auto,NULL, 0);
	                         break;
	                    case MT9M114_Whitebalance_Incandescent:
		                 err = mt9m114_write_table(info->i2c_client, Whitebalance_Incandescent,NULL, 0);
	                         break;
	                    case MT9M114_Whitebalance_Daylight:
		                 err = mt9m114_write_table(info->i2c_client, Whitebalance_Daylight,NULL, 0);
	                         break;
	                    case MT9M114_Whitebalance_Fluorescent:
		                 err = mt9m114_write_table(info->i2c_client, Whitebalance_Fluorescent,NULL, 0);
	                         break;
	                    case MT9M114_Whitebalance_Cloudy:
	                         err = mt9m114_write_table(info->i2c_client, Whitebalance_Cloudy,NULL, 0);
	                         break;
	                    default:
	                         break;
	                }
	                if (err)
			    return err;
	                return 0;
        	}
	 case MT9M114_IOCTL_SET_CONTRAST:	
         {
            int contrast;
            u8 value = 0;
            if (copy_from_user(&contrast,
            (const void __user *)arg,
            sizeof(contrast))) {
            return -EFAULT;
            }
            //printk("yuv SET_Contrast  contrast=%d,%d\n",contrast,ov5640_contrast);
            
                switch(contrast)
                {
                        case MT9M114_Contrast_Level1:
                           #if MT9M114_PRINT
                            printk("yuv SET_Contrast -50\n");
                          #endif
                            err = mt9m114_write_table(info->i2c_client, mt9m114_contrast_level1,NULL, 0);
                            break;
                        case MT9M114_Contrast_Level2:
                            #if MT9M114_PRINT
                            printk("yuv SET_Contrast -100\n");
                            #endif
                            err = mt9m114_write_table(info->i2c_client, mt9m114_contrast_level2,NULL, 0);
                            break;
                        case MT9M114_Contrast_Level3:
                          #if MT9M114_PRINT
                            printk("yuv SET_Contrast 0\n");
                            #endif
                            err = mt9m114_write_table(info->i2c_client, mt9m114_contrast_level3,NULL, 0);
                            break;
                        case MT9M114_Contrast_Level4:
                            #if MT9M114_PRINT
                            printk("yuv SET_Contrast 50\n");
                           #endif //
                            err = mt9m114_write_table(info->i2c_client, mt9m114_contrast_level4,NULL, 0);
                            break;
                        case MT9M114_Contrast_Level5:
                            #if MT9M114_PRINT
                            printk("yuv SET_Contrast 100\n");
                            #endif
                            err = mt9m114_write_table(info->i2c_client, mt9m114_contrast_level5,NULL, 0);
                             break;
                        default:
                             break;
                    }
           
            if(err)
                return err;
            return 0;
        }
	 case MT9M114_IOCTL_SET_COLOR_EFFECT:
	{
		 int effect;
            u8 value = 0;
            if (copy_from_user(&effect,
            (const void __user *)arg,
            sizeof(effect))) {
            return -EFAULT;
            }
            //printk("yuv SET_Contrast  contrast=%d,%d\n",contrast,ov5640_contrast);
            
                switch(effect)
                {
                        case MT9M114_ColorEffect_None:
                           #if MT9M114_PRINT
                            printk("MT9M114_ColorEffect_None\n");
                          #endif
                            err = mt9m114_write_table(info->i2c_client, ColorEffect_None,NULL, 0);
                            break;
                        case MT9M114_ColorEffect_Mono:
                            #if MT9M114_PRINT
                            printk("MT9M114_ColorEffect_Mono\n");
                            #endif
                            err = mt9m114_write_table(info->i2c_client, ColorEffect_Mono,NULL, 0);
                            break;
                        case MT9M114_ColorEffect_Sepia:
                          #if MT9M114_PRINT
                            printk("MT9M114_ColorEffect_Sepia\n");
                            #endif
                            err = mt9m114_write_table(info->i2c_client, ColorEffect_Sepia,NULL, 0);
                            break;
                        case MT9M114_ColorEffect_Negative:
                            #if MT9M114_PRINT
                            printk("MT9M114_ColorEffect_Negative\n");
                           #endif //
                            err = mt9m114_write_table(info->i2c_client, ColorEffect_Negative,NULL, 0);
                            break;
                        case MT9M114_ColorEffect_Aqua:
                            #if MT9M114_PRINT
                            printk("MT9M114_ColorEffect_Aqua\n");
                            #endif
                            err = mt9m114_write_table(info->i2c_client, ColorEffect_Bluish,NULL, 0);
                             break;
                        default:
                             break;
                    }
           
            if(err)
                return err;
            return 0;	
	}
	default:
		return -EINVAL;
	}
	return 0;
}

static struct mt9m114_info *info;

static int mt9m114_open(struct inode *inode, struct file *file)
{
	u8 status;
	file->private_data = info;
	if (info->pdata && info->pdata->power_on)
		info->pdata->power_on();
	mt9m114_get_status(info, &status);
	return 0;
}

int mt9m114_release(struct inode *inode, struct file *file)
{
	if (info->pdata && info->pdata->power_off)
		info->pdata->power_off();
	file->private_data = NULL;
	return 0;
}


static const struct file_operations mt9m114_fileops = {
	.owner = THIS_MODULE,
	.open = mt9m114_open,
	.unlocked_ioctl = mt9m114_ioctl,
	.release = mt9m114_release,
};

static struct miscdevice mt9m114_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mt9m114",
	.fops = &mt9m114_fileops,
};

static int mt9m114_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;

	printk("mt9m114: probing sensor.\n");

	info = kzalloc(sizeof(struct mt9m114_info), GFP_KERNEL);
	if (!info) {
		pr_err("mt9m114: Unable to allocate memory!\n");
		return -ENOMEM;
	}

	err = misc_register(&mt9m114_device);
	if (err) {
		pr_err("mt9m114: Unable to register misc device!\n");
		kfree(info);
		return err;
	}

	info->pdata = client->dev.platform_data;
	info->i2c_client = client;

	i2c_set_clientdata(client, info);
	return 0;
}

static int mt9m114_remove(struct i2c_client *client)
{
	struct mt9m114_info *info;
	info = i2c_get_clientdata(client);
	misc_deregister(&mt9m114_device);
	kfree(info);
	return 0;
}
//
static const struct i2c_device_id mt9m114_id[] = {
	{ "mt9m114", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, mt9m114_id);

static struct i2c_driver mt9m114_i2c_driver = {
	.driver = {
		.name = "mt9m114",
		.owner = THIS_MODULE,
	},
	.probe = mt9m114_probe,
	.remove = mt9m114_remove,
	.id_table = mt9m114_id,
};

static int __init mt9m114_init(void)
{
	printk("mt9m114 sensor driver loading\n");
	return i2c_add_driver(&mt9m114_i2c_driver);
}

static void __exit mt9m114_exit(void)
{
	i2c_del_driver(&mt9m114_i2c_driver);
}

module_init(mt9m114_init);
module_exit(mt9m114_exit);

