/*
 * Driver for isx005 (3MP Camera) from SONY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-i2c-drv.h>
#include <media/isx005_platform.h>

#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif

#include <linux/rtc.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <mach/max8998_function.h>
#include <plat/regs-fimc.h> 

#include "isx005.h"

#define ISX005_DRIVER_NAME	"ISX005"

#define FORMAT_FLAGS_COMPRESSED			0x3
#define SENSOR_JPEG_SNAPSHOT_MEMSIZE	0x33F000     //3403776 //2216 * 1536

//#define ISX005_DEBUG
//#define CONFIG_LOAD_FILE	//For tunning binary

#ifdef ISX005_DEBUG
#define isx005_msg	dev_err
#else
#define isx005_msg 	dev_dbg
#endif

/* protect s_ctrl calls */
static DEFINE_MUTEX(sensor_s_ctrl);

/* stop_af_operation is used to cancel the operation while doing, or even before it has started */
static volatile int stop_af_operation;
//static volatile int af_status_check;

/* Here we store the status of AF; 0 --> AF not started, 1 --> AF started , 2 --> AF operation finished */
static int af_operation_status;

/* Save the focus mode value, it can be marco or auto */
static int af_mode;

static int camera_init = 0;
static int gLowLight = 0;
static int gLowLight_flash = 0;
static int gLowLight_flash_second = 0;
static int gCurrentScene = SCENE_MODE_NONE;
static int gIsoCondition = 0; //kidggang - About techwin temp test binay
static int flash_mode = 1; //default is FLASH OFF
static int flash_check = 0; //default is FLASH OFF - Auto on/off
static int preview_ratio = 16;//zzangdol
static int g_ctrl_entered = 0;
static int first_af_start = 0;

extern unsigned int HWREV;


/* Default resolution & pixelformat. plz ref isx005_platform.h */
#define DEFAULT_PIX_FMT		V4L2_PIX_FMT_UYVY	/* YUV422 */
#define DEFUALT_MCLK		24000000
#define POLL_TIME_MS		10

enum isx005_oprmode {
	ISX005_OPRMODE_VIDEO = 0,
	ISX005_OPRMODE_IMAGE = 1,
};

enum isx005_frame_size {
	ISX005_PREVIEW_QCIF = 0,	/* 176x144 */
	ISX005_PREVIEW_VGA,		/* 640x480 */
	ISX005_PREVIEW_D1,			/* 720x480 */
	ISX005_PREVIEW_WVGA,	/* 800x480 */
	ISX005_PREVIEW_SVGA,		/* 800x600 */
	ISX005_PREVIEW_WSVGA,		/* 1024x600*/
	ISX005_CAPTURE_VGA, 		/* 640 x 480 */
	ISX005_CAPTURE_WVGA,	/* 800 x 480 */
	ISX005_CAPTURE_SVGA,		/* SVGA  - 800x600 */
	ISX005_CAPTURE_WSVGA,		/* SVGA  - 1024x600 */
	ISX005_CAPTURE_W1MP,		/* WUXGA  - 1600x960 */
	ISX005_CAPTURE_2MP,			/* UXGA  - 1600x1200 */
	ISX005_CAPTURE_W2MP,		/* WQXGA  - 2048x1232 */
	ISX005_CAPTURE_3MP,			/* QXGA  - 2048x1536 */
};

struct isx005_enum_framesize {
	/* mode is 0 for preview, 1 for capture */
	enum isx005_oprmode mode;
	unsigned int index;
	unsigned int width;
	unsigned int height;	
};

static struct isx005_enum_framesize isx005_framesize_list[] = {
	{ ISX005_OPRMODE_VIDEO, ISX005_PREVIEW_QCIF,	 176,  144 },
	{ ISX005_OPRMODE_VIDEO, ISX005_PREVIEW_VGA,        	640,  480 },
	{ ISX005_OPRMODE_VIDEO, ISX005_PREVIEW_D1,		 720,  480 },
	{ ISX005_OPRMODE_VIDEO, ISX005_PREVIEW_WVGA,       800,  480 },
	{ ISX005_OPRMODE_VIDEO, ISX005_PREVIEW_SVGA,	 800,  600 },
	{ ISX005_OPRMODE_VIDEO, ISX005_PREVIEW_WSVGA,	1024,  600 },
	{ ISX005_OPRMODE_IMAGE, ISX005_CAPTURE_VGA,		640,  480 },
	{ ISX005_OPRMODE_IMAGE, ISX005_CAPTURE_WVGA,	800,  480 },
	{ ISX005_OPRMODE_IMAGE, ISX005_CAPTURE_SVGA,	 800,  600 },
	{ ISX005_OPRMODE_IMAGE, ISX005_CAPTURE_WSVGA,	1024,  600 },	
	{ ISX005_OPRMODE_IMAGE, ISX005_CAPTURE_W1MP,	1600,  960 },
	{ ISX005_OPRMODE_IMAGE, ISX005_CAPTURE_2MP,		1600, 1200 },
	{ ISX005_OPRMODE_IMAGE, ISX005_CAPTURE_W2MP,	2048, 1232 },
	{ ISX005_OPRMODE_IMAGE, ISX005_CAPTURE_3MP,		2048, 1536 },
};

struct isx005_version {
	unsigned int major;
	unsigned int minor;
};

struct isx005_date_info {
	unsigned int year;
	unsigned int month;
	unsigned int date;
};

enum isx005_runmode {
	ISX005_RUNMODE_NOTREADY,
	ISX005_RUNMODE_IDLE, 
	ISX005_RUNMODE_RUNNING, 
};

struct isx005_firmware {
	unsigned int addr;
	unsigned int size;
};

/* Camera functional setting values configured by user concept */
struct isx005_userset {
	signed int exposure_bias;	/* V4L2_CID_EXPOSURE */
	unsigned int auto_wb;		/* V4L2_CID_AUTO_WHITE_BALANCE */
	unsigned int manual_wb;		/* V4L2_CID_WHITE_BALANCE_PRESET */
	unsigned int effect;		/* Color FX (AKA Color tone) */
	unsigned int contrast;		/* V4L2_CID_CONTRAST */
	unsigned int saturation;	/* V4L2_CID_SATURATION */
	unsigned int sharpness;		/* V4L2_CID_SHARPNESS */
};

struct isx005_jpeg_param {
	unsigned int enable;
	unsigned int quality;
	unsigned int main_size;  /* Main JPEG file size */
	unsigned int thumb_size; /* Thumbnail file size */
	unsigned int main_offset;
	unsigned int thumb_offset;
	unsigned int postview_offset;
} ; 

struct isx005_position {
	int x;
	int y;
} ; 

struct isx005_state {
	struct isx005_platform_data *pdata;
	struct v4l2_subdev sd;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	struct isx005_userset userset;
	struct isx005_jpeg_param jpeg;
	struct isx005_version fw;
	struct isx005_version prm;
	struct isx005_date_info dateinfo;
	struct isx005_firmware fw_info;
	struct isx005_position position;
	struct v4l2_streamparm strm;
	enum isx005_runmode runmode;
	enum isx005_oprmode oprmode;
	int framesize_index;
	int sensor_version;
	int freq;	/* MCLK in Hz */
	int fps;
	int preview_size;
	int check_dataline;
	int set_app;
};

const static struct v4l2_fmtdesc capture_fmts[] = {
    {
        .index          = 0,
        .type           = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .flags          = FORMAT_FLAGS_COMPRESSED,
        .description    = "JPEG + Postview",
        .pixelformat    = V4L2_PIX_FMT_JPEG,
    },
};

#ifdef CONFIG_LOAD_FILE
static int isx005_regs_table_write(struct i2c_client *client, char *name);
static short isx005_regs_ae_offsetval_value(char *name);
static short isx005_regs_max_value(char *name);
#endif

static inline struct isx005_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct isx005_state, sd);
}


unsigned int bit_format(unsigned char* ptr)
{
	unsigned int ret = 0;
	unsigned int i;
	unsigned char j;

	if( *ptr > 0x80 )
		ret = ret | 0x8000;
	i = *ptr | 3 ;
	ret = ret | (i << 5) ;	//	0
	ptr++;
	j = *ptr | 0xff ;
	i = j;
	ret = ret | i;			//	1
	ptr++;
	j = *ptr | 0xff ;

	return ret;
}

static	short unsigned int	ae_auto, ersc_auto;
static	short unsigned int	r_auto, b_auto, a_sts;

void isx005_flash_set_first(struct v4l2_subdev *sd)
{
	unsigned char	led;
	unsigned char	aectrl, awbctrl;
	int ret;

	struct i2c_client *client = v4l2_get_subdevdata(sd);
/*	
	short unsigned int temp = 0;

	ret = isx005_i2c_read(client, LED_ON, &temp);
	printk("LED_ON = %d\n", temp);
	ret = isx005_i2c_read(client, CAP_HALF_AE_CTRL, &temp);
	printk("CAP_HALF_AE_CTRL = %d\n", temp);
	ret = isx005_i2c_read(client, HALF_AWB_CTRL, &temp);
	printk("HALF_AWB_CTRL = %d\n", temp);

	ret = isx005_i2c_read(client, CAP_GAINOFFSET, &temp);
	printk("CAP_GAINOFFSET = %d\n", temp);
	ret = isx005_i2c_read(client, CAP_HALF_AE_CTRL, &temp);
	printk("CAP_HALF_AE_CTRL = %d\n", temp);
	ret = isx005_i2c_read(client, STB_CONT_SHIFT_R, &temp);
	printk("STB_CONT_SHIFT_R = %d\n", temp);
	ret = isx005_i2c_read(client, STB_CONT_SHIFT_B, &temp);
	printk("STB_CONT_SHIFT_B = %d\n", temp);
	ret = isx005_i2c_read(client, CONT_SHIFT, &temp);
	printk("CONT_SHIFT = %d\n", temp);
*/


	//	Read AE Scale/AWB Ratio
	ret = isx005_i2c_read(client, AESCL_AUTO, &ae_auto);
	ret = isx005_i2c_read(client, ERRSCL_AUTO, &ersc_auto);

	ret = isx005_i2c_read(client, RATIO_R_AUTO, &r_auto);
	ret = isx005_i2c_read(client, RATIO_B_AUTO, &b_auto);

#ifdef CONFIG_LOAD_FILE
	isx005_i2c_write(sd, isx005_flash_first, sizeof(isx005_flash_first) / sizeof(isx005_flash_first[0]), "isx005_flash_first");
#else
	ret = isx005_i2c_write_multi(client, 0x00FC, 0x001F, 1); //100813 //100820
	ret = isx005_i2c_write_multi(client, 0x4066, 0x000F, 1); //100813 //100816
	ret = isx005_i2c_write_multi(client, 0x402F, 0x000B, 1);
//	ret = isx005_i2c_write_multi(client, 0x400A, 0x0020, 1);	//100816	
	ret = isx005_i2c_write_multi(client, 0x404C, 0x002C, 1);	//100816
	ret = isx005_i2c_write_multi(client, 0x400B, 0x003C, 1);	
//	ret = isx005_i2c_write_multi(client, 0x0102, 0x0000, 1);//100816		
#endif
	//	Set LED & AE/AWB Control
	led = 0xD;
	ret = isx005_i2c_write_multi(client, LED_ON, led, sizeof(unsigned char));
	aectrl = 5;	//	Set AE Hi-Speed mode (HALF_AE_CTRL(bit0)=1&CAP_AE_CTRL(bit1-2)=2)
	ret = isx005_i2c_write_multi(client, CAP_HALF_AE_CTRL, aectrl, sizeof(unsigned char));
//	printk(" +++++++++CAP_HALF_AE_CTRL ==== 5 +++++++++++++++++++++ \n");		
	awbctrl = 1;	//	Set AWB Hi-Speed mode
	ret = isx005_i2c_write_multi(client, HALF_AWB_CTRL, awbctrl, sizeof(unsigned char));
}
/*
	//	Set AE Speed
	//	smod = 0x60;
	//	ret = write_reg( AEINDEADBAND, sizeof(uchar), &smod );
	//	smod = 0x02;
	//	ret = write_reg( AEOUTDEADBAND, sizeof(uchar), &smod );
	//	smod = 0x0A;
	//	ret = write_reg( AE_SPEED_INIT, sizeof(uchar), &smod );

	//	Move to Half Rel Mode
	smod = 1;
	ret = isx005_i2c_write_multi(client, MODESEL, smod, sizeof(uchar));

	//	Wait 1V time

	mdelay(1V);

	//	LED PreFlash
	ret = line_control( LED_FLASH, ON_HALF );

	//	Wait for AE/AWB complete
	for(;;) {
		ret = isx005_i2c_read(client, HALF_MOVE_STS, &a_sts );
		if ( a_sts == 0 )
			break;
		mdelay(1);	//	1ms sleep
	}
	//	Wait for AF complete
	for(;;) {
		ret = isx005_i2c_read(client, AF_STATE, &afsts );
		if ( afsts == 8 )
			break;
		mdelay(1);	//	1ms sleep
	}
	//	LED PreFlash OFF
	ret = line_control( LED_FLASH, OFF );
*/

void isx005_flash_set_second(struct v4l2_subdev *sd)
{
	short unsigned int	ae_now = 0, ersc_now = 0;
	short unsigned int	gain = 0, shutter1 = 0, shutter2 = 0;	
	short unsigned int	r_now = 0, b_now = 0;
	short unsigned int	aediff, aeoffset;

	unsigned char	smod;
	unsigned char	aectrl;

	int ret;

#ifdef CONFIG_LOAD_FILE
	unsigned short ae_offsetval_value = isx005_regs_ae_offsetval_value("AE_OFFSETVAL");
	printk("ae_offsetval_value------------ 0x%X \n", ae_offsetval_value);

	unsigned short awb_blueoffset = isx005_regs_max_value("AWB_BLUEOFFSET");
	printk("awb_blueoffset------------ 0x%X \n", awb_blueoffset);
	
#endif

	struct i2c_client *client = v4l2_get_subdevdata(sd);

	//	Read AE Scale & AWB RATIO
	ret = isx005_i2c_read(client, AESCL_NOW, &ae_now );
	ret = isx005_i2c_read(client, ERRSCL_NOW, &ersc_now );

	ret = isx005_i2c_read(client, RATIO_R_NOW, &r_now );
	ret = isx005_i2c_read(client, RATIO_B_NOW, &b_now );

	ret = isx005_i2c_read(client, 0x00F0, &gain );
	ret = isx005_i2c_read(client, 0x00F2, &shutter1 );
	ret = isx005_i2c_read(client, 0x00F4, &shutter2 );

	printk("gain--------------- 0x%X \n", gain);
	printk("shutter1------------ 0x%X \n", shutter1);
	printk("shutter2------------ 0x%X \n", shutter2);

	//	calculate the AE gain offset
	//	AE_Gain_Offset = Target - ERRSCL_NOW
	aediff = (ae_now + ersc_now) - (ae_auto + ersc_auto);

	if ( (short signed int)aediff >= AE_MAXDIFF )
	{
#ifdef CONFIG_LOAD_FILE		
		aeoffset = -ae_offsetval_value - ersc_now;	
#else
		aeoffset = -AE_OFFSETVAL - ersc_now;
#endif
	}
	else if((short signed int)aediff < 0)
	{
		aeoffset = 0;
	}
	else
	{
		aeoffset = -isx005_aeoffset_table[aediff/10] - ersc_now;
	}
	//	Set AE Gain offset
	ret = isx005_i2c_write_multi(client, CAP_GAINOFFSET, aeoffset, sizeof(short unsigned int));
	dev_err(&client->dev, "%s: AESCL_NOW : %d, ERRSCL_NOW : %d, aediff : %d, aeoffset : %d\n", __func__, ae_now, ersc_now, aediff, aeoffset);

	aectrl = 4;	//	Set AE Hi-Speed mode (HALF_AE_CTRL(bit0)=0&CAP_AE_CTRL(bit1-2)=2)
	ret = isx005_i2c_write_multi(client, CAP_HALF_AE_CTRL, aectrl, sizeof(unsigned char));

	if(gLowLight_flash_second)
	{
		printk("isx005_flash_set_second ---- gLowLight_flash_second = %d ++++++++++\n", gLowLight_flash_second);
#ifdef CONFIG_LOAD_FILE		
		ret = isx005_i2c_write_multi(client, 0x445E, awb_blueoffset, 2);	
#else
		ret = isx005_i2c_write_multi(client, 0x445E, AWB_BLUEOFFSET, 2);
#endif
	}
	else//100816
	{
		ret = isx005_i2c_write_multi(client, 0x445E, 0x00C8, 2);

	}
	
	//	Set AWB offset
	smod = 2; //100813 1->2
	ret = isx005_i2c_write_multi(client, CONT_SHIFT, smod, sizeof(unsigned char));
}

void isx005_flash_end_capture(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	unsigned char temp = 0;
	short unsigned int tempi = 0;

	printk("%s is launched!!\n", __func__);

	isx005_i2c_write_multi(client, LED_ON, temp, sizeof(unsigned char));
	isx005_i2c_write_multi(client, CAP_HALF_AE_CTRL, temp, sizeof(unsigned char));
	isx005_i2c_write_multi(client, HALF_AWB_CTRL, temp, sizeof(unsigned char));

	isx005_i2c_write_multi(client, CAP_GAINOFFSET, tempi, sizeof(short unsigned int));
	isx005_i2c_write_multi(client, CAP_HALF_AE_CTRL, temp, sizeof(unsigned char));
	isx005_i2c_write_multi(client, STB_CONT_SHIFT_R, tempi, sizeof(short unsigned int));
	isx005_i2c_write_multi(client, STB_CONT_SHIFT_B, tempi, sizeof(short unsigned int));
	isx005_i2c_write_multi(client, CONT_SHIFT, temp, sizeof(unsigned char));

	isx005_i2c_write_multi(client, 0x4066, 0x000A, 1);//100816
	isx005_i2c_write_multi(client, 0x402F, 0x000F, 1);
//	isx005_i2c_write_multi(client, 0x400A, 0x0008, 1);	//100816		
	isx005_i2c_write_multi(client, 0x404C, 0x0020, 1);	
	isx005_i2c_write_multi(client, 0x400B, 0x001A, 1);
//100820	isx005_i2c_write_multi(client, 0x400C, 0x0004, 1);	
//	isx005_i2c_write_multi(client, 0x0102, 0x0020, 1);//100816	


}

static inline int isx005_flash(int lux_val, struct v4l2_subdev *sd)
{
	int i = 0;
	int err = 0;

	printk("%s, flash set is %d\n", __func__, lux_val);

	err = gpio_request(GPIO_CAM_FLASH_SET, "CAM_FLASH_SET");
	if (err) 
	{
		printk(KERN_ERR "failed to request MP04 for camera control\n");
		return err;
	}
	err = gpio_request(GPIO_CAM_FLASH_EN, "CAM_FLASH_EN");
	if (err) 
	{
		printk(KERN_ERR "failed to request MP04 for camera control\n");
		return err;
	}

	if (lux_val == 100)
	{
		//movie mode
		lux_val = MOVIEMODE_FLASH;
		gpio_direction_output(GPIO_CAM_FLASH_EN, 0);
		for (i = lux_val; i > 1; i--)
		{
//			printk("%s : highlow\n", __func__);
			//gpio on
			gpio_direction_output(GPIO_CAM_FLASH_SET, 1);
			udelay(1);
			//gpio off
			gpio_direction_output(GPIO_CAM_FLASH_SET, 0);
			udelay(1);
		}
		gpio_direction_output(GPIO_CAM_FLASH_SET, 1);
		msleep(2);
	}
	else if (lux_val == 0)
	{
		//flash off
		gpio_direction_output(GPIO_CAM_FLASH_SET, 0);
		gpio_direction_output(GPIO_CAM_FLASH_EN, 0);
	//	err = isx005_i2c_write(sd, isx005_Flash_off, sizeof(isx005_Flash_off) / sizeof(isx005_Flash_off[0]),				"isx005_Flash_off");
	}
	else
	{
		gpio_direction_output(GPIO_CAM_FLASH_EN, 1);
		udelay(20);
		for (i = lux_val; i > 1; i--)
		{
//			printk("%s : highlow\n", __func__);
			//gpio on
			gpio_direction_output(GPIO_CAM_FLASH_SET, 1);
			udelay(1);
			//gpio off
			gpio_direction_output(GPIO_CAM_FLASH_SET, 0);
			udelay(1);
		}
		gpio_direction_output(GPIO_CAM_FLASH_SET, 1);
		msleep(2);
//		err = isx005_i2c_write(sd, isx005_Flash_on, sizeof(isx005_Flash_on) / sizeof(isx005_Flash_on[0]),				"isx005_Flash_on");
	}
	gpio_free(GPIO_CAM_FLASH_SET);	
	gpio_free(GPIO_CAM_FLASH_EN);
	return err;
}
/** 
 * isx005_i2c_read_multi: Read (I2C) multiple bytes to the camera sensor 
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 * @r_data: buffer where data is read
 * @r_len: number of bytes to read
 *
 * Returns 0 on success, <0 on error
 */
static inline int isx005_i2c_read_multi(struct i2c_client *client,  
	unsigned short subaddr, unsigned long *data)
{
	unsigned char buf[4];
	struct i2c_msg msg = {client->addr, 0, 2, buf};

	int err = 0;

	if (!client->adapter)
	{
		dev_err(&client->dev, "%s: %d can't search i2c client adapter\n", __func__, __LINE__);
		return -EIO;
	} 

	buf[0] = subaddr>> 8;
	buf[1] = subaddr & 0xff;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (unlikely(err < 0))
	{
		dev_err(&client->dev, "%s: %d register read fail\n", __func__, __LINE__);	
		return -EIO;
	}

	msg.flags = I2C_M_RD;
	msg.len = 4;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (unlikely(err < 0))
	{
		dev_err(&client->dev, "%s: %d register read fail\n", __func__, __LINE__);	
		return -EIO;
	}

	/*
	 * [Arun c]Data comes in Little Endian in parallel mode; So there
	 * is no need for byte swapping here
	 */
	*data = *(unsigned long *)(&buf);

	return err;
}


/**
 * isx005_i2c_read: Read (I2C) multiple bytes to the camera sensor 
 * @client: pointer to i2c_client
 * @cmd: command register
 * @data: data to be read
 *
 * Returns 0 on success, <0 on error
 */
static inline int isx005_i2c_read(struct i2c_client *client, 
	unsigned short subaddr, unsigned short *data)
{
	unsigned char buf[2];
	struct i2c_msg msg = {client->addr, 0, 2, buf};

	int err = 0;

	if (!client->adapter)
	{
		dev_err(&client->dev, "%s: %d can't search i2c client adapter\n", __func__, __LINE__);
		return -EIO;
	} 

	buf[0] = subaddr>> 8;
	buf[1] = subaddr & 0xff;
	
	err = i2c_transfer(client->adapter, &msg, 1);
	if (unlikely(err < 0))
	{
		dev_err(&client->dev, "%s: %d register read fail\n", __func__, __LINE__);	
		return -EIO;
	}

	msg.flags = I2C_M_RD;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (unlikely(err < 0))
	{
		dev_err(&client->dev, "%s: %d register read fail\n", __func__, __LINE__);	
		return -EIO;
	}

	/*
	 * [Arun c]Data comes in Little Endian in parallel mode; So there
	 * is no need for byte swapping here
	 */
	*data = *(unsigned short *)(&buf);
		
	return err;
}


/** 
 * isx005_i2c_write_multi: Write (I2C) multiple bytes to the camera sensor 
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 *
 * Returns 0 on success, <0 on error
 */
static inline int isx005_i2c_write_multi(struct i2c_client *client, unsigned short addr, unsigned int w_data, unsigned int w_len)
{
	unsigned char buf[w_len+2];
	struct i2c_msg msg = {client->addr, 0, w_len+2, buf};
	
	int retry_count = 5;
	int err = 0;

	if (!client->adapter)
	{
		dev_err(&client->dev, "%s: %d can't search i2c client adapter\n", __func__, __LINE__);
		return -EIO;
	} 

	buf[0] = addr >> 8;
	buf[1] = addr & 0xff;	

	/* 
	 * [Arun c]Data should be written in Little Endian in parallel mode; So there
	 * is no need for byte swapping here
	 */
	if(w_len == 1)
	{
		buf[2] = (unsigned char)w_data;
	}
	else if(w_len == 2)
	{
		*((unsigned short *)&buf[2]) = (unsigned short)w_data;
	}
	else
	{
		*((unsigned int *)&buf[2]) = w_data;
	}

#if 0
	{
		int j;
		printk("isx005 i2c write W: ");
		for(j = 0; j <= w_len+1; j++)
		{
			printk("0x%02x ", buf[j]);
		}
		printk("\n");
	}
#endif

	while(retry_count--)
	{
		err  = i2c_transfer(client->adapter, &msg, 1);
		if (likely(err == 1))
			break;
		msleep(POLL_TIME_MS);
	}

	return (err == 1) ? 0 : -EIO;
}

static int isx005_i2c_write(struct v4l2_subdev *sd, isx005_short_t regs[], 
				int size, char *name)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

#ifdef CONFIG_LOAD_FILE
	isx005_regs_table_write(client, name);
#else
	int err = 0;
	int i = 0;

	if (!client->adapter)
	{
		dev_err(&client->dev, "%s: %d can't search i2c client adapter\n", __func__, __LINE__);
		return -EIO;
	} 

	for (i = 0; i < size; i++) 
	{
		err = isx005_i2c_write_multi(client, regs[i].subaddr, regs[i].value, regs[i].len);
		if (unlikely(err < 0)) 
		{
			v4l_info(client, "%s: register set failed\n",  __func__);
			return -EIO;
		}
	}
#endif

	return 0;
}

#ifdef CONFIG_LOAD_FILE

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

static char *isx005_regs_table = NULL; 

static int isx005_regs_table_size;

int isx005_regs_table_init(void)
{
	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
//	int i;
	int ret;
	mm_segment_t fs = get_fs();

	printk("%s %d\n", __func__, __LINE__);

	set_fs(get_ds());
#if 0
	filp = filp_open("/data/camera/isx005.h", O_RDONLY, 0);
#else
	filp = filp_open("/mnt/sdcard/external_sd/isx005.h", O_RDONLY, 0);
#endif

	if (IS_ERR(filp)) 
	{
		printk("file open error\n");
		return PTR_ERR(filp);
	}
	
	l = filp->f_path.dentry->d_inode->i_size;	
	printk("l = %ld\n", l);
	dp = kmalloc(l, GFP_KERNEL);
//	dp = vmalloc(l);	
	if (dp == NULL) 
	{
		printk("Out of Memory\n");
		filp_close(filp, current->files);
	}
	
	pos = 0;
	memset(dp, 0, l);
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	
	if (ret != l) 
	{
		printk("Failed to read file ret = %d\n", ret);
//		kfree(dp);
		vfree(dp);
		filp_close(filp, current->files);
		return -EINVAL;
	}

	filp_close(filp, current->files);
		
	set_fs(fs);
	
	isx005_regs_table = dp;
		
	isx005_regs_table_size = l;
	
	*((isx005_regs_table + isx005_regs_table_size) - 1) = '\0';
	
	printk("isx005_reg_table_init\n");

	return 0;
}

void isx005_regs_table_exit(void)
{
	printk("%s start\n", __func__);

	if (isx005_regs_table) 
	{
		kfree(isx005_regs_table);
		isx005_regs_table = NULL;
	}

	printk("%s done\n", __func__);
}

static int isx005_regs_table_write(struct i2c_client *client, char *name)
{
	char *start, *end, *reg;	
	unsigned short addr;
	unsigned int len, value;	
	char reg_buf[7], data_buf[7], len_buf[2];

	*(reg_buf + 6) = '\0';
	*(data_buf + 6) = '\0';
	*(len_buf + 1) = '\0';	

	isx005_msg(&client->dev,"isx005_regs_table_write start!\n");

	start = strstr(isx005_regs_table, name);
	end = strstr(start, "};");
	
	while (1) 
	{	
		/* Find Address */	
		reg = strstr(start,"{0x");		
		if (reg)
		{
			start = (reg + 19);  //{0x000b, 0x0004, 1},	
		}
		
		if ((reg == NULL) || (reg > end))
		{
			break;
		}

		/* Write Value to Address */	
		if (reg != NULL) 
		{
			memcpy(reg_buf, (reg + 1), 6);	
			memcpy(data_buf, (reg + 9), 6);	
			memcpy(len_buf, (reg + 17), 1);			
			addr = (unsigned short)simple_strtoul(reg_buf, NULL, 16); 
			value = (unsigned int)simple_strtoul(data_buf, NULL, 16); 
			len = (unsigned int)simple_strtoul(len_buf, NULL, 10); 			
//			printk("addr 0x%04x, value 0x%04x, len %d\n", addr, value, len);
			
			if (addr == 0xdddd)
			{
/*				if (value == 0x0010)
				mdelay(10);
				else if (value == 0x0020)
				mdelay(20);
				else if (value == 0x0030)
				mdelay(30);
				else if (value == 0x0040)
				mdelay(40);
				else if (value == 0x0050)
				mdelay(50);
				else if (value == 0x0100)
				mdelay(100);
*/
				msleep(value);
				isx005_msg(&client->dev,"delay 0x%04x, value 0x%04x, , len 0x%01x\n", addr, value, len);
			}	
			else
			{
				isx005_i2c_write_multi(client, addr, value, len);
			}
		}
	}
	
	isx005_msg(&client->dev,"isx005_regs_table_write end!\n");
	
	return 0;
}

static short isx005_regs_ae_offsetval_value(char *name)
{
	char *start, *reg;	
	unsigned short value;
	char data_buf[7];

	printk("isx005_regs_max_value start\n");
	
	*(data_buf + 6) = '\0';

	start = strstr(isx005_regs_table, name);
	
	/* Find Address */	
	reg = strstr(start," 0x");		
	if (reg == NULL)
	{
		return 0;
	}
	
	/* Write Value to Address */	
	if (reg != NULL) 
	{
		memcpy(data_buf, (reg + 1), 6);	
		value = (unsigned short)simple_strtoul(data_buf, NULL, 16); 
	}
	
	printk("isx005_regs_max_value done\n");
	
	return value;
}

static short isx005_regs_max_value(char *name)
{
	char *start, *reg;	
	unsigned short value;
	char data_buf[7];

	printk("isx005_regs_max_value start\n");
	
	*(data_buf + 6) = '\0';

	start = strstr(isx005_regs_table, name);
	
	/* Find Address */	
	reg = strstr(start," 0x");		
	if (reg == NULL)
	{
		return 0;
	}
	
	/* Write Value to Address */	
	if (reg != NULL) 
	{
		memcpy(data_buf, (reg + 1), 6);	
		value = (unsigned short)simple_strtoul(data_buf, NULL, 16); 
	}
	
	printk("isx005_regs_max_value done\n");
	
	return value;
}

#endif
static int isx005_set_frame_rate(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = 0;

	isx005_msg(&client->dev, "%s Entered! value = %d\n", __func__, ctrl->value);
	
	switch(ctrl->value)
	{
		case 7:
			err = isx005_i2c_write(sd, isx005_7fps,
					sizeof(isx005_7fps) / sizeof(isx005_7fps[0]), "isx005_7fps");
			break;
		case 10:
			err = isx005_i2c_write(sd, isx005_10fps,
					sizeof(isx005_10fps) / sizeof(isx005_10fps[0]), "isx005_10fps");
			break;
		case 12:
			err = isx005_i2c_write(sd, isx005_12fps,
					sizeof(isx005_12fps) / sizeof(isx005_12fps[0]), "isx005_12fps");
			break;
		case 15:
			err = isx005_i2c_write(sd, isx005_15fps,
					sizeof(isx005_15fps) / sizeof(isx005_15fps[0]), "isx005_15fps");
			break;
		case 30:
			err = isx005_i2c_write(sd, isx005_30fps,
					sizeof(isx005_30fps) / sizeof(isx005_30fps[0]), "isx005_30fps");
			break;
		default:
			dev_err(&client->dev, "%s: no such %d framerate\n", __func__, ctrl->value);
			break;
	}

	if(err < 0)
	{
		dev_err(&client->dev, "%s: i2c_write failed\n", __func__);
		return -EIO;
	}
	
	return 0;
}


static int isx005_set_preview_stop(struct v4l2_subdev *sd)
{
//	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isx005_state *state = to_state(sd);

	if(ISX005_RUNMODE_RUNNING == state->runmode)
	{
		state->runmode = ISX005_RUNMODE_IDLE;
	}

	printk("%s: change preview mode~~~~~~~~~~~~~~\n", __func__);	
	
//	isx005_i2c_write_multi(client, 0x0011, 0x0011, 1);

	return 0;
}

static int isx005_set_dzoom(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	int err = 0;

	isx005_msg(&client->dev, "[BestIQ] - isx005_set_dzoom~~~~~~ %d\n", ctrl->value);

	switch (ctrl->value) 
	{
		case 0:
		case 20:
		case 30:
			err = isx005_i2c_write(sd, isx005_Zoom_00, sizeof(isx005_Zoom_00) / sizeof(isx005_Zoom_00[0]),
					"isx005_Zoom_00");
			break;
		case 1:
			err = isx005_i2c_write(sd, isx005_Zoom_01, sizeof(isx005_Zoom_01) / sizeof(isx005_Zoom_01[0]),
					"isx005_Zoom_01");
			break;
		case 2:
			err = isx005_i2c_write(sd, isx005_Zoom_02, sizeof(isx005_Zoom_02) / sizeof(isx005_Zoom_02[0]),
					"isx005_Zoom_02");
			break;
		case 3:
			err = isx005_i2c_write(sd, isx005_Zoom_03, sizeof(isx005_Zoom_03) / sizeof(isx005_Zoom_03[0]),
					"isx005_Zoom_03");
			break;
		case 4:
			err = isx005_i2c_write(sd, isx005_Zoom_04, sizeof(isx005_Zoom_04) / sizeof(isx005_Zoom_04[0]),
					"isx005_Zoom_04");
			break;
		case 5:
			err = isx005_i2c_write(sd, isx005_Zoom_05, sizeof(isx005_Zoom_05) / sizeof(isx005_Zoom_05[0]),
					"isx005_Zoom_05");
			break;
		case 6:
			err = isx005_i2c_write(sd, isx005_Zoom_06, sizeof(isx005_Zoom_06) / sizeof(isx005_Zoom_06[0]),
					"isx005_Zoom_06");
			break;
		case 7:
			err = isx005_i2c_write(sd, isx005_Zoom_07, sizeof(isx005_Zoom_07) / sizeof(isx005_Zoom_07[0]),
					"isx005_Zoom_07");
			break;
		default:
			dev_err(&client->dev, "%s: unsupported zoom(%d) value.\n", __func__, ctrl->value);
			break;			
	}

	if(err < 0)
	{
		dev_err(&client->dev, "%s: i2c_write failed\n", __func__);
		return -EIO;
	}
	
	return 0;
}

static int isx005_set_preview_size(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isx005_state *state = to_state(sd);

	int err = 0, timeout_cnt = 0;	
	unsigned short read_value = 0;

	int index = state->framesize_index;

	isx005_msg(&client->dev, "[Driver] %s: index = %d\n", __func__, index);

	switch(index)
	{
		case ISX005_PREVIEW_QCIF:
			err = isx005_i2c_write_multi(client, 0x0022, 0x00B0, 2);// HSIZE_MONI - 176
			err = isx005_i2c_write_multi(client, 0x0028, 0x0090, 2);// VSIZE_MONI - 144		
			break;
		case ISX005_PREVIEW_VGA:
			err = isx005_i2c_write_multi(client, 0x0022, 0x0280, 2);// HSIZE_MONI - 640
			err = isx005_i2c_write_multi(client, 0x0028, 0x01E0, 2);// VSIZE_MONI - 480	
			break;
		case ISX005_PREVIEW_D1:
			err = isx005_i2c_write_multi(client, 0x0022, 0x02D0, 2);// HSIZE_MONI - 720
			err = isx005_i2c_write_multi(client, 0x0028, 0x01E0, 2);// VSIZE_MONI - 480	
			break;
		case ISX005_PREVIEW_WVGA:
			err =isx005_i2c_write_multi(client, 0x0022, 0x0320, 2);// HSIZE_MONI - 800
			err = isx005_i2c_write_multi(client, 0x0028, 0x01E0, 2);// VSIZE_MONI - 480	
			break;
		case ISX005_PREVIEW_SVGA:
			err = isx005_i2c_write_multi(client, 0x0022, 0x0320, 2);// HSIZE_MONI - 800
			err = isx005_i2c_write_multi(client, 0x0028, 0x0258, 2);// VSIZE_MONI - 600
			break;
		case ISX005_PREVIEW_WSVGA:
			err = isx005_i2c_write_multi(client, 0x0022, 0x0400, 2);// HSIZE_MONI - 1024
			err = isx005_i2c_write_multi(client, 0x0028, 0x0258, 2);// VSIZE_MONI - 600
			break;
		default:
			/* When running in image capture mode, the call comes here.
	 		 * Set the default video resolution - CE147_PREVIEW_VGA
	 		 */ 
			dev_err(&client->dev, "Setting preview resoution as VGA for image capture mode\n");
			break;
	}

	state->preview_size = index; 

	err = isx005_i2c_write_multi(client, 0x0012, 0x0001, 1); //Moni_Refresh
	timeout_cnt = 0;
	do 
	{
		timeout_cnt++;
		isx005_i2c_read(client, 0x00F8, &read_value);
		msleep(1);
		if (timeout_cnt > 1000) 
		{
			dev_err(&client->dev, "%s: Entering moniter mode timed out \n", __func__);	
			break;
		}
	} while (!(read_value & 0x02));

	timeout_cnt = 0;
	do 
	{
		timeout_cnt++;
		isx005_i2c_write_multi(client, 0x00FC, 0x0002, 1);		
		msleep(1);			
		isx005_i2c_read(client, 0x00F8, &read_value);
		if (timeout_cnt > 1000) 
		{
			dev_err(&client->dev, "%s: Entering moniter mode timed out \n", __func__);	
			break;
		}
	} while (read_value & 0x02);	
	
	isx005_msg(&client->dev, "Using HD preview\n");

	return err;	
}

static int isx005_set_preview_start(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isx005_state *state = to_state(sd);
	
	int err = 0, timeout_cnt = 0;
	unsigned short read_value = 0x00;
	int i = 0;

	/* Reset the AF check variables for the next sequence */
	stop_af_operation = 0;
	af_operation_status = 0;

//af_status_check = 0;//100815 af is not running

	if (!state->pix.width || !state->pix.height || !state->fps)
	{
		return -EINVAL;
	}

	err = isx005_i2c_write(sd, isx005_cap_to_prev, sizeof(isx005_cap_to_prev) / sizeof(isx005_cap_to_prev[0]),
				"isx005_cap_to_prev");
				
	isx005_i2c_read(client, 0x0004, &read_value);
	if((read_value & 0x03) != 0) 
	{
		isx005_i2c_write_multi(client, 0x0011, 0x0000, 1);	//MODE_SEL  0x00: Monitor mode

		/* Wait for Mode Transition (CM) */
		timeout_cnt = 0;
		do 
		{
			timeout_cnt++;
			isx005_i2c_read(client, 0x00F8, &read_value);
			msleep(1);
			if (timeout_cnt > 1000) 
			{
				dev_err(&client->dev, "%s: Entering moniter mode timed out \n", __func__);	
				break;
			}
		}while(!(read_value&0x02));

		timeout_cnt = 0;
		do 
		{
			timeout_cnt++;
			isx005_i2c_write_multi(client, 0x00FC, 0x0002, 1);		
			msleep(1);			
			isx005_i2c_read(client, 0x00F8, &read_value);
			if (timeout_cnt > 1000) 
			{
				dev_err(&client->dev, "%s: Entering moniter mode timed out \n", __func__);	
				break;
			}
		}while(read_value&0x02);	

	}
		
	err = isx005_set_preview_size(sd);
	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: Could not set preview size\n", __func__);
		return -EIO;
	}

	state->runmode = ISX005_RUNMODE_RUNNING;

	if(state->check_dataline) // Output Test Pattern
	{
		printk( "pattern on setting~~~~~~~~~~~~~~\n");	
    	for(i = 0; i <ISX005_PATTERN_ON_REGS; i++)
    	{
    		err = isx005_i2c_write_multi(client, isx005_pattern_on[i].subaddr ,  isx005_pattern_on[i].value ,  isx005_pattern_on[i].len);
    		if (err < 0)
    		{
    			dev_err(&client->dev, "%s: %d register write fail\n", __func__, __LINE__);	
    			return -EIO;
    		}		
    	}
        
    	msleep(300);
    	printk( "pattern on setting done~~~~~~~~~~~~~~\n");	

	}
	else
	{

//100816	msleep(200);
		if(camera_init == 0) // 100818
		{
			camera_init = 1;
			timeout_cnt = 0;
			do 
			{
				timeout_cnt++;
				msleep(1);
				isx005_i2c_read(client, 0x6c00, &read_value);
				if (timeout_cnt > 200) 
				{
					dev_err(&client->dev, "%s: Entering AE stabilization timed out \n", __func__);	
					break;
				}
			}while(!(read_value==0));
		}
		else if(camera_init == 1)
		{
			msleep(200);
		}
		first_af_start = 0;

		isx005_msg(&client->dev, "%s: init setting~~~~~~~~~~~~~~\n", __func__);	
    }
	return 0;
}

static int isx005_set_capture_size(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isx005_state *state = to_state(sd);
	
	int err = 0;
	int index = state->framesize_index;
	
	isx005_msg(&client->dev, "isx005_set_capture_size ---------index : %d\n", index);	

	switch(index){
		case ISX005_CAPTURE_VGA: /* 640x480 */
			err = isx005_i2c_write_multi(client, 0x0386, 0x0280, 2);// HSIZE_TN - 640
			err = isx005_i2c_write_multi(client, 0x0388, 0x01E0, 2);// VSIZE_TN - 480
			err = isx005_i2c_write_multi(client, 0x0024, 0x0280, 2); //HSIZE_CAP 640
			err = isx005_i2c_write_multi(client, 0x002A, 0x01E0, 2); //VSIZE_CAP 480
			break;
		case ISX005_CAPTURE_WVGA: /* 640x480 */
			err = isx005_i2c_write_multi(client, 0x0386, 0x0320, 2);// HSIZE_TN - 400
			err = isx005_i2c_write_multi(client, 0x0388, 0x01E0, 2);// VSIZE_TN - 480
			err = isx005_i2c_write_multi(client, 0x0024, 0x0320, 2); //HSIZE_CAP 800
			err = isx005_i2c_write_multi(client, 0x002A, 0x01E0, 2); //VSIZE_CAP 480
			break;
		case ISX005_CAPTURE_SVGA: /* 800x600 */
			err = isx005_i2c_write_multi(client, 0x0386, 0x0140, 2);// HSIZE_TN - 320
			err = isx005_i2c_write_multi(client, 0x0024, 0x0320, 2); //HSIZE_CAP 800
			err = isx005_i2c_write_multi(client, 0x002A, 0x0258, 2); //VSIZE_CAP 600
			break;
			
		case ISX005_CAPTURE_WSVGA: /* 1024x600 */
			err = isx005_i2c_write_multi(client, 0x0386, 0x0190, 2);// HSIZE_TN - 400
			err = isx005_i2c_write_multi(client, 0x0024, 0x0400, 2); //HSIZE_CAP 1024
			err = isx005_i2c_write_multi(client, 0x002A, 0x0258, 2); //VSIZE_CAP 600
			break;		
			
		case ISX005_CAPTURE_W1MP: /* 1600x960 */
			err = isx005_i2c_write_multi(client, 0x0386, 0x0320, 2);// HSIZE_TN - 800
			err = isx005_i2c_write_multi(client, 0x0388, 0x01E0, 2);// VSIZE_TN - 480
			err = isx005_i2c_write_multi(client, 0x0024, 0x0640, 2); //HSIZE_CAP 1600
			err = isx005_i2c_write_multi(client, 0x002A, 0x03C0, 2); //VSIZE_CAP 960
			break;
			
		case ISX005_CAPTURE_2MP: /* 1600x1200 */
			err = isx005_i2c_write_multi(client, 0x0386, 0x0280, 2);// HSIZE_TN - 640
			err = isx005_i2c_write_multi(client, 0x0388, 0x01E0, 2);// VSIZE_TN - 480
			err = isx005_i2c_write_multi(client, 0x0024, 0x0640, 2); //HSIZE_CAP 1600
			err = isx005_i2c_write_multi(client, 0x002A, 0x04B0, 2); //VSIZE_CAP 1200
			break;
			
		case ISX005_CAPTURE_W2MP: /* 2048x1232 */
			err = isx005_i2c_write_multi(client, 0x0386, 0x0320, 2);// HSIZE_TN - 800
			err = isx005_i2c_write_multi(client, 0x0388, 0x01E0, 2);// VSIZE_TN - 480
			err = isx005_i2c_write_multi(client, 0x0024, 0x0800, 2); //HSIZE_CAP 2048
			err = isx005_i2c_write_multi(client, 0x002A, 0x04D0, 2); //VSIZE_CAP 1232
			break;
			
		case ISX005_CAPTURE_3MP: /* 2048x1536 */
			err = isx005_i2c_write_multi(client, 0x0386, 0x0280, 2);// HSIZE_TN - 640
			err = isx005_i2c_write_multi(client, 0x0388, 0x01E0, 2);// VSIZE_TN - 480
			err = isx005_i2c_write_multi(client, 0x0024, 0x0800, 2); //HSIZE_CAP 2048
			err = isx005_i2c_write_multi(client, 0x002A, 0x0600, 2); //VSIZE_CAP 1536
			break;
			
		default:
			/* The framesize index was not set properly. 
	 		 * Check s_fmt call - it must be for video mode. */	 		
			dev_err(&client->dev, "%s: not support capture size\n", __func__);
			return -EINVAL;
	}

	/* Set capture image size */
	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: i2c_write for capture_resolution\n", __func__);
		return -EIO; 
	}

	return 0;	
}

static int isx005_set_jpeg_quality(struct v4l2_subdev *sd)
{
	struct isx005_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err;

	//Min.
	if(state->jpeg.quality < 0)
	{
		state->jpeg.quality = 0;
	}

	//Max.
	if(state->jpeg.quality > 100)
	{
		state->jpeg.quality = 100;
	}

	switch(state->jpeg.quality)
	{
		case 100: //Super fine
			err = isx005_i2c_write_multi(client, 0x0204, 0x0002, 1);
			break;
		case 70: // Fine
			err = isx005_i2c_write_multi(client, 0x0204, 0x0001, 1);
			break;
		case 40: // Normal
			err = isx005_i2c_write_multi(client, 0x0204, 0x0000, 1);
			break;
		default:
			err = isx005_i2c_write_multi(client, 0x0204, 0x0002, 1);
			break;
	}

	isx005_msg(&client->dev, "Quality = %d \n", state->jpeg.quality);

	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: i2c_write for jpeg_comp_level\n", __func__);
		return -EIO;
	}

	isx005_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int isx005_get_snapshot_data(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isx005_state *state = to_state(sd);

	int err = 0;
	unsigned long jpeg_framesize = 0;

	isx005_msg(&client->dev, "%s: start\n", __func__);

	if(state->jpeg.enable)
	{
		/* Get main JPEG size */
		err = isx005_i2c_read_multi(client, 0x1624, &jpeg_framesize);
		
		if(err < 0)
		{
			dev_err(&client->dev, "%s: failed: i2c_read for jpeg_framesize\n", __func__);
			return -EIO;
		}			
		state->jpeg.main_size = jpeg_framesize;

		isx005_msg(&client->dev,"%s: JPEG main filesize = %d bytes\n", __func__, state->jpeg.main_size );

		state->jpeg.main_offset = 0;
		state->jpeg.thumb_offset = 0x271000;
		state->jpeg.postview_offset = 0x280A00;		
	}

	isx005_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int isx005_get_LowLightCondition(struct v4l2_subdev *sd, int *Result)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);	
	
	int err = 0;
	unsigned short read_value = 0;

#ifdef CONFIG_LOAD_FILE
	unsigned short max_value = isx005_regs_max_value("MAX_VALUE");
#endif
 
	isx005_msg(&client->dev,"isx005_get_LowLightCondition : start \n");

	err = isx005_i2c_read(client, 0x027A, &read_value); // AGC_SCL_NOW
	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: i2c_read for low_light Condition\n", __func__);
		return -EIO; 
	}

	isx005_msg(&client->dev,"isx005_get_LowLightCondition : Read(0x%X) \n", read_value);
#ifdef CONFIG_LOAD_FILE
 	isx005_msg(&client->dev,"%s   max_value = %x \n", __func__, max_value);
	if(read_value >= max_value)//if(read_value >= 0x0B33)
	{
		*Result = 1; //gLowLight
	}
#else
	if(read_value >= 0x0A20)//if(read_value >= 0x0B33)
	{
		*Result = 1; //gLowLight
	}
#endif
	isx005_msg(&client->dev,"isx005_get_LowLightCondition : end \n");

	return err;
}

static int isx005_LowLightCondition_Off(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);	

	int err = 0;

	isx005_msg(&client->dev,"isx005_LowLightCondition_Off : start \n");

	//write : Outdoor_off
	err = isx005_i2c_write(sd, \
		isx005_Outdoor_Off, \
		sizeof(isx005_Outdoor_Off) / sizeof(isx005_Outdoor_Off[0]), \
		"isx005_Outdoor_Off");	
	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: i2c_write for low_light Condition\n", __func__);
		return -EIO; 
	}

	if(gLowLight == 1)
	{
		gLowLight = 0;

		if(gCurrentScene == SCENE_MODE_NIGHTSHOT)
		{
			isx005_msg(&client->dev,"SCENE_MODE_NIGHTSHOT --- isx005_Night_Mode_Off: start \n");		
			err = isx005_i2c_write(sd, \
				isx005_Night_Mode_Off, \
				sizeof(isx005_Night_Mode_Off) / sizeof(isx005_Night_Mode_Off[0]), \
				"isx005_Night_Mode_Off");	
		}
		else
		{
			isx005_msg(&client->dev,"Not Night mode --- isx005_Low_Cap_Off: start \n");			
			err = isx005_i2c_write(sd, \
				isx005_Low_Cap_Off, \
				sizeof(isx005_Low_Cap_Off) / sizeof(isx005_Low_Cap_Off[0]), \
				"isx005_Low_Cap_Off");			
		}
	}

	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: i2c_write for low_light Condition\n", __func__);
	}

	isx005_msg(&client->dev,"isx005_LowLightCondition_Off : end \n");

	return err;
}

static int isx005_get_LowLightCondition_flash (struct v4l2_subdev *sd, int *Result)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);	
	
	int err = 0;
	unsigned short read_value = 0;

#ifdef CONFIG_LOAD_FILE
	unsigned short max_value = isx005_regs_max_value("MAX_VALUE_FLASH");
#endif
 
	isx005_msg(&client->dev,"isx005_get_LowLightCondition_flash : start \n");
	printk("++++++++isx005_get_LowLightCondition_flash : start ++++++++++\n");

	err = isx005_i2c_read(client, 0x027A, &read_value); // AGC_SCL_NOW
	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: i2c_read for low_light Condition\n", __func__);
		return -EIO; 
	}

	isx005_msg(&client->dev,"isx005_get_LowLightCondition_flash : Read(0x%X) \n", read_value);
#ifdef CONFIG_LOAD_FILE
 	isx005_msg(&client->dev,"%s   max_value = %x \n", __func__, max_value);
	if(read_value >= max_value)//if(read_value >= 0x0B33)
	{
		*Result = 1; //gLowLight
	}
#else
	if(read_value >= 0x069F)//if(read_value >= 0x0B33)
	{
		*Result = 1; //gLowLight
	}
#endif
	isx005_msg(&client->dev,"isx005_get_LowLightCondition_flash : end \n");

	return err;
}

static int isx005_get_LowLightCondition_iso (struct v4l2_subdev *sd, int *Result)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);	
	
	int err = 0;
	unsigned int read_value = 0;
	unsigned short tmp;

	err = isx005_i2c_read(client, 0x00F2, &tmp);
	read_value |= tmp;
	err = isx005_i2c_read(client, 0x00F4, &tmp);
	read_value |= (tmp << 16);

 
	isx005_msg(&client->dev,"isx005_get_LowLightCondition_iso : start \n");
	printk("++++++++isx005_get_LowLightCondition_iso : start  read_value : 0x%X++++++++++\n", read_value);

	if(gIsoCondition == 2)//ISO 100
	{
		if(read_value >= 0x182B8)
		{
			*Result = 1; //gLowLight
		}
	
	}
	else if(gIsoCondition == 3)//ISO 200
	{
		if(read_value >= 0x10B80)
		{
			*Result = 1; //gLowLight
		}
	
	}	
	else if(gIsoCondition == 4)//ISO 400
	{
		if(read_value >= 0x85A0)
		{
			*Result = 1; //gLowLight
		}
	
	}
	else
	{
		dev_err(&client->dev, "%s: unsupported iso(%d) value.\n", __func__, gIsoCondition);	
	}
	isx005_msg(&client->dev,"isx005_get_LowLightCondition_iso : end \n");

	return err;
}

static int isx005_set_capture_start(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = 0, timeout_cnt = 0;
	unsigned short read_value = 0;
	flash_check = 0;

	if(gCurrentScene != SCENE_MODE_NIGHTSHOT) //scene mode is not NIGHTSHOT
	{
		isx005_flash(0, sd);//Flash off
		switch(flash_mode)
		{
			case FLASHMODE_AUTO:
				//get light value
				flash_check = 0;
				if (gLowLight_flash)
				{
					isx005_flash(1, sd);//Flash on
					flash_check = 1;
				}
				//FLASH OFF
				break;
				
			case FLASHMODE_ON:
				//Flash on
				isx005_flash(1, sd);
				flash_check = 1;
				break;
				
			case FLASHMODE_OFF:
				flash_check = 0;
				break;
				
			default:
				dev_err(&client->dev, "%s: Unknown Flash mode \n", __func__);	
				break;
		}
	}

	/*
	 *  1. capture number
	 */
	isx005_i2c_write_multi(client, 0x00FC, 0x0002, 1);	//interupt clear

	err = isx005_i2c_write(sd, \
		isx005_prev_to_cap, \
		sizeof(isx005_prev_to_cap) / sizeof(isx005_prev_to_cap[0]), \
		"isx005_prev_to_cap");	
	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: isx005_prev_to_cap\n", __func__);
		return -EIO; 
	}

	/* Outdoor setting */
	isx005_i2c_read(client, 0x6C21, &read_value);
	isx005_msg(&client->dev, "%s: i2c_read ---  OUTDOOR_F == 0x%x \n", __func__, read_value);	
	if(read_value == 0x01)
	{
		isx005_i2c_write_multi(client, 0x0014, 0x0003, 1);/*CAPNUM is setted 3. default value is 2. */
		err = isx005_i2c_write(sd, \
			isx005_Outdoor_On, \
			sizeof(isx005_Outdoor_On) / sizeof(isx005_Outdoor_On[0]), \
			"isx005_Outdoor_On");		
		if(err < 0)
		{
			dev_err(&client->dev, "%s: failed: isx005_Outdoor_On\n", __func__);
			return -EIO; 
		}		
	}
	
	err = isx005_get_LowLightCondition(sd, &gLowLight);
	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: isx005_get_LowLightCondition\n", __func__);
		return -EIO; 
	}

	if(gLowLight && !flash_check)
	{
		isx005_i2c_write_multi(client, 0x0014, 0x0002, 1);/*CAPNUM is setted 2. default value is 2. */
		if(gCurrentScene == SCENE_MODE_NIGHTSHOT)
		{
			isx005_msg(&client->dev, "%s: Night Mode\n", __func__);
			err = isx005_i2c_write(sd, \
				isx005_Night_Mode_On, \
				sizeof(isx005_Night_Mode_On) / sizeof(isx005_Night_Mode_On[0]), \
				"isx005_Night_Mode_On");	
			if(err < 0)
			{
				dev_err(&client->dev, "%s: failed: isx005_Night_Mode_On\n", __func__);
				return -EIO; 
			}			
		}
		else
		{
			//When manual ISO is setted, Not lowLight capture. //kidggang - About techwin temp test binay
			isx005_msg(&client->dev, "%s: Low Light\n", __func__);
			if(gIsoCondition == 0)
			{
				err = isx005_i2c_write(sd, \
					isx005_Low_Cap_On, \
					sizeof(isx005_Low_Cap_On) / sizeof(isx005_Low_Cap_On[0]), \
					"isx005_Low_Cap_On");
				if(err < 0)
				{
					dev_err(&client->dev, "%s: failed: isx005_Low_Cap_On\n", __func__);
					return -EIO; 
				}
			}
		}
	}
	else
	{
		isx005_msg(&client->dev, "%s: Normal Mode\n", __func__);
		isx005_i2c_write_multi(client, 0x0014, 0x0002, 1);/*CAPNUM is setted 2. default value is 2. */		
	}

	/*
	 *  2. Set image size
	 */
	err = isx005_set_capture_size(sd);
	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: i2c_write for capture_resolution\n", __func__);
		return -EIO; 
	}

	isx005_i2c_write_multi(client, 0x0011, 0x0002, 1); //capture_command

	msleep(30);
	/* Wait for Mode Transition (CM) */
	timeout_cnt = 0;
	do 
	{
		timeout_cnt++;
		isx005_i2c_read(client, 0x00F8, &read_value);
		msleep(1);
		if (timeout_cnt > 1000) 
		{
			dev_err(&client->dev, "%s: Entering capture mode timed out\n", __func__);	
			break;
		}
	}while(!(read_value&0x02));

	timeout_cnt = 0;
	do 
	{
		timeout_cnt++;
		isx005_i2c_write_multi(client, 0x00FC, 0x0002, 1);		
		msleep(1);			
		isx005_i2c_read(client, 0x00F8, &read_value);
		if (timeout_cnt > 1000) 
		{
			dev_err(&client->dev, "%s: Entering capture mode timed out\n", __func__);	
			break;
		}
	}while(read_value&0x02);	

	//capture frame out....
	isx005_msg(&client->dev, "%s: Capture frame out~~~~ \n", __func__);	
	msleep(50);

	timeout_cnt = 0;
	do 
	{
		timeout_cnt++;
		isx005_i2c_read(client, 0x00F8, &read_value);
		msleep(1);		
		if (timeout_cnt > 1000) 
		{
			dev_err(&client->dev, "%s: JPEG capture timed out\n", __func__);	
			break;
		}
	}while(!(read_value&0x08));	

	timeout_cnt = 0;
	do 
	{
		timeout_cnt++;
		isx005_i2c_write_multi(client, 0x00FC, 0x0008, 1);	
		msleep(1);
		isx005_i2c_read(client, 0x00F8, &read_value);
		if (timeout_cnt > 1000) 
		{
			dev_err(&client->dev, "%s: JPEG capture timed out\n", __func__);	
			break;
		}
	}while(read_value&0x08);	

	isx005_i2c_read(client, 0x0200, &read_value);
	isx005_msg(&client->dev, "%s: JPEG STS --- read_value == 0x%x \n", __func__, read_value);			


	/*
	 * 8. Get JPEG Main Data
	 */ 
	err = isx005_get_snapshot_data(sd);
	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: get_snapshot_data\n", __func__);
		return err;
	}

	if(gCurrentScene != SCENE_MODE_NIGHTSHOT) //when scene mode is not nightshot 
	{
		switch(flash_mode)
		{
			case FLASHMODE_AUTO:
				//if lowlight
				if(gLowLight_flash)
				{
					//when state is FLASHON
					isx005_flash(0, sd);//Flash OFF
					isx005_flash_end_capture(sd);
				}
				//when state is FLASH OFF, don't anything for FLASH MODE
				break;
				
			case FLASHMODE_ON:
				isx005_flash(0, sd);//Flash OFF
				isx005_flash_end_capture(sd);
				break;
				
			case FLASHMODE_OFF:
				break;
				
			default:
				dev_err(&client->dev, "%s: Unknown Flash mode \n", __func__);	
				break;
		}
	}

	err = isx005_LowLightCondition_Off(sd);
	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: isx005_LowLightCondition_Off\n", __func__);
		return err;
	}
	return 0;
}

static int isx005_change_scene_mode(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = 0;

	isx005_msg(&client->dev, "%s: start   CurrentScene : %d, Scene : %d\n", __func__, gCurrentScene, ctrl->value);
	
	switch(ctrl->value)
	{
		case SCENE_MODE_NONE:
			err = isx005_i2c_write(sd, \
				isx005_Scene_Default, \
				sizeof(isx005_Scene_Default) / sizeof(isx005_Scene_Default[0]), \
				"isx005_Scene_Default");			
			break;

		case SCENE_MODE_PORTRAIT:
			err = isx005_i2c_write(sd, \
				isx005_Scene_Portrait, \
				sizeof(isx005_Scene_Portrait) / sizeof(isx005_Scene_Portrait[0]), \
				"isx005_Scene_Portrait");			
			break;

		case SCENE_MODE_NIGHTSHOT:
			err = isx005_i2c_write(sd, \
				isx005_Scene_Nightshot, \
				sizeof(isx005_Scene_Nightshot) / sizeof(isx005_Scene_Nightshot[0]), \
				"isx005_Scene_Nightshot");			
			break;

		case SCENE_MODE_BACK_LIGHT:
			err = isx005_i2c_write(sd, \
				isx005_Scene_Backlight, \
				sizeof(isx005_Scene_Backlight) / sizeof(isx005_Scene_Backlight[0]), \
				"isx005_Scene_Backlight");			
			break;

		case SCENE_MODE_LANDSCAPE:
			err = isx005_i2c_write(sd, \
				isx005_Scene_Landscape, \
				sizeof(isx005_Scene_Landscape) / sizeof(isx005_Scene_Landscape[0]), \
				"isx005_Scene_Landscape");			
			break;

		case SCENE_MODE_SPORTS:
			err = isx005_i2c_write(sd, \
				isx005_Scene_Sports, \
				sizeof(isx005_Scene_Sports) / sizeof(isx005_Scene_Sports[0]), \
				"isx005_Scene_Sports");			
			break;

		case SCENE_MODE_PARTY_INDOOR:
			err = isx005_i2c_write(sd, \
				isx005_Scene_Party_Indoor, \
				sizeof(isx005_Scene_Party_Indoor) / sizeof(isx005_Scene_Party_Indoor[0]), \
				"isx005_Scene_Party_Indoor");			
			break;

		case SCENE_MODE_BEACH_SNOW:
			err = isx005_i2c_write(sd, \
				isx005_Scene_Beach_Snow, \
				sizeof(isx005_Scene_Beach_Snow) / sizeof(isx005_Scene_Beach_Snow[0]), \
				"isx005_Scene_Beach_Snow");			
			break;

		case SCENE_MODE_SUNSET:
			err = isx005_i2c_write(sd, \
				isx005_Scene_Sunset, \
				sizeof(isx005_Scene_Sunset) / sizeof(isx005_Scene_Sunset[0]), \
				"isx005_Scene_Sunset");			
			break;

		case SCENE_MODE_DUST_DAWN:
			err = isx005_i2c_write(sd, \
				isx005_Scene_Duskdawn, \
				sizeof(isx005_Scene_Duskdawn) / sizeof(isx005_Scene_Duskdawn[0]), \
				"isx005_Scene_Duskdawn");			
			break;

		case SCENE_MODE_FALL_COLOR:
			err = isx005_i2c_write(sd, \
				isx005_Scene_Fall_Color, \
				sizeof(isx005_Scene_Fall_Color) / sizeof(isx005_Scene_Fall_Color[0]), \
				"isx005_Scene_Fall_Color");			
			break;

		case SCENE_MODE_FIREWORKS:
			err = isx005_i2c_write(sd, \
				isx005_Scene_Fireworks, \
				sizeof(isx005_Scene_Fireworks) / sizeof(isx005_Scene_Fireworks[0]), \
				"isx005_Scene_Fireworks");			
			break;		

		case SCENE_MODE_TEXT:
			err = isx005_i2c_write(sd, \
				isx005_Scene_Text, \
				sizeof(isx005_Scene_Text) / sizeof(isx005_Scene_Text[0]), \
				"isx005_Scene_Text");			
			break;	

		case SCENE_MODE_CANDLE_LIGHT:
			err = isx005_i2c_write(sd, \
				isx005_Scene_Candle_Light, \
				sizeof(isx005_Scene_Candle_Light) / sizeof(isx005_Scene_Candle_Light[0]), \
				"isx005_Scene_Candle_Light");			
			break;			
			
		default:
			dev_err(&client->dev, "%s: unsupported scene(%d) mode\n", __func__, ctrl->value);
			break;
	}

	if(err < 0)
	{
		dev_err(&client->dev, "%s: i2c_write failed\n", __func__);
		return -EIO;
	}
	
	gCurrentScene = ctrl->value;	
	
	isx005_msg(&client->dev, "%s: done   CurrentScene : %d, Scene : %d\n", __func__, gCurrentScene, ctrl->value);
	
	return 0;
}

static int isx005_set_effect(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = 0;

	isx005_msg(&client->dev, "%s: setting value =%d\n", __func__, ctrl->value);

	switch(ctrl->value) 
	{
		case IMAGE_EFFECT_NONE:
		case IMAGE_EFFECT_AQUA:
		case IMAGE_EFFECT_ANTIQUE:
		case IMAGE_EFFECT_SHARPEN:
			err = isx005_i2c_write(sd, isx005_Effect_Normal, sizeof(isx005_Effect_Normal) / sizeof(isx005_Effect_Normal[0]),
				"isx005_Effect_Normal");
			break;
			
		case IMAGE_EFFECT_BNW:
			err = isx005_i2c_write(sd, isx005_Effect_Black_White, sizeof(isx005_Effect_Black_White) / sizeof(isx005_Effect_Black_White[0]),
				"isx005_Effect_Black_White");
			break;
			
		case IMAGE_EFFECT_SEPIA:
			err = isx005_i2c_write(sd, isx005_Effect_Sepia, sizeof(isx005_Effect_Sepia) / sizeof(isx005_Effect_Sepia[0]),
				"isx005_Effect_Sepia");
			break;
			
		case IMAGE_EFFECT_NEGATIVE:
			err = isx005_i2c_write(sd, isx005_Effect_Negative, sizeof(isx005_Effect_Negative) / sizeof(isx005_Effect_Negative[0]),
				"isx005_Effect_Negative");
			break;
			
		default:
			dev_err(&client->dev, "%s: unsupported effect(%d) value.\n", __func__, ctrl->value);
			break;
	}

	if(err < 0)
	{
		dev_err(&client->dev, "%s: i2c_write failed\n", __func__);
		return -EIO;
	}
	return 0;
}

static int isx005_set_saturation(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = 0;

	switch(ctrl->value)
	{
		case SATURATION_MINUS_2:
			err = isx005_i2c_write(sd, \
				isx005_Saturation_Minus_2, \
				sizeof(isx005_Saturation_Minus_2) / sizeof(isx005_Saturation_Minus_2[0]), \
				"isx005_Saturation_Minus_2");	
			break;

		case SATURATION_MINUS_1:
			err = isx005_i2c_write(sd, \
				isx005_Saturation_Minus_1, \
				sizeof(isx005_Saturation_Minus_1) / sizeof(isx005_Saturation_Minus_1[0]), \
				"isx005_Saturation_Minus_1");	
			break;

		case SATURATION_DEFAULT:
			err = isx005_i2c_write(sd, \
				isx005_Saturation_Default, \
				sizeof(isx005_Saturation_Default) / sizeof(isx005_Saturation_Default[0]), \
				"isx005_Saturation_Default");	
			break;

		case SATURATION_PLUS_1:
			err = isx005_i2c_write(sd, \
				isx005_Saturation_Plus_1, \
				sizeof(isx005_Saturation_Plus_1) / sizeof(isx005_Saturation_Plus_1[0]), \
				"isx005_Saturation_Plus_1");	
			break;

		case SATURATION_PLUS_2:
			err = isx005_i2c_write(sd, \
				isx005_Saturation_Plus_2, \
				sizeof(isx005_Saturation_Plus_2) / sizeof(isx005_Saturation_Plus_2[0]), \
				"isx005_Saturation_Plus_2");	
			break;
		
		default:
			dev_err(&client->dev, "%s: unsupported saturation(%d) value.\n", __func__, ctrl->value);
			break;
	}

	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: i2c_write for set_saturation\n", __func__);
		return -EIO;
	}
	
	isx005_msg(&client->dev, "%s: done, saturation: %d\n", __func__, ctrl->value);

	return 0;
}

static int isx005_set_contrast(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = 0;

	switch(ctrl->value)
	{
		case CONTRAST_MINUS_2:
			err = isx005_i2c_write(sd, \
				isx005_Contrast_Minus_2, \
				sizeof(isx005_Contrast_Minus_2) / sizeof(isx005_Contrast_Minus_2[0]), \
				"isx005_Contrast_Minus_2");	
			break;

		case CONTRAST_MINUS_1:
			err = isx005_i2c_write(sd, \
				isx005_Contrast_Minus_1, \
				sizeof(isx005_Contrast_Minus_1) / sizeof(isx005_Contrast_Minus_1[0]), \
				"isx005_Contrast_Minus_1");	
			break;

		case CONTRAST_DEFAULT:
			err = isx005_i2c_write(sd, \
				isx005_Contrast_Default, \
				sizeof(isx005_Contrast_Default) / sizeof(isx005_Contrast_Default[0]), \
				"isx005_Contrast_Default");	
			break;

		case CONTRAST_PLUS_1:
			err = isx005_i2c_write(sd, \
				isx005_Contrast_Plus_1, \
				sizeof(isx005_Contrast_Plus_1) / sizeof(isx005_Contrast_Plus_1[0]), \
				"isx005_Contrast_Plus_1");	
			break;

		case CONTRAST_PLUS_2:
			err = isx005_i2c_write(sd, \
				isx005_Contrast_Plus_2, \
				sizeof(isx005_Contrast_Plus_2) / sizeof(isx005_Contrast_Plus_2[0]), \
				"isx005_Contrast_Plus_2");	
			break;

		default:
			dev_err(&client->dev, "%s: unsupported contrast(%d) value.\n", __func__, ctrl->value);
			break;			
	}

	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: i2c_write for set_contrast\n", __func__);
		return -EIO;
	}
	
	isx005_msg(&client->dev, "%s: done, contrast: %d\n", __func__, ctrl->value);

	return 0;
}

static int isx005_set_sharpness(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = 0;

	switch(ctrl->value)
	{
		case SHARPNESS_MINUS_2:
			err = isx005_i2c_write(sd, \
				isx005_Sharpness_Minus_2, \
				sizeof(isx005_Sharpness_Minus_2) / sizeof(isx005_Sharpness_Minus_2[0]), \
				"isx005_Sharpness_Minus_2");	
			break;

		case SHARPNESS_MINUS_1:
			err = isx005_i2c_write(sd, \
				isx005_Sharpness_Minus_1, \
				sizeof(isx005_Sharpness_Minus_1) / sizeof(isx005_Sharpness_Minus_1[0]), \
				"isx005_Sharpness_Minus_1");	
			break;

		case SHARPNESS_DEFAULT:
			err = isx005_i2c_write(sd, \
				isx005_Sharpness_Default, \
				sizeof(isx005_Sharpness_Default) / sizeof(isx005_Sharpness_Default[0]), \
				"isx005_Sharpness_Default");	
			break;

		case SHARPNESS_PLUS_1:
			err = isx005_i2c_write(sd, \
				isx005_Sharpness_Plus_1, \
				sizeof(isx005_Sharpness_Plus_1) / sizeof(isx005_Sharpness_Plus_1[0]), \
				"isx005_Sharpness_Plus_1");	
			break;

		case SHARPNESS_PLUS_2:
			err = isx005_i2c_write(sd, \
				isx005_Sharpness_Plus_2, \
				sizeof(isx005_Sharpness_Plus_2) / sizeof(isx005_Sharpness_Plus_2[0]), \
				"isx005_Sharpness_Plus_2");	
			break;

		default:
			dev_err(&client->dev, "%s: unsupported sharpness(%d) value.\n", __func__, ctrl->value);
			break;			
	}

	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: i2c_write for set_saturation\n", __func__);
		return -EIO;
	}

	isx005_msg(&client->dev, "%s: done, sharpness: %d\n", __func__, ctrl->value);

	return 0;
}

/*Camcorder fix fps*/
static int isx005_set_sensor_mode(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = 0;
	
	isx005_msg(&client->dev,"func(%s):line(%d):ctrl->value(%d)\n",__func__,__LINE__,ctrl->value);
	
	if (ctrl->value == 1) 
	{
		printk("%s, isx005_camcorder_on\n", __func__);	
		err = isx005_i2c_write(sd, \
			isx005_camcorder_on, \
			sizeof(isx005_camcorder_on) / sizeof(isx005_camcorder_on[0]), \
			"isx005_camcorder_on");
		if (err < 0) 
		{
			dev_err(&client->dev, "%s: failed: i2c_write for set_sensor_mode %d\n", __func__, ctrl->value);
			return -EIO;
		}
	}
	else if (ctrl->value == 2)
	{
		printk("%s, flash is movie mode\n", __func__);
		if (flash_mode == FLASHMODE_ON)
			isx005_flash(100, sd);
	}
	else if (ctrl->value == 3)
	{//problem
		isx005_flash(0, sd);
		isx005_flash_end_capture(sd);
	}
	
	return 0;
}

static int isx005_set_focus_mode(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;

	switch(ctrl->value) 
	{
		case FOCUS_MODE_MACRO:
			isx005_msg(&client->dev, "%s: FOCUS_MODE_MACRO \n", __func__);	
			err = isx005_i2c_write(sd, isx005_AF_Macro_mode, ISX005_AF_MACRO_MODE_REGS, "isx005_AF_Macro_mode");
			if (err < 0) 
			{
				dev_err(&client->dev, "%s: i2c_write failed\n", __func__);
				return -EIO;
			}
			af_mode = FOCUS_MODE_MACRO;
			break;
			
		case FOCUS_MODE_FD:
			break;
			
		case FOCUS_MODE_AUTO:
			isx005_msg(&client->dev, "%s: FOCUS_MODE_AUTO \n", __func__);	
			err = isx005_i2c_write(sd, isx005_AF_Normal_mode, ISX005_AF_NORMAL_MODE_REGS, "isx005_AF_Normal_mode");
			if (err < 0) 
			{
				dev_err(&client->dev, "%s: i2c_write failed\n", __func__);
				return -EIO;
			}
			af_mode = FOCUS_MODE_AUTO;
			break;

		default:
			dev_err(&client->dev, "%s: unsupported focus(%d) value.\n", __func__, ctrl->value);
			break;			
	}

	return 0;
}

static int isx005_set_white_balance(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = 0;

	switch(ctrl->value)
	{
		case WHITE_BALANCE_AUTO:
			err = isx005_i2c_write(sd, \
				isx005_WB_Auto, \
				sizeof(isx005_WB_Auto) / sizeof(isx005_WB_Auto[0]), \
				"isx005_WB_Auto");
			break;

		case WHITE_BALANCE_SUNNY:
			err = isx005_i2c_write(sd, \
				isx005_WB_Sunny, \
				sizeof(isx005_WB_Sunny) / sizeof(isx005_WB_Sunny[0]), \
				"isx005_WB_Sunny");
			break;

		case WHITE_BALANCE_CLOUDY:
			err = isx005_i2c_write(sd, \
				isx005_WB_Cloudy, \
				sizeof(isx005_WB_Cloudy) / sizeof(isx005_WB_Cloudy[0]), \
				"isx005_WB_Cloudy");
			break;

		case WHITE_BALANCE_TUNGSTEN:
			err = isx005_i2c_write(sd, \
				isx005_WB_Tungsten, \
				sizeof(isx005_WB_Tungsten) / sizeof(isx005_WB_Tungsten[0]), \
				"isx005_WB_Tungsten");
			break;

		case WHITE_BALANCE_FLUORESCENT:
			err = isx005_i2c_write(sd, \
				isx005_WB_Fluorescent, \
				sizeof(isx005_WB_Fluorescent) / sizeof(isx005_WB_Fluorescent[0]), \
				"isx005_WB_Fluorescent");
			break;

		default:
			dev_err(&client->dev, "%s: unsupported wb(%d) value.\n", __func__, ctrl->value);
			break;
	}

	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: i2c_write for white_balance\n", __func__);
		return -EIO;
	}
	
	isx005_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int isx005_set_ev(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = 0;

	switch(ctrl->value)
	{
		case EV_MINUS_4:
			err = isx005_i2c_write(sd, \
				isx005_EV_Minus_4, \
				sizeof(isx005_EV_Minus_4) / sizeof(isx005_EV_Minus_4[0]), \
				"isx005_EV_Minus_4");
			break;

		case EV_MINUS_3:
			err = isx005_i2c_write(sd, \
				isx005_EV_Minus_3, \
				sizeof(isx005_EV_Minus_3) / sizeof(isx005_EV_Minus_3[0]), \
				"isx005_EV_Minus_3");
			break;

		case EV_MINUS_2:
			err = isx005_i2c_write(sd, \
				isx005_EV_Minus_2, \
				sizeof(isx005_EV_Minus_2) / sizeof(isx005_EV_Minus_2[0]), \
				"isx005_EV_Minus_2");
			break;

		case EV_MINUS_1:
			err = isx005_i2c_write(sd, \
				isx005_EV_Minus_1, \
				sizeof(isx005_EV_Minus_1) / sizeof(isx005_EV_Minus_1[0]), \
				"isx005_EV_Minus_1");
			break;

		case EV_DEFAULT:
			err = isx005_i2c_write(sd, \
				isx005_EV_Default, \
				sizeof(isx005_EV_Default) / sizeof(isx005_EV_Default[0]), \
				"isx005_EV_Default");
			break;

		case EV_PLUS_1:
			err = isx005_i2c_write(sd, \
				isx005_EV_Plus_1, \
				sizeof(isx005_EV_Plus_1) / sizeof(isx005_EV_Plus_1[0]), \
				"isx005_EV_Default");
			break;

		case EV_PLUS_2:
			err = isx005_i2c_write(sd, \
				isx005_EV_Plus_2, \
				sizeof(isx005_EV_Plus_2) / sizeof(isx005_EV_Plus_2[0]), \
				"isx005_EV_Plus_2");
			break;
		
		case EV_PLUS_3:
			err = isx005_i2c_write(sd, \
				isx005_EV_Plus_3, \
				sizeof(isx005_EV_Plus_3) / sizeof(isx005_EV_Plus_3[0]), \
				"isx005_EV_Plus_3");
			break;

		case EV_PLUS_4:
			err = isx005_i2c_write(sd, \
				isx005_EV_Plus_4, \
				sizeof(isx005_EV_Plus_4) / sizeof(isx005_EV_Plus_4[0]), \
				"isx005_EV_Plus_4");
			break;			

		default:
			dev_err(&client->dev, "%s: unsupported ev(%d) value.\n", __func__, ctrl->value);
			break;
	}

	//printk("isx005_set_ev: set_ev:, data: 0x%02x\n", isx005_buf_set_ev[1]);
	
	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: i2c_write for set_ev\n", __func__);
		return -EIO;
	}
	
	isx005_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int isx005_set_metering(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = 0;

	switch(ctrl->value)
	{
		case METERING_MATRIX:
			err = isx005_i2c_write(sd, \
				isx005_Metering_Matrix, \
				sizeof(isx005_Metering_Matrix) / sizeof(isx005_Metering_Matrix[0]), \
				"isx005_Metering_Matrix");
			break;

		case METERING_CENTER:
			err = isx005_i2c_write(sd, \
				isx005_Metering_Center, \
				sizeof(isx005_Metering_Center) / sizeof(isx005_Metering_Center[0]), \
				"isx005_Metering_Center");
			break;

		case METERING_SPOT:
			err = isx005_i2c_write(sd, \
				isx005_Metering_Spot, \
				sizeof(isx005_Metering_Spot) / sizeof(isx005_Metering_Spot[0]), \
				"isx005_Metering_Spot");
			break;

		default:
			dev_err(&client->dev, "%s: unsupported metering(%d) value.\n", __func__, ctrl->value);
			break;
	}
	
	if(err < 0)
	{
		dev_err(&client->dev, "%s: failed: i2c_write for set_photometry\n", __func__);
		return -EIO;
	}
	
	isx005_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int isx005_set_iso(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = 0;

	switch(ctrl->value)
	{
		case ISO_50:
		case ISO_800:
		case ISO_1600:
		case ISO_SPORTS:
		case ISO_NIGHT:
		case ISO_AUTO:
			err = isx005_i2c_write(sd, \
				isx005_ISO_Auto, \
				sizeof(isx005_ISO_Auto) / sizeof(isx005_ISO_Auto[0]), \
				"isx005_ISO_Auto");
			break;

		case ISO_100:
			err = isx005_i2c_write(sd, \
				isx005_ISO_100, \
				sizeof(isx005_ISO_100) / sizeof(isx005_ISO_100[0]), \
				"isx005_ISO_100");
			break;

		case ISO_200:
			err = isx005_i2c_write(sd, \
				isx005_ISO_200, \
				sizeof(isx005_ISO_200) / sizeof(isx005_ISO_200[0]), \
				"isx005_ISO_200");
			break;

		case ISO_400:
			err = isx005_i2c_write(sd, \
				isx005_ISO_400, \
				sizeof(isx005_ISO_400) / sizeof(isx005_ISO_400[0]), \
				"isx005_ISO_400");
			break;

		default:
			dev_err(&client->dev, "%s: unsupported iso(%d) value.\n", __func__, ctrl->value);
			break;
	}
	
	if(err < 0)
	{
		dev_err(&client->dev, "%s: i2c_write failed\n", __func__);
		return -EIO;
	}
	
	gIsoCondition = ctrl->value; //kidggang - About techwin temp test binay
	
	return 0;
}
	
static DEFINE_MUTEX(af_cancel_op);
/* GAUDI Project([arun.c@samsung.com]) 2010.05.19. [Implemented AF cancel] */
static int isx005_set_auto_focus(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isx005_state *state = to_state(sd);

	int err = 0;
	unsigned short read_value = 0;
	int timeout_cnt = 0;
	int stop_af_temp;

	isx005_msg(&client->dev, "%s: Do AF Behavior~~~~~~~ , %d\n", __func__, ctrl->value);		
//	err = isx005_get_LowLightCondition(sd, &gLowLight);
	/*
	 * ctrl -> value can be 0, 1, 2
	 *
	 * 1 --> start SINGLE AF operation
	 * 0 --> stop SINGLE AF operation or cancel it
	 * 2 --> Check the status of AF cancel operation
	 */
	if (ctrl->value == 1) 
	{
		gLowLight_flash = 0;
		if(gIsoCondition == 0)
		{
			err = isx005_get_LowLightCondition_flash(sd, &gLowLight_flash);
		}
		else
		{
			err = isx005_get_LowLightCondition_iso(sd, &gLowLight_flash);		
		}
		gLowLight_flash_second = gLowLight_flash;
		first_af_start = 1;
		if (af_operation_status == 1)
		{
//100815		stop_af_operation = 0;
			return 0;
		}

		af_operation_status = 0;
		/* Enter moniter mode if it is some different mode */
		isx005_i2c_read(client, 0x0011, &read_value);
		if ((read_value & 0xFF) != 0x00) 
		{
			isx005_i2c_write_multi(client, 0x0011, 0x0000, 1);
			isx005_i2c_write_multi(client, 0x0012, 0x0001, 1); //Moni_Refresh
			/* Wait for Mode Transition (CM) */
			timeout_cnt = 0;
			do 
			{
				timeout_cnt++;
				isx005_i2c_read(client, 0x00F8, &read_value);
				msleep(1);
				if (timeout_cnt > 1000) 
				{
					dev_err(&client->dev, "%s: Entering moniter mode timed out \n", __func__);	
					break;
				}
			} while (!(read_value & 0x02));

			timeout_cnt = 0;
			do 
			{
				timeout_cnt++;
				isx005_i2c_write_multi(client, 0x00FC, 0x0002, 1);		
				msleep(1);			
				isx005_i2c_read(client, 0x00F8, &read_value);
				if (timeout_cnt > 1000) 
				{
					dev_err(&client->dev, "%s: Entering moniter mode timed out \n", __func__);	
					break;
				}
			} while (read_value & 0x02);	

			/* delay for AE and AWB to settle down */
//100813			msleep(300);
		}

		if (af_mode == FOCUS_MODE_MACRO) 
		{
			err = isx005_i2c_write(sd, isx005_AF_Return_Macro_pos, ISX005_AF_RETURN_MACRO_POS_REGS, "isx005_AF_Return_Macro_pos");
			if (err < 0) 
			{
				dev_err(&client->dev, "%s: register write fail \n", __func__);	
				return -EIO;
			}
		} 
		else 
		{
			err = isx005_i2c_write(sd, isx005_AF_Return_Inf_pos, ISX005_AF_RETURN_INF_POS_REGS, "isx005_AF_Return_Inf_pos");
			if (err < 0) 
			{
				dev_err(&client->dev, "%s: register write fail \n", __func__);	
				return -EIO;
			}
		}

		do {
			isx005_i2c_read(client, 0x6D76, &read_value);
			isx005_msg(&client->dev, "%s: i2c_read --- read_value == 0x%x \n", __func__, read_value);		
		} while(!((read_value & 0xFF) == 0x03));

		switch(flash_mode)
		{
			case FLASHMODE_AUTO:
				isx005_i2c_write_multi(client, 0x480E, 0x0320, 2);//minimum af-opd the value for fir
				isx005_i2c_write_multi(client, 0x4810, 0x0320, 2);//maximum af-opd the value for fir					
				if (gLowLight_flash) // when over 10dB, flash on
				{
					isx005_flash_set_first(sd);
				}
				break;
			case FLASHMODE_ON:
				isx005_i2c_write_multi(client, 0x480E, 0x0320, 2);//minimum af-opd the value for fir
				isx005_i2c_write_multi(client, 0x4810, 0x0320, 2);//maximum af-opd the value for fir					
				isx005_flash_set_first(sd);
				break;
			case FLASHMODE_OFF:
				if(!(state->set_app))
				{
					//High speed AE
					err = isx005_i2c_write_multi(client, CAP_HALF_AE_CTRL, 0x0005, 1);
					err = isx005_i2c_write_multi(client, HALF_AWB_CTRL, 0x0001, 1);
					//Move to Half Rel Mode
					err = isx005_i2c_write_multi(client, 0x0011, 0x0001, 1);
				}
				if (gLowLight_flash) // when over 10dB, flash on
				{
					isx005_i2c_write_multi(client, 0x480E, 0x004B, 2);//minimum af-opd the value for fir
					isx005_i2c_write_multi(client, 0x4810, 0x004B, 2);//maximum af-opd the value for fir					
				}
				else
				{
					isx005_i2c_write_multi(client, 0x480E, 0x0320, 2);//minimum af-opd the value for fir
					isx005_i2c_write_multi(client, 0x4810, 0x0320, 2);//maximum af-opd the value for fir					
				}
				break;
			default:
				dev_err(&client->dev, "%s: Unknown Flash mode \n", __func__);	
				break;
		}

		/* Go to Half release mode */
		isx005_i2c_read(client, 0x0011, &read_value);
		if ((read_value & 0xFF) != 0x01) 
		{
			switch(flash_mode)
			{
				case FLASHMODE_AUTO:
					if (gLowLight_flash)
					{
						err = isx005_i2c_write(sd, isx005_half_release_with_flash, sizeof(isx005_half_release_with_flash) / sizeof(isx005_half_release_with_flash[0]),"isx005_half_release_with_flash");
					}
					else
					{
						err = isx005_i2c_write(sd, isx005_half_release, sizeof(isx005_half_release) / sizeof(isx005_half_release[0]),"isx005_half_release");						
					}
					break;
				case FLASHMODE_ON:
					err = isx005_i2c_write(sd, isx005_half_release_with_flash, sizeof(isx005_half_release_with_flash) / sizeof(isx005_half_release_with_flash[0]),"isx005_half_release_with_flash");
					break;
				case FLASHMODE_OFF:
					err = isx005_i2c_write(sd, isx005_half_release, sizeof(isx005_half_release) / sizeof(isx005_half_release[0]),"isx005_half_release");
					break;
				default:
					dev_err(&client->dev, "%s: Unknown Flash mode \n", __func__);	
					break;
			}


			/* Wait for Mode Transition (CM) */
			timeout_cnt = 0;
			do 
			{
				timeout_cnt++;
				isx005_i2c_read(client, 0x00F8, &read_value);
				msleep(1);
				if (timeout_cnt > 1000) 
				{
					dev_err(&client->dev, "%s: Entering Half release mode timed out \n", __func__);	
					break;
				}
			} while (!(read_value & 0x02));

				timeout_cnt = 0;
			do 
			{
				timeout_cnt++;
				isx005_i2c_write_multi(client, 0x00FC, 0x0002, 1);		
				msleep(1);			
				isx005_i2c_read(client, 0x00F8, &read_value);
				if (timeout_cnt > 1000) 
				{
					dev_err(&client->dev, "%s: Entering Half release mode timed out \n", __func__);	
					break;
				}
			} while (read_value & 0x02);	
		} 
		switch(flash_mode)
		{
			case FLASHMODE_AUTO:
				//when lowlight
				if (gLowLight_flash)
				//when FLASH MODE is on
				{
//100816					msleep(150);//waiting 1V time
					isx005_flash(15, sd);//Half flash
				}
				break;
			case FLASHMODE_ON:
				//if lowlight is actived, 1V time is about 150ms, if it's not, that is about 80ms
#if 0//100816				
				if (gLowLight_flash)
					msleep(150);
				else
					msleep(80);
#endif				
				//waiting 1V time
				isx005_flash(15, sd);
				break;
			case FLASHMODE_OFF:
				break;
			default:
				dev_err(&client->dev, "%s: Unknown Flash mode \n", __func__);	
				break;
		}

		switch(flash_mode)
		{
			case FLASHMODE_AUTO:
				//when lowlight
				if (gLowLight_flash) 
				{
					//	Wait for AE/AWB complete
					for(;;) {
						if (timeout_cnt == 200)
						{
							break;
						}
						isx005_i2c_read(client, HALF_MOVE_STS, &a_sts );
	//					dev_err(&client->dev, "%s: AE/AWB STATUS = %d, %d times\n", __func__, a_sts, timeout_cnt);
						if ( a_sts == 0 )
							break;
						msleep(1);	//	1ms sleep
						timeout_cnt++;
					}
				}
				break;
			case FLASHMODE_ON:
				//	Wait for AE/AWB complete
				for(;;) {
					if (timeout_cnt == 200)
					{
						break; 
					}
					isx005_i2c_read(client, HALF_MOVE_STS, &a_sts );
	//				dev_err(&client->dev, "%s: AE/AWB STATUS = %d, %d times\n", __func__, a_sts, timeout_cnt);
					if ( a_sts == 0 )
						break;
					msleep(1);	//	1ms sleep
					timeout_cnt++;
				}
				break;
			case FLASHMODE_OFF:
				break;
			default:
				dev_err(&client->dev, "%s: Unknown Flash mode \n", __func__);	
				break;
		}

#if 0//100813
		err = isx005_i2c_write(sd, isx005_Single_AF_Start, ISX005_SINGLE_AF_START_REGS, "isx005_Single_AF_Start");
		if (err < 0) 
		{
			dev_err(&client->dev, "%s: register write fail \n", __func__);	
//problem
			isx005_flash(0, sd);
			isx005_flash_end_capture(sd);

			return -EIO;
		}
#endif
//100815		af_operation_status = 1;
	}
	else if (ctrl->value == 0 && first_af_start) 
	{
		mutex_lock(&af_cancel_op);
//		stop_af_operation = 1;
		stop_af_operation++;

		switch(flash_mode)
		{
			case FLASHMODE_AUTO:
				if (gLowLight_flash)
				{
					isx005_flash(0, sd);
					isx005_flash_end_capture(sd);
				}
				else
				{
					isx005_flash(0, sd);					
				}
				break;
			case FLASHMODE_ON:
				isx005_flash(0, sd);//Flash OFF
				isx005_flash_end_capture(sd);
				break;
			case FLASHMODE_OFF:
				isx005_flash(0, sd);				
				break;
			default:
				dev_err(&client->dev, "%s: Unknown Flash mode \n", __func__);	
				break;
		}

		if (af_operation_status == 2) 
		{
//			stop_af_operation = 0;
			stop_af_operation--;
			if (stop_af_operation < 0)
			{
				stop_af_operation = 0;
			}
				
			af_operation_status = 0;

			/* Return to moniter mode */
			isx005_i2c_read(client, 0x0011, &read_value);
			if ((read_value & 0xFF) != 0x00) 
			{
				isx005_i2c_write_multi(client, 0x0011, 0x0000, 1);
				isx005_i2c_write_multi(client, 0x0012, 0x0001, 1); //Moni_Refresh

				/* Wait for Mode Transition (CM) */
				timeout_cnt = 0;
				do 
				{
					timeout_cnt++;
					isx005_i2c_read(client, 0x00F8, &read_value);
					msleep(1);
					if (timeout_cnt > 1000) 
					{
						dev_err(&client->dev, "%s: Entering moniter mode timed out \n", __func__);	
						break;
					}
				} while (!(read_value & 0x02));
				timeout_cnt = 0;
				do 
				{
					timeout_cnt++;
					isx005_i2c_write_multi(client, 0x00FC, 0x0002, 1);		
					msleep(1);			
					isx005_i2c_read(client, 0x00F8, &read_value);
					if (timeout_cnt > 1000) 
					{
						dev_err(&client->dev, "%s: Entering moniter mode timed out \n", __func__);	
						break;
					}
				} while (read_value & 0x02);	
			}

			if (af_mode == FOCUS_MODE_MACRO) 
			{
				err = isx005_i2c_write(sd, isx005_AF_Return_Macro_pos, ISX005_AF_RETURN_MACRO_POS_REGS, "isx005_AF_Return_Macro_pos");
				if (err < 0) 
				{
					dev_err(&client->dev, "%s: register write fail \n", __func__);	
					mutex_unlock(&af_cancel_op);
					return -EIO;
				}
			} 
			else 
			{
				err = isx005_i2c_write(sd, isx005_AF_Return_Inf_pos, ISX005_AF_RETURN_INF_POS_REGS, "isx005_AF_Return_Inf_pos");
				if (err < 0) 
				{
					dev_err(&client->dev, "%s: register write fail \n", __func__);	
					mutex_unlock(&af_cancel_op);
					return -EIO;
				}
			}
		}
		mutex_unlock(&af_cancel_op);
	}
	else if (ctrl->value == 2 && first_af_start)
	{
		isx005_msg(&client->dev, "%s:get AF cancel status start \n", __func__);	

		if(!g_ctrl_entered) return 0;
		timeout_cnt = 0;
		stop_af_temp = stop_af_operation - 1;


		do 
		{
			msleep(5);
			printk("111 stop_af : %d, af_oper : %d\n", stop_af_operation, af_operation_status);
			if (af_operation_status == 1)
			{
				break;
			}
		} while (stop_af_operation - stop_af_temp && stop_af_operation);

		isx005_msg(&client->dev, "%s:check FF end \n", __func__);	

		do 
		{
			timeout_cnt++;
			msleep(5);
			printk("222 stop_af : %d, af_oper : %d\n", stop_af_operation, af_operation_status);
#if 0 //100815
			if (timeout_cnt > 20) 
			{
				dev_err(&client->dev, "%s:  timed out \n", __func__);	
				break;
			}
#endif
		} while (af_operation_status == 1);		
//		} while (stop_af_operation);		
		isx005_msg(&client->dev, "%s:get AF cancel status end \n", __func__);	
		isx005_flash(0, sd);
	}

	return 0;
}

static int isx005_get_auto_focus_status(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = 0, count = 0;
	int timeout_cnt = 0;
	unsigned short read_value = 0;		


	isx005_msg(&client->dev, "%s: Check AF Result~~~~~~~ , stop af : %d\n", __func__, stop_af_operation);
	/* If af operation is not started do not perform status check */
	if (af_operation_status != 0) 
	{
		ctrl->value = 0x02;
		g_ctrl_entered = 0;
		return 0;
	}

	g_ctrl_entered = 1;
	af_operation_status = 1;

	/* Check the AF result by polling at 0x6d76 for 0x08*/
	do 
	{
		/* count is used to prevent endless looping here */
		count++;

		/* Check for AF cancel if af is cancelled stop the operation and return */
		if (stop_af_operation) 
		{
			isx005_msg(&client->dev, "AF is cancelled while doing\n");

			isx005_i2c_write_multi(client, 0x4885, 0x0001, 1);

			/* Return to Macro or infinite position */
			if (af_mode == FOCUS_MODE_MACRO) 
			{
				err = isx005_i2c_write(sd, isx005_AF_Return_Macro_pos, ISX005_AF_RETURN_MACRO_POS_REGS, "isx005_AF_Return_Macro_pos");
				if (err < 0) 
				{
					dev_err(&client->dev, "%s: %d register read fail\n", __func__, __LINE__);	
					return -EIO;
				}
			} 
			else 
			{
				err = isx005_i2c_write(sd, isx005_AF_Return_Inf_pos, ISX005_AF_RETURN_INF_POS_REGS, "isx005_AF_Return_Inf_pos");
				if (err < 0) 
				{
					dev_err(&client->dev, "%s: %d register read fail\n", __func__, __LINE__);	
					return -EIO;
				}
			}

			/* Return to moniter mode */
			isx005_i2c_read(client, 0x0011, &read_value);
			if ((read_value & 0xFF) != 0x00) 
			{
				isx005_i2c_write_multi(client, 0x0011, 0x0000, 1);
				isx005_i2c_write_multi(client, 0x0012, 0x0001, 1); //Moni_Refresh

				/* Wait for Mode Transition (CM) */
				timeout_cnt = 0;
				do 
				{
					timeout_cnt++;
					isx005_i2c_read(client, 0x00F8, &read_value);
					msleep(1);
					if (timeout_cnt > 1000) 
					{
						dev_err(&client->dev, "%s: Entering moniter mode timed out \n", __func__);	
						break;
					}
				} while (!(read_value & 0x02));

				timeout_cnt = 0;
				do 
				{
					timeout_cnt++;
					isx005_i2c_write_multi(client, 0x00FC, 0x0002, 1);		
					msleep(1);			
					isx005_i2c_read(client, 0x00F8, &read_value);
					if (timeout_cnt > 1000) {
						dev_err(&client->dev, "%s: Entering moniter mode timed out \n", __func__);	
						break;
					}
				} while (read_value & 0x02);	
			}

			/* Clear AF_LOCK_STS */
			isx005_i2c_write_multi(client, 0x00FC, 0x0010, 1);

			/*
			 * Inform user that AF is cancelled
			 * 0 --> AF failure
			 * 1 --> AF success
			 * 2 --> AF cancel
			 */
			if (count < 100 && count > 15)
			{
				msleep(100);			
			}
			else if (count >= 100)
			{
				msleep(300);			
			}

			ctrl->value = 0x02;
//			stop_af_operation = 0;
			stop_af_operation--;
			if (stop_af_operation < 0) 
				stop_af_operation = 0;
			af_operation_status = 0;//100815
			isx005_msg(&client->dev, "AF cancel finished, stop_af : %d\n", stop_af_operation);

			switch(flash_mode)
			{
				case FLASHMODE_AUTO:
					//when lowlight
					if (gLowLight_flash)
					{
						isx005_flash(0, sd);
						isx005_flash_set_second(sd);
						//100813 printk(" isx005_flash 0 --- isx005_flash_set_second --FLASHMODE_AUTO\n");//100813
					}
					//Turn off the FLASH
					//set the Exposure/WB for capture with FLASH
#if 1					
					else
					{
						isx005_flash(0, sd);					
					}
#endif					
					break;
				case FLASHMODE_ON:
					isx005_flash(0, sd);//Turn off the FLASH
					isx005_flash_set_second(sd);
					 //100813 printk(" isx005_flash 0 --- isx005_flash_set_second --FLASHMODE_ON\n");//100813
					//set the Exposure/WB for capture with FLASH
					break;
				case FLASHMODE_OFF:
					isx005_flash(0, sd);					
					break;
				default:
					dev_err(&client->dev, "%s: Unknown Flash mode \n", __func__);	
					break;
			}
			g_ctrl_entered = 0;

			return 0;
		}
		
		isx005_i2c_read(client, 0x6D76, &read_value);
		isx005_msg(&client->dev, "%s: i2c_read --- read_value == 0x%x \n", __func__, read_value);		
		msleep(1);
		if (count > 1000) 
		{
			ctrl->value = 0x00;	/* 0x00 --> AF failed*/ 
			break;
		}
	}while(!(read_value&0x08));	

	mutex_lock(&af_cancel_op);
	
	/* Clear AF_LOCK_STS */
	isx005_i2c_write_multi(client, 0x00FC, 0x0010, 1);

	/* Read AF result */
	isx005_i2c_read(client, 0x6D77, &read_value);
	isx005_msg(&client->dev, "%s: i2c_read --- read_value == 0x%x \n", __func__, read_value);		

	if ((read_value & 0xFF) == 0x01) 
	{
		isx005_msg(&client->dev, "%s: AF is success~~~~~~~ , stop_af : %d\n", __func__, stop_af_operation);
		ctrl->value = 0x01;	/* 0x01 --> AF sucess */ 
	}
	else if ((read_value & 0xFF) == 0x00) 
	{
		isx005_msg(&client->dev, "%s: AF is Failure~~~~~~~ , stop_af : %d\n", __func__, stop_af_operation);
		ctrl->value = 0x00;	/* 0x00 --> AF failed*/ 
	}
	else
	{
		isx005_msg(&client->dev, "%s: Unknown AF Mode~~~~~~~ , stop_af : %d\n", __func__, stop_af_operation);
	}


	switch(flash_mode)
	{
		case FLASHMODE_AUTO:
			//when lowlight
			if (gLowLight_flash)
				{
				isx005_flash_set_second(sd);
				//100813 printk(" isx005_get_auto_focus_status --- isx005_flash_set_second --FLASHMODE_AUTO\n");//100813
			}
			//Turn off the FLASH
			//set the Exposure/WB for capture with FLASH
			break;
		case FLASHMODE_ON:
			isx005_flash_set_second(sd);
			//100813 printk(" isx005_get_auto_focus_status --- isx005_flash_set_second--FLASHMODE_ON\n");//100813							
			//set the Exposure/WB for capture with FLASH
			break;
		case FLASHMODE_OFF:
			break;
		default:
			dev_err(&client->dev, "%s: Unknown Flash mode \n", __func__);	
			break;
	}

	/* We finished turn off the single AF now */
	isx005_msg(&client->dev, "%s: single AF Off command Setting~~~~ \n", __func__);	

	err = isx005_i2c_write(sd, isx005_Single_AF_Off, ISX005_SINGLE_AF_OFF_REGS, "isx005_Single_AF_Off");
	if (err < 0) 
	{
		dev_err(&client->dev, "%s: register write fail \n", __func__);	
		mutex_unlock(&af_cancel_op);
		return -EIO;
	}
#if 1 //100815
	if (stop_af_operation) 
	{
		/* Return to moniter mode */
		isx005_i2c_read(client, 0x0011, &read_value);
		if ((read_value & 0xFF) != 0x00) 
		{
			isx005_i2c_write_multi(client, 0x0011, 0x0000, 1);
			isx005_i2c_write_multi(client, 0x0012, 0x0001, 1); //Moni_Refresh

			/* Wait for Mode Transition (CM) */
				timeout_cnt = 0;
			do 
			{
				timeout_cnt++;
				isx005_i2c_read(client, 0x00F8, &read_value);
				msleep(1);
				if (timeout_cnt > 1000) 
				{
					dev_err(&client->dev, "%s: Entering moniter mode timed out \n", __func__);	
					break;
				}
			} while (!(read_value & 0x02));

				timeout_cnt = 0;
			do 
			{
				timeout_cnt++;
				isx005_i2c_write_multi(client, 0x00FC, 0x0002, 1);		
				msleep(1);			
				isx005_i2c_read(client, 0x00F8, &read_value);
				if (timeout_cnt > 1000) {
					dev_err(&client->dev, "%s: Entering moniter mode timed out \n", __func__);	
					break;
				}
			} while (read_value & 0x02);	
		}

		/* Return to Macro or infinite position */
		if (af_mode == FOCUS_MODE_MACRO) 
		{
			err = isx005_i2c_write(sd, isx005_AF_Return_Macro_pos, ISX005_AF_RETURN_MACRO_POS_REGS, "isx005_AF_Return_Macro_pos");
			if (err < 0) 
			{
				dev_err(&client->dev, "%s: register write fail \n", __func__);	
				mutex_unlock(&af_cancel_op);
				return -EIO;
			}
		} 
		else 
		{
			err = isx005_i2c_write(sd, isx005_AF_Return_Inf_pos, ISX005_AF_RETURN_INF_POS_REGS, "isx005_AF_Return_Inf_pos");
			if (err < 0) 
			{
				dev_err(&client->dev, "%s: register write fail \n", __func__);	
				mutex_unlock(&af_cancel_op);
				return -EIO;
			}	
		}
		ctrl->value = 0x02;
	}
#endif
//	stop_af_operation = 0;
	stop_af_operation--;
	if (stop_af_operation < 0)
	{
		stop_af_operation = 0;
	}
	af_operation_status = 2;
	isx005_msg(&client->dev, "%s: single AF check finished~~~~ \n", __func__);	
	
	mutex_unlock(&af_cancel_op);
	g_ctrl_entered = 0;
	
	return 0;
}

static int isx005_aeawb_unlock(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	int timeout_cnt = 0;	
	unsigned short read_value = 0;	

	isx005_msg(&client->dev, "%s: unlocking AE&AWB\n", __func__);	

	mutex_lock(&af_cancel_op);
	
	if (af_operation_status == 2) 
	{
		af_operation_status = 0;

		/* Enter moniter mode if it is some different mode */
		isx005_i2c_read(client, 0x0011, &read_value);
		if ((read_value & 0xFF) != 0x00) 
		{
			isx005_i2c_write_multi(client, 0x0011, 0x0000, 1);
			isx005_i2c_write_multi(client, 0x0012, 0x0001, 1); //Moni_Refresh
			
			/* Wait for Mode Transition (CM) */
			timeout_cnt = 0;
			do 
			{
				timeout_cnt++;
				isx005_i2c_read(client, 0x00F8, &read_value);
				msleep(1);
				if (timeout_cnt > 1000) 
				{
					dev_err(&client->dev, "%s: Entering moniter mode timed out \n", __func__);	
					break;
				}
			} while (!(read_value & 0x02));

				timeout_cnt = 0;
			do 
			{
				timeout_cnt++;
				isx005_i2c_write_multi(client, 0x00FC, 0x0002, 1);		
				msleep(1);			
				isx005_i2c_read(client, 0x00F8, &read_value);
				if (timeout_cnt > 1000) 
				{
					dev_err(&client->dev, "%s: Entering moniter mode timed out \n", __func__);	
					break;
				}
			} while (read_value & 0x02);	
		}
	}
	
	mutex_unlock(&af_cancel_op);
	
	return 0;
}

static int isx005_get_iso(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	int err = 0;
	unsigned short tmp;
	unsigned int iso_table[19] = {25, 32, 40, 50, 64,
							 80, 100, 125, 160, 200,
							 250, 320, 400, 500, 640,
							 800, 1000, 1250, 1600};

	err = isx005_i2c_read(client, 0x00F0, &tmp);
	ctrl->value = iso_table[tmp-1];

	return err;
}

static int isx005_get_shutterspeed(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	int err = 0;
	unsigned short tmp;

	err = isx005_i2c_read(client, 0x00F2, &tmp);
	ctrl->value |= tmp;
	err = isx005_i2c_read(client, 0x00F4, &tmp);
	ctrl->value |= (tmp << 16);

	return err;
}

static void isx005_init_parameters(struct v4l2_subdev *sd)
{
	struct isx005_state *state = to_state(sd);

	/* Set initial values for the sensor stream parameters */
	state->strm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	state->strm.parm.capture.timeperframe.numerator = 1;
	state->strm.parm.capture.capturemode = 0;

//	state->framesize_index = ISX005_PREVIEW_SVGA;//bestiq
	state->fps = 30; /* Default value */
	
	state->jpeg.enable = 0;
	state->jpeg.quality = 100;
	state->jpeg.main_offset = 0;
	state->jpeg.main_size = 0;
	state->jpeg.thumb_offset = 0;
	state->jpeg.thumb_size = 0;
	state->jpeg.postview_offset = 0;
}

#if 0
/* Sample code */
static const char *isx005_querymenu_wb_preset[] = {
	"WB Tungsten", "WB Fluorescent", "WB sunny", "WB cloudy", NULL
};
#endif

static struct v4l2_queryctrl isx005_controls[] = {
#if 0
	/* Sample code */
	{
		.id = V4L2_CID_WHITE_BALANCE_PRESET,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "White balance preset",
		.minimum = 0,
		.maximum = ARRAY_SIZE(isx005_querymenu_wb_preset) - 2,
		.step = 1,
		.default_value = 0,
	},
#endif
};

const char **isx005_ctrl_get_menu(u32 id)
{
	switch (id) 
	{
#if 0
		/* Sample code */
		case V4L2_CID_WHITE_BALANCE_PRESET:
			return isx005_querymenu_wb_preset;
#endif
		default:
			return v4l2_ctrl_get_menu(id);
	}
}

static inline struct v4l2_queryctrl const *isx005_find_qctrl(int id)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(isx005_controls); i++)
	{
		if (isx005_controls[i].id == id)
		{
			return &isx005_controls[i];
		}
	}

	return NULL;
}

static int isx005_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(isx005_controls); i++) 
	{
		if (isx005_controls[i].id == qc->id) 
		{
			memcpy(qc, &isx005_controls[i], sizeof(struct v4l2_queryctrl));
			return 0;
		}
	}

	return -EINVAL;
}

static int isx005_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm)
{
	struct v4l2_queryctrl qctrl;

	qctrl.id = qm->id;
	isx005_queryctrl(sd, &qctrl);

	return v4l2_ctrl_query_menu(qm, &qctrl, isx005_ctrl_get_menu(qm->id));
}

/*
 * Clock configuration
 * Configure expected MCLK from host and return EINVAL if not supported clock
 * frequency is expected
 * 	freq : in Hz
 * 	flag : not supported for now
 */
static int isx005_s_crystal_freq(struct v4l2_subdev *sd, u32 freq, u32 flags)
{
	int err = -EINVAL;

	return err;
}

static int isx005_g_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;

	return err;
}


/* This function is called from the g_ctrl api
 *
 * This function should be called only after the s_fmt call,
 * which sets the required width/height value.
 *
 * It checks a list of available frame sizes and returns the 
 * most appropriate index of the frame size.
 *
 * Note: The index is not the index of the entry in the list. It is
 * the value of the member 'index' of the particular entry. This is
 * done to add additional layer of error checking.
 *
 * The list is stored in an increasing order (as far as possible).
 * Hene the first entry (searching from the beginning) where both the 
 * width and height is more than the required value is returned.
 * In case of no match, we return the last entry (which is supposed
 * to be the largest resolution supported.)
 *
 * It returns the index (enum isx005_frame_size) of the framesize entry.
 */
static int isx005_get_framesize_index(struct v4l2_subdev *sd)
{
	struct isx005_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isx005_enum_framesize *frmsize;

	int i = 0;

	isx005_msg(&client->dev, "%s: Requested Res: %dx%d\n", __func__, state->pix.width, state->pix.height);

	/* Check for video/image mode */
	for(i = 0; i < (sizeof(isx005_framesize_list)/sizeof(struct isx005_enum_framesize)); i++)
	{
		frmsize = &isx005_framesize_list[i];

		if(frmsize->mode != state->oprmode)
		{
			continue;
		}

		if(state->oprmode == ISX005_OPRMODE_IMAGE)
		{
			/* In case of image capture mode, if the given image resolution is not supported,
 			 * return the next higher image resolution. */
			if (preview_ratio >= 15)
			{//must search wide
				if ((frmsize->width * 10 / frmsize->height) < 15)
				{
					continue;
				}
			}
			else
			{
			//search not wide
				if ((frmsize->width * 10 / frmsize->height) >= 15)
				{
					continue;
				}
			}
			
			if(frmsize->width >= state->pix.width && frmsize->height >= state->pix.height)
			{
				return frmsize->index;
			}
		} 
		else 
		{
			/* In case of video mode, if the given video resolution is not matching, use
 			 * the default rate (currently ISX005_PREVIEW_WVGA).
 			 */		 
			if(frmsize->width == state->pix.width && frmsize->height == state->pix.height)
			{
				preview_ratio = frmsize->width * 10 / frmsize->height;
				return frmsize->index;
			}
			else
			{
				preview_ratio = state->pix.width * 10 / state->pix.height;
			}
		}

	} 
	if (preview_ratio >= 15)
	{
		return (state->oprmode == ISX005_OPRMODE_IMAGE) ? ISX005_CAPTURE_W2MP : ISX005_PREVIEW_WSVGA;
	}
	else
	{
		/* If it fails, return the default value. */
		return (state->oprmode == ISX005_OPRMODE_IMAGE) ? ISX005_CAPTURE_3MP : ISX005_PREVIEW_SVGA;
	}
}

/* This function is called from the s_ctrl api
 * Given the index, it checks if it is a valid index.
 * On success, it returns 0.
 * On Failure, it returns -EINVAL
 */
static int isx005_set_framesize_index(struct v4l2_subdev *sd, unsigned int index)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isx005_state *state = to_state(sd);
	
	int i = 0;

	for(i = 0; i < (sizeof(isx005_framesize_list)/sizeof(struct isx005_enum_framesize)); i++)
	{
		if(isx005_framesize_list[i].index == index && isx005_framesize_list[i].mode == state->oprmode)
		{
			state->framesize_index 	= isx005_framesize_list[i].index;	
			state->pix.width 		= isx005_framesize_list[i].width;
			state->pix.height		 = isx005_framesize_list[i].height;
			isx005_msg(&client->dev, "%s: Camera Res: %dx%d\n", __func__, state->pix.width, state->pix.height);
			return 0;
		} 
	} 
	
	dev_err(&client->dev, "%s: not support frame size\n", __func__);
	
	return -EINVAL;
}

/* Information received: 
 * width, height
 * pixel_format -> to be handled in the upper layer 
 *
 * */
static int isx005_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	struct isx005_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	int err = 0;
	int framesize_index = -1;

	if(fmt->fmt.pix.pixelformat == V4L2_PIX_FMT_JPEG && fmt->fmt.pix.colorspace != V4L2_COLORSPACE_JPEG)
	{
		dev_err(&client->dev, "%s: mismatch in pixelformat and colorspace\n", __func__);
		return -EINVAL;
	}

	state->pix.width = fmt->fmt.pix.width;
	state->pix.height = fmt->fmt.pix.height;
	
	state->pix.pixelformat = fmt->fmt.pix.pixelformat;

	if(fmt->fmt.pix.colorspace == V4L2_COLORSPACE_JPEG)
	{
		state->oprmode = ISX005_OPRMODE_IMAGE;
	}
	else
	{
		state->oprmode = ISX005_OPRMODE_VIDEO; 
	}

	framesize_index = isx005_get_framesize_index(sd);

	isx005_msg(&client->dev, "%s:framesize_index = %d\n", __func__, framesize_index);
	
	err = isx005_set_framesize_index(sd, framesize_index);
	if(err < 0)
	{
		dev_err(&client->dev, "%s: set_framesize_index failed\n", __func__);
		return -EINVAL;
	}

	if(state->pix.pixelformat == V4L2_PIX_FMT_JPEG)
	{
		state->jpeg.enable = 1;
	} 
	else 
	{
		state->jpeg.enable = 0;
	}

	return 0;
}

static int isx005_enum_framesizes(struct v4l2_subdev *sd, \
					struct v4l2_frmsizeenum *fsize)
{
	struct isx005_state *state = to_state(sd);
	struct isx005_enum_framesize *elem;	
	
	int num_entries = sizeof(isx005_framesize_list)/sizeof(struct isx005_enum_framesize);	
	int index = 0;
	int i = 0;

	/* The camera interface should read this value, this is the resolution
 	 * at which the sensor would provide framedata to the camera i/f
 	 *
 	 * In case of image capture, this returns the default camera resolution (SVGA)
 	 */
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;

	if(state->pix.pixelformat == V4L2_PIX_FMT_JPEG)
	{
		index = ISX005_PREVIEW_SVGA;
	} 
	else 
	{
		index = state->framesize_index;
	}

	for(i = 0; i < num_entries; i++)
	{
		elem = &isx005_framesize_list[i];
		if(elem->index == index)
		{
			fsize->discrete.width = isx005_framesize_list[index].width;
			fsize->discrete.height = isx005_framesize_list[index].height;
			return 0;
		}
	}

	return -EINVAL;
}

static int isx005_enum_frameintervals(struct v4l2_subdev *sd, 
					struct v4l2_frmivalenum *fival)
{
	int err = 0;

	return err;
}

static int isx005_enum_fmt(struct v4l2_subdev *sd, struct v4l2_fmtdesc *fmtdesc)
{
	int num_entries;

	num_entries = sizeof(capture_fmts)/sizeof(struct v4l2_fmtdesc);

	if(fmtdesc->index >= num_entries)
	{
		return -EINVAL;
	}

    memset(fmtdesc, 0, sizeof(*fmtdesc));
    memcpy(fmtdesc, &capture_fmts[fmtdesc->index], sizeof(*fmtdesc));

	return 0;
}

static int isx005_try_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int num_entries = 0;
	int i = 0;

	num_entries = sizeof(capture_fmts)/sizeof(struct v4l2_fmtdesc);

	for(i = 0; i < num_entries; i++)
	{
		if(capture_fmts[i].pixelformat == fmt->fmt.pix.pixelformat)
			return 0;
	} 

	return -EINVAL;
}

/** Gets current FPS value */
static int isx005_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct isx005_state *state = to_state(sd);
	
	int err = 0;

	state->strm.parm.capture.timeperframe.numerator = 1;
	state->strm.parm.capture.timeperframe.denominator = state->fps;

	memcpy(param, &state->strm, sizeof(param));

	return err;
}

/** Sets the FPS value */
static int isx005_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isx005_state *state = to_state(sd);

	int err = 0;

	if(param->parm.capture.timeperframe.numerator != state->strm.parm.capture.timeperframe.numerator ||
	   param->parm.capture.timeperframe.denominator != state->strm.parm.capture.timeperframe.denominator)
	{
		
		int fps = 0;
		int fps_max = 30;

		if(param->parm.capture.timeperframe.numerator && param->parm.capture.timeperframe.denominator)
		{
			fps = (int)(param->parm.capture.timeperframe.denominator/param->parm.capture.timeperframe.numerator);
		}
		else
		{
			fps = 0;
		}

		if(fps <= 0 || fps > fps_max)
		{
			dev_err(&client->dev, "%s: Framerate %d not supported, setting it to %d fps.\n",__func__, fps, fps_max);
			fps = fps_max;
		}

		param->parm.capture.timeperframe.numerator = 1;
		param->parm.capture.timeperframe.denominator = fps;
	
		state->fps = fps;
	}

	/* Don't set the fps value, just update it in the state 
	 * We will set the resolution and fps in the start operation (preview/capture) call */
	
	return err;
}

static int isx005_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isx005_state *state = to_state(sd);
	struct isx005_userset userset = state->userset;
	
	int err = 0;

	printk("%s : control id = 0x%x, %d\n", __func__, ctrl->id, ctrl->id & 0xFF);
	
	switch (ctrl->id) 
	{
		case V4L2_CID_EXPOSURE:
			ctrl->value = userset.exposure_bias;
			break;
			
		case V4L2_CID_AUTO_WHITE_BALANCE:
			ctrl->value = userset.auto_wb;
			break;
			
		case V4L2_CID_WHITE_BALANCE_PRESET:
			ctrl->value = userset.manual_wb;
			break;
			
		case V4L2_CID_COLORFX:
			ctrl->value = userset.effect;
			break;
			
		case V4L2_CID_CONTRAST:
			ctrl->value = userset.contrast;
			break;
			
		case V4L2_CID_SATURATION:
			ctrl->value = userset.saturation;
			break;
			
		case V4L2_CID_SHARPNESS:
			ctrl->value = userset.sharpness;
			break;

		case V4L2_CID_CAM_JPEG_MAIN_SIZE:
			ctrl->value = state->jpeg.main_size;
			break;
		
		case V4L2_CID_CAM_JPEG_MAIN_OFFSET:
			ctrl->value = state->jpeg.main_offset;
			break;

		case V4L2_CID_CAM_JPEG_THUMB_SIZE:
			ctrl->value = state->jpeg.thumb_size;
			break;
			
		case V4L2_CID_CAM_JPEG_THUMB_OFFSET:
			ctrl->value = state->jpeg.thumb_offset;
			break;

		case V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET:
			ctrl->value = state->jpeg.postview_offset;
			break; 
		
		case V4L2_CID_CAM_JPEG_MEMSIZE:
			ctrl->value = SENSOR_JPEG_SNAPSHOT_MEMSIZE;
			break;

		//need to be modified
		case V4L2_CID_CAM_JPEG_QUALITY:
			ctrl->value = state->jpeg.quality;
			break;
			
		case V4L2_CID_CAMERA_OBJ_TRACKING_STATUS:
			break;
			
		case V4L2_CID_CAMERA_SMART_AUTO_STATUS:
			break;

		case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
			err = isx005_get_auto_focus_status(sd, ctrl);
			break;

		case V4L2_CID_CAM_DATE_INFO_YEAR:
			ctrl->value = 2010;//state->dateinfo.year;//bestiq 
			break; 
			
		case V4L2_CID_CAM_DATE_INFO_MONTH:
			ctrl->value = 2;//state->dateinfo.month;
			break; 
			
		case V4L2_CID_CAM_DATE_INFO_DATE:
			ctrl->value = 25;//state->dateinfo.date;
			break; 
			
		case V4L2_CID_CAM_SENSOR_VER:
			ctrl->value = state->sensor_version;
			break; 
			
		case V4L2_CID_CAM_FW_MINOR_VER:
			ctrl->value = state->fw.minor;
			break; 
			
		case V4L2_CID_CAM_FW_MAJOR_VER:
			ctrl->value = state->fw.major;
			break; 
			
		case V4L2_CID_CAM_PRM_MINOR_VER:
			ctrl->value = state->prm.minor;
			break; 
			
		case V4L2_CID_CAM_PRM_MAJOR_VER:
			ctrl->value = state->prm.major;
			break; 

		//case V4L2_CID_CAMERA_FLASH_CHECK:
		//	ctrl -> value = flash_check;
		//	break;
			
		default:
			dev_err(&client->dev, "%s: no such ctrl\n", __func__);
			return -ENOIOCTLCMD;
	}
	
	return err;
}

static int isx005_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isx005_state *state = to_state(sd);
	
	int err = 0;

	isx005_msg(&client->dev, "%s: V4l2 control ID =%d\n", __func__, ctrl->id - V4L2_CID_PRIVATE_BASE);
	printk("V4l2 control ID =%d\n", ctrl->id - V4L2_CID_PRIVATE_BASE);
	if(state->check_dataline)
	{
        		if( ( ctrl->id != V4L2_CID_CAM_PREVIEW_ONOFF ) &&
            		( ctrl->id != V4L2_CID_CAMERA_CHECK_DATALINE_STOP ) &&
            		( ctrl->id != V4L2_CID_CAMERA_CHECK_DATALINE ) )
       		 {
            		return 0;
        		}
	}
    
	mutex_lock(&sensor_s_ctrl);

	switch (ctrl->id) 
	{
		case V4L2_CID_CAMERA_VT_MODE:
			break;
			
		case V4L2_CID_CAMERA_AEAWB_LOCK_UNLOCK:
			err = isx005_aeawb_unlock(sd, ctrl);
			break;
			
		case V4L2_CID_CAMERA_FLASH_MODE:
			flash_mode = ctrl->value;
			printk("flash mode = %d\n", flash_mode);
			break;
			
		case V4L2_CID_CAMERA_BRIGHTNESS:
			err = isx005_set_ev(sd, ctrl);
			break;
			
		case V4L2_CID_CAMERA_WHITE_BALANCE:
			err = isx005_set_white_balance(sd, ctrl);
			break;
			
		case V4L2_CID_CAMERA_EFFECT:
			err = isx005_set_effect(sd, ctrl);
			break;
			
		case V4L2_CID_CAMERA_ISO:
			err = isx005_set_iso(sd, ctrl);
			break;
			
		case V4L2_CID_CAMERA_METERING:
			err = isx005_set_metering(sd, ctrl);
			break;
			
		case V4L2_CID_CAMERA_CONTRAST:
			err = isx005_set_contrast(sd, ctrl);
			break;
			
		case V4L2_CID_CAMERA_SATURATION:
			err = isx005_set_saturation(sd, ctrl);
			break;
			
		case V4L2_CID_CAMERA_SHARPNESS:
			err = isx005_set_sharpness(sd, ctrl);
			break;
			
		/*Camcorder fix fps*/
		case V4L2_CID_CAMERA_SENSOR_MODE:
			err = isx005_set_sensor_mode(sd, ctrl);
			printk("sensor mode = %d\n", ctrl->value);
			break;

		case V4L2_CID_CAMERA_WDR:
			break;

		case V4L2_CID_CAMERA_ANTI_SHAKE:
			break;

		case V4L2_CID_CAMERA_FACE_DETECTION:
			break;

		case V4L2_CID_CAMERA_SMART_AUTO:
			break;

		case V4L2_CID_CAMERA_FOCUS_MODE:
			err = isx005_set_focus_mode(sd, ctrl);
			break;
			
		case V4L2_CID_CAMERA_VINTAGE_MODE:
			break;
			
		case V4L2_CID_CAMERA_BEAUTY_SHOT:
			break;

		case V4L2_CID_CAMERA_FACEDETECT_LOCKUNLOCK:
			break;		

		//need to be modified
		case V4L2_CID_CAM_JPEG_QUALITY:
			if(ctrl->value < 0 || ctrl->value > 100){
				err = -EINVAL;
			} else {
				state->jpeg.quality = ctrl->value;
				err = isx005_set_jpeg_quality(sd);
			}
			break;

		case V4L2_CID_CAMERA_SCENE_MODE:
			err = isx005_change_scene_mode(sd, ctrl);
			isx005_msg(&client->dev,"isx005_change_scene_mode = %d \n", ctrl->value);	
			break;

		case V4L2_CID_CAMERA_GPS_LATITUDE:
			dev_err(&client->dev, "%s: V4L2_CID_CAMERA_GPS_LATITUDE: not implemented\n", __func__);
			break;

		case V4L2_CID_CAMERA_GPS_LONGITUDE:
			dev_err(&client->dev, "%s: V4L2_CID_CAMERA_GPS_LONGITUDE: not implemented\n", __func__);
			break;

		case V4L2_CID_CAMERA_GPS_TIMESTAMP:
			dev_err(&client->dev, "%s: V4L2_CID_CAMERA_GPS_TIMESTAMP: not implemented\n", __func__);
			break;

		case V4L2_CID_CAMERA_GPS_ALTITUDE:
			dev_err(&client->dev, "%s: V4L2_CID_CAMERA_GPS_ALTITUDE: not implemented\n", __func__);
			break;

		case V4L2_CID_CAMERA_ZOOM:
			err = isx005_set_dzoom(sd, ctrl);
			break;

		case V4L2_CID_CAMERA_TOUCH_AF_START_STOP:
			break;
			
		case V4L2_CID_CAMERA_CAF_START_STOP:
			break;	

		case V4L2_CID_CAMERA_OBJECT_POSITION_X:
			state->position.x = ctrl->value;
			break;

		case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
			state->position.y = ctrl->value;
			break;

		case V4L2_CID_CAMERA_OBJ_TRACKING_START_STOP:
			break;

		case V4L2_CID_CAMERA_SET_AUTO_FOCUS:
			err = isx005_set_auto_focus(sd, ctrl);
			break;		

		case V4L2_CID_CAMERA_FRAME_RATE:
			err = isx005_set_frame_rate(sd, ctrl);
			state->fps = ctrl->value;
			break;
			
		case V4L2_CID_CAMERA_ANTI_BANDING:
			break;

		case V4L2_CID_CAM_CAPTURE:
			err = isx005_set_capture_start(sd, ctrl);
			break;
		
		/* Used to start / stop preview operation. 
	 	 * This call can be modified to START/STOP operation, which can be used in image capture also */
		case V4L2_CID_CAM_PREVIEW_ONOFF:
			if(ctrl->value)
				err = isx005_set_preview_start(sd);
			else
				err = isx005_set_preview_stop(sd);
			break;

		case V4L2_CID_CAM_UPDATE_FW:
			break;

		case V4L2_CID_CAM_SET_FW_ADDR:
			break;

		case V4L2_CID_CAM_SET_FW_SIZE:
			break;

		case V4L2_CID_CAM_FW_VER:
			break;
			
		case V4L2_CID_CAMERA_GET_ISO:
			err = isx005_get_iso(sd, ctrl); 	
			break;
		
		case V4L2_CID_CAMERA_GET_SHT_TIME:
			err = isx005_get_shutterspeed(sd, ctrl);		
			break;	

		case V4L2_CID_CAMERA_CHECK_DATALINE:
			state->check_dataline = ctrl->value;
			err = 0;
			break;	

		case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
			err=isx005_check_dataline_stop(sd);
			break;	

		//case V4L2_CID_CAMERA_APP_CHECK:
		//	state->set_app = ctrl->value;
		//	err = 0;
		//	break;

		default:
			dev_err(&client->dev, "%s: no such control\n", __func__);
			//return -ENOIOCTLCMD;
			err = 0;
			break;
	}

	if (err < 0)
	{
		dev_err(&client->dev, "%s: vidioc_s_ctrl failed %d\n", __func__, err);
	}

	mutex_unlock(&sensor_s_ctrl);
	
	return err;
}

static int isx005_calibration(struct v4l2_subdev *sd)
{	
	unsigned long OTP00 = 0, OTP10 = 0;
	unsigned long OTP0 = 0, OTP1 = 0, OTP2 = 0; //OTPX0, OTPX1, OTPX2
	unsigned char valid_OPT = 3;
	unsigned char ret0, ret1;	
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	printk("isx005_calibration : start \n");
	
	ret0 = isx005_i2c_read_multi(client, 0x0238, &OTP10);
	ret1 = isx005_i2c_read_multi(client, 0x022C, &OTP00);	

	if(ret0 & ret1)
	{
		isx005_msg(&client->dev, "%s [cam] OPT10=0x%lx, OPT00=0x%lx \n", __func__, OTP10,OTP00);	

		if(((OTP10 & 0x10) >> 4) == 1) 
		{// CASE1 : READ OPT1 DATA
		    OTP0 = OTP10;
		    
		    isx005_i2c_read_multi(client, 0x023C, &OTP1);
		    isx005_i2c_read_multi(client, 0x0240, &OTP2);
		    valid_OPT = 1;
		}
		else if(((OTP00 & 0x10) >> 4) == 1)
		{// CASE2 : READ OPT0 DATA 
		    OTP0 = OTP00;
		    isx005_i2c_read_multi(client, 0x0230, &OTP1);
		    isx005_i2c_read_multi(client, 0x0234, &OTP2);
		    valid_OPT = 0;
		}
		else  // if((((OTP10 & 0x10) >> 4) == 0) && (((OTP00 & 0x10) >> 4) == 0 ) )
		{// CASE3 : Default cal. 
		    // Module was not calibrated in module vendor.
		    valid_OPT = 2;
		    return FALSE;
		}
	}
	else
	{
	    dev_err(&client->dev,"%s [cam] CALIBRATION : READ FAIL \n", __func__); 
	    return FALSE;
	}

	isx005_msg(&client->dev,"%s [cam] valid_OPT = %d \n", __func__, valid_OPT); 
	isx005_msg(&client->dev,"%s [cam] OTP2=0x%lx, OTP1=0x%lx, OTP0=0x%lx \n", __func__, OTP2, OTP1, OTP0); 


	// Shading Cal.
	if (valid_OPT == 1 || valid_OPT ==0)
	{ //CASE 1 || CASE 2:
	
		//Shading Index : OPTx0 [14-13]

	    unsigned char shading_index;
	    int err = 0;
	    
	    shading_index = (OTP0 & 0x6000) >> 13;
	    isx005_msg(&client->dev,"%s [cam] Shading Cal. : shading_index = %d  \n", __func__, shading_index); 

	    if (shading_index == 1) // 01
	    {
			err = isx005_i2c_write(sd, \
				isx005_shading_2, \
				sizeof(isx005_shading_2) / sizeof(isx005_shading_2[0]), \
				"isx005_shading_2");
			if (err < 0)
			{
				isx005_msg(&client->dev, "%s: register write fail \n", __func__);	
				return -EIO;
			}		
	    }
	    else if (shading_index== 2) // 10
	    {
			err = isx005_i2c_write(sd, \
				isx005_shading_3, \
				sizeof(isx005_shading_3) / sizeof(isx005_shading_3[0]), \
				"isx005_shading_3");
			if (err < 0)
			{
				isx005_msg(&client->dev, "%s: register write fail \n", __func__);	
				return -EIO;
			}		
	    }
	    else //  if (shading_index == 0), 00 or 11
	    {
			err = isx005_i2c_write(sd, \
				isx005_shading_1, \
				sizeof(isx005_shading_1) / sizeof(isx005_shading_1[0]), \
				"isx005_shading_1");
			if (err < 0)
			{
				isx005_msg(&client->dev, "%s: register write fail \n", __func__);	
				return -EIO;
			}		
	    }
	}


	// AWB Cal. 
	{
		unsigned short NORMR, NORMB; //14bit
		unsigned short AWBPRER, AWBPREB; //10bit

		NORMR = ((OTP1 & 0x3F) << 8) | ((OTP0 & 0xFF000000) >> 24);
		isx005_msg(&client->dev,"%s [cam] NORMR = 0x%x \n", __func__, NORMR); 
		if(NORMR <= 0x3FFF)
		{
		    isx005_i2c_write_multi(client, 0x4A04, NORMR, 2);
		}


		NORMB =  ((OTP1 & 0xFFFC0) >> 6);
		isx005_msg(&client->dev,"%s [cam] NORMB = 0x%x \n", __func__, NORMB); 
		if(NORMB <= 0x3FFF)
		{
		    isx005_i2c_write_multi(client, 0x4A06, NORMB, 2);
		}


		AWBPRER = ((OTP1 & 0x3FF00000) >> 20);
		isx005_msg(&client->dev,"%s [cam] AWBPRER = 0x%x \n", __func__, AWBPRER); 
		if(AWBPRER <= 0x3FF)
		{
		    isx005_i2c_write_multi(client, 0x4A08, AWBPRER, 2);
		}


		AWBPREB = ((OTP2 & 0xFF) << 2) | ((OTP1 & 0xC0000000) >> 30);
		isx005_msg(&client->dev,"%s [cam] AWBPREB = 0x%x \n", __func__, AWBPREB); 
		if(AWBPREB <= 0x3FF)
		{
		    isx005_i2c_write_multi(client, 0x4A0A, AWBPREB, 2);
		}
	}

	printk("%s : end \n", __func__); 
	
	return TRUE;		
}

static int  isx005_set_default_calibration(struct v4l2_subdev *sd)
{
	int err = 0;

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	printk("%s [cam] isx005_set_default_calibration  \n", __func__); 

	err = isx005_i2c_write(sd, \
		isx005_default_calibration, \
		sizeof(isx005_default_calibration) / sizeof(isx005_default_calibration[0]), \
		"isx005_default_calibration");
	if (err < 0)
	{
		dev_err(&client->dev, "%s: register write fail \n", __func__);	
		return -EIO;
	}  
	
	return TRUE;
}



static int isx005_check_dataline_stop(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isx005_state *state = to_state(sd);
	int err = -EINVAL, i;
      extern int isx005_power_reset(void);
      extern int isx005_cam_stdby(bool en);

	isx005_msg(&client->dev, "pattern off setting~~~~~~~~~~~~~~\n");	
     
	err =  isx005_i2c_write_multi(client, 0x3202, 0x00, 1);  //off
	
#if defined(CONFIG_TARGET_LOCALE_KOR) || defined(CONFIG_TARGET_LOCALE_EUR) || defined(CONFIG_TARGET_LOCALE_HKTW) || defined(CONFIG_TARGET_LOCALE_HKTW_FET) || defined(CONFIG_TARGET_LOCALE_USAGSM)
	isx005_power_reset();
#endif

	mdelay(150);
    
	for(i = 0; i <ISX005_INIT_REGS; i++)
	{
		err = isx005_i2c_write_multi(client, isx005_init_reg[i].subaddr ,  isx005_init_reg[i].value,  isx005_init_reg[i].len);
	}

	msleep(1);//100816

	isx005_cam_stdby(TRUE);// standby pin //100816	
	msleep(15); //wait stdby mode //100816	

	if (isx005_calibration(sd) == FALSE)
	{
	    isx005_set_default_calibration(sd);
	}
        	        	
	for(i = 0; i <ISX005_INIT_IMAGETUNING_SETTING_REGS; i++)
	{
        err = isx005_i2c_write_multi(client, isx005_init_image_tuning_setting[i].subaddr ,  isx005_init_image_tuning_setting[i].value ,  isx005_init_image_tuning_setting[i].len);
	}	
    
	state->check_dataline = 0;
    
	mdelay(100);
	return err;
}

static int isx005_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	int err = -EINVAL, count=0;
	unsigned short read_value_1 = 0, read_value_2 = 0;

#ifndef CONFIG_LOAD_FILE	
	int i = 0;
#endif

	extern int isx005_cam_stdby(bool en);

	printk("%s: init setting~~~~~~~~~~~~~~\n", __func__);
	
	isx005_init_parameters(sd);
	msleep(5);	//100816
	
#ifdef CONFIG_LOAD_FILE

	isx005_msg(&client->dev,"[BestIQ] + isx005_init\n");
	err = isx005_regs_table_init();
	if (err) 
	{
		dev_err(&client->dev, "%s: config file read fail\n", __func__);
		return -EIO;
	}
	msleep(100);
	
	isx005_i2c_write(sd, isx005_init_reg, ISX005_INIT_REGS, "isx005_init_reg");
	msleep(1);//100816

	isx005_cam_stdby(TRUE);// standby pin //100816	
	msleep(15); //wait stdby mode //100816	

	if (isx005_calibration(sd) == FALSE)
	{
	    isx005_set_default_calibration(sd);
	}

	isx005_msg(&client->dev, "%s: isx005_init_image_tuning_setting~~~~~~~~~~~~~~\n", __func__);		
	isx005_i2c_write(sd, isx005_init_image_tuning_setting, ISX005_INIT_IMAGETUNING_SETTING_REGS, "isx005_init_image_tuning_setting");
	isx005_msg(&client->dev,"[BestIQ] - isx005_init\n");

#else

	for(i = 0; i <ISX005_INIT_REGS; i++)
	{
		err = isx005_i2c_write_multi(client, isx005_init_reg[i].subaddr ,  isx005_init_reg[i].value,  isx005_init_reg[i].len);
		if (err < 0)
		{
			dev_err(&client->dev, "%s: %d register read fail\n", __func__, __LINE__);	
			return -EIO;
		}		
	}

	msleep(1);//100816

	isx005_cam_stdby(TRUE);// standby pin //100816	
	msleep(15); //wait stdby mode //100816
	
	if (isx005_calibration(sd) == FALSE)
	{
	    isx005_set_default_calibration(sd);
	}
	
	printk("%s: isx005_init_image_tuning_setting~~~~~~~~~~~~~~\n", __func__);	
	
	for(i = 0; i <ISX005_INIT_IMAGETUNING_SETTING_REGS; i++)
	{
		err = isx005_i2c_write_multi(client, isx005_init_image_tuning_setting[i].subaddr ,  isx005_init_image_tuning_setting[i].value ,  isx005_init_image_tuning_setting[i].len);
		if (err < 0)
		{
			dev_err(&client->dev, "%s: %d register read fail\n", __func__, __LINE__);	
			return -EIO;
		}		
	}	

//100816	isx005_cam_stdby(TRUE);// standby pin
	
#endif	
//100818	camera_init = 1;
	//Can use AF Command
	do
	{
		isx005_i2c_read(client, 0x000A, &read_value_1);
		isx005_msg(&client->dev, "%s: i2c_read --- read_value_1 == 0x%x \n", __func__, read_value_1);

 		isx005_i2c_read(client, 0x6D76, &read_value_2);
		isx005_msg(&client->dev, "%s: i2c_read --- read_value_2 == 0x%x \n", __func__, read_value_2);
		msleep(10);
		/*
		 * Arun c
		 * When the esd error occures during init the while loop never returns
		 * so keep a count of the loops
		 */
		if (count++ > 100)
			break;
	}while((!(read_value_1&0x02))&&(!(read_value_2&0x03)));		
	
	return err;
}

/*
 * s_config subdev ops
 * With camera device, we need to re-initialize every single opening time therefor,
 * it is not necessary to be initialized on probe time. except for version checking
 * NOTE: version checking is optional
 */
static int isx005_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isx005_state *state = to_state(sd);
	struct isx005_platform_data *pdata;

	pdata = client->dev.platform_data;

	if (!pdata) 
	{
		dev_err(&client->dev, "%s: no platform data\n", __func__);
		return -ENODEV;
	}

	/*
	 * Assign default format and resolution
	 * Use configured default information in platform data
	 * or without them, use default information in driver
	 */
	if (!(pdata->default_width && pdata->default_height)) 
	{
		/* TODO: assign driver default resolution */
	} 
	else 
	{
		state->pix.width = pdata->default_width;
		state->pix.height = pdata->default_height;
	}

	if (!pdata->pixelformat)
	{
		state->pix.pixelformat = DEFAULT_PIX_FMT;
	}
	else
	{
		state->pix.pixelformat = pdata->pixelformat;
	}

	if (!pdata->freq)
	{
		state->freq = DEFUALT_MCLK;	/* 24MHz default */
	}
	else
	{
		state->freq = pdata->freq;
	}

	return 0;
}

static const struct v4l2_subdev_core_ops isx005_core_ops = {
	.init = isx005_init,	/* initializing API */
	.s_config = isx005_s_config,	/* Fetch platform data */
	.queryctrl = isx005_queryctrl,
	.querymenu = isx005_querymenu,
	.g_ctrl = isx005_g_ctrl,
	.s_ctrl = isx005_s_ctrl,
};

static const struct v4l2_subdev_video_ops isx005_video_ops = {
	.s_crystal_freq = isx005_s_crystal_freq,
	.g_fmt = isx005_g_fmt,
	.s_fmt = isx005_s_fmt,
	.enum_framesizes = isx005_enum_framesizes,
	.enum_frameintervals = isx005_enum_frameintervals,
	.enum_fmt = isx005_enum_fmt,
	.try_fmt = isx005_try_fmt,
	.g_parm = isx005_g_parm,
	.s_parm = isx005_s_parm,
};

static const struct v4l2_subdev_ops isx005_ops = {
	.core = &isx005_core_ops,
	.video = &isx005_video_ops,
};

/*
 * isx005_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int isx005_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct isx005_state *state;
	struct v4l2_subdev *sd;

	printk("isx005_probe................................................... \n");
	
	state = kzalloc(sizeof(struct isx005_state), GFP_KERNEL);
	if (state == NULL)
	{
		return -ENOMEM;
	}

	state->runmode = ISX005_RUNMODE_NOTREADY;

	sd = &state->sd;
	strcpy(sd->name, ISX005_DRIVER_NAME);

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &isx005_ops);

	isx005_msg(&client->dev, "3MP camera ISX005 loaded.\n");

	return 0;
}

static int isx005_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	printk("isx005_remove.................................................... \n");

	isx005_flash(0, sd);//flash off

	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));

	isx005_msg(&client->dev, "Unloaded camera sensor ISX005.\n");

	return 0;
}

static const struct i2c_device_id isx005_id[] = {
	{ ISX005_DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, isx005_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = ISX005_DRIVER_NAME,
	.probe = isx005_probe,
	.remove = isx005_remove,
	.id_table = isx005_id,
};

MODULE_DESCRIPTION("NEC ISX005-SONY 3MP camera driver");
MODULE_AUTHOR("Tushar Behera <tushar.b@samsung.com>");
MODULE_LICENSE("GPL");
