/*
 * arch/arm/mach-tegra/board-enterprise-panel.c
 *
 * Copyright (c) 2011-2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/resource.h>
#include <asm/mach-types.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/tegra_pwm_bl.h>
#include <asm/atomic.h>
#include <linux/nvhost.h>
#include <linux/nvmap.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>
#include <mach/hardware.h>
#include <linux/proc_fs.h>

#include "board.h"
#include "board-enterprise.h"
#include "devices.h"
#include "gpio-names.h"

#define DC_CTRL_MODE   0//TEGRA_DC_OUT_ONE_SHOT_MODE // zte lipeng10094834 modify 20120525

/* Select panel to be used. */
#define AVDD_LCD PMU_TCA6416_GPIO_PORT17
#define DSI_PANEL_RESET 0

#define enterprise_lvds_shutdown	TEGRA_GPIO_PL2
#define enterprise_hdmi_hpd		TEGRA_GPIO_PN7

#define enterprise_dsi_panel_reset	TEGRA_GPIO_PW0

#define enterprise_lcd_2d_3d		TEGRA_GPIO_PH1
#define ENTERPRISE_STEREO_3D		0
#define ENTERPRISE_STEREO_2D		1

#define enterprise_lcd_swp_pl		TEGRA_GPIO_PH2
#define ENTERPRISE_STEREO_LANDSCAPE	0
#define ENTERPRISE_STEREO_PORTRAIT	1

#define enterprise_lcd_te		TEGRA_GPIO_PJ1
#define enterprise_lcd_id		TEGRA_GPIO_PE3
#ifdef CONFIG_TEGRA_DC
static struct regulator *enterprise_dsi_reg;
static bool dsi_regulator_status;
static struct regulator *enterprise_lcd_reg = NULL;

static struct regulator *enterprise_hdmi_reg;
static struct regulator *enterprise_hdmi_pll;
static struct regulator *enterprise_hdmi_vddio;
#endif
//zte lipeng10094834 add  20120525
int g_SuspendFlag = 0;
static  int g_lcdid = 0;
#ifdef CONFIG_BACKLIGHT_1_WIRE_MODE
#define TPS61165_CTRL_PIN		TEGRA_GPIO_PW1 
#endif
#define ventana_lcd_1v8_enable	    TEGRA_GPIO_PB2
//zte lipeng10094834 add end 20120525
#define DEFAULT_BRIGHTNESS (132)      /*ZTE: added by tong.weili 初始默认亮度20111130*/
#ifdef CONFIG_BACKLIGHT_1_WIRE_MODE
/*ZTE: added by tong.weili for 1-wire backlight 20111122 ++*/
static int debug = 0;
module_param(debug, int, 0600);
static spinlock_t s_tps61165_lock;
static struct mutex s_tps61165_mutex;  /*ZTE: added by tong.weili 增加背光设置稳定性 20111128*/
static bool s_tps61165_is_inited = true;  /*ZTE: modified  by tong.weili for 已在bootloader中初始化20111130*/
static bool s_bNeedSetBacklight = false; /*ZTE: added by tong.weili 解决开机刚进入kernel 设置默认背光会闪一下20111130*/
static int tps61165_init(void);
static int tps61165_write_bit(u8 b);
static int tps61165_write_byte(u8 bytedata);
static int tps61165_config_ES_timing(void);
int tps61165_set_backlight(int brightness);
static int tps61165_shutdown(void);
/*ZTE: added by tong.weili for 1-wire backlight 20111122 --*/
#endif
static atomic_t sd_brightness = ATOMIC_INIT(255);

static tegra_dc_bl_output enterprise_bl_output_measured_a02 = {
	1, 5, 9, 10, 11, 12, 12, 13,
	13, 14, 14, 15, 15, 16, 16, 17,
	17, 18, 18, 19, 19, 20, 21, 21,
	22, 22, 23, 24, 24, 25, 26, 26,
	27, 27, 28, 29, 29, 31, 31, 32,
	32, 33, 34, 35, 36, 36, 37, 38,
	39, 39, 40, 41, 41, 42, 43, 43,
	44, 45, 45, 46, 47, 47, 48, 49,
	49, 50, 51, 51, 52, 53, 53, 54,
	55, 56, 56, 57, 58, 59, 60, 61,
	61, 62, 63, 64, 65, 65, 66, 67,
	67, 68, 69, 69, 70, 71, 71, 72,
	73, 73, 74, 74, 75, 76, 76, 77,
	77, 78, 79, 79, 80, 81, 82, 83,
	83, 84, 85, 85, 86, 86, 88, 89,
	90, 91, 91, 92, 93, 93, 94, 95,
	95, 96, 97, 97, 98, 99, 99, 100,
	101, 101, 102, 103, 103, 104, 105, 105,
	107, 107, 108, 109, 110, 111, 111, 112,
	113, 113, 114, 115, 115, 116, 117, 117,
	118, 119, 119, 120, 121, 122, 123, 124,
	124, 125, 126, 126, 127, 128, 129, 129,
	130, 131, 131, 132, 133, 133, 134, 135,
	135, 136, 137, 137, 138, 139, 139, 140,
	142, 142, 143, 144, 145, 146, 147, 147,
	148, 149, 149, 150, 151, 152, 153, 153,
	153, 154, 155, 156, 157, 158, 158, 159,
	160, 161, 162, 163, 163, 164, 165, 165,
	166, 166, 167, 168, 169, 169, 170, 170,
	171, 172, 173, 173, 174, 175, 175, 176,
	176, 178, 178, 179, 180, 181, 182, 182,
	183, 184, 185, 186, 186, 187, 188, 188
};

#if 0   //ori data from NV
static tegra_dc_bl_output enterprise_bl_output_measured_a03 = {
	0, 1, 2, 3, 4, 5, 6, 7,
	8, 9, 10, 12, 13, 14, 15, 16,
	17, 19, 20, 21, 22, 22, 23, 24,
	25, 26, 27, 28, 29, 29, 30, 32,
	33, 34, 35, 36, 38, 39, 40, 42,
	43, 44, 46, 47, 49, 50, 51, 52,
	53, 54, 55, 56, 57, 58, 59, 60,
	61, 63, 64, 66, 67, 69, 70, 71,
	72, 73, 74, 75, 76, 77, 78, 79,
	80, 81, 82, 83, 84, 84, 85, 86,
	87, 88, 89, 90, 91, 92, 93, 94,
	95, 96, 97, 98, 99, 100, 101, 102,
	103, 104, 105, 106, 107, 108, 109, 110,
	110, 111, 112, 113, 113, 114, 115, 116,
	116, 117, 118, 118, 119, 120, 121, 122,
	123, 124, 125, 126, 127, 128, 129, 130,
	130, 131, 132, 133, 134, 135, 136, 137,
	138, 139, 140, 141, 142, 143, 144, 145,
	146, 147, 148, 149, 150, 151, 152, 153,
	154, 155, 156, 157, 158, 159, 160, 160,
	161, 162, 163, 163, 164, 165, 165, 166,
	167, 168, 168, 169, 170, 171, 172, 173,
	174, 175, 176, 176, 177, 178, 179, 180,
	181, 182, 183, 184, 185, 186, 187, 188,
	189, 190, 191, 191, 192, 193, 194, 194,
	195, 196, 197, 197, 198, 199, 199, 200,
	202, 203, 205, 206, 208, 209, 211, 212,
	213, 215, 216, 218, 219, 220, 221, 222,
	223, 224, 225, 226, 227, 228, 229, 230,
	231, 232, 233, 234, 235, 236, 237, 238,
	239, 240, 241, 243, 244, 245, 247, 248,
	250, 251, 251, 252, 253, 254, 254, 255,
};
#else
static tegra_dc_bl_output enterprise_bl_output_measured_a03 = {
	  0,  1,  2,  3,  4,  4,  5,  6,  7,  8,
	  9, 10, 10, 11, 11, 12, 12, 13, 14, 14,
	 15, 15, 16, 16, 17, 17, 18, 18, 19, 19,
	 20, 20, 21, 21, 22, 23, 23, 24, 24, 25,
	 26, 26, 27, 27, 28, 28, 29, 30, 30, 31,
	 31, 32, 32, 33, 33, 34, 34, 34, 35, 35,
	 36, 36, 37, 38, 38, 39, 39, 40, 40, 41,
	 42, 42, 43, 44, 45, 45, 46, 47, 48, 48,
	 49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
	 49, 50, 51, 52, 53, 54, 55, 55, 56, 57,
	 58, 59, 59, 60, 60, 60, 61, 61, 62, 62,
	 63, 63, 64, 64, 64, 65, 65, 66, 66, 67,
	 67, 68, 69, 70, 71, 72, 72, 73, 74, 75,
	 76, 77, 79, 80, 81, 83, 84, 85, 87, 88,
	 89, 91, 92, 94, 95, 96, 98, 99,100,102,
	103,104,106,107,108,110,111,112,114,115,
	116,117,118,119,120,121,122,123,123,124,
	125,126,127,128,129,130,131,132,132,133,
	134,135,136,137,138,139,140,140,141,142,
	143,145,146,147,149,150,151,153,154,155,
	157,157,158,159,160,161,162,163,164,165,
	166,167,168,170,171,172,174,175,176,178,
	179,180,182,183,184,186,187,188,190,191,
	192,194,195,196,198,199,200,202,203,204,
	206,209,212,215,218,221,225,228,231,234,
	237,241,244,248,251,255
};
#endif

static p_tegra_dc_bl_output bl_output;

static bool kernel_1st_panel_init = false;

static int enterprise_backlight_notify(struct device *unused, int brightness)
{
/*ZTE: modified by tong.weili 解决背光变化非线性20120815 ++*/
	int cur_sd_brightness = atomic_read(&sd_brightness);
       static int old_brigntness = 0;

    //   printk("enterprise_backlight_notify: Brightness %d old_brigntness %d!\n", brightness, old_brigntness);
       if(g_SuspendFlag == 1)
            brightness = 0;

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");

       if(old_brigntness<=1 && brightness>0)
       {
            msleep(100);
       }
       old_brigntness = brightness;

 /*ZTE: modified by tong.weili 解决背光变化非线性20120815 --*/
       return brightness;
}

static int enterprise_disp1_check_fb(struct device *dev, struct fb_info *info);

/*
 * In case which_pwm is TEGRA_PWM_PM0,
 * gpio_conf_to_sfio should be TEGRA_GPIO_PW0: set LCD_CS1_N pin to SFIO
 * In case which_pwm is TEGRA_PWM_PM1,
 * gpio_conf_to_sfio should be TEGRA_GPIO_PW1: set LCD_M1 pin to SFIO
 */
static struct platform_tegra_pwm_backlight_data enterprise_disp1_backlight_data = {
	.which_dc		= 0,
	.which_pwm		= TEGRA_PWM_PM1,
	.gpio_conf_to_sfio	= TEGRA_GPIO_PM5,
	.switch_to_sfio		= &tegra_gpio_disable,
	.max_brightness		= 255,
	.dft_brightness		= 224,
	.notify		= enterprise_backlight_notify,
	.period			= 0xFF,
	.clk_div		= 0x3FF,
	.clk_select		= 0,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= enterprise_disp1_check_fb,
};

static struct platform_device enterprise_disp1_backlight_device = {
	.name	= "tegra-pwm-bl",
	.id	= -1,
	.dev	= {
		.platform_data = &enterprise_disp1_backlight_data,
	},
};

#ifdef CONFIG_TEGRA_DC
static int enterprise_hdmi_vddio_enable(void)
{
	int ret;
	if (!enterprise_hdmi_vddio) {
		enterprise_hdmi_vddio = regulator_get(NULL, "hdmi_5v0");
		if (IS_ERR_OR_NULL(enterprise_hdmi_vddio)) {
			ret = PTR_ERR(enterprise_hdmi_vddio);
			pr_err("hdmi: couldn't get regulator hdmi_5v0\n");
			enterprise_hdmi_vddio = NULL;
			return ret;
		}
	}
	ret = regulator_enable(enterprise_hdmi_vddio);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator hdmi_5v0\n");
		regulator_put(enterprise_hdmi_vddio);
		enterprise_hdmi_vddio = NULL;
		return ret;
	}
	return ret;
}

static int enterprise_hdmi_vddio_disable(void)
{
	if (enterprise_hdmi_vddio) {
		regulator_disable(enterprise_hdmi_vddio);
		regulator_put(enterprise_hdmi_vddio);
		enterprise_hdmi_vddio = NULL;
	}
	return 0;
}

static int enterprise_hdmi_enable(void)
{
	int ret;
	if (!enterprise_hdmi_reg) {
		enterprise_hdmi_reg = regulator_get(NULL, "avdd_hdmi");
		if (IS_ERR_OR_NULL(enterprise_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			enterprise_hdmi_reg = NULL;
			return PTR_ERR(enterprise_hdmi_reg);
		}
	}
	ret = regulator_enable(enterprise_hdmi_reg);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi\n");
		return ret;
	}
	if (!enterprise_hdmi_pll) {
		enterprise_hdmi_pll = regulator_get(NULL, "avdd_hdmi_pll");
		if (IS_ERR_OR_NULL(enterprise_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			enterprise_hdmi_pll = NULL;
			regulator_put(enterprise_hdmi_reg);
			enterprise_hdmi_reg = NULL;
			return PTR_ERR(enterprise_hdmi_pll);
		}
	}
	ret = regulator_enable(enterprise_hdmi_pll);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi_pll\n");
		return ret;
	}
	return 0;
}

static int enterprise_hdmi_disable(void)
{

	regulator_disable(enterprise_hdmi_reg);
	regulator_put(enterprise_hdmi_reg);
	enterprise_hdmi_reg = NULL;

	regulator_disable(enterprise_hdmi_pll);
	regulator_put(enterprise_hdmi_pll);
	enterprise_hdmi_pll = NULL;

	return 0;
}
static struct resource enterprise_disp1_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.start	= 0,	/* Filled in by enterprise_panel_init() */
		.end	= 0,	/* Filled in by enterprise_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "dsi_regs",
		.start	= TEGRA_DSI_BASE,
		.end	= TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource enterprise_disp2_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_B_GENERAL,
		.end	= INT_DISPLAY_B_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY2_BASE,
		.end	= TEGRA_DISPLAY2_BASE + TEGRA_DISPLAY2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.flags	= IORESOURCE_MEM,
		.start	= 0,
		.end	= 0,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct tegra_dc_sd_settings enterprise_sd_settings = {
	.enable = 1, /* Normal mode operation */
	.use_auto_pwm = false,
	.hw_update_delay = 0,
	.bin_width = -1,
	.aggressiveness = 1,
	.phase_in_adjustments = true,
	.use_vid_luma = false,
	/* Default video coefficients */
	.coeff = {5, 9, 2},
	.fc = {0, 0},
	/* Immediate backlight changes */
	.blp = {1024, 255},
	/* Gammas: R: 2.2 G: 2.2 B: 2.2 */
	/* Default BL TF */
	.bltf = {
			{
				{57, 65, 74, 83},
				{93, 103, 114, 126},
				{138, 151, 165, 179},
				{194, 209, 225, 242},
			},
			{
				{58, 66, 75, 84},
				{94, 105, 116, 127},
				{140, 153, 166, 181},
				{196, 211, 227, 244},
			},
			{
				{60, 68, 77, 87},
				{97, 107, 119, 130},
				{143, 156, 170, 184},
				{199, 215, 231, 248},
			},
			{
				{64, 73, 82, 91},
				{102, 113, 124, 137},
				{149, 163, 177, 192},
				{207, 223, 240, 255},
			},
		},
	/* Default LUT */
	.lut = {
			{
				{250, 250, 250},
				{194, 194, 194},
				{149, 149, 149},
				{113, 113, 113},
				{82, 82, 82},
				{56, 56, 56},
				{34, 34, 34},
				{15, 15, 15},
				{0, 0, 0},
			},
			{
				{246, 246, 246},
				{191, 191, 191},
				{147, 147, 147},
				{111, 111, 111},
				{80, 80, 80},
				{55, 55, 55},
				{33, 33, 33},
				{14, 14, 14},
				{0, 0, 0},
			},
			{
				{239, 239, 239},
				{185, 185, 185},
				{142, 142, 142},
				{107, 107, 107},
				{77, 77, 77},
				{52, 52, 52},
				{30, 30, 30},
				{12, 12, 12},
				{0, 0, 0},
			},
			{
				{224, 224, 224},
				{173, 173, 173},
				{133, 133, 133},
				{99, 99, 99},
				{70, 70, 70},
				{46, 46, 46},
				{25, 25, 25},
				{7, 7, 7},
				{0, 0, 0},
			},
		},
	.sd_brightness = &sd_brightness,
	.bl_device = &enterprise_disp1_backlight_device,
};

static struct tegra_fb_data enterprise_hdmi_fb_data = {
	.win		= 0,
#if CONFIG_LCD_480_800
 	.xres		= 480,
	.yres		= 800,
 #else
	.xres		= 720,
	.yres		= 1280,
#endif
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_out enterprise_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,
	.parent_clk	= "pll_d2_out0",

	.dcc_bus	= 3,
	.hotplug_gpio	= enterprise_hdmi_hpd,

	.max_pixclock	= KHZ2PICOS(148500),

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= enterprise_hdmi_enable,
	.disable	= enterprise_hdmi_disable,
	.postsuspend	= enterprise_hdmi_vddio_disable,
	.hotplug_init	= enterprise_hdmi_vddio_enable,
};

static struct tegra_dc_platform_data enterprise_disp2_pdata = {
	.flags		= 0,
	.default_out	= &enterprise_disp2_out,
	.fb		= &enterprise_hdmi_fb_data,
	.emc_clk_rate	= 300000000,
};

static int avdd_dsi_csi_rail_enable(void)
{
	int ret;
      printk("avdd_dsi_csi_rail_enable \n");
	if (dsi_regulator_status == true)
		return 0;

	if (enterprise_dsi_reg == NULL) {
		enterprise_dsi_reg = regulator_get(NULL, "avdd_dsi_csi");
		if (IS_ERR_OR_NULL(enterprise_dsi_reg)) {
			pr_err("dsi: Could not get regulator avdd_dsi_csi\n");
			enterprise_dsi_reg = NULL;
			return PTR_ERR(enterprise_dsi_reg);
		}
	}
	ret = regulator_enable(enterprise_dsi_reg);
	if (ret < 0) {
		pr_err("DSI regulator avdd_dsi_csi could not be enabled\n");
		return ret;
	}
	dsi_regulator_status = true;
      printk("avdd_dsi_csi_rail_enable end\n");
	return 0;
}

static int avdd_dsi_csi_rail_disable(void)
{
	int ret;
      printk("avdd_dsi_csi_rail_disable \n");
	if (dsi_regulator_status == false)
		return 0;

	if (enterprise_dsi_reg == NULL) {
		pr_warn("%s: unbalanced disable\n", __func__);
		return -EIO;
	}

	ret = regulator_disable(enterprise_dsi_reg);
	if (ret < 0) {
		pr_err("DSI regulator avdd_dsi_csi cannot be disabled\n");
		return ret;
	}
	dsi_regulator_status = false;
          printk("avdd_dsi_csi_rail_disable end \n");

	return 0;
}
extern void tegra_read_lcd_data();
static int enterprise_dsi_panel_enable(void)
{
	int ret;
	struct board_info board_info;

     printk("enterprise_dsi_panel_enable \n");
	tegra_get_board_info(&board_info);
      	ret = avdd_dsi_csi_rail_enable();
       if (ret)
        	return ret;
  

#if DSI_PANEL_RESET

/*	if (board_info.fab >= BOARD_FAB_A03) {
		if (enterprise_lcd_reg == NULL) {
			enterprise_lcd_reg = regulator_get(NULL, "lcd_vddio_en");
			if (IS_ERR_OR_NULL(enterprise_lcd_reg)) {
				pr_err("Could not get regulator lcd_vddio_en\n");
				ret = PTR_ERR(enterprise_lcd_reg);
				enterprise_lcd_reg = NULL;
				return ret;
			}
		}
		if (enterprise_lcd_reg != NULL) {
			ret = regulator_enable(enterprise_lcd_reg);
			if (ret < 0) {
				pr_err("Could not enable lcd_vddio_en\n");
				return ret;
			}
		}
	}
*/
#endif
	if (kernel_1st_panel_init == true)  
        {
           gpio_request(ventana_lcd_1v8_enable,  "ventana_lcd_1v8_enable");
            tegra_gpio_enable(ventana_lcd_1v8_enable);
            gpio_direction_output(ventana_lcd_1v8_enable, 1);
            printk("[lip]:enterprise_panel_init enable 1v8!\n");
            	ret = gpio_request(enterprise_dsi_panel_reset, "panel reset");
		if (ret < 0)
			return ret;

		ret = gpio_direction_output(enterprise_dsi_panel_reset, 1);
		if (ret < 0) {
			gpio_free(enterprise_dsi_panel_reset);
			return ret;
		}
		tegra_gpio_enable(enterprise_dsi_panel_reset);
              mdelay(20);
            kernel_1st_panel_init = false;
        }


	return ret;
}

static int enterprise_dsi_panel_disable(void)
{
       avdd_dsi_csi_rail_disable();
/*	if (enterprise_lcd_reg != NULL)
		regulator_disable(enterprise_lcd_reg);
*/
	return 0;
}
#endif
void enterprise_dsi_lcd_power_restart()
{
    static int initflag = 0;
    int ret;
    avdd_dsi_csi_rail_disable();
    if(initflag == 0)
    {
        gpio_request(ventana_lcd_1v8_enable,  "ventana_lcd_1v8_enable");
        tegra_gpio_enable(ventana_lcd_1v8_enable);
        initflag = 1;
    }
    if (enterprise_lcd_reg == NULL) {
                enterprise_lcd_reg = regulator_get(NULL, "avdd_lcd");
                if (IS_ERR_OR_NULL(enterprise_lcd_reg)) {
                    pr_err("[shangzhi]:Could not get regulator avdd_lcd\n");
                    ret = PTR_ERR(enterprise_lcd_reg);
                    enterprise_lcd_reg = NULL;
                    return;
                }
            }
    if (enterprise_lcd_reg != NULL) {
             ret = regulator_disable(enterprise_lcd_reg);
             if (ret < 0) {
                 pr_err("[shangzhi]:Could not enable avdd_lcd\n");
             }
         }
    gpio_direction_output(ventana_lcd_1v8_enable, 0);

    msleep(50);
    gpio_direction_output(ventana_lcd_1v8_enable, 1);
    if (enterprise_lcd_reg != NULL) {
			ret = regulator_enable(enterprise_lcd_reg);
			if (ret < 0) {
				pr_err("[shangzhi]:Could not enable avdd_lcd\n");
			}
		}

    avdd_dsi_csi_rail_enable();
    printk("[shangzhi]:enterprise_dsi_lcd_power_restart !\n");
}

static void enterprise_stereo_set_mode(int mode)
{
	switch (mode) {
	case TEGRA_DC_STEREO_MODE_2D:
		gpio_set_value(TEGRA_GPIO_PH1, ENTERPRISE_STEREO_2D);
		break;
	case TEGRA_DC_STEREO_MODE_3D:
		gpio_set_value(TEGRA_GPIO_PH1, ENTERPRISE_STEREO_3D);
		break;
	}
}

static void enterprise_stereo_set_orientation(int mode)
{
	switch (mode) {
	case TEGRA_DC_STEREO_LANDSCAPE:
		gpio_set_value(TEGRA_GPIO_PH2, ENTERPRISE_STEREO_LANDSCAPE);
		break;
	case TEGRA_DC_STEREO_PORTRAIT:
		gpio_set_value(TEGRA_GPIO_PH2, ENTERPRISE_STEREO_PORTRAIT);
		break;
	}
}

#ifdef CONFIG_TEGRA_DC
static int enterprise_dsi_panel_postsuspend(void)
{
	/* Disable enterprise dsi rail */
	return avdd_dsi_csi_rail_disable();
}
#endif

static struct tegra_dsi_cmd dsi_init_cmd[]= {
	DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(20),
#if(DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	DSI_CMD_SHORT(0x15, 0x35, 0x00),
#endif
	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(20),
};

static struct tegra_dsi_cmd dsi_early_suspend_cmd[] = {
	DSI_CMD_SHORT(0x05, 0x28, 0x00),
	DSI_DLY_MS(40),
#if(DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	DSI_CMD_SHORT(0x05, 0x34, 0x00),
#endif
	DSI_CMD_SHORT(0x05, 0x10, 0x00),
	DSI_DLY_MS(120),
};

static struct tegra_dsi_cmd dsi_late_resume_cmd[] = {
#if(DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	DSI_CMD_SHORT(0x15, 0x35, 0x00),
#endif
	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(20),
};

static struct tegra_dsi_cmd dsi_suspend_cmd[] = {
	DSI_CMD_SHORT(0x05, 0x28, 0x00),
	DSI_DLY_MS(40),
#if(DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	DSI_CMD_SHORT(0x05, 0x34, 0x00),
#endif
	DSI_CMD_SHORT(0x05, 0x10, 0x00),
	DSI_DLY_MS(120),
};

static u8 TMHX8369B_para1[]={0xB9,0xFF,0x83,0x69};
static u8 TMHX8369B_para2[]={0xB1,0x14,0x85,0x77,0x00,0x11,0x11,0x1F,0x1F};
static u8 TMHX8369B_para3[]={0xB2,0x00,0x10,0x02};
static u8 TMHX8369B_para4[]={0xB3,0x83,0x00,0x31,0x03};
static u8 TMHX8369B_para5[]={0xB4,0x00};
static u8 TMHX8369B_para6[]={0xB5,0x15,0x15,0x3E};
static u8 TMHX8369B_para7[]={0xB6,0x8E,0x8E};
static u8 TMHX8369B_para8[]={0xE0,0x00,0x05,0x0b,0x0a,0x07,0x3F,0x20,0x2f,0x09,0x13,0x0e,0x15,0x16,0x14,0x15,0x11,0x17,0x00,0x05,0x0b,0x0a,0x07,0x3F,0x20,0x2f,0x09,0x13,0x0e,0x15,0x16,0x14,0x15,0x11,0x17,0x01};
static u8 TMHX8369B_para9[]={0xBC,0x5E};
static u8 TMHX8369B_para10[]={0xC6,0x40};
static u8 TMHX8369B_para11[]={0xCC,0x00};
static u8 TMHX8369B_para12[]={0xD5,0x00,0x00,0x12,0x03,0x33,0x00,0x00,0x10,0x01,0x00,0x00,0x00,0x10,0x40,0x14,0x00,0x00,0x23,0x10,0x3e,0x13,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,
		0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xef,0x00,0x22,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xef,0x00,0x33,
		0x00,0x11,0x00,0x51,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x03,0xcc,0xc0,0xff,0xff,0x03,0xcc,0xc0,0xff,0xff,0x00,0x0f,0x5A};
static u8 TMHX8369B_para13[]={0xEA,0x62};
static u8 TMHX8369B_para14[]={0xBA,0x41,0x00,0x16,0xC6,0x80,0x0A,0x00,0x10,0x24,0x02,0x21,0x21,0x9A,0x11,0x14};

static struct tegra_dsi_cmd TMHX8369B_dsi_init_cmd[]= {
	DSI_CMD_LONG(0x39, TMHX8369B_para1),   
	DSI_CMD_LONG(0x39, TMHX8369B_para2),   
	DSI_CMD_LONG(0x39, TMHX8369B_para3),   
	DSI_CMD_LONG(0x39, TMHX8369B_para4),   
	DSI_CMD_LONG(0x39, TMHX8369B_para5),   
	DSI_CMD_LONG(0x39, TMHX8369B_para6),   
	DSI_CMD_LONG(0x39, TMHX8369B_para7),   
	DSI_CMD_LONG(0x39, TMHX8369B_para8),  
	DSI_CMD_LONG(0x39, TMHX8369B_para9),   
	DSI_CMD_LONG(0x39, TMHX8369B_para10),   
	DSI_CMD_LONG(0x39, TMHX8369B_para11),   
	DSI_CMD_LONG(0x39, TMHX8369B_para12),   
	DSI_CMD_LONG(0x39, TMHX8369B_para13),   
    	DSI_CMD_LONG(0x39, TMHX8369B_para14),   


	DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(120),

	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(20),
};

static u8 TMHX8369_para1[]={0xB9,0xFF,0x83,0x69}; 
static u8 TMHX8369_para2[]={0xB1,0xF5,0x00,0x44,0x07,0x00,0x11,0x11,0x36,0x3E,0x3F,0x3F,0x01,0x23,0x01,0xE6,0xE6,0xE6,0xE6,0xE6};  
static u8 TMHX8369_para3[]={0xB2,0x00,0x2B,0x03,0x03,0x70,0x00,0xFF,0x00,0x00,0x00,0x00,0x03,0x03,0x00,0x01};  
static u8 TMHX8369_para4[]={0xB4,0x02,0x18,0x70,0x13,0x05};  
static u8 TMHX8369_para5[]={0xB6,0x18,0x18}; 
static u8 TMHX8369_para6[]={0x36,0x00};
static u8 TMHX8369_para7[]={0xD5,0x00,0x02,0x03,0x25,0x01,0x0a,0x00,0x80,0x11,0x13,0x02,0x13,0xF4,0xF6,0xF5,0xF7,0x31,0x20,0x05,0x07,0x04,0x06,0x07,0x0F,0x04,0x04};
static u8 TMHX8369_para8[]={0xE0,0x00,0x02,0x02,0x0D,0x08,0x14,0x27,0x34,0x0B,0x11,0x11,0x16,0x18,0x15,0x16,0x13,0x17,0x00,0x02,0x02,0x0D,0x08,0x14,0x27,0x34,0x0B,0x11,0x11,0x16,0x18,0x15,0x16,0x13,0x17};
static u8 TMHX8369_para9[]={0x3A,0x77,0xCC,0x08}; 
static u8 TMHX8369_para10[]={0xBA,0x00,0xA0,0xC6,0x00,0x0A,0x00,0x10,0x30,0x6F,0x02,0x11,0x18,0x40};
static u8 TMHX8369_para11[]={0xcc, 0x00};
static struct tegra_dsi_cmd TMHX8369_dsi_init_cmd[]= {
	DSI_CMD_LONG(0x39, TMHX8369_para1),   
	DSI_CMD_LONG(0x39, TMHX8369_para2),   
	DSI_CMD_LONG(0x39, TMHX8369_para3),   
	DSI_CMD_LONG(0x39, TMHX8369_para4),   
	DSI_CMD_LONG(0x39, TMHX8369_para5),   
	DSI_CMD_LONG(0x39, TMHX8369_para6),   
	DSI_CMD_LONG(0x39, TMHX8369_para7),   
	DSI_CMD_LONG(0x39, TMHX8369_para8),  
	DSI_CMD_LONG(0x39, TMHX8369_para9),   
	DSI_CMD_LONG(0x39, TMHX8369_para10),   
	DSI_CMD_LONG(0x39, TMHX8369_para11),   


	DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(120),

	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(20),
};


static u8 NT35510_para1[]={0xF0,0x55,0xAA,0x52,0x08,0x01}; // LV2 Page 1 enable
static u8 NT35510_para2[]={0xB0,0x0D,0x0D,0x0D};  //AVDD Set AVDD 5.2V
static u8 NT35510_para3[]={0xB6,0x34,0x34,0x34};  //AVDD ratio
static u8 NT35510_para4[]={0xB1,0x0D,0x0D,0x0D};  //AVEE -5.2V
static u8 NT35510_para4_1[]={0xB7,0x34,0x34,0x34};  //AVEE -5.2V
static u8 NT35510_para5[]={0xB2,0x00,0x00,0x00}; //VCL -2.5V
static u8 NT35510_para6[]={0xB8,0x24,0x24,0x24}; //VCL ratio
static u8 NT35510_para7[]={0xBF,0x01}; //VGH 15V (Free pump)
static u8 NT35510_para8[]={0xB3,0x0F,0x0F,0x0F};
static u8 NT35510_para9[]={0xB9,0x34,0x34,0x34}; //VGH ratio
static u8 NT35510_para10[]={0xB5,0x08,0x08,0x08}; //VGL_REG -10V
static u8 NT35510_para11[]={0xC2,0x03};
static u8 NT35510_para12[]={0xBA,0x24,0x24,0x24}; //VGLX ratio
static u8 NT35510_para13[]={0xBC,0x00,0x78,0x00}; //VGMP/VGSP 4.5V/0V
static u8 NT35510_para14[]={0xBD,0x00,0x78,0x00}; //VGMN/VGSN -4.5V/0V
static u8 NT35510_para15[]={0xBE,0x00,0x73}; //VCOM
static u8 NT35510_para15_lead[]={0xBE,0x00,0x7e}; //VCOM
static u8 NT35510_para16[]={0xD1,0x00,0x33,0x00,0x34,0x00,0x3A,0x00,0x4A,0x00,0x5C,0x00,0x81,0x00,0xA6,0x00,0xE5,
    0x01,0x13,0x01,0x54,0x01,0x82,0x01,0xCA,0x02,0x00,0x02,0x01,0x02,0x34,0x02,0x67,0x02,0x84,0x02,
    0xA4,0x02,0xB7,0x02,0xCF,0x02,0xDE,0x02,0xF2,0x02,0xFE,0x03,0x10,0x03,0x33,0x03,0x6D};
static u8 NT35510_para17[]={0xD2,0x00,0x33,0x00,0x34,0x00,0x3A,0x00,0x4A,0x00,0x5C,0x00,0x81,0x00,0xA6,0x00,0xE5,
    0x01,0x13,0x01,0x54,0x01,0x82,0x01,0xCA,0x02,0x00,0x02,0x01,0x02,0x34,0x02,0x67,0x02,0x84,
    0x02,0xA4,0x02,0xB7,0x02,0xCF,0x02,0xDE,0x02,0xF2,0x02,0xFE,0x03,0x10,0x03,0x33,0x03,0x6D};
static u8 NT35510_para18[]={0xD3,0x00,0x33,0x00,0x34,0x00,0x3A,0x00,0x4A,0x00,0x5C,0x00,0x81,0x00,0xA6,0x00,0xE5,
    0x01,0x13,0x01,0x54,0x01,0x82,0x01,0xCA,0x02,0x00,0x02,0x01,0x02,0x34,0x02,0x67,0x02,0x84,
    0x02,0xA4,0x02,0xB7,0x02,0xCF,0x02,0xDE,0x02,0xF2,0x02,0xFE,0x03,0x10,0x03,0x33,0x03,0x6D};
static u8 NT35510_para19[]={0xD4,0x00,0x33,0x00,0x34,0x00,0x3A,0x00,0x4A,0x00,0x5C,0x00,0x81,0x00,0xA6,0x00,0xE5,
    0x01,0x13,0x01,0x54,0x01,0x82,0x01,0xCA,0x02,0x00,0x02,0x01,0x02,0x34,0x02,0x67,0x02,0x84,0x02,
    0xA4,0x02,0xB7,0x02,0xCF,0x02,0xDE,0x02,0xF2,0x02,0xFE,0x03,0x10,0x03,0x33,0x03,0x6D};
static u8 NT35510_para20[]={0xD5,0x00,0x33,0x00,0x34,0x00,0x3A,0x00,0x4A,0x00,0x5C,0x00,0x81,0x00,0xA6,0x00,0xE5,
    0x01,0x13,0x01,0x54,0x01,0x82,0x01,0xCA,0x02,0x00,0x02,0x01,0x02,0x34,0x02,0x67,0x02,0x84,
    0x02,0xA4,0x02,0xB7,0x02,0xCF,0x02,0xDE,0x02,0xF2,0x02,0xFE,0x03,0x10,0x03,0x33,0x03,0x6D};
static u8 NT35510_para21[]={0xD6,0x00,0x33,0x00,0x34,0x00,0x3A,0x00,0x4A,0x00,0x5C,0x00,0x81,0x00,0xA6,0x00,0xE5,
    0x01,0x13,0x01,0x54,0x01,0x82,0x01,0xCA,0x02,0x00,0x02,0x01,0x02,0x34,0x02,0x67,0x02,0x84,
    0x02,0xA4,0x02,0xB7,0x02,0xCF,0x02,0xDE,0x02,0xF2,0x02,0xFE,0x03,0x10,0x03,0x33,0x03,0x6D};
static u8 NT35510_para22[]={0xF0,0x55,0xAA,0x52,0x08,0x00}; //LV2 Page 0 enable
static u8 NT35510_para23[]={0xB1,0xCC,0x00};// {0xB1,0xFC,0x00}; //Display control
static u8 NT35510_para24[]={0xB6,0x05}; //Source hold time
static u8 NT35510_para25[]={0xB7,0x70,0x70}; //Gate EQ control
static u8 NT35510_para26[]={0xB8,0x01,0x03,0x03,0x03}; //Source EQ control (Mode 2)
static u8 NT35510_para27[]={0xBC,0x00,0x00,0x00}; // Inversion mode (2-dot)
static u8 NT35510_para28[]={0xC9,0xD0,0x02,0x50,0x50,0x50}; //Timing control 4H w/ 4-delay
static u8 NT35510_para29[]={0x36,0xc0};
static struct tegra_dsi_cmd NT35510_dsi_init_cmd[]= {
	DSI_CMD_LONG(0x39, NT35510_para1),   
	DSI_CMD_LONG(0x39, NT35510_para2),   
	DSI_CMD_LONG(0x39, NT35510_para3),   
	DSI_CMD_LONG(0x39, NT35510_para4),   
	DSI_CMD_LONG(0x39, NT35510_para4_1),
	DSI_CMD_LONG(0x39, NT35510_para5),   
	DSI_CMD_LONG(0x39, NT35510_para6),   
	DSI_CMD_SHORT(0x15, 0xBF,0x01),   
	DSI_CMD_LONG(0x39, NT35510_para8),  
	DSI_CMD_LONG(0x39, NT35510_para9),   
	DSI_CMD_LONG(0x39, NT35510_para10),   
	DSI_CMD_SHORT(0x15,0xC2,0x03), 
 	DSI_CMD_LONG(0x39, NT35510_para12),   
	DSI_CMD_LONG(0x39, NT35510_para13),  
	DSI_CMD_LONG(0x39, NT35510_para14),   
	DSI_CMD_LONG(0x39, NT35510_para15),   
	DSI_CMD_LONG(0x39, NT35510_para16),   
	DSI_CMD_LONG(0x39, NT35510_para17),   
	DSI_CMD_LONG(0x39, NT35510_para18),  
	DSI_CMD_LONG(0x39, NT35510_para19),   
	DSI_CMD_LONG(0x39, NT35510_para20),   
	DSI_CMD_LONG(0x39, NT35510_para21), 
 	DSI_CMD_LONG(0x39, NT35510_para22),   
	DSI_CMD_LONG(0x39, NT35510_para23),  
	DSI_CMD_SHORT(0x15, 0xB6,0x05),    
	DSI_CMD_LONG(0x39, NT35510_para25),   
	DSI_CMD_LONG(0x39, NT35510_para26),   
	DSI_CMD_LONG(0x39, NT35510_para27),   
       DSI_CMD_SHORT(0x15, 0x3A, 0x77),
	DSI_CMD_LONG(0x39, NT35510_para28),  
//	DSI_CMD_LONG(0x39, NT35510_para29),  
#if(DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	DSI_CMD_SHORT(0x15, 0x35, 0x00),
#endif

	DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(120),

	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(20),
};
static struct tegra_dsi_cmd NT35510_dsi_init_cmd_lead[]= {
	DSI_CMD_LONG(0x39, NT35510_para1),   
	DSI_CMD_LONG(0x39, NT35510_para2),   
	DSI_CMD_LONG(0x39, NT35510_para3),   
	DSI_CMD_LONG(0x39, NT35510_para4),   
	DSI_CMD_LONG(0x39, NT35510_para4_1),
	DSI_CMD_LONG(0x39, NT35510_para5),   
	DSI_CMD_LONG(0x39, NT35510_para6),   
	DSI_CMD_SHORT(0x15, 0xBF,0x01),   
	DSI_CMD_LONG(0x39, NT35510_para8),  
	DSI_CMD_LONG(0x39, NT35510_para9),   
	DSI_CMD_LONG(0x39, NT35510_para10),   
	DSI_CMD_SHORT(0x15,0xC2,0x03), 
 	DSI_CMD_LONG(0x39, NT35510_para12),   
	DSI_CMD_LONG(0x39, NT35510_para13),  
	DSI_CMD_LONG(0x39, NT35510_para14),   
	DSI_CMD_LONG(0x39, NT35510_para15_lead),   
	DSI_CMD_LONG(0x39, NT35510_para16),   
	DSI_CMD_LONG(0x39, NT35510_para17),   
	DSI_CMD_LONG(0x39, NT35510_para18),  
	DSI_CMD_LONG(0x39, NT35510_para19),   
	DSI_CMD_LONG(0x39, NT35510_para20),   
	DSI_CMD_LONG(0x39, NT35510_para21), 
 	DSI_CMD_LONG(0x39, NT35510_para22),   
	DSI_CMD_LONG(0x39, NT35510_para23),  
	DSI_CMD_SHORT(0x15, 0xB6,0x05),    
	DSI_CMD_LONG(0x39, NT35510_para25),   
	DSI_CMD_LONG(0x39, NT35510_para26),   
	DSI_CMD_LONG(0x39, NT35510_para27),   
       DSI_CMD_SHORT(0x15, 0x3A, 0x77),
	DSI_CMD_LONG(0x39, NT35510_para28),  
//	DSI_CMD_LONG(0x39, NT35510_para29),  
#if(DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	DSI_CMD_SHORT(0x15, 0x35, 0x00),
#endif

	DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(120),

	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(20),
};
static u8 OTM1281_para1[]={0xff,0x12,0x80,0x01};
static u8 OTM1281_para2[]={0x00,0x80};
static u8 OTM1281_para3[]={0xff,0x12,0x80};


static u8 OTM1281_para6[]={0x00,0x90};
//static u8 OTM1281_para7[]={0xc5,0x10,0x6F,0x02,0x88,0x1D,0x15,0x00,0x04};
static u8 OTM1281_para7[]={0xc5,0x10,0x6F,0x02,0x88,0x1D,0x15,0x00,0x04,0x44,0x44,0x44};
static u8 OTM1281_para8[]={0x00,0xa0};
static u8 OTM1281_para9[]={0xc5,0x10,0x6F,0x02,0x88,0x1D,0x15,0x00,0x04};
static u8 OTM1281_para10[]={0x00,0x80};
static u8 OTM1281_para11[]={0xc5,0x20,0x01,0x00,0xb0,0xb0,0x00,0x04,0x00};
static u8 OTM1281_para12[]={0x00,0x00};
static u8 OTM1281_para13[]={0xd8,0x58,0x00,0x58,0x00};
static u8 OTM1281_para_2lan0[]={0x00,0xb8};
static u8 OTM1281_para_2lan1[]={0xf5,0x0c,0x12};

static u8 OTM1281_para14[]={0xff,0x00,0x00,0x00};
static u8 OTM1281_para15[]={0x2A,0x00,0x00,0x02,0xCF};
static u8 OTM1281_para16[]={0x2B,0x00,0x00,0x04,0xFF};
static u8 OTM1281_para17[]={0xff,0x00,0x00};
static u8 OTM1281_para18[]={0x36,0xd0};
static u8 OTM1281_para19[]={0x00,0x82};
static u8 OTM1281_para20[]={0xc1,0x09};
static u8 OTM1281_para21[]={0x00,0xB0};
static u8 OTM1281_para22[]={0xB0,0x20};
static struct tegra_dsi_cmd OTM1281_dsi_init_cmd[]= {
	DSI_CMD_LONG(0x39, OTM1281_para1),   
	DSI_CMD_LONG(0x39, OTM1281_para2),   
	DSI_CMD_LONG(0x39, OTM1281_para3),   
//	DSI_CMD_LONG(0x39, OTM1281_para4),   
//	DSI_CMD_LONG(0x39, OTM1281_para5),   
	DSI_CMD_LONG(0x39, OTM1281_para6),   
	DSI_CMD_LONG(0x39, OTM1281_para7),   
	DSI_CMD_LONG(0x39, OTM1281_para8),  
	DSI_CMD_LONG(0x39, OTM1281_para9),   
	DSI_CMD_LONG(0x39, OTM1281_para10),   
	DSI_CMD_LONG(0x39, OTM1281_para11), 
 	DSI_CMD_LONG(0x39, OTM1281_para12),   
	DSI_CMD_LONG(0x39, OTM1281_para13),  
	DSI_CMD_LONG(0x39, OTM1281_para_2lan0), 
	DSI_CMD_LONG(0x39, OTM1281_para_2lan1),    
//	DSI_CMD_LONG(0x39, OTM1281_para10),   
//	DSI_CMD_LONG(0x39, OTM1281_para17),   
 //	DSI_CMD_LONG(0x39, OTM1281_para12),   
//	DSI_CMD_LONG(0x39, OTM1281_para14), 	
	DSI_CMD_LONG(0x39, OTM1281_para18),  
	DSI_CMD_LONG(0x39, OTM1281_para19),   
	DSI_CMD_LONG(0x39, OTM1281_para20),   
	DSI_CMD_LONG(0x39, OTM1281_para21),   
	DSI_CMD_LONG(0x39, OTM1281_para22), 
	DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(120),
#if(1/*DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE*/)
	DSI_CMD_SHORT(0x15, 0x35, 0x00),
#endif
	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(10),

	///DSI_CMD_SHORT(0x05, 0x2c, 0x00),
};

static struct tegra_dsi_cmd OTM1281_dsi_late_resume_cmd[] = {
	DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(120),
#if(DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	DSI_CMD_SHORT(0x15, 0x35, 0x00),
#endif
	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(120),

	DSI_CMD_SHORT(0x05, 0x2c, 0x00),
};
#if 0
static u8 OTM1280_para1[]={0xff,0x12,0x80,0x01};	// Orise mode enable	      
static u8 OTM1280_para2[]={0x00,0x80};		    // Orise mode enable step 2
static u8 OTM1280_para3[]={0xff,0x12,0x80};

static u8 OTM1280_para4[]={0x00,0xa0}; //turn off zigzag
static u8 OTM1280_para5[]={0xb3,0x38,0x38};


//-------------------- panel setting --------------------//
static u8 OTM1280_para6[]={0x00,0x80}; //砞﹚power sequence︽
static u8 OTM1280_para7[]={0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
	
static u8 OTM1280_para8[]={0x00,0x90}; //砞﹚power sequence︽
static u8 OTM1280_para9[]={0xcb,0x00,0xc0,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static u8 OTM1280_para10[]={0x00,0xa0}; //砞﹚power sequence︽
static u8 OTM1280_para11[]={0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static u8 OTM1280_para12[]={0x00,0xb0}; //砞power sequence︽
static u8 OTM1280_para13[]={0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static u8 OTM1280_para14[]={0x00,0xc0}; //砞﹚power sequence︽
static u8 OTM1280_para15[]={0xcb,0x04,0x00,0x0f,0x00,0x00,0x00,0x04,0x04,0x04,0x04,0x04,0x04,0xf4};

static u8 OTM1280_para16[]={0x00,0xd0}; //砞﹚power sequence︽
static u8 OTM1280_para17[]={0xcb,0xf4,0xf4,0x00,0xf4,0x08,0x04,0x04,0x04,0x00,0x00,0x00,0x00,0x00};

static u8 OTM1280_para18[]={0x00,0xe0}; //砞﹚power sequence︽
static u8 OTM1280_para19[]={0xcb,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x00,0x00};

static u8 OTM1280_para20[]={0x00,0xf0}; //砞﹚power sequence︽
static u8 OTM1280_para21[]={0xcb,0x00,0x70,0x01,0x00,0x00};

static u8 OTM1280_para22[]={0x00,0x80}; //砞﹚タ苯mapping癟腹
static u8 OTM1280_para23[]={0xcc,0x41,0x42,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x52,0x55,0x43,0x53,0x65,0x51,0x4D,0x4E,0x4F,0x91,0x8D,0x8E,0x8F,0x40,0x40,0x40,0x40};

static u8 OTM1280_para24[]={0x00,0xa0}; //砞﹚は苯mapping癟腹
static u8 OTM1280_para25[]={0xcc,0x41,0x42,0x47,0x48,0x4C,0x4B,0x4A,0x49,0x52,0x55,0x43,0x53,0x65,0x51,0x4D,0x4E,0x4F,0x91,0x8D,0x8E,0x8F,0x40,0x40,0x40,0x40,0xFF,0xFF,0xFF,0x01};

static u8 OTM1280_para26[]={0x00,0xc0}; //砞﹚タ苯mapping癟腹
static u8 OTM1280_para27[]={0xcc,0x41,0x42,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x52,0x55,0x43,0x53,0x54,0x51,0x4D,0x4E,0x4F,0x91,0x8D,0x8E,0x8F,0x40,0x40,0x40,0x40};

static u8 OTM1280_para28[]={0x00,0xe0}; //砞﹚は苯mapping癟腹
static u8 OTM1280_para29[]={0xcc,0x41,0x42,0x47,0x48,0x4C,0x4B,0x4A,0x49,0x52,0x55,0x43,0x53,0x54,0x51,0x4D,0x4E,0x4F,0x91,0x8D,0x8E,0x8F,0x40,0x40,0x40,0x40,0xFF,0xFF,0xFF,0x01};

static u8 OTM1280_para30[]={0x00,0x90};//砞﹚Oscillator埃繵
static u8 OTM1280_para31[]={0xc1,0x22,0x00,0x00,0x00,0x00};

static u8 OTM1280_para32[]={0x00,0x80}; //砞﹚power sequence & display RTN, FP, BP
static u8 OTM1280_para33[]={0xc0,0x00,0x87,0x00,0x06,0x0a,0x00,0x87,0x06,0x0a,0x00,0x00,0x00};
//static u8 OTM1280_para33[]={0xc0,0x00,0x87,0x00,0x0a,0x0a,0x00,0x87,0x0a,0x0a,0x00,0x00,0x00};

static u8 OTM1280_para34[]={0x00,0x90}; 
static u8 OTM1280_para35[]={0xc0,0x00,0x0a,0x00,0x14,0x00,0x2a};

static u8 OTM1280_para36[]={0x00,0xa0}; //砞﹚ prech & ckh timing (width & nop)
static u8 OTM1280_para37[]={0xc0,0x00,0x03,0x01,0x01,0x01,0x01,0x1a,0x03,0x00,0x02};

static u8 OTM1280_para38[]={0x00,0x80}; //砞﹚STV & DUMMY line
static u8 OTM1280_para39[]={0xc2,0x03,0x02,0x00,0x00,0x00,0x02,0x00,0x22};

static u8 OTM1280_para40[]={0x00,0x90}; //砞﹚CKVA / B width
static u8 OTM1280_para41[]={0xc2,0x03,0x00,0xff,0xff,0x00,0x00,0x00,0x00,0x22};

static u8 OTM1280_para42[]={0x00,0xb0};
static u8 OTM1280_para43[]={0xc2,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static u8 OTM1280_para44[]={0x00,0xa0}; //砞﹚CKV1/2/3/4 non-overlap
static u8 OTM1280_para45[]={0xc2,0xff,0x00,0xff,0x00,0x00,0x0a,0x00,0x0a};

static u8 OTM1280_para46[]={0x00,0xc0};
static u8 OTM1280_para47[]={0xc2,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static u8 OTM1280_para48[]={0x00,0xe0}; //砞﹚prech mode, のprech/CKHdummy line琌toggle
static u8 OTM1280_para49[]={0xc2,0x84,0x00,0x10,0x0d};

static u8 OTM1280_para50[]={0x00,0xb3};//砞﹚interval-scan跋丁
static u8 OTM1280_para51[]={0xc0,0x0f};

static u8 OTM1280_para52[]={0x00,0xa2};//time out turn off 20111107
static u8 OTM1280_para53[]={0xc1,0xff};

static u8 OTM1280_para54[]={0x00,0xb4}; //inversion setting 
static u8 OTM1280_para55[]={0xc0,0x54,0x00}; //bit[7:4] column inversion [suggest]
//Gen_write_2P(0xc0,0x04,0x00); //bit[7:4] dot inversion


static u8 OTM1280_para56[]={0x00,0x80}; //Charge Pumpも笆砞﹚
static u8 OTM1280_para57[]={0xc5,0x20,0x07,0x00,0xb0,0xb0,0x00,0x00,0x00}; 

static u8 OTM1280_para58[]={0x00,0x90}; //Normal mode, pump 瞯, VGH/VGL=10/-5.5V
static u8 OTM1280_para59[]={0xc5,0x30,0x85,0x02,0x88,0x96,0x15,0x00,0x0c};     //20111107

static u8 OTM1280_para60[]={0x00,0x00}; //GVDD
static u8 OTM1280_para61[]={0xd8,0x52,0x00,0x52,0x00};          //20111107

static u8 OTM1280_para62[]={0x00,0x00}; //VCOMDC for flicker adjusting
static u8 OTM1280_para63[]={0xd9,0x8f,0x73,0x80};   //0x8f verify     20111107

static u8 OTM1280_para64[]={0x00,0xc0}; //VBRR enable
static u8 OTM1280_para65[]={0xc0,0x95};

static u8 OTM1280_para66[]={0x00,0xd0}; //VBRR voltage setting
static u8 OTM1280_para67[]={0xc0,0x05};

static u8 OTM1280_para68[]={0x00,0xb6};  // VGLO Off
static u8 OTM1280_para69[]={0xf5,0x00,0x00};

static u8 OTM1280_para70[]={0x00,0xb0}; //sleep in耞sram 
static u8 OTM1280_para71[]={0xb3,0x11};	

static u8 OTM1280_para72[]={0x00,0xb0}; //vcomdc秨丁
static u8 OTM1280_para73[]={0xf5,0x00,0x20};

static u8 OTM1280_para74[]={0x00,0xb8}; //VGHO秨丁
static u8 OTM1280_para75[]={0xf5,0x0c,0x12};

static u8 OTM1280_para76[]={0x00,0x94};  
static u8 OTM1280_para77[]={0xf5,0x0a,0x14,0x06,0x17};

static u8 OTM1280_para78[]={0x00,0xa2};  
static u8 OTM1280_para79[]={0xf5,0x0a,0x14,0x07,0x14};

static u8 OTM1280_para80[]={0x00,0x90}; //VCI1 & VCI2 &VCI3 & VR秨丁秨丁
static u8 OTM1280_para81[]={0xf5,0x07,0x16,0x07,0x14};

static u8 OTM1280_para82[]={0x00,0xa0};// pump1~6秨丁 
static u8 OTM1280_para83[]={0xf5,0x02,0x12,0x0a,0x12,0x07,0x12,0x06,0x12,0x0b,0x12,0x08,0x12}; 

//CMI_LCM_OTM1280A_Gamma_setting 
//Red
static u8 OTM1280_para84[]={0x00,0x00};
static u8 OTM1280_para85[]={0xE1,0x2C,0x2F,0x36,0x3E,0x0B,0x05,0x14,0x09,0x07,0x08,0x09,0x1C,0x05,0x0B,0x11,0x0E,0x0B,0x0B};
static u8 OTM1280_para86[]={0x00,0x00};
static u8 OTM1280_para87[]={0xE2,0x2C,0x2F,0x36,0x3E,0x0B,0x05,0x14,0x09,0x07,0x08,0x09,0x1C,0x05,0x0B,0x11,0x0E,0x0B,0x0B};

//Green
static u8 OTM1280_para88[]={0x00,0x00};
static u8 OTM1280_para89[]={0xE3,0x2C,0x2E,0x35,0x3C,0x0D,0x06,0x16,0x09,0x07,0x08,0x0A,0x1A,0x05,0x0B,0x12,0x0E,0x0B,0x0B};
static u8 OTM1280_para90[]={0x00,0x00};
static u8 OTM1280_para91[]={0xE4,0x2C,0x2E,0x35,0x3C,0x0D,0x06,0x16,0x09,0x07,0x08,0x0A,0x1A,0x05,0x0B,0x12,0x0E,0x0B,0x0B};

//Blue
static u8 OTM1280_para92[]={0x00,0x00};
static u8 OTM1280_para93[]={0xE5,0x0E,0x16,0x23,0x2E,0x0E,0x07,0x1C,0x0A,0x08,0x07,0x09,0x19,0x05,0x0C,0x11,0x0D,0x0B,0x0B};
static u8 OTM1280_para94[]={0x00,0x00};
static u8 OTM1280_para95[]={0xE6,0x0E,0x16,0x23,0x2E,0x0E,0x07,0x1C,0x0A,0x08,0x07,0x09,0x19,0x05,0x0C,0x11,0x0D,0x0B,0x0B};	
static u8 OTM1280_para96[]={0x36,0xd0};
#else
static u8 OTM1280_para1[] = {0xFF, 0x12,0x80,0x01};
static u8 OTM1280_para2[] = {0x00, 0x80};
static u8 OTM1280_para3[] = {0xFF, 0x12,0x80};
static u8 OTM1280_para4[] = {0x00,0xa0};
static u8 OTM1280_para5[] = {0xb3,0x38,0x38};
static u8 OTM1280_para6[] = {0x00, 0xb0}; 
static u8 OTM1280_para7[] = {0xb0, 0x20};
static u8 OTM1280_para8[] = {0x00,0x80};
static u8 OTM1280_para9[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM1280_para10[] = {0x00,0x90};
static u8 OTM1280_para11[] = {0xcb,0x00,0xc0,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM1280_para12[] = {0x00,0xa0};
static u8 OTM1280_para13[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM1280_para14[] = {0x00,0xb0};
static u8 OTM1280_para15[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM1280_para16[] = {0x00,0xc0};
static u8 OTM1280_para17[] = {0xcb,0x04,0x00,0x0f,0x00,0x00,0x00,0x04,0x04,0x04,0x04,0x04,0x04,0xf4};
static u8 OTM1280_para18[] = {0x00,0xd0};
static u8 OTM1280_para19[] = {0xcb,0xf4,0xf4,0x00,0xf4,0x08,0x04,0x04,0x04,0x00,0x00,0x00,0x00,0x00};
static u8 OTM1280_para20[] = {0x00,0xe0};
static u8 OTM1280_para21[] = {0xcb,0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x00,0x00};
static u8 OTM1280_para22[] = {0x00,0xf0};
static u8 OTM1280_para23[] = {0xcb,0x00,0x70,0x01,0x00,0x00};
static u8 OTM1280_para24[] = {0x00,0x80};
static u8 OTM1280_para25[] = {0xcc,0x41,0x42,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x52,0x55,0x43,0x53,0x65,0x51,0x4D,0x4E,0x4F,0x91,0x8D,0x8E,0x8F,0x40,0x40,0x40,0x40};
static u8 OTM1280_para26[] = {0x00,0xA0};
static u8 OTM1280_para27[] = {0xcc,0x41,0x42,0x47,0x48,0x4C,0x4B,0x4A,0x49,0x52,0x55,0x43,0x53,0x65,0x51,0x4D,0x4E,0x4F,0x91,0x8D,0x8E,0x8F,0x40,0x40,0x40,0x40,0xFF,0xFF,0xFF,0x01};
static u8 OTM1280_para28[] = {0x00,0xC0};
static u8 OTM1280_para29[] = {0xcc,0x41,0x42,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x52,0x55,0x43,0x53,0x54,0x51,0x4D,0x4E,0x4F,0x91,0x8D,0x8E,0x8F,0x40,0x40,0x40,0x40};
static u8 OTM1280_para30[] = {0x00,0xE0};
static u8 OTM1280_para31[] = {0xcc,0x41,0x42,0x47,0x48,0x4C,0x4B,0x4A,0x49,0x52,0x55,0x43,0x53,0x54,0x51,0x4D,0x4E,0x4F,0x91,0x8D,0x8E,0x8F,0x40,0x40,0x40,0x40,0xFF,0xFF,0xFF,0x01};
static u8 OTM1280_para32[] = {0x00,0x90};
static u8 OTM1280_para33[] = {0xc1,0x22,0x00,0x00,0x00,0x00};
static u8 OTM1280_para34[] = {0x00,0x80};
static u8 OTM1280_para35[] = {0xc0,0x00,0x87,0x00,0x06,0x0a,0x00,0x87,0x06,0x0a,0x00,0x00,0x00};
static u8 OTM1280_para36[] = {0x00,0x90};
static u8 OTM1280_para37[] = {0xc0,0x00,0x0a,0x00,0x14,0x00,0x2a};
static u8 OTM1280_para38[] = {0x00,0xA0};
static u8 OTM1280_para39[] = {0xc0,0x00,0x03,0x01,0x01,0x01,0x01,0x1a,0x03,0x00,0x02};
static u8 OTM1280_para40[] = {0x00,0x80};
static u8 OTM1280_para41[] = {0xc2,0x03,0x02,0x00,0x00,0x00,0x02,0x00,0x22};
static u8 OTM1280_para42[] = {0x00,0x90};
static u8 OTM1280_para43[] = {0xc2,0x03,0x00,0xff,0xff,0x00,0x00,0x00,0x00,0x22};
static u8 OTM1280_para44[] = {0x00,0xA0};
static u8 OTM1280_para45[] = {0xc2,0xff,0x00,0xff,0x00,0x00,0x0a,0x00,0x0a};
static u8 OTM1280_para46[] = {0x00,0xB0};
static u8 OTM1280_para47[] = {0xc2,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM1280_para48[] = {0x00,0xC0};
static u8 OTM1280_para49[] = {0xc2,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM1280_para50[] = {0x00,0xE0};
static u8 OTM1280_para51[] = {0xc2,0x84,0x00,0x10,0x0d};
static u8 OTM1280_para52[] = {0x00,0xB3};
static u8 OTM1280_para53[] = {0xc0,0x0f};
static u8 OTM1280_para54[] = {0x00,0xA2};
static u8 OTM1280_para55[] = {0xc1,0xff};
static u8 OTM1280_para56[] = {0x00,0xB4};
static u8 OTM1280_para57[] = {0xc0,0x54,0x00};
static u8 OTM1280_para58[] = {0x00,0x80};
static u8 OTM1280_para59[] = {0xc5,0x20,0x07,0x00,0xb0,0xb0,0x00,0x6c,0x00};
static u8 OTM1280_para60[] = {0x00,0x90};
static u8 OTM1280_para61[] = {0xc5,0x30,0x85,0x02,0x88,0x96,0x15,0x00,0x0c,0x44,0x44,0x44};
static u8 OTM1280_para62[] = {0x00,0x00};
static u8 OTM1280_para63[] = {0xd8,0x52,0x00,0x52,0x00};
static u8 OTM1280_para64[] = {0x00,0x00};
static u8 OTM1280_para65[] = {0xd9,0x8f,0x73,0x80};
static u8 OTM1280_para66[] = {0x00,0xC0};
static u8 OTM1280_para67[] = {0xc0,0x95};
static u8 OTM1280_para68[] = {0x00,0xD0};
static u8 OTM1280_para69[] = {0xc0,0x05};
static u8 OTM1280_para70[] = {0x00,0xB6};
static u8 OTM1280_para71[] = {0xf5,0x00,0x00};
static u8 OTM1280_para72[] = {0x00,0xB0};
static u8 OTM1280_para73[] = {0xb3,0x11};
static u8 OTM1280_para74[] = {0x00,0xB0};
static u8 OTM1280_para75[] = {0xf5,0x00,0x20};
static u8 OTM1280_para76[] = {0x00,0xB8};
static u8 OTM1280_para77[] = {0xf5,0x0c,0x12};
static u8 OTM1280_para78[] = {0x00,0x94};
static u8 OTM1280_para79[] = {0xf5,0x0a,0x14,0x06,0x17};
static u8 OTM1280_para80[] = {0x00,0xA2};
static u8 OTM1280_para81[] = {0xf5,0x0a,0x14,0x07,0x14};
static u8 OTM1280_para82[] = {0x00,0x90};
static u8 OTM1280_para83[] = {0xf5,0x07,0x16,0x07,0x14};
static u8 OTM1280_para84[] = {0x00,0xA0};
static u8 OTM1280_para85[] = {0xf5,0x02,0x12,0x0a,0x12,0x07,0x12,0x06,0x12,0x0b,0x12,0x08,0x12};
static u8 OTM1280_para86[] = {0x36,0xd0};
static u8 OTM1280_para87[] = {0x00,0x80};
static u8 OTM1280_para88[] = {0xd6,0x00};
static u8 OTM1280_para89[] = {0x00,0x00};
static u8 OTM1280_para90[] = {0xE1,0x2C,0x2F,0x36,0x3E,0x0B,0x05,0x14,0x09,0x07,0x08,0x09,0x1C,0x05,0x0B,0x11,0x0E,0x0B,0x0B};
static u8 OTM1280_para91[] = {0x00,0x00};
static u8 OTM1280_para92[] = {0xE2,0x2C,0x2F,0x36,0x3E,0x0B,0x05,0x14,0x09,0x07,0x08,0x09,0x1C,0x05,0x0B,0x11,0x0E,0x0B,0x0B};
static u8 OTM1280_para93[] = {0x00,0x00};
static u8 OTM1280_para94[] = {0xE3,0x2C,0x2E,0x35,0x3C,0x0D,0x06,0x16,0x09,0x07,0x08,0x0A,0x1A,0x05,0x0B,0x12,0x0E,0x0B,0x0B};
static u8 OTM1280_para95[] = {0x00,0x00};
static u8 OTM1280_para96[] = {0xE4,0x2C,0x2E,0x35,0x3C,0x0D,0x06,0x16,0x09,0x07,0x08,0x0A,0x1A,0x05,0x0B,0x12,0x0E,0x0B,0x0B};
static u8 OTM1280_para97[] = {0x00,0x00};
static u8 OTM1280_para98[] = {0xE5,0x0E,0x16,0x23,0x2E,0x0E,0x07,0x1C,0x0A,0x08,0x07,0x09,0x19,0x05,0x0C,0x11,0x0D,0x0B,0x0B};
static u8 OTM1280_para99[] = {0x00,0x00};
static u8 OTM1280_para100[] = {0xE6,0x0E,0x16,0x23,0x2E,0x0E,0x07,0x1C,0x0A,0x08,0x07,0x09,0x19,0x05,0x0C,0x11,0x0D,0x0B,0x0B};
static u8 OTM1280_para101[]={0x00, 0x00};
static u8 OTM1280_para102[]={0x2A, 0x00, 0x00, 0x02, 0xcf};
static u8 OTM1280_para103[]={0x00, 0x00};
static u8 OTM1280_para104[]={0x2b, 0x00, 0x00, 0x04, 0xff};
static u8 OTM1280_para105[]={0x00,0x82};
static u8 OTM1280_para106[]={0xc1,0x05};
#endif
static struct tegra_dsi_cmd OTM1280_dsi_init_cmd[]= {
	DSI_CMD_LONG(0x39, OTM1280_para1),   
	DSI_CMD_LONG(0x39, OTM1280_para2),   
	DSI_CMD_LONG(0x39, OTM1280_para3),   
	DSI_CMD_LONG(0x39, OTM1280_para4),   
	DSI_CMD_LONG(0x39, OTM1280_para5),   
	DSI_CMD_LONG(0x39, OTM1280_para6),   
	DSI_CMD_LONG(0x39, OTM1280_para7),   
	DSI_CMD_LONG(0x39, OTM1280_para8),  
	DSI_CMD_LONG(0x39, OTM1280_para9),   
	DSI_CMD_LONG(0x39, OTM1280_para10),   
	DSI_CMD_LONG(0x39, OTM1280_para11), 
 	DSI_CMD_LONG(0x39, OTM1280_para12),   
	DSI_CMD_LONG(0x39, OTM1280_para13),  
	DSI_CMD_LONG(0x39, OTM1280_para14),   
	DSI_CMD_LONG(0x39, OTM1280_para15),   
	DSI_CMD_LONG(0x39, OTM1280_para16),   
	DSI_CMD_LONG(0x39, OTM1280_para17),   
	DSI_CMD_LONG(0x39, OTM1280_para18),  
	DSI_CMD_LONG(0x39, OTM1280_para19),   
	DSI_CMD_LONG(0x39, OTM1280_para20),   
	DSI_CMD_LONG(0x39, OTM1280_para21), 
 	DSI_CMD_LONG(0x39, OTM1280_para22),   
	DSI_CMD_LONG(0x39, OTM1280_para23),  
	DSI_CMD_LONG(0x39, OTM1280_para24),    
	DSI_CMD_LONG(0x39, OTM1280_para25),   
	DSI_CMD_LONG(0x39, OTM1280_para26),   
	DSI_CMD_LONG(0x39, OTM1280_para27),   
	DSI_CMD_LONG(0x39, OTM1280_para28),  
	DSI_CMD_LONG(0x39, OTM1280_para29),   
	DSI_CMD_LONG(0x39, OTM1280_para30),   
	DSI_CMD_LONG(0x39, OTM1280_para31), 
 	DSI_CMD_LONG(0x39, OTM1280_para32),   
	DSI_CMD_LONG(0x39, OTM1280_para33),  
	DSI_CMD_LONG(0x39, OTM1280_para34),  
	DSI_CMD_LONG(0x39, OTM1280_para35),   
	DSI_CMD_LONG(0x39, OTM1280_para36),   
	DSI_CMD_LONG(0x39, OTM1280_para37),   
	DSI_CMD_LONG(0x39, OTM1280_para38),  
	DSI_CMD_LONG(0x39, OTM1280_para39),   
	DSI_CMD_LONG(0x39, OTM1280_para40),   
	DSI_CMD_LONG(0x39, OTM1280_para41), 
 	DSI_CMD_LONG(0x39, OTM1280_para42),   
	DSI_CMD_LONG(0x39, OTM1280_para43),  
	DSI_CMD_LONG(0x39, OTM1280_para44),  
	DSI_CMD_LONG(0x39, OTM1280_para45),   
	DSI_CMD_LONG(0x39, OTM1280_para46),   
	DSI_CMD_LONG(0x39, OTM1280_para47),   
	DSI_CMD_LONG(0x39, OTM1280_para48),  
	DSI_CMD_LONG(0x39, OTM1280_para49),   
	DSI_CMD_LONG(0x39, OTM1280_para50),   
	DSI_CMD_LONG(0x39, OTM1280_para51), 
 	DSI_CMD_LONG(0x39, OTM1280_para52),   
	DSI_CMD_LONG(0x39, OTM1280_para53),  
	DSI_CMD_LONG(0x39, OTM1280_para54),  
	DSI_CMD_LONG(0x39, OTM1280_para55),   
	DSI_CMD_LONG(0x39, OTM1280_para56),   
	DSI_CMD_LONG(0x39, OTM1280_para57),   
	DSI_CMD_LONG(0x39, OTM1280_para58),  
	DSI_CMD_LONG(0x39, OTM1280_para59),   
	DSI_CMD_LONG(0x39, OTM1280_para60),   
	DSI_CMD_LONG(0x39, OTM1280_para61), 
 	DSI_CMD_LONG(0x39, OTM1280_para62),   
	DSI_CMD_LONG(0x39, OTM1280_para63),  
	DSI_CMD_LONG(0x39, OTM1280_para64),  
	DSI_CMD_LONG(0x39, OTM1280_para65),   
	DSI_CMD_LONG(0x39, OTM1280_para66),   
	DSI_CMD_LONG(0x39, OTM1280_para67),   
	DSI_CMD_LONG(0x39, OTM1280_para68),  
	DSI_CMD_LONG(0x39, OTM1280_para69),   
	DSI_CMD_LONG(0x39, OTM1280_para70),   
	DSI_CMD_LONG(0x39, OTM1280_para71), 
 	DSI_CMD_LONG(0x39, OTM1280_para72),   
	DSI_CMD_LONG(0x39, OTM1280_para73),  
	DSI_CMD_LONG(0x39, OTM1280_para74),  
	DSI_CMD_LONG(0x39, OTM1280_para75),   
	DSI_CMD_LONG(0x39, OTM1280_para76),   
	DSI_CMD_LONG(0x39, OTM1280_para77),   
	DSI_CMD_LONG(0x39, OTM1280_para78),  
	DSI_CMD_LONG(0x39, OTM1280_para79),   
	DSI_CMD_LONG(0x39, OTM1280_para80),   
	DSI_CMD_LONG(0x39, OTM1280_para81), 
 	DSI_CMD_LONG(0x39, OTM1280_para82),   
	DSI_CMD_LONG(0x39, OTM1280_para83),  
	DSI_CMD_LONG(0x39, OTM1280_para84),  
	DSI_CMD_LONG(0x39, OTM1280_para85),   
	DSI_CMD_LONG(0x39, OTM1280_para86),   
	DSI_CMD_LONG(0x39, OTM1280_para87),   
	DSI_CMD_LONG(0x39, OTM1280_para88),  
	DSI_CMD_LONG(0x39, OTM1280_para89),   
	DSI_CMD_LONG(0x39, OTM1280_para90),   
	DSI_CMD_LONG(0x39, OTM1280_para91), 
 	DSI_CMD_LONG(0x39, OTM1280_para92),   
	DSI_CMD_LONG(0x39, OTM1280_para93),  
	DSI_CMD_LONG(0x39, OTM1280_para94),  
	DSI_CMD_LONG(0x39, OTM1280_para95),  
	DSI_CMD_LONG(0x39, OTM1280_para96),  
	DSI_CMD_LONG(0x39, OTM1280_para97),   
	DSI_CMD_LONG(0x39, OTM1280_para98),  
	DSI_CMD_LONG(0x39, OTM1280_para99),   
	DSI_CMD_LONG(0x39, OTM1280_para100),   
	DSI_CMD_LONG(0x39, OTM1280_para101), 
 	DSI_CMD_LONG(0x39, OTM1280_para102),   
	DSI_CMD_LONG(0x39, OTM1280_para103),  
	DSI_CMD_LONG(0x39, OTM1280_para104),  
	DSI_CMD_LONG(0x39, OTM1280_para105),  
	DSI_CMD_LONG(0x39, OTM1280_para106),  
	DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(100),
#if(1/*DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE*/)
	DSI_CMD_SHORT(0x15, 0x35, 0x00),
#endif
//36 ,ml
	//DSI_CMD_SHORT(0x05, 0x13, 0x00),
	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(15),
	DSI_DLY_MS(15),
	DSI_CMD_SHORT(0x05, 0x2c, 0x00),
};

static u8 OTM8018_para1[] = {0xff,0x80,0x09,0x01};
static u8 OTM8018_para2[] = {0x00, 0x80};
static u8 OTM8018_para3[] = {0xFF,0x80,0x09};
static u8 OTM8018_para4[] = {0x00,0x03};
static u8 OTM8018_para5[] = {0xFF,0x01};
static u8 OTM8018_para6[] = {0x00,0xB4}; 
static u8 OTM8018_para7[] = {0xC0,0x10};
static u8 OTM8018_para8[] = {0x00,0x82};
static u8 OTM8018_para9[] = {0xC5,0xA3};
static u8 OTM8018_para10[] = {0x00,0x90};
static u8 OTM8018_para11[] = {0xC5,0x96,0x76};
static u8 OTM8018_para12[] = {0x00,0x00};
static u8 OTM8018_para13[] = {0xD8,0x75,0x73};
static u8 OTM8018_para14[] = {0x00,0x00};
static u8 OTM8018_para15[] = {0xD9,0x5E};
static u8 OTM8018_para16[] = {0x00,0x81};
static u8 OTM8018_para17[] = {0xC1,0x66};
static u8 OTM8018_para18[] = {0x00,0xA1};
static u8 OTM8018_para19[] = {0xC1,0x08};
static u8 OTM8018_para20[] = {0x00,0x89};
static u8 OTM8018_para21[] = {0xC4,0x08};
static u8 OTM8018_para22[] = {0x00,0xA2};
static u8 OTM8018_para23[] = {0xC0,0x1B,0x00,0x02};
static u8 OTM8018_para24[] = {0x00,0x81};
static u8 OTM8018_para25[] = {0xC4,0x83};
static u8 OTM8018_para26[] = {0x00,0x92};
static u8 OTM8018_para27[] = {0xC5,0x01};
static u8 OTM8018_para28[] = {0x00,0xB1};
static u8 OTM8018_para29[] = {0xC5,0xA9};
static u8 OTM8018_para30[] = {0x00,0x90};
static u8 OTM8018_para31[] = {0xC0,0x00,0x44,0x00,0x00,0x00,0x03};
static u8 OTM8018_para32[] = {0x00,0xA6};
static u8 OTM8018_para33[] = {0xC1,0x00,0x00,0x00};
static u8 OTM8018_para34[] = {0x00,0x80};
static u8 OTM8018_para35[] = {0xCE,0x87,0x03,0x00,0x85,0x03,0x00,0x86,0x03,0x00,0x84,0x03,0x00};
static u8 OTM8018_para36[] = {0x00,0xA0};
static u8 OTM8018_para37[] = {0xCE,0x38,0x03,0x03,0x58,0x00,0x00,0x00,0x38,0x02,0x03,0x59,0x00,0x00,0x00};
static u8 OTM8018_para38[] = {0x00,0xB0};
static u8 OTM8018_para39[] = {0xCE,0x38,0x01,0x03,0x5A,0x00,0x00,0x00,0x38,0x00,0x03,0x5B,0x00,0x00,0x00};
static u8 OTM8018_para40[] = {0x00,0xC0};
static u8 OTM8018_para41[] = {0xCE,0x30,0x00,0x03,0x5C,0x00,0x00,0x00,0x30,0x01,0x03,0x5D,0x00,0x00,0x00};
static u8 OTM8018_para42[] = {0x00,0xD0};
static u8 OTM8018_para43[] = {0xCE,0x30,0x02,0x03,0x5E,0x00,0x00,0x00,0x30,0x03,0x03,0x5F,0x00,0x00,0x00};
static u8 OTM8018_para44[] = {0x00,0xC7};
static u8 OTM8018_para45[] = {0xCF,0x00};
static u8 OTM8018_para46[] = {0x00,0xC9};
static u8 OTM8018_para47[] = {0xCF,0x00};
static u8 OTM8018_para48[] = {0x00,0xC4};
static u8 OTM8018_para49[] = {0xCB,0x04,0x04,0x04,0x04,0x04,0x04};
static u8 OTM8018_para50[] = {0x00,0xD9};
static u8 OTM8018_para51[] = {0xCB,0x04,0x04,0x04,0x04,0x04,0x04};
static u8 OTM8018_para52[] = {0x00,0x84};
static u8 OTM8018_para53[] = {0xCC,0x0C,0x0A,0x10,0x0E,0x03,0x04};
static u8 OTM8018_para54[] = {0x00,0x9E};
static u8 OTM8018_para55[] = {0xCC,0x0B};
static u8 OTM8018_para56[] = {0x00,0xA0};
static u8 OTM8018_para57[] = {0xCC,0x09,0x0F,0x0D,0x01,0x02};
static u8 OTM8018_para58[] = {0x00,0xB4};
static u8 OTM8018_para59[] = {0xCC,0x0D,0x0F,0x09,0x0B,0x02,0x01};
static u8 OTM8018_para60[] = {0x00,0xCE};
static u8 OTM8018_para61[] = {0xCC,0x0E};
static u8 OTM8018_para62[] = {0x00,0xD0};
static u8 OTM8018_para63[] = {0xCC,0x10,0x0A,0x0C,0x04,0x03};
static u8 OTM8018_para64[] = {0x00,0x00};
static u8 OTM8018_para65[] = {0x3A,0x77};
static struct tegra_dsi_cmd OTM8018_dsi_init_cmd[]= {
	DSI_CMD_LONG(0x39, OTM8018_para1),   
	DSI_CMD_LONG(0x39, OTM8018_para2),   
	DSI_CMD_LONG(0x39, OTM8018_para3),   
	DSI_CMD_LONG(0x39, OTM8018_para4),   
	DSI_CMD_LONG(0x39, OTM8018_para5),   
	DSI_CMD_LONG(0x39, OTM8018_para6),   
	DSI_CMD_LONG(0x39, OTM8018_para7),   
	DSI_CMD_LONG(0x39, OTM8018_para8),  
	DSI_CMD_LONG(0x39, OTM8018_para9),   
	DSI_CMD_LONG(0x39, OTM8018_para10),   
	DSI_CMD_LONG(0x39, OTM8018_para11), 
 	DSI_CMD_LONG(0x39, OTM8018_para12),   
	DSI_CMD_LONG(0x39, OTM8018_para13),  
	DSI_CMD_LONG(0x39, OTM8018_para14),   
	DSI_CMD_LONG(0x39, OTM8018_para15),   
	DSI_CMD_LONG(0x39, OTM8018_para16),   
	DSI_CMD_LONG(0x39, OTM8018_para17),   
	DSI_CMD_LONG(0x39, OTM8018_para18),  
	DSI_CMD_LONG(0x39, OTM8018_para19),   
	DSI_CMD_LONG(0x39, OTM8018_para20),   
	DSI_CMD_LONG(0x39, OTM8018_para21), 
 	DSI_CMD_LONG(0x39, OTM8018_para22),   
	DSI_CMD_LONG(0x39, OTM8018_para23),  
	DSI_CMD_LONG(0x39, OTM8018_para24),    
	DSI_CMD_LONG(0x39, OTM8018_para25),   
	DSI_CMD_LONG(0x39, OTM8018_para26),   
	DSI_CMD_LONG(0x39, OTM8018_para27),   
	DSI_CMD_LONG(0x39, OTM8018_para28),  
	DSI_CMD_LONG(0x39, OTM8018_para29),   
	DSI_CMD_LONG(0x39, OTM8018_para30),   
	DSI_CMD_LONG(0x39, OTM8018_para31), 
 	DSI_CMD_LONG(0x39, OTM8018_para32),   
	DSI_CMD_LONG(0x39, OTM8018_para33),  
	DSI_CMD_LONG(0x39, OTM8018_para34),  
	DSI_CMD_LONG(0x39, OTM8018_para35),   
	DSI_CMD_LONG(0x39, OTM8018_para36),   
	DSI_CMD_LONG(0x39, OTM8018_para37),   
	DSI_CMD_LONG(0x39, OTM8018_para38),  
	DSI_CMD_LONG(0x39, OTM8018_para39),   
	DSI_CMD_LONG(0x39, OTM8018_para40),   
	DSI_CMD_LONG(0x39, OTM8018_para41), 
 	DSI_CMD_LONG(0x39, OTM8018_para42),   
	DSI_CMD_LONG(0x39, OTM8018_para43),  
	DSI_CMD_LONG(0x39, OTM8018_para44),  
	DSI_CMD_LONG(0x39, OTM8018_para45),   
	DSI_CMD_LONG(0x39, OTM8018_para46),   
	DSI_CMD_LONG(0x39, OTM8018_para47),   
	DSI_CMD_LONG(0x39, OTM8018_para48),  
	DSI_CMD_LONG(0x39, OTM8018_para49),   
	DSI_CMD_LONG(0x39, OTM8018_para50),   
	DSI_CMD_LONG(0x39, OTM8018_para51), 
 	DSI_CMD_LONG(0x39, OTM8018_para52),   
	DSI_CMD_LONG(0x39, OTM8018_para53),  
	DSI_CMD_LONG(0x39, OTM8018_para54),  
	DSI_CMD_LONG(0x39, OTM8018_para55),   
	DSI_CMD_LONG(0x39, OTM8018_para56),   
	DSI_CMD_LONG(0x39, OTM8018_para57),   
	DSI_CMD_LONG(0x39, OTM8018_para58),  
	DSI_CMD_LONG(0x39, OTM8018_para59),   
	DSI_CMD_LONG(0x39, OTM8018_para60),   
	DSI_CMD_LONG(0x39, OTM8018_para61), 
 	DSI_CMD_LONG(0x39, OTM8018_para62),   
	DSI_CMD_LONG(0x39, OTM8018_para63),  
	DSI_CMD_LONG(0x39, OTM8018_para64),  
	DSI_CMD_LONG(0x39, OTM8018_para65),   


	DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(100),
#if(0/*DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE*/)
	DSI_CMD_SHORT(0x15, 0x35, 0x00),
#endif

	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_CMD_SHORT(0x05, 0x2c, 0x00),
};
#if 0
static u8 OTM8009_para1[] = {0xFF,0x80,0x09,0x01};
static u8 OTM8009_para2[] = {0x00, 0x80};
static u8 OTM8009_para3[] = {0xFF,0x80,0x09};
static u8 OTM8009_para4[] = {0x00,0xB4};
static u8 OTM8009_para5[] = {0xC0,0x50};
static u8 OTM8009_para6[] = {0x00,0x92}; 
static u8 OTM8009_para7[] = {0xC5,0x01};
static u8 OTM8009_para8[] = {0x00,0x90};
static u8 OTM8009_para9[] = {0xC5,0x96};
static u8 OTM8009_para10[] = {0x00,0x95};
static u8 OTM8009_para11[] = {0xC5,0x34};
static u8 OTM8009_para12[] = {0x00,0x94};
static u8 OTM8009_para13[] = {0xC5,0x33};
static u8 OTM8009_para14[] = {0x00,0xA3};
static u8 OTM8009_para15[] = {0xC0,0x1B};
static u8 OTM8009_para16[] = {0x00,0x82};
static u8 OTM8009_para17[] = {0xC5,0x83};
static u8 OTM8009_para18[] = {0x00,0x81};
static u8 OTM8009_para19[] = {0xC4,0x83};
static u8 OTM8009_para20[] = {0x00,0xB1};
static u8 OTM8009_para21[] = {0xC5,0xA9};
static u8 OTM8009_para22[] = {0x00,0x91};
static u8 OTM8009_para23[] = {0xC5,0x2C};//modify
static u8 OTM8009_para24[] = {0x00,0x00};
static u8 OTM8009_para25[] = {0xD8,0x79};
static u8 OTM8009_para26[] = {0x00,0x01};
static u8 OTM8009_para27[] = {0xD8,0x79};
static u8 OTM8009_para28[] = {0x00,0x81};
static u8 OTM8009_para29[] = {0xC1,0x55};
static u8 OTM8009_para30[] = {0x00,0xA1};
static u8 OTM8009_para31[] = {0xC1,0x0E};
static u8 OTM8009_para32[] = {0x00,0x00};
static u8 OTM8009_para33[] = {0xD9,0x3E};
static u8 OTM8009_para34[] = {0xEC,0x40,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x42,0x06};
static u8 OTM8009_para35[] = {0xED,0x40,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x03};
static u8 OTM8009_para36[] = {0xEE,0x40,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x22,0x08};
static u8 OTM8009_para37[] = {0x00,0x80};
static u8 OTM8009_para38[] = {0xCE,0x86,0x01,0x00,0x85,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM8009_para39[] = {0x00,0xA0};
static u8 OTM8009_para40[] = {0xCE,0x18,0x04,0x03,0x21,0x00,0x00,0x00,0x18,0x03,0x03,0x22,0x00,0x00,0x00};
static u8 OTM8009_para41[] = {0x00,0xB0};
static u8 OTM8009_para42[] = {0xCE,0x18,0x02,0x03,0x23,0x00,0x00,0x00,0x18,0x01,0x03,0x24,0x00,0x00,0x00};
static u8 OTM8009_para43[] = {0x00,0xC0};
static u8 OTM8009_para44[] = {0xCF,0x01,0x01,0x20,0x20,0x00,0x00,0x01,0x00,0x00,0x00};
static u8 OTM8009_para45[] = {0x00,0xD0};
static u8 OTM8009_para46[] = {0xCF,0xD0};
static u8 OTM8009_para47[] = {0x00,0xC0};
static u8 OTM8009_para48[] = {0xCB,0x00,0x04,0x04,0x04,0x04,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM8009_para49[] = {0x00,0xD0};
static u8 OTM8009_para50[] = {0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x04,0x04,0x04,0x04,0x00,0x00,0x00,0x00};
static u8 OTM8009_para51[] = {0x00,0xE0};
static u8 OTM8009_para52[] = {0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM8009_para53[] = {0x00,0xF0};
static u8 OTM8009_para54[] = {0xCB,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static u8 OTM8009_para55[] = {0x00,0x80};
static u8 OTM8009_para56[] = {0xCC,0x00,0x26,0x09,0x0b,0x01,0x25,0x00,0x00,0x00,0x00};
static u8 OTM8009_para57[] = {0x00,0x90};
static u8 OTM8009_para58[] = {0xCC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x26,0x0a,0x0c,0x02};
static u8 OTM8009_para59[] = {0x00,0xA0};
static u8 OTM8009_para60[] = {0xCC,0x25,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM8009_para61[] = {0x00,0xB0};
static u8 OTM8009_para62[] = {0xCC,0x00,0x25,0x0c,0x0a,0x02,0x26,0x00,0x00,0x00,0x00};
static u8 OTM8009_para63[] = {0x00,0xC0};
static u8 OTM8009_para64[] = {0xCC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x25,0x0b,0x09,0x01};
static u8 OTM8009_para65[] = {0x00,0xD0};
static u8 OTM8009_para66[] = {0xCC,0x26,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static struct tegra_dsi_cmd OTM8009_dsi_init_cmd[]= {
	DSI_CMD_LONG(0x39, OTM8009_para1),   
	DSI_CMD_LONG(0x39, OTM8009_para2),   
	DSI_CMD_LONG(0x39, OTM8009_para3),   
	DSI_CMD_LONG(0x39, OTM8009_para4),   
	DSI_CMD_LONG(0x39, OTM8009_para5),   
	DSI_CMD_LONG(0x39, OTM8009_para6),   
	DSI_CMD_LONG(0x39, OTM8009_para7),   
	DSI_CMD_LONG(0x39, OTM8009_para8),  
	DSI_CMD_LONG(0x39, OTM8009_para9),   
	DSI_CMD_LONG(0x39, OTM8009_para10),   
	DSI_CMD_LONG(0x39, OTM8009_para11), 
 	DSI_CMD_LONG(0x39, OTM8009_para12),   
	DSI_CMD_LONG(0x39, OTM8009_para13),  
	DSI_CMD_LONG(0x39, OTM8009_para14),   
	DSI_CMD_LONG(0x39, OTM8009_para15),   
	DSI_CMD_LONG(0x39, OTM8009_para16),   
	DSI_CMD_LONG(0x39, OTM8009_para17),   
	DSI_CMD_LONG(0x39, OTM8009_para18),  
	DSI_CMD_LONG(0x39, OTM8009_para19),   
	DSI_CMD_LONG(0x39, OTM8009_para20),   
	DSI_CMD_LONG(0x39, OTM8009_para21), 
 	DSI_CMD_LONG(0x39, OTM8009_para22),   
	DSI_CMD_LONG(0x39, OTM8009_para23),  
	DSI_CMD_LONG(0x39, OTM8009_para24),    
	DSI_CMD_LONG(0x39, OTM8009_para25),   
	DSI_CMD_LONG(0x39, OTM8009_para26),   
	DSI_CMD_LONG(0x39, OTM8009_para27),   
	DSI_CMD_LONG(0x39, OTM8009_para28),  
	DSI_CMD_LONG(0x39, OTM8009_para29),   
	DSI_CMD_LONG(0x39, OTM8009_para30),   
	DSI_CMD_LONG(0x39, OTM8009_para31), 
 	DSI_CMD_LONG(0x39, OTM8009_para32),   
	DSI_CMD_LONG(0x39, OTM8009_para33),  
	DSI_CMD_LONG(0x39, OTM8009_para34),  
	DSI_CMD_LONG(0x39, OTM8009_para35),   
	DSI_CMD_LONG(0x39, OTM8009_para36),   
	DSI_CMD_LONG(0x39, OTM8009_para37),   
	DSI_CMD_LONG(0x39, OTM8009_para38),  
	DSI_CMD_LONG(0x39, OTM8009_para39),   
	DSI_CMD_LONG(0x39, OTM8009_para40),   
	DSI_CMD_LONG(0x39, OTM8009_para41), 
 	DSI_CMD_LONG(0x39, OTM8009_para42),   
	DSI_CMD_LONG(0x39, OTM8009_para43),  
	DSI_CMD_LONG(0x39, OTM8009_para44),  
	DSI_CMD_LONG(0x39, OTM8009_para45),   
	DSI_CMD_LONG(0x39, OTM8009_para46),   
	DSI_CMD_LONG(0x39, OTM8009_para47),   
	DSI_CMD_LONG(0x39, OTM8009_para48),  
	DSI_CMD_LONG(0x39, OTM8009_para49),   
	DSI_CMD_LONG(0x39, OTM8009_para50),   
	DSI_CMD_LONG(0x39, OTM8009_para51), 
 	DSI_CMD_LONG(0x39, OTM8009_para52),   
	DSI_CMD_LONG(0x39, OTM8009_para53),  
	DSI_CMD_LONG(0x39, OTM8009_para54),  
	DSI_CMD_LONG(0x39, OTM8009_para55),   
	DSI_CMD_LONG(0x39, OTM8009_para56),   
	DSI_CMD_LONG(0x39, OTM8009_para57),   
	DSI_CMD_LONG(0x39, OTM8009_para58),  
	DSI_CMD_LONG(0x39, OTM8009_para59),   
	DSI_CMD_LONG(0x39, OTM8009_para60),   
	DSI_CMD_LONG(0x39, OTM8009_para61), 
 	DSI_CMD_LONG(0x39, OTM8009_para62),   
	DSI_CMD_LONG(0x39, OTM8009_para63),  
	DSI_CMD_LONG(0x39, OTM8009_para64),  
	DSI_CMD_LONG(0x39, OTM8009_para65),   
	DSI_CMD_LONG(0x39, OTM8009_para66),   


	DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(100),
#if(1/*DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE*/)
	DSI_CMD_SHORT(0x15, 0x35, 0x00),
#endif

	DSI_CMD_SHORT(0x05, 0x29, 0x00),

};
#else
static u8 OTM8009_para1[] = {0x00,0x00};
static u8 OTM8009_para2[] = {0xff,0x80,0x09,0x01};
static u8 OTM8009_para3[] = {0x00,0x80};
static u8 OTM8009_para4[] = {0xff,0x80,0x09};
static u8 OTM8009_para5[] = {0x00,0x03};
static u8 OTM8009_para6[] = {0xff,0x01}; 
static u8 OTM8009_para7[] = {0x00,0xb1};
static u8 OTM8009_para8[] = {0xc5,0xa9};
static u8 OTM8009_para9[] = {0x00,0x91};
static u8 OTM8009_para10[] = {0xc5,0x2c}; //{0xc5,0x2c}; 
static u8 OTM8009_para11[] = {0x00,0x00};
static u8 OTM8009_para12[] = {0xd8,0x79,0x79};
static u8 OTM8009_para13[] = {0x00,0xb4};
static u8 OTM8009_para14[] = {0xc0,0x50};
static u8 OTM8009_para15[] = {0x00,0x92};
static u8 OTM8009_para16[] = {0xc5,0x01};
static u8 OTM8009_para17[] = {0x00,0x95};
static u8 OTM8009_para18[] = {0xc5,0x34};
static u8 OTM8009_para19[] = {0x00,0x90};
static u8 OTM8009_para20[] = {0xc5,0x96};
static u8 OTM8009_para21[] = {0x00,0x94};
static u8 OTM8009_para22[] = {0xc5,0x33};
static u8 OTM8009_para23[] = {0x00,0xa3};
static u8 OTM8009_para24[] = {0xc0,0x1b};
static u8 OTM8009_para25[] = {0x00,0x82};
static u8 OTM8009_para26[] = {0xc5,0x83};
static u8 OTM8009_para27[] = {0x00,0x81};
static u8 OTM8009_para28[] = {0xc4,0x83};
static u8 OTM8009_para29[] = {0x00,0x81};
static u8 OTM8009_para30[] = {0xc1,0x66};
static u8 OTM8009_para31[] = {0x00,0xa1};
static u8 OTM8009_para32[] = {0xc1,0x08};
static u8 OTM8009_para33[] = {0x00,0x00};
static u8 OTM8009_para34[] = {0xd9,0x44};
static u8 OTM8009_para35[] = {0x00,0x00};
static u8 OTM8009_para36[] = {0xe1,0x00,0x10,0x16,0x0F,0x07,0x10,0x0A,0x09,0x04,0x07,0x0C,0x08,0x0F,0x10,0x0A,0x01};
static u8 OTM8009_para37[] = {0x00,0x00};
static u8 OTM8009_para38[] = {0xe2,0x00,0x10,0x16,0x0F,0x07,0x10,0x0A,0x09,0x04,0x07,0x0C,0x08,0x0F,0x10,0x0A,0x01};
static u8 OTM8009_para39[] = {0x00,0x00};
static u8 OTM8009_para40[] = {0x00,0x00};
static u8 OTM8009_para41[] = {0xEC,0x40,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x42,0x06};
static u8 OTM8009_para42[] = {0x00,0x00};
static u8 OTM8009_para43[] = {0xED,0x40,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x03};
static u8 OTM8009_para44[] = {0x00,0x00};
static u8 OTM8009_para45[] = {0xEE,0x40,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x22,0x08};
static u8 OTM8009_para46[] = {0x00,0x00};
static u8 OTM8009_para47[] = {0x00,0xa6};
static u8 OTM8009_para48[] = {0xb3,0x20,0x01};
static u8 OTM8009_para49[] = {0x00,0x80};
static u8 OTM8009_para50[] = {0xce,0x85,0x01,0x00,0x84,0x01,0x00};
static u8 OTM8009_para51[] = {0x00,0xa0};
static u8 OTM8009_para52[] = {0xce,0x18,0x04,0x03,0x39,0x00,0x00,0x00,0x18,0x03,0x03,0x3a,0x00,0x00,0x00};
static u8 OTM8009_para53[] = {0x00,0xb0};
static u8 OTM8009_para54[] = {0xce,0x18,0x02,0x03,0x3b,0x00,0x00,0x00,0x18,0x01,0x03,0x3c,0x00,0x00,0x00};
static u8 OTM8009_para55[] = {0x00,0xc0};
static u8 OTM8009_para56[] = {0xcf,0x01,0x01,0x20,0x20,0x00,0x00,0x01,0x00,0x00,0x00};
static u8 OTM8009_para57[] = {0x00,0xd0};
static u8 OTM8009_para58[] = {0xcf,0x00};
static u8 OTM8009_para59[] = {0x00,0x80};
static u8 OTM8009_para60[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM8009_para61[] = {0x00,0x90};
static u8 OTM8009_para62[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM8009_para63[] = {0x00,0xa0};
static u8 OTM8009_para64[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM8009_para65[] = {0x00,0xb0};
static u8 OTM8009_para66[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM8009_para67[] = {0x00,0xc0};
static u8 OTM8009_para68[] = {0xcb,0x00,0x04,0x04,0x04,0x04,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM8009_para69[] = {0x00,0xd0};
static u8 OTM8009_para70[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x04,0x04,0x04,0x04,0x00,0x00,0x00,0x00};
static u8 OTM8009_para71[] = {0x00,0xe0};
static u8 OTM8009_para72[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM8009_para73[] = {0x00,0xf0};
static u8 OTM8009_para74[] = {0xcb,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
static u8 OTM8009_para75[] = {0x00,0x80};
static u8 OTM8009_para76[] = {0xcc,0x00,0x26,0x09,0x0b,0x01,0x25,0x00,0x00,0x00,0x00};
static u8 OTM8009_para77[] = {0x00,0x90};
static u8 OTM8009_para78[] = {0xcc,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x26,0x0a,0x0c,0x02};
static u8 OTM8009_para79[] = {0x00,0xa0};
static u8 OTM8009_para80[] = {0xcc,0x25,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 OTM8009_para81[] = {0x00,0xb0};
static u8 OTM8009_para82[] = {0xcc,0x00,0x25,0x0c,0x0a,0x02,0x26,0x00,0x00,0x00,0x00};
static u8 OTM8009_para83[] = {0x00,0xc0};
static u8 OTM8009_para84[] = {0xcc,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x25,0x0b,0x09,0x01};
static struct tegra_dsi_cmd OTM8009_dsi_init_cmd[]= {
	DSI_CMD_LONG(0x39, OTM8009_para1),   
	DSI_CMD_LONG(0x39, OTM8009_para2),   
	DSI_CMD_LONG(0x39, OTM8009_para3),   
	DSI_CMD_LONG(0x39, OTM8009_para4),   
	DSI_CMD_LONG(0x39, OTM8009_para5),   
	DSI_CMD_LONG(0x39, OTM8009_para6),   
	DSI_CMD_LONG(0x39, OTM8009_para7),   
	DSI_CMD_LONG(0x39, OTM8009_para8),  
	DSI_CMD_LONG(0x39, OTM8009_para9),   
	DSI_CMD_LONG(0x39, OTM8009_para10),   
	DSI_CMD_LONG(0x39, OTM8009_para11), 
 	DSI_CMD_LONG(0x39, OTM8009_para12),   
	DSI_CMD_LONG(0x39, OTM8009_para13),  
	DSI_CMD_LONG(0x39, OTM8009_para14),   
	DSI_CMD_LONG(0x39, OTM8009_para15),   
	DSI_CMD_LONG(0x39, OTM8009_para16),   
	DSI_CMD_LONG(0x39, OTM8009_para17),   
	DSI_CMD_LONG(0x39, OTM8009_para18),  
	DSI_CMD_LONG(0x39, OTM8009_para19),   
	DSI_CMD_LONG(0x39, OTM8009_para20),   
	DSI_CMD_LONG(0x39, OTM8009_para21), 
 	DSI_CMD_LONG(0x39, OTM8009_para22),   
	DSI_CMD_LONG(0x39, OTM8009_para23),  
	DSI_CMD_LONG(0x39, OTM8009_para24),    
	DSI_CMD_LONG(0x39, OTM8009_para25),   
	DSI_CMD_LONG(0x39, OTM8009_para26),   
	DSI_CMD_LONG(0x39, OTM8009_para27),   
	DSI_CMD_LONG(0x39, OTM8009_para28),  
	DSI_CMD_LONG(0x39, OTM8009_para29),   
	DSI_CMD_LONG(0x39, OTM8009_para30),   
	DSI_CMD_LONG(0x39, OTM8009_para31), 
 	DSI_CMD_LONG(0x39, OTM8009_para32),   
	DSI_CMD_LONG(0x39, OTM8009_para33),  
	DSI_CMD_LONG(0x39, OTM8009_para34),  
	DSI_CMD_LONG(0x39, OTM8009_para35),   
	DSI_CMD_LONG(0x39, OTM8009_para36),   
	DSI_CMD_LONG(0x39, OTM8009_para37),   
	DSI_CMD_LONG(0x39, OTM8009_para38),  
	DSI_CMD_LONG(0x39, OTM8009_para39),   
	DSI_CMD_LONG(0x39, OTM8009_para40),   
	DSI_CMD_LONG(0x39, OTM8009_para41), 
 	DSI_CMD_LONG(0x39, OTM8009_para42),   
	DSI_CMD_LONG(0x39, OTM8009_para43),  
	DSI_CMD_LONG(0x39, OTM8009_para44),  
	DSI_CMD_LONG(0x39, OTM8009_para45),   
	DSI_CMD_LONG(0x39, OTM8009_para46),   
	DSI_CMD_LONG(0x39, OTM8009_para47),   
	DSI_CMD_LONG(0x39, OTM8009_para48),  
	DSI_CMD_LONG(0x39, OTM8009_para49),   
	DSI_CMD_LONG(0x39, OTM8009_para50),   
	DSI_CMD_LONG(0x39, OTM8009_para51), 
 	DSI_CMD_LONG(0x39, OTM8009_para52),   
	DSI_CMD_LONG(0x39, OTM8009_para53),  
	DSI_CMD_LONG(0x39, OTM8009_para54),  
	DSI_CMD_LONG(0x39, OTM8009_para55),   
	DSI_CMD_LONG(0x39, OTM8009_para56),   
	DSI_CMD_LONG(0x39, OTM8009_para57),   
	DSI_CMD_LONG(0x39, OTM8009_para58),  
	DSI_CMD_LONG(0x39, OTM8009_para59),   
	DSI_CMD_LONG(0x39, OTM8009_para60),   
	DSI_CMD_LONG(0x39, OTM8009_para61), 
 	DSI_CMD_LONG(0x39, OTM8009_para62),   
	DSI_CMD_LONG(0x39, OTM8009_para63),  
	DSI_CMD_LONG(0x39, OTM8009_para64),  
	DSI_CMD_LONG(0x39, OTM8009_para65),   
	DSI_CMD_LONG(0x39, OTM8009_para66),   
	DSI_CMD_LONG(0x39, OTM8009_para67),   
	DSI_CMD_LONG(0x39, OTM8009_para68),  
	DSI_CMD_LONG(0x39, OTM8009_para69),   
	DSI_CMD_LONG(0x39, OTM8009_para70),   
	DSI_CMD_LONG(0x39, OTM8009_para71), 
 	DSI_CMD_LONG(0x39, OTM8009_para72),   
	DSI_CMD_LONG(0x39, OTM8009_para73),  
	DSI_CMD_LONG(0x39, OTM8009_para74),  
	DSI_CMD_LONG(0x39, OTM8009_para75),   
	DSI_CMD_LONG(0x39, OTM8009_para76),   
	DSI_CMD_LONG(0x39, OTM8009_para77),   
	DSI_CMD_LONG(0x39, OTM8009_para78),  
	DSI_CMD_LONG(0x39, OTM8009_para79),   
	DSI_CMD_LONG(0x39, OTM8009_para80),   
	DSI_CMD_LONG(0x39, OTM8009_para81), 
 	DSI_CMD_LONG(0x39, OTM8009_para82),   
	DSI_CMD_LONG(0x39, OTM8009_para83),  
	DSI_CMD_LONG(0x39, OTM8009_para84),  
 

	DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(200),
#if(1/*DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE*/)
	DSI_CMD_SHORT(0x15, 0x35, 0x00),
#endif

	DSI_CMD_SHORT(0x05, 0x29, 0x00),
   //    DSI_CMD_SHORT(0x05, 0x2c, 0x00),
};
#endif

struct tegra_dsi_out enterprise_dsi = {
	.n_data_lanes = 2,
	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
#if(DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	/* For one-shot mode, actual refresh rate is decided by the
	 * frequency of TE signal. Although the frequency of TE is
	 * expected running at rated_refresh_rate (typically 60Hz),
	 * it may vary. Mismatch between freq of DC and TE signal
	 * would cause frame drop. We increase refresh_rate to the
	 * value larger than maximum TE frequency to avoid missing
	 * any TE signal. The value of refresh_rate is also used to
	 * calculate the pixel clock.
	 */
	.refresh_rate = 66,
	.rated_refresh_rate = 60,
#else
#if CONFIG_LCD_480_800
	.refresh_rate = 59,//55,
#else
	.refresh_rate = 57,//58,
#endif
#endif
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.panel_has_frame_buffer = true,
	.dsi_instance = 0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,
	.n_init_cmd = ARRAY_SIZE(OTM1281_dsi_init_cmd),
	.dsi_init_cmd = OTM1281_dsi_init_cmd,

	.n_early_suspend_cmd = ARRAY_SIZE(dsi_early_suspend_cmd),
	.dsi_early_suspend_cmd = dsi_early_suspend_cmd,

	.n_late_resume_cmd = ARRAY_SIZE(OTM1281_dsi_init_cmd),
	.dsi_late_resume_cmd = OTM1281_dsi_init_cmd,

	.n_suspend_cmd = ARRAY_SIZE(dsi_suspend_cmd),
	.dsi_suspend_cmd = dsi_suspend_cmd,
#if CONFIG_LCD_480_800
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE, //TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE, //TTEGRA_DSI_VIDEO_TYPE_VIDEO_MODE,//TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE, 
#else
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE, //TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE, //
//	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END,
//	.enable_hs_clock_on_lp_cmd_mode = true,
//	.hs_clk_in_lp_cmd_mode_freq_khz = 200000,
//	.hs_cmd_mode_on_blank_supported = true,

#endif
	// .lp_cmd_mode_freq_khz = 20000,
        .video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_TX_ONLY,
#if CONFIG_LCD_480_800
	/* TODO: Get the vender recommended freq */
	.lp_read_cmd_mode_freq_khz = 184000,
	.lp_cmd_mode_freq_khz = 184000,
#else
	/* TODO: Get the vender recommended freq */
	.lp_read_cmd_mode_freq_khz = 200000,
	.lp_cmd_mode_freq_khz = 200000,
#endif

};

static struct tegra_stereo_out enterprise_stereo = {
	.set_mode		= &enterprise_stereo_set_mode,
	.set_orientation	= &enterprise_stereo_set_orientation,
};

#ifdef CONFIG_TEGRA_DC
static struct tegra_dc_mode enterprise_dsi_modes[] = {
	{
#if CONFIG_LCD_480_800
        	.pclk = 12927000,
		.h_ref_to_sync = 2,
		.v_ref_to_sync = 1,
		.h_sync_width = 20,
		.v_sync_width = 20,
		.h_back_porch = 52,
		.v_back_porch = 80,
         	.h_active		= 480,
	       .v_active		= 800,
		.h_front_porch = 60,
		.v_front_porch = 10,
#else
        	.pclk = 12927000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 4,
		.h_sync_width = 10,
		.v_sync_width = 10,
		.h_back_porch = 40,
		.v_back_porch = 140,
		.h_active = 720,
		.v_active = 1280,
		.h_front_porch = 32,
		.v_front_porch = 10,
#endif
//		.flags = TEGRA_DC_MODE_FLAG_NEG_V_SYNC | TEGRA_DC_MODE_FLAG_NEG_H_SYNC,
	},
};

static struct tegra_fb_data enterprise_dsi_fb_data = {
	.win		= 0,
#if CONFIG_LCD_480_800
       .xres		= 480,
       .yres		= 800,
#else
	.xres		= 720,
	.yres		= 1280,
#endif
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_out enterprise_disp1_out = {
	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.sd_settings	= &enterprise_sd_settings,

	.flags		= DC_CTRL_MODE,

	.type		= TEGRA_DC_OUT_DSI,

	.modes		= enterprise_dsi_modes,
	.n_modes	= ARRAY_SIZE(enterprise_dsi_modes),

	.dsi		= &enterprise_dsi,
	.stereo		= &enterprise_stereo,

	.enable		= enterprise_dsi_panel_enable,
	.disable	= enterprise_dsi_panel_disable,
	.postsuspend	= enterprise_dsi_panel_postsuspend,

	.width		= 53,
	.height		= 95,
};
static struct tegra_dc_platform_data enterprise_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &enterprise_disp1_out,
	.emc_clk_rate	= 204000000,
	.fb		= &enterprise_dsi_fb_data,
};

static struct nvhost_device enterprise_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= enterprise_disp1_resources,
	.num_resources	= ARRAY_SIZE(enterprise_disp1_resources),
	.dev = {
		.platform_data = &enterprise_disp1_pdata,
	},
};

static int enterprise_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &enterprise_disp1_device.dev;
}

static struct nvhost_device enterprise_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= enterprise_disp2_resources,
	.num_resources	= ARRAY_SIZE(enterprise_disp2_resources),
	.dev = {
		.platform_data = &enterprise_disp2_pdata,
	},
};
#endif

#if defined(CONFIG_TEGRA_NVMAP)
static struct nvmap_platform_carveout enterprise_carveouts[] = {
	[0] = NVMAP_HEAP_CARVEOUT_IRAM_INIT,
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.base		= 0,	/* Filled in by enterprise_panel_init() */
		.size		= 0,	/* Filled in by enterprise_panel_init() */
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data enterprise_nvmap_data = {
	.carveouts	= enterprise_carveouts,
	.nr_carveouts	= ARRAY_SIZE(enterprise_carveouts),
};

static struct platform_device enterprise_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &enterprise_nvmap_data,
	},
};
#endif

static struct platform_device *enterprise_gfx_devices[] __initdata = {
#if defined(CONFIG_TEGRA_NVMAP)
	&enterprise_nvmap_device,
#endif
	&tegra_pwfm0_device,
};

static struct platform_device *enterprise_bl_devices[]  = {
	&enterprise_disp1_backlight_device,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
/* put early_suspend/late_resume handlers here for the display in order
 * to keep the code out of the display driver, keeping it closer to upstream
 */
static int cplog_apk_status = 0;
static int cplog_apk_proc_read(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
   return sprintf(page, "%d\n", cplog_apk_status);
}
static int cplog_apk_proc_write(struct file *file, const char *buffer,
			   size_t count, loff_t *data)
{
    static char kbuf[128];
	char *buf = kbuf;
	char cmd;
	if (count > 128)
		count = 128;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
    
    printk("[%s]CXG set_cplog_apk_status=%c\n ",__func__,buf[0]);
    if('1' == buf[0] )
    {
        cplog_apk_status = 1;
    }
    else if('0' == buf[0] )
    {
        cplog_apk_status = 0;
    }
    else
    {
        printk("usage: echo {1/0} > /proc/driver/cplog_apk_status\n");
    }
    return count;
}

static void create_cplog_apk_status_proc_file(void)	
{  
	struct proc_dir_entry *entry = 
        create_proc_entry("driver/cplog_apk_status", 0666, NULL);     
   	if (entry)  
    {
        entry->read_proc = cplog_apk_proc_read;
		entry->write_proc = cplog_apk_proc_write;
    }      
}

struct early_suspend enterprise_panel_early_suspender;

static void enterprise_panel_early_suspend(struct early_suspend *h)
{
	 printk("[%s] SuspendFlag %d\n", __FUNCTION__, g_SuspendFlag);

	g_SuspendFlag = 1;
	/* power down LCD, add use a black screen for HDMI */
	if (num_registered_fb > 0)
		fb_blank(registered_fb[0], FB_BLANK_POWERDOWN);
	if (num_registered_fb > 1)
		fb_blank(registered_fb[1], FB_BLANK_NORMAL);

#ifdef CONFIG_TEGRA_CONVSERVATIVE_GOV_ON_EARLYSUPSEND
	if(!cplog_apk_status)
	{
		cpufreq_store_default_gov();
		cpufreq_change_gov(cpufreq_conservative_gov);
	}
#endif
    printk("[%s] SuspendFlag end \n", __FUNCTION__);
}
extern void tegra_dc_promote_emc_rate(void);
extern void tegra_dc_restore_emc_rate(void);
static void enterprise_panel_late_resume(struct early_suspend *h)
{
	unsigned i;
	tegra_dc_promote_emc_rate();
#ifdef CONFIG_TEGRA_CONVSERVATIVE_GOV_ON_EARLYSUPSEND
	cpufreq_restore_default_gov();
#endif
	for (i = 0; i < num_registered_fb; i++)
		fb_blank(registered_fb[i], FB_BLANK_UNBLANK);
	tegra_dc_restore_emc_rate();
}
#endif

int enterprise_panel_set_lcd_type(int panelid)
{
    int  ret;
    int lcdid;
    printk("+++++++++++++++++++++++++++enterprise_panel_set_lcd_type\n");
  
     if(panelid == 0x80)
     {
            printk("+++++++++++++++++++++++++++enterprise_panel_set_lcd_type otm1280\n");
            enterprise_dsi.n_init_cmd = ARRAY_SIZE(OTM1280_dsi_init_cmd);
            enterprise_dsi.dsi_init_cmd = OTM1280_dsi_init_cmd;	
            enterprise_dsi.n_late_resume_cmd = ARRAY_SIZE(OTM1280_dsi_init_cmd);
            enterprise_dsi.dsi_late_resume_cmd = OTM1280_dsi_init_cmd;       
            enterprise_dsi_modes[0].h_ref_to_sync = 4;
            enterprise_dsi_modes[0].v_ref_to_sync = 4;
            enterprise_dsi_modes[0].h_sync_width = 10;// 4;
            enterprise_dsi_modes[0].v_sync_width = 10;// 4;
            enterprise_dsi_modes[0].h_back_porch = 40;// 50;
            enterprise_dsi_modes[0].v_back_porch = 140;// 130;
            enterprise_dsi_modes[0].h_front_porch = 32;// 60;
            enterprise_dsi_modes[0].v_front_porch = 10;

      }
      else if(panelid == 0x81)
      {
            printk("+++++++++++++++++++++++++++enterprise_panel_set_lcd_type otm1281\n");
       }
      else if(panelid == 0x02){
            printk("+++++++++++++++++++++++++++enterprise_panel_set_lcd_type TMHX8369\n");
            enterprise_dsi.n_init_cmd = ARRAY_SIZE(TMHX8369B_dsi_init_cmd);
            enterprise_dsi.dsi_init_cmd = TMHX8369B_dsi_init_cmd;	
            enterprise_dsi.n_late_resume_cmd = ARRAY_SIZE(TMHX8369B_dsi_init_cmd);
            enterprise_dsi.dsi_late_resume_cmd = TMHX8369B_dsi_init_cmd;     
            enterprise_dsi_modes[0].h_ref_to_sync = 2;
            enterprise_dsi_modes[0].v_ref_to_sync = 1;
            enterprise_dsi_modes[0].h_sync_width = 2;
            enterprise_dsi_modes[0].v_sync_width = 10;
            enterprise_dsi_modes[0].h_back_porch = 52;
            enterprise_dsi_modes[0].v_back_porch = 26;
            enterprise_dsi_modes[0].h_front_porch = 60;
            enterprise_dsi_modes[0].v_front_porch = 10;
       }
      else if(panelid == 0x01){
            printk("+++++++++++++++++++++++++++enterprise_panel_set_lcd_type NT35510\n");
            enterprise_dsi.n_init_cmd = ARRAY_SIZE(NT35510_dsi_init_cmd);
            enterprise_dsi.dsi_init_cmd = NT35510_dsi_init_cmd;	
            enterprise_dsi.n_late_resume_cmd = ARRAY_SIZE(NT35510_dsi_init_cmd);
            enterprise_dsi.dsi_late_resume_cmd = NT35510_dsi_init_cmd;         
            enterprise_dsi.video_data_type = TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE;
            enterprise_dsi_modes[0].h_ref_to_sync = 2;
            enterprise_dsi_modes[0].v_ref_to_sync = 1;
            enterprise_dsi_modes[0].h_sync_width = 20;
            enterprise_dsi_modes[0].v_sync_width = 20;
            enterprise_dsi_modes[0].h_back_porch = 80;
            enterprise_dsi_modes[0].v_back_porch = 80;
            enterprise_dsi_modes[0].h_front_porch = 100;
            enterprise_dsi_modes[0].v_front_porch = 10;
         //   enterprise_dsi_modes[0].flags = TEGRA_DC_MODE_FLAG_NEG_V_SYNC | TEGRA_DC_MODE_FLAG_NEG_H_SYNC,

       }
      else if(panelid == 0x03){
            printk("+++++++++++++++++++++++++++enterprise_panel_set_lcd_type NT35510\n");
            enterprise_dsi.n_init_cmd = ARRAY_SIZE(NT35510_dsi_init_cmd_lead);
            enterprise_dsi.dsi_init_cmd = NT35510_dsi_init_cmd_lead;	
            enterprise_dsi.n_late_resume_cmd = ARRAY_SIZE(NT35510_dsi_init_cmd_lead);
            enterprise_dsi.dsi_late_resume_cmd = NT35510_dsi_init_cmd_lead;         
            enterprise_dsi.video_data_type = TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE;
            enterprise_dsi_modes[0].h_ref_to_sync = 2;
            enterprise_dsi_modes[0].v_ref_to_sync = 1;
            enterprise_dsi_modes[0].h_sync_width = 20;
            enterprise_dsi_modes[0].v_sync_width = 20;
            enterprise_dsi_modes[0].h_back_porch = 80;
            enterprise_dsi_modes[0].v_back_porch = 80;
            enterprise_dsi_modes[0].h_front_porch = 100;
            enterprise_dsi_modes[0].v_front_porch = 10;
         //   enterprise_dsi_modes[0].flags = TEGRA_DC_MODE_FLAG_NEG_V_SYNC | TEGRA_DC_MODE_FLAG_NEG_H_SYNC,

       }      else if(panelid == 0x04){
            printk("+++++++++++++++++++++++++++enterprise_panel_set_lcd_type NT35510\n");
            enterprise_dsi.n_init_cmd = ARRAY_SIZE(NT35510_dsi_init_cmd);
            enterprise_dsi.dsi_init_cmd = NT35510_dsi_init_cmd;	
            enterprise_dsi.n_late_resume_cmd = ARRAY_SIZE(NT35510_dsi_init_cmd);
            enterprise_dsi.dsi_late_resume_cmd = NT35510_dsi_init_cmd;
            enterprise_dsi.video_data_type = TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE;
            enterprise_dsi_modes[0].h_ref_to_sync = 2;
            enterprise_dsi_modes[0].v_ref_to_sync = 1;
            enterprise_dsi_modes[0].h_sync_width = 20;
            enterprise_dsi_modes[0].v_sync_width = 20;
            enterprise_dsi_modes[0].h_back_porch = 80;
            enterprise_dsi_modes[0].v_back_porch = 80;
            enterprise_dsi_modes[0].h_front_porch = 100;
            enterprise_dsi_modes[0].v_front_porch = 10;
         //   enterprise_dsi_modes[0].flags = TEGRA_DC_MODE_FLAG_NEG_V_SYNC | TEGRA_DC_MODE_FLAG_NEG_H_SYNC,

       }      
         else{
            printk("+++++++++++++++++++++++++++enterprise_panel_set_lcd_type OTM8009\n");
            enterprise_dsi.n_init_cmd = ARRAY_SIZE(OTM8009_dsi_init_cmd);
            enterprise_dsi.dsi_init_cmd = OTM8009_dsi_init_cmd;	
            enterprise_dsi.n_late_resume_cmd = ARRAY_SIZE(OTM8009_dsi_init_cmd);
            enterprise_dsi.dsi_late_resume_cmd = OTM8009_dsi_init_cmd;         
/*            
            enterprise_dsi.n_init_cmd = ARRAY_SIZE(OTM8018_dsi_init_cmd);
            enterprise_dsi.dsi_init_cmd = OTM8018_dsi_init_cmd;	
            enterprise_dsi.n_late_resume_cmd = ARRAY_SIZE(OTM8018_dsi_init_cmd);
            enterprise_dsi.dsi_late_resume_cmd = OTM8018_dsi_init_cmd; 
            enterprise_dsi_modes[0].h_ref_to_sync = 2;
            enterprise_dsi_modes[0].v_ref_to_sync = 1;
            enterprise_dsi_modes[0].h_sync_width = 2;
            enterprise_dsi_modes[0].v_sync_width = 10;
            enterprise_dsi_modes[0].h_back_porch = 52;
            enterprise_dsi_modes[0].v_back_porch = 26;
            enterprise_dsi_modes[0].h_front_porch = 60;
            enterprise_dsi_modes[0].v_front_porch = 10; */
      }      
      return 0;
}
EXPORT_SYMBOL(enterprise_panel_set_lcd_type);

int enterprise_panel_get_lcd_id(void)
{
      return zte_get_lcd_id();
}
EXPORT_SYMBOL(enterprise_panel_get_lcd_id);


int __init enterprise_panel_init(void)
{
	int err;
	struct resource __maybe_unused *res;
	struct board_info board_info;
//zte lipeng10094834 add  20120525
#ifdef CONFIG_BACKLIGHT_1_WIRE_MODE
        tegra_gpio_enable(TPS61165_CTRL_PIN);  
	gpio_request(TPS61165_CTRL_PIN,  "ventana_lcd_backlight");
	gpio_direction_output(TPS61165_CTRL_PIN, 1);
      s_tps61165_is_inited = false;
      spin_lock_init(&s_tps61165_lock); /*ZTE: added by tong.weili 解决背光设置稳定性问题 20111123*/
      mutex_init(&s_tps61165_mutex); /*ZTE: added by tong.weili 增加背光设置稳定性 20111128*/
#endif
    g_lcdid = zte_get_lcd_id();
    enterprise_panel_set_lcd_type(g_lcdid);


//zte lipeng10094834 add end 20120525
	tegra_get_board_info(&board_info);

	BUILD_BUG_ON(ARRAY_SIZE(enterprise_bl_output_measured_a03) != 256);
	BUILD_BUG_ON(ARRAY_SIZE(enterprise_bl_output_measured_a02) != 256);

	if (board_info.fab >= BOARD_FAB_A03) {
		enterprise_disp1_backlight_data.clk_div = 0x1D;
		bl_output = enterprise_bl_output_measured_a03;
	} else
		bl_output = enterprise_bl_output_measured_a02;

	enterprise_dsi.chip_id = tegra_get_chipid();
	enterprise_dsi.chip_rev = tegra_get_revision();

#if defined(CONFIG_TEGRA_NVMAP)
	enterprise_carveouts[1].base = tegra_carveout_start;
	enterprise_carveouts[1].size = tegra_carveout_size;
#endif

	tegra_gpio_enable(enterprise_hdmi_hpd);
	gpio_request(enterprise_hdmi_hpd, "hdmi_hpd");
	gpio_direction_input(enterprise_hdmi_hpd);

	tegra_gpio_enable(enterprise_lcd_2d_3d);
	gpio_request(enterprise_lcd_2d_3d, "lcd_2d_3d");
	gpio_direction_output(enterprise_lcd_2d_3d, 0);
	enterprise_stereo_set_mode(enterprise_stereo.mode_2d_3d);

	tegra_gpio_enable(enterprise_lcd_swp_pl);
	gpio_request(enterprise_lcd_swp_pl, "lcd_swp_pl");
	gpio_direction_output(enterprise_lcd_swp_pl, 0);
	enterprise_stereo_set_orientation(enterprise_stereo.orientation);

#if !(DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	tegra_gpio_enable(enterprise_lcd_te);
	gpio_request(enterprise_lcd_te, "lcd_te"); // ZTE:modified by pengtao for WARNING
	gpio_direction_input(enterprise_lcd_te);

#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	enterprise_panel_early_suspender.suspend = enterprise_panel_early_suspend;
	enterprise_panel_early_suspender.resume = enterprise_panel_late_resume;
	enterprise_panel_early_suspender.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&enterprise_panel_early_suspender);
	create_cplog_apk_status_proc_file();
#endif

#ifdef CONFIG_TEGRA_GRHOST
	err = nvhost_device_register(&tegra_grhost_device);
	if (err)
		return err;
#endif

	err = platform_add_devices(enterprise_gfx_devices,
				ARRAY_SIZE(enterprise_gfx_devices));

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	res = nvhost_get_resource_byname(&enterprise_disp1_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;
#endif

	/* Copy the bootloader fb to the fb. */
	tegra_move_framebuffer(tegra_fb_start, tegra_bootloader_fb_start,
		min(tegra_fb_size, tegra_bootloader_fb_size));

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	if (!err)
		err = nvhost_device_register(&enterprise_disp1_device);

	res = nvhost_get_resource_byname(&enterprise_disp2_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb2_start;
	res->end = tegra_fb2_start + tegra_fb2_size - 1;
	if (!err)
		err = nvhost_device_register(&enterprise_disp2_device);
#endif

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_NVAVP)
	if (!err)
		err = nvhost_device_register(&nvavp_device);
#endif

	if (!err)
		err = platform_add_devices(enterprise_bl_devices,
				ARRAY_SIZE(enterprise_bl_devices));
	return err;
}
#ifdef CONFIG_BACKLIGHT_1_WIRE_MODE
/*ZTE: added by tong.weili for 1-wire backlight 20111122 ++*/
#define TPS61165_DEVICE_ADDR  (0x72)
#define BIT_DELAY_UNIT (50)
#define LOGIC_FACTOR (3)
#define CONDITION_DELAY (50)
#define ES_DETECT_DELAY  (200) 
#define ES_DETECT_TIME  (300) 
#define ES_TIMING_WINDOW  (1000)

#define TPS61165_DELAY(n) udelay(n)

static int tps61165_write_bit(u8 b)
{
    if(1 == b)
    {
        gpio_set_value(TPS61165_CTRL_PIN, 0);
        TPS61165_DELAY(BIT_DELAY_UNIT);
        gpio_set_value(TPS61165_CTRL_PIN, 1);
        TPS61165_DELAY(LOGIC_FACTOR*BIT_DELAY_UNIT);
        //gpio_set_value(TPS61165_CTRL_PIN, 0);
    }
    else if(0 == b)
    {
        gpio_set_value(TPS61165_CTRL_PIN, 0);
        TPS61165_DELAY(LOGIC_FACTOR*BIT_DELAY_UNIT);
        gpio_set_value(TPS61165_CTRL_PIN, 1);
        TPS61165_DELAY(BIT_DELAY_UNIT);
        //gpio_set_value(TPS61165_CTRL_PIN, 0);
    }
    else
    {
        printk("[tong]:tps61165_write_bit: error param!\n");
        return -1;
    }
    
    return 0;
}

static int tps61165_write_byte(u8 bytedata)
{
    u8 bit_cnt = 8;
    u8 val = bytedata;
    int ret = 0;
    unsigned long flags;
    
    //spin_lock(&s_tps61165_lock); /*ZTE: added by tong.weili 解决待机唤醒时偶发背光设置出错 20111128*/
    spin_lock_irqsave(&s_tps61165_lock, flags);/*ZTE: modified by tong.weili 增加背光设置稳定性 20120310*/
    gpio_set_value(TPS61165_CTRL_PIN, 1);
    TPS61165_DELAY(CONDITION_DELAY); //Start condition,  at least 2us

    bit_cnt = 8;
		
    while(bit_cnt)
    {
        bit_cnt--;
        if((val >> bit_cnt) & 1)
        {          
            ret = tps61165_write_bit(1);
        }
        else
        {
            ret = tps61165_write_bit(0);
        } 

        if(ret)
        {
            printk("[tong]:tps61165_write_byte:failed!\n");
            //spin_unlock(&s_tps61165_lock); /*ZTE: added by tong.weili 解决待机唤醒时偶发背光设置出错 20111128*/
            spin_unlock_irqrestore(&s_tps61165_lock, flags);
            return ret;
        }     
    }

    gpio_set_value(TPS61165_CTRL_PIN, 0);
    TPS61165_DELAY(CONDITION_DELAY); //EOS condition, at least 2us
    gpio_set_value(TPS61165_CTRL_PIN, 1);
    
    //spin_unlock(&s_tps61165_lock); /*ZTE: added by tong.weili 解决待机唤醒时偶发背光设置出错 20111128*/
    spin_unlock_irqrestore(&s_tps61165_lock, flags);
    
    return 0;
}

static int tps61165_config_ES_timing(void)
{
    unsigned long flags;//tong test
    
    printk("[tong]:tps61165_config_ES_timing\n");
    
    //spin_lock(&s_tps61165_lock); /*ZTE: added by tong.weili 解决待机唤醒时偶发背光设置出错 20111128*/
    spin_lock_irqsave(&s_tps61165_lock, flags);/*ZTE: modified by tong.weili 增加背光设置稳定性 20120310*/
    
    gpio_set_value(TPS61165_CTRL_PIN, 0);
    TPS61165_DELAY(CONDITION_DELAY);
    
    gpio_set_value(TPS61165_CTRL_PIN, 1);  //start ES Timing Window
    TPS61165_DELAY(ES_DETECT_DELAY); //at least 100us

    gpio_set_value(TPS61165_CTRL_PIN, 0);
    TPS61165_DELAY(ES_DETECT_TIME); //at least 260us

    gpio_set_value(TPS61165_CTRL_PIN, 1);
    TPS61165_DELAY(ES_TIMING_WINDOW - ES_DETECT_DELAY - ES_DETECT_TIME);

    mdelay(1);
    
    //spin_unlock(&s_tps61165_lock); /*ZTE: added by tong.weili 解决待机唤醒时偶发背光设置出错 20111128*/
    spin_unlock_irqrestore(&s_tps61165_lock, flags);
    
    return 0;
}

static int tps61165_shutdown(void)
{
    printk("[tong]:tps61165_shutdown\n");
    
    gpio_set_value(TPS61165_CTRL_PIN, 0);
    mdelay(4); //enter shutdown mode, at least 2.5ms
    return 0;
}

static int tps61165_init(void)
{
    tps61165_shutdown();
    tps61165_config_ES_timing();
    return 0;
}

int tps61165_set_backlight(int brightness)
{
    u8 tps61165_level;
    static u8 old_level = -1;
 
    /*ZTE: added by tong.weili 开机刚进入kernel 不设默认背光，因为已在bootloader中设置过 20111130 ++*/
    if(DEFAULT_BRIGHTNESS == brightness)
    {
        if(!s_bNeedSetBacklight)
        {
            s_bNeedSetBacklight = true;
            return 0;
        }      
    }
    /*ZTE: added by tong.weili 开机刚进入kernel 不设默认背光，因为已在bootloader中设置过 20111130 --*/
    
    tps61165_level = (brightness & 0xFF) >> 3;/*convert level 0~255  to  0~31*/

    if(old_level == tps61165_level)
    {
        //printk("[tong]:tps61165_set_backlight: the same level as before, nothing done!level=%d\n", tps61165_level);
        return 0;
    }

    
    /*ZTE: modified by tong.weili for backlight print 20120202 ++*/
    if(debug)
    {
        printk("[tong]:tps61165_set_backlight: brightness=%d, tps61165_level=%d\n", brightness, tps61165_level);
    }
    /*ZTE: modified by tong.weili for backlight print 20120202 --*/

    /*ZTE: added by tong.weili 优化背光处理流程 20111125 ++*/
    if(tps61165_level)
    {
        if(!s_tps61165_is_inited)
        {
            tps61165_init();
            s_tps61165_is_inited = true;
        }      
    }
    else
    {
        tps61165_shutdown();
        old_level = tps61165_level;
        s_tps61165_is_inited = false;
        return 0;
    }
    /*ZTE: added by tong.weili 优化背光处理流程 20111125 --*/

    mutex_lock(&s_tps61165_mutex); /*ZTE: added by tong.weili 增加背光设置稳定性 20111128*/
    
    tps61165_write_byte(TPS61165_DEVICE_ADDR);
    tps61165_write_byte(tps61165_level);

    mutex_unlock(&s_tps61165_mutex); /*ZTE: added by tong.weili 增加背光设置稳定性 20111128*/

    old_level = tps61165_level;
    return 0;    
}
/*ZTE: added by tong.weili for 1-wire backlight 20111122 --*/
#endif

