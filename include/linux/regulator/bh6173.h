/*
 * bh6173.c  --  Voltage and current regulation for the ROHM BH6173GUL
 *
 * Copyright (C) 2010 Samsung Electrnoics
 * Aakash Manik <aakash.manik@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef REGULATOR_BH6173
#define REGULATOR_BH6173

#include <linux/regulator/machine.h>

enum {
	BH6173_LDO1 = 1,	//VT_CAMA_2.8V
	BH6173_LDO2,		//VGA_IO_1.8V
	BH6173_LDO3,		//LED_A_2.8V
};

struct bh6173_subdev_data {
	int		id;
	struct regulator_init_data	*initdata;
};

struct bh6173_platform_data {
	int num_regulators;
	struct bh6173_subdev_data *regulators;
};

int REG_power_onoff(int ONOFF_REG);
int bh6173_ldo_is_enabled_status(int ldo);					//NAGSM_Android_SEL_Kernel_Aakash_20101021

int bh6173_ldo_enable_direct(int ldo);
int bh6173_ldo_disable_direct(int ldo);

int bh6173_ldo_get_voltage_direct(int ldo);					//NAGSM_Android_SEL_Kernel_Aakash_20101019
int bh6173_ldo_set_voltage_direct(int ldo,int min_uV, int max_uV); 	//NAGSM_Android_SEL_Kernel_Aakash_20101019

#define BH6173GUL_ADDR		0x4A	// ADRS = L

#define BH6173GUL_REGCNT_NONE			0x00
#define BH6173GUL_REGCNT_REG			0x01
#define BH6173GUL_REGCNT_LDO1			0x02
#define BH6173GUL_REGCNT_LDO2			0x04
#define BH6173GUL_REGCNT_LDO3			0x08
#define BH6173GUL_REGCNT_ALL			0x0F

#define BH6173GUL_SWADJ_080			0x00	// 0.80V
#define BH6173GUL_SWADJ_085			0x01	// 0.85V
#define BH6173GUL_SWADJ_090			0x02	// 0.90V
#define BH6173GUL_SWADJ_095			0x03	// 0.95V
#define BH6173GUL_SWADJ_100			0x04	// 1.00V
#define BH6173GUL_SWADJ_105			0x05	// 1.05V
#define BH6173GUL_SWADJ_110			0x06	// 1.10V
#define BH6173GUL_SWADJ_115			0x07	// 1.15V
#define BH6173GUL_SWADJ_120			0x08	// 1.20V
#define BH6173GUL_SWADJ_130			0x09	// 1.30V
#define BH6173GUL_SWADJ_1365			0x0A	// 1.365V
#define BH6173GUL_SWADJ_140			0x0B	// 1.40V
#define BH6173GUL_SWADJ_150			0x0C	// 1.50V
#define BH6173GUL_SWADJ_180			0x0D	// 1.80V
#define BH6173GUL_SWADJ_185			0x0E	// 1.85V
#define BH6173GUL_SWADJ_240			0x0F	// 2.40V

#define BH6173GUL_LDO1_100			0x00	// 1.00V
#define BH6173GUL_LDO1_110			0x01	// 1.10V
#define BH6173GUL_LDO1_120			0x02	// 1.20V
#define BH6173GUL_LDO1_130			0x03	// 1.30V
#define BH6173GUL_LDO1_140			0x04	// 1.40V
#define BH6173GUL_LDO1_150			0x05	// 1.50V
#define BH6173GUL_LDO1_160			0x06	// 1.60V
#define BH6173GUL_LDO1_170			0x07	// 1.70V
#define BH6173GUL_LDO1_180			0x08	// 1.80V
#define BH6173GUL_LDO1_185			0x09	// 1.85V
#define BH6173GUL_LDO1_260			0x0A	// 2.60V
#define BH6173GUL_LDO1_270			0x0B	// 2.70V
#define BH6173GUL_LDO1_280			0x0C	// 2.80V
#define BH6173GUL_LDO1_285			0x0D	// 2.85V
#define BH6173GUL_LDO1_300			0x0E	// 3.00V
#define BH6173GUL_LDO1_330			0x0F	// 3.30V

#define BH6173GUL_LDO2_100			0x00	// 1.00V
#define BH6173GUL_LDO2_110			0x10	// 1.10V
#define BH6173GUL_LDO2_120			0x20	// 1.20V
#define BH6173GUL_LDO2_130			0x30	// 1.30V
#define BH6173GUL_LDO2_140			0x40	// 1.40V
#define BH6173GUL_LDO2_150			0x50	// 1.50V
#define BH6173GUL_LDO2_160			0x60	// 1.60V
#define BH6173GUL_LDO2_170			0x70	// 1.70V
#define BH6173GUL_LDO2_180			0x80	// 1.80V
#define BH6173GUL_LDO2_185			0x90	// 1.85V
#define BH6173GUL_LDO2_260			0xA0	// 2.60V
#define BH6173GUL_LDO2_270			0xB0	// 2.70V
#define BH6173GUL_LDO2_280			0xC0	// 2.80V
#define BH6173GUL_LDO2_285			0xD0	// 2.85V
#define BH6173GUL_LDO2_300			0xE0	// 3.00V
#define BH6173GUL_LDO2_330			0xF0	// 3.30V

#define BH6173GUL_LDO3_120			0x00	// 1.20V
#define BH6173GUL_LDO3_130			0x01	// 1.30V
#define BH6173GUL_LDO3_140			0x02	// 1.40V
#define BH6173GUL_LDO3_150			0x03	// 1.50V
#define BH6173GUL_LDO3_160			0x04	// 1.60V
#define BH6173GUL_LDO3_170			0x05	// 1.70V
#define BH6173GUL_LDO3_180			0x06	// 1.80V
#define BH6173GUL_LDO3_185			0x07	// 1.85V
#define BH6173GUL_LDO3_190			0x08	// 1.90V
#define BH6173GUL_LDO3_200			0x09	// 2.00V
#define BH6173GUL_LDO3_260			0x0A	// 2.60V
#define BH6173GUL_LDO3_270			0x0B	// 2.70V
#define BH6173GUL_LDO3_280			0x0C	// 2.80V
#define BH6173GUL_LDO3_285			0x0D	// 2.85V
#define BH6173GUL_LDO3_300			0x0E	// 3.00V
#define BH6173GUL_LDO3_330			0x0F	// 3.30V

#define BH6173GUL_PULLDOWN_REG			0x01
#define BH6173GUL_PULLDOWN_LDO1			0x02
#define BH6173GUL_PULLDOWN_LDO2			0x04
#define BH6173GUL_PULLDOWN_LDO3			0x08
#define BH6173GUL_PULLDOWN_ALL			0x0F

#define BH6173GUL_PULLDOWN_1K			0x00
#define BH6173GUL_PULLDOWN_10K			0x01

#define BH6173GUL_POWERDOWN_REG			0x01
#define BH6173GUL_POWERDOWN_LDO1		0x02
#define BH6173GUL_POWERDOWN_LDO2		0x04
#define BH6173GUL_POWERDOWN_LDO3		0x08
#define BH6173GUL_POWERDOWN_ALL			0x0F

#define BH6173GUL_POWERDOWN_HiZ			0x00
#define BH6173GUL_POWERDOWN_Discharge	0x01

#define BH6173GUL_REG_SW_PWMPFM			0x00
#define BH6173GUL_REG_SW_PWMONLY		0x01

#endif //REGULATOR_BH6173
