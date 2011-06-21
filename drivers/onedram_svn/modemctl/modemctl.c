/**
 * header for modem control
 *
 * Copyright (C) 2010 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

//#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/workqueue.h>
#include <linux/timer.h>

#include <linux/modemctl.h>

#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#define MODEM_CTL_DEFAULT_WAKLOCK_HZ	(2*HZ)
#endif

#if defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined(CONFIG_S5PC110_HAWK_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) || defined(CONFIG_S5PC110_SIDEKICK_BOARD)
#include <linux/regulator/max8998.h>
#include <linux/regulator/max8998.h>
#include <mach/max8998_function.h>
//extern pmic_status_type pmic_read(byte slave, byte reg, byte *data, byte length);
extern boolean Set_MAX8998_PM_REG(max8998_pm_section_type reg_num, byte value);
extern byte Get_MAX8998_PM_REG(max8998_pm_section_type reg_num);
extern boolean Set_MAX8998_PM_BUCK3_LDO4TO7_11_17_Voltage(max8998_pm_section_type ldo_num, out_voltage_type set_value);
#endif

#if defined (CONFIG_CP_CHIPSET_STE) 
static struct wake_lock modemctl_wake_lock_int_resout;
static struct wake_lock modemctl_wake_lock_cp_pwr_rst;
#endif

#define DRVNAME "modemctl"

#define SIM_DEBOUNCE_TIME_HZ	(HZ)

struct modemctl;

struct modemctl_ops {
	void (*modem_on)(struct modemctl *);
	void (*modem_off)(struct modemctl *);
	void (*modem_reset)(struct modemctl *);
	void (*modem_boot_on)(struct modemctl *);
	void (*modem_boot_off)(struct modemctl *);
};

struct modemctl_info {
	const char *name;
	struct modemctl_ops ops;
};

struct modemctl {
	int irq_phone_active;
	int irq_sim_ndetect;
#if defined (CONFIG_CP_CHIPSET_STE) 
	int irq_int_resout;
	int irq_int_cp_pwr_rst;
#endif

	unsigned gpio_phone_on;
	unsigned gpio_phone_active;
	unsigned gpio_pda_active;
	unsigned gpio_cp_reset;
	unsigned gpio_usim_boot;
	unsigned gpio_flm_sel;
#if defined (CONFIG_CP_CHIPSET_STE) 
	unsigned gpio_int_resout;
	unsigned gpio_int_cp_pwr_rst;
#endif

	unsigned gpio_sim_ndetect;
	unsigned sim_reference_level;
	unsigned sim_change_reset;
	struct timer_list sim_irq_debounce_timer;

#ifdef CONFIG_HAS_WAKELOCK
	struct wake_lock mc_wlock;
	long	 waketime;
#endif	

	struct modemctl_ops *ops;

	struct class *class;
	struct device *dev;
	const struct attribute_group *group;

	struct work_struct work;
#if defined (CONFIG_CP_CHIPSET_STE) 
	struct work_struct work_int_resout;
	struct work_struct work_int_cp_pwr_rst;
#endif
};

enum {
	SIM_LEVEL_NONE = -1,
	SIM_LEVEL_STABLE,
	SIM_LEVEL_CHANGED
};

#ifdef CONFIG_HAS_WAKELOCK
static inline void _wake_lock_init(struct modemctl *mc)
{
	wake_lock_init(&mc->mc_wlock, WAKE_LOCK_SUSPEND, "modemctl");
	mc->waketime = MODEM_CTL_DEFAULT_WAKLOCK_HZ;
}

static inline void _wake_lock_destroy(struct modemctl *mc)
{
	wake_lock_destroy(&mc->mc_wlock);
}

static inline void _wake_lock_timeout(struct modemctl *mc)
{
	wake_lock_timeout(&mc->mc_wlock, mc->waketime);
}

static inline void _wake_lock_settime(struct modemctl *mc, long time)
{
	if (mc)
		mc->waketime = time;
}

static inline long _wake_lock_gettime(struct modemctl *mc)
{
	return mc?mc->waketime:MODEM_CTL_DEFAULT_WAKLOCK_HZ;
}
#else
#  define _wake_lock_init(mc) do { } while(0)
#  define _wake_lock_destroy(mc) do { } while(0)
#  define _wake_lock_timeout(mc) do { } while(0)
#  define _wake_lock_settime(mc, time) do { } while(0)
#  define _wake_lock_gettime(mc) (0)
#endif

static int sim_check_status(struct modemctl *);
static int sim_get_reference_status(struct modemctl *);
static void sim_irq_debounce_timer_func(unsigned);


#if defined (CONFIG_TARGET_LOCALE_EUR) ||  defined (CONFIG_ARIES_EUR)
#if (defined CONFIG_CP_CHIPSET_STE)
static void m5720_on(struct modemctl *);
static void m5720_off(struct modemctl *);
static void m5720_reset(struct modemctl *);
static void m5720_boot(struct modemctl *);

static struct modemctl_info mdmctl_info[] = {
	{
		.name = "xmm",
		.ops = {
			.modem_on = m5720_on,
			.modem_off = m5720_off,
			.modem_reset = m5720_reset,
			.modem_boot_on = m5720_boot,
		},
	},
};
#else
static void xmm_on(struct modemctl *);
static void xmm_off(struct modemctl *);
static void xmm_reset(struct modemctl *);
static void xmm_boot(struct modemctl *);

static struct modemctl_info mdmctl_info[] = {
	{
		.name = "xmm",
		.ops = {
			.modem_on = xmm_on,
			.modem_off = xmm_off,
			.modem_reset = xmm_reset,
			.modem_boot_on = xmm_boot,
		},
	},
};
#endif
#elif defined (CONFIG_TARGET_LOCALE_KOR) ||  defined(CONFIG_ARIES_NTT)
static void msm_on(struct modemctl *);
static void msm_off(struct modemctl *);
static void msm_reset(struct modemctl *);
static void msm_boot_on(struct modemctl *);
static void msm_boot_off(struct modemctl *);

static struct modemctl_info mdmctl_info[] = {
	{
		.name = "msm",
		.ops = {
			.modem_on = msm_on,
			.modem_off = msm_off,
			.modem_reset = msm_reset,
			.modem_boot_on = msm_boot_on,
			.modem_boot_off = msm_boot_off,
		},
	},
};
#endif

static ssize_t show_control(struct device *d,
		struct device_attribute *attr, char *buf);
static ssize_t store_control(struct device *d,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t show_status(struct device *d,
		struct device_attribute *attr, char *buf);
static ssize_t show_debug(struct device *d,
		struct device_attribute *attr, char *buf);
static ssize_t show_phoneactive(struct device *d,
		struct device_attribute *attr, char *buf);

static DEVICE_ATTR(control, S_IRUGO | S_IWUGO, show_control, store_control);
static DEVICE_ATTR(status, S_IRUGO, show_status, NULL);
static DEVICE_ATTR(debug, S_IRUGO, show_debug, NULL);
static DEVICE_ATTR(phoneactive, S_IRUGO, show_phoneactive, NULL);

static struct attribute *modemctl_attributes[] = {
	&dev_attr_control.attr,
	&dev_attr_status.attr,
	&dev_attr_debug.attr,
	&dev_attr_phoneactive.attr,	
	NULL
};

static const struct attribute_group modemctl_group = {
	.attrs = modemctl_attributes,
};

#if defined (CONFIG_TARGET_LOCALE_EUR) ||  defined (CONFIG_ARIES_EUR)
/* declare mailbox init function for xmm */
extern void onedram_init_mailbox(void);

#if defined (CONFIG_CP_CHIPSET_STE)
static void m5720_on(struct modemctl *mc)
{	
	printk("%s\n", __func__);

	/* ensure pda active pin set to low */
	gpio_set_value(mc->gpio_pda_active, 0);
	/* call mailbox init : BA goes to high, AB goes to low */
	onedram_init_mailbox();
	/* ensure cp_reset pin set to low */
	gpio_set_value(mc->gpio_cp_reset, 0);

	msleep(100); /*wait modem stable */ 
	
	if(mc->gpio_phone_on)
	  gpio_set_value(mc->gpio_phone_on, 1);


#if defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined(CONFIG_S5PC110_HAWK_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD)
	msleep(18);
	// PMIC 0x14 register, EN32kHzCP enable
		Set_MAX8998_PM_BUCK3_LDO4TO7_11_17_Voltage( LDO6, VCC_1p800);  // 1.8V set

	// LDO6 ON
	if (! max8998_ldo_is_enabled_direct(MAX8998_LDO6))
		max8998_ldo_enable_direct(MAX8998_LDO6);
	 
	Set_MAX8998_PM_REG(EN32KHZCP, 1);
#elif defined (CONFIG_S5PC110_SIDEKICK_BOARD)	
	msleep(18);
	// PMIC 0x14 register, EN32kHzCP enable
		//Set_MAX8998_PM_BUCK3_LDO4TO7_11_17_Voltage( LDO6, VCC_1p800);  // 1.8V set

	// LDO6 ON
	if (! max8998_ldo_is_enabled_direct(MAX8998_LDO6))
		max8998_ldo_enable_direct(MAX8998_LDO6);
	 
	Set_MAX8998_PM_REG(EN32KHZCP, 1);
#endif

	gpio_set_value(mc->gpio_pda_active, 1);

	msleep(150); /*wait modem stable */ 
	
	return;
}

static void m5720_off(struct modemctl *mc)
{
	int r;
	int gpio_int_cp_pwr_rst;

	printk("%s\n", __func__);

	if(mc->gpio_phone_on)
		gpio_set_value(mc->gpio_phone_on, 0);
	
	if((gpio_get_value(GPIO_INT_RESOUT) == 0) && (gpio_get_value(GPIO_CP_PWR_RST) == 0))
	{
		Set_MAX8998_PM_REG(EN32KHZCP, 0);
		return;
	}
	
	r = gpio_get_value(GPIO_CP_PWR_RST);
	if(r) // GPIO_CP_PWR_RST is HIGH
	{
		printk("%s, GPIO_CP_PWR_RST is high\n", __func__);
		
		gpio_set_value(mc->gpio_cp_reset, 1);
		while(1)
		{
			gpio_int_cp_pwr_rst = gpio_get_value(GPIO_CP_PWR_RST);

			if(!gpio_int_cp_pwr_rst)
				break;

			msleep(1000); /*wait modem stable */
		}
	}
	else
	{
		printk("%s, GPIO_CP_PWR_RST is already low\n", __func__);
	}
	
	Set_MAX8998_PM_REG(EN32KHZCP, 0);
	gpio_set_value(mc->gpio_cp_reset, 0);

	return;
}

static void m5720_reset(struct modemctl *mc)
{
	int r;
	int gpio_int_cp_pwr_rst;

	// OFF
	printk("[%s][OFF]\n", __func__);

	if(mc->gpio_phone_on)
		gpio_set_value(mc->gpio_phone_on, 0);
	
	r = gpio_get_value(GPIO_CP_PWR_RST);
	if(r) // GPIO_CP_PWR_RST is HIGH
	{
		printk("%s, GPIO_CP_PWR_RST is high\n", __func__);
		
		gpio_set_value(mc->gpio_cp_reset, 1);
		while(1)
		{
				gpio_int_cp_pwr_rst = gpio_get_value(GPIO_CP_PWR_RST);

				if(!gpio_int_cp_pwr_rst)
				break;

			msleep(1000); /*wait modem stable */
		}
		gpio_set_value(mc->gpio_cp_reset, 0);
	}
	else
	{
		printk("%s, GPIO_CP_PWR_RST is already low\n", __func__);
	}

	// ON	
	printk("[%s][ON]\n", __func__);
	
	msleep(150); /*wait modem stable */

	if(mc->gpio_phone_on)
	  gpio_set_value(mc->gpio_phone_on, 1);

	gpio_set_value(mc->gpio_pda_active, 1);

	msleep(150); /*wait modem stable */ 
	
	return;
}

static void m5720_boot(struct modemctl *mc)
{
	printk("%s\n", __func__);

	return;
}
#else
static void xmm_on(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->gpio_cp_reset)
		return;

	/* ensure pda active pin set to low */
	gpio_set_value(mc->gpio_pda_active, 0);
	/* call mailbox init : BA goes to high, AB goes to low */
	onedram_init_mailbox();
	/* ensure cp_reset pin set to low */
	gpio_set_value(mc->gpio_cp_reset, 0);
	
	msleep(100);

	//gpio_set_value(mc->gpio_cp_reset, 1);
	if(mc->gpio_phone_on)
	gpio_set_value(mc->gpio_phone_on, 1);
	gpio_set_value(mc->gpio_cp_reset, 0);
	msleep(100);  /* no spec, confirm later exactly how much time
			needed to initialize CP with RESET_PMU_N */

	gpio_set_value(mc->gpio_cp_reset, 1);
	/* Follow RESET timming delay not Power-On timming,
	   because CP_RST & PHONE_ON have been set high already. */
	// msleep(30);  /* > 26.6 + 2 msec */
	//msleep(40);     /* > 37.2 + 2 msec */
	msleep(100); /*wait modem stable */

	gpio_set_value(mc->gpio_pda_active, 1);
}

static void xmm_off(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->gpio_cp_reset)
		return;

	if(mc->gpio_phone_on)
	gpio_set_value(mc->gpio_phone_on, 0);
	gpio_set_value(mc->gpio_cp_reset, 0);
}

static void xmm_reset(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->gpio_cp_reset)
		return;

	/* To Do :
	 * hard_reset(RESET_PMU_N) and soft_reset(RESET_REQ_N)
	 * should be divided later.
	 * soft_reset is used for CORE_DUMP
	 */
	gpio_set_value(mc->gpio_cp_reset, 0);
	msleep(100); /* no spec, confirm later exactly how much time
		       needed to initialize CP with RESET_PMU_N */
	gpio_set_value(mc->gpio_cp_reset, 1);
	//msleep(40); /* > 37.2 + 2 msec */
	msleep(100); /*wait modem stable */

	/* leave it as reset state */
//	gpio_set_value(mc->gpio_phone_on, 0);
//	gpio_set_value(mc->gpio_cp_reset, 0);
}

static void xmm_boot(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);

	if(mc->gpio_usim_boot)
		gpio_set_value(mc->gpio_usim_boot, 1);

	if(mc->gpio_flm_sel)
		gpio_set_value(mc->gpio_flm_sel, 0);
}
#endif
#elif defined (CONFIG_TARGET_LOCALE_KOR)  ||  defined(CONFIG_ARIES_NTT)
static void msm_on(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->gpio_cp_reset || !mc->gpio_phone_on)
		return;

	//gpio_set_value(mc->gpio_phone_on, 0);
	//gpio_set_value(mc->gpio_cp_reset, 0);
	//msleep(500);
	gpio_set_value(mc->gpio_phone_on, 1);
	msleep(30);
	gpio_set_value(mc->gpio_cp_reset, 1);
	msleep(300);
	gpio_set_value(mc->gpio_phone_on, 0);
	msleep(500);
	gpio_set_value(mc->gpio_pda_active, 1);
	msleep(100);

	if(!mc->gpio_sim_ndetect)
		return;
}

static void msm_off(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->gpio_cp_reset || !mc->gpio_phone_on)
		return;

	gpio_set_value(mc->gpio_phone_on, 0);
	gpio_set_value(mc->gpio_cp_reset, 0);
}

static void msm_reset(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->gpio_cp_reset || !mc->gpio_phone_on)
		return;

	/* To Do :
	 * hard_reset(RESET_PMU_N) and soft_reset(RESET_REQ_N)
	 * should be divided later.
	 * soft_reset is used for CORE_DUMP
	 */
	gpio_set_value(mc->gpio_cp_reset, 0);
	msleep(500); /* no spec, confirm later exactly how much time
		       needed to initialize CP with RESET_PMU_N */
	gpio_set_value(mc->gpio_cp_reset, 1);
	msleep(40); /* > 37.2 + 2 msec */

	gpio_set_value(mc->gpio_phone_on, 0);
	gpio_set_value(mc->gpio_cp_reset, 0);
}

static void msm_boot_on(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);

	if(mc->gpio_usim_boot)
		gpio_set_value(mc->gpio_usim_boot, 1);

	if(mc->gpio_flm_sel)
		gpio_set_value(mc->gpio_flm_sel, 1);
}

static void msm_boot_off(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);

	if(mc->gpio_usim_boot)
		gpio_set_value(mc->gpio_usim_boot, 0);

	if(mc->gpio_flm_sel)
		gpio_set_value(mc->gpio_flm_sel, 0);
}
#endif

static int modem_on(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->ops || !mc->ops->modem_on) {
		// 
		return -ENXIO;
	}

	mc->ops->modem_on(mc);

	return 0;
}

static int modem_off(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->ops || !mc->ops->modem_off) {
		// 
		return -ENXIO;
	}

	mc->ops->modem_off(mc);

	return 0;
}

static int modem_reset(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->ops || !mc->ops->modem_reset) {
		// 
		return -ENXIO;
	}

	mc->ops->modem_reset(mc);

	return 0;
}

static int modem_boot_on(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->ops || !mc->ops->modem_boot_on) {
		// 
		return -ENXIO;
	}

	mc->ops->modem_boot_on(mc);

	return 0;
}

static int modem_boot_off(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->ops || !mc->ops->modem_boot_off) {
		// 
		return -ENXIO;
	}

	mc->ops->modem_boot_off(mc);

	return 0;
}

static int pda_on(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->gpio_pda_active) {

		return -ENXIO;
	}

	gpio_set_value(mc->gpio_pda_active, 1);

	return 0;
}

static int pda_off(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->gpio_pda_active) {

		return -ENXIO;
	}

	gpio_set_value(mc->gpio_pda_active, 0);

	return 0;
}

#if defined (CONFIG_CP_CHIPSET_STE)
static int modem_get_active(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->gpio_phone_active)
		return -ENXIO;

	dev_dbg(mc->dev, "phone %d\n", gpio_get_value(mc->gpio_phone_active));

	return gpio_get_value(mc->gpio_phone_active);
}
#else
static int modem_get_active(struct modemctl *mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->gpio_phone_active || !mc->gpio_cp_reset)
		return -ENXIO;

	dev_dbg(mc->dev, "cp %d phone %d\n",
			gpio_get_value(mc->gpio_cp_reset),
			gpio_get_value(mc->gpio_phone_active));

	if(gpio_get_value(mc->gpio_cp_reset))
		return !!gpio_get_value(mc->gpio_phone_active);

	return 0;
}
#endif

static ssize_t show_control(struct device *d,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	struct modemctl *mc = dev_get_drvdata(d);
	struct modemctl_ops *ops = mc->ops;

	if(ops) {
		if(ops->modem_on)
			p += sprintf(p, "on ");
		if(ops->modem_off)
			p += sprintf(p, "off ");
		if(ops->modem_reset)
			p += sprintf(p, "reset ");
		if(ops->modem_boot_on)
			p += sprintf(p, "boot_on ");

		if(ops->modem_boot_off)
			p += sprintf(p, "boot_off ");
	} else {
		p += sprintf(p, "(No ops)");
	}

	p += sprintf(p, "\n");
	return p - buf;
}

static ssize_t store_control(struct device *d,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct modemctl *mc = dev_get_drvdata(d);

	if(!strncmp(buf, "on", 2)) {
		modem_on(mc);
		return count;
	}

	if(!strncmp(buf, "off", 3)) {
		modem_off(mc);
		return count;
	}

	if(!strncmp(buf, "reset", 5)) {
		modem_reset(mc);
		return count;
	}

	if(!strncmp(buf, "boot_on", 7)) {
		modem_boot_on(mc);
		return count;
	}

	if(!strncmp(buf, "boot_off", 8)) {
		modem_boot_off(mc);
		return count;
	}
	// for compatibility
	if(!strncmp(buf, "boot", 4)) {
		modem_boot_on(mc);
		return count;
	}

	return count;
}

static ssize_t show_status(struct device *d,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	struct modemctl *mc = dev_get_drvdata(d);

	p += sprintf(p, "%d\n", modem_get_active(mc));

	return p - buf;
}

static ssize_t show_phoneactive(struct device *d,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	int level = 3;
	struct modemctl *mc = dev_get_drvdata(d);

	if (mc->gpio_phone_active) {
		level = gpio_get_value(mc->gpio_phone_active);
	}

	p += sprintf(p, "%d\n", level);
	return p - buf;
}

static ssize_t show_debug(struct device *d,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	int i;
	struct modemctl *mc = dev_get_drvdata(d);

	if(mc->irq_phone_active)
		p += sprintf(p, "Irq Phone Active: %d\n", mc->irq_phone_active);
	if(mc->irq_sim_ndetect)
		p += sprintf(p, "Irq Sim nDetect: %d\n", mc->irq_sim_ndetect);

	p += sprintf(p, "GPIO ---- \n");

	if(mc->gpio_phone_on)
		p += sprintf(p, "\t%3d %d : phone on\n", mc->gpio_phone_on,
				gpio_get_value(mc->gpio_phone_on));
	if(mc->gpio_phone_active)
		p += sprintf(p, "\t%3d %d : phone active\n", mc->gpio_phone_active,
				gpio_get_value(mc->gpio_phone_active));
	if(mc->gpio_pda_active)
		p += sprintf(p, "\t%3d %d : pda active\n", mc->gpio_pda_active,
				gpio_get_value(mc->gpio_pda_active));
	if(mc->gpio_cp_reset)
		p += sprintf(p, "\t%3d %d : CP reset\n", mc->gpio_cp_reset,
				gpio_get_value(mc->gpio_cp_reset));
	if(mc->gpio_usim_boot)
		p += sprintf(p, "\t%3d %d : USIM boot\n", mc->gpio_usim_boot,
				gpio_get_value(mc->gpio_usim_boot));
	if(mc->gpio_flm_sel)
		p += sprintf(p, "\t%3d %d : FLM sel\n", mc->gpio_flm_sel,
				gpio_get_value(mc->gpio_flm_sel));
	if(mc->gpio_sim_ndetect)
		p += sprintf(p, "\t%3d %d : Sim n Detect\n", mc->gpio_sim_ndetect,
				gpio_get_value(mc->gpio_sim_ndetect));

	#if (defined CONFIG_CP_CHIPSET_STE)
	// GPIO_INT_RESOUT
	p += sprintf(p, "\t[GPIO_INT_RESOUT] %d : CP int resout\n", gpio_get_value(GPIO_INT_RESOUT));

	// GPIO_CP_PWR_RST
	p += sprintf(p, "\t[GPIO_CP_PWR_RST] %d : CP int cp pwr rst\n", gpio_get_value(GPIO_CP_PWR_RST));
	#endif

	p += sprintf(p, "Support types --- \n");
	for(i=0;i<ARRAY_SIZE(mdmctl_info);i++) {
		if(mc->ops == &mdmctl_info[i].ops) {
			p += sprintf(p, "\t * ");
		} else {
			p += sprintf(p, "\t   ");
		}
		p += sprintf(p, "%s\n", mdmctl_info[i].name);
	}

	return p - buf;
}

static void mc_work(struct work_struct *work)
{
	struct modemctl *mc = container_of(work, struct modemctl, work);
	int r;


#if defined (CONFIG_CP_CHIPSET_STE) && defined (CONFIG_S5PC110_HAWK_BOARD)
	r = 1;
	printk("(mc_work) mc->sim_change_reset =[%d], mc->sim_reference_level=[%d] \n", mc->sim_change_reset, mc->sim_reference_level);
#else
	r = modem_get_active(mc);
	if (r < 0) {
		dev_err(mc->dev, "Not initialized\n");
		return;
	}

	dev_info(mc->dev, "PHONE ACTIVE: %d\n", r);
#endif

	if (r) {
		if (mc->sim_change_reset == SIM_LEVEL_CHANGED) {
			kobject_uevent(&mc->dev->kobj, KOBJ_CHANGE);	
		} else {
			if (mc->sim_reference_level == SIM_LEVEL_NONE) {
				sim_get_reference_status(mc);
			}
			kobject_uevent(&mc->dev->kobj, KOBJ_ONLINE);
		}
	}
	else
		kobject_uevent(&mc->dev->kobj, KOBJ_OFFLINE);
}

static irqreturn_t modemctl_irq_handler(int irq, void *dev_id)
{
	struct modemctl *mc = (struct modemctl *)dev_id;

	if (!work_pending(&mc->work))
		schedule_work(&mc->work);

	return IRQ_HANDLED;
}

#if defined (CONFIG_CP_CHIPSET_STE) 
static void mc_int_resout_work(struct work_struct *work)
{
	struct modemctl *mc = container_of(work, struct modemctl, work_int_resout);

	if(gpio_get_value(GPIO_INT_RESOUT) == 1)
		return;

	if ( (mc->gpio_phone_on) && (gpio_get_value(mc->gpio_phone_on)) )
	{
		wake_lock(&modemctl_wake_lock_int_resout);
		printk("[STE][%s] INT_RESOUT goes to Low. So, set PHONE_ON to Low. = [%d], get EN32kHzCP = [%d]\n", __func__, mc->gpio_phone_on, Get_MAX8998_PM_REG(EN32KHZCP));
		
		kobject_uevent(&mc->dev->kobj, KOBJ_OFFLINE);

		wake_lock_timeout(&modemctl_wake_lock_int_resout, 600 * HZ);
	}	
	else
	{
		printk("[STE][%s] mc->gpio_phone_on is [%d] \n", __func__, gpio_get_value(mc->gpio_phone_on));
	}

	return;
}

static irqreturn_t modemctl_irq_handler_int_resout(int irq, void *dev_id)
{
	struct modemctl *mc = (struct modemctl *)dev_id;

	if(gpio_get_value(GPIO_INT_RESOUT) == 1)
		return;

	if (!work_pending(&mc->work_int_resout))
		schedule_work(&mc->work_int_resout);	

	return IRQ_HANDLED;
}
static void mc_int_cp_pwr_rst_work(struct work_struct *work)
{
	struct modemctl *mc = container_of(work, struct modemctl, work_int_cp_pwr_rst);

	if(gpio_get_value(GPIO_CP_PWR_RST) == 1)
		return;

	if ( (mc->gpio_phone_on) && (gpio_get_value(mc->gpio_phone_on)))
	{
		wake_lock(&modemctl_wake_lock_cp_pwr_rst);

		printk("[STE][%s] INT_CP_PWR_RST goes to Low. get EN32kHzCP = [%d]\n", __func__, Get_MAX8998_PM_REG(EN32KHZCP));
		
		wake_lock_timeout(&modemctl_wake_lock_cp_pwr_rst, 600 * HZ);
	}	
	else
	{
		printk("[STE][%s] mc->gpio_phone_on is [%d] \n", __func__, gpio_get_value(mc->gpio_phone_on));
	}

	return;
}

static irqreturn_t modemctl_irq_handler_int_cp_pwr_rst(int irq, void *dev_id)
{
	struct modemctl *mc = (struct modemctl *)dev_id;

	if(gpio_get_value(GPIO_CP_PWR_RST) == 1)
		return;

	if (!work_pending(&mc->work_int_cp_pwr_rst))
		schedule_work(&mc->work_int_cp_pwr_rst);	

	return IRQ_HANDLED;
}
#endif // defined (CONFIG_CP_CHIPSET_STE) 

static int sim_get_reference_status(struct modemctl* mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->gpio_sim_ndetect)
		return -ENXIO;

	mc->sim_reference_level = gpio_get_value(mc->gpio_sim_ndetect);	

	return 0;
}

#if defined (CONFIG_CP_CHIPSET_STE) && defined (CONFIG_S5PC110_HAWK_BOARD)

static unsigned current_sim_status = 0;

static int sim_check_status_first(struct modemctl* mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->gpio_sim_ndetect)
			return -ENXIO;

	current_sim_status = gpio_get_value(mc->gpio_sim_ndetect);
	
	mc->sim_change_reset = SIM_LEVEL_CHANGED;
	
	return 0;
}
static int sim_check_status_second(struct modemctl* mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->gpio_sim_ndetect)
			return -ENXIO;

	if (current_sim_status == gpio_get_value(mc->gpio_sim_ndetect)) 
	{
		mc->sim_change_reset = SIM_LEVEL_CHANGED;
	}		
	else
	{
		mc->sim_change_reset = SIM_LEVEL_STABLE;
	}

	return 0;
}

#else
static int sim_check_status(struct modemctl* mc)
{
	dev_dbg(mc->dev, "%s\n", __func__);
	if(!mc->gpio_sim_ndetect || mc->sim_reference_level == SIM_LEVEL_NONE) {
		return -ENXIO;
	}

	if (mc->sim_reference_level != gpio_get_value(mc->gpio_sim_ndetect)) {
		mc->sim_change_reset = SIM_LEVEL_CHANGED;
		}		
	else
		{
		mc->sim_change_reset = SIM_LEVEL_STABLE;
		}

	return 0;
}
#endif

static void sim_irq_debounce_timer_func(unsigned aulong)
{
	struct modemctl *mc = (struct modemctl *)aulong;
	int r;

#if defined (CONFIG_CP_CHIPSET_STE) && defined (CONFIG_S5PC110_HAWK_BOARD)
	r = sim_check_status_second(mc);
#else
	r = sim_check_status(mc);
#endif
	
	if (r < 0) {
		dev_err(mc->dev, "Not initialized\n");
		return;
	}

	if (mc->sim_change_reset == SIM_LEVEL_CHANGED) {
		if (!work_pending(&mc->work))
			schedule_work(&mc->work);

		_wake_lock_timeout(mc);
	}
}

static irqreturn_t simctl_irq_handler(int irq, void *dev_id)
{
	struct modemctl *mc = (struct modemctl *)dev_id;
	int r;

#if defined (CONFIG_CP_CHIPSET_STE) && defined (CONFIG_S5PC110_HAWK_BOARD)
	printk("[simctl_irq_handler #1] mc->sim_reference_level =[%d], mc->gpio_int_resout=[%d] \n", mc->sim_reference_level, mc->gpio_int_resout); 
	r = sim_check_status_first(mc);
#else
	if ( mc->sim_reference_level == SIM_LEVEL_NONE) {
		return IRQ_HANDLED;
	}
	
	r = sim_check_status(mc);
#endif

	if (r < 0) {
		dev_err(mc->dev, "Not initialized\n");
		return IRQ_HANDLED;
	}

	if (mc->sim_change_reset == SIM_LEVEL_CHANGED) {
		mod_timer(&mc->sim_irq_debounce_timer, jiffies + SIM_DEBOUNCE_TIME_HZ);
		_wake_lock_timeout(mc);
	}

	return IRQ_HANDLED;
}

static struct modemctl_ops* _find_ops(const char *name)
{
	int i;
	struct modemctl_ops *ops = NULL;

	for(i=0;i<ARRAY_SIZE(mdmctl_info);i++) {
		if(mdmctl_info[i].name && !strcmp(name, mdmctl_info[i].name))
			ops = &mdmctl_info[i].ops;
	}

	return ops;
}

static void _free_all(struct modemctl *mc)
{
	if(mc) {
		if(mc->ops)
			mc->ops = NULL;

		if(mc->group)
			sysfs_remove_group(&mc->dev->kobj, mc->group);

		if(mc->irq_phone_active)
			free_irq(mc->irq_phone_active, mc);

		if(mc->irq_sim_ndetect)
			free_irq(mc->irq_sim_ndetect, mc);

		if(mc->dev)
			device_destroy(mc->class, mc->dev->devt);

		if(mc->class)
			class_destroy(mc->class);

		_wake_lock_destroy(mc);

		kfree(mc);
	}
}

static int __devinit modemctl_probe(struct platform_device *pdev)
{
	struct modemctl *mc = NULL;
	struct modemctl_platform_data *pdata;
	struct resource *res;
	int r = 0;
	int irq_phone_active, irq_sim_ndetect;
#if defined (CONFIG_CP_CHIPSET_STE) 
	int irq_int_resout;
	int irq_int_cp_pwr_rst;
#endif

	printk("[%s]\n",__func__);

	pdata = pdev->dev.platform_data;
	if(!pdata || !pdata->cfg_gpio) {
		dev_err(&pdev->dev, "No platform data\n");
		r = -EINVAL;
		goto err;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if(!res)  {
		dev_err(&pdev->dev, "failed to get irq number\n");
		r = -EINVAL;
		goto err;
	}
	irq_phone_active = res->start;

#if !(defined(CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_T959_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) || defined(CONFIG_S5PC110_CELOX_BOARD) || defined(CONFIG_S5PC110_FLEMING_BOARD)|| defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined (CONFIG_S5PC110_SIDEKICK_BOARD))

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	if(!res)  {
		dev_err(&pdev->dev, "failed to get irq number\n");
		r = -EINVAL;
		goto err;
	}
	irq_sim_ndetect = res->start;
#endif

#if defined (CONFIG_CP_CHIPSET_STE) 
	#if defined (CONFIG_S5PC110_HAWK_BOARD)
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 2);
	if(!res)  {
		dev_err(&pdev->dev, "failed to get irq number\n");
		r = -EINVAL;
		goto err;
	}
	irq_int_resout = res->start;
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 3);
	if(!res)  {
		dev_err(&pdev->dev, "failed to get irq number\n");
		r = -EINVAL;
		goto err;
	}
	irq_int_cp_pwr_rst = res->start;	
	#elif defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined(CONFIG_S5PC110_SIDEKICK_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD)
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	if(!res)  {
		dev_err(&pdev->dev, "failed to get irq number\n");
		r = -EINVAL;
		goto err;
	}
	irq_int_resout = res->start;
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 2);
	if(!res)  {
		dev_err(&pdev->dev, "failed to get irq number\n");
		r = -EINVAL;
		goto err;
	}
	irq_int_cp_pwr_rst = res->start;	
	#endif
#endif

	
	mc = kzalloc(sizeof(struct modemctl), GFP_KERNEL);
	if(!mc) {
		dev_err(&pdev->dev, "failed to allocate device\n");
		r = -ENOMEM;
		goto err;
	}

	mc->gpio_phone_on = pdata->gpio_phone_on;
	mc->gpio_phone_active = pdata->gpio_phone_active;
	mc->gpio_pda_active = pdata->gpio_pda_active;
	mc->gpio_cp_reset = pdata->gpio_cp_reset;
	mc->gpio_usim_boot = pdata->gpio_usim_boot;
	mc->gpio_flm_sel = pdata->gpio_flm_sel;
	mc->gpio_sim_ndetect = pdata->gpio_sim_ndetect;
	mc->sim_change_reset = SIM_LEVEL_NONE;
	mc->sim_reference_level = SIM_LEVEL_NONE;	
#if defined (CONFIG_CP_CHIPSET_STE) 
	mc->gpio_int_resout = pdata->gpio_int_resout;
	mc->gpio_int_cp_pwr_rst = pdata->gpio_int_cp_pwr_rst;
#endif	

	mc->ops = _find_ops(pdata->name);
	if(!mc->ops) {
		dev_err(&pdev->dev, "can't find operations: %s\n", pdata->name);
		goto err;
	}

	mc->class = class_create(THIS_MODULE, "modemctl");
	if(IS_ERR(mc->class)) {
		dev_err(&pdev->dev, "failed to create sysfs class\n");
		r = PTR_ERR(mc->class);
		mc->class = NULL;
		goto err;
	}

	pdata->cfg_gpio();

	mc->dev = device_create(mc->class, &pdev->dev, MKDEV(0, 0), NULL, "%s", pdata->name);
	if(IS_ERR(mc->dev)) {
		dev_err(&pdev->dev, "failed to create device\n");
		r = PTR_ERR(mc->dev);
		goto err;
	}
	dev_set_drvdata(mc->dev, mc);

	r = sysfs_create_group(&mc->dev->kobj, &modemctl_group);
	if(r) {
		dev_err(&pdev->dev, "failed to create sysfs files\n");
		goto err;
	}
	mc->group = &modemctl_group;

#if defined (CONFIG_CP_CHIPSET_STE) 
	INIT_WORK(&mc->work_int_resout, mc_int_resout_work);

	r = request_irq(irq_int_resout, modemctl_irq_handler_int_resout,  
			IRQF_TRIGGER_FALLING, "int_resout", mc);
	if(r) {
		dev_err(&pdev->dev, "failed to allocate an interrupt(%d)\n", 
				irq_int_resout);
		goto err;
	}
	mc->irq_int_resout = irq_int_resout;
	INIT_WORK(&mc->work_int_cp_pwr_rst, mc_int_cp_pwr_rst_work);

	r = request_irq(irq_int_cp_pwr_rst, modemctl_irq_handler_int_cp_pwr_rst,  
			IRQF_TRIGGER_FALLING, "INT_CP_PWR_RST", mc);
	if(r) {
		dev_err(&pdev->dev, "failed to allocate an interrupt(%d)\n", 
				irq_int_cp_pwr_rst);
		goto err;
	}
	mc->irq_int_cp_pwr_rst = irq_int_cp_pwr_rst;
	#if defined (CONFIG_S5PC110_HAWK_BOARD)
	INIT_WORK(&mc->work, mc_work);	
	if (mc->sim_reference_level == SIM_LEVEL_NONE) 
	{
		sim_get_reference_status(mc);
	}
	#endif
#else
	INIT_WORK(&mc->work, mc_work);

	r = request_irq(irq_phone_active, modemctl_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"phone_active", mc);
	if(r) {
		dev_err(&pdev->dev, "failed to allocate an interrupt(%d)\n",
				irq_phone_active);
		goto err;
	}
	mc->irq_phone_active = irq_phone_active;
#endif

#if defined (CONFIG_CP_CHIPSET_STE) && defined (CONFIG_S5PC110_HAWK_BOARD)
	_wake_lock_init(mc);
#endif

#if !(defined(CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_T959_BOARD) || defined(CONFIG_S5PC110_FLEMING_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined (CONFIG_S5PC110_SIDEKICK_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD))

	setup_timer(&mc->sim_irq_debounce_timer, (void*)sim_irq_debounce_timer_func,(unsigned long)mc);

	r = request_irq(irq_sim_ndetect, simctl_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"sim_ndetect", mc);
	if(r) {
		dev_err(&pdev->dev, "failed to allocate an interrupt(%d)\n",
				irq_sim_ndetect);
		goto err;
	}
	mc->irq_sim_ndetect= irq_sim_ndetect;	
#endif 	

#if defined (CONFIG_CP_CHIPSET_STE) && defined (CONFIG_S5PC110_HAWK_BOARD)

#else
	_wake_lock_init(mc);
#endif

	platform_set_drvdata(pdev, mc);

	return 0;

err:
	_free_all(mc);
	return r;
}

static int __devexit modemctl_remove(struct platform_device *pdev)
{
	struct modemctl *mc = platform_get_drvdata(pdev);

	flush_work(&mc->work);
	platform_set_drvdata(pdev, NULL);
	_free_all(mc);
	return 0;
}

#ifdef CONFIG_PM
static int modemctl_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct modemctl *mc = platform_get_drvdata(pdev);

	pda_off(mc);

	return 0;
}

static int modemctl_resume(struct platform_device *pdev)
{
	struct modemctl *mc = platform_get_drvdata(pdev);

	pda_on(mc);

	return 0;
}
#else
#  define modemctl_suspend NULL
#  define modemctl_resume NULL
#endif

static struct platform_driver modemctl_driver = {
	.probe = modemctl_probe,
	.remove = __devexit_p(modemctl_remove),
	.suspend = modemctl_suspend,
	.resume = modemctl_resume,
	.driver = {
		.name = DRVNAME,
	},
};

static int __init modemctl_init(void)
{
	printk("[%s]\n",__func__);
		
#if defined (CONFIG_CP_CHIPSET_STE) 
	wake_lock_init(&modemctl_wake_lock_int_resout, WAKE_LOCK_SUSPEND, "modemctl_wakelock");
	wake_lock_init(&modemctl_wake_lock_cp_pwr_rst, WAKE_LOCK_SUSPEND, "modemctl_wakelock");
#endif
		
	return platform_driver_register(&modemctl_driver);
}

static void __exit modemctl_exit(void)
{
	platform_driver_unregister(&modemctl_driver);
}

module_init(modemctl_init);
module_exit(modemctl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Suchang Woo <suchang.woo@samsung.com>");
MODULE_DESCRIPTION("Modem control");
