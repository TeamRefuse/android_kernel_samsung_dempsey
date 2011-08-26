/* linux/arch/arm/mach-s5pv210/mach-aries.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/max8998.h>

#ifdef CONFIG_REGULATOR_MAX8893			
#include <linux/mfd/max8893.h>
#endif
#ifdef CONFIG_REGULATOR_BH6173			
#include <linux/mfd/bh6173.h>
#endif

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/usb/ch9.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/clk.h>
#include <linux/usb/ch9.h>
#include <linux/input/cypress-touchkey.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/skbuff.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#if defined(CONFIG_TOUCHSCREEN_MXT224E)
#include <linux/i2c/mxt224e.h>
#endif

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/gpio.h>
#include <mach/gpio-aries.h>
#if defined (CONFIG_S5PC110_KEPLER_BOARD)
#include <mach/gpio-settings-kepler.h>
#elif  defined (CONFIG_S5PC110_HAWK_BOARD)
#include <mach/gpio-settings-hawk.h>
#elif  defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD)
#include <mach/gpio-settings-vibrantplus.h>
#elif  defined(CONFIG_S5PC110_DEMPSEY_BOARD)				// MR work 
#include <mach/gpio-settings-dempsey.h>
#else
#include <mach/gpio-settings.h>
#endif
#include <mach/adc.h>
#if defined(CONFIG_S5PC110_DEMPSEY_BOARD)
#if defined (CONFIG_FB_S3C_LDI) 
#include <linux/lcd.h>
#endif
#endif

#ifdef CONFIG_SENSORS_L3G4200D_GYRO
#include <linux/i2c/l3g4200d.h>
#endif


#include <mach/param.h>
#include <mach/system.h>
#include <mach/sec_switch.h>

#include <linux/usb/gadget.h>
#include <linux/fsa9480.h>
#include <linux/pn544.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/wlan_plat.h>
#include <linux/mfd/wm8994/wm8994_pdata.h>

#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#include <plat/media.h>
#include <mach/media.h>
#endif

#ifdef CONFIG_S5PV210_POWER_DOMAIN
#include <mach/power-domain.h>
#endif

#include <media/ce147_platform.h>
#include <media/s5ka3dfx_platform.h>
#include <media/s5k4ecgx.h>

#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_SIDEKICK_BOARD)
#include <media/s5k5ccgx_platform.h>
#include <media/sr030pc30_platform.h>
#endif

#ifdef CONFIG_VIDEO_M5MO
#include <media/m5mo_platform.h>
#endif
#ifdef CONFIG_VIDEO_SR130PC10 //NAGSM_Android_HQ_Camera_SungkooLee_20101230
#include <media/sr130pc10_platform.h>
#endif

#include <plat/regs-serial.h>
#include <plat/s5pv210.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/fb.h>
#include <plat/mfc.h>
#include <plat/iic.h>
//+CG2900_GingerBread
#include <plat/gpio-cfg.h>
//-CG2900_GingerBread
#include <plat/pm.h>
#include <plat/regs-fimc.h>
#include <plat/csis.h>
#include <plat/sdhci.h>
#include <plat/fimc.h>
#include <plat/jpeg.h>
#include <plat/clock.h>
#include <plat/regs-otg.h>
#if defined (CONFIG_OPTICAL_TAOS_TRITON)
#include <linux/taos.h>
#else
#include <linux/gp2a.h>
#endif
#include <../../../drivers/video/samsung/s3cfb.h>
#include <linux/sec_jack.h>
#include <linux/input/mxt224.h>
#include <linux/max17040_battery.h>
#include <linux/mfd/max8998.h>
#include <linux/switch.h>

#ifdef CONFIG_KERNEL_DEBUG_SEC
#include <linux/kernel_sec_common.h>
#endif

#if defined(CONFIG_S5PC110_DEMPSEY_BOARD)
#include <linux/input/k3g.h>
#include <linux/k3dh.h>
#include <linux/i2c/ak8975.h>
#include <linux/cm3663.h>
#endif
#include "aries.h"

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

struct device *switch_dev;
EXPORT_SYMBOL(switch_dev);

struct device *gps_dev = NULL;
EXPORT_SYMBOL(gps_dev);

unsigned int HWREV =0;
EXPORT_SYMBOL(HWREV);

void (*sec_set_param_value)(int idx, void *value);
EXPORT_SYMBOL(sec_set_param_value);

void (*sec_get_param_value)(int idx, void *value);
EXPORT_SYMBOL(sec_get_param_value);

#define KERNEL_REBOOT_MASK      0xFFFFFFFF
#define REBOOT_MODE_FAST_BOOT		7

#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER	24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define WLAN_SKB_BUF_NUM	17

#if defined(CONFIG_S5PC110_HAWK_BOARD)
unsigned int HWREV_HAWK=0;
EXPORT_SYMBOL(HWREV_HAWK);
#endif

#if defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
unsigned int VPLUSVER=0;
EXPORT_SYMBOL(VPLUSVER);
#endif


static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wifi_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static int aries_notifier_call(struct notifier_block *this,
					unsigned long code, void *_cmd)
{
	int mode = REBOOT_MODE_NONE;

	if ((code == SYS_RESTART) && _cmd) {
		if (!strcmp((char *)_cmd, "arm11_fota"))
			mode = REBOOT_MODE_ARM11_FOTA;
		else if (!strcmp((char *)_cmd, "arm9_fota"))
			mode = REBOOT_MODE_ARM9_FOTA;
		else if (!strcmp((char *)_cmd, "recovery"))
			mode = REBOOT_MODE_RECOVERY;
		else if (!strcmp((char *)_cmd, "bootloader"))
			mode = REBOOT_MODE_FAST_BOOT;
		else if (!strcmp((char *)_cmd, "download"))
			mode = REBOOT_MODE_DOWNLOAD;
		else
			mode = REBOOT_MODE_NONE;
	}
	if (code != SYS_POWER_OFF) {
		if (sec_set_param_value) {
			sec_set_param_value(__REBOOT_MODE, &mode);
		}
	}

	return NOTIFY_DONE;
}

static struct notifier_block aries_reboot_notifier = {
	.notifier_call = aries_notifier_call,
};

static ssize_t hwrev_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", HWREV);
}

static DEVICE_ATTR(hwrev, S_IRUGO, hwrev_show, NULL);

#if !defined(CONFIG_ARIES_NTT)
static void gps_gpio_init(void)
{
	struct device *gps_dev;

	gps_dev = device_create(sec_class, NULL, 0, NULL, "gps");
	if (IS_ERR(gps_dev)) {
		pr_err("Failed to create device(gps)!\n");
		goto err;
	}
	if (device_create_file(gps_dev, &dev_attr_hwrev) < 0)
		pr_err("Failed to create device file(%s)!\n",
		       dev_attr_hwrev.attr.name);
	
	gpio_request(GPIO_GPS_nRST, "GPS_nRST");	/* XMMC3CLK */
	s3c_gpio_setpull(GPIO_GPS_nRST, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_GPS_nRST, S3C_GPIO_OUTPUT);
	gpio_direction_output(GPIO_GPS_nRST, 1);

	gpio_request(GPIO_GPS_PWR_EN, "GPS_PWR_EN");	/* XMMC3CLK */
	s3c_gpio_setpull(GPIO_GPS_PWR_EN, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_GPS_PWR_EN, S3C_GPIO_OUTPUT);
	gpio_direction_output(GPIO_GPS_PWR_EN, 0);

	s3c_gpio_setpull(GPIO_GPS_RXD, S3C_GPIO_PULL_UP);
	gpio_export(GPIO_GPS_nRST, 1);
	gpio_export(GPIO_GPS_PWR_EN, 1);

	gpio_export_link(gps_dev, "GPS_nRST", GPIO_GPS_nRST);
	gpio_export_link(gps_dev, "GPS_PWR_EN", GPIO_GPS_PWR_EN);

 err:
	return;
}
#endif

static void aries_switch_init(void)
{
	sec_class = class_create(THIS_MODULE, "sec");

	if (IS_ERR(sec_class))
		pr_err("Failed to create class(sec)!\n");

	switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");

	if (IS_ERR(switch_dev))
		pr_err("Failed to create device(switch)!\n");
};

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define S5PV210_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define S5PV210_ULCON_DEFAULT	S3C2410_LCON_CS8

#define S5PV210_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg aries_uartcfgs[] __initdata = {
	{
		.hwport		= 0,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
#ifdef CONFIG_MACH_HERRING
		.wake_peer	= aries_bt_uart_wake_peer,
#endif
	},
	{
		.hwport		= 1,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
#if defined(CONFIG_S5PC110_T959_BOARD) || defined(CONFIG_S5PC110_HAWK_BOARD) || defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD)
#ifdef CONFIG_GPS_CHIPSET_STE_CG2900 /* STE for CG2900 */
                .ufcon		 = S3C2410_UFCON_FIFOMODE | S5PV210_UFCON_TXTRIG64 | S5PV210_UFCON_RXTRIG8, // -> RX trigger leve : 8byte.
#else
		.ufcon		 = S3C2410_UFCON_FIFOMODE | S5PV210_UFCON_TXTRIG64 | S5PV210_UFCON_RXTRIG1, // -> RX trigger leve : 8byte.
#endif
#else
		.ufcon		= S5PV210_UFCON_DEFAULT,
#endif 
	},
#ifndef CONFIG_FIQ_DEBUGGER
	{
		.hwport		= 2,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
#endif
	{
		.hwport		= 3,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
};

#if defined (CONFIG_S5PC110_HAWK_BOARD) /* nat */
static struct s3cfb_lcd s6e63m0 = {
	.width = 480,
	.height = 800,
	.p_width = 52,
	.p_height = 86,
	.bpp = 24,
	.freq = 60,
	
  .timing = {
    .h_fp = 82, 
    .h_bp = 2, 
    .h_sw = 4,  
    .v_fp = 5,  
    .v_fpe = 1,
    .v_bp = 1,
    .v_bpe = 1,
    .v_sw = 2,
    },

	.polarity = {
		.rise_vclk = 1,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 0, 
	},
};
#else
static struct s3cfb_lcd s6e63m0 = {
	.width = 480,
	.height = 800,
	.p_width = 52,
	.p_height = 86,
	.bpp = 24,
	.freq = 60,

	.timing = {
		.h_fp = 16,
		.h_bp = 16,
		.h_sw = 2,
		.v_fp = 28,
		.v_fpe = 1,
		.v_bp = 1,
		.v_bpe = 1,
		.v_sw = 2,
	},
	.polarity = {
		.rise_vclk = 1,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 1,
	},
};
#endif

#if 0
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC0 (14745 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC1 (9900 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC2 (14745 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC0 (32768 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC1 (32768 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMD (4800 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_JPEG (8192 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_PMEM (8192 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_GPU1 (3300 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_ADSP (6144 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_TEXTSTREAM (3000 * SZ_1K)
#else	// optimized settings, 19th Jan.2011
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC0 (12288 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC1 (9900 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC2 (12288 * SZ_1K)
#if !defined(CONFIG_ARIES_NTT)   
/*#if  defined(CONFIG_S5PC110_DEMPSEY_BOARD)// NASW_20110622, miranda.lee : Dempsey - support playing 1080p 	
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC0 (36864 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC1 (36864 * SZ_1K)
#else*/
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC0 (32768 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC1 (32768 * SZ_1K)
//#endif
#else    /* NTT - support playing 1080p */
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC0 (36864 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC1 (36864 * SZ_1K)
#endif
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMD (3000 * SZ_1K)
#if defined(CONFIG_S5PC110_DEMPSEY_BOARD)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_JPEG (8312 * SZ_1K)
#else
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_JPEG (5012 * SZ_1K)
#endif
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_PMEM (5550 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_GPU1 (3300 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_ADSP (1500 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_TEXTSTREAM (3000 * SZ_1K)
#endif


static struct s5p_media_device aries_media_devs[] = {
	[0] = {
		.id = S5P_MDEV_MFC,
		.name = "mfc",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC0,
		.paddr = 0,
	},
	[1] = {
		.id = S5P_MDEV_MFC,
		.name = "mfc",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC1,
		.paddr = 0,
	},
	[2] = {
		.id = S5P_MDEV_FIMC0,
		.name = "fimc0",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC0,
		.paddr = 0,
	},
	[3] = {
		.id = S5P_MDEV_FIMC1,
		.name = "fimc1",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC1,
		.paddr = 0,
	},
	[4] = {
		.id = S5P_MDEV_FIMC2,
		.name = "fimc2",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC2,
		.paddr = 0,
	},
	[5] = {
		.id = S5P_MDEV_JPEG,
		.name = "jpeg",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_JPEG,
		.paddr = 0,
	},
	[6] = {
		.id = S5P_MDEV_FIMD,
		.name = "fimd",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMD,
		.paddr = 0,
	},
	[7] = {
		.id = S5P_MDEV_PMEM,
		.name = "pmem",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_PMEM,
		.paddr = 0,
	},
	[8] = {
		.id = S5P_MDEV_PMEM_GPU1,
		.name = "pmem_gpu1",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_GPU1,
		.paddr = 0,
	},	
	[9] = {
		.id = S5P_MDEV_PMEM_ADSP,
		.name = "pmem_adsp",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_ADSP,
		.paddr = 0,
	},		
	[10] = {
		.id = S5P_MDEV_TEXSTREAM,
		.name = "s3c_bc",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_TEXTSTREAM,
		.paddr = 0,
	},		
};

static struct regulator_consumer_supply ldo3_consumer[] = {
	REGULATOR_SUPPLY("usb_io", NULL),
};

#if  defined(CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_HAWK_BOARD) || defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD)	// MR work
static struct regulator_consumer_supply ldo4_consumer[] = {
	REGULATOR_SUPPLY("vadcldo4", NULL),
};
#endif
static struct regulator_consumer_supply ldo5_consumer[] = {
	REGULATOR_SUPPLY("vtf", NULL),
};

#if defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined(CONFIG_S5PC110_HAWK_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) || defined (CONFIG_S5PC110_DEMPSEY_BOARD) 
static struct regulator_consumer_supply ldo6_consumer[] = {
	REGULATOR_SUPPLY("cp_rtc", NULL),
};
#endif



static struct regulator_consumer_supply ldo7_consumer[] = {
	REGULATOR_SUPPLY("vlcd", NULL),
};

static struct regulator_consumer_supply ldo8_consumer[] = {
	REGULATOR_SUPPLY("usb_core", NULL),
	REGULATOR_SUPPLY("tvout", NULL),
};

//#if defined(CONFIG_VIDEO_S5K5CCGX) || defined(CONFIG_VIDEO_SR030PC30)
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_SIDEKICK_BOARD)
static struct regulator_consumer_supply ldo11_consumer[] = {
	REGULATOR_SUPPLY("cam_vga_af", NULL),
};
#else
static struct regulator_consumer_supply ldo11_consumer[] = {
	REGULATOR_SUPPLY("cam_af", NULL),
};
#endif

//#if defined(CONFIG_VIDEO_S5K5CCGX) || defined(CONFIG_VIDEO_SR030PC30)
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_SIDEKICK_BOARD)
static struct regulator_consumer_supply ldo12_consumer[] = {
	REGULATOR_SUPPLY("cam_vga_avdd", NULL),
};
#elif  defined(CONFIG_S5PC110_DEMPSEY_BOARD)
static struct regulator_consumer_supply ldo12_consumer[] = {
	REGULATOR_SUPPLY("cam_vmipi", NULL),
};
#else
static struct regulator_consumer_supply ldo12_consumer[] = {
	REGULATOR_SUPPLY("cam_sensor", NULL),
};
#endif


#if defined (CONFIG_OPTICAL_TAOS_TRITON)
static struct regulator_consumer_supply ldo13_consumer[] = {
	REGULATOR_SUPPLY("taos_triton", NULL),
};
#elif defined (CONFIG_S5PC110_DEMPSEY_BOARD) 

static struct regulator_consumer_supply ldo13_consumer[] = {
	REGULATOR_SUPPLY("touch", NULL),
};


#else
static struct regulator_consumer_supply ldo13_consumer[] = {
	REGULATOR_SUPPLY("vga_vddio", NULL),
};
#endif


#if defined (CONFIG_S5PC110_HAWK_BOARD) 
static struct regulator_consumer_supply ldo14_consumer[] = {
	REGULATOR_SUPPLY("key_led", NULL),
};
#elif defined (CONFIG_S5PC110_DEMPSEY_BOARD)	
static struct regulator_consumer_supply ldo14_consumer[] = {
	REGULATOR_SUPPLY("tsp_vdd", NULL),
};

#else
static struct regulator_consumer_supply ldo14_consumer[] = {
	REGULATOR_SUPPLY("vga_dvdd", NULL),
};
#endif


//#if defined(CONFIG_VIDEO_S5K5CCGX) || defined(CONFIG_VIDEO_SR030PC30)
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_SIDEKICK_BOARD)
static struct regulator_consumer_supply ldo15_consumer[] = {
	REGULATOR_SUPPLY("vga_core", NULL),
};
#elif defined (CONFIG_S5PC110_DEMPSEY_BOARD)	
static struct regulator_consumer_supply ldo15_consumer[] = {
	REGULATOR_SUPPLY("tsp_avdd", NULL),
};

#else
static struct regulator_consumer_supply ldo15_consumer[] = {
	REGULATOR_SUPPLY("cam_isp_host", NULL),
};
#endif

//#if defined(CONFIG_VIDEO_S5K5CCGX) || defined(CONFIG_VIDEO_SR030PC30)
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_SIDEKICK_BOARD)
static struct regulator_consumer_supply ldo16_consumer[] = {
	REGULATOR_SUPPLY("cam_vga_vddio", NULL),
};
#else
static struct regulator_consumer_supply ldo16_consumer[] = {
	REGULATOR_SUPPLY("vga_avdd", NULL),
};
#endif

static struct regulator_consumer_supply ldo17_consumer[] = {
	REGULATOR_SUPPLY("vcc_lcd", NULL),
};

static struct regulator_consumer_supply buck1_consumer[] = {
	REGULATOR_SUPPLY("vddarm", NULL),
};

static struct regulator_consumer_supply buck2_consumer[] = {
	REGULATOR_SUPPLY("vddint", NULL),
};
//#if defined(CONFIG_VIDEO_S5K5CCGX) || defined(CONFIG_VIDEO_SR030PC30)
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_SIDEKICK_BOARD)
static struct regulator_consumer_supply buck4_consumer[] = {
	REGULATOR_SUPPLY("cam_core", NULL),
};
#else
static struct regulator_consumer_supply buck4_consumer[] = {
	REGULATOR_SUPPLY("cam_isp_core", NULL),
};
#endif
static struct regulator_init_data aries_ldo2_data = {
	.constraints	= {
		.name		= "VALIVE_1.2V",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled = 1,
		},
	},
};

static struct regulator_init_data aries_ldo3_data = {
	.constraints	= {
		.name		= "VUSB_1.1V",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo3_consumer),
	.consumer_supplies	= ldo3_consumer,
};

#if  defined(CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_HAWK_BOARD)	|| defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined (CONFIG_S5PC110_DEMPSEY_BOARD) // mr work
static struct regulator_init_data aries_ldo4_data = {
	.constraints	= {
		.name		= "VADC_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
			.enabled = 0,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo4_consumer),
	.consumer_supplies	= ldo4_consumer,	
};
#else
static struct regulator_init_data aries_ldo4_data = {
	.constraints	= {
		.name		= "VADC_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
};
#endif
static struct regulator_init_data aries_ldo5_data = {
	.constraints	= {
		.name		= "VTF_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo5_consumer),
	.consumer_supplies	= ldo5_consumer,
};

#if defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined(CONFIG_S5PC110_HAWK_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD)
static struct regulator_init_data aries_ldo6_data = {
	.constraints	= {
		.name		= "CP_RTC_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo6_consumer),
	.consumer_supplies	= ldo6_consumer,
};

#endif



static struct regulator_init_data aries_ldo7_data = {
	.constraints	= {
		.name		= "VLCD_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo7_consumer),
	.consumer_supplies	= ldo7_consumer,
};

static struct regulator_init_data aries_ldo8_data = {
	.constraints	= {
		.name		= "VUSB_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo8_consumer),
	.consumer_supplies	= ldo8_consumer,
};

static struct regulator_init_data aries_ldo9_data = {
	.constraints	= {
		.name		= "VCC_2.8V_PDA",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
	},
};

//#if defined(CONFIG_VIDEO_S5K5CCGX) || defined(CONFIG_VIDEO_SR030PC30)
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_SIDEKICK_BOARD)
static struct regulator_init_data aries_ldo11_data = {
	.constraints	= {
		.name		= "CAM_VGA_AF_2.8V",
		.min_uV 	= 2800000,
		.max_uV 	= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo11_consumer),
	.consumer_supplies	= ldo11_consumer,
};
#elif defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) /*namarta*/
static struct regulator_init_data aries_ldo11_data = {
	.constraints	= {
		.name		= "CAM_AF_3.0V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo11_consumer),
	.consumer_supplies	= ldo11_consumer,
};
#else
static struct regulator_init_data aries_ldo11_data = {
	.constraints	= {
		.name		= "CAM_AF_3.0V",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo11_consumer),
	.consumer_supplies	= ldo11_consumer,
};
#endif

//#if defined(CONFIG_VIDEO_S5K5CCGX) || defined(CONFIG_VIDEO_SR030PC30)
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_SIDEKICK_BOARD)
static struct regulator_init_data aries_ldo12_data = {
	.constraints	= {
		.name		= "CAM_VGA_AVDD_2.8V",
		.min_uV 	= 2800000,
		.max_uV 	= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo12_consumer),
	.consumer_supplies	= ldo12_consumer,
};
#elif  defined(CONFIG_S5PC110_DEMPSEY_BOARD)
static struct regulator_init_data aries_ldo12_data = {
	.constraints	= {
		.name		= "VMIPI1.8V",
		.min_uV 	= 1800000,
		.max_uV 	= 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo12_consumer),
	.consumer_supplies	= ldo12_consumer,
};
#else
static struct regulator_init_data aries_ldo12_data = {
	.constraints	= {
		.name		= "CAM_SENSOR_CORE_1.2V",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo12_consumer),
	.consumer_supplies	= ldo12_consumer,
};
#endif

#if defined (CONFIG_OPTICAL_TAOS_TRITON)
static struct regulator_init_data aries_ldo13_data = {
	.constraints	= {
		.name		= "VALS_3.0V",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo13_consumer),
	.consumer_supplies	= ldo13_consumer,
};
#elif defined (CONFIG_S5PC110_DEMPSEY_BOARD) 
static struct regulator_init_data aries_ldo13_data = {
	.constraints	= {
		.name		= "TOUCH_2.8V",
		.min_uV		= 3200000,
		.max_uV		= 3200000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.disabled = 0,
			.enabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo13_consumer),
	.consumer_supplies	= ldo13_consumer,
};


#else
static struct regulator_init_data aries_ldo13_data = {
	.constraints	= {
		.name		= "VGA_VDDIO_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo13_consumer),
	.consumer_supplies	= ldo13_consumer,
};
#endif


#if defined (CONFIG_S5PC110_HAWK_BOARD)/* nat */
static struct regulator_init_data aries_ldo14_data = {
	.constraints	= {
		.name		= "KEY_LED_1.8V",
		.min_uV		= 1200000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo14_consumer),
	.consumer_supplies	= ldo14_consumer,
};
#elif defined (CONFIG_S5PC110_DEMPSEY_BOARD) 			
static struct regulator_init_data aries_ldo14_data = {
	.constraints	= {
		.name		= "TSP_VDD_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo14_consumer),
	.consumer_supplies	= ldo14_consumer,
};

#else
static struct regulator_init_data aries_ldo14_data = {
	.constraints	= {
		.name		= "VGA_DVDD_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo14_consumer),
	.consumer_supplies	= ldo14_consumer,
};
#endif


//#if defined(CONFIG_VIDEO_S5K5CCGX) || defined(CONFIG_VIDEO_SR030PC30)
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_SIDEKICK_BOARD)
static struct regulator_init_data aries_ldo15_data = {
	.constraints	= {
		.name		= "VGA_CORE_1.8V",
		.min_uV 	= 1800000,
		.max_uV 	= 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo15_consumer),
	.consumer_supplies	= ldo15_consumer,
};
#elif defined (CONFIG_S5PC110_DEMPSEY_BOARD) 			
static struct regulator_init_data aries_ldo15_data = {
	.constraints	= {
		.name		= "TSP_AVDD_3.3V",
		.min_uV 	= 3300000,
		.max_uV 	= 3300000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo15_consumer),
	.consumer_supplies	= ldo15_consumer,
};
#else
static struct regulator_init_data aries_ldo15_data = {
	.constraints	= {
		.name		= "CAM_ISP_HOST_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo15_consumer),
	.consumer_supplies	= ldo15_consumer,
};
#endif

//#if defined(CONFIG_VIDEO_S5K5CCGX) || defined(CONFIG_VIDEO_SR030PC30)
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_SIDEKICK_BOARD)
static struct regulator_init_data aries_ldo16_data = {
	.constraints	= {
		.name		= "CAM_VGA_VDDIO_2.8V",
		.min_uV 	= 2800000,
		.max_uV 	= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo16_consumer),
	.consumer_supplies	= ldo16_consumer,
};
#elif defined (CONFIG_S5PC110_KEPLER_BOARD)	|| defined (CONFIG_S5PC110_DEMPSEY_BOARD)	
static struct regulator_init_data aries_ldo16_data = {
	.constraints	= {
		.name		= "VGA_AVDD_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo16_consumer),
	.consumer_supplies	= ldo16_consumer,
};
#else
static struct regulator_init_data aries_ldo16_data = {
	.constraints	= {
		.name		= "VGA_AVDD_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo16_consumer),
	.consumer_supplies	= ldo16_consumer,
};
#endif

static struct regulator_init_data aries_ldo17_data = {
	.constraints	= {
		.name		= "VCC_3.0V_LCD",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo17_consumer),
	.consumer_supplies	= ldo17_consumer,
};

static struct regulator_init_data aries_buck1_data = {
	.constraints	= {
		.name		= "VDD_ARM",
		.min_uV		= 750000,
		.max_uV		= 1500000,
		.apply_uV	= 1,
		.initial_state    = PM_SUSPEND_MEM,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.uV	= 1250000,
			.mode	= REGULATOR_MODE_NORMAL,
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck1_consumer),
	.consumer_supplies	= buck1_consumer,
};

static struct regulator_init_data aries_buck2_data = {
	.constraints	= {
		.name		= "VDD_INT",
		.min_uV		= 750000,
		.max_uV		= 1500000,
		.apply_uV	= 1,
		.initial_state    = PM_SUSPEND_MEM,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.uV	= 1100000,
			.mode	= REGULATOR_MODE_NORMAL,
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck2_consumer),
	.consumer_supplies	= buck2_consumer,
};

static struct regulator_init_data aries_buck3_data = {
	.constraints	= {
		.name		= "VCC_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
	},
};

//#if defined(CONFIG_VIDEO_S5K5CCGX) || defined(CONFIG_VIDEO_SR030PC30)
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_SIDEKICK_BOARD)
static struct regulator_init_data aries_buck4_data = {
	.constraints	= {
		.name		= "CAM_CORE_1.2V",
		.min_uV 	= 1200000,
		.max_uV 	= 1200000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck4_consumer),
	.consumer_supplies	= buck4_consumer,
};
#else
static struct regulator_init_data aries_buck4_data = {
	.constraints	= {
		.name		= "CAM_ISP_CORE_1.2V",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck4_consumer),
	.consumer_supplies	= buck4_consumer,
};
#endif

static struct max8998_regulator_data aries_regulators[] = {
	{ MAX8998_LDO2,  &aries_ldo2_data },
	{ MAX8998_LDO3,  &aries_ldo3_data },
	{ MAX8998_LDO4,  &aries_ldo4_data },
	{ MAX8998_LDO5,  &aries_ldo5_data },
#if defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined(CONFIG_S5PC110_HAWK_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD)
	{ MAX8998_LDO6,  &aries_ldo6_data },
#endif	
	{ MAX8998_LDO7,  &aries_ldo7_data },
	{ MAX8998_LDO8,  &aries_ldo8_data },
	{ MAX8998_LDO9,  &aries_ldo9_data },
	{ MAX8998_LDO11, &aries_ldo11_data },
	{ MAX8998_LDO12, &aries_ldo12_data },
	{ MAX8998_LDO13, &aries_ldo13_data },
	{ MAX8998_LDO14, &aries_ldo14_data },
	{ MAX8998_LDO15, &aries_ldo15_data },
	{ MAX8998_LDO16, &aries_ldo16_data },
	{ MAX8998_LDO17, &aries_ldo17_data },
	{ MAX8998_BUCK1, &aries_buck1_data },
	{ MAX8998_BUCK2, &aries_buck2_data },
	{ MAX8998_BUCK3, &aries_buck3_data },
	{ MAX8998_BUCK4, &aries_buck4_data },
};


#if defined (CONFIG_S5PC110_KEPLER_BOARD)
static struct max8998_adc_table_data temper_table[] =  {
	/* ADC, Temperature (C) */
	{ 206,		700	},		
	{ 220,		690	},	
	{ 234,		680	},		
	{ 248,		670	},	
	{ 262,		660	},	
	{ 276,		650	},	
	{ 290,		640	},	
	{ 304,		630	},
	{ 314,		620	},	
	{ 323,		610	},
	{ 337,		600	},
	{ 351,		590	},
	{ 364,		580	},
	{ 379,		570	},
	{ 395,		560	},
	{ 408,		550	},
	{ 423,		540	},	
	{ 438,		530	},
	{ 453,		520	},
	{ 465,		510	},
	{ 478,		500	},
	{ 495,		490	},
	{ 513,		480	},
	{ 528,		470	},
	{ 544,		460	},
	{ 564,		450	},
	{ 584,		440	},
	{ 602,		430	},
	{ 621,		420	},
	{ 643,		410	},
	{ 665,		400	},
	{ 682,		390	},
	{ 702,		380	},
	{ 729,		370	},
	{ 752,		360	},
	{ 775,		350	},
	{ 798,		340	},
	{ 821,		330	},	
	{ 844,		320	},
	{ 867,		310	},
	{ 890,		300	},
	{ 913,		290	},
	{ 936,		280	},
	{ 959,		270	},
	{ 982,		260	},
	{ 1005,		250	},
	{ 1028,		240	},
	{ 1051,		230	},
	{ 1074,		220	},
	{ 1097,		210	},
	{ 1120,		200	},
	{ 1143,		190	},
	{ 1166,		180	},
	{ 1189,		170	},
	{ 1212,		160	},
	{ 1235,		150	},
	{ 1258,		140	},
	{ 1281,		130	},
	{ 1304,		120	},
	{ 1327,		110	},
	{ 1350,		100	},	
	{ 1373,		90	},
	{ 1396,		80	},
	{ 1419,		70	},
	{ 1442,		60	},
	{ 1465,		50	},	
	{ 1484,		40	}, 
	{ 1504,		30	}, 
	{ 1526,		20	}, 
	{ 1543,		10	}, // +10
	{ 1567,		0	}, // 10
	{ 1569,		-10	}, 
	{ 1592,		-20	}, 
	{ 1613,		-30	}, 
	{ 1633,		-40	}, 
	{ 1653,		-50	}, 
	{ 1654,		-60	}, 	
	{ 1671,		-70	}, 
	{ 1691,		-80	}, 
	{ 1711,		-90	}, 
	{ 1731,		-100}, // 0
};

#elif defined (CONFIG_S5PC110_HAWK_BOARD)
static struct max8998_adc_table_data temper_table[] =  {
	
	/* ADC, Temperature (C) */
	{ 206,		700	},
	{ 220,		690	},
	{ 230,		680	},
	{ 264,		670	},
	{ 274,		660	},
	{ 284,		650	},
	{ 294 ,		640	},
	{ 304,		630	},
	{ 315,		620	},
	{ 328,		610	},
	{ 338,		600	},  	//10
	{ 348,		590	},
	{ 360,		580	},
	{ 370,		570	},
	{ 382,		560	},
	{ 395,		550	},
	{ 407,		540	},
	{ 420,		530	},
	{ 433,		520	},
	{ 448,		510	},
	{ 463,		500	},	//20	
	{ 478,		490	},
	{ 495,		480	},
	{ 510,		470	},
	{ 530,		460	},
	{ 546,		450	},
	{ 562,		440	},
	{ 582,		430	},
	{ 600,		420	},
	{ 618,		410	},
	{ 635,		400	},	//30
	{ 655,		390	},
	{ 675,		380	},
	{ 690,		370	},
	{ 708,		360	},
	{ 728,		350	},
	{ 750,		340	},
	{ 772,		330	},
	{ 794,		320	},
	{ 816,		310	},
	{ 841,		300	},	//40
	{ 865,		290	},
	{ 889,		280	},
	{ 913,		270	},
	{ 937,		260	},
	{ 963,		250	},
	{ 987,		240	},
	{ 1011,		230	},
	{ 1035,		220	},
	{ 1059,		210	},
	{ 1086,		200	},	//50
	{ 1110,		190	},
	{ 1134,		180	},
	{ 1158,		170	},
	{ 1182,		160	},
	{ 1206,		150	},
	{ 1230,		140	},
	{ 1254,		130	},
	{ 1278,		120	},
	{ 1302,		110	},
	{ 1326,		100	},	//60
	{ 1346,		90	},
	{ 1366,		80	},
	{ 1386,		70	},
	{ 1406,		60	},
	{ 1420,		50	},
	{ 1450,		40	},
	{ 1470,		30	},
	{ 1495,		20	},
	{ 1510,		10	},
	{ 1530,		0	},	//70
	{ 1547,		-10	},
	{ 1565,		-20	},
	{ 1583,		-30	},
	{ 1610,		-40	},
	{ 1635,		-50	},
	{ 1649,		-60	},
	{ 1663,		-70	},
	{ 1677,		-80 },
	{ 1691,		-90 },
	{ 1705,		-100},	//80
	{ 1722,		-110},
	{ 1739,		-120},
	{ 1756,		-130},
	{ 1773,		-140},
	{ 1790,		-150},
	{ 1804,		-160},
	{ 1818,		-170},
	{ 1832,		-180},
	{ 1846,		-190},
	{ 1859,		-200},	//90	
};

#elif defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD)
static struct max8998_adc_table_data temper_table[] =  {
	/* ADC, Temperature (C)  // froyo */
	{ 206,		700	},
	{ 220,		690	},
	{ 240,		680	}, 
	{ 254,		670	},
	{ 265,		660	},
	{ 279,		650	},
	{ 290,		640	}, // [[junghyunseok edit temperature table 20100531
	{ 296,		630	},
	{ 303,		620	},
	{ 311,		610	},
	{ 325,		600	},
	{ 334,		590	},
	{ 347,		580	},
	{ 360,		570	},
    { 375,		560	},
	{ 396,		550	},
	{ 405,		540	},
	{ 416,		530	},
	{ 431,		520	},
	{ 440,		510	}, // [[junghyunseok edit temperature table 20100531	
	{ 461,		500	},	
	{ 478,		490	},	
	{ 495,		480	},
	{ 512,		470	},
	{ 529,		460	},
	{ 548,		450	},
	{ 565,		440	},
	{ 580,		430	}, // [[junghyunseok edit temperature table 20100531
	{ 599,		420	},
	{ 616,		410	},
	{ 636,		400	},
	{ 654,		390	},
	{ 672,		380	},
	{ 690,		370	},
	{ 708,		360	},
	{ 728,		350	},
	{ 750,		340	},
	{ 772,		330	},
	{ 794,		320	},
	{ 816,		310	},
	{ 841,		300	},
	{ 865,		290	},
	{ 889,		280	},
	{ 913,		270	},
	{ 937,		260	},
	{ 963,		250	},
	{ 987,		240	},
	{ 1011,		230	},
	{ 1035,		220	},
	{ 1059,		210	},
	{ 1086,		200	},
	{ 1110,		190	},
	{ 1134,		180	},
	{ 1158,		170	},
	{ 1182,		160	},
	{ 1206,		150	},
    { 1230,		140	},
	{ 1254,		130	},
	{ 1278,		120	},
	{ 1302,		110	},
	{ 1326,		100	},	
	{ 1346,		90	},	
	{ 1366,		80	},	
	{ 1386,		70	},
	{ 1406,		60	},
	{ 1420,		50	}, 
	{ 1430,		40	}, 
	{ 1450,		30	}, 
	{ 1460,		20	},
    { 1470,		10	},
    { 1480,		0	}, // 20
	{ 1490,		-10	},
	{ 1500,		-20	},
	{ 1510,		-30	},
	{ 1550,		-40	},
	{ 1635,		-50	},
	{ 1649,		-60	},
	{ 1663,		-70	},
	{ 1677,		-80 }, 	
	{ 1691,		-90 }, 
	{ 1705,		-100}, // 10
	{ 1722,		-110}, 
	{ 1739,		-120},
	{ 1756,		-130},
	{ 1773,		-140},
	{ 1790,		-150},
	{ 1804,		-160},
	{ 1818,		-170},
	{ 1832,		-180},
	{ 1846,		-190},
    { 1859,		-200},
};
#elif defined(CONFIG_S5PC110_DEMPSEY_BOARD)		// mr work
static struct max8998_adc_table_data temper_table[] =  {
	/* ADC, Temperature (C) */
	{ 217, 		700 	},
	{ 228, 		690 	},
	{ 239, 		680 	},
	{ 250, 		670 	},
	{ 263, 		660 	},
	{ 275, 		650 	},
	{ 284, 		640 	},
	{ 294, 		630 	},
	{ 305, 		620 	},
	{ 317, 		610 	},
	{ 329, 		600 	},
	{ 341, 		590 	},
	{ 353, 		580 	},
	{ 365, 		570 	},
	{ 378, 		560 	},
	{ 393, 		550 	},
	{ 405, 		540 	},
	{ 419, 		530 	},
	{ 433, 		520 	},	
	{ 447, 		510 	},
	{ 461, 		500 	},
	{ 478, 		490 	},
	{ 497, 		480 	},
	{ 517, 		470 	},
	{ 533, 		460 	},
	{ 549, 		450 	},
	{ 585, 		430 	},
	{ 604, 		420 	},
	{ 619, 		410 	},
	{ 641, 		400 	},
	{ 663, 		390 	},
	{ 681, 		380 	},
	{ 704,		370 	},
	{ 725, 		360 	},
	{ 746, 		350 	},
	{ 767, 		340 	},
	{ 788, 		330 	},
	{ 809, 		320 	},
	{ 830, 		310 	},
	{ 851, 		300 	},
	{ 880, 		290 	},
	{ 904, 		280 	},
	{ 928, 		270 	},
	{ 952, 		260 	},
	{ 976, 		250 	},
	{ 1000, 	240 	},	
	{ 1024, 	230 	},
	{ 1048, 	220 	},
	{ 1072, 	210 	},
	{ 1096, 	200 	},
	{ 1117, 	190 	},
	{ 1139, 	180 	},
	{ 1161, 	170 	},	
	{ 1183, 	160 	},
	{ 1205, 	150 	},
	{ 1227, 	140 	},
	{ 1249, 	130 	},
	{ 1271, 	120 	},
	{ 1293, 	110 	},
	{ 1315, 	100 	},
	{ 1339, 	90 	},
	{ 1364, 	80 	},
	{ 1389, 	70 	},
	{ 1414, 	60 	},
	{ 1439, 	50 	},
	{ 1462, 	40 	},
	{ 1485, 	30 	},
	{ 1504, 	20 	},	
	{ 1525, 	10 	}, // +10
	{ 1547, 	0 	}, // 10 // 0 A
	{ 1562, 	-10 	},
	{ 1585, 	-20 	},
	{ 1595, 	-30 	},
	{ 1617, 	-40 	},
	{ 1621, 	-50 	},
	{ 1641,		-60 	},
	{ 1652, 	-70 	},
	{ 1667, 	-80 	},
	{ 1708, 	-100 	}, //-10 A
};
#else
static struct max8998_adc_table_data temper_table[] =  {
	{  264,  650 },
	{  275,  640 },
	{  286,  630 },
	{  293,  620 },
	{  299,  610 },
	{  306,  600 },
#if !defined(CONFIG_ARIES_NTT)
	{  324,  590 },
	{  341,  580 },
	{  354,  570 },
	{  368,  560 },
#else
	{  310,  590 },
	{  315,  580 },
	{  320,  570 },
	{  324,  560 },
#endif
	{  381,  550 },
	{  396,  540 },
	{  411,  530 },
	{  427,  520 },
	{  442,  510 },
	{  457,  500 },
	{  472,  490 },
	{  487,  480 },
	{  503,  470 },
	{  518,  460 },
	{  533,  450 },
	{  554,  440 },
	{  574,  430 },
	{  595,  420 },
	{  615,  410 },
	{  636,  400 },
	{  656,  390 },
	{  677,  380 },
	{  697,  370 },
	{  718,  360 },
	{  738,  350 },
	{  761,  340 },
	{  784,  330 },
	{  806,  320 },
	{  829,  310 },
	{  852,  300 },
	{  875,  290 },
	{  898,  280 },
	{  920,  270 },
	{  943,  260 },
	{  966,  250 },
	{  990,  240 },
	{ 1013,  230 },
	{ 1037,  220 },
	{ 1060,  210 },
	{ 1084,  200 },
	{ 1108,  190 },
	{ 1131,  180 },
	{ 1155,  170 },
	{ 1178,  160 },
	{ 1202,  150 },
	{ 1226,  140 },
	{ 1251,  130 },
	{ 1275,  120 },
	{ 1299,  110 },
	{ 1324,  100 },
	{ 1348,   90 },
	{ 1372,   80 },
	{ 1396,   70 },
	{ 1421,   60 },
	{ 1445,   50 },
	{ 1468,   40 },
	{ 1491,   30 },
	{ 1513,   20 },
#if !defined(CONFIG_ARIES_NTT)
	{ 1536,   10 },
	{ 1559,    0 },
	{ 1577,  -10 },
	{ 1596,  -20 },
#else
	{ 1518,   10 },
	{ 1524,    0 },
	{ 1544,  -10 },
	{ 1570,  -20 },
#endif
	{ 1614,  -30 },
	{ 1619,  -40 },
	{ 1632,  -50 },
	{ 1658,  -60 },
	{ 1667,  -70 }, 
};
#endif
struct max8998_charger_callbacks *charger_callbacks;
static enum cable_type_t set_cable_status;

static void max8998_charger_register_callbacks(
		struct max8998_charger_callbacks *ptr)
{
	charger_callbacks = ptr;
	/* if there was a cable status change before the charger was
	ready, send this now */
	if ((set_cable_status != 0) && charger_callbacks && charger_callbacks->set_cable)
		charger_callbacks->set_cable(charger_callbacks, set_cable_status);
}

static struct max8998_charger_data aries_charger = {
	.register_callbacks	= &max8998_charger_register_callbacks,
	.adc_table		= temper_table,
	.adc_array_size		= ARRAY_SIZE(temper_table),
};

static struct max8998_platform_data max8998_pdata = {
	.num_regulators		= ARRAY_SIZE(aries_regulators),
	.regulators		= aries_regulators,
	.charger		= &aries_charger,
	.buck1_set1		= GPIO_BUCK_1_EN_A,
	.buck1_set2		= GPIO_BUCK_1_EN_B,
	.buck2_set3		= GPIO_BUCK_2_EN,
#if defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
	.buck1_voltage_set	= { 1325000, 1250000, 1100000, 1000000 },
	.buck2_voltage_set	= { 1100000, 1000000 },	
#elif defined (CONFIG_S5PC110_HAWK_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) 
	.buck1_voltage_set	= { 1300000, 1225000, 1075000, 975000 },
	.buck2_voltage_set	= { 1125000, 1025000 },
#else
	.buck1_voltage_set	= { 1275000, 1200000, 1050000, 950000 },
	.buck2_voltage_set	= { 1100000, 1000000 },
#endif	
};

struct platform_device sec_device_dpram = {
	.name	= "dpram-device",
	.id	= -1,
};

#ifdef CONFIG_FB_S3C_TL2796 
static void tl2796_cfg_gpio(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++)
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_SFN(2));

	for (i = 0; i < 8; i++)
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_SFN(2));

	for (i = 0; i < 8; i++)
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_SFN(2));

	for (i = 0; i < 4; i++)
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_SFN(2));

	/* mDNIe SEL: why we shall write 0x2 ? */
#ifdef CONFIG_FB_S3C_MDNIE
	writel(0x1, S5P_MDNIE_SEL);
#else
	writel(0x2, S5P_MDNIE_SEL);
#endif

	s3c_gpio_setpull(GPIO_OLED_DET, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_OLED_ID, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_DIC_ID, S3C_GPIO_PULL_NONE);

#if defined (CONFIG_S5PC110_HAWK_BOARD)/* nat */
	/* DISPLAY_SDO*/
	s3c_gpio_cfgpin(GPIO_DISPLAY_SDO, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_DISPLAY_SDO, S3C_GPIO_PULL_UP);	
#endif
#if defined (CONFIG_S5PC110_HAWK_BOARD)
	gpio_set_value(GPIO_DISPLAY_CS, 1);
#endif
}

void lcd_cfg_gpio_early_suspend(void)
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_OUTPUT);
		gpio_set_value(S5PV210_GPF0(i), 0);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_OUTPUT);
		gpio_set_value(S5PV210_GPF1(i), 0);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_OUTPUT);
		gpio_set_value(S5PV210_GPF2(i), 0);
	}

	for (i = 0; i < 4; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_OUTPUT);
		gpio_set_value(S5PV210_GPF3(i), 0);
	}

	gpio_set_value(GPIO_MLCD_RST, 0);

	gpio_set_value(GPIO_DISPLAY_CS, 0);
	gpio_set_value(GPIO_DISPLAY_CLK, 0);
	gpio_set_value(GPIO_DISPLAY_SI, 0);

	s3c_gpio_setpull(GPIO_OLED_DET, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(GPIO_OLED_ID, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(GPIO_DIC_ID, S3C_GPIO_PULL_DOWN);
}
EXPORT_SYMBOL(lcd_cfg_gpio_early_suspend);

void lcd_cfg_gpio_late_resume(void)
{

}
EXPORT_SYMBOL(lcd_cfg_gpio_late_resume);

static int tl2796_reset_lcd(struct platform_device *pdev)
{
	int err;

	err = gpio_request(GPIO_MLCD_RST, "MLCD_RST");
	if (err) {
		printk(KERN_ERR "failed to request MP0(5) for "
				"lcd reset control\n");
		return err;
	}

	gpio_direction_output(GPIO_MLCD_RST, 1);
	msleep(10);

	gpio_set_value(GPIO_MLCD_RST, 0);
	msleep(10);

	gpio_set_value(GPIO_MLCD_RST, 1);
	msleep(10);

	gpio_free(GPIO_MLCD_RST);

	return 0;
}

static int tl2796_backlight_on(struct platform_device *pdev)
{
	return 0;
}

static struct s3c_platform_fb tl2796_data __initdata = {
	.hw_ver		= 0x62,
	.clk_name	= "sclk_fimd",
	.nr_wins	= 5,
	.default_win	= CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap		= FB_SWAP_HWORD | FB_SWAP_WORD,

	.lcd = &s6e63m0,
	.cfg_gpio	= tl2796_cfg_gpio,
	.backlight_on	= tl2796_backlight_on,
#if defined (CONFIG_S5PC110_HAWK_BOARD) /* nat */
	.reset_lcd = NULL,
#else
	.reset_lcd	= tl2796_reset_lcd,
#endif
};

#define LCD_BUS_NUM	3
#define DISPLAY_CS	S5PV210_MP01(1)
#define SUB_DISPLAY_CS	S5PV210_MP01(2)
#define DISPLAY_CLK	S5PV210_MP04(1)
#define DISPLAY_SI	S5PV210_MP04(3)
#if defined (CONFIG_S5PC110_HAWK_BOARD) /* nat */
#define DISPLAY_SDO      S5PV210_MP04(2)
#endif



static struct spi_board_info spi_board_info[] __initdata = {
	{
		.modalias	= "tl2796",
		.platform_data	= &aries_panel_data,
		.max_speed_hz	= 1200000,
		.bus_num	= LCD_BUS_NUM,
		.chip_select	= 0,
		.mode		= SPI_MODE_3,
		.controller_data = (void *)DISPLAY_CS,
	},
};

static struct spi_gpio_platform_data tl2796_spi_gpio_data = {
	.sck	= DISPLAY_CLK,
	.mosi	= DISPLAY_SI,
#if defined (CONFIG_S5PC110_HAWK_BOARD) /* nat */
        .miso   = DISPLAY_SDO,
#else
	.miso	= -1,
#endif
	.num_chipselect = 2,
};

static struct platform_device s3c_device_spi_gpio = {
	.name	= "spi_gpio",
	.id	= LCD_BUS_NUM,
	.dev	= {
		.parent		= &s3c_device_fb.dev,
		.platform_data	= &tl2796_spi_gpio_data,
	},
};
#endif
#if defined(CONFIG_S5PC110_DEMPSEY_BOARD)
#if defined (CONFIG_FB_S3C_LDI)

void lcd_cfg_gpio_early_suspend(void)
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_OUTPUT);
		gpio_set_value(S5PV210_GPF0(i), 0);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_OUTPUT);
		gpio_set_value(S5PV210_GPF1(i), 0);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_OUTPUT);
		gpio_set_value(S5PV210_GPF2(i), 0);
	}

	for (i = 0; i < 4; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_OUTPUT);
		gpio_set_value(S5PV210_GPF3(i), 0);
	}

	gpio_set_value(GPIO_MLCD_RST, 0);

	gpio_set_value(GPIO_DISPLAY_CS, 1);
	gpio_set_value(GPIO_DISPLAY_CLK, 1);
	gpio_set_value(GPIO_DISPLAY_SI, 1);

	s3c_gpio_setpull(GPIO_OLED_DET, S3C_GPIO_PULL_DOWN);
	s3c_gpio_setpull(GPIO_OLED_ID, S3C_GPIO_PULL_DOWN);
	//s3c_gpio_setpull(GPIO_DIC_ID, S3C_GPIO_PULL_DOWN);
}

EXPORT_SYMBOL(lcd_cfg_gpio_early_suspend);

void lcd_cfg_gpio_late_resume(void)
{
	printk("[%s]\n", __func__);

}
EXPORT_SYMBOL(lcd_cfg_gpio_late_resume);

static int lcd_cfg_gpio(void)
{
	int i;

	for (i = 0; i < 8; i++)
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_SFN(2));

	for (i = 0; i < 8; i++)
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_SFN(2));

	for (i = 0; i < 8; i++)
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_SFN(2));

	for (i = 0; i < 4; i++)
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_SFN(2));

	/* mDNIe SEL: why we shall write 0x2 ? */
#ifdef CONFIG_FB_S3C_MDNIE
	writel(0x1, S5P_MDNIE_SEL);
#else
	writel(0x2, S5P_MDNIE_SEL);
#endif

	s3c_gpio_setpull(GPIO_OLED_DET, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_OLED_ID, S3C_GPIO_PULL_NONE);
//	s3c_gpio_setpull(GPIO_DIC_ID, S3C_GPIO_PULL_NONE);

}


static int ldi_lcd_power_on(struct lcd_device *ld, int enable)
{
	return 1;
}

static int lcd_power_on(struct platform_device *pdev)
{

#if 0
	struct regulator *regulator;

	if (ld == NULL) {
		printk(KERN_ERR "lcd device object is NULL.\n");
		return 0;
	}

	if (enable) {
		regulator = regulator_get(NULL, "vlcd_3.0v");
		if (IS_ERR(regulator))
			return 0;
		regulator_enable(regulator);
		regulator_put(regulator);
	} else {
		regulator = regulator_get(NULL, "vlcd_3.0v");
		if (IS_ERR(regulator))
			return 0;
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);
	}
	
#endif

	return 1;
}

static int ldi_reset_lcd(struct lcd_device *ld)
{
	return 1;
}

static int reset_lcd()
{
#if 0
	static unsigned int first = 1;
	int reset_gpio = -1;

	reset_gpio = S5PV310_GPY4(5);

	if (first) {
		gpio_request(reset_gpio, "MLCD_RST");
		first = 0;
	}

	mdelay(10);
	gpio_direction_output(reset_gpio, 0);
	mdelay(10);
	gpio_direction_output(reset_gpio, 1);
#endif


	int err;

	static unsigned int first = 1;

	//  Ver1 & Ver2 universal board kyoungheon
	if (first) {
		err = gpio_request(GPIO_MLCD_RST, "MLCD_RST");
		if (err) {
			printk(KERN_ERR "failed to request GPIO_MLCD_RST for "
					"lcd reset control\n");
			return err;
		}
	first = 0;
	}
	//gpio_direction_output(GPIO_MLCD_RST, 1);
	msleep(10);

	gpio_set_value(GPIO_MLCD_RST, 0);
	msleep(10);

	gpio_set_value(GPIO_MLCD_RST, 1);
	msleep(20);
	
	gpio_free(GPIO_MLCD_RST);

	return 1;

}

static struct lcd_platform_data ldi_platform_data = {
	.reset			= ldi_reset_lcd,
	.power_on		= ldi_lcd_power_on,
	/* it indicates whether lcd panel is enabled from u-boot. */
	.lcd_enabled		= 1,
	.reset_delay		= 20,	/* 20ms */
	.power_on_delay		= 300,	/* 300ms */
	.power_off_delay	= 300,	/* 120ms */
};

#define LCD_BUS_NUM	3

static struct spi_board_info spi_board_info[] __initdata = {
	{
		.modalias	= "amoled display",
                .platform_data  = &ldi_platform_data,
		.max_speed_hz	= 1200000,
		.bus_num	= LCD_BUS_NUM,
		.chip_select	= 0,
		.mode		= SPI_MODE_3,
		.controller_data = (void *)GPIO_DISPLAY_CS,
	},
};

static struct spi_gpio_platform_data lcd_spi_gpio_data = {
	.sck			= GPIO_DISPLAY_CLK,
	.mosi			= GPIO_DISPLAY_SI,
	//.miso			= SPI_GPIO_NO_MISO,

	.miso			= -1,
	.num_chipselect		= 1,
	//.num_chipselect		= 2,
};

static struct platform_device ldi_spi_gpio = {
	.name			= "spi_gpio",
	.id			= LCD_BUS_NUM,
	.dev			= {
		.parent		= &s3c_device_fb.dev,
		.platform_data	= &lcd_spi_gpio_data,
	},
};

static struct s3cfb_lcd ldi_info = {
	.width = 480,
	.height = 800,
	.p_width = 52,
	.p_height = 86,
	.bpp = 24,
	.freq = 60,
	
	.timing = {
		.h_fp = 16,
		.h_bp = 16,
		.h_sw = 2,
		.v_fp = 10,
		.v_fpe = 1,
		.v_bp = 6,
		.v_bpe = 1,
		.v_sw = 2,
	},

	.polarity = {
		.rise_vclk = 1,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 1,
	},
};

static struct s3c_platform_fb fb_platform_data __initdata = {

#if 0
	.hw_ver			= 0x70,
	.clk_name		= "fimd",
	.nr_wins		= 5,
#ifdef CONFIG_FB_S3C_DEFAULT_WINDOW
	.default_win		= CONFIG_FB_S3C_DEFAULT_WINDOW,
#else
	.default_win		= 0,
#endif
	.swap			= FB_SWAP_HWORD | FB_SWAP_WORD,

#endif

	.hw_ver = 0x62,
        .clk_name = "sclk_fimd",
        .nr_wins = 5,
        .default_win = CONFIG_FB_S3C_DEFAULT_WINDOW,
        .swap = FB_SWAP_HWORD | FB_SWAP_WORD,


	.lcd = &ldi_info, 
	.cfg_gpio = lcd_cfg_gpio,
        .backlight_on = lcd_power_on,
        .reset_lcd = reset_lcd,

};


#define LCD_ON_FROM_BOOTLOADER 1

static void __init ldi_fb_init(void)
{
	spi_register_board_info(spi_board_info,
		ARRAY_SIZE(spi_board_info));

#ifndef LCD_ON_FROM_BOOTLOADER
	lcd_cfg_gpio();
#endif
	s3cfb_set_platdata(&fb_platform_data);
}

#endif
#endif
static struct i2c_gpio_platform_data i2c4_platdata = {
	.sda_pin		= GPIO_AP_SDA_18V,
	.scl_pin		= GPIO_AP_SCL_18V,
	.udelay			= 2, /* 250KHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

static struct platform_device s3c_device_i2c4 = {
	.name			= "i2c-gpio",
	.id			= 4,
	.dev.platform_data	= &i2c4_platdata,
};
#if !defined(CONFIG_S5PC110_DEMPSEY_BOARD)
static struct i2c_gpio_platform_data i2c5_platdata = {
	.sda_pin		= GPIO_AP_SDA_28V,
	.scl_pin		= GPIO_AP_SCL_28V,
	.udelay			= 2, /* 250KHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};
#else
static  struct  i2c_gpio_platform_data  i2c5_platdata = {
        .sda_pin                = GPIO_SENSE_SDA_28V,
        .scl_pin                = GPIO_SENSE_SCL_28V,
        .udelay                 = 2,    /* 250KHz */
//      .udelay                 = 4,
        .sda_is_open_drain      = 0,
        .scl_is_open_drain      = 0,
        .scl_is_output_only     = 0,
//      .scl_is_output_only     = 1,
};


#endif

static struct platform_device s3c_device_i2c5 = {
	.name			= "i2c-gpio",
	.id			= 5,
	.dev.platform_data	= &i2c5_platdata,
};

static struct i2c_gpio_platform_data i2c6_platdata = {
	.sda_pin		= GPIO_AP_PMIC_SDA,
	.scl_pin		= GPIO_AP_PMIC_SCL,
	.udelay 		= 2, /* 250KHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

static struct platform_device s3c_device_i2c6 = {
	.name			= "i2c-gpio",
	.id			= 6,
	.dev.platform_data	= &i2c6_platdata,
};

static struct i2c_gpio_platform_data i2c7_platdata = {
	.sda_pin		= GPIO_USB_SDA_28V,
	.scl_pin		= GPIO_USB_SCL_28V,
	.udelay 		= 2, /* 250KHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

static struct platform_device s3c_device_i2c7 = {
	.name			= "i2c-gpio",
	.id			= 7,
	.dev.platform_data	= &i2c7_platdata,
};
#if !(defined (CONFIG_S5PC110_HAWK_BOARD) || defined(CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD)  || defined (CONFIG_S5PC110_DEMPSEY_BOARD)) 
#if !defined(CONFIG_ARIES_NTT)
static struct i2c_gpio_platform_data i2c8_platdata = {
	.sda_pin		= GPIO_FM_SDA_28V,
	.scl_pin		= GPIO_FM_SCL_28V,
	.udelay 		= 2, /* 250KHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

static struct platform_device s3c_device_i2c8 = {
	.name			= "i2c-gpio",
	.id			= 8,
	.dev.platform_data	= &i2c8_platdata,
};
#endif
#endif // NAGSM_Android_HQ_KERNEL_MINJEONGKO_20100806 for hwak temp key --

static struct i2c_gpio_platform_data i2c9_platdata = {
	.sda_pin		= FUEL_SDA_18V,
	.scl_pin		= FUEL_SCL_18V,
	.udelay 		= 2, /* 250KHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

static struct platform_device s3c_device_i2c9 = {
	.name			= "i2c-gpio",
	.id			= 9,
	.dev.platform_data	= &i2c9_platdata,
};

static struct i2c_gpio_platform_data i2c10_platdata = {
	.sda_pin		= _3_TOUCH_SDA_28V,
	.scl_pin		= _3_TOUCH_SCL_28V,
	.udelay 		= 0, /* 250KHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

static struct platform_device s3c_device_i2c10 = {
	.name			= "i2c-gpio",
	.id			= 10,
	.dev.platform_data	= &i2c10_platdata,
};

#if defined (CONFIG_OPTICAL_TAOS_TRITON)
static	struct	i2c_gpio_platform_data	i2c11_platdata = {
	.sda_pin		= GPIO_ALS_SDA_18V,
	.scl_pin		= GPIO_ALS_SCL_18V,
	.udelay			= 2,	/* 250KHz */		
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};
#elif defined(CONFIG_OPTICAL_CAPELLA_TRITON)
static struct i2c_gpio_platform_data i2c11_platdata = {
	.sda_pin		= GPIO_PS_ALS_SDA_28V,
	.scl_pin		= GPIO_PS_ALS_SCL_28V,
	.udelay 		= 2, /* 250KHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};
#else
static struct i2c_gpio_platform_data i2c11_platdata = {
	.sda_pin		= GPIO_ALS_SDA_28V,
	.scl_pin		= GPIO_ALS_SCL_28V,
	.udelay 		= 2, /* 250KHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

#endif

static struct platform_device s3c_device_i2c11 = {
	.name			= "i2c-gpio",
	.id			= 11,
	.dev.platform_data	= &i2c11_platdata,
};
#if !defined (CONFIG_S5PC110_DEMPSEY_BOARD)
static struct i2c_gpio_platform_data i2c12_platdata = {
	.sda_pin		= GPIO_MSENSE_SDA_28V,
	.scl_pin		= GPIO_MSENSE_SCL_28V,
	.udelay 		= 0, /* 250KHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};
#else
static struct i2c_gpio_platform_data i2c12_platdata = {
	.sda_pin		= GPIO_MSENSOR_SDA_28V,
	.scl_pin		= GPIO_MSENSOR_SCL_28V,
	.udelay 		= 2, /* 250KHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

#endif

static struct platform_device s3c_device_i2c12 = {
	.name			= "i2c-gpio",
	.id			= 12,
	.dev.platform_data	= &i2c12_platdata,
};

//[ hdlnc_bp_ytkwon : 20100301
static	struct	i2c_gpio_platform_data	i2c13_platdata = {
#if defined(CONFIG_S5PC110_KEPLER_BOARD)|| defined (CONFIG_S5PC110_DEMPSEY_BOARD) 
	.sda_pin		= GPIO_A1026_SDA,
	.scl_pin		= GPIO_A1026_SCL,
#else
	.sda_pin		= -1,
	.scl_pin		= -1,
#endif
	.udelay			= 1,	/* 250KHz */		
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};
static struct platform_device s3c_device_i2c13 = {
	.name				= "i2c-gpio",
	.id					= 13,
	.dev.platform_data	= &i2c13_platdata,
};
//] hdlnc_bp_ytkwon : 20100301

static struct i2c_gpio_platform_data i2c14_platdata = {
	.sda_pin		= NFC_SDA_18V,
	.scl_pin		= NFC_SCL_18V,
	.udelay			= 2,
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

static struct platform_device s3c_device_i2c14 = {
	.name			= "i2c-gpio",
	.id			= 14,
	.dev.platform_data	= &i2c14_platdata,
};

static void touch_keypad_gpio_init(void)
{
	int ret = 0;
#if !defined (CONFIG_S5PC110_DEMPSEY_BOARD) 
	ret = gpio_request(_3_GPIO_TOUCH_EN, "TOUCH_EN");
#endif
	if (ret)
		printk(KERN_ERR "Failed to request gpio touch_en.\n");
}

static void touch_keypad_onoff(int onoff)
{
#if !defined (CONFIG_S5PC110_DEMPSEY_BOARD) 
	gpio_direction_output(_3_GPIO_TOUCH_EN, onoff);
#endif
	if (onoff == TOUCHKEY_OFF)
		msleep(30);
	else
		msleep(25);
}

#if defined (CONFIG_S5PC110_KEPLER_BOARD)
static const int touch_keypad_code[] = {
	KEY_MENU,
	KEY_HOME,
	KEY_BACK,
	KEY_SEARCH,
};
#elif defined (CONFIG_S5PC110_DEMPSEY_BOARD) 
static const int touch_keypad_code[] = {
	KEY_MENU,
	KEY_HOME,
	KEY_BACK,
	KEY_SEARCH,
};

#elif defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD)
static const int touch_keypad_code[] = {
        KEY_MENU,
        KEY_BACK,
        KEY_HOME,
        KEY_SEARCH,
};

#else
static const int touch_keypad_code[] = {
	KEY_MENU,
	KEY_BACK,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_UP,
	KEY_DOWN,
	KEY_CAMERA,
	KEY_SEND,	
};
#endif

#if defined (CONFIG_VIDEO_MHL_V1)
static	struct	i2c_gpio_platform_data	i2c18_platdata = {
	.sda_pin		= GPIO_AP_SDA, 
	.scl_pin		= GPIO_AP_SCL, 
	.udelay			= 5,	/* 250KHz */		
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};
static struct platform_device s3c_device_i2c18 = {
	.name			= "i2c-gpio",
	.id			= 18,
	.dev.platform_data	= &i2c18_platdata,
};
#endif

static struct touchkey_platform_data touchkey_data = {
	.keycode_cnt = ARRAY_SIZE(touch_keypad_code),
	.keycode = touch_keypad_code,
	.touchkey_onoff = touch_keypad_onoff,
};

static struct gpio_event_direct_entry aries_keypad_key_map[] = {
#if defined (CONFIG_S5PC110_HAWK_BOARD) /* 20110207 nat */
{
	.gpio	= S5PV210_GPH3(7),
	.code	= KEY_POWER,
},
{
	.gpio	= S5PV210_GPH3(2),
	.code	= KEY_VOLUMEDOWN,
},
{
	.gpio	= S5PV210_GPH3(1),
	.code	= KEY_VOLUMEUP,
},
{
	.gpio	= S5PV210_GPH3(0),
	.code	= KEY_HOME,
}
#elif defined(CONFIG_S5PC110_KEPLER_BOARD) 
	{
		.gpio	= S5PV210_GPH2(6),
		.code	= KEY_POWER,
	},
	{
		.gpio	= S5PV210_GPH3(1),
		.code	= KEY_VOLUMEDOWN,
	},
	{
		.gpio	= S5PV210_GPH3(2),
		.code	= KEY_VOLUMEUP,
	}

#elif defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD)
// NAGSM_Android_SEL_Kernel_20110422
	{
		.gpio	= S5PV210_GPH2(6),
		.code	= KEY_POWER,
	},
	{
		.gpio	= S5PV210_GPH3(1),
		.code	= KEY_VOLUMEUP,
	},
	{
		.gpio	= S5PV210_GPH3(2),
		.code	= KEY_VOLUMEDOWN,
	}
#elif defined (CONFIG_S5PC110_DEMPSEY_BOARD)
	{
		.gpio	= S5PV210_GPH2(6),
		.code	= KEY_POWER,
	},
	{
		.gpio	= S5PV210_GPH3(1),
		.code	= KEY_VOLUMEUP,
	},
    {
		.gpio	= S5PV210_GPH3(0),
		.code	= KEY_VOLUMEDOWN,
	}
#else
	{
		.gpio	= S5PV210_GPH2(6),
		.code	= KEY_POWER,
	},
	{
		.gpio	= S5PV210_GPH3(1),
		.code	= KEY_VOLUMEDOWN,
	},
	{
		.gpio	= S5PV210_GPH3(2),
		.code	= KEY_VOLUMEUP,
	},
	{
		.gpio	= S5PV210_GPH3(5),
		.code	= KEY_HOME,
	}
#endif
};

static struct gpio_event_input_info aries_keypad_key_info = {
	.info.func = gpio_event_input_func,
	.info.no_suspend = true,
	.debounce_time.tv.nsec = 5 * NSEC_PER_MSEC,
	.type = EV_KEY,
#if 0 // defined(CONFIG_S5PC110_HAWK_BOARD)
	.flags = GPIOEDF_PRINT_KEYS,
#endif
	.keymap = aries_keypad_key_map,
	.keymap_size = ARRAY_SIZE(aries_keypad_key_map)
};

static struct gpio_event_info *aries_input_info[] = {
	&aries_keypad_key_info.info,
};


static struct gpio_event_platform_data aries_input_data = {
	.names = {
		"aries-keypad",
		NULL,
	},
	.info = aries_input_info,
	.info_count = ARRAY_SIZE(aries_input_info),
};

static struct platform_device aries_input_device = {
	.name = GPIO_EVENT_DEV_NAME,
	.id = 0,
	.dev = {
		.platform_data = &aries_input_data,
	},
};

#ifdef CONFIG_S5P_ADC
static struct s3c_adc_mach_info s3c_adc_platform __initdata = {
	/* s5pc110 support 12-bit resolution */
	.delay		= 10000,
	.presc		= 65,
	.resolution	= 12,
};
#endif

/* There is a only common mic bias gpio in aries H/W */
static DEFINE_SPINLOCK(mic_bias_lock);
static bool wm8994_mic_bias;
static bool wm8994_submic_bias;
bool jack_mic_bias;
EXPORT_SYMBOL(jack_mic_bias);
static void set_shared_mic_bias(void)
{
// [[ HDLNC_BP_pyoungkuenoh_20110223
#if defined(CONFIG_S5PC110_KEPLER_BOARD)
	if( ( HWREV == 0x04 ) || ( HWREV == 0x08 ) || ( HWREV == 0x0C ) || ( HWREV == 0x02 ) || ( HWREV == 0x0A ) )
	{
		gpio_set_value(GPIO_MICBIAS_EN ,jack_mic_bias || wm8994_mic_bias);
		gpio_set_value(GPIO_EARPATH_SEL, wm8994_mic_bias || jack_mic_bias);
	}
	else
	{
		gpio_set_value(GPIO_MICBIAS_EN, wm8994_mic_bias);   // GPJ4(2)
		gpio_set_value(GPIO_EARMICBIAS_EN, jack_mic_bias);	// GPJ4(4) : Use earMicbias since hwrev-0.5
		gpio_set_value(GPIO_EARPATH_SEL, wm8994_mic_bias || jack_mic_bias);
	}
#elif defined (CONFIG_S5PC110_DEMPSEY_BOARD) 	
	gpio_set_value(GPIO_MICBIAS_EN, wm8994_mic_bias);   	// GPJ4(2)
	gpio_set_value(GPIO_EARMICBIAS_EN, jack_mic_bias);	// GPJ4(4) : Use earMicbias since hwrev-0.5
	gpio_set_value(GPIO_EARPATH_SEL, wm8994_mic_bias || jack_mic_bias);
	
#elif defined(CONFIG_S5PC110_HAWK_BOARD)|| defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD)
  	gpio_set_value(GPIO_MICBIAS_EN, wm8994_mic_bias);// GPJ4(2)
	gpio_set_value(GPIO_MICBIAS_EN2, jack_mic_bias||wm8994_submic_bias); // GPJ2(5)
	gpio_set_value(GPIO_EARPATH_SEL, wm8994_mic_bias || jack_mic_bias);
#else
// ]] HDLNC_BP_pyoungkuenoh_20110223	
#if !defined(CONFIG_ARIES_NTT)
	gpio_set_value(GPIO_MICBIAS_EN, wm8994_mic_bias || jack_mic_bias);
#else
	gpio_set_value(GPIO_MICBIAS_EN, wm8994_mic_bias);
	gpio_set_value(GPIO_SUB_MICBIAS_EN, jack_mic_bias);
#endif
	/* high : earjack, low: TV_OUT */
	gpio_set_value(GPIO_EARPATH_SEL, wm8994_mic_bias || jack_mic_bias);
#endif
}

static void wm8994_set_mic_bias(bool on)
{
	unsigned long flags;
	spin_lock_irqsave(&mic_bias_lock, flags);
	wm8994_mic_bias = on;
	set_shared_mic_bias();
	spin_unlock_irqrestore(&mic_bias_lock, flags);
}

static void wm8994_set_submic_bias(bool on)
{
	unsigned long flags;
	spin_lock_irqsave(&mic_bias_lock, flags);
	wm8994_submic_bias = on;
	set_shared_mic_bias();
	spin_unlock_irqrestore(&mic_bias_lock, flags);
}

static void sec_jack_set_micbias_state(bool on)
{
	unsigned long flags;

	spin_lock_irqsave(&mic_bias_lock, flags);
	jack_mic_bias = on;
	set_shared_mic_bias();
	spin_unlock_irqrestore(&mic_bias_lock, flags);
}

static struct wm8994_platform_data wm8994_pdata = {
	.ldo = GPIO_CODEC_LDO_EN,
	.set_mic_bias = wm8994_set_mic_bias,
	.set_submic_bias = wm8994_set_submic_bias
};

/*
 * Guide for Camera Configuration for Crespo board
 * ITU CAM CH A: LSI s5k4ecgx
 */

#ifdef CONFIG_VIDEO_CE147
/*
 * Guide for Camera Configuration for Jupiter board
 * ITU CAM CH A: CE147
*/

static struct regulator *cam_isp_core_regulator;/*buck4*/
static struct regulator *cam_isp_host_regulator;/*15*/
static struct regulator *cam_af_regulator;/*11*/
static struct regulator *cam_sensor_core_regulator;/*12*/
static struct regulator *cam_vga_vddio_regulator;/*13*/
static struct regulator *cam_vga_dvdd_regulator;/*14*/
static struct regulator *cam_vga_avdd_regulator;/*16*/
static bool ce147_powered_on;

static int ce147_regulator_init(void)
{
/*BUCK 4*/
	if (IS_ERR_OR_NULL(cam_isp_core_regulator)) {
		cam_isp_core_regulator = regulator_get(NULL, "cam_isp_core");
		if (IS_ERR_OR_NULL(cam_isp_core_regulator)) {
			pr_err("failed to get cam_isp_core regulator");
			return -EINVAL;
		}
	}
/*ldo 11*/
	if (IS_ERR_OR_NULL(cam_af_regulator)) {
		cam_af_regulator = regulator_get(NULL, "cam_af");
		if (IS_ERR_OR_NULL(cam_af_regulator)) {
			pr_err("failed to get cam_af regulator");
			return -EINVAL;
		}
	}
/*ldo 12*/
	if (IS_ERR_OR_NULL(cam_sensor_core_regulator)) {
		cam_sensor_core_regulator = regulator_get(NULL, "cam_sensor");
		if (IS_ERR_OR_NULL(cam_sensor_core_regulator)) {
			pr_err("failed to get cam_sensor regulator");
			return -EINVAL;
		}
	}
/*ldo 13*/
	if (IS_ERR_OR_NULL(cam_vga_vddio_regulator)) {
		cam_vga_vddio_regulator = regulator_get(NULL, "vga_vddio");
		if (IS_ERR_OR_NULL(cam_vga_vddio_regulator)) {
			pr_err("failed to get vga_vddio regulator");
			return -EINVAL;
		}
	}
/*ldo 14*/
	if (IS_ERR_OR_NULL(cam_vga_dvdd_regulator)) {
		cam_vga_dvdd_regulator = regulator_get(NULL, "vga_dvdd");
		if (IS_ERR_OR_NULL(cam_vga_dvdd_regulator)) {
			pr_err("failed to get vga_dvdd regulator");
			return -EINVAL;
		}
	}
/*ldo 15*/
	if (IS_ERR_OR_NULL(cam_isp_host_regulator)) {
		cam_isp_host_regulator = regulator_get(NULL, "cam_isp_host");
		if (IS_ERR_OR_NULL(cam_isp_host_regulator)) {
			pr_err("failed to get cam_isp_host regulator");
			return -EINVAL;
		}
	}
/*ldo 16*/
	if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
		cam_vga_avdd_regulator = regulator_get(NULL, "vga_avdd");
		if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
			pr_err("failed to get vga_avdd regulator");
			return -EINVAL;
		}
	}
	pr_debug("cam_isp_core_regulator = %p\n", cam_isp_core_regulator);
	pr_debug("cam_isp_host_regulator = %p\n", cam_isp_host_regulator);
	pr_debug("cam_af_regulator = %p\n", cam_af_regulator);
	pr_debug("cam_sensor_core_regulator = %p\n", cam_sensor_core_regulator);
	pr_debug("cam_vga_vddio_regulator = %p\n", cam_vga_vddio_regulator);
	pr_debug("cam_vga_dvdd_regulator = %p\n", cam_vga_dvdd_regulator);
	pr_debug("cam_vga_avdd_regulator = %p\n", cam_vga_avdd_regulator);
	return 0;
}

static void ce147_init(void)
{
	/* CAM_IO_EN - GPB(7) */
	if (gpio_request(GPIO_GPB7, "GPB7") < 0)
		pr_err("failed gpio_request(GPB7) for camera control\n");
	/* CAM_MEGA_nRST - GPJ1(5) */
	if (gpio_request(GPIO_CAM_MEGA_nRST, "GPJ1") < 0)
		pr_err("failed gpio_request(GPJ1) for camera control\n");
	/* CAM_MEGA_EN - GPJ0(6) */
	if (gpio_request(GPIO_CAM_MEGA_EN, "GPJ0") < 0)
		pr_err("failed gpio_request(GPJ0) for camera control\n");
}

static int ce147_ldo_en(bool en)
{
	int err = 0;
	int result;

	if (IS_ERR_OR_NULL(cam_isp_core_regulator) ||
		IS_ERR_OR_NULL(cam_isp_host_regulator) ||
		IS_ERR_OR_NULL(cam_af_regulator) || //) {// ||
		IS_ERR_OR_NULL(cam_sensor_core_regulator) ||
		IS_ERR_OR_NULL(cam_vga_vddio_regulator) ||
		IS_ERR_OR_NULL(cam_vga_dvdd_regulator) ||
		IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
		pr_err("Camera regulators not initialized\n");
		return -EINVAL;
	}

	if (!en)
		goto off;

	/* Turn CAM_ISP_CORE_1.2V(VDD_REG) on BUCK 4*/
	err = regulator_enable(cam_isp_core_regulator);
	if (err) {
		pr_err("Failed to enable regulator cam_isp_core\n");
		goto off;
	}
	mdelay(1);

	/* Turn CAM_AF_2.8V or 3.0V on ldo 11*/
	err = regulator_enable(cam_af_regulator);
	if (err) {
		pr_err("Failed to enable regulator cam_af\n");
		goto off;
	}
	udelay(50);

	/*ldo 12*/
	err = regulator_enable(cam_sensor_core_regulator);
	if (err) {
		pr_err("Failed to enable regulator cam_sensor\n");
		goto off;
	}
	udelay(50);

	/*ldo 13*/
	err = regulator_enable(cam_vga_vddio_regulator);
	if (err) {
		pr_err("Failed to enable regulator cam_vga_vddio\n");
		goto off;
	}
	udelay(50);

	/*ldo 14*/
	err = regulator_enable(cam_vga_dvdd_regulator);
	if (err) {
		pr_err("Failed to enable regulator cam_vga_dvdd\n");
		goto off;
	}
	udelay(50);

	/* Turn CAM_ISP_HOST_2.8V(VDDIO) on ldo 15*/
	err = regulator_enable(cam_isp_host_regulator);
	if (err) {
		pr_err("Failed to enable regulator cam_isp_core\n");
		goto off;
	}
	udelay(50);

	/*ldo 16*/
	err = regulator_enable(cam_vga_avdd_regulator);
	if (err) {
		pr_err("Failed to enable regulator cam_vga_avdd\n");
		goto off;
	}
	udelay(50);
	
	/* Turn CAM_SENSOR_A_2.8V(VDDA) on */
	gpio_set_value(GPIO_GPB7, 1);
	mdelay(1);

	return 0;

off:
	result = err;

	gpio_direction_output(GPIO_GPB7, 1);
	gpio_set_value(GPIO_GPB7, 0);

	/* ldo 11 */
	err = regulator_disable(cam_af_regulator);
	if (err) {
		pr_err("Failed to disable regulator cam_isp_core\n");
		result = err;
	}
	/* ldo 12 */
	err = regulator_disable(cam_sensor_core_regulator);
	if (err) {
		pr_err("Failed to disable regulator cam_sensor\n");
		result = err;
	}
	/* ldo 13 */
	err = regulator_disable(cam_vga_vddio_regulator);
	if (err) {
		pr_err("Failed to disable regulator cam_vga_vddio\n");
		result = err;
	}
	/* ldo 14 */
	err = regulator_disable(cam_vga_dvdd_regulator);
	if (err) {
		pr_err("Failed to disable regulator cam_vga_dvdd\n");
		result = err;
	}
	/* ldo 15 */
	err = regulator_disable(cam_isp_host_regulator);
	if (err) {
		pr_err("Failed to disable regulator cam_isp_core\n");
		result = err;
	}
	/* ldo 16 */
	err = regulator_disable(cam_vga_avdd_regulator);
	if (err) {
		pr_err("Failed to disable regulator cam_vga_avdd\n");
		result = err;
	}
	/* BUCK 4 */
	err = regulator_disable(cam_isp_core_regulator);
	if (err) {
		pr_err("Failed to disable regulator cam_isp_core\n");
		result = err;
	}
	return result;
}

static int ce147_power_on(void)
{	
	int err;
	bool TRUE = true;

	if (ce147_regulator_init()) {
			pr_err("Failed to initialize camera regulators\n");
			return -EINVAL;
	}
	
	ce147_init();

	/* CAM_VGA_nSTBY - GPB(0)  */
	err = gpio_request(GPIO_CAM_VGA_nSTBY, "GPB0");

	if (err) {
		printk(KERN_ERR "failed to request GPB0 for camera control\n");

		return err;
	}

	/* CAM_VGA_nRST - GPB(2) */
	err = gpio_request(GPIO_CAM_VGA_nRST, "GPB2");

	if (err) {
		printk(KERN_ERR "failed to request GPB2 for camera control\n");

		return err;
	}
	
	ce147_ldo_en(TRUE);

	mdelay(1);

	// CAM_VGA_nSTBY  HIGH		
	gpio_direction_output(GPIO_CAM_VGA_nSTBY, 0);

	gpio_set_value(GPIO_CAM_VGA_nSTBY, 1);

	mdelay(1);

	// Mclk enable
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(0x02));

	mdelay(1);

	// CAM_VGA_nRST  HIGH		
	gpio_direction_output(GPIO_CAM_VGA_nRST, 0);

	gpio_set_value(GPIO_CAM_VGA_nRST, 1);	

	mdelay(1);

	// CAM_VGA_nSTBY  LOW	
	gpio_direction_output(GPIO_CAM_VGA_nSTBY, 1);

	gpio_set_value(GPIO_CAM_VGA_nSTBY, 0);

	mdelay(1);

	// CAM_MEGA_EN HIGH
	gpio_direction_output(GPIO_CAM_MEGA_EN, 0);

	gpio_set_value(GPIO_CAM_MEGA_EN, 1);

	mdelay(1);

	// CAM_MEGA_nRST HIGH
	gpio_direction_output(GPIO_CAM_MEGA_nRST, 0);

	gpio_set_value(GPIO_CAM_MEGA_nRST, 1);

	gpio_free(GPIO_CAM_MEGA_EN);
	gpio_free(GPIO_CAM_MEGA_nRST);
	gpio_free(GPIO_CAM_VGA_nSTBY);
	gpio_free(GPIO_CAM_VGA_nRST);
	gpio_free(GPIO_GPB7);

	mdelay(5);

	return 0;
}


static int ce147_power_off(void)
{
	int err;
	bool FALSE = false;

	/* CAM_IO_EN - GPB(7) */
	err = gpio_request(GPIO_GPB7, "GPB7");
	
	if(err) {
		printk(KERN_ERR "failed to request GPB7 for camera control\n");
	
		return err;
	}

	/* CAM_MEGA_EN - GPJ0(6) */
	err = gpio_request(GPIO_CAM_MEGA_EN, "GPJ0");

	if(err) {
		printk(KERN_ERR "failed to request GPJ0 for camera control\n");
	
		return err;
	}

	/* CAM_MEGA_nRST - GPJ1(5) */
	err = gpio_request(GPIO_CAM_MEGA_nRST, "GPJ1");
	
	if(err) {
		printk(KERN_ERR "failed to request GPJ1 for camera control\n");
	
		return err;
	}

	/* CAM_VGA_nRST - GPB(2) */
	err = gpio_request(GPIO_CAM_VGA_nRST, "GPB2");

	if (err) {
		printk(KERN_ERR "failed to request GPB2 for camera control\n");

		return err;
	}
	/* CAM_VGA_nSTBY - GPB(0)  */
	err = gpio_request(GPIO_CAM_VGA_nSTBY, "GPB0");

	if (err) {
		printk(KERN_ERR "failed to request GPB0 for camera control\n");

		return err;
	}

	// CAM_VGA_nSTBY  LOW	
	gpio_direction_output(GPIO_CAM_VGA_nSTBY, 1);

	gpio_set_value(GPIO_CAM_VGA_nSTBY, 0);

	mdelay(1);

	// CAM_VGA_nRST  LOW		
	gpio_direction_output(GPIO_CAM_VGA_nRST, 1);
	
	gpio_set_value(GPIO_CAM_VGA_nRST, 0);

	mdelay(1);

	// CAM_MEGA_nRST - GPJ1(5) LOW
	gpio_direction_output(GPIO_CAM_MEGA_nRST, 1);
	
	gpio_set_value(GPIO_CAM_MEGA_nRST, 0);
	
	mdelay(1);

	// Mclk disable
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, 0);
	
	mdelay(1);

	// CAM_MEGA_EN - GPJ0(6) LOW
	gpio_direction_output(GPIO_CAM_MEGA_EN, 1);
	
	gpio_set_value(GPIO_CAM_MEGA_EN, 0);

	mdelay(1);

	ce147_ldo_en(FALSE);

	mdelay(1);
	
	gpio_free(GPIO_CAM_MEGA_EN);
	gpio_free(GPIO_CAM_MEGA_nRST);
	gpio_free(GPIO_CAM_VGA_nRST);
	gpio_free(GPIO_CAM_VGA_nSTBY);
	gpio_free(GPIO_GPB7);

	return 0;
}


static int ce147_power_en(int onoff)
{
	int bd_level;
	int err = 0;
#if 0
	if(onoff){
		ce147_ldo_en(true);
		s3c_gpio_cfgpin(S5PV210_GPE1(3), S5PV210_GPE1_3_CAM_A_CLKOUT);
		ce147_cam_en(true);
		ce147_cam_nrst(true);
	} else {
		ce147_cam_en(false);
		ce147_cam_nrst(false);
		s3c_gpio_cfgpin(S5PV210_GPE1(3), 0);
		ce147_ldo_en(false);
	}

	return 0;
#endif

	if (onoff != ce147_powered_on) {
		if (onoff)
			err = ce147_power_on();
		else {
			err = ce147_power_off();
			s3c_i2c0_force_stop();
		}
		if (!err)
			ce147_powered_on = onoff;
	}

	return 0;
}

static int smdkc110_cam1_power(int onoff)
{
	int err;
	/* Implement on/off operations */

	/* CAM_VGA_nSTBY - GPB(0) */
	err = gpio_request(S5PV210_GPB(0), "GPB");

	if (err) {
		printk(KERN_ERR "failed to request GPB for camera control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPB(0), 0);
	
	mdelay(1);

	gpio_direction_output(S5PV210_GPB(0), 1);

	mdelay(1);

	gpio_set_value(S5PV210_GPB(0), 1);

	mdelay(1);

	gpio_free(S5PV210_GPB(0));
	
	mdelay(1);

	/* CAM_VGA_nRST - GPB(2) */
	err = gpio_request(S5PV210_GPB(2), "GPB");

	if (err) {
		printk(KERN_ERR "failed to request GPB for camera control\n");
		return err;
	}

	gpio_direction_output(S5PV210_GPB(2), 0);

	mdelay(1);

	gpio_direction_output(S5PV210_GPB(2), 1);

	mdelay(1);

	gpio_set_value(S5PV210_GPB(2), 1);

	mdelay(1);

	gpio_free(S5PV210_GPB(2));

	return 0;
}

/*
 * Guide for Camera Configuration for Jupiter board
 * ITU CAM CH A: CE147
*/

/* External camera module setting */
static struct ce147_platform_data ce147_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 0,
	.power_en = ce147_power_en,
};

static struct i2c_board_info  ce147_i2c_info = {
	I2C_BOARD_INFO("CE147", 0x78>>1),
	.platform_data = &ce147_plat,
};

static struct s3c_platform_camera ce147 = {
	.id		= CAMERA_PAR_A,
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 0,
	.info		= &ce147_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "xusbxti",
	.clk_name	= "sclk_cam",//"sclk_cam0",
	.clk_rate	= 24000000,
	.line_length	= 1920,
	.width		= 640,
	.height		= 480,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

	// Polarity 
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,
	.cam_power	= ce147_power_en,
};
#endif

#ifdef CONFIG_VIDEO_S5KA3DFX
/* External camera module setting */
static DEFINE_MUTEX(s5ka3dfx_lock);
static struct regulator *s5ka3dfx_vga_avdd;
static struct regulator *s5ka3dfx_vga_vddio;
static struct regulator *s5ka3dfx_cam_isp_host;
static struct regulator *s5ka3dfx_vga_dvdd;
static bool s5ka3dfx_powered_on;

static int s5ka3dfx_request_gpio(void)
{
	int err;

	/* CAM_VGA_nSTBY - GPB(0) */
	err = gpio_request(GPIO_CAM_VGA_nSTBY, "GPB0");
	if (err) {
		pr_err("Failed to request GPB0 for camera control\n");
		return -EINVAL;
	}

	/* CAM_VGA_nRST - GPB(2) */
	err = gpio_request(GPIO_CAM_VGA_nRST, "GPB2");
	if (err) {
		pr_err("Failed to request GPB2 for camera control\n");
		gpio_free(GPIO_CAM_VGA_nSTBY);
		return -EINVAL;
	}
	/* CAM_IO_EN - GPB(7) */
	err = gpio_request(GPIO_GPB7, "GPB7");

	if(err) {
		pr_err("Failed to request GPB2 for camera control\n");
		gpio_free(GPIO_CAM_VGA_nSTBY);
		gpio_free(GPIO_CAM_VGA_nRST);
		return -EINVAL;
	}

	return 0;
}

static int s5ka3dfx_power_init(void)
{
	/*if (IS_ERR_OR_NULL(s5ka3dfx_vga_avdd))
		s5ka3dfx_vga_avdd = regulator_get(NULL, "vga_avdd");

	if (IS_ERR_OR_NULL(s5ka3dfx_vga_avdd)) {
		pr_err("Failed to get regulator vga_avdd\n");
		return -EINVAL;
	}*/

	if (IS_ERR_OR_NULL(s5ka3dfx_vga_vddio))
		s5ka3dfx_vga_vddio = regulator_get(NULL, "vga_vddio");

	if (IS_ERR_OR_NULL(s5ka3dfx_vga_vddio)) {
		pr_err("Failed to get regulator vga_vddio\n");
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(s5ka3dfx_cam_isp_host))
		s5ka3dfx_cam_isp_host = regulator_get(NULL, "cam_isp_host");

	if (IS_ERR_OR_NULL(s5ka3dfx_cam_isp_host)) {
		pr_err("Failed to get regulator cam_isp_host\n");
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(s5ka3dfx_vga_dvdd))
		s5ka3dfx_vga_dvdd = regulator_get(NULL, "vga_dvdd");

	if (IS_ERR_OR_NULL(s5ka3dfx_vga_dvdd)) {
		pr_err("Failed to get regulator vga_dvdd\n");
		return -EINVAL;
	}

	return 0;
}

static int s5ka3dfx_power_on(void)
{
	int err = 0;
	int result;

	if (s5ka3dfx_power_init()) {
		pr_err("Failed to get all regulator\n");
		return -EINVAL;
	}

	s5ka3dfx_request_gpio();
	/* Turn VGA_AVDD_2.8V on */
	/*err = regulator_enable(s5ka3dfx_vga_avdd);
	if (err) {
		pr_err("Failed to enable regulator vga_avdd\n");
		return -EINVAL;
	}
	msleep(3);*/
	// Turn CAM_ISP_SYS_2.8V on
	gpio_direction_output(GPIO_GPB7, 0);
	gpio_set_value(GPIO_GPB7, 1);

	mdelay(1);

	/* Turn VGA_VDDIO_2.8V on */
	err = regulator_enable(s5ka3dfx_vga_vddio);
	if (err) {
		pr_err("Failed to enable regulator vga_vddio\n");
		return -EINVAL;//goto off_vga_vddio;
	}
	udelay(20);

	/* Turn VGA_DVDD_1.8V on */
	err = regulator_enable(s5ka3dfx_vga_dvdd);
	if (err) {
		pr_err("Failed to enable regulator vga_dvdd\n");
		goto off_vga_dvdd;
	}
	udelay(100);

	/* CAM_VGA_nSTBY HIGH */
	gpio_direction_output(GPIO_CAM_VGA_nSTBY, 0);
	gpio_set_value(GPIO_CAM_VGA_nSTBY, 1);

	udelay(10);

	/* Mclk enable */
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(0x02));
	udelay(430);

	/* Turn CAM_ISP_HOST_2.8V on */
	err = regulator_enable(s5ka3dfx_cam_isp_host);
	if (err) {
		pr_err("Failed to enable regulator cam_isp_host\n");
		goto off_cam_isp_host;
	}
	udelay(150);

	/* CAM_VGA_nRST HIGH */
	gpio_direction_output(GPIO_CAM_VGA_nRST, 0);
	gpio_set_value(GPIO_CAM_VGA_nRST, 1);
	mdelay(5);

	return 0;
off_cam_isp_host:
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, 0);
	udelay(1);
	gpio_direction_output(GPIO_CAM_VGA_nSTBY, 1);
	gpio_set_value(GPIO_CAM_VGA_nSTBY, 0);
	udelay(1);
	err = regulator_disable(s5ka3dfx_vga_dvdd);
	if (err) {
		pr_err("Failed to disable regulator vga_dvdd\n");
		result = err;
	}
off_vga_dvdd:
	err = regulator_disable(s5ka3dfx_vga_vddio);
	if (err) {
		pr_err("Failed to disable regulator vga_vddio\n");
		result = err;
	}
/*off_vga_vddio:
	err = regulator_disable(s5ka3dfx_vga_avdd);
	if (err) {
		pr_err("Failed to disable regulator vga_avdd\n");
		result = err;
	}*/

	return result;
}

static int s5ka3dfx_power_off(void)
{
	int err;

	if (/*!s5ka3dfx_vga_avdd ||*/ !s5ka3dfx_vga_vddio ||
		!s5ka3dfx_cam_isp_host || !s5ka3dfx_vga_dvdd) {
		pr_err("Faild to get all regulator\n");
		return -EINVAL;
	}

	/* Turn CAM_ISP_HOST_2.8V off */
	err = regulator_disable(s5ka3dfx_cam_isp_host);
	if (err) {
		pr_err("Failed to disable regulator cam_isp_host\n");
		return -EINVAL;
	}

	/* CAM_VGA_nRST LOW */
	gpio_direction_output(GPIO_CAM_VGA_nRST, 1);
	gpio_set_value(GPIO_CAM_VGA_nRST, 0);
	udelay(430);

	/* Mclk disable */
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, 0);

	udelay(1);

	/* Turn VGA_VDDIO_2.8V off */
	err = regulator_disable(s5ka3dfx_vga_vddio);
	if (err) {
		pr_err("Failed to disable regulator vga_vddio\n");
		return -EINVAL;
	}

	/* Turn VGA_DVDD_1.8V off */
	err = regulator_disable(s5ka3dfx_vga_dvdd);
	if (err) {
		pr_err("Failed to disable regulator vga_dvdd\n");
		return -EINVAL;
	}

	/* CAM_VGA_nSTBY LOW */
	gpio_direction_output(GPIO_CAM_VGA_nSTBY, 1);
	gpio_set_value(GPIO_CAM_VGA_nSTBY, 0);

	udelay(1);

	/* Turn VGA_AVDD_2.8V off */
	/*err = regulator_disable(s5ka3dfx_vga_avdd);
	if (err) {
		pr_err("Failed to disable regulator vga_avdd\n");
		return -EINVAL;
	}*/

	gpio_free(GPIO_GPB7);
	gpio_free(GPIO_CAM_VGA_nRST);
	gpio_free(GPIO_CAM_VGA_nSTBY);

	return err;
}

static int s5ka3dfx_power_en(int onoff)
{
	int err = 0;
	mutex_lock(&s5ka3dfx_lock);
	/* we can be asked to turn off even if we never were turned
	 * on if something odd happens and we are closed
	 * by camera framework before we even completely opened.
	 */
	if (onoff != s5ka3dfx_powered_on) {
		if (onoff)
			err = s5ka3dfx_power_on();
		else {
			err = s5ka3dfx_power_off();
			s3c_i2c0_force_stop();
		}
		if (!err)
			s5ka3dfx_powered_on = onoff;
	}
	mutex_unlock(&s5ka3dfx_lock);

	return err;
}

static struct s5ka3dfx_platform_data s5ka3dfx_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 0,

	.cam_power = s5ka3dfx_power_en,
};

static struct i2c_board_info s5ka3dfx_i2c_info = {
	I2C_BOARD_INFO("S5KA3DFX", 0xc4>>1),
	.platform_data = &s5ka3dfx_plat,
};

static struct s3c_platform_camera s5ka3dfx = {
	.id		= CAMERA_PAR_A,
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 0,
	.info		= &s5ka3dfx_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "xusbxti",
	.clk_name	= "sclk_cam",
	.clk_rate	= 24000000,
	.line_length	= 480,
	.width		= 640,
	.height		= 480,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,
	.cam_power	= s5ka3dfx_power_en,
};
#endif

#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_SIDEKICK_BOARD)
#ifdef CONFIG_VIDEO_S5K5CCGX
/*
 * Guide for Camera Configuration for Jupiter board
 * ITU CAM CH A: S5K5CCGX
*/

static struct regulator *cam_vga_avdd_regulator;/*LDO12 3M & VGA CAM_A_2.8V*/
static struct regulator *vga_core_regulator;/*LDO15 VGA CAM_D(core)_1.8V*/
static struct regulator *cam_core_regulator;/*buck4 3M CAM_D(core)_1.2V*/
static struct regulator *cam_vga_vddio_regulator;/*LDO16 3M&VGA CAM_IO_2.8V*/
static struct regulator *cam_vga_af_regulator;/*LDO11 3M & VGA CAM_AF_2.8V*/
static bool s5k5ccgx_powered_on;

static int s5k5ccgx_regulator_init(void)
{

printk("===== s5k5ccgx_regulator_init\n");
/*ldo 12*/
	if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
		cam_vga_avdd_regulator = regulator_get(NULL, "cam_vga_avdd");
		if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
			pr_err("failed to get cam_vga_avdd_regulator");
			return -EINVAL;
		}
	}
/*ldo 15*/
	if (IS_ERR_OR_NULL(vga_core_regulator)) {
		vga_core_regulator = regulator_get(NULL, "vga_core");
		if (IS_ERR_OR_NULL(vga_core_regulator)) {
			pr_err("failed to get vga_core_regulator");
			return -EINVAL;
		}
	}
/*BUCK 4*/
	if (IS_ERR_OR_NULL(cam_core_regulator)) {
		cam_core_regulator = regulator_get(NULL, "cam_core");
		if (IS_ERR_OR_NULL(cam_core_regulator)) {
			pr_err("failed to get cam_core_regulator");
			return -EINVAL;
		}
	}
/*ldo 16*/
	if (IS_ERR_OR_NULL(cam_vga_vddio_regulator)) {
		cam_vga_vddio_regulator = regulator_get(NULL, "cam_vga_vddio");
		if (IS_ERR_OR_NULL(cam_vga_vddio_regulator)) {
			pr_err("failed to get cam_vga_vddio_regulator");
			return -EINVAL;
		}
	}
/*ldo 11*/
	if (IS_ERR_OR_NULL(cam_vga_af_regulator)) {
		cam_vga_af_regulator = regulator_get(NULL, "cam_vga_af");
		if (IS_ERR_OR_NULL(cam_vga_af_regulator)) {
			pr_err("failed to get cam_vga_af_regulator");
			return -EINVAL;
		}
	}

	pr_debug("cam_vga_avdd_regulator = %p\n", cam_vga_avdd_regulator);
	pr_debug("vga_core_regulator = %p\n", vga_core_regulator);
	pr_debug("cam_core_regulator = %p\n", cam_core_regulator);
	pr_debug("cam_vga_vddio_regulator = %p\n", cam_vga_vddio_regulator);
	pr_debug("cam_vga_af_regulator = %p\n", cam_vga_af_regulator);

	return 0;
}

static int s5k5ccgx_ldo_en(bool en)
{
	int err = 0;
	int result;
	printk("===== s5k5ccgx_ldo_en : %s\n", en ? "enable" : "disable");

	if (IS_ERR_OR_NULL(cam_vga_avdd_regulator) ||
		IS_ERR_OR_NULL(vga_core_regulator) ||
		IS_ERR_OR_NULL(cam_core_regulator) ||
		IS_ERR_OR_NULL(cam_vga_vddio_regulator) ||
		IS_ERR_OR_NULL(cam_vga_af_regulator)){
		pr_err("Camera regulators not initialized\n");
		return -EINVAL;
	}

	if (!en)
		goto off;

	/* ldo 12 */
	/* Turn 3M & VGA CAM_A_2.8V on*/
	err = regulator_enable(cam_vga_avdd_regulator);
	if (err) {
		pr_err("Failed to enable cam_vga_avdd_regulator\n");
		goto off;
	}

	/* ldo 15 */
	/* Turn VGA CAM_D(core)_1.8V on*/
	err = regulator_enable(vga_core_regulator);
	if (err) {
		pr_err("Failed to enable vga_core_regulator\n");
		goto off;
	}
	udelay(20); //20us

	/* BUCK 4 */
	/* Turn 3M CAM_D(core)_1.2V on*/
	err = regulator_enable(cam_core_regulator);
	if (err) {
		pr_err("Failed to enable cam_core_regulator\n");
		goto off;
	}
	udelay(15);//15us

	/* ldo 16 */
	/* Turn 3M&VGA CAM_IO_2.8V on*/
	err = regulator_enable(cam_vga_vddio_regulator);
	if (err) {
		pr_err("Failed to enable rcam_vga_vddio_regulator\n");
		goto off;
	}

	/* ldo 11 */
	/* Turn 3M & VGA CAM_AF_2.8V on*/
	err = regulator_enable(cam_vga_af_regulator);
	if (err) {
		pr_err("Failed to enable cam_vga_af_regulator\n");
		goto off;
	}

	return 0;

off:
	result = err;
	
	/* ldo 16 */
	/*Turn 3M &VGA CAM_IO_2.8V off*/
	err = regulator_disable(cam_vga_vddio_regulator);
	if (err) {
		pr_err("Failed to disable cam_vga_vddio_regulator\n");
		result = err;
	}

	/* ldo 11 */
	/*Turn 3M &VGA CAM_AF_2.8V off*/
	err = regulator_disable(cam_vga_af_regulator);
	if (err) {
		pr_err("Failed to disable regulator cam_isp_core\n");
		result = err;
	}

	/* BUCK 4 */
	/*Turn 3M CAM_D_1.2V off*/
	err = regulator_disable(cam_core_regulator);
	if (err) {
		pr_err("Failed to disable cam_core_regulator\n");
		result = err;
	}

	/* ldo 15 */
	/*Turn VGA CAM_D_1.8V off*/
	err = regulator_disable(vga_core_regulator);
	if (err) {
		pr_err("Failed to disable vga_core_regulator\n");
		result = err;
	}
	
	/* ldo 12 */
	/*Turn 3M &VGA CAM_A_2.8V off*/
	err = regulator_disable(cam_vga_avdd_regulator);
	if (err) {
		pr_err("Failed to disable cam_vga_avdd_regulator\n");
		result = err;
	}

	return result;
}

static int s5k5ccgx_power_on(void)
{	
	int err;
	bool TRUE = true;
	printk("===== s5k5ccgx_power_on\n");

	//printk(KERN_DEBUG "s5k5ccgx_power_on\n");

	if (s5k5ccgx_regulator_init()) {
		pr_err("Failed to initialize camera regulators\n");
		return -EINVAL;
	}
	
	/* CAM_MEGA_EN - GPJ0(6) */
	err = gpio_request(GPIO_CAM_MEGA_EN, "GPJ0");
	if(err) {
		printk(KERN_ERR "failed to request GPJ0 for camera control\n");
		return err;
	}

	/* CAM_MEGA_nRST - GPJ1(5) */
	err = gpio_request(GPIO_CAM_MEGA_nRST, "GPJ1");
	if(err) {
		printk(KERN_ERR "failed to request GPJ1 for camera control\n");
		return err;
	}

	/* CAM_VGA_EN - GPJ1(2) */
	err = gpio_request(S5PV210_GPJ1(2), "GPJ12");
	if(err) {
		printk(KERN_ERR "failed to request GPJ12 for camera control\n");
		return err;
	}

	/* CAM_VGA_nRST - GPJ1(4) */
	err = gpio_request(S5PV210_GPJ1(4), "GPJ14");
	if(err) {
		printk(KERN_ERR "failed to request GPJ14 for camera control\n");
		return err;
	}
	
	//LDO enable	
	s5k5ccgx_ldo_en(TRUE);
	
	udelay(20); //20us
	
	// VGA CAM_VGA_EN HIGH
	gpio_direction_output(S5PV210_GPJ1(2), 0);
	gpio_set_value(S5PV210_GPJ1(2), 1);

	// Mclk enable
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(0x02));

	mdelay(5); // 4ms
	
	// VGA CAM_VGA_nRST HIGH
	gpio_direction_output(S5PV210_GPJ1(4), 0);
	gpio_set_value(S5PV210_GPJ1(4), 1);
	
	mdelay(7); // 6ms
	
	// VGA CAM_VGA_EN LOW
	gpio_direction_output(S5PV210_GPJ1(2), 0);
	gpio_set_value(S5PV210_GPJ1(2), 0);
	
	udelay(15); //10us
	
	// CAM_MEGA_EN HIGH
	gpio_direction_output(GPIO_CAM_MEGA_EN, 0);
	gpio_set_value(GPIO_CAM_MEGA_EN, 1);
	
	udelay(20); //15us
	
	// CAM_MEGA_nRST HIGH
	gpio_direction_output(GPIO_CAM_MEGA_nRST, 0);
	gpio_set_value(GPIO_CAM_MEGA_nRST, 1);
	
	msleep(50); //50ms
	
	// VGA CAM_GPIO free
	gpio_free(S5PV210_GPJ1(2));
	gpio_free(S5PV210_GPJ1(4));
	
	//CAM_GPIO free
	gpio_free(GPIO_CAM_MEGA_EN);
	gpio_free(GPIO_CAM_MEGA_nRST);

	return 0;
}


static int s5k5ccgx_power_off(void)
{
	int err;
	bool FALSE = false;
	
	printk(KERN_DEBUG "s5k5ccgx_power_off\n");

	/* CAM_MEGA_EN - GPJ0(6) */
	err = gpio_request(GPIO_CAM_MEGA_EN, "GPJ0");
	if(err) {
		printk(KERN_ERR "failed to request GPJ0 for camera control\n");
		return err;
	}
	
	/* CAM_MEGA_nRST - GPJ1(5) */
	err = gpio_request(GPIO_CAM_MEGA_nRST, "GPJ1");
	if(err) {
		printk(KERN_ERR "failed to request GPJ1 for camera control\n");
		return err;
	}
	
	/* CAM_VGA_EN - GPJ1(2) */
	err = gpio_request(S5PV210_GPJ1(2), "GPJ12");
	if(err) {
		printk(KERN_ERR "failed to request GPJ12 for camera control\n");
		return err;
	}
	
	/* CAM_VGA_nRST - GPJ1(4) */
	err = gpio_request(S5PV210_GPJ1(4), "GPJ14");
	if(err) {
		printk(KERN_ERR "failed to request GPJ14 for camera control\n");
		return err;
	}
		
	// 3M CAM_MEGA_nRST - GPJ1(5) LOW
	gpio_direction_output(GPIO_CAM_MEGA_nRST, 0);
	gpio_set_value(GPIO_CAM_MEGA_nRST, 0);
	
	udelay(50); //50us
	
	// 3M&VGA Mclk disable
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, 0);
	
	// 3M CAM_MEGA_EN - GPJ0(6) LOW
	gpio_direction_output(GPIO_CAM_MEGA_EN, 0);
	gpio_set_value(GPIO_CAM_MEGA_EN, 0);
	
	// VGA CAM_VGA_nRST - GPJ1(4) LOW
	gpio_direction_output(S5PV210_GPJ1(4), 0);
	gpio_set_value(S5PV210_GPJ1(4), 0);
	
	// VGA CAM_VGA_EN - GPJ1(2) LOW
	gpio_direction_output(S5PV210_GPJ1(2), 0);
	gpio_set_value(S5PV210_GPJ1(2), 0);
	
	//LDO disable
	s5k5ccgx_ldo_en(FALSE);
	
	// VGA CAM_GPIO free
	gpio_free(S5PV210_GPJ1(2));
	gpio_free(S5PV210_GPJ1(4));
	
	//CAM_GPIO free
	gpio_free(GPIO_CAM_MEGA_EN);
	gpio_free(GPIO_CAM_MEGA_nRST);

	return 0;
}


static int s5k5ccgx_power_en(int onoff)
{
	int err = 0;

	if (onoff != s5k5ccgx_powered_on) {
		if (onoff)
			err = s5k5ccgx_power_on();
		else {
			err = s5k5ccgx_power_off();
			s3c_i2c0_force_stop();
		}
		if (!err)
			s5k5ccgx_powered_on = onoff;
	}

	return 0;
}

/*
 * Guide for Camera Configuration for Jupiter board
 * ITU CAM CH A: S5K5CCGX
*/

/* External camera module setting */
static struct s5k5ccgx_platform_data s5k5ccgx_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 0,
	.power_en = s5k5ccgx_power_en,
};

static struct i2c_board_info  s5k5ccgx_i2c_info = {
	I2C_BOARD_INFO("S5K5CCGX", 0x78>>1),
	.platform_data = &s5k5ccgx_plat,
};

static struct s3c_platform_camera s5k5ccgx = {
	.id 	= CAMERA_PAR_A,
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum = 0,
	.info		= &s5k5ccgx_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name = "xusbxti",
	.clk_name	= "sclk_cam",//"sclk_cam0",
	.clk_rate	= 24000000,
	.line_length	= 480,
	.width		= 640,
	.height 		= 480,
	.window 		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height 	= 480,
	},

	// Polarity 
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,
	.cam_power	= s5k5ccgx_power_en,
};
#endif

#ifdef CONFIG_VIDEO_SR030PC30
/*
 * Guide for Camera Configuration for Jupiter board
 * ITU CAM CH A: SR030PC30
*/

static struct regulator *cam_vga_avdd_regulator;/*LDO12 3M & VGA CAM_A_2.8V*/
static struct regulator *vga_core_regulator;/*LDO15 VGA CAM_D(core)_1.8V*/
static struct regulator *cam_core_regulator;/*buck4 3M CAM_D(core)_1.2V*/
static struct regulator *cam_vga_vddio_regulator;/*LDO16 3M&VGA CAM_IO_2.8V*/
static struct regulator *cam_vga_af_regulator;/*LDO11 3M & VGA CAM_AF_2.8V*/
static bool SR030pc30_powered_on;

static int SR030pc30_regulator_init(void)
{

printk("===== SR030pc30_regulator_init\n");
/*ldo 12*/
	if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
		cam_vga_avdd_regulator = regulator_get(NULL, "cam_vga_avdd");
		if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
			pr_err("failed to get cam_vga_avdd_regulator");
			return -EINVAL;
		}
	}
/*ldo 15*/
	if (IS_ERR_OR_NULL(vga_core_regulator)) {
		vga_core_regulator = regulator_get(NULL, "vga_core");
		if (IS_ERR_OR_NULL(vga_core_regulator)) {
			pr_err("failed to get vga_core_regulator");
			return -EINVAL;
		}
	}
/*BUCK 4*/
	if (IS_ERR_OR_NULL(cam_core_regulator)) {
		cam_core_regulator = regulator_get(NULL, "cam_core");
		if (IS_ERR_OR_NULL(cam_core_regulator)) {
			pr_err("failed to get cam_core_regulator");
			return -EINVAL;
		}
	}
/*ldo 16*/
	if (IS_ERR_OR_NULL(cam_vga_vddio_regulator)) {
		cam_vga_vddio_regulator = regulator_get(NULL, "cam_vga_vddio");
		if (IS_ERR_OR_NULL(cam_vga_vddio_regulator)) {
			pr_err("failed to get cam_vga_vddio_regulator");
			return -EINVAL;
		}
	}
/*ldo 11*/
	if (IS_ERR_OR_NULL(cam_vga_af_regulator)) {
		cam_vga_af_regulator = regulator_get(NULL, "cam_vga_af");
		if (IS_ERR_OR_NULL(cam_vga_af_regulator)) {
			pr_err("failed to get cam_vga_af_regulator");
			return -EINVAL;
		}
	}

	pr_debug("cam_vga_avdd_regulator = %p\n", cam_vga_avdd_regulator);
	pr_debug("vga_core_regulator = %p\n", vga_core_regulator);
	pr_debug("cam_core_regulator = %p\n", cam_core_regulator);
	pr_debug("cam_vga_vddio_regulator = %p\n", cam_vga_vddio_regulator);
	pr_debug("cam_vga_af_regulator = %p\n", cam_vga_af_regulator);

	return 0;
}

static int SR030pc30_ldo_en(bool en)
{
	int err = 0;
	int result;
	printk("===== SR030pc30_ldo_en\n");

	if (IS_ERR_OR_NULL(cam_vga_avdd_regulator) ||
		IS_ERR_OR_NULL(vga_core_regulator) ||
		IS_ERR_OR_NULL(cam_core_regulator) ||
		IS_ERR_OR_NULL(cam_vga_vddio_regulator) ||
		IS_ERR_OR_NULL(cam_vga_af_regulator)){
		pr_err("Camera regulators not initialized\n");
		return -EINVAL;
	}

	if (!en)
		goto off;

	/* ldo 12 */
	/* Turn 3M & VGA CAM_A_2.8V on*/
	err = regulator_enable(cam_vga_avdd_regulator);
	if (err) {
		pr_err("Failed to enable cam_vga_avdd_regulator\n");
		goto off;
	}

	/* ldo 15 */
	/* Turn VGA CAM_D(core)_1.8V on*/
	err = regulator_enable(vga_core_regulator);
	if (err) {
		pr_err("Failed to enable vga_core_regulator\n");
		goto off;
	}
	udelay(20); //20us

	/* BUCK 4 */
	/* Turn 3M CAM_D(core)_1.2V on*/
	err = regulator_enable(cam_core_regulator);
	if (err) {
		pr_err("Failed to enable cam_core_regulator\n");
		goto off;
	}
	udelay(15);//15us

	/* ldo 16 */
	/* Turn 3M&VGA CAM_IO_2.8V on*/
	err = regulator_enable(cam_vga_vddio_regulator);
	if (err) {
		pr_err("Failed to enable rcam_vga_vddio_regulator\n");
		goto off;
	}

	/* ldo 11 */
	/* Turn 3M & VGA CAM_AF_2.8V on*/
	err = regulator_enable(cam_vga_af_regulator);
	if (err) {
		pr_err("Failed to enable cam_vga_af_regulator\n");
		goto off;
	}

	return 0;

off:
	result = err;
	
	/* ldo 16 */
	/*Turn 3M &VGA CAM_IO_2.8V off*/
	err = regulator_disable(cam_vga_vddio_regulator);
	if (err) {
		pr_err("Failed to disable cam_vga_vddio_regulator\n");
		result = err;
	}

	/* ldo 11 */
	/*Turn 3M &VGA CAM_AF_2.8V off*/
	err = regulator_disable(cam_vga_af_regulator);
	if (err) {
		pr_err("Failed to disable regulator cam_isp_core\n");
		result = err;
	}

	/* BUCK 4 */
	/*Turn 3M CAM_D_1.2V off*/
	err = regulator_disable(cam_core_regulator);
	if (err) {
		pr_err("Failed to disable cam_core_regulator\n");
		result = err;
	}

	/* ldo 15 */
	/*Turn VGA CAM_D_1.8V off*/
	err = regulator_disable(vga_core_regulator);
	if (err) {
		pr_err("Failed to disable vga_core_regulator\n");
		result = err;
	}
	
	/* ldo 12 */
	/*Turn 3M &VGA CAM_A_2.8V off*/
	err = regulator_disable(cam_vga_avdd_regulator);
	if (err) {
		pr_err("Failed to disable cam_vga_avdd_regulator\n");
		result = err;
	}

	return result;
}

static int SR030pc30_power_on(void)
{	
	int err;
	bool TRUE = true;
	printk("===== SR030pc30_power_on\n");

	printk(KERN_DEBUG "SR030pc30_power_on\n");

	if (SR030pc30_regulator_init()) {
		pr_err("Failed to initialize camera regulators\n");
		return -EINVAL;
	}
	
	/* CAM_MEGA_EN - GPJ0(6) */
	err = gpio_request(GPIO_CAM_MEGA_EN, "GPJ0");
	if(err) {
		printk(KERN_ERR "failed to request GPJ0 for camera control\n");
		return err;
	}

	/* CAM_MEGA_nRST - GPJ1(5) */
	err = gpio_request(GPIO_CAM_MEGA_nRST, "GPJ1");
	if(err) {
		printk(KERN_ERR "failed to request GPJ1 for camera control\n");
		return err;
	}

	/* CAM_VGA_EN - GPJ1(2) */
	err = gpio_request(S5PV210_GPJ1(2), "GPJ12");
	if(err) {
		printk(KERN_ERR "failed to request GPJ12 for camera control\n");
		return err;
	}

	/* CAM_VGA_nRST - GPJ1(4) */
	err = gpio_request(S5PV210_GPJ1(4), "GPJ14");
	if(err) {
		printk(KERN_ERR "failed to request GPJ14 for camera control\n");
		return err;
	}
	
	//LDO enable	
	SR030pc30_ldo_en(TRUE);
	
	udelay(20); //20us
	
	// VGA CAM_VGA_EN HIGH
	gpio_direction_output(S5PV210_GPJ1(2), 0);
	gpio_set_value(S5PV210_GPJ1(2), 1);

	// Mclk enable
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(0x02));

	udelay(10); //10us
	
	// 3M CAM_MEGA_EN HIGH
	gpio_direction_output(GPIO_CAM_MEGA_EN, 0);
	gpio_set_value(GPIO_CAM_MEGA_EN, 1);

	mdelay(6); //5ms
	
	// 3M CAM_MEGA_nRST HIGH
	gpio_direction_output(GPIO_CAM_MEGA_nRST, 0);
	gpio_set_value(GPIO_CAM_MEGA_nRST, 1);

	mdelay(7); //6.5ms

	// 3M CAM_MEGA_nRST LOW
	gpio_direction_output(GPIO_CAM_MEGA_EN, 0);
	gpio_set_value(GPIO_CAM_MEGA_EN, 0);

	udelay(10); //10us
	
	// VGA CAM_VGA_nRST HIGH
	gpio_direction_output(S5PV210_GPJ1(4), 0);
	gpio_set_value(S5PV210_GPJ1(4), 1);
	
	msleep(50); //50ms
		
	// VGA CAM_GPIO free
	gpio_free(S5PV210_GPJ1(2));
	gpio_free(S5PV210_GPJ1(4));
	
	//CAM_GPIO free
	gpio_free(GPIO_CAM_MEGA_EN);
	gpio_free(GPIO_CAM_MEGA_nRST);

	return 0;
}


static int SR030pc30_power_off(void)
{
	int err;
	bool FALSE = false;
	
	printk(KERN_DEBUG "SR030pc30_power_off\n");

	/* CAM_MEGA_EN - GPJ0(6) */
	err = gpio_request(GPIO_CAM_MEGA_EN, "GPJ0");
	if(err) {
		printk(KERN_ERR "failed to request GPJ0 for camera control\n");
		return err;
	}
	
	/* CAM_MEGA_nRST - GPJ1(5) */
	err = gpio_request(GPIO_CAM_MEGA_nRST, "GPJ1");
	if(err) {
		printk(KERN_ERR "failed to request GPJ1 for camera control\n");
		return err;
	}
	
	/* CAM_VGA_EN - GPJ1(2) */
	err = gpio_request(S5PV210_GPJ1(2), "GPJ12");
	if(err) {
		printk(KERN_ERR "failed to request GPJ12 for camera control\n");
		return err;
	}
	
	/* CAM_VGA_nRST - GPJ1(4) */
	err = gpio_request(S5PV210_GPJ1(4), "GPJ14");
	if(err) {
		printk(KERN_ERR "failed to request GPJ14 for camera control\n");
		return err;
	}
	
	// VGA CAM_VGA_nRST - GPJ1(4) LOW
	gpio_direction_output(S5PV210_GPJ1(4), 0);
	gpio_set_value(S5PV210_GPJ1(4), 0);	

	// 3M CAM_MEGA_nRST - GPJ1(5) LOW
	gpio_direction_output(GPIO_CAM_MEGA_nRST, 0);
	gpio_set_value(GPIO_CAM_MEGA_nRST, 0);
	
	udelay(50); //50us
	
	// 3M&VGA Mclk disable
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, 0);
	
	// VGA CAM_VGA_EN - GPJ1(2) LOW
	gpio_direction_output(S5PV210_GPJ1(2), 0);
	gpio_set_value(S5PV210_GPJ1(2), 0);

	// 3M CAM_MEGA_EN - GPJ0(6) LOW
	gpio_direction_output(GPIO_CAM_MEGA_EN, 0);
	gpio_set_value(GPIO_CAM_MEGA_EN, 0);
		
	//LDO disable
	SR030pc30_ldo_en(FALSE);
	
	// VGA CAM_GPIO free
	gpio_free(S5PV210_GPJ1(2));
	gpio_free(S5PV210_GPJ1(4));
	
	//CAM_GPIO free
	gpio_free(GPIO_CAM_MEGA_EN);
	gpio_free(GPIO_CAM_MEGA_nRST);

	return 0;
}


static int SR030pc30_power_en(int onoff)
{
	int err = 0;

	if (onoff != SR030pc30_powered_on) {
		if (onoff)
			err = SR030pc30_power_on();
		else {
			err = SR030pc30_power_off();
				 s3c_i2c0_force_stop();
		}
		if (!err)
			SR030pc30_powered_on = onoff;
	}

	return 0;
}

/*
 * Guide for Camera Configuration for Jupiter board
 * ITU CAM CH A: SR030PC30
*/

/* External camera module setting */
static struct SR030pc30_platform_data sr030pc30_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 0,
	.power_en = SR030pc30_power_en,
};

static struct i2c_board_info  sr030pc30_i2c_info = {
	I2C_BOARD_INFO("SR030pc30", 0x60>>1),
	.platform_data = &sr030pc30_plat,
};

static struct s3c_platform_camera sr030pc30 = {
	.id 		= CAMERA_PAR_A,
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum = 0,
	.info		= &sr030pc30_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name = "xusbxti",
	.clk_name	= "sclk_cam",//"sclk_cam0",
	.clk_rate	= 24000000,
	.line_length	= 480,
	.width		= 640,
	.height 		= 480,
	.window 		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height 	= 480,
	},

	// Polarity 
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,
	.cam_power	= SR030pc30_power_en,
};
#endif
#endif

#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
static struct regulator *cam_vga_avdd_regulator;/*LDO12 3M & VGA CAM_A_2.8V*/
static struct regulator *cam_af_regulator;/*11*/
static struct regulator *cam_isp_core_regulator;/*buck4*/
static bool sr130pc10_powered_on;
#if defined(CONFIG_VIDEO_SR130PC10) //NAGSM_Android_HQ_Camera_SungkooLee_20101230
static inline int sr130pc10_power_on()
{

	int err;

	printk(KERN_ERR "sr130pc10_power_on : mach\n");

	/* CAM_VGA_nSTBY - MP02(0)  */
	err = gpio_request(GPIO_CAM_VGA_nSTBY, "MP02(0)");
	if (err) {
		printk(KERN_ERR "failed to request GPIO for camera nSTBY pin\n");
		return err;
	}

	/* CAM_VGA_nRST - MP02(1) */
	err = gpio_request(GPIO_CAM_VGA_nRST, "MP02(1)");
        if (err) {
		printk(KERN_ERR "failed to request GPIO for camera nRST pin\n");
                return err;
        }

	/*CAM_CORE_EN */
	err = gpio_request(GPIO_CAM_CORE_EN, "GPC1(1)");
	if (err) {
		printk(KERN_ERR "failed to request gpio(GPIO_CAM_CORE_EN)\n");
		return err;
	}		

	/* main CAM_SENSOR_A2.8V */
	err = gpio_request(GPIO_CAM_IO_EN, "GPB7");
	if (err) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_IO_EN)\n");
		return err;
	}	

	//NAGSM_Android_HQ_Camera_SungkooLee_20101224 : VT_CORE_EN was added in revision 0.5
	/* VT_CORE_EN - MP01(0)  */ 
	if(HWREV >= 0x06)
	{
		err = gpio_request(S5PV210_MP04(0), "MP04(0)");
		if (err) {
			printk(KERN_ERR "failed to request GPIO for camera VT_CORE_EN pin\n");
			return err;
		}
	}

	/* CAM_ISP_CORE_1.2V */ 

	if (IS_ERR_OR_NULL(cam_isp_core_regulator)) {
		cam_isp_core_regulator = regulator_get(NULL, "cam_isp_core");
		if (IS_ERR_OR_NULL(cam_isp_core_regulator)) {
			pr_err("failed to get cam_isp_core regulator");
			return -EINVAL;
		}
	}
	pr_err("cam_isp_core_regulator = %p\n", cam_isp_core_regulator);
	err = regulator_enable(cam_isp_core_regulator);
	if (err) {
		pr_err("Failed to enable regulator cam_isp_core\n");
		return -EINVAL;
	}
	udelay(1);

	/* CAM_SENSOR_CORE_1.2V */
	gpio_direction_output(GPIO_CAM_CORE_EN, 1);
	gpio_free(GPIO_CAM_CORE_EN);
	udelay(1);
	
	/* main CAM_SENSOR_A2.8V */
	gpio_direction_output(GPIO_CAM_IO_EN, 1);
	gpio_free(GPIO_CAM_IO_EN);
	udelay(1);

	/* CAM_SENSOR_IO_1.8V */	
	if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
		cam_vga_avdd_regulator = regulator_get(NULL, "vga_avdd");
		if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
			pr_err("failed to get cam_vga_avdd_regulator");
			return -EINVAL;
		}
	}
	pr_err("cam_isp_host_regulator = %p\n", cam_vga_avdd_regulator);
	err = regulator_enable(cam_vga_avdd_regulator);
	if (err) {
		pr_err("Failed to enable cam_vga_avdd_regulator\n");
		return -EINVAL;
	}
	udelay(1);
	
		/* CAM_ISP_1.8V */
//	max8998_ldo_enable_direct(MAX8998_LDO14);	
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
	REG_power_onoff(1); //on
#endif
	udelay(1);	

#if 0
	/* CAM_AF_2.8V */		
	if (IS_ERR_OR_NULL(cam_af_regulator)) {
		cam_af_regulator = regulator_get(NULL, "cam_af");
		if (IS_ERR_OR_NULL(cam_af_regulator)) {
			pr_err("failed to get cam_af regulator");
			return -EINVAL;
		}
	}
	pr_err("cam_af_regulator = %p\n", cam_af_regulator);
err = regulator_enable(cam_af_regulator);
	if (err) {
		pr_err("Failed to enable regulator cam_af_regulator\n");
		return -EINVAL;
	}
	udelay(1);	
#endif

	/* Turn CAM_SENSOR_IO_1.6V on */
//	Set_MAX8998_PM_OUTPUT_Voltage(LDO16, VCC_1p600);
//	Set_MAX8998_PM_REG(ELDO16, 1);
//	udelay(50);

	/* Turn VT_SENSOR_A_2.8V on */
#ifdef CONFIG_REGULATOR_MAX8893
	bh6173_ldo_enable_direct(1);//LDO 1
#endif
	udelay(100);	

	//NAGSM_Android_HQ_Camera_SungkooLee_20101224 : VT_CORE_EN was added in revision 0.5	/* VT_CORE_EN  HIGH */
	if(HWREV >= 0x06)
	{
		gpio_direction_output(S5PV210_MP04(0), 0);
		gpio_set_value(S5PV210_MP04(0), 1);
		udelay(10);
		gpio_free(S5PV210_MP04(0));
	}

	/* Turn VT_IO_1.8V on */
#ifdef CONFIG_REGULATOR_MAX8893
	bh6173_ldo_enable_direct(2);//LDO 2
#endif 
	udelay(300);

	/* CAM_VGA_nSTBY  HIGH */
	gpio_direction_output(GPIO_CAM_VGA_nSTBY, 0);
	gpio_set_value(GPIO_CAM_VGA_nSTBY, 1);
	udelay(1);

	/* Mclk enable */
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, S5PV210_GPE1_3_CAM_A_CLKOUT);
	mdelay(12);

	/* CAM_VGA_nRST  HIGH */
	gpio_direction_output(GPIO_CAM_VGA_nRST, 0);
	gpio_set_value(GPIO_CAM_VGA_nRST, 1);		
	mdelay(1);

	gpio_free(GPIO_CAM_VGA_nSTBY);
	gpio_free(GPIO_CAM_VGA_nRST);	

	return 0;

}

static inline int sr130pc10_power_off()
{	

	int err;

	printk(KERN_ERR "sr130pc10_power_off : mach\n");

	/* CAM_VGA_nSTBY - GPB(0)  */
	err = gpio_request(GPIO_CAM_VGA_nSTBY, "MP02(0)");
	if (err) {
		printk(KERN_ERR "failed to request GPIO for camera nSTBY pin\n");
		return err;
}

	/* CAM_VGA_nRST - GPB(2) */
	err = gpio_request(GPIO_CAM_VGA_nRST, "MP02(1)");
	if(err) {
		printk(KERN_ERR "failed to request GPIO for camera nRST pin\n");
		return err;
	}

	err = gpio_request(GPIO_CAM_CORE_EN, "GPC1(1)");
	if (err) {
		printk(KERN_ERR "failed to request gpio(GPIO_CAM_CORE_EN)\n");
		return err;
	}	

	/* CAM_SENSOR_A2.8V */
	err = gpio_request(GPIO_CAM_IO_EN, "GPB7");
	if (err) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_IO_EN)\n");
		return err;
	}

	//NAGSM_Android_HQ_Camera_SungkooLee_20101224 : VT_CORE_EN was added in revision 0.5
	/* VT_CORE_EN - MP01(0)  */
	if(HWREV >= 0x06)
	{
		err = gpio_request(S5PV210_MP04(0), "MP04(0)");
		if (err) {
			printk(KERN_ERR "failed to request GPIO for camera VT_CORE_EN pin\n");
			return err;
		}
	} 

	/* CAM_VGA_nRST  LOW */
	gpio_direction_output(GPIO_CAM_VGA_nRST, 1);
	gpio_set_value(GPIO_CAM_VGA_nRST, 0);
	mdelay(12);

	/* Mclk disable */
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, 0);
	udelay(10);

	/* CAM_VGA_nSTBY  LOW */
	gpio_direction_output(GPIO_CAM_VGA_nSTBY, 1);
	gpio_set_value(GPIO_CAM_VGA_nSTBY, 0);
	udelay(200);	

	/* Turn VT_IO_1.8V off */
#ifdef CONFIG_REGULATOR_MAX8893
	bh6173_ldo_disable_direct(2);//LDO 2
#endif
	udelay(1);

	//NAGSM_Android_HQ_Camera_SungkooLee_20101224 : VT_CORE_EN was added in revision 0.5 
	/* VT_CORE_EN  LOW */
	if(HWREV >= 0x06)
	{
		gpio_direction_output(S5PV210_MP04(0), 1);
		gpio_set_value(S5PV210_MP04(0), 0);
		udelay(10);
		gpio_free(S5PV210_MP04(0));		
	} 
	udelay(100);
	
	/* Turn VT_SENSOR_A_2.8V */
#ifdef CONFIG_REGULATOR_MAX8893
	bh6173_ldo_disable_direct(1);//LDO 1
#endif
	udelay(1);

#if 0
	/* CAM_AF_2.8V */
		if (IS_ERR_OR_NULL(cam_af_regulator)) {
		cam_af_regulator = regulator_get(NULL, "cam_af");
		if (IS_ERR_OR_NULL(cam_af_regulator)) {
			pr_err("failed to get cam_af regulator");
			return -EINVAL;
		}
	}
		pr_err("cam_af_regulator = %p\n", cam_af_regulator);
//	udelay(50);
		err = regulator_disable(cam_af_regulator);
	if (err) {
		pr_err("Failed to disable regulator cam_isp_core\n");
		return -EINVAL;
	}
	udelay(1);
#endif
	
	/* CAM_ISP_1.8V */
//	max8998_ldo_disable_direct(MAX8998_LDO14);
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
	REG_power_onoff(0); //off
#endif 
	udelay(1);

	/* CAM_SENSOR_IO_1.8V */
	if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
		cam_vga_avdd_regulator = regulator_get(NULL, "vga_avdd");
		if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
			pr_err("failed to get cam_vga_avdd_regulator");
			return -EINVAL;
		}
	}
	pr_err("cam_isp_host_regulator = %p\n", cam_vga_avdd_regulator);
		err = regulator_disable(cam_vga_avdd_regulator);
	if (err) {
		pr_err("Failed to disable cam_vga_vddio_regulator\n");
		return -EINVAL;
	}
	udelay(1);
	
	/* CAM_SENSOR_A2.8V */
	gpio_direction_output(GPIO_CAM_IO_EN, 0);
	gpio_free(GPIO_CAM_IO_EN);
	udelay(1);

	/* CAM_ISP_CORE_1.2V */ 
	if (IS_ERR_OR_NULL(cam_isp_core_regulator)) {
		cam_isp_core_regulator = regulator_get(NULL, "cam_isp_core");
		if (IS_ERR_OR_NULL(cam_isp_core_regulator)) {
			pr_err("failed to get cam_isp_core regulator");
			return -EINVAL;
		}
	}
	pr_err("cam_isp_core_regulator = %p\n", cam_isp_core_regulator);
	err = regulator_disable(cam_isp_core_regulator);
	if (err) {
		pr_err("Failed to enable regulator cam_isp_core\n");
		return -EINVAL;
	}
	udelay(1);

	/* CAM_SENSOR_CORE_1.2V */
	gpio_direction_output(GPIO_CAM_CORE_EN, 0);
	
	gpio_free(GPIO_CAM_CORE_EN);	
	gpio_free(GPIO_CAM_VGA_nSTBY);
	gpio_free(GPIO_CAM_VGA_nRST);	

	return 0;

}

static int sr130pc10_power_en(int onoff)
{
	int err=0;
	
	if (onoff != sr130pc10_powered_on) {
		if (onoff)
			err = sr130pc10_power_on();
		else {
			err = sr130pc10_power_off();
			s3c_i2c0_force_stop();
		}
		if (!err)
			sr130pc10_powered_on = onoff;
	}

	return 0;
}

int sr130pc10_power_reset(void)
{
	sr130pc10_power_en(0);
	sr130pc10_power_en(1);

	return 0;
}
static struct sr130pc10_platform_data sr130pc10_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_UYVY,
};

static struct i2c_board_info sr130pc10_i2c_info= {
	I2C_BOARD_INFO("SR130PC10", 0x40 >> 1),
	.platform_data = &sr130pc10_plat,
};

static struct s3c_platform_camera sr130pc10 = {
	.id		= CAMERA_PAR_A,
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_YCBYCR,
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD //NAGSM_Android_HQ_Camera_SungkooLee_20101022
	.i2c_busnum	= 17, //NAGSM_Android_HQ_Camera_SoojinKim_20110603
#else
	.i2c_busnum	= 0,
#endif
	.info		= &sr130pc10_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "xusbxti",
	.clk_name	= "sclk_cam",
	.clk_rate	= 24000000,
	.line_length	= 640,
	.width		= 640,
	.height		= 480,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

	/* Polarity */
	.inv_pclk	= 1, //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110114
	.inv_vsync 	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized 	= 0,
	.cam_power	= sr130pc10_power_en,
};
#endif

#ifdef CONFIG_VIDEO_M5MO
static bool m5mo_powered_on;

static int m5mo_power_on_sr130()
{

	int ret;
	
	ret = gpio_request(GPIO_CAM_CORE_EN, "GPC1(1)");
	if (ret) {
		printk(KERN_ERR "failed to request gpio(GPIO_CAM_CORE_EN)\n");
		return ret;
	}

	/* CAM_VGA_nSTBY - MP02(0)  */
	ret = gpio_request(GPIO_CAM_VGA_nSTBY, "MP02(0)");
	if (ret) {
		printk(KERN_ERR "failed to request GPIO for camera nSTBY pin\n");
		return ret;
	}

	/* CAM_VGA_nRST - MP02(1) */
	ret = gpio_request(GPIO_CAM_VGA_nRST, "MP02(1)");
        if (ret) {
		printk(KERN_ERR "failed to request GPIO for camera nRST pin\n");
                return ret;
        }

	/* VT_CORE_EN - MP01(0)  */ 
	if(HWREV >= 0x06)
	{
		ret = gpio_request(S5PV210_MP04(0), "MP04(0)");
		if (ret) {
			printk(KERN_ERR "failed to request GPIO for camera VT_CORE_EN pin\n");
			return ret;
		}
	}

	/* CAM_SENSOR_CORE_1.2V */
	ret = gpio_request(GPIO_CAM_IO_EN, "GPB7");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_IO_EN)\n");
		return ret;
	}

	/* CAM_ISP_CORE_1.2V */ 
	if (IS_ERR_OR_NULL(cam_isp_core_regulator)) {
		cam_isp_core_regulator = regulator_get(NULL, "cam_isp_core");
		if (IS_ERR_OR_NULL(cam_isp_core_regulator)) {
			pr_err("failed to get cam_isp_core regulator");
			return -EINVAL;
		}
	}
	pr_err("cam_isp_core_regulator = %p\n", cam_isp_core_regulator);

       /* CAM_AF_2.8V */		
	if (IS_ERR_OR_NULL(cam_af_regulator)) {
		cam_af_regulator = regulator_get(NULL, "cam_af");
		if (IS_ERR_OR_NULL(cam_af_regulator)) {
			pr_err("failed to get cam_af regulator");
			return -EINVAL;
		}
	}
	pr_err("cam_af_regulator = %p\n", cam_af_regulator);
	
	/* CAM_SENSOR_IO_1.8V */	
	if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
		cam_vga_avdd_regulator = regulator_get(NULL, "vga_avdd");
		if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
			pr_err("failed to get cam_vga_avdd_regulator");
			return -EINVAL;
		}
	}
	pr_err("cam_isp_host_regulator = %p\n", cam_vga_avdd_regulator);

	
	/* CAM_ISP_CORE_1.2V */ 
	ret = regulator_enable(cam_isp_core_regulator);
	if (ret) {
		pr_err("Failed to enable regulator cam_isp_core\n");
		return -EINVAL;
	}
	udelay(5);
		
	/* CAM_SENSOR_CORE_1.2V */
	gpio_direction_output(GPIO_CAM_CORE_EN, 1);
	gpio_free(GPIO_CAM_CORE_EN);
	udelay(2);

	/* CAM_AF_2.8V */		
	ret = regulator_enable(cam_af_regulator);
	if (ret) {
		pr_err("Failed to enable regulator cam_af_regulator\n");
		return -EINVAL;
	}
	mdelay(7);

	/* CAM_SENSOR_IO_A2.8V */
	gpio_direction_output(GPIO_CAM_IO_EN, 1);
	gpio_free(GPIO_CAM_IO_EN);
	udelay(1);

	/* CAM_SENSOR_IO_1.8V */	
	ret = regulator_enable(cam_vga_avdd_regulator);
	if (ret) {
		pr_err("Failed to enable cam_vga_avdd_regulator\n");
		return -EINVAL;
	}
	udelay(1);	

	/* CAM_ISP_1.8V */
//	max8998_ldo_enable_direct(MAX8998_LDO14);	
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
	REG_power_onoff(1); //on
#endif 	
	udelay(1);	

	/* Turn VT_CAM_SENSOR_A_2.8V */
#ifdef CONFIG_REGULATOR_MAX8893
	bh6173_ldo_enable_direct(1);//LDO 1
#endif
	udelay(100);

	// VT_CORE_EN was added in revision 0.5	/* VT_CORE_EN  HIGH */
	if(HWREV >= 0x06)
	{
		gpio_direction_output(S5PV210_MP04(0), 0);
		gpio_set_value(S5PV210_MP04(0), 1);
		udelay(100);
		gpio_free(S5PV210_MP04(0));
	}
	udelay(1);

	/* Turn VT_IO_1.8V on */
#ifdef CONFIG_REGULATOR_MAX8893
	bh6173_ldo_enable_direct(2);//LDO 2
#endif 
	udelay(300);

	/* CAM_VGA_nSTBY  HIGH */
	gpio_direction_output(GPIO_CAM_VGA_nSTBY, 0);
	gpio_set_value(GPIO_CAM_VGA_nSTBY, 1);
	udelay(10);

	/* MCLK */
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(2));	
	mdelay(12);

	/* CAM_VGA_nRST  HIGH */
	gpio_direction_output(GPIO_CAM_VGA_nRST, 0);
	gpio_set_value(GPIO_CAM_VGA_nRST, 1);	
	mdelay(2);

	/* CAM_VGA_nSTBY  LOW */
	gpio_direction_output(GPIO_CAM_VGA_nSTBY, 1);
	gpio_set_value(GPIO_CAM_VGA_nSTBY, 0);
	udelay(10);

	/*	ISP_RESET */
	//NAGSM_Android_HQ_Camera_SungkooLee_20101224 : GPIO_ISP_RESET was changed to MP04(1) in revision 0.5
	if(HWREV >= 0x06)
	{
		ret = gpio_request(S5PV210_MP04(1), "MP04(1)");
		if (ret) {
			printk(KERN_ERR "faile to request gpio(GPIO_ISP_RESET)\n");
			return ret;
		}
		gpio_direction_output(S5PV210_MP04(1), 1);
		gpio_free(S5PV210_MP04(1));
	}
	else
	{
		ret = gpio_request(GPIO_ISP_RESET, "MP01");
		if (ret) {
			printk(KERN_ERR "faile to request gpio(GPIO_ISP_RESET)\n");
			return ret;
		}
		gpio_direction_output(GPIO_ISP_RESET, 1);
		gpio_free(GPIO_ISP_RESET);
	}

	gpio_free(GPIO_CAM_VGA_nSTBY);
	gpio_free(GPIO_CAM_VGA_nRST);	

	mdelay(5);

	return ret;
}

static int m5mo_power_down_sr130()
{

	int ret;
	
	s3c_i2c0_force_stop();

	mdelay(5);

	/* CAM_VGA_nSTBY - GPB(0)  */
	ret = gpio_request(GPIO_CAM_VGA_nSTBY, "MP02(0)");
	if (ret) {
		printk(KERN_ERR "failed to request GPIO for camera nSTBY pin\n");
		return ret;
	}

	/* CAM_VGA_nRST - GPB(2) */
	ret = gpio_request(GPIO_CAM_VGA_nRST, "MP02(1)");
	if(ret) {
		printk(KERN_ERR "failed to request GPIO for camera nRST pin\n");
		return ret;
	}

	/* VT_CORE_EN - MP01(0)  */
	if(HWREV >= 0x06)
	{
		ret = gpio_request(S5PV210_MP04(0), "MP04(0)");
		if (ret) {
			printk(KERN_ERR "failed to request GPIO for camera VT_CORE_EN pin\n");
			return ret;
		}
	} 
	/* CAM_SENSOR_A2.8V */
	ret = gpio_request(GPIO_CAM_IO_EN, "GPB7");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_IO_EN)\n");
		return ret;
	}

	/* CAM_SENSOR_CORE_1.2V */
	ret = gpio_request(GPIO_CAM_CORE_EN, "GPC1(1)");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_CORE_EN)\n");
		return ret;
	}
	

	//NAGSM_Android_HQ_Camera_SungkooLee_20101224 : GPIO_ISP_RESET was changed to MP04(1) in revision 0.5
	/*	ISP_RESET */
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
	udelay(1);

	/* CAM_VGA_nRST  LOW */
	gpio_direction_output(GPIO_CAM_VGA_nRST, 1);
	gpio_set_value(GPIO_CAM_VGA_nRST, 0);
	mdelay(2);

	/* MCLK */
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(0));
	udelay(1);

	/* CAM_VGA_nSTBY  LOW */
	gpio_direction_output(GPIO_CAM_VGA_nSTBY, 1);
	gpio_set_value(GPIO_CAM_VGA_nSTBY, 0);
	udelay(200);

	/* Turn VT_IO_1.8V off */
#ifdef CONFIG_REGULATOR_MAX8893
	bh6173_ldo_disable_direct(2);//LDO 2
#endif
	udelay(1);


	// VT_CORE_EN was added in revision 0.5 
	/* VT_CORE_EN  LOW */
	if(HWREV >= 0x06)
	{
		gpio_direction_output(S5PV210_MP04(0), 1);
		gpio_set_value(S5PV210_MP04(0), 0);
		udelay(10);
		gpio_free(S5PV210_MP04(0));		
	} 
	udelay(100);

	/* Turn VT_CAM_SENSOR_A_2.8V*/
#ifdef CONFIG_REGULATOR_MAX8893
	bh6173_ldo_disable_direct(1);//LDO 1
#endif
	udelay(1);
	
	/* CAM_AF_2.8V */		
		if (IS_ERR_OR_NULL(cam_af_regulator)) {
		cam_af_regulator = regulator_get(NULL, "cam_af");
		if (IS_ERR_OR_NULL(cam_af_regulator)) {
			pr_err("failed to get cam_af regulator");
			return -EINVAL;
		}
	}
		pr_err("cam_af_regulator = %p\n", cam_af_regulator);
		ret = regulator_disable(cam_af_regulator);
	if (ret) {
		pr_err("Failed to disable regulator cam_isp_core\n");
		return -EINVAL;
	}
	udelay(5);	

		/* CAM_ISP_1.8V */
//	max8998_ldo_disable_direct(MAX8998_LDO14);
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
	REG_power_onoff(0); //off
#endif 	
	udelay(1);
	
	/* CAM_SENSOR_IO_1.8V */
	if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
		cam_vga_avdd_regulator = regulator_get(NULL, "vga_avdd");
		if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
			pr_err("failed to get cam_vga_avdd_regulator");
			return -EINVAL;
		}
	}
	pr_err("cam_isp_host_regulator = %p\n", cam_vga_avdd_regulator);
		ret = regulator_disable(cam_vga_avdd_regulator);
	if (ret) {
		pr_err("Failed to disable cam_vga_vddio_regulator\n");
		return -EINVAL;
	}
	udelay(1);	

	/* CAM_SENSOR_A2.8V */
	gpio_direction_output(GPIO_CAM_IO_EN, 0);
	gpio_free(GPIO_CAM_IO_EN);
	udelay(1);	

	/* CAM_SENSOR_CORE_1.2V */
	gpio_direction_output(GPIO_CAM_CORE_EN, 0);
	udelay(5);

	/* CAM_ISP_CORE_1.2V */ 
	if (IS_ERR_OR_NULL(cam_isp_core_regulator)) {
		cam_isp_core_regulator = regulator_get(NULL, "cam_isp_core");
		if (IS_ERR_OR_NULL(cam_isp_core_regulator)) {
			pr_err("failed to get cam_isp_core regulator");
			return -EINVAL;
		}
	}
	pr_err("cam_isp_core_regulator = %p\n", cam_isp_core_regulator);

	ret = regulator_disable(cam_isp_core_regulator);
	if (ret) {
		pr_err("Failed to enable regulator cam_isp_core\n");
		return -EINVAL;
	}
	gpio_free(GPIO_CAM_IO_EN);
	gpio_free(GPIO_CAM_CORE_EN);
	gpio_free(GPIO_CAM_VGA_nSTBY);
	gpio_free(GPIO_CAM_VGA_nRST);	

	return ret;

}

static int m5mo_power_on()
{

	int ret;
	
	ret = gpio_request(GPIO_CAM_CORE_EN, "GPC1(1)");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_CORE_EN)\n");
		return ret;
	}
	/* CAM_SENSOR_CORE_1.2V */
	gpio_direction_output(GPIO_CAM_CORE_EN, 1);
	gpio_free(GPIO_CAM_CORE_EN);

	/* CAM_ISP_CORE_1.2V */ 
	if (IS_ERR_OR_NULL(cam_isp_core_regulator)) {
		cam_isp_core_regulator = regulator_get(NULL, "cam_isp_core");
		if (IS_ERR_OR_NULL(cam_isp_core_regulator)) {
			pr_err("failed to get cam_isp_core regulator");
			return -EINVAL;
		}
	}
	pr_err("cam_isp_core_regulator = %p\n", cam_isp_core_regulator);
	ret = regulator_enable(cam_isp_core_regulator);
	if (ret) {
		pr_err("Failed to enable regulator cam_isp_core\n");
		return -EINVAL;
	}
	mdelay(2);

	ret = gpio_request(GPIO_CAM_IO_EN, "GPB7");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_IO_EN)\n");
		return ret;
	}
	gpio_direction_output(GPIO_CAM_IO_EN, 1);
	gpio_free(GPIO_CAM_IO_EN);
	mdelay(50);

	/* CAM_ISP_1.8V */
//	max8998_ldo_enable_direct(MAX8998_LDO14);	
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
	REG_power_onoff(1); //on
#endif 

	/* CAM_SENSOR_IO_1.8V */	
	if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
		cam_vga_avdd_regulator = regulator_get(NULL, "vga_avdd");
		if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
			pr_err("failed to get cam_vga_avdd_regulator");
			return -EINVAL;
		}
	}
	pr_err("cam_isp_host_regulator = %p\n", cam_vga_avdd_regulator);
	ret = regulator_enable(cam_vga_avdd_regulator);
	if (ret) {
		pr_err("Failed to enable cam_vga_avdd_regulator\n");
		return -EINVAL;
	}
	/* CAM_AF_2.8V */		

	if (IS_ERR_OR_NULL(cam_af_regulator)) {
		cam_af_regulator = regulator_get(NULL, "cam_af");
		if (IS_ERR_OR_NULL(cam_af_regulator)) {
			pr_err("failed to get cam_af regulator");
			return -EINVAL;
		}
	}
	pr_err("cam_af_regulator = %p\n", cam_af_regulator);
ret = regulator_enable(cam_af_regulator);
	if (ret) {
		pr_err("Failed to enable regulator cam_af_regulator\n");
		return -EINVAL;
	}
	mdelay(1);
	/* MCLK */
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(2));	
	mdelay(2);
	/*	ISP_RESET */

	//NAGSM_Android_HQ_Camera_SungkooLee_20101224 : GPIO_ISP_RESET was changed to MP04(1) in revision 0.5
	if(HWREV >= 0x06)
	{
		ret = gpio_request(S5PV210_MP04(1), "MP04(1)");
		if (ret) {
			printk(KERN_ERR "faile to request gpio(GPIO_ISP_RESET)\n");
			return ret;
		}
		gpio_direction_output(S5PV210_MP04(1), 1);
		gpio_free(S5PV210_MP04(1));
	}
	else
	{
		ret = gpio_request(GPIO_ISP_RESET, "MP01");
		if (ret) {
			printk(KERN_ERR "faile to request gpio(GPIO_ISP_RESET)\n");
			return ret;
		}
		gpio_direction_output(GPIO_ISP_RESET, 1);
		gpio_free(GPIO_ISP_RESET);
	}

	mdelay(5);

	return ret;

}

static int m5mo_power_down()
{

	int ret;
	

	s3c_i2c0_force_stop();
	//NAGSM_Android_HQ_Camera_SungkooLee_20101224 : GPIO_ISP_RESET was changed to MP04(1) in revision 0.5
	/*	ISP_RESET */
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
	/* MCLK */
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(0));
	mdelay(1);
	/* CAM_AF_2.8V */		
		if (IS_ERR_OR_NULL(cam_af_regulator)) {
		cam_af_regulator = regulator_get(NULL, "cam_af");
		if (IS_ERR_OR_NULL(cam_af_regulator)) {
			pr_err("failed to get cam_af regulator");
			return -EINVAL;
		}
	}
		pr_err("cam_af_regulator = %p\n", cam_af_regulator);
		ret = regulator_disable(cam_af_regulator);
	if (ret) {
		pr_err("Failed to disable regulator cam_isp_core\n");
		return -EINVAL;
	}	
	/* CAM_SENSOR_IO_1.8V */
	if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
		cam_vga_avdd_regulator = regulator_get(NULL, "vga_avdd");
		if (IS_ERR_OR_NULL(cam_vga_avdd_regulator)) {
			pr_err("failed to get cam_vga_avdd_regulator");
			return -EINVAL;
		}
	}
	pr_err("cam_isp_host_regulator = %p\n", cam_vga_avdd_regulator);
	ret = regulator_disable(cam_vga_avdd_regulator);
	if (ret) {
		pr_err("Failed to disable cam_vga_vddio_regulator\n");
		return -EINVAL;
	}

	/* CAM_ISP_1.8V */
//	max8998_ldo_disable_direct(MAX8998_LDO14);
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
	REG_power_onoff(0); //off
#endif 
	mdelay(50);	
	/* CAM_SENSOR_A2.8V */
	ret = gpio_request(GPIO_CAM_IO_EN, "GPB7");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_IO_EN)\n");
		return ret;
	}
	gpio_direction_output(GPIO_CAM_IO_EN, 0);
	gpio_free(GPIO_CAM_IO_EN);
	mdelay(2);
	/* CAM_ISP_CORE_1.2V */ 
	if (IS_ERR_OR_NULL(cam_isp_core_regulator)) {
		cam_isp_core_regulator = regulator_get(NULL, "cam_isp_core");
		if (IS_ERR_OR_NULL(cam_isp_core_regulator)) {
			pr_err("failed to get cam_isp_core regulator");
			return -EINVAL;
		}
	}
	pr_err("cam_isp_core_regulator = %p\n", cam_isp_core_regulator);

	ret = regulator_disable(cam_isp_core_regulator);
	if (ret) {
		pr_err("Failed to enable regulator cam_isp_core\n");
		return -EINVAL;
	}
	/* CAM_SENSOR_CORE_1.2V */
	ret = gpio_request(GPIO_CAM_CORE_EN, "GPC1(1)");
	if (ret) {
		printk(KERN_ERR "faile to request gpio(GPIO_CAM_CORE_EN)\n");
		return ret;
	}
	gpio_direction_output(GPIO_CAM_CORE_EN, 0);
	gpio_free(GPIO_CAM_CORE_EN);

	return ret;

}

extern unsigned int ldo3_status;
extern unsigned int ldo8_status; //Subhransu20110304

extern void __s5p_hdmi_phy_power_offtest(void); //Subhransu20110304


//Subhransu20110304
void ldo8_control_and_hdmi_phyoff_test()
{
#if 0 //SJKIM_TEMP
	printk(KERN_ERR "[%s]: ldo8_status = %d\n", __func__, ldo8_status);
	Set_MAX8998_PM_REG(ELDO8, 1 );
	ldo8_status = 1;
	__s5p_hdmi_phy_power_offtest();
#endif
	return 0;
}


extern int tv_power_status; 
static struct regulator *cam_mipi_c_regulator;
static struct regulator *cam_mipi_regulator;
void s3c_csis_power(int enable)
{
int err;

	if (enable) {
		if (ldo3_status == 0)
		{
		    cam_mipi_c_regulator = regulator_get(NULL, "usb_io");
			if (IS_ERR_OR_NULL(cam_mipi_c_regulator)) {
				pr_err("failed to get cam_mipi_c_regulator");
			}
    		err = regulator_enable(cam_mipi_c_regulator);
			if (err) {
					pr_err("Failed to enable cam_mipi_c_regulator\n");
					}
		}
		ldo3_status |= 1 << LDO3_MIPI;

		
	cam_mipi_regulator = regulator_get(NULL, "cam_vmipi");
			if (IS_ERR_OR_NULL(cam_mipi_regulator)) {
				pr_err("failed to get cam_mipi_regulator");
			}
	   		err = regulator_enable(cam_mipi_regulator);
			if (err) {
					pr_err("Failed to enable cam_mipi_regulator\n");
	}
	/*	if(!tv_power_status)
		{
			ldo8_control_and_hdmi_phyoff_test();
		
		
	}*/
	}
	else {
	cam_mipi_regulator = regulator_get(NULL, "cam_vmipi");
			if (IS_ERR_OR_NULL(cam_mipi_regulator)) {
				pr_err("failed to get cam_mipi_regulator");
			return -EINVAL;
			}
    		err = regulator_disable(cam_mipi_regulator);
			if (err) {
					pr_err("Failed to enable cam_mipi_regulator\n");
					return err;
					}
		ldo3_status &= ~(1 << LDO3_MIPI);
		if (ldo3_status == 0)	
		{
		        cam_mipi_c_regulator = regulator_get(NULL, "usb_io");
			if (IS_ERR_OR_NULL(cam_mipi_c_regulator)) {
				pr_err("failed to get cam_mipi_c_regulator");
			return -EINVAL;
	}
    		err = regulator_disable(cam_mipi_c_regulator);
			if (err) {
					pr_err("Failed to disable cam_mipi_c_regulator\n");
					return err;
					}
		}
	}

}

static int m5mo_power(int enable)
{
	int ret;

	printk("%s %s : mach, HWREV 0x%x\n", __func__, enable ? "on" : "down", HWREV);	

	if (enable != m5mo_powered_on) {
		if(HWREV >= 0x07)
		{
			if(enable)
				ret = m5mo_power_on_sr130();
			else
				ret = m5mo_power_down_sr130();
		}
		else
		{
			if(enable)
				ret = m5mo_power_on();
			else
				ret = m5mo_power_down();
		}
		if (!ret)
			m5mo_powered_on = enable;

		s3c_csis_power(enable);
	}
	

	return ret;
	}

static int m5mo_config_isp_irq()
{	

	s3c_gpio_cfgpin(GPIO_ISP_INT, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(GPIO_ISP_INT, S3C_GPIO_PULL_NONE);

	return 0;
}

/* External camera module setting */
static struct m5mo_platform_data m5mo_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 1,
	.config_isp_irq = m5mo_config_isp_irq,
};

static struct i2c_board_info  m5mo_i2c_info = {
	I2C_BOARD_INFO("M5MO", 0x1F),
	.platform_data = &m5mo_plat,
	.irq = IRQ_EINT10,
};

static struct s3c_platform_camera m5mo = {
	.id		= CAMERA_PAR_A,
	.type		= CAM_TYPE_MIPI,//bestiq_MIPI
//bestiq_parallel	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 0,
	.info		= &m5mo_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "xusbxti",
	.clk_name	= "sclk_cam",
	.clk_rate	= 24000000,
	.line_length	= 1920,
	.width		= 1920,
	.height		= 1080,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

	.mipi_lanes	= 2,
	.mipi_settle	= 12,
	.mipi_align	= 32,
	
	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync 	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized 	= 0,
	.cam_power	= m5mo_power,
};
#endif
#endif 
/* Interface setting */
static struct s3c_platform_fimc fimc_plat_lsi = {
	.srclk_name	= "mout_mpll",
	.clk_name	= "sclk_fimc",
	.lclk_name	= "sclk_fimc_lclk",
	.clk_rate	= 166750000,
	.default_cam	= CAMERA_PAR_A,
	.camera		= {
#ifdef CONFIG_VIDEO_CE147
		&ce147,
#endif
#ifdef CONFIG_VIDEO_S5K5CCGX
		&s5k5ccgx,
#endif
#ifdef CONFIG_VIDEO_M5MO
		&m5mo,
#endif
#ifdef CONFIG_VIDEO_SR130PC10 //NAGSM_Android_HQ_Camera_SungkooLee_20101230
		&sr130pc10,
#endif
#ifdef CONFIG_VIDEO_S5KA3DFX
		&s5ka3dfx,
#endif
#ifdef CONFIG_VIDEO_SR030PC30
		&sr030pc30,
#endif

	},
	.hw_ver		= 0x43,
};

#ifdef CONFIG_VIDEO_JPEG_V2
static struct s3c_platform_jpeg jpeg_plat __initdata = {
#if defined(CONFIG_S5PC110_DEMPSEY_BOARD)
	.max_main_width	= 1280,
	.max_main_height	= 960,
#else
	.max_main_width	= 800,
	.max_main_height	= 480,
#endif
	.max_thumb_width	= 320,
	.max_thumb_height	= 240,
};
#endif

/* I2C0 */
static struct i2c_board_info i2c_devs0[] __initdata = {
};

static struct i2c_board_info i2c_devs4[] __initdata = {
	{
		I2C_BOARD_INFO("wm8994", (0x34>>1)),
		.platform_data = &wm8994_pdata,
	},
};

/* I2C1 */
static struct i2c_board_info i2c_devs1[] __initdata = {
};

#ifdef CONFIG_TOUCHSCREEN_QT602240
/* I2C2 */
static struct i2c_board_info i2c_devs2[] __initdata = {
	{
		I2C_BOARD_INFO("qt602240_ts", 0x4a),
		.irq = IRQ_EINT_GROUP(18, 5),
	},
};
#endif

#ifdef CONFIG_TOUCHSCREEN_MXT224
static void mxt224_power_on(void)
{
	gpio_direction_output(GPIO_TOUCH_EN, 1);

	mdelay(40);
}

static void mxt224_power_off(void)
{
	gpio_direction_output(GPIO_TOUCH_EN, 0);
}

#define MXT224_MAX_MT_FINGERS 5

static u8 t7_config[] = {GEN_POWERCONFIG_T7,
				64, 255, 50};
static u8 t8_config[] = {GEN_ACQUISITIONCONFIG_T8,
				7, 0, 5, 0, 0, 0, 9, 35};
static u8 t9_config[] = {TOUCH_MULTITOUCHSCREEN_T9,
				139, 0, 0, 19, 11, 0, 32, 25, 2, 1, 25, 3, 1,
				46, MXT224_MAX_MT_FINGERS, 5, 14, 10, 255, 3,
				255, 3, 18, 18, 10, 10, 141, 65, 143, 110, 18};
static u8 t18_config[] = {SPT_COMCONFIG_T18,
				0, 1};
static u8 t20_config[] = {PROCI_GRIPFACESUPPRESSION_T20,
				7, 0, 0, 0, 0, 0, 0, 80, 40, 4, 35, 10};
static u8 t22_config[] = {PROCG_NOISESUPPRESSION_T22,
				5, 0, 0, 0, 0, 0, 0, 3, 30, 0, 0, 29, 34, 39,
				49, 58, 3};
static u8 t28_config[] = {SPT_CTECONFIG_T28,
				1, 0, 3, 16, 63, 60};
static u8 end_config[] = {RESERVED_T255};

static const u8 *mxt224_config[] = {
	t7_config,
	t8_config,
	t9_config,
	t18_config,
	t20_config,
	t22_config,
	t28_config,
	end_config,
};

static struct mxt224_platform_data mxt224_data = {
	.max_finger_touches = MXT224_MAX_MT_FINGERS,
	.gpio_read_done = GPIO_TOUCH_INT,
	.config = mxt224_config,
	.min_x = 0,
	.max_x = 1023,
	.min_y = 0,
	.max_y = 1023,
	.min_z = 0,
	.max_z = 255,
	.min_w = 0,
	.max_w = 30,
	.power_on = mxt224_power_on,
	.power_off = mxt224_power_off,
};

/* I2C2 */
static struct i2c_board_info i2c_devs2[] __initdata = {
	{
		I2C_BOARD_INFO(MXT224_DEV_NAME, 0x4a),
		.platform_data = &mxt224_data,
		.irq = IRQ_EINT_GROUP(18, 5),
	},
};
#endif


#if defined(CONFIG_TOUCHSCREEN_MXT224E)
static struct regulator *tspvdd = NULL, *tspavdd = NULL;

static void mxt224_power_on(void)
{

printk("mxt224_power_on\n");
#if 0
	s3c_gpio_cfgpin(GPIO_TSP_LDO_ON, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TSP_LDO_ON, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_TSP_LDO_ON, GPIO_LEVEL_HIGH);
	mdelay(70);
	s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_SFN(0xf));
	/*mdelay(40); */
	/* printk("mxt224_power_on is finished\n"); */



	int tint = GPIO_TOUCH_INT;
	s3c_gpio_cfgpin(tint, S3C_GPIO_INPUT);
	s3c_gpio_setpull(tint, S3C_GPIO_PULL_UP);

tspvdd = regulator_get(NULL, "tsp_vdd");
	if (IS_ERR(tspvdd)) {
			printk(KERN_ERR "%s: cant get TSP_VDD\n", __func__);
	return;
	}
	printk("mxt224_power_on vdd\n");

tspavdd = regulator_get(NULL, "tsp_avdd");
	if (IS_ERR(tspvdd)) {
			printk(KERN_ERR "%s: cant get TSP_AVDD\n", __func__);
	return;
	}	
	printk("mxt224_power_on avdd\n");

//regulator_disable(tspvdd);
//regulator_disable(tspavdd);


msleep(10);
regulator_enable(tspvdd);
regulator_enable(tspavdd);
#endif



}

static void mxt224_power_off(void)
{
#if 0
	s3c_gpio_cfgpin(GPIO_TSP_INT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_TSP_INT, S3C_GPIO_PULL_DOWN);

	s3c_gpio_cfgpin(GPIO_TSP_LDO_ON, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TSP_LDO_ON, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_TSP_LDO_ON, GPIO_LEVEL_LOW);
	/* printk("mxt224_power_off is finished\n"); */
#endif
}

static void mxt224_register_callback(void *function)
{
	printk("mxt224_register_callback\n");

	//charging_cbs.tsp_set_charging_cable = function;
}

static void mxt224_read_ta_status(bool *ta_status)
{
	
	printk("mxt224_read_ta_status\n");
	

	//*ta_status = is_cable_attached;
}

/*
	Configuration for MXT224
*/
static u8 t7_config[] = {GEN_POWERCONFIG_T7,
				48,		/* IDLEACQINT */
				255,	/* ACTVACQINT */
				25		/* ACTV2IDLETO: 25 * 200ms = 5s */};
static u8 t8_config[] = {GEN_ACQUISITIONCONFIG_T8,
				10, 0, 5, 1, 0, 0, 9, 30};/*byte 3: 0*/
static u8 t9_config[] = {TOUCH_MULTITOUCHSCREEN_T9,
				131, 0, 0, 19, 11, 0, 32, MXT224_THRESHOLD, 2, 1,
				0,
				15,		/* MOVHYSTI */
				1, 11, MXT224_MAX_MT_FINGERS, 5, 40, 10, 31, 3,
				223, 1, 0, 0, 0, 0, 143, 55, 143, 90, 18};

static u8 t18_config[] = {SPT_COMCONFIG_T18,
				0, 1};
static u8 t20_config[] = {PROCI_GRIPFACESUPPRESSION_T20,
				7, 0, 0, 0, 0, 0, 0, 30, 20, 4, 15, 10};
static u8 t22_config[] = {PROCG_NOISESUPPRESSION_T22,
				143, 0, 0, 0, 0, 0, 0, 3, 30, 0, 0,  29, 34, 39,
				49, 58, 3};
static u8 t28_config[] = {SPT_CTECONFIG_T28,
				0, 0, 3, 16, 19, 60};
static u8 end_config[] = {RESERVED_T255};

static const u8 *mxt224_config[] = {
	t7_config,
	t8_config,
	t9_config,
	t18_config,
	t20_config,
	t22_config,
	t28_config,
	end_config,
};

/*
	Configuration for MXT224-E
*/

static u8 t7_config_e[] = {GEN_POWERCONFIG_T7,
				32, 255, 50};
static u8 t8_config_e[] = {GEN_ACQUISITIONCONFIG_T8,
				27, 0, 5, 1, 0, 0, 8, 8, 40, 55};

/* MXT224E_0V5_CONFIG */
/* NEXTTCHDI added */
static u8 t9_config_e[] = {TOUCH_MULTITOUCHSCREEN_T9,
				139, 0, 0, 19, 11, 0, 16, 35, 2, 1,
				10, 3, 1, 0, MXT224_MAX_MT_FINGERS, 5, 40, 10, 31, 3,
				223, 1, 10, 10, 10, 10, 143, 40, 143, 80,
				18, 15, 50, 50, 2};

static u8 t15_config_e[] = {TOUCH_KEYARRAY_T15,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static u8 t18_config_e[] = {SPT_COMCONFIG_T18,
				0, 0};

static u8 t23_config_e[] = {TOUCH_PROXIMITY_T23,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static u8 t25_config_e[] = {SPT_SELFTEST_T25,
				0, 0, 3000, 0, 0, 0, 0, 0};

static u8 t40_config_e[] = {PROCI_GRIPSUPPRESSION_T40,
				0, 0, 0, 0, 0};

static u8 t42_config_e[] = {PROCI_TOUCHSUPPRESSION_T42,
				0, 0, 0, 0, 0, 0, 0, 0};

static u8 t46_config_e[] = {SPT_CTECONFIG_T46,
				0, 3, 16, 28, 0, 0, 1, 0, 0};

static u8 t47_config_e[] = {PROCI_STYLUS_T47,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/*MXT224E_0V5_CONFIG */
static u8 t48_config_e[] = {PROCG_NOISESUPPRESSION_T48,
				1, 12, 112, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 6, 6, 0, 0, 100, 4, 64,
				10, 0, 20, 5, 0, 38, 0, 20, 0, 0,
				0, 0, 0, 0, 0, 55, 2, 5, 2, 0,
				5, 10, 10, 0, 0, 16, 17, 146, 60, 149,
				68, 25, 15, 3};



static u8 end_config_e[] = {RESERVED_T255};

static const u8 *mxt224e_config[] = {
	t7_config_e,
	t8_config_e,
	t9_config_e,
	t15_config_e,
	t18_config_e,
	t23_config_e,
	t25_config_e,
	t40_config_e,
	t42_config_e,
	t46_config_e,
	t47_config_e,
	t48_config_e,
	end_config_e,
};


static struct mxt224_platform_data mxt224_data = {
	.max_finger_touches = MXT224_MAX_MT_FINGERS,
	.gpio_read_done =  IRQ_EINT_GROUP(18, 5),
	.config = mxt224_config,
	.config_e = mxt224e_config,
	.min_x = 0,
	.max_x = 480,
	.min_y = 0,
	.max_y = 800,
	.min_z = 0,
	.max_z = 255,
	.min_w = 0,
	.max_w = 30,
	.power_on = mxt224_power_on,
	.power_off = mxt224_power_off,
	.register_cb = mxt224_register_callback,
	.read_ta_status = mxt224_read_ta_status,
};




static struct i2c_board_info i2c_devs2[] __initdata = {
	{
		I2C_BOARD_INFO(MXT224_DEV_NAME, 0x4a),
		.platform_data  = &mxt224_data,
	},
};



#endif

/* I2C10 */
static struct i2c_board_info i2c_devs10[] __initdata = {
	{
		I2C_BOARD_INFO(CYPRESS_TOUCHKEY_DEV_NAME, 0x20),
		.platform_data = &touchkey_data,
		.irq = (IRQ_EINT_GROUP22_BASE + 1),
	},
};

#if defined (CONFIG_INPUT_BMA222)
static struct i2c_board_info i2c_devs5[] __initdata = {
	{
		I2C_BOARD_INFO("bma222", 0x08),
	},
};
#elif defined(CONFIG_SENSORS_K3DH) || defined(CONFIG_GYRO_K3G)
static struct k3dh_platform_data k3dh_data = {
        .gpio_acc_int = S5PV210_GPH0(1),	
};



static struct k3g_platform_data k3g_pdata = {
        .axis_map_x = 1,
        .axis_map_y = 1,
        .axis_map_z = 1,
        .negate_x = 0,
        .negate_y = 0,
        .negate_z = 0,
};


static struct i2c_board_info i2c_devs5[] __initdata = {
        {
                I2C_BOARD_INFO("k3g", 0x69),
                .irq = IRQ_EINT(19),		
               .platform_data = &k3g_pdata,

        },
        {
                I2C_BOARD_INFO("k3dh", 0x19),
              .platform_data  = &k3dh_data,
        },
};

#else
static struct i2c_board_info i2c_devs5[] __initdata = {
	{
		I2C_BOARD_INFO("bma023", 0x38),
	},
};
#endif

#if !defined (CONFIG_S5PC110_DEMPSEY_BOARD)	
static struct i2c_board_info i2c_devs8[] __initdata = {
	{
		I2C_BOARD_INFO("Si4709", 0x20 >> 1),
		.irq = (IRQ_EINT_GROUP20_BASE + 4), /* J2_4 */
	},
};
#endif

static int fsa9480_init_flag = 0;
static bool mtp_off_status;
extern int max8998_check_vdcin();
static void fsa9480_usb_cb(bool attached)
{
	struct usb_gadget *gadget = platform_get_drvdata(&s3c_device_usbgadget);

	if (gadget) {
		if (attached)
			usb_gadget_vbus_connect(gadget);
		else
			usb_gadget_vbus_disconnect(gadget);
	}

	mtp_off_status = false;
#if !defined (CONFIG_S5PC110_HAWK_BOARD) && !defined (CONFIG_S5PC110_KEPLER_BOARD) && !defined (CONFIG_S5PC110_DEMPSEY_BOARD) && !defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)		// mr work
       if( max8998_check_vdcin())
	set_cable_status = attached ? CABLE_TYPE_USB : CABLE_TYPE_NONE;
	else
	set_cable_status = CABLE_TYPE_NONE;	

	if (charger_callbacks && charger_callbacks->set_cable)
		charger_callbacks->set_cable(charger_callbacks, set_cable_status);
#endif
	
}

static void fsa9480_charger_cb(bool attached)
{

#if !defined (CONFIG_S5PC110_HAWK_BOARD) && !defined (CONFIG_S5PC110_KEPLER_BOARD) && !defined (CONFIG_S5PC110_DEMPSEY_BOARD) && !defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD) // mr work
       if( max8998_check_vdcin())
	set_cable_status = attached ? CABLE_TYPE_AC : CABLE_TYPE_NONE;
	else
	set_cable_status = CABLE_TYPE_NONE;	

	if (charger_callbacks && charger_callbacks->set_cable)
		charger_callbacks->set_cable(charger_callbacks, set_cable_status);
#endif

}

static struct switch_dev switch_dock = {
	.name = "dock",
};

static void fsa9480_deskdock_cb(bool attached)
{

struct usb_gadget *gadget = platform_get_drvdata(&s3c_device_usbgadget);	//Build Error

	if (attached)
		switch_set_state(&switch_dock, 1);
	else
		switch_set_state(&switch_dock, 0);
		
#if !defined (CONFIG_S5PC110_HAWK_BOARD) && !defined (CONFIG_S5PC110_KEPLER_BOARD) && !defined (CONFIG_S5PC110_DEMPSEY_BOARD) && !defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD) // mr work
	if (gadget) 
	{
		if (attached)
			usb_gadget_vbus_connect(gadget);
		else
			usb_gadget_vbus_disconnect(gadget);
	}

	mtp_off_status = false;

       if( max8998_check_vdcin())
	set_cable_status = attached ? CABLE_TYPE_USB : CABLE_TYPE_NONE;
	else
	set_cable_status = CABLE_TYPE_NONE;	

	   
	if (charger_callbacks && charger_callbacks->set_cable)
		charger_callbacks->set_cable(charger_callbacks, set_cable_status);
#endif
	
}

static void fsa9480_cardock_cb(bool attached)
{
 
struct usb_gadget *gadget = platform_get_drvdata(&s3c_device_usbgadget);
	if (attached)
		switch_set_state(&switch_dock, 2);
	else
		switch_set_state(&switch_dock, 0);
		
#if !defined (CONFIG_S5PC110_HAWK_BOARD) && !defined (CONFIG_S5PC110_KEPLER_BOARD) && !defined (CONFIG_S5PC110_DEMPSEY_BOARD) && !defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD) // mr work
//#if 0 /* doodlejump */
// HDLNC_OPK_20110324 : For USB Charging in Cardock mode		
	if (gadget) 
	{
		if (attached)
			usb_gadget_vbus_connect(gadget);
		else
			usb_gadget_vbus_disconnect(gadget);
	}

	mtp_off_status = false;

       if( max8998_check_vdcin())
	set_cable_status = attached ? CABLE_TYPE_USB : CABLE_TYPE_NONE;
	else
	set_cable_status = CABLE_TYPE_NONE;		

	if (charger_callbacks && charger_callbacks->set_cable)
		charger_callbacks->set_cable(charger_callbacks, set_cable_status);
#endif	
//#endif
}

static void fsa9480_reset_cb(void)
{
	int ret;

	/* for CarDock, DeskDock */
	ret = switch_dev_register(&switch_dock);
	if (ret < 0)
		pr_err("Failed to register dock switch. %d\n", ret);
}

static void fsa9480_set_init_flag(void)
{
	fsa9480_init_flag = 1;
}

static struct fsa9480_platform_data fsa9480_pdata = {
	.usb_cb = fsa9480_usb_cb,
	.charger_cb = fsa9480_charger_cb,
	.deskdock_cb = fsa9480_deskdock_cb,
	.cardock_cb = fsa9480_cardock_cb,
	.reset_cb = fsa9480_reset_cb,
	.set_init_flag = fsa9480_set_init_flag,
};

static struct i2c_board_info i2c_devs7[] __initdata = {
	{
		I2C_BOARD_INFO("fsa9480", 0x4A >> 1),
		.platform_data = &fsa9480_pdata,
		.irq = IRQ_EINT(23),
	},
};

static struct i2c_board_info i2c_devs6[] __initdata = {
#ifdef CONFIG_REGULATOR_MAX8998
	{
		/* The address is 0xCC used since SRAD = 0 */
		I2C_BOARD_INFO("max8998", (0xCC >> 1)),
		.platform_data	= &max8998_pdata,
		.irq		= IRQ_EINT7,
	}, {
		I2C_BOARD_INFO("rtc_max8998", (0x0D >> 1)),
	},
#endif
};

#if !(defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined (CONFIG_S5PC110_DEMPSEY_BOARD))	
static struct pn544_i2c_platform_data pn544_pdata = {
	.irq_gpio = NFC_IRQ,
	.ven_gpio = NFC_EN,
	.firm_gpio = NFC_FIRM,
};

static struct i2c_board_info i2c_devs14[] __initdata = {
	{
		I2C_BOARD_INFO("pn544", 0x2b),
		.irq = IRQ_EINT(12),
		.platform_data = &pn544_pdata,
	},
};
#endif
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
static  struct  i2c_gpio_platform_data  i2c17_platdata = {
        .sda_pin                = GPIO_VT_CAM_SDA_18V,
        .scl_pin                = GPIO_VT_CAM_SCL_18V,
        .udelay                 = 5,    /* 250KHz */
        .sda_is_open_drain      = 0,
        .scl_is_open_drain      = 0,
        .scl_is_output_only     = 0,
};

static struct platform_device s3c_device_i2c17 = {
        .name                           = "i2c-gpio",
        .id                                     = 17,
        .dev.platform_data      = &i2c17_platdata,
};
#endif

#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)	
#ifdef CONFIG_REGULATOR_BH6173
static struct regulator_init_data bh6173_ldo1_data = {
        .constraints    = {
                .name           = "VT_CAMA_2.8V",
                .min_uV         = 2800000,
                .max_uV         = 2800000,
                .always_on      = 0,
                .apply_uV       = 1,
                .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
        },
};

static struct regulator_init_data bh6173_ldo2_data = {
        .constraints    = {
                .name           = "VGA_IO_1.8V",
                .min_uV         = 1800000,
                .max_uV         = 1800000,
                .always_on      = 0,
                .apply_uV       = 1,
                .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
        },
};

static struct regulator_init_data bh6173_ldo3_data = {
        .constraints    = {
                .name           = "LED_A_2.8V",
                .min_uV         = 2800000,
                .max_uV         = 2800000,
                .always_on      = 0,
                .apply_uV       = 1,
                .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
        },
};

static struct bh6173_subdev_data universal_bh6173_regulators[] = {
        { BH6173_LDO1, &bh6173_ldo1_data },
        { BH6173_LDO2, &bh6173_ldo2_data },
	{ BH6173_LDO3, &bh6173_ldo3_data },
};


static struct bh6173_platform_data bh6173_platform_data = {
        .num_regulators = ARRAY_SIZE(universal_bh6173_regulators),
        .regulators     = universal_bh6173_regulators,
};


static struct i2c_board_info i2c_devs16[] __initdata = {
        {
                I2C_BOARD_INFO("bh6173", (0x4A)),
                .platform_data = &bh6173_platform_data,
        },
};

static  struct  i2c_gpio_platform_data  i2c16_platdata = {
        .sda_pin                = GPIO_CAM_LDO_SDA,
        .scl_pin                = GPIO_CAM_LDO_SCL,
        .udelay                 = 2,    /* 250KHz */
        .sda_is_open_drain      = 0,
        .scl_is_open_drain      = 0,
        .scl_is_output_only     = 0,
};

static struct platform_device s3c_device_i2c16 = {
        .name                   = "i2c-gpio",
        .id                     = 16,
        .dev.platform_data      = &i2c16_platdata,
};

#endif



#ifdef CONFIG_REGULATOR_MAX8893

static  struct  i2c_gpio_platform_data  i2c15_platdata = {
        .sda_pin                = GPIO_SUBPM_SDA_28V,
        .scl_pin                = GPIO_SUBPM_SCL_28V,
        .udelay                 = 2,    /* 250KHz */
        .sda_is_open_drain      = 0,
        .scl_is_open_drain      = 0,
        .scl_is_output_only     = 0,
};

static struct platform_device s3c_device_i2c15 = {
        .name                   = "i2c-gpio",
        .id                     = 15,
        .dev.platform_data      = &i2c15_platdata,
};


static struct regulator_init_data max8893_ldo1_data = {
        .constraints    = {
                .name           = "VMEM_VDDF_3.0V",
                .min_uV         = 3000000,
                .max_uV         = 3000000,
		.always_on	= 1,
                .apply_uV       = 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
        },
};



static struct regulator_init_data max8893_ldo2_data = {
        .constraints    = {
                .name           = "VMEM_VDD_2.8V",
                .min_uV         = 2800000,   
                .max_uV         = 2800000,  
		.always_on	= 1,
                .apply_uV       = 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
        },
};



static struct regulator_init_data max8893_ldo3_data = {
        .constraints    = {
                .name           = "VCC_3.0_MOTOR",
                .min_uV         = 3000000,   
                .max_uV         = 3000000,  
		.always_on	= 0,
                .apply_uV       = 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
        },
};


static struct regulator_consumer_supply ldo4_mhl_consumer[] = {
	REGULATOR_SUPPLY("mhl_1p8v", NULL),
};
static struct regulator_init_data max8893_ldo4_data = {
        .constraints    = {
                .name           = "VCC_1.8V_MHL",
                .min_uV         = 1800000,   
                .max_uV         = 1800000,  
		.always_on	= 0,
                .apply_uV       = 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
        },
        .num_consumer_supplies	= ARRAY_SIZE(ldo4_mhl_consumer),
	.consumer_supplies	= ldo4_mhl_consumer,
};

static struct regulator_consumer_supply ldo5_mhl_consumer[] = {
	REGULATOR_SUPPLY("mhl_3p3v", NULL),
};
static struct regulator_init_data max8893_ldo5_data = {
        .constraints    = {
                .name           = "VCC_3.3V_MHL",
                .min_uV         = 3300000,   
                .max_uV         = 3300000,  
		.always_on	= 0,
                .apply_uV       = 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
        },
        .num_consumer_supplies	= ARRAY_SIZE(ldo5_mhl_consumer),
	.consumer_supplies	= ldo5_mhl_consumer,
};

static struct regulator_consumer_supply buck_mhl_consumer[] = {
	REGULATOR_SUPPLY("mhl_1p2v", NULL),
};
static struct regulator_init_data max8893_buck_data = {
        .constraints    = {
                .name           = "VSIL_1.2A",
                .min_uV         = 1200000,   
                .max_uV         = 1200000,  
		.always_on	= 0,
                .apply_uV       = 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
        },
        .num_consumer_supplies	= ARRAY_SIZE(buck_mhl_consumer),
	.consumer_supplies	= buck_mhl_consumer,
};


static struct max8893_subdev_data universal_8893_regulators[] = {
	{ MAX8893_LDO1, &max8893_ldo1_data },
	{ MAX8893_LDO2, &max8893_ldo2_data },
	{ MAX8893_LDO3, &max8893_ldo3_data },
	{ MAX8893_LDO4, &max8893_ldo4_data },
	{ MAX8893_LDO5, &max8893_ldo5_data },
	{ MAX8893_BUCK, &max8893_buck_data },
};

static struct max8893_platform_data max8893_platform_data = {
	.num_regulators	= ARRAY_SIZE(universal_8893_regulators),
	.regulators	= universal_8893_regulators,
};

static struct i2c_board_info i2c_devs15[] __initdata = {
	{
		I2C_BOARD_INFO("max8893", (0x3E)),
		.platform_data = &max8893_platform_data,
	},
};

struct platform_device s3c_device_8893consumer = {
        .name             = "max8893-consumer",
        .id               = 0,
  	.dev = { .platform_data = &max8893_platform_data },
};
#endif //CONFIG_REGULATOR_MAX8893
#endif

static int max17040_power_supply_register(struct device *parent,
	struct power_supply *psy)
{
	aries_charger.psy_fuelgauge = psy;
	return 0;
}

static void max17040_power_supply_unregister(struct power_supply *psy)
{
	aries_charger.psy_fuelgauge = NULL;
}

static struct max17040_platform_data max17040_pdata = {
	.power_supply_register = max17040_power_supply_register,
	.power_supply_unregister = max17040_power_supply_unregister,
	
#if  defined(CONFIG_S5PC110_KEPLER_BOARD)
	.rcomp_value = 0xD000,
#elif defined(CONFIG_S5PC110_HAWK_BOARD)		
	.rcomp_value = 0xB000,
#elif defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD)	
	.rcomp_value = 0xD000,
#elif defined (CONFIG_S5PC110_DEMPSEY_BOARD)		// mr work
	.rcomp_value = 0xD000,
#endif		

};

static struct i2c_board_info i2c_devs9[] __initdata = {
	{
		I2C_BOARD_INFO("max17040", (0x6D >> 1)),
		.platform_data = &max17040_pdata,
#if  defined(CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_HAWK_BOARD) || defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined (CONFIG_S5PC110_DEMPSEY_BOARD)	// mr work
		.irq = IRQ_EINT(27),
#endif		
	},
};

#if defined (CONFIG_OPTICAL_TAOS_TRITON)	/* nat */
static struct regulator *optical_taos_triton_regulator; /*LDO 13*/
static int optical_taos_intialized;
const unsigned long optical_taos_triton_LDO_volt = 3000000;

extern int taos_api_power_on(void);
extern int taos_api_power_off(void);
extern int taos_api_get_light_adcvalue(void);

static void taos_gpio_init(void)
{
	printk("taos_gpio_init\n");
	optical_taos_intialized = 0;	
}

static int taos_power(bool on)
{
	int err = 0;

	/* this controls the power supply rail to the taos IC */
	printk("[T759] taos_power on=[%d]\n", on );

	if(optical_taos_intialized == 0) {
		if (IS_ERR_OR_NULL(optical_taos_triton_regulator)) {

			optical_taos_triton_regulator = regulator_get(NULL, "taos_triton");
			printk("optical_taos_triton_regulator = %p\n", optical_taos_triton_regulator);

			if (IS_ERR_OR_NULL(optical_taos_triton_regulator)) {
				pr_err("[ERROR] failed to get optical_taos_triton_regulator");
				return -1;
			}
			
		}		
		optical_taos_intialized = 1;
	}

	if (IS_ERR_OR_NULL(optical_taos_triton_regulator) ) {
		pr_err("optical_taos_triton_regulator  not initialized\n");
		return -EINVAL;
	}

	if ( on ) 	{
	
		regulator_set_voltage(optical_taos_triton_regulator, optical_taos_triton_LDO_volt, optical_taos_triton_LDO_volt);

		/* Turn LDO13*/
		err = regulator_enable(optical_taos_triton_regulator);
		if (err) {
			pr_err("[ERROR] Failed to enable optical_taos_triton_regulator \n");
			return err;
		}

		taos_api_power_on();
	} 
	else {
	
		taos_api_power_off();
		
		err = regulator_disable(optical_taos_triton_regulator);
		if (err) {
			pr_err("[ERROR]  Failed to disable optical_taos_triton_regulator \n");
			return err;
		}		
	}

	return 0;
}

static int taos_light_adc_value(void)
{
	/* this function seems to be deprecated. */
	int ret = 0;

	ret = taos_api_get_light_adcvalue();

	printk("(%s) adcvalue = [%d]\n", __func__, ret );
	
	return ret; 
}

static struct taos_platform_data taos_pdata = {
	.power = taos_power,
	.als_int = GPIO_TAOS_ALS,
	.light_adc_value = taos_light_adc_value
};

static struct i2c_board_info i2c_devs11[] __initdata = {
	{
		I2C_BOARD_INFO("gp2a", (0x39 )),	
		.platform_data = &taos_pdata,
	},
};
#elif defined(CONFIG_OPTICAL_GP2A) /* defined (CONFIG_OPTICAL_TAOS_TRITON)	*/

static void gp2a_gpio_init(void)
{
	int ret = gpio_request(GPIO_PS_ON, "gp2a_power_supply_on");
	if (ret)
		printk(KERN_ERR "Failed to request gpio gp2a power supply.\n");
}

static int gp2a_power(bool on)
{
	/* this controls the power supply rail to the gp2a IC */
	gpio_direction_output(GPIO_PS_ON, on);
	return 0;
}

static int gp2a_light_adc_value(void)
{
	return s3c_adc_get_adc_data(9);
}

static struct gp2a_platform_data gp2a_pdata = {
	.power = gp2a_power,
	.p_out = GPIO_PS_VOUT,
	.light_adc_value = gp2a_light_adc_value
};

static struct i2c_board_info i2c_devs11[] __initdata = {
	{
		I2C_BOARD_INFO("gp2a", (0x88 >> 1)),
		.platform_data = &gp2a_pdata,
	},
};
#elif defined(CONFIG_OPTICAL_CAPELLA_TRITON)

static int cm3663_ldo(bool on)
{
	if(on)
		bh6173_ldo_enable_direct(3);
	else
		bh6173_ldo_disable_direct(3);
	return 0;
}
static struct cm3663_platform_data cm3663_pdata = {
        .proximity_power = cm3663_ldo,
};


static struct i2c_board_info i2c_devs11[] __initdata = {
	{
		I2C_BOARD_INFO("cm3663", 0x20),
		.irq = GPIO_PS_ALS_INT,
		.platform_data = &cm3663_pdata,
	},
};
#endif /* defined (CONFIG_OPTICAL_TAOS_TRITON)	*/

#if defined (CONFIG_SENSORS_AK8975) 
static struct akm8975_platform_data akm8975_pdata = {
        .gpio_data_ready_int = GPIO_MSENSOR_INT,
};
static struct i2c_board_info i2c_devs12[] __initdata = {
	{
		I2C_BOARD_INFO("ak8975", 0x0C),
		.platform_data = &akm8975_pdata,
	},
};
#else
static struct i2c_board_info i2c_devs12[] __initdata = {
	{
		I2C_BOARD_INFO("yas529", 0x2e),
	},
};
#endif

#if defined (CONFIG_VIDEO_MHL_V1)
static struct i2c_board_info i2c_devs18[] __initdata = {
        {
		I2C_BOARD_INFO("SII9234", 0x72>>1),
	},
	{
		I2C_BOARD_INFO("SII9234A", 0x7A>>1),
	},
	{
		I2C_BOARD_INFO("SII9234B", 0x92>>1),
	},
	{
		I2C_BOARD_INFO("SII9234C", 0xC8>>1),
	},
};
#endif
//hdlnc_ldj_20100823 
static struct i2c_board_info i2c_devs13[] __initdata = {
	{
		I2C_BOARD_INFO("A1026_driver", (0x3E)),
	},
};
//hdlnc_ldj_20100823 

static struct resource ram_console_resource[] = {
	{
		.flags = IORESOURCE_MEM,
	}
};

static struct platform_device ram_console_device = {
	.name = "ram_console",
	.id = -1,
	.num_resources = ARRAY_SIZE(ram_console_resource),
	.resource = ram_console_resource,
};

#ifdef CONFIG_ANDROID_PMEM
static struct android_pmem_platform_data pmem_pdata = {
	.name = "pmem",
	.no_allocator = 1,
	.cached = 1,
	.start = 0,
	.size = 0,
};

static struct android_pmem_platform_data pmem_gpu1_pdata = {
	.name = "pmem_gpu1",
	.no_allocator = 1,
	.cached = 1,
	.buffered = 1,
	.start = 0,
	.size = 0,
};

static struct android_pmem_platform_data pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.no_allocator = 1,
	.cached = 1,
	.buffered = 1,
	.start = 0,
	.size = 0,
};

static struct platform_device pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = { .platform_data = &pmem_pdata },
};

static struct platform_device pmem_gpu1_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &pmem_gpu1_pdata },
};

static struct platform_device pmem_adsp_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &pmem_adsp_pdata },
};

static void __init android_pmem_set_platdata(void)
{
	pmem_pdata.start = (u32)s5p_get_media_memory_bank(S5P_MDEV_PMEM, 0);
	pmem_pdata.size = (u32)s5p_get_media_memsize_bank(S5P_MDEV_PMEM, 0);

	pmem_gpu1_pdata.start =
		(u32)s5p_get_media_memory_bank(S5P_MDEV_PMEM_GPU1, 0);
	pmem_gpu1_pdata.size =
		(u32)s5p_get_media_memsize_bank(S5P_MDEV_PMEM_GPU1, 0);

	pmem_adsp_pdata.start =
		(u32)s5p_get_media_memory_bank(S5P_MDEV_PMEM_ADSP, 0);
	pmem_adsp_pdata.size =
		(u32)s5p_get_media_memsize_bank(S5P_MDEV_PMEM_ADSP, 0);
}
#endif

struct platform_device sec_device_battery = {
	.name	= "sec-battery",
	.id	= -1,
};

static int sec_switch_get_cable_status(void)
{
	return mtp_off_status ? CABLE_TYPE_NONE : set_cable_status;
}

static int sec_switch_get_phy_init_status(void)
{
	return fsa9480_init_flag;
}

static void sec_switch_set_vbus_status(u8 mode)
{
	if (mode == USB_VBUS_ALL_OFF)
		mtp_off_status = true;

	if (charger_callbacks && charger_callbacks->set_esafe)
		charger_callbacks->set_esafe(charger_callbacks, mode);
}

static void sec_switch_set_usb_gadget_vbus(bool en)
{
	struct usb_gadget *gadget = platform_get_drvdata(&s3c_device_usbgadget);

	if (gadget) {
		if (en)
			usb_gadget_vbus_connect(gadget);
		else
			usb_gadget_vbus_disconnect(gadget);
	}
}

static struct sec_switch_platform_data sec_switch_pdata = {
	.set_vbus_status = sec_switch_set_vbus_status,
	.set_usb_gadget_vbus = sec_switch_set_usb_gadget_vbus,
	.get_cable_status = sec_switch_get_cable_status,
	.get_phy_init_status = sec_switch_get_phy_init_status,
};

struct platform_device sec_device_switch = {
	.name	= "sec_switch",
	.id	= 1,
	.dev	= {
		.platform_data	= &sec_switch_pdata,
	}
};

static struct platform_device sec_device_rfkill = {
	.name	= "bt_rfkill",
	.id	= -1,
};

static struct platform_device sec_device_btsleep = {
	.name	= "bt_sleep",
	.id	= -1,
};

#if !defined(CONFIG_ARIES_NTT)
static struct sec_jack_zone sec_jack_zones[] = {
	{
		/* adc == 0, unstable zone, default to 3pole if it stays
		 * in this range for 300ms (15ms delays, 20 samples)
		 */
		.adc_high = 0,
		.delay_ms = 15,
		.check_count = 20,
		.jack_type = SEC_HEADSET_3POLE,
	},
	{
		/* 0 < adc <= 900, unstable zone, default to 3pole if it stays
		 * in this range for 800ms (10ms delays, 80 samples)
		 */
#if defined(CONFIG_S5PC110_KEPLER_BOARD) || defined (CONFIG_S5PC110_DEMPSEY_BOARD)  
		.adc_high = 600,
#elif defined(CONFIG_S5PC110_HAWK_BOARD)
		.adc_high = 300,
#else
		.adc_high = 900,
#endif
		.delay_ms = 10,
		.check_count = 80,
		.jack_type = SEC_HEADSET_3POLE,
	},
	{
		/* 900 < adc <= 2000, unstable zone, default to 4pole if it
		 * stays in this range for 800ms (10ms delays, 80 samples)
		 */
#if defined(CONFIG_S5PC110_KEPLER_BOARD) || defined (CONFIG_S5PC110_DEMPSEY_BOARD)  // Ansari
		.adc_high = 700,
#elif defined(CONFIG_S5PC110_HAWK_BOARD)
		.adc_high = 350,
#else
		.adc_high = 2000,
#endif
		.delay_ms = 10,
		.check_count = 80,
		.jack_type = SEC_HEADSET_3POLE,
	},
	{
		/* 2000 < adc <= 3400, 4 pole zone, default to 4pole if it
		 * stays in this range for 100ms (10ms delays, 10 samples)
		 */
#if defined(CONFIG_S5PC110_KEPLER_BOARD) || defined (CONFIG_S5PC110_DEMPSEY_BOARD)  
		.adc_high = 3000,
#elif defined(CONFIG_S5PC110_HAWK_BOARD)
		.adc_high = 3300,
#else
		.adc_high = 3400,
#endif
		.delay_ms = 10,
		.check_count = 10,
		.jack_type = SEC_HEADSET_4POLE,
	},
	{
		/* adc > 3400, unstable zone, default to 3pole if it stays
		 * in this range for two seconds (10ms delays, 200 samples)
		 */
		.adc_high = 0x7fffffff,
		.delay_ms = 10,
		.check_count = 200,
		.jack_type = SEC_HEADSET_3POLE,
	},
};
#else
static struct sec_jack_zone sec_jack_zones[] = {
	{
		/* adc == 0, unstable zone, default to 3pole if it stays
		 * in this range for 300ms (15ms delays, 20 samples)
		 */
		.adc_high = 0,
		.delay_ms = 15,
		.check_count = 20,
		.jack_type = SEC_HEADSET_3POLE,
	},
	{
		/* 0 < adc <= 500, unstable zone, default to 3pole if it stays
		 * in this range for 800ms (10ms delays, 80 samples)
		 */
		.adc_high = 500,
		.delay_ms = 10,
		.check_count = 80,
		.jack_type = SEC_HEADSET_3POLE,
	},
	{
		/* 500 < adc <= 3300, 4 pole zone, default to 4pole if it
		 * stays in this range for 800ms (10ms delays, 80 samples)
		 */
		.adc_high = 3300,
		.delay_ms = 10,
		.check_count = 10,
		.jack_type = SEC_HEADSET_4POLE,
	},
	{
		/* 3300 < adc <= 3400, unstable zone, default to 3pole if it
		 * stays in this range for 800ms (10ms delays, 80 samples)
		 */
		.adc_high = 3400,
		.delay_ms = 10,
		.check_count = 80,
		.jack_type = SEC_HEADSET_3POLE,
	},
	{
		/* 3400 < adc <= 3600, 4 pole zone, default to 4 pole if it
		 * stays in this range for 200ms (10ms delays, 20 samples)
		 */
		.adc_high = 3600,
		.delay_ms = 10,
		.check_count = 20,
		.jack_type = SEC_HEADSET_4POLE,
	},	
	{
		/* adc > 3600, unstable zone, default to 3pole if it stays
		 * in this range for two seconds (10ms delays, 200 samples)
		 */
		.adc_high = 0x7fffffff,
		.delay_ms = 10,
		.check_count = 200,
		.jack_type = SEC_HEADSET_3POLE,
	},
};
#endif
/* Only support one button of earjack in S1_EUR HW.
 * If your HW supports 3-buttons earjack made by Samsung and HTC,
 * add some zones here.
 */
static struct sec_jack_buttons_zone sec_jack_buttons_zones[] = {
	{
		/* 0 <= adc <=1000, stable zone */
		.code		= KEY_MEDIA,
		.adc_low	= 0,
#if defined(CONFIG_S5PC110_KEPLER_BOARD)|| defined (CONFIG_S5PC110_DEMPSEY_BOARD)		
		.adc_high	= 4000,
#else	
		.adc_high	= 1000,
#endif			
	},
};

static int sec_jack_get_adc_value(void)
{
	return s3c_adc_get_adc_data(3);
}

struct sec_jack_platform_data sec_jack_pdata = {
	.set_micbias_state = sec_jack_set_micbias_state,
	.get_adc_value = sec_jack_get_adc_value,
	.zones = sec_jack_zones,
	.num_zones = ARRAY_SIZE(sec_jack_zones),
	.buttons_zones = sec_jack_buttons_zones,
	.num_buttons_zones = ARRAY_SIZE(sec_jack_buttons_zones),
	.det_gpio = GPIO_DET_35,
	.send_end_gpio = GPIO_EAR_SEND_END,
	.send_end_gpio_35 = GPIO_EAR_SEND_END35,

#if defined CONFIG_S5PC110_DEMPSEY_BOARD	
	.det_active_high = 0,
#else
	.det_active_high = 1,
#endif
};

static struct platform_device sec_device_jack = {
	.name			= "sec_jack",
	.id			= 1, /* will be used also for gpio_event id */
	.dev.platform_data	= &sec_jack_pdata,
};

#define S5PV210_PS_HOLD_CONTROL_REG (S3C_VA_SYS+0xE81C)
#if defined (CONFIG_CP_CHIPSET_STE)
extern int  EN32KhzCP_CTRL(int on);
#endif

#if defined(CONFIG_S5PC110_HAWK_BOARD)
extern void touch_led_on(bool bOn);
#endif

#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)
extern int touchkey_ldo_on(bool on);
#endif

#if defined (CONFIG_S5PC110_KEPLER_BOARD)  || defined (CONFIG_S5PC110_DEMPSEY_BOARD)	
extern bool charging_mode_get(void);
#endif

static void aries_power_off(void)
{
	int err;
	int mode = REBOOT_MODE_NONE;
	char reset_mode = 'r';
	int phone_wait_cnt = 0;

	/* Change this API call just before power-off to take the dump. */
	/* kernel_sec_clear_upload_magic_number(); */
#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)
	touchkey_ldo_on(0);
#endif


	printk(KERN_INFO "%s: Start power-off process\n", __func__);

	err = gpio_request(GPIO_PHONE_ACTIVE, "GPIO_PHONE_ACTIVE");
	/* will never be freed */
	WARN(err, "failed to request GPIO_PHONE_ACTIVE");

	gpio_direction_input(GPIO_nPOWER);
	gpio_direction_input(GPIO_PHONE_ACTIVE);

	/* prevent phone reset when AP off */
	gpio_set_value(GPIO_PHONE_ON, 0);

	#if defined (CONFIG_S5PC110_KEPLER_BOARD) || defined (CONFIG_S5PC110_DEMPSEY_BOARD) 	
	if(charging_mode_get())
	{
		phone_wait_cnt = 11;
	}
	#endif

	/* confirm phone off */
	#if defined (CONFIG_CP_CHIPSET_STE)
	#if 1
	if ((gpio_get_value(GPIO_CP_PWR_RST) == 1) && (gpio_get_value(GPIO_INT_RESOUT) == 1)) // GPIO_CP_PWR_RST & GPIO_INT_RESOUT is HIGH
	{
		printk(KERN_EMERG "%s: Try to Turn Phone Off\n", __func__);

		gpio_set_value(GPIO_CP_RST, 1);
		while(1)
		{
			printk(KERN_EMERG "%s: Try to Turn Phone Off by CP_RST\n", __func__);

			if(!gpio_get_value(GPIO_CP_PWR_RST))
			{
				printk(KERN_EMERG "%s: PHONE OFF Success\n", __func__);
				break;
			}

			if(phone_wait_cnt > 20)
			{
				printk(KERN_EMERG "%s: PHONE OFF Failed\n", __func__);
				break;
			}

			phone_wait_cnt++;
			msleep(1000); /*wait modem stable */
		}
		gpio_set_value(GPIO_CP_RST, 0);
	}
	else // GPIO_CP_PWR_RST is LOW
	{
		printk(KERN_EMERG "%s: PHONE OFF Success\n", __func__);
	}
	#else
	while(1) 
	{
		if (gpio_get_value(GPIO_CP_PWR_RST)) // GPIO_CP_PWR_RST is HIGH
		{
			printk(KERN_EMERG "%s: Try to Turn Phone Off by CP_PWR_RST\n", __func__);

			if (phone_wait_cnt > 10) 
			{
				printk(KERN_EMERG "%s: PHONE OFF Failed\n", __func__);

				gpio_set_value(GPIO_CP_RST, 1);
				while(1)
				{
					printk(KERN_EMERG "%s: Retry to Turn Phone Off by CP_RST\n", __func__);

					if(!gpio_get_value(GPIO_INT_RESOUT))
					{
						printk(KERN_EMERG "%s: PHONE OFF Success\n", __func__);
						break;
					}

					if(phone_wait_cnt > 20)
					{
						printk(KERN_EMERG "%s: PHONE OFF Failed\n", __func__);
						break;
					}

					phone_wait_cnt++;
					msleep(1000); /*wait modem stable */
				}
				gpio_set_value(GPIO_CP_RST, 0);

				break;
			}
		}
		else // GPIO_CP_PWR_RST is LOW
		{
			printk(KERN_EMERG "%s: PHONE OFF Success\n", __func__);
			break;
		}

		phone_wait_cnt++;
		msleep(1000);/*wait modem stable */
	}
	#endif
	#else
	while (1) {
		if (gpio_get_value(GPIO_PHONE_ACTIVE)) {
			if (phone_wait_cnt > 10) {
				printk(KERN_EMERG
				       "%s: Try to Turn Phone Off by CP_RST\n",
				       __func__);
				gpio_set_value(GPIO_CP_RST, 0);
			}
			if (phone_wait_cnt > 12) {
				printk(KERN_EMERG "%s: PHONE OFF Failed\n",
				       __func__);
				break;
			}
			phone_wait_cnt++;
			msleep(1000);
		} else {
			printk(KERN_EMERG "%s: PHONE OFF Success\n", __func__);
			break;
		}
	}
	#endif

	while (1) {
		/* Check reboot charging */
		if (charger_callbacks &&
		    charger_callbacks->get_vdcin &&
		    charger_callbacks->get_vdcin(charger_callbacks)) {
			int reboot_mode = REBOOT_MODE_NONE;

			/* watchdog reset */
			pr_info("%s: charger connected, rebooting\n", __func__);
			if (sec_get_param_value)
				sec_get_param_value(__REBOOT_MODE, &reboot_mode);

			if (reboot_mode == REBOOT_MODE_ARM11_FOTA)
				mode = REBOOT_MODE_ARM11_FOTA;
			else
			mode = REBOOT_MODE_CHARGING;

			if (sec_set_param_value)
				sec_set_param_value(__REBOOT_MODE, &mode);
			kernel_sec_clear_upload_magic_number();
			kernel_sec_hw_reset(1);
			arch_reset('r', NULL);
			pr_crit("%s: waiting for reset!\n", __func__);
			while (1);
		}

		kernel_sec_clear_upload_magic_number();

#if defined(CONFIG_S5PC110_HAWK_BOARD)
		touch_led_on(false); // Turn off KEY LED.
#endif
		/* wait for power button release */
		if (gpio_get_value(GPIO_nPOWER)) {
			pr_info("%s: set PS_HOLD low\n", __func__);

			/* PS_HOLD high  PS_HOLD_CONTROL, R/W, 0xE010_E81C */
			writel(readl(S5PV210_PS_HOLD_CONTROL_REG) & 0xFFFFFEFF,
			       S5PV210_PS_HOLD_CONTROL_REG);

			pr_crit("%s: should not reach here!\n", __func__);
		}

		/* if power button is not released, wait and check TA again */
		pr_info("%s: PowerButton is not released.\n", __func__);
		mdelay(1000);
	}
}

static void config_gpio_table(int array_size, unsigned int (*gpio_table)[4])
{
	u32 i, gpio;

	for (i = 0; i < array_size; i++) {
		gpio = gpio_table[i][0];
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(gpio_table[i][1]));
		if (gpio_table[i][2] != S3C_GPIO_SETPIN_NONE)
			gpio_set_value(gpio, gpio_table[i][2]);
		s3c_gpio_setpull(gpio, gpio_table[i][3]);
	}
}

static void config_sleep_gpio_table(int array_size, unsigned int (*gpio_table)[3])
{
	u32 i, gpio;

	for (i = 0; i < array_size; i++) {
		gpio = gpio_table[i][0];
		s3c_gpio_slp_cfgpin(gpio, gpio_table[i][1]);
		s3c_gpio_slp_setpull_updown(gpio, gpio_table[i][2]);
	}
}

static void config_init_gpio(void)
{
	config_gpio_table(ARRAY_SIZE(initial_gpio_table), initial_gpio_table);
}

void config_sleep_gpio(void)
{
	config_gpio_table(ARRAY_SIZE(sleep_alive_gpio_table), sleep_alive_gpio_table);
	config_sleep_gpio_table(ARRAY_SIZE(sleep_gpio_table), sleep_gpio_table);
#if !defined(CONFIG_S5PC110_DEMPSEY_BOARD)
	if (gpio_get_value(GPIO_PS_ON)) {
		s3c_gpio_slp_setpull_updown(GPIO_ALS_SDA_28V, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_setpull_updown(GPIO_ALS_SCL_28V, S3C_GPIO_PULL_NONE);
	} else {
		s3c_gpio_setpull(GPIO_PS_VOUT, S3C_GPIO_PULL_DOWN);
	}
#endif
	printk(KERN_DEBUG "SLPGPIO : BT(%d) WLAN(%d) BT+WIFI(%d)\n",
		gpio_get_value(GPIO_BT_nRST), gpio_get_value(GPIO_WLAN_nRST), gpio_get_value(GPIO_WLAN_BT_EN));
#if !defined(CONFIG_ARIES_NTT)
	printk(KERN_DEBUG "SLPGPIO : CODEC_LDO_EN(%d) MICBIAS_EN(%d) EARPATH_SEL(%d)\n",
		gpio_get_value(GPIO_CODEC_LDO_EN), gpio_get_value(GPIO_MICBIAS_EN), gpio_get_value(GPIO_EARPATH_SEL));
#if !defined(CONFIG_S5PC110_DEMPSEY_BOARD)
	printk(KERN_DEBUG "SLPGPIO : PS_ON(%d) FM_RST(%d) UART_SEL(%d)\n",
		gpio_get_value(GPIO_PS_ON), gpio_get_value(GPIO_FM_RST), gpio_get_value(GPIO_UART_SEL));
#else
	printk(KERN_DEBUG "SLPGPIO : UART_SEL(%d)\n", gpio_get_value(GPIO_UART_SEL));
#endif
#else
	printk(KERN_DEBUG "SLPGPIO : CODEC_LDO_EN(%d) MICBIAS_EN(%d) SUB_MICBIAS_EN(%d) EARPATH_SEL(%d)\n",
	gpio_get_value(GPIO_CODEC_LDO_EN), gpio_get_value(GPIO_MICBIAS_EN), gpio_get_value(GPIO_SUB_MICBIAS_EN), gpio_get_value(GPIO_EARPATH_SEL));
#endif

#if defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) 
	s3c_gpio_setpull(S5PV210_GPH3(5), S3C_GPIO_PULL_DOWN);
#endif

}
EXPORT_SYMBOL(config_sleep_gpio);

static unsigned int wlan_sdio_on_table[][4] = {
	{GPIO_WLAN_SDIO_CLK, GPIO_WLAN_SDIO_CLK_AF, GPIO_LEVEL_NONE,
		S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_CMD, GPIO_WLAN_SDIO_CMD_AF, GPIO_LEVEL_NONE,
		S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D0, GPIO_WLAN_SDIO_D0_AF, GPIO_LEVEL_NONE,
		S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D1, GPIO_WLAN_SDIO_D1_AF, GPIO_LEVEL_NONE,
		S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D2, GPIO_WLAN_SDIO_D2_AF, GPIO_LEVEL_NONE,
		S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D3, GPIO_WLAN_SDIO_D3_AF, GPIO_LEVEL_NONE,
		S3C_GPIO_PULL_NONE},
};

static unsigned int wlan_sdio_off_table[][4] = {
	{GPIO_WLAN_SDIO_CLK, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_CMD, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D0, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D1, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D2, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D3, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
};

static int wlan_power_en(int onoff)
{
	if (onoff) {
		s3c_gpio_cfgpin(GPIO_WLAN_HOST_WAKE,
				S3C_GPIO_SFN(GPIO_WLAN_HOST_WAKE_AF));
		s3c_gpio_setpull(GPIO_WLAN_HOST_WAKE, S3C_GPIO_PULL_DOWN);

#if !defined(CONFIG_S5PC110_DEMPSEY_BOARD)
		s3c_gpio_cfgpin(GPIO_WLAN_WAKE,
				S3C_GPIO_SFN(GPIO_WLAN_WAKE_AF));
		s3c_gpio_setpull(GPIO_WLAN_WAKE, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_WLAN_WAKE, GPIO_LEVEL_LOW);
#else
		s3c_gpio_cfgpin(GPIO_WLAN_BT_EN,
				S3C_GPIO_SFN(GPIO_WLAN_nRST_AF));
		s3c_gpio_setpull(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_LOW);
#endif

		s3c_gpio_cfgpin(GPIO_WLAN_nRST,
				S3C_GPIO_SFN(GPIO_WLAN_nRST_AF));
		s3c_gpio_setpull(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_HIGH);
		s3c_gpio_slp_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_SLP_OUT1);
		s3c_gpio_slp_setpull_updown(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);

		s3c_gpio_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_HIGH);
		s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT1);
		s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_EN,
					S3C_GPIO_PULL_NONE);

		msleep(80);
	} else {
		gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_LOW);
		s3c_gpio_slp_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_SLP_OUT0);
		s3c_gpio_slp_setpull_updown(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);

		//if (gpio_get_value(GPIO_BT_nRST) == 0) {
			gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_LOW);
			s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT0);
			s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_EN,
						S3C_GPIO_PULL_NONE);
		//}
	}
	return 0;
}

static int wlan_reset_en(int onoff)
{
	gpio_set_value(GPIO_WLAN_nRST,
			onoff ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
	return 0;
}

static int wlan_carddetect_en(int onoff)
{
	u32 i;
	u32 sdio;

	if (onoff) {
		for (i = 0; i < ARRAY_SIZE(wlan_sdio_on_table); i++) {
			sdio = wlan_sdio_on_table[i][0];
			s3c_gpio_cfgpin(sdio,
					S3C_GPIO_SFN(wlan_sdio_on_table[i][1]));
			s3c_gpio_setpull(sdio, wlan_sdio_on_table[i][3]);
			if (wlan_sdio_on_table[i][2] != GPIO_LEVEL_NONE)
				gpio_set_value(sdio, wlan_sdio_on_table[i][2]);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(wlan_sdio_off_table); i++) {
			sdio = wlan_sdio_off_table[i][0];
			s3c_gpio_cfgpin(sdio,
				S3C_GPIO_SFN(wlan_sdio_off_table[i][1]));
			s3c_gpio_setpull(sdio, wlan_sdio_off_table[i][3]);
			if (wlan_sdio_off_table[i][2] != GPIO_LEVEL_NONE)
				gpio_set_value(sdio, wlan_sdio_off_table[i][2]);
		}
	}
	udelay(5);
#if !defined(CONFIG_S5PC110_DEMPSEY_BOARD)
	sdhci_s3c_force_presence_change(&s3c_device_hsmmc1);
#else
	sdhci_s3c_force_presence_change(&s3c_device_hsmmc3);
#endif
	return 0;
}

static struct resource wifi_resources[] = {
	[0] = {
#if !defined(CONFIG_S5PC110_DEMPSEY_BOARD)
		.name	= "bcm4329_wlan_irq",
#else
		.name	= "bcm4330_wlan_irq",
#endif
		.start	= IRQ_EINT(20),
		.end	= IRQ_EINT(20),
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	},
};

static struct wifi_mem_prealloc wifi_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)}
};

static void *aries_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;

	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wifi_mem_array[section].size < size)
		return NULL;

	return wifi_mem_array[section].mem_ptr;
}

#define DHD_SKB_HDRSIZE 		336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)
int __init aries_init_wifi_mem(void)
{
	int i;
	int j;

	for (i = 0; i < 8; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}
	
	for (; i < 16; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}
	
	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;

	for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
		wifi_mem_array[i].mem_ptr =
				kmalloc(wifi_mem_array[i].size, GFP_KERNEL);

		if (!wifi_mem_array[i].mem_ptr)
			goto err_mem_alloc;
	}
	printk("%s: WIFI MEM Allocated\n", __FUNCTION__);
	return 0;

 err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		kfree(wifi_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

 err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}
static struct wifi_platform_data wifi_pdata = {
	.set_power		= wlan_power_en,
	.set_reset		= wlan_reset_en,
	.set_carddetect		= wlan_carddetect_en,
	.mem_prealloc		= aries_mem_prealloc,
};

static struct platform_device sec_device_wifi = {
#if !defined(CONFIG_S5PC110_DEMPSEY_BOARD)
	.name			= "bcm4329_wlan",
#else
	.name			= "bcm4330_wlan",
#endif
	.id			= 1,
	.num_resources		= ARRAY_SIZE(wifi_resources),
	.resource		= wifi_resources,
	.dev			= {
		.platform_data = &wifi_pdata,
	},
};

static struct platform_device watchdog_device = {
	.name = "watchdog",
	.id = -1,
};

static struct platform_device *aries_devices[] __initdata = {
	&watchdog_device,
#ifdef CONFIG_FIQ_DEBUGGER
	&s5pv210_device_fiqdbg_uart2,
#endif
	&s5pc110_device_onenand,
#ifdef CONFIG_RTC_DRV_S3C
	&s5p_device_rtc,
#endif
	&aries_input_device,
	&s3c_device_keypad,

#ifdef	CONFIG_S5P_ADC
	&s3c_device_adc,
#endif	

	&s5pv210_device_iis0,
#if !defined(CONFIG_S5PC110_DEMPSEY_BOARD)
	&s5pv210_device_pcm1,
#endif
	&s3c_device_wdt,

#ifdef CONFIG_FB_S3C
	&s3c_device_fb,
#endif

#ifdef CONFIG_VIDEO_MFC50
	&s3c_device_mfc,
#endif
#ifdef CONFIG_VIDEO_FIMC
	&s3c_device_fimc0,
	&s3c_device_fimc1,
	&s3c_device_fimc2,
	&s3c_device_csis,
#endif

#ifdef CONFIG_VIDEO_JPEG_V2
	&s3c_device_jpeg,
#endif

	&s3c_device_g3d,
	&s3c_device_lcd,

#if defined CONFIG_FB_S3C_TL2796 || defined (CONFIG_FB_S3C_uPD161224)
	&s3c_device_spi_gpio,
#endif

#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)	
#if defined (CONFIG_FB_S3C_LDI)
	&ldi_spi_gpio,
#endif
#endif


	&sec_device_jack,

	&s3c_device_i2c0,
#if defined(CONFIG_S3C_DEV_I2C1)
	&s3c_device_i2c1,
#endif

#if defined(CONFIG_S3C_DEV_I2C2)
	&s3c_device_i2c2,
#endif
	&s3c_device_i2c4,
	&s3c_device_i2c5,  /* accel sensor */
	&s3c_device_i2c6,
	&s3c_device_i2c7,
#if !(defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined (CONFIG_S5PC110_DEMPSEY_BOARD))	
	&s3c_device_i2c8,  /* FM radio */
#else
//&s3c_device_i2c8, 
#endif	
	&s3c_device_i2c9,  /* max1704x:fuel_guage */
	&s3c_device_i2c11, /* optical sensor */
	&s3c_device_i2c12, /* magnetic sensor */
	&s3c_device_i2c13,  // hdlnc_bp_ytkwon : 20100301
#if ! (defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD)|| defined (CONFIG_S5PC110_DEMPSEY_BOARD))	
	&s3c_device_i2c14, /* nfc sensor */
#endif	

#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)	
#ifdef CONFIG_REGULATOR_MAX8893
	&s3c_device_i2c15, /*max8893*/
	&s3c_device_8893consumer,
#endif

#ifdef CONFIG_REGULATOR_BH6173
	&s3c_device_i2c16, /*bh6173*/

#endif

	&s3c_device_i2c17, //NAGSM_Android_HQ_Camera_SoojinKim_20110603
#endif
#if defined (CONFIG_VIDEO_MHL_V1)
	&s3c_device_i2c18,	
#endif
#ifdef CONFIG_USB_GADGET
	&s3c_device_usbgadget,
#endif
#ifdef CONFIG_USB_ANDROID
	&s3c_device_android_usb,
#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
	&s3c_device_usb_mass_storage,
#endif
#ifdef CONFIG_USB_ANDROID_RNDIS
	&s3c_device_rndis,
#endif
#endif

#if !defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD) && !defined (CONFIG_S5PC110_HAWK_BOARD) && !defined (CONFIG_S5PC110_SIDEKICK_BOARD)
#ifdef CONFIG_S3C_DEV_HSMMC
	&s3c_device_hsmmc0,
#endif
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	&s3c_device_hsmmc1,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	&s3c_device_hsmmc2,
#endif
#if !defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD) && !defined (CONFIG_S5PC110_HAWK_BOARD) && !defined (CONFIG_S5PC110_SIDEKICK_BOARD)
#ifdef CONFIG_S3C_DEV_HSMMC3
	&s3c_device_hsmmc3,
#endif
#endif
#ifdef CONFIG_VIDEO_TV20
        &s5p_device_tvout,
#endif
	&sec_device_battery,
	&s3c_device_i2c10,

	&sec_device_switch,  // samsung switch driver

#ifdef CONFIG_S5PV210_POWER_DOMAIN
	&s5pv210_pd_audio,
	&s5pv210_pd_cam,
	&s5pv210_pd_tv,
	&s5pv210_pd_lcd,
	&s5pv210_pd_g3d,
	&s5pv210_pd_mfc,
#endif

#ifdef CONFIG_ANDROID_PMEM
	&pmem_device,
	&pmem_gpu1_device,
	&pmem_adsp_device,
#endif

#ifdef CONFIG_HAVE_PWM
	&s3c_device_timer[0],
	&s3c_device_timer[1],
	&s3c_device_timer[2],
	&s3c_device_timer[3],
#endif
	&sec_device_rfkill,
	&sec_device_btsleep,
	&ram_console_device,

	&s5p_device_ace,
#ifdef CONFIG_SND_S5P_RP
	&s5p_device_rp,
#endif
	&sec_device_wifi,
};




static void init_gpio_point(void)
{
	printk("gpio init done point.. \n");
}

EXPORT_SYMBOL(init_gpio_point);

static void __init aries_map_io(void)
{
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s5pv210_gpiolib_init();
	s3c24xx_init_uarts(aries_uartcfgs, ARRAY_SIZE(aries_uartcfgs));
	s5p_reserve_bootmem(aries_media_devs, ARRAY_SIZE(aries_media_devs));
#ifdef CONFIG_MTD_ONENAND
	s5pc110_device_onenand.name = "s5pc110-onenand";
#endif
}

unsigned int pm_debug_scratchpad;

static unsigned int ram_console_start;
static unsigned int ram_console_size;

static void __init aries_fixup(struct machine_desc *desc,
		struct tag *tags, char **cmdline,
		struct meminfo *mi)
{
	mi->bank[0].start = 0x30000000;
	mi->bank[0].size = 80 * SZ_1M;
	mi->bank[0].node = 0;

	mi->bank[1].start = 0x40000000;
	mi->bank[1].size = 256 * SZ_1M;
	mi->bank[1].node = 1;

#if defined (CONFIG_S5PC110_DEMPSEY_BOARD) 
	mi->nr_banks = 2;
#endif

	mi->bank[2].start = 0x50000000;
	/* 1M for ram_console buffer */
#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)		
	mi->bank[2].size = 255 * SZ_1M;
#else
	mi->bank[2].size = 127 * SZ_1M;
#endif
	mi->bank[2].node = 2;
	mi->nr_banks = 3;

	ram_console_start = mi->bank[2].start + mi->bank[2].size;
	ram_console_size = SZ_1M - SZ_4K;

	pm_debug_scratchpad = ram_console_start + ram_console_size;
}

/* this function are used to detect s5pc110 chip version temporally */
int s5pc110_version ;

void _hw_version_check(void)
{
	void __iomem *phy_address ;
	int temp;

	phy_address = ioremap(0x40, 1);

	temp = __raw_readl(phy_address);

	if (temp == 0xE59F010C)
		s5pc110_version = 0;
	else
		s5pc110_version = 1;

	printk(KERN_INFO "S5PC110 Hardware version : EVT%d\n",
				s5pc110_version);

	iounmap(phy_address);
}

/*
 * Temporally used
 * return value 0 -> EVT 0
 * value 1 -> evt 1
 */

int hw_version_check(void)
{
	return s5pc110_version ;
}
EXPORT_SYMBOL(hw_version_check);

static void __init fsa9480_gpio_init(void)
{
#if !defined (CONFIG_S5PC110_DEMPSEY_BOARD)		
	s3c_gpio_cfgpin(GPIO_USB_SEL, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_USB_SEL, S3C_GPIO_PULL_NONE);
#endif
	s3c_gpio_cfgpin(GPIO_UART_SEL, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_UART_SEL, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_JACK_nINT, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_JACK_nINT, S3C_GPIO_PULL_NONE);
}

#if defined (CONFIG_S5PC110_KEPLER_BOARD) || defined (CONFIG_S5PC110_HAWK_BOARD) || defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined (CONFIG_S5PC110_DEMPSEY_BOARD) // mr work
static void __init fuelgauge_gpio_init(void)
{
//       s3c_gpio_cfgpin(GPIO_KBR3, S3C_GPIO_SFN(GPIO_KBR3_WAKE_AF));
//	s3c_gpio_setpull(GPIO_KBR3, S3C_GPIO_PULL_NONE);

	 s3c_gpio_cfgpin(GPIO_KBR3, S5PV210_GPH3_3_EXT_INT33_3);
	 s3c_gpio_setpull(GPIO_KBR3, S3C_GPIO_PULL_NONE);	
}
#endif
static void __init setup_ram_console_mem(void)
{
	ram_console_resource[0].start = ram_console_start;
	ram_console_resource[0].end = ram_console_start + ram_console_size - 1;
}

static void __init sound_init(void)
{
	u32 reg;

	reg = __raw_readl(S5P_OTHERS);
	reg &= ~(0x3 << 8);
	reg |= 3 << 8;
	__raw_writel(reg, S5P_OTHERS);

	reg = __raw_readl(S5P_CLK_OUT);
	reg &= ~(0x1f << 12);
	reg |= 19 << 12;
	__raw_writel(reg, S5P_CLK_OUT);

	reg = __raw_readl(S5P_CLK_OUT);
	reg &= ~0x1;
	reg |= 0x1;
	__raw_writel(reg, S5P_CLK_OUT);

#if defined(CONFIG_S5PC110_KEPLER_BOARD) || defined (CONFIG_S5PC110_DEMPSEY_BOARD) 	
	gpio_request(GPIO_MICBIAS_EN, "micbias_enable");
	gpio_request(GPIO_EARMICBIAS_EN, "sub_micbias_enable");	
#elif defined(CONFIG_S5PC110_HAWK_BOARD)
  	gpio_request(GPIO_MICBIAS_EN,  "micbias_enable");		 // GPJ4(2)
	gpio_request(GPIO_MICBIAS_EN2, "sub_micbias_enable"); // GPJ2(5)
#elif defined(CONFIG_ARIES_NTT)
	gpio_request(GPIO_MICBIAS_EN, "micbias_enable");
	gpio_request(GPIO_SUB_MICBIAS_EN, "sub_micbias_enable");
#else
	gpio_request(GPIO_MICBIAS_EN, "micbias_enable");
#endif
}

static void __init onenand_init()
{
	struct clk *clk = clk_get(NULL, "onenand");
	BUG_ON(!clk);
	clk_enable(clk);
}

static void __init aries_machine_init(void)
{
	setup_ram_console_mem();
	s3c_usb_set_serial();
	platform_add_devices(aries_devices, ARRAY_SIZE(aries_devices));

	/* Find out S5PC110 chip version */
	_hw_version_check();

	pm_power_off = aries_power_off ;

	s3c_gpio_cfgpin(GPIO_HWREV_MODE0, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_HWREV_MODE0, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_HWREV_MODE1, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_HWREV_MODE1, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_HWREV_MODE2, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_HWREV_MODE2, S3C_GPIO_PULL_NONE);
	HWREV = gpio_get_value(GPIO_HWREV_MODE0);
	HWREV = HWREV | (gpio_get_value(GPIO_HWREV_MODE1) << 1);
	HWREV = HWREV | (gpio_get_value(GPIO_HWREV_MODE2) << 2);
	s3c_gpio_cfgpin(GPIO_HWREV_MODE3, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_HWREV_MODE3, S3C_GPIO_PULL_NONE);
#if !defined(CONFIG_ARIES_NTT)
	HWREV = HWREV | (gpio_get_value(GPIO_HWREV_MODE3) << 3);
	printk(KERN_INFO "HWREV is 0x%x\n", HWREV);
#else
	HWREV = 0x0E;
	printk("HWREV is 0x%x\n", HWREV);
#endif
#if defined(CONFIG_S5PC110_HAWK_BOARD)
	HWREV_HAWK = HWREV;	
	HWREV = 0xa; // The last version of T959 and  : HWREV = 0xa;	
	printk("HWREV_HAWK is 0x%x\n", HWREV_HAWK);
#endif

#if defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
	s3c_gpio_cfgpin(S5PV210_GPH3(5), S3C_GPIO_INPUT);
	s3c_gpio_setpull( S5PV210_GPH3(5), S3C_GPIO_PULL_NONE); 
	VPLUSVER = gpio_get_value(S5PV210_GPH3(5));
	printk("VPLUSVER is 0x%x\n", VPLUSVER);
#endif
	/*initialise the gpio's*/
	config_init_gpio();

	init_gpio_point();

#ifdef CONFIG_ANDROID_PMEM
	android_pmem_set_platdata();
#endif

	
#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)	
	s3c_gpio_cfgpin(GPIO_MASSMEMORY_EN2, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_MASSMEMORY_EN2, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_MASSMEMORY_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_MASSMEMORY_EN, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_MASSMEMORY_EN2, GPIO_LEVEL_HIGH);
	gpio_set_value(GPIO_MASSMEMORY_EN, GPIO_LEVEL_HIGH);
#endif	


	/* i2c */
	s3c_i2c0_set_platdata(NULL);
#ifdef CONFIG_S3C_DEV_I2C1
	s3c_i2c1_set_platdata(NULL);
#endif

#ifdef CONFIG_S3C_DEV_I2C2
	s3c_i2c2_set_platdata(NULL);
#endif
	/* H/W I2C lines */
	if (system_rev >= 0x05) {
		/* gyro sensor */
		i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));
		/* magnetic and accel sensor */
		i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));
	}
	i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));

	/* wm8994 codec */
	sound_init();
	i2c_register_board_info(4, i2c_devs4, ARRAY_SIZE(i2c_devs4));
	/* accel sensor */
	i2c_register_board_info(5, i2c_devs5, ARRAY_SIZE(i2c_devs5));
	i2c_register_board_info(6, i2c_devs6, ARRAY_SIZE(i2c_devs6));

	/* Touch Key */
#if !defined (CONFIG_S5PC110_DEMPSEY_BOARD)		
	touch_keypad_gpio_init();
#endif
	i2c_register_board_info(10, i2c_devs10, ARRAY_SIZE(i2c_devs10));
	/* FSA9480 */
	fsa9480_gpio_init();
	i2c_register_board_info(7, i2c_devs7, ARRAY_SIZE(i2c_devs7));

	/* FM Radio */
#if !defined (CONFIG_S5PC110_DEMPSEY_BOARD)		
	i2c_register_board_info(8, i2c_devs8, ARRAY_SIZE(i2c_devs8));
#endif

#if defined (CONFIG_S5PC110_KEPLER_BOARD) || defined (CONFIG_S5PC110_HAWK_BOARD) || defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined (CONFIG_S5PC110_DEMPSEY_BOARD) // mr work
	fuelgauge_gpio_init();
#endif
	i2c_register_board_info(9, i2c_devs9, ARRAY_SIZE(i2c_devs9));
	/* optical sensor */
#if defined (CONFIG_OPTICAL_TAOS_TRITON)	
	taos_gpio_init();
#elif defined (CONFIG_OPTICAL_GP2A)
	gp2a_gpio_init();
#endif
	i2c_register_board_info(11, i2c_devs11, ARRAY_SIZE(i2c_devs11));
	/* magnetic sensor */
	i2c_register_board_info(12, i2c_devs12, ARRAY_SIZE(i2c_devs12));

//hdlnc_ldj_20100823 	
	i2c_register_board_info(13, i2c_devs13, ARRAY_SIZE(i2c_devs13)); /* audience A1026 */
//hdlnc_ldj_20100823 	

#if !(defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined (CONFIG_S5PC110_DEMPSEY_BOARD))	
	/* nfc sensor */
	i2c_register_board_info(14, i2c_devs14, ARRAY_SIZE(i2c_devs14));
#endif


#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)	
#ifdef CONFIG_REGULATOR_MAX8893
	i2c_register_board_info(15, i2c_devs15, ARRAY_SIZE(i2c_devs15));
#endif
#ifdef CONFIG_REGULATOR_BH6173
	i2c_register_board_info(16, i2c_devs16, ARRAY_SIZE(i2c_devs16));
#endif
#endif
#if defined (CONFIG_VIDEO_MHL_V1)
	i2c_register_board_info(18, i2c_devs18, ARRAY_SIZE(i2c_devs18)); 
#endif


#if defined (CONFIG_FB_S3C_TL2796)|| defined (CONFIG_FB_S3C_uPD161224) 
	spi_register_board_info(spi_board_info, ARRAY_SIZE(spi_board_info));
	s3cfb_set_platdata(&tl2796_data);
#endif

#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)	
#if defined (CONFIG_FB_S3C_LDI)
	ldi_fb_init();
#endif
#endif
#if defined(CONFIG_S5P_ADC)
	s3c_adc_set_platdata(&s3c_adc_platform);
#endif

#if defined(CONFIG_PM)
	s3c_pm_init();
#endif

#ifdef CONFIG_VIDEO_FIMC
	/* fimc */
	s3c_fimc0_set_platdata(&fimc_plat_lsi);
	s3c_fimc1_set_platdata(&fimc_plat_lsi);
	s3c_fimc2_set_platdata(&fimc_plat_lsi);
	s3c_csis_set_platdata(NULL);
#endif

#ifdef CONFIG_VIDEO_JPEG_V2
	s3c_jpeg_set_platdata(&jpeg_plat);
#endif

#ifdef CONFIG_VIDEO_MFC50
	/* mfc */
	s3c_mfc_set_platdata(NULL);
#endif
#if !defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD) && !defined (CONFIG_S5PC110_HAWK_BOARD) && !defined (CONFIG_S5PC110_SIDEKICK_BOARD)
#ifdef CONFIG_S3C_DEV_HSMMC
	s5pv210_default_sdhci0();
#endif
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	s5pv210_default_sdhci1();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	s5pv210_default_sdhci2();
#endif
#if !defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD) && !defined (CONFIG_S5PC110_HAWK_BOARD) && !defined (CONFIG_S5PC110_SIDEKICK_BOARD)
#ifdef CONFIG_S3C_DEV_HSMMC3
	s5pv210_default_sdhci3();
#endif
#endif
#ifdef CONFIG_S5PV210_SETUP_SDHCI
	s3c_sdhci_set_platdata();
#endif

	regulator_has_full_constraints();

	register_reboot_notifier(&aries_reboot_notifier);

	aries_switch_init();
	
#if ! defined (CONFIG_GPS_CHIPSET_STE_CG2900)
#if !defined(CONFIG_ARIES_NTT)
	gps_gpio_init();
#endif
#endif

	aries_init_wifi_mem();

	onenand_init();

#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
	s3c_gpio_cfgpin( GPIO_VT_CAM_SCL_18V, 1 );
	s3c_gpio_setpull( GPIO_VT_CAM_SCL_18V, S3C_GPIO_PULL_UP); 
	s3c_gpio_cfgpin( GPIO_VT_CAM_SDA_18V, 1 );
	s3c_gpio_setpull(GPIO_VT_CAM_SDA_18V, S3C_GPIO_PULL_UP); 	
#endif
#if !defined (CONFIG_S5PC110_DEMPSEY_BOARD)	
	if (gpio_is_valid(GPIO_MSENSE_nRST)) {
		if (gpio_request(GPIO_MSENSE_nRST, "GPB"))
			printk(KERN_ERR "Failed to request GPIO_MSENSE_nRST!\n");
		gpio_direction_output(GPIO_MSENSE_nRST, 1);
	}
	gpio_free(GPIO_MSENSE_nRST);
#endif
}

#ifdef CONFIG_USB_SUPPORT
/* Initializes OTG Phy. */
void otg_phy_init(void)
{
	/* USB PHY0 Enable */
	writel(readl(S5P_USB_PHY_CONTROL) | (0x1<<0),
			S5P_USB_PHY_CONTROL);
	writel((readl(S3C_USBOTG_PHYPWR) & ~(0x3<<3) & ~(0x1<<0)) | (0x1<<5),
			S3C_USBOTG_PHYPWR);
	writel((readl(S3C_USBOTG_PHYCLK) & ~(0x5<<2)) | (0x3<<0),
			S3C_USBOTG_PHYCLK);
	writel((readl(S3C_USBOTG_RSTCON) & ~(0x3<<1)) | (0x1<<0),
			S3C_USBOTG_RSTCON);
	msleep(1);
	writel(readl(S3C_USBOTG_RSTCON) & ~(0x7<<0),
			S3C_USBOTG_RSTCON);
	msleep(1);

	/* rising/falling time */
	writel(readl(S3C_USBOTG_PHYTUNE) | (0x1<<20),
			S3C_USBOTG_PHYTUNE);

	/* set DC level as 6 (6%) */
/*	writel((readl(S3C_USBOTG_PHYTUNE) & ~(0xf)) | (0x1<<2) | (0x1<<1),
			S3C_USBOTG_PHYTUNE);         eye-diagram tuning      */
	
	writel((readl(S3C_USBOTG_PHYTUNE) & ~(0xf)) | (0xf),
			S3C_USBOTG_PHYTUNE);
	
}
EXPORT_SYMBOL(otg_phy_init);

/* USB Control request data struct must be located here for DMA transfer */
struct usb_ctrlrequest usb_ctrl __attribute__((aligned(64)));

/* OTG PHY Power Off */
void otg_phy_off(void)
{
	writel(readl(S3C_USBOTG_PHYPWR) | (0x3<<3),
			S3C_USBOTG_PHYPWR);
	writel(readl(S5P_USB_PHY_CONTROL) & ~(1<<0),
			S5P_USB_PHY_CONTROL);
}
EXPORT_SYMBOL(otg_phy_off);

void usb_host_phy_init(void)
{
	struct clk *otg_clk;

	otg_clk = clk_get(NULL, "otg");
	clk_enable(otg_clk);

	if (readl(S5P_USB_PHY_CONTROL) & (0x1<<1))
		return;

	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL) | (0x1<<1),
			S5P_USB_PHY_CONTROL);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYPWR)
			& ~(0x1<<7) & ~(0x1<<6)) | (0x1<<8) | (0x1<<5),
			S3C_USBOTG_PHYPWR);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYCLK) & ~(0x1<<7)) | (0x3<<0),
			S3C_USBOTG_PHYCLK);
	__raw_writel((__raw_readl(S3C_USBOTG_RSTCON)) | (0x1<<4) | (0x1<<3),
			S3C_USBOTG_RSTCON);
	__raw_writel(__raw_readl(S3C_USBOTG_RSTCON) & ~(0x1<<4) & ~(0x1<<3),
			S3C_USBOTG_RSTCON);
}
EXPORT_SYMBOL(usb_host_phy_init);

void usb_host_phy_off(void)
{
	__raw_writel(__raw_readl(S3C_USBOTG_PHYPWR) | (0x1<<7)|(0x1<<6),
			S3C_USBOTG_PHYPWR);
	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL) & ~(1<<1),
			S5P_USB_PHY_CONTROL);
}
EXPORT_SYMBOL(usb_host_phy_off);
#endif

MACHINE_START(SMDKC110, "SMDKC110")
	/* Maintainer: Kukjin Kim <kgene.kim@samsung.com> */
	.phys_io	= S3C_PA_UART & 0xfff00000,
	.io_pg_offst	= (((u32)S3C_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.fixup		= aries_fixup,
	.init_irq	= s5pv210_init_irq,
	.map_io		= aries_map_io,
	.init_machine	= aries_machine_init,
#if	defined(CONFIG_S5P_HIGH_RES_TIMERS)
	.timer		= &s5p_systimer,
#else
	.timer		= &s3c24xx_timer,
#endif
MACHINE_END

MACHINE_START(ARIES, "aries")
	.phys_io	= S3C_PA_UART & 0xfff00000,
	.io_pg_offst	= (((u32)S3C_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.fixup		= aries_fixup,
	.init_irq	= s5pv210_init_irq,
	.map_io		= aries_map_io,
	.init_machine	= aries_machine_init,
	.timer		= &s5p_systimer,
MACHINE_END

void s3c_setup_uart_cfg_gpio(unsigned char port)
{
	switch (port) {
	case 0:
		s3c_gpio_cfgpin(GPIO_BT_RXD, S3C_GPIO_SFN(GPIO_BT_RXD_AF));
		s3c_gpio_setpull(GPIO_BT_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_TXD, S3C_GPIO_SFN(GPIO_BT_TXD_AF));
		s3c_gpio_setpull(GPIO_BT_TXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_CTS, S3C_GPIO_SFN(GPIO_BT_CTS_AF));
		s3c_gpio_setpull(GPIO_BT_CTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_RTS, S3C_GPIO_SFN(GPIO_BT_RTS_AF));
		s3c_gpio_setpull(GPIO_BT_RTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(GPIO_BT_RXD, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(GPIO_BT_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(GPIO_BT_TXD, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(GPIO_BT_TXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(GPIO_BT_CTS, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(GPIO_BT_CTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(GPIO_BT_RTS, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(GPIO_BT_RTS, S3C_GPIO_PULL_NONE);
		break;
	case 1:
#ifdef CONFIG_GPS_CHIPSET_STE_CG2900 /* STE for CG2900 */
		s3c_gpio_cfgpin(GPIO_GPS_RXD, S3C_GPIO_SFN(GPIO_GPS_RXD_AF));
		s3c_gpio_setpull(GPIO_GPS_RXD, S3C_GPIO_PULL_NONE);// up -> none
		s3c_gpio_cfgpin(GPIO_GPS_TXD, S3C_GPIO_SFN(GPIO_GPS_TXD_AF));
		s3c_gpio_setpull(GPIO_GPS_TXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_GPS_CTS, S3C_GPIO_SFN(GPIO_GPS_CTS_AF));
		s3c_gpio_setpull(GPIO_GPS_CTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_GPS_RTS, S3C_GPIO_SFN(GPIO_GPS_RTS_AF));
		s3c_gpio_setpull(GPIO_GPS_RTS, S3C_GPIO_PULL_NONE);
#else
		s3c_gpio_cfgpin(GPIO_GPS_RXD, S3C_GPIO_SFN(GPIO_GPS_RXD_AF));
		s3c_gpio_setpull(GPIO_GPS_RXD, S3C_GPIO_PULL_UP);
		s3c_gpio_cfgpin(GPIO_GPS_TXD, S3C_GPIO_SFN(GPIO_GPS_TXD_AF));
		s3c_gpio_setpull(GPIO_GPS_TXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_GPS_CTS, S3C_GPIO_SFN(GPIO_GPS_CTS_AF));
		s3c_gpio_setpull(GPIO_GPS_CTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_GPS_RTS, S3C_GPIO_SFN(GPIO_GPS_RTS_AF));
		s3c_gpio_setpull(GPIO_GPS_RTS, S3C_GPIO_PULL_NONE);
#endif
		break;
	case 2:
		s3c_gpio_cfgpin(GPIO_AP_RXD, S3C_GPIO_SFN(GPIO_AP_RXD_AF));
		s3c_gpio_setpull(GPIO_AP_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_AP_TXD, S3C_GPIO_SFN(GPIO_AP_TXD_AF));
		s3c_gpio_setpull(GPIO_AP_TXD, S3C_GPIO_PULL_NONE);
		break;
	case 3:
		s3c_gpio_cfgpin(GPIO_FLM_RXD, S3C_GPIO_SFN(GPIO_FLM_RXD_AF));
		s3c_gpio_setpull(GPIO_FLM_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_FLM_TXD, S3C_GPIO_SFN(GPIO_FLM_TXD_AF));
		s3c_gpio_setpull(GPIO_FLM_TXD, S3C_GPIO_PULL_NONE);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(s3c_setup_uart_cfg_gpio);
#if defined (CONFIG_GPS_CHIPSET_STE_CG2900)  /* STE for CG2900 */
void cg29xx_uart_disable(void)
{
	printk("cg29xx_uart_disable");
	/* Set TXD to LOW to apply the BREAK condition */
	s3c_gpio_cfgpin(GPIO_GPS_TXD, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_GPS_TXD, S3C_GPIO_PULL_DOWN);
	gpio_set_value(GPIO_GPS_TXD, 0);
//	s3c_gpio_setpin(GPIO_GPS_TXD, 0);  Kernel Panic
}
EXPORT_SYMBOL(cg29xx_uart_disable);

void cg29xx_uart_enable(void)
{
	printk("cg29xx_uart_enable");
	s3c_setup_uart_cfg_gpio(1);
}		
EXPORT_SYMBOL(cg29xx_uart_enable);
void cg29xx_rts_gpio_control(int flag)
{
	printk("cg29xx_rts_gpio_control %d\n", flag);
	if(flag)
	{
		/* Enable back the the RTS Flow by making HOST_RTS high */
		s3c_gpio_cfgpin(GPIO_GPS_RTS, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_GPS_RTS, S3C_GPIO_PULL_DOWN);
		gpio_set_value(GPIO_GPS_RTS, 0);
//		s3c_gpio_setpin(GPIO_GPS_RTS, 0); Kernel Panic
	}
	else
	{
		/* Disable the RTS Flow by making HOST_RTS high */
		s3c_gpio_cfgpin(GPIO_GPS_RTS, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_GPS_RTS, S3C_GPIO_PULL_UP);
		gpio_set_value(GPIO_GPS_RTS, 1);
//		s3c_gpio_setpin(GPIO_GPS_RTS, 1); Kernel Panic
	}		
		
}
EXPORT_SYMBOL(cg29xx_rts_gpio_control);
int cg29xx_cts_gpio_level(void)
{
	return gpio_get_value(GPIO_GPS_CTS);
}
EXPORT_SYMBOL(cg29xx_cts_gpio_level);
int cg29xx_cts_gpio_pin_number(void)
{
	return GPIO_GPS_CTS;
}
EXPORT_SYMBOL(cg29xx_cts_gpio_pin_number);
#endif
