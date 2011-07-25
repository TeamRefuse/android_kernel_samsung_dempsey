/*
 *  max17040_battery.c
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
#include <linux/max17040_battery.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/fs.h>

#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) // mr work
#define ATT_TMO_COMMON
#endif

#if defined (ATT_TMO_COMMON) 
#include <linux/interrupt.h>
#include <linux/irq.h>
#endif
#define MAX17040_VCELL_MSB	0x02
#define MAX17040_VCELL_LSB	0x03
#define MAX17040_SOC_MSB	0x04
#define MAX17040_SOC_LSB	0x05
#define MAX17040_MODE_MSB	0x06
#define MAX17040_MODE_LSB	0x07
#define MAX17040_VER_MSB	0x08
#define MAX17040_VER_LSB	0x09
#define MAX17040_RCOMP_MSB	0x0C
#define MAX17040_RCOMP_LSB	0x0D
#define MAX17040_CMD_MSB	0xFE
#define MAX17040_CMD_LSB	0xFF

#define MAX17040_DELAY		1000
#define MAX17040_BATTERY_FULL	95

#define MAX17040_MAJOR		174

struct max17040_chip {
	struct i2c_client		*client;
	struct power_supply		battery;
	struct max17040_platform_data	*pdata;
	struct timespec			next_update_time;
	struct device			*fg_atcmd;

	/* State Of Connect */
	int online;
	/* battery voltage */
	int vcell;
	/* battery capacity */
	int soc;
	/* State Of Charge */
	int status;
};

extern struct class *sec_class;
struct i2c_client *fg_i2c_client;

#if defined (ATT_TMO_COMMON)
struct max17040_chip *fg_chip;
#define FUEL_INT_1ST	0
#define FUEL_INT_2ND	1
#define FUEL_INT_3RD	2
#define LOW_BATTERY_IRQ		IRQ_EINT(27)
#define S5PV210_GPH3_3_EXT_INT33_3	(0xf << 12)

int max17040_lowbat_warning;
EXPORT_SYMBOL(max17040_lowbat_warning);

int max17040_low_battery;
EXPORT_SYMBOL(max17040_low_battery);


static int rcomp_status;

static struct work_struct low_bat_work;
static struct wake_lock low_battery_wake_lock;

extern  int ce_for_fuelgauge;
#endif

#if defined(CONFIG_S5PC110_HAWK_BOARD)
extern unsigned int HWREV_HAWK;
#endif

#if defined(CONFIG_S5PC110_DEMPSEY_BOARD)		// mr work
//extern unsigned int HWREV_DEMPSEY;
#endif

static void max17040_update_values(struct max17040_chip *chip);
static void max17043_set_threshold(struct i2c_client *client, int mode);

static int max17040_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17040_chip *chip = container_of(psy,
				struct max17040_chip, battery);
	struct timespec now;

	ktime_get_ts(&now);
	monotonic_to_bootbased(&now);
	if (timespec_compare(&now, &chip->next_update_time) >= 0)
		max17040_update_values(chip);

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
	default:
		return -EINVAL;
	}
	return 0;
}

static int max17040_write_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int max17040_read_reg(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static void max17040_get_vcell(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	u8 msb;
	u8 lsb;

	msb = max17040_read_reg(client, MAX17040_VCELL_MSB);
	lsb = max17040_read_reg(client, MAX17040_VCELL_LSB);

	chip->vcell = ((msb << 4) + (lsb >> 4)) * 1250;
}

#if  defined(CONFIG_S5PC110_KEPLER_BOARD) 
static void max17040_get_soc(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	u8 msb;
	u8 lsb;
	int  pure_soc, adj_soc, soc;

	msb = max17040_read_reg(client, MAX17040_SOC_MSB);
	lsb = max17040_read_reg(client, MAX17040_SOC_LSB);


	pure_soc = msb * 100 +  ((lsb*100)/256);

	 if(ce_for_fuelgauge){
		 adj_soc = ((pure_soc - 130)*10000)/9720; // (FGPureSOC-EMPTY(1.2))/(FULL-EMPTY(?))*100
	 }
	 else{
		 adj_soc = ((pure_soc - 130)*10000)/9430; // (FGPureSOC-EMPTY(1.2))/(FULL-EMPTY(?))*100
	 }
       soc=adj_soc/100;
	   
	if( (soc==4) && adj_soc%100 >= 80){
		soc+=1;
	 	} 
	
	 if( (soc== 0) && (adj_soc>0) ){
	       soc = 1;  
	 	}
	 
	 if(adj_soc <= 0){
	       soc = 0;  	 	
	 	}
	 
	 if(soc>=100)
	 {
		  soc=100;
	 }
	 
//  printk("[ max17043]  max17040_get_soc, pure_soc= %d, adj_soc= %d, soc=%d \n", pure_soc, adj_soc, soc);
 
	chip->soc = soc;
}
#elif  defined(CONFIG_S5PC110_HAWK_BOARD) 
static void max17040_get_soc(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	u8 msb;
	u8 lsb;
	int  pure_soc, adj_soc, soc;

	msb = max17040_read_reg(client, MAX17040_SOC_MSB);
	lsb = max17040_read_reg(client, MAX17040_SOC_LSB);

	pure_soc = msb * 100 +  ((lsb*100)/256);
	
	if((HWREV_HAWK > 0x6) && (HWREV_HAWK < 0xD))
	{
		adj_soc = ((pure_soc - 60)*10000)/9640; // (FGPureSOC-EMPTY(0.6))/(FULL-EMPTY(?))*100
	}
	else
	{
		adj_soc = ((pure_soc - 80)*10000)/9820; // (FGPureSOC-EMPTY(0.8))/(FULL-EMPTY(?))*100
	}
	
       soc=adj_soc/100;
	
	if((soc== 0)&&(adj_soc>=10))
		{
		   soc = 1;  
		}
	if(adj_soc <= 0)
		{
		   soc = 0;		
		}
	if(soc>=100)
		{
	       soc=100;
		}

//  printk("[ max17043] hawk max17040_get_soc, pure_soc= %d, adj_soc= %d, soc=%d \n", pure_soc, adj_soc, soc);
		 
	chip->soc = soc;
}

#elif  defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) 
static void max17040_get_soc(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	u8 msb;
	u8 lsb;
	int  pure_soc, adj_soc, soc;

	msb = max17040_read_reg(client, MAX17040_SOC_MSB);
	lsb = max17040_read_reg(client, MAX17040_SOC_LSB);

	pure_soc = msb * 100 +  ((lsb*100)/256);
	
		adj_soc = ((pure_soc - 30)*10000)/9660; // (FGPureSOC-EMPTY(0.8))/(FULL-EMPTY(?))*100
	
       soc=adj_soc/100;
	
	if((soc== 0)&&(adj_soc>=10))
		{
		   soc = 1;  
		}
	if(adj_soc <= 0)
		{
		   soc = 0;		
		}
	if(soc>=100)
		{
	       soc=100;
		}

 // printk("[ max17043] vibantPlus max17040_get_soc, pure_soc= %d, adj_soc= %d, soc=%d \n", pure_soc, adj_soc, soc);
		 
	chip->soc = soc;
}

#elif  defined(CONFIG_S5PC110_DEMPSEY_BOARD)										// mr work  
static void max17040_get_soc(struct i2c_client *client)
{
		struct max17040_chip *chip = i2c_get_clientdata(client);
	 u8 data[2];
	 int FGPureSOC = 0;
	 int FGAdjustSOC = 0;
	 int FGSOC = 0;
	 
	 u8 msb;
	 u8 lsb;
	 

	 msb = max17040_read_reg(client, MAX17040_SOC_MSB);
	 lsb = max17040_read_reg(client, MAX17040_SOC_LSB);
	 
	 // calculating soc
	 FGPureSOC = msb *100+((lsb*100)/256);
	 	 
		FGAdjustSOC = ((FGPureSOC - 50)*10000)/9550; // empty 0.5   // (FGPureSOC-EMPTY(2))/(FULL-EMPTY2))*100

	 //printk("\n[FUEL] PSOC = %d, ASOC = %d\n", FGPureSOC, FGAdjustSOC);
	 // rounding off and Changing to percentage.
	 FGSOC=FGAdjustSOC/100;

	//if((FGSOC==4)&&FGAdjustSOC%100 >= 80){
	//	FGSOC+=1;
	// 	}
	 //if((FGSOC== 0)&&(FGAdjustSOC>0)){
	 if((FGSOC== 0)&&(FGAdjustSOC>=10)){
	       FGSOC = 1;  
	 	}
	 if(FGAdjustSOC <= 0){
	       FGSOC = 0;  	 	
	 	}
	 if(FGSOC>=100)
	 {
		  FGSOC=100;
	 }
	 
 	chip->soc = FGSOC;
}

#else
static void max17040_get_soc(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	u8 msb;
	u8 lsb;
	uint pure_soc, adj_soc, soc;

	msb = max17040_read_reg(client, MAX17040_SOC_MSB);
	lsb = max17040_read_reg(client, MAX17040_SOC_LSB);

	pure_soc = msb * 100 + (lsb * 100) / 256;

	if (pure_soc >= 100)
		adj_soc = pure_soc;
	else if (pure_soc >= 70)
		adj_soc = 100; // 1%
	else
		adj_soc = 0; // 0%

	if (adj_soc < 1500)
		soc = (adj_soc * 4 / 3 + 50) / 100;
	else if (adj_soc < 7600)
		soc = adj_soc / 100 + 5;
	else
		soc = ((adj_soc - 7600) * 8 / 10 + 50) / 100 + 81;

	chip->soc = min(soc, (uint)100);
}
#endif

static void max17040_get_version(struct i2c_client *client)
{
	u8 msb;
	u8 lsb;

	msb = max17040_read_reg(client, MAX17040_VER_MSB);
	lsb = max17040_read_reg(client, MAX17040_VER_LSB);

	dev_info(&client->dev, "MAX17040 Fuel-Gauge Ver %d%d\n", msb, lsb);
}

static void max17040_get_online(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

	if (chip->pdata && chip->pdata->battery_online)
		chip->online = chip->pdata->battery_online();
	else
		chip->online = 1;
}

static void max17040_get_status(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

	if (!chip->pdata || !chip->pdata->charger_online ||
		!chip->pdata->charger_enable) {
		chip->status = POWER_SUPPLY_STATUS_UNKNOWN;
		return;
	}

	if (chip->pdata->charger_online()) {
		if (chip->pdata->charger_enable())
			chip->status = POWER_SUPPLY_STATUS_CHARGING;
		else
			chip->status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	} else {
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	if (chip->soc > MAX17040_BATTERY_FULL)
		chip->status = POWER_SUPPLY_STATUS_FULL;
}

static int max17040_reset_chip(struct i2c_client *client)
{
	int ret;
	u16 rst_cmd = 0x4000;

	ret = i2c_smbus_write_word_data(client, MAX17040_MODE_MSB, swab16(rst_cmd));
	msleep(500);

	return ret;
}

void max17040_reset_soc(void)
{
	struct i2c_client *client = fg_i2c_client;
	max17040_reset_chip(client);
}
EXPORT_SYMBOL(max17040_reset_soc);

#if  defined(CONFIG_S5PC110_KEPLER_BOARD) 
static void max17043_set_threshold(struct i2c_client *client, int mode)
{
	u16 regValue;

	switch (mode) 
		{
			case FUEL_INT_1ST:
				regValue = 0x10; // 15%
				rcomp_status = 0;
				break;	
				
			case FUEL_INT_2ND:
 				regValue = 0x1A; // 5%
				rcomp_status = 1;
				
				break;
			case FUEL_INT_3RD:
 				regValue = 0x1E; // 1%
				rcomp_status = 2;
				break;
				
 			default:
 				regValue = 0x1E; // 1%
				rcomp_status = 2;				
				break;		
 		}

	regValue = fg_chip->pdata->rcomp_value |regValue;
    //   printk("[ max17043]  max17043_set_threshold %d, regValue = %x \n",mode,regValue);
	
	i2c_smbus_write_word_data(client, MAX17040_RCOMP_MSB,   swab16(regValue));		
 }
#elif  defined(CONFIG_S5PC110_HAWK_BOARD) 
static void max17043_set_threshold(struct i2c_client *client, int mode)
{
//	struct i2c_client *client = fg_i2c_client;
	u16 regValue;

	switch (mode) 
		{
			case FUEL_INT_1ST:
				regValue = 0x10; // 15%
				rcomp_status = 0;
				break;	
				
			case FUEL_INT_2ND:
 				regValue = 0x1A; // 5%
				rcomp_status = 1;
				
				break;
			case FUEL_INT_3RD:
 				regValue = 0x1F; // 1%
				rcomp_status = 2;
				break;
				
 			default:
 				regValue = 0x1F; // 1%
				rcomp_status = 2;				
				break;		
 		}
 
	if((HWREV_HAWK > 0x6) && (HWREV_HAWK < 0xD))
	{
		regValue = 0xC000 | regValue;
	}
	else
	{
		regValue = fg_chip->pdata->rcomp_value |regValue;
	}
	 //printk("[ max17043]  max17043_set_threshold %d, regValue = %x \n",mode,regValue);
	i2c_smbus_write_word_data(client, MAX17040_RCOMP_MSB,   swab16(regValue));		
	
 }

#elif  defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) 
static void max17043_set_threshold(struct i2c_client *client, int mode)
{
//	struct i2c_client *client = fg_i2c_client;
	u16 regValue;

	switch (mode) 
		{
			case FUEL_INT_1ST:
				regValue = 0x10; // 15%
				rcomp_status = 0;
				break;	
				
			case FUEL_INT_2ND:
 				regValue = 0x1A; // 5%
				rcomp_status = 1;
				
				break;
			case FUEL_INT_3RD:
 				regValue = 0x1F; // 1%
				rcomp_status = 2;
				break;
				
 			default:
 				regValue = 0x1F; // 1%
				rcomp_status = 2;				
				break;		
 		}
 
		regValue = fg_chip->pdata->rcomp_value |regValue;
	 //printk("[ max17043]  max17043_set_threshold %d, regValue = %x \n",mode,regValue);
	i2c_smbus_write_word_data(client, MAX17040_RCOMP_MSB,   swab16(regValue));		
	
 }

#elif  defined(CONFIG_S5PC110_DEMPSEY_BOARD)								// mr work  
static void max17043_set_threshold(struct i2c_client *client, int mode)
{
//	struct i2c_client *client = fg_i2c_client;
	u16 regValue;

	switch (mode) 
		{
			case FUEL_INT_1ST:
				regValue = 0x10; // 15%
				rcomp_status = 0;
				break;	
				
			case FUEL_INT_2ND:
 				regValue = 0x1A; // 5%
				rcomp_status = 1;
				
				break;
			case FUEL_INT_3RD:
 				regValue = 0x1F; // 1%
				rcomp_status = 2;
				break;
				
 			default:
 				regValue = 0x1F; // 1%
				rcomp_status = 2;				
				break;		
 		}

		regValue = fg_chip->pdata->rcomp_value |regValue;
	 //printk("[ max17043]  max17043_set_threshold %d, regValue = %x \n",mode,regValue);
	i2c_smbus_write_word_data(client, MAX17040_RCOMP_MSB,   swab16(regValue));		
	
 }
 #endif 
 
#if defined (ATT_TMO_COMMON) 
static irqreturn_t low_battery_isr(int irq, void * client)
{
 	schedule_work(&low_bat_work);
 	
	return IRQ_HANDLED;
}


static void s3c_low_bat_work(struct work_struct *work)
{
  struct i2c_client *client  = fg_i2c_client;
  struct max17040_chip *chip = i2c_get_clientdata(client);

  wake_lock(&low_battery_wake_lock);
  
  max17040_get_soc(client);
  
  if ( chip->soc < 0)
  {
	dev_err(&client->dev,"%s : Failed to request pmic irq\n", __func__);
    return;	
  }
  
  printk("[ max17043]  s3c_low_bat_work [LBW] RCS= %d, SOC= %d\n", rcomp_status, chip->soc);

  switch (rcomp_status){
  	case 0:
		if(chip->soc <= 18){
			max17043_set_threshold(chip->client, FUEL_INT_2ND);
			wake_lock_timeout(&low_battery_wake_lock, HZ * 5);
			}
		else{
			max17043_set_threshold(chip->client, FUEL_INT_1ST);		
			wake_lock_timeout(&low_battery_wake_lock, HZ * 5);		
			}
		break;
	case 1:
		if(chip->soc <= 8){		
			max17040_lowbat_warning = 1;
			max17043_set_threshold(chip->client, FUEL_INT_3RD);
			wake_lock_timeout(&low_battery_wake_lock, HZ * 5);
			}
		else{
			max17043_set_threshold(chip->client, FUEL_INT_2ND);			
			wake_lock_timeout(&low_battery_wake_lock, HZ * 5);		
			}
		break;
	case 2:
		if(chip->soc <= 3){			
			max17040_low_battery = 1;
			wake_lock_timeout(&low_battery_wake_lock, HZ * 30);		
			}
		else{
			max17043_set_threshold(chip->client, FUEL_INT_3RD);			
			wake_lock_timeout(&low_battery_wake_lock, HZ * 5);		
			}
		break;
	default:
		dev_err(&client->dev,"%s : Failed to request pmic irq\n", __func__);	// [[ junghyunseok edit for exception code 20100511	
		break;		
  }
}

#endif

static void max17040_update_values(struct max17040_chip *chip)
{
      int i;
      int value;
	  
	max17040_get_vcell(chip->client);
	max17040_get_soc(chip->client);
	max17040_get_online(chip->client);
	max17040_get_status(chip->client);

#if defined (CONFIG_S5PC110_HAWK_BOARD )	 || defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD)
   	if(chip->soc >= 16)
		{
			max17043_set_threshold(chip->client, FUEL_INT_1ST);
		}
	else if(chip->soc >= 6){		
			max17043_set_threshold(chip->client, FUEL_INT_2ND);		
		}
	else{
			max17043_set_threshold(chip->client, FUEL_INT_3RD);			
		}

#elif defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD)	
	 if(chip->soc >= 6){	
			max17043_set_threshold(chip->client, FUEL_INT_2ND);		
		}
	else{
			max17043_set_threshold(chip->client, FUEL_INT_3RD);			
		}
#else
			max17043_set_threshold(chip->client, FUEL_INT_3RD);	
#endif	

#ifdef fg_debug
	printk("[ max17043] DUMP : ");	
	for (i = 0x2; i <= 0xD ; i++)
		{
                   value = max17040_read_reg(chip->client, i);
					printk(" %x= %x ", i , value);					   
		}
	printk(" \n");	
#endif
	/* next update must be at least 1 second later */
	ktime_get_ts(&chip->next_update_time);
	monotonic_to_bootbased(&chip->next_update_time);
	chip->next_update_time.tv_sec++;
}

static ssize_t max17040_show_fg_soc(struct device *device,
				    struct device_attribute *attr, char *buf)
{
	struct max17040_chip *chip = (struct max17040_chip *)dev_get_drvdata(device);

	max17040_get_soc(chip->client);

	return sprintf(buf, "%d\n", chip->soc);	
}
static DEVICE_ATTR(set_fuel_gauage_read, 0664, max17040_show_fg_soc, NULL);

static ssize_t max17040_show_fg_reset(struct device *device,
				      struct device_attribute *attr, char *buf)
{
	struct max17040_chip *chip = (struct max17040_chip *)dev_get_drvdata(device);
	int ret;

	ret = max17040_reset_chip(chip->client);
	max17040_get_soc(chip->client);

	return sprintf(buf,"%d\n", ret);
}
static DEVICE_ATTR(set_fuel_gauage_reset, 0664, max17040_show_fg_reset, NULL);

static enum power_supply_property max17040_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
};

static int __devinit max17040_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17040_chip *chip;
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
	chip->battery.get_property	= max17040_get_property;
	chip->battery.properties	= max17040_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(max17040_battery_props);
	
	if (chip->pdata && chip->pdata->power_supply_register)
		ret = chip->pdata->power_supply_register(&client->dev, &chip->battery);
	else
		ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		goto err_psy_register;
	}

	if (chip->pdata)
	{
#if defined(CONFIG_S5PC110_HAWK_BOARD) 
		if((HWREV_HAWK > 0x6) && (HWREV_HAWK < 0xD))
		{
    		i2c_smbus_write_word_data(client, MAX17040_RCOMP_MSB,   swab16(0xC000));	
    	}
    	else
    	{
    		i2c_smbus_write_word_data(client, MAX17040_RCOMP_MSB,   swab16(chip->pdata->rcomp_value));	
    	}
#else
		i2c_smbus_write_word_data(client, MAX17040_RCOMP_MSB,   swab16(chip->pdata->rcomp_value));	
#endif
	}
   
   

#if defined (ATT_TMO_COMMON)
       fg_i2c_client = client;
       fg_chip = chip;
	   
	rcomp_status = -1;
	max17040_low_battery = 0;
       max17040_lowbat_warning = 0;
	   
	max17040_update_values(chip);
	max17040_get_version(client);

	INIT_WORK(&low_bat_work, s3c_low_bat_work);	
       wake_lock_init(&low_battery_wake_lock, WAKE_LOCK_SUSPEND, "low_battery_wake_lock");
       	
//	ret = request_threaded_irq(  client->irq  , NULL, low_battery_isr,
//							   IRQF_TRIGGER_FALLING, "fuel gauge low battery irq", (void *) client);

	set_irq_type(LOW_BATTERY_IRQ, IRQ_TYPE_EDGE_FALLING);
	ret = request_irq(LOW_BATTERY_IRQ, low_battery_isr, IRQF_SAMPLE_RANDOM, "low battery irq", (void *) client);

	if (ret) {
					dev_err(&client->dev,"%s : Failed to request pmic irq\n", __func__);
					goto err_fg_atcmd;	
		 	}

	ret = enable_irq_wake(client->irq);
	if (ret) {
					dev_err(&client->dev, "%s : Failed to enable pmic irq wake\n", __func__);
					goto err_irq;
	          }
#endif

	chip->fg_atcmd = device_create(sec_class, NULL, MKDEV(MAX17040_MAJOR, 0),
					NULL, "fg_atcom_test");
	if (IS_ERR(chip->fg_atcmd)) {
		dev_err(&client->dev, "failed to create fg_atcmd\n");
		goto err_fg_atcmd;
	}

	dev_set_drvdata(chip->fg_atcmd, chip);

	ret = device_create_file(chip->fg_atcmd, &dev_attr_set_fuel_gauage_read);
	if (ret)
		goto err_fg_read;

	ret = device_create_file(chip->fg_atcmd, &dev_attr_set_fuel_gauage_reset);
	if (ret)
		goto err_fg_reset;

	return 0;
	
#if defined (CONFIG_S5PC110_KEPLER_BOARD) || defined (CONFIG_S5PC110_HAWK_BOARD) || defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) // mr work 
err_irq:
		free_irq(client->irq, NULL);
#endif
err_fg_reset:
	device_remove_file(chip->fg_atcmd, &dev_attr_set_fuel_gauage_read);
err_fg_read:
	device_destroy(sec_class, MKDEV(MAX17040_MAJOR, 0));
err_fg_atcmd:
	if (chip->pdata && chip->pdata->power_supply_unregister)
		chip->pdata->power_supply_unregister(&chip->battery);
	else
		power_supply_unregister(&chip->battery);

err_psy_register:
	kfree(chip);

	return ret;
}

static int __devexit max17040_remove(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

	device_remove_file(chip->fg_atcmd, &dev_attr_set_fuel_gauage_reset);
	device_remove_file(chip->fg_atcmd, &dev_attr_set_fuel_gauage_read);
	device_destroy(sec_class, MKDEV(MAX17040_MAJOR, 0));

	if (chip->pdata && chip->pdata->power_supply_unregister)
		chip->pdata->power_supply_unregister(&chip->battery);
	else
		power_supply_unregister(&chip->battery);
	kfree(chip);
	return 0;
}

static const struct i2c_device_id max17040_id[] = {
	{ "max17040", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17040_id);

static struct i2c_driver max17040_i2c_driver = {
	.driver	= {
		.name	= "max17040",
	},
	.probe		= max17040_probe,
	.remove		= __devexit_p(max17040_remove),
	.id_table	= max17040_id,
};

static int __init max17040_init(void)
{
	return i2c_add_driver(&max17040_i2c_driver);
}
module_init(max17040_init);

static void __exit max17040_exit(void)
{
	i2c_del_driver(&max17040_i2c_driver);
}
module_exit(max17040_exit);

MODULE_AUTHOR("Minkyu Kang <mk7.kang@samsung.com>");
MODULE_DESCRIPTION("MAX17040 Fuel Gauge");
MODULE_LICENSE("GPL");
