/*
 * ldi AMOLED LCD panel driver.
 *
 *
 * Copyright (c) 2009 Samsung Electronics
 * Aakash Manik <aakash.manik@samsung.com>
 *
 * Derived from drivers/video/samsung/s3cfb_tl2796.c
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/lcd.h>
#include <linux/backlight.h>

#include <plat/gpio-cfg.h>

#include "ldi_common.h"

#define PACKET_LEN		8
unsigned char DELAY=1;

static DEFINE_MUTEX(spi_rd);



static inline void lcd_cs_value(int level)
{
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
	gpio_set_value(S5PV210_MP05(4), level);//GPIO_DISPLAY_CS
#else
	gpio_set_value(S5PV210_MP01(1), level);//GPIO_DISPLAY_CS
#endif 
}

static inline void lcd_clk_value(int level)
{
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
	gpio_set_value(S5PV210_GPC1(3), level);//GPIO_DISPLAY_CLK
#else
	gpio_set_value(S5PV210_GPC0(1), level);//GPIO_DISPLAY_CLK
#endif 
}

static inline void lcd_si_value(int level)
{
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
	gpio_set_value(S5PV210_GPC1(0), level);//GPIO_DISPLAY_DAT
#else
	gpio_set_value(S5PV210_MP01(0), level);//GPIO_DISPLAY_DAT
#endif 
}

static inline int get_lcd_si_value()
{
	return (gpio_get_value(GPIO_DISPLAY_SI)?1:0);
}


int ldi_spi_read(unsigned char reg)
{
	return ldi_spi_read_byte(reg);
}

int ldi_spi_read_byte(unsigned char command)
{
	int     j,ret;
	unsigned short data;

	mutex_lock(&spi_rd);

	data = (0x00 << 8) + command;

	lcd_cs_value(1);
	lcd_si_value(1);
	lcd_clk_value(1);
	udelay(DELAY);

	lcd_cs_value(0);
	udelay(DELAY);


	for (j = PACKET_LEN; j >= 0; j--)
	{
		lcd_clk_value(0);

		/* data high or low */
		if ((data >> j) & 0x0001)
			lcd_si_value(1);
		else
			lcd_si_value(0);

		udelay(DELAY);

		lcd_clk_value(1);
		udelay(DELAY);
	}

	s3c_gpio_cfgpin(GPIO_DISPLAY_SI, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_DISPLAY_SI, S3C_GPIO_PULL_NONE);
	
	ret = 0;
	for (j = PACKET_LEN ;j >= 0; j--)
	{
		lcd_clk_value(0);

		udelay(2);

		lcd_clk_value(1);

		ret |=  get_lcd_si_value() << j;
		
		udelay(2);
	}
	ret = 0; 
	for (j = PACKET_LEN ;j >= 0; j--)
	{
		lcd_clk_value(0);

		udelay(2);

		lcd_clk_value(1);

		ret |=  get_lcd_si_value() << j;
		
		udelay(2);
	}
 
	s3c_gpio_cfgpin(GPIO_DISPLAY_SI, GPIO_DISPLAY_SI_AF);
	s3c_gpio_setpull(GPIO_DISPLAY_SI, S3C_GPIO_PULL_NONE);
	
	//printk("ldi_spi_read_byte-------------------------: %x\n",ret);

	lcd_cs_value(1);
	udelay(DELAY);

	mutex_unlock(&spi_rd);
	
	return ret;
}

