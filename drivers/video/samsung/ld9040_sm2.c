/*
 * ld9040 AMOLED LCD panel driver.
 *
 * Author: Donghwa Lee  <dh09.lee@samsung.com>
 *
 * Derived from drivers/video/omap/lcd-apollon.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#include "ld9040_sm2_gamma.h"
#include "ldi_common.h"
const unsigned short LD9040_SM2_SEQ_SWRESET[] = {
	0x01, COMMAND_ONLY,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_USER_SETTING[] = {
	0xF0, 0x5A,

	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_ACL_ON[] = {
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
	DATA_ONLY, 0x0F,
	
	DATA_ONLY, 0x16,
	DATA_ONLY, 0x1D,
	DATA_ONLY, 0x24,
	DATA_ONLY, 0x2A,
	
	DATA_ONLY, 0x31,
	DATA_ONLY, 0x38,
	DATA_ONLY, 0x3F,
	DATA_ONLY, 0x46,

	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_ELVSS_ON[] = {
	0xB1, 0x0F,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x16,
	ENDDEF, 0x00
};

 
const unsigned short LD9040_SM2_SEQ_ELVSS_OFF[] = {
  0xB1, 0x0E,

  DATA_ONLY, 0x00,
  DATA_ONLY, 0x16,
  ENDDEF, 0x00
};

 
  static const unsigned short LD9040_SM2_SEQ_ELVSS_45[] = {
  	
	0xB1, 0x0F,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x16,

	 0xB2, 0x14,
	 DATA_ONLY, 0x14,
	 DATA_ONLY, 0x14,
	 DATA_ONLY, 0x14,
	 ENDDEF, 0x00
 };
 
 static const unsigned short LD9040_SM2_SEQ_ELVSS_38[] = {

	
	0xB1, 0x0F,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x16,

	 0xB2, 0x1B,
	 DATA_ONLY, 0x1B,
	 DATA_ONLY, 0x1B,
	 DATA_ONLY, 0x1B,
	 ENDDEF, 0x00
 };
 
 
 static const unsigned short LD9040_SM2_SEQ_ELVSS_35[] = {
 	
	0xB1, 0x0F,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x16,

	 0xB2, 0x1E,
	 DATA_ONLY, 0x1E,
	 DATA_ONLY, 0x1E,
	 DATA_ONLY, 0x1E,
	 ENDDEF, 0x00
 };
 
 static const unsigned short LD9040_SM2_SEQ_ELVSS_31[] = {
 	
	0xB1, 0x0F,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x16,

	 0xB2, 0x22,
	 DATA_ONLY, 0x22,
	 DATA_ONLY, 0x22,
	 DATA_ONLY, 0x22,
	 ENDDEF, 0x00
 };
 


 const unsigned short LD9040_SM2_SEQ_GTCON[] = {
	0xF7, 0x09,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_PANEL_CONDITION[] = {
	0xF8, 0x05,
#if 1
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
	DATA_ONLY, 0x05,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x1F,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
#else
	DATA_ONLY, 0x65,
	DATA_ONLY, 0x96,
	DATA_ONLY, 0x71,
	DATA_ONLY, 0x7D,
	DATA_ONLY, 0x19,
	DATA_ONLY, 0x3B,
	DATA_ONLY, 0x0D,
	DATA_ONLY, 0x19,
	DATA_ONLY, 0x7E,
	DATA_ONLY, 0x0D,
	DATA_ONLY, 0xE2,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x7E,
	DATA_ONLY, 0x7D,
	DATA_ONLY, 0x07,
	DATA_ONLY, 0x07,
	DATA_ONLY, 0x20,
	DATA_ONLY, 0x20,
	DATA_ONLY, 0x20,
	DATA_ONLY, 0x02,
	DATA_ONLY, 0x02,
#endif
	ENDDEF, 0x00
};


#if 0 
//Original Gamma at init set to 22_300cd

 const unsigned short LD9040_SEQ_GAMMA_SET1[] = {
	0xF9, 0x00,
#if 0
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0xC4,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0xC9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x7B,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB5,
	DATA_ONLY, 0xC0,
	DATA_ONLY, 0xA8,
	DATA_ONLY, 0xC7,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA3,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0xC6,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xA4,
#else
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xAE,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x91,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xB4,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xBB,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xAC,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB1,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0xBC,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB3,
#endif
	ENDDEF, 0x00
};

#else

//Original Gamma at init set to 22_30cd_dimming
 const unsigned short LD9040_SM2_SEQ_GAMMA_SET1[] = {
	0xF9, 0x00,

	DATA_ONLY, 0xA7,
	DATA_ONLY, 0x9C,
	DATA_ONLY, 0xBA,
	DATA_ONLY, 0xD3,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x20,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB2,
	DATA_ONLY, 0xC0,
	DATA_ONLY, 0xBD,
	DATA_ONLY, 0xD0,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x26,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xB3,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xB7,
	DATA_ONLY, 0xCF,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x31,

	ENDDEF, 0x00
};

#endif

 const unsigned short LD9040_SM2_SEQ_GAMMA_CTRL[] = {
	0xFB, 0x02,

	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_GAMMA_START[] = {
	0xF9, COMMAND_ONLY,

	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_APON[] = {
	0xF3, 0x00,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x0A,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_DISPCTL[] = {
	0xF2, 0x02,

#if 0
	DATA_ONLY, 0x06,
	DATA_ONLY, 0x0A,
#else
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x08,
#endif
	DATA_ONLY, 0x10,
	DATA_ONLY, 0x10,
	ENDDEF, 0x00
};


 const unsigned short LD9040_SM2_SEQ_MANPWR[] = {
	0xB0, 0x04,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_PWR_CTRL[] = {
	0xF4, 0x0A,

	DATA_ONLY, 0x87,
	DATA_ONLY, 0x25,
	DATA_ONLY, 0x6A,
	DATA_ONLY, 0x44,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_SLPOUT[] = {
	0x11, COMMAND_ONLY,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_SLPIN[] = {
	0x10, COMMAND_ONLY,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_DISPON[] = {
	0x29, COMMAND_ONLY,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_INVON[] = {
	0x21, COMMAND_ONLY,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_DISPOFF[] = {
	0x28, COMMAND_ONLY,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_VCI1_1ST_EN[] = {
	0xF3, 0x10,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_VL1_EN[] = {
	0xF3, 0x11,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_VL2_EN[] = {
	0xF3, 0x13,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_VCI1_2ND_EN[] = {
	0xF3, 0x33,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_VL3_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_VREG1_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0x01,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_VGH_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0x11,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_VGL_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0x31,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_VMOS_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xB1,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_VINT_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xF1,
	/* DATA_ONLY, 0x71,	VMOS/VBL/VBH not used */
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	/* DATA_ONLY, 0x02,	VMOS/VBL/VBH not used */
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_VBH_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xF9,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_VBL_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xFD,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_GAM_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xFF,
	/* DATA_ONLY, 0x73,	VMOS/VBL/VBH not used */
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	/* DATA_ONLY, 0x02,	VMOS/VBL/VBH not used */
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_SD_AMP_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xFF,
	/* DATA_ONLY, 0x73,	VMOS/VBL/VBH not used */
	DATA_ONLY, 0x80,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	/* DATA_ONLY, 0x02,	VMOS/VBL/VBH not used */
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_GLS_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xFF,
	/* DATA_ONLY, 0x73,	VMOS/VBL/VBH not used */
	DATA_ONLY, 0x81,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	/* DATA_ONLY, 0x02,	VMOS/VBL/VBH not used */
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_ELS_EN[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xFF,
	/* DATA_ONLY, 0x73,	VMOS/VBL/VBH not used */
	DATA_ONLY, 0x83,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	/* DATA_ONLY, 0x02,	VMOS/VBL/VBH not used */
	ENDDEF, 0x00
};

 const unsigned short LD9040_SM2_SEQ_EL_ON[] = {
	0xF3, 0x37,

	DATA_ONLY, 0xFF,
	/* DATA_ONLY, 0x73,	VMOS/VBL/VBH not used */
	DATA_ONLY, 0x87,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x03,
	/* DATA_ONLY, 0x02,	VMOS/VBL/VBH not used */
	ENDDEF, 0x00
};

int _ld9040_sm2_gamma_ctl(const unsigned int *gamma)
{
	unsigned int i = 0;
	int ret = 0;

	/* start gamma table updating. */
	ret = ldi_panel_send_sequence(LD9040_SM2_SEQ_GAMMA_START);
	if (ret) {
		printk( "failed to disable gamma table updating.\n");
		goto gamma_err;
	}

	for (i = 0 ; i < GAMMA_TABLE_COUNT; i++) {
		ret = ldi_spi_write(DATA_ONLY, gamma[i]);
		if (ret) {
			printk( "failed to set gamma table.\n");
			goto gamma_err;
		}
	}

	/* update gamma table. */
	ret = ldi_panel_send_sequence(LD9040_SM2_SEQ_GAMMA_CTRL);
	if (ret)
		printk( "failed to update gamma table.\n");

gamma_err:
	return ret;
}

 int ld9040_sm2_gamma_ctl(int gamma)
{
	int ret = 0;

	ret = _ld9040_sm2_gamma_ctl(ld9040_sm2_gamma_table.gamma_22_table[gamma]);

	return ret;
}

 int ld9040_sm2_19gamma_ctl(int gamma)
{
	int ret = 0;
	ret = _ld9040_sm2_gamma_ctl(ld9040_sm2_gamma_table.gamma_19_table[gamma]);
	return ret;
}


 int ld9040_sm2_set_elvss(int brightness)
{
	int ret = 0;
	const unsigned short * elvss_mode;
	switch (brightness) {
	case 0: //30cd/m2		
	case 1: //40cd/m2		
	case 2: //70cd/m2
	case 3: //90cd/m2	
	case 4: //100cd/m2
		elvss_mode = LD9040_SM2_SEQ_ELVSS_31;
		break;
	case 5: //110cd/m2		
	case 6: //120cd/m2		
	case 7: //130cd/m2
	case 8: //140cd/m2	
	case 9: //150cd/m2
	case 10: //160cd/m2 	
		elvss_mode = LD9040_SM2_SEQ_ELVSS_35;
		break;
	case 11: //170cd/m2
	case 12: //180cd/m2
	case 13: //190cd/m2 
	case 14: //200cd/m2
		elvss_mode = LD9040_SM2_SEQ_ELVSS_38;
		break;
	case 15: //210cd/m2
	case 16: //220cd/m2
	case 17: //230cd/m2 
	case 18: //240cd/m2
	case 19: //250cd/m2 	
	case 20: //260cd/m2
	case 21: //270cd/m2 
	case 22: //280cd/m2
	case 23: //290cd/m2 
	case 24: //300cd/m2
		elvss_mode = LD9040_SM2_SEQ_ELVSS_45;			
		break;	
	default:
		break;	
	}
	//printk("level for DLVSS = %d\n",brightness); 

	ret = ldi_panel_send_sequence(elvss_mode);
	if (ret) {
		printk( "failed to initialize ldi.\n");
		return -EIO;
	}
	return ret;
}


int ld9040_sm2_ldi_init()
{
	int ret, i;

	const unsigned short *init_seq_part2[] = {
		LD9040_SM2_SEQ_PWR_CTRL,
		//Settings to be taken care in gamma ctl		
		//LD9040_SEQ_GAMMA_SET1,
		//LD9040_SEQ_GAMMA_CTRL,
		//Settings to be taken care in gamma ctl
		//LD9040_SEQ_ACL_ON,
		//LD9040_SEQ_MANPWR,
	};

	for (i = 0; i < ARRAY_SIZE(init_seq_part2); i++) {
		ret = ldi_panel_send_sequence(init_seq_part2[i]);
		msleep(5);
		if (ret)
			break;
	}


	//Display is ready for Gamma Settings 1. First do gamma settings then 2. DISP ON

	printk("######ld9040_sm2_ldi_init###### %d\n", i);	

	return ret;
}

int ld9040_sm2_ldi_enable()
{
	int ret = 0;

	ret = ldi_panel_send_sequence(LD9040_SM2_SEQ_DISPON);
	ret = ldi_panel_send_sequence(LD9040_SM2_SEQ_SLPOUT);
	msleep(120);

	return ret;
}

int ld9040_sm2_ldi_disable()
{
	int ret;

//	ret = ldi_panel_send_sequence(LD9040_SM2_SEQ_DISPOFF);
	ret = ldi_panel_send_sequence(LD9040_SM2_SEQ_ELVSS_OFF); 
	msleep(20);
	ret = ldi_panel_send_sequence(LD9040_SM2_SEQ_SLPIN);

	return ret;
}


