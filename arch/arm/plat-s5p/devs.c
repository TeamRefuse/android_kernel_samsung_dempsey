/* linux/arch/arm/plat-s5p/devs.c
 *
 * Copyright (c) 2009 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Base S5P resource and device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/dm9000.h>
#include <linux/platform_device.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <mach/hardware.h>
#include <mach/map.h>
#include <mach/adcts.h>
#include <mach/adc.h>

#include <plat/devs.h>
#include <plat/irqs.h>
#include <plat/fb.h>
#include <plat/fimc.h>
#include <plat/csis.h>

/* Android Gadget */
#include <linux/usb/android_composite.h>
#define S3C_VENDOR_ID		0x18d1
#define S3C_PRODUCT_ID		0x0001
#define S3C_ADB_PRODUCT_ID	0x0005
#define MAX_USB_SERIAL_NUM	17

static char *usb_functions_ums[] = {
	"usb_mass_storage",
};

static char *usb_functions_rndis[] = {
	"rndis",
};

static char *usb_functions_rndis_adb[] = {
	"rndis",
	"adb",
};
static char *usb_functions_ums_adb[] = {
	"usb_mass_storage",
	"adb",
};

static char *usb_functions_all[] = {
#ifdef CONFIG_USB_ANDROID_RNDIS
	"rndis",
#endif
	"usb_mass_storage",
	"adb",
#ifdef CONFIG_USB_ANDROID_ACM
	"acm",
#endif
};
static struct android_usb_product usb_products[] = {
	{
		.product_id	= S3C_PRODUCT_ID,
		.num_functions	= ARRAY_SIZE(usb_functions_ums),
		.functions	= usb_functions_ums,
	},
	{
		.product_id	= S3C_ADB_PRODUCT_ID,
		.num_functions	= ARRAY_SIZE(usb_functions_ums_adb),
		.functions	= usb_functions_ums_adb,
	},
	/*
	{
		.product_id	= S3C_RNDIS_PRODUCT_ID,
		.num_functions	= ARRAY_SIZE(usb_functions_rndis),
		.functions	= usb_functions_rndis,
	},
	{
		.product_id	= S3C_RNDIS_ADB_PRODUCT_ID,
		.num_functions	= ARRAY_SIZE(usb_functions_rndis_adb),
		.functions	= usb_functions_rndis_adb,
	},
	*/
};
// serial number should be changed as real device for commercial release
static char device_serial[MAX_USB_SERIAL_NUM]="0123456789ABCDEF";
/* standard android USB platform data */

// Information should be changed as real product for commercial release
static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id		= S3C_VENDOR_ID,
	.product_id		= S3C_PRODUCT_ID,
	.manufacturer_name	= "Android",//"Samsung",
	.product_name		= "Android",//"Samsung SMDKV210",
	.serial_number		= device_serial,
	.num_products 		= ARRAY_SIZE(usb_products),
	.products 		= usb_products,
	.num_functions 		= ARRAY_SIZE(usb_functions_all),
	.functions 		= usb_functions_all,
};

struct platform_device s3c_device_android_usb = {
	.name	= "android_usb",
	.id	= -1,
	.dev	= {
		.platform_data	= &android_usb_pdata,
	},
};
EXPORT_SYMBOL(s3c_device_android_usb);

static struct usb_mass_storage_platform_data ums_pdata = {
	.vendor			= "Android   ",//"Samsung",
	.product		= "UMS Composite",//"SMDKV210",
	.release		= 1,
};
struct platform_device s3c_device_usb_mass_storage= {
	.name	= "usb_mass_storage",
	.id	= -1,
	.dev	= {
		.platform_data = &ums_pdata,
	},
};
EXPORT_SYMBOL(s3c_device_usb_mass_storage);

/* DM9000 registrations */

static struct resource s5p_dm9000_resources[] = {
	[0] = {
		.start = S5P_PA_DM9000,
		.end   = S5P_PA_DM9000,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
#if defined(CONFIG_DM9000_16BIT)
		.start = S5P_PA_DM9000 + 2,
		.end   = S5P_PA_DM9000 + 2,
		.flags = IORESOURCE_MEM,
#else
		.start = S5P_PA_DM9000 + 1,
		.end   = S5P_PA_DM9000 + 1,
		.flags = IORESOURCE_MEM,
#endif
	},
	[2] = {
#if defined(CONFIG_MACH_VOGUEV210)
#if defined(CONFIG_TYPE_PROTO0)
		.start = IRQ_EINT_GROUP(19, 0),
		.end   = IRQ_EINT_GROUP(19, 0),
#elif defined(CONFIG_TYPE_PROTO2) || defined(CONFIG_TYPE_PROTO3)
		.start = IRQ_EINT_GROUP(18, 0),
		.end   = IRQ_EINT_GROUP(18, 0),
#endif
#else
		.start = IRQ_EINT9,
		.end   = IRQ_EINT9,
#endif
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	}
};

static struct dm9000_plat_data s5p_dm9000_platdata = {
#if defined(CONFIG_DM9000_16BIT)
	.flags = DM9000_PLATF_16BITONLY | DM9000_PLATF_NO_EEPROM,
#else
	.flags = DM9000_PLATF_8BITONLY | DM9000_PLATF_NO_EEPROM,
#endif
};

struct platform_device s5p_device_dm9000 = {
	.name		= "dm9000",
	.id		=  0,
	.num_resources	= ARRAY_SIZE(s5p_dm9000_resources),
	.resource	= s5p_dm9000_resources,
	.dev		= {
		.platform_data = &s5p_dm9000_platdata,
	}
};

/* RTC */

static struct resource s5p_rtc_resource[] = {
        [0] = {
                .start = S5P_PA_RTC,
                .end   = S5P_PA_RTC + 0xff,
                .flags = IORESOURCE_MEM,
        },
        [1] = {
                .start = IRQ_RTC_ALARM,
                .end   = IRQ_RTC_ALARM,
                .flags = IORESOURCE_IRQ,
        },
        [2] = {
                .start = IRQ_RTC_TIC,
                .end   = IRQ_RTC_TIC,
                .flags = IORESOURCE_IRQ
        }
};

struct platform_device s5p_device_rtc = {
        .name             = "s3c2410-rtc",
        .id               = -1,
        .num_resources    = ARRAY_SIZE(s5p_rtc_resource),
        .resource         = s5p_rtc_resource,
};

/* WATCHDOG TIMER*/
static struct resource s3c_wdt_resource[] = {
	[0] = {
		.start = S5PV210_PA_WDT,
		.end = S5PV210_PA_WDT + 0xff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_WDT,
		.end = IRQ_WDT,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_wdt = {
	.name = "s3c2410-wdt",
	.id = -1,
	.num_resources = ARRAY_SIZE(s3c_wdt_resource),
	.resource = s3c_wdt_resource,
};
EXPORT_SYMBOL(s3c_device_wdt);

 /* USB Host Controlle EHCI registrations */

static struct resource s3c_usb__ehci_resource[] = {
	[0] = {
		.start = S5P_PA_USB_EHCI ,
		.end   = S5P_PA_USB_EHCI  + S5P_SZ_USB_EHCI - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_UHOST,
		.end   = IRQ_UHOST,
		.flags = IORESOURCE_IRQ,
	}
};

static u64 s3c_device_usb_ehci_dmamask = 0xffffffffUL;

struct platform_device s3c_device_usb_ehci = {
	.name		= "s5pv210-ehci",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_usb__ehci_resource),
	.resource	= s3c_usb__ehci_resource,
	.dev		= {
		.dma_mask = &s3c_device_usb_ehci_dmamask,
		.coherent_dma_mask = 0xffffffffUL
	}
};
EXPORT_SYMBOL(s3c_device_usb_ehci);

 /* USB Host Controlle OHCI registrations */

 static struct resource s3c_usb__ohci_resource[] = {
	[0] = {
		.start = S5P_PA_USB_OHCI ,
		.end   = S5P_PA_USB_OHCI  + S5P_SZ_USB_OHCI - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_UHOST,
		.end   = IRQ_UHOST,
		.flags = IORESOURCE_IRQ,
	}
};

static u64 s3c_device_usb_ohci_dmamask = 0xffffffffUL;

struct platform_device s3c_device_usb_ohci = {
	.name		= "s5pv210-ohci",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_usb__ohci_resource),
	.resource	= s3c_usb__ohci_resource,
	.dev		= {
		.dma_mask = &s3c_device_usb_ohci_dmamask,
		.coherent_dma_mask = 0xffffffffUL
	}
};
EXPORT_SYMBOL(s3c_device_usb_ohci);

/* USB Device (Gadget)*/

static struct resource s3c_usbgadget_resource[] = {
	[0] = {
		.start = S5P_PA_OTG,
		.end   = S5P_PA_OTG+S5P_SZ_OTG-1,
		.flags = IORESOURCE_MEM,
		},
	[1] = {
		.start = IRQ_OTG,
		.end   = IRQ_OTG,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device s3c_device_usbgadget = {
	.name		= "s3c-usbgadget",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_usbgadget_resource),
	.resource	= s3c_usbgadget_resource,
};
EXPORT_SYMBOL(s3c_device_usbgadget);

/*TSI Interface*/
static u64 tsi_dma_mask = 0xffffffffUL;

static struct resource s3c_tsi_resource[] = {
        [0] = {
                .start = S5P_PA_TSI,
                .end   = S5P_PA_TSI + S5P_SZ_TSI - 1,
                .flags = IORESOURCE_MEM,
        },
        [1] = {
                .start = IRQ_TSI,
                .end   = IRQ_TSI,
                .flags = IORESOURCE_IRQ,
        }
};

struct platform_device s3c_device_tsi = {
        .name             = "s3c-tsi",
        .id               = -1,
        .num_resources    = ARRAY_SIZE(s3c_tsi_resource),
        .resource         = s3c_tsi_resource,
	.dev              = {
		.dma_mask		= &tsi_dma_mask,
		.coherent_dma_mask	= 0xffffffffUL
	}


};
EXPORT_SYMBOL(s3c_device_tsi);
/* JPEG controller  */
static struct resource s3c_jpeg_resource[] = {
        [0] = {
                .start = S5PV210_PA_JPEG,
                .end   = S5PV210_PA_JPEG + S5PV210_SZ_JPEG - 1,
                .flags = IORESOURCE_MEM,
        },
        [1] = {
                .start = IRQ_JPEG,
                .end   = IRQ_JPEG,
                .flags = IORESOURCE_IRQ,
        }

};

struct platform_device s3c_device_jpeg = {
        .name             = "s3c-jpg",
        .id               = -1,
        .num_resources    = ARRAY_SIZE(s3c_jpeg_resource),
        .resource         = s3c_jpeg_resource,
};
EXPORT_SYMBOL(s3c_device_jpeg);

/* rotator interface */
static struct resource s5p_rotator_resource[] = {
        [0] = {
                .start = S5PV210_PA_ROTATOR,
                .end   = S5PV210_PA_ROTATOR + S5PV210_SZ_ROTATOR - 1,
                .flags = IORESOURCE_MEM,
        },
        [1] = {
                .start = IRQ_ROTATOR,
                .end   = IRQ_ROTATOR,
                .flags = IORESOURCE_IRQ,
        }
};

struct platform_device s5p_device_rotator = {
        .name             = "s5p-rotator",
        .id               = -1,
        .num_resources    = ARRAY_SIZE(s5p_rotator_resource),
        .resource         = s5p_rotator_resource
};
EXPORT_SYMBOL(s5p_device_rotator);

/* Keypad interface */
static struct resource s3c_keypad_resource[] = {
        [0] = {
                .start = S3C_PA_KEYPAD,
                .end   = S3C_PA_KEYPAD+ S3C_SZ_KEYPAD - 1,
                .flags = IORESOURCE_MEM,
        },
        [1] = {
                .start = IRQ_KEYPAD,
                .end   = IRQ_KEYPAD,
                .flags = IORESOURCE_IRQ,
        }
};

struct platform_device s3c_device_keypad = {
        .name             = "s3c-keypad",
        .id               = -1,
        .num_resources    = ARRAY_SIZE(s3c_keypad_resource),
        .resource         = s3c_keypad_resource,
};

EXPORT_SYMBOL(s3c_device_keypad);

#ifdef CONFIG_S5PV210_ADCTS
/* ADCTS */
static struct resource s3c_adcts_resource[] = {
        [0] = {
                .start = S3C_PA_ADC,
                .end   = S3C_PA_ADC + SZ_4K - 1,
                .flags = IORESOURCE_MEM,
        },
        [1] = {
                .start = IRQ_PENDN,
                .end   = IRQ_PENDN,
                .flags = IORESOURCE_IRQ,
        },
        [2] = {
                .start = IRQ_ADC,
                .end   = IRQ_ADC,
                .flags = IORESOURCE_IRQ,
        }

};

struct platform_device s3c_device_adcts = {
	.name		  = "s3c-adcts",
        .id               = -1,
	.num_resources	  = ARRAY_SIZE(s3c_adcts_resource),
	.resource	  = s3c_adcts_resource,
};

void __init s3c_adcts_set_platdata(struct s3c_adcts_plat_info *pd)
{
	struct s3c_adcts_plat_info *npd;

        npd = kmalloc(sizeof(*npd), GFP_KERNEL);
        if (npd) {
                memcpy(npd, pd, sizeof(*npd));
		s3c_device_adcts.dev.platform_data = npd;
        } else {
                printk(KERN_ERR "no memory for ADC platform data\n");
        }
}
EXPORT_SYMBOL(s3c_device_adcts);
#endif /* CONFIG_S5PV210_ADCTS */

#ifdef CONFIG_S5P_ADC
/* ADCTS */
static struct resource s3c_adc_resource[] = {
        [0] = {
                .start = S3C_PA_ADC,
                .end   = S3C_PA_ADC + SZ_4K - 1,
                .flags = IORESOURCE_MEM,
        },
        [1] = {
                .start = IRQ_PENDN,
                .end   = IRQ_PENDN,
                .flags = IORESOURCE_IRQ,
        },
        [2] = {
                .start = IRQ_ADC,
                .end   = IRQ_ADC,
                .flags = IORESOURCE_IRQ,
        }

};

struct platform_device s3c_device_adc = {
	.name		  = "s3c-adc",
        .id               = -1,
	.num_resources	  = ARRAY_SIZE(s3c_adc_resource),
	.resource	  = s3c_adc_resource,
};

void __init s3c_adc_set_platdata(struct s3c_adc_mach_info *pd)
{
	struct s3c_adc_mach_info *npd;

        npd = kmalloc(sizeof(*npd), GFP_KERNEL);
        if (npd) {
                memcpy(npd, pd, sizeof(*npd));
		s3c_device_adc.dev.platform_data = npd;
        } else {
                printk(KERN_ERR "no memory for ADC platform data\n");
        }
}
EXPORT_SYMBOL(s3c_device_adc);
#endif /* CONFIG_S5P_ADC */

/* TVOUT interface */
static struct resource s5p_tvout_resources[] = {
        [0] = {
                .start  = S5PV210_PA_TVENC,
                .end    = S5PV210_PA_TVENC + S5PV210_SZ_TVENC - 1,
                .flags  = IORESOURCE_MEM,
        },
        [1] = {
                .start  = S5PV210_PA_VP,
                .end    = S5PV210_PA_VP + S5PV210_SZ_VP - 1,
                .flags  = IORESOURCE_MEM,
        },
        [2] = {
                .start  = S5PV210_PA_MIXER,
                .end    = S5PV210_PA_MIXER + S5PV210_SZ_MIXER - 1,
                .flags  = IORESOURCE_MEM,
        },
        [3] = {
                .start  = S5PV210_PA_HDMI,
                .end    = S5PV210_PA_HDMI + S5PV210_SZ_HDMI - 1,
                .flags  = IORESOURCE_MEM,
        },
        [4] = {
                .start  = I2C_HDMI_PHY_BASE,
                .end    = I2C_HDMI_PHY_BASE + I2C_HDMI_SZ_PHY_BASE - 1,
                .flags  = IORESOURCE_MEM,
        },
        [5] = {
                .start  = IRQ_MIXER,
                .end    = IRQ_MIXER,
                .flags  = IORESOURCE_IRQ,
        },
        [6] = {
                .start  = IRQ_HDMI,
                .end    = IRQ_HDMI,
                .flags  = IORESOURCE_IRQ,
        },
        [7] = {
                .start  = IRQ_TVENC,
                .end    = IRQ_TVENC,
                .flags  = IORESOURCE_IRQ,
        },
        [8] = {
                .start  = IRQ_EINT5,
                .end    = IRQ_EINT5,
                .flags  = IORESOURCE_IRQ,
        }
};

struct platform_device s5p_device_tvout = {
	.name           = "s5p-tvout",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(s5p_tvout_resources),
	.resource       = s5p_tvout_resources,
};
EXPORT_SYMBOL(s5p_device_tvout);

/* CEC */
static struct resource s5p_cec_resources[] = {
	[0] = {
		.start  = S5PV210_PA_CEC,
		.end    = S5PV210_PA_CEC + S5PV210_SZ_CEC - 1,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_CEC,
		.end    = IRQ_CEC,
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device s5p_device_cec = {
	.name           = "s5p-cec",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(s5p_cec_resources),
	.resource       = s5p_cec_resources,
};
EXPORT_SYMBOL(s5p_device_cec);

/* HPD */
struct platform_device s5p_device_hpd = {
	.name           = "s5p-hpd",
	.id             = -1,
};
EXPORT_SYMBOL(s5p_device_hpd);

/* FIMG-2D controller */
static struct resource s5p_g2d_resource[] = {
	[0] = {
		.start	= S5P_PA_G2D,
		.end	= S5P_PA_G2D + S5P_SZ_G2D - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_2D,
		.end	= IRQ_2D,
		.flags	= IORESOURCE_IRQ,
	}
};

struct platform_device s5p_device_g2d = {
	.name		= "s5p-g2d",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s5p_g2d_resource),
	.resource	= s5p_g2d_resource
};
EXPORT_SYMBOL(s5p_device_g2d);

/* OneNAND Controller */
static struct resource s3c_onenand_resource[] = {
	[0] = {
		.start = S5P_PA_ONENAND,
		.end   = S5P_PA_ONENAND + S5P_SZ_ONENAND - 1,
		.flags = IORESOURCE_MEM,
	}
};

struct platform_device s3c_device_onenand = {
	.name		= "onenand",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_onenand_resource),
	.resource	= s3c_onenand_resource,
};

EXPORT_SYMBOL(s3c_device_onenand);

#if defined(CONFIG_S5P_DEV_FB)
static struct resource s3cfb_resource[] = {
	[0] = {
		.start = S5P_PA_LCD,
		.end   = S5P_PA_LCD + S5P_SZ_LCD - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_LCD1,
		.end   = IRQ_LCD1,
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.start = IRQ_LCD0,
		.end   = IRQ_LCD0,
		.flags = IORESOURCE_IRQ,
	},
};

static u64 fb_dma_mask = 0xffffffffUL;

struct platform_device s3c_device_fb = {
	.name		  = "s3cfb",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(s3cfb_resource),
	.resource	  = s3cfb_resource,
	.dev		  = {
		.dma_mask		= &fb_dma_mask,
		.coherent_dma_mask	= 0xffffffffUL
	}
};

static struct s3c_platform_fb default_fb_data __initdata = {
#if defined(CONFIG_CPU_S5PV210_EVT0)
	.hw_ver	= 0x60,
#else
	.hw_ver	= 0x62,
#endif
	.nr_wins = 5,
	.default_win = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap = FB_SWAP_WORD | FB_SWAP_HWORD,
};

void __init s3cfb_set_platdata(struct s3c_platform_fb *pd)
{
	struct s3c_platform_fb *npd;
	int i;

	if (!pd)
		pd = &default_fb_data;

	npd = kmemdup(pd, sizeof(struct s3c_platform_fb), GFP_KERNEL);
	if (!npd)
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);
	else {
		for (i = 0; i < npd->nr_wins; i++)
			npd->nr_buffers[i] = 1;

		npd->nr_buffers[npd->default_win] = CONFIG_FB_S3C_NR_BUFFERS;

		s3cfb_get_clk_name(npd->clk_name);
		//npd->cfg_gpio = s3cfb_cfg_gpio;
		//npd->backlight_onoff = s3cfb_backlight_onoff;
		npd->backlight_onoff = NULL;
		//npd->reset_lcd = s3cfb_reset_lcd;
		//npd->reset_lcd =NULL;
		npd->clk_on = s3cfb_clk_on;
		npd->clk_off = s3cfb_clk_off;

		s3c_device_fb.dev.platform_data = npd;
	}
}
#endif


#if defined(CONFIG_S5P_DEV_FIMC0)
static struct resource s3c_fimc0_resource[] = {
	[0] = {
		.start	= S5P_PA_FIMC0,
		.end	= S5P_PA_FIMC0 + S5P_SZ_FIMC0 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_FIMC0,
		.end	= IRQ_FIMC0,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_fimc0 = {
	.name		= "s3c-fimc",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(s3c_fimc0_resource),
	.resource	= s3c_fimc0_resource,
};

static struct s3c_platform_fimc default_fimc0_data __initdata = {
	.default_cam	= CAMERA_PAR_A,
#if defined(CONFIG_CPU_S5PV210_EVT1)
	.hw_ver	= 0x45,
#else
	.hw_ver	= 0x43,
#endif
};

void __init s3c_fimc0_set_platdata(struct s3c_platform_fimc *pd)
{
	struct s3c_platform_fimc *npd;

	if (!pd)
		pd = &default_fimc0_data;

	npd = kmemdup(pd, sizeof(struct s3c_platform_fimc), GFP_KERNEL);
	if (!npd)
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);
	else {
		if (!npd->cfg_gpio)
			npd->cfg_gpio = s3c_fimc0_cfg_gpio;

		if (!npd->clk_on)
			npd->clk_on = s3c_fimc_clk_on;

		if (!npd->clk_off)
			npd->clk_off = s3c_fimc_clk_off;

#if defined(CONFIG_CPU_S5PV210_EVT1)
		npd->hw_ver = 0x45;
#else
		npd->hw_ver = 0x43;
#endif

		s3c_device_fimc0.dev.platform_data = npd;
	}
}
#endif

#if defined(CONFIG_S5P_DEV_FIMC1)
static struct resource s3c_fimc1_resource[] = {
	[0] = {
		.start	= S5P_PA_FIMC1,
		.end	= S5P_PA_FIMC1 + S5P_SZ_FIMC1 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_FIMC1,
		.end	= IRQ_FIMC1,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_fimc1 = {
	.name		= "s3c-fimc",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(s3c_fimc1_resource),
	.resource	= s3c_fimc1_resource,
};

static struct s3c_platform_fimc default_fimc1_data __initdata = {
	.default_cam	= CAMERA_PAR_A,
#if defined(CONFIG_CPU_S5PV210_EVT1)
	.hw_ver	= 0x50,
#else
	.hw_ver	= 0x43,
#endif
};

void __init s3c_fimc1_set_platdata(struct s3c_platform_fimc *pd)
{
	struct s3c_platform_fimc *npd;

	if (!pd)
		pd = &default_fimc1_data;

	npd = kmemdup(pd, sizeof(struct s3c_platform_fimc), GFP_KERNEL);
	if (!npd)
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);
	else {
		if (!npd->cfg_gpio)
			npd->cfg_gpio = s3c_fimc1_cfg_gpio;

		if (!npd->clk_on)
			npd->clk_on = s3c_fimc_clk_on;

		if (!npd->clk_off)
			npd->clk_off = s3c_fimc_clk_off;

#if defined(CONFIG_CPU_S5PV210_EVT1)
		npd->hw_ver = 0x50;
#else
		npd->hw_ver = 0x43;
#endif

		s3c_device_fimc1.dev.platform_data = npd;
	}
}
#endif

#if defined(CONFIG_S5P_DEV_FIMC2)
static struct resource s3c_fimc2_resource[] = {
	[0] = {
		.start	= S5P_PA_FIMC2,
		.end	= S5P_PA_FIMC2 + S5P_SZ_FIMC2 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_FIMC2,
		.end	= IRQ_FIMC2,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device s3c_device_fimc2 = {
	.name		= "s3c-fimc",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(s3c_fimc2_resource),
	.resource	= s3c_fimc2_resource,
};

static struct s3c_platform_fimc default_fimc2_data __initdata = {
	.default_cam	= CAMERA_PAR_A,
#if defined(CONFIG_CPU_S5PV210_EVT1)
	.hw_ver	= 0x45,
#else
	.hw_ver	= 0x43,
#endif
};

void __init s3c_fimc2_set_platdata(struct s3c_platform_fimc *pd)
{
	struct s3c_platform_fimc *npd;

	if (!pd)
		pd = &default_fimc2_data;

	npd = kmemdup(pd, sizeof(struct s3c_platform_fimc), GFP_KERNEL);
	if (!npd)
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);
	else {
		if (!npd->cfg_gpio)
			npd->cfg_gpio = s3c_fimc2_cfg_gpio;

		if (!npd->clk_on)
			npd->clk_on = s3c_fimc_clk_on;

		if (!npd->clk_off)
			npd->clk_off = s3c_fimc_clk_off;

#if defined(CONFIG_CPU_S5PV210_EVT1)
		npd->hw_ver = 0x45;
#else
		npd->hw_ver = 0x43;
#endif

		s3c_device_fimc2.dev.platform_data = npd;
	}
}
#endif

#if defined(CONFIG_S5P_DEV_IPC)	
static struct resource s3c_ipc_resource[] = {
	[0] = {
		.start	= S5P_PA_IPC,
		.end	= S5P_PA_IPC + S5P_SZ_IPC - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device s3c_device_ipc = {
	.name		= "s3c-ipc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(s3c_ipc_resource),
	.resource	= s3c_ipc_resource,
};

static struct resource s3c_csis_resource[] = {
	[0] = {
		.start	= S5P_PA_CSIS,
		.end	= S5P_PA_CSIS + S5P_SZ_CSIS - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_MIPICSI,
		.end	= IRQ_MIPICSI,
		.flags	= IORESOURCE_IRQ,
	},
};
#endif

#if defined(CONFIG_S5P_DEV_CSIS)
struct platform_device s3c_device_csis = {
	.name		= "s3c-csis",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(s3c_csis_resource),
	.resource	= s3c_csis_resource,
};

static struct s3c_platform_csis default_csis_data __initdata = {
	.srclk_name	= "mout_mpll",
	.clk_name	= "sclk_csis",
	.clk_rate	= 166000000,
};

void __init s3c_csis_set_platdata(struct s3c_platform_csis *pd)
{
	struct s3c_platform_csis *npd;

	if (!pd)
		pd = &default_csis_data;

	npd = kmemdup(pd, sizeof(struct s3c_platform_csis), GFP_KERNEL);
	if (!npd)
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);

	npd->cfg_gpio = s3c_csis_cfg_gpio;
	npd->cfg_phy_global = s3c_csis_cfg_phy_global;

	s3c_device_csis.dev.platform_data = npd;
}
#endif

