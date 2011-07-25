/*
 * ldi AMOLED LCD panel driver header file.
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


#define SLEEPMSEC		0x1000
#define ENDDEF			0x2000
#define	DEFMASK			0xFF00
#define COMMAND_ONLY		0xFE
#define DATA_ONLY		0xFF

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255

#define LCD_ON_FROM_BOOTLOADER 1

#define CONFIG_FB_S3C_LDI_ACL 1



#define POWER_IS_ON(pwr)	((pwr) <= FB_BLANK_NORMAL)


int IsLDIEnabled(void);
static void wait_ldi_enable(void);
static void wait_disp_ready(void);
static void SetLDIEnabledFlag(int OnOff);

struct ldi {
	struct device			*dev;
	struct spi_device		*spi;
	unsigned int			power;
	unsigned int			gamma_mode;
	unsigned int			current_brightness;
	unsigned int			gamma_table_count;

	struct lcd_device		*ld;
	struct backlight_device		*bd;
	struct lcd_platform_data	*lcd_pd;
};

int ldi_spi_read(unsigned char reg);
int ldi_spi_read_byte(unsigned char command);

int ldi_spi_write_byte(int addr, int data);

int ldi_spi_write(unsigned char address, unsigned char command);

int ldi_panel_send_sequence(const unsigned short *wbuf);

int ldi_gamma_ctl(int gamma);

int ldi_set_elvss(int brightness);

int ldi_set_acl(int brightness);

int ldi_common_init(void);

int ldi_dev_init(void);

int ldi_common_enable(void);

int ldi_common_disable(void);

int ldi_power_on(void);

int ldi_power_off(void);

int ldi_power(int power);

int ldi_set_power(struct lcd_device *ld, int power);

int ldi_get_power(struct lcd_device *ld);

int ldi_get_brightness(struct backlight_device *bd);

int ldi_set_brightness(struct backlight_device *bd);

int ldi_dimming(struct ldi *lcd, int  mode);


