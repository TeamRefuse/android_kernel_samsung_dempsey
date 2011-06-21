#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>

extern struct device *gps_dev;

static void __init gps_gpio_init(void)
{
#ifndef CONFIG_GPS_CHIPSET_STE_CG2900
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
	gpio_request(GPIO_GPS_nRST, "GPS_nRST");	/* XMMC1CLK */
	s3c_gpio_setpull(GPIO_GPS_nRST, S3C_GPIO_PULL_UP);
#else
	gpio_request(GPIO_GPS_nRST, "GPS_nRST");	/* XMMC3CLK */
	s3c_gpio_setpull(GPIO_GPS_nRST, S3C_GPIO_PULL_NONE);

#endif
	s3c_gpio_cfgpin(GPIO_GPS_nRST, S3C_GPIO_OUTPUT);
	gpio_direction_output(GPIO_GPS_nRST, 1);
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
	gpio_request(GPIO_GPS_PWR_EN, "GPS_PWR_EN");	/* XMMC1CMD */
	s3c_gpio_setpull(GPIO_GPS_PWR_EN, S3C_GPIO_PULL_UP);
#else
	gpio_request(GPIO_GPS_PWR_EN, "GPS_PWR_EN");	/* XMMC3CLK */
	s3c_gpio_setpull(GPIO_GPS_PWR_EN, S3C_GPIO_PULL_NONE);

#endif
	s3c_gpio_cfgpin(GPIO_GPS_PWR_EN, S3C_GPIO_OUTPUT);
	gpio_direction_output(GPIO_GPS_PWR_EN, 0);
	
	s3c_gpio_setpull(GPIO_GPS_RXD, S3C_GPIO_PULL_UP);
	gpio_export(GPIO_GPS_nRST, 1);
	gpio_export(GPIO_GPS_PWR_EN, 1);

	BUG_ON(!gps_dev);
	gpio_export_link(gps_dev, "GPS_nRST", GPIO_GPS_nRST);
	gpio_export_link(gps_dev, "GPS_PWR_EN", GPIO_GPS_PWR_EN);
#endif
}

arch_initcall(gps_gpio_init);
