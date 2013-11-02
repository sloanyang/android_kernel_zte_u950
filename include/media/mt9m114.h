/**
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __MT9M114_H__
#define __MT9M114_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

#define MT9M114_IOCTL_SET_MODE		_IOW('o', 1, struct mt9m114_mode)
#define MT9M114_IOCTL_GET_STATUS		_IOR('o', 2, __u8)
#define MT9M114_IOCTL_SET_COLOR_EFFECT   _IOW('o', 3,  enum mt9m114_ColorEffect_mode)
#define MT9M114_IOCTL_SET_WHITE_BALANCE  _IOW('o', 4, enum mt9m114_balance_mode)
#define MT9M114_IOCTL_SET_CONTRAST       _IOW('o', 5,  enum mt9m114_Contrast_mode)
#define MT9M114_IOCTL_SET_BRIGHTNESS  _IOW('o', 6, enum mt9m114_Brightness_mode)

struct mt9m114_mode {
	int xres;
	int yres;
};
enum {
        MT9M114_ColorEffect = 0,
        MT9M114_Whitebalance,
        MT9M114_SceneMode,
        MT9M114_Exposure,
        /* ZTE: add by yaoling for 9v114 brightness 20110930 */
         MT9M114_Brightness ,
	 MT9M114_Iso,
        MT9M114_Contrast
};

enum mt9m114_Brightness_mode{
        mt9m114_Brightness_Level1= 30,
        mt9m114_Brightness_Level2 = 40,
        mt9m114_Brightness_Level3 = 50,
        mt9m114_Brightness_Level4 = 60,
        mt9m114_Brightness_Level5 = 70,
        mt9m114_Brightness_Level6 = 80
};
enum mt9m114_balance_mode{
        MT9M114_Whitebalance_Invalid = 0,
        MT9M114_Whitebalance_Auto,
        MT9M114_Whitebalance_Incandescent,
        MT9M114_Whitebalance_Fluorescent,
        MT9M114_Whitebalance_WarmFluorescent,
        MT9M114_Whitebalance_Daylight,
        MT9M114_Whitebalance_Cloudy
};
enum mt9m114_Contrast_mode{
        MT9M114_Contrast_Level1 =1, //-100,
        MT9M114_Contrast_Level2 =2,// -50,
        MT9M114_Contrast_Level3 = 0,
        MT9M114_Contrast_Level4 = 50,
        MT9M114_Contrast_Level5 = 100
};
enum mt9m114_ColorEffect_mode{
	MT9M114_ColorEffect_Invalid = 0,
	MT9M114_ColorEffect_Aqua,
	MT9M114_ColorEffect_Blackboard,
	MT9M114_ColorEffect_Mono,
	MT9M114_ColorEffect_Negative,
	MT9M114_ColorEffect_None,
	MT9M114_ColorEffect_Posterize,
	MT9M114_ColorEffect_Sepia,
	MT9M114_ColorEffect_Solarize,
	MT9M114_ColorEffect_Whiteboard,
	MT9M114_ColorEffect_vivid,
	MT9M114_YUV_ColorEffect_Emboss,
	MT9M114_ColorEffect_redtint,
	MT9M114_ColorEffect_bluetint,
	MT9M114_ColorEffect_greentint
     
};
#ifdef __KERNEL__
struct mt9m114_platform_data {
	int (*power_on)(void);
	int (*power_off)(void);

};
#endif /* __KERNEL__ */

#endif  /* __MT9M114_H__ */

