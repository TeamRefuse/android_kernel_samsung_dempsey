/* linux/drivers/media/video/samsung/tv20/s5pv210/tv_power_s5pv210.c
 *
 * power raw ftn  file for Samsung TVOut driver
 *
 * Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#include <mach/map.h>
#include <mach/regs-clock.h>


#include "tv_out_s5pv210.h"

#if defined USE_POWERCON_FUNCTION
#undef USE_POWERCON_FUNCTION
#endif

#ifdef COFIG_TVOUT_RAW_DBG
#define S5P_TVOUT_PM_DEBUG 1
#endif

#ifdef S5P_TVOUT_PM_DEBUG
#define TVPMPRINTK(fmt, args...) \
	printk("\t\t[TVPM] %s: " fmt, __FUNCTION__ , ## args)
#else
#define TVPMPRINTK(fmt, args...)
#endif

// NORMAL_CFG
#define TVPWR_SUBSYSTEM_ACTIVE (1<<4)
#define TVPWR_SUBSYSTEM_LP     (0<<4)

// MTC_STABLE
#define TVPWR_MTC_COUNTER_CLEAR(a) (((~0xf)<<16)&a)
#define TVPWR_MTC_COUNTER_SET(a)   ((0xf&a)<<16)

// BLK_PWR_STAT
#define TVPWR_TV_BLOCK_STATUS(a)    ((0x1<<4)&a)

static unsigned short g_dacPwrOn = 0; // DAC Power

extern int ldo38_control_by_tvout(bool onoff) ;
extern int ldo38_control_by_tvout_test();

void __s5p_tv_power_init_mtc_stable_counter(unsigned int value)
{
	TVPMPRINTK("(%d)\n\r", value);

	writel(TVPWR_MTC_COUNTER_CLEAR((readl(S5P_MTC_STABLE) | TVPWR_MTC_COUNTER_SET(value))),
	       S5P_MTC_STABLE);

	TVPMPRINTK("(0x%08x)\n\r", readl(S5P_MTC_STABLE));
}

void __s5p_tv_powerinitialize_dac_onoff(unsigned short on)
{
	TVPMPRINTK("(%d)\n\r", on);

	g_dacPwrOn = on;

	TVPMPRINTK("(0x%08x)\n\r", g_dacPwrOn);
}

void __s5p_tv_powerset_dac_onoff(unsigned short on)
{
	TVPMPRINTK("(%d)\n\r", on);

	if (on) 
	{
        ldo38_control_by_tvout(1); //ldo8 is used by tvout and usb. so it must not be controlled directly.
		writel(S5P_DAC_ENABLE, S5P_DAC_CONTROL);
	}
	else 
	{
	 	writel(S5P_DAC_DISABLE, S5P_DAC_CONTROL);
        ldo38_control_by_tvout(0);
	}
	
	TVPMPRINTK("(0x%08x)\n\r", readl(S5P_DAC_CONTROL));
}


unsigned short __s5p_tv_power_get_power_status(void)
{
	TVPMPRINTK("(0x%08x)\n\r", readl(S5P_BLK_PWR_STAT));


	return (TVPWR_TV_BLOCK_STATUS(readl(S5P_BLK_PWR_STAT)) ? 1 : 0);
}

unsigned short __s5p_tv_power_get_dac_power_status(void)
{
	TVPMPRINTK("()\n\r");

	TVPMPRINTK("(0x%08x)\n\r", readl(S5P_DAC_CONTROL));

	return((readl(S5P_DAC_CONTROL) & S5P_DAC_ENABLE) ? 1 : 0);
}

//C110: for test. PMU modifying needed.
void __s5p_tv_poweron(void)
{
	TVPMPRINTK("0x%08x\n\r",readl(S3C_VA_SYS + 0xE804));
	
	writel(readl(S3C_VA_SYS + 0xE804) | 0x1, S3C_VA_SYS + 0xE804);

	writel(readl(S5P_NORMAL_CFG) | TVPWR_SUBSYSTEM_ACTIVE, S5P_NORMAL_CFG);

	while (!TVPWR_TV_BLOCK_STATUS(readl(S5P_BLK_PWR_STAT))) {
		msleep(1);
	}

	TVPMPRINTK("0x%08x,0x%08x)\n\r", readl(S5P_NORMAL_CFG), readl(S5P_BLK_PWR_STAT));
}


void __s5p_tv_poweroff(void)
{
	TVPMPRINTK("()\n\r");

	__s5p_tv_powerset_dac_onoff(0);
 
	writel(readl(S5P_NORMAL_CFG) & ~TVPWR_SUBSYSTEM_ACTIVE, S5P_NORMAL_CFG);

	while (TVPWR_TV_BLOCK_STATUS(readl(S5P_BLK_PWR_STAT))) {
		msleep(1);
	}

	TVPMPRINTK("0x%08x,0x%08x)\n\r", readl(S5P_NORMAL_CFG), readl(S5P_BLK_PWR_STAT));
}

//Subhransu20110304
void __s5p_tv_poweroff_test(void)
{
	TVPMPRINTK("()\n\r");

	//__s5p_tv_powerset_dac_onoff(0);
 
	writel(readl(S5P_NORMAL_CFG) & ~TVPWR_SUBSYSTEM_ACTIVE, S5P_NORMAL_CFG);

	while (TVPWR_TV_BLOCK_STATUS(readl(S5P_BLK_PWR_STAT))) {
		msleep(1);
	}

	TVPMPRINTK("0x%08x,0x%08x)\n\r", readl(S5P_NORMAL_CFG), readl(S5P_BLK_PWR_STAT));
}

