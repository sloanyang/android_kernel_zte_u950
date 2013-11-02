/*
 * drivers/power/tps80031_charger.c
 *
 * Battery charger driver for TI's tps80031
 *
 * Copyright (c) 2011, NVIDIA Corporation.
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
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/tps80031.h>
#include <linux/tps80031-charger.h>

#define CONTROLLER_CTRL1	0xe1
#define CONTROLLER_STAT1	0xe3
#define CHARGERUSB_CTRL2	0xe9
#define CHARGERUSB_CTRL3	0xea
#define CHARGERUSB_VOREG	0xec
#define CHARGERUSB_VICHRG	0xed
#define CHARGERUSB_CINLIMIT	0xee
#define CHARGERUSB_CTRLLIMIT2	0xf0
#define CHARGERUSB_CTRLLIMIT1	0xef
#define CHARGERUSB_VICHRG_PC	0xdd
#define CONTROLLER_WDG		0xe2
#define LINEAR_CHRG_STS		0xde
#define CHARGERUSB_STATUS_INT	0xe6

#define CONTROLLER_VSEL_COMP 0xdb

#define TPS80031_VBUS_DET	BIT(2)
#define TPS80031_VAC_DET	BIT(3)

#ifdef CONFIG_HIGH_VOLTAGE_LEVEL_BATTERY
#define MAX_CHARGE_VOLTAGE      0x2c
#define CHARGE_VOLTAGE      0x2b
#else
#define MAX_CHARGE_VOLTAGE      0x23
#define CHARGE_VOLTAGE      0x23
#endif
/*ZTE:add by caixiaoguang for set a flag before power off++*/
bool power_off_flag = 0;
static bool vbus_ovp_flag = 0;
static int usb_high_current = 0 ;
/*ZTE:add by caixiaoguang for set a flag before power off--*/
struct tps80031_charger {
	int			max_charge_current_mA;
	int			max_charge_volt_mV;
	struct device		*dev;
	struct regulator_dev	*rdev;
	struct regulator_desc	reg_desc;
	struct regulator_init_data		reg_init_data;
	struct tps80031_charger_platform_data	*pdata;
	int (*board_init)(void *board_data);
	void			*board_data;
	int			irq_base;
	int			watch_time_sec;
	enum charging_states	state;
	int			charging_term_current_mA;
	charging_callback_t	charger_cb;
	void			*charger_cb_data;
};
/*ZTE:add by caixiaoguang 20120531 for charge++*/
struct delayed_work		wdg_work; 
struct work_struct     vbus_int_work;
extern void battery_update_by_interrupt(void);
extern int fsl_charger_detect(void);
extern int zte_get_board_temp(void);
/*ZTE:add by caixiaoguang 20120531 for charge--*/
static struct tps80031_charger *charger_data;
static uint8_t charging_current_val_code[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0x27,
	0x37, 0x28, 0x38, 0x29, 0x39, 0x2A, 0x3A, 0x2B, 0x3B, 0x2C,
	0x3C, 0x2D, 0x3D, 0x2E,
};

static int set_charge_current_limit(struct regulator_dev *rdev,
		int min_uA, int max_uA)
{
	struct tps80031_charger *charger = rdev_get_drvdata(rdev);
	int max_vbus_current = 1500;
	int max_charge_current = 1500;
	int ret;

	dev_info(charger->dev, "%s(): Min curr %dmA and max current %dmA\n",
		__func__, min_uA/1000, max_uA/1000);

	if (!max_uA) {
		ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
						CONTROLLER_CTRL1, 0x0);
		if (ret < 0)
			dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CONTROLLER_CTRL1);

		ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
						CONTROLLER_WDG, 0x0);
		if (ret < 0)
			dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CONTROLLER_WDG);
		charger->state = charging_state_charging_stopped;
		if (charger->charger_cb)
			charger->charger_cb(charger->state,
					charger->charger_cb_data);
		return ret;
	}

	max_vbus_current = min(max_uA/1000, max_vbus_current);
	max_vbus_current = max_vbus_current/50;
	if (max_vbus_current)
		max_vbus_current--;
	ret = tps80031_update(charger->dev->parent, SLAVE_ID2,
			CHARGERUSB_CINLIMIT,
			charging_current_val_code[max_vbus_current], 0x3F);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CHARGERUSB_CINLIMIT);
		return ret;
	}

	max_charge_current = min(max_uA/1000, max_charge_current);
	if (max_charge_current <= 300)
		max_charge_current = 0;
	else if ((max_charge_current > 300) && (max_charge_current <= 500))
		max_charge_current = (max_charge_current - 300)/50;
	else
		max_charge_current = (max_charge_current - 500) / 100 + 4;
	ret = tps80031_update(charger->dev->parent, SLAVE_ID2,
			CHARGERUSB_VICHRG, (uint8_t)max_charge_current, 0xF);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CHARGERUSB_VICHRG);
		return ret;
	}

	/* Enable watchdog timer */
	ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
				CONTROLLER_WDG, charger->watch_time_sec);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CONTROLLER_WDG);
		return ret;
	}

	/* Enable the charging */
	ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
				CONTROLLER_CTRL1, 0x30);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CONTROLLER_CTRL1);
		return ret;
	}
	charger->state = charging_state_charging_in_progress;
	if (charger->charger_cb)
		charger->charger_cb(charger->state,
				charger->charger_cb_data);
	return 0;
}

static struct regulator_ops tegra_regulator_ops = {
	.set_current_limit = set_charge_current_limit,
};

int register_charging_state_callback(charging_callback_t cb, void *args)
{
	struct tps80031_charger *charger = charger_data;
	if (!charger_data)
		return -ENODEV;

	charger->charger_cb = cb;
	charger->charger_cb_data = args;
	return 0;
}
EXPORT_SYMBOL_GPL(register_charging_state_callback);
/*ZTE:add by caixiaoguang 20120531 for charge++*/
int tps80031_get_vbus_status(void)
{
    struct tps80031_charger *charger = charger_data;
    int ret;
    int is_present = -1;
    uint8_t controller_stat1_reg;
    ret = tps80031_read(charger->dev->parent, SLAVE_ID2,
			CONTROLLER_STAT1, &controller_stat1_reg);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in reading register 0x%02x\n",
				__func__, CONTROLLER_STAT1);
	    }
    controller_stat1_reg &= 0x04;
    if(controller_stat1_reg == 0)
        {
            is_present = 0;
        }
    else
        {
            is_present = 1;
        } 
    return is_present;
}
/*ZTE:add by caixiaoguang 20120531 for charge--*/
static int configure_charging_parameter(struct tps80031_charger *charger)
{
	int ret;
	int max_charge_current;
	int max_charge_volt;
	int term_current;
    uint8_t chargerusb_vichrg_reg;
    uint8_t chargerusb_int__mask_reg;
    int vbus_preset = -1;      
    int chargertype = -1;
	/* Disable watchdog timer */
	ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
				CONTROLLER_WDG, 0x0);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CONTROLLER_WDG);
		return ret;
	}

	/* Disable the charging if any */
	ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
				CONTROLLER_CTRL1, 0x0);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CONTROLLER_CTRL1);
		return ret;
	}

	if (charger->board_init) {
		ret = charger->board_init(charger->board_data);
		if (ret < 0) {
			dev_err(charger->dev, "%s(): Failed in board init\n",
				__func__);
			return ret;
		}
	}

	/* Unlock value */
	ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
			CHARGERUSB_CTRLLIMIT2, 0);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CHARGERUSB_CTRLLIMIT2);
		return ret;
	}

	/* Set max current limit */
    #if 0
	max_charge_current = min(1500, charger->max_charge_current_mA);
	if (max_charge_current < 100)
		max_charge_current = 0;
	else
		max_charge_current = (max_charge_current - 100)/100;
	max_charge_current &= 0xF;
	ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
		CHARGERUSB_CTRLLIMIT2, (uint8_t)max_charge_current);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register "
				"0x%02x\n", __func__, CHARGERUSB_CTRLLIMIT2);
		return ret;
	}
    #endif

	/* Set max voltage limit */
	max_charge_volt = min(4760, charger->max_charge_volt_mV);
	max_charge_volt = max(3500, max_charge_volt);
	max_charge_volt -= 3500;
	max_charge_volt = max_charge_volt/20;
	ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
		CHARGERUSB_CTRLLIMIT1, (uint8_t)max_charge_volt);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CHARGERUSB_CTRLLIMIT1);
		return ret;
	}
#if 0
	/* Lock value */
	ret = tps80031_set_bits(charger->dev->parent, SLAVE_ID2,
			CHARGERUSB_CTRLLIMIT2, (1 << 4));
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CHARGERUSB_CTRLLIMIT2);
		return ret;
	}
#endif
	/* set Pre Charge current to 400mA */
	ret = tps80031_write(charger->dev->parent, SLAVE_ID2, 0xDE, 0x3);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, 0xDD);
		return ret;
	}

	/* set charging termination current*/
	if (charger->charging_term_current_mA > 400)
		term_current =  7;
	else
		term_current = (charger->charging_term_current_mA - 50)/50;
	term_current = term_current << 5;
	ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
			CHARGERUSB_CTRL2, term_current);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CHARGERUSB_CTRL2);
		return ret;
	}
    /*ZTE:add by caixiaoguang 20120531 for charge++*/
    #if 0
    vbus_preset = tps80031_get_vbus_status();
    chargertype = fsl_charger_detect();
    if(1 == vbus_preset)
        {
            ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
				CONTROLLER_CTRL1, 0x30);
        	if (ret < 0) {
        		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
        				__func__, CONTROLLER_CTRL1);
        		return ret;
        	}
            //AC set CINLIMIT 1000mA,USB set 500mA
            if(1==chargertype)
                {
            ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
        				CHARGERUSB_CINLIMIT, 0x29);                	
                }
            else
                {
                     ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
        				CHARGERUSB_CINLIMIT, 0x09);                	
                }
        	if (ret < 0) {
        		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
        				__func__, CHARGERUSB_CINLIMIT);
        		return ret;
        	}
            
        }
    #endif
    //mask CHARGERUSB_STAT interrupt in CHARGERUSB_INT 
    ret = tps80031_read(charger->dev->parent, SLAVE_ID2,
			0xE5, &chargerusb_int__mask_reg);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in reading register 0x%02x\n",
				__func__, 0xE5);
	}
    chargerusb_int__mask_reg |= 0x1E;
    ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
				0xE5, chargerusb_int__mask_reg);
        	if (ret < 0) {
        		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
        				__func__, 0xE5);
        		return ret;
        	}  
	return 0;
}

static irqreturn_t linch_status_isr(int irq, void *dev_id)
{
	struct tps80031_charger *charger = dev_id;
	uint8_t linch_status;
	int ret;
	dev_info(charger->dev, "%s() got called\n", __func__);

	ret = tps80031_read(charger->dev->parent, SLAVE_ID2,
			LINEAR_CHRG_STS, &linch_status);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in reading register 0x%02x\n",
				__func__, LINEAR_CHRG_STS);
	} else {
		dev_info(charger->dev, "%s():The status of LINEAR_CHRG_STS is 0x%02x\n",
				 __func__, linch_status);
		if (linch_status & 0x20) {
			charger->state = charging_state_charging_completed;
			if (charger->charger_cb)
				charger->charger_cb(charger->state,
					charger->charger_cb_data);
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t vbus_ovp_status_isr(int irq, void *dev_id)
{
	struct tps80031_charger *charger = dev_id;
	
	dev_info(charger->dev, "%s() got called\n", __func__);

       battery_update_by_interrupt();
	return IRQ_HANDLED;
}
bool tps80031_get_ovp_status(void)
{
    /*ZTE:add by liyun 20121109 for charge status by MHL charger++*/
	struct tps80031_charger *charger = charger_data;
	int ret;
	uint8_t vbus_ovp_status;
	ret = tps80031_read(charger->dev->parent, SLAVE_ID2,
			CHARGERUSB_STATUS_INT, &vbus_ovp_status);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in reading register 0x%02x\n",
				__func__, CHARGERUSB_STATUS_INT);
	}
        vbus_ovp_status &= 0x01;
		if ( 1 == vbus_ovp_status ) 
        {
			vbus_ovp_flag = 1;
		}
        else
        {
            vbus_ovp_flag = 0;
        }
/*ZTE:add by liyun 20121109 for charge status by MHL charger--*/
    return vbus_ovp_flag;
}
static irqreturn_t watchdog_expire_isr(int irq, void *dev_id)
{
	struct tps80031_charger *charger = dev_id;
	int ret;

	dev_info(charger->dev, "%s()\n", __func__);
	if (charger->state != charging_state_charging_in_progress)
		return IRQ_HANDLED;

	/* Enable watchdog timer again*/
	ret = tps80031_write(charger->dev->parent, SLAVE_ID2, CONTROLLER_WDG,
			charger->watch_time_sec);
	if (ret < 0)
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CONTROLLER_WDG);

	/* Rewrite to enable the charging */
	if (!ret) {
		ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
			CONTROLLER_CTRL1, 0x30);
		if (ret < 0)
			dev_err(charger->dev, "%s(): Failed in writing "
				"register 0x%02x\n",
				__func__, CONTROLLER_CTRL1);
	}
	return IRQ_HANDLED;
}
/*ZTE:add by caixiaoguang 20120531 for charge++*/
void tps80031_vbus_int(void)
{
    schedule_work(&vbus_int_work);     
}

void tps80031_set_usb_high_current(int high)
{
    int is_need_high_current = high;
    if(1 == is_need_high_current)
        {
            usb_high_current = 1;
        }
    else
        {
            usb_high_current = 0;
        }
    printk("[%s]CXG set usb_high_current = %d\n",__func__,usb_high_current); 
    return ;
}

void tps80031_config_charge_parameter(void)
{
    struct tps80031_charger *charger = charger_data;
    int ret; 
    int vbus_preset = -1;       
    int chargertype = -1;
    uint8_t chargerusb_voreg=0;
    uint8_t chargerusb_ctrllimit=0;
    uint8_t vsel_comp_reg = 0;
    uint8_t reg1,reg2,reg3;
    int board_temp = 0;
    uint8_t chargeusb_vichrg= 0;    
    vbus_preset = tps80031_get_vbus_status();
    chargertype = fsl_charger_detect();    
    board_temp = zte_get_board_temp();    
    if(board_temp > 50 )
        {
            chargeusb_vichrg = 0x0;//100mA
        }    
    else if(board_temp > 45 )
        {
            chargeusb_vichrg = 0x2;//300mA
        }
    else if(board_temp > 43 )
        {
            chargeusb_vichrg = 0x4;//500mA
        }
    else
        {
            chargeusb_vichrg = 0x9;//1000mA
        }
    
    printk("[%s]CXG get vbus_preset=%d,chargertype=%d,chargeusb_vichrg=%d,usb_high_current=%d\n",__func__,vbus_preset,chargertype,chargeusb_vichrg,usb_high_current);
    /* Unlock value */
	ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
			CHARGERUSB_CTRLLIMIT2, 0);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CHARGERUSB_CTRLLIMIT2);
		return ret;
	}
    if(1 == vbus_preset)
        {
                if(1==chargertype)
                {
                    //set vbus in current limit 1000mA
                    ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
        				CHARGERUSB_CINLIMIT, 0x2e);
                	if (ret < 0) {
                		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
                				__func__, CHARGERUSB_CINLIMIT);
                		return ret;
                	}
                   
                    //set charge current 1000mA
                    ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
                				CHARGERUSB_VICHRG, chargeusb_vichrg);
                        	if (ret < 0) {
                        		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
                        				__func__, CHARGERUSB_VICHRG);
                        		return ret;
                        	}
                    //set max charge current 1000mA
                    ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
                				CHARGERUSB_CTRLLIMIT2, 0x09);
                        	if (ret < 0) {
                        		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
                        				__func__, CHARGERUSB_VICHRG);
                        		return ret;
                        	}
                    }
            else
                {
                    if(1 == usb_high_current)
                        {
                            //set vbus in current limit 1000mA
                            ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
                				CHARGERUSB_CINLIMIT, 0x2e);
                        	if (ret < 0) {
                        		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
                        				__func__, CHARGERUSB_CINLIMIT);
                        		return ret;
                        	}
                           
                            //set charge current 1000mA
                            ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
                        				CHARGERUSB_VICHRG, 0x9);
                                	if (ret < 0) {
                                		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
                                				__func__, CHARGERUSB_VICHRG);
                                		return ret;
                                	}
                            //set max charge current 1000mA
                            ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
                        				CHARGERUSB_CTRLLIMIT2, 0x09);
                                	if (ret < 0) {
                                		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
                                				__func__, CHARGERUSB_VICHRG);
                                		return ret;
                                	}   
                                }
                    else
                        {
                            //printk("[%s]CXG get usb_high_current=0,so set charge current 500mA\n",__func__);
                           //set vbus in current limit 500mA
                            ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
                				CHARGERUSB_CINLIMIT, 0x09);
                        	if (ret < 0) {
                        		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
                        				__func__, CHARGERUSB_CINLIMIT);
                        		return ret;
                        	}
                           
                            //set charge current 500mA
                            ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
                        				CHARGERUSB_VICHRG, 0x04);
                                	if (ret < 0) {
                                		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
                                				__func__, CHARGERUSB_VICHRG);
                                		return ret;
                                	}
                            //set max charge current 500mA
                            ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
                        				CHARGERUSB_CTRLLIMIT2, 0x04);
                                	if (ret < 0) {
                                		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
                                				__func__, CHARGERUSB_CTRLLIMIT2);
                                		return ret;
                                	} 
                        }
                    
                    #if 0
                    if(1 == usb_high_current)
                        {
                            //printk("[%s]CXG get usb_high_current=1,so set charge current 1500mA\n",__func__);
                            //set vbus in current limit 500mA
                            ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
                				CHARGERUSB_CINLIMIT, 0x2E);
                        	if (ret < 0) {
                        		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
                        				__func__, CHARGERUSB_CINLIMIT);
                        		return ret;
                        	}
                           
                            //set charge current 500mA
                            ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
                        				CHARGERUSB_VICHRG, 0x0E);
                                	if (ret < 0) {
                                		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
                                				__func__, CHARGERUSB_VICHRG);
                                		return ret;
                                	}
                            //set max charge current 500mA
                            ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
                        				CHARGERUSB_CTRLLIMIT2, 0x0F);
                                	if (ret < 0) {
                                		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
                                				__func__, CHARGERUSB_CTRLLIMIT2);
                                		return ret;
                                	} 
                        }
                    else
                        {
                            //printk("[%s]CXG get usb_high_current=0,so set charge current 500mA\n",__func__);
                   //set vbus in current limit 500mA
                    ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
        				CHARGERUSB_CINLIMIT, 0x09);
                	if (ret < 0) {
                		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
                				__func__, CHARGERUSB_CINLIMIT);
                		return ret;
                	}
                   
                    //set charge current 500mA
                    ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
                				CHARGERUSB_VICHRG, 0x04);
                        	if (ret < 0) {
                        		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
                        				__func__, CHARGERUSB_VICHRG);
                        		return ret;
                        	}
                    //set max charge current 500mA
                    ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
                				CHARGERUSB_CTRLLIMIT2, 0x04);
                        	if (ret < 0) {
                        		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
                        				__func__, CHARGERUSB_CTRLLIMIT2);
                        		return ret;
                        	} 
                }
                    #endif
             
                }
             
            //charge enable
            ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
				CONTROLLER_CTRL1, 0x30);
        	if (ret < 0) {
        		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
        				__func__, CONTROLLER_CTRL1);
        		return ret;
        	}
            //set max charge voltage 
            ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
				CHARGERUSB_CTRLLIMIT1, MAX_CHARGE_VOLTAGE);
        	if (ret < 0) {
        		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
        				__func__, CHARGERUSB_CTRLLIMIT1);
        		return ret;
        	}
            //set charge voltage
            ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
				CHARGERUSB_VOREG, CHARGE_VOLTAGE);
        	if (ret < 0) {
        		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
        				__func__, CHARGERUSB_VOREG);
        		return ret;
        	}
        }
    else
        {
            printk("[%s]CXG  detect vbus pull out!\n",__func__);
        }   
    //Set DLIN 50mA
    ret = tps80031_read(charger->dev->parent, SLAVE_ID2,
			CONTROLLER_VSEL_COMP, &vsel_comp_reg);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in reading register 0x%02x\n",
				__func__, CONTROLLER_VSEL_COMP);
	}
    vsel_comp_reg &= 0x9f;
    vsel_comp_reg |= 0x20;//100v
    ret = tps80031_write(charger->dev->parent, SLAVE_ID2,
				CONTROLLER_VSEL_COMP, vsel_comp_reg);
        	if (ret < 0) {
        		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
        				__func__, CONTROLLER_VSEL_COMP);
        		return ret;
        	}   
}
#ifdef CONFIG_TOUCHSCREEN_FT5X06
extern void ft5x0x_usb_detect(void);
#endif
void tps80031_usb_detect_fun(void)
{
    tps80031_config_charge_parameter();
    battery_update_by_interrupt();
#ifdef CONFIG_TOUCHSCREEN_FT5X06
    ft5x0x_usb_detect();
#endif
    return ;
}

void wdg_work_fun(void)
{
    struct tps80031_charger *charger = charger_data;
    int ret;
    static int count=0;  
    ret = tps80031_write(charger->dev->parent, SLAVE_ID2, CONTROLLER_WDG,
			0xFF);
	if (ret < 0)
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CONTROLLER_WDG);
    
    schedule_delayed_work(&wdg_work,2000);
    printk("[%s]CXG  enter wdg_work_fun !\n",__func__);
    return;    
}
/*ZTE:add by caixiaoguang 20120531 for charge--*/
/*ZTE:add by caixiaoguang for set a flag before power off++*/
void tps80031_set_power_off_flag(void)
{
    power_off_flag = 1;
    printk("[%s]CXG get power_off_flag = %d\n",__func__,power_off_flag); 
    return;
}

static int tps80031_charger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct tps80031_charger *charger;
	struct tps80031_charger_platform_data *pdata = pdev->dev.platform_data;

	dev_info(dev, "%s()\n", __func__);

	if (!pdata) {
		dev_err(dev, "%s() No platform data, exiting..\n", __func__);
		return -ENODEV;
	}
#if 0
	if (!pdata->num_consumer_supplies) {
		dev_err(dev, "%s() No consumer supply list, exiting..\n",
				__func__);
		return -ENODEV;
	}
#endif
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger) {
		dev_err(dev, "failed to allocate memory status\n");
		return -ENOMEM;
	}

	charger->dev =  &pdev->dev;

	charger->max_charge_current_mA = (pdata->max_charge_current_mA) ?
					pdata->max_charge_current_mA : 1000;
	charger->max_charge_volt_mV = (pdata->max_charge_volt_mV) ?
					pdata->max_charge_volt_mV : 4200;
	charger->irq_base = pdata->irq_base;
	charger->watch_time_sec = min(pdata->watch_time_sec, 127);
	if (!charger->watch_time_sec)
		charger->watch_time_sec = 127;
	charger->charging_term_current_mA =
			min(50, pdata->charging_term_current_mA);
	if (charger->charging_term_current_mA < 50)
		charger->charging_term_current_mA = 50;

	charger->reg_desc.name = "vbus_charger";
	charger->reg_desc.id = pdata->regulator_id;
	charger->reg_desc.ops = &tegra_regulator_ops;
	charger->reg_desc.type = REGULATOR_CURRENT;
	charger->reg_desc.owner = THIS_MODULE;

	charger->reg_init_data.supply_regulator = NULL;
	charger->reg_init_data.num_consumer_supplies =
					pdata->num_consumer_supplies;
	charger->reg_init_data.consumer_supplies = pdata->consumer_supplies;
	charger->reg_init_data.regulator_init = NULL;
	charger->reg_init_data.driver_data = charger;
	charger->reg_init_data.constraints.name = "vbus_charger";
	charger->reg_init_data.constraints.min_uA = 0;
	charger->reg_init_data.constraints.max_uA =
					pdata->max_charge_current_mA * 1000;
	charger->reg_init_data.constraints.valid_modes_mask =
					REGULATOR_MODE_NORMAL |
					REGULATOR_MODE_STANDBY;
	charger->reg_init_data.constraints.valid_ops_mask =
					REGULATOR_CHANGE_MODE |
					REGULATOR_CHANGE_STATUS |
					REGULATOR_CHANGE_CURRENT;

	charger->board_init = pdata->board_init;
	charger->board_data = pdata->board_data;
	charger->state = charging_state_idle;

	charger->rdev = regulator_register(&charger->reg_desc, &pdev->dev,
					&charger->reg_init_data, charger);
	if (IS_ERR(charger->rdev)) {
		dev_err(&pdev->dev, "failed to register %s\n",
						charger->reg_desc.name);
		ret = PTR_ERR(charger->rdev);
		goto regulator_fail;
	}

	ret = request_threaded_irq(charger->irq_base + TPS80031_INT_LINCH_GATED,
			NULL, linch_status_isr,	0, "tps80031-linch", charger);
	if (ret) {
		dev_err(&pdev->dev, "Unable to register irq %d; error %d\n",
			charger->irq_base + TPS80031_INT_LINCH_GATED, ret);
		goto irq_linch_fail;
	}

    ret = request_threaded_irq(charger->irq_base + TPS80031_INT_INT_CHRG,
			NULL, vbus_ovp_status_isr,	0, "tps80031-vbus-ovp", charger);
	if (ret) {
		dev_err(&pdev->dev, "Unable to register irq %d; error %d\n",
			charger->irq_base + TPS80031_INT_INT_CHRG, ret);
		goto irq_vbus_fail;
	}
    
	ret = request_threaded_irq(charger->irq_base + TPS80031_INT_FAULT_WDG,
			NULL, watchdog_expire_isr, 0, "tps80031-wdg", charger);
	if (ret) {
		dev_err(&pdev->dev, "Unable to register irq %d; error %d\n",
			charger->irq_base + TPS80031_INT_FAULT_WDG, ret);
		goto irq_wdg_fail;
	}
    /*ZTE:add by caixiaoguang 20120531 for charge++*/
    INIT_DELAYED_WORK(&wdg_work,wdg_work_fun); 
    schedule_delayed_work(&wdg_work,2000); 
    INIT_WORK(&vbus_int_work, tps80031_usb_detect_fun);
    /*ZTE:add by caixiaoguang 20120531 for charge--*/
	dev_set_drvdata(&pdev->dev, charger);
	charger_data = charger;
    ret = configure_charging_parameter(charger);
	if (ret)
		goto config_fail;
	return ret;

config_fail:
	free_irq(charger->irq_base + TPS80031_INT_FAULT_WDG, charger);
irq_wdg_fail:
	free_irq(charger->irq_base + TPS80031_INT_LINCH_GATED, charger);
irq_vbus_fail:
	free_irq(charger->irq_base + TPS80031_INT_INT_CHRG, charger);    
irq_linch_fail:
	regulator_unregister(charger->rdev);
regulator_fail:
	kfree(charger);
	return ret;
}

#ifdef CONFIG_PM

static int tps80031_charger_suspend(struct i2c_client *client,
		pm_message_t state)
{
    //printk("[%s] \n",__func__);
	cancel_delayed_work(&wdg_work);
	return 0;
}

static int tps80031_charger_resume(struct i2c_client *client)
{
    //printk("[%s] \n",__func__);
	schedule_delayed_work(&wdg_work, 2000);
	return 0;
}

#else

#define tps80031_charger_suspend NULL
#define tps80031_charger_resume NULL

#endif /* CONFIG_PM */


static int tps80031_charger_remove(struct platform_device *pdev)
{
	struct tps80031_charger *charger = dev_get_drvdata(&pdev->dev);

	regulator_unregister(charger->rdev);
	kfree(charger);
	return 0;
}

static struct platform_driver tps80031_charger_driver = {
	.driver	= {
		.name	= "tps80031-charger",
		.owner	= THIS_MODULE,
	},
	.probe	= tps80031_charger_probe,
	.remove = tps80031_charger_remove,
	.suspend= tps80031_charger_suspend,
	.resume	= tps80031_charger_resume,
};

static int __init tps80031_charger_init(void)
{
	return platform_driver_register(&tps80031_charger_driver);
}

static void __exit tps80031_charger_exit(void)
{
	platform_driver_unregister(&tps80031_charger_driver);
}

subsys_initcall(tps80031_charger_init);
module_exit(tps80031_charger_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("tps80031 battery charger driver");
