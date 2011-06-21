/*
 *  linux/arch/arm/plat-s5pc11x/max8998_consumer.c
 *
 *  CPU frequency scaling for S5PC110
 *
 *  Copyright (C) 2009 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <plat/gpio-cfg.h>
#include <asm/gpio.h>
#include <mach/cpu-freq-v210.h>
#include <linux/platform_device.h>
#include <linux/regulator/max8998.h>
#include <linux/regulator/consumer.h>

#include <mach/max8998_function.h>


#define DBG(fmt...)
//#define DBG printk

#define PMIC_ARM		0
#define PMIC_INT		1
#define PMIC_BOTH		2

#if defined(CONFIG_S5PC110_DEMPSEY_BOARD) 
#define USE_1DOT2GH 1//Rajucm
#endif

#if defined (USE_1DOT2GH) //Rajucm
#ifdef CONFIG_CPU_FREQ
unsigned int S5PC11X_VOLTAGE_TAB;
#endif
#endif

#if defined (USE_1DOT2GH) //Rajucm
#undef DECREASE_DVFS_DELAY
#else
#define DECREASE_DVFS_DELAY
#endif

#ifdef DECREASE_DVFS_DELAY
#define PMIC_SET_MASK   (0x38) //(0x7 << 3)
#define PMIC_SET1_BIT   (0x8)  //(0x1 << 3)
#define PMIC_SET2_BIT   (0x10) //(0x1 << 4)
#define PMIC_SET3_BIT   (0x20) //(0x1 << 5)
#else
#define PMIC_ARM_MASK		(0x3 << 3)
#define PMIC_SET1_HIGH		(0x1 << 3)
#define PMIC_SET2_HIGH		(0x1 << 4)
#endif

#ifndef CONFIG_CPU_FREQ
unsigned int S5PC11X_FREQ_TAB = 1;
#endif

#define RAMP_RATE 10 // 10mv/usec
unsigned int step_curr;

#if defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
extern unsigned int VPLUSVER ;
#endif

enum PMIC_VOLTAGE {
	VOUT_0_75, 
	VOUT_0_80, 
	VOUT_0_85, 
	VOUT_0_90, 
	VOUT_0_95, 
	VOUT_1_00, 
	VOUT_1_05, 
	VOUT_1_10, 
	VOUT_1_15, 
	VOUT_1_20, 
	VOUT_1_25, 
	VOUT_1_30, 
	VOUT_1_35, 
	VOUT_1_40, 
	VOUT_1_45, 
	VOUT_1_50 	
};


/* frequency voltage matching table */
static const unsigned int frequency_match_1GHZ[][4] = {
/* frequency, Mathced VDD ARM voltage , Matched VDD INT*/
#if 1
#if defined (USE_1DOT2GH) //Rajucm
        {1000000, 1250, 1100, 0},
        {800000, 1200, 1100, 1},
        {400000, 1050, 1100, 2},
        {200000, 950, 1100, 4},
        {100000, 950, 1000, 5},
#else
        {1000000, 1275, 1100, 0},
        {800000, 1200, 1100, 1},
        {400000, 1050, 1100, 2},
        {200000, 950, 1100, 4},
        {100000, 950, 1000, 5},
#endif
#else //just for dvs test
        {1000000, 1250, 1100, 0},
        {800000, 1250, 1100, 1},
        {400000, 1250, 1100, 2},
        {200000, 1250, 1100, 4},
        {100000, 950, 1000, 5},
#endif
};

static const unsigned int frequency_match_800MHZ[][4] = {
/* frequency, Mathced VDD ARM voltage , Matched VDD INT*/
        {800000, 1200, 1100, 0},
        {400000, 1050, 1100, 1},
        {200000, 950, 1100, 3},
        {100000, 950, 1000, 4},
};
#if !defined (USE_1DOT2GH)//Rajucm
const unsigned int (*frequency_match[2])[4] = {
        frequency_match_1GHZ,
        frequency_match_800MHZ,
};
#endif

#if 0
/*  voltage table */
static const unsigned int voltage_table[16] = {
	750, 800, 850, 900, 950, 1000, 1050,
	1100, 1150, 1200, 1250, 1300, 1350,
	1400, 1450, 1500
};
#endif

#if defined (USE_1DOT2GH) //Rajucm
static const unsigned int frequency_match_1DOT2GHZ_ASV0[][4] = {
/* frequency, Mathced VDD ARM voltage , Matched VDD INT*/
        {1200000, 1325, 1175, 0},
        {1000000, 1325, 1150, 1},
        {800000, 1275, 1150, 2},
        {400000, 1125, 1150, 3},
        {200000, 1025, 1150, 4},
        {100000, 1025, 1050, 5},
};

static const unsigned int frequency_match_1DOT2GHZ_ASV1[][4] = {
/* frequency, Mathced VDD ARM voltage , Matched VDD INT*/
        {1200000, 13000, 1125, 0},
        {1000000, 12000, 1025, 1},
        {800000, 1175, 1025, 2},
        {400000, 1025, 1025, 3},
        {200000, 1025, 1025, 4},
        {100000, 1025, 1025, 5},
};

static const unsigned int frequency_match_1DOT2GHZ_ASV2[][4] = {
/* frequency, Mathced VDD ARM voltage , Matched VDD INT*/
        {1200000, 1250, 1075, 0},
        {1000000, 1175, 1025, 1},
        {800000, 1125, 1025, 2},
        {400000, 1025, 1025, 3},
        {200000, 1025, 1025, 4},
        {100000, 1025, 1025, 5},
};

const unsigned int (*frequency_match[4])[4] = {
        frequency_match_1GHZ,
        frequency_match_1DOT2GHZ_ASV0,
        frequency_match_1DOT2GHZ_ASV1,
        frequency_match_1DOT2GHZ_ASV2,
};
#endif //USE_1DOT2GH

extern unsigned int S5PC11X_FREQ_TAB;
//extern const unsigned int (*frequency_match[2])[4];

static struct regulator *Reg_Arm = NULL, *Reg_Int = NULL;

static unsigned int s_arm_voltage=0, s_int_voltage=0;

#ifndef DECREASE_DVFS_DELAY
/*only 4 Arm voltages and 2 internal voltages possible*/
static const unsigned int dvs_volt_table_800MHZ[][3] = {
        {L0, DVSARM2, DVSINT1},
        {L1, DVSARM3, DVSINT1},
 //266       {L2, DVSARM3, DVSINT1},
        {L2, DVSARM4, DVSINT1},
        {L3, DVSARM4, DVSINT2},
//        {L4, DVSARM4, DVSINT2},
//        {L5, DVSARM4, DVSINT2},
};
#if !defined (USE_1DOT2GH) //Rajucm
static const unsigned int dvs_volt_table_1GHZ[][3] = {
        {L0, DVSARM1, DVSINT1},//DVSINT0
        {L1, DVSARM2, DVSINT1},
        {L2, DVSARM3, DVSINT1},
 //266       {L3, DVSARM3, DVSINT1},
        {L3, DVSARM4, DVSINT1},
        {L4, DVSARM4, DVSINT2},
//        {L5, DVSARM4, DVSINT2},
//        {L6, DVSARM4, DVSINT2},
};


const unsigned int (*dvs_volt_table[2])[3] = {
        dvs_volt_table_1GHZ,
        dvs_volt_table_800MHZ,
};
#endif
static const unsigned int dvs_arm_voltage_set[][2] = {
	{DVSARM1, 1275},
	{DVSARM2, 1200},
	{DVSARM3, 1050},
	{DVSARM4, 950},
	{DVSINT1, 1100},
	{DVSINT2, 1000},
};
#endif

#if defined (USE_1DOT2GH) //Rajucm
/*only 4 Arm voltages and 2 internal voltages possible*/
static const unsigned int dvs_volt_table_1DOT2GHZ_ASV0[][3] = {
        {L0, DVSARM4, DVSINT2},// use 100mhz gpio con value for arm, int
        {L1, DVSARM1, DVSINT1},
        {L2, DVSARM2, DVSINT1},
        {L3, DVSARM3, DVSINT1},
        {L4, DVSARM4, DVSINT1},
        {L5, DVSARM4, DVSINT2},

};


static const unsigned int dvs_volt_table_1DOT2GHZ_ASV1[][3] = {
        {L0, DVSARM1, DVSINT1},
        {L1, DVSARM2, DVSINT2},
        {L2, DVSARM3, DVSINT2},
        {L3, DVSARM4, DVSINT2},
        {L4, DVSARM4, DVSINT2},
        {L5, DVSARM4, DVSINT2},

};

static const unsigned int dvs_volt_table_1DOT2GHZ_ASV2[][3] = {
        {L0, DVSARM1, DVSINT1},
        {L1, DVSARM2, DVSINT2},
        {L2, DVSARM3, DVSINT2},
        {L3, DVSARM4, DVSINT2},
        {L4, DVSARM4, DVSINT2},
        {L5, DVSARM4, DVSINT2},

};

static const unsigned int dvs_volt_table_1GHZ[][3] = {
        {L0, DVSARM1, DVSINT1},
        {L1, DVSARM2, DVSINT1},
        {L2, DVSARM3, DVSINT1},
        {L3, DVSARM4, DVSINT1},
        {L4, DVSARM4, DVSINT2},
};


const unsigned int (*dvs_volt_table[4])[3] = {
        dvs_volt_table_1GHZ,
        dvs_volt_table_1DOT2GHZ_ASV0,
        dvs_volt_table_1DOT2GHZ_ASV1,
        dvs_volt_table_1DOT2GHZ_ASV2,
};


static unsigned int dvs_arm_voltage_set_1DOT2GHZ_ASV0[][2] = {
	{DVSARM1, 1250},
	{DVSARM2, 1200},
	{DVSARM3, 1050},
	{DVSARM4, 1300}, //{DVSARM4, 950},
	{DVSINT1, 1100},
	{DVSINT2, 1125}, //{DVSINT2, 1000},
};

static unsigned int dvs_arm_voltage_set_1DOT2GHZ_ASV1[][2] = {
	{DVSARM1, 1225},
	{DVSARM2, 1125},
	{DVSARM3, 1100},
	{DVSARM4, 950},
	{DVSINT1, 1075},
	{DVSINT2, 975},
};

static unsigned int dvs_arm_voltage_set_1DOT2GHZ_ASV2[][2] = {
	{DVSARM1, 1175},
	{DVSARM2, 1100},
	{DVSARM3, 1050},
	{DVSARM4, 950},
	{DVSINT1, 1025},
	{DVSINT2, 975},
};

static unsigned int dvs_arm_voltage_set_1GHZ[][2] = {
	{DVSARM1, 1250},
	{DVSARM2, 1200},
	{DVSARM3, 1050},
	{DVSARM4, 950},
	{DVSINT1, 1100},
	{DVSINT2, 1000},
};


unsigned int (*dvs_arm_voltage_set_table[4])[2] = {
        dvs_arm_voltage_set_1GHZ,
	dvs_arm_voltage_set_1DOT2GHZ_ASV0,
	dvs_arm_voltage_set_1DOT2GHZ_ASV1,
	dvs_arm_voltage_set_1DOT2GHZ_ASV2,
};

int saved_1dot2G_dvs_stat=0;
void dvs_set_for_1dot2Ghz (int onoff) {
	unsigned int (*dvs_arm_voltage_set)[2] = dvs_arm_voltage_set_table[S5PC11X_VOLTAGE_TAB];
	const unsigned int (*freq_match_set)[4] = frequency_match[S5PC11X_VOLTAGE_TAB];

	//printk("KSOO dvs_set_for_1dot2Ghz %d\n", onoff);
	if (onoff == saved_1dot2G_dvs_stat) return;
	if (S5PC11X_VOLTAGE_TAB > 1) return;
	if (onoff) {
			max8998_set_dvsarm_direct(DVSARM4, freq_match_set[0][1]);
			dvs_arm_voltage_set[DVSARM4 - DVSARM1][1] = freq_match_set[0][1];

			max8998_set_dvsint_direct(DVSINT2, freq_match_set[0][2]);
			dvs_arm_voltage_set[DVSINT2 - DVSARM1][1] = freq_match_set[0][2];
	} else {
			max8998_set_dvsarm_direct(DVSARM4, freq_match_set[5][1]);
			dvs_arm_voltage_set[DVSARM4 - DVSARM1][1] = freq_match_set[5][1];

			max8998_set_dvsint_direct(DVSINT2, freq_match_set[5][2]);
			dvs_arm_voltage_set[DVSINT2 - DVSARM1][1] = freq_match_set[5][2];
		}
	saved_1dot2G_dvs_stat = onoff;
	// need a udelay like 
	// int set_voltage_dvs(enum perf_level p_lv)
	// but, we may skip udelay, because this level will be used after some process execution.
	return;
}
#endif //USE_1DOT2GH

static int set_max8998(unsigned int pwr, enum perf_level p_lv)
{
	int voltage;
	int pmic_val;
	int ret = 0;
#if defined (USE_1DOT2GH) //Rajucm
	const unsigned int (*frequency_match_tab)[4] = frequency_match[S5PC11X_VOLTAGE_TAB];	
#else
	const unsigned int (*frequency_match_tab)[4] = frequency_match[S5PC11X_FREQ_TAB];	
#endif
	DBG("%s : p_lv = %d : pwr = %d \n", __FUNCTION__, p_lv,pwr);

	if(pwr == PMIC_ARM) {

#if defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
//		if (VPLUSVER) T959P has no voltage drop in dvfs
		voltage = frequency_match_tab[p_lv][pwr + 1]+50;
//		else
//			voltage = frequency_match_tab[p_lv][pwr + 1]+125;
#else
		voltage = frequency_match_tab[p_lv][pwr + 1];
#endif

		if(voltage == s_arm_voltage)
			return ret;

		pmic_val = voltage * 1000;
		
		DBG("regulator_set_voltage =%d\n",voltage);
		/*set Arm voltage*/
		ret = regulator_set_voltage(Reg_Arm,pmic_val,pmic_val);
	        if(ret != 0)
        	{
			printk(KERN_ERR "%s: Cannot set VCC_ARM\n", __func__);
			return -EINVAL;
        	}
		/*put delay according to ramp rate*/
		//udelay(20);
		if(voltage > s_arm_voltage)
			udelay((voltage - s_arm_voltage)/RAMP_RATE);
		else
			udelay((s_arm_voltage - voltage)/RAMP_RATE);

		s_arm_voltage = voltage;	
		
	} else if(pwr == PMIC_INT) {

#if defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
//		if (VPLUSVER) T959P has no voltage drop in dvfs
		voltage = frequency_match_tab[p_lv][pwr + 1];
//		else
//			voltage = frequency_match_tab[p_lv][pwr + 1]+50;
#else	
		voltage = frequency_match_tab[p_lv][pwr + 1];
#endif		
		if(voltage == s_int_voltage)
                        return ret;

		pmic_val = voltage * 1000;

		DBG("regulator_set_voltage = %d\n",voltage);
		/*set Arm voltage*/
		ret = regulator_set_voltage(Reg_Int, pmic_val, pmic_val);
	        if(ret != 0)
        	{
			printk(KERN_ERR "%s: Cannot set VCC_INT\n", __func__);
			return -EINVAL;
        	}

		/*put delay according to ramp rate*/
		//udelay(20);
		if(voltage > s_int_voltage)
			udelay((voltage - s_int_voltage)/RAMP_RATE);
		else
			udelay((s_int_voltage - voltage)/RAMP_RATE);

                s_int_voltage = voltage;

	}else {
		printk("[error]: set_power, check mode [pwr] value\n");
		return -EINVAL;
	}
	return 0;
}

void set_pmic_gpio(void)
{
	/*set set1, set2, set3 of max8998 driver as 0*/
	/* set GPH0(3), GPH0(4) & GPH0(5) as low*/
	s3c_gpio_cfgpin(GPIO_BUCK_1_EN_A, S3C_GPIO_OUTPUT);
	s3c_gpio_setpin(GPIO_BUCK_1_EN_A, 0);
	s3c_gpio_setpull(GPIO_BUCK_1_EN_A, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_BUCK_1_EN_B, S3C_GPIO_OUTPUT);
	s3c_gpio_setpin(GPIO_BUCK_1_EN_B, 0);
	s3c_gpio_setpull(GPIO_BUCK_1_EN_B, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_BUCK_2_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpin(GPIO_BUCK_2_EN, 0);
	s3c_gpio_setpull(GPIO_BUCK_2_EN, S3C_GPIO_PULL_NONE);

	s3c_gpio_setpull(GPIO_AP_PS_HOLD, S3C_GPIO_PULL_NONE);
}
EXPORT_SYMBOL_GPL(set_pmic_gpio);


int set_voltage(enum perf_level p_lv)
{
	DBG("%s : p_lv = %d\n", __FUNCTION__, p_lv);
	if(step_curr != p_lv) 
	{
		/*Commenting gpio initialisation*/
		//set_pmic_gpio();

		set_max8998(PMIC_ARM, p_lv);
		set_max8998(PMIC_INT, p_lv);
		step_curr = p_lv;
	}
	return 0;
}

EXPORT_SYMBOL(set_voltage);

#ifdef DECREASE_DVFS_DELAY
int set_gpio_dvs(enum perf_level p_lv)
{
	switch(p_lv)
    {
        case L0:
            writel(((readl(S5PV210_GPH0DAT) & ~PMIC_SET_MASK)                                                ), S5PV210_GPH0DAT);
            break;
        case L1:
            writel(((readl(S5PV210_GPH0DAT) & ~PMIC_SET_MASK) | PMIC_SET1_BIT                                ), S5PV210_GPH0DAT);
            break;
        case L2:
            writel(((readl(S5PV210_GPH0DAT) & ~PMIC_SET_MASK)                 | PMIC_SET2_BIT                ), S5PV210_GPH0DAT);
            break;
        case L3:
            writel(((readl(S5PV210_GPH0DAT) & ~PMIC_SET_MASK) | PMIC_SET1_BIT | PMIC_SET2_BIT                ), S5PV210_GPH0DAT);
            break;
        case L4:
            writel(((readl(S5PV210_GPH0DAT) & ~PMIC_SET_MASK) | PMIC_SET1_BIT | PMIC_SET2_BIT | PMIC_SET3_BIT), S5PV210_GPH0DAT);
            break;
        default:
            pr_err("[PWR] %s : Invalid parameters (%d)\n", __func__, p_lv);
            return -EINVAL;
	}

	DBG("[PWR] %s : GPH0DAT=%x, GPH0CON=%x, GPH0PUD=%x\n",__func__, readl(S5PV210_GPH0DAT), readl(S5PV210_GPH0CON), readl(S5PV210_GPH0PUD));
	
	return 0;
}
#else
int set_gpio_dvs(int armSet)
{
	unsigned int curPmicArm = readl(S5PV210_GPH0DAT);
	DBG("set dvs with %d\n", armSet);
	switch(armSet) {
	case DVSARM1:
		curPmicArm = curPmicArm & ~(PMIC_ARM_MASK); //set GPH0[3],GPH0[4] low
		writel(curPmicArm,S5PV210_GPH0DAT);
		//gpio_set_value(GPIO_BUCK_1_EN_A, 0);
		//gpio_set_value(GPIO_BUCK_1_EN_B, 0);
		break;
	case DVSARM2:
		curPmicArm = (curPmicArm & ~(PMIC_ARM_MASK)) | PMIC_SET1_HIGH; //set GPH0[3] high, GPH0[4] low
		writel(curPmicArm,S5PV210_GPH0DAT);
		//gpio_set_value(GPIO_BUCK_1_EN_B, 0);
		//gpio_set_value(GPIO_BUCK_1_EN_A, 1);
		break;
	case DVSARM3:
		curPmicArm = (curPmicArm & ~(PMIC_ARM_MASK)) | PMIC_SET2_HIGH; //set GPH0[3] low, GPH0[4] high
		writel(curPmicArm,S5PV210_GPH0DAT);		
		//gpio_set_value(GPIO_BUCK_1_EN_A, 0);
		//gpio_set_value(GPIO_BUCK_1_EN_B, 1);
		break;
	case DVSARM4:
		curPmicArm = (curPmicArm & ~(PMIC_ARM_MASK)) | PMIC_SET1_HIGH | PMIC_SET2_HIGH; //set GPH0[3],GPH0[4] high
		writel(curPmicArm,S5PV210_GPH0DAT);	
		//gpio_set_value(GPIO_BUCK_1_EN_A, 1);
		//gpio_set_value(GPIO_BUCK_1_EN_B, 1);
		break;
	case DVSINT1:
		gpio_set_value(GPIO_BUCK_2_EN, 0);
		break;
	case DVSINT2:
		gpio_set_value(GPIO_BUCK_2_EN, 1);
		break;
	default:
		printk("Invalid parameters to %s\n",__FUNCTION__);
		return -EINVAL;
	}

	DBG("S5PV210_GPH0CON=%x,S5PV210_GPH0DAT=%x,S5PV210_GPH0PUD=%x\n",readl(S5PV210_GPH0CON),readl(S5PV210_GPH0DAT),readl(S5PV210_GPH0PUD));

	return 0;
}
#endif

int dvs_initilized=0;
int set_voltage_dvs(enum perf_level p_lv)
{
#ifdef DECREASE_DVFS_DELAY
	unsigned int delay, arm_delay, int_delay;
	const unsigned int (*frequency_match_tab)[4] = frequency_match[S5PC11X_FREQ_TAB];

	if((step_curr == p_lv) && dvs_initilized)
		return 0;

	if (p_lv < step_curr) // clk up
	{
		arm_delay = frequency_match_tab[p_lv][1] - frequency_match_tab[step_curr][1];
		int_delay = frequency_match_tab[p_lv][2] - frequency_match_tab[step_curr][2];
	}
	else // clk down
	{
		arm_delay = frequency_match_tab[step_curr][1] - frequency_match_tab[p_lv][1];
		int_delay = frequency_match_tab[step_curr][2] - frequency_match_tab[p_lv][2];
	}
	delay = ((arm_delay > int_delay) ? arm_delay : int_delay) / RAMP_RATE;

	set_gpio_dvs(p_lv);
	udelay(delay);

	DBG("[PWR] %s : level (%d -> %d), delay (%u)\n", __func__, step_curr, p_lv, delay);

	step_curr = p_lv;

	return 0;
#else
	unsigned int new_voltage, old_voltage; 
#if defined (USE_1DOT2GH) //Rajucm
	const unsigned int (*dvs_volt_table_ptr)[3] = dvs_volt_table[S5PC11X_VOLTAGE_TAB];
	unsigned int (*dvs_arm_voltage_set)[2] = dvs_arm_voltage_set_table[S5PC11X_VOLTAGE_TAB];
#else
	const unsigned int (*dvs_volt_table_ptr)[3] = dvs_volt_table[S5PC11X_FREQ_TAB];
#endif

	DBG("%s : p_lv = %d\n", __FUNCTION__, p_lv);
	if((step_curr == p_lv) && dvs_initilized)
		return 0;

	/*For arm vdd*/
	new_voltage = dvs_arm_voltage_set[dvs_volt_table_ptr[p_lv][1]-DVSARM1][1];
	old_voltage = dvs_arm_voltage_set[dvs_volt_table_ptr[step_curr][1]-DVSARM1][1];
	DBG("%s ARM VDD: new voltage = %u old voltage = %u\n",__func__,new_voltage,old_voltage);
	if(new_voltage != old_voltage){
		set_gpio_dvs(dvs_volt_table_ptr[p_lv][1]);
		if(new_voltage > old_voltage)
			udelay((new_voltage-old_voltage)/RAMP_RATE);
		else
			udelay((old_voltage-new_voltage)/RAMP_RATE);
	}
	/*For arm int*/
	new_voltage = dvs_arm_voltage_set[dvs_volt_table_ptr[p_lv][2]-DVSARM1][1];
	old_voltage = dvs_arm_voltage_set[dvs_volt_table_ptr[step_curr][2]-DVSARM1][1];
	DBG("%s ARM INT: new voltage = %u old voltage = %u\n",__func__,new_voltage,old_voltage);
	if(new_voltage != old_voltage){
		set_gpio_dvs(dvs_volt_table_ptr[p_lv][2]);
		if(new_voltage > old_voltage)
			udelay((new_voltage-old_voltage)/RAMP_RATE);
		else
			udelay((old_voltage-new_voltage)/RAMP_RATE);
	}

	step_curr = p_lv;
	//if (!dvs_initilized) dvs_initilized=1;
	//msleep(10);

	return 0;
#endif
}
EXPORT_SYMBOL(set_voltage_dvs);

void max8998_init(void)
{
	if(S5PC11X_FREQ_TAB) // for 1.2GHZ table
	{
		step_curr = L0;
		set_voltage_dvs(L2); //switch to 800MHZ
	}
	else // for 1GHZ table
	{
		step_curr = L0;
		set_voltage_dvs(L1); //switch to 800MHZ
	}
	if (!dvs_initilized) dvs_initilized=1;
}

EXPORT_SYMBOL(max8998_init);

#if defined (USE_1DOT2GH) //Rajucm
static void max8998_init_dvs_registers()
{
	unsigned int (*dvs_arm_voltage_set)[2] = dvs_arm_voltage_set_table[S5PC11X_VOLTAGE_TAB];
	//printk(" $#KSOO$# Function=%s, voltage_tabl = %d, ARM1=%d \n",__func__, S5PC11X_VOLTAGE_TAB, dvs_arm_voltage_set[0][1]);

	/*initialise the dvs registers*/
	max8998_set_dvsarm_direct(DVSARM1, dvs_arm_voltage_set[0][1]);
	max8998_set_dvsarm_direct(DVSARM2, dvs_arm_voltage_set[1][1]);
	max8998_set_dvsarm_direct(DVSARM3, dvs_arm_voltage_set[2][1]);
	max8998_set_dvsarm_direct(DVSARM4, dvs_arm_voltage_set[3][1]);
	max8998_set_dvsint_direct(DVSINT1, dvs_arm_voltage_set[4][1]);
	max8998_set_dvsint_direct(DVSINT2, dvs_arm_voltage_set[5][1]);

}
#endif

static int max8998_consumer_probe(struct platform_device *pdev)
{
        int ret = 0;
#ifdef DECREASE_DVFS_DELAY
	const unsigned int (*frequency_match_tab)[4] = frequency_match[S5PC11X_FREQ_TAB];
#endif

#if defined (USE_1DOT2GH) //Rajucm
	//unsigned int (*dvs_arm_voltage_set)[2] = dvs_arm_voltage_set_table[S5PC11X_VOLTAGE_TAB];
	int regval=0;

	if (S5PC11X_FREQ_TAB) {
		regval = __raw_readl(S5P_VA_CHIPID + 0x10) & 0x1f;
		if ((regval >= 1) && (regval <= 10))
			S5PC11X_VOLTAGE_TAB = 1;
		else if ((regval >= 11) && (regval <= 20))
			S5PC11X_VOLTAGE_TAB = 2;
		else if ((regval >= 21) && (regval <= 30))
			S5PC11X_VOLTAGE_TAB = 3;
		else
			S5PC11X_VOLTAGE_TAB = 1;
	} else {
		S5PC11X_VOLTAGE_TAB = 0;
	}
printk ("$#KSOO$# freq_tabl=%d, voltage tab=%d, avs val=%d",S5PC11X_FREQ_TAB, S5PC11X_VOLTAGE_TAB, regval);	
#endif
	DBG(" Function=%s \n",__func__);
        Reg_Arm = regulator_get(NULL, "vddarm");
        if (IS_ERR(Reg_Arm)) {
                printk(KERN_ERR "%s: cant get VCC_ARM\n", __func__);
                return PTR_ERR(Reg_Arm);
        }

	Reg_Int = regulator_get(NULL, "vddint");
        if (IS_ERR(Reg_Int)) {
                printk(KERN_ERR "%s: cant get VCC_INT\n", __func__);
                return PTR_ERR(Reg_Int);
        }


	/*initialise the dvs registers*/
#if defined (USE_1DOT2GH) //Rajucm
	max8998_init_dvs_registers();
#else
#ifdef DECREASE_DVFS_DELAY
#if defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
//		if (VPLUSVER) T959P has no voltage drop in dvfs
//		{
			max8998_set_dvsarm_direct(DVSARM1, frequency_match_tab[0][1]+50);
			max8998_set_dvsarm_direct(DVSARM2, frequency_match_tab[1][1]+50);
			max8998_set_dvsarm_direct(DVSARM3, frequency_match_tab[2][1]+50);
			max8998_set_dvsarm_direct(DVSARM4, frequency_match_tab[3][1]+50);
			max8998_set_dvsint_direct(DVSINT1, frequency_match_tab[0][2]);
			max8998_set_dvsint_direct(DVSINT2, frequency_match_tab[4][2]);
//		}
/*		else
		{
			max8998_set_dvsarm_direct(DVSARM1, frequency_match_tab[0][1]+125);
			max8998_set_dvsarm_direct(DVSARM2, frequency_match_tab[1][1]+125);
			max8998_set_dvsarm_direct(DVSARM3, frequency_match_tab[2][1]+125);
			max8998_set_dvsarm_direct(DVSARM4, frequency_match_tab[3][1]+125);
			max8998_set_dvsint_direct(DVSINT1, frequency_match_tab[0][2]+50);
			max8998_set_dvsint_direct(DVSINT2, frequency_match_tab[4][2]+50);
		}
*/
#else
	max8998_set_dvsarm_direct(DVSARM1, frequency_match_tab[0][1]);
	max8998_set_dvsarm_direct(DVSARM2, frequency_match_tab[1][1]);
	max8998_set_dvsarm_direct(DVSARM3, frequency_match_tab[2][1]);
	max8998_set_dvsarm_direct(DVSARM4, frequency_match_tab[3][1]);
	max8998_set_dvsint_direct(DVSINT1, frequency_match_tab[0][2]);
	max8998_set_dvsint_direct(DVSINT2, frequency_match_tab[4][2]);
#endif
#else
	max8998_set_dvsarm_direct(DVSARM1, dvs_arm_voltage_set[0][1]);
	max8998_set_dvsarm_direct(DVSARM2, dvs_arm_voltage_set[1][1]);
	max8998_set_dvsarm_direct(DVSARM3, dvs_arm_voltage_set[2][1]);
	max8998_set_dvsarm_direct(DVSARM4, dvs_arm_voltage_set[3][1]);
	max8998_set_dvsint_direct(DVSINT1, dvs_arm_voltage_set[4][1]);
	max8998_set_dvsint_direct(DVSINT2, dvs_arm_voltage_set[5][1]);
#endif
#endif
	/*Initialize the pmic voltage*/
	max8998_init();
	
	return ret;	
}

static int max8998_consumer_remove(struct platform_device *pdev)
{
        int ret = 0;
	DBG(" Function=%s \n",__func__);
	if(Reg_Arm)
		regulator_put(Reg_Arm);

	if(Reg_Int)
		regulator_put(Reg_Int);
	
	Reg_Arm = NULL;
	Reg_Int = NULL;
	return ret;

}

unsigned int universal_sdhci2_detect_ext_cd(void);
extern short int get_headset_status(void);

//defined sec_jack.h
enum
{
	SEC_JACK_NO_DEVICE					= 0x0,
	SEC_HEADSET_4_POLE_DEVICE			= 0x01 << 0,	
	SEC_HEADSET_3_POLE_DEVICE			= 0x01 << 1,
//doesn't use below 3 dev.	
	SEC_TTY_DEVICE						= 0x01 << 2,
	SEC_FM_HEADSET						= 0x01 << 3,
	SEC_FM_SPEAKER						= 0x01 << 4,	
	SEC_TVOUT_DEVICE					= 0x01 << 5,
	SEC_UNKNOWN_DEVICE				= 0x01 << 6,	
};
//static int ldo_disable_check(int ldo)
//return true: disable
//return false: do not anything
static int ldo_disable_check(int ldo)
{
	switch(ldo){
		case MAX8998_LDO9:
#if defined(CONFIG_ARIES_NTT) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) // Modify NTTS1
		case MAX8998_LDO6:	//xmoondash :: modem uart
#endif
			return 0;
			break;
#if defined(CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
#if defined(CONFIG_HAWK_VER_B1_REAL_ADDED_FEATURE) //NAGSM_Android_HQ_KERNEL_CLEE_20100928 : Setup Hawk Real Board Rev 0.1 Proximity sensor
		case MAX8998_LDO13:		
			return 0;
			break;
#endif
		case MAX8998_LDO6:		//NAGSM_Android_HQ_KERNEL_CLEE_20100909 : Due to Hand-off fail issue in sleep mode.
			return 0;
			break;
#endif

#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
		case MAX8998_LDO14:
		case MAX8998_LDO15:
			return 0; 
			break;

#endif
#if 0
		case MAX8998_LDO5: //sdcard
			if(universal_sdhci2_detect_ext_cd())
				return 0;
			else
				return 1;
			break;
#endif
		case MAX8998_LDO4:
			if(get_headset_status()==SEC_HEADSET_4_POLE_DEVICE) //fix sendend is low in sleep
				return 0;
			else
				return 1;

			break;	
		default:
			return 1;
	}
}

static int pmic_controling_list;// a global variable to store ldo's status
static int s3c_consumer_suspend(struct platform_device *dev, pm_message_t state)
{
	int i ;
	
	pmic_controling_list = 0;
	
	for (i=MAX8998_LDO3; i<=MAX8998_LDO17;i++) {
			if (ldo_disable_check(i)) {
				if (max8998_ldo_is_enabled_direct(i)) {
					//printk("ksoo++++ ldo%d disabled", i);
					pmic_controling_list |= (0x1 << i);
					max8998_ldo_disable_direct(i);
				}
			}
		}

	//max8998_ldo_disable_direct(MAX8998_DCDC4);
	//max8998_ldo_disable_direct(MAX8998_DCDC3);
	max8998_ldo_disable_direct(MAX8998_DCDC2);
	max8998_ldo_disable_direct(MAX8998_DCDC1);
	return 0;
}

static int s3c_consumer_resume(struct platform_device *dev)
{
	int i, saved_control;
	
	max8998_ldo_enable_direct(MAX8998_DCDC1);
	max8998_ldo_enable_direct(MAX8998_DCDC2);
	//max8998_ldo_enable_direct(MAX8998_DCDC3);
	//max8998_ldo_enable_direct(MAX8998_DCDC4);
	
	// done in BL code.
	saved_control = pmic_controling_list;
	
	for (i=MAX8998_LDO3;i<=MAX8998_LDO17;i++) {
		if ((saved_control  >> i) & 0x1) max8998_ldo_enable_direct(i);
	}

	return 0;
}

static struct platform_driver max8998_consumer_driver = {
	.driver = {
		   .name = "max8998-consumer",
		   .owner = THIS_MODULE,
		   },
	.probe = max8998_consumer_probe,
	.remove = max8998_consumer_remove,
        .suspend = s3c_consumer_suspend, 
        .resume = s3c_consumer_resume,

};

static int max8998_consumer_init(void)
{
	return platform_driver_register(&max8998_consumer_driver);
}
late_initcall(max8998_consumer_init);

static void max8998_consumer_exit(void)
{
	platform_driver_unregister(&max8998_consumer_driver);
}
module_exit(max8998_consumer_exit);

MODULE_AUTHOR("Amit Daniel");
MODULE_DESCRIPTION("MAX 8998 consumer driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:max8998-consumer");
