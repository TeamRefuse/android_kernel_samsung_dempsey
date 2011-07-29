/*
 * linux/drivers/power/s3c6410_battery.c
 *
 * Battery measurement code for S3C6410 platform.
 *
 * based on palmtx_battery.c
 *
 * Copyright (C) 2009 Samsung Electronics.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <asm/mach-types.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/mfd/max8998.h>
#include <linux/mfd/max8998-private.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <mach/battery.h>
#include <mach/hardware.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-gpio.h>
#include <mach/adc.h>
#include <plat/gpio-cfg.h>
#include <linux/android_alarm.h>
#include "s5pc110_battery.h"

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/hrtimer.h>

#if (defined CONFIG_S5PC110_DEMPSEY_BOARD)
extern unsigned int HWREV;
#endif


struct battery_info {
	int batt_temp;		/* Battery Temperature (C) from ADC */
	u32 batt_temp_adc;	/* Battery Temperature ADC value */
	u32 batt_health;	/* Battery Health (Authority) */
	u32 dis_reason;
	u32 batt_vcell;
	u32 batt_soc;
	u32 charging_status;
	bool batt_is_full;      /* 0 : Not full 1: Full */
#if defined (ATT_TMO_COMMON)
	u32 event_dev_state;  		 /* Battery charging in using a Event which consume lots of current  */
	u32 batt_is_recharging;  		 /* Battery Recharing Flag */
	u32 batt_current;              	 /* The figure which is a cumsuing current in charging mode  */
	u32 batt_v_f_adc;                     /* Battery VF ADC value */ 
#endif
};

struct adc_sample_info {
	unsigned int cnt;
	int total_adc;
	int average_adc;
	int adc_arr[ADC_TOTAL_COUNT];
	int index;
};

struct chg_data {
	struct device		*dev;
	struct max8998_dev	*iodev;
	struct work_struct	bat_work;
	struct max8998_charger_data *pdata;

	struct power_supply	psy_bat;
	struct power_supply	psy_usb;
	struct power_supply	psy_ac;
	struct workqueue_struct *monitor_wqueue;
	struct wake_lock	vbus_wake_lock;
	struct wake_lock	work_wake_lock;
	struct wake_lock	lowbat_wake_lock;
	struct adc_sample_info	adc_sample[ENDOFADC];
	struct battery_info	bat_info;
	struct mutex		mutex;
	struct timer_list	bat_work_timer;

	enum cable_type_t	cable_status;
	int			charging;
	bool			set_charge_timeout;
	bool			lowbat_warning;
	int			present;
	u8			esafe;
	bool			set_batt_full;
	unsigned long		discharging_time;
	struct max8998_charger_callbacks callbacks;
};

static bool lpm_charging_mode;
extern struct usb_gadget *usb_gadget_p;

static char *supply_list[] = {
	"battery",
};

static enum power_supply_property max8998_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static enum power_supply_property s3c_power_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

#if defined (ATT_TMO_COMMON)
extern int  max17040_lowbat_warning;
extern int  max17040_low_battery;

extern bool jack_mic_bias;

int  ce_for_fuelgauge;
EXPORT_SYMBOL(ce_for_fuelgauge);

static struct i2c_client *max8998_i2c;
static struct chg_data *max8998_chg;


static unsigned int event_start_time_msec;
static unsigned int event_total_time_msec;

static unsigned int global_cable_status;         	// cable_type_t

static int count_check_chg_current;
static int count_check_recharging_bat;

static int charging_state;
static int VF_count;

static int  b_charing; 
static int  b_cable_status; 
static int  b_bat_info_dis_reason; 
static int  b_chg_esafe;
static int  b_bat_info_batt_soc;
static int  b_set_batt_full;
static int  b_bat_info_batt_is_full;
static int  b_bat_info_batt_temp;
static int  b_bat_info_batt_health;
static int  b_bat_info_batt_vcell;
static int  b_bat_info_event_dev_state;
static int  b_bat_info_batt_is_recharging;
static int  b_bat_info_batt_current;
static int  b_bat_info_batt_v_f_adc;

static int latest_cable_status;
	
static int stepchargingCount;
static u8 stepchargingreg_buff[2];   // max8998 0x0c, 0x0d
static struct timer_list chargingstep_timer;
static struct work_struct stepcharging_work;
static unsigned char cnt_lpm_mode;   // HDLNC_BP_OPK_20110228
#define CHARGINGSTEP_INTERVAL	50

//NAGSM_Android_SEL_Kernel_Aakash_20100312
struct class *battery_class;
struct device *s5pc110bat_dev;
struct regulator *vADC_regulator; /*LDO4*/



EXPORT_SYMBOL(vADC_regulator);


static int vADC_regulator_intialized;

static struct hrtimer charger_timer;
struct work_struct pmic_int_work;
struct workqueue_struct *pmic_int_wq;

static int event_occur = 0;	
static unsigned int event_start_time_msec;
static unsigned int event_total_time_msec;


#endif
static ssize_t s3c_bat_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf);

static ssize_t s3c_bat_store_attrs(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count);

#define SEC_BATTERY_ATTR(_name)						\
{									\
	.attr = {.name = #_name, .mode = 0664, .owner = THIS_MODULE },	\
	.show = s3c_bat_show_attrs,					\
	.store = s3c_bat_store_attrs,					\
}

static struct device_attribute s3c_battery_attrs[] = {
  	SEC_BATTERY_ATTR(batt_vol),
  	SEC_BATTERY_ATTR(batt_vol_adc),
#if defined (ATT_TMO_COMMON)
       SEC_BATTERY_ATTR(batt_vol_adc_cal),
#endif
	SEC_BATTERY_ATTR(batt_temp),
	SEC_BATTERY_ATTR(batt_temp_adc),
#if (defined CONFIG_S5PC110_DEMPSEY_BOARD)
        SEC_BATTERY_ATTR(LCD_temp_adc),
#endif
#if defined (ATT_TMO_COMMON)
       SEC_BATTERY_ATTR(batt_temp_adc_cal),
	SEC_BATTERY_ATTR(batt_vol_adc_aver),
	SEC_BATTERY_ATTR(batt_test_mode),
	SEC_BATTERY_ATTR(batt_vol_aver),
	SEC_BATTERY_ATTR(batt_temp_aver),
	SEC_BATTERY_ATTR(batt_temp_adc_aver),
	SEC_BATTERY_ATTR(batt_v_f_adc),
	SEC_BATTERY_ATTR(batt_chg_current),
	SEC_BATTERY_ATTR(charging_source),
	SEC_BATTERY_ATTR(vibrator),
	SEC_BATTERY_ATTR(camera),
	SEC_BATTERY_ATTR(mp3),
	SEC_BATTERY_ATTR(video),
	SEC_BATTERY_ATTR(talk_gsm),
	SEC_BATTERY_ATTR(talk_wcdma),
	SEC_BATTERY_ATTR(data_call),
	SEC_BATTERY_ATTR(wifi),
	SEC_BATTERY_ATTR(gps),	
	SEC_BATTERY_ATTR(device_state),
	SEC_BATTERY_ATTR(batt_compensation),
	SEC_BATTERY_ATTR(is_booting),
#endif
	SEC_BATTERY_ATTR(fg_soc),
	SEC_BATTERY_ATTR(reset_soc),
	SEC_BATTERY_ATTR(charging_mode_booting),
	SEC_BATTERY_ATTR(batt_temp_check),
	SEC_BATTERY_ATTR(batt_full_check),
};

extern u8 FSA9480_Get_JIG_Status(void);
extern u8 FSA9480_Get_JIG_UART_Status(void);
static void s3c_temp_control(struct chg_data *chg, int mode);
static void s3c_set_chg_en(struct chg_data *chg, int enable);

unsigned char maxim_chg_status();
extern int FSA9480_Get_I2C_USB_Status(void);
extern int set_tsp_for_ta_detect(int state);

#if defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
static int is_camera_charging_on = 0;
#define CAM_CHG_EN	S5PV210_MP01(0)
#define CAM_CHG		S5PV210_GPH2(3)
#endif


#if defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
static void MAX8922_hw_init(void)
{	
	s3c_gpio_cfgpin(CAM_CHG_EN, S3C_GPIO_SFN(0x1));   
	s3c_gpio_setpull(CAM_CHG_EN, S3C_GPIO_PULL_NONE);
	gpio_set_value(CAM_CHG_EN, 1);
	
	s3c_gpio_cfgpin(CAM_CHG, S3C_GPIO_SFN(0x0));   
	s3c_gpio_setpull(CAM_CHG, S3C_GPIO_PULL_NONE);	
}
#endif


void PMIC_dump(void)
{
    int count,kcount;
    u8 reg_buff[0x40]; 	


    for(count = 0; count<0x40 ; count++)
    	{
            reg_buff[count] = 0;
	     max8998_read_reg(max8998_i2c, count, &reg_buff[count]);
    	}
	
            printk(" PMIC dump =========================== \n ");	
  
    for(kcount = 0; kcount < 8 ; kcount++)
    	{
    	     for(count = 0;count < 8 ; count++)
	     printk(" [%x] = %x ", count+(8*kcount), reg_buff[count+(8*kcount)]);
	     printk("  \n ");			 
    	}

}
EXPORT_SYMBOL(PMIC_dump);

#if defined (ATT_TMO_COMMON)
/*===========================================================================
// [[ junghyunseok edit for stepcharging workqueue function 20100517
FUNCTION chargingstep_timer_func                                

DESCRIPTION
    to apply  step increment current 

INPUT PARAMETERS

RETURN VALUE

DEPENDENCIES
SIDE EFFECTS
EXAMPLE 

===========================================================================*/

static void chargingstep_timer_func(unsigned long param)
{
//	struct chg_data *chg = (struct chg_data *)param;
	bat_dbg(" [BAT] %s   \n", __func__);

	schedule_work(&stepcharging_work);
}



static void maxim_stepcharging_work(struct work_struct *work)
{

       stepchargingCount++; 

       if(  max8998_chg->cable_status == CABLE_TYPE_AC)
       {
       	bat_dbg("[BAT]:%s,PM_CHARGER_TA ,  stepchargingCount = %d\n", __func__, stepchargingCount);

	      if(stepchargingCount == 1)				
           	{            // 90mA
			stepchargingreg_buff[0] = (stepchargingreg_buff[0] & 0xF8) | 0x00;
			max8998_write_reg(max8998_i2c, MAX8998_REG_CHGR1,stepchargingreg_buff[0]);
			max8998_write_reg(max8998_i2c, MAX8998_REG_CHGR2,stepchargingreg_buff[1]);
			mod_timer(&chargingstep_timer, jiffies + msecs_to_jiffies(CHARGINGSTEP_INTERVAL));				
	        }
	    else if(stepchargingCount == 2)
		{             //380mA
			stepchargingreg_buff[0] = (stepchargingreg_buff[0] & 0xF8) | 0x01;
			max8998_write_reg(max8998_i2c, MAX8998_REG_CHGR1,stepchargingreg_buff[0]);
			max8998_write_reg(max8998_i2c, MAX8998_REG_CHGR2,stepchargingreg_buff[1]);
			mod_timer(&chargingstep_timer, jiffies + msecs_to_jiffies(CHARGINGSTEP_INTERVAL));				
              }		 	
	     else if(stepchargingCount == 3)
		{             //475mA
			stepchargingreg_buff[0] = (stepchargingreg_buff[0] & 0xF8) | 0x02;
			max8998_write_reg(max8998_i2c, MAX8998_REG_CHGR1,stepchargingreg_buff[0]);
			max8998_write_reg(max8998_i2c, MAX8998_REG_CHGR2,stepchargingreg_buff[1]);
			mod_timer(&chargingstep_timer, jiffies + msecs_to_jiffies(CHARGINGSTEP_INTERVAL));				
		}
	    else if(stepchargingCount == 4)
		{             //600mA
			stepchargingCount = 0;
			stepchargingreg_buff[0] = (stepchargingreg_buff[0] & 0xF8) | 0x05;
			max8998_write_reg(max8998_i2c, MAX8998_REG_CHGR1,stepchargingreg_buff[0]);
			max8998_write_reg(max8998_i2c, MAX8998_REG_CHGR2,stepchargingreg_buff[1]);
		}
	     else
		{
				stepchargingCount = 0;
		}
       }		
       else if(max8998_chg->cable_status == CABLE_TYPE_USB)
       {
       	bat_dbg("[BAT]:%s,PM_CHARGER_USB_INSERT ,  stepchargingCount = %d\n", __func__, stepchargingCount);
       

	      if(stepchargingCount == 1)				
           	{            // 90mA
				stepchargingreg_buff[0] = (stepchargingreg_buff[0] & 0xF8) | 0x00;
				max8998_write_reg(max8998_i2c, MAX8998_REG_CHGR1,stepchargingreg_buff[0]);
				max8998_write_reg(max8998_i2c, MAX8998_REG_CHGR2,stepchargingreg_buff[1]);
				mod_timer(&chargingstep_timer, jiffies + msecs_to_jiffies(CHARGINGSTEP_INTERVAL));				
		    }
	    else if(stepchargingCount == 2)
		    {             //380mA
			    	stepchargingreg_buff[0] = (stepchargingreg_buff[0] & 0xF8) | 0x01;
				max8998_write_reg(max8998_i2c, MAX8998_REG_CHGR1,stepchargingreg_buff[0]);
				max8998_write_reg(max8998_i2c, MAX8998_REG_CHGR2,stepchargingreg_buff[1]);
				mod_timer(&chargingstep_timer, jiffies + msecs_to_jiffies(CHARGINGSTEP_INTERVAL));				
		    }		 	
	    else if(stepchargingCount == 3)
		    {             //475mA
            	           	stepchargingCount = 0;
			    	stepchargingreg_buff[0] = (stepchargingreg_buff[0] & 0xF8) | 0x02;
				max8998_write_reg(max8998_i2c, MAX8998_REG_CHGR1,stepchargingreg_buff[0]);
				max8998_write_reg(max8998_i2c, MAX8998_REG_CHGR2,stepchargingreg_buff[1]);
		    }
	   else
		   {
				stepchargingCount = 0;
		   }
      	}	
	else
	{
		//disable charge
		stepchargingreg_buff[1] = (stepchargingreg_buff[1] | 0x01);		
		max8998_write_reg(max8998_i2c, MAX8998_REG_CHGR2,stepchargingreg_buff[1]);		
	}
	
}
#endif

#ifndef ATT_TMO_COMMON
static void max8998_lowbat_config(struct chg_data *chg, int on)
{
	struct i2c_client *i2c = chg->iodev->i2c;

	if (on) {
		if (!chg->lowbat_warning)
			max8998_update_reg(i2c, MAX8998_REG_ONOFF3, 0x1, 0x1); //lowbat1
		max8998_update_reg(i2c, MAX8998_REG_ONOFF3, 0x2, 0x2); //lowbat2
	} else
		max8998_update_reg(i2c, MAX8998_REG_ONOFF3, 0x0, 0x3);
}

static void max8998_lowbat_warning(struct chg_data *chg)
{
	bat_dbg("%s\n", __func__);
	wake_lock_timeout(&chg->lowbat_wake_lock, 5 * HZ);
	chg->lowbat_warning = 1;
}

static void max8998_lowbat_critical(struct chg_data *chg)
{
	bat_dbg("%s\n", __func__);
	wake_lock_timeout(&chg->lowbat_wake_lock, 30 * HZ);
	chg->bat_info.batt_soc = 0;
}
#endif

static int max8998_charging_control(struct chg_data *chg)
{
        struct i2c_client *i2c = chg->iodev->i2c;
	static int prev_charging = -1, prev_cable = -1;
	int ret = 0;
	
	bat_dbg(" [BAT] 0  %s  \n", __func__);

	if ((prev_charging == chg->charging) && (prev_cable == chg->cable_status))
		return 0;
/*
	bat_dbg("%s : charging(%d) cable_status(%d) dis_reason(%X) esafe(%d) bat(%d,%d,%d) batt_temp(%d) batt_health(%d) batt_vcell(%d)  \
		event_dev_state(%x) batt_is_recharging(%d) batt_current(%d) \n", __func__,                                                                                     \
		chg->charging, chg->cable_status, chg->bat_info.dis_reason, chg->esafe,                                                                                      \
		chg->bat_info.batt_soc, chg->set_batt_full, chg->bat_info.batt_is_full, chg->bat_info.batt_temp, chg->bat_info.batt_health,              \
		chg->bat_info.batt_vcell, chg->bat_info.event_dev_state, chg->bat_info.batt_is_recharging, chg->bat_info.batt_current);             
*/
	if (!chg->charging) {
	
		/* disable charging */
		ret = max8998_write_reg(i2c, MAX8998_REG_CHGR2,
			(chg->esafe		<< MAX8998_SHIFT_ESAFEOUT) |
#if defined (ATT_TMO_COMMON)
			(MAX8998_CHGTIME_DISABLE	<< MAX8998_SHIFT_FT) |
#else
			(MAX8998_CHGTIME_7HR	<< MAX8998_SHIFT_FT) |
#endif
			(MAX8998_CHGEN_DISABLE	<< MAX8998_SHIFT_CHGEN));
		if (ret < 0)
			goto err;
	} else {
		/* enable charging */
		if (chg->cable_status == CABLE_TYPE_AC) {
			/* ac */
#if defined (ATT_TMO_COMMON)
			stepchargingreg_buff[0] = (MAX8998_TOPOFF_20	<< MAX8998_SHIFT_TOPOFF) |
								         (MAX8998_RSTR_DISABLE	<< MAX8998_SHIFT_RSTR) |
								         (MAX8998_ICHG_600	<< MAX8998_SHIFT_ICHG);
#else
			if (chg->set_batt_full)
				ret = max8998_write_reg(i2c, MAX8998_REG_CHGR1,
					(MAX8998_TOPOFF_10	<< MAX8998_SHIFT_TOPOFF) |
					(MAX8998_RSTR_DISABLE	<< MAX8998_SHIFT_RSTR) |
					(MAX8998_ICHG_600	<< MAX8998_SHIFT_ICHG));
			else
				ret = max8998_write_reg(i2c, MAX8998_REG_CHGR1,
					(MAX8998_TOPOFF_20	<< MAX8998_SHIFT_TOPOFF) |
					(MAX8998_RSTR_DISABLE	<< MAX8998_SHIFT_RSTR) |
					(MAX8998_ICHG_600	<< MAX8998_SHIFT_ICHG));
			if (ret < 0)
				goto err;
#endif

#if defined (ATT_TMO_COMMON)
			stepchargingreg_buff[1] = (chg->esafe		<< MAX8998_SHIFT_ESAFEOUT) |
								         (MAX8998_CHGTIME_DISABLE	<< MAX8998_SHIFT_FT) |
									  (MAX8998_CHGEN_ENABLE	<< MAX8998_SHIFT_CHGEN);			 
#else
			ret = max8998_write_reg(i2c, MAX8998_REG_CHGR2,
				(chg->esafe		<< MAX8998_SHIFT_ESAFEOUT) |
				(MAX8998_CHGTIME_7HR	<< MAX8998_SHIFT_FT) |
				(MAX8998_CHGEN_ENABLE	<< MAX8998_SHIFT_CHGEN));
			if (ret < 0)
				goto err;
#endif
		} else {

#if defined (ATT_TMO_COMMON)
			stepchargingreg_buff[0] =  (MAX8998_TOPOFF_30	<< MAX8998_SHIFT_TOPOFF) |
									   (MAX8998_RSTR_DISABLE	<< MAX8998_SHIFT_RSTR) |
					                               (MAX8998_ICHG_475	<< MAX8998_SHIFT_ICHG);
#else
			/* usb */
			if (chg->set_batt_full)
				ret = max8998_write_reg(i2c, MAX8998_REG_CHGR1,
					(MAX8998_TOPOFF_25	<< MAX8998_SHIFT_TOPOFF) |
					(MAX8998_RSTR_DISABLE	<< MAX8998_SHIFT_RSTR) |
					(MAX8998_ICHG_475	<< MAX8998_SHIFT_ICHG));
			else
				ret = max8998_write_reg(i2c, MAX8998_REG_CHGR1,
					(MAX8998_TOPOFF_30	<< MAX8998_SHIFT_TOPOFF) |
					(MAX8998_RSTR_DISABLE	<< MAX8998_SHIFT_RSTR) |
					(MAX8998_ICHG_475	<< MAX8998_SHIFT_ICHG));
			if (ret < 0)
				goto err;
#endif

#if defined (ATT_TMO_COMMON)
			stepchargingreg_buff[1] = (chg->esafe		<< MAX8998_SHIFT_ESAFEOUT) |
								         (MAX8998_CHGTIME_DISABLE	<< MAX8998_SHIFT_FT) |
						                       (MAX8998_CHGEN_ENABLE	<< MAX8998_SHIFT_CHGEN);		
#else
			ret = max8998_write_reg(i2c, MAX8998_REG_CHGR2,
				(chg->esafe		<< MAX8998_SHIFT_ESAFEOUT) |
				(MAX8998_CHGTIME_7HR	<< MAX8998_SHIFT_FT) |
				(MAX8998_CHGEN_ENABLE	<< MAX8998_SHIFT_CHGEN));
			if (ret < 0)
				goto err;
#endif
		}
#if defined (ATT_TMO_COMMON)
		prev_charging = chg->charging;
		prev_cable = chg->cable_status;

		// step charging 
		stepchargingCount = 0;
		mod_timer(&chargingstep_timer, jiffies + msecs_to_jiffies(CHARGINGSTEP_INTERVAL));
#endif
	}

	prev_charging = chg->charging;
	prev_cable = chg->cable_status;

	return false;
err:
	pr_err("max8998_read_reg error\n");
	return ret;
}


extern struct i2c_client * max8998_I2CPTR;
int max8998_check_vdcin(void)
{
//        struct i2c_client *i2c = chg->iodev->i2c;
	u8 data = 0;
	int ret;
	
	bat_dbg(" [BAT] %s  \n", __func__);

	ret = max8998_read_reg(max8998_I2CPTR, MAX8998_REG_STATUS2, &data);

	if (ret < 0) {
		pr_err("max8998_read_reg error\n");
		return ret;
	}

	return (data & MAX8998_MASK_VDCIN) ? true : false;
}
EXPORT_SYMBOL(max8998_check_vdcin);


/*
static int max8998_check_valid_battery(struct chg_data *chg)
{
        struct i2c_client *i2c = chg->iodev->i2c;
	u8 data = 0;
	int ret;

	ret = max8998_read_reg(i2c, MAX8998_REG_STATUS2, &data);

	if (ret < 0) {
		pr_err("max8998_read_reg error\n");
		return ret;
	}

	return (data & MAX8998_MASK_DETBAT) ? 0 : 1;
}
*/


static bool max8998_get_vdcin(struct max8998_charger_callbacks *ptr)
{
	struct chg_data *chg = container_of(ptr, struct chg_data, callbacks);
	bat_dbg(" [BAT] %s  \n", __func__);
	return (max8998_check_vdcin() == true);
}






static void max8998_set_cable(struct max8998_charger_callbacks *ptr,
				enum cable_type_t status)
{

#if 0
	struct chg_data *chg = container_of(ptr, struct chg_data, callbacks);

	if(max8998_check_vdcin())
	  {
			 printk(" [BAT] %s	vdcin OK  YES \n", __func__);
	  		 chg->cable_status = status;
	  }
	else
	  {
		  printk(" [BAT] %s  vdcin NO [CABLE_TYPE_NONE]\n", __func__);
		  chg->cable_status = CABLE_TYPE_NONE;
	  }

#if defined (ATT_TMO_COMMON)

	if(chg->cable_status != CABLE_TYPE_NONE) 	// USB, TA is inserted 
	{
		 set_tsp_for_ta_detect_flag = 1;
		 
	 	if(chg->bat_info.batt_health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) 
		{
			s3c_temp_control(chg,POWER_SUPPLY_HEALTH_GOOD);
		}		 
	}
	else // NON-Inserted 
	{
		set_tsp_for_ta_detect_flag = 0;
	}


	if( chg->cable_status == CABLE_TYPE_NONE)
		del_timer_sync(&chargingstep_timer);
	
	if(lpm_charging_mode)
	{
		usb_gadget_p->ops->vbus_session(usb_gadget_p, 0);
	}

#endif

	chg->lowbat_warning = false;
	if (chg->esafe == MAX8998_ESAFE_ALLOFF)
		chg->esafe = MAX8998_USB_VBUS_AP_ON;
	bat_dbg("%s : cable_status(%d) esafe(%d)\n", __func__, status, chg->esafe);
	power_supply_changed(&chg->psy_ac);
	power_supply_changed(&chg->psy_usb);
	wake_lock(&chg->work_wake_lock);
	queue_work(chg->monitor_wqueue, &chg->bat_work);

#endif	
}

static bool max8998_set_esafe(struct max8998_charger_callbacks *ptr, u8 esafe)
{
	struct chg_data *chg = container_of(ptr, struct chg_data, callbacks);
	if (esafe > 3) {
		pr_err("%s : esafe value must not be bigger than 3\n", __func__);
		return false;
	}
	chg->esafe = esafe;
	max8998_update_reg(chg->iodev->i2c, MAX8998_REG_CHGR2,
		(esafe << MAX8998_SHIFT_ESAFEOUT), MAX8998_MASK_ESAFEOUT);
	bat_dbg("%s : esafe = %d\n", __func__, esafe);
	return true;
}



static void check_lpm_charging_mode(struct chg_data *chg)
{
	bat_dbg(" [BAT] %s  \n", __func__);
	if (readl(S5P_INFORM5)) {
		lpm_charging_mode = true;
		if (max8998_check_vdcin() != true)
			if (pm_power_off)
				pm_power_off();
	} else
		lpm_charging_mode = false;

	bat_dbg("%s : lpm_charging_mode(%d)\n", __func__, lpm_charging_mode);
}

bool charging_mode_get(void)
{
	bat_dbg(" [BAT] %s  \n", __func__);
	return lpm_charging_mode;
}
EXPORT_SYMBOL(charging_mode_get);

static int s3c_bat_get_property(struct power_supply *bat_ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct chg_data *chg = container_of(bat_ps, struct chg_data, psy_bat);

//	bat_dbg(" [BAT] %s  \n", __func__);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chg->bat_info.charging_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chg->bat_info.batt_health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chg->present;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = chg->bat_info.batt_temp;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		/* battery is always online */
		val->intval = true;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (chg->pdata &&
			 chg->pdata->psy_fuelgauge &&
			 chg->pdata->psy_fuelgauge->get_property &&
			 chg->pdata->psy_fuelgauge->get_property(
				chg->pdata->psy_fuelgauge, psp, val) < 0)
			return -EINVAL;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (chg->bat_info.batt_soc == 0)
			val->intval = 0;
		else if (chg->set_batt_full)
			val->intval = 100;
		else if (chg->pdata &&
			 chg->pdata->psy_fuelgauge &&
			 chg->pdata->psy_fuelgauge->get_property &&
			 chg->pdata->psy_fuelgauge->get_property(
				chg->pdata->psy_fuelgauge, psp, val) < 0)
			return -EINVAL;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	default:
		return -EINVAL;
	}
	return false;
}

static int s3c_usb_get_property(struct power_supply *ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct chg_data *chg = container_of(ps, struct chg_data, psy_usb);
	bat_dbg(" [BAT] %s  \n", __func__);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the USB charger is connected */
	val->intval = ((chg->cable_status == CABLE_TYPE_USB) &&
			max8998_check_vdcin());

	return false;
}

static int s3c_ac_get_property(struct power_supply *ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct chg_data *chg = container_of(ps, struct chg_data, psy_ac);
	bat_dbg(" [BAT] %s  \n", __func__);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the AC charger is connected */
	val->intval = (chg->cable_status == CABLE_TYPE_AC);

	return false;
}

static int s3c_bat_get_adc_data(enum adc_channel_type adc_ch)
{
	int adc_data;
	int adc_max = 0;
	int adc_min = 0;
	int adc_total = 0;
	int i;
	bat_dbg(" [BAT] %s  \n", __func__);

	for (i = 0; i < ADC_DATA_ARR_SIZE; i++) {
		adc_data = s3c_adc_get_adc_data(adc_ch);

		if (i != 0) {
			if (adc_data > adc_max)
				adc_max = adc_data;
			else if (adc_data < adc_min)
				adc_min = adc_data;
		} else {
			adc_max = adc_data;
			adc_min = adc_data;
		}
		adc_total += adc_data;
	}

	return (adc_total - adc_max - adc_min) / (ADC_DATA_ARR_SIZE - 2);
}

static unsigned long calculate_average_adc(enum adc_channel_type channel,
					   int adc, struct chg_data *chg)
{
	unsigned int cnt = 0;
	int total_adc = 0;
	int average_adc = 0;
	int index = 0;
	bat_dbg(" [BAT] %s  \n", __func__);

	cnt = chg->adc_sample[channel].cnt;
	total_adc = chg->adc_sample[channel].total_adc;

	if (adc <= 0) {
		pr_err("%s : invalid adc : %d\n", __func__, adc);
		adc = chg->adc_sample[channel].average_adc;
	}

	if (cnt < ADC_TOTAL_COUNT) {
		chg->adc_sample[channel].adc_arr[cnt] = adc;
		chg->adc_sample[channel].index = cnt;
		chg->adc_sample[channel].cnt = ++cnt;

		total_adc += adc;
		average_adc = total_adc / cnt;
	} else {
		index = chg->adc_sample[channel].index;
		if (++index >= ADC_TOTAL_COUNT)
			index = 0;

		total_adc = total_adc - chg->adc_sample[channel].adc_arr[index] + adc;
		average_adc = total_adc / ADC_TOTAL_COUNT;

		chg->adc_sample[channel].adc_arr[index] = adc;
		chg->adc_sample[channel].index = index;
	}

	chg->adc_sample[channel].total_adc = total_adc;
	chg->adc_sample[channel].average_adc = average_adc;

	chg->bat_info.batt_temp_adc = average_adc;

	return average_adc;
}

static unsigned long s3c_read_temp(struct chg_data *chg)
{
	int adc = 0;
	bat_dbg(" [BAT] %s  \n", __func__);

	adc = s3c_bat_get_adc_data(S3C_ADC_TEMPERATURE);
	return calculate_average_adc(S3C_ADC_TEMPERATURE, adc, chg);
}
#if defined (ATT_TMO_COMMON)
static void s3c_set_bat_health(struct chg_data *chg, u32 batt_health)
{
	bat_dbg("[BAT]:%s\n", __func__);

	chg->bat_info.batt_health = batt_health;
}

static void s3c_cable_check_status(struct chg_data *chg)
{
	bat_dbg("[BAT]:%s\n", __func__);


      if(latest_cable_status != chg->cable_status)
      	{
             	del_timer_sync(&chargingstep_timer); 
              chg->charging = false;
		chg->set_batt_full = false;
		chg->bat_info.batt_is_full = false;
		chg->bat_info.batt_is_recharging = false;
		chg->set_charge_timeout = false;
		chg->discharging_time = 0;

//[HDLNC]uhm21c for 8922 start
#if defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)

		if(chg->bat_info.event_dev_state & (OFFSET_CAMERA_ON | OFFSET_VIDEO_PLAY ))
			{
			      if(is_camera_charging_on)
			      	{
			        	gpio_set_value(CAM_CHG_EN, 1);
			        	msleep(5);
			        	is_camera_charging_on = 0;
			      	}
			}
#endif
		s3c_set_chg_en(chg,false);      	
      	}
       	
	latest_cable_status = chg->cable_status; 
	
	if (chg->cable_status)
	{
		if (chg->bat_info.batt_health != POWER_SUPPLY_HEALTH_GOOD)
		{
			pr_info("[BAT]:%s:Unhealth battery state!\n", __func__);
			global_cable_status = CABLE_TYPE_NONE;
		}
		else if (chg->cable_status == CABLE_TYPE_USB)
		{
			global_cable_status = CABLE_TYPE_USB;
			//pr_info("[BAT]:%s: status : USB\n", __func__);
		}
		else
		{
			global_cable_status = CABLE_TYPE_AC;
			//pr_info("[BAT]:%s: status : AC\n", __func__);
		}

	} 
	else
	{
		global_cable_status = CABLE_TYPE_NONE;
		//pr_info("[BAT]:%s: status : BATTERY\n", __func__);
	}

}

static void s3c_temp_control(struct chg_data *chg, int mode)
{
	bat_dbg(" [BAT] %s + \n", __func__);

	switch (mode)
	{
		case POWER_SUPPLY_HEALTH_GOOD:
			//pr_info("[BAT]:%s: GOOD\n", __func__);
			s3c_set_bat_health(chg,mode);
			break;
		case POWER_SUPPLY_HEALTH_OVERHEAT:
			//pr_info("[BAT]:%s: OVERHEAT\n", __func__);
			s3c_set_bat_health(chg,mode);
			chg->bat_info.charging_status = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		case POWER_SUPPLY_HEALTH_COLD:
			//pr_info("[BAT]:%s: FREEZE\n", __func__);
			s3c_set_bat_health(chg,mode);
			chg->bat_info.charging_status = POWER_SUPPLY_STATUS_DISCHARGING;
			
			break;
		default:
			break;
	}
}

static void s3c_set_time_for_event(int mode)
{
	bat_dbg(" [BAT] %s + \n", __func__);

	if (mode)
	{
		/* record start time for abs timer */
		event_start_time_msec = jiffies_to_msecs(jiffies);
		//pr_info("[BAT]:%s: start_time(%u)\n", __func__, event_start_time_msec);
	}
	else
	{
		/* initialize start time for abs timer */
		event_start_time_msec = 0;
		event_total_time_msec = 0;
		//pr_info("[BAT]:%s: start_time_msec(%u)\n", __func__, event_start_time_msec);
	}
	
}

static int is_over_event_time(void)
{
	unsigned int total_time=0;

	bat_dbg("[BAT]:%s\n", __func__);

	if(!event_start_time_msec)
	{
		return true;
	}
		
      total_time = TOTAL_EVENT_TIME;

	if(jiffies_to_msecs(jiffies) >= event_start_time_msec)
	{
		event_total_time_msec = jiffies_to_msecs(jiffies) - event_start_time_msec;
	}
	else
	{
		event_total_time_msec = 0xFFFFFFFF - event_start_time_msec + jiffies_to_msecs(jiffies);
	}

	if (event_total_time_msec > total_time && event_start_time_msec)
	{
		bat_dbg("[BAT]:%s:abs time is over.:event_start_time_msec=%u, event_total_time_msec=%u\n", __func__, event_start_time_msec, event_total_time_msec);
		return true;
	}	
	else
	{
		return false;
	}
	
}

#endif

static int s3c_get_bat_temp(struct chg_data *chg)
{
	int temp = 0;
	int temp_adc = s3c_read_temp(chg);
	int health = chg->bat_info.batt_health;
	int left_side = 0;
	int right_side = chg->pdata->adc_array_size - 1;
	int mid;
	unsigned int ex_case = 0;
	bat_dbg(" [BAT] %s  \n", __func__);

	while (left_side <= right_side) {
		mid = (left_side + right_side) / 2 ;
		if (mid == 0 ||
		    mid == chg->pdata->adc_array_size - 1 ||
		    (chg->pdata->adc_table[mid].adc_value <= temp_adc &&
		     chg->pdata->adc_table[mid+1].adc_value > temp_adc)) {
			temp = chg->pdata->adc_table[mid].temperature;
			break;
		} else if (temp_adc - chg->pdata->adc_table[mid].adc_value > 0)
			left_side = mid + 1;
		else
			right_side = mid - 1;
	}

	chg->bat_info.batt_temp = temp;

#if defined (ATT_TMO_COMMON)
	ex_case = OFFSET_MP3_PLAY | OFFSET_VOICE_CALL_2G | OFFSET_VOICE_CALL_3G | OFFSET_DATA_CALL | OFFSET_VIDEO_PLAY |
                       OFFSET_CAMERA_ON | OFFSET_WIFI | OFFSET_GPS;
	
  	if (chg->bat_info.event_dev_state & ex_case) 
	{
	  	  if (temp >= EVENT_TEMP_HIGH_BLOCK) 
		  {
	  	 	 	if ( health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)
	  			s3c_temp_control(chg,POWER_SUPPLY_HEALTH_OVERHEAT);
		  }
	         else if (temp < EVENT_TEMP_HIGH_BLOCK && temp > TEMP_LOW_BLOCK)	
		  {
	 	 	if ( (health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) && (health != POWER_SUPPLY_HEALTH_COLD) &&  (health != POWER_SUPPLY_HEALTH_OVERHEAT) )
			{
				s3c_temp_control(chg,POWER_SUPPLY_HEALTH_GOOD);
			}
                     else if( (temp <= EVENT_TEMP_HIGH_RECOVER) &&  (temp >= TEMP_LOW_RECOVER) )
                     {
		 	 	if (  (health  == POWER_SUPPLY_HEALTH_COLD) || (health == POWER_SUPPLY_HEALTH_OVERHEAT) )
				{
					s3c_temp_control(chg,POWER_SUPPLY_HEALTH_GOOD);
				}	                     
                     }
		  }				
		  else if (temp <= TEMP_LOW_BLOCK)
		  {
	 	 	if ( health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)
			{
				s3c_temp_control(chg,POWER_SUPPLY_HEALTH_COLD);
			}
		  }

	  if(event_occur == 0)
	  {	
	    event_occur = 1;
	  } 
		  
  	}
      else
      	{
		       if(event_occur == 1){
			      s3c_set_time_for_event(1);
			      event_occur = 0;
			}
		
			if (temp >= TEMP_HIGH_BLOCK)

			{
		 	 	if ( health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE  &&  is_over_event_time() )
				{
					s3c_temp_control(chg,POWER_SUPPLY_HEALTH_OVERHEAT);
					s3c_set_time_for_event(0);			
				}
			}
			else if(temp < TEMP_HIGH_BLOCK && temp > TEMP_LOW_BLOCK)
			{
		 	 	if ( (health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) && (health != POWER_SUPPLY_HEALTH_COLD) &&  (health != POWER_SUPPLY_HEALTH_OVERHEAT) )
				{
					s3c_temp_control(chg,POWER_SUPPLY_HEALTH_GOOD);
				}
	                     else if( (temp <= TEMP_HIGH_RECOVER) &&  (temp >= TEMP_LOW_RECOVER) )
	                     {
			 	 	if ( (health  == POWER_SUPPLY_HEALTH_COLD) || (health == POWER_SUPPLY_HEALTH_OVERHEAT) )
					{
						s3c_temp_control(chg,POWER_SUPPLY_HEALTH_GOOD);
					}	                     
	                     }
			}
			else if (temp <= TEMP_LOW_BLOCK)
			{
		 	 	if ( health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)			
				{
					s3c_temp_control(chg,POWER_SUPPLY_HEALTH_COLD);
				}
			}


#else
	if (temp >= HIGH_BLOCK_TEMP) {
		if (health != POWER_SUPPLY_HEALTH_OVERHEAT &&
		    health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)
			chg->bat_info.batt_health = POWER_SUPPLY_HEALTH_OVERHEAT;
	} else if (temp <= HIGH_RECOVER_TEMP && temp >= LOW_RECOVER_TEMP) {
		if (health == POWER_SUPPLY_HEALTH_OVERHEAT ||
		    health == POWER_SUPPLY_HEALTH_COLD)
			chg->bat_info.batt_health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (temp <= LOW_BLOCK_TEMP) {
		if (health != POWER_SUPPLY_HEALTH_COLD &&
		    health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)
			chg->bat_info.batt_health = POWER_SUPPLY_HEALTH_COLD;
		}
#endif
	}

	return true;
}

static void s3c_bat_discharge_reason(struct chg_data *chg)
{
	int discharge_reason;
	ktime_t ktime;
	struct timespec cur_time;
	union power_supply_propval value;
	
	bat_dbg(" [BAT] %s  \n", __func__);

	if (chg->pdata &&
	    chg->pdata->psy_fuelgauge &&
	    chg->pdata->psy_fuelgauge->get_property) {
		chg->pdata->psy_fuelgauge->get_property(
			chg->pdata->psy_fuelgauge, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
		chg->bat_info.batt_vcell = value.intval;

		chg->pdata->psy_fuelgauge->get_property(
			chg->pdata->psy_fuelgauge, POWER_SUPPLY_PROP_CAPACITY, &value);
			chg->bat_info.batt_soc = value.intval;
	}

	discharge_reason = chg->bat_info.dis_reason & 0xf;

//	if ((chg->bat_info.batt_vcell/1000) < RECHARGE_COND_VOLTAGE)
//		chg->bat_info.dis_reason &= ~DISCONNECT_BAT_FULL;


	if (chg->bat_info.batt_health != POWER_SUPPLY_HEALTH_GOOD)
		chg->bat_info.dis_reason |=
			(chg->bat_info.batt_health == POWER_SUPPLY_HEALTH_OVERHEAT) ?
			DISCONNECT_TEMP_OVERHEAT : DISCONNECT_TEMP_FREEZE;
				
   else //	if ( (TEMP_LOW_RECOVER <= chg->bat_info.batt_temp) &&  (chg->bat_info.batt_temp <= TEMP_HIGH_RECOVER) )
	      {
		chg->bat_info.dis_reason &= ~DISCONNECT_TEMP_OVERHEAT;
		chg->bat_info.dis_reason &= ~DISCONNECT_TEMP_FREEZE;
		}

	if ((discharge_reason & DISCONNECT_OVER_TIME) &&
	    (chg->bat_info.batt_vcell/1000) < RECHARGE_COND_VOLTAGE)
		chg->bat_info.dis_reason &= ~DISCONNECT_OVER_TIME;

//	if (chg->bat_info.batt_is_full)
//		chg->bat_info.dis_reason |= DISCONNECT_BAT_FULL;



	ktime = alarm_get_elapsed_realtime();
	cur_time = ktime_to_timespec(ktime);

	if (chg->discharging_time &&
	    cur_time.tv_sec > chg->discharging_time) {
		chg->set_charge_timeout = true;
		chg->bat_info.dis_reason |= DISCONNECT_OVER_TIME;
//		chg->set_batt_full = true;
	}
/*
	bat_dbg("bat(%d,%d) tmp(%d,%d) full(%d,%d) cable(%d) chg(%d) dis(%X)\n",
		chg->bat_info.batt_soc, chg->bat_info.batt_vcell/1000,
		chg->bat_info.batt_temp, chg->bat_info.batt_temp_adc,
		chg->set_batt_full, chg->bat_info.batt_is_full,
		chg->cable_status, chg->bat_info.charging_status, chg->bat_info.dis_reason);
	bat_dbg(" [BAT] %s - \n", __func__);
*/
	
}

#if defined (ATT_TMO_COMMON)
static void check_chg_current(struct chg_data *chg)
{
	unsigned long chg_current = 0; 
	bat_dbg(" [BAT] %s  \n", __func__);

	chg_current = s3c_bat_get_adc_data(S3C_ADC_CHG_CURRENT);

	
#ifdef BATTERY_DEBUG
	bat_dbg("[chg_current] %s:  chg_current = %ld , count_check_chg_current = %d\n", __func__, chg_current, count_check_chg_current);
#endif	

	chg->bat_info.batt_current = chg_current;

      if ( (!chg->set_batt_full)  && (chg_current<= CURRENT_OF_FULL_CHG) ) 
	{
		count_check_chg_current++;
		if (count_check_chg_current >= 5) {
			printk( "[BAT] %s: battery first FULL \n", __func__);
			chg->set_batt_full= true;
			count_check_chg_current = 0;
		}
	}  
       else if( chg->set_batt_full  && (chg_current < CURRENT_OF_TOPOFF_CHG) && (chg_current != 0) )
       {
              count_check_chg_current++;
       	if (count_check_chg_current >= 10) 
			{
				printk("[BAT] %s: battery second FULL \n", __func__);     
				chg->bat_info.batt_is_full = true;
				count_check_chg_current = 0;				
       		}	
       }
	else {
		count_check_chg_current = 0;
	}
		bat_dbg(" [BAT] %s - \n", __func__);

}

/* Device Type 1 */
#define DEV_USB_OTG		(1 << 7)
#define DEV_DEDICATED_CHG	(1 << 6)
#define DEV_USB_CHG		(1 << 5)
#define DEV_CAR_KIT		(1 << 4)
#define DEV_UART		(1 << 3)
#define DEV_USB			(1 << 2)
#define DEV_AUDIO_2		(1 << 1)
#define DEV_AUDIO_1		(1 << 0)

#define DEV_T1_USB_MASK		(DEV_USB_OTG | DEV_USB)
#define DEV_T1_UART_MASK	(DEV_UART)
#define DEV_T1_CHARGER_MASK	(DEV_DEDICATED_CHG | DEV_USB_CHG | DEV_CAR_KIT)

/* Device Type 2 */
#define DEV_AV			(1 << 6)
#define DEV_TTY			(1 << 5)
#define DEV_PPD			(1 << 4)
#define DEV_JIG_UART_OFF	(1 << 3)
#define DEV_JIG_UART_ON		(1 << 2)
#define DEV_JIG_USB_OFF		(1 << 1)
#define DEV_JIG_USB_ON		(1 << 0)

#define DEV_T2_USB_MASK		(DEV_JIG_USB_OFF | DEV_JIG_USB_ON)
#define DEV_T2_UART_MASK	DEV_JIG_UART_OFF
#define DEV_T2_JIG_MASK		(DEV_JIG_USB_OFF | DEV_JIG_USB_ON | \
				DEV_JIG_UART_OFF)

unsigned char maxim_chg_status()
{
	int i;
	int  value;
   
	if(max8998_check_vdcin())
	{
		value = FSA9480_Get_I2C_USB_Status();

		if((DEV_USB & value) || ((DEV_T2_USB_MASK<<8) & value))
		{
			printk("[BAT]  cable maxim_USB_connect~~~ \n");
			max8998_chg->cable_status = CABLE_TYPE_USB;
			return 1;					
		}
		else if(DEV_T1_CHARGER_MASK & value)
		{
			printk("[BAT] cable maxim_TA_connect~~~ \n");
			max8998_chg->cable_status = CABLE_TYPE_AC;
			return 2;					
		}
		else
		{
			/* Recognition fails, try to read at most 5 times more for 1 sec. */
			for (i=0; i<5; i++) 
			{
				msleep(200);
				value = FSA9480_Get_I2C_USB_Status();
				if((DEV_USB & value) || ((DEV_T2_USB_MASK<<8) & value))
				{
					printk("[BAT]  cable maxim_USB_connect~~~ \n");
					max8998_chg->cable_status = CABLE_TYPE_USB;
					return 1;					
				}
			}
			
			/* Recognize the inserted as a AC Charger due to failing to recognize it. */
			printk("[BAT]  cable maxim_not USB,TA _connect~~~ value =[0x%x] \n", value);
			max8998_chg->cable_status = CABLE_TYPE_AC;
			return 2;	
			}
 	}

	max8998_chg->cable_status = CABLE_TYPE_NONE;
	return 0;
	
	}
EXPORT_SYMBOL(maxim_chg_status);

static void s3c_set_chg_en(struct chg_data *chg, int enable)
{
	bool vdc_status;
	int ret;
	ktime_t ktime;
	int chg_en_val;	
	struct timespec cur_time;
	
	bat_dbg(" [BAT] %s , charging_state = %d, enable = %d \n", __func__, charging_state, enable);

	if(charging_state != enable)
	{
     		chg_en_val = maxim_chg_status();
			
		if (enable && chg_en_val )
		{
		
  		   	       charging_state = true;
			   // VDCIN  == AC or USB
				vdc_status = true;  //  VDCIN  == AC or USB
#if 0				
				if (chg->bat_info.dis_reason) 
				{   // high temp, low temp
					/* have vdcin, but cannot charge */
					
					chg->charging = true;
					ret = max8998_charging_control(chg);    // charging off
					if (ret < 0)
					goto err;
					
					if (chg->bat_info.dis_reason & (DISCONNECT_TEMP_OVERHEAT | DISCONNECT_TEMP_FREEZE))
					chg->set_batt_full = false;

					//status updates
					chg->bat_info.charging_status = chg->set_batt_full ?
					POWER_SUPPLY_STATUS_FULL :
					POWER_SUPPLY_STATUS_DISCHARGING;
					chg->discharging_time = 0;
					chg->bat_info.batt_is_full = false;
					goto update;
				} 
#endif
				if (chg->discharging_time == 0) 
//				else if (chg->discharging_time == 0) 
				{
				
					ktime = alarm_get_elapsed_realtime();
					cur_time = ktime_to_timespec(ktime);
					chg->discharging_time =
					(chg->set_batt_full || chg->set_charge_timeout) ?
					cur_time.tv_sec + TOTAL_RECHARGING_TIME :
					cur_time.tv_sec + TOTAL_CHARGING_TIME;
				}

				/* able to charge */
				chg->charging = true;
				ret = max8998_charging_control(chg);
				if (ret < 0)
				goto err;

				chg->bat_info.charging_status = chg->set_batt_full ?
				POWER_SUPPLY_STATUS_FULL : POWER_SUPPLY_STATUS_CHARGING;
					
					
		}
		else
		{
			charging_state = false;

			vdc_status = false;
			/* no vdc in, not able to charge */
			chg->charging = false;
			ret = max8998_charging_control(chg);
			if (ret < 0)
				goto err;

                     if (chg->cable_status != CABLE_TYPE_NONE)
				chg->bat_info.charging_status = POWER_SUPPLY_STATUS_DISCHARGING;
			else
				chg->bat_info.charging_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
				
			chg->discharging_time = 0;
			chg->set_charge_timeout = false;

			if (lpm_charging_mode && pm_power_off && (chg->cable_status == CABLE_TYPE_NONE))
				pm_power_off();

	      }
		
update:
	
  	ce_for_fuelgauge = chg->charging; 
	
//	if (vdc_status)
err:   ;
	
	}


	if(max8998_check_vdcin())
	{
		if (chg->bat_info.dis_reason) 
			{
				if (chg->bat_info.dis_reason & (DISCONNECT_TEMP_OVERHEAT | DISCONNECT_TEMP_FREEZE))
						chg->set_batt_full = false;
				
				chg->bat_info.charging_status = chg->set_batt_full ?
								POWER_SUPPLY_STATUS_FULL :
								POWER_SUPPLY_STATUS_DISCHARGING;
								
				chg->discharging_time = 0;
//				chg->bat_info.batt_is_full = false;							
			}
		else
			{
				chg->bat_info.charging_status = chg->set_batt_full ?
				POWER_SUPPLY_STATUS_FULL : POWER_SUPPLY_STATUS_CHARGING;
			}	
	}


	if (chg->cable_status != CABLE_TYPE_NONE)
		wake_lock(&chg->vbus_wake_lock);
	else
		{
		chg->bat_info.charging_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		wake_unlock(&chg->vbus_wake_lock);
		}
	return ;

	
}


//NAGSM_Android_SEL_Kernel_Aakash_20100503
static int charging_connect;
static int charging_control_overide = true;
int is_under_at_sleep_cmd = 0;

int get_wakelock_for_AT_SLEEP(void)
{

	wake_lock(&max8998_chg->vbus_wake_lock);

	return false;
}

int release_wakelock_for_AT_SLEEP(void)
{
	wake_lock_timeout(&max8998_chg->vbus_wake_lock,HZ * 5);
	return false;
}

static ssize_t charging_status_show(struct device *dev, struct device_attribute *attr, char *sysfsbuf)
{	
	charging_connect = max8998_chg->charging;
	return sprintf(sysfsbuf, "%d\n", charging_connect);
}

static ssize_t charging_con_store(struct device *dev, struct device_attribute *attr,const char *sysfsbuf, size_t size)
{
	sscanf(sysfsbuf, "%d", &charging_connect);

	mutex_lock(&max8998_chg->mutex);		

	bat_dbg("+++++ charging_connect = %d +++++\n", charging_connect);
	
	charging_control_overide = charging_connect;
	s3c_set_chg_en(max8998_chg, charging_connect);

	release_wakelock_for_AT_SLEEP();	
	is_under_at_sleep_cmd = !charging_control_overide;
	
	mutex_unlock(&max8998_chg->mutex);	

	return size;
}

static DEVICE_ATTR(usbchargingcon, S_IRUGO | S_IWUSR | S_IWGRP, charging_status_show, charging_con_store);
//NAGSM_Android_SEL_Kernel_Aakash_20100503


static void check_recharging_bat(struct chg_data *chg)
{
	bat_dbg(" [BAT] %s  \n", __func__);
	
	if ( (chg->bat_info.batt_vcell/1000) < RECHARGE_COND_VOLTAGE) 
	{
		if (++count_check_recharging_bat >= 10) 
		{
			printk( "[BAT]%s: recharging , count_check_recharging_bat = %d\n", __func__, count_check_recharging_bat);
			s3c_set_chg_en(chg,true);	
			chg->bat_info.batt_is_recharging = true;
			count_check_recharging_bat = 0;
		}
	}
	else 
	{
		count_check_recharging_bat = 0;
	}
	
}


static int s3c_bat_is_full_charged(struct chg_data *chg)
{
	if(chg->bat_info.batt_is_full == true  && chg->set_batt_full== true )
	{
		bat_dbg("[BAT]:%s : battery is fully charged\n", __func__);
		chg->bat_info.batt_is_full = false; 		
		return true;
	}
	else
	{
		bat_dbg("[BAT]:%s : battery is not fully charged\n", __func__);
		return false;
	}

}

static void s3c_bat_charging_control(struct chg_data *chg)
{
	ktime_t ktime;
	struct timespec cur_time;
	struct i2c_client *i2c = chg->iodev->i2c;
	
	bat_dbg(" [BAT] %s , global_cable_status = %d \n", __func__, global_cable_status);

	if(global_cable_status == CABLE_TYPE_NONE  || !max8998_check_vdcin() || chg->bat_info.batt_health != POWER_SUPPLY_HEALTH_GOOD)
	{
             	del_timer_sync(&chargingstep_timer); 
              chg->charging = false;
		chg->set_batt_full = false;
		chg->bat_info.batt_is_full = false;
		chg->bat_info.batt_is_recharging = false;
		chg->set_charge_timeout = false;
		chg->discharging_time = 0;

//[HDLNC]uhm21c for 8922 start
#if defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)

		if(chg->bat_info.event_dev_state & (OFFSET_CAMERA_ON | OFFSET_VIDEO_PLAY ))
			{
			      if(is_camera_charging_on)
			      	{
			        	gpio_set_value(CAM_CHG_EN, 1);
			        	msleep(5);
			        	is_camera_charging_on = 0;
			      	}
			}
#endif
		
		s3c_set_chg_en(chg,false);
	}
	
	else if (chg->set_batt_full && (!chg->charging))
		{
			check_recharging_bat(chg);
		}	
		
	else if(s3c_bat_is_full_charged(chg))
	{
		s3c_set_chg_en(chg, false);

#if defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
		if(chg->bat_info.event_dev_state & (OFFSET_CAMERA_ON | OFFSET_VIDEO_PLAY ))
		{
		             if(is_camera_charging_on)
		             	{
				        	gpio_set_value(CAM_CHG_EN, 1);
				        	msleep(5);
				        	
				        	is_camera_charging_on =0;
		             	}
		}
#endif
		
	}
	else if(chg->set_charge_timeout)    // Time over 
	{
		s3c_set_chg_en(chg,false);

#if defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
		if(chg->bat_info.event_dev_state & (OFFSET_CAMERA_ON | OFFSET_VIDEO_PLAY ))
		{
		             if(is_camera_charging_on)
		             	{
				        	gpio_set_value(CAM_CHG_EN, 1);
				        	msleep(5);
				        	
				        	is_camera_charging_on =0;
		             	}	
		}
#endif
		
		chg->set_batt_full = true;
		chg->bat_info.batt_is_full = true;
	} 
#if defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
	else if((chg->bat_info.event_dev_state & (OFFSET_CAMERA_ON | OFFSET_VIDEO_PLAY )) && chg->charging)
	{
        	if(!is_camera_charging_on)
        	{
			if(global_cable_status==CABLE_TYPE_AC  || global_cable_status== CABLE_TYPE_USB)
			{
				max8998_write_reg(i2c, MAX8998_REG_CHGR2,
				(chg->esafe		<< MAX8998_SHIFT_ESAFEOUT) |
				(MAX8998_CHGTIME_DISABLE	<< MAX8998_SHIFT_FT) |
				(MAX8998_CHGEN_DISABLE	<< MAX8998_SHIFT_CHGEN));
			
				if (chg->discharging_time == 0) 
//				else if (chg->discharging_time == 0) 
				{
				
					ktime = alarm_get_elapsed_realtime();
					cur_time = ktime_to_timespec(ktime);
					chg->discharging_time =
					(chg->set_batt_full || chg->set_charge_timeout) ?
					cur_time.tv_sec + TOTAL_RECHARGING_TIME :
					cur_time.tv_sec + TOTAL_CHARGING_TIME;
				}
			}

                 // MAX8922 control ON 
                    gpio_set_value(CAM_CHG_EN, 0);  //400mA
                    is_camera_charging_on = 1;
					charging_state = 1;	
					
		      chg->charging = charging_state;
		      ce_for_fuelgauge = charging_state;
				

        	}
        	else
        	{
        		s3c_set_chg_en(chg, true);
        	}	
     	
	}
	else if( !(chg->bat_info.event_dev_state  & (OFFSET_CAMERA_ON | OFFSET_VIDEO_PLAY )) && chg->charging )
	{
             // Output LOW 5msec	
		if(is_camera_charging_on)
			{
		        	gpio_set_value(CAM_CHG_EN, 1);
		        	msleep(5);
				is_camera_charging_on = 0;	
				charging_state = 0;
				s3c_set_chg_en(chg, true);
			}
		else
		{
			s3c_set_chg_en(chg, true);
		}
	}
#endif 
	else
	{
		s3c_set_chg_en(chg, true);
	}
	
}

static unsigned int s3c_bat_check_v_f(struct chg_data *chg)
{
	unsigned int rc = 0;
	int adc = 0;

	bat_dbg("[BAT]:%s\n", __func__);
	
	adc = s3c_bat_get_adc_data(S3C_ADC_V_F);

	chg->bat_info.batt_v_f_adc=adc;

	if ((adc <= BATT_VF_MAX && adc >= BATT_VF_MIN) ||  FSA9480_Get_JIG_Status() || FSA9480_Get_JIG_UART_Status() )
	{
		rc = true;
		VF_count = 0; 
		if(chg->bat_info.batt_health == POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)
			s3c_set_bat_health(chg, POWER_SUPPLY_HEALTH_GOOD);		
	}
	else
	{
		VF_count++;
		if(VF_count == 1)
			{
				printk("[BAT]:%s: Unauthorized battery!, V_F ADC = %d, VF_count = %d \n", __func__, adc,VF_count);

				if(chg->bat_info.batt_health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)
					s3c_set_bat_health(chg, POWER_SUPPLY_HEALTH_UNSPEC_FAILURE);
				rc = false;
				VF_count = 0;
			}
	}

#ifdef BATTERY_DEBUG
	bat_dbg("[BAT]:%s: V_F ADC = %d, VF_count = %d\n", __func__, adc, VF_count);
#endif	

	return rc;
}
static void s3c_bat_set_compesation(int mode, int offset, int compensate_value)
{

	bat_dbg(" [BAT] %s +  \n", __func__);

	if (mode)
	{
		if (!(max8998_chg->bat_info.event_dev_state & offset))
		{
			max8998_chg->bat_info.event_dev_state |= offset;
		}
	}
	else
	{
		if (max8998_chg->bat_info.event_dev_state & offset)
		{
			max8998_chg->bat_info.event_dev_state &= ~offset;
		}
	}

	//	is_calling_or_playing = event_dev_state;
	//pr_info("[BAT]:%s: device_state=0x%x, compensation=%d\n", __func__, chg->bat_info.event_dev_state, batt_compensation);
	
}
#endif

#ifndef ATT_TMO_COMMON
static int s3c_cable_status_update(struct chg_data *chg)
{
	int ret;
	bool vdc_status;
	ktime_t ktime;
	struct timespec cur_time;

	/* if max8998 has detected vdcin */
	if (max8998_check_vdcin() == 1) {
		vdc_status = 1;
		if (chg->bat_info.dis_reason) {
			/* have vdcin, but cannot charge */
			chg->charging = 0;
			ret = max8998_charging_control(chg);
			if (ret < 0)
				goto err;
			if (chg->bat_info.dis_reason & 
			    (DISCONNECT_TEMP_OVERHEAT | DISCONNECT_TEMP_FREEZE))
				chg->set_batt_full = false;
			chg->bat_info.charging_status = chg->set_batt_full ?
				POWER_SUPPLY_STATUS_FULL :
				POWER_SUPPLY_STATUS_NOT_CHARGING;
			chg->discharging_time = 0;
			chg->bat_info.batt_is_full = false;
			goto update;
		} else if (chg->discharging_time == 0) {
			ktime = alarm_get_elapsed_realtime();
			cur_time = ktime_to_timespec(ktime);
			chg->discharging_time =
				(chg->set_batt_full || chg->set_charge_timeout) ?
				cur_time.tv_sec + TOTAL_RECHARGING_TIME :
				cur_time.tv_sec + TOTAL_CHARGING_TIME;
		}

		/* able to charge */
		chg->charging = 1;
		ret = max8998_charging_control(chg);
		if (ret < 0)
			goto err;

		chg->bat_info.charging_status = chg->set_batt_full ?
			POWER_SUPPLY_STATUS_FULL : POWER_SUPPLY_STATUS_CHARGING;
	} else {
		vdc_status = 0;
		/* no vdc in, not able to charge */
		chg->charging = 0;
		ret = max8998_charging_control(chg);
		if (ret < 0)
			goto err;

		chg->bat_info.charging_status = POWER_SUPPLY_STATUS_DISCHARGING;

		chg->bat_info.batt_is_full = false;
		chg->set_charge_timeout = false;
		chg->set_batt_full = false;
		chg->bat_info.dis_reason = 0;
		chg->discharging_time = 0;

		if (lpm_charging_mode && pm_power_off)
			pm_power_off();
	}

update:
	if (vdc_status)
		wake_lock(&chg->vbus_wake_lock);
	else
		wake_lock_timeout(&chg->vbus_wake_lock, HZ / 5);

	return 0;
err:
	return ret;
}
#endif

static void s3c_bat_work(struct work_struct *work)
{
	struct chg_data *chg = container_of(work, struct chg_data, bat_work);
	int ret;

	bool cur_lpm_status;	// HDLNC_BP_OPK_20110228
	bat_dbg(" [BAT] %s +  \n", __func__);
	mutex_lock(&chg->mutex);

#if defined (ATT_TMO_COMMON)

	ret = s3c_get_bat_temp(chg);
	if (ret < 0)
		goto err;

	s3c_bat_discharge_reason(chg);
	s3c_cable_check_status(chg);	
	
	if ( (  (chg->bat_info.batt_vcell/1000) >= FULL_CHARGE_COND_VOLTAGE)  && chg->charging)
		{
			check_chg_current(chg);	
   	 	}


#if 0
       if(global_cable_status == CABLE_TYPE_NONE )
	{
             	del_timer_sync(&chargingstep_timer); 
              chg->charging = false;
		chg->set_batt_full = false;
		chg->bat_info.batt_is_full = false;
		chg->bat_info.batt_is_recharging = false;
		chg->set_charge_timeout = false;
		chg->bat_info.dis_reason = 0;
		chg->discharging_time = 0;
		
		s3c_set_chg_en(chg,false);
	}
	else if (chg->set_batt_full &&  (!chg->charging))
		{
			check_recharging_bat(chg);
		}
#endif

        s3c_bat_charging_control(chg);

        if( (s3c_bat_check_v_f(chg) == false) &&	(chg->bat_info.batt_health == POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) )
        	{
			if (chg->cable_status != CABLE_TYPE_NONE)
			{
				bat_info("[BAT]:%s: Unauthorized battery!, curent_device_type = %d \n", __func__, chg->cable_status);
			
				if (pm_power_off)
					pm_power_off();
			}
		}
#else
	s3c_get_bat_temp(chg);
	s3c_bat_discharge_reason(chg);

	ret = s3c_cable_status_update(chg);
	if (ret < 0)
		goto err;
#endif

	mutex_unlock(&chg->mutex);

#if defined (ATT_TMO_COMMON)

      	chg->bat_info.batt_vcell = chg->bat_info.batt_vcell /1000;   

	  
      if( (chg->cable_status != CABLE_TYPE_NONE) && (global_cable_status != CABLE_TYPE_NONE) && max8998_check_vdcin() )  
      	{
	  	chg->lowbat_warning = false;
		max17040_lowbat_warning = 0;
		max17040_low_battery = 0;
      	}	
	  
      if(max17040_lowbat_warning)
 	       chg->lowbat_warning = true;

       if(max17040_low_battery)
	       chg->bat_info.batt_soc = 0;
	   

#if defined  (BATTERY_DEBUG)
       PMIC_dump();
#endif

if(cnt_lpm_mode < 5)
	{	
	if(cur_lpm_status = charging_mode_get()){
		chg->callbacks.set_esafe(&chg->callbacks,MAX8998_ESAFE_ALLOFF);
		cnt_lpm_mode++;	
	}
}

  if(  (b_charing != chg->charging) || (b_cable_status != chg->cable_status)  || \
  	 (b_bat_info_dis_reason != chg->bat_info.dis_reason) || (b_chg_esafe != chg->esafe) || \
  	 (b_bat_info_batt_soc != chg->bat_info.batt_soc) || (b_set_batt_full != chg->set_batt_full) || \
	 (b_bat_info_batt_is_full != chg->bat_info.batt_is_full) || (b_bat_info_event_dev_state != chg->bat_info.event_dev_state) || \
        (b_bat_info_batt_health != chg->bat_info.batt_health) || (b_bat_info_batt_is_recharging != chg->bat_info.batt_is_recharging) )
  	{
		printk("[BAT] CHR(%d) CAS(%d) CHS(%d) DCR(%X) ACP(%d) BAT(%d,%d,%d) TE(%d) HE(%d) VO(%d) ED(%x) RC(%d) CC(%d) VF(%d) LO(%d) \n",   \ 
		chg->charging, chg->cable_status,chg->bat_info.charging_status, chg->bat_info.dis_reason, chg->esafe,      \   
		chg->bat_info.batt_soc, chg->set_batt_full, chg->bat_info.batt_is_full, chg->bat_info.batt_temp/10, chg->bat_info.batt_health,           \
		chg->bat_info.batt_vcell, chg->bat_info.event_dev_state, chg->bat_info.batt_is_recharging, chg->bat_info.batt_current, chg->bat_info.batt_v_f_adc,max17040_low_battery);
  	}
  
    b_charing  = chg->charging; 
	b_cable_status = chg->cable_status; 
	b_bat_info_dis_reason = chg->bat_info.dis_reason;
	b_chg_esafe = chg->esafe;
	b_bat_info_batt_soc = chg->bat_info.batt_soc;
	b_set_batt_full = chg->set_batt_full;
	b_bat_info_batt_is_full = chg->bat_info.batt_is_full;
	b_bat_info_batt_temp = chg->bat_info.batt_temp;
	b_bat_info_batt_health = chg->bat_info.batt_health;
	b_bat_info_batt_vcell = chg->bat_info.batt_vcell;
	b_bat_info_event_dev_state = chg->bat_info.event_dev_state;
	b_bat_info_batt_is_recharging = chg->bat_info.batt_is_recharging;
	b_bat_info_batt_current = chg->bat_info.batt_current;
	b_bat_info_batt_v_f_adc = chg->bat_info.batt_v_f_adc;


#endif
	power_supply_changed(&chg->psy_bat);

	mod_timer(&chg->bat_work_timer, jiffies + msecs_to_jiffies(BAT_POLLING_INTERVAL));

	wake_unlock(&chg->work_wake_lock);
	return;
err:
	mutex_unlock(&chg->mutex);
	wake_unlock(&chg->work_wake_lock);
	pr_err("battery workqueue fail\n");
}

static void s3c_bat_work_timer_func(unsigned long param)
{
	struct chg_data *chg = (struct chg_data *)param;
	bat_dbg(" [BAT] %s   \n", __func__);

	wake_lock(&chg->work_wake_lock);
	queue_work(chg->monitor_wqueue, &chg->bat_work);
}

extern void max17040_reset_soc(void);

static ssize_t s3c_bat_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct chg_data *chg = container_of(psy, struct chg_data, psy_bat);
	int i = 0;
	const ptrdiff_t off = attr - s3c_battery_attrs;
	union power_supply_propval value;
#if (defined CONFIG_S5PC110_DEMPSEY_BOARD)
        s32 GLOBAL_LCD_TEMP_ADC;
#endif
	bat_dbg(" [BAT] %s +  \n", __func__);

	switch (off) {
	case BATT_VOL:
		if (chg->pdata &&
		    chg->pdata->psy_fuelgauge &&
		    chg->pdata->psy_fuelgauge->get_property) {
			chg->pdata->psy_fuelgauge->get_property(
				chg->pdata->psy_fuelgauge,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
#if defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)			
			chg->bat_info.batt_vcell = value.intval + 20;
#else
			chg->bat_info.batt_vcell = value.intval;
#endif
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chg->bat_info.batt_vcell/1000);
		break;
	case BATT_VOL_ADC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", 0);
		break;
	case BATT_TEMP:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chg->bat_info.batt_temp);
		break;
	case BATT_TEMP_ADC:
		chg->bat_info.batt_temp_adc = s3c_bat_get_adc_data(S3C_ADC_TEMPERATURE);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chg->bat_info.batt_temp_adc);
		break;
#if (defined CONFIG_S5PC110_DEMPSEY_BOARD)
	case LCD_TEMP_ADC:
		if(HWREV >= 10)
			{
				GLOBAL_LCD_TEMP_ADC = s3c_bat_get_adc_data(S3C_LCD_ADC_TEMP);
				i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", GLOBAL_LCD_TEMP_ADC);
			}
		else
			{
				GLOBAL_LCD_TEMP_ADC = s3c_bat_get_adc_data(S3C_ADC_TEMPERATURE);
				i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", GLOBAL_LCD_TEMP_ADC);
			}				
		break;	
#endif			

#if (defined ATT_TMO_COMMON)
	case BATT_CHG_CURRENT:
	  chg->bat_info.batt_current = s3c_bat_get_adc_data(S3C_ADC_CHG_CURRENT);
	  i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chg->bat_info.batt_current);
	  break;
#endif
		
	case BATT_CHARGING_SOURCE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chg->cable_status);
		break;
	case BATT_FG_SOC:
		if (chg->pdata &&
		    chg->pdata->psy_fuelgauge &&
		    chg->pdata->psy_fuelgauge->get_property) {
			chg->pdata->psy_fuelgauge->get_property(
				chg->pdata->psy_fuelgauge,
				POWER_SUPPLY_PROP_CAPACITY, &value);
			chg->bat_info.batt_soc = value.intval;
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chg->bat_info.batt_soc);
		break;
	case CHARGING_MODE_BOOTING:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", lpm_charging_mode);
		break;
	case BATT_TEMP_CHECK:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chg->bat_info.batt_health);
		break;
	case BATT_FULL_CHECK:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",  chg->set_batt_full);
		break;
	default:
		i = -EINVAL;
	}

	return i;
}

static ssize_t s3c_bat_store_attrs(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
//	struct power_supply *psy = dev_get_drvdata(dev);
//	struct chg_data *chg = container_of(psy, struct chg_data, psy_bat);
	int x = 0;
	int ret = 0;
	const ptrdiff_t off = attr - s3c_battery_attrs;

	bat_dbg(" [BAT] %s +  \n", __func__);
	switch (off) 
		{
#if defined (ATT_TMO_COMMON)
			case BATT_VIBRATOR:
				if (sscanf(buf, "%d\n", &x) == 1)
				{
				s3c_bat_set_compesation(x, OFFSET_VIBRATOR_ON,	COMPENSATE_VIBRATOR);
				ret = count;
				}
				//bat_dbg("[BAT]:%s: vibrator = %d\n", __func__, x);
				break;
			case BATT_CAMERA:
				if (sscanf(buf, "%d\n", &x) == 1)
				{
				s3c_bat_set_compesation(x, OFFSET_CAMERA_ON, COMPENSATE_CAMERA);
				ret = count;
				}
				//bat_dbg("[BAT]:%s: camera = %d\n", __func__, x);
				break;
			case BATT_MP3:
				if (sscanf(buf, "%d\n", &x) == 1)
				{
				s3c_bat_set_compesation(x, OFFSET_MP3_PLAY,	COMPENSATE_MP3);
				ret = count;
				}
				//bat_dbg("[BAT]:%s: mp3 = %d\n", __func__, x);
				break;
			case BATT_VIDEO:
				if (sscanf(buf, "%d\n", &x) == 1)
				{
				s3c_bat_set_compesation(x, OFFSET_VIDEO_PLAY, COMPENSATE_VIDEO);
				ret = count;
				}
				//bat_dbg("[BAT]:%s: video = %d\n", __func__, x);
				break;
			case BATT_VOICE_CALL_2G:
				if (sscanf(buf, "%d\n", &x) == 1)
				{
				s3c_bat_set_compesation(x, OFFSET_VOICE_CALL_2G, COMPENSATE_VOICE_CALL_2G);
				ret = count;
				}
				//bat_dbg("[BAT]:%s: voice call 2G = %d\n", __func__, x);
				break;
			case BATT_VOICE_CALL_3G:
				if (sscanf(buf, "%d\n", &x) == 1)
				{
				s3c_bat_set_compesation(x, OFFSET_VOICE_CALL_3G, COMPENSATE_VOICE_CALL_3G);
				ret = count;
				}
				//bat_dbg("[BAT]:%s: voice call 3G = %d\n", __func__, x);
				break;
			case BATT_DATA_CALL:
				if (sscanf(buf, "%d\n", &x) == 1)
				{
				s3c_bat_set_compesation(x, OFFSET_DATA_CALL, COMPENSATE_DATA_CALL);
				ret = count;
				}
				//bat_dbg("[BAT]:%s: data call = %d\n", __func__, x);
				break;
			case BATT_WIFI:
				if (sscanf(buf, "%d\n", &x) == 1)
				{
				s3c_bat_set_compesation(x, OFFSET_WIFI, COMPENSATE_WIFI);
				ret = count;
				}
				//bat_dbg("[BAT]:%s: wifi = %d\n", __func__, x);
				break;
			case BATT_GPS:
				if (sscanf(buf, "%d\n", &x) == 1)
				{
				s3c_bat_set_compesation(x, OFFSET_GPS, COMPENSATE_GPS);
				ret = count;
				}
				//bat_dbg("[BAT]:%s: gps = %d\n", __func__, x);
				break;						

			case BATT_BOOTING:
				if (sscanf(buf, "%d\n", &x) == 1)
				{
				s3c_bat_set_compesation(x, OFFSET_BOOTING, COMPENSATE_BOOTING);
				ret = count;
				}
				//bat_dbg("[BAT]:%s: boot complete = %d\n", __func__, x);
				break;
#endif
	case BATT_RESET_SOC:
		if (sscanf(buf, "%d\n", &x) == 1) {
			if (x == 1)
				max17040_reset_soc();
			ret = count;
		}
		break;
	case CHARGING_MODE_BOOTING:
		if (sscanf(buf, "%d\n", &x) == 1) {
			lpm_charging_mode = x;
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int s3c_bat_create_attrs(struct device *dev)
{
	int i, rc;
	bat_dbg(" [BAT] %s +  \n", __func__);

	for (i = 0; i < ARRAY_SIZE(s3c_battery_attrs); i++) {
		rc = device_create_file(dev, &s3c_battery_attrs[i]);
		if (rc)
			goto s3c_attrs_failed;
	}
	goto succeed;

s3c_attrs_failed:
	while (i--)
		device_remove_file(dev, &s3c_battery_attrs[i]);
succeed:
	return rc;
}

static irqreturn_t max8998_int_work_func_irq(int irq, void *max8998_chg)
{
	struct chg_data *chg = max8998_chg;

	bat_dbg(" [BAT] %s +  \n", __func__);

	wake_lock(&chg->work_wake_lock);
	
	disable_irq_nosync(max8998_i2c->irq); 

	hrtimer_cancel(&charger_timer);
	hrtimer_start(&charger_timer,
					ktime_set(500 / 1000, (500 % 1000) * 1000000),
					HRTIMER_MODE_REL);
	bat_dbg(" [BAT] %s -  \n", __func__);

	return IRQ_HANDLED;	
}

static enum hrtimer_restart charger_timer_func(struct hrtimer *timer)
{
	bat_dbg(" [BAT] %s +  \n", __func__);

	queue_work(pmic_int_wq, &pmic_int_work);
	bat_dbg(" [BAT] %s -  \n", __func__);
	
	return HRTIMER_NORESTART;
}

void max8998_int_work_func(struct work_struct *work)
{
	int ret;
	u8 data[MAX8998_NUM_IRQ_REGS];
	u8 UsbStatus=0;
	
	struct chg_data *chg = max8998_chg;
       struct i2c_client *i2c = chg->iodev->i2c;

	bat_dbg(" [BAT] %s +  \n", __func__);

	ret = max8998_bulk_read(i2c, MAX8998_REG_IRQ1, MAX8998_NUM_IRQ_REGS, data);


	if(maxim_chg_status())
	  {
			chg->lowbat_warning = false;
			set_tsp_for_ta_detect(1);
			
		 	if(chg->bat_info.batt_health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) 
			{
				s3c_temp_control(chg,POWER_SUPPLY_HEALTH_GOOD);
			}

		
         } 
	  else 
	  {
		  del_timer_sync(&chargingstep_timer);
		  set_tsp_for_ta_detect(0);
	  }

	if(lpm_charging_mode)
	{
		usb_gadget_p->ops->vbus_session(usb_gadget_p, 0);
	}

	if (chg->esafe == MAX8998_ESAFE_ALLOFF)
		chg->esafe = MAX8998_USB_VBUS_AP_ON;
	bat_dbg("%s : cable_status(%d) esafe(%d)\n", __func__, status, chg->esafe);	
	
	power_supply_changed(&chg->psy_ac);
	power_supply_changed(&chg->psy_usb);

	queue_work(chg->monitor_wqueue, &chg->bat_work);

	enable_irq(max8998_i2c->irq);

	bat_dbg(" [BAT] %s - \n", __func__);

}

static __devinit int max8998_charger_probe(struct platform_device *pdev)
{
	struct max8998_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct max8998_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct chg_data *chg;
        struct i2c_client *i2c = iodev->i2c;
	int ret = 0;
       int err = 0;

	bat_dbg(" [BAT] %s +  \n", __func__);

	chg = kzalloc(sizeof(*chg), GFP_KERNEL);
	if (!chg)
		return -ENOMEM;

	chg->iodev = iodev;
	chg->pdata = pdata->charger;

	if (!chg->pdata || !chg->pdata->adc_table) {
		pr_err("%s : No platform data & adc_table supplied\n", __func__);
		ret = -EINVAL;
		goto err_bat_table;
	}

	chg->psy_bat.name = "battery";
	chg->psy_bat.type = POWER_SUPPLY_TYPE_BATTERY;
	chg->psy_bat.properties = max8998_battery_props;
	chg->psy_bat.num_properties = ARRAY_SIZE(max8998_battery_props);
	chg->psy_bat.get_property = s3c_bat_get_property;

	chg->psy_usb.name = "usb";
	chg->psy_usb.type = POWER_SUPPLY_TYPE_USB;
	chg->psy_usb.supplied_to = supply_list;
	chg->psy_usb.num_supplicants = ARRAY_SIZE(supply_list);
	chg->psy_usb.properties = s3c_power_properties;
	chg->psy_usb.num_properties = ARRAY_SIZE(s3c_power_properties);
	chg->psy_usb.get_property = s3c_usb_get_property;

	chg->psy_ac.name = "ac";
	chg->psy_ac.type = POWER_SUPPLY_TYPE_MAINS;
	chg->psy_ac.supplied_to = supply_list;
	chg->psy_ac.num_supplicants = ARRAY_SIZE(supply_list);
	chg->psy_ac.properties = s3c_power_properties;
	chg->psy_ac.num_properties = ARRAY_SIZE(s3c_power_properties);
	chg->psy_ac.get_property = s3c_ac_get_property;

	chg->present = true;
	chg->bat_info.batt_health = POWER_SUPPLY_HEALTH_GOOD;
	chg->bat_info.batt_is_full = false;
	chg->set_batt_full = false;
	chg->set_charge_timeout = false;

	chg->cable_status = CABLE_TYPE_NONE;
	chg->esafe = MAX8998_USB_VBUS_AP_ON;

	mutex_init(&chg->mutex);

	platform_set_drvdata(pdev, chg);

	ret = max8998_write_reg(i2c, MAX8998_REG_IRQM1,
		~(MAX8998_IRQ_DCINR_MASK | MAX8998_IRQ_DCINF_MASK));
	if (ret < 0)
		goto err_kfree;

	ret = max8998_write_reg(i2c, MAX8998_REG_IRQM2, 0xFF);
	if (ret < 0)
		goto err_kfree;

#if defined (ATT_TMO_COMMON)
	ret = max8998_write_reg(i2c, MAX8998_REG_IRQM3, 0xFF);
#else
	ret = max8998_write_reg(i2c, MAX8998_REG_IRQM3, ~MAX8998_IRQ_TOPOFFR_MASK);
#endif
	if (ret < 0)
		goto err_kfree;

#if defined (ATT_TMO_COMMON)
	ret = max8998_write_reg(i2c, MAX8998_REG_IRQM4, 0xFF);
#else
	ret = max8998_write_reg(i2c, MAX8998_REG_IRQM4,
		~(MAX8998_IRQ_LOBAT2_MASK | MAX8998_IRQ_LOBAT1_MASK));
#endif
	if (ret < 0)
		goto err_kfree;

	ret = max8998_update_reg(i2c, MAX8998_REG_ONOFF3,
		(1 << MAX8998_SHIFT_ENBATTMON), MAX8998_MASK_ENBATTMON);
	if (ret < 0)
		goto err_kfree;

#ifndef ATT_TMO_COMMON
	ret = max8998_update_reg(i2c, MAX8998_REG_LBCNFG1, 0x7, 0x37); //3.57V
	if (ret < 0)
		goto err_kfree;

	ret = max8998_update_reg(i2c, MAX8998_REG_LBCNFG2, 0x5, 0x37); //3.4V
	if (ret < 0)
		goto err_kfree;

	max8998_lowbat_config(chg, 0);
#endif

	wake_lock_init(&chg->vbus_wake_lock, WAKE_LOCK_SUSPEND, "vbus_present");
	wake_lock_init(&chg->work_wake_lock, WAKE_LOCK_SUSPEND, "max8998-charger");
	
#if defined (ATT_TMO_COMMON)
//	wake_lock_init(&chg->lowbat_wake_lock, WAKE_LOCK_SUSPEND, "max8998-lowbat");

	INIT_WORK(&stepcharging_work, maxim_stepcharging_work);	
	setup_timer(&chargingstep_timer, chargingstep_timer_func, (unsigned long)chg);
#endif

	INIT_WORK(&chg->bat_work, s3c_bat_work);
	setup_timer(&chg->bat_work_timer, s3c_bat_work_timer_func, (unsigned long)chg);

	chg->monitor_wqueue = create_freezeable_workqueue(dev_name(&pdev->dev));
	if (!chg->monitor_wqueue) {
		pr_err("%s : Failed to create freezeable workqueue\n", __func__);
		ret = -ENOMEM;
		goto err_wake_lock;
	}

#if defined (ATT_TMO_COMMON)
       max8998_chg = chg;
       max8998_i2c = i2c;
	   
       global_cable_status = CABLE_TYPE_NONE;
	latest_cable_status = -1;
       count_check_chg_current = 0;
	count_check_recharging_bat =  0;   
	charging_state = -1;
	event_occur = 0;
	VF_count = 0;
	stepchargingCount = 0;
	ce_for_fuelgauge = 0;

    b_charing  = 0; 
	b_cable_status = 0; 
	b_bat_info_dis_reason = 0;
	b_chg_esafe = 0;
	b_bat_info_batt_soc = 0;
	b_set_batt_full = 0;
	b_bat_info_batt_is_full = 0;
	b_bat_info_batt_temp = 0;
	b_bat_info_batt_health = 0;
	b_bat_info_batt_vcell = 0;
	b_bat_info_event_dev_state = 0;
	b_bat_info_batt_is_recharging = 0;
	b_bat_info_batt_current = 0;
	b_bat_info_batt_v_f_adc = 0;
	vADC_regulator_intialized = 0;

	event_start_time_msec = 0;
	event_total_time_msec = 0;
	
// LDO4 enable

	if(vADC_regulator_intialized == 0) 
	{
		if (IS_ERR_OR_NULL(vADC_regulator)) 
		{
			vADC_regulator = regulator_get(NULL, "vadcldo4");
			printk("vADC_regulator = %p\n", vADC_regulator);

			if (IS_ERR_OR_NULL(vADC_regulator)) {
				pr_err("[ERROR] failed to get vADC_regulator");
				return -1;
			}
		}	


		vADC_regulator_intialized = 1;
	}

	if (IS_ERR_OR_NULL(vADC_regulator) ) 
	{
		pr_err("vADC_regulator  not initialized\n");
		return -EINVAL;
	}
	
	/* Turn LDO4*/
	err = regulator_enable(vADC_regulator);
	if (err) {
			pr_err("[ERROR] Failed to enable vADC_regulator \n");
			return err;
	}
	
#endif

	check_lpm_charging_mode(chg);
  
	/* init power supplier framework */
	ret = power_supply_register(&pdev->dev, &chg->psy_bat);
	if (ret) {
		pr_err("%s : Failed to register power supply psy_bat\n", __func__);
		goto err_wqueue;
	}

	ret = power_supply_register(&pdev->dev, &chg->psy_usb);
	if (ret) {
		pr_err("%s : Failed to register power supply psy_usb\n", __func__);
		goto err_supply_unreg_bat;
	}

	ret = power_supply_register(&pdev->dev, &chg->psy_ac);
	if (ret) {
		pr_err("%s : Failed to register power supply psy_ac\n", __func__);
		goto err_supply_unreg_usb;
	}

//#ifndef ATT_TMO_COMMON

	pmic_int_wq = create_singlethread_workqueue("pmic_int_wq");
	if (!pmic_int_wq)
		return;

	INIT_WORK(&pmic_int_work, max8998_int_work_func );			

	hrtimer_init(&charger_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	charger_timer.function = charger_timer_func;

	ret = request_threaded_irq(iodev->i2c->irq, NULL, max8998_int_work_func_irq,
				   IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "max8998-charger", chg);
	if (ret) {
		pr_err("%s : Failed to request pmic irq\n", __func__);
		goto err_supply_unreg_ac;
	}

	ret = enable_irq_wake(iodev->i2c->irq);
	if (ret) {
		pr_err("%s : Failed to enable pmic irq wake\n", __func__);
		goto err_irq;
	}
//#endif	

	ret = s3c_bat_create_attrs(chg->psy_bat.dev);
	if (ret) {
		pr_err("%s : Failed to create_attrs\n", __func__);
		goto err_irq;
	}

	chg->callbacks.set_cable = max8998_set_cable;
	chg->callbacks.set_esafe = max8998_set_esafe;
	chg->callbacks.get_vdcin = max8998_get_vdcin;
	if (chg->pdata->register_callbacks)
		chg->pdata->register_callbacks(&chg->callbacks);

	wake_lock(&chg->work_wake_lock);
	queue_work(chg->monitor_wqueue, &chg->bat_work);
	bat_dbg(" [BAT] %s -  \n", __func__);

	return false;

err_irq:
	free_irq(iodev->i2c->irq, NULL);
#if defined (ATT_TMO_COMMON)
err_supply_unreg_ac:
	power_supply_unregister(&chg->psy_ac);
#endif	
err_supply_unreg_usb:
	power_supply_unregister(&chg->psy_usb);
err_supply_unreg_bat:
	power_supply_unregister(&chg->psy_bat);
err_wqueue:
	destroy_workqueue(chg->monitor_wqueue);
	cancel_work_sync(&chg->bat_work);
err_wake_lock:
	wake_lock_destroy(&chg->work_wake_lock);
	wake_lock_destroy(&chg->vbus_wake_lock);
	wake_lock_destroy(&chg->lowbat_wake_lock);
err_kfree:
	mutex_destroy(&chg->mutex);
err_bat_table:
	kfree(chg);
	return ret;
}

static int __devexit max8998_charger_remove(struct platform_device *pdev)
{
	struct chg_data *chg = platform_get_drvdata(pdev);
       int err;

	bat_dbg(" [BAT] %s +  \n", __func__);
	free_irq(chg->iodev->i2c->irq, NULL);
	flush_workqueue(chg->monitor_wqueue);
	destroy_workqueue(chg->monitor_wqueue);
	power_supply_unregister(&chg->psy_bat);
	power_supply_unregister(&chg->psy_usb);
	power_supply_unregister(&chg->psy_ac);

	wake_lock_destroy(&chg->work_wake_lock);
	wake_lock_destroy(&chg->vbus_wake_lock);
// LDO4 enable

	if (IS_ERR_OR_NULL(vADC_regulator) ) 
	{
		pr_err("vADC_regulator  not initialized\n");
		return -EINVAL;
	}
	
	/* Turn LDO4*/
	if(!jack_mic_bias)
	{
		err = regulator_disable(vADC_regulator);
		if (err) 
		{
				pr_err("[ERROR] Failed to enable vADC_regulator \n");
				return err;
		}
	}
#if defined (ATT_TMO_COMMON)
//	wake_lock_destroy(&chg->lowbat_wake_lock);
#endif	
	mutex_destroy(&chg->mutex);
	kfree(chg);

	return false;
}

#if defined (ATT_TMO_COMMON)
static void s3c_temp_data_clear(struct chg_data *chg)
{
	int tmp_erase =0;
	bat_dbg(" [BAT] %s +  \n", __func__);

	for(tmp_erase =0; tmp_erase < 10; tmp_erase++){
	chg->adc_sample[S3C_ADC_TEMPERATURE].adc_arr[tmp_erase]=0;
		}
	chg->adc_sample[S3C_ADC_TEMPERATURE].average_adc=0;
	chg->adc_sample[S3C_ADC_TEMPERATURE].total_adc=0;
	chg->adc_sample[S3C_ADC_TEMPERATURE].cnt=0;
	chg->adc_sample[S3C_ADC_TEMPERATURE].index=0;

}
#endif	

static int max8998_charger_suspend(struct device *dev)
{
	struct chg_data *chg = dev_get_drvdata(dev);
#if defined (ATT_TMO_COMMON)
	int err = 0;
//	max8998_lowbat_config(chg, 1);
	if (IS_ERR_OR_NULL(vADC_regulator) ) 
	{
		pr_err("vADC_regulator  not initialized\n");
	}
	
	/* Turn LDO4*/
	if(!jack_mic_bias)
	{	
		err = regulator_disable(vADC_regulator);
		if (err) 
		{
			pr_err("[ERROR] Failed to enable vADC_regulator \n");
		}
	}
#endif	
	bat_dbg(" [BAT] %s +  \n", __func__);
	del_timer_sync(&chg->bat_work_timer);

	return false;
}

static void max8998_charger_resume(struct device *dev)
{
	struct chg_data *chg = dev_get_drvdata(dev);
	bat_dbg(" [BAT] %s +  \n", __func__);
#if defined (ATT_TMO_COMMON)
	int err = 0;
//	max8998_lowbat_config(chg, 0);
      	s3c_temp_data_clear(chg);
	if (IS_ERR_OR_NULL(vADC_regulator) ) 
	{
		pr_err("vADC_regulator  not initialized\n");
	}
	
	/* Turn LDO4*/
       if( ! regulator_is_enabled(vADC_regulator) )
       {
		err = regulator_enable(vADC_regulator);
		if (err) 
		{
				pr_err("[ERROR] Failed to enable vADC_regulator \n");
		}
       }

#endif	
	wake_lock(&chg->work_wake_lock);
	queue_work(chg->monitor_wqueue, &chg->bat_work);
}

static const struct dev_pm_ops max8998_charger_pm_ops = {
	.prepare        = max8998_charger_suspend,
	.complete       = max8998_charger_resume,
};

static struct platform_driver max8998_charger_driver = {
	.driver = {
		.name = "max8998-charger",
		.owner = THIS_MODULE,
		.pm = &max8998_charger_pm_ops,
	},
	.probe = max8998_charger_probe,
	.remove = __devexit_p(max8998_charger_remove),
};

static int __init max8998_charger_init(void)
{
#if defined (ATT_TMO_COMMON)
//NAGSM_Android_SEL_Kernel_Aakash_20100312
//This FD is implemented to control battery charging via DFTA
	battery_class = class_create(THIS_MODULE, "batterychrgcntrl");
	if (IS_ERR(battery_class))
		pr_err("Failed to create class(batterychrgcntrl)!\n");

	s5pc110bat_dev= device_create(battery_class, NULL, 0, NULL, "charging_control");
	if (IS_ERR(s5pc110bat_dev))
		pr_err("Failed to create device(charging_control)!\n");
	if (device_create_file(s5pc110bat_dev, &dev_attr_usbchargingcon) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_usbchargingcon.attr.name);
//NAGSM_Android_SEL_Kernel_Aakash_20100312
#endif	
	return platform_driver_register(&max8998_charger_driver);
}

static void __exit max8998_charger_exit(void)
{
	platform_driver_register(&max8998_charger_driver);
}

late_initcall(max8998_charger_init);
module_exit(max8998_charger_exit);

MODULE_AUTHOR("Minsung Kim <ms925.kim@samsung.com>");
MODULE_DESCRIPTION("S3C6410 battery driver");
MODULE_LICENSE("GPL");

