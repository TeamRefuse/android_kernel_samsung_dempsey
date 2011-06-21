/*
 * ea8868 AMOLED LCD panel driver.
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
#include "ea8868_gamma.h"
#include "ldi_common.h"


 const unsigned short EA8868_SEQ_SWRESET[] = {
	0x01, COMMAND_ONLY,
	ENDDEF, 0x00
};
 

 
 const unsigned short EA8868_SEQ_USER_SETTING[] = {
	0xF0, 0x5A,

	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};
 

 const unsigned short EA8868_SEQ_MTPENABLE[] = {
	0xF1, 0x5A,

	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};
 

 
 const unsigned short EA8868_SEQ_ACL_ON[] = {
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
 

 
 const unsigned short EA8868_SEQ_ELVSS_ON[] = {
	0xB1, 0x0F,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x16,
	ENDDEF, 0x00
};
 

 const unsigned short EA8868_SEQ_ELVSS_49[] = {
	 0xB1, 0x0F,
	 
	 DATA_ONLY, 0x00,
	 DATA_ONLY, 0x16,

	0xB2, 0x10,

	DATA_ONLY, 0x10,
	DATA_ONLY, 0x10,
	DATA_ONLY, 0x10,
	ENDDEF, 0x00
};


 
 const unsigned short EA8868_SEQ_ELVSS_42[] = {
	 0xB1, 0x0F,
	 
	 DATA_ONLY, 0x00,
	 DATA_ONLY, 0x16,

	0xB2, 0x17,
	DATA_ONLY, 0x17,
	DATA_ONLY, 0x17,
	DATA_ONLY, 0x17,
	ENDDEF, 0x00
};




 const unsigned short EA8868_SEQ_ELVSS_39[] = {
	 0xB1, 0x0F,
	 
	 DATA_ONLY, 0x00,
	 DATA_ONLY, 0x16,

	0xB2, 0x1A,
	DATA_ONLY, 0x1A,
	DATA_ONLY, 0x1A,
	DATA_ONLY, 0x1A,
	ENDDEF, 0x00
};




 const unsigned short EA8868_SEQ_ELVSS_35[] = {
	 0xB1, 0x0F,
	 
	 DATA_ONLY, 0x00,
	 DATA_ONLY, 0x16,

	0xB2, 0x1E,
	DATA_ONLY, 0x1E,
	DATA_ONLY, 0x1E,
	DATA_ONLY, 0x1E,
	ENDDEF, 0x00
};



 const unsigned short EA8868_SEQ_GTCON[] = {
	0xF7, 0x09,


	ENDDEF, 0x00
};
 



 const unsigned short EA8868_SEQ_PANEL_CONDITION[] = {
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
 




 const unsigned short EA8868_SEQ_GAMMA_SET1[] = {
	0xF9, 0x00,

	DATA_ONLY, 0xBF,
	DATA_ONLY, 0xAA,
	DATA_ONLY, 0x8D,
	DATA_ONLY, 0xC1,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0x55,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xE8,
	DATA_ONLY, 0xA7,
	DATA_ONLY, 0x8C,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0xAB,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0x55,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0xE6,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0x8C,
	DATA_ONLY, 0xBF,
	DATA_ONLY, 0xA9,
	DATA_ONLY, 0xA6,
	DATA_ONLY, 0x55,
	ENDDEF, 0x00
};


 const unsigned short EA8868_SEQ_GAMMA_CTRL_ENABLE[] = {
	0xFB, 0x00,

	DATA_ONLY, 0x5A,
	ENDDEF, 0x00
};




 const unsigned short EA8868_SEQ_GAMMA_START[] = {
	0xF9, COMMAND_ONLY,

	ENDDEF, 0x00
};
 
   const unsigned short EA8868_SEQ_GAMMA_CTRL_DISABLE[] = {
	  0xFB, 0x00,
  
	  DATA_ONLY, 0xA5,
	  ENDDEF, 0x00
  };
  

 

 const unsigned short EA8868_SEQ_APON[] = {
	0xF3, 0x00,

	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x00,
	ENDDEF, 0x00
};
 

 
 const unsigned short EA8868_SEQ_DISPCTL[] = {
	0xF2, 0x02,

	DATA_ONLY, 0x08,
	DATA_ONLY, 0x08,
	DATA_ONLY, 0x10,
	DATA_ONLY, 0x10,
	ENDDEF, 0x00
};




 const unsigned short EA8868_SEQ_MANPWR[] = {
	0xB0, 0x00,	//Default
	ENDDEF, 0x00
};


 const unsigned short EA8868_SEQ_PWR_CTRL[] = {
	0xF4, 0xA0,

	DATA_ONLY, 0x6A,
	DATA_ONLY, 0x00,
	DATA_ONLY, 0x55,
	DATA_ONLY, 0x02,
	ENDDEF, 0x00
};


 
 const unsigned short EA8868_SEQ_SLPOUT[] = {
	0x11, COMMAND_ONLY,
	ENDDEF, 0x00
};



 const unsigned short EA8868_SEQ_SLPIN[] = {
	0x10, COMMAND_ONLY,
	ENDDEF, 0x00
};


 
 const unsigned short EA8868_SEQ_DISPON[] = {
	0x29, COMMAND_ONLY,
	ENDDEF, 0x00
};



 const unsigned short EA8868_SEQ_INVON[] = {
	0x21, COMMAND_ONLY,
	ENDDEF, 0x00
};


 const unsigned short EA8868_SEQ_DISPOFF[] = {
	0x28, COMMAND_ONLY,
	ENDDEF, 0x00
};



int _ea8868_gamma_ctl(const unsigned int *gamma)
{
	unsigned int i = 0;
	int ret = 0;


	/* update gamma table. */
	ret = ldi_panel_send_sequence(EA8868_SEQ_GAMMA_CTRL_ENABLE);
	if (ret)
		printk( "failed to update gamma table.\n");

	/* start gamma table updating. */
	ret = ldi_panel_send_sequence(EA8868_SEQ_GAMMA_START);
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
	ret = ldi_panel_send_sequence(EA8868_SEQ_GAMMA_CTRL_DISABLE);
	if (ret)
		printk( "failed to update gamma table.\n");

gamma_err:
	return ret;
}

 int ea8868_gamma_ctl(int gamma)
{
	int ret = 0;

	ret = _ea8868_gamma_ctl(ea8868_gamma_table.gamma_22_table[gamma]);

	return ret;
}


 int ea8868_19gamma_ctl(int gamma)
{
	int ret = 0;

	ret = _ea8868_gamma_ctl(ea8868_gamma_table.gamma_19_table[gamma]);

	return ret;
}

 int ea8868_set_elvss(int brightness)
{
	int ret = 0;
	const unsigned short * elvss_mode;

	switch (brightness) {
	case 0: //30cd/m2		
	case 1: //40cd/m2		
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
	case 13: //190cd/m2 
	case 14: //200cd/m2
		elvss_mode = EA8868_SEQ_ELVSS_42;
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
		elvss_mode = EA8868_SEQ_ELVSS_49;			
		break;	
	default:
		break;	
	}
	//printk("level  = %d\n",brightness);	

	ret = ldi_panel_send_sequence(elvss_mode);
	if (ret) {
		printk( "failed to initialize ldi.\n");
		return -EIO;
	}
	return ret;
}

int ea8868_ldi_init()
{
	int ret, i;

	
	const unsigned short *init_seq_part2[] = {
		EA8868_SEQ_MTPENABLE,
		EA8868_SEQ_PWR_CTRL,
		//Settings to be taken care in gamma ctl		
		//EA8868_SEQ_GAMMA_SET1,
		//EA8868_SEQ_GAMMA_CTRL,
		//Settings to be taken care in gamma ctl
	};

	for (i = 0; i < ARRAY_SIZE(init_seq_part2); i++) {
		ret = ldi_panel_send_sequence(init_seq_part2[i]);
		mdelay(5);
		if (ret)
			break;
	}

	//Display is ready for Gamma Settings 1. First do gamma settings then 2. DISP ON

	printk("######ea8868_ldi_init2###### %d\n", i);	

	return ret;
}

int ea8868_ldi_enable()
{
	int ret = 0;

	ret = ldi_panel_send_sequence(EA8868_SEQ_DISPON);

	return ret;
}

int ea8868_ldi_disable()
{
	int ret;

	ret = ldi_panel_send_sequence(EA8868_SEQ_DISPOFF);
	ret = ldi_panel_send_sequence(EA8868_SEQ_SLPIN);

	return ret;
}

