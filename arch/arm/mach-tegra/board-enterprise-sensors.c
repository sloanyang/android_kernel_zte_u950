/*
 * arch/arm/mach-tegra/board-enterprise-sensors.c
 *
 * Copyright (c) 2011, NVIDIA CORPORATION, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of NVIDIA CORPORATION nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <mach/gpio.h>
#include <mach/edp.h>
#include <mach/thermal.h>
#include "cpu-tegra.h"
#include "gpio-names.h"
#include "board-enterprise.h"
#include "board.h"
#include <media/ov5640.h>
#include <media/ov7692.h>
#include <media/ov8825.h>
#include <media/ov8825_focuser.h>
#include <media/mt9m114.h>
#include <media/adp1650.h>
#include <linux/platform_device.h>
/*sensor header files*/
#include <linux/nct1008.h>
#include <linux/i2c/tmd2771x.h>
#include <linux/i2c/l3g4200d.h>
#include <linux/i2c/akm8962.h>
#include <linux/accel.h>

#define MAX17058_ALERT_GPIO	TEGRA_GPIO_PI5

/*voltage measurement*/
#include <linux/max17058_battery.h>
/*ZTE: added by li.liguo for Sii8334 I2C Address 20120529*/
#ifdef CONFIG_MHL_SII8334
#define CI2CA  true  // CI2CA depend on the CI2CA pin's level
#ifdef CI2CA 
#define SII8334_plus 0x02  //Define sii8334's I2c Address of all pages by the status of CI2CA.
#else
#define SII8334_plus 0x00  //Define sii8334's I2c Address of all pages by the status of CI2CA.
#endif
/*ZTE: added by li.liguo for Sii8334 I2C Address  20120529*/

/*ZTE: added by li.liguo for Sii8334 Init 20120529*/
struct MHL_platform_data {
	void (*reset) (void);
};

static void Sii8334_reset(void)
{	
	int ret;

	printk("[MHL] sii8334 reset sequence\n");
	tegra_gpio_enable(AP_MHL_RESET);
	ret = gpio_request(AP_MHL_RESET, "mhl_reset");
	if (ret < 0)
		printk("[MHL] sii8334 reset sequence request gpio fail...\n");
	
	ret = gpio_direction_output(AP_MHL_RESET, 1);
	if (ret < 0) {
		gpio_free(AP_MHL_RESET);
	}
	
#if 0
	gpio_export(AP_MHL_RESET, false);	
	gpio_set_value(AP_MHL_RESET, 0);
	mdelay(100);
	gpio_set_value(AP_MHL_RESET, 1);

	//gpio_free(AP_MHL_RESET);
#endif
}

static struct MHL_platform_data Sii8334_data = {
	.reset = Sii8334_reset,
};

static int __init enterprise_mhl_init(void)
{    
//	int ret;
	printk("[MHL] sii8334 init sequence\n");

	gpio_request(AP_MHL_INT, "mhl-irq");	  
       gpio_direction_input(AP_MHL_INT); 
       tegra_gpio_enable(AP_MHL_INT);

#if 0
	  printk("[MHL] init irq & power\n");
      	if (!ventana_LDO8) {
		ventana_LDO8 = regulator_get(NULL, "vdd_ldo8"); /* LD03 */
		if (IS_ERR_OR_NULL(ventana_LDO8)) {
			printk(KERN_ERR "dsi: couldn't get regulator vdd_ldo8\n");
			ventana_LDO8 = NULL;
			return PTR_ERR(ventana_LDO8);
		}
        	/* set HDMI voltage to 3V3*/
       ret = regulator_set_voltage(ventana_LDO8, 3300*1000, 3300*1000);
	if (ret) {
		printk(KERN_ERR "%s: Failed to set vdd_ldo8 to 3.3v\n", __func__);
              regulator_put(ventana_LDO8);
		return PTR_ERR(ventana_LDO8);
	}
            regulator_enable(ventana_LDO8); 

    }
#endif
     return 0;
}
#endif
/*ZTE: added by li.liguo for Sii8334  20120529*/

static int nct_get_temp(void *_data, long *temp)
{
	struct nct1008_data *data = _data;
	return nct1008_thermal_get_temp(data, temp);
}

static int nct_get_temp_low(void *_data, long *temp)
{
	struct nct1008_data *data = _data;
	return nct1008_thermal_get_temp_low(data, temp);
}

static int nct_set_limits(void *_data,
			long lo_limit_milli,
			long hi_limit_milli)
{
	struct nct1008_data *data = _data;
	return nct1008_thermal_set_limits(data,
					lo_limit_milli,
					hi_limit_milli);
}

static int nct_set_alert(void *_data,
				void (*alert_func)(void *),
				void *alert_data)
{
	struct nct1008_data *data = _data;
	return nct1008_thermal_set_alert(data, alert_func, alert_data);
}

static int nct_set_shutdown_temp(void *_data, long shutdown_temp)
{
	struct nct1008_data *data = _data;
	return nct1008_thermal_set_shutdown_temp(data,
						shutdown_temp);
}

#ifdef CONFIG_TEGRA_SKIN_THROTTLE
static int nct_get_itemp(void *dev_data, long *temp)
{
        struct nct1008_data *data = dev_data;
        return nct1008_thermal_get_temps(data, NULL, temp);
}
#endif

static void nct1008_probe_callback(struct nct1008_data *data)
{
	struct tegra_thermal_device *thermal_device;

	thermal_device = kzalloc(sizeof(struct tegra_thermal_device),
					GFP_KERNEL);
	if (!thermal_device) {
		pr_err("unable to allocate thermal device\n");
		return;
	}

	thermal_device->name = "nct1008";
	thermal_device->data = data;
	thermal_device->id = THERMAL_DEVICE_ID_NCT_EXT;	
	thermal_device->offset = TDIODE_OFFSET;
	thermal_device->get_temp = nct_get_temp;
	thermal_device->get_temp_low = nct_get_temp_low;
	thermal_device->set_limits = nct_set_limits;
	thermal_device->set_alert = nct_set_alert;
	thermal_device->set_shutdown_temp = nct_set_shutdown_temp;

	tegra_thermal_device_register(thermal_device);

#ifdef CONFIG_TEGRA_SKIN_THROTTLE
        {
                struct tegra_thermal_device *int_nct;
                int_nct = kzalloc(sizeof(struct tegra_thermal_device),
                                                GFP_KERNEL);
                if (!int_nct) {
                        kfree(int_nct);
                        pr_err("unable to allocate thermal device\n");
                        return;
                }

                int_nct->name = "nct_int";
                int_nct->id = THERMAL_DEVICE_ID_NCT_INT;
                int_nct->data = data;
                int_nct->get_temp = nct_get_itemp;

                tegra_thermal_device_register(int_nct);
        }
#endif

}

static struct nct1008_platform_data enterprise_nct1008_pdata = {
	.supported_hwrev = true,
	.ext_range = true,
	.conv_rate = 0x08,
	.offset = 0, /* 4 * offset(float) offset must be times of 0.25 */
	.probe_callback = nct1008_probe_callback,
};

/*ZTE: added by li.liguo for Sii8334  I2C BoardInfo 20120529*/
static struct i2c_board_info Sii8334_i2c0_board_info[] = {
	
	#ifdef CONFIG_MHL_SII8334
	{
	  I2C_BOARD_INFO("sii8334_PAGE_TPI", 0x39 + SII8334_plus),
	 .irq = TEGRA_GPIO_TO_IRQ(AP_MHL_INT),  //define the interrupt signal input pin
	 .platform_data = &Sii8334_data,
	},
	{
	  I2C_BOARD_INFO("sii8334_PAGE_TX_L0", 0x39 + SII8334_plus),
	},
	{
	  I2C_BOARD_INFO("sii8334_PAGE_TX_L1", 0x3d + SII8334_plus),
	},
	{
	  I2C_BOARD_INFO("sii8334_PAGE_TX_2", 0x49 + SII8334_plus),
	},
	{
	  I2C_BOARD_INFO("sii8334_PAGE_TX_3", 0x4d + SII8334_plus),
	},
	{
	  I2C_BOARD_INFO("sii8334_PAGE_CBUS", 0x64 + SII8334_plus),
	},
#endif

	};
/*ZTE: added by li.liguo for Sii8334  I2C BoardInfo 20120529*/
#ifdef CONFIG_SENSORS_TMD2771
static struct prox_config prox_config_pdata = {
	.pulse_count = 4,
	.led_drive = PROX_LED_DRIVE_25MA,
	.threshold_lo = 500,
	.threshold_hi = 560,
	.thresh_offset = 0,
	.thresh_gain_hi = 200,
	.thresh_gain_lo = 170,
};


static struct tmd2771x_platform_data tmd2771_pdata = {
	.prox_config = &prox_config_pdata,
};
#endif

#ifdef CONFIG_ACCEL_SENSORS
static unsigned short accel_addr_list[] = {
    0x0F, // kionix
    0x19, // st
    I2C_CLIENT_END
};

static struct accel_platform_data accel_pdata ={
	.adapt_nr = 0,
	.accel_list = accel_addr_list,
	.poll_inerval = 10,
	.g_range = ACCEL_FS_2G,
};

static struct platform_device accel_platform_device = {
    .name		= "accel_platform",
    .id = -1,
    .dev = {
        .platform_data = &accel_pdata,
    },
};
#endif

#ifdef CONFIG_SENSORS_L3G4200D
static struct l3g4200d_gyr_platform_data l3g4200d_pdata = {
	.poll_interval = 10,
	.min_interval = 2,
	.fs_range      = L3G4200D_FS_2000DPS,
	#ifdef CONFIG_PROJECT_U950
	.axis_map_x = 1,
	.axis_map_y = 0,	
	#else
	.axis_map_x = 0,
	.axis_map_y = 1,
	#endif
	.axis_map_z = 2,

	.negate_x = 0,
	#ifdef CONFIG_PROJECT_U950
	.negate_y = 0,
	#else
	.negate_y = 1,
	#endif
	.negate_z = 1,
};
#endif

#ifdef CONFIG_SENSORS_AK8962
static struct akm8962_platform_data akm8962_pdata = {
	.gpio_DRDY = TEGRA_GPIO_PH5,
};
#endif

#ifdef CONFIG_BATTERY_MAX17058
extern int tps80031_get_vbus_status(void);
static int battery_is_online(void)
{
	return 1;
}
static int charger_is_online(void)
{
	int charger_present = -1;
    charger_present = tps80031_get_vbus_status();
	return charger_present;
}

static int charger_enable(void)
{
	return 1;
}

static struct max17058_platform_data enterprise_max17058_pdata = {
	.battery_online = battery_is_online,
	.charger_online = charger_is_online,
	.charger_enable = charger_enable,
	#ifdef CONFIG_HIGH_VOLTAGE_LEVEL_BATTERY   
	.ini_data={
                        .title = "939_2_062812",
                        .emptyadjustment = 0,
                        .fulladjustment = 100,
                        .rcomp0 = 89,				/* Starting RCOMP value */
                        .tempcoup = -1025,			/* Temperature (hot) coeffiecient for RCOMP, should div 10 ,origin is -1.025*/
                        .tempcodown = -6300,			/* Temperature (cold) coeffiecient for RCOMP, should div 10,origin is -6.3*/
                        .ocvtest = 57936,			/* OCVTest vaule */
                        .socchecka = 234,			/* SOCCheck low value */
                        .soccheckb = 236,			/* SOCCheck high value */
                        .bits = 19,				/* 18 or 19 bit model */
                        .data = {   0x98,0x71,0x7F,0x8B,0xAD,0xC2,0xD0,0xDB,
                                    0xE5,0x08,0x2D,0x75,0x7F,0x9F,0x2D,0x85,
                                    0xE1,0x0E,0x01,0x0E,0x21,0x0E,0x71,0x08,
                                    0x71,0x0E,0x90,0x0E,0x20,0x0C,0x80,0x0C,
                                    0xA9,0x80,0xB7,0x10,0xB7,0xF0,0xB8,0xB0,
                                    0xBA,0xD0,0xBC,0x20,0xBD,0x00,0xBD,0xB0,
                                    0xBE,0x50,0xC0,0x80,0xC2,0xD0,0xC7,0x50,
                                    0xC7,0xF0,0xC9,0xF0,0xD2,0xD0,0xD8,0x50,
                                    0x03,0x80,0x1C,0xA0,0x34,0x00,0x14,0x00,
                                    0x1E,0x00,0x44,0x00,0x38,0x00,0x3C,0x40,
                                    0x1D,0xE0,0x19,0xE0,0x11,0xE0,0x13,0x80,
                                    0x15,0xE0,0x0D,0xE0,0x0E,0xC0,0x0E,0xC0,
                                    0x90,0x08,0x71,0x0A,0x73,0x00,0x81,0x00,
                                    0xA1,0x00,0xC4,0x00,0xD3,0x00,0xD3,0x04,
                                    0x38,0xCA,0x40,0x40,0xE0,0x40,0x80,0xC4,
                                    0xDE,0x9E,0x1E,0x38,0x5E,0xDE,0xEC,0xEC,
                                        } ,
	},
	#else
    #if 1
    .ini_data={
                        .title = "938_2_062812",
                        .emptyadjustment = 0,
                        .fulladjustment = 100,
                        .rcomp0 = 87,				/* Starting RCOMP value */
                        .tempcoup = -600,			/* Temperature (hot) coeffiecient for RCOMP, should div 10 ,origin is -0.6*/
                        .tempcodown = -2900,			/* Temperature (cold) coeffiecient for RCOMP, should div 10,origin is -2.9 */
                        .ocvtest = 56016,			/* OCVTest vaule */
                        .socchecka = 238,			/* SOCCheck low value */
                        .soccheckb = 240,			/* SOCCheck high value */
                        .bits = 19,				/* 18 or 19 bit model */
                        .data = {   0x78,0x6E,0x8A,0xAC,0xBB,0xCC,0xD6,0xDC,
                                    0xE3,0xF1,0x17,0x59,0x78,0x98,0xD2,0x0D,
                                    0xE1,0x0E,0xF2,0x0E,0x11,0x0E,0x51,0x04,
                                    0x71,0x08,0x90,0x00,0xD1,0x00,0x01,0x00,
                                    0xA7,0x80,0xB6,0xE0,0xB8,0xA0,0xBA,0xC0,
                                    0xBB,0xB0,0xBC,0xC0,0xBD,0x60,0xBD,0xC0,
                                    0xBE,0x30,0xBF,0x10,0xC1,0x70,0xC5,0x90,
                                    0xC7,0x80,0xC9,0x80,0xCD,0x20,0xD0,0xD0,
                                    0x01,0xD0,0x22,0x00,0x1C,0x00,0x40,0x00,
                                    0x2F,0x00,0x58,0xB0,0x73,0x90,0x4D,0xC0,
                                    0x1B,0xE0,0x23,0xE0,0x15,0xE0,0x18,0x40,
                                    0x17,0x80,0x0F,0x00,0x10,0x00,0x10,0x00,
                                    0x70,0x0D,0x62,0x00,0x81,0x00,0xA4,0x00,
                                    0xB2,0x00,0xC5,0x0B,0xD7,0x09,0xD4,0x0C,
                                    0x1D,0x20,0xC0,0x00,0xF0,0x8B,0x39,0xDC,
                                    0xBE,0x3E,0x5E,0x84,0x78,0xF0,0x00,0x00,
                                        } ,
    #endif
     #if 0                                  
    .ini_data={
                        .title = "839_1_032612",
                        .emptyadjustment = 0,
                        .fulladjustment = 100,
                        .rcomp0 = 70,				/* Starting RCOMP value */
                        .tempcoup = -0.375,			/* Temperature (hot) coeffiecient for RCOMP, should div 10 */
                        .tempcodown = -3.225,			/* Temperature (cold) coeffiecient for RCOMP, should div 10 */
                        .ocvtest = 55888,			/* OCVTest vaule */
                        .socchecka = 233,			/* SOCCheck low value */
                        .soccheckb = 235,			/* SOCCheck high value */
                        .bits = 19,				/* 18 or 19 bit model */
                        .data = {       0x4F,0x5F,0x92,0x9D,0xB7,0xC1,0xC8,0xD4,
                                        0xE2,0x0B,0x31,0x57,0x80,0xA9,0xD9,0x05,
                                        0xE1,0x05,0x01,0x00,0x31,0x00,0x51,0x00,
                                        0x81,0x00,0xA1,0x01,0xD0,0x00,0x00,0x00,
                                        0xA4,0xF0,0xB5,0xF0,0xB9,0x20,0xB9,0xD0,
                                        0xBB,0x70,0xBC,0x10,0xBC,0x80,0xBD,0x40,
                                        0xBE,0x20,0xC0,0xB0,0xC3,0x10,0xC5,0x70,
                                        0xC8,0x00,0xCA,0x90,0xCD,0x90,0xD0,0x50,
                                        0x01,0x90,0x1D,0xD0,0x0F,0xF0,0x14,0xD0,
                                        0x4E,0x40,0x6E,0x30,0x5F,0x00,0x4B,0x60,
                                        0x1C,0x50,0x1B,0x00,0x1B,0x00,0x14,0x00,
                                        0x14,0x00,0x13,0x10,0x0E,0x00,0x0E,0x00,
                                        0x40,0x09,0x51,0x0D,0x90,0x0F,0x91,0x0D,
                                        0xB4,0x04,0xC6,0x03,0xC5,0x00,0xD4,0x06,
                                        0x19,0xDD,0xFF,0x4D,0xE4,0xE3,0xF0,0xB6,
                                        0xC5,0xB0,0xB0,0x40,0x40,0x31,0xE0,0xE0,
                                        } ,
        #endif
                                        
                                        
	},
    #endif
};
static struct i2c_board_info enterprise_i2c4_max17058_board_info[] = {
	{
		I2C_BOARD_INFO("max17058", 0x36),
		.irq = TEGRA_GPIO_TO_IRQ(MAX17058_ALERT_GPIO),
		.platform_data = &enterprise_max17058_pdata,
	},
};

static void enterprise_max17058_gpio_init(void)
{
	int ret;

	tegra_gpio_enable(MAX17058_ALERT_GPIO);
	ret = gpio_request(MAX17058_ALERT_GPIO, "battery_alert");
	if (ret < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, ret);
		gpio_free(MAX17058_ALERT_GPIO);
		return;
	}

	ret = gpio_direction_input(MAX17058_ALERT_GPIO);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input failed %d\n", __func__, ret);
		gpio_free(MAX17058_ALERT_GPIO);
		return;
	}
}
static int __init enterprise_max17058_init(void)
{
	enterprise_max17058_gpio_init();

	return i2c_register_board_info(4, enterprise_i2c4_max17058_board_info,
		ARRAY_SIZE(enterprise_i2c4_max17058_board_info));
}
#endif

static const struct i2c_board_info enterprise_i2c0_board_info[] = {
#ifdef CONFIG_SENSORS_TMD2771
	{
		I2C_BOARD_INFO("tmd2771x", 0x39),
		.platform_data = &tmd2771_pdata,
		.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PJ2),
	},
#endif
#ifdef CONFIG_SENSORS_L3G4200D
	{
		I2C_BOARD_INFO(L3G4200D_I2C_NAME, 0x69),
		.platform_data = &l3g4200d_pdata,
	},
#endif
#ifdef CONFIG_SENSORS_LSM330D_G
	{
		I2C_BOARD_INFO("lsm330dlc_gyr", 0x6A),
	},
#endif
#ifdef CONFIG_SENSORS_AK8962
	{
		I2C_BOARD_INFO(AKM8962_I2C_NAME, 0x0C),
		.platform_data = &akm8962_pdata,
		.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PH5),
	},
#endif
};

static struct i2c_board_info enterprise_i2c4_board_info[] = {
	{
		I2C_BOARD_INFO("nct1008", 0x4C),
		.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PH7),
		.platform_data = &enterprise_nct1008_pdata,
	},
#ifdef CONFIG_BATTERY_MAX17058
	{
		I2C_BOARD_INFO("max17058", 0x36),
		.platform_data = &enterprise_max17058_pdata,
	},
#endif
};

struct tegra_sensor_gpios {
	const char *name;
	int gpio;
	bool internal_gpio;
	bool output_dir;
	int init_value;
};

#define TEGRA_SENSOR_GPIO(_name, _gpio, _internal_gpio, _output_dir, _init_value)	\
	{									\
		.name = _name,					\
		.gpio = _gpio,					\
		.internal_gpio = _internal_gpio,		\
		.output_dir = _output_dir,			\
		.init_value = _init_value,			\
	}

static struct tegra_sensor_gpios enterprise_sensor_gpios[] = {
	TEGRA_SENSOR_GPIO("temp_alert", TEGRA_GPIO_PH7, 1, 0, 0),
	TEGRA_SENSOR_GPIO("taos_irq", TEGRA_GPIO_PJ2, 1, 0, 0),
	TEGRA_SENSOR_GPIO("akm_int", TEGRA_GPIO_PH5, 1, 0, 0),
};
static void __init enterprise_gpios_init(void)
{
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(enterprise_sensor_gpios); i++) {

		if (enterprise_sensor_gpios[i].internal_gpio)
			tegra_gpio_enable(enterprise_sensor_gpios[i].gpio);

		ret = gpio_request(enterprise_sensor_gpios[i].gpio,
			enterprise_sensor_gpios[i].name);
		if (ret < 0) {
			pr_err("gpio_request failed for gpio #%d(%s)\n",
				i, enterprise_sensor_gpios[i].name);
			gpio_free(enterprise_sensor_gpios[i].gpio);
			continue;
		}

		if(enterprise_sensor_gpios[i].output_dir){
			gpio_direction_output(enterprise_sensor_gpios[i].gpio,
				enterprise_sensor_gpios[i].init_value);
		}else{
			gpio_direction_input(enterprise_sensor_gpios[i].gpio);
		}

		gpio_export(enterprise_sensor_gpios[i].gpio, false);
	}
}
#define CAM_REAR_PWDN  TEGRA_GPIO_PF2
#define    CAM_REAR_RST  TEGRA_GPIO_PF3
#define    CAM_AVDD_2V8_EN  TEGRA_GPIO_PM3
#define   CAM_VCM_PWR_2V8  TEGRA_GPIO_PM1
#define   CAM_DVDD_1V5_EN TEGRA_GPIO_PM4
#define   CAM_IO_1V8_EN   TEGRA_GPIO_PM2
#define   CAM_FRONT_RST  TEGRA_GPIO_PF4
#define   CAM_FLASH_EN   TEGRA_GPIO_PBB3
#define   CAM_FLASH_TOR  TEGRA_GPIO_PBB6
#define   CAM_FLASH_STROBE  TEGRA_GPIO_PBB7

static struct tegra_sensor_gpios enterprise_cam_gpios[] = {
	
    TEGRA_SENSOR_GPIO( "cam_rear_pwdn", CAM_REAR_PWDN,1,1,0),
    TEGRA_SENSOR_GPIO("cam_rear_rst",CAM_REAR_RST,1,1, 0),
    TEGRA_SENSOR_GPIO( "cam_avdd_2v8_en",CAM_AVDD_2V8_EN,1,1, 0),
    TEGRA_SENSOR_GPIO( "cam_vcm_pwr_2v8", CAM_VCM_PWR_2V8,1,1,0),
    TEGRA_SENSOR_GPIO( "cam_dvdd_1v5_en",CAM_DVDD_1V5_EN,1,1, 0),
    TEGRA_SENSOR_GPIO( "cam_io_1v8_en", CAM_IO_1V8_EN,1,1,0),
    TEGRA_SENSOR_GPIO("cam_front_rst",CAM_FRONT_RST,1,1, 0), // modify by yaoling for power
    TEGRA_SENSOR_GPIO( "cam_flash_en",CAM_FLASH_EN,1,1, 0),
    TEGRA_SENSOR_GPIO( "cam_flash_tor",CAM_FLASH_TOR,1,1, 1),
    TEGRA_SENSOR_GPIO( "cam_flash_strobe", CAM_FLASH_STROBE,1,1,0),
};
 #ifdef  CONFIG_VIDEO_OV5640
 static int enterprise_ov5640_power_on(void)
{
    printk("enterprise_ov5640_power_on\n");

    gpio_set_value(CAM_FRONT_RST, 1);
    gpio_direction_output(CAM_IO_1V8_EN, 1);
    gpio_direction_output(CAM_AVDD_2V8_EN, 1);
    gpio_direction_output(CAM_VCM_PWR_2V8,1);
    mdelay(5); 
    gpio_set_value(CAM_REAR_PWDN,0);

    mdelay(1);  
    gpio_set_value(CAM_REAR_RST, 1); // can try to del this code 
    mdelay(1);
    gpio_set_value(CAM_REAR_RST, 0);
    mdelay(2);
    gpio_set_value(CAM_REAR_RST, 1);

    mdelay(20);

    return 0;


   
}

static int enterprise_ov5640_power_off(void)
{
        printk("enterprise_ov8820_power_off\n"); 
        gpio_set_value(CAM_REAR_PWDN, 1);
        //gpio_set_value(CAM_REAR_RST, 0);
        gpio_direction_output(CAM_VCM_PWR_2V8,0);
        gpio_direction_output(CAM_AVDD_2V8_EN, 0);
        gpio_direction_output(CAM_IO_1V8_EN, 0);
        return 0;
}
struct ov5640_platform_data enterprise_ov5640_data = {
	.power_on = enterprise_ov5640_power_on,
	.power_off = enterprise_ov5640_power_off,
};
#endif
#ifdef CONFIG_VIDEO_OV7692
static int enterprise_ov7692_power_on(void)
{
        pr_err("enterprise_mt9v114_power_on\n");
        gpio_set_value(CAM_REAR_PWDN, 0);
        gpio_set_value(CAM_REAR_RST, 0);

        gpio_direction_output(CAM_IO_1V8_EN, 1);
        gpio_direction_output(CAM_AVDD_2V8_EN, 1);
      //  gpio_set_value(CAM_FRONT_RST, 1);
     //   udelay(2);
        gpio_set_value(CAM_FRONT_RST, 0);
        udelay(2);
     //   gpio_set_value(CAM_FRONT_RST, 1);
        return 0;

}

static int enterprise_ov7692_power_off(void)
{
        gpio_set_value(CAM_FRONT_RST, 1);
        gpio_direction_output(CAM_AVDD_2V8_EN, 0);
        gpio_direction_output(CAM_IO_1V8_EN, 0);
        return 0;
}


struct ov7692_platform_data enterprise_ov7692_data = {
	.power_on = enterprise_ov7692_power_on,
	.power_off = enterprise_ov7692_power_off,
};
    
#endif
#ifdef CONFIG_VIDEO_OV8825
static int enterprise_ov8825_power_on(void)
{
    printk("enterprise_ov8825_power_on\n");

    gpio_set_value(CAM_FRONT_RST, 0);
    gpio_direction_output(CAM_IO_1V8_EN, 1);
    gpio_direction_output(CAM_AVDD_2V8_EN, 1);
    gpio_direction_output(CAM_DVDD_1V5_EN,1);
    gpio_direction_output(CAM_VCM_PWR_2V8,1);
    mdelay(5); 
    gpio_set_value(CAM_REAR_PWDN,1);

    mdelay(1);  
    gpio_set_value(CAM_REAR_RST, 1); // can try to del this code 
    mdelay(1);
    gpio_set_value(CAM_REAR_RST, 0);
    mdelay(2);
    gpio_set_value(CAM_REAR_RST, 1);

    mdelay(20);

    return 0;


   
}

static int enterprise_ov8825_power_off(void)
{
        printk("enterprise_ov8820_power_off\n"); 
        gpio_set_value(CAM_REAR_PWDN, 0);
        gpio_set_value(CAM_REAR_RST, 0);
        gpio_direction_output(CAM_VCM_PWR_2V8,0);
        gpio_direction_output(CAM_DVDD_1V5_EN,0);
        gpio_direction_output(CAM_AVDD_2V8_EN, 0);
        gpio_direction_output(CAM_IO_1V8_EN, 0);
        return 0;
}
struct ov8825_platform_data enterprise_ov8825_data = {
	.power_on = enterprise_ov8825_power_on,
	.power_off = enterprise_ov8825_power_off,
};
static struct platform_device ov8825_device = {
        .name = "ov8825",
        .id = -1,
        /* 0503 */
        .dev ={
        .platform_data = &enterprise_ov8825_data,
            },
};
struct ov8825_focuser_platform_data enterprise_ov8825_focuser_pdata = {
	.num		= 1,
	.sync		= 2,
	.dev_name	= "focuser",
	/* 0503 */
	.power_on= enterprise_ov8825_power_on,
	.power_off=enterprise_ov8825_power_off,
};

static struct platform_device ov8825_focuser_device = {
        .name = "ov8825_focuser",
        .id = -1,
        .dev = {
		    .platform_data = &enterprise_ov8825_focuser_pdata,
	    },
};
#endif
#ifdef CONFIG_VIDEO_MT9M114
static int enterprise_mt9m114_power_on(void)
{
        pr_err("enterprise_mt9v114_power_on\n");
        gpio_set_value(CAM_REAR_PWDN, 0);
        gpio_set_value(CAM_REAR_RST, 0);
       /* tegra_gpio_enable(CAM_FRONT_RST);
        gpio_free(CAM_FRONT_RST);
        gpio_request(CAM_FRONT_RST, "cam_front_rst");
        gpio_direction_output(CAM_FRONT_RST, 1);
        gpio_export(CAM_FRONT_RST, false); */
      
       // mdelay(10);
	gpio_direction_output(CAM_IO_1V8_EN, 1);
	gpio_direction_output(CAM_AVDD_2V8_EN, 1);
	gpio_set_value(CAM_FRONT_RST, 1);
	mdelay(2);
	gpio_set_value(CAM_FRONT_RST, 0);
	mdelay(2);
	gpio_set_value(CAM_FRONT_RST, 1);
	mdelay(50);
	return 0;

}

static int enterprise_mt9m114_power_off(void)
{
     
       // gpio_set_value(CAM_FRONT_RST, 1);
	 gpio_set_value(CAM_FRONT_RST, 0);  // modify by yaoling for power 
        gpio_direction_output(CAM_AVDD_2V8_EN, 0);
        gpio_direction_output(CAM_IO_1V8_EN, 0);
        return 0;
}


struct mt9m114_platform_data enterprise_mt9m114_data = {
	.power_on = enterprise_mt9m114_power_on,
	.power_off = enterprise_mt9m114_power_off,
};
#endif
static int enterprise_adp1650_init(void)
{
    gpio_set_value(CAM_FLASH_EN, 0);
    mdelay(5);
    gpio_set_value(CAM_FLASH_EN, 1);
    return 0;
}

static void enterprise_adp1650_exit(void)
{
    gpio_set_value(CAM_FLASH_EN, 0);
    return 0;
     
}

static int enterprise_adp1650_gpio_strb(int val)
{
        gpio_set_value(CAM_FLASH_STROBE, val);
	return 0;
};

static int enterprise_adp1650_gpio_tor(int val)
{
        gpio_set_value(CAM_FLASH_TOR, val); 
	return 0;
};
#define CAMERA_FLASH_OP_MODE		1//0 /*0=I2C mode, 1=GPIO mode*/
#define CAMERA_FLASH_MAX_LED_AMP	7
#define CAMERA_FLASH_MAX_TORCH_AMP	11
#define CAMERA_FLASH_MAX_FLASH_AMP	31
static struct adp1650_platform_data enterprise_adp1650_data = {
        .config		= CAMERA_FLASH_OP_MODE,
        .max_amp_indic	= CAMERA_FLASH_MAX_LED_AMP,
        .max_amp_torch	= CAMERA_FLASH_MAX_TORCH_AMP,
        .max_amp_flash	= CAMERA_FLASH_MAX_FLASH_AMP,
        .init = enterprise_adp1650_init,
        .exit = enterprise_adp1650_exit,
        .gpio_en	= NULL,
        .gpio_strb	= enterprise_adp1650_gpio_strb,
        .gpio_tor  =enterprise_adp1650_gpio_tor,

};
static const struct i2c_board_info enterprise_i2c2_boardinfo[] = {
        #ifdef  CONFIG_VIDEO_OV5640
        { 
            I2C_BOARD_INFO("ov5640", 0x78>>1), 
            .platform_data = &enterprise_ov5640_data,
        },
        #endif 
        #ifdef CONFIG_VIDEO_OV7692
        {
            I2C_BOARD_INFO("ov7692", 0x3f),
            .platform_data = &enterprise_ov7692_data,
        },
        #endif
        #ifdef CONFIG_VIDEO_OV8825
        {
            I2C_BOARD_INFO("ov8825-i2c", 0x36),
            .platform_data = &enterprise_ov8825_data,
        },
        #endif 
        #ifdef CONFIG_VIDEO_MT9M114
        {
            I2C_BOARD_INFO("mt9m114", 0x90>>1),
            .platform_data = &enterprise_mt9m114_data,
        },
        #endif
        {
            I2C_BOARD_INFO("adp1650", 0x60>>1),
            .platform_data = &enterprise_adp1650_data,
        },
        
};

static int __init enterprise_cam_init(void)
{
       int ret;
	int i;
	struct board_info bi;
	struct board_info cam_bi;
	bool i2c_mux = false;

	printk("%s:++\n", __func__);
	
     
        for (i = 0; i < ARRAY_SIZE(enterprise_cam_gpios); i++) {

          //  gpio_free(enterprise_cam_gpios[i].gpio);
         //   tegra_gpio_enable(enterprise_cam_gpios[i].gpio);

            ret = gpio_request(enterprise_cam_gpios[i].gpio,
            enterprise_cam_gpios[i].name);
            if (ret < 0) {
            pr_err("gpio_request failed for gpio #%d(%s)\n",
            i, enterprise_cam_gpios[i].name);
            goto fail_free_gpio;
            }
            gpio_direction_output(enterprise_cam_gpios[i].gpio,
            enterprise_cam_gpios[i].init_value);

            gpio_export(enterprise_cam_gpios[i].gpio, false);
             tegra_gpio_enable(enterprise_cam_gpios[i].gpio);
        }   
            i2c_register_board_info(2, enterprise_i2c2_boardinfo,
            ARRAY_SIZE(enterprise_i2c2_boardinfo));
            #ifdef CONFIG_VIDEO_OV8825
            platform_device_register(&ov8825_device);
            platform_device_register(&ov8825_focuser_device);
            #endif
            printk("platform_device_register\n");

	
	return 0;

      fail_free_gpio:
	pr_err("%s enterprise_cam_init failed!\n", __func__);
	while (i--)
		gpio_free(enterprise_cam_gpios[i].gpio);
        return ret;
        //return 0;
}

int __init enterprise_sensors_init(void)
{
	enterprise_gpios_init();

	i2c_register_board_info(0, enterprise_i2c0_board_info,
					ARRAY_SIZE(enterprise_i2c0_board_info));

/*ZTE: added by li.liguo for Sii8334 20120529*/
 #ifdef CONFIG_MHL_SII8334
  
	enterprise_mhl_init();

	i2c_register_board_info(0, Sii8334_i2c0_board_info,
  			ARRAY_SIZE(Sii8334_i2c0_board_info));
 
  #endif
/*ZTE: added by li.liguo for Sii8334  20120529*/

 #ifdef CONFIG_ACCEL_SENSORS
    platform_device_register(&accel_platform_device);
#endif

#ifdef CONFIG_BATTERY_MAX17058
    enterprise_max17058_init();
#endif

	i2c_register_board_info(4, enterprise_i2c4_board_info,
					ARRAY_SIZE(enterprise_i2c4_board_info));

	enterprise_cam_init();

	return 0;
}

