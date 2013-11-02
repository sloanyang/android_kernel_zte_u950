
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

#ifndef __OV5640_H__
#define __OV5640_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

#define OV5640_IOCTL_SET_MODE		_IOW('o', 1, struct ov5640_mode)

/* ZTE: modify by yaoling for yuv balance , EFFECT,scene  20110812 ++ */
/* #define OV5640_IOCTL_SET_FRAME_LENGTH	_IOW('o', 2, __u32)
#define OV5640_IOCTL_SET_COARSE_TIME	_IOW('o', 3, __u32)
#define OV5640_IOCTL_SET_GAIN		_IOW('o', 4, __u16)
#define OV5640_IOCTL_GET_STATUS		_IOR('o', 5, __u8)
*/
#define OV5640_IOCTL_GET_STATUS		_IOR('o', 2, __u8)
#define OV5640_IOCTL_SET_COLOR_EFFECT   _IOW('o', 3, __u8)
#define OV5640_IOCTL_SET_WHITE_BALANCE  _IOW('o', 4,enum ov5640_balance_mode)
#define OV5640_IOCTL_SET_SCENE_MODE     _IOW('o', 5, enum ov5640_scene_mode)
#define OV5640_IOCTL_GET_EXP     _IOR('o', 11, __u8)
#define OV5640_IOCTL_SET_AF_MODE  _IOW('o', 6, __u8)
#define OV5640_IOCTL_GET_AF_STATUS  _IOR('o', 12, __u8)
/* ZTE: modify by yaoling for yuv balance,EFFECT,scene 20110812 -- */

#define OV5640_IOCTL_TEST_PATTERN	_IOW('o', 7, enum ov5640_test_pattern)
/* ZTE: add  by yaoling for yuv expose 20110812 ++ */
#define OV5640_IOCTL_SET_EXPOSURE       _IOW('o', 8, int)
 /* ZTE: add by yaoling for yuv expose 20110812 -- */
#define OV5640_IOCTL_SET_BRIGHTNESS  _IOW('o', 9, enum ov5640_Brightness_mode)
#define OV5640_IOCTL_SET_CONTRAST      _IOW('o', 13, enum ov5640_Contrast_mode)
//#define OV5640_IOCTL_SET_CONTRAST      _IOW('o', 13, int)
#define OV5640_IOCTL_SET_SATURATION   _IOW('o', 14, enum ov5640_Saturation_mode)
#define OV5640_IOCTL_SET_CAMERA_MODE	_IOW('o', 10, __u32)
 /* ZTE: add by yaoling for sharpness iso 20111010  ++ */
#define OV5640_IOCTL_SET_SHARPNESS _IOW('o', 15,enum ov5640_Sharpness_mode )
#define OV5640_IOCTL_SET_ISO _IOW('o', 16,enum ov5640_Iso_mode )
 /* ZTE: add by yaoling for sharpness  iso 20111010  -- */
/* ZTE: add by yaoling for photoflash 20111202  */
#define OV5640_IOCTL_SET_ANTIBANDING _IOW('o', 17,enum ov5640_Antibanding_mode )
#define OV5640_IOCTL_SET_AF_REGION_MODE  _IOW('o', 18, int)
#define OV5640_IOCTL_SET_CONTINUE_AF_MODE  _IOW('o', 19, __u8) 
#define OV5640_IOCTL_SET_CONTINUE_AF_PAUSE  _IOW('o', 20, __u8) 
#define OV5640_IOCTL_SET_INFINITY_AF_MODE  _IOW('o', 21, __u8) 
enum ov5640_test_pattern {
        OV5640_TEST_PATTERN_NONE,
        OV5640_TEST_PATTERN_COLORBARS,
        OV5640_TEST_PATTERN_CHECKERBOARD

};

/* ZTE: add  by yaoling for yuv balance,expose,scene  20110812 ++ */
enum {
        OV5640_ColorEffect = 0,
        OV5640_Whitebalance,
        OV5640_SceneMode,
        OV5640_Exposure,
        OV5640_Brightness,
        OV5640_Iso,
        OV5640_Contrast,
        OV5640_Saturation,
        OV5640_Shrpness,
        OV5640_Flashmode,
        OV5640_Antibanding,
        OV5640_FocusMode
        
};
/* ZTE: add by yaoling for photoflash 20111202 ++  */
enum ov5640_Antibanding_mode{
        OV5640_Antibanding_50hz=1,
        OV5640_Antibanding_60hz,
        OV5640_Antibanding_Auto,
        OV5640_Antibanding_Off

};
/* ZTE: add by yaoling for photoflash 20111202 --  */
enum ov5640_balance_mode{
        OV5640_Whitebalance_Invalid = 0,
        OV5640_Whitebalance_Auto,
        OV5640_Whitebalance_Incandescent,
        OV5640_Whitebalance_Fluorescent,
        OV5640_Whitebalance_WarmFluorescent,
        OV5640_Whitebalance_Daylight,
        OV5640_Whitebalance_Cloudy
};
enum ov5640_scene_mode{
        OV5640_SceneMode_Invalid = 0,
        OV5640_SceneMode_Auto,
        OV5640_SceneMode_Action,
        OV5640_SceneMode_Portrait,
        OV5640_SceneMode_Landscape,
        OV5640_SceneMode_Beach,
        OV5640_SceneMode_Candlelight,
        OV5640_SceneMode_Fireworks,
        OV5640_SceneMode_Night,
        OV5640_SceneMode_NightPortrait,
        OV5640_SceneMode_Party,
        OV5640_SceneMode_Snow,
        OV5640_SceneMode_Sports,
        OV5640_SceneMode_SteadyPhoto,
        OV5640_SceneMode_Sunset,
        OV5640_SceneMode_Theatre,
        OV5640_SceneMode_Barcode,
        OV5640_SceneMode_Backlight,
        OV5640_SceneMode_Normal
};
enum ov5640_exposure_mode{
        OV5640_Exposure_0,
        OV5640_Exposure_1,
        OV5640_Exposure_2,
        OV5640_Exposure_Negative_1=3,//-1,
        OV5640_Exposure_Negative_2 =4, //-2
 
};
enum ov5640_ColorEffect_mode{
        OV5640_ColorEffect_Invalid = 0,
        OV5640_ColorEffect_Aqua,
        OV5640_ColorEffect_Blackboard,
        OV5640_ColorEffect_Mono,
        OV5640_ColorEffect_Negative,
        OV5640_ColorEffect_None,
        OV5640_ColorEffect_Posterize,
        OV5640_ColorEffect_Sepia,
        OV5640_ColorEffect_Solarize,
        OV5640_ColorEffect_Whiteboard,
       /* ZTE:add by yaoling for effect red blue green 20111108 ++ */
       OV5640_ColorEffect_vivid,
	   OV5640_YUV_ColorEffect_Emboss,
        OV5640_ColorEffect_redtint,
        OV5640_ColorEffect_bluetint,
        OV5640_ColorEffect_greentint
        /* ZTE:add by yaoling for effect red blue green 20111108 -- */
};

/* ZTE: add  by yaoling for yuv balance,expose,scene  20110812 -- */
enum ov5640_Brightness_mode{
        OV5640_Brightness_Level1= 30,
        OV5640_Brightness_Level2 =40,
        OV5640_Brightness_Level3 = 50,
        OV5640_Brightness_Level4 = 60,
        OV5640_Brightness_Level5 = 70,
        OV5640_Brightness_Level6 = 80,
};
enum ov5640_Contrast_mode{
        OV5640_Contrast_Level1 =1, //-100,
        OV5640_Contrast_Level2 =2,// -50,
        OV5640_Contrast_Level3 = 0,
        OV5640_Contrast_Level4 = 50,
        OV5640_Contrast_Level5 = 100
};
enum ov5640_Saturation_mode{
        OV5640_Saturation_Level1 =1, //-100,
        OV5640_Saturation_Level2 =2, //-50,
        OV5640_Saturation_Level3 = 0,
        OV5640_Saturation_Level4 = 50,
        OV5640_Saturation_Level5 = 100
};
  /* ZTE: add by yaoling for sharpness iso  20111010 ++ */
 enum ov5640_Sharpness_mode{
        OV5640_Sharpness_Level1= 0,
        OV5640_Sharpness_Level2,
        OV5640_Sharpness_Level3,
        OV5640_Sharpness_Level4,
        OV5640_Sharpness_Level5
};
  enum ov5640_Iso_mode{
        OV5640_Iso_Level1= 0,
        OV5640_Iso_Level2 = 100 ,
        OV5640_Iso_Level3 = 200,
        OV5640_Iso_Level4 = 400,
        OV5640_Iso_Level5 = 800,
        OV5640_Iso_Level6 = 1600
};
  /* ZTE: add by yaoling for sharpness iso 20111010 -- */   
  
struct ov5640_mode {
	int xres;
	int yres;
	__u32 frame_length;
	__u32 coarse_time;
	__u16 gain;
};
#ifdef __KERNEL__
struct ov5640_platform_data {
	int (*power_on)(void);
	int (*power_off)(void);

};
#endif /* __KERNEL__ */

#endif  /* __OV5640_H__ */
