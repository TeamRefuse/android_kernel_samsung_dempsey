/* linux/drivers/media/video/samsung/tv20/s5pv210/hdcp_s5pv210.c
 *
 * hdcp raw ftn  file for Samsung TVOut driver
 *
 * Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/irq.h>


#include <asm/io.h>
#include <mach/gpio.h>

#include "../ddc.h"
#include "tv_out_s5pv210.h"
#include "regs/regs-hdmi.h"

#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>

/* for Operation check */
//#define COFIG_TVOUT_RAW_DBG
#ifdef COFIG_TVOUT_RAW_DBG
#define S5P_HDCP_DEBUG 1
#define S5P_HDCP_I2C_DEBUG 1
#define S5P_HDCP_AUTH_DEBUG 1
#endif

#ifdef S5P_HDCP_DEBUG
#define HDCPPRINTK(fmt, args...) \
	printk("\t\t[HDCP] %s: " fmt, __FUNCTION__ , ## args)
#else
#define HDCPPRINTK(fmt, args...)
#endif

/* for authentication key check */
#ifdef S5P_HDCP_AUTH_DEBUG
#define AUTHPRINTK(fmt, args...) \
	printk("\t\t\t[AUTHKEY] %s: " fmt, __FUNCTION__ , ## args)
#else
#define AUTHPRINTK(fmt, args...)
#endif

#if defined(CONFIG_S5PC110_DEMPSEY_BOARD) 
#ifdef CONFIG_HDMI_HPD
//Rajucm: This is to exit work in no hdmi cable conncted
#define EXIT_WORK_ON_CABLE_DISCONNECTION \
do{\
	if(!s5p_hpd_get_state())	{\
		HDCPPRINTK("HDMI Cable Removed##\n\r");\
		return false;\
	}\
}while(0)
#endif
#else
#define EXIT_WORK_ON_CABLE_DISCONNECTION  do{}while(0)
#endif
extern  int s5p_hpd_get_state(void);
extern void __s5p_hdmi_video_set_bluescreen(bool en,
				     u8 cb_b,
				     u8 y_g,
				     u8 cr_r);
extern int s5p_hdmi_register_isr(hdmi_isr isr, u8 irq_num);
extern void s5p_hdmi_enable_interrupts(s5p_tv_hdmi_interrrupt intr);
extern void s5p_hdmi_disable_interrupts(s5p_tv_hdmi_interrrupt intr);
extern void s5p_hdmi_hpd_gen(void);

extern void __iomem		*hdmi_base;

enum hdmi_run_mode {
	DVI_MODE,
	HDMI_MODE
};

enum hdmi_resolution {
	SD480P,
	SD480I,
	WWSD480P,
	HD720P,
	SD576P,
	WWSD576P,
	HD1080I
};

enum hdmi_color_bar_type {
	HORIZONTAL,
	VERTICAL
};

enum hdcp_event {
	/* Stop HDCP */
	HDCP_EVENT_STOP,
	/* Start HDCP*/
	HDCP_EVENT_START,
	/* Start to read Bksv,Bcaps */
	HDCP_EVENT_READ_BKSV_START,
	/* Start to write Aksv,An */
	HDCP_EVENT_WRITE_AKSV_START,
	/* Start to check if Ri is equal to Rj */
	HDCP_EVENT_CHECK_RI_START,
	/* Start 2nd authentication process */
	HDCP_EVENT_SECOND_AUTH_START
};

enum hdcp_state {
	NOT_AUTHENTICATED, 
	RECEIVER_READ_READY, 
	BCAPS_READ_DONE, 
	BKSV_READ_DONE, 
	AN_WRITE_DONE, 
	AKSV_WRITE_DONE, 
	FIRST_AUTHENTICATION_DONE, 
	SECOND_AUTHENTICATION_RDY, 
	RECEIVER_FIFOLSIT_READY, 
	SECOND_AUTHENTICATION_DONE,
};

/*
 * Below CSC_TYPE is temporary. CSC_TYPE enum. 
 * may be included in SetSD480pVars_60Hz etc.
 *
 * LR : Limited Range (16~235)
 * FR : Full Range (0~255)
 */
enum hdmi_intr_src {
	WAIT_FOR_ACTIVE_RX,
	WDT_FOR_REPEATER,
	EXCHANGE_KSV,
	UPDATE_P_VAL,
	UPDATE_R_VAL,
	AUDIO_OVERFLOW,
	AUTHEN_ACK,
	UNKNOWN_INT
};

struct s5p_hdcp_info {
	bool	is_repeater;
	bool	hpd_status;
	u32	time_out;
	u32	hdcp_enable;

	spinlock_t 	lock;
	spinlock_t 	reset_lock;
	
	struct i2c_client 	*client;

	wait_queue_head_t 	waitq;
	enum hdcp_event 	event;
	enum hdcp_state 	auth_status;

	struct work_struct  	work;
#if defined(CONFIG_S5PC110_DEMPSEY_BOARD) //Rajucm: P110225-1327_system_lockup
        struct workqueue_struct *hdcp_workqueue;
#endif
};

static struct s5p_hdcp_info hdcp_info = {
	.is_repeater 	= false,
	.time_out	= 0,
	.hdcp_enable	= false,
	.client		= NULL,
	.event 		= HDCP_EVENT_STOP,
	.auth_status	= NOT_AUTHENTICATED,
	
};

#define HDCP_RI_OFFSET          0x08
#define INFINITE		0xffffffff

#define HDMI_SYS_ENABLE		(1 << 0)
#define HDMI_ASP_ENABLE		(1 << 2)
#define HDMI_ASP_DISABLE	(~HDMI_ASP_ENABLE)

#define MAX_DEVS_EXCEEDED          (0x1 << 7)
#define MAX_CASCADE_EXCEEDED       (0x1 << 3)

#define MAX_CASCADE_EXCEEDED_ERROR 	(-1)
#define MAX_DEVS_EXCEEDED_ERROR    	(-2)
#define REPEATER_ILLEGAL_DEVICE_ERROR	(-3)
#define REPEATER_TIMEOUT_ERROR		(-4)

#define AINFO_SIZE		1
#define BCAPS_SIZE		1
#define BSTATUS_SIZE		2
#define SHA_1_HASH_SIZE		20

#define KSV_FIFO_READY	(0x1 << 5)

/* spmoon for test : it's not in manual */
#define SET_HDCP_KSV_WRITE_DONE		(0x1 << 3)
#define CLEAR_HDCP_KSV_WRITE_DONE	(~SET_HDCP_KSV_WRITE_DONE)

#define SET_HDCP_KSV_LIST_EMPTY 	(0x1 << 2)
#define CLEAR_HDCP_KSV_LIST_EMPTY	(~SET_HDCP_KSV_LIST_EMPTY)
#define SET_HDCP_KSV_END		(0x1 << 1)
#define CLEAR_HDCP_KSV_END		(~SET_HDCP_KSV_END)
#define SET_HDCP_KSV_READ		(0x1 << 0)
#define CLEAR_HDCP_KSV_READ		(~SET_HDCP_KSV_READ)

#define SET_HDCP_SHA_VALID_READY	(0x1 << 1)
#define CLEAR_HDCP_SHA_VALID_READY	(~SET_HDCP_SHA_VALID_READY)
#define SET_HDCP_SHA_VALID	(0x1 << 0)
#define CLEAR_HDCP_SHA_VALID	(~SET_HDCP_SHA_VALID)

#define TRANSMIT_EVERY_VSYNC	(0x1 << 1)
extern u8 hdcp_protocol_status; // 0 - hdcp stopped, 1 - hdcp started, 2 - hdcp reset

/* must be checked */
static bool sw_reset;
static bool is_dvi;
static bool av_mute;
static bool audio_en;
static int test_counter =0;
void s5p_hdmi_set_audio(bool en)
{
	if (en)
		audio_en = true;
	else
		audio_en = false;
}

int s5p_hdcp_is_reset(void)
{
	int ret = 0;

	if (spin_is_locked(&hdcp_info.reset_lock))
		return 1;

	return ret;
}

int s5p_hdmi_set_dvi(bool en)
{
	if (en)
		is_dvi = true;
	else
		is_dvi = false;

	return 0;
}

int s5p_hdmi_set_mute(bool en)
{
	if (en)
		av_mute = true;
	else
		av_mute = false;

	return 0;
}

int s5p_hdmi_get_mute(void)
{
	return av_mute ? true : false;
}

int s5p_hdmi_audio_enable(bool en)
{
	u8 reg;

	if (!is_dvi) {
		reg = readl(hdmi_base + S5P_HDMI_CON_0);

		if (en) {
			reg |= ASP_EN;
			writel(HDMI_TRANS_EVERY_SYNC , hdmi_base + S5P_AUI_CON);
		} else {
			reg &= ~ASP_EN;
			writel(HDMI_DO_NOT_TANS , hdmi_base + S5P_AUI_CON);
		}

		writel(reg, hdmi_base + S5P_HDMI_CON_0);
	}

	return 0;
}

void s5p_hdmi_mute_en(bool en)
{
	if (!av_mute) {
		if (en) {
			__s5p_hdmi_video_set_bluescreen(true, 0, 0, 0);
			s5p_hdmi_audio_enable(false);
		} else {
			__s5p_hdmi_video_set_bluescreen(false, 0, 0, 0);
			if (audio_en)
				s5p_hdmi_audio_enable(true);
		}
	}
}

/*
 * 1st Authentication step func.
 * Write the Ainfo data to Rx
 */
static bool write_ainfo(void)
{
	int ret = 0;
	u8 ainfo[2];

	ainfo[0] = HDCP_Ainfo;
	ainfo[1] = 0;

	ret = ddc_write( ainfo, 2);
	if(ret < 0)
		HDCPPRINTK("Can't write ainfo data through i2c bus\n");

	return (ret<0) ? false : true;
}

/*
 * Write the An data to Rx
 */
static bool write_an(void)
{
	int ret = 0;
	u8 an[AN_SIZE+1];
#if defined(CONFIG_S5PC110_DEMPSEY_BOARD) 
	mdelay(100);//Rajucm
#endif
	an[0] = HDCP_An;

	// Read An from HDMI
	an[1] = readb(hdmi_base + S5P_HDCP_An_0_0);
	an[2] = readb(hdmi_base + S5P_HDCP_An_0_1);
	an[3] = readb(hdmi_base + S5P_HDCP_An_0_2);
	an[4] = readb(hdmi_base + S5P_HDCP_An_0_3);
	an[5] = readb(hdmi_base + S5P_HDCP_An_1_0);
	an[6] = readb(hdmi_base + S5P_HDCP_An_1_1);
	an[7] = readb(hdmi_base + S5P_HDCP_An_1_2);
	an[8] = readb(hdmi_base + S5P_HDCP_An_1_3);

	ret = ddc_write( an, AN_SIZE + 1);
	if(ret < 0)
		HDCPPRINTK("Can't write an data through i2c bus\n");
	
#ifdef S5P_HDCP_AUTH_DEBUG
	{
		u16 i = 0;
		for (i = 1; i < AN_SIZE + 1; i++) {
			AUTHPRINTK("HDCPAn[%d]: 0x%x \n", i, an[i]);
		}
	}
#endif		

	return (ret<0) ? false : true;
}

/*
 * Write the Aksv data to Rx
 */
static bool write_aksv(void)
{
	int ret = 0;	
	u8 aksv[AKSV_SIZE+1];

	aksv[0] = HDCP_Aksv;

	// Read Aksv from HDMI
	aksv[1] = readb(hdmi_base + S5P_HDCP_AKSV_0_0);
	aksv[2] = readb(hdmi_base + S5P_HDCP_AKSV_0_1);
	aksv[3] = readb(hdmi_base + S5P_HDCP_AKSV_0_2);
	aksv[4] = readb(hdmi_base + S5P_HDCP_AKSV_0_3);
	aksv[5] = readb(hdmi_base + S5P_HDCP_AKSV_1);

	if (aksv[1] == 0 &&
		aksv[2] == 0 &&
		aksv[3] == 0 &&
		aksv[4] == 0 &&
		aksv[5] == 0)
		return false;

	ret = ddc_write(  aksv, AKSV_SIZE + 1);
	if(ret < 0)
		HDCPPRINTK("Can't write aksv data through i2c bus\n");

#ifdef S5P_HDCP_AUTH_DEBUG
	{
		u16 i = 0;
		for (i = 1; i < AKSV_SIZE + 1; i++) {
			AUTHPRINTK("HDCPAksv[%d]: 0x%x\n", i, aksv[i]);
		}
	}
#endif

	return (ret<0) ? false : true;
}

static bool read_bcaps(void)
{
	int ret = 0;
	u8 bcaps[BCAPS_SIZE] = {0};

	ret = ddc_read( HDCP_Bcaps, bcaps, BCAPS_SIZE);

	if(ret < 0){
		HDCPPRINTK("Can't read bcaps data from i2c bus\n");
		return false;
	}

	writel(bcaps[0], hdmi_base + S5P_HDCP_BCAPS);

	HDCPPRINTK("BCAPS(from i2c) : 0x%08x\n", bcaps[0]);

	if (bcaps[0] & REPEATER_SET)
		hdcp_info.is_repeater = true;
	else
		hdcp_info.is_repeater = false;

	HDCPPRINTK("attached device type :  %s !! \n\r",
		   hdcp_info.is_repeater ? "REPEATER" : "SINK");
	HDCPPRINTK("BCAPS(from sfr) = 0x%08x\n", 
		readl(hdmi_base + S5P_HDCP_BCAPS));

	return true;
}

static bool read_again_bksv(void)
{
	u8 bk_sv[BKSV_SIZE] = {0,0,0,0,0};
	u8 i = 0;
	u8 j = 0;
	u32 no_one = 0;
	u32 no_zero = 0;
	u32 result = 0;
	int ret = 0;
	
	ret = ddc_read( HDCP_Bksv, bk_sv, BKSV_SIZE);

	if(ret < 0){
		HDCPPRINTK("Can't read bk_sv data from i2c bus\n");
		return false;
	}

#ifdef S5P_HDCP_AUTH_DEBUG
	for (i=0; i<BKSV_SIZE; i++)
		AUTHPRINTK("i2c read : Bksv[%d]: 0x%x\n", i, bk_sv[i]);
#endif

	for (i = 0; i < BKSV_SIZE; i++) {

		for (j = 0; j < 8; j++) {

			result = bk_sv[i] & (0x1 << j);

			if (result == 0)
				no_zero += 1;
			else
				no_one += 1;
		}
	}

	if ((no_zero == 20) && (no_one == 20)) {
		HDCPPRINTK("Suucess: no_zero, and no_one is 20\n");

		writel(bk_sv[0], hdmi_base + S5P_HDCP_BKSV_0_0);
		writel(bk_sv[1], hdmi_base + S5P_HDCP_BKSV_0_1);
		writel(bk_sv[2], hdmi_base + S5P_HDCP_BKSV_0_2);
		writel(bk_sv[3], hdmi_base + S5P_HDCP_BKSV_0_3);
		writel(bk_sv[4], hdmi_base + S5P_HDCP_BKSV_1);	

#ifdef S5P_HDCP_AUTH_DEBUG
		for (i=0; i<BKSV_SIZE; i++)
			AUTHPRINTK("set reg : Bksv[%d]: 0x%x\n", i, bk_sv[i]);

		//writel(HDCP_ENC_ENABLE, hdmi_base + S5P_ENC_EN);
#endif		
		return true;
	} else {
		HDCPPRINTK("Failed: no_zero or no_one is NOT 20\n");
		return false;
	}
}

static bool read_bksv(void)
{
	u8 bk_sv[BKSV_SIZE] = {0,0,0,0,0};
	
	int i = 0;
	int j = 0;

	u32 no_one = 0;
	u32 no_zero = 0;
	u32 result = 0;
	u32 count = 0;
	int ret = 0;

	ret = ddc_read( HDCP_Bksv, bk_sv, BKSV_SIZE);

	if(ret < 0){
		HDCPPRINTK("Can't read bk_sv data from i2c bus\n");
		return false;
	}

#ifdef S5P_HDCP_AUTH_DEBUG
	for (i=0; i<BKSV_SIZE; i++)
		AUTHPRINTK("i2c read : Bksv[%d]: 0x%x\n", i, bk_sv[i]);
#endif

	for (i = 0; i < BKSV_SIZE; i++) {

		for (j = 0; j < 8; j++) {

			result = bk_sv[i] & (0x1 << j);

			if (result == 0)
				no_zero ++;
			else
				no_one ++;
		}
	}

	if ((no_zero == 20) && (no_one == 20)) {

		writel(bk_sv[0], hdmi_base + S5P_HDCP_BKSV_0_0);
		writel(bk_sv[1], hdmi_base + S5P_HDCP_BKSV_0_1);
		writel(bk_sv[2], hdmi_base + S5P_HDCP_BKSV_0_2);
		writel(bk_sv[3], hdmi_base + S5P_HDCP_BKSV_0_3);
		writel(bk_sv[4], hdmi_base + S5P_HDCP_BKSV_1);	
		
#ifdef S5P_HDCP_AUTH_DEBUG
		for (i=0; i<BKSV_SIZE; i++)
			AUTHPRINTK("set reg : Bksv[%d]: 0x%x\n", i, bk_sv[i]);
#endif
		
		printk("Success: no_zero, and no_one is 20\n");
	
	} else {
		
		printk("Failed: no_zero or no_one is NOT 20\n");

		//writel(HDCP_ENC_DISABLE, hdmi_base + S5P_ENC_EN);
		
		while(! read_again_bksv()) {
			
			count ++;
			EXIT_WORK_ON_CABLE_DISCONNECTION;
			mdelay(200);

			if (count == 14)
				return false;
		}		
	}

	return true;
}

/*
 * Compare the R value of Tx with that of Rx
 */

static bool compare_r_val(void)
{
	int ret = 0;
	u8 ri[2] = {0,0};
	u8 rj[2] = {0,0};
	u16 i;
#if 0//Rajucms: error simulation for 1A_06_FAIL case
        test_counter++;
#endif
	for (i = 0; i < R_VAL_RETRY_CNT; i++) {
                EXIT_WORK_ON_CABLE_DISCONNECTION;
		if (hdcp_info.auth_status < AKSV_WRITE_DONE) {
			ret = false;
			break;
		}

		/* Read R value from Tx */
		ri[0] = readl(hdmi_base + S5P_HDCP_Ri_0);
		ri[1] = readl(hdmi_base + S5P_HDCP_Ri_1);

		/* Read R value from Rx */
		ret = ddc_read( HDCP_Ri, rj, 2);
		if(ret < 0){
			printk("Can't read r data from i2c bus\n");
			return false;
		}

#ifdef S5P_HDCP_AUTH_DEBUG
		AUTHPRINTK("retries :: %d\n", i);
		printk("\t\t\t Rx(ddc) ->");
		printk("rj[0]: 0x%02x, rj[1]: 0x%02x\n", rj[0], rj[1]);
		printk("\t\t\t Tx(register) ->");
		printk("ri[0]: 0x%02x, ri[1]: 0x%02x\n", ri[0], ri[1]);
#endif
#if 0//Rajucms: error simulation for 1A_06_FAIL case
if (test_counter > 2)
{
test_counter = 0;
writel(Ri_MATCH_RESULT__NO, hdmi_base + S5P_HDCP_CHECK_RESULT);
printk("Test code R0, R0' is not matched!!\n");
ret = false;
break;
}
#endif
		/* Compare R value */
		if ((ri[0] == rj[0]) && (ri[1] == rj[1]) && (ri[0] | ri[1])) {
			writel(Ri_MATCH_RESULT__YES, 
				hdmi_base + S5P_HDCP_CHECK_RESULT);
			printk("R0, R0' is matched!!\n");
			ret = true;
			break;
		} else {
			writel(Ri_MATCH_RESULT__NO, 
				hdmi_base + S5P_HDCP_CHECK_RESULT);
			printk("R0, R0' is not matched!!\n");
			ret = false;
		}
		ri[0] = 0;
		ri[1] = 0;
		rj[0] = 0;
		rj[1] = 0; //Rajucm
	}
if (!ret) {
		hdcp_info.event 	= HDCP_EVENT_STOP;
		hdcp_info.auth_status 	= NOT_AUTHENTICATED;
	}

	return ret ? true:false;
}


/*
 * Enable/Disable Software HPD control
 */
void sw_hpd_enable(bool enable)
{
	u8 reg;

	reg = readb(hdmi_base + S5P_HPD);
	reg &= ~HPD_SW_ENABLE;
	
	if (enable)
		writeb(reg |HPD_SW_ENABLE, hdmi_base + S5P_HPD);
	else
		writeb(reg, hdmi_base + S5P_HPD);
}

/*
 * Set Software HPD level
 *
 * @param   level   [in]    if 0 - low;othewise, high
 */
void set_sw_hpd(bool level)
{
	u8 reg;

	reg = readb(hdmi_base + S5P_HPD);
	reg &= ~HPD_ON;
	
	if (level)
		writeb(reg |HPD_ON, hdmi_base + S5P_HPD);
	else
		writeb(reg, hdmi_base + S5P_HPD);
}


/*
 * Reset Authentication
 */
#if defined(CONFIG_S5PC110_DEMPSEY_BOARD)
void hdcp_reset_timer_func(u32 cond)
{
	static DEFINE_TIMER(hdcp_restart_Timer, hdcp_reset_timer_func, 0, 0);//Rajucm
	if (cond)
	{
		HDCPPRINTK("[TIMER:]## Timer Sart\n");
		mod_timer(&hdcp_restart_Timer, jiffies + msecs_to_jiffies(500));//rajucm: 500miliseconds
		return;
	}
	HDCPPRINTK("[TIMER:]##hdcp_info.auth_status = %d   cable sate = %d\n", hdcp_info.auth_status, s5p_hpd_get_state());
        hdcp_info.auth_status = NOT_AUTHENTICATED; //to avoid frequent GScreen problem
	if (/*(hdcp_info.auth_status 	== NOT_AUTHENTICATED)&&*/ s5p_hpd_get_state())
	{
		printk("[TIMER:] ## Ready to restart HDCP after 1 sec ### \n");
		//s5p_hdmi_mute_en(true);
		sw_hpd_enable(true);//Rajucm:to restart hdcp
	}
}
#endif
void reset_authentication(void)
{
	u8 reg;

	spin_lock_irq(&hdcp_info.reset_lock);
	hdcp_info.time_out 	= INFINITE;
	hdcp_info.event 	= HDCP_EVENT_STOP;
	hdcp_info.auth_status 	= NOT_AUTHENTICATED;

	HDCPPRINTK("Now reset authentication\n");
        s5p_hdmi_mute_en(true);
	/* set hdcp_int disable */
	reg = readb(hdmi_base + S5P_STATUS_EN);
	reg &= ~(WTFORACTIVERX_INT_OCCURRED | WATCHDOG_INT_OCCURRED |
		EXCHANGEKSV_INT_OCCURRED | UPDATE_RI_INT_OCCURRED);
	writeb(reg, hdmi_base + S5P_STATUS_EN);

	/* clear all result */
	writeb(CLEAR_ALL_RESULTS, hdmi_base + S5P_HDCP_CHECK_RESULT);

	/* disable hdmi status enable reg. */
	reg = readb(hdmi_base + S5P_STATUS_EN);
	reg &= HDCP_STATUS_DIS_ALL;
 	writeb(reg, hdmi_base + S5P_STATUS_EN);

	/* clear all status pending */
	reg = readb(hdmi_base + S5P_STATUS);
	reg |= HDCP_STATUS_EN_ALL;
	writeb(reg, hdmi_base + S5P_STATUS);

	/* Disable encryption */
	writeb(HDCP_ENC_DIS, hdmi_base + S5P_ENC_EN);

	/* Disable hdcp */
	writeb(0x0, hdmi_base + S5P_HDCP_CTRL1);
	writeb(0x0, hdmi_base + S5P_HDCP_CTRL2);
        sw_reset = true;	
#if defined(CONFIG_S5PC110_DEMPSEY_BOARD)	
	reg = s5p_hdmi_get_enabled_interrupt();		//NAGSM_Android_SEL_Kernel_Aakash_20101214
#endif
	/*
	 * 1. Mask HPD plug and unplug interrupt
         * disable HPD INT
         */
	s5p_hdmi_disable_interrupts(HDMI_IRQ_HPD_PLUG);
	s5p_hdmi_disable_interrupts(HDMI_IRQ_HPD_UNPLUG);

	/* 2. Enable software HPD */
        sw_hpd_enable(true);
	
	/* 3. Make software HPD logical ¡®0¡¯*/
        set_sw_hpd(false);

	/* 4. Make software HPD logical ¡®1¡¯*/
        set_sw_hpd(true);

	/* 5. Disable software HPD */
        sw_hpd_enable(false);	


#if defined(CONFIG_S5PC110_DEMPSEY_BOARD)	
//NAGSM_Android_SEL_Kernel_Aakash_20101214
	/* 6. Unmask HPD plug and unplug interrupt */
	if (reg & 1<<HDMI_IRQ_HPD_PLUG)
	s5p_hdmi_enable_interrupts(HDMI_IRQ_HPD_PLUG);
	if (reg & 1<<HDMI_IRQ_HPD_UNPLUG)
	s5p_hdmi_enable_interrupts(HDMI_IRQ_HPD_UNPLUG);
#else	
//NAGSM_Android_SEL_Kernel_Aakash_20101214
	/* 6. Unmask HPD plug and unplug interrupt */
	s5p_hdmi_enable_interrupts(HDMI_IRQ_HPD_PLUG);
	s5p_hdmi_enable_interrupts(HDMI_IRQ_HPD_UNPLUG);
#endif		//NAGSM_Android_SEL_Kernel_Aakash_20101214

        sw_reset = false;
	/* set hdcp_int enable */
	reg = readb(hdmi_base + S5P_STATUS_EN);
	reg |= WTFORACTIVERX_INT_OCCURRED | 
		WATCHDOG_INT_OCCURRED |
		EXCHANGEKSV_INT_OCCURRED | 
		UPDATE_RI_INT_OCCURRED;
	writeb(reg, hdmi_base + S5P_STATUS_EN);

	/* HDCP Enable */
	writeb(CP_DESIRED_EN, hdmi_base + S5P_HDCP_CTRL1);

	spin_unlock_irq(&hdcp_info.reset_lock);
#if defined(CONFIG_S5PC110_DEMPSEY_BOARD)
	//hdcp_reset_timer_func(1); //workaround timer implimentation to start safe hdcp authentication
	//msleep(10);
	s5p_hdmi_enable_interrupts(HDMI_IRQ_HDCP);
	sw_hpd_enable(true);//Rajucm:to restart hdcp
#endif
}

/*
 * Set the timing parameter for load e-fuse key.
 */

/* TODO: must use clk_get for pclk rate */
#define PCLK_D_RATE_FOR_HDCP	166000000

u32 efuse_ceil(u32 val, u32 time)
{
	u32 res;

	res = val / time;

	if ( val % time)
		res += 1;

	return res;
}

static void hdcp_efuse_timing(void)
{
	u32 time, val;

	/* TODO: must use clk_get for pclk rate */
	time = 1000000000/PCLK_D_RATE_FOR_HDCP;

	val = efuse_ceil(EFUSE_ADDR_WIDTH,time);	
	writeb(val, hdmi_base + S5P_EFUSE_ADDR_WIDTH);

	val = efuse_ceil(EFUSE_SIGDEV_ASSERT,time);
	writeb(val, hdmi_base + S5P_EFUSE_SIGDEV_ASSERT);

	val = efuse_ceil(EFUSE_SIGDEV_DEASSERT,time);
	writeb(val, hdmi_base + S5P_EFUSE_SIGDEV_DEASSERT);

	val = efuse_ceil( EFUSE_PRCHG_ASSERT,time);
	writeb(val, hdmi_base + S5P_EFUSE_PRCHG_ASSERT);

	val = efuse_ceil( EFUSE_PRCHG_DEASSERT,time);
	writeb(val, hdmi_base + S5P_EFUSE_PRCHG_DEASSERT);

	val = efuse_ceil( EFUSE_FSET_ASSERT,time);
	writeb(val, hdmi_base + S5P_EFUSE_FSET_ASSERT);

	val = efuse_ceil( EFUSE_FSET_DEASSERT,time);
	writeb(val, hdmi_base + S5P_EFUSE_FSET_DEASSERT);

	val = efuse_ceil( EFUSE_SENSING,time);
	writeb(val, hdmi_base + S5P_EFUSE_SENSING);

	val = efuse_ceil( EFUSE_SCK_ASSERT,time);
	writeb(val, hdmi_base + S5P_EFUSE_SCK_ASSERT);

	val = efuse_ceil( EFUSE_SCK_DEASSERT,time);
	writeb(val, hdmi_base + S5P_EFUSE_SCK_DEASSERT);

	val = efuse_ceil( EFUSE_SDOUT_OFFSET,time);
	writeb(val, hdmi_base + S5P_EFUSE_SDOUT_OFFSET);

	val = efuse_ceil( EFUSE_READ_OFFSET,time);
	writeb(val, hdmi_base + S5P_EFUSE_READ_OFFSET);

}

/*
 * load hdcp key from e-fuse mem.
 */
static int hdcp_loadkey(void)
{
	u8 status;

#if 0
	hdcp_efuse_timing();
#endif
	/* read HDCP key from E-Fuse */
	writeb(EFUSE_CTRL_ACTIVATE, hdmi_base + S5P_EFUSE_CTRL);

	do
	{
		status = readb(hdmi_base + S5P_EFUSE_STATUS);
	} while(!(status & EFUSE_ECC_DONE));

	if(readb(hdmi_base + S5P_EFUSE_STATUS) & EFUSE_ECC_FAIL) {
		HDCPPRINTK("Can't load key from fuse ctrl.\n");
		return -EINVAL;
	}	

	return 0;
	
}

/*
 * Start encryption
 */
static void start_encryption(void)
{
	u32  hdcp_status;
	u32 time_out = 1000;

	if (readl(hdmi_base + S5P_HDCP_CHECK_RESULT) ==
		Ri_MATCH_RESULT__YES) {

		while (time_out) {
                        EXIT_WORK_ON_CABLE_DISCONNECTION;
			if (readl(hdmi_base + S5P_STATUS) & AUTHENTICATED) {
				writel(HDCP_ENC_ENABLE,
					hdmi_base + S5P_ENC_EN);
				printk("Encryption start!!\n");
				s5p_hdmi_mute_en(false);
				break;
			} else {
				time_out--;
				mdelay(1);
			}
			}
	} else {
		writel(HDCP_ENC_DISABLE, hdmi_base + S5P_ENC_EN);
		s5p_hdmi_mute_en(true);
		HDCPPRINTK("Encryption stop!!\n");
	}

printk("start_encryption: count = %d\n", 1000-time_out);
}

/*
 * Check  whether Rx is repeater or not
 */
static int check_repeater(void)
{
	int ret = 0;

	u8 i = 0;
	u16 j = 0;
	
	u8 bcaps[BCAPS_SIZE] = {0};
	u8 status[BSTATUS_SIZE] = {0,0};
	u8 rx_v[SHA_1_HASH_SIZE] = {0};
	u8 ksv_list[HDCP_MAX_DEVS*HDCP_KSV_SIZE] = {0};
	
	u32 hdcp_ctrl = 0;
	u32 dev_cnt;
	u32 stat;	

	bool ksv_fifo_ready = false;

	memset(rx_v, 0x0, SHA_1_HASH_SIZE);
	memset(ksv_list, 0x0, HDCP_MAX_DEVS*HDCP_KSV_SIZE);
	
	while (j <= 50) {
	        EXIT_WORK_ON_CABLE_DISCONNECTION;
		ret = ddc_read( HDCP_Bcaps, 
				bcaps, BCAPS_SIZE);

		if(ret < 0){
			HDCPPRINTK("Can't read bcaps data from i2c bus\n");
			return false;
		}

		if (bcaps[0] & KSV_FIFO_READY) {
			HDCPPRINTK("ksv fifo is ready\n");
			ksv_fifo_ready = true;
			writel(bcaps[0], hdmi_base + S5P_HDCP_BCAPS);
			break;
		}else{
			HDCPPRINTK("ksv fifo is not ready\n");
			ksv_fifo_ready = false;
			mdelay(100);
			j++;
		}
	
		bcaps[0] = 0;
	}

	if (!ksv_fifo_ready)
		return REPEATER_TIMEOUT_ERROR;

	/*
	 * Check MAX_CASCADE_EXCEEDED 
	 * or MAX_DEVS_EXCEEDED indicator
	 */
	ret = ddc_read( HDCP_BStatus, 
		status, BSTATUS_SIZE);
	
	if(ret < 0){
		HDCPPRINTK("Can't read status data from i2c bus\n");
		return false;
	}
	
	/* MAX_CASCADE_EXCEEDED || MAX_DEVS_EXCEEDED */
	if (status[1] & MAX_CASCADE_EXCEEDED) {
		HDCPPRINTK("MAX_CASCADE_EXCEEDED\n");
		return MAX_CASCADE_EXCEEDED_ERROR;
	} else if(status[0] & MAX_DEVS_EXCEEDED) {
		HDCPPRINTK("MAX_CASCADE_EXCEEDED\n");
		return MAX_DEVS_EXCEEDED_ERROR;
	}

	writel(status[0], hdmi_base + S5P_HDCP_BSTATUS_0);
	writel(status[1], hdmi_base + S5P_HDCP_BSTATUS_1);
		 
	/* Read KSV list */
	dev_cnt = status[0] & 0x7f;

	HDCPPRINTK("status[0] :0x%08x, status[1] :0x%08x!!\n",
		status[0],status[1]);

	if(dev_cnt) {

		u32 val = 0;

		/* read ksv */
		ret = ddc_read( HDCP_KSVFIFO, ksv_list, 
				dev_cnt * HDCP_KSV_SIZE);
		if (ret <0) {
			HDCPPRINTK("Can't read ksv fifo!!\n");
			return false;
		}

		/* write ksv */
		for (i = 0; i < dev_cnt - 1; i++) {

			writel(ksv_list[(i*5) + 0],
				hdmi_base + S5P_HDCP_RX_KSV_0_0);
			writel(ksv_list[(i*5) + 1],
				hdmi_base + S5P_HDCP_RX_KSV_0_1);
			writel(ksv_list[(i*5) + 2],
				hdmi_base + S5P_HDCP_RX_KSV_0_2);
			writel(ksv_list[(i*5) + 3],
				hdmi_base + S5P_HDCP_RX_KSV_0_3);
			writel(ksv_list[(i*5) + 4],
				hdmi_base + S5P_HDCP_RX_KSV_0_4);

			mdelay(1);
			writel(SET_HDCP_KSV_WRITE_DONE,
				hdmi_base + S5P_HDCP_RX_KSV_LIST_CTRL);
			mdelay(1);

			stat = readl(hdmi_base + S5P_HDCP_RX_KSV_LIST_CTRL);

			if (!(stat & SET_HDCP_KSV_READ))
				return false;

			HDCPPRINTK("HDCP_RX_KSV_1 = 0x%x\n\r",
				readl(hdmi_base + S5P_HDCP_RX_KSV_LIST_CTRL));
			HDCPPRINTK("i : %d, dev_cnt : %d, val = 0x%08x\n",
				i, dev_cnt, val);
		}

		writel(ksv_list[(i*5) + 0], hdmi_base + S5P_HDCP_RX_KSV_0_0);
		writel(ksv_list[(i*5) + 1], hdmi_base + S5P_HDCP_RX_KSV_0_1);
		writel(ksv_list[(i*5) + 2], hdmi_base + S5P_HDCP_RX_KSV_0_2);
		writel(ksv_list[(i*5) + 3], hdmi_base + S5P_HDCP_RX_KSV_0_3);
		writel(ksv_list[(i*5) + 4], hdmi_base + S5P_HDCP_RX_KSV_0_4);

		mdelay(1);

		/* end of ksv */
//		val = readl(hdmi_base + S5P_HDCP_RX_KSV_LIST_CTRL);
		val = SET_HDCP_KSV_END|SET_HDCP_KSV_WRITE_DONE;
		writel(val, hdmi_base + S5P_HDCP_RX_KSV_LIST_CTRL);

		HDCPPRINTK("HDCP_RX_KSV_1 = 0x%x\n\r",
				readl(hdmi_base + S5P_HDCP_RX_KSV_LIST_CTRL));
		HDCPPRINTK("i : %d, dev_cnt : %d, val = 0x%08x\n",i,dev_cnt,val);

	} else {
		
		/*
		mdelay(200);
		*/
		
		writel(SET_HDCP_KSV_LIST_EMPTY, 
			hdmi_base + S5P_HDCP_RX_KSV_LIST_CTRL);
	}


	/* Read SHA-1 from receiver */
	ret = ddc_read( HDCP_SHA1, 
		rx_v, SHA_1_HASH_SIZE);
	
	if (ret < 0) {
		HDCPPRINTK("Can't read sha_1_hash data from i2c bus\n");
		return false;
	}

	for(i = 0; i < SHA_1_HASH_SIZE; i++) {
		HDCPPRINTK("SHA_1 rx :: %x\n",rx_v[i]);
	}

	/* write SHA-1 to register */
	writeb(rx_v[0], hdmi_base + S5P_HDCP_RX_SHA1_0_0);
	writeb(rx_v[1], hdmi_base + S5P_HDCP_RX_SHA1_0_1);
	writeb(rx_v[2], hdmi_base + S5P_HDCP_RX_SHA1_0_2);
	writeb(rx_v[3], hdmi_base + S5P_HDCP_RX_SHA1_0_3);
	writeb(rx_v[4], hdmi_base + S5P_HDCP_RX_SHA1_1_0);
	writeb(rx_v[5], hdmi_base + S5P_HDCP_RX_SHA1_1_1);
	writeb(rx_v[6], hdmi_base + S5P_HDCP_RX_SHA1_1_2);
	writeb(rx_v[7], hdmi_base + S5P_HDCP_RX_SHA1_1_3);
	writeb(rx_v[8], hdmi_base + S5P_HDCP_RX_SHA1_2_0);
	writeb(rx_v[9], hdmi_base + S5P_HDCP_RX_SHA1_2_1);
	writeb(rx_v[10], hdmi_base + S5P_HDCP_RX_SHA1_2_2);
	writeb(rx_v[11], hdmi_base + S5P_HDCP_RX_SHA1_2_3);
	writeb(rx_v[12], hdmi_base + S5P_HDCP_RX_SHA1_3_0);
	writeb(rx_v[13], hdmi_base + S5P_HDCP_RX_SHA1_3_1);
	writeb(rx_v[14], hdmi_base + S5P_HDCP_RX_SHA1_3_2);
	writeb(rx_v[15], hdmi_base + S5P_HDCP_RX_SHA1_3_3);
	writeb(rx_v[16], hdmi_base + S5P_HDCP_RX_SHA1_4_0);
	writeb(rx_v[17], hdmi_base + S5P_HDCP_RX_SHA1_4_1);
	writeb(rx_v[18], hdmi_base + S5P_HDCP_RX_SHA1_4_2);
	writeb(rx_v[19], hdmi_base + S5P_HDCP_RX_SHA1_4_3);

	/* SHA write done, and wait for SHA computation being done */
	mdelay(1);
	
	/* check authentication success or not */
	stat = readb(hdmi_base + S5P_HDCP_AUTH_STATUS);
		
	HDCPPRINTK("auth status %d\n",stat);

	if (stat & SET_HDCP_SHA_VALID_READY) {

		stat = readb(hdmi_base + S5P_HDCP_AUTH_STATUS);
		
		if (stat & SET_HDCP_SHA_VALID)
			ret = true;
		else
			ret = false;
	} else {
	
		HDCPPRINTK("SHA not ready 0x%x \n\r",stat);
		ret = false;
	}
		

	/* clear all validate bit */
	writeb(0x0, hdmi_base + S5P_HDCP_AUTH_STATUS);

	return ret;

}
static bool try_read_receiver(void)
{
	u16 i = 0;
	bool ret = false;

	s5p_hdmi_mute_en(true);

	for (i = 0; i < 400; i++) {


		EXIT_WORK_ON_CABLE_DISCONNECTION;
		msleep(250); //mdelay(250); Rajucm: cpu utilization is 94% @tvoff

		if (hdcp_info.auth_status != RECEIVER_READ_READY){

			HDCPPRINTK("hdcp stat. changed!!"
				"failed attempt no = %d\n\r",i);

			return false;
		}

		ret = read_bcaps();

		if (ret) {
			
			HDCPPRINTK("succeeded at attempt no= %d \n\r",i);
			
			return true;
			
		} else
			HDCPPRINTK("can't read bcaps!!"
				"failed attempt no=%d\n\r",i);
	}

	return false;
}


/*
 * stop  - stop functions are only called under running HDCP
 */
bool __s5p_stop_hdcp(void)
{
	u32  sfr_val = 0;

	HDCPPRINTK("HDCP ftn. Stop!!\n");

#if 0
	s5p_hdmi_disable_interrupts(HDMI_IRQ_HPD_PLUG);
	s5p_hdmi_disable_interrupts(HDMI_IRQ_HPD_UNPLUG);
#endif
	s5p_hdmi_disable_interrupts(HDMI_IRQ_HDCP);

	hdcp_protocol_status = 0;

	hdcp_info.time_out 	= INFINITE;
	hdcp_info.event 	= HDCP_EVENT_STOP;
	hdcp_info.auth_status 	= NOT_AUTHENTICATED;
	hdcp_info.hdcp_enable 	= false;

	/*
	hdcp_info.client	= NULL;
	*/

	/* 3. disable hdcp control reg. */
	sfr_val = readl(hdmi_base + S5P_HDCP_CTRL1);
	sfr_val &= ( ENABLE_1_DOT_1_FEATURE_DIS 
			& CLEAR_REPEATER_TIMEOUT 
			& EN_PJ_DIS
			& CP_DESIRED_DIS);
	writel(sfr_val, hdmi_base + S5P_HDCP_CTRL1);

	/* 1-3. disable hdmi hpd reg. */
	sw_hpd_enable(false);
	//writel(CABLE_UNPLUGGED, hdmi_base + S5P_HPD);

	/* 1-2. disable hdmi status enable reg. */
	sfr_val = readl(hdmi_base + S5P_STATUS_EN);
	sfr_val &= HDCP_STATUS_DIS_ALL;
 	writel(sfr_val, hdmi_base + S5P_STATUS_EN);

	/* 1-1. clear all status pending */
	sfr_val = readl(hdmi_base + S5P_STATUS);
	sfr_val |= HDCP_STATUS_EN_ALL;
	writel(sfr_val, hdmi_base + S5P_STATUS);

	/* disable encryption */
	HDCPPRINTK("Stop Encryption by Stop!!\n");
	writel(HDCP_ENC_DISABLE, hdmi_base + S5P_ENC_EN);
        //s5p_hdmi_mute_en(true); //Rajucm: avoid displaying green screen for hdcp disabled Image

	/* clear result */
	writel(Ri_MATCH_RESULT__NO, hdmi_base + S5P_HDCP_CHECK_RESULT);
//	writel(readl(hdmi_base + S5P_HDMI_CON_0) & HDMI_DIS, 
//		hdmi_base + S5P_HDMI_CON_0);	
//	writel(readl(hdmi_base + S5P_HDMI_CON_0) | HDMI_EN, 
//		hdmi_base + S5P_HDMI_CON_0);	
	writel(CLEAR_ALL_RESULTS, hdmi_base + S5P_HDCP_CHECK_RESULT);

	/* hdmi disable */
#if 0
	sfr_val = readl(hdmi_base + S5P_HDMI_CON_0);
	sfr_val &= ~(PWDN_ENB_NORMAL | HDMI_EN | ASP_EN);
	writel( sfr_val, hdmi_base + S5P_HDMI_CON_0);
	*/
	HDCPPRINTK( "\tSTATUS \t0x%08x\n",readl(hdmi_base + S5P_STATUS));
	HDCPPRINTK( "\tSTATUS_EN \t0x%08x\n",readl(hdmi_base + S5P_STATUS_EN));
	HDCPPRINTK( "\tHPD \t0x%08x\n",readl(hdmi_base + S5P_HPD));
	HDCPPRINTK( "\tHDCP_CTRL \t0x%08x\n",readl(hdmi_base + S5P_HDCP_CTRL1));
	HDCPPRINTK( "\tMODE_SEL \t0x%08x\n",readl(hdmi_base + S5P_MODE_SEL));
	HDCPPRINTK( "\tENC_EN \t0x%08x\n",readl(hdmi_base + S5P_ENC_EN));	
	HDCPPRINTK( "\tHDMI_CON_0 \t0x%08x\n",readl(hdmi_base + S5P_HDMI_CON_0));	
#endif
	return true;
}


void __s5p_hdcp_reset(void)
{

	__s5p_stop_hdcp();

	hdcp_protocol_status = 2;

	HDCPPRINTK("HDCP ftn. reset!!\n");
}

/*
 * start  - start functions are only called under stopping HDCP
 */
bool __s5p_start_hdcp(void)
{		
	u32  sfr_val;
	u8 reg;			

	hdcp_info.event 	= HDCP_EVENT_STOP;
	hdcp_info.time_out 	= INFINITE;
	hdcp_info.auth_status 	= NOT_AUTHENTICATED;	

	HDCPPRINTK("HDCP ftn. Start!!\n");

	sw_reset = true;
#if defined(CONFIG_S5PC110_DEMPSEY_BOARD)
	reg = s5p_hdmi_get_enabled_interrupt();
#endif

	s5p_hdmi_disable_interrupts(HDMI_IRQ_HPD_PLUG);
	s5p_hdmi_disable_interrupts(HDMI_IRQ_HPD_UNPLUG);

	/* 2. Enable software HPD */
	sw_hpd_enable(true);

	/* 3. Make software HPD logical */
	set_sw_hpd(false);
	/* 4. Make software HPD logical */
	set_sw_hpd(true);

	/* 5. Disable software HPD */
	sw_hpd_enable(false);
	set_sw_hpd(false);

#if defined(CONFIG_S5PC110_DEMPSEY_BOARD) 
//NAGSM_Android_SEL_Kernel_Aakash_20101214
	/* 6. Unmask HPD plug and unplug interrupt */
	if (reg & 1<<HDMI_IRQ_HPD_PLUG)
	s5p_hdmi_enable_interrupts(HDMI_IRQ_HPD_PLUG);
	if (reg & 1<<HDMI_IRQ_HPD_UNPLUG)
	s5p_hdmi_enable_interrupts(HDMI_IRQ_HPD_UNPLUG);
	
#else
	/* 6. Unmask HPD plug and unplug interrupt */
	s5p_hdmi_enable_interrupts(HDMI_IRQ_HPD_PLUG);
	s5p_hdmi_enable_interrupts(HDMI_IRQ_HPD_UNPLUG);
#endif 	
//NAGSM_Android_SEL_Kernel_Aakash_20101214

	sw_reset = false;
	HDCPPRINTK("Stop Encryption by Start!!\n");

	writel(HDCP_ENC_DISABLE, hdmi_base + S5P_ENC_EN);
	s5p_hdmi_mute_en(true);

	hdcp_protocol_status = 1;

	if (hdcp_loadkey() < 0)
		return false;

	/* for av mute */
	writel(DO_NOT_TRANSMIT, hdmi_base + S5P_GCP_CON);

	/* 
	 * 1-1. set hdmi status enable reg. 
	 * Update_Ri_int_en should be enabled after 
	 * s/w gets ExchangeKSV_int.
	 */
	writel(HDCP_STATUS_EN_ALL, hdmi_base + S5P_STATUS_EN);

	/*
	 * 3. set hdcp control reg.
	 * Disable advance cipher option, Enable CP(Content Protection),
	 * Disable time-out (This bit is only available in a REPEATER)
	 * Disable XOR shift,Disable Pj port update,Use external key
	 */
	sfr_val = 0;
	sfr_val |= CP_DESIRED_EN;
	writel(sfr_val, hdmi_base + S5P_HDCP_CTRL1);

	s5p_hdmi_enable_interrupts(HDMI_IRQ_HDCP);
	set_sw_hpd(true);
        sw_hpd_enable(true);//Rajucm:to restart hdcp
	if (!read_bcaps()) {
		HDCPPRINTK("can't read ddc port!\n");
		reset_authentication();
	}

	hdcp_info.hdcp_enable = true;
	
	HDCPPRINTK( "\tSTATUS \t0x%08x\n",readl(hdmi_base + S5P_STATUS));
	HDCPPRINTK( "\tSTATUS_EN \t0x%08x\n",readl(hdmi_base + S5P_STATUS_EN));
	HDCPPRINTK( "\tHPD \t0x%08x\n",readl(hdmi_base + S5P_HPD));
	HDCPPRINTK( "\tHDCP_CTRL \t0x%08x\n",readl(hdmi_base + S5P_HDCP_CTRL1));
	HDCPPRINTK( "\tMODE_SEL \t0x%08x\n",readl(hdmi_base + S5P_MODE_SEL));
	HDCPPRINTK( "\tENC_EN \t0x%08x\n",readl(hdmi_base + S5P_ENC_EN));
	HDCPPRINTK( "\tHDMI_CON_0 \t0x%08x\n",readl(hdmi_base + S5P_HDMI_CON_0));


	return true;
}


static void bksv_start_bh(void)
{
	bool ret = false;

	HDCPPRINTK("HDCP_EVENT_READ_BKSV_START bh\n");

	hdcp_info.auth_status = RECEIVER_READ_READY;

	ret = read_bcaps();

	if(!ret) {
		
		ret = try_read_receiver();
		
		if(!ret) {
			HDCPPRINTK("Can't read bcaps!! retry failed!!\n" 
				"\t\t\t\thdcp ftn. will be stopped\n");

			reset_authentication();
			return;	
		}
	}
		
	hdcp_info.auth_status = BCAPS_READ_DONE;

	ret = read_bksv();

	if(!ret) {
		HDCPPRINTK("Can't read bksv!!" 
			"hdcp ftn. will be reset\n");

		reset_authentication();
		return;
	}

	hdcp_info.auth_status = BKSV_READ_DONE;

	HDCPPRINTK("authentication status : bksv is done (0x%08x)\n", 
		hdcp_info.auth_status);
}

static void second_auth_start_bh(void)
{
	u8 count = 0;
	int reg;
	bool ret = false;

	int ret_err;
	
	u32 bcaps;

	HDCPPRINTK("HDCP_EVENT_SECOND_AUTH_START bh\n");

	ret = read_bcaps();
	
	if (!ret) {
		
		ret = try_read_receiver();
		
		if (!ret) {
			
			HDCPPRINTK("Can't read bcaps!! retry failed!!\n" 
				"\t\t\t\thdcp ftn. will be stopped\n");

			reset_authentication();
			return;
		}

	}

	bcaps = readl(hdmi_base + S5P_HDCP_BCAPS);
	bcaps &= (KSV_FIFO_READY);
	
	if (!bcaps) {
		
		HDCPPRINTK("ksv fifo is not ready\n");

		do {
			count ++;
			
			ret = read_bcaps();

			if (!ret) {
				
				ret = try_read_receiver();
				if (!ret)
					reset_authentication();
				return;

			}

			bcaps = readl(hdmi_base + S5P_HDCP_BCAPS);
			bcaps &= (KSV_FIFO_READY);

			if (bcaps) {
				HDCPPRINTK("bcaps retries : %d\n",count);
				break;
			}
			EXIT_WORK_ON_CABLE_DISCONNECTION;
			mdelay(100);
			
			if (!hdcp_info.hdcp_enable) {

				reset_authentication();
				return;

			}
			
		} while(count <= 50);
#if 0 //Rajucms: error simulation for 1B_03_FAIL case
        if (test_counter++ > 5)
        {  
            test_counter=0;
            HDCPPRINTK("####ERROR CONDITION SIMULATED####");
           count = 100; //1B_03_FAIL
        }
#endif		
		/* wait times exceeded 5 seconds */
		if (count >50) {

			hdcp_info.time_out = INFINITE;

			/* time-out (This bit is only available in a REPEATER) */
			writel(readl(hdmi_base + S5P_HDCP_CTRL1) | 0x1 << 2,
				hdmi_base + S5P_HDCP_CTRL1);

			reset_authentication();
			return;
		}
	}

	HDCPPRINTK("ksv fifo ready\n");

	ret_err = check_repeater();
#if 0 //Rajucms: error simulation for 1B_03_FAIL, 1B_05_FAIL, 1B_06_FAIL case
        if (test_counter++ > 4)
        {  
            test_counter=0;
            HDCPPRINTK("####ERROR CONDITION SIMULATED####");
            ret_err = REPEATER_TIMEOUT_ERROR; //for 1B_05_FAIL
            //ret_err = false; //1B_06_FAIL
        }
#endif
	if (ret_err == true) {
		u32 flag;
		
		hdcp_info.auth_status = SECOND_AUTHENTICATION_DONE;
		HDCPPRINTK("second authentication done!!\n");

		flag = readb(hdmi_base + S5P_STATUS);
		HDCPPRINTK("hdcp state : %s authenticated!!\n",
			flag & AUTHENTICATED ? "":"not not");
		
		start_encryption();
	} else if (ret_err == false) {
		/* i2c error */
		HDCPPRINTK("repeater check error!!\n");
		reset_authentication();
	} else {
		if (ret_err == REPEATER_ILLEGAL_DEVICE_ERROR) {
			/* 
			 * No need to start the HDCP 
			 * in case of invalid KSV (revocation case)
			 */
			HDCPPRINTK("illegal dev. error!!\n");
	
			reg = readl(hdmi_base + S5P_HDCP_CTRL2);
			reg = 0x1;
			writel(reg, hdmi_base + S5P_HDCP_CTRL2);
			reg = 0x0;
			writel(reg, hdmi_base + S5P_HDCP_CTRL2);
			hdcp_info.auth_status = NOT_AUTHENTICATED;
		} else if (ret_err == REPEATER_TIMEOUT_ERROR) {
			reg = readl(hdmi_base + S5P_HDCP_CTRL1);
			reg |= SET_REPEATER_TIMEOUT;
			writel(reg, hdmi_base + S5P_HDCP_CTRL1);
			reg &= ~SET_REPEATER_TIMEOUT;
			writel(reg, hdmi_base + S5P_HDCP_CTRL1);
			hdcp_info.auth_status = NOT_AUTHENTICATED;
		} else {
			/* 
			 * MAX_CASCADE_EXCEEDED_ERROR
			 * MAX_DEVS_EXCEEDED_ERROR
			 */
			HDCPPRINTK("repeater check error(MAX_EXCEEDED)!!\n");
			reset_authentication();
		}
	}
}

static bool write_aksv_start_bh(void)
{
	bool ret = false;

	HDCPPRINTK("HDCP_EVENT_WRITE_AKSV_START bh\n");

	if (hdcp_info.auth_status != BKSV_READ_DONE) {
		HDCPPRINTK("bksv is not ready!!\n");
		return false;
	}
#if 0 //Rajucm
	ret = write_ainfo();
	if(!ret)
		return false;

	HDCPPRINTK("ainfo write done!!\n");	
#endif	
	ret = write_an();
	if(!ret)
		return false;
	
	hdcp_info.auth_status = AN_WRITE_DONE;
	
	HDCPPRINTK("an write done!!\n");			
	
	ret = write_aksv();
	if(!ret)
		return false;

	/*
	 * Wait for 100ms. Transmitter must not read 
	 * Ro' value sooner than 100ms after writing 
	 * Aksv
	 */
	mdelay(100);

	hdcp_info.auth_status = AKSV_WRITE_DONE;

	HDCPPRINTK("aksv write done!!\n");

	return ret;			
}

static bool check_ri_start_bh(void)
{
	bool ret = false;


	HDCPPRINTK("HDCP_EVENT_CHECK_RI_START bh\n");

	if (hdcp_info.auth_status == AKSV_WRITE_DONE ||
	    hdcp_info.auth_status == FIRST_AUTHENTICATION_DONE ||
	    hdcp_info.auth_status == SECOND_AUTHENTICATION_DONE) {

		ret = compare_r_val();

		if (ret) {
			
			if (hdcp_info.auth_status == AKSV_WRITE_DONE) {
				/* 
				 * Check whether HDMI receiver is 
				 * repeater or not
				 */
				if (hdcp_info.is_repeater)
					hdcp_info.auth_status 
						= SECOND_AUTHENTICATION_RDY;
				else {
					hdcp_info.auth_status 
						= FIRST_AUTHENTICATION_DONE;
					start_encryption();
				}
			}
		
		} else {
		
			HDCPPRINTK("authentication reset\n");
			reset_authentication();
		}

		HDCPPRINTK("auth_status = 0x%08x\n",
			hdcp_info.auth_status);

		
		return true;
	} 
	else
		{
		HDCPPRINTK("##auth_status = 0x%08x\n", 	hdcp_info.auth_status);
		reset_authentication();
		}
	HDCPPRINTK("aksv_write or first/second" 
		" authentication is not done\n");
	
	return false;
}

/* 
 * bottom half for hdmi interrupt
 *
 */
static void hdcp_work(void *arg)
{
	/*
	HDCPPRINTK("event : 0x%08x \n\r", hdcp_info.event);
	*/

	/* 
	 * I2C int. was occurred 
	 * for reading Bksv and Bcaps
	 */
	
	if (hdcp_info.event & (1 << HDCP_EVENT_READ_BKSV_START)) {
		
		bksv_start_bh();

		/* clear event */
		/*
		spin_lock_bh(&hdcp_info.lock);
		*/
		hdcp_info.event &= ~( 1 << HDCP_EVENT_READ_BKSV_START );
		/*
		spin_unlock_bh(&hdcp_info.lock);
		*/
	}

	/* 
	 * Watchdog timer int. was occurred 
	 * for checking repeater
	 */
	if (hdcp_info.event & (1 << HDCP_EVENT_SECOND_AUTH_START)) {

		second_auth_start_bh();

		/* clear event */	
		/*
		spin_lock_bh(&hdcp_info.lock);
		*/
		hdcp_info.event &= ~(1 << HDCP_EVENT_SECOND_AUTH_START);
		/*
		spin_unlock_bh(&hdcp_info.lock);
		*/
	}

	/* 
	 * An_Write int. was occurred 
	 * for writing Ainfo, An and Aksv
	 */
	if (hdcp_info.event & (1 << HDCP_EVENT_WRITE_AKSV_START)) {

		write_aksv_start_bh();

		/* clear event */
		/*
		spin_lock_bh(&hdcp_info.lock);
		*/
		hdcp_info.event  &= ~(1 << HDCP_EVENT_WRITE_AKSV_START);
		/*
		spin_unlock_bh(&hdcp_info.lock);
		*/
	}		
		
	/* 
	 * Ri int. was occurred 
	 * for comparing Ri and Ri'(from HDMI sink) 
	 */
	if (hdcp_info.event & (1 << HDCP_EVENT_CHECK_RI_START)) {


		check_ri_start_bh();

		/* clear event */
		/*
		spin_lock_bh(&hdcp_info.lock);
		*/
		hdcp_info.event &= ~(1 << HDCP_EVENT_CHECK_RI_START);
		/*
		spin_unlock_bh(&hdcp_info.lock);
		*/
	}	

}

void __s5p_init_hdcp(bool hpd_status, struct i2c_client *ddc_port)
{

	HDCPPRINTK("HDCP ftn. Init!!\n");

	is_dvi = false;
	av_mute = false;
	audio_en = true;

	/* for bh */
	INIT_WORK(&hdcp_info.work, (work_func_t)hdcp_work);

	init_waitqueue_head(&hdcp_info.waitq);

	/* for dev_dbg err. */
	spin_lock_init(&hdcp_info.lock);

}


irqreturn_t __s5p_hdcp_irq_handler(int irq)

{
	u32 event = 0;
	u8 flag;

	event = 0;
	/* check HDCP Status */
	flag = readb(hdmi_base + S5P_STATUS);
	
	HDCPPRINTK("irq_status : 0x%08x\n", readb(hdmi_base + S5P_STATUS));

	HDCPPRINTK("hdcp state : %s authenticated!!\n",
		flag & AUTHENTICATED ? "":"not");

	spin_lock_irq(&hdcp_info.lock);
	
	/* 
	 * processing interrupt 
	 * interrupt processing seq. is firstly set event for workqueue,
	 * and interrupt pending clear. 'flag|' was used for preventing
	 * to clear AUTHEN_ACK.- it caused many problem. be careful.
	 */
	/* I2C INT */
	if (flag & WTFORACTIVERX_INT_OCCURRED) {
		event |= (1 << HDCP_EVENT_READ_BKSV_START);
		writeb(flag | WTFORACTIVERX_INT_OCCURRED,
			 hdmi_base + S5P_STATUS);
		writeb(0x0, hdmi_base + S5P_HDCP_I2C_INT);
	}

	/* AN INT */
	if (flag & EXCHANGEKSV_INT_OCCURRED) {
		event |= (1 << HDCP_EVENT_WRITE_AKSV_START);
		writeb(flag | EXCHANGEKSV_INT_OCCURRED, 
			hdmi_base + S5P_STATUS);
		writeb(0x0, hdmi_base + S5P_HDCP_AN_INT);
	}

	/* RI INT */
	if (flag & UPDATE_RI_INT_OCCURRED) {
		event |= (1 << HDCP_EVENT_CHECK_RI_START);
		writeb(flag | UPDATE_RI_INT_OCCURRED,
			hdmi_base + S5P_STATUS);
		writeb(0x0, hdmi_base + S5P_HDCP_RI_INT);
	}

	/* WATCHDOG INT */
	if (flag & WATCHDOG_INT_OCCURRED) {
		event |= (1 << HDCP_EVENT_SECOND_AUTH_START);
		writeb(flag | WATCHDOG_INT_OCCURRED,
			hdmi_base + S5P_STATUS);
		writeb(0x0, hdmi_base + S5P_HDCP_WDT_INT);
	}

	if (!event) {
		HDCPPRINTK("unknown irq.\n");
		return IRQ_HANDLED;
	}

	hdcp_info.event |= event;

#if defined(CONFIG_S5PC110_DEMPSEY_BOARD) //Rajucm: P110225-1327_system_lockup
	queue_work(hdcp_info.hdcp_workqueue, &hdcp_info.work);
#else
	schedule_work(&hdcp_info.work);
#endif

	spin_unlock_irq(&hdcp_info.lock);

	return IRQ_HANDLED;
}

bool __s5p_set_hpd_detection(bool detection, bool hdcp_enabled, 
				struct i2c_client *client)
{
	u32 hpd_reg_val = 0;

	if (detection) 
		hpd_reg_val = CABLE_PLUGGED;
	else {
		hpd_reg_val = CABLE_UNPLUGGED;
	}
	
	writel(hpd_reg_val, hdmi_base + S5P_HPD);
	
	HDCPPRINTK("HPD status :: 0x%08x\n\r", 
		readl(hdmi_base + S5P_HPD));

	return true;
}

int __s5p_hdcp_init(void)
{
	/* for bh */
	INIT_WORK(&hdcp_info.work, (work_func_t)hdcp_work);
	is_dvi = false;
	av_mute = false;
	audio_en = true;

	init_waitqueue_head(&hdcp_info.waitq);

	/* for dev_dbg err. */
	spin_lock_init(&hdcp_info.lock);
	spin_lock_init(&hdcp_info.reset_lock);
	
#if defined(CONFIG_S5PC110_DEMPSEY_BOARD) //Rajucm: P110225-1327_system_lockup
        hdcp_info.hdcp_workqueue = create_singlethread_workqueue("hdcp_workqueue");
#endif
	s5p_hdmi_register_isr((hdmi_isr)__s5p_hdcp_irq_handler, (u8)HDMI_IRQ_HDCP);
	
	return 0;
}
/* called by hpd */
int s5p_hdcp_encrypt_stop(bool on)
{
	u32 reg;
	if (hdcp_info.hdcp_enable) {
		/* clear interrupt pending all */
		writeb(0x0, hdmi_base + S5P_HDCP_I2C_INT);
		writeb(0x0, hdmi_base + S5P_HDCP_AN_INT);
		writeb(0x0, hdmi_base + S5P_HDCP_RI_INT);
		writeb(0x0, hdmi_base + S5P_HDCP_WDT_INT);
		writel(HDCP_ENC_DISABLE, hdmi_base + S5P_ENC_EN);
		s5p_hdmi_mute_en(true);
		if (!sw_reset) {
			reg = readl(hdmi_base + S5P_HDCP_CTRL1);
			if (on) {
				writel(reg | CP_DESIRED_EN,
					hdmi_base + S5P_HDCP_CTRL1);
				s5p_hdmi_enable_interrupts(HDMI_IRQ_HDCP);
			} else {
				hdcp_info.event
					= HDCP_EVENT_STOP;
				hdcp_info.auth_status
					= NOT_AUTHENTICATED;
				writel(reg & ~CP_DESIRED_EN,
					hdmi_base + S5P_HDCP_CTRL1);
				s5p_hdmi_disable_interrupts(HDMI_IRQ_HDCP);
			}
		}
		HDCPPRINTK("Stop Encryption by HPD Event!!\n");
	}
	return 0;
}
EXPORT_SYMBOL(s5p_hdcp_encrypt_stop);
