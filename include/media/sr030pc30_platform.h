/*
 * Driver for SR030pc30 (VGA camera) from SAMSUNG ELECTRONICS
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

struct SR030pc30_platform_data {
	unsigned int default_width;
	unsigned int default_height;
	unsigned int pixelformat;
	int freq;	/* MCLK in Hz */

	/* This SoC supports Parallel & CSI-2 */
	int is_mipi;
};

//#define CAMERA_GPIO_I2C //NAGSM_Android_HQ_Camera_SungkooLee_20100914 0.0 board used I2C by GPIO
#define ADD_5M_CAM //skLee_temp 