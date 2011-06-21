/*
 * Driver for M5MO (5MP Camera) from NEC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/i2c.h>
#include <media/v4l2-i2c-drv.h>
#include <media/v4l2-device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>

#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif

#include <media/m5mo_platform.h>
#include "m5mo.h"


#define M5MO_DRIVER_NAME		"M5MO"

#define M5MO_FSI_FW_PATH			"/system/firmware/FSI/RS_M5LS.bin"
#define M5MO_BSI_FW_PATH			"/system/firmware/BSI/RS_M5LS.bin"

#define SDCARD_FW
#ifdef SDCARD_FW
#define M5MO_SD_FW_PATH			"/mnt/sdcard/RS_M5LS.bin"
#endif

#define M5MO_FW_VER_LEN			6 //21 //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110216
#define M5MO_FW_VER_FILE_CUR	0x16FF00

#define M5MO_FLASH_BASE_ADDR	0x10000000
#define M5MO_INT_RAM_BASE_ADDR	0x68000000

#define M5MO_I2C_RETRY			5
#define M5MO_I2C_VERIFY			100
#define M5MO_ISP_INT_TIMEOUT	1000

#define M5MO_JPEG_MAXSIZE		0x3A0000
#define M5MO_THUMB_MAXSIZE		0xFC00
#define M5MO_POST_MAXSIZE		0xBB800

#define M5MO_DEF_APEX_DEN	100

#define m5mo_readb(c, g, b, v)		m5mo_read(c, 1, g, b, v)
#define m5mo_readw(c, g, b, v)		m5mo_read(c, 2, g, b, v)
#define m5mo_readl(c, g, b, v)		m5mo_read(c, 4, g, b, v)

#define m5mo_writeb(c, g, b, v)		m5mo_write(c, 1, g, b, v)
#define m5mo_writew(c, g, b, v)		m5mo_write(c, 2, g, b, v)
#define m5mo_writel(c, g, b, v)		m5mo_write(c, 4, g, b, v)

#define CHECK_ERR(x)   if ((x) < 0) { \
							cam_err("i2c falied, err %d\n", x); \
							return x; \
						}

//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101215
static int IS_M5MO_FSI = 0;

static const struct m5mo_frmsizeenum m5mo_pre_frmsizes[] = {
	{ M5MO_PREVIEW_QCIF,	176,		144,		0x05 },
	{ M5MO_PREVIEW_QVGA,	320,		240,		0x09 },
	{ M5MO_PREVIEW_3XQCIF,	528,		432,		0x2C }, //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110130
	{ M5MO_PREVIEW_VGA,		640,		480,		0x17 },
	{ M5MO_PREVIEW_D1,		720,		480,		0x18 },
	{ M5MO_PREVIEW_WVGA,	800,		480,		0x1A },
	{ M5MO_PREVIEW_720P,	1280,	720,		0x21 },
	{ M5MO_PREVIEW_1080P,	1920,	1080,	0x28 },	
};

static const struct m5mo_frmsizeenum m5mo_cap_frmsizes[] = {
	{ M5MO_CAPTURE_VGA,		640,		480,		0x09 },
	{ M5MO_CAPTURE_WVGA,	800,		480,		0x0A },
	{ M5MO_CAPTURE_W1MP,	1600,	960,		0x2B },
	{ M5MO_CAPTURE_2MP,		1600,	1200,	0x17 },
	{ M5MO_CAPTURE_W2MP,	2048,	1232,	0x2C },
	{ M5MO_CAPTURE_3MP,		2048,	1536,	0x1B },
	{ M5MO_CAPTURE_W4MP,	2560,	1536,	0x1D }, //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110113
	{ M5MO_CAPTURE_5MP,		2560,	1920,	0x1F },
	{ M5MO_CAPTURE_W7MP,	3264,	1968,	0x2D },
	{ M5MO_CAPTURE_8MP,		3264,	2448,	0x25 }, 
};

static struct m5mo_control m5mo_ctrls[] = {
	{
		.id = V4L2_CID_CAMERA_SENSOR_MODE,
		.minimum = SENSOR_CAMERA,
		.maximum = SENSOR_CAMCORDER,
		.step = 1,
		.value = SENSOR_CAMERA,
		.default_value = SENSOR_CAMERA,
	}, {
#if 0 //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110118	
		.id = V4L2_CID_CAMERA_FLASH_MODE,
		.minimum = FLASH_MODE_BASE + 1,
		.maximum = FLASH_MODE_MAX - 1,
		.step = 1,
		.value = FLASH_MODE_OFF,
		.default_value = FLASH_MODE_OFF,
	}, {
#endif	
		.id = V4L2_CID_CAMERA_BRIGHTNESS,
		.minimum = EV_MINUS_4,
		.maximum = EV_MAX - 1,
		.step = 1,
		.value = EV_DEFAULT,
		.default_value = EV_DEFAULT,
	}, {
		.id = V4L2_CID_CAMERA_CONTRAST,
		.minimum = CONTRAST_MINUS_2,
		.maximum = CONTRAST_MAX - 1,
		.step = 1,
		.value = CONTRAST_DEFAULT,		
		.default_value = CONTRAST_DEFAULT,
	}, {		
		.id = V4L2_CID_CAMERA_SATURATION,
		.minimum = SATURATION_MINUS_2,
		.maximum = SATURATION_MAX - 1,
		.step = 1,
		.value = SATURATION_DEFAULT,
		.default_value = SATURATION_DEFAULT,
	}, {		
		.id = V4L2_CID_CAMERA_SHARPNESS,
		.minimum = SHARPNESS_MINUS_2,
		.maximum = SHARPNESS_MAX - 1,
		.step = 1,
		.value = SHARPNESS_DEFAULT,
		.default_value = SHARPNESS_DEFAULT,
	}, {		
		.id = V4L2_CID_CAMERA_METERING,
		.minimum = METERING_BASE + 1,
		.maximum = METERING_MAX - 1,
		.step = 1,
		.value = METERING_CENTER,
		.default_value = METERING_CENTER,
	}, {		
		.id = V4L2_CID_CAMERA_WHITE_BALANCE,
		.minimum = WHITE_BALANCE_BASE + 1,
		.maximum = WHITE_BALANCE_MAX - 1,
		.step = 1,
		.value = WHITE_BALANCE_AUTO,
		.default_value = WHITE_BALANCE_AUTO,
	}, {
		.id = V4L2_CID_CAMERA_EFFECT,
		.minimum = IMAGE_EFFECT_BASE + 1,
		.maximum = IMAGE_EFFECT_MAX - 1,
		.step = 1,
		.value = IMAGE_EFFECT_NONE,
		.default_value = IMAGE_EFFECT_NONE,
	}, {
		.id = V4L2_CID_CAMERA_ISO,
		.minimum = ISO_AUTO,
		#if 1  //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101214 : : temp code for 10776			
		.maximum = ISO_SPORTS - 1 , 
		#else
		.maximum = ISO_MAX - 1,
		#endif
		.step = 1,
		.value = ISO_AUTO,
		.default_value = ISO_AUTO,
	}, {
 #if 0 //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110131	
		.id = V4L2_CID_CAMERA_FOCUS_MODE,
		.minimum = FOCUS_MODE_AUTO,
		.maximum = FOCUS_MODE_MAX - 1,
		.step = 1,
		.value = FOCUS_MODE_AUTO,
		.default_value = FOCUS_MODE_AUTO,
	}, {
#endif	
		.id = V4L2_CID_CAMERA_ZOOM,
		.minimum = ZOOM_LEVEL_0,
		.maximum = ZOOM_LEVEL_MAX - 1,
		.step = 1,
		.value = ZOOM_LEVEL_0,
		.default_value = ZOOM_LEVEL_0,
	}, {
 #if 0 //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110131	
		.id = V4L2_CID_CAM_JPEG_QUALITY,
		.minimum = 1,
		.maximum = 100,
		.step = 1,
		.value = 100,
		.default_value = 100,
	},{
#endif	
		.id = V4L2_CID_CAMERA_ANTI_BANDING,
		.minimum = ANTI_BANDING_AUTO,
		.maximum = ANTI_BANDING_OFF,
		.step = 1,
		.value = ANTI_BANDING_60HZ,
		.default_value = ANTI_BANDING_50HZ,
		}, { 
		.id = V4L2_CID_CAMERA_CHECK_DATALINE,
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.value = 0,
		.default_value = 0,
	},
};

static inline struct m5mo_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct m5mo_state, sd);
}

static int m5mo_read(struct i2c_client *c, 
	u8 len, u8 category, u8 byte, int *val)
{
	struct i2c_msg msg;
	unsigned char data[5];
	unsigned char recv_data[len + 1];
	int i, err = 0;

	if (!c->adapter)
		return -ENODEV;
	
	if (len != 0x01 && len != 0x02 && len != 0x04)		
		return -EINVAL;

	msg.addr = c->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	/* high byte goes out first */
	data[0] = msg.len;
	data[1] = 0x01;			/* Read category parameters */
	data[2] = category;
	data[3] = byte;
	data[4] = len;
	
	for (i = M5MO_I2C_RETRY; i; i--) {
		err = i2c_transfer(c->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	if (err != 1) {
		cam_err("category %#x, byte %#x\n", category, byte);
		return err;
	}

	msg.flags = I2C_M_RD;
	msg.len = sizeof(recv_data);
	msg.buf = recv_data;
	for(i = M5MO_I2C_RETRY; i; i--) {			
		err = i2c_transfer(c->adapter, &msg, 1);
		if (err == 1)
			break;		
		msleep(20);
	}

	if (err != 1) {
		cam_err("category %#x, byte %#x\n", category, byte);
		return err;
	}

	if (recv_data[0] != sizeof(recv_data))
		cam_warn("expected length %d, but return length %d\n", 
				 sizeof(recv_data), recv_data[0]);	
	
	if (len == 0x01)
		*val = recv_data[1];
	else if (len == 0x02)
		*val = recv_data[1] << 8 | recv_data[2];
	else
		*val = recv_data[1] << 24 | recv_data[2] << 16 | 
				recv_data[3] << 8 | recv_data[4];
	
	cam_dbg("category %#02x, byte %#x, value %#x\n", category, byte, *val);
	return err;
}

static int m5mo_write(struct i2c_client *c, 
	u8 len, u8 category, u8 byte, int val)
{
	struct i2c_msg msg;
	unsigned char data[len + 4];
	int i, err;

	if (!c->adapter)
		return -ENODEV;

	if (len != 0x01 && len != 0x02 && len != 0x04)
		return -EINVAL;

	msg.addr = c->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	data[0] = msg.len;
	data[1] = 0x02;			/* Write category parameters */
	data[2] = category;
	data[3] = byte;
	if (len == 0x01) {
		data[4] = val & 0xFF;
	} else if (len == 0x02) {		
		data[4] = (val >> 8) & 0xFF;
		data[5] = val & 0xFF;
	} else {
		data[4] = (val >> 24) & 0xFF;
		data[5] = (val >> 16) & 0xFF;
		data[6] = (val >> 8) & 0xFF;
		data[7] = val & 0xFF;		
	}

	cam_dbg("category %#x, byte %#x, value %#x\n", category, byte, val);
	
	for (i = M5MO_I2C_RETRY; i; i--) {
		err = i2c_transfer(c->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}
	
	return err;	
}

static int m5mo_mem_read(struct i2c_client *c, u16 len, u32 addr, u8 *val)
{
	struct i2c_msg msg;
	unsigned char data[8];
	unsigned char recv_data[len + 3];
	int i, err = 0;
	u16 recv_len;

	if (!c->adapter)
		return -ENODEV;
	
	if (len <= 0)
		return -EINVAL;	
		
	msg.addr = c->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	/* high byte goes out first */
	data[0] = 0x00;
	data[1] = 0x03;
	data[2] = (addr >> 24) & 0xFF;
	data[3] = (addr >> 16) & 0xFF;
	data[4] = (addr >> 8) & 0xFF;
	data[5] = addr & 0xFF;
	data[6] = (len >> 8) & 0xFF;
	data[7] = len & 0xFF;

	for (i = M5MO_I2C_RETRY; i; i--) {
		err = i2c_transfer(c->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	if (err != 1)
		return err;

	msg.flags = I2C_M_RD;
	msg.len = sizeof(recv_data);
	msg.buf = recv_data;
	for(i = M5MO_I2C_RETRY; i; i--) {
		err = i2c_transfer(c->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	if (err != 1)
		return err;

	if (recv_len != (recv_data[0] << 8 | recv_data[1]))
		cam_warn("expected length %d, but return length %d\n", 
				 len, recv_len);

	memcpy(val, recv_data + 1 + sizeof(recv_len), len);	

	return err;
}

static int m5mo_mem_write(struct i2c_client *c, u16 len, u32 addr, u8 *val)
{
	struct i2c_msg msg;
	unsigned char data[len + 8];
	int i, err = 0;

	if (!c->adapter)
		return -ENODEV;

	msg.addr = c->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	/* high byte goes out first */
	data[0] = 0x00;
	data[1] = 0x04;
	data[2] = (addr >> 24) & 0xFF;
	data[3] = (addr >> 16) & 0xFF;
	data[4] = (addr >> 8) & 0xFF;
	data[5] = addr & 0xFF;
	data[6] = (len >> 8) & 0xFF;
	data[7] = len & 0xFF;
	memcpy(data + 2 + sizeof(addr) + sizeof(len), val, len);

	for(i = M5MO_I2C_RETRY; i; i--) {
		err = i2c_transfer(c->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	return err;
}

static inline void m5mo_clear_interrupt(struct v4l2_subdev *sd)
{	
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m5mo_state *state = to_state(sd);	
	int int_factor;

	state->isp.issued = 0;
	m5mo_readb(client, M5MO_CATEGORY_SYS, M5MO_SYS_INT_FACTOR, &int_factor);
}

static irqreturn_t m5mo_isp_isr(int irq, void *dev_id)
{
	struct v4l2_subdev *sd = (struct v4l2_subdev *)dev_id;
	struct m5mo_state *state = to_state(sd);

	cam_dbg("**************** interrupt ****************\n");
	if (!state->isp.issued) {
		wake_up_interruptible(&state->isp.wait);
		state->isp.issued = 1;
	}

	return IRQ_HANDLED;
}

static u32 m5mo_wait_interrupt(struct v4l2_subdev *sd, 
	unsigned int timeout)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m5mo_state *state = to_state(sd);	
	u32 int_factor = 0;	
	cam_dbg("E\n");
	
	if (wait_event_interruptible_timeout(state->isp.wait, 
	 									state->isp.issued == 1, 
	 									msecs_to_jiffies(timeout)) == 0) {
		cam_err("timeout\n");
		return 0;
	}
	
	m5mo_readb(client, M5MO_CATEGORY_SYS, M5MO_SYS_INT_FACTOR, &int_factor);	

	cam_dbg("X\n");
	return int_factor;
}

static int m5mo_set_mode(struct i2c_client *client, u32 mode, u32 *old_mode)
{
	int i, err;
	u32 val;	
	cam_dbg("E\n");

	err = m5mo_readb(client, M5MO_CATEGORY_SYS, M5MO_SYS_MODE, &val);
	if (err < 0)
		return err;

	if (old_mode)
		*old_mode = val;

	if (val == mode) {
		cam_warn("mode is same\n");
		return 0;
	}

	switch (val) {
	case M5MO_SYSINIT_MODE:
		cam_warn("sensor is initializing\n");
		err = -EBUSY;
		break;
		
	case M5MO_PARMSET_MODE:
		if (mode == M5MO_STILLCAP_MODE) {
			err = m5mo_writeb(client, M5MO_CATEGORY_SYS, M5MO_SYS_MODE, M5MO_MONITOR_MODE);
			if (err < 0)
				return err;
			for (i = M5MO_I2C_VERIFY; i; i--) {
				err = m5mo_readb(client, M5MO_CATEGORY_SYS, M5MO_SYS_MODE, &val);
				if (val == M5MO_MONITOR_MODE)
					break;
				msleep(10);
			}
		}
	case M5MO_MONITOR_MODE:		
	case M5MO_STILLCAP_MODE:
		err = m5mo_writeb(client, M5MO_CATEGORY_SYS, M5MO_SYS_MODE, mode);
		break;
		
	default:
		cam_warn("current mode is unknown\n");
		err = -EINVAL;
	}

	if (err < 0)
		return err;
	
	for (i = M5MO_I2C_VERIFY; i; i--) {
		err = m5mo_readb(client, M5MO_CATEGORY_SYS, M5MO_SYS_MODE, &val);
		if (val == mode)
			break;
		msleep(10);
	}

	cam_dbg("X\n");
	return err;
}

static int m5mo_flash_on(struct v4l2_subdev *sd, int enable)
{
	struct m5mo_state *state = to_state(sd);
	cam_dbg("E\n");
	
#if 0	
	if (HWREV < SEINE_HWREV_UNIV03) {
		cam_warn("flash isn't supported on this H/W Revision, %d\n", HWREV);
		return 0;
	}

	if (state->sensor_mode == SENSOR_CAMERA)
		MAX8966_flash_led_en(enable);
	else
		MAX8966_flash_movie_en(enable);

#endif

	cam_dbg("X\n");
	return 0;
}

/*
 * v4l2_subdev_video_ops
 */
static const struct m5mo_frmsizeenum *m5mo_get_frmsize(const struct m5mo_frmsizeenum *frmsizes,
	int num_entries, int index)
{
	int i;
	
	for (i = 0; i < num_entries; i++) {
		if (frmsizes[i].index == index)
			return &frmsizes[i];
	}

	return NULL;
}

static int m5mo_set_frmsize(struct v4l2_subdev *sd)
{	
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;
	cam_dbg("E\n");
	
	if (state->colorspace != V4L2_COLORSPACE_JPEG) {
		err = m5mo_writeb(client, M5MO_CATEGORY_PARM, M5MO_PARM_MON_SIZE, 
							state->frmsize->reg_val);
		CHECK_ERR(err);
	} else {
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_MAIN_IMG_SIZE, 
							state->frmsize->reg_val);
		CHECK_ERR(err);
	}
	cam_info("frame size %dx%d\n", state->frmsize->width, state->frmsize->height);
	return 0;
}

static int m5mo_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *f)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i, num_entries, err;
	u32 old_index;
	cam_dbg("E, %dx%d\n", f->fmt.pix.width, f->fmt.pix.height);

	state->colorspace = f->fmt.pix.colorspace;

	old_index = state->frmsize ? state->frmsize->index : -1;
	state->frmsize = NULL;

	if (state->colorspace != V4L2_COLORSPACE_JPEG) {
		num_entries = ARRAY_SIZE(m5mo_pre_frmsizes);
		if (f->fmt.pix.width == 800 && f->fmt.pix.height == 450) {
			state->frmsize = m5mo_get_frmsize(m5mo_pre_frmsizes, num_entries, 
												M5MO_PREVIEW_1080P);
		} else if (f->fmt.pix.width == 800 && f->fmt.pix.height == 448) {
			state->frmsize = m5mo_get_frmsize(m5mo_pre_frmsizes, num_entries, 
												M5MO_PREVIEW_720P);
		} else {
			for (i = 0; i < num_entries; i++) {
				if (f->fmt.pix.width == m5mo_pre_frmsizes[i].width &&
					f->fmt.pix.height == m5mo_pre_frmsizes[i].height) {
					state->frmsize = &m5mo_pre_frmsizes[i];
				}
			}
		}
	} else {
		num_entries = ARRAY_SIZE(m5mo_cap_frmsizes);
		for (i = 0; i < num_entries; i++) {
			if (f->fmt.pix.width == m5mo_cap_frmsizes[i].width &&
				f->fmt.pix.height == m5mo_cap_frmsizes[i].height) {
				state->frmsize = &m5mo_cap_frmsizes[i];
			}
		}
	}

	if (state->frmsize == NULL) {
		cam_warn("invalid frame size %dx%d\n", f->fmt.pix.width, f->fmt.pix.height);
		state->frmsize = state->colorspace != V4L2_COLORSPACE_JPEG ?
			m5mo_get_frmsize(m5mo_pre_frmsizes, num_entries, M5MO_PREVIEW_VGA) :
			m5mo_get_frmsize(m5mo_cap_frmsizes, num_entries, M5MO_CAPTURE_3MP);
	}

	if (state->initialized) {
		cam_dbg("old index %d, new index %d\n", old_index, state->frmsize->index);
		if (old_index != state->frmsize->index) {
			if (state->colorspace != V4L2_COLORSPACE_JPEG) {
				err = m5mo_set_mode(client, M5MO_PARMSET_MODE, NULL);
				CHECK_ERR(err);
			}
			m5mo_set_frmsize(sd);
		}
	}
	
	cam_dbg("X\n");
	return 0;
}

static int m5mo_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct m5mo_state *state = to_state(sd);

	a->parm.capture.timeperframe.numerator = 1;
	a->parm.capture.timeperframe.denominator = state->fps;

	return 0;
}

static int m5mo_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

	u32 new_fps = a->parm.capture.timeperframe.denominator / 
					a->parm.capture.timeperframe.numerator;

	cam_info("%d\n", new_fps);
	
	if (new_fps != state->fps) {
		if(new_fps <= 0 || new_fps > 30) {
			cam_err("invalid frame rate %d\n", new_fps);
			new_fps = 30;
		}
		state->fps = new_fps;		
	}
	
	 //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110216
	if (state->initialized) {
		err = m5mo_writeb(client, M5MO_CATEGORY_PARM,
			M5MO_PARM_FLEX_FPS, state->fps != 30 ? state->fps : 0);
		CHECK_ERR(err);
	}
	
	return 0;
}

static int m5mo_enum_framesizes(struct v4l2_subdev *sd, 
	struct v4l2_frmsizeenum *fsize)
{
	struct m5mo_state *state = to_state(sd);

	if (state->frmsize == NULL || state->frmsize->index < 0)
		return -EINVAL;

	/* The camera interface should read this value, this is the resolution
 	 * at which the sensor would provide framedata to the camera i/f
 	 *
 	 * In case of image capture, this returns the default camera resolution (VGA)
 	 */
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = state->frmsize->width;
	fsize->discrete.height = state->frmsize->height;

	return 0;
}

// NAGSM_ANDROID_HQ_CAMERA_SORACHOI_20110118 : lens move to zero step
static int m5mo_set_zero_step(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i, err;
	u32 status = 0;	
	cam_dbg("E\n");
	
	err = m5mo_set_mode(client, M5MO_MONITOR_MODE, NULL);
	CHECK_ERR(err);

	#if 0 
	err = m5mo_writeb(client, M5MO_CATEGORY_LENS, M5MO_LENS_AF_STEP_UPPER, 0x80);
	CHECK_ERR(err);
	err = m5mo_writeb(client, M5MO_CATEGORY_LENS, M5MO_LENS_AF_STEP_LOWER, 0x00);
	CHECK_ERR(err);
	err = m5mo_writeb(client, M5MO_CATEGORY_LENS, M5MO_LENS_AF_START, 0x12);
	CHECK_ERR(err);		
	#else
	err = m5mo_writeb(client, M5MO_CATEGORY_LENS, 0x01, 0x07);
	CHECK_ERR(err);	
	#endif

	for (i = M5MO_I2C_VERIFY; i; i--) {
		msleep(10);
		err = m5mo_readb(client, M5MO_CATEGORY_LENS, M5MO_LENS_AF_STATUS, &status);
		CHECK_ERR(err);
		
		if ((status & 0x01) == 0x00)
			break;
	}

	if ((status & 0x01) != 0x00) {
		cam_err("failed - status: %d\n",status);
		return 0;
	}	
	
	cam_dbg("X\n");
	return err;
}

static int m5mo_check_dataline(struct v4l2_subdev *sd, int val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;

	cam_dbg("E, value %d\n", val);

	err = m5mo_writeb(client, M5MO_CATEGORY_TEST,
		M5MO_TEST_OUTPUT_YCO_TEST_DATA, val ? 0x01: 0x00);
	CHECK_ERR(err);

	cam_dbg("X\n");
	return 0;
}

static int m5mo_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;
	u32 int_factor;
	
	if (enable == 3 || enable == 4) {	/* FIXME */
		if (state->sensor_mode == SENSOR_CAMCORDER && 
				state->flash_mode != FLASH_MODE_OFF)
			m5mo_flash_on(sd, enable == 3);
		return 0;
	}

	cam_info("%s\n", enable ? "on" : "off");

	if (enable) {
		m5mo_clear_interrupt(sd);

		if (state->sensor_mode == SENSOR_CAMERA && 
				state->flash_mode != FLASH_MODE_OFF)
			m5mo_flash_on(sd, 1);
		
		if (state->colorspace != V4L2_COLORSPACE_JPEG) {
			err = m5mo_writeb(client, M5MO_CATEGORY_SYS, M5MO_SYS_INT_EN, M5MO_INT_MODE);
			CHECK_ERR(err);
			
			err = m5mo_set_mode(client, M5MO_MONITOR_MODE, NULL);
			if (err <= 0) {
				cam_err("m5mo_set_mode failed\n");
				return err;
			}
			
			int_factor = m5mo_wait_interrupt(sd, M5MO_ISP_INT_TIMEOUT);
			if (!(int_factor & M5MO_INT_MODE)) {
				cam_err("M5MO_INT_MODE isn't issued, %#x\n", int_factor);
				return -ETIMEDOUT;
			}

			err = m5mo_check_dataline(sd, state->check_dataline);
			CHECK_ERR(err);
			
		} else {		
			err = m5mo_writeb(client, M5MO_CATEGORY_SYS, M5MO_SYS_INT_EN, M5MO_INT_CAPTURE);
			CHECK_ERR(err);
			
			err = m5mo_set_mode(client, M5MO_STILLCAP_MODE, NULL);
			if (err <= 0) {
				cam_err("m5mo_set_mode failed\n");
				return err;
			}
			
			int_factor = m5mo_wait_interrupt(sd, 5000/*M5MO_ISP_INT_TIMEOUT*/); //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110112 : for vintage and veauty shot
			if (!(int_factor & M5MO_INT_CAPTURE)) {
				cam_err("M5MO_INT_CAPTURE isn't issued, %#x\n", int_factor);
				return -ETIMEDOUT;
			}
			
			err = m5mo_writeb(client, M5MO_CATEGORY_CAPCTRL, 
								M5MO_CAPCTRL_FRM_SEL, 0x01);
			CHECK_ERR(err);
		}
	} else {
		if (state->sensor_mode == SENSOR_CAMERA && 
				state->flash_mode != FLASH_MODE_OFF)
			m5mo_flash_on(sd, 0);
	}
	
	cam_dbg("X\n");
	return 0;
}

/*
 * v4l2_subdev_core_ops
 */
static int m5mo_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i;
	
	for (i = 0; i < ARRAY_SIZE(m5mo_ctrls); i++) {
		if (qc->id == m5mo_ctrls[i].id) {
			qc->maximum = m5mo_ctrls[i].maximum;
			qc->minimum = m5mo_ctrls[i].minimum;
			qc->step = m5mo_ctrls[i].step;
			qc->default_value = m5mo_ctrls[i].default_value;
			return 0;
		}
	}
	
	return -EINVAL;
}

static int m5mo_get_af_status(struct v4l2_subdev *sd, 
	struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u32 val = 0;
	int err;

	err = m5mo_readb(client, M5MO_CATEGORY_LENS, M5MO_LENS_AF_STATUS, &val);
	CHECK_ERR(err);
	
	ctrl->value = val;

	return 0;
}

//NAGSM_ANDROID_HQ_CAMERA_SORACHOI_20101231 : EV-P for Scene Mode
static int m5mo_set_ev_program_mode(struct v4l2_subdev *sd, int val)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

	if (state->sensor_mode != SENSOR_CAMERA)
		return 0;

	if(state->m_ev_program_mode == val)
	    return 0;

	cam_dbg("E, value %d\n", val);
	
retry:
	switch (val) {
	case SCENE_MODE_NONE:	
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x00);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x00);
		CHECK_ERR(err);
		break;

	case SCENE_MODE_PORTRAIT: 
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x01);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x01);
		CHECK_ERR(err);           
		break;

	case SCENE_MODE_LANDSCAPE:            
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x02);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x02);
		CHECK_ERR(err);           
		break;

	case SCENE_MODE_SPORTS:           
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x03);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x03);
		CHECK_ERR(err);             
		break;

	case SCENE_MODE_PARTY_INDOOR:            
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x04);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x04);
		CHECK_ERR(err);                    
		break;

	case SCENE_MODE_BEACH_SNOW:                
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x05);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x05);
		CHECK_ERR(err);                
		break;

	case SCENE_MODE_SUNSET:               
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x06);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x06);
		CHECK_ERR(err);               
		break;

	case SCENE_MODE_DUST_DAWN:           
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x07);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x07);
		CHECK_ERR(err);                     
		break;
		
	case SCENE_MODE_FALL_COLOR:    
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x08);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x08);
		CHECK_ERR(err);                              
		break;

	case SCENE_MODE_NIGHTSHOT:   
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x09);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x09);
		CHECK_ERR(err);     
		break;

	case SCENE_MODE_BACK_LIGHT:   
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x0A);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x0A);
		CHECK_ERR(err);      
		break;

	case SCENE_MODE_FIREWORKS:      
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x0B);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x0B);
		CHECK_ERR(err);      
		break;

	case SCENE_MODE_TEXT:            
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x0C);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x0C);
		CHECK_ERR(err);    
		break;

	case SCENE_MODE_CANDLE_LIGHT:   
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x0D);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x0D);
		CHECK_ERR(err);                
		break;
			
	default:
		cam_warn("invalid value, %d\n", val);
		val = SCENE_MODE_NONE;
		goto retry;
	}
	
	state->m_ev_program_mode = val;
	
	cam_dbg("X\n");
	return 0;
}

//NAGSM_ANDROID_HQ_CAMERA_SORACHOI_20101231 : MCC for Scene Mode
static int m5mo_set_mcc_mode(struct v4l2_subdev *sd, int val) 
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;
	
	if (state->sensor_mode != SENSOR_CAMERA)
		return 0;

	if(state->m_mcc_mode == val)
		return 0;

	cam_dbg("E, value %d\n", val);

       if(val == SCENE_MODE_NONE) {
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_MCC_MODE, 0x01);
	} else {
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_MCC_MODE, 0x00);
	}
	
	CHECK_ERR(err);

	state->m_mcc_mode = val;
	
	cam_dbg("X\n");
	return err;
}

static int m5mo_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m5mo_state *state = to_state(sd);
	int err = 0;
	
	switch (ctrl->id) {	
	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
		err = m5mo_get_af_status(sd, ctrl);
		break;
		
	case V4L2_CID_CAM_JPEG_MEMSIZE:
		ctrl->value = M5MO_JPEG_MAXSIZE + M5MO_THUMB_MAXSIZE + M5MO_POST_MAXSIZE;
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

	case V4L2_CID_CAMERA_EXIF_EXPTIME:
		ctrl->value = state->exif.exptime;
		break;
		
	case V4L2_CID_CAMERA_EXIF_FLASH:
		ctrl->value = state->exif.flash;
		break;

	case V4L2_CID_CAMERA_EXIF_ISO:
		ctrl->value = state->exif.iso;
		break;

	case V4L2_CID_CAMERA_EXIF_TV:
		ctrl->value = state->exif.tv;
		break;

	case V4L2_CID_CAMERA_EXIF_BV:
		ctrl->value = state->exif.bv;
		break;

	case V4L2_CID_CAMERA_EXIF_EBV:
		ctrl->value = state->exif.ebv;
		break;

//NAGSM_ANDROID_HQ_CAMERA_SUNGKOOLEE_20110118		
	case V4L2_CID_CAM_AF_VER_LOW:
		ctrl->value = state->af_info.low;
		break; 

	case V4L2_CID_CAM_AF_VER_HIGH:
		ctrl->value = state->af_info.high;
		break; 	

	case V4L2_CID_CAM_GAMMA_RG_LOW:
		ctrl->value = state->gamma.rg_low;
		break; 

	case V4L2_CID_CAM_GAMMA_RG_HIGH:
		ctrl->value = state->gamma.rg_high;
		break; 		

	case V4L2_CID_CAM_GAMMA_BG_LOW:
		ctrl->value = state->gamma.bg_low;
		break; 

	case V4L2_CID_CAM_GAMMA_BG_HIGH:
		ctrl->value = state->gamma.bg_high;
		break; 			

	default:
		cam_err("no such control id %d\n", 
				ctrl->id - V4L2_CID_PRIVATE_BASE);
		//err = -ENOIOCTLCMD
		err = 0;
		break;
	}
	
	if (err < 0 && err != -ENOIOCTLCMD)
		cam_err("failed, id %d\n", ctrl->id - V4L2_CID_PRIVATE_BASE);
	
	return err;
}

static int m5mo_get_standard_fw_version(char *str,
	struct m5mo_fw_version *ver)
{	
	switch (ver->company) {
	case 'O':
		str[0] = '0';
		str[1] = 'P';
		break;

	case 'S':
		str[0] = 'S';
		str[1] = 'E';
		break;

	case 'T':
		str[0] = 'T';
		str[1] = 'E';
		break;
	}

	//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101215
	switch (ver->module_vesion) {
	case 'A': //FSI
		str[2] = 'S';
		str[3] = 'F';
		break;

	case 'B': //BSI
		str[2] = 'S';
		str[3] = 'B';
		break;
	}

	str[4] = ver->year;
	str[5] = ver->month;
	str[6] = ver->day[0];
	str[7] = ver->day[1];
	str[8] = '\0';

	return 0;	
}

static int m5mo_get_sensor_fw_version(struct v4l2_subdev *sd, 
	char *str)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct m5mo_fw_version ver;
	u8 buf[M5MO_FW_VER_LEN] = {0, };
	int i = 0, val, err;

	#if 0 //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110216
	do {
		err = m5mo_readb(client, M5MO_CATEGORY_SYS, M5MO_SYS_USER_VER, &val);
		CHECK_ERR(err);
		buf[i++] = (char)val;
	} while ((char)val != '\0' && i < sizeof(buf));
	
	memcpy(&ver, buf, sizeof(ver));
	#else
	/* set pin */
	val = 0x7E;
	err = m5mo_mem_write(client, sizeof(val), 0x50000308, &val);
	CHECK_ERR(err);

	err = m5mo_mem_read(client, M5MO_FW_VER_LEN,
		M5MO_FLASH_BASE_ADDR + M5MO_FW_VER_FILE_CUR, buf);

	strncpy(str, buf, sizeof(buf));
	#endif
	
	//m5mo_get_standard_fw_version(str, &ver);
	
	//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101215
	if (str[1] == 'A')
		IS_M5MO_FSI = 1;

	cam_info("%s\n", str);

	//NAGSM_ANDROID_HQ_CAMERA_SUNGKOOLEE_20110118
	//start to get cal valuse.
	struct m5mo_state *state = to_state(sd);
	
	state->af_info.high = 0; //AF high is always 0.
	
	err = m5mo_readb(client, M5MO_CATEGORY_LENS, M5MO_ADJST_AF_CAL, &val);
	CHECK_ERR(err);
	state->af_info.low = (unsigned int)val;

	err = m5mo_readb(client, M5MO_CATEGORY_ADJST, M5MO_ADJST_RG_HIGH, &val);
	CHECK_ERR(err);
	state->gamma.rg_low = (unsigned int)val;

	err = m5mo_readb(client, M5MO_CATEGORY_ADJST, M5MO_ADJST_RG_LOW, &val);
	CHECK_ERR(err);

	state->gamma.rg_high = (unsigned int)val;

	err = m5mo_readb(client, M5MO_CATEGORY_ADJST, M5MO_ADJST_BG_HIGH, &val);
	CHECK_ERR(err);	
	state->gamma.bg_low = (unsigned int)val;

	err = m5mo_readb(client, M5MO_CATEGORY_ADJST, M5MO_ADJST_BG_LOW, &val);
	CHECK_ERR(err);
	state->gamma.bg_high= (unsigned int)val;

	return 0;
}

static int m5mo_get_phone_fw_version(struct v4l2_subdev *sd, char *str)
{
	//struct m5mo_fw_version ver;
	u8 buf[M5MO_FW_VER_LEN] = {0, };
	int err;

	struct file *fp;
	mm_segment_t old_fs;
	long nread;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	#ifdef SDCARD_FW
	fp = filp_open(M5MO_SD_FW_PATH, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) 
	#endif	
	{
		if(IS_M5MO_FSI)
			fp = filp_open(M5MO_FSI_FW_PATH, O_RDONLY, S_IRUSR);
		else
			fp = filp_open(M5MO_BSI_FW_PATH, O_RDONLY, S_IRUSR);

		if (IS_ERR(fp)) {
			if(IS_M5MO_FSI)
				cam_err("failed to open %s\n", M5MO_FSI_FW_PATH);
			else
				cam_err("failed to open %s\n", M5MO_BSI_FW_PATH);
					
			return -ENOENT;
		}
	}

	err = vfs_llseek(fp, M5MO_FW_VER_FILE_CUR, SEEK_SET);
	if (err < 0) {
		cam_warn("failed to fseek, %d\n", err);
		return err;
	}

	nread = vfs_read(fp, (char __user *)buf, sizeof(buf), &fp->f_pos);
	if (nread != sizeof(buf)) {
		cam_err("failed to read firmware file, nread %ld Bytes\n", nread);
		return -EIO;
	}
	
	strncpy(str, buf, sizeof(buf));
	
	//m5mo_get_standard_fw_version(str, &ver);
	
	cam_info("%s\n", str);
	return 0;
}


static int m5mo_g_ext_ctrl(struct v4l2_subdev *sd, struct v4l2_ext_control *ctrl)
{
	int err = 0;

	switch (ctrl->id) {
	case V4L2_CID_CAM_SENSOR_FW_VER:
		err = m5mo_get_sensor_fw_version(sd, ctrl->string);
		break;

	case V4L2_CID_CAM_PHONE_FW_VER:
		err = m5mo_get_phone_fw_version(sd, ctrl->string);
		break;
		
	default:
		cam_err("no such control id %d, value %d\n", 
				ctrl->id - V4L2_CID_CAMERA_CLASS_BASE, ctrl->value);
		// err = -ENOIOCTLCMD;
		break;
	}

	if (err < 0 && err != -ENOIOCTLCMD)
		cam_err("failed, id %d, value %d\n", 
				ctrl->id - V4L2_CID_CAMERA_CLASS_BASE, ctrl->value);

	return err;
}

static int m5mo_g_ext_ctrls(struct v4l2_subdev *sd, struct v4l2_ext_controls *ctrls)
{
	struct v4l2_ext_control *ctrl = ctrls->controls;
	int i, err = 0;

	for (i = 0; i < ctrls->count; i++, ctrl++) {
		err = m5mo_g_ext_ctrl(sd, ctrl);
		if (err) {
			ctrls->error_idx = i;
			break;
		}
	}
	return err;
}

static int m5mo_set_sensor_mode(struct v4l2_subdev *sd, int val)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;
	u32 hdmovie, fps;

	if(state->sensor_mode == val)
		return 0;

	cam_dbg("E, value %d\n", val);

retry:
	switch (val) {
	case SENSOR_CAMERA:
		hdmovie = 0x00;
		fps = 0x01;
		break;
		
	case SENSOR_CAMCORDER:
		hdmovie = 0x01;
		if(state->frmsize->index == M5MO_PREVIEW_1080P)
			fps = 0x01;
		else
			fps = 0x02;
		break;
		
	default:
		cam_warn("invalid value, %d\n", val);
		val = SENSOR_CAMERA;
		goto retry;
	}
	
	err = m5mo_set_mode(client, M5MO_PARMSET_MODE, NULL);
	CHECK_ERR(err);
	
	err = m5mo_writeb(client, M5MO_CATEGORY_PARM, M5MO_PARM_HDMOVIE, hdmovie);
	CHECK_ERR(err); 
	
	#if 0 //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110118
	err = m5mo_writeb(client, M5MO_CATEGORY_PARM, M5MO_PARM_MON_FPS, fps); 
	CHECK_ERR(err);
	#endif

	state->sensor_mode = val;

	cam_dbg("X\n");
	return 0;
}

static int m5mo_set_flash(struct v4l2_subdev *sd, int val)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

	#if 0
	if(state->flash_mode == val)
		return 0;
	#endif
	
	cam_dbg("E, value %d\n", val);

retry:
	switch (val) {
	case FLASH_MODE_OFF:
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_LIGHT_CTRL, 0x00);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_FLASH_CTRL, 0x00);
		CHECK_ERR(err);
		m5mo_flash_on(sd, 0);
		break;
		
	case FLASH_MODE_AUTO:
		if (state->sensor_mode == SENSOR_CAMERA) {
			m5mo_flash_on(sd, 1);
			err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_LIGHT_CTRL, 0x02);
			CHECK_ERR(err);
			err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_FLASH_CTRL, 0x02);
			CHECK_ERR(err);
		} else {
			cam_warn("Auto camcorder flash mode isn't supported yet\n");
			//err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_LIGHT_CTRL, 0x04);
			//CHECK_ERR(err);
		}
		break;
		
	case FLASH_MODE_ON:
		if (state->sensor_mode == SENSOR_CAMERA) {
			m5mo_flash_on(sd, 1);
			err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_LIGHT_CTRL, 0x01);
			CHECK_ERR(err);
			err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_FLASH_CTRL, 0x01);
			CHECK_ERR(err);
		} else {
			err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_LIGHT_CTRL, 0x03);
			CHECK_ERR(err);
		}
		break;
		
	case FLASH_MODE_TORCH:
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_LIGHT_CTRL, 0x03);
		CHECK_ERR(err);
		break;

	default:
		cam_warn("invalid value, %d\n", val);
		val = FLASH_MODE_OFF;
		goto retry;
	}

	state->flash_mode = val;

	cam_dbg("X\n");
	return 0;
}

static int m5mo_set_exposure(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_queryctrl qc = {0,};
	int val = ctrl->value, err;
	u32 exposure[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

	if((state->m_ev_mode == val) && (state->m_ev_mode != 0))
	    return 0;

	cam_dbg("E, value %d\n", val);

	qc.id = ctrl->id;
	m5mo_queryctrl(sd, &qc);

	if (val < qc.minimum || val > qc.maximum) {
		cam_warn("invalied value, %d\n", val);
		val = qc.default_value;
	}

	val -= qc.minimum;

	err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_EV_CONTROL, exposure[val]);
	CHECK_ERR(err);

	state->m_ev_mode = val;
	
	cam_dbg("X\n");
	return 0;
}

static int m5mo_set_contrast(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_queryctrl qc = {0,};
	int val = ctrl->value, err;
	u32 contrast[] = {0x03, 0x04, 0x05, 0x06, 0x07};
	u32 contrast_camcorder[] = {0x03, 0x04, 0x0C, 0x06, 0x07};

	if(state->sensor_mode == SENSOR_CAMERA && state->m_contrast_value == contrast[val])
	    return 0;

	if(state->sensor_mode == SENSOR_CAMCORDER && state->m_contrast_value == contrast_camcorder[val])
	    return 0;
		
	cam_dbg("E, value %d\n", val);

	qc.id = ctrl->id;
	m5mo_queryctrl(sd, &qc);

	if (val < qc.minimum || val > qc.maximum) {
		cam_warn("invalied value, %d\n", val);
		val = qc.default_value;
	}

	val -= qc.minimum;

	if (state->sensor_mode == SENSOR_CAMERA)
	{
		err = m5mo_writeb(client, M5MO_CATEGORY_MON, M5MO_MON_TONE_CTRL, contrast[val]);
		CHECK_ERR(err);

		state->m_contrast_value = contrast[val];
	}
	else
	{
		err = m5mo_writeb(client, M5MO_CATEGORY_MON, M5MO_MON_TONE_CTRL, contrast_camcorder[val]);
		CHECK_ERR(err);

		state->m_contrast_value = contrast_camcorder[val];
	}		

	cam_dbg("X\n");
	return 0;
}

static int m5mo_set_saturation(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_queryctrl qc = {0,};
	int val = ctrl->value, err;
	u32 saturation[] = {0x01, 0x02, 0x03, 0x04, 0x05};

	if(state->m_saturation_mode == val)
	    return 0;

	cam_dbg("E, value %d\n", val);

	qc.id = ctrl->id;
	m5mo_queryctrl(sd, &qc);

	if (val < qc.minimum || val > qc.maximum) {
		cam_warn("invalied value, %d\n", val);
		val = qc.default_value;
	}

	val -= qc.minimum;

	//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110127
	err = m5mo_writeb(client, M5MO_CATEGORY_MON, M5MO_MON_CHROMA_EN, 0x01);
	CHECK_ERR(err);

	err = m5mo_writeb(client, M5MO_CATEGORY_MON, M5MO_MON_CHROMA_LVL, saturation[val]);
	CHECK_ERR(err);

	state->m_saturation_mode = val;

	cam_dbg("X\n");
	return 0;
}

static int m5mo_set_sharpness(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_queryctrl qc = {0,};
	int val = ctrl->value, err;
	u32 sharpness[] = {0x03, 0x04, 0x05, 0x06, 0x07};
	u32 sharpness_camcorder[] = {0x03, 0x04, 0x0C, 0x06, 0x07};

	if(state->sensor_mode == SENSOR_CAMERA && state->m_sharpness_value == sharpness[val])
	    return 0;

	if(state->sensor_mode == SENSOR_CAMCORDER && state->m_sharpness_value == sharpness_camcorder[val])
	    return 0;
	
	cam_dbg("E, value %d\n", val);

	qc.id = ctrl->id;
	m5mo_queryctrl(sd, &qc);

	if (val < qc.minimum || val > qc.maximum) {
		cam_warn("invalied value, %d\n", val);
		val = qc.default_value;
	}

	val -= qc.minimum;

	//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110127
	err = m5mo_writeb(client, M5MO_CATEGORY_MON, M5MO_MON_EDGE_EN, 0x01);
	CHECK_ERR(err);
		
	if (state->sensor_mode == SENSOR_CAMERA)
	{
		err = m5mo_writeb(client, M5MO_CATEGORY_MON, M5MO_MON_EDGE_LVL, sharpness[val]);
		CHECK_ERR(err);

		state->m_sharpness_value = sharpness[val];
	}
	else
	{	
		err = m5mo_writeb(client, M5MO_CATEGORY_MON, M5MO_MON_EDGE_LVL, sharpness_camcorder[val]);
		CHECK_ERR(err);

		state->m_sharpness_value = sharpness_camcorder[val];
	}
	
	cam_dbg("X\n");
	return 0;
}
	
static int m5mo_set_metering(struct v4l2_subdev *sd, int val)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

	if (state->sensor_mode != SENSOR_CAMERA)
		return 0;
	
	if(state->m_metering_mode == val)
		return 0;

	cam_dbg("E, value %d\n", val);

retry:
	switch (val) {
	case METERING_CENTER:
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_MODE, 0x03);
		CHECK_ERR(err);
		break;
	case METERING_SPOT:
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_MODE, 0x06);
		CHECK_ERR(err);
		break;
	case METERING_MATRIX:
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_MODE, 0x01);
		CHECK_ERR(err);
		break;
	default:
		cam_warn("invalid value, %d\n", val);
		val = METERING_CENTER;
		goto retry;
	}

	state->m_metering_mode = val;
		
	cam_dbg("X\n");
	return 0;
}

static int m5mo_set_whitebalance(struct v4l2_subdev *sd, int val)
{	
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

	if(state->m_white_balance_mode == val)
		return 0;

	cam_dbg("E, value %d\n", val);

retry:
	switch (val) {
	case WHITE_BALANCE_AUTO:
		err = m5mo_writeb(client, M5MO_CATEGORY_WB, M5MO_WB_AWB_MODE, 0x01);
		CHECK_ERR(err);
		break;

	case WHITE_BALANCE_SUNNY:
		err = m5mo_writeb(client, M5MO_CATEGORY_WB, M5MO_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_WB, M5MO_WB_AWB_MANUAL, 0x04);
		CHECK_ERR(err);
		break;

	case WHITE_BALANCE_CLOUDY:
		err = m5mo_writeb(client, M5MO_CATEGORY_WB, M5MO_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);		
		err = m5mo_writeb(client, M5MO_CATEGORY_WB, M5MO_WB_AWB_MANUAL, 0x05);
		CHECK_ERR(err);
		break;

	case WHITE_BALANCE_TUNGSTEN:
		err = m5mo_writeb(client, M5MO_CATEGORY_WB, M5MO_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_WB, M5MO_WB_AWB_MANUAL, 0x01);
		CHECK_ERR(err);
		break;

	case WHITE_BALANCE_FLUORESCENT:
		err = m5mo_writeb(client, M5MO_CATEGORY_WB, M5MO_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_WB, M5MO_WB_AWB_MANUAL, 0x02);
		CHECK_ERR(err);
		break;

	default:
		cam_warn("invalid value, %d\n", val);
		val = WHITE_BALANCE_AUTO;
		goto retry;
	}
	
	state->m_white_balance_mode = val;
		
	cam_dbg("X\n");
	return 0;
}

static int m5mo_set_effect_color(struct i2c_client *c, int val)
{
	int on, err;	

	err = m5mo_readb(c, M5MO_CATEGORY_PARM, M5MO_PARM_EFFECT, &on);
	CHECK_ERR(err);
	if (on)	{
		err = m5mo_writeb(c, M5MO_CATEGORY_PARM, M5MO_PARM_EFFECT, 0);		
		CHECK_ERR(err);	
	}

	switch (val) {
	case IMAGE_EFFECT_NONE:
		err = m5mo_writeb(c, M5MO_CATEGORY_MON, M5MO_MON_COLOR_EFFECT, 0x00);
		CHECK_ERR(err);
		break;
			
	case IMAGE_EFFECT_SEPIA:
		err = m5mo_writeb(c, M5MO_CATEGORY_MON, M5MO_MON_COLOR_EFFECT, 0x01);
		CHECK_ERR(err);
		err = m5mo_writeb(c, M5MO_CATEGORY_MON, M5MO_MON_CFIXB, 0xD8);
		CHECK_ERR(err);
		err = m5mo_writeb(c, M5MO_CATEGORY_MON, M5MO_MON_CFIXR, 0x18);
		CHECK_ERR(err);
		break;
		
	case IMAGE_EFFECT_BNW:
		err = m5mo_writeb(c, M5MO_CATEGORY_MON, M5MO_MON_COLOR_EFFECT, 0x01);
		CHECK_ERR(err);
		err = m5mo_writeb(c, M5MO_CATEGORY_MON, M5MO_MON_CFIXB, 0x00);
		CHECK_ERR(err);
		err = m5mo_writeb(c, M5MO_CATEGORY_MON, M5MO_MON_CFIXR, 0x00);
		CHECK_ERR(err);
		break;
		
	case IMAGE_EFFECT_ANTIQUE:
		err = m5mo_writeb(c, M5MO_CATEGORY_MON, M5MO_MON_COLOR_EFFECT, 0x01);
		CHECK_ERR(err);
		err = m5mo_writeb(c, M5MO_CATEGORY_MON, M5MO_MON_CFIXB, 0xD0);
		CHECK_ERR(err);
		err = m5mo_writeb(c, M5MO_CATEGORY_MON, M5MO_MON_CFIXR, 0x30);
		CHECK_ERR(err);
		break;		
	}

	return 0;
}

static int m5mo_set_effect_gamma(struct i2c_client *c, s32 val)
{
	int on, err;	

	err = m5mo_readb(c, M5MO_CATEGORY_MON, M5MO_MON_COLOR_EFFECT, &on);
	CHECK_ERR(err);	
	if (on) {
		err = m5mo_writeb(c, M5MO_CATEGORY_MON, M5MO_MON_COLOR_EFFECT, 0);		
		CHECK_ERR(err);	
	}
	
	switch (val) {
	case IMAGE_EFFECT_NEGATIVE:
		err = m5mo_writeb(c, M5MO_CATEGORY_PARM, M5MO_PARM_EFFECT, 0x01);
		CHECK_ERR(err);
		break;
		
	case IMAGE_EFFECT_AQUA:
		err = m5mo_writeb(c, M5MO_CATEGORY_PARM, M5MO_PARM_EFFECT, 0x08);
		CHECK_ERR(err);
		break;
	}

	return err;
}

static int m5mo_set_effect(struct v4l2_subdev *sd, int val)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int old_mode, err;

       if(state->m_effect_mode == val)
            return 0;

	cam_dbg("E, value %d\n", val);
	
	err = m5mo_set_mode(client, M5MO_PARMSET_MODE, &old_mode);
	CHECK_ERR(err);

retry:
	switch (val) {
	case IMAGE_EFFECT_NONE:
	case IMAGE_EFFECT_BNW:
	case IMAGE_EFFECT_SEPIA:
	case IMAGE_EFFECT_ANTIQUE:
	case IMAGE_EFFECT_SHARPEN:
		err = m5mo_set_effect_color(client, val);
		CHECK_ERR(err);
		break;
		
	case IMAGE_EFFECT_AQUA:
	case IMAGE_EFFECT_NEGATIVE:
		err = m5mo_set_effect_gamma(client, val);
		CHECK_ERR(err);
		break;
		
	default:
		cam_warn("invalid value, %d\n", val);
		val = IMAGE_EFFECT_NONE;
		goto retry;
	}
	
	err = m5mo_set_mode(client, old_mode, NULL);
	CHECK_ERR(err);

	state->m_effect_mode = val;
		
	cam_dbg("X\n");
	return 0;
}

static int m5mo_set_iso(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_queryctrl qc = {0,};
	int val = ctrl->value, err;
	u32 iso[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};

	if (state->sensor_mode != SENSOR_CAMERA)
		return 0;
	
       if(state->m_iso_mode == val)
            return 0;

	cam_dbg("E, value %d\n", val);

	qc.id = ctrl->id;
	m5mo_queryctrl(sd, &qc);

	if (val < qc.minimum || val > qc.maximum) {
		cam_warn("invalied value, %d\n", val);
		val = qc.default_value;
	}

	val -= qc.minimum;

	err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_ISOSEL, iso[val]);
	CHECK_ERR(err);

	state->m_iso_mode = val;

	cam_dbg("X\n");
	return 0;
}

static int m5mo_set_af_mode(struct v4l2_subdev *sd, int val)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u32 mode, status = 0;
	int i, err;

	//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110215
       if(state->focus.mode == val && val < FOCUS_MODE_AUTO_DEFAULT )
            return 0;
		   
	cam_dbg("E, new value %d old value %d\n", val, state->focus.mode);

retry:
	switch (val) {
	case FOCUS_MODE_AUTO:
	case FOCUS_MODE_AUTO_DEFAULT:
		mode = 0x00;
		break;
		
	case FOCUS_MODE_MACRO:
	case FOCUS_MODE_MACRO_DEFAULT:
		mode = 0x01;
		break;
		
	case FOCUS_MODE_CONTINUOUS:
		mode = 0x02;
		break;
		
	case FOCUS_MODE_FD:
	case FOCUS_MODE_FD_DEFAULT:
		mode = 0x03; 
		break;

	case FOCUS_MODE_TOUCH:
		mode = 0x04;
		break;

	default:
		cam_warn("invalid value, %d", val);
		val = FOCUS_MODE_AUTO;
		goto retry;
	}

	#if 0 //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101228
	if (val == FOCUS_MODE_FD || val == FOCUS_MODE_FD_DEFAULT) {	/* enable face detection */
		err = m5mo_writeb(client, M5MO_CATEGORY_FD, M5MO_FD_SIZE, 0x01);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_FD, M5MO_FD_MAX, 0x0B);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_FD, M5MO_FD_CTL, 0x11);
		CHECK_ERR(err);
		msleep(10);
	} else if (state->focus.mode == FOCUS_MODE_FD || state->focus.mode == FOCUS_MODE_FD_DEFAULT) {	/* disable face detection */
		err = m5mo_writeb(client, M5MO_CATEGORY_FD, M5MO_FD_CTL, 0x00);
		CHECK_ERR(err);
	}
	#endif

	//state->focus.mode = val; //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110202
	
	err = m5mo_writeb(client, M5MO_CATEGORY_LENS, M5MO_LENS_AF_MODE, mode);
	CHECK_ERR(err);
	
	for (i = M5MO_I2C_VERIFY; i; i--) {
		msleep(10);
		err = m5mo_readb(client, M5MO_CATEGORY_LENS, M5MO_LENS_AF_STATUS, &status);
		CHECK_ERR(err);
		
		if ((status & 0x01) == 0x00) 
			break;
	}

	if ((status & 0x01) != 0x00) {
		cam_err("failed\n");

		//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110201
		err = m5mo_writeb(client, M5MO_CATEGORY_LENS, M5MO_LENS_AF_START, 0);
		CHECK_ERR(err);

		for (i = M5MO_I2C_VERIFY; i; i--) {
			msleep(10);
			err = m5mo_readb(client, M5MO_CATEGORY_LENS, M5MO_LENS_AF_STATUS, &status);
			CHECK_ERR(err);
			
			if ((status & 0x01) == 0x00) 
				break;
		}		
		
		return 0;
	}

	if(val < FOCUS_MODE_AUTO_DEFAULT )
		state->focus.mode = val; //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110202
	
	cam_dbg("X\n");
	return 0;
}

static int m5mo_set_af(struct v4l2_subdev *sd, int val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m5mo_state *state = to_state(sd);
	int i, status, err;
	
	
	cam_info("%s, mode %#x\n", val ? "start" : "stop", state->focus.mode);

	if (state->focus.mode != FOCUS_MODE_CONTINUOUS) {
		err = m5mo_writeb(client, M5MO_CATEGORY_LENS, M5MO_LENS_AF_START, val);
		CHECK_ERR(err);
	} else {
		err = m5mo_writeb(client, M5MO_CATEGORY_LENS, M5MO_LENS_AF_START, 
			val ? 0x02 : 0x00);
		CHECK_ERR(err);

		err = -EBUSY;
		for (i = M5MO_I2C_VERIFY; i && err; i--) {
			msleep(10);
			err = m5mo_readb(client, M5MO_CATEGORY_LENS, M5MO_LENS_AF_STATUS, &status);
			CHECK_ERR(err);
		
			if ((val && status == 0x05) || (!val && status != 0x05))
				err = 0;
		}
	}	
	
	cam_dbg("X\n");
	return err;
}

static int m5mo_set_touch_auto_focus(struct v4l2_subdev *sd, int val)
{	
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m5mo_state *state = to_state(sd);
	int err=0;
	cam_info("%s\n", val ? "start" : "stop");

	if (val) {
		if(state->focus.mode != FOCUS_MODE_TOUCH)
		{
			err = m5mo_set_af_mode(sd, FOCUS_MODE_TOUCH);
			if (err < 0) {
				cam_err("m5mo_set_af_mode failed\n");
				return err;
			}
		}
		err = m5mo_writew(client, M5MO_CATEGORY_LENS, 
				M5MO_LENS_AF_TOUCH_POSX, state->focus.pos_x);
		CHECK_ERR(err);
		err = m5mo_writew(client, M5MO_CATEGORY_LENS, 
				M5MO_LENS_AF_TOUCH_POSY, state->focus.pos_y);
		CHECK_ERR(err);
	}

	cam_dbg("X\n");
	return err;
}

static int m5mo_set_continous_af(struct v4l2_subdev *sd, int val)
{
	int err;
	cam_info("%s\n", val ? "start" : "stop");

	if (val) {
		err = m5mo_set_af_mode(sd, FOCUS_MODE_CONTINUOUS);
		if (err < 0) {
			cam_err("m5mo_set_af_mode failed\n");
			return err;
		}
	}
	err = m5mo_set_af(sd, val);
	if (err < 0)
		cam_err("m5mo_set_af failed\n");

	cam_dbg("X\n");
	return err;
}

static int m5mo_set_zoom(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_queryctrl qc = {0,};
	int val = ctrl->value, err;
	int zoom[31] = { 1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15, 17, 18, 19, 20,
					 21, 22, 24, 25, 26, 28, 29, 30, 31, 32, 34, 35, 36, 38, 39};

	#if 0 //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110202
	if(state->m_zoom_level == val)
		return 0;
	#endif

	cam_dbg("E, value %d\n", val);

	qc.id = ctrl->id;
	m5mo_queryctrl(sd, &qc);

	if (val < qc.minimum || val > qc.maximum) {
		cam_warn("invalied value, %d\n", val);
		val = qc.default_value;
	}
	
	err = m5mo_writeb(client, M5MO_CATEGORY_MON, M5MO_MON_ZOOM, zoom[val]);
	CHECK_ERR(err);
	
	state->m_zoom_level = val;
		
	cam_dbg("X\n");
	return 0;
}

static int m5mo_set_jpeg_quality(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct v4l2_queryctrl qc = {0,};
	int val = ctrl->value, err;
	cam_dbg("E, value %d\n", val);
	
	#if 0
	qc.id = ctrl->id;
	m5mo_queryctrl(sd, &qc);

	if (val < qc.minimum || val > qc.maximum) {
		cam_warn("invalied value, %d\n", val);
		val = qc.default_value;
	}
	#endif
	
	err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_JPEG_RATIO, 0x62);
	CHECK_ERR(err);

	if(val > 70)    // superfine
	{
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_JPEG_RATIO_OFS, 0x00);
		CHECK_ERR(err);
	}
	else if(val > 40)   // fine
	{
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_JPEG_RATIO_OFS, 0x05);
		CHECK_ERR(err);
	}
	else    // normal
	{
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_JPEG_RATIO_OFS, 0x0A);
		CHECK_ERR(err);
	}

	cam_dbg("X\n");
	return 0;
}

//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101210
static int m5mo_set_anti_banding(struct v4l2_subdev *sd, int val)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

	if(state->m_anti_banding_mode == val)
		return 0;

	cam_dbg("E, value %d\n", val);

retry:
	switch (val) {
	case ANTI_BANDING_AUTO:
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_FLICKER, 0x00);
		CHECK_ERR(err);
		break;
	case ANTI_BANDING_50HZ:
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_FLICKER, 0x01);
		CHECK_ERR(err);
		break;
	case ANTI_BANDING_60HZ:
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_FLICKER, 0x02);
		CHECK_ERR(err);
		break;
	case ANTI_BANDING_OFF:
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_FLICKER, 0x04);
		CHECK_ERR(err);
		break;		
	default:
		cam_warn("invalid value, %d\n", val);
		val = ANTI_BANDING_60HZ;
		goto retry;
	}
	
	state->m_anti_banding_mode = val;
		
	cam_dbg("X\n");
	return 0;
}


//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101222
static int m5mo_set_anti_shake(struct v4l2_subdev *sd, int val)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

	if (state->sensor_mode != SENSOR_CAMERA)
		return 0;
	
	if(state->m_anti_shake_mode == val)
		return 0;

	cam_dbg("E, value %d\n", val);
	
retry:
	switch (val) {
	case ANTI_SHAKE_OFF:
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x00);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x00);
		CHECK_ERR(err);
		break;
	case ANTI_SHAKE_STILL_ON:
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x0E);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x0E);
		CHECK_ERR(err);
		break;
	case ANTI_SHAKE_MOVIE_ON:
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x0E);
		CHECK_ERR(err);		
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_CAP, 0x0E);
		CHECK_ERR(err);
		break;
	default:
		cam_warn("invalid value, %d\n", val);
		val = ANTI_SHAKE_OFF;
		goto retry;
	}
	
	state->m_anti_shake_mode = val;
		
	cam_dbg("X\n");
	return 0;
}

//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101228
static int m5mo_set_face_detection(struct v4l2_subdev *sd, int val)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

	if(state->m_face_detection_mode == val)
		return 0;
	
	cam_dbg("E, value %d\n", val);

retry:
	switch (val) {
	case FACE_DETECTION_OFF:
		err = m5mo_writeb(client, M5MO_CATEGORY_FD, M5MO_FD_CTL, 0x00);
		CHECK_ERR(err);
		break;
        case FACE_DETECTION_ON:
        case FACE_DETECTION_NOLINE:
        case FACE_DETECTION_ON_BEAUTY:
		err = m5mo_writeb(client, M5MO_CATEGORY_FD, M5MO_FD_CTL, 0x11);
		CHECK_ERR(err);		
		break;
	default:
		cam_warn("invalid value, %d\n", val);
		val = FACE_DETECTION_OFF;
		goto retry;
	}
	
	state->m_face_detection_mode = val;
		
	cam_dbg("X\n");
	return 0;
}


static int m5mo_start_capture(struct v4l2_subdev *sd, int val)
{	
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m5mo_state *state = to_state(sd);
	int err, int_factor;
	int num, den;
	cam_dbg("E\n");

	m5mo_clear_interrupt(sd);
	
	//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110113
	err = m5mo_set_frmsize(sd);
	CHECK_ERR(err);
	
	err = m5mo_writeb(client, M5MO_CATEGORY_CAPCTRL, M5MO_CAPCTRL_TRANSFER, 0x01);	
	int_factor = m5mo_wait_interrupt(sd, M5MO_ISP_INT_TIMEOUT);
	if (!(int_factor & M5MO_INT_CAPTURE)) {
		cam_warn("M5MO_INT_CAPTURE isn't issued\n");
		return -ETIMEDOUT;
	}

	err = m5mo_readl(client, M5MO_CATEGORY_CAPCTRL, M5MO_CAPCTRL_IMG_SIZE, 
				&state->jpeg.main_size);
	CHECK_ERR(err);	
	err = m5mo_readl(client, M5MO_CATEGORY_CAPCTRL, M5MO_CAPCTRL_THUMB_SIZE, 
				&state->jpeg.thumb_size);
	CHECK_ERR(err);
	
	state->jpeg.main_offset = 0;
	state->jpeg.thumb_offset = M5MO_JPEG_MAXSIZE;
	state->jpeg.postview_offset = M5MO_JPEG_MAXSIZE + M5MO_THUMB_MAXSIZE;

	/* EXIF */
	err = m5mo_readl(client, M5MO_CATEGORY_EXIF, M5MO_EXIF_EXPTIME_NUM, &num);
	CHECK_ERR(err);
	err = m5mo_readl(client, M5MO_CATEGORY_EXIF, M5MO_EXIF_EXPTIME_DEN, &den);
	CHECK_ERR(err);	
	state->exif.exptime = (u32)num*1000000/den;

	err = m5mo_readw(client, M5MO_CATEGORY_EXIF, M5MO_EXIF_FLASH, &num);
	CHECK_ERR(err);
	state->exif.flash = (u16)num;
	
	err = m5mo_readw(client, M5MO_CATEGORY_EXIF, M5MO_EXIF_ISO, &num);
	CHECK_ERR(err);
	state->exif.iso = (u16)num;

	err = m5mo_readl(client, M5MO_CATEGORY_EXIF, M5MO_EXIF_TV_NUM, &num);
	CHECK_ERR(err);
	err = m5mo_readl(client, M5MO_CATEGORY_EXIF, M5MO_EXIF_TV_DEN, &den);
	CHECK_ERR(err);	
	state->exif.tv = num*M5MO_DEF_APEX_DEN/den;
	
	err = m5mo_readl(client, M5MO_CATEGORY_EXIF, M5MO_EXIF_BV_NUM, &num);
	CHECK_ERR(err);
	err = m5mo_readl(client, M5MO_CATEGORY_EXIF, M5MO_EXIF_BV_DEN, &den);
	CHECK_ERR(err);
	state->exif.bv = num*M5MO_DEF_APEX_DEN/den;

	err = m5mo_readl(client, M5MO_CATEGORY_EXIF, M5MO_EXIF_EBV_NUM, &num);
	CHECK_ERR(err);
	err = m5mo_readl(client, M5MO_CATEGORY_EXIF, M5MO_EXIF_EBV_DEN, &den);
	CHECK_ERR(err);	
	state->exif.ebv = num*M5MO_DEF_APEX_DEN/den;

	cam_dbg("X\n");
	return err;
}

//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101214 : face beauty	
static int m5mo_set_face_beauty(struct v4l2_subdev *sd, int val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err, status;
	cam_dbg("E, value %d\n", val);

retry:
	switch (val) {
	case BEAUTY_SHOT_ON:
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPRAM_BEAUTY_EN, 0x01);
		CHECK_ERR(err);
		
		break;
		
	case BEAUTY_SHOT_OFF:
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPRAM_BEAUTY_EN, 0x00);
		CHECK_ERR(err);
		break;

	default:
		cam_warn("invalid value, %d\n", val);
		val = BEAUTY_SHOT_OFF;
		goto retry;
	}
	
	cam_dbg("X\n");
	return 0;
}

//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101214 : vintage mode	
static int m5mo_set_vintage_mode(struct v4l2_subdev *sd, int val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;
	cam_dbg("E, value %d\n", val);

retry:
	switch (val) {
	case VINTAGE_MODE_NORMAL:
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPRAM_VINTAGE_TYPE, 0x00);
		CHECK_ERR(err);		

		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPRAM_VINTAGE_EN, 0x01);
		CHECK_ERR(err);		
		break;

	case VINTAGE_MODE_WARM:
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPRAM_VINTAGE_TYPE, 0x01);
		CHECK_ERR(err);	

		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPRAM_VINTAGE_EN, 0x01);
		CHECK_ERR(err);		
		break;

	case VINTAGE_MODE_COOL:
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPRAM_VINTAGE_TYPE, 0x02);
		CHECK_ERR(err);		
		
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPRAM_VINTAGE_EN, 0x01);
		CHECK_ERR(err);		
		break;

	case VINTAGE_MODE_BNW:
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPRAM_VINTAGE_TYPE, 0x03);
		CHECK_ERR(err);		

		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPRAM_VINTAGE_EN, 0x01);
		CHECK_ERR(err);		
		break;
		
	case VINTAGE_MODE_OFF:
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPRAM_VINTAGE_EN, 0x00);
		CHECK_ERR(err);
		break;

	default:
		cam_warn("invalid value, %d\n", val);
		val = VINTAGE_MODE_OFF;
		goto retry;
	}
	
	cam_dbg("X\n");
	return 0;
}


//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101229
static int m5mo_set_wdr(struct v4l2_subdev *sd, int val)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

	if (state->sensor_mode != SENSOR_CAMERA)
		return 0;
	
	if(state->m_wdr_mode == val)
		return 0;

	cam_dbg("E, value %d\n", val);
	
retry:
	switch (val) {
    	case WDR_OFF:
		err = m5mo_writeb(client, M5MO_CATEGORY_MON, M5MO_MON_TONE_CTRL, 0x05);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_WDR_EN, 0x00);
		CHECK_ERR(err);
		break;
    	case WDR_ON:
		err = m5mo_writeb(client, M5MO_CATEGORY_MON, M5MO_MON_TONE_CTRL, 0x09);
		CHECK_ERR(err);		
		err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_WDR_EN, 0x01); //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110127
		CHECK_ERR(err);
		break;
	default:
		cam_warn("invalid value, %d\n", val);
		val = WDR_OFF;
		goto retry;
	}
	
	state->m_wdr_mode = val;
		
	cam_dbg("X\n");
	return 0;
}

//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110119
static int m5mo_set_ae_awb(struct v4l2_subdev *sd, int val)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

	if(state->m_ae_awb_mode == val)
		return 0;
	
	cam_dbg("E, value %d\n", val);

retry:
	switch (val) {
    	case AE_LOCK_AWB_UNLOCK:
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_LOCK, 0x01);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_WB, M5MO_WB_AWB_LOCK, 0x00);
		CHECK_ERR(err);
		break;
    	case AE_UNLOCK_AWB_LOCK:
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_LOCK, 0x00);
		CHECK_ERR(err);		
		err = m5mo_writeb(client, M5MO_CATEGORY_WB, M5MO_WB_AWB_LOCK, 0x01);
		CHECK_ERR(err);
		break;
    	case AE_LOCK_AWB_LOCK:
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_LOCK, 0x01);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_WB, M5MO_WB_AWB_LOCK, 0x01);
		CHECK_ERR(err);
		break;
    	case AE_UNLOCK_AWB_UNLOCK:
		err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_LOCK, 0x00);
		CHECK_ERR(err);		
		err = m5mo_writeb(client, M5MO_CATEGORY_WB, M5MO_WB_AWB_LOCK, 0x00);
		CHECK_ERR(err);
		break;		
	default:
		cam_warn("invalid value, %d\n", val);
		val = AE_UNLOCK_AWB_UNLOCK;
		goto retry;
	}

	state->m_ae_awb_mode = val;
	
	cam_dbg("X\n");
	return 0;
}

static int m5mo_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i, err = 0;

	cam_dbg("id %d, value %d\n", ctrl->id - V4L2_CID_PRIVATE_BASE, ctrl->value);
	
	if (!state->initialized) {
		for (i = 0; i < ARRAY_SIZE(m5mo_ctrls); i++) {
			if (ctrl->id == m5mo_ctrls[i].id) {
				m5mo_ctrls[i].value = ctrl->value;
				return 0;
			}
		}
		cam_warn("camera isn't initialized\n");
		return 0;
	}

	switch (ctrl->id) {
	case V4L2_CID_CAMERA_SENSOR_MODE:
		err = m5mo_set_sensor_mode(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_FLASH_MODE:
		err = m5mo_set_flash(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_BRIGHTNESS:
		err = m5mo_set_exposure(sd, ctrl);
		break;
		
	case V4L2_CID_CAMERA_CONTRAST:
		err = m5mo_set_contrast(sd, ctrl);
		break;
	
	case V4L2_CID_CAMERA_SATURATION:
		err = m5mo_set_saturation(sd, ctrl);
		break;
	
	case V4L2_CID_CAMERA_SHARPNESS:
		err = m5mo_set_sharpness(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_METERING:
		err = m5mo_set_metering(sd, ctrl->value);
		break;
		
	case V4L2_CID_CAMERA_WHITE_BALANCE:
		err = m5mo_set_whitebalance(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_EFFECT:
		err = m5mo_set_effect(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_ISO:
		err = m5mo_set_iso(sd, ctrl);
		break;
		
	case V4L2_CID_CAMERA_FOCUS_MODE:
		err = m5mo_set_af_mode(sd, ctrl->value);
		// state->focus.mode = ctrl->value; //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110202
		break;

	case V4L2_CID_CAMERA_SET_AUTO_FOCUS:
		err = m5mo_set_af(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_OBJECT_POSITION_X:
		state->focus.pos_x = ctrl->value;
		break;

	case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
		state->focus.pos_y = ctrl->value;
		break;
	
	case V4L2_CID_CAMERA_TOUCH_AF_START_STOP:
		if(ctrl->value)
		err = m5mo_set_touch_auto_focus(sd, ctrl->value);
		break;

	case V4L2_CID_CAMERA_CAF_START_STOP:
		err = m5mo_set_continous_af(sd, ctrl->value);
		break;
		
	case V4L2_CID_CAMERA_ZOOM:
		err = m5mo_set_zoom(sd, ctrl);
		break;

	case V4L2_CID_CAM_JPEG_QUALITY:
		err = m5mo_set_jpeg_quality(sd, ctrl);		
		break;

	case V4L2_CID_CAMERA_CAPTURE:
		err = m5mo_start_capture(sd, ctrl->value);
		break;

	//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101210
	case V4L2_CID_CAMERA_ANTI_BANDING: 
		err = m5mo_set_anti_banding(sd, ctrl->value);
		break;
		
	//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101214 : vintage mode
	case V4L2_CID_CAMERA_VINTAGE_MODE: 
		err = 0; //m5mo_set_vintage_mode(sd, ctrl->value);
		break;

	//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101214 : face beauty
	case V4L2_CID_CAMERA_BEAUTY_SHOT: 
		err = 0; //m5mo_set_face_beauty(sd, ctrl->value);
		break;

	 //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101222
        case V4L2_CID_CAMERA_ANTI_SHAKE:
            err = m5mo_set_anti_shake(sd, ctrl->value);
            break;		
		
	//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101228
        case V4L2_CID_CAMERA_FACE_DETECTION:
            err = m5mo_set_face_detection(sd, ctrl->value);
            break;
			
 	//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101229
        case V4L2_CID_CAMERA_WDR:
            err = m5mo_set_wdr(sd, ctrl->value);
	     break;		

       //NAGSM_ANDROID_HQ_CAMERA_SORACHOI_20101231 : EV-P for Scene Mode    
	case V4L2_CID_CAMERA_EV_PROGRAM_MODE:
            err = m5mo_set_ev_program_mode(sd, ctrl->value);
            break;

       //NAGSM_ANDROID_HQ_CAMERA_SORACHOI_20101231 : MCC for Scene Mode 
	case V4L2_CID_CAMERA_MCC_MODE:
            err = m5mo_set_mcc_mode(sd, ctrl->value);       
            break;			
			
        //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20101224
	case V4L2_CID_CAMERA_AEAWB_LOCK_UNLOCK:
		err = m5mo_set_ae_awb(sd, ctrl->value);
		break;			
		
        //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110122
	case V4L2_CID_CAM_AF_ZERO_STEP:
		err = m5mo_set_zero_step(sd);
		break;		
		
	//NAGSM_ANDROID_HQ_CAMERA_Sungkoolee_20110218 : added rear VT mode 
	case V4L2_CID_CAMERA_VT_MODE:
		if(ctrl->value == 1) // VT mode
		  {
		    cam_err( "%s: V4L2_CID_CAMERA_VT_MODE : state->vt_mode %d \n", __func__, ctrl->value);
		    err = m5mo_writeb(client, M5MO_CATEGORY_AE, M5MO_AE_EVP_MON, 0x11); // VT set
		  }
		break;

	case V4L2_CID_CAMERA_CHECK_DATALINE: 
		state->check_dataline = ctrl->value;
		break;

	case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
		err = m5mo_check_dataline(sd, 0);
		break;	
		
	default:
		cam_err("no such control id %d, value %d\n", 
				ctrl->id - V4L2_CID_PRIVATE_BASE, ctrl->value);
		// err = -ENOIOCTLCMD;
		err = 0;
		break;
	}

	if (err < 0 && err != -ENOIOCTLCMD)
		cam_err("failed, id %d, value %d\n", 
				ctrl->id - V4L2_CID_PRIVATE_BASE, ctrl->value);
	return err;
}

static int 
m5mo_program_fw(struct i2c_client *c, u8 *buf, u32 addr, u32 unit, u32 count)
{
	u32 val;
	u32 intram_unit = 0x1000;
	int i, j, retries, err = 0;

	for (i = 0; i < count; i++) {
		/* Set Flash ROM memory address */
		err = m5mo_writel(c, M5MO_CATEGORY_FLASH, M5MO_FLASH_ADDR, addr);
		CHECK_ERR(err);

		/* Erase FLASH ROM entire memory */
		err = m5mo_writeb(c, M5MO_CATEGORY_FLASH, M5MO_FLASH_ERASE, 0x01);
		CHECK_ERR(err);
		/* Response while sector-erase is operating */
		retries = 0;
		do {
			mdelay(10);
			err = m5mo_readb(c, M5MO_CATEGORY_FLASH, M5MO_FLASH_ERASE, &val);			
			CHECK_ERR(err);
		} while (val && retries++ < M5MO_I2C_VERIFY);
		
		/* Set FLASH ROM programming size */
		err = m5mo_writew(c, M5MO_CATEGORY_FLASH, M5MO_FLASH_BYTE, 
									unit == SZ_64K ? 0 : unit);
		CHECK_ERR(err);

		/* Clear M-5MoLS internal RAM */
		err = m5mo_writeb(c, M5MO_CATEGORY_FLASH, M5MO_FLASH_RAM_CLEAR, 0x01);
		CHECK_ERR(err);

		/* Set Flash ROM programming address */
		err = m5mo_writel(c, M5MO_CATEGORY_FLASH, M5MO_FLASH_ADDR, addr);
		CHECK_ERR(err);
	
		/* Send programmed firmware */
		for (j = 0; j < unit; j += intram_unit) {
			err = m5mo_mem_write(c, intram_unit, M5MO_INT_RAM_BASE_ADDR + j, 
													buf + (i * unit) + j);
			CHECK_ERR(err);
			mdelay(10);
		}

		/* Start Programming */
		err = m5mo_writeb(c, M5MO_CATEGORY_FLASH, M5MO_FLASH_WR, 0x01);
		CHECK_ERR(err);
		
		/* Confirm programming has been completed */
		retries = 0;
		do {
			mdelay(10);
			err = m5mo_readb(c, M5MO_CATEGORY_FLASH, M5MO_FLASH_WR, &val);			
			CHECK_ERR(err);
		} while (val && retries++ < M5MO_I2C_VERIFY);
		
		/* Increase Flash ROM memory address */
		addr += unit;
	}
	return 0;
}


//#define MDNIE_TUNING

#ifndef MDNIE_TUNING

static int m5mo_load_fw(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 *buf, val;
	int err;

	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	
	#ifdef SDCARD_FW
	fp = filp_open(M5MO_SD_FW_PATH, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) 
	#endif	
	{
		if(IS_M5MO_FSI)
			fp = filp_open(M5MO_FSI_FW_PATH, O_RDONLY, S_IRUSR);
		else
			fp = filp_open(M5MO_BSI_FW_PATH, O_RDONLY, S_IRUSR);

		if (IS_ERR(fp)) {
			if(IS_M5MO_FSI)
				cam_err("failed to open %s\n", M5MO_FSI_FW_PATH);
			else
				cam_err("failed to open %s\n", M5MO_BSI_FW_PATH);
					
			return -ENOENT;
		}
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	
	if(IS_M5MO_FSI)
		cam_info("start, file path %s, size %ld Bytes\n", M5MO_FSI_FW_PATH, fsize);
	else
		cam_info("start, file path %s, size %ld Bytes\n", M5MO_BSI_FW_PATH, fsize);
	
	buf = vmalloc(fsize);
	if (!buf) {
		cam_err("failed to allocate memory\n");
		return -ENOMEM;
	}

	nread = vfs_read(fp, (char __user *)buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		cam_err("failed to read firmware file, nread %ld Bytes\n", nread);
		return -EIO;
	}	

	/* set pin */
	val = 0x7E;
	err = m5mo_mem_write(client, sizeof(val), 0x50000308, &val);
	CHECK_ERR(err);
	/* select flash memory */
	err = m5mo_writeb(client, M5MO_CATEGORY_FLASH, M5MO_FLASH_SEL, 0x01);
	CHECK_ERR(err);
	/* program FLSH ROM */
	err = m5mo_program_fw(client, buf, M5MO_FLASH_BASE_ADDR, SZ_64K, 31);
	CHECK_ERR(err);
	err = m5mo_program_fw(client, buf, M5MO_FLASH_BASE_ADDR + SZ_64K * 31, SZ_8K, 4);
	CHECK_ERR(err);
	
	cam_info("end\n");
	

	vfree(buf);
	filp_close(fp, current->files);
	set_fs(old_fs);

	return 0;
}


#else
extern unsigned short *test[1];
EXPORT_SYMBOL(test);
extern void mDNIe_txtbuf_to_parsing(void);
extern void mDNIe_txtbuf_to_parsing_for_lightsensor(void);
extern void mDNIe_txtbuf_to_parsing_for_backlight(void);
static int m5mo_load_fw(struct v4l2_subdev *sd)
{


	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 *buf, val;
	int err;
	int i = 0;

	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	
	fp = filp_open(M5MO_SD_FW_PATH, O_RDONLY, S_IRUSR);

	if (IS_ERR(fp)) 
	{
		printk("filp_open error \n");
		return 0;
	}


	fsize = fp->f_path.dentry->d_inode->i_size;
	
	printk("m5mo_load_fw  fsize = %d\n", fsize);
	
	buf = vmalloc(fsize);
	if (!buf) {
		cam_err("failed to allocate memory\n");
		return -ENOMEM;
	}

	nread = vfs_read(fp, (char __user *)buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		cam_err("failed to read firmware file, nread %ld Bytes\n", nread);
		return -EIO;
	}	


	test[0] = buf;
	
	for(i = 0; i < fsize/2; i++){
		//	printk("ce147_update_fw: , fw_size[0]: %d, test[0]: 0x%x\n", fsize, test[0][i]);
			test[0][i] = ((test[0][i]&0xff00)>>8)|((test[0][i]&0x00ff)<<8);
			printk("ce147_update_fw: , test1[0][%d]: 0x%x\n", i, test[0][i]);				
		}

	/*for mdnie tuning*/
	printk("\n\n finish copy\n");
	mDNIe_txtbuf_to_parsing();
	//mDNIe_txtbuf_to_parsing_for_lightsensor();
	//mDNIe_txtbuf_to_parsing_for_backlight();


	return 0;
}
#endif




static int m5mo_check_version(struct i2c_client *client)
{
	u32 ver_chip, ver_param, ver_awb, val;
	char ver_user[M5MO_FW_VER_LEN + 1];
	int i;
	
	m5mo_readb(client, M5MO_CATEGORY_SYS, M5MO_SYS_PJT_CODE, &ver_chip);
	m5mo_readw(client, M5MO_CATEGORY_SYS, M5MO_SYS_VER_PARAM, &ver_param);
	m5mo_readw(client, M5MO_CATEGORY_SYS, M5MO_SYS_VER_AWB, &ver_awb);

	for (i = 0; i < M5MO_FW_VER_LEN; i++) {
		m5mo_readb(client, M5MO_CATEGORY_SYS, M5MO_SYS_USER_VER, &val);
		ver_user[i] = (char)val;
	}
	ver_user[i] = '\0';

	cam_info("***********************************\n");
	cam_info("Chip\tParam\tAWB\tUSER\n");
	cam_info("-----------------------------------\n");
	cam_info("%#02x\t%#04x\t%#04x\t%s\n",
		ver_chip, ver_param, ver_awb, ver_user);
	cam_info("***********************************\n");

	return 0;
}

static int m5mo_init_param(struct v4l2_subdev *sd)
{
	struct m5mo_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_control ctrl;
	int i, err;
	cam_dbg("E\n");

	err = m5mo_set_frmsize(sd);
	CHECK_ERR(err);

	err = m5mo_writeb(client, M5MO_CATEGORY_PARM, M5MO_PARM_OUT_SEL, 0x02);
	CHECK_ERR(err);

	//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110216
	err = m5mo_writeb(client, M5MO_CATEGORY_PARM, M5MO_PARM_FLEX_FPS, state->fps != 30 ? state->fps : 0);
	CHECK_ERR(err);
	
	err = m5mo_writeb(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_YUVOUT_MAIN, 0x20);
	CHECK_ERR(err);
	
	err = m5mo_writel(client, M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_THUMB_JPEG_MAX, M5MO_THUMB_MAXSIZE);
	CHECK_ERR(err);

#if 0
	if (HWREV == SEINE_HWREV_UNIV02 || HWREV == SEINE_HWREV_UNIV03) {
		err = m5mo_writeb(client, M5MO_CATEGORY_MON, M5MO_MON_MON_REVERSE, 0x01);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_MON, M5MO_MON_MON_MIRROR, 0x01);
		CHECK_ERR(err);		
		err = m5mo_writeb(client, M5MO_CATEGORY_MON, M5MO_MON_SHOT_REVERSE, 0x01);
		CHECK_ERR(err);
		err = m5mo_writeb(client, M5MO_CATEGORY_MON, M5MO_MON_SHOT_MIRROR, 0x01);
		CHECK_ERR(err);
	}
#endif

	for (i = 0; i < ARRAY_SIZE(m5mo_ctrls); i++) {
		if (m5mo_ctrls[i].value != m5mo_ctrls[i].default_value) {
			ctrl.id = m5mo_ctrls[i].id;
			ctrl.value = m5mo_ctrls[i].value;
			m5mo_s_ctrl(sd, &ctrl);
		}
	}

	cam_dbg("X\n");
	return 0;
}

static int m5mo_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct m5mo_state *state = to_state(sd);
	u32 int_factor;
	int err;

	state->initialized = 1;
	
	m5mo_clear_interrupt(sd);
	
	/* start camera program(parallel FLASH ROM) */
	err = m5mo_writeb(client, M5MO_CATEGORY_FLASH, M5MO_FLASH_CAM_START, 0x01);
	CHECK_ERR(err);
	
	int_factor = m5mo_wait_interrupt(sd, M5MO_ISP_INT_TIMEOUT);
	if (!(int_factor & M5MO_INT_MODE)) {
		cam_err("firmware was erased?\n");
		return -ETIMEDOUT;
	}
	
	/* check up F/W version */
	err = m5mo_check_version(client);
	CHECK_ERR(err);
	
	m5mo_init_param(sd);
	
	return 0;
}

/*
 * s_config subdev ops
 * With camera device, we need to re-initialize every single opening time therefor,
 * it is not necessary to be initialized on probe time. except for version checking
 * NOTE: version checking is optional
 */
static int m5mo_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct m5mo_state *state = to_state(sd);
	struct m5mo_platform_data *pdata = (struct m5mo_platform_data *)platform_data;
	int err = 0;

	/* Default state values */
	state->initialized = 0;
	state->isp.issued = 1;
	
	state->colorspace = 0;
	state->frmsize = NULL;

	state->sensor_mode = SENSOR_CAMERA;
	state->flash_mode = FLASH_MODE_OFF;
	
	state->fps = 0;			/* auto */
	
	state->jpeg.main_size = 0;
	state->jpeg.main_offset = 0;
	state->jpeg.postview_offset = 0;

	state->check_dataline = 0;
	
	//NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110131
	state->m_contrast_value=0x05;
       state->m_sharpness_value=0x05;
       state->m_wdr_mode=WDR_OFF;
       state->m_anti_shake_mode=ANTI_SHAKE_OFF;
       state->m_white_balance_mode=WHITE_BALANCE_AUTO;
       state->m_anti_banding_mode=ANTI_BANDING_50HZ;
       state->m_ev_mode=EV_DEFAULT;
       state->m_iso_mode=ISO_AUTO;
       state->m_effect_mode=IMAGE_EFFECT_NONE;
       state->m_metering_mode=METERING_CENTER;
       state->m_zoom_level=ZOOM_LEVEL_0;
       state->m_saturation_mode=SATURATION_DEFAULT;
       state->m_ev_program_mode=SCENE_MODE_NONE;
       state->m_mcc_mode=SCENE_MODE_NONE;
	state->m_face_detection_mode=FACE_DETECTION_OFF;   	
	state->focus.mode = FOCUS_MODE_AUTO_DEFAULT; //NAGSM_ANDROID_HQ_CAMERA_SoojinKim_20110215
	state->m_ae_awb_mode = AE_UNLOCK_AWB_UNLOCK;
	
	/* Register ISP irq */
	if (!pdata) {
		cam_err("no platform data\n");
		return -ENODEV;
	}
	
	if (irq) {
		if (pdata->config_isp_irq)
			pdata->config_isp_irq();
		
 		err = request_irq(irq, m5mo_isp_isr, IRQF_TRIGGER_RISING, "m5mo isp", sd);
		if (err) {
			cam_err("failed to request irq\n");
			return err;
		}
		state->isp.irq = irq;
				
		/* wait queue initialize */
		init_waitqueue_head(&state->isp.wait);
	}
	
	return 0;
}


static const struct v4l2_subdev_core_ops m5mo_core_ops = {
	.s_config = m5mo_s_config,	/* Fetch platform data */
	.init = m5mo_init,			/* initializing API */
	.load_fw = m5mo_load_fw,
	.queryctrl = m5mo_queryctrl,
	.g_ctrl = m5mo_g_ctrl,
	.g_ext_ctrls = m5mo_g_ext_ctrls,
	.s_ctrl = m5mo_s_ctrl,
};

static const struct v4l2_subdev_video_ops m5mo_video_ops = {
	.s_fmt = m5mo_s_fmt,
	.g_parm = m5mo_g_parm,
	.s_parm = m5mo_s_parm,
	.enum_framesizes = m5mo_enum_framesizes,
	.s_stream = m5mo_s_stream,
};

static const struct v4l2_subdev_ops m5mo_ops = {
	.core = &m5mo_core_ops,
	.video = &m5mo_video_ops,
};

/*
 * m5mo_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int m5mo_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct m5mo_state *state;
	struct v4l2_subdev *sd;
	
	state = kzalloc(sizeof(struct m5mo_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;
	
	sd = &state->sd;
	strcpy(sd->name, M5MO_DRIVER_NAME);
	
	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &m5mo_ops);
	
	return 0;
}

static int m5mo_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct m5mo_state *state = to_state(sd);
	
	state->initialized = 0;
	
	if (state->isp.irq > 0)
	free_irq(state->isp.irq, sd);
	
	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
	
	return 0;
}

static const struct i2c_device_id m5mo_id[] = {
	{ M5MO_DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, m5mo_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = M5MO_DRIVER_NAME,
	.probe = m5mo_probe,
	.remove = m5mo_remove,
	.id_table = m5mo_id,
};

MODULE_DESCRIPTION("Fujitsu M5MO LS 8MP ISP driver");
MODULE_AUTHOR("BestIQ <@samsung.com>");
MODULE_LICENSE("GPL");
