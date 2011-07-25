/*
 * linux/drivers/power/s3c6410_battery.h
 *
 * Battery measurement code for S3C6410 platform.
 *
 * Copyright (C) 2009 Samsung Electronics.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define DRIVER_NAME	"sec-battery"

/*
 * Battery Table
 */
#define BATT_CAL		2447	/* 3.60V */

#define BATT_MAXIMUM		406	/* 4.176V */
#define BATT_FULL		353	/* 4.10V  */
#define BATT_SAFE_RECHARGE	353	/* 4.10V */
#define BATT_ALMOST_FULL	188	/* 3.8641V */
#define BATT_HIGH		112	/* 3.7554V */
#define BATT_MED		66	/* 3.6907V */
#define BATT_LOW		43	/* 3.6566V */
#define BATT_CRITICAL		8	/* 3.6037V */
#define BATT_MINIMUM		(-28)	/* 3.554V */
#define BATT_OFF		(-128)	/* 3.4029V */

#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) // mr work 
#define ATT_TMO_COMMON
#endif

//compile test
//#define CONFIG_S5PC110_KEPLER_BOARD

#if defined (ATT_TMO_COMMON) 

//#define BATTERY_DEBUG

#ifdef BATTERY_DEBUG
#define bat_dbg(fmt, ...) printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#define bat_info(fmt, ...) printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#else
#define bat_dbg(fmt, ...)
#define bat_info(fmt, ...) printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif

#define BAT_POLLING_INTERVAL	10000
#define ADC_TOTAL_COUNT		10
#define ADC_DATA_ARR_SIZE	6

#define OFFSET_VIBRATOR_ON		(0x1 << 0)
#define OFFSET_CAMERA_ON		(0x1 << 1)
#define OFFSET_MP3_PLAY			(0x1 << 2)
#define OFFSET_VIDEO_PLAY		(0x1 << 3)
#define OFFSET_VOICE_CALL_2G		(0x1 << 4)
#define OFFSET_VOICE_CALL_3G		(0x1 << 5)
#define OFFSET_DATA_CALL		(0x1 << 6)
#define OFFSET_LCD_ON			(0x1 << 7)
#define OFFSET_TA_ATTACHED		(0x1 << 8)
#define OFFSET_CAM_FLASH		(0x1 << 9)
#define OFFSET_BOOTING			(0x1 << 10)
#define OFFSET_WIFI				(0x1 << 11)
#define OFFSET_GPS				(0x1 << 12)

#define DISCONNECT_BAT_FULL		0x1
#define DISCONNECT_TEMP_OVERHEAT	0x2
#define DISCONNECT_TEMP_FREEZE		0x4
#define DISCONNECT_OVER_TIME		0x8

#define ATTACH_USB	1
#define ATTACH_TA	2


#define FUEL_INT_1ST	0
#define FUEL_INT_2ND	1
#define FUEL_INT_3RD	2

#define TOTAL_CHARGING_TIME	(6*60*60)	/* 6 hours */
#define TOTAL_RECHARGING_TIME	  (90*60)	/* 1.5 hours */

#define COMPENSATE_VIBRATOR		19
#define COMPENSATE_CAMERA		25
#define COMPENSATE_MP3			17
#define COMPENSATE_VIDEO		28
#define COMPENSATE_VOICE_CALL_2G	13
#define COMPENSATE_VOICE_CALL_3G	14
#define COMPENSATE_DATA_CALL		25
#define COMPENSATE_LCD			0
#define COMPENSATE_TA			0
#define COMPENSATE_CAM_FALSH		0
#define COMPENSATE_BOOTING		52
#define COMPENSATE_WIFI			0
#define COMPENSATE_GPS			0


#define SOC_LB_FOR_POWER_OFF		27
//#define RECHARGE_COND_VOLTAGE	4130000

#define RECHARGE_COND_TIME		(30*1000)	/* 30 seconds */
#define TOTAL_EVENT_TIME  (40*60*1000)  /* 40 minites */


/*
 * ADC channel
 */
enum adc_channel_type{
	S3C_ADC_CHG_CURRENT = 2,
	S3C_ADC_EAR = 3,
	S3C_ADC_V_F = 4,
	S3C_ADC_TEMPERATURE = 6,
#if (defined CONFIG_S5PC110_DEMPSEY_BOARD)
	S3C_LCD_ADC_TEMP,
#endif
	S3C_ADC_VOLTAGE,
	ENDOFADC
};


enum {
       BATT_VOL = 0,
       BATT_VOL_ADC,
       BATT_VOL_ADC_CAL,
       BATT_TEMP,
       BATT_TEMP_ADC,
#if (defined CONFIG_S5PC110_DEMPSEY_BOARD)
        LCD_TEMP_ADC,
#endif
       BATT_TEMP_ADC_CAL,
	BATT_VOL_ADC_AVER,
	BATT_TEST_MODE,
	BATT_VOL_AVER,
	BATT_TEMP_AVER,
	BATT_TEMP_ADC_AVER,
	BATT_V_F_ADC,
	BATT_CHG_CURRENT,	
	BATT_CHARGING_SOURCE,
	BATT_VIBRATOR,
	BATT_CAMERA,
	BATT_MP3,
	BATT_VIDEO,
	BATT_VOICE_CALL_2G,
	BATT_VOICE_CALL_3G,
	BATT_DATA_CALL,
	BATT_WIFI,
	BATT_GPS,		
	BATT_DEV_STATE,
	BATT_COMPENSATION,
	BATT_BOOTING,
	BATT_FG_SOC,
	BATT_RESET_SOC,
	CHARGING_MODE_BOOTING,
	BATT_TEMP_CHECK,
	BATT_FULL_CHECK,
};
#endif


#if defined (CONFIG_S5PC110_KEPLER_BOARD)
#define EVENT_TEMP_HIGH_BLOCK	       640  //  temper_table[85][0]		// 65 ¢®E¡ËcE
#define EVENT_TEMP_HIGH_RECOVER	570  //  temper_table[75][0]		// 55 ¢®E¡ËcE

#define TEMP_HIGH_BLOCK	              500  // temper_table[67][0]		// 47 ¢®E¡ËcE
#define TEMP_HIGH_RECOVER	              430  // temper_table[60][0]		// 40 ¢®E¡ËcE
#define TEMP_LOW_BLOCK		              10  //  temper_table[19][0]		// -1 temp
#define TEMP_LOW_RECOVER	              60   //  temper_table[24][0]		// 4 temp

#define FULL_CHARGE_COND_VOLTAGE	 4100
#define RECHARGE_COND_VOLTAGE        4140
#define RECHARGE_COND_TIME		 (30*1000)	

#define CURRENT_OF_FULL_CHG		 279		//150mA
#define CURRENT_OF_TOPOFF_CHG		 100	


#define BATT_VF_MAX	800 // 0.55
#define BATT_VF_MIN	       400 // 0.45

#elif defined (CONFIG_S5PC110_HAWK_BOARD)

#define EVENT_TEMP_HIGH_BLOCK	       650  //  temper_table[85][0]		// 65 ¢®E¡ËcE
#define EVENT_TEMP_HIGH_RECOVER	470  //  temper_table[75][0]		// 55 ¢®E¡ËcE

#define TEMP_HIGH_BLOCK	              490  // temper_table[67][0]		// 46 ¢®E¡ËcE
#define TEMP_HIGH_RECOVER	              410  // temper_table[60][0]		// 40 ¢®E¡ËcE
#define TEMP_LOW_BLOCK		               -30  //  temper_table[19][0]		// 0 temp
#define TEMP_LOW_RECOVER	               0   //  temper_table[24][0]		// 3 temp

#define FULL_CHARGE_COND_VOLTAGE	 4100
#define RECHARGE_COND_VOLTAGE        4140
#define RECHARGE_COND_TIME		 (30*1000)

#define CURRENT_OF_FULL_CHG		 240		//140mA
#define CURRENT_OF_TOPOFF_CHG		 130		//78mA

#define BATT_VF_MAX	800 // 0.55
#define BATT_VF_MIN	       400 // 0.45

#elif defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD)

#define EVENT_TEMP_HIGH_BLOCK	        680		// 68 ¡Ë¢çE   
#define EVENT_TEMP_HIGH_RECOVER	 550		// 64 ¡Ë¢çE  // 55C

#define TEMP_HIGH_BLOCK		        500		// 50 ¡Ë¢çE
#define TEMP_HIGH_RECOVER	               430 		// 43 ¡Ë¢çE
#define TEMP_LOW_BLOCK		                0 		// 0 temp
#define TEMP_LOW_RECOVER	                30		// 3 temp

#define FULL_CHARGE_COND_VOLTAGE	 4100
#define RECHARGE_COND_VOLTAGE        4140
#define RECHARGE_COND_TIME		 (30*1000)

#define CURRENT_OF_FULL_CHG		 240		//140mA
#define CURRENT_OF_TOPOFF_CHG		 220	//90mA

#define BATT_VF_MAX	800 // 0.55
#define BATT_VF_MIN	       400 // 0.45

#elif defined (CONFIG_S5PC110_DEMPSEY_BOARD)				// mr work 

#define EVENT_TEMP_HIGH_BLOCK	       670  //  temper_table[77][0]		// 67 ¡Ë¢çE
#define EVENT_TEMP_HIGH_RECOVER			 580  //  temper_table[68][0]		// 58 ¡Ë¢çE

#define TEMP_HIGH_BLOCK	              490  // temper_table[59][0]		// 49 ¡Ë¢çE
#define TEMP_HIGH_RECOVER	              410  // temper_table[51][0]		// 41 ¡Ë¢çE
#define TEMP_LOW_BLOCK		               -20  // temper_table[8][0]		// -2  ¡Ë¢çE
#define TEMP_LOW_RECOVER	               40   // temper_table[14][0]		// 4 ¡Ë¢çE

#define FULL_CHARGE_COND_VOLTAGE	       4150							
#define RECHARGE_COND_VOLTAGE		4140
#define RECHARGE_COND_TIME		      (30*1000)	/* 30 seconds */


#define CURRENT_OF_FULL_CHG		325		//??mA
#define CURRENT_OF_TOPOFF_CHG		186


#define BATT_VF_MAX				800 // 0.55
#define BATT_VF_MIN	       400 // 0.45

#endif
