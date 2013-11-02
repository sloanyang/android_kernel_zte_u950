/*
 * arch/arm/mach-tegra/board-enterprise.c
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/serial_8250.h>
#include <linux/i2c.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/spi/spi.h>
#include <linux/spi-tegra.h>
#include <linux/tegra_uart.h>
#include <linux/fsl_devices.h>
#include <linux/i2c/atmel_mxt_ts.h>
#include <linux/memblock.h>

#ifdef CONFIG_TOUCHSCREEN_FT5X06
#include <linux/input/ft5x06_ts.h>    /*ZTE: added by tong.weili for Ft5X06 Touch 20120613 */
#endif

#include <linux/nfc/pn544.h>
#if defined (CONFIG_SND_SOC_MAX98095)
#include <sound/max98095.h>
#include <linux/spi-tegra.h>
#endif
#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/io_dpd.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/usb_phy.h>
#include <mach/i2s.h>
#include <mach/tegra_asoc_pdata.h>
#include <mach/thermal.h>
#include <mach/tegra-bb-power.h>
#include "board.h"
#include "clock.h"
#include "board-enterprise.h"
#include "baseband-xmm-power.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"
#include "pm.h"
#include "zte_board_ext_func.h" // ZTE£ºadded by pengtao
#include <linux/proc_fs.h>
//#include <linux/syscalls.h>

//modify lupeng
bool tps80031_init_flag = false;

static struct balanced_throttle throttle_list[] = {
	{
		.id = BALANCED_THROTTLE_ID_TJ,
		.throt_tab_size = 10,
		.throt_tab = {
			{      0, 1000 },
			{ 640000, 1000 },
			{ 640000, 1000 },
			{ 640000, 1000 },
			{ 640000, 1000 },
			{ 640000, 1000 },
			{ 760000, 1000 },
			{ 760000, 1050 },
			{1000000, 1050 },
			{1000000, 1100 },
		},
	},
#ifdef CONFIG_TEGRA_SKIN_THROTTLE
	{
		.id = BALANCED_THROTTLE_ID_SKIN,
		.throt_tab_size = 6,
		.throt_tab = {
			{ 640000, 1200 },
			{ 640000, 1200 },
			{ 760000, 1200 },
			{ 760000, 1200 },
			{1000000, 1200 },
			{1000000, 1200 },
		},
	},
#endif
};

/* All units are in millicelsius */
static struct tegra_thermal_data thermal_data = {
	.shutdown_device_id = THERMAL_DEVICE_ID_NCT_EXT,
	.temp_shutdown = 90000,
#if defined(CONFIG_TEGRA_EDP_LIMITS) || defined(CONFIG_TEGRA_THERMAL_THROTTLE)
	.throttle_edp_device_id = THERMAL_DEVICE_ID_NCT_EXT,
#endif
#ifdef CONFIG_TEGRA_EDP_LIMITS
	.edp_offset = TDIODE_OFFSET,  /* edp based on tdiode */
	.hysteresis_edp = 3000,
#endif
#ifdef CONFIG_TEGRA_THERMAL_THROTTLE
	#ifdef CONFIG_PROJECT_U950
		#ifdef CONFIG_TEGRA_SKIN_THROTTLE
		.temp_throttle = 85000,
		#else
		.temp_throttle = 57000,
		#endif
	#endif

	#ifdef CONFIG_PROJECT_V985
	.temp_throttle = 58000,
	#endif

	#ifdef CONFIG_PROJECT_U985
		#ifdef CONFIG_TEGRA_SKIN_THROTTLE
		.temp_throttle = 85000,
		#else
		.temp_throttle = 58000,
		#endif	
	#endif

	.tc1 = 0,
	.tc2 = 1,
	.passive_delay = 2000,
#endif
#ifdef CONFIG_TEGRA_SKIN_THROTTLE
        #ifdef CONFIG_PROJECT_U985
        .skin_device_id = THERMAL_DEVICE_ID_SKIN,
        .temp_throttle_skin = 35000,
        .tc1_skin = 0,
        .tc2_skin = 1,
        .passive_delay_skin = 5000,

        .skin_temp_offset = -347,
        .skin_period = 1100,
        .skin_devs_size = 3,
        .skin_devs = {
                {
			   //NV For U985
                        THERMAL_DEVICE_ID_BATT,
                        {
                                -2, -2, -2, -2,
                                -2, -2, -2, -2,
                                -1, -1, 0, 1,
                                1, 1, 1, 1,
                                1, 1, 1, 3
                        }
	         },
                {
			//NV For U985
                        THERMAL_DEVICE_ID_NCT_EXT,
                        {
                                10, 1, -2, -2,
                                -2, -2, -2, -2,
                                -1, -1, -2, -2,
                                -2, -1, -1, -3,
                                -3, -4, -7, -23
                        }
                },
                {
			//NV For U985
                        THERMAL_DEVICE_ID_NCT_INT,
                        {
                                10, 8, 7, 5,
                                5, 5, 5, 4,
                                4, 4, 5, 5,
                                6, 7, 8, 9,
                                10, 12, 13, 16
                        }
                },
        },
        #endif
		
        #ifdef CONFIG_PROJECT_U950
        .skin_device_id = THERMAL_DEVICE_ID_SKIN,
        .temp_throttle_skin = 35000,
        .tc1_skin = 0,
        .tc2_skin = 1,
        .passive_delay_skin = 5000,

        .skin_temp_offset = 8121,
        .skin_period = 1100,
        .skin_devs_size = 3,
        .skin_devs = {
                {
					//NV For U985
                        THERMAL_DEVICE_ID_NCT_INT,
                        {
                                10, 6, 4, 3,
                                4, 3, 4, 4,
                                4, 3, 3, 3,
                                3, 4, 3, 3,
                                3, 2, 1, -1
                        }
	         },
                {
			//NV For U985
                        THERMAL_DEVICE_ID_NCT_EXT,
                        {
                                2, 0, -1, -2,
                                -2, -2, -2, -2,
                                -2, -2, -2, -1,
                                -2, -3, -3, -2,
                                -1, -2, -3, -14
                        }
                },
                {
			//NV For U985
                        THERMAL_DEVICE_ID_BATT,
                        {
                                1, 1, 1, 1,
                                2, 2, 1, 2,
                                2, 2, 3, 3,
                                3, 3, 4, 5,
                                5, 5, 7, 9
                        }
                },
        },
        #endif

#endif

};

#if 1
// jprimero: these two battery functions should be replaced by the actual
// battery driver and should avoid reading the battery tempeature through sysfs
static int batt_get_temp(void *dev_data, long *temp)
{
    int ret = 0;
    //fd, count;
    //char buf[64];
    //mm_segment_t old_fs;
    long btemp;

    #if 0
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    fd = sys_open("/sys/class/power_supply/battery/temp", O_RDONLY, 0644);
    
    if (fd >= 0) {
		count = sys_read(fd, buf, 64);
		if (count < 64)
			buf[count]='\0';
		ret = kstrol(buf, 10, &btemp);
		sys_close(fd);

		*temp = btemp*100; // jprimero: return temperature in millicelsius
		} else {
		    ret = -1;
		}
		
    set_fs(old_fs);
    #else
    #if 1
    //printk("*****btemp = %d***tps80031_init_flag = %d***\n",btemp,tps80031_init_flag);
    if(tps80031_init_flag)
    {
         //printk("****tps80031_get_battery_temp****\n");
         btemp = tps80031_get_battery_temp();   
    }
    else
    {
         ret = 0;
	  //printk("****228****\n");
	  	 *temp = -50000;
         return ret; 
    }
    
    if(btemp < 0)
    {
         ret = -16;
    }
    else
    {
         *temp = btemp*100;
	  //printk("******temp = %d******\n",btemp*100);
    }
    #endif
	#endif
    //printk("****241****\n");
	
    return ret;
}

static void batt_init()
{
    struct tegra_thermal_device *batt_device;

    printk("****batt_init******\n");
	
    batt_device = kzalloc(sizeof(struct tegra_thermal_device),
	GFP_KERNEL);
	
    if (!batt_device) {
	pr_err("unable to allocate thermal device\n");
	return;
    }
	
    batt_device->name = "batt_dev";
    batt_device->id = THERMAL_DEVICE_ID_BATT;
    batt_device->get_temp = batt_get_temp;
	
    printk("******batt_device********\n");
    tegra_thermal_device_register(batt_device);
}
#endif

static struct resource enterprise_bcm4329_rfkill_resources[] = {
	{
		.name   = "bcm4329_nshutdown_gpio",
		.start  = TEGRA_GPIO_PE6,
		.end    = TEGRA_GPIO_PE6,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device enterprise_bcm4329_rfkill_device = {
	.name = "bcm4329_rfkill",
	.id		= -1,
	.num_resources  = ARRAY_SIZE(enterprise_bcm4329_rfkill_resources),
	.resource       = enterprise_bcm4329_rfkill_resources,
};

static struct resource enterprise_bluesleep_resources[] = {
	[0] = {
		.name = "gpio_host_wake",
			.start  = TEGRA_GPIO_PS2,
			.end    = TEGRA_GPIO_PS2,
			.flags  = IORESOURCE_IO,
	},
	[1] = {
		.name = "gpio_ext_wake",
			.start  = TEGRA_GPIO_PE7,
			.end    = TEGRA_GPIO_PE7,
			.flags  = IORESOURCE_IO,
	},
	[2] = {
		.name = "host_wake",
			.start  = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PS2),
			.end    = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PS2),
			.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
};

static struct platform_device enterprise_bluesleep_device = {
	.name           = "bluesleep",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(enterprise_bluesleep_resources),
	.resource       = enterprise_bluesleep_resources,
};

static void __init enterprise_setup_bluesleep(void)
{
	platform_device_register(&enterprise_bluesleep_device);
	tegra_gpio_enable(TEGRA_GPIO_PS2);
	tegra_gpio_enable(TEGRA_GPIO_PE7);
	return;
}

static __initdata struct tegra_clk_init_table enterprise_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "pll_m",	NULL,		0,		false},
	{ "hda",	"pll_p",	108000000,	false},
	{ "hda2codec_2x","pll_p",	48000000,	false},
	{ "pwm",	"clk_32k",	32768,		false},
	{ "blink",	"clk_32k",	32768,		true},
	{ "i2s0",	"pll_a_out0",	0,		false},
	{ "i2s1",	"pll_a_out0",	0,		false},
	{ "i2s2",	"pll_a_out0",	0,		false},
	{ "i2s3",	"pll_a_out0",	0,		false},
	{ "spdif_out",	"pll_a_out0",	0,		false},
	{ "d_audio",	"clk_m",	12000000,	false},
	{ "dam0",	"clk_m",	12000000,	false},
	{ "dam1",	"clk_m",	12000000,	false},
	{ "dam2",	"clk_m",	12000000,	false},
	{ "audio0",	"i2s0_sync",	0,		false},
	{ "audio1",	"i2s1_sync",	0,		false},
	{ "audio2",	"i2s2_sync",	0,		false},
	{ "audio3",	"i2s3_sync",	0,		false},
	{ "vi",		"pll_p",	0,		false},
	{ "vi_sensor",	"pll_p",	0,		false},
	{ NULL,		NULL,		0,		0},
};

static struct tegra_i2c_platform_data enterprise_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PC4, 0},
	.sda_gpio		= {TEGRA_GPIO_PC5, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data enterprise_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
	.is_clkon_always = true,
	.scl_gpio		= {TEGRA_GPIO_PT5, 0},
	.sda_gpio		= {TEGRA_GPIO_PT6, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data enterprise_i2c3_platform_data = {
	.adapter_nr	= 2,
	.bus_count	= 1,
	.bus_clk_rate	= { 271000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PBB1, 0},
	.sda_gpio		= {TEGRA_GPIO_PBB2, 0},
	.arb_recovery = arb_lost_recovery,
};

#ifdef CONFIG_MHL_SII8334
static struct tegra_i2c_platform_data enterprise_i2c4_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 65000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PV4, 0},
	.sda_gpio		= {TEGRA_GPIO_PV5, 0},
	.arb_recovery = arb_lost_recovery,
};
#else
static struct tegra_i2c_platform_data enterprise_i2c4_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 10000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PV4, 0},
	.sda_gpio		= {TEGRA_GPIO_PV5, 0},
	.arb_recovery = arb_lost_recovery,
};
#endif

static struct tegra_i2c_platform_data enterprise_i2c5_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PZ6, 0},
	.sda_gpio		= {TEGRA_GPIO_PZ7, 0},
	.arb_recovery = arb_lost_recovery,
};

#ifdef CONFIG_SND_SOC_MAX98095
/*ZTE: added for CODEC, Begin*/
/* Equalizer filter coefs generated from the MAXIM MAX98095
 * Evaluation Kit (EVKIT) software tool */
static struct max98095_eq_cfg eq_cfg[] = {
	{ /* Flat response */
	.name = "SPK_MUSIC",
	.rate = 48000,
	.band1 = {0x268B, 0xC008, 0x3F08, 0x01EB, 0x0B16},
	.band2 = {0x6601, 0xC5C2, 0x3506, 0x1A87, 0x23D6},
	.band3 = {0x0A50, 0xC35F, 0x2146, 0x147E, 0x36AB},
	.band4 = {0x7FFE, 0xD606, 0x1E77, 0x304F, 0x3848},
	.band5 = {0x2594, 0xC01D, 0x3E37, 0x03C2, 0x0F02},
	},
	{ /* Low pass Fc=1KHz */
	.name = "HP_MUSIC",
	.rate = 48000,
	.band1 = {0x2997, 0xC002, 0x3F7E, 0x00E3, 0x0804},
	.band2 = {0x2405, 0xC009, 0x3F1D, 0x0218, 0x0A9D},
	.band3 = {0x2045, 0xC06B, 0x3F1A, 0x0745, 0x0AAA},
	.band4 = {0x2638, 0xC3AC, 0x32FF, 0x155D, 0x26AB},
	.band5 = {0x293E, 0xF89B, 0x0DE2, 0x3F92, 0x3E79},
	},
	{ /* BASS=-12dB, TREBLE=+9dB, Fc=5KHz */
		.name = "HIBOOST",
		.rate = 44100,
		.band1 = {0x0815, 0xC001, 0x3AA4, 0x0003, 0x19A2},
		.band2 = {0x0815, 0xC103, 0x092F, 0x0B55, 0x3F56},
		.band3 = {0x0E0A, 0xC306, 0x1E5C, 0x136E, 0x3856},
		.band4 = {0x2459, 0xF665, 0x0CAA, 0x3F46, 0x3EBB},
		.band5 = {0x5BBB, 0x3FFF, 0xCEB0, 0x0000, 0x28CA},
	},
	{ /* BASS=12dB, TREBLE=+12dB */
		.name = "LOUD12DB",
		.rate = 44100,
		.band1 = {0x7FC1, 0xC001, 0x3EE8, 0x0020, 0x0BC7},
		.band2 = {0x51E9, 0xC016, 0x3C7C, 0x033F, 0x14E9},
		.band3 = {0x1745, 0xC12C, 0x1680, 0x0C2F, 0x3BE9},
		.band4 = {0x4536, 0xD7E2, 0x0ED4, 0x31DD, 0x3E42},
		.band5 = {0x7FEF, 0x3FFF, 0x0BAB, 0x0000, 0x3EED},
	},
	{
		.name = "FLAT",
		.rate = 16000,
		.band1 = {0x2000, 0xC004, 0x4000, 0x0141, 0x0000},
		.band2 = {0x2000, 0xC033, 0x4000, 0x0505, 0x0000},
		.band3 = {0x2000, 0xC268, 0x4000, 0x115F, 0x0000},
		.band4 = {0x2000, 0xDA62, 0x4000, 0x33C6, 0x0000},
		.band5 = {0x2000, 0x4000, 0x4000, 0x0000, 0x0000},
	},
	{
		.name = "LOWPASS1K",
		.rate = 16000,
		.band1 = {0x2000, 0xC004, 0x4000, 0x0141, 0x0000},
		.band2 = {0x5BE8, 0xC3E0, 0x3307, 0x15ED, 0x26A0},
		.band3 = {0x0F71, 0xD15A, 0x08B3, 0x2BD0, 0x3F67},
		.band4 = {0x0815, 0x3FFF, 0xCF78, 0x0000, 0x29B7},
		.band5 = {0x0815, 0x3FFF, 0xCF78, 0x0000, 0x29B7},
	},
	{ /* BASS=-12dB, TREBLE=+9dB, Fc=2KHz */
		.name = "HIBOOST",
		.rate = 16000,
		.band1 = {0x0815, 0xC001, 0x3BD2, 0x0009, 0x16BF},
		.band2 = {0x080E, 0xC17E, 0xF653, 0x0DBD, 0x3F43},
		.band3 = {0x0F80, 0xDF45, 0xEE33, 0x36FE, 0x3D79},
		.band4 = {0x590B, 0x3FF0, 0xE882, 0x02BD, 0x3B87},
		.band5 = {0x4C87, 0xF3D0, 0x063F, 0x3ED4, 0x3FB1},
	},
	{ /* BASS=12dB, TREBLE=+12dB */
		.name = "LOUD12DB",
		.rate = 16000,
		.band1 = {0x7FC1, 0xC001, 0x3D07, 0x0058, 0x1344},
		.band2 = {0x2DA6, 0xC013, 0x3CF1, 0x02FF, 0x138B},
		.band3 = {0x18F1, 0xC08E, 0x244D, 0x0863, 0x34B5},
		.band4 = {0x2BE0, 0xF385, 0x04FD, 0x3EC5, 0x3FCE},
		.band5 = {0x7FEF, 0x4000, 0x0BAB, 0x0000, 0x3EED},
	},
};

static struct max98095_biquad_cfg bq_cfg[] = {
	{
	.name = "LP4K",
	.rate = 44100,
	.band1 = {0x5019, 0xe0de, 0x03c2, 0x0784, 0x03c2},
	.band2 = {0x5013, 0xe0e5, 0x03c1, 0x0783, 0x03c1},
	},
	{
	.name = "HP4K",
	.rate = 44100,
	.band1 = {0x5019, 0xe0de, 0x2e4b, 0xa36a, 0x2e4b},
	.band2 = {0x5013, 0xe0e5, 0x2e47, 0xa371, 0x2e47},
	},
};

static struct max98095_pdata enterprise_max98095_pdata = {
	/* equalizer configuration */
	.eq_cfg = eq_cfg,
	.eq_cfgcnt = ARRAY_SIZE(eq_cfg), 

	/* biquad filter configuration */
	.bq_cfg = bq_cfg,
	.bq_cfgcnt = ARRAY_SIZE(bq_cfg), 

	/* microphone configuration */
	.digmic_left_mode = 0,  /* 0 = normal analog mic */
	.digmic_right_mode = 0, /* 0 = normal analog mic */
};

static struct i2c_board_info __initdata max98095_board_info = {
	I2C_BOARD_INFO("max98095", 0x10),
	.platform_data = &enterprise_max98095_pdata,
	.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_CDC_IRQ),
};

static void audio_codec_enbale(bool enable)
{
    gpio_request(CODEC_CEN_PIN, "CODEC_ENABLE");	
    if (enable)
    {
        gpio_direction_output(CODEC_CEN_PIN, 1);
    }
    else
    {
        gpio_direction_output(CODEC_CEN_PIN, 0);
    }
    tegra_gpio_enable(CODEC_CEN_PIN);
}

static struct spi_clk_parent spi_codec_parent_clk[] = {
        [0] = {.name = "pll_p"},        
};

static struct tegra_spi_platform_data spi_codec_pdata = {
        .is_dma_based           = true,
        .max_dma_buffer         = (16 * 1024),
        .is_clkon_always        = false,
        .max_rate               = 5000000,
};

static struct tegra_spi_device_controller_data codec_spi_control_info = {
	.is_hw_based_cs = false,
	.cs_setup_clk_count = 0,
	.cs_hold_clk_count = 0,
};

static struct spi_board_info codec_spi_board_info[] = {
	/* spi master */
	{
		.modalias = "max98095",
	       #ifdef CONFIG_PROJECT_V985
		.bus_num = 0,
		.chip_select = 0,
		#else
		.bus_num = 4,
		.chip_select = 2,
		#endif
		.mode = SPI_MODE_1,
		.max_speed_hz = 16000000,
		.platform_data = &enterprise_max98095_pdata,
		.controller_data = &codec_spi_control_info,
		.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_CDC_IRQ),
	},
};

static void codec_spi_bus_init(void)
{
    int err;
    int i;
    struct clk *c;

    for (i = 0; i < ARRAY_SIZE(spi_codec_parent_clk); ++i) {
		c = tegra_get_clock_by_name(spi_codec_parent_clk[i].name);
              if (IS_ERR_OR_NULL(c)) {
                   pr_err("Not able to get the clock for %s\n",
                                                spi_codec_parent_clk[i].name);
                        continue;
                }
                spi_codec_parent_clk[i].parent_clk = c;
                spi_codec_parent_clk[i].fixed_clk_rate = clk_get_rate(c);
   }
   spi_codec_pdata.parent_clk_list = spi_codec_parent_clk;
   spi_codec_pdata.parent_clk_count = ARRAY_SIZE(spi_codec_parent_clk);
   #ifdef CONFIG_PROJECT_V985
   tegra_spi_device1.dev.platform_data = &spi_codec_pdata;
   platform_device_register(&tegra_spi_device1);
   #else
   tegra_spi_device5.dev.platform_data = &spi_codec_pdata;
   platform_device_register(&tegra_spi_device5);
   #endif
    err = spi_register_board_info(codec_spi_board_info,
    		                    ARRAY_SIZE(codec_spi_board_info));
    if (err < 0)
    {
        pr_err("%s: spi_register_board returned error %d\n",__func__, err);
    }
    printk("[codec] codec_spi_bus_init end\n");
}
#endif

#ifdef CONFIG_PROJECT_V985
static struct pn544_i2c_platform_data nfc_pdata = {
		.irq_gpio = TEGRA_GPIO_PZ3,
		.ven_gpio = TEGRA_GPIO_PB3,
		.firm_gpio = TEGRA_GPIO_PJ3,
};

static struct i2c_board_info __initdata nfc_board_info = {
	I2C_BOARD_INFO("pn544", 0x28),
	.platform_data = &nfc_pdata,
	.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PZ3),
};
#else
static struct pn544_i2c_platform_data nfc_pdata = {
		.irq_gpio = TEGRA_GPIO_PS4,
		.ven_gpio = TEGRA_GPIO_PM6,
		.firm_gpio = 0,
};

static struct i2c_board_info __initdata nfc_board_info = {
	I2C_BOARD_INFO("pn544", 0x28),
	.platform_data = &nfc_pdata,
	.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PS4),
};
#endif

static void enterprise_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &enterprise_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &enterprise_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &enterprise_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &enterprise_i2c4_platform_data;
	tegra_i2c_device5.dev.platform_data = &enterprise_i2c5_platform_data;

	platform_device_register(&tegra_i2c_device5);
	platform_device_register(&tegra_i2c_device4);
	platform_device_register(&tegra_i2c_device3);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device1);

       i2c_register_board_info(0, &nfc_board_info, 1);
       
    #ifdef CONFIG_PROJECT_U985
	#ifdef CONFIG_SND_SOC_MAX98095
	if (0 == zte_get_board_id()){
	    i2c_register_board_info(0, &max98095_board_info, 1);
	}
	#endif
    #endif
}

static struct platform_device *enterprise_uart_devices[] __initdata = {
	&tegra_uarta_device,
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
	&tegra_uarte_device,
};

static struct uart_clk_parent uart_parent_clk[] = {
	[0] = {.name = "clk_m"},
	[1] = {.name = "pll_p"},
#ifndef CONFIG_TEGRA_PLLM_RESTRICTED
	[2] = {.name = "pll_m"},
#endif
};
static struct tegra_uart_platform_data enterprise_uart_pdata;
static struct tegra_uart_platform_data enterprise_loopback_uart_pdata;
#ifndef CONFIG_MODEM_ICERA_E450
static struct tegra_uart_platform_data modem_uart_pdata;

static void wake_modem(struct uart_port *port)
{
    int value = 1;
    int retry;
    
    //printk(KERN_DEBUG "%s: wakeup modem start\n", dev_name(port->dev));
    gpio_set_value(AP_TO_MODEM_WKUP, 1);
    gpio_set_value(AP_TO_MODEM_WKUP, 0);

    //totally wait for 100ms
    retry = 10000;
    while(retry){
        value = gpio_get_value(MODEM_TO_AP_SLP);
        if (0 > value) {
            printk(KERN_ERR "%s: failed to get modem sleep status %d\n", 
                    dev_name(port->dev), value);
            return;
        }
        
        if (0 == value){
            //printk(KERN_DEBUG "%s: wakeup modem succ, retry left %d\n", dev_name(port->dev), retry);
            break;
        }
        udelay(10);
        retry--;
    }
    
    gpio_set_value(AP_TO_MODEM_WKUP, 1);

    if (0 == retry && 1 == value)
        printk(KERN_ERR "%s: wakeup modem failed\n", dev_name(port->dev));  
}

static void modem_sleep_control(struct uart_port *port, int val, int step)
{
    if (1 == val)
    {
    	if (step == 0) {
    		pr_info("%s: set ap sleep state 1\n", __func__);
        	gpio_set_value(AP_TO_MODEM_SLP, 1);
	    } else if (step == 1) {
    		//1Add disable code if use uart level-transform chip
    	}
    }
    else if (0 == val)
    {
    	if (step == 0) {
		//1Add enable code if use uart level-transform chip
	    } else if (step == 1) {
    		pr_info("%s: set ap sleep state 0\n", __func__);
        	gpio_set_value(AP_TO_MODEM_SLP, 0);
    	}
    }
    return;
}
#endif

static void __init uart_debug_init(void)
{
	unsigned long rate;
	struct clk *c;

	/* UARTD is the debug port. */
	pr_info("Selecting UARTD as the debug console\n");
	enterprise_uart_devices[3] = &debug_uartd_device;
	debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartd_device.dev.platform_data))->mapbase;
	debug_uart_clk = clk_get_sys("serial8250.0", "uartd");

	/* Clock enable for the debug channel */
	if (!IS_ERR_OR_NULL(debug_uart_clk)) {
		rate = ((struct plat_serial8250_port *)(
			debug_uartd_device.dev.platform_data))->uartclk;
		pr_info("The debug console clock name is %s\n",
						debug_uart_clk->name);
		c = tegra_get_clock_by_name("pll_p");
		if (IS_ERR_OR_NULL(c))
			pr_err("Not getting the parent clock pll_p\n");
		else
			clk_set_parent(debug_uart_clk, c);

		clk_enable(debug_uart_clk);
		clk_set_rate(debug_uart_clk, rate);
	} else {
		pr_err("Not getting the clock %s for debug console\n",
				debug_uart_clk->name);
	}
}

static void __init enterprise_uart_init(void)
{
	int i;
	struct clk *c;

	for (i = 0; i < ARRAY_SIZE(uart_parent_clk); ++i) {
		c = tegra_get_clock_by_name(uart_parent_clk[i].name);
		if (IS_ERR_OR_NULL(c)) {
			pr_err("Not able to get the clock for %s\n",
						uart_parent_clk[i].name);
			continue;
		}
		uart_parent_clk[i].parent_clk = c;
		uart_parent_clk[i].fixed_clk_rate = clk_get_rate(c);
	}
	enterprise_uart_pdata.parent_clk_list = uart_parent_clk;
	enterprise_uart_pdata.parent_clk_count = ARRAY_SIZE(uart_parent_clk);
	enterprise_loopback_uart_pdata.parent_clk_list = uart_parent_clk;
	enterprise_loopback_uart_pdata.parent_clk_count =
						ARRAY_SIZE(uart_parent_clk);
	enterprise_loopback_uart_pdata.is_loopback = true;

#ifndef CONFIG_MODEM_ICERA_E450
	modem_uart_pdata = enterprise_uart_pdata;
	modem_uart_pdata.wake_peer = wake_modem;
	modem_uart_pdata.sleep_ctrl = modem_sleep_control;	
	tegra_uarta_device.dev.platform_data = &modem_uart_pdata;
#else
	tegra_uarta_device.dev.platform_data = &enterprise_uart_pdata;
#endif	

	tegra_uartb_device.dev.platform_data = &enterprise_uart_pdata;
	tegra_uartc_device.dev.platform_data = &enterprise_uart_pdata;
	tegra_uartd_device.dev.platform_data = &enterprise_uart_pdata;
	/* UARTE is used for loopback test purpose */
	tegra_uarte_device.dev.platform_data = &enterprise_loopback_uart_pdata;

	/* Register low speed only if it is selected */
	if (!is_tegra_debug_uartport_hs())
		uart_debug_init();

	platform_add_devices(enterprise_uart_devices,
				ARRAY_SIZE(enterprise_uart_devices));
}



static struct resource tegra_rtc_resources[] = {
	[0] = {
		.start = TEGRA_RTC_BASE,
		.end = TEGRA_RTC_BASE + TEGRA_RTC_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_RTC,
		.end = INT_RTC,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device tegra_rtc_device = {
	.name = "tegra_rtc",
	.id   = -1,
	.resource = tegra_rtc_resources,
	.num_resources = ARRAY_SIZE(tegra_rtc_resources),
};

static struct platform_device tegra_camera = {
	.name = "tegra_camera",
	.id = -1,
};

#ifdef CONFIG_SND_SOC_MAX98095
static struct tegra_asoc_platform_data enterprise_audio_max98095_pdata = {
	.gpio_spkr_en		= -1,
	.gpio_hp_det		= -1,
	.gpio_hp_mute	= -1,
	.gpio_int_mic_en	= -1,
	.gpio_ext_mic_en = -1,
	.audio_port_id		= {
		[HIFI_CODEC] = 0,
		[BASEBAND] = 2,
		[BT_SCO] = 3,
	},
};

static struct tegra_asoc_platform_data enterprise_audio_max98095_spi_pdata = {
	.gpio_spkr_en		= -1,
	.gpio_hp_det		= TEGRA_GPIO_HP_DET,
	.gpio_hp_mute	= -1,
	.gpio_int_mic_en	= -1,
	.gpio_ext_mic_en = -1,
	//.gpio_mic_det		= TEGRA_GPIO_CDC_IRQ,	
	/*defaults for Verbier-Enterprise (E1197) board with TI AIC326X codec*/
	.audio_port_id		= {
		[HIFI_CODEC] = 0,
		[BASEBAND] = 2,
		[BT_SCO] = 3,
	},
};
static struct platform_device enterprise_audio_max98095_device = {
	.name	= "tegra-snd-max98095",
	.id	= 0,
	.dev	= {
		.platform_data  = &enterprise_audio_max98095_pdata,
	},
};

static struct platform_device enterprise_audio_max98095_spi_device = {
	.name	= "tegra-snd-max98095",
	.id	= 0,
	.dev	= {
		.platform_data  = &enterprise_audio_max98095_spi_pdata,
	},
};

#endif

#ifndef CONFIG_MODEM_ICERA_E450
static struct platform_device modem_control_device = {
	.name = "modem_control",
	.id = -1,
};
#endif

static struct platform_device *enterprise_devices[] __initdata = {
	&tegra_pmu_device,
	&tegra_rtc_device,
	&tegra_udc_device,
#if defined(CONFIG_TEGRA_IOVMM_SMMU) || defined(CONFIG_TEGRA_IOMMU_SMMU)
	&tegra_smmu_device,
#endif
	&tegra_wdt_device,
#if defined(CONFIG_TEGRA_AVP)
	&tegra_avp_device,
#endif
	&tegra_camera,
	&enterprise_bcm4329_rfkill_device,
	&tegra_spi_device4,
	&tegra_hda_device,
#if defined(CONFIG_CRYPTO_DEV_TEGRA_SE)
	&tegra_se_device,
#endif
#if defined(CONFIG_CRYPTO_DEV_TEGRA_AES)
	&tegra_aes_device,
#endif
#ifndef CONFIG_MODEM_ICERA_E450
	&modem_control_device,
#endif
};

/*ZTE: modified by tong.weili for Ft5X06 Touch 20120613 ++*/
#ifdef CONFIG_TOUCHSCREEN_FT5X06
int  ft_touch_wake(void)
{
        printk("[FTS]ft_touch_wake enter \n");
        gpio_direction_output(TEGRA_GPIO_PH6, 1); 
        msleep(3);
        gpio_set_value(TEGRA_GPIO_PH6, 0);
        msleep(10);
        gpio_set_value(TEGRA_GPIO_PH6, 1);
        msleep(150);
        gpio_direction_input(TEGRA_GPIO_PH6); 
        msleep(3);

        printk("[FTS]ft_touch_wake exit \n");
        return 0;
}
EXPORT_SYMBOL(ft_touch_wake);

int  ft_touch_reset(void)
{
        printk("[FTS]ft_touch_reset enter \n");
        gpio_set_value(TEGRA_GPIO_PF5, 1);
        msleep(3);
        gpio_set_value(TEGRA_GPIO_PF5, 0);
        msleep(3);
        gpio_set_value(TEGRA_GPIO_PF5, 1);
        msleep(250);  
        printk("[FTS]ft_touch_reset exit \n");
        return 0;
}
EXPORT_SYMBOL(ft_touch_reset);
static int touch_hw_init(void)
{
      gpio_request(TEGRA_GPIO_PH6, "touch-irq");	  
      gpio_direction_input(TEGRA_GPIO_PH6); 
      tegra_gpio_enable(TEGRA_GPIO_PH6);    

      gpio_request(TEGRA_GPIO_PF5, "touch-reset");	  
      gpio_direction_output(TEGRA_GPIO_PF5, 1);
      tegra_gpio_enable(TEGRA_GPIO_PF5);

      return 0;
}

static struct Ft5x06_ts_platform_data ft5x06_i2c_data = {
              .maxx = 319,      
              .maxy = 479,       
              .model = 2010,
              .x_plate_ohms = 300,
              .irq_flags    = IRQF_TRIGGER_LOW,
              .init_platform_hw = touch_hw_init,
};

static struct i2c_board_info ft5x06_i2c_board_info[] = {
	{
            I2C_BOARD_INFO("ft5x0x_ts", 0x3E),
		.platform_data = &ft5x06_i2c_data,
		.irq		= TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PH6),
	},
};  
#endif
/*ZTE: modified by tong.weili for Ft5X06 Touch 20120613 --*/

/*ZTE: modified by tong.weili for touchscreen driver 20120523 ++*/
#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXT
#define MXT_CONFIG_CRC 0x62F903
/*
 * Config converted from memory-mapped cfg-file with
 * following version information:
 *
 *
 *
 *      FAMILY_ID=128
 *      VARIANT=1
 *      VERSION=32
 *      BUILD=170
 *      VENDOR_ID=255
 *      PRODUCT_ID=TBD
 *      CHECKSUM=0xC189B6
 *
 *
 */

static const u8 config[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0x32, 0x0A, 0x00, 0x05, 0x01, 0x00,
        0x00, 0x1E, 0x0A, 0x8B, 0x00, 0x00, 0x13, 0x0B,
        0x00, 0x10, 0x32, 0x03, 0x03, 0x00, 0x03, 0x01,
        0x00, 0x0A, 0x0A, 0x0A, 0x0A, 0xBF, 0x03, 0x1B,
        0x02, 0x00, 0x00, 0x37, 0x37, 0x00, 0x00, 0x00,
        0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xA9, 0x7F, 0x9A, 0x0E, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x03, 0x23, 0x00, 0x00, 0x00, 0x0A,
        0x0F, 0x14, 0x19, 0x03, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x08, 0x10,
        0x00
};

static struct mxt_platform_data atmel_mxt_info = {
        .x_line         = 19,
        .y_line         = 11,
        .x_size         = 960,
        .y_size         = 540,
        .blen           = 0x10,
        .threshold      = 0x32,
        .voltage        = 3300000,              /* 3.3V */
        .orient         = 3,
        .config         = config,
        .config_length  = 168,
        .config_crc     = MXT_CONFIG_CRC,
        .irqflags       = IRQF_TRIGGER_FALLING,
/*      .read_chg       = &read_chg, */
        .read_chg       = NULL,
};

static struct i2c_board_info __initdata atmel_i2c_info[] = {
	{
		I2C_BOARD_INFO("atmel_mxt_ts", MXT224_I2C_ADDR1),
		.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PH6),
		.platform_data = &atmel_mxt_info,
	}
};
#endif

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI
static struct i2c_board_info synaptics_i2c_board_info[] = {
	{
		I2C_BOARD_INFO("synaptics-rmi-ts", 0x22),
		//.platform_data = &synaptics_i2c_data,
		.irq		= TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PH6),		
	},
};
#endif
/*ZTE: modified by tong.weili for touchscreen driver 20120523 --*/

static int __init enterprise_touch_init(void)
{

/*ZTE: modified by tong.weili for touchscreen driver 20120523 ++*/

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI
      gpio_request(TEGRA_GPIO_PH6, "touch-irq");	  
      gpio_direction_input(TEGRA_GPIO_PH6); 
      tegra_gpio_enable(TEGRA_GPIO_PH6);

      gpio_request(TEGRA_GPIO_PF5, "touch-reset");
      gpio_direction_output(TEGRA_GPIO_PF5, 1);
      tegra_gpio_enable(TEGRA_GPIO_PF5);
      
      i2c_register_board_info(1, synaptics_i2c_board_info, ARRAY_SIZE(synaptics_i2c_board_info));
#endif

#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXT
	tegra_gpio_enable(TEGRA_GPIO_PH6);
	tegra_gpio_enable(TEGRA_GPIO_PF5);

	gpio_request(TEGRA_GPIO_PH6, "atmel-irq");
	gpio_direction_input(TEGRA_GPIO_PH6);

	gpio_request(TEGRA_GPIO_PF5, "atmel-reset");
	gpio_direction_output(TEGRA_GPIO_PF5, 0);
	msleep(1);
	gpio_set_value(TEGRA_GPIO_PF5, 1);
	msleep(100);

	i2c_register_board_info(1, atmel_i2c_info, 1);
#endif
/*ZTE: modified by tong.weili for touchscreen driver 20120523 --*/

/*ZTE: modified by tong.weili for Ft5X06 Touch 20120613 ++*/
#ifdef CONFIG_TOUCHSCREEN_FT5X06
	i2c_register_board_info(1, ft5x06_i2c_board_info,
							ARRAY_SIZE(ft5x06_i2c_board_info));
#endif
/*ZTE: modified by tong.weili for Ft5X06 Touch 20120613 --*/

	return 0;
}

static void enterprise_usb_hsic_postsupend(void)
{
	pr_debug("%s\n", __func__);
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L2);
#endif
}

static void enterprise_usb_hsic_preresume(void)
{
	pr_debug("%s\n", __func__);
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L2TOL0);
#endif
}

static void enterprise_usb_hsic_phy_power(void)
{
	pr_debug("%s\n", __func__);
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L0);
#endif
}

static void enterprise_usb_hsic_post_phy_off(void)
{
	pr_debug("%s\n", __func__);
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L3);
#endif
}

static struct tegra_usb_phy_platform_ops hsic_xmm_plat_ops = {
	.post_suspend = enterprise_usb_hsic_postsupend,
	.pre_resume = enterprise_usb_hsic_preresume,
	.port_power = enterprise_usb_hsic_phy_power,
	.post_phy_off = enterprise_usb_hsic_post_phy_off,
};

static struct tegra_usb_platform_data tegra_ehci2_hsic_xmm_pdata = {
	.port_otg = false,
	.has_hostpc = true,
	.phy_intf = TEGRA_USB_PHY_INTF_HSIC,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.hot_plug = false,
		.remote_wakeup_supported = false,
		.power_off_on_suspend = false,
	},
	.u_cfg.hsic = {
		.sync_start_delay = 9,
		.idle_wait_delay = 17,
		.term_range_adj = 0,
		.elastic_underrun_limit = 16,
		.elastic_overrun_limit = 16,
	},
	.ops = &hsic_xmm_plat_ops,
};



static struct tegra_usb_platform_data tegra_udc_pdata = {
	.port_otg = true,
	.has_hostpc = true,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_DEVICE,
	.u_data.dev = {
		.vbus_pmu_irq = ENT_TPS80031_IRQ_BASE +
				TPS80031_INT_VBUS_DET,
		.vbus_gpio = -1,
		.charging_supported = false,
		.remote_wakeup_supported = false,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
	},
};

static struct tegra_usb_platform_data tegra_ehci1_utmi_pdata = {
	.port_otg = true,
	.has_hostpc = true,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.vbus_reg = "usb_vbus",
		.hot_plug = true,
		.remote_wakeup_supported = true,
		.power_off_on_suspend = true,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 15,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
	},
};

static struct tegra_usb_otg_data tegra_otg_pdata = {
	.ehci_device = &tegra_ehci1_device,
	.ehci_pdata = &tegra_ehci1_utmi_pdata,
};

struct platform_device *tegra_usb_hsic_host_register(void)
{
	struct platform_device *pdev;
	int val;

	pdev = platform_device_alloc(tegra_ehci2_device.name,
		tegra_ehci2_device.id);
	if (!pdev)
		return NULL;

	val = platform_device_add_resources(pdev, tegra_ehci2_device.resource,
		tegra_ehci2_device.num_resources);
	if (val)
		goto error;

	pdev->dev.dma_mask =  tegra_ehci2_device.dev.dma_mask;
	pdev->dev.coherent_dma_mask = tegra_ehci2_device.dev.coherent_dma_mask;

	val = platform_device_add_data(pdev, &tegra_ehci2_hsic_xmm_pdata,
			sizeof(struct tegra_usb_platform_data));
	if (val)
		goto error;

	val = platform_device_add(pdev);
	if (val)
		goto error;

	return pdev;

error:
	pr_err("%s: failed to add the host contoller device\n", __func__);
	platform_device_put(pdev);
	return NULL;
}

void tegra_usb_hsic_host_unregister(struct platform_device *pdev)
{
	platform_device_unregister(pdev);
}

static void enterprise_usb_init(void)
{
	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;

	tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
	platform_device_register(&tegra_otg_device);
}

static struct platform_device *enterprise_audio_devices[] __initdata = {
	&tegra_ahub_device,
	&tegra_dam_device0,
	&tegra_dam_device1,
	&tegra_dam_device2,
	&tegra_i2s_device0,
	&tegra_i2s_device1,
	&tegra_i2s_device2,
	&tegra_i2s_device3,
	&tegra_spdif_device,
	&spdif_dit_device,
	&bluetooth_dit_device,
	&baseband_dit_device,
	&tegra_pcm_device,
	#ifdef CONFIG_SND_SOC_MAX98095
	&enterprise_audio_max98095_device,
	#endif
};


static struct platform_device *enterprise_audio_spi_devices[] __initdata = {
	&tegra_ahub_device,
	&tegra_dam_device0,
	&tegra_dam_device1,
	&tegra_dam_device2,
	&tegra_i2s_device0,
	&tegra_i2s_device1,
	&tegra_i2s_device2,
	&tegra_i2s_device3,
	&tegra_spdif_device,
	&spdif_dit_device,
	&bluetooth_dit_device,
	&baseband_dit_device,
	&tegra_pcm_device,
	#ifdef CONFIG_SND_SOC_MAX98095
	&enterprise_audio_max98095_spi_device,
	#endif
};


static void enterprise_gps_init(void)
{
	tegra_gpio_enable(TEGRA_GPIO_PE4);
	tegra_gpio_enable(TEGRA_GPIO_PE5);
}

static struct baseband_power_platform_data tegra_baseband_power_data = {
	.baseband_type = BASEBAND_XMM,
	.modem = {
		.xmm = {
			.bb_rst = XMM_GPIO_BB_RST,
			.bb_on = XMM_GPIO_BB_ON,
			.ipc_bb_wake = XMM_GPIO_IPC_BB_WAKE,
			.ipc_ap_wake = XMM_GPIO_IPC_AP_WAKE,
			.ipc_hsic_active = XMM_GPIO_IPC_HSIC_ACTIVE,
			.ipc_hsic_sus_req = XMM_GPIO_IPC_HSIC_SUS_REQ,
		},
	},
};

static struct platform_device tegra_baseband_power_device = {
	.name = "baseband_xmm_power",
	.id = -1,
	.dev = {
		.platform_data = &tegra_baseband_power_data,
	},
};

static struct platform_device tegra_baseband_power2_device = {
	.name = "baseband_xmm_power2",
	.id = -1,
	.dev = {
		.platform_data = &tegra_baseband_power_data,
	},
};

#ifdef CONFIG_TEGRA_BB_M7400
static union tegra_bb_gpio_id m7400_gpio_id = {
	.m7400 = {
		.pwr_status = GPIO_BB_RESET,
		.pwr_on = GPIO_BB_PWRON,
		.uart_awr = GPIO_BB_APACK,
		.uart_cwr = GPIO_BB_CPACK,
		.usb_awr = GPIO_BB_APACK2,
		.usb_cwr = GPIO_BB_CPACK2,
		.service = GPIO_BB_RSVD2,
		.resout2 = GPIO_BB_RSVD1,
	},
};

static struct tegra_bb_pdata m7400_pdata = {
	.id = &m7400_gpio_id,
	.device = &tegra_ehci2_device,
	.ehci_register = tegra_usb_hsic_host_register,
	.ehci_unregister = tegra_usb_hsic_host_unregister,
	.bb_id = TEGRA_BB_M7400,
};

static struct platform_device tegra_baseband_m7400_device = {
	.name = "tegra_baseband_power",
	.id = -1,
	.dev = {
		.platform_data = &m7400_pdata,
	},
};
#endif

static void enterprise_baseband_init(void)
{
	int modem_id = tegra_get_modem_id();

#ifdef CONFIG_MODEM_ICERA_E450
       printk(KERN_INFO "enterprise_baseband_init modem_id %d", modem_id);
       modem_id = 1;
       printk(KERN_INFO "set modem_id to %d", modem_id);
#endif

	switch (modem_id) {
	case TEGRA_BB_PH450: /* PH450 ULPI */
		enterprise_modem_init();
		break;
	case TEGRA_BB_XMM6260: /* XMM6260 HSIC */
		/* baseband-power.ko will register ehci2 device */
		tegra_ehci2_device.dev.platform_data =
					&tegra_ehci2_hsic_xmm_pdata;
		/* enable XMM6260 baseband gpio(s) */
		tegra_gpio_enable(tegra_baseband_power_data.modem.generic
			.mdm_reset);
		tegra_gpio_enable(tegra_baseband_power_data.modem.generic
			.mdm_on);
		tegra_gpio_enable(tegra_baseband_power_data.modem.generic
			.ap2mdm_ack);
		tegra_gpio_enable(tegra_baseband_power_data.modem.generic
			.mdm2ap_ack);
		tegra_gpio_enable(tegra_baseband_power_data.modem.generic
			.ap2mdm_ack2);
		tegra_gpio_enable(tegra_baseband_power_data.modem.generic
			.mdm2ap_ack2);
		tegra_baseband_power_data.hsic_register =
						&tegra_usb_hsic_host_register;
		tegra_baseband_power_data.hsic_unregister =
						&tegra_usb_hsic_host_unregister;
		platform_device_register(&tegra_baseband_power_device);
		platform_device_register(&tegra_baseband_power2_device);
		break;
#ifdef CONFIG_TEGRA_BB_M7400
	case TEGRA_BB_M7400: /* M7400 HSIC */
		tegra_ehci2_hsic_xmm_pdata.u_data.host.power_off_on_suspend = 0;
		tegra_ehci2_device.dev.platform_data
			= &tegra_ehci2_hsic_xmm_pdata;
		platform_device_register(&tegra_baseband_m7400_device);
		break;
#endif
	}
}


static void enterprise_audio_init(void)
{
	struct board_info board_info;

   #ifdef CONFIG_SND_SOC_MAX98095
      audio_codec_enbale(1);
      #ifdef CONFIG_PROJECT_V985
      codec_spi_bus_init();
      
      #else
      #ifdef CONFIG_PROJECT_U985
      if (0 != zte_get_board_id())
      #endif
      {
	  	codec_spi_bus_init();
      }
      #endif
   #endif

#ifdef CONFIG_PROJECT_V985
        platform_add_devices(enterprise_audio_spi_devices,
        ARRAY_SIZE(enterprise_audio_spi_devices));
#else
    #ifdef CONFIG_PROJECT_U985
    if(0 == zte_get_board_id()){   
        platform_add_devices(enterprise_audio_devices,
        ARRAY_SIZE(enterprise_audio_devices));
    }else
    #endif
    {   
        platform_add_devices(enterprise_audio_spi_devices,
        ARRAY_SIZE(enterprise_audio_spi_devices));
    }    
#endif
    
   
}

static void enterprise_nfc_init(void)
{
	struct board_info bi;

#ifdef CONFIG_PROJECT_V985
	tegra_gpio_enable(TEGRA_GPIO_PB3);
	tegra_gpio_enable(TEGRA_GPIO_PJ3);
	tegra_gpio_enable(TEGRA_GPIO_PZ3);
#else
	tegra_gpio_enable(TEGRA_GPIO_PS4);
	tegra_gpio_enable(TEGRA_GPIO_PM6);
#endif

	/* Enable firmware GPIO PX7 for board E1205 */
	tegra_get_board_info(&bi);
	#if 0  
	//delete NFC firm_gpio for SPI1 of CMMB  wangtao 20120628
	if (bi.board_id == BOARD_E1205 && bi.fab >= BOARD_FAB_A03) {
		nfc_pdata.firm_gpio = TEGRA_GPIO_PX7;
		tegra_gpio_enable(TEGRA_GPIO_PX7);
	}
	#endif
}

#ifndef CONFIG_ZTE_CMMB
void cmmb_gpio_init(void)
{
    int ret;
    //printk("[cmmbspi] cmmb_gpio_init\n");

    ret = gpio_request(TEGRA_GPIO_PE1, "CMMB_PWR_EN");
    if (ret)
   {
        printk("[cmmbspi]TEGRA_GPIO_PE1 fail\n"); 
        goto fail_err;
    }
    ret = gpio_request(TEGRA_GPIO_PE2, "CMMB_RST");
    if (ret) {
        printk("[cmmbspi]TEGRA_GPIO_PE2 fail\n"); 
        gpio_free(TEGRA_GPIO_PE1);
        goto fail_err;
    }
    ret = gpio_request(TEGRA_GPIO_PS0, "cmmb-int-gpio");
    if (ret) 
    {
        printk("[cmmbspi] TEGRA_GPIO_PS0 fail\n");
        gpio_free(TEGRA_GPIO_PE1);
        gpio_free(TEGRA_GPIO_PE2);
        goto fail_err;
    }    

    tegra_gpio_enable(TEGRA_GPIO_PE1);
    tegra_gpio_enable(TEGRA_GPIO_PE2);
    tegra_gpio_enable(TEGRA_GPIO_PS0);

    gpio_direction_output(TEGRA_GPIO_PE1, 0);
    gpio_direction_output(TEGRA_GPIO_PE2, 0);
    gpio_direction_input(TEGRA_GPIO_PS0);
    return;
    
fail_err:
    printk("[cmmbspi] cmmb_gpio_init fail\n"); 
    return;
}
#endif

#ifdef CONFIG_ZTE_CMMB
static struct spi_board_info cmmb_spi_board_info[] = {
	/* spi master */
	{
		.modalias = "spi_sms2186",
		.bus_num = 0,
		.chip_select = 0,
		.mode = SPI_MODE_0,
		.max_speed_hz = 10000000,
		.platform_data = NULL,
		.irq = 0,
	},
};

static struct spi_clk_parent spi_cmmb_parent_clk[] = {
        [0] = {.name = "pll_p"},        
};

static struct tegra_spi_platform_data spi_cmmb_pdata = {
        .is_dma_based           = true,
        .max_dma_buffer         = (16 * 1024),
        .is_clkon_always        = true,
        .max_rate               = 5000000,
};

int enterprise_cmmb_init(void)
{
    int err;
    int i;
    struct clk *c;

    printk("%s: [cmmb] enterprise_cmmb_init entry \n",__func__);

    for (i = 0; i < ARRAY_SIZE(spi_cmmb_parent_clk); ++i) {
		c = tegra_get_clock_by_name(spi_cmmb_parent_clk[i].name);
              if (IS_ERR_OR_NULL(c)) {
                   pr_err("Not able to get the clock for %s\n",
                                                spi_cmmb_parent_clk[i].name);
                        continue;
                }
                spi_cmmb_parent_clk[i].parent_clk = c;
                spi_cmmb_parent_clk[i].fixed_clk_rate = clk_get_rate(c);
   }
   spi_cmmb_pdata.parent_clk_list = spi_cmmb_parent_clk;
   spi_cmmb_pdata.parent_clk_count = ARRAY_SIZE(spi_cmmb_parent_clk);
   tegra_spi_device1.dev.platform_data = &spi_cmmb_pdata;
    
    platform_device_register(&tegra_spi_device1);
    err = spi_register_board_info(cmmb_spi_board_info,
    		                    ARRAY_SIZE(cmmb_spi_board_info));
    if (err < 0)
    {
        pr_err("%s: spi_register_board returned error %d\n",__func__, err);
    }
    return 0;
}
#endif

#ifdef CONFIG_SIRF_GPS    // add by jiabf for sirf gps
#define GPS_ON_OFF_GPIO	36
#define GPS_RESET_GPIO	      37
#define GPS_POWER_GPIO       59

static void gps_sirf_gpio_init(void)
{
    int ret;
    printk("gps_sirf_gpio_init\n");

    ret = gpio_request(GPS_POWER_GPIO, "gps_power");
    if (ret)
   {
        printk("GPS_POWER_GPIO fail\n"); 
        goto fail_err;
    }
    ret = gpio_request(GPS_RESET_GPIO, "gps_reset");
    if (ret) {
        printk("GPS_RESET_GPIO fail\n"); 
        gpio_free(GPS_POWER_GPIO);
        goto fail_err;
    }
    ret = gpio_request(GPS_ON_OFF_GPIO, "gps_on_off");
    if (ret) 
    {
        printk(" GPS_ON_OFF_GPIO fail\n");
        gpio_free(GPS_POWER_GPIO);
        gpio_free(GPS_RESET_GPIO);
        goto fail_err;
    }    

    tegra_gpio_enable(GPS_POWER_GPIO);
    tegra_gpio_enable(GPS_RESET_GPIO);
    tegra_gpio_enable(GPS_ON_OFF_GPIO);

    gpio_direction_output(GPS_POWER_GPIO, 0);
    gpio_direction_output(GPS_RESET_GPIO, 0);
    gpio_direction_output(GPS_ON_OFF_GPIO, 0);
    return;
fail_err:
    printk(" gps_sirf_gpio_init fail\n"); 
    return;
    
}    
static void gps_power_on(void)
{
      gpio_set_value(GPS_POWER_GPIO, 1);
	gpio_set_value(GPS_RESET_GPIO, 0);
	gpio_set_value(GPS_ON_OFF_GPIO, 0);	
	printk(KERN_INFO "sirf gps chip powered on\n");
}

static void gps_power_off(void)
{
      gpio_set_value(GPS_POWER_GPIO, 0);
	gpio_set_value(GPS_RESET_GPIO, 0);
	gpio_set_value(GPS_ON_OFF_GPIO, 0);	
	printk(KERN_INFO "sirf gps chip powered off\n");
}

static void gps_reset(int flag)
{
       gpio_set_value(GPS_RESET_GPIO, flag);
	printk(KERN_INFO "sirf gps chip reset flag =%d\n",flag);
}

static void gps_on_off(int flag)
{
	gpio_set_value(GPS_ON_OFF_GPIO, flag);	
	printk(KERN_INFO "sirf gps chip off on flag =%d\n",flag);
}

static char sirf_status[4] = "off";
static ssize_t sirf_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	int len = strlen(sirf_status);

	sprintf(page, "%s\n", sirf_status);
	return len + 1;
}

static ssize_t sirf_write_proc(struct file *filp,
		const char *buff, size_t len, loff_t *off)
{
	char messages[256];
	int flag, ret;
	char buffer[7];

	if (len > 256)
		len = 256;

	if (copy_from_user(messages, buff, len))
		return -EFAULT;

	if (strncmp(messages, "off", 3) == 0) {
		strcpy(sirf_status, "off");
		gps_power_off();
	} else if (strncmp(messages, "on", 2) == 0) {
		strcpy(sirf_status, "on");
		gps_power_on();
	} else if (strncmp(messages, "reset", 5) == 0) {
		strcpy(sirf_status, messages);
		ret = sscanf(messages, "%s %d", buffer, &flag);
		if (ret == 2)
			gps_reset(flag);
	} else if (strncmp(messages, "sirfon", 5) == 0) {
		strcpy(sirf_status, messages);
		ret = sscanf(messages, "%s %d", buffer, &flag);
		if (ret == 2)
			gps_on_off(flag);
	} else {
		printk("usage: echo {on/off} > /proc/driver/sirf\n");
	}

	return len;
}

static void create_sirf_proc_file(void)
{
	struct proc_dir_entry *sirf_proc_file =
		create_proc_entry("driver/sirf", 0777, NULL);

	if (sirf_proc_file) {
		sirf_proc_file->read_proc = sirf_read_proc;
		sirf_proc_file->write_proc = (write_proc_t  *)sirf_write_proc;
	} else
		printk(KERN_INFO "proc file create failed!\n");
      gps_sirf_gpio_init();
      gps_power_on();
}
#endif
static void __init tegra_enterprise_init(void)
{
	tegra_thermal_init(&thermal_data,
				throttle_list,
				ARRAY_SIZE(throttle_list));
	tegra_clk_init_from_table(enterprise_clk_init_table);
	enterprise_pinmux_init();
	enterprise_i2c_init();
	enterprise_uart_init();
	enterprise_usb_init();
	platform_add_devices(enterprise_devices, ARRAY_SIZE(enterprise_devices));
	tegra_ram_console_debug_init();
	enterprise_regulator_init();
	tegra_io_dpd_init();
	enterprise_sdhci_init();
#ifdef CONFIG_TEGRA_EDP_LIMITS
	enterprise_edp_init();
#endif
	/* enterprise_kbc_init(); */ /* ZTE: modified by pengtao for gpio-key 20120528*/
    zteenterprise_keys_init(); // ZTE: added by pengtao for gpio-key 20120528
	enterprise_touch_init();
	enterprise_audio_init();
	enterprise_gps_init();
	enterprise_baseband_init();
	enterprise_panel_init();
	enterprise_setup_bluesleep();
	enterprise_emc_init();
	enterprise_sensors_init();
	enterprise_suspend_init();
	enterprise_bpc_mgmt_init();
	tegra_release_bootloader_fb();
	enterprise_nfc_init();
	batt_init();
#ifdef CONFIG_ZTE_CMMB
      enterprise_cmmb_init();
#endif
#ifndef CONFIG_ZTE_CMMB
      cmmb_gpio_init();
#endif
#ifdef CONFIG_SIRF_GPS
     create_sirf_proc_file();
#endif
     zte_hver_proc_init();
}

static void __init tegra_enterprise_reserve(void)
{
#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM)
	tegra_reserve(0, SZ_4M, SZ_8M);
#else
	tegra_reserve(SZ_128M, SZ_4M, SZ_8M);
#endif
	tegra_ram_console_debug_reserve(SZ_1M);
}

MACHINE_START(TEGRA_ENTERPRISE, "tegra_enterprise")
	.boot_params    = 0x80000100,
	.map_io         = tegra_map_common_io,
	.reserve        = tegra_enterprise_reserve,
	.init_early	= tegra_init_early,
	.init_irq       = tegra_init_irq,
	.timer          = &tegra_timer,
	.init_machine   = tegra_enterprise_init,
MACHINE_END
