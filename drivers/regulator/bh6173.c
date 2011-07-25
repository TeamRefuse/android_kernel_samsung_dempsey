/*
 * bh6173.c  --  Voltage and current regulation for the ROHM BH6173GUL
 *
 * Copyright (C) 2010 Samsung Electronics
 * Aakash Manik <aakash.manik@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/mfd/bh6173.h> //ansari
#include <linux/mutex.h>
#include <linux/delay.h>
#include<linux/slab.h> //ansari

#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/hardware.h>
#include <mach/gpio.h>

#define DBG(fmt...)				
//#define DBG(fmt...) printk(fmt)


/* Registers */
#define BH6173GUL_Register_REGCNT		0x00	// initial value 0x00
#define BH6173GUL_Register_SWADJ		0x01	// initial value 0x04
#define BH6173GUL_Register_LDOADJ1		0x02	// initial value 0xC8
#define BH6173GUL_Register_LDOADJ2		0x03	// initial value 0x0C
#define BH6173GUL_Register_PDSEL		0x04	// initial value 0x00
#define BH6173GUL_Register_PDCNT		0x05	// initial value 0x0F
#define BH6173GUL_Register_SWREGCNT		0x06	// initial value 0x01

#define BH6173GUL_REG		0x01
#define BH6173GUL_LDO1		0x01
#define BH6173GUL_LDO2		0x02
#define BH6173GUL_LDO3		0x04

struct bh6173_data {
	struct i2c_client	*client;
	struct device		*dev;

	struct mutex		mutex;

	int			num_regulators;
	struct regulator_dev	**rdev;
};

struct i2c_client	*bh6173_client;

struct bh6173_data *client_6173data_p = NULL;

static u8 bh6173_cache_regs[]={	
 0x00, 0x04, 0xC8, 0x0C, 0x00, 0x0F, 0x01,
};

static const int ldo12_6173_voltage_map[] = {
	 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1850, 2600, 2700, 2800, 2850, 3000, 3300,
};

static const int ldo3_6173_voltage_map[] = {
	 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1850, 1900, 2000, 2600, 2700, 2800, 2850, 3000, 3300,
};

static const int *ldo_6173_voltage_map[] = {
	NULL,
	ldo12_6173_voltage_map,	/* LDO1~2 */
	ldo12_6173_voltage_map,	/* LDO1~2 */
	ldo3_6173_voltage_map,	/* LDO3 */
};
//[NAGSM_Android_HDLNC_SeoJW_20101230 : check HWREV
extern unsigned int HWREV;

static int bh6173_i2c_cache_read(struct i2c_client *client, u8 reg, u8 *dest)
{
	*dest = bh6173_cache_regs[reg];
	return 0;
}

static int bh6173_i2c_read(struct i2c_client *client, u8 reg, u8 *dest)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
 
	if (ret < 0)
		return -EIO;
		
	bh6173_cache_regs[reg] = ret;

	DBG("func =%s : reg = %d, value = %x\n",__func__,reg, ret);  

	*dest = ret & 0xff;
	return 0;
}

static int bh6173_i2c_write(struct i2c_client *client, u8 reg, u8 value)
{
	DBG("func =%s : reg = %d, value = %x\n",__func__,reg, value); 

	bh6173_cache_regs[reg] = value;
	
	return i2c_smbus_write_byte_data(client, reg, value);
}

static u8 bh6173_read_reg(struct bh6173_data *bh6173, u8 reg)
{

	u8 val = 0;
	DBG("func =%s called for reg= %x\n",__func__,reg);

#ifndef CONFIG_CPU_FREQ
	mutex_lock(&bh6173->mutex);
#endif
	bh6173_i2c_cache_read(bh6173->client, reg, &val);
#ifndef CONFIG_CPU_FREQ
	mutex_unlock(&bh6173->mutex);
#endif
	DBG("func =%s called for reg= %x\n",__func__,reg, val);
	return val;
}

static int bh6173_write_reg(struct bh6173_data *bh6173, u8 value, u8 reg)
{
	DBG("func =%s called for reg= %x, data=%x\n",__func__,reg,value);

#ifndef CONFIG_CPU_FREQ
	mutex_lock(&bh6173->mutex);
#endif

	bh6173_i2c_write(bh6173->client, reg, value);
#ifndef CONFIG_CPU_FREQ
	mutex_unlock(&bh6173->mutex);
#endif
	return 0;
}


static int bh6173_get_ldo(struct regulator_dev *rdev)
{
	
	if (!rdev)
		return -EINVAL;		//NAGSM_Android_SEL_Kernel_Aakash_20101021

	return rdev_get_id(rdev);
}


static int bh6173_ldo_is_enabled(struct regulator_dev *rdev)
{

	if (!rdev)
		return -EINVAL;		//NAGSM_Android_SEL_Kernel_Aakash_20101021

	struct bh6173_data *bh6173 = rdev_get_drvdata(rdev);
	int ldo = bh6173_get_ldo(rdev);
	u8 value, shift;   

	if ((ldo < BH6173_LDO1) || (ldo > BH6173_LDO3)) {
                printk("ERROR: Invalid argument passed\n");
                return -EINVAL;
        }

	value = bh6173_read_reg(bh6173, BH6173GUL_Register_REGCNT);
	shift = ldo;											//NAGSM_Android_SEL_Kernel_Aakash_20101019
	return (value >> shift) & 0x1;
}


//NAGSM_Android_SEL_Kernel_Aakash_20101021
//ldo = 0	for SWREG
//ldo = 1~3 for ldo1~ldo3

int bh6173_ldo_is_enabled_status(int ldo)
{
	struct bh6173_data *bh6173 = client_6173data_p;
	
	u8 value, shift;   

	if(ldo <= BH6173_LDO3)	//SWREG = 0, LDO1~LDO3 (1-3)
	{
	value = bh6173_read_reg(bh6173, BH6173GUL_Register_REGCNT);
	shift = ldo;											
	}
	else {
                printk("ERROR: Invalid argument passed\n");
                return -EINVAL;
    }

	
	return (value >> shift) & 0x1;
}
EXPORT_SYMBOL_GPL(bh6173_ldo_is_enabled_status);
//NAGSM_Android_SEL_Kernel_Aakash_20101021


static int bh6173_ldo_enable(struct regulator_dev *rdev)
{

	if (!rdev)
		return -EINVAL;		//NAGSM_Android_SEL_Kernel_Aakash_20101021

	struct bh6173_data *bh6173 = rdev_get_drvdata(rdev);

	
	int ldo = bh6173_get_ldo(rdev);
	u8 value;   

	DBG("func =%s called for regulator = %d\n",__func__,ldo);

	//NAGSM_Android_SEL_Kernel_Aakash_20101019
	if((ldo < BH6173_LDO1) || (ldo > BH6173_LDO3))
	{
		printk("ERROR: Invalid argument passed\n");
		return -EINVAL;
	}
	//NAGSM_Android_SEL_Kernel_Aakash_20101019

	value = bh6173_read_reg(bh6173, BH6173GUL_Register_REGCNT);

	if (ldo == BH6173_LDO1)
		value |= BH6173GUL_REGCNT_LDO1;
	if (ldo == BH6173_LDO2)
		value |= BH6173GUL_REGCNT_LDO2;
	if (ldo == BH6173_LDO3)
		value |= BH6173GUL_REGCNT_LDO3;
		

	return bh6173_write_reg(bh6173, value, BH6173GUL_Register_REGCNT);
 
}

static int bh6173_ldo_disable(struct regulator_dev *rdev)
{

	if (!rdev)
		return -EINVAL;		//NAGSM_Android_SEL_Kernel_Aakash_20101021

	struct bh6173_data *bh6173 = rdev_get_drvdata(rdev);
	int ldo = bh6173_get_ldo(rdev);
	u8 value, shift;   
	
	DBG("func =%s called for regulator = %d\n",__func__,ldo);

	if (ldo <= BH6173_LDO3) { /*LDO1~LDO3*/
		value = bh6173_read_reg(bh6173, BH6173GUL_Register_REGCNT);
		shift = ldo;
		value &= ~(1 << shift);
		bh6173_write_reg(bh6173, value, BH6173GUL_Register_REGCNT);
	}
	else{
		DBG("func =%s Invalid Param\n",__func__);
		return -EINVAL;
	}

	return 0;
}

static int bh6173_ldo_get_voltage(struct regulator_dev *rdev)
{

	if (!rdev)
		return -EINVAL;		//NAGSM_Android_SEL_Kernel_Aakash_20101021

	struct bh6173_data *bh6173 = rdev_get_drvdata(rdev);
	int ldo = bh6173_get_ldo(rdev);
	int value, shift = 0, mask = 0xff, reg;

	DBG("func =%s called for regulator = %d\n",__func__,ldo);

	if (ldo == BH6173_LDO1) {
		reg  = BH6173GUL_Register_LDOADJ1;		//NAGSM_Android_SEL_Kernel_Aakash_20101019
	} else if (ldo == BH6173_LDO2) {
		reg  = BH6173GUL_Register_LDOADJ1;		//NAGSM_Android_SEL_Kernel_Aakash_20101019
	} else if (ldo == BH6173_LDO3) {
		reg  = BH6173GUL_Register_LDOADJ2;		//NAGSM_Android_SEL_Kernel_Aakash_20101019
	}
	value = bh6173_read_reg(bh6173, reg); 

	if (ldo == BH6173_LDO2)
	{
		value = value >> 4;
	}

	return 1000 * ldo_6173_voltage_map[ldo][value];
	
}

static int bh6173_ldo_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV)
{

	if (!rdev)
		return -EINVAL;		//NAGSM_Android_SEL_Kernel_Aakash_20101021

	struct bh6173_data *bh6173 = rdev_get_drvdata(rdev);
	int ldo = bh6173_get_ldo(rdev);
	int value, shift = 0, mask = 0xff, reg;
	int min_vol = min_uV / 1000, max_vol = max_uV / 1000;


	const int *vol_map = ldo_6173_voltage_map[ldo];
	int i = 0;
 
	DBG("func =%s called for regulator = %d, min_v=%d, max_v=%d\n",__func__,ldo,min_uV,max_uV);

	if (min_vol < 800 ||
	    max_vol > 3300)
		return -EINVAL;

	while (vol_map[i]) {
		if (vol_map[i] >= min_vol)
			break;
		i++;
	}

 value = i;
	
	DBG("Found voltage=%d, i = %x\n",vol_map[i], i);

	if (!vol_map[i])
		return -EINVAL;

	if (vol_map[i] > max_vol)
		return -EINVAL;

	if (ldo == BH6173_LDO1) {
		reg  = BH6173GUL_Register_LDOADJ1;
	} else if (ldo == BH6173_LDO2) {
		reg  = BH6173GUL_Register_LDOADJ1;
	} else if (ldo == BH6173_LDO3) {
		reg  = BH6173GUL_Register_LDOADJ2;
	}

//	value = bh6173_read_reg(bh6173, reg);
//	value &= ~mask;
//	value |= (i << shift);

	if(ldo == BH6173_LDO2)
	{
		value = value << 4;
		value = (value | (0x0F & (bh6173_read_reg(bh6173, reg))));	//NAGSM_Android_SEL_Kernel_Aakash_20101019
	}

	bh6173_write_reg(bh6173, value, reg);
	
	return 0;
}


int REG_power_onoff(int ONOFF_REG)
{
	struct bh6173_data *bh6173 = client_6173data_p;
	int return_value = 0;	
	u8 value = 0; 
    u8 temp =0;
	value = bh6173_read_reg(bh6173, BH6173GUL_Register_REGCNT);	//NAGSM_Android_SEL_Kernel_Aakash_20101021

	if (ONOFF_REG)
		value |= BH6173GUL_REGCNT_REG;
	else 
		value &= ~BH6173GUL_REGCNT_REG;
//[NAGSM_Android_HDLNC_SeoJW_20101209 : REG_SETTING		
	if(HWREV <= 0x06){
		bh6173_write_reg(bh6173, BH6173GUL_SWADJ_180, BH6173GUL_Register_SWADJ);
		temp &= 0xF0;
		temp |= BH6173GUL_LDO1_280;

		bh6173_write_reg(bh6173, temp, BH6173GUL_Register_LDOADJ1);
		temp &= 0x0F;
		temp |= BH6173GUL_LDO2_180;
		bh6173_write_reg(bh6173, temp, BH6173GUL_Register_LDOADJ1);
		temp = BH6173GUL_LDO3_280;
		bh6173_write_reg(bh6173, temp, BH6173GUL_Register_LDOADJ2);
		printk("[bh6173] check HWREV <= 5\n");
	}else{
		printk("[bh6173] check HWREV > 5 \n");
	}
//[NAGSM_Android_HDLNC_SeoJW_20101209 : REG_SETTING	
	return bh6173_write_reg(bh6173, value, BH6173GUL_Register_REGCNT);
}



int bh6173_ldo_enable_direct(int ldo)
{
	struct bh6173_data *bh6173 = client_6173data_p;
	u8 value, shift, temp;   

	if((ldo < BH6173_LDO1) || (ldo > BH6173_LDO3))
	{
		printk("ERROR: Invalid argument passed\n");
		return -EINVAL;
	}
	if(BH6173_LDO3)
	{
		temp = BH6173GUL_LDO3_280;
		bh6173_write_reg(bh6173, temp, BH6173GUL_Register_LDOADJ2);
	}
	DBG("func =%s called for regulator = %d\n",__func__,ldo);

	value = bh6173_read_reg(bh6173, BH6173GUL_Register_REGCNT);

    if (ldo == BH6173_LDO1)
		value |= BH6173GUL_REGCNT_LDO1;
	if (ldo == BH6173_LDO2)
		value |= BH6173GUL_REGCNT_LDO2;
	if (ldo == BH6173_LDO3)
		value |= BH6173GUL_REGCNT_LDO3;
		

	return bh6173_write_reg(bh6173, value, BH6173GUL_Register_REGCNT);

}
EXPORT_SYMBOL_GPL(bh6173_ldo_enable_direct);

int bh6173_ldo_disable_direct(int ldo)
{
	struct bh6173_data *bh6173 = client_6173data_p;
	u8 value, shift;   

	if (ldo <= BH6173_LDO3) { /*LDO1~LDO3*/
		value = bh6173_read_reg(bh6173, BH6173GUL_Register_REGCNT);
		shift = ldo;
		value &= ~(1 << shift);
		bh6173_write_reg(bh6173, value, BH6173GUL_Register_REGCNT);
	}
	else{
		DBG("func =%s Invalid Param\n",__func__);
		return -EINVAL;
	}

	return 0;

}
EXPORT_SYMBOL_GPL(bh6173_ldo_disable_direct);

//NAGSM_Android_SEL_Kernel_Aakash_20101019
int bh6173_ldo_get_voltage_direct(int ldo)
{
	struct bh6173_data *bh6173 = client_6173data_p;
	int value, reg;

	DBG("func =%s called for regulator = %d\n",__func__,ldo);

	if (ldo == BH6173_LDO1) {
		reg  = BH6173GUL_Register_LDOADJ1;		
	} else if (ldo == BH6173_LDO2) {
		reg  = BH6173GUL_Register_LDOADJ1;		
	} else if (ldo == BH6173_LDO3) {
		reg  = BH6173GUL_Register_LDOADJ2;		
	}
	value = bh6173_read_reg(bh6173, reg); 

	if (ldo == BH6173_LDO2)
	{
		value = value >> 4;
	}

	return 1000 * ldo_6173_voltage_map[ldo][value];
	
}
EXPORT_SYMBOL_GPL(bh6173_ldo_get_voltage_direct);
//NAGSM_Android_SEL_Kernel_Aakash_20101019


int bh6173_ldo_set_voltage_direct(int ldo,
				int min_uV, int max_uV)
{
	struct bh6173_data *bh6173 = client_6173data_p;
	int value, shift = 0, mask = 0xff, reg;
	int min_vol = min_uV / 1000, max_vol = max_uV / 1000;
	

	const int *vol_map = ldo_6173_voltage_map[ldo];
	int i = 0;

	if((ldo < BH6173_LDO1) || (ldo > BH6173_LDO3))
	{
		printk("ERROR: Invalid argument passed\n");
		return -EINVAL;
	}
 
	DBG("func =%s called for regulator = %d, min_v=%d, max_v=%d\n",__func__,ldo,min_uV,max_uV);

	if (min_vol < 800 ||
	    max_vol > 3300)
		return -EINVAL;

	while (vol_map[i]) {
		if (vol_map[i] >= min_vol)
			break;
		i++;
	}

 value = i;
	
	DBG("Found voltage=%d, i = %x\n",vol_map[i], i);

	if (!vol_map[i])
		return -EINVAL;

	if (vol_map[i] > max_vol)
		return -EINVAL;

	if (ldo == BH6173_LDO1) {
		reg  = BH6173GUL_Register_LDOADJ1;
	} else if (ldo == BH6173_LDO2) {
		reg  = BH6173GUL_Register_LDOADJ1;
	} else if (ldo == BH6173_LDO3) {
		reg  = BH6173GUL_Register_LDOADJ2;
	}

	
	if(ldo == BH6173_LDO2)
	{
		value = value << 4;
		value = (value | (0x0F & (bh6173_read_reg(bh6173, reg))));	//NAGSM_Android_SEL_Kernel_Aakash_20101019
	}

	bh6173_write_reg(bh6173, value, reg);

	return 0;
}

EXPORT_SYMBOL_GPL(bh6173_ldo_set_voltage_direct);


static struct regulator_ops bh6173_ldo_ops = {
	.is_enabled	= bh6173_ldo_is_enabled,
	.enable		= bh6173_ldo_enable,
	.disable	= bh6173_ldo_disable,
	.get_voltage	= bh6173_ldo_get_voltage,
	.set_voltage	= bh6173_ldo_set_voltage,
};



static struct regulator_desc regulators_6173[] = {
	{
		.name		= "6173_LDO1",
		.id		= BH6173_LDO1,
		.ops		= &bh6173_ldo_ops,
		.n_voltages	= ARRAY_SIZE(ldo12_6173_voltage_map),
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "6173_LDO2",
		.id		= BH6173_LDO2,
		.ops		= &bh6173_ldo_ops,
		.n_voltages	= ARRAY_SIZE(ldo12_6173_voltage_map),
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, {
		.name		= "6173_LDO3",
		.id		= BH6173_LDO3,
		.ops		= &bh6173_ldo_ops,
		.n_voltages	= ARRAY_SIZE(ldo3_6173_voltage_map),
		.type		= REGULATOR_VOLTAGE,
		.owner		= THIS_MODULE,
	}, 
};


u8 pulldown_select(u8 reg_ldo, u8 registance)
{
	struct bh6173_data *bh6173 = client_6173data_p;
	u8 return_value = 0;
	u8 pdsel;

	pdsel = bh6173_read_reg(bh6173, BH6173GUL_Register_PDSEL);

	if (BH6173GUL_PULLDOWN_1K == registance) {
		if (reg_ldo&BH6173GUL_PULLDOWN_REG)
			pdsel &= 0xFE;
		if (reg_ldo&BH6173GUL_PULLDOWN_LDO1)
			pdsel &= 0xFD;
		if (reg_ldo&BH6173GUL_PULLDOWN_LDO2)
			pdsel &= 0xFB;
		if (reg_ldo&BH6173GUL_PULLDOWN_LDO3)
			pdsel &= 0xF7;	
	}
	else if (BH6173GUL_PULLDOWN_10K == registance) {
		if (reg_ldo&BH6173GUL_PULLDOWN_REG)
			pdsel |= 0x01;
		if (reg_ldo&BH6173GUL_PULLDOWN_LDO1)
			pdsel |= 0x02;
		if (reg_ldo&BH6173GUL_PULLDOWN_LDO2)
			pdsel |= 0x04;
		if (reg_ldo&BH6173GUL_PULLDOWN_LDO3)
			pdsel |= 0x08;	
	}

	return_value =  bh6173_write_reg(bh6173, pdsel, BH6173GUL_Register_PDSEL);
	
	return return_value;
}


u8 powerdown_select(u8 reg_ldo, u8 state)
{
	struct bh6173_data *bh6173 = client_6173data_p;
	u8 return_value = 0;
	u8 pdcnt;

	pdcnt = bh6173_read_reg(bh6173, BH6173GUL_Register_PDCNT);

	if (BH6173GUL_POWERDOWN_HiZ == state) {
		if (reg_ldo&BH6173GUL_POWERDOWN_REG)
			pdcnt &= 0xFE;
		if (reg_ldo&BH6173GUL_POWERDOWN_LDO1)
			pdcnt &= 0xFD;
		if (reg_ldo&BH6173GUL_POWERDOWN_LDO2)
			pdcnt &= 0xFB;
		if (reg_ldo&BH6173GUL_POWERDOWN_LDO3)
			pdcnt &= 0xF7;	
	}
	else if (BH6173GUL_POWERDOWN_Discharge == state) {
		if (reg_ldo&BH6173GUL_POWERDOWN_REG)
			pdcnt |= 0x01;
		if (reg_ldo&BH6173GUL_POWERDOWN_LDO1)
			pdcnt |= 0x02;
		if (reg_ldo&BH6173GUL_POWERDOWN_LDO2)
			pdcnt |= 0x04;
		if (reg_ldo&BH6173GUL_POWERDOWN_LDO3)
			pdcnt |= 0x08;	
	}

	return_value =  bh6173_write_reg(bh6173, pdcnt, BH6173GUL_Register_PDCNT);

	return return_value;
}


static int __devinit bh6173_pmic_probe(struct i2c_client *client,
				const struct i2c_device_id *i2c_id)
{
	struct regulator_dev **rdev;
	struct bh6173_platform_data *pdata = client->dev.platform_data;
	struct bh6173_data *bh6173;
	int i = 0, id, ret;	
	int gpio_value=0;
	u8 temp;

	DBG("func =%s :: Start!!!\n",__func__);

	if (!pdata)
		return -EINVAL;

	bh6173 = kzalloc(sizeof(struct bh6173_data), GFP_KERNEL);
	if (!bh6173)
		return -ENOMEM;

	bh6173->rdev = kzalloc(sizeof(struct regulator_dev *) * (pdata->num_regulators + 1), GFP_KERNEL);
	if (!bh6173->rdev) {
		kfree(bh6173);
		return -ENOMEM;
	}

	bh6173->client = client;
	bh6173->dev = &client->dev;
	mutex_init(&bh6173->mutex);

	bh6173_client=client;

	bh6173->num_regulators = pdata->num_regulators;
	for (i = 0; i < pdata->num_regulators; i++) {

		DBG("regulator_register called for ldo=%d\n",pdata->regulators[i].id);
		id = pdata->regulators[i].id - BH6173_LDO1;

		bh6173->rdev[i] = regulator_register(&regulators_6173[id],
			bh6173->dev, pdata->regulators[i].initdata, bh6173);

		ret = IS_ERR(bh6173->rdev[i]);
		if (ret)
			dev_err(bh6173->dev, "regulator init failed\n");
	}

	rdev = bh6173->rdev;
    i2c_set_clientdata(client, bh6173);
    client_6173data_p = bh6173; // store 6173 client data to be used later

//We should read bh6173_register for Setting bh6173_cache_regs buffer
	bh6173_i2c_read(bh6173_client, BH6173GUL_Register_REGCNT,&temp);
	bh6173_i2c_read(bh6173_client, BH6173GUL_Register_SWADJ,&temp);
	bh6173_i2c_read(bh6173_client, BH6173GUL_Register_LDOADJ1,&temp);
	bh6173_i2c_read(bh6173_client, BH6173GUL_Register_LDOADJ2,&temp);
	bh6173_i2c_read(bh6173_client, BH6173GUL_Register_PDSEL,&temp);
	bh6173_i2c_read(bh6173_client, BH6173GUL_Register_PDCNT,&temp);
	bh6173_i2c_read(bh6173_client, BH6173GUL_Register_SWREGCNT,&temp);
	
	
//[NAGSM_Android_HDLNC_SDcard_Seo jaewoong_20101203 : edit i2c_read()
	/*Power On Sequence Start*/

	bh6173_write_reg(bh6173, BH6173GUL_SWADJ_180, BH6173GUL_Register_SWADJ);
	temp = bh6173_read_reg(bh6173, BH6173GUL_Register_SWADJ);


	//bh6173_read_reg(bh6173_client, BH6173GUL_Register_LDOADJ1,&temp);
	temp &= 0xF0;
	temp |= BH6173GUL_LDO1_280;

	bh6173_write_reg(bh6173, temp, BH6173GUL_Register_LDOADJ1);
	temp = bh6173_read_reg(bh6173, BH6173GUL_Register_LDOADJ1);

	//bh6173_read_reg(bh6173_client, BH6173GUL_Register_LDOADJ1,&temp);
	temp &= 0x0F;
	temp |= BH6173GUL_LDO2_180;

	bh6173_write_reg(bh6173, temp, BH6173GUL_Register_LDOADJ1);


	temp = bh6173_read_reg(bh6173, BH6173GUL_Register_LDOADJ2);
	temp = BH6173GUL_LDO3_280;
	bh6173_write_reg(bh6173, temp, BH6173GUL_Register_LDOADJ2);


	pulldown_select(BH6173GUL_PULLDOWN_ALL, BH6173GUL_PULLDOWN_1K);

	powerdown_select(BH6173GUL_POWERDOWN_ALL, BH6173GUL_POWERDOWN_Discharge);

	
	bh6173_write_reg(bh6173, BH6173GUL_REG_SW_PWMONLY, BH6173GUL_Register_SWREGCNT);
	
//	bh6173_write_reg(bh6173, BH6173GUL_REGCNT_ALL, BH6173GUL_Register_REGCNT);
	
   

	/*Power On Sequence End*/
	

	//NAGSM_Android_SEL_Kernel_Aakash_20101021
//[NAGSM_Android_HDLNC_SeoJW_20101230 : edit ISP_RESET GPIO
	if(HWREV >= 0x06)
	{
	   	ret = gpio_request(S5PV210_MP04(1), "MP04(1)");
		if (ret) {
			printk(KERN_ERR "faile to request gpio(GPIO_ISP_RESET)\n");
			return ret;
		}
		gpio_direction_output(S5PV210_MP04(1), 0);
		gpio_free(S5PV210_MP04(1));
   	}
   	else
   	{
   		ret = gpio_request(GPIO_ISP_RESET, "MP01");
      	if (ret) {
         	printk(KERN_ERR "faile to request gpio(GPIO_ISP_RESET)\n");
         	return ret;
      	}
   		gpio_direction_output(GPIO_ISP_RESET, 0);
   		gpio_free(GPIO_ISP_RESET);
	}
	mdelay(2);	
//[NAGSM_Android_HDLNC_SeoJW_20101230 : edit ISP_RESET GPIO
	//Default Settings

	REG_power_onoff(0);	//Default Turn off SWREG
	//Default Turn off LDO1~LDO3
	bh6173_ldo_disable_direct(1);
	bh6173_ldo_disable_direct(2);
	bh6173_ldo_disable_direct(3);
	//Default Turn off LDO1~LDO3
	//NAGSM_Android_SEL_Kernel_Aakash_20101021



	DBG("func =%s :: End!!!\n",__func__);
 	return 0;
}


static int __devexit bh6173_pmic_remove(struct i2c_client *client)
{
	struct bh6173_data *bh6173 = i2c_get_clientdata(client);
	struct regulator_dev **rdev = bh6173->rdev;
	int i;

	for (i = 0; i <= bh6173->num_regulators; i++)
		if (rdev[i])
	regulator_unregister(rdev[i]);
	kfree(bh6173->rdev);
	kfree(bh6173);
	i2c_set_clientdata(client, NULL);
	client_6173data_p = NULL;

	return 0;
}

static const struct i2c_device_id bh6173_ids[] = {
	{ "bh6173", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, bh6173_ids);

static struct i2c_driver bh6173_pmic_driver = {
	.probe		= bh6173_pmic_probe,
	.remove		= __devexit_p(bh6173_pmic_remove),
	.driver		= {
	.name	= "bh6173",
	},
	.id_table	= bh6173_ids,
};

static int __init bh6173_pmic_init(void)
{
	return i2c_add_driver(&bh6173_pmic_driver);
}
//subsys_initcall(bh6173_pmic_init);
late_initcall(bh6173_pmic_init);			
static void __exit bh6173_pmic_exit(void)
{
	i2c_del_driver(&bh6173_pmic_driver);
};
module_exit(bh6173_pmic_exit);

MODULE_DESCRIPTION("BH6173GUL voltage regulator driver");
MODULE_AUTHOR("Aakash Manik");
MODULE_LICENSE("GPL");

