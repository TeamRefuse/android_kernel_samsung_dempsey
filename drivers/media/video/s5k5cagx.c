/*
 * Driver for S5K5CAGX (UXGA camera) from Samsung Electronics
 * 
 * 1/4" 2.0Mp CMOS Image Sensor SoC with an Embedded Image Processor
 *
 * Copyright (C) 2009, Jinsung Yang <jsgood.yang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-i2c-drv.h>
#include <media/s5k5cagx_platform.h>

#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif

#include "s5k5cagx.h"
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <mach/max8998_function.h>

//#define CONFIG_LOAD_FILE
//#define VGA_CAM_DEBUG
#define S5K5CAGX_DEBUG
#define S5K5CAGX_INFO
#define  S5K5CAGX_RESULOTION_SUPPORT


#define S5K5CAGX_DRIVER_NAME	"S5K5CAGX"

/* Default resolution & pixelformat. plz ref s5k5cagx_platform.h */
#define FORMAT_FLAGS_COMPRESSED		0x3
#define DEFAULT_RES		WVGA	/* Index of resoultion */
#define DEFAUT_FPS_INDEX	S5K5CAGX_15FPS
#define DEFAULT_FMT		V4L2_PIX_FMT_UYVY	/* YUV422 */

/*
 * Specification
 * Parallel : ITU-R. 656/601 YUV422, RGB565, RGB888 (Up to VGA), RAW10 
 * Serial : MIPI CSI2 (single lane) YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Resolution : 1280 (H) x 1024 (V)
 * Image control : Brightness, Contrast, Saturation, Sharpness, Glamour
 * Effect : Mono, Negative, Sepia, Aqua, Sketch
 * FPS : 15fps @full resolution, 30fps @VGA, 24fps @720p
 * Max. pixel clock frequency : 48MHz(upto)
 * Internal PLL (6MHz to 27MHz input frequency)
 */

#ifdef S5K5CAGX_DEBUG
#define s5k5cagx_msg	dev_err
#else
#define s5k5cagx_msg 	dev_dbg
#endif

#ifdef S5K5CAGX_INFO
#define s5k5cagx_info	dev_err
#else
#define s5k5cagx_info	dev_dbg
#endif

#define CDBG(format, arg...) if (cdbg == 1) { printk("<s5k5cagx> %s " format, __func__, ## arg); }

static int cdbg = 0;
static int flash_mode = 1; 

static int s5k5cagx_init(struct v4l2_subdev *sd, u32 val);		//for fixing build error	//s1_camera [ Defense process by ESD input ]

enum s5k5cagx_oprmode {
	S5K5CAGX_OPRMODE_VIDEO = 0,
	S5K5CAGX_OPRMODE_IMAGE = 1,
};

/* Camera functional setting values configured by user concept */
struct s5k5cagx_userset {
	signed int exposure_bias;	/* V4L2_CID_EXPOSURE */
	unsigned int ae_lock;
	unsigned int awb_lock;
	unsigned int auto_wb;	/* V4L2_CID_AUTO_WHITE_BALANCE */
	unsigned int manual_wb;	/* V4L2_CID_WHITE_BALANCE_PRESET */
	unsigned int wb_temp;	/* V4L2_CID_WHITE_BALANCE_TEMPERATURE */
	unsigned int effect;	/* Color FX (AKA Color tone) */
	unsigned int contrast;	/* V4L2_CID_CONTRAST */
	unsigned int saturation;	/* V4L2_CID_SATURATION */
	unsigned int sharpness;		/* V4L2_CID_SHARPNESS */
	unsigned int glamour;
};

struct s5k5cagx_jpeg_param {
	unsigned int enable;
	unsigned int quality;
	unsigned int main_size;  /* Main JPEG file size */
	unsigned int thumb_size; /* Thumbnail file size */
	unsigned int main_offset;
	unsigned int thumb_offset;
	unsigned int postview_offset;
} ; 

enum s5k5cagx_runmode {
	S5K5CAGX_RUNMODE_NOTREADY,
	S5K5CAGX_RUNMODE_IDLE, 
	S5K5CAGX_RUNMODE_RUNNING, 
};

struct s5k5cagx_position {
	int x;
	int y;
} ; 

struct s5k5cagx_state {
	struct s5k5cagx_platform_data *pdata;
	struct v4l2_subdev sd;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	struct s5k5cagx_userset userset;
	struct s5k5cagx_jpeg_param jpeg;
	struct s5k5cagx_position position;
	struct v4l2_streamparm strm;
	enum s5k5cagx_runmode runmode;
	enum s5k5cagx_oprmode oprmode;
	enum v4l2_focusmode focus_mode; //create_new
	int framesize_index;
	int freq;	/* MCLK in KHz */
	int is_mipi;
	int isize;
	int ver;
	int fps;
	int vt_mode; /*For VT camera*/
	int check_dataline;
	int check_previewdata;
	int preview_size;
	int iso;
	int metering;
	int ev;
	int effect;
	int wb;
};


///frame
enum s5k5cagx_frame_size {
	S5K5CAGX_PREVIEW_QCIF = 0,
	S5K5CAGX_PREVIEW_QVGA,
	S5K5CAGX_PREVIEW_VGA,
	S5K5CAGX_PREVIEW_D1,
	S5K5CAGX_PREVIEW_WVGA, /* 800 x 480 */
	S5K5CAGX_PREVIEW_XGA, /*1024 X 768*/
	S5K5CAGX_PREVIEW_720P,
	S5K5CAGX_CAPTURE_VGA, /* 640 x 480 */	
	S5K5CAGX_CAPTURE_WVGA, /* 800 x 480 */
	S5K5CAGX_CAPTURE_SXGA, /* SXGA  - 1280 x 960 */
	S5K5CAGX_CAPTURE_W1MP, /* 1600 x 960 */
	S5K5CAGX_CAPTURE_2MP, /* UXGA  - 1600 x 1200 */
	S5K5CAGX_CAPTURE_W2MP, /*  2048 x 1232 */
	S5K5CAGX_CAPTURE_3MP, /* QXGA  - 2048 x 1536 */
};

struct s5k5cagx_enum_framesize {
	/* mode is 0 for preview, 1 for capture */
	enum s5k5cagx_oprmode mode;
	unsigned int index;
	unsigned int width;
	unsigned int height;	
};

static struct s5k5cagx_enum_framesize s5k5cagx_framesize_list[] = {
	{ S5K5CAGX_OPRMODE_VIDEO, S5K5CAGX_PREVIEW_QCIF,       176,  144 },
	{ S5K5CAGX_OPRMODE_VIDEO, S5K5CAGX_PREVIEW_QVGA,       320,  240 },
	{ S5K5CAGX_OPRMODE_VIDEO, S5K5CAGX_PREVIEW_VGA,        640,  480 },
	{ S5K5CAGX_OPRMODE_VIDEO, S5K5CAGX_PREVIEW_D1,         720,  480 },
	{ S5K5CAGX_OPRMODE_VIDEO, S5K5CAGX_PREVIEW_WVGA,       800,  480 },
	{ S5K5CAGX_OPRMODE_VIDEO, S5K5CAGX_PREVIEW_XGA,       1024, 768 },
	{ S5K5CAGX_OPRMODE_VIDEO, S5K5CAGX_PREVIEW_720P,      1280,  720 },
	{ S5K5CAGX_OPRMODE_IMAGE, S5K5CAGX_CAPTURE_VGA,	640,  480 },
	{ S5K5CAGX_OPRMODE_IMAGE, S5K5CAGX_CAPTURE_WVGA,	800,  480 },
	{ S5K5CAGX_OPRMODE_IMAGE, S5K5CAGX_CAPTURE_SXGA,	1280,  960},	
	{ S5K5CAGX_OPRMODE_IMAGE, S5K5CAGX_CAPTURE_W1MP,   1600,  960 },
	{ S5K5CAGX_OPRMODE_IMAGE, S5K5CAGX_CAPTURE_2MP,       1600, 1200 },
	{ S5K5CAGX_OPRMODE_IMAGE, S5K5CAGX_CAPTURE_W2MP,	2048, 1232 },
	{ S5K5CAGX_OPRMODE_IMAGE, S5K5CAGX_CAPTURE_3MP,       2048, 1536 },
	//{ S5K5CAGX_OPRMODE_IMAGE, S5K5CAGX_CAPTURE_W4MP,      2560, 1536 },
	//{ S5K5CAGX_OPRMODE_IMAGE, S5K5CAGX_CAPTURE_5MP,       2960, 1536 },
	//{ S5K5CAGX_OPRMODE_IMAGE, S5K5CAGX_CAPTURE_5MP,       2560, 1920 },
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

static inline struct s5k5cagx_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct s5k5cagx_state, sd);
}

//s1_camera [ Defense process by ESD input ] _[
static void s5k5cagx_ldo_en(bool onoff)
{
	if(onoff){
		// Trun CAM_D_1.8V on
		Set_MAX8998_PM_OUTPUT_Voltage(LDO15, VCC_1p800);
		Set_MAX8998_PM_REG(ELDO15, 1);
		
		mdelay(1);
		
		// Turn CAM_A_2.8V on
		Set_MAX8998_PM_OUTPUT_Voltage(LDO12, VCC_2p800);
		Set_MAX8998_PM_REG(ELDO12, 1);

		// Turn CAM_AF_2.8V_on
		Set_MAX8998_PM_OUTPUT_Voltage(LDO11, VCC_2p800);
		Set_MAX8998_PM_REG(ELDO11, 1);

		// Turn CAM_IO_1.8V on
		Set_MAX8998_PM_OUTPUT_Voltage(LDO16, VCC_2p800);
		Set_MAX8998_PM_REG(ELDO16, 1);
		
	} else {
		
		// Turn CAM_IO_1.8V off
		Set_MAX8998_PM_REG(ELDO16, 0);
		
		mdelay(1); 
		
		// Turn CAM_AF_2.8V off
		Set_MAX8998_PM_REG(ELDO11, 0);
	  
		// Turn CAM_A_2.8V off
		Set_MAX8998_PM_REG(ELDO12, 0);

		// Turn CAM_D_1.8V off
		Set_MAX8998_PM_REG(ELDO15, 0);
	}
}

static int s5k5cagx_power_on(void)
{
	int err;
	
	printk(KERN_DEBUG "s5k5cagx_power_on\n");

	/* CAM_MEGA_EN - GPJ0(6) */
	err = gpio_request(GPIO_CAM_MEGA_EN, "GPJ0");
	if(err) {
		printk(KERN_ERR "failed to request GPJ0 for camera control\n");
		return err;
	}

	/* CAM_MEGA_nRST - GPJ1(5) */
	err = gpio_request(GPIO_CAM_MEGA_nRST, "GPJ1");
	if(err) {
		printk(KERN_ERR "failed to request GPJ1 for camera control\n");
		return err;
	}
		
	//LDO enable
	s5k5cagx_ldo_en(TRUE);

	// Mclk enable
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, S5PV210_GPE1_3_CAM_A_CLKOUT);

	// CAM_MEGA_EN HIGH
	gpio_direction_output(GPIO_CAM_MEGA_EN, 0);
	gpio_set_value(GPIO_CAM_MEGA_EN, 1);

	mdelay(1);

	// CAM_MEGA_nRST HIGH
	gpio_direction_output(GPIO_CAM_MEGA_nRST, 0);
	gpio_set_value(GPIO_CAM_MEGA_nRST, 1);
	
	msleep(10);

	//CAM_GPIO free
	gpio_free(GPIO_CAM_MEGA_EN);
	gpio_free(GPIO_CAM_MEGA_nRST);
	
	return 0;
}

static int s5k5cagx_power_off(void)
{
	int err;
	
	printk(KERN_DEBUG "s5k5cagx_power_off\n");

	/* CAM_MEGA_EN - GPJ0(6) */
	err = gpio_request(GPIO_CAM_MEGA_EN, "GPJ0");
	if(err) {
		printk(KERN_ERR "failed to request GPJ0 for camera control\n");
		return err;
	}

	/* CAM_MEGA_nRST - GPJ1(5) */
	err = gpio_request(GPIO_CAM_MEGA_nRST, "GPJ1");
	if(err) {
		printk(KERN_ERR "failed to request GPJ1 for camera control\n");
		return err;
	}

	// CAM_MEGA_nRST - GPJ1(5) LOW
	gpio_direction_output(GPIO_CAM_MEGA_nRST, 1);
	gpio_set_value(GPIO_CAM_MEGA_nRST, 0);

	// CAM_MEGA_EN - GPJ0(6) LOW
	gpio_direction_output(GPIO_CAM_MEGA_EN, 1);
	gpio_set_value(GPIO_CAM_MEGA_EN, 0);

	// Mclk disable
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, 0);

	mdelay(1);

	//LDO disable
	s5k5cagx_ldo_en(FALSE);
	
	gpio_free(GPIO_CAM_MEGA_EN);
	gpio_free(GPIO_CAM_MEGA_nRST);

	return 0;
}

static int s5k5cagx_power_en(int onoff)
{
	if(onoff) {
		s5k5cagx_power_on();
	}

	else {
		s5k5cagx_power_off();
	}

	return 0;
}

static int s5k5cagx_reset(struct v4l2_subdev *sd)
{
	s5k5cagx_power_en(0);
	mdelay(5);
	s5k5cagx_power_en(1);
	mdelay(5);
	s5k5cagx_init(sd, 0);
	return 0;
}
// _]

/*
 * S5K5CAGX register structure : 2bytes address, 2bytes value
 * retry on write failure up-to 5 times
 */
 
#ifdef CONFIG_LOAD_FILE
	static int s5k5cagx_regs_table_write(struct i2c_client *client, char *name);
#endif

static int s5k5cagx_i2c_read(struct i2c_client *client, 
	unsigned short subaddr, unsigned short *data)
{
	unsigned char buf[2];
	int i, err = 0;
	struct i2c_msg msg = {client->addr, 0, 2, buf};

	buf[0] = subaddr>> 8;
	buf[1] = subaddr & 0xff;
	err = i2c_transfer(client->adapter, &msg, 1);

	if(err < 0)
	{
		s5k5cagx_msg(&client->dev, "%s: register read fail %d\n", __func__, i);	
	}

	msg.flags = I2C_M_RD;

	err = i2c_transfer(client->adapter, &msg, 1);

	/* Little Endian in parallel mode*/
	
//	*d1 = buf[1];
//	*d2 = buf[0];
	*data = ((buf[0] << 8) | buf[1]);
	

	if(err < 0)
	{
		s5k5cagx_msg(&client->dev, "%s: register read fail %d\n", __func__, i);	
	}
		
	return err;
}

static int s5k5cagx_i2c_write(struct i2c_client *client, unsigned short addr, unsigned short data)
{
	struct i2c_msg msg[1];
	unsigned char reg[4];
	int err = 0;
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;

again:
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 4;
	msg->buf = reg;

	reg[0] = addr >> 8;
	reg[1] = addr & 0xff;	
	reg[2] = data >> 8;
	reg[3] = data & 0xff;
	
	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
		return err;	/* Returns here on success */

	/* abnormal case: retry 5 times */
	if (retry < 5) {
		dev_err(&client->dev, "%s: address: 0x%02x%02x, " \
			"value: 0x%02x%02x\n", __func__, \
			reg[0], reg[1], reg[2], reg[3]);
		retry++;
		goto again;
	}

	return err;
}

static int s5k5cagx_i2c_write_block(struct v4l2_subdev *sd, s5k5cagx_short_t regs[], 
							int index, char *name)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i =0, err =0;
	int delay;
	
#ifdef CONFIG_LOAD_FILE
	s5k5cagx_regs_table_write(client, name);
#else
	for (i=0 ; i <index; i++) {
		if(regs[i].addr == 0xffff)
			{
		/*
			if(regs[i].val == 0x0064) msleep(100);
			else if(regs[i].val == 0x0085) msleep(133);
			else if(regs[i].val == 0x00a0) msleep(160);
			else if(regs[i].val == 0x00c8) msleep(200);
			else if(regs[i].val == 0x012c) msleep(300);
			else
				{
				printk("Not defined dalay\n");
				msleep(300);
				}
		*/
			msleep(regs[i].val);
			}
		else
			err = s5k5cagx_i2c_write(client, regs[i].addr, regs[i].val);
		
		if (unlikely(err < 0)) {
			v4l_info(client, "%s: register set failed\n", \
			__func__);
			return err;
		}
	}
#endif
	return 0;
}

/************************************************************************
CONFIG_LOAD_FILE
************************************************************************/

#ifdef CONFIG_LOAD_FILE

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <asm/uaccess.h>

static char *s5k5cagx_regs_table = NULL;

static int s5k5cagx_regs_table_size;

void s5k5cagx_regs_table_exit(void);

int s5k5cagx_regs_table_init(void)
{
#ifdef VIEW_FUNCTION_CALL	
	printk("[S5K5CAGX] %s function %d line launched!\n", __func__, __LINE__);
#endif

	printk("[BestIQ] + s5k5cagx_regs_table_init\n");
	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int i;
	int ret;
	mm_segment_t fs = get_fs();

	printk("%s %d\n", __func__, __LINE__);

	s5k5cagx_regs_table_exit();
		
	set_fs(get_ds());
#if 0
	filp = filp_open("/data/camera/isx005.h", O_RDONLY, 0);
#else
	filp = filp_open("/sdcard/sd/s5k5cagx.h", O_RDONLY, 0);
#endif

	if (IS_ERR(filp)) {
		printk("file open error\n");
		return PTR_ERR(filp);
	}
	
	l = filp->f_path.dentry->d_inode->i_size;	
	printk("l = %ld\n", l);
	dp = kmalloc(l, GFP_KERNEL);
//	dp = vmalloc(l);	
	if (dp == NULL) {
		printk("Out of Memory\n");
		filp_close(filp, current->files);
	}
	
	pos = 0;
	memset(dp, 0, l);
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	
	if (ret != l) {
		printk("Failed to read file ret = %d\n", ret);
		kfree(dp);
//		vfree(dp);
		filp_close(filp, current->files);
		return -EINVAL;
	}

	filp_close(filp, current->files);
		
	set_fs(fs);
	
	s5k5cagx_regs_table = dp;
		
	s5k5cagx_regs_table_size = l;
	
	*((s5k5cagx_regs_table + s5k5cagx_regs_table_size) - 1) = '\0';
	
	printk("s5k5cagx_regs_table 0x%08x, %ld\n", dp, l);
	printk("[BestIQ] - s5k5cagx_reg_table_init\n");

	return 0;
}

void s5k5cagx_regs_table_exit(void)
{
#ifdef VIEW_FUNCTION_CALL	
	printk("[S5K5CAGX] %s function %d line launched!\n", __func__, __LINE__);
#endif

	printk("[BestIQ] + s5k5cagx_regs_table_exit\n");
	printk("%s %d\n", __func__, __LINE__);
	if (s5k5cagx_regs_table) {
		kfree(s5k5cagx_regs_table);
		s5k5cagx_regs_table = NULL;
	}
	printk("[BestIQ] - s5k5cagx_regs_table_exit\n");
}

static int s5k5cagx_regs_table_write(struct i2c_client *client, char *name)
{
	printk("[BestIQ] + s5k5cagx_regs_table_write\n");
	char *start, *end, *reg, *data;	
	unsigned short addr, value;
	char reg_buf[7], data_buf[7];
	#ifdef VIEW_FUNCTION_CALL	
	printk("[S5K5CAGX] %s function %d line launched!\n", __func__, __LINE__);
	#endif
	
	*(reg_buf + 6) = '\0';
	*(data_buf + 6) = '\0';

//	printk("[BestIQ] + s5k5cagx_regs_table_write ------- start\n");
	start = strstr(s5k5cagx_regs_table, name);
	end = strstr(start, "};");
	
	while (1) {	
		/* Find Address */	
		reg = strstr(start,"{0x");		
		if (reg)
			start = (reg + 16);  //{0xFCFC, 0xD000}	
		if ((reg == NULL) || (reg > end))
			break;
		/* Write Value to Address */	
		if (reg != NULL) {
			memcpy(reg_buf, (reg + 1), 6);	
			memcpy(data_buf, (reg + 9), 6);				
			addr = (unsigned short)simple_strtoul(reg_buf, NULL, 16); 
			value = (unsigned short)simple_strtoul(data_buf, NULL, 16); 			
//			printk("addr 0x%04x, value1 0x%04x, value2 0x%04x\n", addr, value1, value2);
			if (addr == 0xffff)
				msleep(value);
			else
				s5k5cagx_i2c_write(client, addr, value);
		}
	}
	printk("[BestIQ] - s5k5cagx_regs_table_write\n");
	return 0;
}

#endif

static int s5k5cagx_set_preview_size(struct v4l2_subdev *sd)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k5cagx_state *state = to_state(sd);
	int index = state->framesize_index;

	s5k5cagx_msg(&client->dev, "%s: index = %d\n", __func__, index);

	switch(index){
		
	case S5K5CAGX_PREVIEW_QCIF: //176 X 144 (0)
	         err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_PREVIEW_SIZE_176,S5K5CAGX_PREVIEW_SIZE_176_INDEX,"S5K5CAGX_PREVIEW_SIZE_176");
		break;
         case S5K5CAGX_PREVIEW_QVGA: //320 X 240 (0)
                  err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_PREVIEW_SIZE_320,S5K5CAGX_PREVIEW_SIZE_320_INDEX,"S5K5CAGX_PREVIEW_SIZE_320");
		break;
	case S5K5CAGX_PREVIEW_VGA: //640 X 480	 (0)
	         err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_PREVIEW_SIZE_640,S5K5CAGX_PREVIEW_SIZE_640_INDEX,"S5K5CAGX_PREVIEW_SIZE_640");
		break;
	case S5K5CAGX_PREVIEW_WVGA: //800 X 480 (X)
	         err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_PREVIEW_SIZE_1024,S5K5CAGX_PREVIEW_SIZE_1024_INDEX,"S5K5CAGX_PREVIEW_SIZE_1024");
		break;
	case S5K5CAGX_PREVIEW_XGA: //1024 X 768 (0)
	         err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_PREVIEW_SIZE_1024,S5K5CAGX_PREVIEW_SIZE_1024_INDEX,"S5K5CAGX_PREVIEW_SIZE_1024");
		break;
	case S5K5CAGX_PREVIEW_D1: //720 X 480 (X)
	         err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_PREVIEW_SIZE_1024,S5K5CAGX_PREVIEW_SIZE_1024_INDEX,"S5K5CAGX_PREVIEW_SIZE_1024");
		break;
         case S5K5CAGX_PREVIEW_720P: //1280 X 720 (X)
         	err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_PREVIEW_SIZE_1024,S5K5CAGX_PREVIEW_SIZE_1024_INDEX,"S5K5CAGX_PREVIEW_SIZE_1024");
		break;
				
	default:
		/* When running in image capture mode, the call comes here.
 		 * Set the default video resolution - S5K5CAGX_PREVIEW_VGA
 		 */ 
		s5k5cagx_msg(&client->dev, "Setting preview resoution as VGA for image capture mode\n");
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_PREVIEW_SIZE_1024,S5K5CAGX_PREVIEW_SIZE_1024_INDEX,"S5K5CAGX_PREVIEW_SIZE_1024");
		break;
	}

	state->preview_size = index; 

	return err;	
}

static int s5k5cagx_set_preview_start(struct v4l2_subdev *sd)
{		
	int err, i, count;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k5cagx_state *state = to_state(sd);
	unsigned char read_value;

	if( !state->pix.width || !state->pix.height || !state->fps){
		return -EINVAL;
	}
	
	//Need to check the change mode
		
	err = s5k5cagx_set_preview_size(sd);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: Could not set preview size\n", __func__);
	       return -EIO;
	}

	err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_PREVIEW,S5K5CAGX_PREVIEW_INDEX,"S5K5CAGX_PREVIEW");
	if(err < 0){
		dev_err(&client->dev, "%s: failed: preview\n", __func__);
	       return -EIO;
	}
	state->runmode = S5K5CAGX_RUNMODE_RUNNING;

	return 0;
}

static int s5k5cagx_set_preview_stop(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k5cagx_state *state = to_state(sd);

	if(S5K5CAGX_RUNMODE_RUNNING == state->runmode){
		state->runmode = S5K5CAGX_RUNMODE_IDLE;
	}

	s5k5cagx_msg(&client->dev, "%s: change preview mode~~~~~~~~~~~~~~\n", __func__);
	
	return 0;
}

static int s5k5cagx_set_capture_size(struct v4l2_subdev *sd)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k5cagx_state *state = to_state(sd);

	int index = state->framesize_index;
	printk("s5k5cagx_set_capture_size ---------index : %d\n", index);	

	switch(index){
	case S5K5CAGX_CAPTURE_VGA: /* 640x480 */ 
		err = s5k5cagx_i2c_write_block(sd, S5K5CAGX_CAPTURE_SIZE_640,S5K5CAGX_CAPTURE_SIZE_640_INDEX,"S5K5CAGX_CAPTURE_SIZE_640");	
		break;
	case S5K5CAGX_CAPTURE_WVGA: /* 2048x1536 */
		err = s5k5cagx_i2c_write_block(sd, S5K5CAGX_CAPTURE_SIZE_2048,S5K5CAGX_CAPTURE_SIZE_2048_INDEX,"S5K5CAGX_CAPTURE_SIZE_2048");	
		break;
	case S5K5CAGX_CAPTURE_SXGA: /* 1280x960 */
		err = s5k5cagx_i2c_write_block(sd, S5K5CAGX_CAPTURE_SIZE_1280,S5K5CAGX_CAPTURE_SIZE_1280_INDEX,"S5K5CAGX_CAPTURE_SIZE_1280");	
		break;
	case S5K5CAGX_CAPTURE_W1MP: /* 2048x1536 */
		err = s5k5cagx_i2c_write_block(sd, S5K5CAGX_CAPTURE_SIZE_2048,S5K5CAGX_CAPTURE_SIZE_2048_INDEX,"S5K5CAGX_CAPTURE_SIZE_2048");	
		break;
	case S5K5CAGX_CAPTURE_2MP: /* 1600x1200 */
		err = s5k5cagx_i2c_write_block(sd, S5K5CAGX_CAPTURE_SIZE_1600,S5K5CAGX_CAPTURE_SIZE_1600_INDEX,"S5K5CAGX_CAPTURE_SIZE_1600");	
		break;
	case S5K5CAGX_CAPTURE_W2MP: /* 2048x1536 */
		err = s5k5cagx_i2c_write_block(sd, S5K5CAGX_CAPTURE_SIZE_2048,S5K5CAGX_CAPTURE_SIZE_2048_INDEX,"S5K5CAGX_CAPTURE_SIZE_2048");	
		break;
	case S5K5CAGX_CAPTURE_3MP: /* 2048x1536 */
		err = s5k5cagx_i2c_write_block(sd, S5K5CAGX_CAPTURE_SIZE_2048,S5K5CAGX_CAPTURE_SIZE_2048_INDEX,"S5K5CAGX_CAPTURE_SIZE_2048");	
		break;
	default:
		err = s5k5cagx_i2c_write_block(sd, S5K5CAGX_CAPTURE_SIZE_2048,S5K5CAGX_CAPTURE_SIZE_2048_INDEX,"S5K5CAGX_CAPTURE_SIZE_2048");
		/* The framesize index was not set properly. 
 		 * Check s_fmt call - it must be for video mode. */
		return -EINVAL;
	}

	/* Set capture image size */
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for capture_resolution\n", __func__);
		return -EIO; 
	}

	printk("s5k5cagx_set_capture_size: %d\n", index);

	return 0;	
}

static int s5k5cagx_set_capture_start(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	unsigned char read_value;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k5cagx_state *state = to_state(sd);

	s5k5cagx_msg(&client->dev, "%s: S5K5CAGX_CAPTURE\n", __func__);

	err =s5k5cagx_set_capture_size(sd);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for capture_resolution\n", __func__);
		return -EIO; 
	}

	err = s5k5cagx_i2c_write_block(sd, S5K5CAGX_NORMAL_SNAPSHOT,S5K5CAGX_NORMAL_SNAPSHOT_INDEX,"S5K5CAGX_NORMAL_SNAPSHOT");		
	if (err < 0) {
		dev_err(&client->dev, "%s: camera capture. err(%d)\n", __func__, err);
		return -EIO;	/* FIXME */	
	}
	
	return 0;
}

static int s5k5cagx_set_dzoom(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct s5k5cagx_state *state = to_state(sd);
	int err;
	int count;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int s5k5cagx_set_ae_awb(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	switch(ctrl->value)
	{
		case AE_LOCK_AWB_UNLOCK:  //not support
			s5k5cagx_msg(&client->dev, "%s: Not support AE_LOCK_AWB_UNLOCK\n", __func__);
			break;

		case AE_UNLOCK_AWB_LOCK:  //not support
			s5k5cagx_msg(&client->dev, "%s: Not support AE_UNLOCK_AWB_LOCK\n", __func__);
			break;
			
		case AE_LOCK_AWB_LOCK: 
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_AWB_AE_LOCK,S5K5CAGX_AWB_AE_LOCK_INDEX,"S5K5CAGX_AWB_AE_LOCK");
			break;

		case AE_UNLOCK_AWB_UNLOCK: 	
		default:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_AWB_AE_UNLOCK,S5K5CAGX_AWB_AE_UNLOCK_INDEX,"S5K5CAGX_AWB_AE_UNLOCK");	
			break;
	}
	
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for set_effect\n", __func__);
		return -EIO;
	}

	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

/**
 *  lock:	1 to lock, 0 to unlock
 */
static int s5k5cagx_set_awb_lock(struct v4l2_subdev *sd, int lock)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);


	if(lock)
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_AWB_AE_LOCK,S5K5CAGX_AWB_AE_LOCK_INDEX,"S5K5CAGX_AWB_AE_LOCK");
	else
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_AWB_AE_UNLOCK,S5K5CAGX_AWB_AE_UNLOCK_INDEX,"S5K5CAGX_AWB_AE_UNLOCK");

	
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for awb_lock\n", __func__);
		return -EIO;
	}

	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}


static int s5k5cagx_change_scene_mode(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value)
	{
		case SCENE_MODE_NONE:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SCENE_OFF,S5K5CAGX_SCENE_OFF_INDEX,"S5K5CAGX_SCENE_OFF");
		break;

		case SCENE_MODE_PORTRAIT:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SCENE_PORTRAIT,S5K5CAGX_SCENE_PORTRAIT_INDEX,"S5K5CAGX_SCENE_PORTRAIT");
		break;

		case SCENE_MODE_NIGHTSHOT:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SCENE_NIGHT,S5K5CAGX_SCENE_NIGHT_INDEX,"S5K5CAGX_SCENE_NIGHT");
		break;

		case SCENE_MODE_BACK_LIGHT:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SCENE_BACKLIGHT,S5K5CAGX_SCENE_BACKLIGHT_INDEX,"S5K5CAGX_SCENE_BACKLIGHT");
		break;

		case SCENE_MODE_LANDSCAPE:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SCENE_LANDSCAPE,S5K5CAGX_SCENE_LANDSCAPE_INDEX,"S5K5CAGX_SCENE_LANDSCAPE");
		break;

		case SCENE_MODE_SPORTS:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SCENE_SPORTS,S5K5CAGX_SCENE_SPORTS_INDEX,"S5K5CAGX_SCENE_SPORTS");
		break;

		case SCENE_MODE_PARTY_INDOOR:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SCENE_PARTY,S5K5CAGX_SCENE_PARTY_INDEX,"S5K5CAGX_SCENE_PARTY");
		break;

		case SCENE_MODE_BEACH_SNOW:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SCENE_BEACH,S5K5CAGX_SCENE_BEACH_INDEX,"S5K5CAGX_SCENE_BEACH");
		break;

		case SCENE_MODE_SUNSET:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SCENE_SUNSET,S5K5CAGX_SCENE_SUNSET_INDEX,"S5K5CAGX_SCENE_SUNSET");
		break;
/*
		case SCENE_MODE_DUSK_DAWN:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SCENE_DAWN,S5K5CAGX_SCENE_DAWN_INDEX,"S5K5CAGX_SCENE_DAWN");
		break;
*/
		case SCENE_MODE_FALL_COLOR:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGXG_SCENE_FALL,S5K5CAGXG_SCENE_FALL_INDEX,"S5K5CAGXG_SCENE_FALL");
		break;

		case SCENE_MODE_FIREWORKS:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SCENE_FIRE,S5K5CAGX_SCENE_FIRE_INDEX,"S5K5CAGX_SCENE_FIRE");
		break;		

		case SCENE_MODE_TEXT:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SCENE_TEXT,S5K5CAGX_SCENE_TEXT_INDEX,"S5K5CAGX_SCENE_TEXT");
		break;	

		case SCENE_MODE_CANDLE_LIGHT:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SCENE_CANDLE,S5K5CAGX_SCENE_CANDLE_INDEX,"S5K5CAGX_SCENE_CANDLE");
		break;			
			
		default:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SCENE_OFF,S5K5CAGX_SCENE_OFF_INDEX,"S5K5CAGX_SCENE_OFF");
		break;
	}

	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for set_effect\n", __func__);
		return -EIO;
	}
	
	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);

	return 0;
	

}

static int s5k5cagx_set_flash(int lux_val, struct v4l2_subdev *sd)
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
		lux_val = 3;
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
	}
	gpio_free(GPIO_CAM_FLASH_SET);	
	gpio_free(GPIO_CAM_FLASH_EN);
	return err;
}

static int s5k5cagx_set_effect(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value)
	{
		case IMAGE_EFFECT_NONE:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_EFFECT_OFF,S5K5CAGX_EFFECT_OFF_INDEX,"S5K5CAGX_EFFECT_OFF");
		break;

		case IMAGE_EFFECT_BNW:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_EFFECT_MONO,S5K5CAGX_EFFECT_MONO_INDEX,"S5K5CAGX_EFFECT_MONO");
		break;

		case IMAGE_EFFECT_SEPIA:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_EFFECT_SEPIA,S5K5CAGX_EFFECT_SEPIA_INDEX,"S5K5CAGX_EFFECT_SEPIA");
		break;

		case IMAGE_EFFECT_AQUA:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_EFFECT_AQUA,S5K5CAGX_EFFECT_AQUA_INDEX,"S5K5CAGX_EFFECT_AQUA");
		break;

		case IMAGE_EFFECT_ANTIQUE: //Not support
			s5k5cagx_msg(&client->dev, "%s: Not support IMAGE_EFFECT_ANTIQUE\n", __func__);
		break;

		case IMAGE_EFFECT_NEGATIVE:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_EFFECT_NEGATIVE,S5K5CAGX_EFFECT_NEGATIVE_INDEX,"S5K5CAGX_EFFECT_NEGATIVE");
		break;

		case IMAGE_EFFECT_SHARPEN: //Not support
			s5k5cagx_msg(&client->dev, "%s: Not support IMAGE_EFFECT_SHARPEN\n", __func__);
		break;
		
		//case IMAGE_EFFECT_SKETCH: //Not support
		//	err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_EFFECT_SKETCH,S5K5CAGX_EFFECT_SKETCH_INDEX,"S5K5CAGX_EFFECT_SKETCH");
		//break;
		
		default:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_EFFECT_OFF,S5K5CAGX_EFFECT_OFF_INDEX,"S5K5CAGX_EFFECT_OFF");
		break;
	}

	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for set_effect\n", __func__);
		return -EIO;
	}
	
	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int s5k5cagx_set_saturation(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value)
	{
		case SATURATION_MINUS_2:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SATURATION_M_2,S5K5CAGX_SATURATION_M_2_INDEX,"S5K5CAGX_SATURATION_M_2");
		break;

		case SATURATION_MINUS_1:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SATURATION_M_1,S5K5CAGX_SATURATION_M_1_INDEX,"S5K5CAGX_SATURATION_M_1");
		break;

		case SATURATION_DEFAULT:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SATURATION_0,S5K5CAGX_SATURATION_0_INDEX,"S5K5CAGX_SATURATION_0");
		break;

		case SATURATION_PLUS_1:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SATURATION_P_1,S5K5CAGX_SATURATION_P_1_INDEX,"S5K5CAGX_SATURATION_P_1");
		break;

		case SATURATION_PLUS_2:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SATURATION_P_2,S5K5CAGX_SATURATION_P_2_INDEX,"S5K5CAGX_SATURATION_P_2");
		break;

		default:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SATURATION_0,S5K5CAGX_SATURATION_0_INDEX,"S5K5CAGX_SATURATION_0");
		break;
	}

	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for set_saturation\n", __func__);
		return -EIO;
	}
	
	s5k5cagx_msg(&client->dev, "%s: done, saturation: %d\n", __func__, ctrl->value);

	return 0;
}

static int s5k5cagx_set_contrast(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value)
	{
		case CONTRAST_MINUS_2:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_CONTRAST_M_2,S5K5CAGX_CONTRAST_M_2_INDEX,"S5K5CAGX_CONTRAST_M_2");
		break;

		case CONTRAST_MINUS_1:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_CONTRAST_M_1,S5K5CAGX_CONTRAST_M_1_INDEX,"S5K5CAGX_CONTRAST_M_1");
		break;

		case CONTRAST_DEFAULT:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_CONTRAST_0,S5K5CAGX_CONTRAST_0_INDEX,"S5K5CAGX_CONTRAST_0");
		break;

		case CONTRAST_PLUS_1:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_CONTRAST_P_1,S5K5CAGX_CONTRAST_P_1_INDEX,"S5K5CAGX_CONTRAST_P_1");
		break;

		case CONTRAST_PLUS_2:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_CONTRAST_P_2,S5K5CAGX_CONTRAST_P_2_INDEX,"S5K5CAGX_CONTRAST_P_2");
		break;

		default:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_CONTRAST_0,S5K5CAGX_CONTRAST_0_INDEX,"S5K5CAGX_CONTRAST_0");
		break;
	}

	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for set_contrast\n", __func__);
		return -EIO;
	}
	
	s5k5cagx_msg(&client->dev, "%s: done, contrast: %d\n", __func__, ctrl->value);

	return 0;
}

static int s5k5cagx_set_sharpness(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value)
	{
		case SHARPNESS_MINUS_2:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SHAPNESS_M_2,S5K5CAGX_SHAPNESS_M_2_INDEX,"S5K5CAGX_SHAPNESS_M_2");
		break;

		case SHARPNESS_MINUS_1:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SHAPNESS_M_1,S5K5CAGX_SHAPNESS_M_1_INDEX,"S5K5CAGX_SHAPNESS_M_1");
		break;

		case SHARPNESS_DEFAULT:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SHAPNESS_0,S5K5CAGX_SHAPNESS_0_INDEX,"S5K5CAGX_SHAPNESS_0");
		break;

		case SHARPNESS_PLUS_1:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SHAPNESS_P_1,S5K5CAGX_SHAPNESS_P_1_INDEX,"S5K5CAGX_SHAPNESS_P_1");
		break;

		case SHARPNESS_PLUS_2:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SHAPNESS_P_2,S5K5CAGX_SHAPNESS_P_2_INDEX,"S5K5CAGX_SHAPNESS_P_2");
		break;
			
		default:
			err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_SHAPNESS_0,S5K5CAGX_SHAPNESS_0_INDEX,"S5K5CAGX_SHAPNESS_0");
		break;
	}

	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for set_saturation\n", __func__);
		return -EIO;
	}
	
	s5k5cagx_msg(&client->dev, "%s: done, sharpness: %d\n", __func__, ctrl->value);

	return 0;
}

static int s5k5cagx_set_wdr(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	switch(ctrl->value)
	{
		case WDR_ON:
		break;
		
		case WDR_OFF:			
		default:
			
		break;
	}

	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for set_wdr\n", __func__);
		return -EIO;
	}

	return 0;
}

static int s5k5cagx_set_anti_shake(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	switch(ctrl->value)
	{
		case ANTI_SHAKE_STILL_ON:
			
		break;

		case ANTI_SHAKE_MOVIE_ON:
		
		break;

		case ANTI_SHAKE_OFF:

		default:
	
		break;
	}

	
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for anti_shake\n", __func__);
		return -EIO;
	}

	return 0;
}

static int s5k5cagx_set_continous_af(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	int count;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int s5k5cagx_set_object_tracking(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	int count;
	
	struct s5k5cagx_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);	

	return 0;
}

static int s5k5cagx_get_object_tracking(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	int count;

	struct s5k5cagx_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);	

	return 0;
}

static int s5k5cagx_set_face_detection(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	switch(ctrl->value)
	{
		case FACE_DETECTION_ON:

		break;

		case FACE_DETECTION_ON_BEAUTY:

		break;

		case FACE_DETECTION_NOLINE:
			
		break;

		case FACE_DETECTION_OFF:

		default:

		break;
	}
	
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for face_detection\n", __func__);
		return -EIO;
	}

	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int s5k5cagx_set_smart_auto(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	if(ctrl->value == SMART_AUTO_ON)
	{
	
	}
	else
	{
	}

	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int s5k5cagx_get_smart_auto_status(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	int count;

	struct s5k5cagx_state *state = to_state(sd);	
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	s5k5cagx_msg(&client->dev, "%s: done(smartauto_status)\n", __func__);

	return 0;
}

static int s5k5cagx_set_touch_auto_focus(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for touch_auto_focus\n", __func__);
		return -EIO;
	}

	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int s5k5cagx_set_vintage_mode(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	switch(ctrl->value)
	{
		case VINTAGE_MODE_OFF:

		break;

		case VINTAGE_MODE_NORMAL:
		
		break;

		case VINTAGE_MODE_WARM:
		
		break;

		case VINTAGE_MODE_COOL:
		
		break;
		
		case VINTAGE_MODE_BNW:
		
		break;

		default:

		break;
	}

	
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for vintage_mode\n", __func__);
		return -EIO;
	}

	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int s5k5cagx_set_face_beauty(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value)
	{
		case BEAUTY_SHOT_ON:

		break;
		
		case BEAUTY_SHOT_OFF:
		default:
		break;
	}
	
	//Need to set face detection as 'face beauty on' mode.
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for set_face_beauty\n", __func__);
		return -EIO;
	}

	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int s5k5cagx_set_white_balance(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value)
	{
		case WHITE_BALANCE_AUTO:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_WB_AUTO,S5K5CAGX_WB_AUTO_INDEX,"S5K5CAGX_WB_AUTO");
		break;

		case WHITE_BALANCE_SUNNY:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_WB_DAYLIGHT,S5K5CAGX_WB_DAYLIGHT_INDEX,"S5K5CAGX_WB_DAYLIGHT");
		break;

		case WHITE_BALANCE_CLOUDY:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_WB_CLOUDY,S5K5CAGX_WB_CLOUDY_INDEX,"S5K5CAGX_WB_CLOUDY");
		break;

		case WHITE_BALANCE_TUNGSTEN:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_WB_INCANDESCENT,S5K5CAGX_WB_INCANDESCENT_INDEX,"S5K5CAGX_WB_INCANDESCENT");
		break;

		case WHITE_BALANCE_FLUORESCENT:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_WB_FLUORESCENT,S5K5CAGX_WB_FLUORESCENT_INDEX,"S5K5CAGX_WB_FLUORESCENT");
		break;

		default:
			dev_err(&client->dev, "%s: failed: to set_white_balance, enum: %d\n", __func__, ctrl->value);
			return -EINVAL;
		break;
	}

	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for white_balance\n", __func__);
		return -EIO;
	}
	
	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int s5k5cagx_set_ev(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value)
	{
		case EV_MINUS_4:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_BRIGHTNESS_N_4,S5K5CAGX_BRIGHTNESS_N_4_INDEX,"S5K5CAGX_BRIGHTNESS_N_4");
		break;

		case EV_MINUS_3:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_BRIGHTNESS_N_3,S5K5CAGX_BRIGHTNESS_N_3_INDEX,"S5K5CAGX_BRIGHTNESS_N_3");
		break;

		case EV_MINUS_2:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_BRIGHTNESS_N_2,S5K5CAGX_BRIGHTNESS_N_2_INDEX,"S5K5CAGX_BRIGHTNESS_N_2");
		break;

		case EV_MINUS_1:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_BRIGHTNESS_N_1,S5K5CAGX_BRIGHTNESS_N_1_INDEX,"S5K5CAGX_BRIGHTNESS_N_1");
		break;

		case EV_DEFAULT:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_BRIGHTNESS_0,S5K5CAGX_BRIGHTNESS_0_INDEX,"S5K5CAGX_BRIGHTNESS_0");
		break;

		case EV_PLUS_1:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_BRIGHTNESS_P_1,S5K5CAGX_BRIGHTNESS_P_1_INDEX,"S5K5CAGX_BRIGHTNESS_P_1");
		break;

		case EV_PLUS_2:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_BRIGHTNESS_P_2,S5K5CAGX_BRIGHTNESS_P_2_INDEX,"S5K5CAGX_BRIGHTNESS_P_2");
		break;
		
		case EV_PLUS_3:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_BRIGHTNESS_P_3,S5K5CAGX_BRIGHTNESS_P_3_INDEX,"S5K5CAGX_BRIGHTNESS_P_3");
		break;

		case EV_PLUS_4:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_BRIGHTNESS_P_4,S5K5CAGX_BRIGHTNESS_P_4_INDEX,"S5K5CAGX_BRIGHTNESS_P_4");
		break;			

		default:
			dev_err(&client->dev, "%s: failed: to set_ev, enum: %d\n", __func__, ctrl->value);
			return -EINVAL;
		break;
	}

		if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for set_ev\n", __func__);
		return -EIO;
	}
	
	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int s5k5cagx_set_metering(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value)
	{
		case METERING_MATRIX:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_METERING_NORMAL,S5K5CAGX_METERING_NORMAL_INDEX,"S5K5CAGX_METERING_NORMAL");
		break;

		case METERING_CENTER:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_METERING_CENTER,S5K5CAGX_METERING_CENTER_INDEX,"S5K5CAGX_METERING_CENTER");
		break;

		case METERING_SPOT:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_METERING_SPOT,S5K5CAGX_METERING_SPOT_INDEX,"S5K5CAGX_METERING_SPOT");
		break;

		default:
			dev_err(&client->dev, "%s: failed: to set_photometry, enum: %d\n", __func__, ctrl->value);
			return -EINVAL;
		break;
	}
	
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for set_photometry\n", __func__);
		return -EIO;
	}
	
	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}


static int s5k5cagx_set_iso(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value)
	{
		case ISO_AUTO:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_ISO_AUTO,S5K5CAGX_ISO_AUTO_INDEX,"S5K5CAGX_ISO_AUTO");
		break;

		case ISO_50: //Not support
			s5k5cagx_msg(&client->dev, "%s: failed: to set_iso, enum: %d\n", __func__, ctrl->value);
		break;

		case ISO_100:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_ISO_100,S5K5CAGX_ISO_100_INDEX,"S5K5CAGX_ISO_100");
		break;

		case ISO_200:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_ISO_200,S5K5CAGX_ISO_200_INDEX,"S5K5CAGX_ISO_200");
		break;

		case ISO_400:
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_ISO_400,S5K5CAGX_ISO_400_INDEX,"S5K5CAGX_ISO_400");
		break;

		case ISO_800: //Not support
			s5k5cagx_msg(&client->dev, "%s: failed: to set_iso, enum: %d\n", __func__, ctrl->value);
		break;

		case ISO_1600: //Not support
			s5k5cagx_msg(&client->dev, "%s: failed: to set_iso, enum: %d\n", __func__, ctrl->value);
		break;

		/* This is additional setting for Sports' scene mode */
		case ISO_SPORTS: //Not support
			s5k5cagx_msg(&client->dev, "%s: failed: to set_iso, enum: %d\n", __func__, ctrl->value);	
		break;
		
		/* This is additional setting for 'Night' scene mode */
		case ISO_NIGHT: //Not support
			s5k5cagx_msg(&client->dev, "%s: failed: to set_iso, enum: %d\n", __func__, ctrl->value);	
		break;

		case ISO_MOVIE: //Not support
			s5k5cagx_msg(&client->dev, "%s: failed: to set_iso, enum: %d\n", __func__, ctrl->value);	
		break;
		
		default:
			s5k5cagx_msg(&client->dev, "%s: failed: to set_iso, enum: %d\n", __func__, ctrl->value);
			return -EINVAL;
		break;
	}
	
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for set_iso\n", __func__);
		return -EIO;
	}
	
	s5k5cagx_msg(&client->dev, "%s: done, to set_iso, enum: %d\n", __func__, ctrl->value);

	return 0;
}


static int s5k5cagx_set_face_lock(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value) 
	{
		case FACE_DETECTION_ON:
			
		break;

		case FACE_DETECTION_OFF:
		default:
			
		break;
	}
	
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for face_lock\n", __func__);
		return -EIO;
	}

	s5k5cagx_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int s5k5cagx_set_focus_mode(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	int count;
	struct s5k5cagx_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value)
	{
		case FOCUS_MODE_MACRO:
		case FOCUS_MODE_MACRO_DEFAULT:
			//state->focus_mode = FOCUS_MODE_MACRO;
			err=s5k5cagx_i2c_write_block(sd,S5K5CAGX_AF_MACRO_ON,S5K5CAGX_AF_MACRO_ON_INDEX,"S5K5CAGX_AF_MACRO_ON");
		break;

		case FOCUS_MODE_FD:
		break;

		case FOCUS_MODE_AUTO:
		case FOCUS_MODE_AUTO_DEFAULT:
		case FOCUS_MODE_FD_DEFAULT:
		default:
			//state->focus_mode = FOCUS_MODE_AUTO;
			err=s5k5cagx_i2c_write_block(sd,S5K5CAGX_AF_NORMAL_ON,S5K5CAGX_AF_NORMAL_ON_INDEX,"S5K5CAGX_AF_NORMAL_ON");
		break;
	}

	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for s5k5cagx_set_focus_mode\n", __func__);
		return -EIO;
	}
	return 0;
}

static int s5k5cagx_set_auto_focus(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err, i;
	int count;
	struct s5k5cagx_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	//AF_Start
	s5k5cagx_msg(&client->dev, "%s:s5k5cagx_Single_AF_Start Setting~~~~ \n", __func__);

	if (ctrl->value)
	{
		s5k5cagx_msg(&client->dev, "%s:s5k5cagx_Single_AF_Start ~~~~ \n", __func__);
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_AF_DO,S5K5CAGX_AF_DO_INDEX,"S5K5CAGX_AF_DO");
		msleep(133); //spec 133m		
		if(err < 0){
			dev_err(&client->dev, "%s: failed: preview\n", __func__);
	      	 return -EIO;
		}
	}
	else 
	{
		s5k5cagx_msg(&client->dev, "%s:s5k5cagx_Single_AF_Cancel~~~~ \n", __func__);
		err = s5k5cagx_i2c_write_block(sd,S5K5CAGX_AF_OFF,S5K5CAGX_AF_OFF_INDEX,"S5K5CAGX_AF_OFF");
		if(err < 0){
			dev_err(&client->dev, "%s: failed: preview\n", __func__);
	      	 return -EIO;
		}
	}

	return 0;
}

static int s5k5cagx_get_auto_focus_status(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
	{
	int err, i;
	unsigned char read_value;	
	unsigned char s5k5cagx_buf_get_af_status[1] = { 0x00 };
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	s5k5cagx_buf_get_af_status[0] = 0x00;

	//Check AF Result
	err=s5k5cagx_i2c_write(client,0x002C, 0x7000);
	err=s5k5cagx_i2c_write(client,0x002E, 0x26FE);
	err=s5k5cagx_i2c_read(client, 0x0F12, &read_value);
	if(err < 0){
		s5k5cagx_msg(&client->dev, "%s: failed: s5k5cagx_get_auto_focus_status\n", __func__);
	      	 return -EIO;
		}
	s5k5cagx_msg(&client->dev, "%s: i2c_read --- read_value == 0x%x \n", __func__, read_value);	

	//idle 		0x0000
	//progress 	0x0001	-> HAL 0x05
	//success  	0x0002 	-> HAL 0x02
	//lowconf	0x0003	-> HAL	
	//canceled 	0x0004	-> HAL 0x04
	
	if (read_value == 0x0001)  s5k5cagx_buf_get_af_status[0] = 0x05;
	else if(read_value == 0x0002)  s5k5cagx_buf_get_af_status[0] = 0x02;
	else if (read_value == 0x0004)  s5k5cagx_buf_get_af_status[0] = 0x04;
	else {
		s5k5cagx_buf_get_af_status[0] = 0004;
	}
	
	ctrl->value = s5k5cagx_buf_get_af_status[0];
	//printk("isx005_get_auto_focus_status is called");
	return 0;
}


static void s5k5cagx_init_parameters(struct v4l2_subdev *sd)
{
	struct s5k5cagx_state *state = to_state(sd);

	/* Set initial values for the sensor stream parameters */
	state->strm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	state->strm.parm.capture.timeperframe.numerator = 1;
	state->strm.parm.capture.capturemode = 0;
#ifdef S5K5CAGX_RESULOTION_SUPPORT
	state->framesize_index = S5K5CAGX_PREVIEW_VGA;//bestiq
#else
	state->framesize_index = S5K5CAGX_PREVIEW_XGA;//bestiq
#endif
	state->fps = 30; /* Default value */
	
	state->jpeg.enable = 0;
	state->jpeg.quality = 100;
	state->jpeg.main_offset = 0;
	state->jpeg.main_size = 0;
	state->jpeg.thumb_offset = 0;
	state->jpeg.thumb_size = 0;
	state->jpeg.postview_offset = 0;
}


static struct v4l2_queryctrl s5k5cagx_controls[] = {
#if 0	// temporary delete
	{
		/*
		 * For now, we just support in preset type
		 * to be close to generic WB system,
		 * we define color temp range for each preset
		 */
		.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "White balance in kelvin",
		.minimum = 0,
		.maximum = 10000,
		.step = 1,
		.default_value = 0,	/* FIXME */
	},
	{
		.id = V4L2_CID_WHITE_BALANCE_PRESET,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "White balance preset",
		.minimum = 0,
		.maximum = ARRAY_SIZE(s5k5cagx_querymenu_wb_preset) - 2,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_AUTO_WHITE_BALANCE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Auto white balance",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_EXPOSURE,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Exposure bias",
		.minimum = 0,
		.maximum = ARRAY_SIZE(s5k5cagx_querymenu_ev_bias_mode) - 2,
		.step = 1,
		.default_value = (ARRAY_SIZE(s5k5cagx_querymenu_ev_bias_mode) - 2) / 2,	/* 0 EV */
	},
	{
		.id = V4L2_CID_COLORFX,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Image Effect",
		.minimum = 0,
		.maximum = ARRAY_SIZE(s5k5cagx_querymenu_effect_mode) - 2,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_CONTRAST,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Contrast",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
	{
		.id = V4L2_CID_SATURATION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Saturation",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
	{
		.id = V4L2_CID_SHARPNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Sharpness",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
#endif	
};

const char **s5k5cagx_ctrl_get_menu(u32 id)
{
	printk(KERN_DEBUG "s5k5cagx_ctrl_get_menu is called... id : %d \n", id);

	switch (id) {
#if 0	// temporary delete
	case V4L2_CID_WHITE_BALANCE_PRESET:
		return s5k5cagx_querymenu_wb_preset;

	case V4L2_CID_COLORFX:
		return s5k5cagx_querymenu_effect_mode;

	case V4L2_CID_EXPOSURE:
		return s5k5cagx_querymenu_ev_bias_mode;
#endif
	default:
		return v4l2_ctrl_get_menu(id);
	}
}

static inline struct v4l2_queryctrl const *s5k5cagx_find_qctrl(int id)
{
	int i;

	printk(KERN_DEBUG "s5k5cagx_find_qctrl is called...  id : %d \n", id);

	for (i = 0; i < ARRAY_SIZE(s5k5cagx_controls); i++)
		if (s5k5cagx_controls[i].id == id)
			return &s5k5cagx_controls[i];

	return NULL;
}

static int s5k5cagx_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i;

	printk(KERN_DEBUG "s5k5cagx_queryctrl is called... \n");

	for (i = 0; i < ARRAY_SIZE(s5k5cagx_controls); i++) {
		if (s5k5cagx_controls[i].id == qc->id) {
			memcpy(qc, &s5k5cagx_controls[i], \
				sizeof(struct v4l2_queryctrl));
			return 0;
		}
	}

	return -EINVAL;
}

static int s5k5cagx_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm)
{
	struct v4l2_queryctrl qctrl;

	printk(KERN_DEBUG "s5k5cagx_querymenu is called... \n");

	qctrl.id = qm->id;
	s5k5cagx_queryctrl(sd, &qctrl);

	return v4l2_ctrl_query_menu(qm, &qctrl, s5k5cagx_ctrl_get_menu(qm->id));
}

/*
 * Clock configuration
 * Configure expected MCLK from host and return EINVAL if not supported clock
 * frequency is expected
 * 	freq : in Hz
 * 	flag : not supported for now
 */
static int s5k5cagx_s_crystal_freq(struct v4l2_subdev *sd, u32 freq, u32 flags)
{
	int err = -EINVAL;

	printk(KERN_DEBUG "s5k5cagx_s_crystal_freq is called... \n");

	return err;
}

static int s5k5cagx_g_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;

	printk(KERN_DEBUG "s5k5cagx_g_fmt is called... \n");

	return err;
}

static int s5k5cagx_get_framesize_index(struct v4l2_subdev *sd);
static int s5k5cagx_set_framesize_index(struct v4l2_subdev *sd, unsigned int index);
/* Information received: 
 * width, height
 * pixel_format -> to be handled in the upper layer 
 *
 * */
static int s5k5cagx_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;
	struct s5k5cagx_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int framesize_index = -1;

	s5k5cagx_msg(&client->dev, "%s\n", __func__);

	if(fmt->fmt.pix.pixelformat == V4L2_PIX_FMT_JPEG && fmt->fmt.pix.colorspace != V4L2_COLORSPACE_JPEG){
		dev_err(&client->dev, "%s: mismatch in pixelformat and colorspace\n", __func__);
		return -EINVAL;
	}

	if(fmt->fmt.pix.width == 800 && fmt->fmt.pix.height == 448) {
		state->pix.width = 1280;
		state->pix.height = 720;
	}

	else {
		state->pix.width = fmt->fmt.pix.width;
		state->pix.height = fmt->fmt.pix.height;
	}
	
	state->pix.pixelformat = fmt->fmt.pix.pixelformat;

	if(fmt->fmt.pix.colorspace == V4L2_COLORSPACE_SRGB)
		state->oprmode = S5K5CAGX_OPRMODE_IMAGE;
	else
		state->oprmode = S5K5CAGX_OPRMODE_VIDEO; 


	framesize_index = s5k5cagx_get_framesize_index(sd);

	s5k5cagx_msg(&client->dev, "%s:framesize_index = %d\n", __func__, framesize_index);
	
	err = s5k5cagx_set_framesize_index(sd, framesize_index);
	if(err < 0){
		dev_err(&client->dev, "%s: set_framesize_index failed\n", __func__);
		return -EINVAL;
	}

	if(state->pix.pixelformat == V4L2_PIX_FMT_JPEG){
		state->jpeg.enable = 1;
	} else {
		state->jpeg.enable = 0;
	}

	return 0;
}

static int s5k5cagx_enum_framesizes(struct v4l2_subdev *sd, \
					struct v4l2_frmsizeenum *fsize)
{
	struct  s5k5cagx_state *state = to_state(sd);
	int num_entries = sizeof(s5k5cagx_framesize_list)/sizeof(struct s5k5cagx_enum_framesize);	
	struct s5k5cagx_enum_framesize *elem;	
	int index = 0;
	int i = 0;

	printk(KERN_DEBUG "s5k5cagx_enum_framesizes is called... \n");

	/* The camera interface should read this value, this is the resolution
 	 * at which the sensor would provide framedata to the camera i/f
 	 *
 	 * In case of image capture, this returns the default camera resolution (WVGA)
 	 */
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;

	if(state->pix.pixelformat == V4L2_PIX_FMT_JPEG){
		index = S5K5CAGX_PREVIEW_VGA;
	} else {
		index = state->framesize_index;
	}

	for(i = 0; i < num_entries; i++){
		elem = &s5k5cagx_framesize_list[i];
		if(elem->index == index){
			fsize->discrete.width = s5k5cagx_framesize_list[index].width;
			fsize->discrete.height = s5k5cagx_framesize_list[index].height;
			return 0;
		}
	}

	return -EINVAL;
}


static int s5k5cagx_enum_frameintervals(struct v4l2_subdev *sd, 
					struct v4l2_frmivalenum *fival)
{
	int err = 0;

	printk(KERN_DEBUG "s5k5cagx_enum_frameintervals is called... \n");
	
	return err;
}

static int s5k5cagx_enum_fmt(struct v4l2_subdev *sd, struct v4l2_fmtdesc *fmtdesc)
{
	int num_entries;
	printk(KERN_DEBUG "s5k5cagx_enum_fmt()\n");
	
	num_entries = sizeof(capture_fmts)/sizeof(struct v4l2_fmtdesc);

	if(fmtdesc->index >= num_entries)
		return -EINVAL;

        memset(fmtdesc, 0, sizeof(*fmtdesc));
        memcpy(fmtdesc, &capture_fmts[fmtdesc->index], sizeof(*fmtdesc));

	return 0;
}

static int s5k5cagx_try_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int num_entries;
	int i;

	num_entries = sizeof(capture_fmts)/sizeof(struct v4l2_fmtdesc);

	for(i = 0; i < num_entries; i++){
		if(capture_fmts[i].pixelformat == fmt->fmt.pix.pixelformat)
			return 0;
	} 

	return -EINVAL;
}

static int s5k5cagx_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct s5k5cagx_state *state = to_state(sd);
	int err = 0;

	state->strm.parm.capture.timeperframe.numerator = 1;
	state->strm.parm.capture.timeperframe.denominator = state->fps;

	memcpy(param, &state->strm, sizeof(param));

	return err;
}

static int s5k5cagx_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k5cagx_state *state = to_state(sd);

	if(param->parm.capture.timeperframe.numerator != state->strm.parm.capture.timeperframe.numerator ||
			param->parm.capture.timeperframe.denominator != state->strm.parm.capture.timeperframe.denominator){
		
		int fps = 0;
		int fps_max = 30;

		if(param->parm.capture.timeperframe.numerator && param->parm.capture.timeperframe.denominator)
			fps = (int)(param->parm.capture.timeperframe.denominator/param->parm.capture.timeperframe.numerator);
		else 
			fps = 0;

		if(fps <= 0 || fps > fps_max){
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

static int s5k5cagx_get_framesize_index(struct v4l2_subdev *sd)
{
	int i = 0;
	struct s5k5cagx_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k5cagx_enum_framesize *frmsize;

	s5k5cagx_info(&client->dev, "%s: Requested Res: %dx%d\n", __func__, state->pix.width, state->pix.height);

#ifdef S5K5CAGX_RESULOTION_SUPPORT
	/* Check for video/image mode */
	for(i = 0; i < (sizeof(s5k5cagx_framesize_list)/sizeof(struct s5k5cagx_enum_framesize)); i++)
	{
		frmsize = &s5k5cagx_framesize_list[i];

		if(frmsize->mode != state->oprmode)
			continue;

		if(state->oprmode == S5K5CAGX_OPRMODE_IMAGE)
		{
			/* In case of image capture mode, if the given image resolution is not supported,
 			 * return the next higher image resolution. */
			if(frmsize->width == state->pix.width && frmsize->height == state->pix.height)
			{
				//printk("frmsize->index(%d) \n", frmsize->index);
				return frmsize->index;
			}
		}
		else //S5K5CAGX_OPRMODE_VIDEO
		{
			/* In case of video mode, if the given video resolution is not matching, use
 			 * the default rate (currently S5K5CAGX_PREVIEW_VGA).
 			 */		 
			if(frmsize->width == state->pix.width && frmsize->height == state->pix.height)
			{
				//printk("frmsize->index(%d) \n", frmsize->index);
				return frmsize->index;
			}
		} 
	} 
#else
		if(state->oprmode == S5K5CAGX_OPRMODE_IMAGE)
		{
			return S5K5CAGX_CAPTURE_3MP;
		}
		else //S5K5CAGX_OPRMODE_VIDEO
		{
			return S5K5CAGX_PREVIEW_XGA;
		}	
#endif
	/* If it fails, return the default value. */
	return (state->oprmode == S5K5CAGX_OPRMODE_IMAGE) ? S5K5CAGX_CAPTURE_3MP : S5K5CAGX_PREVIEW_XGA;
}

static int s5k5cagx_set_framesize_index(struct v4l2_subdev *sd, unsigned int index)
{
	int i = 0;
	struct s5k5cagx_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	/* Check for video/image mode */
	for(i = 0; i < (sizeof(s5k5cagx_framesize_list)/sizeof(struct s5k5cagx_enum_framesize)); i++)
	{
		if(s5k5cagx_framesize_list[i].index == index){
			state->framesize_index = index; 
			state->pix.width = s5k5cagx_framesize_list[i].width;
			state->pix.height = s5k5cagx_framesize_list[i].height;
			s5k5cagx_msg(&client->dev, "%s: Camera Res: %dx%d\n", __func__, state->pix.width, state->pix.height);
			return 0;
		} 
	} 
	
	return -EINVAL;
}


/* if you need, add below some functions below */

static int s5k5cagx_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k5cagx_state *state = to_state(sd);
	struct s5k5cagx_userset userset = state->userset;
	int err = -EINVAL;

	s5k5cagx_msg(&client->dev, "%s: id : 0x%08lx \n", __func__, ctrl->id);

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		ctrl->value = userset.exposure_bias;
		err = 0;
		break;

	case V4L2_CID_AUTO_WHITE_BALANCE:
		ctrl->value = userset.auto_wb;
		err = 0;
		break;

	case V4L2_CID_WHITE_BALANCE_PRESET:
		ctrl->value = userset.manual_wb;
		err = 0;
		break;

	case V4L2_CID_COLORFX:
		ctrl->value = userset.effect;
		err = 0;
		break;

	case V4L2_CID_CONTRAST:
		ctrl->value = userset.contrast;
		err = 0;
		break;

	case V4L2_CID_SATURATION:
		ctrl->value = userset.saturation;
		err = 0;
		break;

	case V4L2_CID_SHARPNESS:
		ctrl->value = userset.saturation;
		err = 0;
		break;

	case V4L2_CID_CAM_JPEG_MAIN_SIZE:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_JPEG_MAIN_SIZE\n", __func__);
		err = 0;
		break;
	
	case V4L2_CID_CAM_JPEG_MAIN_OFFSET:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_JPEG_MAIN_OFFSET\n", __func__);
		err = 0;
		break;

	case V4L2_CID_CAM_JPEG_THUMB_SIZE:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_JPEG_THUMB_SIZE\n", __func__);
		err = 0;
		break;
	
	case V4L2_CID_CAM_JPEG_THUMB_OFFSET:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_JPEG_THUMB_OFFSET\n", __func__);
		err = 0;
		break;

	case V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET\n", __func__);
		err = 0;
		break; 
	
	case V4L2_CID_CAM_JPEG_MEMSIZE:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_JPEG_MEMSIZE\n", __func__);
		err = 0;
		break;

	case V4L2_CID_CAM_JPEG_QUALITY:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_JPEG_QUALITY\n", __func__);
		err = 0;
		break;

	case V4L2_CID_CAMERA_OBJ_TRACKING_STATUS:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_OBJ_TRACKING_STATUS\n", __func__);
		err = 0;
		break;

	case V4L2_CID_CAMERA_SMART_AUTO_STATUS:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_SMART_AUTO_STATUS\n", __func__);
		break;

	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_AUTO_FOCUS_RESULT\n", __func__);
		err = s5k5cagx_get_auto_focus_status(sd, ctrl);
		break;

	case V4L2_CID_CAM_DATE_INFO_YEAR:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_DATE_INFO_YEAR\n", __func__);
		err = 0;
		break; 

	case V4L2_CID_CAM_DATE_INFO_MONTH:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_DATE_INFO_MONTH\n", __func__);
		err = 0;
		break; 

	case V4L2_CID_CAM_DATE_INFO_DATE:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_DATE_INFO_DATE\n", __func__);
		err = 0;
		break; 

	case V4L2_CID_CAM_SENSOR_VER:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_SENSOR_VER\n", __func__);
		err = 0;
		break; 

	case V4L2_CID_CAM_FW_MINOR_VER:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_FW_MINOR_VER\n", __func__);
		err = 0;
		break; 

	case V4L2_CID_CAM_FW_MAJOR_VER:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_FW_MAJOR_VER\n", __func__);
		err = 0;
		break; 

	case V4L2_CID_CAM_PRM_MINOR_VER:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_PRM_MINOR_VER\n", __func__);
		err = 0;
		break; 

	case V4L2_CID_CAM_PRM_MAJOR_VER:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_PRM_MAJOR_VER\n", __func__);
		err = 0;
		break; 

	default:
		s5k5cagx_msg(&client->dev, "%s: no such ctrl\n", __func__);
		break;
	}
	
	return err;
}

static int s5k5cagx_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k5cagx_state *state = to_state(sd);
	int err = -EINVAL;

	//printk( "s5k5cagx_s_ctrl() : ctrl->id 0x%08lx, ctrl->value %d \n",ctrl->id, ctrl->value);

	switch (ctrl->id) {
		
	case V4L2_CID_CAMERA_AEAWB_LOCK_UNLOCK:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_AEAWB_LOCK_UNLOCK\n", __func__);
		err = s5k5cagx_set_ae_awb(sd, ctrl);
		break;
		
	case V4L2_CID_CAMERA_FLASH_MODE: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_FLASH_MODE\n", __func__);
		flash_mode = ctrl->value;
		err =0;
		break;

	case V4L2_CID_CAMERA_BRIGHTNESS:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_BRIGHTNESS\n", __func__);
		if(state->runmode != S5K5CAGX_RUNMODE_RUNNING)
		{
			state->ev = ctrl->value;
			err = 0;
		}
		else
		{
			err = s5k5cagx_set_ev(sd, ctrl);
		}
		break;

	case V4L2_CID_CAMERA_WHITE_BALANCE:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_AUTO_WHITE_BALANCE\n", __func__);
		if(state->runmode != S5K5CAGX_RUNMODE_RUNNING)
		{
			state->wb= ctrl->value;
			err = 0;
		}
		else
		{
			err = s5k5cagx_set_white_balance(sd, ctrl);
		}
		break;

	case V4L2_CID_CAMERA_EFFECT:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_EFFECT\n", __func__);
		if(state->runmode != S5K5CAGX_RUNMODE_RUNNING)
		{
			state->effect= ctrl->value;
			err = 0;
		}
		else
		{
			err =s5k5cagx_set_effect(sd, ctrl);
		}
		break;

	case V4L2_CID_CAMERA_ISO:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_ISO\n", __func__);
		if(state->runmode != S5K5CAGX_RUNMODE_RUNNING)
		{
			state->iso= ctrl->value;
			err = 0;
		}
		else
		{
			err =s5k5cagx_set_iso(sd, ctrl);
		}
		break;

	case V4L2_CID_CAMERA_METERING:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_METERING\n", __func__);
		if(state->runmode != S5K5CAGX_RUNMODE_RUNNING)
		{
			state->metering= ctrl->value;
			err = 0;
		}
		else
		{
			err =s5k5cagx_set_metering(sd, ctrl);
		}
		err = 0;
		break;

	case V4L2_CID_CAMERA_CONTRAST:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_CONTRAST\n", __func__);
		err = s5k5cagx_set_contrast(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_SATURATION:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_SATURATION\n", __func__);
		err =s5k5cagx_set_saturation(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_SHARPNESS:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_SHARPNESS\n", __func__);
		err = s5k5cagx_set_sharpness(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_WDR: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_WDR\n", __func__);
		//err = s5k5cagx_set_wdr(sd, ctrl);
		err =0;
		break;

	case V4L2_CID_CAMERA_ANTI_SHAKE: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_ANTI_SHAKE\n", __func__);
		//err = s5k5cagx_set_anti_shake(sd, ctrl);
		err =0;
		break;

	case V4L2_CID_CAMERA_FACE_DETECTION: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_FACE_DETECTION\n", __func__);
		//err = s5k5cagx_set_face_detection(sd, ctrl);
		err =0;
		break;

	case V4L2_CID_CAMERA_SMART_AUTO: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_SMART_AUTO\n", __func__);
		//err = s5k5cagx_set_smart_auto(sd, ctrl);
		err =0;
		break;

	case V4L2_CID_CAMERA_FOCUS_MODE:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_FOCUS_MODE\n", __func__);
		err = s5k5cagx_set_focus_mode(sd, ctrl);
		break;
		
	case V4L2_CID_CAMERA_VINTAGE_MODE: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_VINTAGE_MODE\n", __func__);
		//err = s5k5cagx_set_vintage_mode(sd, ctrl);
		err =0;
		break;
		
	case V4L2_CID_CAMERA_BEAUTY_SHOT: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_BEAUTY_SHOT\n", __func__);
		//err = s5k5cagx_set_face_beauty(sd, ctrl);
		err =0;
		break;

	case V4L2_CID_CAMERA_FACEDETECT_LOCKUNLOCK: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_FACEDETECT_LOCKUNLOCK\n", __func__);
		//err = s5k5cagx_set_face_lock(sd, ctrl);
		err =0;
		break;

	case V4L2_CID_CAM_JPEG_QUALITY: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_JPEG_QUALITY\n", __func__);
		if(ctrl->value < 0 || ctrl->value > 100){
			err = -EINVAL;
		} else {
			state->jpeg.quality = ctrl->value;
			err = 0;
		};
		break;

	case V4L2_CID_CAMERA_SCENE_MODE:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_SCENE_MODE\n", __func__);
		err = s5k5cagx_change_scene_mode(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_GPS_LATITUDE: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_GPS_LATITUDE\n", __func__);
		err = 0;
		break;

	case V4L2_CID_CAMERA_GPS_LONGITUDE: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_GPS_LONGITUDE\n", __func__);
		err = 0;
		break;

	case V4L2_CID_CAMERA_GPS_TIMESTAMP: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_GPS_TIMESTAMP\n", __func__);
		err = 0;
		break;

	case V4L2_CID_CAMERA_GPS_ALTITUDE: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_GPS_ALTITUDE\n", __func__);
		err = 0;
		break;

	case V4L2_CID_CAMERA_ZOOM: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_ZOOM\n", __func__);
		//err = s5k5cagx_set_dzoom(sd, ctrl);
		err =0;
		break;

	case V4L2_CID_CAMERA_TOUCH_AF_START_STOP: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_TOUCH_AF_START_STOP\n", __func__);
		//err = s5k5cagx_set_touch_auto_focus(sd, ctrl);
		err = 0;
		break;
		
	case V4L2_CID_CAMERA_CAF_START_STOP: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_CAF_START_STOP\n", __func__);
		if (ctrl->value)
		{
			switch(flash_mode)
				{
				case FLASH_MODE_AUTO : 
				case FLASH_MODE_ON :
					err = s5k5cagx_set_flash(100,sd);
					break;
				default : 
					err = s5k5cagx_set_flash(0,sd);
					break;
				}
		
		}
		else
		{
			err = s5k5cagx_set_flash(0,sd);
		}
		break;	

	case V4L2_CID_CAMERA_OBJECT_POSITION_X: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_OBJECT_POSITION_X\n", __func__);
		state->position.x = ctrl->value;
		err = 0;
		break;

	case V4L2_CID_CAMERA_OBJECT_POSITION_Y: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_OBJECT_POSITION_Y\n", __func__);
		state->position.y = ctrl->value;
		err = 0;
		break;

	case V4L2_CID_CAMERA_OBJ_TRACKING_START_STOP:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_OBJ_TRACKING_START_STOP\n", __func__);
		//err = s5k5cagx_set_object_tracking(sd, ctrl);
		err = 0;
		break;

	case V4L2_CID_CAMERA_SET_AUTO_FOCUS:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_SET_AUTO_FOCUS\n", __func__);
		err = s5k5cagx_set_auto_focus(sd, ctrl);
		break;		

	case V4L2_CID_CAMERA_FRAME_RATE:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_FRAME_RATE\n", __func__);
		state->fps = ctrl->value;
		err = 0;	
		break;
		
	case V4L2_CID_CAMERA_ANTI_BANDING: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_ANTI_BANDING\n", __func__);
		err = 0;	
		break;
		
	case V4L2_CID_CAMERA_SET_GAMMA: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_SET_GAMMA\n", __func__);
		err = 0;
		break;
	
	case V4L2_CID_CAMERA_SET_SLOW_AE: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_SET_SLOW_AE\n", __func__);
		err = 0;
		break;
		
	case V4L2_CID_CAMERA_BATCH_REFLECTION: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAMERA_BATCH_REFLECTION\n", __func__);
		err = 0;
		break;

	case V4L2_CID_CAM_CAPTURE:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_CAPTURE\n", __func__);
		err = s5k5cagx_set_capture_start(sd, ctrl);
		break;
	
	case V4L2_CID_CAM_PREVIEW_ONOFF:
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_PREVIEW_ONOFF\n", __func__);
		if(ctrl->value)
			err = s5k5cagx_set_preview_start(sd);
		else
			err = s5k5cagx_set_preview_stop(sd);
		break;

	case V4L2_CID_CAM_UPDATE_FW: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_UPDATE_FW\n", __func__);
		err = 0;
		break;

	case V4L2_CID_CAM_SET_FW_ADDR: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_SET_FW_ADDR\n", __func__);
		err = 0;
		break;

	case V4L2_CID_CAM_SET_FW_SIZE: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_SET_FW_SIZE\n", __func__);
		err = 0;
		break;

	case V4L2_CID_CAM_FW_VER: //NOT SUPPORT
		s5k5cagx_msg(&client->dev, "%s: V4L2_CID_CAM_FW_VER\n", __func__);
		err = 0;
		break;

	default:
		s5k5cagx_msg(&client->dev, "%s: no support control in camera sensor, S5K5CAGX\n", __func__);
		err = 0;
		break;
	}

	if (err < 0)
		goto out;
	else
		return 0;

out:
	s5k5cagx_msg(&client->dev, "%s: vidioc_s_ctrl failed\n", __func__);
	return err;
}

static int s5k5cagx_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k5cagx_state *state = to_state(sd);
	int err = -EINVAL, i;

	s5k5cagx_msg(&client->dev, "%s\n", __func__);

	s5k5cagx_init_parameters(sd);

#ifdef CONFIG_LOAD_FILE
	printk("[BestIQ] +s5k5cagx_init\n");
	err = s5k5cagx_regs_table_init();
	if (err) {
		dev_err(&client->dev, "%s: config file read fail\n", __func__);
		return -EIO;
	}
#endif

	err = s5k5cagx_i2c_write_block(sd, S5K5CAGX_INIT_SET,S5K5CAGX_INIT_SET_INDEX,"S5K5CAGX_INIT_SET");		
	if (err < 0) {
		//This is preview fail 
		s5k5cagx_msg(&client->dev, "%s: camera initialization failed. err(%d)\n", \
			__func__, state->check_previewdata);
		return -EIO;	/* FIXME */	
	}

	//This is preview success
	state->runmode =  S5K5CAGX_RUNMODE_IDLE;
	return 0;
}

/*
 * s_config subdev ops
 * With camera device, we need to re-initialize every single opening time therefor,
 * it is not necessary to be initialized on probe time. except for version checking
 * NOTE: version checking is optional
 */
static int s5k5cagx_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k5cagx_state *state = to_state(sd);
	struct s5k5cagx_platform_data *pdata;

	dev_dbg(&client->dev, "fetching platform data\n");

	pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "%s: no platform data\n", __func__);
		return -ENODEV;
	}

	/*
	 * Assign default format and resolution
	 * Use configured default information in platform data
	 * or without them, use default information in driver
	 */
	if (!(pdata->default_width && pdata->default_height)) {
		/* TODO: assign driver default resolution */
	} else {
		state->pix.width = pdata->default_width;
		state->pix.height = pdata->default_height;
	}

	if (!pdata->pixelformat)
		state->pix.pixelformat = DEFAULT_FMT;
	else
		state->pix.pixelformat = pdata->pixelformat;

	if (!pdata->freq)
		state->freq = 24000000;	/* 24MHz default */
	else
		state->freq = pdata->freq;

	if (!pdata->is_mipi) {
		state->is_mipi = 0;
		dev_dbg(&client->dev, "parallel mode\n");
	} else
		state->is_mipi = pdata->is_mipi;

	return 0;
}

static const struct v4l2_subdev_core_ops s5k5cagx_core_ops = {
	.init = s5k5cagx_init,	/* initializing API */
	.s_config = s5k5cagx_s_config,	/* Fetch platform data */
	.queryctrl = s5k5cagx_queryctrl,
	.querymenu = s5k5cagx_querymenu,
	.g_ctrl = s5k5cagx_g_ctrl,
	.s_ctrl = s5k5cagx_s_ctrl,
};

static const struct v4l2_subdev_video_ops s5k5cagx_video_ops = {
	.s_crystal_freq = s5k5cagx_s_crystal_freq,
	.g_fmt = s5k5cagx_g_fmt,
	.s_fmt = s5k5cagx_s_fmt,
	.enum_framesizes = s5k5cagx_enum_framesizes,
	.enum_frameintervals = s5k5cagx_enum_frameintervals,
	.enum_fmt = s5k5cagx_enum_fmt,
	.try_fmt = s5k5cagx_try_fmt,
	.g_parm = s5k5cagx_g_parm,
	.s_parm = s5k5cagx_s_parm,
};

static const struct v4l2_subdev_ops s5k5cagx_ops = {
	.core = &s5k5cagx_core_ops,
	.video = &s5k5cagx_video_ops,
};

/*
 * s5k5cagx_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int s5k5cagx_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct s5k5cagx_state *state;
	struct v4l2_subdev *sd;

	state = kzalloc(sizeof(struct s5k5cagx_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	sd = &state->sd;
	strcpy(sd->name, S5K5CAGX_DRIVER_NAME);

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &s5k5cagx_ops);

	dev_dbg(&client->dev, "s5k5cagx has been probed\n");
	return 0;
}


static int s5k5cagx_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	v4l2_device_unregister_subdev(sd);

	s5k5cagx_set_flash(0, sd);

	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id s5k5cagx_id[] = {
	{ S5K5CAGX_DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, s5k5cagx_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = S5K5CAGX_DRIVER_NAME,
	.probe = s5k5cagx_probe,
	.remove = s5k5cagx_remove,
	.id_table = s5k5cagx_id,
};

MODULE_DESCRIPTION("Samsung Electronics S5K5CAGX UXGA camera driver");
MODULE_AUTHOR("Jinsung Yang <jsgood.yang@samsung.com>");
MODULE_LICENSE("GPL");

