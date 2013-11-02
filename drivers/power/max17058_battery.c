/*
 *  max17058_battery.c
 *  fuel-gauge systems for lithium-ion (Li+) batteries
 *
 *  Copyright (C) 2009 Samsung Electronics
 *  Minkyu Kang <mk7.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/max17058_battery.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include "../gpio-names.h"
#include <mach/gpio.h>
#include "../../arch/arm/mach-tegra/gpio-names.h"

#define max17058_VCELL_MSB	0x02
#define max17058_VCELL_LSB	0x03
#define max17058_SOC_MSB	0x04
#define max17058_SOC_LSB	0x05
#define max17058_MODE_MSB	0x06
#define max17058_MODE_LSB	0x07
#define max17058_VER_MSB	0x08
#define max17058_VER_LSB	0x09
#define max17058_RCOMP_MSB	0x0C
#define max17058_RCOMP_LSB	0x0D
#define max17058_OCV_MSB	0x0E
#define max17058_OCV_LSB	0x0F
#define max17058_VRESET	    0x18
#define max17058_CMD_MSB	0xFE
#define max17058_CMD_LSB	0xFF

#define max17058_DELAY		1000
#define max17058_BATTERY_FULL	95
#define max17058_BATTERY_LEVEL_TOO_LOW  2800
#define max17058_BATTERY_LEVEL_LOW 2880
#define max17058_WARN_CHARGE_PERIOD 30000
#define max17058_LED_LIGHT_TIME 200

#define VERIFY_AND_FIX 1
#define LOAD_MODEL !(VERIFY_AND_FIX)

struct max17058_chip {
	struct i2c_client		*client;
	struct delayed_work		work;
	struct work_struct             alert_work;
    struct work_struct		charge_indicate_work;//ZTE:add by caixiaoguang 20120706
	struct power_supply		battery;
	struct power_supply		ac;
	struct power_supply		usb;
	struct max17058_platform_data	*pdata;

	/* State Of Connect */
	int online;
	/* battery voltage */
	int vcell;
	/* battery capacity */
	int soc;
	/* State Of Charge */
	int status;
	/* State Of load model */
	int load_status;
	/** ZTE_MODIFY end */
};

static struct max17058_chip *charger_chip;
struct wake_lock load_model_wakelock;
static struct wake_lock battery_low_wake_lock;
static int charge_full_flag = 0;
static int charge_indicate_work_init_flag = 0;
static int original_OCV = 0;
static s16 is_suspended;
extern bool tps80031_get_ovp_status(void);
extern void tps80031_config_charge_parameter(void);
extern int tps80031_get_battery_temp(void);
extern void tps80031_gpadc_enable(void);
extern void tps80031_gpadc_disable(void);
static void handle_model(int load_or_verify, struct i2c_client *client);
static void prepare_to_load_model(struct i2c_client *client) ;
static void load_model(struct i2c_client *client);
static bool verify_model_is_correct(struct i2c_client *client);
static void cleanup_model_load(struct i2c_client *client);
extern int zte_get_board_id(void);
extern int fsl_charger_detect(void);
int max17058_get_vcell(struct i2c_client *client) ;

int max17058_vcell_filter(void)
{
    int vol_local  = 0;
    int min_vol = {0};
    int iTemp[6] = {0};
    int i,j;

    for(i=0;i<6;i++)
    {
        iTemp[i]= max17058_get_vcell(charger_chip->client);
        msleep(100);
    }

    for(i=0; i<5; i++)
        for(j=i+1; j<6; j++)
        {
            if(iTemp[j] > iTemp[i])
            {
                min_vol   = iTemp[i];
                iTemp[i]  = iTemp[j];
                iTemp[j] = min_vol;
            }
        }

    for(i=1; i<5; i++)
    {
        vol_local +=iTemp[i];
    }
    vol_local = vol_local/4;

    return vol_local;
}

static int max17058_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17058_chip *chip = container_of(psy,
				struct max17058_chip, battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->status;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->online;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chip->vcell;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = chip->soc;
		break;
        case POWER_SUPPLY_PROP_TEMP:
		val->intval = tps80031_get_battery_temp();
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max17058_ac_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17058_chip *chip = container_of(psy,
				struct max17058_chip, ac);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
{
if((1==chip->pdata->charger_online())&&(1==fsl_charger_detect()))
                                val->intval=1;
                            else                                
                                val->intval=0;                                
    		                break;
}

	default:
		return -EINVAL;
	}
	return 0;
}
static int max17058_usb_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{

	struct max17058_chip *chip = container_of(psy,
				struct max17058_chip, usb);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
{
     if((1==chip->pdata->charger_online() )&&(0==fsl_charger_detect()))        
                    val->intval=1;        
                else                    
                    val->intval=0;                    
    		    break;
}

	default:
		return -EINVAL;
	}
	return 0;
}

enum supply_type {
	SUPPLY_TYPE_BATTERY = 0,
	SUPPLY_TYPE_AC,
	SUPPLY_TYPE_USB,
};

static char *power_supplied_to[] = {
	"battery",
};

static enum power_supply_property max17058_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
};

static enum power_supply_property max17058_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property max17058_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

/*ZTE:add by caixiaoguang 20111107 for power supply--*/


static int max17058_read_reg(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

/** ZTE_MODIFY liuzhongzhi added for load customer model, liuzhongzhi0008 */
/*
 * max17058_write_word() - Write a word to max17058 register
 * @client:	The i2c client
 * @reg	  : Register to be write
 * @value : New register value
 */
static int max17058_write_word(struct i2c_client *client, u8 reg, u16 value)
{
	int ret;
	u8 data[2];
	data[0] = (value >> 8) & 0xFF;
	data[1] = value & 0xFF;

	ret = i2c_smbus_write_i2c_block_data(client, reg, 2, data);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

/*
 * max17058_write_word() - Read a word from max17058 register
 * @client:	The i2c client
 * @reg	  : Register to be read
 */
static int max17058_read_word(struct i2c_client *client, u8 reg)
{
	int ret;
	u8 data[2];

	ret = i2c_smbus_read_i2c_block_data(client, reg, 2, data);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ((data[0] << 8) | data[1]);
}

/*
 * max17058_write_block_data() - Write a blcok data to max17058 memory
 * @client:	The i2c client
 * @reg	  : Start memory addr to be write
 * @len   : Block data length
 * @value : Block data addr
 */
static int max17058_write_block_data(struct i2c_client *client, u8 reg, u8 len, const u8 *value)
{
	int ret;

	ret = i2c_smbus_write_i2c_block_data(client, reg, len, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}
/** ZTE_MODIFY end */



int max17058_get_vcell(struct i2c_client *client)
{
	struct max17058_chip *chip = i2c_get_clientdata(client);
	u8 msb;
	u8 lsb;

	msb = max17058_read_reg(client, max17058_VCELL_MSB);
	lsb = max17058_read_reg(client, max17058_VCELL_LSB);

	chip->vcell = (((msb << 4) + (lsb >> 4)) * 1250)/1000; //vcell one unit is 1.25 mv
    #ifdef CONFIG_HIGH_VOLTAGE_LEVEL_BATTERY
    if(chip->vcell > 4350)
    {
    	  chip->vcell =4350;
    }
    #else
    if(chip->vcell > 4200)
    {
    	  chip->vcell =4200;
    }
    #endif
     //printk("[%s]CXG get chip->vcell = %d mv\n",__func__,chip->vcell);
    return chip->vcell;
}

/*ZTE:  add by tengfei for optimization of battery capacity display, 2012.4.12, ++*/

/*********************************************************
*            Real                     Display
*         100~95                  100~95     Cd=Cr;
*         94~30                    94~50       Cd=Cr + 20;
*         29~7                      49~14       Cd=2Cr;
*         6~0                        13~0        Cd=3Cr;
**********************************************************/
int bat_cap_conversion(int cap)
{
	int cap_real =cap;
	int cap_disp = 0;

	if (cap_real >= 95)
	{
		cap_disp = 100;
	}
    else if(cap_real >= 80)
    {
        cap_disp = cap_real + 5;
    }
	else if( cap_real >= 30 )
	{
		cap_disp = 50 + ((cap_real - 30)*5)/7;
	}
	else if(cap_real >= 7)
	{
		cap_disp = 14 + ((cap_real - 7)*35)/22;
	}
	else
	{
		cap_disp = (cap_real*13)/6;
	}
	printk("[Max17058]  %s: Real(%d) and Dispay(%d)...\n", __func__,cap_real, cap_disp);
	return cap_disp;
}
/*ZTE:  add by tengfei for optimization of battery capacity display, 2012.4.12, --*/

//static void max17058_get_soc(struct i2c_client *client)
int max17058_get_soc(struct i2c_client *client)  //huxb fixed, 2011.02.06
{
	struct max17058_chip *chip = i2c_get_clientdata(client);
	u8 msb;
	u8 lsb;
	static int soc_last = 100;
    int vell_local =0;

	msb = max17058_read_reg(client, max17058_SOC_MSB);
	lsb = max17058_read_reg(client, max17058_SOC_LSB);
	/** ZTE_MODIFY liuzhongzhi added for load customer model, liuzhongzhi0008 */

	if (chip->pdata->ini_data.bits == 19)
		chip->soc = msb / 2;
	else
		chip->soc = msb;

	printk("[Max17058]  %s: Original soc =%d\n", __func__,chip->soc);

	if (chip->soc > 100)
	    chip->soc = soc_last;
	else
	    soc_last = chip->soc;

	chip->soc = bat_cap_conversion(chip->soc);

#if 1
    if(chip->soc <= 2)
    {
        vell_local= max17058_vcell_filter();
        printk("[%s]CXG get low power level = %d %%, voltage = %d mv\n",__func__,chip->soc,vell_local);
        if(vell_local > 3400)
        {
            chip->soc =1;
        }
        else
        {
            chip->soc = 0;
        }
    }
#endif

    if(charge_full_flag==1)
    {
        chip->soc = 100;
    }
	if(chip->soc > 100)
		chip->soc = 100;
	/** ZTE_MODIFY end */
    return chip->soc;
}

static void max17058_get_version(struct i2c_client *client)
{
	u8 msb;
	u8 lsb;

	msb = max17058_read_reg(client, max17058_VER_MSB);
	lsb = max17058_read_reg(client, max17058_VER_LSB);

	dev_info(&client->dev, "max17058 Fuel-Gauge Ver %d%d\n", msb, lsb);
}

/****************************************************************/
/* If you know the model is not loaded, call:
handle_model(LOAD_MODEL);
If you want to verify the model, and correct errors, call:
handle_model(VERIFY_AND_FIX);
*/
void handle_model(int load_or_verify, struct i2c_client *client)
{
	bool model_load_ok = false;
	int i = 0;    
    //other board id is other value ,exectute the right code
    wake_lock(&load_model_wakelock);
	do {
		// Steps 1-4
		prepare_to_load_model(client);

		if(load_or_verify == LOAD_MODEL) {
		// Step 5
		load_model(client);
		}

		// Steps 6-9
		model_load_ok = verify_model_is_correct(client);
		if(!model_load_ok) {
			load_or_verify = LOAD_MODEL;
		}
		printk("[Max17058]  %s:Load_or_verify=%d...\n", __func__, load_or_verify);
	} while ((!model_load_ok) && (i++ <3));

	printk("[Max17058]  Load or verify has been completed!\n");

	// Steps 10-11
	cleanup_model_load(client);
	wake_unlock(&load_model_wakelock);    
}

void prepare_to_load_model(struct i2c_client *client)
{
step1:

	/********************************************************
	Step 1. Unlock Model Access
	This enables access to the OCV and table registers
	*/
	max17058_write_word(client, 0x3E, 0x4A57);

	/*********************************************************
	Step 2. Read OCV
	The OCV Register will be modified during the process of loading the custom
	model. Read and store this value so that it can be written back to the
	device after the model has been loaded.
	*/
	original_OCV = max17058_read_word(client, max17058_OCV_MSB);

	/*********************************************************
	Step 2.5. Verify Model Access Unlocked
	If Model Access was correctly unlocked in Step 1, then the OCV bytes read
	in Step 2 will not be 0xFF. If the values of both bytes are 0xFF,
	that indicates that Model Access was not correctly unlocked and the
	sequence should be repeated from Step 1.
	*/
	if( original_OCV == 0xFFFF)
	{
	 	goto step1;
	}

	/***********************************************************
	Step 3. Write OCV (max17058/1/3/4 only)
	Find OCVTest_High_Byte and OCVTest_Low_Byte values in INI file
	*/
	//WriteWord(0x0E, INI_OCVTest_High_Byte, INI_OCVTest_Low_Byte);

	/***********************************************************
	Step 4. Write RCOMP to its Maximum Value (max17058/1/3/4 only)
	Make the fuel-gauge respond as slowly as possible (MSB = 0xFF), and disable
	alerts during model loading (LSB = 0x00)
	*/
	//WriteWord(0x0C, 0xFF, 0x00);
}

void load_model(struct i2c_client *client)
{
	struct max17058_chip *chip;
	struct max17058_ini_data *ini_data;
	int i;

	chip = i2c_get_clientdata(client);
	ini_data = &chip->pdata->ini_data;
	dev_info(&client->dev, "@ini_data=%p, title=%s\n", ini_data, ini_data->title);

	/*********************************************************
	Step 5. Write the Model
	Once the model is unlocked, the host software must write the 64 byte model
	to the device. The model is located between memory 0x40 and 0x7F.
	The model is available in the INI file provided with your performance
	report. See the end of this document for an explanation of the INI file.
	Note that the table registers are write-only and will always read
	0xFF. Step 9 will confirm the values were written correctly.
	*/
	for (i = 0x40; i < 0x80; i += 0x10){
		max17058_write_block_data(client, i, 16, &ini_data->data[i - 0x20]);
	}

	/************************************************************
	Step 5.1 Write RCOMPSeg (MAX17048/MAX17049 only)
	MAX17048 and MAX17049 have expanded RCOMP range to battery with high
	resistance due to low temperature or chemistry design. RCOMPSeg can be
	used to expand the range of RCOMP. For these devices, RCOMPSeg should be
	written along with the default model. For INI files without RCOMPSeg
	specified, use RCOMPSeg = 0x0080.
	*/
	//PASS
}

bool verify_model_is_correct(struct i2c_client *client)
{
	int soc1;
	struct max17058_chip *chip;
	struct max17058_ini_data *ini_data;

	chip = i2c_get_clientdata(client);
	ini_data = &chip->pdata->ini_data;

	/*******************************************************
	Step 6. Delay at least 150ms (max17058/1/3/4 only)
	This delay must be at least 150mS, but the upper limit is not critical
	in this step.
	*/
	//sleep_ms(150);

	/*******************************************************
	Step 7. Write OCV
	This OCV should produce the SOC_Check values in Step 9
	*/
	max17058_write_word(client, max17058_OCV_MSB, ini_data->ocvtest);

	/*********************************************************
	Step 7.1 Disable Hibernate (MAX17048/49 only)
	The IC updates SOC less frequently in hibernate mode, so make sure it
	is not hibernating
	*/
	//WriteWord(0x0A, 0);

	/**********************************************************
	Step 7.2. Lock Model Access (MAX17048/49/58/59 only)
	To allow the ModelGauge algorithm to run in MAX17048/49/58/59 only, the model must
	be locked. This is harmless but unnecessary for max17058/1/3/4
	*/
	max17058_write_word(client, 0x3E, 0x0000);

	/**********************************************************
	Step 8. Delay between 150ms and 600ms
	This delay must be between 150ms and 600ms. Delaying beyond 600ms could
	cause the verification to fail.
	*/
	mdelay(200);

	/***********************************************************
	Step 9. Read SOC Register and compare to expected result
	There will be some variation in the SOC register due to the ongoing
	activity of the ModelGauge algorithm. Therefore, the upper byte of the SOC
	register is verified to be within a specified range to verify that the
	model was loaded correctly. This value is not an indication of the state of
	the actual battery.
	*/
	soc1 = max17058_read_reg(client, max17058_SOC_MSB);
	printk("[Max17058]  %s: 0x%x...\n", __func__, soc1);

	if ((soc1 >= ini_data->socchecka) && (soc1 <= ini_data->soccheckb)){
		// model was loaded successfully
		return true;
	}else{
		// model was NOT loaded successfully
		return false;
	}

}

void cleanup_model_load(struct i2c_client *client)
{
	struct max17058_chip *chip;
	struct max17058_ini_data *ini_data;
	chip = i2c_get_clientdata(client);
	ini_data = &chip->pdata->ini_data;

	/********************************************************
	Step 9.1. Unlock Model Access (MAX17048/49/58/59 only)
	To write OCV, MAX17048/49/58/59 requires model access to be unlocked.
	*/
	max17058_write_word(client, 0x3E, 0x4A57);

	/********************************************************
	Step 10. Restore CONFIG and OCV
	It is up to the application how to configure the LSB of the CONFIG
	register; any byte value is valid.
	19-bit: 0x1f>>0.5%  0x1e>>1%
	*/
	max17058_write_word(client, max17058_RCOMP_MSB, (ini_data->rcomp0 << 8) | (0x1f));
	max17058_write_word(client, max17058_OCV_MSB, original_OCV);

	/*********************************************************
	Step 10.1 Restore Hibernate (MAX17048/49 only)
	Remember to restore your desired Hibernate configuration after the
	model was verified.
	*/
	// Restore your desired value of HIBRT

	/**********************************************************
	Step 11. Lock Model Access
	*/
	max17058_write_word(client, 0x3E, 0x0000);

	/***********************************************************
	Step 12. Delay
	This delay must be at least 150mS before reading the SOC Register to allow
	the correct value to be calculated by the device.
	*/
	mdelay(200);
}
/****************************************************************/


/*
 * show_load_model_status() - Show last load model status
 */
static ssize_t show_load_model_status(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct max17058_chip *chip;
	struct power_supply *psy;

	psy = dev_get_drvdata(dev);
	chip = container_of(psy, struct max17058_chip, battery);

	return sprintf(buf, "%u\n", chip->load_status);
}

/*
 * store_load_model() - Force to load model
 */
static ssize_t store_load_model(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct max17058_chip *chip;
	struct power_supply *psy;
	char *after;
	unsigned long num;

	psy = dev_get_drvdata(dev);
	chip = container_of(psy, struct max17058_chip, battery);

	num = simple_strtoul(buf, &after, 10);
	if (num)
	{
		/* adjust RCOMP register first */
		//max17058_write_word(chip->client, max17058_RCOMP_MSB, (chip->pdata->ini_data.rcomp0 << 8));
	      handle_model(LOAD_MODEL,chip->client);
	}

	return count;
}

static struct device_attribute load_model_attr =
	__ATTR(load_model, S_IRUGO | S_IWUSR, show_load_model_status, store_load_model);

/** ZTE_MODIFY end */
/*ZTE:add by caixiaoguang 20121030 for update rcomp++*/
void max17058_update_rcomp(void)
{
    //RCOMP value at 20 degrees C
    int INI_RCOMP = charger_chip->pdata->ini_data.rcomp0;
    //RCOMP change per degree for every degree above 20 degrees C(TempCoUp_from_INI_file),should div 1000
    int TempCoHot = charger_chip->pdata->ini_data.tempcoup;
    //RCOMP change per degree for every degree below 20 degrees C(TempCoDown_from_INI_file),should div 1000
    int TempCoCold = charger_chip->pdata->ini_data.tempcodown;
    static int last_temp = 20;
    int Temperature = tps80031_get_battery_temp();
    Temperature = Temperature/10;  
    int NewRCOMP = 0;    
    int reg_temp = 0;
    if(((last_temp-Temperature)>= 3)||((Temperature-last_temp)>= 3))
    {
        printk("[%s]CXG get last_temp=%d,Temperature=%d\n",__func__,last_temp,Temperature);
        last_temp = Temperature;
        if(Temperature > 20)
            {
                NewRCOMP = INI_RCOMP +((Temperature - 20)*TempCoHot)/1000;
            }
        else if(Temperature < 20)
            {
                NewRCOMP =INI_RCOMP +((Temperature - 20)*TempCoCold)/1000;
            }
        else
            {
                NewRCOMP = INI_RCOMP;
            }

        if(NewRCOMP > 0xFF)
            {
                NewRCOMP = 0xFF;
            }
        else if(NewRCOMP < 0 )
            {
                NewRCOMP = 0;
            }
        reg_temp = max17058_read_word(charger_chip->client, max17058_RCOMP_MSB);
        printk("[%s]CXG get reg_temp=0x%x,NewRCOMP=0x%x\n", __func__,reg_temp,NewRCOMP);
        reg_temp &= 0x00FF;        
        reg_temp |= (NewRCOMP<<8); 
        max17058_write_word(charger_chip->client, max17058_RCOMP_MSB, reg_temp);         
    }   
      
}
/*ZTE:add by caixiaoguang 20121030 for update rcomp--*/
static void max17058_get_online(struct i2c_client *client)
{
	struct max17058_chip *chip = i2c_get_clientdata(client);

	if (chip->pdata->battery_online)
		chip->online = chip->pdata->battery_online();
	else
		chip->online = 1;
}

static void max17058_get_status(struct i2c_client *client)
{
    struct max17058_chip *chip = i2c_get_clientdata(client);

    if (!chip->pdata->charger_online || !chip->pdata->charger_enable)
    {
		chip->status = POWER_SUPPLY_STATUS_UNKNOWN;
		return;
	}

	if (chip->pdata->charger_online())
    {
		if (chip->pdata->charger_enable())
        {
            if(0==tps80031_get_ovp_status())
                {
                    //printk("[%s]CXG get vbus_ovp_flag=0,so go on charging\n",__func__);
                    chip->status = POWER_SUPPLY_STATUS_CHARGING;
                }
            else
                {
                    //printk("[%s]CXG get vbus_ovp_flag=1,so discharging\n",__func__);
                    chip->status = POWER_SUPPLY_STATUS_NOT_CHARGING;                    
                }
    			
            tps80031_config_charge_parameter();               
    		            
                    if(chip->soc==100)
                        charge_full_flag=1;
        }
		else
	    {	    
			chip->status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

        
	}
    else
    {
        charge_full_flag=0;
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
        if (chip->soc > max17058_BATTERY_FULL)
        {
            printk("power status is full!!!\n");
    		chip->status = POWER_SUPPLY_STATUS_FULL;
        }
	}

    dev_dbg(&client->dev, "max17058 Fuel-Gauge status: %d\n", chip->status);
}

static void max17058_work(struct work_struct *work)
{
	struct max17058_chip *chip;
	static int load_time = max17058_DELAY;
	int reg_temp = 0;

	chip = container_of(work, struct max17058_chip, work.work);
    /*ZTE:add by caixiaoguang 20121009 for max17058 suspend++*/
    if ( is_suspended )
        return;
    /*ZTE:add by caixiaoguang 20121009 for max17058 suspend--*/
	max17058_get_vcell(chip->client);
	max17058_get_soc(chip->client);
	printk("CXG get battery voltage =%d mV, level = %d %%\n",chip->vcell,chip->soc);

	max17058_get_online(chip->client);
	max17058_get_status(chip->client);

	if ((load_time % (3600 * HZ)) == 0)	/* load the model per hour */
	{
		handle_model(VERIFY_AND_FIX, chip->client);
		load_time = 0;
	}
	load_time += max17058_DELAY;

	power_supply_changed(&chip->battery);
	power_supply_changed(&chip->ac);
	power_supply_changed(&chip->usb);

    max17058_update_rcomp();
    /*Renable max17058 alert interrupt*/
	reg_temp = max17058_read_word(charger_chip->client, max17058_RCOMP_MSB);
	printk("[Max17058]  %s: Register 0x0d is 0x%x...\n", __func__, (reg_temp & 0x00ff));
	if(reg_temp & 0x0020)
	{
		max17058_write_word(charger_chip->client, max17058_RCOMP_MSB, (reg_temp & 0xffdf));
	}

	schedule_delayed_work(&chip->work, max17058_DELAY);
}

static irqreturn_t battery_alert_irq(int irq, void *dev_id)
{
	struct max17058_chip *chip = dev_id;

	printk("[Max17058]  %s !!!\n", __func__);
	schedule_work(&chip->alert_work);
    wake_lock_timeout(&battery_low_wake_lock, msecs_to_jiffies(10000));
	return IRQ_HANDLED;
}

void battery_update_by_interrupt(void)
{
    if(1==charge_indicate_work_init_flag)
    {
    schedule_work(&charger_chip->charge_indicate_work); 
    }   
    printk("[%s]  CXG enter battery_update_by_interrupt !!!\n", __func__);
}
static void charge_indicate_work_func(struct work_struct *work)
{
    printk("CXG  enter  into charge_indicate_work_func!!\n");
    max17058_get_soc(charger_chip->client); 
    max17058_get_status(charger_chip->client);    
    power_supply_changed(&charger_chip->battery);     
    power_supply_changed(&charger_chip->ac); 
    power_supply_changed(&charger_chip->usb);
}
/*ZTE:add by caixiaoguang 20121026 for set vreset++*/
void max17058_set_reset_threshold(void)
{
    int reg_temp = 0;
    reg_temp = max17058_read_word(charger_chip->client, max17058_VRESET);	
    reg_temp &=0x00FF;
    reg_temp |=0x8200;
    max17058_write_word(charger_chip->client, max17058_VRESET, reg_temp);		   
}
/*ZTE:add by caixiaoguang 20121026 for set vreset++*/
static void max17058_alert_func(struct work_struct *work)
{
	int voltage  = 0;
	int soc = 0;
	int reg_temp = 0;

	voltage = max17058_get_vcell(charger_chip->client);
	soc = max17058_get_soc(charger_chip->client);
	reg_temp = max17058_read_word(charger_chip->client, max17058_RCOMP_MSB);

	printk("[Max17058]  %s, alert=%x vol = %d mv level = %d %%...\n", __func__, (reg_temp & 0x0020),
			voltage, soc);

	power_supply_changed(&charger_chip->battery);
}

static int __devinit max17058_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17058_chip *chip;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->pdata = client->dev.platform_data;
	i2c_set_clientdata(client, chip);

	chip->battery.name		= "battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= max17058_get_property;
	chip->battery.properties	= max17058_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(max17058_battery_props);

    ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		kfree(chip);
		return ret;
	}

    chip->ac.name = "ac",
    chip->ac.type = POWER_SUPPLY_TYPE_MAINS,
    chip->ac.supplied_to = power_supplied_to,
    chip->ac.num_supplicants = ARRAY_SIZE(power_supplied_to),
    chip->ac.properties = max17058_ac_props,
    chip->ac.num_properties = ARRAY_SIZE(max17058_ac_props),
    chip->ac.get_property = max17058_ac_get_property,

	ret = power_supply_register(&client->dev, &chip->ac);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		kfree(chip);
		return ret;
	}

    chip->usb.name = "usb",
    chip->usb.type = POWER_SUPPLY_TYPE_USB,
    chip->usb.supplied_to = power_supplied_to,
    chip->usb.num_supplicants = ARRAY_SIZE(power_supplied_to),
    chip->usb.properties = max17058_usb_props,
    chip->usb.num_properties = ARRAY_SIZE(max17058_usb_props),
    chip->usb.get_property = max17058_usb_get_property,

    ret = power_supply_register(&client->dev, &chip->usb);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		kfree(chip);
		return ret;
	}

	ret = device_create_file(chip->battery.dev, &load_model_attr);
	if (ret) {
		dev_err(&client->dev, "failed: create loadmodel file\n");
	}

	charger_chip = chip;
    max17058_set_reset_threshold();
	INIT_WORK(&chip->alert_work, max17058_alert_func);
    INIT_WORK(&chip->charge_indicate_work, charge_indicate_work_func);/*ZTEadd by caixiaoguang for charge indicate*/
    charge_indicate_work_init_flag = 1;
    wake_lock_init(&battery_low_wake_lock, WAKE_LOCK_SUSPEND, "battery_low_lock");
	ret = request_irq(chip->client->irq, battery_alert_irq,
					IRQF_DISABLED | IRQF_TRIGGER_FALLING ,"max17058", chip);
	if (ret < 0)
		printk( "failed to request battery_alert_irq %d\n", ret);
	enable_irq_wake(chip->client->irq);

    handle_model(LOAD_MODEL,chip->client);
    max17058_update_rcomp();
	max17058_get_version(client);

	INIT_DELAYED_WORK_DEFERRABLE(&chip->work, max17058_work);
	schedule_delayed_work(&chip->work, max17058_DELAY);
    is_suspended = false;
	return 0;
}

static int __devexit max17058_remove(struct i2c_client *client)
{
	struct max17058_chip *chip = i2c_get_clientdata(client);
	free_irq(chip->client->irq, chip);
	device_remove_file(chip->battery.dev, &load_model_attr);	/** ZTE_MODIFY liuzhongzhi added for load customer model, liuzhongzhi0008 */
	power_supply_unregister(&chip->battery);
	power_supply_unregister(&chip->ac);
	power_supply_unregister(&chip->usb);
	cancel_delayed_work(&chip->work);
	kfree(chip);
	return 0;
}

#ifdef CONFIG_PM
static int max17058_suspend(struct i2c_client *client,
		pm_message_t state)
{
	struct max17058_chip *chip = i2c_get_clientdata(client);
	int reg_temp = 0;

	cancel_delayed_work(&chip->work);

	reg_temp = max17058_read_word(chip->client, max17058_RCOMP_MSB);
	if(reg_temp & 0x0020)
	{
		printk("[Max17058]  %s: Register 0x0d is 0x%x...\n", __func__, (reg_temp & 0x00ff));
		max17058_write_word(chip->client, max17058_RCOMP_MSB, (reg_temp & 0xffdf));
	}
    tps80031_gpadc_disable();
    is_suspended = true;
	return 0;
}

static int max17058_resume(struct i2c_client *client)
{
	struct max17058_chip *chip = i2c_get_clientdata(client);
    is_suspended =false;
    tps80031_gpadc_enable();
	schedule_delayed_work(&chip->work, HZ/2);

	return 0;
}
#else
#define max17058_suspend NULL
#define max17058_resume NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id max17058_id[] = {
	{ "max17058", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17058_id);

static struct i2c_driver max17058_i2c_driver = {
	.driver	= {
		.name	= "max17058",
	},
	.probe		= max17058_probe,
	.remove		= __devexit_p(max17058_remove),
	.suspend	= max17058_suspend,
	.resume		= max17058_resume,
	.id_table	= max17058_id,
};

static int __init max17058_init(void)
{
	wake_lock_init(&load_model_wakelock, WAKE_LOCK_SUSPEND, "Load_model");
	return i2c_add_driver(&max17058_i2c_driver);
}
module_init(max17058_init);

static void __exit max17058_exit(void)
{
	wake_lock_destroy(&load_model_wakelock);
	i2c_del_driver(&max17058_i2c_driver);
}
module_exit(max17058_exit);

MODULE_AUTHOR("Minkyu Kang <mk7.kang@samsung.com>");
MODULE_DESCRIPTION("max17058 Fuel Gauge");
MODULE_LICENSE("GPL");
