/*
 * Driver for sr130pc10 (VGA camera) from Samsung Electronics
 * 
 * 1/6" SXGA CMOS Image Sensor SoC with an Embedded Image Processor
 *
 * Copyright (C) 2010, Sungkoo Lee <skoo0.lee@samsung.com>
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
#include <media/sr130pc10_platform.h>

#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif

#include "sr130pc10.h"
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <mach/max8998_function.h>

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

//#define SXGA_CAM_DEBUG
#define CAM_TUNING_MODE

#ifdef SXGA_CAM_DEBUG
#define dev_dbg	dev_err
#endif

#define SR130PC10_DRIVER_NAME	"SR130PC10"

/* Default resolution & pixelformat. plz ref sr130pc10_platform.h */
#define DEFAULT_RES			VGA				/* Index of resoultion */
#define DEFAUT_FPS_INDEX	SR130PC10_15FPS
#define DEFAULT_FMT			V4L2_PIX_FMT_UYVY	/* YUV422 */
#define DEFAULT_WIDTH	640
#define DEFAULT_HEIGHT	480

//NAGSM_HQ_CAMERA_LEESUNGKOO_20101231 : to support two type front camera
extern int gcamera_sensor_front_type;
extern int gcamera_sensor_front_checked;

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

static int sr130pc10_init(struct v4l2_subdev *sd, u32 val);

/* Camera functional setting values configured by user concept */
struct sr130pc10_userset {
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

struct sr130pc10_state {
	struct sr130pc10_platform_data *pdata;
	struct v4l2_subdev sd;
	struct v4l2_fract timeperframe;
	struct sr130pc10_userset userset;
	struct v4l2_pix_format req_fmt;
	struct v4l2_pix_format set_fmt;	
	int framesize_index;
	int freq;	/* MCLK in KHz */
	int is_mipi;
	int isize;
	int ver;
	int fps;
	int vt_mode; /*For VT camera*/
	int check_dataline;
	int check_previewdata;
};

enum {
	sr130pc10_PREVIEW_VGA,
} sr130pc10_FRAME_SIZE;

struct sr130pc10_enum_framesize {
	unsigned int index;
	unsigned int width;
	unsigned int height;	
};

static char *sr130pc10_cam_tunning_table = NULL;

static int sr130pc10_cam_tunning_table_size;

static unsigned short sr130pc10_cam_tuning_data[1] = {
0xffff
};

static int CamTunningStatus = 1;

static inline struct sr130pc10_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct sr130pc10_state, sd);
}

/**
 * sr130pc10_i2c_read: Read 2 bytes from sensor 
 */

static inline int sr130pc10_i2c_read(struct i2c_client *client, 
	unsigned char subaddr, unsigned char *data)
{
	unsigned char buf[1];
	int err = 0;
	struct i2c_msg msg = {client->addr, 0, 1, buf};

	buf[0] = subaddr;
	
	printk(KERN_DEBUG "sr130pc10_i2c_read address buf[0] = 0x%x!!", buf[0]); 	
	
	err = i2c_transfer(client->adapter, &msg, 1);
	if (unlikely(err < 0))
		printk(KERN_DEBUG "%s: register read fail\n", __func__);

	msg.flags = I2C_M_RD;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (unlikely(err < 0))
		printk(KERN_DEBUG "%s: register read fail\n", __func__);	

	/*
	 * [Arun c]Data comes in Little Endian in parallel mode; So there
	 * is no need for byte swapping here
	 */
	*data = buf[0];
	
	return err;
}

static int sr130pc10_i2c_write_unit(struct i2c_client *client, unsigned char addr, unsigned char val)
{
	struct i2c_msg msg[1];
	unsigned char reg[2];
	int err = 0;
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;

	// NAGSM_ANDROID_HQ_CAMERA_SORACHOI_20110118 : sensor delay
	if ((addr == REG_DELAY) && (val > 0))
	{
		mdelay(val*10); // val * 10ms
		dev_err(&client->dev, "%s: delay: %d ms\n", __func__, val*10);
		return err;
	}  

again:
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = reg;

	reg[0] = addr ;
	reg[1] = val ;	

	//printk("[wsj] ---> address: 0x%02x, value: 0x%02x\n", reg[0], reg[1]);
	
	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
		return err;	/* Returns here on success */

	/* abnormal case: retry 5 times */
	if (retry < 5) {
		dev_err(&client->dev, "%s: address: 0x%02x, " \
			"value: 0x%02x\n", __func__, \
			reg[0], reg[1]);
		retry++;
		goto again;
	}

	return err;
}

static int sr130pc10_i2c_write(struct v4l2_subdev *sd, unsigned short i2c_data[], 
							int index)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i =0, err =0;
	int delay;
	int count = index/sizeof(unsigned short);

	dev_dbg(&client->dev, "%s: count %d \n", __func__, count);
	for (i=0 ; i <count ; i++) {
		err = sr130pc10_i2c_write_unit(client, i2c_data[i] >>8, i2c_data[i] & 0xff );
		if (unlikely(err < 0)) {
			v4l_info(client, "%s: register set failed\n", \
			__func__);
			return err;
		}
	}
	return 0;
}

//NAGSM_HQ_CAMERA_LEESUNGKOO_20110103
static int sr130pc10_check_sensorId(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;
	unsigned char read_value;
	
	if (gcamera_sensor_front_checked) //NAGSM_HQ_CAMERA_LEESUNGKOO_20110223 : to support two type front camera
	{
		printk("%s: gcamera_sensor_front_checked(%d) gcamera_sensor_front_type(%d)\n", __func__ , gcamera_sensor_front_checked ,gcamera_sensor_front_type);
		if (gcamera_sensor_front_type == CAMERA_SENSOR_ID_FRONT_SR130PC10)
			return 0;
		else
			return -EIO;
	}
	
	err = sr130pc10_i2c_read(client, 0x04, &read_value); //device ID
	if(err < 0)
	{
		dev_err(&client->dev, "%s Fail \n", __func__); 
	}
	else
	{
		dev_err(&client->dev, "sr130pc10 Device ID 0x94 = 0x%x !!\n", read_value); 
		gcamera_sensor_front_type = CAMERA_SENSOR_ID_FRONT_SR130PC10; //NAGSM_HQ_CAMERA_LEESUNGKOO_20110223 : to support two type front camera
		gcamera_sensor_front_checked = true;
	}

	return err;
} 
//NAGSM_HQ_CAMERA_LEESUNGKOO_20110103

int sr130pc10_CamTunning_table_init(void)
{
#if !defined(CAM_TUNING_MODE)
	printk(KERN_DEBUG "%s always sucess, L = %d!!", __func__, __LINE__);
	return 1; // return 1 is binary mode.
#endif

	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int i;
	int ret;
	mm_segment_t fs = get_fs();

	printk(KERN_DEBUG "%s %d\n", __func__, __LINE__);

	set_fs(get_ds());

	filp = filp_open("/mnt/sdcard/external_sd/sr130pc10.h", O_RDONLY, 0);

	if (IS_ERR(filp)) {
		printk(KERN_ERR "file open error or sr130pc10.h file is not.\n");
		return 1; //binary mode
	}
	
	l = filp->f_path.dentry->d_inode->i_size;	
	printk(KERN_DEBUG "l = %ld\n", l);
	dp = kmalloc(l, GFP_KERNEL);
	if (dp == NULL) {
		printk(KERN_ERR "Out of Memory\n");
		filp_close(filp, current->files);
	}
	
	pos = 0;
	memset(dp, 0, l);
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	
	if (ret != l) {
		printk(KERN_ERR "Failed to read file ret = %d\n", ret);
		vfree(dp);
		filp_close(filp, current->files);
		return -EINVAL;
	}

	filp_close(filp, current->files);
		
	set_fs(fs);
	
	sr130pc10_cam_tunning_table = dp;
		
	sr130pc10_cam_tunning_table_size = l;
	
	*((sr130pc10_cam_tunning_table + sr130pc10_cam_tunning_table_size) - 1) = '\0';
	
	printk(KERN_DEBUG "sr130pc10_regs_table 0x%08x, %ld\n", dp, l);
	printk(KERN_DEBUG "%s end, line = %d\n",__func__, __LINE__);
	
	return 0; // tuning mode
}

static int sr130pc10_regs_table_write(struct v4l2_subdev *sd, char *name)
{
	char *start, *end, *reg;	
	unsigned short addr;
	unsigned int count = 0;
	char reg_buf[7];
	struct i2c_client *client = v4l2_get_subdevdata(sd);	

	dev_err(&client->dev, "%s start, name = %s\n",__func__, name);

	*(reg_buf + 6) = '\0';

	start = strstr(sr130pc10_cam_tunning_table, name);
	end = strstr(start, "};");

	while (1) {	
		/* Find Address */	
		reg = strstr(start,"0x");		
		if ((reg == NULL) || (reg > end))
		{
			break;
		}

		if (reg)
			start = (reg + 8); 

		/* Write Value to Address */	
		if (reg != NULL) {
			memcpy(reg_buf, reg, 6);	
			addr = (unsigned short)simple_strtoul(reg_buf, NULL, 16); 
			if (((addr&0xff00)>>8) == 0xff)
			{
				mdelay((addr&0xff)*10); //default 10ms
				dev_err(&client->dev, "delay 0x%04x,\n", addr&0xff);
			}	
			else
			{
#ifdef SXGA_CAM_DEBUG
				dev_dbg(&client->dev, "addr = 0x%x, ", addr);
				if((count%10) == 0)
					dev_dbg(&client->dev, "\n");
#endif
				sr130pc10_cam_tuning_data[0] = addr;
				sr130pc10_i2c_write(sd, sr130pc10_cam_tuning_data, 2); // 2byte
			}
		}
		count++;
	}
	dev_err(&client->dev, "count = %d, %s end, line = %d\n", count, __func__, __LINE__);
	return 0;
}

static int sr130pc10_regs_write(struct v4l2_subdev *sd, unsigned short i2c_data[], unsigned short length, char *name)
{
	int err = -EINVAL;	
	struct i2c_client *client = v4l2_get_subdevdata(sd);	
	
	dev_err(&client->dev, "%s start, Status is %s mode, parameter name = %s\n",\
						__func__, (CamTunningStatus != 0) ? "binary"  : "tuning",name);
	
	 if(CamTunningStatus) // binary mode
 	{
		err = sr130pc10_i2c_write(sd, i2c_data, length);
 	}
	 else // cam tuning mode
 	{
		err = sr130pc10_regs_table_write(sd, name);
 	}

	return err;
}				

const char **sr130pc10_ctrl_get_menu(u32 id)
{
	printk(KERN_DEBUG "sr130pc10_ctrl_get_menu is called... id : %d \n", id);

	switch (id) {
	default:
		return v4l2_ctrl_get_menu(id);
	}
}

static inline struct v4l2_queryctrl const *sr130pc10_find_qctrl(int id)
{
	int i;

	printk(KERN_DEBUG "sr130pc10_find_qctrl is called...  id : %d \n", id);

	return NULL;
}

static int sr130pc10_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i;
	struct i2c_client *client = v4l2_get_subdevdata(sd);	

	dev_dbg(&client->dev, "sr130pc10_queryctrl is called... \n");

	return -EINVAL;
}

static int sr130pc10_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm)
{
	struct v4l2_queryctrl qctrl;
	struct i2c_client *client = v4l2_get_subdevdata(sd);	

	dev_err(&client->dev, "sr130pc10_querymenu is called... \n");

	qctrl.id = qm->id;
	sr130pc10_queryctrl(sd, &qctrl);

	return v4l2_ctrl_query_menu(qm, &qctrl, sr130pc10_ctrl_get_menu(qm->id));
}

/*
 * Clock configuration
 * Configure expected MCLK from host and return EINVAL if not supported clock
 * frequency is expected
 * 	freq : in Hz
 * 	flag : not supported for now
 */
static int sr130pc10_s_crystal_freq(struct v4l2_subdev *sd, u32 freq, u32 flags)
{
	int err = -EINVAL;
	struct i2c_client *client = v4l2_get_subdevdata(sd);	

	dev_dbg(&client->dev, "sr130pc10_s_crystal_freq is called... \n");

	return err;
}

static int sr130pc10_g_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);	

	dev_dbg(&client->dev, "sr130pc10_g_fmt is called... \n");

	return err;
}

static int sr130pc10_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;
	struct sr130pc10_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);	

	/*
	 * Just copying the requested format as of now.
	 * We need to check here what are the formats the camera support, and
	 * set the most appropriate one according to the request from FIMC
	 */
	state->req_fmt.width = fmt->fmt.pix.width;
	state->req_fmt.height = fmt->fmt.pix.height;
	state->set_fmt.width = fmt->fmt.pix.width;
	state->set_fmt.height = fmt->fmt.pix.height;

	state->req_fmt.pixelformat = fmt->fmt.pix.pixelformat;
	state->req_fmt.colorspace = fmt->fmt.pix.colorspace;	

	dev_err(&client->dev, "%s : width - %d , height - %d\n", __func__, state->req_fmt.width, state->req_fmt.height);

	return err;
}
static int sr130pc10_enum_framesizes(struct v4l2_subdev *sd, \
					struct v4l2_frmsizeenum *fsize)
{
	int err = 0;

	struct sr130pc10_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);	
	
	/*
	 * Return the actual output settings programmed to the camera
	 */
	fsize->discrete.width = state->set_fmt.width;
	fsize->discrete.height = state->set_fmt.height;
	
	dev_err(&client->dev, "%s : width - %d , height - %d\n", __func__, fsize->discrete.width, fsize->discrete.height);	

	return err;
}


static int sr130pc10_enum_frameintervals(struct v4l2_subdev *sd, 
					struct v4l2_frmivalenum *fival)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);	

	dev_dbg(&client->dev, "sr130pc10_enum_frameintervals is called... \n");
	
	return err;
}

static int sr130pc10_enum_fmt(struct v4l2_subdev *sd, struct v4l2_fmtdesc *fmtdesc)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);	

	dev_dbg(&client->dev, "sr130pc10_enum_fmt is called... \n");

	return err;
}

static int sr130pc10_try_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);	

	dev_dbg(&client->dev, "sr130pc10_enum_fmt is called... \n");

	return err;
}

static int sr130pc10_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;

	dev_dbg(&client->dev, "%s\n", __func__);

	return err;
}

static int sr130pc10_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;

	dev_dbg(&client->dev, "%s: numerator %d, denominator: %d\n", \
		__func__, param->parm.capture.timeperframe.numerator, \
		param->parm.capture.timeperframe.denominator);

	return err;
}

 static int sr130pc10_reset(struct v4l2_subdev *sd)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);	
	
	dev_dbg(&client->dev, "sr130pc10_reset is called... \n");
	
	return err;	
}

/* set sensor register values for adjusting brightness */
static int sr130pc10_set_brightness(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sr130pc10_state *state = to_state(sd);

	int err = -EINVAL;
	int ev_value = 0;

	dev_err(&client->dev, "%s: value : %d state->vt_mode %d \n", __func__, ctrl->value, state->vt_mode);

	ev_value = ctrl->value;

	if(state->vt_mode == 1)
	{
		switch(ev_value)
		{	
			case EV_MINUS_4:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_vt_m4, sizeof(sr130pc10_ev_vt_m4), "sr130pc10_ev_vt_m4");
			break;

			case EV_MINUS_3:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_vt_m3, sizeof(sr130pc10_ev_vt_m3), "sr130pc10_ev_vt_m3");
			break;

			
			case EV_MINUS_2:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_vt_m2, sizeof(sr130pc10_ev_vt_m2), "sr130pc10_ev_vt_m2");
			break;
			
			case EV_MINUS_1:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_vt_m1, sizeof(sr130pc10_ev_vt_m1), "sr130pc10_ev_vt_m1");
			break;

			case EV_DEFAULT:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_vt_default, sizeof(sr130pc10_ev_vt_default), "sr130pc10_ev_vt_default");
			break;

			case EV_PLUS_1:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_vt_p1, sizeof(sr130pc10_ev_vt_p1), "sr130pc10_ev_vt_p1");
 			break;

			case EV_PLUS_2:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_vt_p2, sizeof(sr130pc10_ev_vt_p2), "sr130pc10_ev_vt_p2");
 			break;

			case EV_PLUS_3:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_vt_p3, sizeof(sr130pc10_ev_vt_p3), "sr130pc10_ev_vt_p3");
 			break;

			case EV_PLUS_4:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_vt_p4, sizeof(sr130pc10_ev_vt_p4), "sr130pc10_ev_vt_p4");
 			break;	
			
			default:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_vt_default, sizeof(sr130pc10_ev_vt_default), "sr130pc10_ev_vt_default");
 			break;
		}
	}
	else
	{
		switch(ev_value)
		{	
			case EV_MINUS_4:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_m4, sizeof(sr130pc10_ev_m4), "sr130pc10_ev_m4");
 			break;

			case EV_MINUS_3:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_m3, sizeof(sr130pc10_ev_m3), "sr130pc10_ev_m3");
 			break;
			
			case EV_MINUS_2:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_m2, sizeof(sr130pc10_ev_m2), "sr130pc10_ev_m2");
 			break;
			
			case EV_MINUS_1:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_m1, sizeof(sr130pc10_ev_m1), "sr130pc10_ev_m1");
 			break;

			case EV_DEFAULT:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_default, sizeof(sr130pc10_ev_default), "sr130pc10_ev_default");
 			break;

			case EV_PLUS_1:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_p1, sizeof(sr130pc10_ev_p1), "sr130pc10_ev_p1");
 			break;

			case EV_PLUS_2:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_p2, sizeof(sr130pc10_ev_p2), "sr130pc10_ev_p2");
 			break;

			case EV_PLUS_3:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_p3, sizeof(sr130pc10_ev_p3), "sr130pc10_ev_p3");
			break;

			case EV_PLUS_4:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_p4, sizeof(sr130pc10_ev_p4), "sr130pc10_ev_p4");
			break;	
			
			default:
				err = sr130pc10_regs_write(sd, sr130pc10_ev_default, sizeof(sr130pc10_ev_default), "sr130pc10_ev_default");
			break;
		}
	}
	if (err < 0)
	{
		dev_err(&client->dev, "%s: register set failed\n", __func__);
		return -EIO;
	}
	return err;
}

/* set sensor register values for adjusting whitebalance, both auto and manual */
static int sr130pc10_set_wb(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;

	dev_dbg(&client->dev, "%s:  value : %d \n", __func__, ctrl->value);

	switch(ctrl->value)
	{
	case WHITE_BALANCE_AUTO:
		err = sr130pc10_regs_write(sd, sr130pc10_wb_auto, sizeof(sr130pc10_wb_auto), "sr130pc10_wb_auto");
		break;

	case WHITE_BALANCE_SUNNY:
		err = sr130pc10_regs_write(sd, sr130pc10_wb_sunny, sizeof(sr130pc10_wb_sunny), "sr130pc10_wb_sunny");
		break;

	case WHITE_BALANCE_CLOUDY:
		err = sr130pc10_regs_write(sd, sr130pc10_wb_cloudy, sizeof(sr130pc10_wb_cloudy), "sr130pc10_wb_cloudy");
		break;

	case WHITE_BALANCE_TUNGSTEN:
		err = sr130pc10_regs_write(sd, sr130pc10_wb_tungsten, sizeof(sr130pc10_wb_tungsten), "sr130pc10_wb_tungsten");
		break;

	case WHITE_BALANCE_FLUORESCENT:
		err = sr130pc10_regs_write(sd, sr130pc10_wb_fluorescent, sizeof(sr130pc10_wb_fluorescent), "sr130pc10_wb_fluorescent");
		break;

	default:
		dev_dbg(&client->dev, "%s: Not Support value \n", __func__);
		err = 0;
		break;

	}
	return err;
}

/* set sensor register values for adjusting color effect */
static int sr130pc10_set_effect(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;

	dev_dbg(&client->dev, "%s: value : %d \n", __func__, ctrl->value);

	switch(ctrl->value)
	{
	case IMAGE_EFFECT_NONE:
		err = sr130pc10_regs_write(sd, sr130pc10_effect_none, sizeof(sr130pc10_effect_none), "sr130pc10_effect_none");
		break;

	case IMAGE_EFFECT_BNW:		//Gray
		err = sr130pc10_regs_write(sd, sr130pc10_effect_gray, sizeof(sr130pc10_effect_gray), "sr130pc10_effect_gray");
		break;

	case IMAGE_EFFECT_SEPIA:
		err = sr130pc10_regs_write(sd, sr130pc10_effect_sepia, sizeof(sr130pc10_effect_sepia), "sr130pc10_effect_sepia");
		break;

	case IMAGE_EFFECT_AQUA:
		err = sr130pc10_regs_write(sd, sr130pc10_effect_aqua, sizeof(sr130pc10_effect_aqua), "sr130pc10_effect_aqua");
		break;

	case IMAGE_EFFECT_NEGATIVE:
		err = sr130pc10_regs_write(sd, sr130pc10_effect_negative, sizeof(sr130pc10_effect_negative), "sr130pc10_effect_negative");
		break;

	default:
		dev_err(&client->dev, "%s: Not Support value \n", __func__);
		err = 0;
		break;

	}
	
	return err;
}

/* set sensor register values for frame rate(fps) setting */
static int sr130pc10_set_frame_rate(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sr130pc10_state *state = to_state(sd);

	int err = -EINVAL;
	int i = 0;

	dev_err(&client->dev, "%s: value : %d, state->vt_mode : %d \n", __func__, ctrl->value, state->vt_mode);
	
	if(state->vt_mode == 1)
	{
		switch(ctrl->value)
		{
		case 7:
			err = sr130pc10_regs_write(sd, sr130pc10_vt_fps_7, sizeof(sr130pc10_vt_fps_7), "sr130pc10_vt_fps_7");
			break;

		case 10:
			err = sr130pc10_regs_write(sd, sr130pc10_vt_fps_10, sizeof(sr130pc10_vt_fps_10), "sr130pc10_vt_fps_10");
			break;
			
		case 15:
			err = sr130pc10_regs_write(sd, sr130pc10_vt_fps_15, sizeof(sr130pc10_vt_fps_15), "sr130pc10_vt_fps_15");
			break;

		default:
			dev_dbg(&client->dev, "%s: Not Support value \n", __func__);
			err = 0;
			break;
		}
	}
#if 0
	else
	{
		switch(ctrl->value)
		{
		case 7:
			err = sr130pc10_regs_write(sd, sr130pc10_fps_7, sizeof(sr130pc10_fps_7), "sr130pc10_fps_7");
			break;

		case 10:
			err = sr130pc10_regs_write(sd, sr130pc10_fps_10, sizeof(sr130pc10_fps_10), "sr130pc10_fps_10");
			break;
			
		case 15:
			err = sr130pc10_regs_write(sd, sr130pc10_fps_15, sizeof(sr130pc10_fps_15), "sr130pc10_fps_15");
			break;

		default:
			dev_err(&client->dev, "%s: Not Support value \n", __func__);
			err = 0;
			break;
		}
	}
#endif
	return err;
}

static int sr130pc10_check_dataline_stop(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sr130pc10_state *state = to_state(sd);
	int err = -EINVAL, i;

	dev_dbg(&client->dev, "%s\n", __func__);

	err = sr130pc10_regs_write(sd, sr130pc10_dataline_stop, \
				sizeof(sr130pc10_dataline_stop), "sr130pc10_dataline_stop");
	if (err < 0)
	{
		dev_err(&client->dev, "%s: register set failed\n", __func__);
		return -EIO;
	}

	state->check_dataline = 0;

	return err;
}

static int sr130pc10_set_preview_start(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int err = -EINVAL;

	err = sr130pc10_regs_write(sd, sr130pc10_preview_reg, \
		sizeof(sr130pc10_preview_reg), "sr130pc10_preview_reg");
	if (err < 0)
	{
		dev_err(&client->dev, "%s: register set failed\n", \
		__func__);
	}

	return err;
}

static int sr130pc10_set_preview_stop(struct v4l2_subdev *sd)
{
	int err = 0;

	return err;
}

/* if you need, add below some functions below */

static int sr130pc10_set_capture_start(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	int err = -EINVAL;
	
	err = sr130pc10_regs_write(sd, sr130pc10_capture_reg, \
		sizeof(sr130pc10_capture_reg), "sr130pc10_capture_reg");
	if (err < 0)
	{
		dev_err(&client->dev, "%s: register set failed\n", \
		__func__);
	}

	return err;
}

static int sr130pc10_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sr130pc10_state *state = to_state(sd);
	struct sr130pc10_userset userset = state->userset;
	int err = -EINVAL;

	dev_dbg(&client->dev, "%s: id : 0x%08x \n", __func__, ctrl->id);

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

//NAGSM_HQ_CAMERA_LEESUNGKOO_20110103
	case V4L2_CID_CAM_FRONT_SENSOR_TYPE:
		err = sr130pc10_check_sensorId(sd);
		break;
//NAGSM_HQ_CAMERA_LEESUNGKOO_20110103	

	default:
		dev_err(&client->dev, "%s: no such ctrl\n", __func__);
		break;
	}
	
	return err;
}

static int sr130pc10_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
#ifdef SR130PC10_COMPLETE //NAGSM_HQ_CAMERA_LEESUNGKOO_20110103
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sr130pc10_state *state = to_state(sd);

	int err = 0;

	dev_err(&client->dev, "sr130pc10_s_ctrl() : ctrl->id %d, ctrl->value %d \n",ctrl->id - V4L2_CID_PRIVATE_BASE, ctrl->value);

	switch (ctrl->id) {

	case V4L2_CID_CAMERA_BRIGHTNESS:	//V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_BRIGHTNESS\n", __func__);
		err = sr130pc10_set_brightness(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_CAPTURE:
		dev_dbg(&client->dev, "V4L2_CID_CAM_CAPTURE [] \n");
		err = sr130pc10_set_capture_start(sd);
		break;

	case V4L2_CID_CAM_PREVIEW_ONOFF:
		if(state->check_dataline)
		{	
			err = sr130pc10_regs_write(sd, sr130pc10_dataline, \
						sizeof(sr130pc10_dataline), "sr130pc10_dataline");
		}
		else
		{
			if (ctrl->value)
			{
				err = sr130pc10_set_preview_start(sd);
			}
			else
			{
				err = sr130pc10_set_preview_stop(sd);
			}
		}

		dev_err(&client->dev, "V4L2_CID_CAM_PREVIEW_ONOFF [%d], check_dataline[%d] \n", ctrl->value, state->check_dataline);
		break;

	case V4L2_CID_CAMERA_FRAME_RATE:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_FRAME_RATE\n", __func__);
		if(state->vt_mode)
		{
			err = sr130pc10_set_frame_rate(sd, ctrl);	
		}
		break;
		
	case V4L2_CID_CAMERA_VT_MODE:
		state->vt_mode = ctrl->value;
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_VT_MODE : state->vt_mode %d \n", __func__, state->vt_mode);
		break;
		
	case V4L2_CID_CAMERA_WHITE_BALANCE: //V4L2_CID_AUTO_WHITE_BALANCE:
		dev_dbg(&client->dev, "%s: V4L2_CID_AUTO_WHITE_BALANCE\n", __func__);
		err = sr130pc10_set_wb(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_EFFECT:	//V4L2_CID_COLORFX:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_EFFECT\n", __func__);
		err = sr130pc10_set_effect(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_CHECK_DATALINE:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_CHECK_DATALINE[%d]\n", __func__, ctrl->value);
		state->check_dataline = ctrl->value;
		break;	

	case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
		err = sr130pc10_check_dataline_stop(sd);
		break;

	default:
		dev_err(&client->dev, "%s: no support control in camera sensor, sr130pc10\n", __func__);
		break;
	}

	if (err < 0)
		goto out;
	else
		return 0;

out:
	dev_err(&client->dev, "%s: vidioc_s_ctrl failed, cmd[%d], err[%d]\n", __func__, ctrl->id - V4L2_CID_PRIVATE_BASE, err);
	return err;
#else
	return 0;
#endif
}

static int sr130pc10_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sr130pc10_state *state = to_state(sd);
	int err = -EINVAL, i;
	
#ifdef SXGA_CAM_DEBUG
	unsigned char read_value;
#endif

	if (!gcamera_sensor_front_checked) //NAGSM_HQ_CAMERA_LEESUNGKOO_20110223 : to support two type front camera
		return -EIO;

	//v4l_info(client, "%s: camera initialization start : state->vt_mode %d \n", __func__, state->vt_mode);
	printk(KERN_DEBUG "camera initialization start, state->vt_mode : %d \n", state->vt_mode); 
	printk(KERN_DEBUG "state->check_dataline : %d \n", state->check_dataline); 
	CamTunningStatus = sr130pc10_CamTunning_table_init();
	err = CamTunningStatus;
	if (CamTunningStatus==0) { // if CamTunningStatus is 0, meaning is tuning mode
		msleep(100);
	}
			
	if(state->vt_mode == 0)
	{

#if defined(SXGA_CAM_DEBUG)
		sr130pc10_i2c_read(client, 0x04, &read_value); //device ID
		dev_dbg(&client->dev, "sr130pc10 Device ID 0x94 = 0x%x \n!!", read_value); 
		mdelay(3);
#endif	

		if(0)
		{	
			err = sr130pc10_i2c_write(sd, sr130pc10_dataline, \
						sizeof(sr130pc10_dataline));
			if (err < 0)
			{
				v4l_info(client, "%s: register set failed\n", \
				__func__);
			}	
		}
		else
		{
			err = sr130pc10_regs_write(sd, sr130pc10_init_reg, \
				sizeof(sr130pc10_init_reg), "sr130pc10_init_reg");
			if (err < 0)
			{
				v4l_info(client, "%s: register set failed\n", \
				__func__);
			}
#if defined(SXGA_CAM_DEBUG)
			else
			{
				dev_dbg(&client->dev, "sr130pc10_init_reg done\n!!"); 
				mdelay(2);
			}
#endif	
		}
	}
	else
	{
		err = sr130pc10_regs_write(sd, sr130pc10_init_vt_reg, \
					sizeof(sr130pc10_init_vt_reg), "sr130pc10_init_vt_reg");
		if (err < 0)
		{
			v4l_info(client, "%s: register set failed\n", \
			__func__);
		}	
	}

	if (err < 0) {
		//This is preview fail 
		state->check_previewdata = 100;
		v4l_err(client, "%s: camera initialization failed. err(%d)\n", \
			__func__, state->check_previewdata);
		return -EIO;	/* FIXME */	
	}

	state->set_fmt.width = DEFAULT_WIDTH;
	state->set_fmt.height = DEFAULT_HEIGHT;		

	//This is preview success
	state->check_previewdata = 0;
	return 0;
}

/*
 * s_config subdev ops
 * With camera device, we need to re-initialize every single opening time therefor,
 * it is not necessary to be initialized on probe time. except for version checking
 * NOTE: version checking is optional
 */
static int sr130pc10_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sr130pc10_state *state = to_state(sd);
	struct sr130pc10_platform_data *pdata;

	dev_info(&client->dev, "fetching platform data\n");

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
	if (!(pdata->default_width && pdata->default_height)) 
	{
		state->req_fmt.width = DEFAULT_WIDTH;
		state->req_fmt.height = DEFAULT_HEIGHT;
	}
	else 
	{
		state->req_fmt.width = pdata->default_width;
		state->req_fmt.height = pdata->default_height;

		printk("%s : width - %d , height - %d\n", __func__, state->req_fmt.width, state->req_fmt.height);
	}

	if (!pdata->pixelformat)
	{
		state->req_fmt.pixelformat = DEFAULT_FMT;
	}
	else
	{
		state->req_fmt.pixelformat = pdata->pixelformat;
	}

	return 0;
}

static const struct v4l2_subdev_core_ops sr130pc10_core_ops = {
	.init = sr130pc10_init,	/* initializing API */
	.s_config = sr130pc10_s_config,	/* Fetch platform data */
	.queryctrl = sr130pc10_queryctrl,
	.querymenu = sr130pc10_querymenu,
	.g_ctrl = sr130pc10_g_ctrl,
	.s_ctrl = sr130pc10_s_ctrl,
};

static const struct v4l2_subdev_video_ops sr130pc10_video_ops = {
	.s_crystal_freq = sr130pc10_s_crystal_freq,
	.g_fmt = sr130pc10_g_fmt,
	.s_fmt = sr130pc10_s_fmt,
	.enum_framesizes = sr130pc10_enum_framesizes,
	.enum_frameintervals = sr130pc10_enum_frameintervals,
	.enum_fmt = sr130pc10_enum_fmt,
	.try_fmt = sr130pc10_try_fmt,
	.g_parm = sr130pc10_g_parm,
	.s_parm = sr130pc10_s_parm,
};

static const struct v4l2_subdev_ops sr130pc10_ops = {
	.core = &sr130pc10_core_ops,
	.video = &sr130pc10_video_ops,
};

/*
 * sr130pc10_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int sr130pc10_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct sr130pc10_state *state;
	struct v4l2_subdev *sd;

	state = kzalloc(sizeof(struct sr130pc10_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	sd = &state->sd;
	strcpy(sd->name, SR130PC10_DRIVER_NAME);

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &sr130pc10_ops);

	dev_dbg(&client->dev, "sr130pc10 has been probed\n");
	return 0;
}


static int sr130pc10_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id sr130pc10_id[] = {
	{ SR130PC10_DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, sr130pc10_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = SR130PC10_DRIVER_NAME,
	.probe = sr130pc10_probe,
	.remove = sr130pc10_remove,
	.id_table = sr130pc10_id,
};

MODULE_DESCRIPTION("Samsung Electronics sr130pc10 SXGA camera driver");
MODULE_AUTHOR("Sungkoo Lee <skoo0.lee@samsung.com>");
MODULE_LICENSE("GPL");

