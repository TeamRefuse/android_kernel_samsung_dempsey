/*
 * ldi AMOLED LCD panel driver.
 *
 *
 * Copyright (c) 2009 Samsung Electronics
 * Aakash Manik <aakash.manik@samsung.com>
 *
 * Derived from drivers/video/samsung/ld9040.c
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/lcd.h>
#include <linux/backlight.h>

#include <plat/gpio-cfg.h>

//#include <plat/s5pv310.h>
#include "s3cfb.h"
#include "ldi_common.h"
#include "ld9040_gamma.h"
#include "ea8868_gamma.h"
#include "ld9040_sm2_gamma.h"

static int LCD_ID;

#ifdef LCD_ON_FROM_BOOTLOADER
static int on_firsboot = 1;
#endif

#if defined (CONFIG_FB_S3C_MDNIE)
extern void init_mdnie_class(void);
#endif

////////////////////////////Need to be Checked
#if defined (CONFIG_FB_S3C_MDNIE_TUNINGMODE_FOR_BACKLIGHT)
extern void mDNIe_Mode_set_for_backlight(u16 *buf);
extern u16 *pmDNIe_Gamma_set[];
extern int pre_val;
extern int autobrightness_mode;
#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)
extern int capella_pre_val;
//extern int taos_pre_val;
extern int capella_autobrightness_mode;
extern int taos_autobrightness_mode;
#endif
#endif
////////////////////////////Need to be Checked


unsigned int g_Brightness_Level; 
int current_gamma_value = -1;
static int ldi_enable = 0;

static int disp_ready = 0;


#ifdef CONFIG_FB_S3C_LDI_ACL
static int acl_cntrl_enable = 0;

struct class *acl_class;
struct device *switch_aclset_dev;


#endif //CONFIG_FB_S3C_LDI_ACL


#define GAMMASET_CONTROL

#ifdef GAMMASET_CONTROL
struct class *gammaset_class;
struct device *switch_gammaset_dev;
#endif

static int on_19gamma;   //Default 0 if GAMMASET_CONTROL not defined

////////////////////Common LDI settings

const unsigned short SEQ_SWRESET[] = {
	0x01, COMMAND_ONLY,
	ENDDEF, 0x00
};

const unsigned short SEQ_USER_SETTING[] = {
	0xF0, 0x5A,

	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

const unsigned short SEQ_DISPCTL[] = {
	0xF2, 0xFE,

#if 1
	DATA_ONLY, 0x06,
	DATA_ONLY, 0x0A,
#else
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x0A,
#endif
	DATA_ONLY, 0x12,
	DATA_ONLY, 0x10,
	ENDDEF, 0x00
};

const unsigned short SEQ_GTCON[] = {
	0xF7, 0x09,

	ENDDEF, 0x00
};

const unsigned short SEQ_PANEL_CONDITION[] = {
	0xF8, 0x05,
	DATA_ONLY, 0x5E,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x6B,
	DATA_ONLY, 0x7D,
	DATA_ONLY, 0x0D,
	DATA_ONLY, 0x3F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x32,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x07,
	DATA_ONLY, 0x07,
	DATA_ONLY, 0x20,
	DATA_ONLY, 0x20,
	DATA_ONLY, 0x20,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	ENDDEF, 0x00
};


const unsigned short SEQ_ELVSS_ON[] = {
	0xb1, 0x0F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x16,
	ENDDEF, 0x00
};


const unsigned short SEQ_SLPOUT[] = {
	0x11, COMMAND_ONLY,
	ENDDEF, 0x00
};

 const unsigned short SEQ_DISPON[] = {
	0x29, COMMAND_ONLY,
	ENDDEF, 0x00
};

#ifdef CONFIG_FB_S3C_LDI_ACL

const unsigned short SEQ_ACL_ENABLE[] = {
	0xC0, 0x01,
	ENDDEF, 0x00
};


const unsigned short SEQ_ACL_DISABLE[] = {
	0xC0, 0x00,
	ENDDEF, 0x00
};

const unsigned short SEQ_ACL_CUTOFF_8p[] = {
	0xC1, 0x4D,

	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x02,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x04,
	DATA_ONLY, 0x05,
	DATA_ONLY, 0x06,
	DATA_ONLY, 0x07,
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x09,
	DATA_ONLY, 0x0A,
	DATA_ONLY, 0x0B,

	ENDDEF, 0x00
};


const unsigned short SEQ_ACL_CUTOFF_14p[] = {
	0xC1,	0x4D,

	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,       	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x05,
	DATA_ONLY, 0x06,
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x0A,
	DATA_ONLY, 0x0C,	
	DATA_ONLY, 0x0E,
	DATA_ONLY, 0x0F,
	DATA_ONLY, 0x11,
	DATA_ONLY, 0x13,

	ENDDEF, 0x00
};

const unsigned short SEQ_ACL_CUTOFF_20p[] = {
	0xC1, 0x4D,

	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,       	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x08,       	
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0x0F,
	DATA_ONLY, 0x12,
	DATA_ONLY, 0x14,       	
	DATA_ONLY, 0x15,
	DATA_ONLY, 0x17,
	DATA_ONLY, 0x18,
	DATA_ONLY, 0x19,

	ENDDEF, 0x00
};

const unsigned short SEQ_ACL_CUTOFF_24p[] = {
	0xC1, 0x4D,

	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,       	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x04,
	DATA_ONLY, 0x07,       	
	DATA_ONLY, 0x0A,
	DATA_ONLY, 0x0D,
	DATA_ONLY, 0x11,
	DATA_ONLY, 0x14,       	
	DATA_ONLY, 0x17,
	DATA_ONLY, 0x1A,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x20,

	ENDDEF, 0x00
};

const unsigned short SEQ_ACL_CUTOFF_28p[] = {
	0xC1, 0x4D,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x05,
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0x0F,
	DATA_ONLY, 0x13,
	DATA_ONLY, 0x17,
	DATA_ONLY, 0x1A,
	DATA_ONLY, 0x1E,
	DATA_ONLY, 0x21,
	DATA_ONLY, 0x25,

	ENDDEF, 0x00
};


const unsigned short SEQ_ACL_CUTOFF_32p[] = {
	0xC1, 0x4D,

	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x05,
	DATA_ONLY, 0x09,       	
	DATA_ONLY, 0x0D,
	DATA_ONLY, 0x11,
	DATA_ONLY, 0x16,
	DATA_ONLY, 0x1A,       	
	DATA_ONLY, 0x1E,
	DATA_ONLY, 0x22,
	DATA_ONLY, 0x26,
	DATA_ONLY, 0x2A,

	ENDDEF, 0x00
};

const unsigned short SEQ_ACL_CUTOFF_35p[] = {
	0xC1, 0x4D,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x06,
	DATA_ONLY, 0x0A,
	DATA_ONLY, 0x0F,
	DATA_ONLY, 0x13,
	DATA_ONLY, 0x18,
	DATA_ONLY, 0x1C,       	
	DATA_ONLY, 0x21,
	DATA_ONLY, 0x25,
	DATA_ONLY, 0x2A,
	DATA_ONLY, 0x2E,

	ENDDEF, 0x00
};

const unsigned short SEQ_ACL_CUTOFF_37p[] = {
	0xC1, 0x4D,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x06,
	DATA_ONLY, 0x0B,       	
	DATA_ONLY, 0x10,
	DATA_ONLY, 0x15,
	DATA_ONLY, 0x1A,
	DATA_ONLY, 0x1E,       	
	DATA_ONLY, 0x23,
	DATA_ONLY, 0x28,
	DATA_ONLY, 0x2D,
	DATA_ONLY, 0x32,

	ENDDEF, 0x00
};


const unsigned short SEQ_ACL_CUTOFF_40p[] = {
	0xC1, 0x4D,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,       	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x06,
	DATA_ONLY, 0x11,       	
	DATA_ONLY, 0x1A,
	DATA_ONLY, 0x20,
	DATA_ONLY, 0x25,
	DATA_ONLY, 0x29,       	
	DATA_ONLY, 0x2D,
	DATA_ONLY, 0x30,
	DATA_ONLY, 0x33,
	DATA_ONLY, 0x35,
	
	ENDDEF, 0x00
};

const unsigned short SEQ_ACL_CUTOFF_43p[] = {
	0xC1, 0x4D,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,       	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x07,
	DATA_ONLY, 0x12,       	
	DATA_ONLY, 0x1C,
	DATA_ONLY, 0x23,
	DATA_ONLY, 0x29,
	DATA_ONLY, 0x2D,       	
	DATA_ONLY, 0x31,
	DATA_ONLY, 0x34,
	DATA_ONLY, 0x37,
	DATA_ONLY, 0x3A,
	
	ENDDEF, 0x00
};


const unsigned short SEQ_ACL_CUTOFF_45p[] = {
	0xC1, 0x4D,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,       	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x07,
	DATA_ONLY, 0x13,       	
	DATA_ONLY, 0x1E,
	DATA_ONLY, 0x25,
	DATA_ONLY, 0x2B,
	DATA_ONLY, 0x30,       	
	DATA_ONLY, 0x34,
	DATA_ONLY, 0x37,
	DATA_ONLY, 0x3A,
	DATA_ONLY, 0x3D,
	
	ENDDEF, 0x00
};

const unsigned short SEQ_ACL_CUTOFF_47p[] = {
	0xC1, 0x4D,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,       	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x07,
	DATA_ONLY, 0x14,       	
	DATA_ONLY, 0x20,
	DATA_ONLY, 0x28,
	DATA_ONLY, 0x2E,
	DATA_ONLY, 0x33,       	
	DATA_ONLY, 0x37,
	DATA_ONLY, 0x3B,
	DATA_ONLY, 0x3E,
	DATA_ONLY, 0x41,
	
	ENDDEF, 0x00
};

const unsigned short SEQ_ACL_CUTOFF_48p[] = {
	0xC1, 0x4D,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,       	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x15,
	DATA_ONLY, 0x20,
	DATA_ONLY, 0x29,
	DATA_ONLY, 0x2F,
	DATA_ONLY, 0x34,       	
	DATA_ONLY, 0x39,
	DATA_ONLY, 0x3D,
	DATA_ONLY, 0x40,
	DATA_ONLY, 0x43,
	
	ENDDEF, 0x00
};

const unsigned short SEQ_ACL_CUTOFF_50p[] = {
	0xC1, 0x4D,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,       	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x16,       	
	DATA_ONLY, 0x22,
	DATA_ONLY, 0x2B,
	DATA_ONLY, 0x31,
	DATA_ONLY, 0x37,       	
	DATA_ONLY, 0x3B,
	DATA_ONLY, 0x3F,
	DATA_ONLY, 0x43,
	DATA_ONLY, 0x46,	
	
	ENDDEF, 0x00
};

// 55
const unsigned short SEQ_ACL_CUTOFF_55p[] = {
	0xC1, 0x4D,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,       	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x09,
	DATA_ONLY, 0x18,       	
	DATA_ONLY, 0x26,
	DATA_ONLY, 0x2F,
	DATA_ONLY, 0x37,
	DATA_ONLY, 0x3D,       	
	DATA_ONLY, 0x42,
	DATA_ONLY, 0x47,
	DATA_ONLY, 0x4A,
	DATA_ONLY, 0x4E,	
	
	ENDDEF, 0x00
};

const unsigned short SEQ_ACL_CUTOFF_60p[] = {	
	0xC1, 0x4D,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x0A,
	DATA_ONLY, 0x1B,
	DATA_ONLY, 0x2A,
	DATA_ONLY, 0x35,
	DATA_ONLY, 0x3D,
	DATA_ONLY, 0x44,
	DATA_ONLY, 0x4A,
	DATA_ONLY, 0x4F,
	DATA_ONLY, 0x53,
	DATA_ONLY, 0x57,
	
	ENDDEF, 0x00
};

const unsigned short SEQ_ACL_CUTOFF_65p[] = {
	0xC1, 0x4D,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,       	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x0B,
	DATA_ONLY, 0x1E,       	
	DATA_ONLY, 0x2F,
	DATA_ONLY, 0x3B,
	DATA_ONLY, 0x44,
	DATA_ONLY, 0x4C,       	
	DATA_ONLY, 0x52,
	DATA_ONLY, 0x58,
	DATA_ONLY, 0x5D,
	DATA_ONLY, 0x61,	

	ENDDEF, 0x00
};

// 70
const unsigned short SEQ_ACL_CUTOFF_70p[] = {
	0xC1, 0x4D,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,       	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x0C,
	DATA_ONLY, 0x21,       	
	DATA_ONLY, 0x34,
	DATA_ONLY, 0x41,
	DATA_ONLY, 0x4B,
	DATA_ONLY, 0x53,       	
	DATA_ONLY, 0x5B,
	DATA_ONLY, 0x61,
	DATA_ONLY, 0x66,
	DATA_ONLY, 0x6B,
	
	ENDDEF, 0x00
};

const unsigned short SEQ_ACL_CUTOFF_75p[] = {
	0xC1, 0x4D,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,       	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x0D,
	DATA_ONLY, 0x24,       	
	DATA_ONLY, 0x39,
	DATA_ONLY, 0x47,
	DATA_ONLY, 0x53,
	DATA_ONLY, 0x5C,       	
	DATA_ONLY, 0x64,
	DATA_ONLY, 0x6B,
	DATA_ONLY, 0x71,
	DATA_ONLY, 0x76,	

	ENDDEF, 0x00
};

const unsigned short SEQ_ACL_CUTOFF_80p[] = {
	0xC1, 0x4D,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x0E,
	DATA_ONLY, 0x28,       	
	DATA_ONLY, 0x3F,
	DATA_ONLY, 0x4F,
	DATA_ONLY, 0x5C,
	DATA_ONLY, 0x66,       	
	DATA_ONLY, 0x6F,
	DATA_ONLY, 0x76,
	DATA_ONLY, 0x7D,
	DATA_ONLY, 0x83,	

	ENDDEF, 0x00
};

const unsigned short SEQ_ACL_CUTOFF_85p[] = {
	0xC1, 0x4D,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0xDF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,       	
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x01,
	DATA_ONLY, 0x10,
	DATA_ONLY, 0x2D,       	
	DATA_ONLY, 0x47,
	DATA_ONLY, 0x59,
	DATA_ONLY, 0x67,
	DATA_ONLY, 0x73,       	
	DATA_ONLY, 0x7C,
	DATA_ONLY, 0x85,
	DATA_ONLY, 0x8C,
	DATA_ONLY, 0x93,	

	ENDDEF, 0x00
};


const unsigned short *SEQ_ACL_CUTOFF_SET[] = {
	SEQ_ACL_DISABLE, //0
	SEQ_ACL_CUTOFF_8p,
	SEQ_ACL_CUTOFF_14p,
	SEQ_ACL_CUTOFF_20p,
	SEQ_ACL_CUTOFF_24p,
	SEQ_ACL_CUTOFF_28p, //5
	SEQ_ACL_CUTOFF_32p,
	SEQ_ACL_CUTOFF_35p,
	SEQ_ACL_CUTOFF_37p,
	SEQ_ACL_CUTOFF_40p, 
	SEQ_ACL_CUTOFF_43p, //10
	SEQ_ACL_CUTOFF_45p, 
	SEQ_ACL_CUTOFF_47p, 
	SEQ_ACL_CUTOFF_48p, 
	SEQ_ACL_CUTOFF_50p, 
	SEQ_ACL_CUTOFF_55p, //15
	SEQ_ACL_CUTOFF_60p, 
	SEQ_ACL_CUTOFF_65p, 
	SEQ_ACL_CUTOFF_70p, 
	SEQ_ACL_CUTOFF_75p, 
	SEQ_ACL_CUTOFF_80p, //20
	SEQ_ACL_CUTOFF_85p, //21
};

#endif //CONFIG_FB_S3C_LDI_ACL

////////////////////Common LDI settings



///////////////////SysFS Interfaces

#ifdef CONFIG_FB_S3C_LDI_ACL

static ssize_t aclset_file_cmd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("called %s \n",__func__);

	return sprintf(buf,"%u\n", acl_cntrl_enable);
}
static ssize_t aclset_file_cmd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, value;

	sscanf(buf, "%d", &value);

	printk("[acl set] in aclset_file_cmd_store, input value = %d \n", value);

	if (!IsLDIEnabled())	{
		printk("[acl set] return because LDI is disabled, input value = %d \n", value);
		return size;
	}
	if ((value != 0) && (value != 1))	{
		printk("\naclset_file_cmd_store value is same : value(%d)\n", value);
		return size;
	}

	if (acl_cntrl_enable != value)	{
		acl_cntrl_enable = value;
	
		if (acl_cntrl_enable)	{
			ret = ldi_set_acl(g_Brightness_Level); 
			if (ret) {
				printk( "lcd brightness setting failed.\n");
				return -EIO;
			}		
	
		}
		else	{
			ret = ldi_panel_send_sequence(SEQ_ACL_DISABLE);
			if (ret) {
				printk( "lcd brightness setting failed.\n");
				return -EIO;
			}
	
		}
	}

	return size;
}

static DEVICE_ATTR(aclset_file_cmd,0666, aclset_file_cmd_show, aclset_file_cmd_store);
#endif //CONFIG_FB_S3C_LDI_ACL



#ifdef GAMMASET_CONTROL //for 1.9/2.2 gamma control from platform
static ssize_t gammaset_file_cmd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("called %s \n",__func__);
	return sprintf(buf,"%u %u\n", g_Brightness_Level, on_19gamma);
}
static ssize_t gammaset_file_cmd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int value;
	
    sscanf(buf, "%d", &value);

	//printk(KERN_INFO "[gamma set] in gammaset_file_cmd_store, input value = %d \n",value);
	if (!IsLDIEnabled())	{
		printk("[gamma set] return because LDI is disabled, input value = %d \n", value);
		return size;
	}

	if ((value != 0) && (value != 1))	{
		printk("\ngammaset_file_cmd_store value(%d) on_19gamma(%d) \n", value,on_19gamma);
		return size;
	}

	if (value != on_19gamma)	{
		on_19gamma = value;
		ldi_gamma_ctl(g_Brightness_Level);
	}

	return size;
}

static DEVICE_ATTR(gammaset_file_cmd,0666, gammaset_file_cmd_show, gammaset_file_cmd_store);
#endif



//////////////////////////////SysFS Interfaces


int IsLDIEnabled(void)
{
	return ldi_enable;
}

static void wait_ldi_enable(void)
{
	int i = 0;

	for (i = 0; i < 100; i++)	{
		printk("ldi_enable : %d \n", ldi_enable);

		if(IsLDIEnabled())
			break;
		
		msleep(10);
	};
}


static void wait_disp_ready(void)
{
	int i = 0;

	for (i = 0; i < 200; i++){
	if(disp_ready)
	break;
		
		msleep(10);
	};
}


static void SetLDIEnabledFlag(int OnOff)
{
	ldi_enable = OnOff;
}

struct ldi g_lcd;
static DEFINE_MUTEX(spi_use);

int ldi_spi_write_byte(int addr, int data)
{
	u16 buf[1];
	struct spi_message msg;

	struct spi_transfer xfer = {
		.len		= 2,
		.tx_buf		= buf,
	};

	buf[0] = (addr << 8) | data;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	return spi_sync(g_lcd.spi, &msg);
}

 int ldi_spi_write(unsigned char address, unsigned char command)
{
	int ret = 0;

	if (address != DATA_ONLY)
		ret = ldi_spi_write_byte(0x0, address);
	if (command != COMMAND_ONLY)
		ret = ldi_spi_write_byte(0x1, command);

	return ret;
}


 int ldi_panel_send_sequence(const unsigned short *wbuf)
{
	int ret = 0, i = 0;

	mutex_lock(&spi_use);
	while ((wbuf[i] & DEFMASK) != ENDDEF) {
		if ((wbuf[i] & DEFMASK) != SLEEPMSEC) {
			ret = ldi_spi_write(wbuf[i], wbuf[i+1]);
			if (ret)
				break;
		} else
			udelay(wbuf[i+1]*1000);
		i += 2;
	}
	mutex_unlock(&spi_use);

	return ret;
}



 int ldi_gamma_ctl(int gamma)
{
	int ret = 0;

	if(LCD_ID == 1)
	{
		if(!on_19gamma)
		{
		ret = ld9040_gamma_ctl(gamma);
	}
		else
		{
			ret = ld9040_19gamma_ctl(gamma);
		}
	}
	else if (LCD_ID == 2)
	{
		if(!on_19gamma)
		{
		ret = ea8868_gamma_ctl(gamma);
	}	
	else
	{
			ret = ea8868_19gamma_ctl(gamma);
		}
	}
	else if (LCD_ID == 3)
	{
		if(!on_19gamma)
		{
			ret = ld9040_sm2_gamma_ctl(gamma);
		}	

		else
		{
			ret = ld9040_sm2_19gamma_ctl(gamma);
		}
	}
	else
	{
		printk("######ldi_unidentified######\n");
		return -1;	
	}

	return ret;
}

 int ldi_set_elvss(int brightness)
{
	int ret = 0;
	if(LCD_ID == 1)
	{
		ret = ld9040_set_elvss(brightness);
	}
	else if (LCD_ID == 2)
	{
		ret = ea8868_set_elvss(brightness);
	}	
	else if (LCD_ID == 3)
	{
		ret = ld9040_sm2_set_elvss(brightness);
	}		
	else
	{
		printk("######ldi_unidentified######\n");
		return -1;	
	}
	return ret;
}


#ifdef CONFIG_FB_S3C_LDI_ACL
 int ldi_set_acl(int brightness)
{
	int ret = 0;

//	printk("#######ldi_set_acl#######");

	const unsigned short * acl_mode;
	switch (brightness) {

	case 0: //30cd/m2		
	case 1: //40cd/m2 
		acl_mode = SEQ_ACL_DISABLE;	
		//printk("level  = %d\n",brightness);
		break;		
	case 2: //70cd/m2
	case 3: //90cd/m2	
	case 4: //100cd/m2	
	case 5: //1//110cd/m2		
	case 6: //120cd/m2		
	case 7: //130cd/m2
	case 8: //140cd/m2	
	case 9: //150cd/m2
	case 10: //160cd/m2 	
	case 11: //170cd/m2
	case 12: //180cd/m2
		acl_mode = SEQ_ACL_CUTOFF_40p;	
		//printk("level  = %d\n",brightness); 
		break;
	case 13: //190cd/m2 
		acl_mode = SEQ_ACL_CUTOFF_43p;	
		//printk("level  = %d\n",brightness); 
		break;
	case 14: //200cd/m2
		acl_mode = SEQ_ACL_CUTOFF_45p;	
		//printk("level  = %d\n",brightness); 
		break;
	case 15: //210cd/m2
		acl_mode = SEQ_ACL_CUTOFF_47p;	
		//printk("level  = %d\n",brightness); 
		break;
	case 16: //220cd/m2
		acl_mode = SEQ_ACL_CUTOFF_48p;	
		//printk("level  = %d\n",brightness); 
		break;
	case 17: //230cd/m2 
	case 18: //240cd/m2
	case 19: //250cd/m2 	
	case 20: //260cd/m2
	case 21: //270cd/m2 
	case 22: //280cd/m2
	case 23: //290cd/m2 
	case 24: //300cd/m2
		acl_mode = SEQ_ACL_CUTOFF_50p;	
		//printk("level  = %d\n",brightness); 
		break;	
	default:
		acl_mode = SEQ_ACL_CUTOFF_50p;	
		//printk("level  = %d\n",brightness); 
		break;
	}	
	ret = ldi_panel_send_sequence(acl_mode);
	if (ret) {
		printk( "acl settings failed.\n");
		return -EIO;
	}

	if(brightness >= 2)
	{
		ret = ldi_panel_send_sequence(SEQ_ACL_ENABLE);
		if (ret) {
			printk("acl enable failed.\n");
			return -EIO;
		}
	}
		
	return ret;
}
#endif //CONFIG_FB_S3C_LDI_ACL

int ldi_common_init()
{
	int ret, i;
	current_gamma_value = -1; 

	const unsigned short *init_seq_part1[] = {
		//SEQ_SWRESET,
		SEQ_USER_SETTING,
		SEQ_DISPCTL,
		SEQ_GTCON,
		SEQ_PANEL_CONDITION,
		//Settings to be taken care in gamma ctl		
		//SEQ_ELVSS_ON,	
		//SEQ_ELVSS_49,
		//Settings to be taken care in gamma ctl
		//SEQ_SLPOUT,
	};

	for (i = 0; i < ARRAY_SIZE(init_seq_part1); i++) {
		ret = ldi_panel_send_sequence(init_seq_part1[i]);
		//msleep(5);
		if (ret)
			break;
	}
	//msleep(120);
	printk("######ldi_common_init###### %d\n", i);	

	return ret;
}

void ldi_recognize(void)
{

	int id1, id2, id3;

	id1 = ldi_spi_read(0xDA);
	printk("ID1 = %x\n", id1);
	id2 = ldi_spi_read(0xDB);
	printk("ID2 = %x\n", id2);
	id3 = ldi_spi_read(0xDC);
	printk("ID3 = %x\n", id3);

	if((id1 == 0x86)&& (id2 == 0x08) && (id3 == 0x44))
	{
		LCD_ID = 1; 
		printk("######ld9040_ldi######\n");
	}
	else if((id1 == 0x84) && (id2 == 0x08) && (id3 == 0x88))
	{
		LCD_ID = 2; 
		printk("######ea8868_ldi######\n");
	}
	else if((id1 == 0x86  ) && (id2 == 0x48 ) && (id3 == 0x44 ))
	{
		LCD_ID = 3;
		printk("######ld9040_SM2_ldi######\n");
	}
	else
		LCD_ID = 3; 	//Default LDI is ld9040

        return;

}


int ldi_dev_init()
{
	int ret;
	
#ifdef LCD_ON_FROM_BOOTLOADER

	if(on_firsboot)
	{

	ldi_recognize();
	on_firsboot = 0;
	}
#endif

	if(LCD_ID == 1)
	{
		ret = ld9040_ldi_init();
	}
	else if (LCD_ID == 2)
	{
		ret = ea8868_ldi_init();
	}	
	else if(LCD_ID == 3)
	{
		ret = ld9040_sm2_ldi_init();
	}		
	else
	{
		printk("######ldi_unidentified######\n");
		return -1;	
	}	
	
	

	disp_ready = 1; //Display is ready for Gamma Settings 1. First do gamma settings then 2. DISP ON

	printk("######ldi_dev_init complete######\n");

	return ret;
}

int ldi_common_dispon()
{
	int ret ;	
	ret = ldi_panel_send_sequence(SEQ_DISPON);
	return ret; 
}

int ldi_common_enable()
{
	int ret;
	
	if(LCD_ID == 1)
	{
		ret = ld9040_ldi_enable();
	}
	else if (LCD_ID == 2)
	{
		ret = ea8868_ldi_enable();
	}
	else if(LCD_ID == 3)
	{
		ret = ld9040_sm2_ldi_enable(); 
	}
	else
	{
		printk("######ldi_unidentified######\n");
		return -1;	
	}	

	SetLDIEnabledFlag(1);

	return ret;
}

int ldi_common_disable()
{
	int ret;

	if(LCD_ID == 1)
	{
		ret = ld9040_ldi_disable();
	}
	else if (LCD_ID == 2)
	{
		ret = ea8868_ldi_disable();
	}
	else if(LCD_ID == 3)
	{
		ret = ld9040_sm2_ldi_disable();
	}
	else
	{
		printk("######ldi_unidentified######\n");
		return -1;	
	}

	SetLDIEnabledFlag(0);

	disp_ready = 0;
	return ret;
}

int ldi_power_on()
{
	int ret = 0;
	struct lcd_platform_data *pd = NULL;
	pd = g_lcd.lcd_pd;
#if 0
	if (!pd) {
		dev_err(g_lcd.dev, "platform data is NULL.\n");
		return -EFAULT;
	}

	if (!pd->power_on) {
		dev_err(g_lcd.dev, "power_on is NULL.\n");
		return -EFAULT;
	} else {
		pd->power_on(g_lcd.ld, 1);
		msleep(pd->power_on_delay);
	}

	if (!pd->reset) {
		dev_err(g_lcd.dev, "reset is NULL.\n");
		return -EFAULT;
	} else {
		pd->reset(g_lcd.ld);
		msleep(pd->reset_delay);
	}
#endif 
	ret = ldi_common_init();


	if (ret) {
		dev_err(g_lcd.dev, "failed to initialize common ldi.\n");
		return ret;
	}

	ret = ldi_dev_init();

		if (ret) {
		dev_err(g_lcd.dev, "failed to initialize rev specific ldi.\n");
		return ret;
	}


	return 0;
}

int ldi_power_off()
{
	int ret = 0;
	struct lcd_platform_data *pd = NULL;
	current_gamma_value = -1; 

	pd = g_lcd.lcd_pd;
	if (!pd) {
		dev_err(g_lcd.dev, "platform data is NULL.\n");
		return -EFAULT;
	}

	ret = ldi_common_disable();
	if (ret) {
		dev_err(g_lcd.dev, "lcd setting failed.\n");
		return -EIO;
	}

	msleep(pd->power_off_delay);

	if (!pd->power_on) {
		dev_err(g_lcd.dev, "power_on is NULL.\n");
		return -EFAULT;
	} else
		pd->power_on(g_lcd.ld, 0);

	return 0;
}

 int ldi_power(int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(g_lcd.power))
		ret = ldi_power_on();
	else if (!POWER_IS_ON(power) && POWER_IS_ON(g_lcd.power))
		ret = ldi_power_off();

	if (!ret)
		g_lcd.power = power;

	return ret;
}

 int ldi_set_power(struct lcd_device *ld, int power)
{
	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(g_lcd.dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return ldi_power(power);
}

 int ldi_get_power(struct lcd_device *ld)
{
	return g_lcd.power;
}

 int ldi_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

 int ldi_set_brightness(struct backlight_device *bd)
{
	int ret = 0, brightness = bd->props.brightness;

	int gamma_value = 0;
	
#ifdef LCD_ON_FROM_BOOTLOADER

	if(on_firsboot)
	{

	ldi_recognize();
	on_firsboot = 0;
	}
#endif



	//If Display is OFF Brightness settings on resume always need to be called after LDI init 
	if(!IsLDIEnabled())
	{
	wait_disp_ready();
	}

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bd->props.max_brightness) {
		dev_err(&bd->dev, "lcd brightness should be %d to %d.\n",
			MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		return -EINVAL;
	}

	/* brightness setting from platform is from 0 to 255
	 * But in this driver, brightness is only supported from 0 to 24 */
	brightness = g_Brightness_Level = (brightness*10)/106; 
	
	if(current_gamma_value == brightness)
		return; 
	
	printk("platform brightness level : %d\n",brightness);
	ret=ldi_set_elvss(g_Brightness_Level); 
	if (ret) {
		dev_err(&bd->dev, "lcd brightness setting failed.\n");
		return -EIO;
	}
#ifdef CONFIG_FB_S3C_LDI_ACL

	if(acl_cntrl_enable)	
	{
		ret = ldi_set_acl(g_Brightness_Level); 
		if (ret) {
			dev_err(&bd->dev, "lcd brightness setting failed.\n");
			return -EIO;
		}
	}
#endif //CONFIG_FB_S3C_LDI_ACL


	ret = ldi_gamma_ctl(brightness);

	if (ret) {
		dev_err(&bd->dev, "lcd brightness setting failed.\n");
		return -EIO;
	}



	if(!IsLDIEnabled())
	{
	msleep(10);
	 ret = ldi_common_enable();

	printk("######## DISP ON#########");
	 if (ret)
		dev_err(g_lcd.dev, "failed to display ON.\n");

	}
	else
		ldi_common_dispon();
		


	gamma_value = brightness;

#if defined (CONFIG_FB_S3C_MDNIE_TUNINGMODE_FOR_BACKLIGHT)
#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)
		if ((capella_pre_val==1) && (gamma_value < 24) &&(capella_autobrightness_mode)){
			mDNIe_Mode_set_for_backlight(pmDNIe_Gamma_set[2]);
			printk("s5p_bl_update_status - pmDNIe_Gamma_set[2]\n" );
			capella_pre_val = -1;
		}

#else
	if ((pre_val==1) && (gamma_value < 24) &&(autobrightness_mode))		{
		mDNIe_Mode_set_for_backlight(pmDNIe_Gamma_set[2]);
		printk("s5p_bl_update_status - pmDNIe_Gamma_set[2]\n" );
		pre_val = -1;
	}
#endif
#endif

	current_gamma_value = gamma_value;


	return ret;
}

 struct lcd_ops ldi_lcd_ops = {
	.set_power = ldi_set_power,
	.get_power = ldi_get_power,
};

struct backlight_ops ldi_backlight_ops  = {
	.get_brightness = ldi_get_brightness,
	.update_status = ldi_set_brightness,
};


#define FADE_IN          1
#define FADE_OUT       2
 int ldi_dimming(struct ldi *lcd, int  mode)
{
	int ret=0, exit = 0;
	int brightness = 0;
	 int brightness_org = 0;
	struct backlight_device *bd = NULL;

	if(!lcd)
	{
		ret = -1;
		goto err;
	}
	else
		bd = lcd->bd;

	if(mode!=FADE_IN)
		brightness = brightness_org = ldi_get_brightness(bd);

	while(!exit)
	{
		if(mode!=FADE_IN)
		{
			if(brightness>0)
				brightness--;
			else
				exit = 1;
		}
		else
		{
			if(brightness<brightness_org)
				brightness++;
			else
				exit = 1;
		}

		if(!exit)
		{
			bd->props.brightness = brightness;
			ldi_set_brightness(bd);
			msleep(50);
		}
	}

err:
	return ret;
}

 ssize_t ldi_sysfs_dimming_test(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t len)
{
	struct ldi *lcd = dev_get_drvdata(dev);

	ldi_dimming(lcd, FADE_OUT);

	mdelay(50);

	ldi_dimming(lcd, FADE_IN);

	return 0;
}


 ssize_t ldi_sysfs_backlihgt_level_test(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t len)
{
	struct backlight_device *bd = NULL;
	unsigned long brightness;
	int rc;

	bd = g_lcd.bd;
	rc = strict_strtoul(buf, 0, &brightness);
	if (rc < 0)
		return rc;
	else 
		bd->props.brightness = brightness;

	ldi_set_brightness(bd);
	return 0;
}

 DEVICE_ATTR(dimming_test, 0644,
		NULL, ldi_sysfs_dimming_test);

 DEVICE_ATTR(baktst, 0644,
		NULL, ldi_sysfs_backlihgt_level_test);

 ssize_t ldi_sysfs_show_gamma_mode(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	char temp[10];

	switch (g_lcd.gamma_mode) {
	case 0:
		sprintf(temp, "2.2 mode\n");
		strcat(buf, temp);
		break;
	case 1:
		sprintf(temp, "1.9 mode\n");
		strcat(buf, temp);
		break;
	default:
		dev_info(dev, "gamma mode could be 0:2.2, 1:1.9 or 2:1.7)n");
		break;
	}

	return strlen(buf);
}

 ssize_t ldi_sysfs_store_gamma_mode(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t len)
{
	struct backlight_device *bd = NULL;
	int brightness, rc;

	rc = strict_strtoul(buf, 0, (unsigned long *)&g_lcd.gamma_mode);
	if (rc < 0)
		return rc;

	bd = g_lcd.bd;

	brightness = bd->props.brightness;

	/* brightness setting from platform is from 0 to 255
	 * But in this driver, brightness is only supported from 0 to 24 */
	brightness /= 10;
	if (brightness > 24) brightness = 24;

	switch (g_lcd.gamma_mode) {
	case 0:
		ldi_gamma_ctl(brightness);
		break;
	case 1:
		//Mode Unsupported
		break;
	default:
		dev_info(dev, "gamma mode could be 0:2.2, 1:1.9 or 2:1.7\n");
		ldi_gamma_ctl(brightness);
		break;
	}
	return len;
}

 DEVICE_ATTR(gamma_mode, 0644,
		ldi_sysfs_show_gamma_mode, ldi_sysfs_store_gamma_mode);

 ssize_t ldi_sysfs_show_gamma_table(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	char temp[3];

	sprintf(temp, "%d\n", g_lcd.gamma_table_count);
	strcpy(buf, temp);

	return strlen(buf);
}

 DEVICE_ATTR(gamma_table, 0644,
		ldi_sysfs_show_gamma_table, NULL);

 int ldi_probe(struct spi_device *spi)
{
	int ret = 0;
	struct lcd_device *ld = NULL;
	struct backlight_device *bd = NULL;

	/* ldi lcd panel uses 3-wire 9bits SPI Mode. */
	spi->bits_per_word = 9;

	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "spi setup failed.\n");
		goto out_free_lcd;
	}

	g_lcd.spi = spi;
	g_lcd.dev = &spi->dev;

	g_lcd.lcd_pd = (struct lcd_platform_data *)spi->dev.platform_data;
	if (!g_lcd.lcd_pd) {
		dev_err(&spi->dev, "platform data is NULL.\n");
		goto out_free_lcd;
	}

	
	ld = lcd_device_register("ldi_lcd", &spi->dev, &g_lcd, &ldi_lcd_ops);
	if (IS_ERR(ld)) {
		ret = PTR_ERR(ld);
		goto out_free_lcd;
	}

	g_lcd.ld = ld;
	
	
	bd = backlight_device_register("s5p_bl", &spi->dev,&g_lcd, &ldi_backlight_ops);
	//bd = backlight_device_register("pwm-backlight", &spi->dev,&g_lcd, &ldi_backlight_ops,NULL);
	if (IS_ERR(ld)) {
		ret = PTR_ERR(ld);
		goto out_free_lcd;
	}
	
	if(!bd)
	{
		goto out_free_lcd;
	}
	

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = MAX_BRIGHTNESS;
	g_lcd.bd = bd;


	/*
	 * it gets gamma table count available so it gets user
	 * know that.
	 */
	if(LCD_ID == 1)
	{
		g_lcd.gamma_table_count =    sizeof(ld9040_gamma_table) / (MAX_GAMMA_LEVEL * sizeof(int));
	}
	else if (LCD_ID == 2)
	{
		g_lcd.gamma_table_count =    sizeof(ea8868_gamma_table) / (MAX_GAMMA_LEVEL * sizeof(int));
	}
	else if(LCD_ID == 3)
	{
		g_lcd.gamma_table_count =    sizeof(ld9040_sm2_gamma_table) / (MAX_GAMMA_LEVEL * sizeof(int));
	}	


	ret = device_create_file(&(spi->dev), &dev_attr_gamma_mode);
	if (ret < 0)
		dev_err(&(spi->dev), "failed to add sysfs entries\n");

	ret = device_create_file(&(spi->dev), &dev_attr_gamma_table);
	if (ret < 0)
		dev_err(&(spi->dev), "failed to add sysfs entries\n");

	ret = device_create_file(&(spi->dev), &dev_attr_dimming_test);
	if (ret < 0)
		dev_err(&(spi->dev), "failed to add sysfs entries\n");
		
	ret = device_create_file(&(spi->dev), &dev_attr_baktst);
	if (ret < 0)
		dev_err(&(spi->dev), "failed to add sysfs entries\n");


#ifndef LCD_ON_FROM_BOOTLOADER
	g_lcd.lcd_pd->lcd_enabled = 0;
#else
	g_lcd.lcd_pd->lcd_enabled = 1;
#endif
	/*
	 * if lcd panel was on from bootloader like u-boot then
	 * do not lcd on.
	 */
	if (!g_lcd.lcd_pd->lcd_enabled) {
		/*
		 * if lcd panel was off from bootloader then
		 * current lcd status is powerdown and then
		 * it enables lcd panel.
		 */
		g_lcd.power = FB_BLANK_POWERDOWN;

		ldi_power(FB_BLANK_UNBLANK);
	} else
		g_lcd.power = FB_BLANK_UNBLANK;

	

	dev_set_drvdata(&spi->dev, &g_lcd);

////Compatibility APIs
#if defined (CONFIG_FB_S3C_MDNIE)
	init_mdnie_class();  //set mDNIe UI mode, Outdoormode
#endif
////Compatibility APIs




#ifdef CONFIG_FB_S3C_LDI_ACL
 //ACL On,Off
	acl_class = class_create(THIS_MODULE, "aclset");
	if (IS_ERR(acl_class))
		pr_err("Failed to create class(acl_class)!\n");

	switch_aclset_dev = device_create(acl_class, NULL, 0, NULL, "switch_aclset");
	if (IS_ERR(switch_aclset_dev))
		pr_err("Failed to create device(switch_aclset_dev)!\n");

	if (device_create_file(switch_aclset_dev, &dev_attr_aclset_file_cmd) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_aclset_file_cmd.attr.name);
#endif


#ifdef GAMMASET_CONTROL //for 1.9/2.2 gamma control from platform
	gammaset_class = class_create(THIS_MODULE, "gammaset");
	if (IS_ERR(gammaset_class))
		pr_err("Failed to create class(gammaset_class)!\n");

	switch_gammaset_dev = device_create(gammaset_class, NULL, 0, NULL, "switch_gammaset");
	if (IS_ERR(switch_gammaset_dev))
		pr_err("Failed to create device(switch_gammaset_dev)!\n");

	if (device_create_file(switch_gammaset_dev, &dev_attr_gammaset_file_cmd) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_gammaset_file_cmd.attr.name);
#endif


	SetLDIEnabledFlag(1);

	printk("ldi panel driver has been probed.\n");
	return 0;

out_free_lcd:

	printk("ldi panel driver has not been probed.\n");
	return ret;
}

 int __devexit ldi_remove(struct spi_device *spi)
{
	ldi_power(FB_BLANK_POWERDOWN);
	lcd_device_unregister(g_lcd.ld);

	return 0;
}

#if defined(CONFIG_PM)
unsigned int beforepower;

void ldi_power_down()
{
	beforepower = g_lcd.power;
	ldi_power(FB_BLANK_POWERDOWN);
}

void ldi_power_up()
{
	if (beforepower == FB_BLANK_UNBLANK)
		g_lcd.power = FB_BLANK_POWERDOWN;
	ldi_power(beforepower);
}

 int ldi_suspend(struct spi_device *spi, pm_message_t mesg)
{
	int ret = 0;

	dev_dbg(&spi->dev, "g_lcd.power = %d\n", g_lcd.power);

	beforepower = g_lcd.power;

	/*
	 * when lcd panel is suspend, lcd panel becomes off
	 * regardless of status.
	 */
	//ret = ldi_power(FB_BLANK_POWERDOWN);

	return ret;
}

 int ldi_resume(struct spi_device *spi)
{
	int ret = 0;

	/*
	 * after suspended, if lcd panel status is FB_BLANK_UNBLANK
	 * (at that time, power is FB_BLANK_UNBLANK) then
	 * it changes that status to FB_BLANK_POWERDOWN to get lcd on.
	 */
	if (beforepower == FB_BLANK_UNBLANK)
		g_lcd.power = FB_BLANK_POWERDOWN;

	dev_dbg(&spi->dev, "power = %d\n", beforepower);

	//ret = ldi_power(beforepower);

	return ret;
}
#else
#define ldi_suspend		NULL
#define ldi_resume		NULL
#endif

/* Power down all displays on reboot, poweroff or halt. */
 void ldi_shutdown(struct spi_device *spi)
{
	ldi_power(FB_BLANK_POWERDOWN);
}

 struct spi_driver ldi_driver = {
	.driver = { 
		.name	= "amoled display",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
	.probe		= ldi_probe,
	.remove		= __devexit_p(ldi_remove),
	.shutdown	= ldi_shutdown,
};

 int __init ldi_init(void)
{
	return spi_register_driver(&ldi_driver);
}

 void __exit ldi_exit(void)
{
	spi_unregister_driver(&ldi_driver);
}

module_init(ldi_init);
module_exit(ldi_exit);


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



void s3cfb_set_lcd_info(struct s3cfb_global *ctrl)
{
        ctrl->lcd = &ldi_info;
}

MODULE_AUTHOR("Aakash Manik <aakash.manik@samsung.com>");
MODULE_DESCRIPTION("Amoled Common Driver");
MODULE_LICENSE("GPL");

