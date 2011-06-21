/*
 * Driver for MC7 (5MP Camera ISP)
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
#include <media/mc7_platform.h>

#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif

#include <linux/rtc.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <mach/max8998_function.h>

//#define MDNIE_TUNING

#define MC7_DRIVER_NAME	"MC7"

#define FORMAT_FLAGS_COMPRESSED		0x3
#define SENSOR_JPEG_SNAPSHOT_MEMSIZE	0x360000

#define MC7_DEBUG
#define MC7_INFO
#define MC7_CAM_POWER

#ifdef MC7_DEBUG
#define mc7_msg	dev_err
#else
#define mc7_msg 	dev_dbg
#endif

#ifdef MC7_INFO
#define mc7_info	dev_err
#else
#define mc7_info	dev_dbg
#endif

/* Default resolution & pixelformat. plz ref mc7_platform.h */
#define DEFAULT_PIX_FMT		V4L2_PIX_FMT_UYVY	/* YUV422 */
#define DEFUALT_MCLK		24000000
#define POLL_TIME_MS		10

/* Camera ISP command */
#define CMD_RAM_READ 0x00
#define CAM_RAM_WRITE 0x01

#define MC7_LOADER_PATH	"/system/firmware/MC7_SFL.bin"
#define MC7_WRITER_PATH	"/system/firmware/MC7_SFW.bin"

static int fw_update_status = 100;
static int camfw_update = 2;

#define MC7_8BIT   0x0001
#define MC7_16BIT 0x0002
#define MC7_32BIT 0x0004

/* MC7 Sensor Mode */
#define MC7_SYSINIT_MODE		0x0
#define MC7_PARMSET_MODE	0x1
#define MC7_MONITOR_MODE	0x2
#define MC7_STILLCAP_MODE	0x3
#define MC7_I2C_VERIFY_RETRY			200

static unsigned char	MAIN_SW_FW[4]			= {0x0, 0x0, 0x0, 0x0};	/* {FW Maj, FW Min, PRM Maj, PRM Min} */
static int				MAIN_SW_DATE_INFO[3]	= {0x0, 0x0, 0x0};		/* {Year, Month, Date} */

static unsigned char mc7_buf_set_dzoom[32] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F, 0x20};
static int DZoom_State = 0;

enum mc7_oprmode {
	MC7_OPRMODE_VIDEO = 0,
	MC7_OPRMODE_IMAGE = 1,
};

/* Declare Funtion */
static int mc7_set_awb_lock(struct v4l2_subdev *sd, int lock);
static int mc7_set_iso(struct v4l2_subdev * sd, struct v4l2_control * ctrl);
static int mc7_set_metering(struct v4l2_subdev * sd, struct v4l2_control * ctrl);
static int mc7_set_ev(struct v4l2_subdev * sd, struct v4l2_control * ctrl);
static int mc7_set_slow_ae(struct v4l2_subdev * sd, struct v4l2_control * ctrl);
static int mc7_set_gamma(struct v4l2_subdev * sd, struct v4l2_control * ctrl);
static int mc7_set_effect(struct v4l2_subdev * sd, struct v4l2_control * ctrl);
static int mc7_set_white_balance(struct v4l2_subdev * sd, struct v4l2_control * ctrl);
static int mc7_s_ext_ctrl(struct v4l2_subdev *sd, struct v4l2_ext_control *ctrl);

enum mc7_frame_size {
	MC7_PREVIEW_QCIF = 0,
	MC7_PREVIEW_QVGA,
	MC7_PREVIEW_592x480,
	MC7_PREVIEW_VGA,
	MC7_PREVIEW_D1,
	MC7_PREVIEW_WVGA,
	MC7_PREVIEW_720P,
	MC7_PREVIEW_VERTICAL_QCIF,
	MC7_CAPTURE_VGA, /* 640 x 480 */	
	MC7_CAPTURE_WVGA, /* 800 x 480 */
	MC7_CAPTURE_W1MP, /* 1600 x 960 */
	MC7_CAPTURE_2MP, /* UXGA  - 1600 x 1200 */
	MC7_CAPTURE_W2MP, /* 35mm Academy Offset Standard 1.66  - 2048 x 1232, 2.4MP */	
	MC7_CAPTURE_3MP, /* QXGA  - 2048 x 1536 */
	MC7_CAPTURE_W4MP, /* WQXGA - 2560 x 1536 */
	MC7_CAPTURE_5MP, /* 2560 x 1920 */
};

struct mc7_enum_framesize {
	/* mode is 0 for preview, 1 for capture */
	enum mc7_oprmode mode;
	unsigned int index;
	unsigned int width;
	unsigned int height;	
};

static struct mc7_enum_framesize mc7_framesize_list[] = {
	{ MC7_OPRMODE_VIDEO, MC7_PREVIEW_QCIF,       176,  144 },
	{ MC7_OPRMODE_VIDEO, MC7_PREVIEW_QVGA,       320,  240 },
	{ MC7_OPRMODE_VIDEO, MC7_PREVIEW_592x480,    592,  480 },
	{ MC7_OPRMODE_VIDEO, MC7_PREVIEW_VGA,		 640,  480 },
	{ MC7_OPRMODE_VIDEO, MC7_PREVIEW_D1,         720,  480 },
	{ MC7_OPRMODE_VIDEO, MC7_PREVIEW_WVGA,       800,  480 },
	{ MC7_OPRMODE_VIDEO, MC7_PREVIEW_720P,      1280,  720 },
	{ MC7_OPRMODE_VIDEO, MC7_PREVIEW_VERTICAL_QCIF,	144,	176},
	{ MC7_OPRMODE_IMAGE, MC7_CAPTURE_VGA,		 640,  480 },
	{ MC7_OPRMODE_IMAGE, MC7_CAPTURE_WVGA,		 800,  480 },
	{ MC7_OPRMODE_IMAGE, MC7_CAPTURE_W1MP,   	1600,  960 },
	{ MC7_OPRMODE_IMAGE, MC7_CAPTURE_2MP,       1600, 1200 },
	{ MC7_OPRMODE_IMAGE, MC7_CAPTURE_W2MP,		2048, 1232 },
	{ MC7_OPRMODE_IMAGE, MC7_CAPTURE_3MP,       2048, 1536 },
	{ MC7_OPRMODE_IMAGE, MC7_CAPTURE_W4MP,      2560, 1536 },
	{ MC7_OPRMODE_IMAGE, MC7_CAPTURE_5MP,       2560, 1920 },
};

struct mc7_version {
	unsigned int major;
	unsigned int minor;
};

struct mc7_date_info {
	unsigned int year;
	unsigned int month;
	unsigned int date;
};

enum mc7_runmode {
	MC7_RUNMODE_NOTREADY,
	MC7_RUNMODE_IDLE, 
	MC7_RUNMODE_READY,
	MC7_RUNMODE_RUNNING, 
};

struct mc7_firmware {
	unsigned int addr;
	unsigned int size;
};

/* Camera functional setting values configured by user concept */
struct mc7_userset {
	signed int exposure_bias;	/* V4L2_CID_EXPOSURE */
	unsigned int ae_lock;
	unsigned int awb_lock;
	unsigned int auto_wb;		/* V4L2_CID_AUTO_WHITE_BALANCE */
	unsigned int manual_wb;		/* V4L2_CID_WHITE_BALANCE_PRESET */
	unsigned int wb_temp;		/* V4L2_CID_WHITE_BALANCE_TEMPERATURE */
	unsigned int effect;		/* Color FX (AKA Color tone) */
	unsigned int contrast;		/* V4L2_CID_CONTRAST */
	unsigned int saturation;	/* V4L2_CID_SATURATION */
	unsigned int sharpness;		/* V4L2_CID_SHARPNESS */
	unsigned int glamour;
};

struct mc7_jpeg_param {
	unsigned int enable;
	unsigned int quality;
	unsigned int main_size;  /* Main JPEG file size */
	unsigned int thumb_size; /* Thumbnail file size */
	unsigned int main_offset;
	unsigned int thumb_offset;
	unsigned int postview_offset;
} ; 

struct mc7_position {
	int x;
	int y;
} ; 

struct mc7_gps_info{
	unsigned char mc7_gps_buf[8];
	unsigned char mc7_altitude_buf[4];
	long gps_timeStamp;
};

struct mc7_sensor_maker{
	unsigned int maker;
	unsigned int optical;
};

struct mc7_version_af{
	unsigned int low;
	unsigned int high;
};

struct mc7_gamma{
	unsigned int rg_low;
	unsigned int rg_high;
	unsigned int bg_low;
	unsigned int bg_high;	
};

#if 0
struct tm {
   int     tm_sec;         /* seconds */
   int     tm_min;         /* minutes */
   int     tm_hour;        /* hours */
   int     tm_mday;        /* day of the month */
   int     tm_mon;         /* month */
   int     tm_year;        /* year */
   int     tm_wday;        /* day of the week */
   int     tm_yday;        /* day in the year */
   int     tm_isdst;       /* daylight saving time */

   long int tm_gmtoff;     /* Seconds east of UTC.  */
   const char *tm_zone;    /* Timezone abbreviation.  */
};
#endif

struct gps_info_common {
	unsigned int 	direction;
	unsigned int 	dgree;
	unsigned int	minute;
	unsigned int	second;
};

struct mc7_state {
	struct mc7_platform_data *pdata;
	struct v4l2_subdev sd;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	struct mc7_userset userset;
	struct mc7_jpeg_param jpeg;
	struct mc7_version fw;
	struct mc7_version prm;
	struct mc7_date_info dateinfo;
	struct mc7_firmware fw_info;
	struct mc7_position position;
	struct mc7_sensor_maker sensor_info;	
	struct mc7_version_af af_info;
	struct mc7_gamma gamma;
	struct v4l2_streamparm strm;
	struct mc7_gps_info gpsInfo;
	enum mc7_runmode runmode;
	enum mc7_oprmode oprmode;
	int framesize_index;
	int sensor_version;
	int freq;	/* MCLK in Hz */
	int fps;
	int ot_status;
	int sa_status;
	int anti_banding;	
	int preview_size;
	unsigned int fw_dump_size;
	int hd_preview_on;
	int pre_af_status;
	int cur_af_status;
	struct mc7_version main_sw_fw;
	struct mc7_version main_sw_prm;
	struct mc7_date_info main_sw_dateinfo;
	int exif_orientation_info;
	int check_dataline;
	int hd_slow_ae;
	int hd_gamma;
	int iso;
	int metering;
	int ev;
	int effect;
	int wb;
	struct tm *exifTimeInfo;
#if defined(CONFIG_ARIES_NTT) // Modify NTTS1
	int disable_aeawb_lock;
#endif
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

static inline struct mc7_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct mc7_state, sd);
}

/** 
 * mc7_i2c_write_multi: Write (I2C) multiple bytes to the camera sensor 
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 *
 * Returns 0 on success, <0 on error
 */
static int mc7_i2c_write_multi(struct i2c_client *client, 
		unsigned char *w_data, unsigned int w_len)
{
	int retry_count = 1;
	struct i2c_msg msg = {client->addr, 0, w_len, w_data};

	int ret = -1;

#if 0 //def MC7_DEBUG
	{
		int j;
		printk("W: ");
		for(j = 0; j <= w_len; j++){
			printk("0x%02x ", buf[j]);
		}
		printk("\n");
	}
#endif

	while(retry_count--){
		ret  = i2c_transfer(client->adapter, &msg, 1);
		if(ret == 1)
			break;
		msleep(POLL_TIME_MS);
		}

	return (ret == 1) ? 0 : -EIO;
}

/** 
 * mc7_i2c_read_multi: Read (I2C) multiple bytes to the camera sensor 
 * @client: pointer to i2c_client
 * @cmd: command register
 * @w_data: data to be written
 * @w_len: length of data to be written
 * @r_data: buffer where data is read
 * @r_len: number of bytes to read
 *
 * Returns 0 on success, <0 on error
 */
static int mc7_i2c_read_multi(struct i2c_client *client,
		unsigned char *w_data, unsigned int w_len, 
		unsigned char *r_data, unsigned int r_len)
{
	struct i2c_msg msg = {client->addr, 0, w_len, w_data};
	int ret = -1;
	int retry_count = 1;

#if 0 //def MC7_DEBUG
	{
		int j;
		printk("R: ");
		for(j = 0; j <= w_len; j++){
			printk("0x%02x ", buf[j]);
		}
		printk("\n");
	}
#endif

	while(retry_count--){
		ret = i2c_transfer(client->adapter, &msg, 1);
		if(ret == 1)
			break;
		msleep(POLL_TIME_MS);
	}

	if(ret < 0)
		return -EIO;

	msg.flags = I2C_M_RD;
	msg.len = r_len;
	msg.buf = r_data;

	retry_count = 1;
	while(retry_count--){
		ret  = i2c_transfer(client->adapter, &msg, 1);
		if(ret == 1)
			break;
		msleep(POLL_TIME_MS);
	}

	return (ret == 1) ? 0 : -EIO;
}

#if 0
static int mc7_i2c_write_multi(struct i2c_client *client, unsigned char cmd, 
		unsigned char *w_data, unsigned int w_len)
{
	int retry_count = 1;
	unsigned char buf[w_len+1];
	struct i2c_msg msg = {client->addr, 0, w_len+1, buf};

	int ret = -1;

	buf[0] = cmd;
	memcpy(buf+1, w_data, w_len);

#ifdef MC7_DEBUG
	{
		int j;
		printk("W: ");
		for(j = 0; j <= w_len; j++){
			printk("0x%02x ", buf[j]);
		}
		printk("\n");
	}
#endif

	while(retry_count--){
		ret  = i2c_transfer(client->adapter, &msg, 1);
		if(ret == 1)
			break;
		msleep(POLL_TIME_MS);
		}

	return (ret == 1) ? 0 : -EIO;
}
#endif

static int mc7_write_category_parm(struct i2c_client *client,  
										unsigned char data_length, unsigned char category, unsigned char byte, unsigned int val)
{
	struct i2c_msg msg[1];
	unsigned char data[data_length+4];
	int retry = 0, err;

	if (!client->adapter)
		return -ENODEV;

	if (data_length != MC7_8BIT && data_length != MC7_16BIT && data_length != MC7_32BIT)
		return -EINVAL; 

again:
	printk( "write value from [category:0x%x], [Byte:0x%x] is value:0x%x\n", category, byte, val);
	//print_currenttime();
	
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = data_length + 4;
	msg->buf = data;

	/* high byte goes out first */
	data[0] = data_length + 4;
	data[1] = 0x02;
	data[2] = category;
	data[3] = byte;
	if (data_length == MC7_8BIT)
		data[4] = (unsigned char) (val & 0xff);
	else if (data_length == MC7_16BIT) 
	{
		data[4] = (unsigned char) (val >> 8);
		data[5] = (unsigned char) (val & 0xff);
	} 
	else 
	{
		data[4] = (unsigned char) (val >> 24);
		data[5] = (unsigned char) ( (val >> 16) & 0xff );
		data[6] = (unsigned char) ( (val >> 8) & 0xff );
		data[7] = (unsigned char) (val & 0xff);
	}
	
	err = i2c_transfer(client->adapter, msg, 1);
	if (err < 0)
	{
		printk("wrote 0x%x to offset [category:0x%x], [Byte:0x%x]  addr 0x%x error %d", val, category, byte, msg->addr, err);
		if (retry <= 5) {
			printk("i2c write retry ... %d\n", retry);
			retry++;
			msleep(20);
			goto again;
		}	
		return err;		
	}
	else
	{
		return 0;
	}
}


static int mc7_read_category_parm(struct i2c_client *client, 
								unsigned char data_length, unsigned char category, unsigned char byte, unsigned int* val)
{
	struct i2c_msg msg[1];
	unsigned char data[5];
	unsigned char recv_data[data_length + 1];
	int err, retry=0;

	if (!client->adapter)
		return -ENODEV;
	if (data_length != MC7_8BIT && data_length != MC7_16BIT	&& data_length != MC7_32BIT)		
		return -EINVAL;

read_again:
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 5;
	msg->buf = data;

	/* high byte goes out first */
	data[0] = 0x05;
	data[1] = 0x01;
	data[2] = category;
	data[3] = byte;
	data[4] = data_length;
	
	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0) 
	{
		msg->flags = I2C_M_RD;
		msg->len = data_length + 1;
		msg->buf = recv_data; 
		
		err = i2c_transfer(client->adapter, msg, 1);
	}
	if (err >= 0) 
	{
		/* high byte comes first */		
		if (data_length == MC7_8BIT)			
			*val = recv_data[1];		
		else if (data_length == MC7_16BIT)			
			*val = recv_data[2] + (recv_data[1] << 8);		
		else			
			*val = recv_data[4] + (recv_data[3] << 8) + (recv_data[2] << 16) + (recv_data[1] << 24);

		printk( "read value from [category:0x%x], [Byte:0x%x] is 0x%x\n", category, byte, *val);
		//print_currenttime();
		
		return 0;
	}
	printk( "read from offset [category:0x%x], [Byte:0x%x]  addr 0x%x error %d\n", category, byte, msg->addr, err);
	
	if (retry <= 5) {
		printk("i2c read retry ... %d\n", retry);
		retry++;
		msleep(20);
		goto read_again;
	}

	return err;
}


static int mc7_i2c_verify(struct i2c_client *client, unsigned char category, unsigned char byte, unsigned int value)
{
	unsigned int val = 0, i;

	for(i= 0; i < MC7_I2C_VERIFY_RETRY; i++) 
	{
		mc7_read_category_parm(client, MC7_8BIT, category, byte, &val);
		msleep(10);
		if(val == value) 
		{
			printk("[mc7] i2c verified [c,b,v] [%02x, %02x, %02x] (try = %d)\n", category, byte, value, i);
			return 0;
		}
	}

	return -EBUSY;
}

static int mc7_write_mem(struct i2c_client *client, unsigned short data_length, unsigned int reg, unsigned char* val)
{
	struct i2c_msg msg[1];
	unsigned char *data;
	int retry = 0, err;

	if (!client->adapter)
		return -ENODEV;

	data = kmalloc(8 + data_length, GFP_KERNEL);

again:
	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = data_length + 8;
	msg->buf = data;

	/* high byte goes out first */
	data[0] = 0x00;
	data[1] = 0x04;
	data[2] = (unsigned char) (reg >> 24);
	data[3] = (unsigned char) ( (reg >> 16) & 0xff );
	data[4] = (unsigned char) ( (reg >> 8) & 0xff );
	data[5] = (unsigned char) (reg & 0xff);
	data[6] = (unsigned char) (data_length >> 8);
	data[7] = (unsigned char) (data_length & 0xff);
	memcpy(data+8 , val, data_length);

	err = i2c_transfer(client->adapter, msg, 1);
	if (err >= 0)
	{
		kfree(data);
		return 0;
	}

	printk( "wrote to offset 0x%x addr 0x%x error %d \n", reg, msg->addr, err);

	kfree(data);
	return err;
}


#ifdef MC7_CAM_POWER
#define ADD_VGA_CAM

extern int ldo38_onoff_state;
extern int ldo3_mipi_onoff_state;

static void mc7_ldo_en(bool onoff)
{
	int err;

	/* CAM_IO_EN - GPB(7) */
	err = gpio_request(GPIO_GPB7, "GPB7");

	if(err) {
		printk(KERN_ERR "failed to request GPB7 for camera control\n");

		return err;
	}

	if(onoff == TRUE) { //power on 

		// Turn MIPI_1.1V on if USB or TV out is not enabled
		if(!ldo38_onoff_state)
		{
			//Set_MAX8998_PM_OUTPUT_Voltage(LDO3, VCC_1p100);
			Set_MAX8998_PM_REG(ELDO3, 1);
		}
		
		//  Turn MIPI_1.8V on
		Set_MAX8998_PM_OUTPUT_Voltage(LDO6, VCC_1p800);
		Set_MAX8998_PM_REG(ELDO6, 1);
					
		// Turn 5M_CAM_1.2V on
		Set_MAX8998_PM_OUTPUT_Voltage(BUCK4, VCC_1p200);
		Set_MAX8998_PM_REG(EN4, 1);


		// Turn CAM_SENSOR_A2.8V on
		gpio_direction_output(GPIO_GPB7, 0);	
		gpio_set_value(GPIO_GPB7, 1);
		mdelay(1);


		// Turn 5M_CAM_1.8V on
		Set_MAX8998_PM_OUTPUT_Voltage(LDO14, VCC_1p800);
		Set_MAX8998_PM_REG(ELDO14, 1);

		//Set_MAX8998_PM_OUTPUT_Voltage(LDO16, VCC_1p800);
		//Set_MAX8998_PM_REG(ELDO16, 1);
		
		//Turn CAM_I2C_2.8V on
		Set_MAX8998_PM_OUTPUT_Voltage(LDO15, VCC_2p800);
		Set_MAX8998_PM_REG(ELDO15, 1);
		
		// Turn 5M_CAM_AF_3.0V on
		Set_MAX8998_PM_OUTPUT_Voltage(LDO11, VCC_3p000);
		Set_MAX8998_PM_REG(ELDO11, 1);

		ldo3_mipi_onoff_state = 1;
	}
	
	else { // power off

		// Turn 5M_CAM_AF_3.0V on
		Set_MAX8998_PM_REG(ELDO11, 0);

		// Turn CAM_I2C_2.8V off
		Set_MAX8998_PM_REG(ELDO15, 0);

		// Turn 5M_CAM_1.8V off
		Set_MAX8998_PM_REG(ELDO14, 0);
		//Set_MAX8998_PM_REG(ELDO16, 0);

		mdelay(1);
		
	       // Turn CAM_SENSOR_A2.8V off
		gpio_direction_output(GPIO_GPB7, 1);
		gpio_set_value(GPIO_GPB7, 0);
	
		// Turn 5M_CAM_1.2V  off
		Set_MAX8998_PM_REG(EN4, 0);

		// Turn MIPI_1.1V off if USB or TV out is not enabled
		if(!ldo38_onoff_state)
		{
			Set_MAX8998_PM_REG(ELDO3, 0);
		}			

		// Turn MIPI_1.8V off
		Set_MAX8998_PM_REG(ELDO6, 0);	

		ldo3_mipi_onoff_state = 0;
	}

	gpio_free(GPIO_GPB7);
}

#ifdef ADD_VGA_CAM
static void vga_ldo_en(bool onoff)
{
	if(onoff == TRUE) { //power on 
	// Turn CAM_ISP_RAM_1.8V on
	Set_MAX8998_PM_OUTPUT_Voltage(LDO12, VCC_1p800);
	Set_MAX8998_PM_REG(ELDO12, 1);

	udelay(10);

	// Turn CAM_ISP_HOST_1.8V on
	Set_MAX8998_PM_OUTPUT_Voltage(LDO13, VCC_1p800);
	Set_MAX8998_PM_REG(ELDO13, 1);

	}
	
	else { // power off

	// Turn CAM_ISP_HOST_2.8V off
	Set_MAX8998_PM_REG(ELDO13, 0);

	udelay(10);
	
	// Turn CAM_ISP_RAM_1.8V off
	Set_MAX8998_PM_REG(ELDO12, 0);
		
	}

}
#endif

static int mc7_power_on(void)
{	
	int err;

	printk(KERN_DEBUG "mc7_power_on\n");

	/* 5M_CAM_nRST - GPC1(1) */
	err = gpio_request(GPIO_5M_CAM_nRST, "GPC1");

	if(err) {
		printk(KERN_ERR "failed to request GPC1 for camera control\n");

		return err;
	}
	
#ifdef ADD_VGA_CAM
	/* CAM_VGA_nSTBY - GPC1(0)  */
	err = gpio_request(GPIO_CAM_VGA_nSTBY, "GPC1(0)");

	if (err) {
		printk(KERN_ERR "failed to request GPB0 for camera control\n");

		return err;
	}

	/* CAM_VGA_nRST - GPC1(4) */
	err = gpio_request(GPIO_CAM_VGA_nRST, "GPC1(4)");

	if (err) {
		printk(KERN_ERR "failed to request GPB2 for camera control\n");

		return err;
	}
#endif

	mc7_ldo_en(TRUE);

	//mdelay(1);

#ifdef ADD_VGA_CAM
	vga_ldo_en(TRUE);

	mdelay(1);

	// CAM_VGA_nSTBY  HIGH		
	gpio_direction_output(GPIO_CAM_VGA_nSTBY, 0);
	gpio_set_value(GPIO_CAM_VGA_nSTBY, 1);
#endif

	// Mclk enable
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, S5PV210_GPE1_3_CAM_A_CLKOUT);

	mdelay(4); //spec 4ms

#ifdef ADD_VGA_CAM
	// CAM_VGA_nRST  HIGH		
	gpio_direction_output(GPIO_CAM_VGA_nRST, 0);
	gpio_set_value(GPIO_CAM_VGA_nRST, 1);	
	mdelay(4); //spec 4ms

	// CAM_VGA_nSTBY  LOW	
	gpio_direction_output(GPIO_CAM_VGA_nSTBY, 1);
	gpio_set_value(GPIO_CAM_VGA_nSTBY, 0);	
	mdelay(1);
#endif

	// 5M_CAM_nRST HIGH
	gpio_direction_output(GPIO_5M_CAM_nRST, 0);
	gpio_set_value(GPIO_5M_CAM_nRST, 1);
	mdelay(3); //spec 3ms

	gpio_free(GPIO_5M_CAM_nRST);
	
#ifdef ADD_VGA_CAM	
	gpio_free(GPIO_CAM_VGA_nSTBY);
	gpio_free(GPIO_CAM_VGA_nRST);
#endif

	return 0;
}


static int mc7_power_off(void)
{
	int err;
	
	printk(KERN_DEBUG "mc7_power_off\n");

	/* 5M_CAM_nRST - GPC1(1) */
	err = gpio_request(GPIO_5M_CAM_nRST, "GPC1");
	
	if(err) {
		printk(KERN_ERR "failed to request GPC1 for camera control\n");
	
		return err;
	}

#ifdef ADD_VGA_CAM	
	/* CAM_VGA_nRST - GPC1(4) */
	err = gpio_request(GPIO_CAM_VGA_nRST, "GPC1(4)");

	if (err) {
		printk(KERN_ERR "failed to request GPB2 for camera control\n");

		return err;
	}
#endif

       mdelay(3); //spec 3ms

	// CAM_MEGA_nRST - GPC1(1) LOW
	gpio_direction_output(GPIO_5M_CAM_nRST, 1);	
	gpio_set_value(GPIO_5M_CAM_nRST, 0);
	

#ifdef ADD_VGA_CAM	
	// CAM_VGA_nRST  LOW		
	gpio_direction_output(GPIO_CAM_VGA_nRST, 1);	
	gpio_set_value(GPIO_CAM_VGA_nRST, 0);

	mdelay(1);
#endif

	// Mclk disable
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, 0);

#ifdef ADD_VGA_CAM		
	vga_ldo_en(TRUE);
#endif

	mc7_ldo_en(FALSE);

	gpio_free(GPIO_5M_CAM_nRST);

#ifdef ADD_VGA_CAM	
	gpio_free(GPIO_CAM_VGA_nRST);
#endif

	return 0;
}


static int mc7_power_en(int onoff)
{
	if(onoff == 1) {
		mc7_power_on();
	}

	else {
		mc7_power_off();
	}

	return 0;
}

#endif

/**
 *  This function checks the status of the camera sensor by polling
 *  through the 'cmd' command register. 
 *
 *  'polling_interval' is the delay between successive polling events
 *
 *  The function returns if
 *  	o 'timeout' (in ms) expires
 *  	o the data read from device matches 'value'
 *
 *  On success it returns the time taken for successful wait.
 *  On failure, it returns -EBUSY.
 *  On I2C related failure, it returns -EIO.
 */
static int mc7_waitfor_int_timeout(struct i2c_client *client, unsigned char value, 
					int timeout, int polling_interval)
{
	int err, gpio_val;
	unsigned long jiffies_start = jiffies;
	unsigned long jiffies_timeout = jiffies_start + msecs_to_jiffies(timeout);

	unsigned char buf_read_interrupt[5] = { 0x05, 0x01, 0x00, 0x1C, 0x01 };
	unsigned char buf_read_data[2] = {0x00, 0x00};
	
	if(polling_interval < 0)
		polling_interval = POLL_TIME_MS;

	err = gpio_request(GPIO_5M_CAM_INT, "GPC1");

	if(err) {
		printk(KERN_ERR "failed to request GPC1 for camera control\n");

		return err;
	}

	gpio_direction_input(GPIO_5M_CAM_INT);

	
	while(time_before(jiffies, jiffies_timeout)){

		gpio_val = gpio_get_value(GPIO_5M_CAM_INT);

	       if(gpio_val)
		   	break;

		#if 0   
		err = mc7_i2c_read_multi(client, buf_read_interrupt, 5, buf_read_data, 2);
		if(err < 0)
			return -EIO;

		mc7_msg(&client->dev, "Status check returns %02x\n", buf_read_data[1]);

		if(buf_read_data[1] == value) 
			break;
		#endif

		msleep(polling_interval);
	}

	gpio_free(GPIO_5M_CAM_INT);

	mc7_msg(&client->dev, "GPIO_5M_CAM_INT value %0d\n", gpio_val);

	#if 0
	if(buf_read_data[1] != value)
	#endif	
	if(!gpio_val)
		return -EBUSY;
	else
		return jiffies_to_msecs(jiffies - jiffies_start);
}

static int mc7_get_mode(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	int err, mode;
	unsigned int val;

	printk("mc7_get_mode \n");
	err = mc7_read_category_parm(client, MC7_8BIT, 0x00, 0x0B, &val);
	if(err)
	{
		printk( "Can not read I2C command\n");
		return -EPERM;
	}

	mode = val;

	return mode;
}

static int mc7_set_mode_common(struct v4l2_subdev *sd, u32 mode )
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	int err = 0;

	mc7_write_category_parm(client, MC7_8BIT, 0x00, 0x0B, mode);

	if(mode != MC7_MONITOR_MODE)
	{
		err = mc7_i2c_verify(client, 0x00, 0x0B, mode);
		if(err)
		{
			printk( "Can not Set Sensor Mode\n");
			return err;
		}
	}

	return err;
}

static int mc7_set_mode(struct v4l2_subdev *sd, u32 mode )
{	
	int org_value;
	int err = -EINVAL;

	org_value = mc7_get_mode(sd);

	switch(org_value)
	{
		case MC7_PARMSET_MODE :
			if(mode == MC7_PARMSET_MODE)
			{
				return 0;
			}
			else if(mode == MC7_MONITOR_MODE)
			{
				if((err = mc7_set_mode_common(sd, MC7_MONITOR_MODE)))
					return err;
			}
			else if(mode == MC7_STILLCAP_MODE)
			{
				if((err = mc7_set_mode_common(sd, MC7_MONITOR_MODE)))
					return err;
				if((err = mc7_set_mode_common(sd, MC7_STILLCAP_MODE)))
					return err;
			}
			else
			{
				printk( "Requested Sensor Mode is incorrect!\n");
				return -EINVAL;
			}

			break;

		case MC7_MONITOR_MODE :
			if(mode == MC7_PARMSET_MODE)
			{
				if((err = mc7_set_mode_common(sd, MC7_PARMSET_MODE)))
					return err;
			}
			else if(mode == MC7_MONITOR_MODE)
			{
				return 0;
			}
			else if(mode == MC7_STILLCAP_MODE)
			{
				if((err = mc7_set_mode_common(sd, MC7_STILLCAP_MODE)))
					return err;
			}
			else
			{
				printk( "Requested Sensor Mode is incorrect!\n");
				return -EINVAL;
			}

			break;

		case MC7_STILLCAP_MODE :
			if(mode == MC7_PARMSET_MODE)
			{
				if((err = mc7_set_mode_common(sd, MC7_MONITOR_MODE)))
					return err;
				if((err = mc7_set_mode_common(sd, MC7_PARMSET_MODE)))
					return err;
			}
			else if(mode == MC7_MONITOR_MODE)
			{
				if((err = mc7_set_mode_common(sd, MC7_MONITOR_MODE)))
					return err;
			}
			else if(mode == MC7_STILLCAP_MODE)
			{
				return 0;
			}
			else
			{
				printk( "Requested Sensor Mode is incorrect!\n");
				return -EINVAL;
			}

			break;
	}
	
	return err;
}

static int mc7_get_batch_reflection_status(struct v4l2_subdev *sd)
{
	return 0;
}

static int mc7_read_fw_bin(const char *path, char *fwBin, int *fwSize)
{
	return 0;
}

static int mc7_get_main_sw_fw_version(struct v4l2_subdev *sd)
{
	return 0;
}

	
int mc7_read_loader_data(unsigned char *buffer, char *filename, int *size)
{
	unsigned int file_size = 0;

	struct file *filep = NULL;
	mm_segment_t old_fs;

	//printk("mc7_read_main_SW_fw_version is called...\n");

	filep = filp_open(filename, O_RDONLY, 0) ;

	if (filep && (filep!= 0xfffffffe))
	{
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		
		file_size = filep->f_op->llseek(filep, 0, SEEK_END);
		filep->f_op->llseek(filep, 0, SEEK_SET);
		
		filep->f_op->read(filep, buffer, file_size, &filep->f_pos);
		
		filp_close(filep, current->files);

		set_fs(old_fs);

		*size = file_size;

		printk("File size : %d\n", file_size);
	}
	else
	{
		return -EINVAL;
	}

	return 0;
}

static int mc7_transfer_loader(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
        struct mc7_state *state = to_state(sd);
	unsigned char buf_send_loader[8] = { 0x00, 0x04, 0x68, 0x07, 0xE0, 0x00, 0x20, 0x00 }; //size : 8192bytes (0x2000 bytes)
	unsigned int len_send_loader = 8;
	int err;
	
	unsigned char *loader = NULL;
	int loader_filesize=8192;

	printk(KERN_DEBUG, "mc7_transfer_loader\n");
	
	//loader_filesize = get_file_size(loader_file);
	//printk("filename = %s, filesize = %d\n", loader_file, &loader_filesize);

	if(loader_filesize >= 0){
		loader =  (char*)kmalloc(loader_filesize, GFP_KERNEL);;
		if(NULL == loader)
			return -1;
	} else {
		printk("Invalid input %d\n", loader_filesize);
		return -1;
	}

	if(mc7_read_loader_data(loader, MC7_LOADER_PATH, &loader_filesize)){
		printk(KERN_DEBUG, "Error reading loader file.\n");
		return -1;
	}

	err = mc7_write_mem(client, loader_filesize, 0x6807E000, loader);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: mc7_write_mem\n", __func__);
		kfree(loader);
		return -EIO;
	}

	state->runmode = MC7_RUNMODE_IDLE;

	kfree(loader);
	
	return 0;
}


static int mc7_transfer_writer(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
        struct mc7_state *state = to_state(sd);
	int err;
	
	unsigned char *loader = NULL;
	int loader_filesize=8192;

	printk(KERN_DEBUG, "mc7_transfer_writer\n");
	
	//loader_filesize = get_file_size(loader_file);
	//printk("filename = %s, filesize = %d\n", loader_file, &loader_filesize);

	if(loader_filesize >= 0){
		loader =  (char*)kmalloc(loader_filesize, GFP_KERNEL);;
		if(NULL == loader)
			return -1;
	} else {
		printk("Invalid input %d\n", loader_filesize);
		return -1;
	}

	if(mc7_read_loader_data(loader, MC7_WRITER_PATH, &loader_filesize)){
		printk(KERN_DEBUG, "Error reading loader file.\n");
		return -1;
	}

	err = mc7_write_mem(client, loader_filesize, 0x6807E000, loader);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: mc7_write_mem\n", __func__);
		kfree(loader);
		return -EIO;
	}

	state->runmode = MC7_RUNMODE_IDLE;

	kfree(loader);
	
	return 0;
}

static int mc7_get_version(struct v4l2_subdev *sd, int object_id, unsigned char version_info[])
{
	return 0;
}

static int mc7_get_fw_version(struct v4l2_subdev *sd)
{
	return 0;
}

static int mc7_get_dateinfo(struct v4l2_subdev *sd)
{
	return 0;
}

static int mc7_get_sensor_version(struct v4l2_subdev *sd)
{
	return 0;
}

static int mc7_get_sensor_maker_version(struct v4l2_subdev *sd)
{
	return 0;
}

static int mc7_get_af_version(struct v4l2_subdev *sd)
{
	return 0;
}

static int mc7_get_gamma_version(struct v4l2_subdev *sd)
{
	return 0;
}

#ifndef MDNIE_TUNING
static int mc7_update_fw(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_state *state = to_state(sd);
	
	int j, retry, ret;
	unsigned char *mbuf = NULL;
	int fw_size[1];
	int index = 0;
	int i = 0;
	int err;	
	char intr_value;

	mbuf = vmalloc(state->fw_info.size);

	if(NULL == mbuf){
		printk("mbuf == %d \n", mbuf);
		return -ENOMEM;
	}

	if (copy_from_user(mbuf, (void*)state->fw_info.addr, state->fw_info.size)){
		vfree(mbuf);
		printk("vfree(mbuf) == %d \n", mbuf);		
		return -EFAULT;
	}

	printk("state->fw_info.size = %d \n", state->fw_info.size);
	/** The firmware buffer is now copied to mbuf, so the firmware code is now in mbuf.
 	 *  We can use mbuf with i2c_tranfer call  */
	if(index > state->fw_info.size - 4){
		dev_err(&client->dev, "%s:Error size parameter\n", __func__);
	}
	memcpy(fw_size, mbuf, 4);

	dev_err(&client->dev, "%s: index = %d, [%d] fw_size = %d, mbuf = 0x%p\n", __func__, index, i, fw_size[0], mbuf[0]);

	mc7_power_en(1);

	#if 1
	err = mc7_waitfor_int_timeout(client, 0x01, 100, POLL_TIME_MS);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: mc7_waitfor_int_timeout\n", __func__);
		return -EIO;
	}
	else
	{
		//clear interrupt
		mc7_read_category_parm(client, MC7_8BIT, 0x00, 0x1C, &intr_value);
	       dev_err(&client->dev, "mc7_init intr_value :0x%02x\n", intr_value);

	}	
	#endif
	
	printk("Firmware Update Start!!\n");

	for(i = 0; i < 6; i++)
	{
		printk("Firmware Update is processing... %d!!\n", i);


		/* Send programmed firmware */
		if(fw_size[0] >= 0x10000)
			mc7_write_mem(client, 0x10000, 0x68000000+(i * 0x10000), ((unsigned char*)(mbuf) + (i * 0x10000) + 4));
		else
			mc7_write_mem(client, fw_size[0], 0x68000000+(i * 0x10000), ((unsigned char*)(mbuf) + (i * 0x10000) + 4));

		mdelay(10);

		fw_update_status += 15;

		fw_size[0] -= 0x10000;
		
	}

	printk("Firmware Update Success!!\n");

       //transfer writer
	err = mc7_transfer_writer(sd);
	if(err < 0){
		dev_err(&client->dev, "%s: Failed: transfer_writer\n", __func__);
		return -EIO;
	}

	fw_update_status += 15;
	
	//set start address of the writer
	err = mc7_write_category_parm(client, MC7_32BIT, 0x0F, 0x0C, 0x6807E000);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for buf_set_start_addr\n", __func__);
		return -EIO;
	}

	//start loader program
	err = mc7_write_category_parm(client, MC7_8BIT, 0x0F, 0x12, 0x01);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for buf_start_loader\n", __func__);
		return -EIO;
	}

	//wait interrupt (startup complete)
	//mdelay(1000);
	
	#if 1
	err = mc7_waitfor_int_timeout(client, 0x01, 1000, POLL_TIME_MS);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: mc7_waitfor_int_timeout\n", __func__);
		return -EIO;
	}
	else
	{
		//clear interrupt
		mc7_read_category_parm(client, MC7_8BIT, 0x00, 0x1C, &intr_value);
	       dev_err(&client->dev, "mc7_init intr_value :0x%02x\n", intr_value);

	}
	#endif
	
	fw_update_status = 100;
	if(camfw_update == 1)
		camfw_update = 2;

	vfree(mbuf);		

	msleep(100);
	mc7_power_en(0);
	msleep(100);     

	return 0;
}

#else
unsigned short *test[1];
EXPORT_SYMBOL(test);
extern void mDNIe_txtbuf_to_parsing(void);
extern void mDNIe_txtbuf_to_parsing_for_lightsensor(void);
extern void mDNIe_txtbuf_to_parsing_for_backlight(void);
static int mc7_update_fw(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct mc7_state *state = to_state(sd);
	unsigned char *mbuf = NULL;
	unsigned char *fw_buf[4];
	int fw_size[4];
	int index = 0;
	int i = 0;
	int err;

	const unsigned int packet_size = 129; //Data 128 + Checksum 1
	unsigned int packet_num, k, j, l = 0;
	unsigned char res = 0x00;
	unsigned char data[129];
	unsigned char data2[129];

	//dev_err(&client->dev, "%s: mc7_fw: buf = 0x%p, len = %d\n", __func__, (void*)state->fw_info.addr, state->fw_info.size);

	mbuf = vmalloc(state->fw_info.size);

	if(NULL == mbuf){
		return -ENOMEM;
	}

	if (copy_from_user(mbuf, (void*)state->fw_info.addr, state->fw_info.size)){
		vfree(mbuf);
		return -EFAULT;
	}

	/** The firmware buffer is now copied to mbuf, so the firmware code is now in mbuf.
 	 *  We can use mbuf with i2c_tranfer call  */
	for(i = 0; i < 4; i++){
		if(index > state->fw_info.size - 4){
			dev_err(&client->dev, "%s:Error size parameter\n", __func__);
			break;
		}
		memcpy(fw_size+i, mbuf + index, 4);
		index += 4;
		fw_buf[i] = mbuf + index;
		index += ((fw_size[i]-1) & (~0x3)) + 4;
		dev_err(&client->dev, "%s: [%d] fw_size = %d, fw_buf = 0x%p\n", __func__, i, fw_size[i], fw_buf[i]);
	}
	
	test[0] = fw_buf[0];
	
	for(j = 0; j < fw_size[0]; j++){
			printk("mc7_update_fw: , fw_size[0]: %d, test[0]: 0x%x\n", fw_size[0], test[0][j]);
			test[0][j] = ((test[0][j]&0xff00)>>8)|((test[0][j]&0x00ff)<<8);
			printk("mc7_update_fw: , test1[0]: 0x%x\n", test[0][j]);				
		}

	/*for mdnie tuning*/
	
	mDNIe_txtbuf_to_parsing();
	//mDNIe_txtbuf_to_parsing_for_lightsensor();
	//mDNIe_txtbuf_to_parsing_for_backlight();

	return 0;
}
#endif


static int mc7_dump_fw(struct v4l2_subdev *sd)
{	
	return 0;
}

static int mc7_check_dataline(struct v4l2_subdev *sd)
{
	return 0;
}

static int mc7_check_dataline_stop(struct v4l2_subdev *sd)
{
	return 0;
}

static int mc7_set_preview_size(struct v4l2_subdev *sd)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_state *state = to_state(sd);
	int index = state->framesize_index;

	mc7_msg(&client->dev, "%s: index = %d\n", __func__, index);

	switch(index){
		case MC7_PREVIEW_QCIF:
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x01, 0x05);
			break;
			
        	case MC7_PREVIEW_QVGA:
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x01, 0x09);
			break;

		case MC7_PREVIEW_592x480:
			break;

		case MC7_PREVIEW_VGA:
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x01, 0x17);
			break;
			
		case MC7_PREVIEW_WVGA:
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x01, 0x1D); //ned to change : 0x1D (854x480) => 800x480
			break;
			
		case MC7_PREVIEW_D1:
			break;
			
        	case MC7_PREVIEW_720P: //CHECK
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x01, 0x21);
			break;
			
		case MC7_PREVIEW_VERTICAL_QCIF:
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x01, 0x04);
			break;
			
		default:
			/* When running in image capture mode, the call comes here.
 		 	* Set the default video resolution - MC7_PREVIEW_VGA
 		 	*/ 
 		 	mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x01, 0x17);
			mc7_msg(&client->dev, "Setting preview resoution as VGA for image capture mode\n");
			break;
	}

	mc7_msg(&client->dev, "Done\n");	

	return err;	
}

static int mc7_set_frame_rate(struct v4l2_subdev *sd)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_state *state = to_state(sd);	
	
	switch(state->fps)
	{
		case FRAME_RATE_7:
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x02, 0x07);
		break;

		case FRAME_RATE_15:
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x02, 0x0F);
		break;

		case FRAME_RATE_30:
		default:
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x02, 0x1E);

		break;
	}

	mc7_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int mc7_set_anti_banding(struct v4l2_subdev *sd)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_state *state = to_state(sd);	

	switch(state->anti_banding)
	{
		case ANTI_BANDING_OFF:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x06, 0x04);
		break;

		case ANTI_BANDING_AUTO:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x06, 0x00);
		break;

		case ANTI_BANDING_50HZ:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x06, 0x01);
		break;

		case ANTI_BANDING_60HZ:
		default:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x06, 0x02);
		break;
	}

	mc7_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int mc7_set_preview_stop(struct v4l2_subdev *sd)
{
	return 0;
}


static int mc7_set_dzoom(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct mc7_state *state = to_state(sd);
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if(MC7_RUNMODE_RUNNING == state->runmode){
		err = mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x01, mc7_buf_set_dzoom[ctrl->value]);
		if(err < 0){
			dev_err(&client->dev, "%s: failed: i2c_write for set_dzoom\n", __func__);
			return -EIO;
		}

		//msleep(100);
	}

	DZoom_State = ctrl->value;

	mc7_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}


static int mc7_set_preview_start(struct v4l2_subdev *sd)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_state *state = to_state(sd);
	struct v4l2_control ctrl;

	unsigned char buf_enable_interrupt[5] = { 0x05, 0x02, 0x00, 0x10, 0x01 };
	unsigned int len_enable_interrupt = 5;
	
	unsigned char buf_monitor_mode[5] = { 0x05, 0x02, 0x00, 0x0B, 0x02 };
	unsigned int len_monitor_mode = 5;
	unsigned char intr_value;
	
	if( !state->pix.width || !state->pix.height || !state->fps){
		return -EINVAL;
	}

       //change monitor size to VGA : temp code
       #if 0
       err = mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x01, 0x17);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for buf_monitor_mode\n", __func__);
		return -EIO;
	}
	#endif
	err = mc7_set_preview_size(sd);
	if(err < 0){
		 dev_err(&client->dev, "%s: failed: Could not set preview size\n", __func__);
	        return -EIO;
	}
	
       //enable interrupt signal (start YUV)
       //err = mc7_i2c_write_multi(client, buf_enable_interrupt, len_enable_interrupt);
       err = mc7_write_category_parm(client, MC7_8BIT, 0x00, 0x10, 0x01);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for buf_monitor_mode\n", __func__);
		return -EIO;
	}
	
       //change to monitor mode (viewfinder)
       //err = mc7_i2c_write_multi(client, buf_monitor_mode, len_monitor_mode);			 
       err = mc7_write_category_parm(client, MC7_8BIT, 0x00, 0x0B, 0x02);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for buf_monitor_mode\n", __func__);
		return -EIO;
	}

	//clear interrupt
	err = mc7_waitfor_int_timeout(client, 0x01, 1000, POLL_TIME_MS);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: mc7_waitfor_int_timeout\n", __func__);
		return -EIO;
	}
	else
	{
		//clear interrupt
		mc7_read_category_parm(client, MC7_8BIT, 0x00, 0x1C, &intr_value);
	       dev_err(&client->dev, "mc7_init intr_value :0x%02x\n", intr_value);

	}
	
	state->runmode = MC7_RUNMODE_RUNNING;

	return 0;
}

static int mc7_set_capture_size(struct v4l2_subdev *sd)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_state *state = to_state(sd);

	int index = state->framesize_index;
	unsigned char mc7_capture_size = 0x1F; /* Default 5 MP */

	switch(index){
	case MC7_CAPTURE_VGA: /* 640x480 */
		mc7_capture_size = 0x09;
		break;
	case MC7_CAPTURE_WVGA: /* 800x480 */
		//mc7_capture_size = 0x0A;
		break;
	case MC7_CAPTURE_W1MP: /* 1600x960 */
		//mc7_capture_size = 0x1F;
		break;		
	case MC7_CAPTURE_2MP: /* 1600x1200 */
		mc7_capture_size = 0x17;
		break;
	case MC7_CAPTURE_W2MP: /* 2048x1232 */
		//mc7_capture_size = 0x1F;
		break;
	case MC7_CAPTURE_3MP: /* 2048x1536 */
		mc7_capture_size = 0x1B;
		break;
	case MC7_CAPTURE_W4MP: /* 2560x1536 */
		//mc7_capture_size = 0x1F;
		break;
	case MC7_CAPTURE_5MP: /* 2560x1920 */
		mc7_capture_size = 0x1F;
		break;
	default:
		/* The framesize index was not set properly. 
 		 * Check s_fmt call - it must be for video mode. */
		return -EINVAL;
	}

	/* Set capture image size */
	err = mc7_write_category_parm(client, MC7_8BIT, 0x0B, 0x01, mc7_capture_size);	
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for capture_resolution\n", __func__);
		return -EIO; 
	}
#if 0
	//This is for postview
	if(mc7_capture_size < 0x09)
	{
		state->preview_size = MC7_PREVIEW_VGA; 
		//printk("[5B] mc7_set_capture_size: preview_size is VGA (%d)\n", state->preview_size);
	}
	else
	{
		state->preview_size = MC7_PREVIEW_WVGA; 
		//printk("[5B] mc7_set_capture_size: preview_size is WVGA (%d)\n", state->preview_size);		
	}
#endif
	//printk("mc7_set_capture_size: 0x%02x\n", index);

	return 0;	
}

static int mc7_set_ae_awb(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

/**
 *  lock:	1 to lock, 0 to unlock
 */
static int mc7_set_awb_lock(struct v4l2_subdev *sd, int lock)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if(lock)
	{
		/* AE/AWB Lock */
		mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x00, 0x01); 
		mc7_write_category_parm(client, MC7_8BIT, 0x06, 0x00, 0x01); 	
	}
	else
	{
		/* AE/AWB Unlock */
		mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x00, 0x00); 
		mc7_write_category_parm(client, MC7_8BIT, 0x06, 0x00, 0x00); 	
	}

	mc7_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int mc7_set_capture_cmd(struct v4l2_subdev *sd)
{
	return 0;
}

static int mc7_set_capture_exif(struct v4l2_subdev *sd)
{
	return 0;
}

static int mc7_set_jpeg_quality(struct v4l2_subdev *sd)
{
	return 0;
}

static int mc7_set_jpeg_config(struct v4l2_subdev *sd)
{
	return 0;
}

static int mc7_set_capture_config(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int mc7_set_capture_start(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_state *state = to_state(sd);	
	unsigned char intr_value;
	
	mc7_msg(&client->dev, "%s\n", __func__);
	
	//Set main image format : YUV422
	mc7_write_category_parm(client, MC7_8BIT, 0x0B, 0x00, 0x00); 

	mc7_set_capture_size(sd);

	//Enable interrupt signal
	mc7_write_category_parm(client, MC7_8BIT, 0x00, 0x10, 0x88); 
	
	//Start single capture
	mc7_write_category_parm(client, MC7_8BIT, 0x00, 0x0B, 0x03); 

	//wait and clear interrupt
	err = mc7_waitfor_int_timeout(client, 0x01, 1000, POLL_TIME_MS);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: mc7_waitfor_int_timeout\n", __func__);
		return -EIO;
	}
	else
	{
		//clear interrupt
		mc7_read_category_parm(client, MC7_8BIT, 0x00, 0x1C, &intr_value);
	       dev_err(&client->dev, "mc7_init intr_value :0x%02x\n", intr_value);
	}	

	//Start data output
	mc7_write_category_parm(client, MC7_8BIT, 0x0C, 0x09, 0x01); 

	//wait for frame data
	msleep(1000);
	
	//Stop data output
	mc7_write_category_parm(client, MC7_8BIT, 0x0C, 0x09, 0x02); 

	return 0;
}

static int mc7_get_focus_mode(struct i2c_client *client, unsigned char cmd, unsigned char *value)
{
	return 0;
}

static int mc7_set_af_softlanding(struct v4l2_subdev *sd)
{
	return 0;
}

static int mc7_set_flash(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int mc7_set_effect_gamma(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int rval, orgmode;
	u32 readval = 0;

	/* Read Color Effect ON/OFF */
	mc7_read_category_parm(client, MC7_8BIT, 0x02, 0x0B, &readval);
	/* If Color Effect is on, turn it off */
	if(readval)
		mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0B, 0x00); 
	/* Check Original Mode */
	orgmode =mc7_get_mode(sd);
	/* Set Parameter Mode */
	rval = mc7_set_mode(sd, MC7_PARMSET_MODE);
	if(rval)
	{
		dev_err(&client->dev, "%s : Can not set operation mode\n", __func__);
		return rval;
	}
	
	switch(value)
	{
		case IMAGE_EFFECT_NEGATIVE:
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x0B, 0x01);
			break;
#if 0
		case MC7_EFFECT_SOLARIZATION1:
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x0B, 0x02);
			break;
		case MC7_EFFECT_SOLARIZATION2:
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x0B, 0x03);
			break;

		case MC7_EFFECT_SOLARIZATION3:
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x0B, 0x04);
			break;

		case MC7_EFFECT_SOLARIZATION4:
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x0B, 0x05);
			break;
		case MC7_EFFECT_EMBOSS:
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x0B, 0x06);
			break;
		case MC7_EFFECT_OUTLINE:
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x0B, 0x07);
			break;
#endif			
		case IMAGE_EFFECT_AQUA:
			mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x0B, 0x08);
			break;
		default:
			break;	
	}

	/* Set Monitor Mode */
	if(orgmode == MC7_MONITOR_MODE)
	{
		rval = mc7_set_mode(sd, MC7_MONITOR_MODE);
		if(rval)
		{
			dev_err(&client->dev, "%s : Can not start preview\n", __func__);
			return rval;
		}
	}

	return 0;
}

static int mc7_set_effect_color(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	int rval, orgmode;
	u32 readval = 0;

	/* Read Gamma Effect ON/OFF */
	mc7_read_category_parm(client, MC7_8BIT, 0x01, 0x0B, &readval);
	/* If Gamma Effect is on, turn it off */
	if(readval)
	{
		orgmode = mc7_get_mode(sd);
		rval = mc7_set_mode(sd, MC7_PARMSET_MODE);
		if(rval)
		{
			dev_err(&client->dev, "%s : Can not set operation mode\n", __func__);
			return rval;
		}
		mc7_write_category_parm(client, MC7_8BIT, 0x01, 0x0B, 0x00); 

		if(orgmode == MC7_MONITOR_MODE)
		{
			rval = mc7_set_mode(sd, MC7_MONITOR_MODE);
			if(rval)
			{
				dev_err(&client->dev, "%s : Can not start preview\n", __func__);
				return rval;
			}
		}
	}

	switch(value)
	{
		case IMAGE_EFFECT_NONE:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0B, 0x00);
			break;
		case IMAGE_EFFECT_SEPIA:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0B, 0x01); 
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x09, 0xD8);
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0A, 0x18);
			break;
		case IMAGE_EFFECT_BNW:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0B, 0x01); 
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x09, 0x00);
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0A, 0x00);
			break;
#if 0			
		case MC7_EFFECT_RED:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0B, 0x01); 
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x09, 0x00);
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0A, 0x6B);
			break;
		case MC7_EFFECT_GREEN:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0B, 0x01); 
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x09, 0xE0);
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0A, 0xE0);
			break;
		case MC7_EFFECT_BLUE:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0B, 0x01); 
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x09, 0x40);
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0A, 0x00);
			break;
		case MC7_EFFECT_PINK:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0B, 0x01); 
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x09, 0x20);
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0A, 0x40);
			break;
		case MC7_EFFECT_YELLOW:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0B, 0x01); 
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x09, 0x80);
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0A, 0x00);
			break;
		case MC7_EFFECT_PURPLE:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0B, 0x01); 
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x09, 0x50);
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0A, 0x20);
			break;
#endif			
		case IMAGE_EFFECT_ANTIQUE:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0B, 0x01); 
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x09, 0xD0);
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0A, 0x30);
			break;	
		default:
			break;
	}
		
	return 0;
}

static int mc7_set_effect(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value)
	{
		case IMAGE_EFFECT_NONE:
		case IMAGE_EFFECT_BNW:
		case IMAGE_EFFECT_SEPIA:
		case IMAGE_EFFECT_ANTIQUE:
		case IMAGE_EFFECT_SHARPEN:
			err = mc7_set_effect_color(sd, ctrl->value);
		break;
		
		case IMAGE_EFFECT_AQUA:
		case IMAGE_EFFECT_NEGATIVE:
			err = mc7_set_effect_gamma(sd, ctrl->value);
		break;
		
		default:
			dev_err(&client->dev, "%s : [Effect]Invalid value is ordered!!!\n", __func__);
		break;
	}

	mc7_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int mc7_set_saturation(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value)
	{
		case SATURATION_MINUS_2:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0F, 0x01);
		break;

		case SATURATION_MINUS_1:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0F, 0x02);
		break;

		case SATURATION_DEFAULT:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0F, 0x03);
		break;

		case SATURATION_PLUS_1:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0F, 0x04);
		break;

		case SATURATION_PLUS_2:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x0F, 0x05);
		break;
			dev_err(&client->dev, "%s: failed: to set_saturation, enum: %d\n", __func__, ctrl->value);	
		default:

		break;
	}

	mc7_msg(&client->dev, "%s: done, saturation: %d\n", __func__, ctrl->value);

	return 0;
}

static int mc7_set_contrast(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value)
	{
		case CONTRAST_MINUS_2:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x25, 0x03);
		break;

		case CONTRAST_MINUS_1:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x25, 0x04);
		break;

		case CONTRAST_DEFAULT:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x25, 0x05);
		break;

		case CONTRAST_PLUS_1:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x25, 0x06);
		break;

		case CONTRAST_PLUS_2:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x25, 0x07);
		break;

		default:
			dev_err(&client->dev, "%s: failed: to set_contrast, enum: %d\n", __func__, ctrl->value);	
		break;
	}

	mc7_msg(&client->dev, "%s: done, contrast: %d\n", __func__, ctrl->value);

	return 0;
}

static int mc7_set_sharpness(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value)
	{
		case SHARPNESS_MINUS_2:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x11, 0x03);
		break;

		case SHARPNESS_MINUS_1:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x11, 0x04);
		break;

		case SHARPNESS_DEFAULT:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x11, 0x05);
		break;

		case SHARPNESS_PLUS_1:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x11, 0x06);
		break;

		case SHARPNESS_PLUS_2:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x11, 0x07);
		break;

		default:
			dev_err(&client->dev, "%s: failed: to set_sharpness, enum: %d\n", __func__, ctrl->value);			
		break;
	}

	mc7_msg(&client->dev, "%s: done, sharpness: %d\n", __func__, ctrl->value);

	return 0;
}

static int mc7_set_wdr(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	switch(ctrl->value)
	{
		case WDR_ON:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x25, 0x0A);
			mc7_write_category_parm(client, MC7_8BIT, 0x0B, 0x2C, 0x01);
		break;
		
		case WDR_OFF:			
		default:
			mc7_write_category_parm(client, MC7_8BIT, 0x02, 0x25, 0x05);
			mc7_write_category_parm(client, MC7_8BIT, 0x0B, 0x2C, 0x00);
		break;
	}

	mc7_msg(&client->dev, "%s: done, wdr: %d\n", __func__, ctrl->value);

	return 0;
}

static int mc7_set_anti_shake(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int mc7_set_continous_af(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int mc7_set_object_tracking(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int mc7_get_object_tracking(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int mc7_set_face_detection(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int mc7_set_smart_auto(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int mc7_get_smart_auto_status(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int mc7_set_touch_auto_focus(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int mc7_set_focus_mode(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int mc7_set_vintage_mode(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int mc7_set_face_beauty(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}


static int mc7_set_white_balance(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch(ctrl->value)
	{
		case WHITE_BALANCE_AUTO:
			mc7_write_category_parm(client, MC7_8BIT, 0x06, 0x02, 0x01);
		break;

		case WHITE_BALANCE_SUNNY:
			mc7_write_category_parm(client, MC7_8BIT, 0x06, 0x02, 0x02);
			mc7_write_category_parm(client, MC7_8BIT, 0x06, 0x03, 0x04);
		break;

		case WHITE_BALANCE_CLOUDY:
			mc7_write_category_parm(client, MC7_8BIT, 0x06, 0x02, 0x02);
			mc7_write_category_parm(client, MC7_8BIT, 0x06, 0x03, 0x05);
		break;

		case WHITE_BALANCE_TUNGSTEN:
			mc7_write_category_parm(client, MC7_8BIT, 0x06, 0x02, 0x02);
			mc7_write_category_parm(client, MC7_8BIT, 0x06, 0x03, 0x01);//incandescent
		break;

		case WHITE_BALANCE_FLUORESCENT:
			mc7_write_category_parm(client, MC7_8BIT, 0x06, 0x02, 0x02);
			mc7_write_category_parm(client, MC7_8BIT, 0x06, 0x03, 0x02);
		break;

		default:
			dev_err(&client->dev, "%s: failed: to set_white_balance, enum: %d\n", __func__, ctrl->value);
			return -EINVAL;
		break;
	}

	mc7_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int mc7_set_ev(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_state *state = to_state(sd);

	switch(ctrl->value)
	{
		case EV_MINUS_4:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x09, 0x13);
		break;

		case EV_MINUS_3:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x09, 0x16);
		break;

		case EV_MINUS_2:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x09, 0x19);
		break;

		case EV_MINUS_1:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x09, 0x1B);
		break;

		case EV_DEFAULT:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x09, 0x1E);
		break;

		case EV_PLUS_1:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x09, 0x21);
		break;

		case EV_PLUS_2:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x09, 0x24);
		break;
		
		case EV_PLUS_3:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x09, 0x27);
		break;

		case EV_PLUS_4:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x09, 0x29);
		break;			

		default:
			dev_err(&client->dev, "%s: failed: to set_ev, enum: %d\n", __func__, ctrl->value);
			return -EINVAL;
		break;
	}

	mc7_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int mc7_set_metering(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_state *state = to_state(sd);

	switch(ctrl->value)
	{
		case METERING_MATRIX:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x01, 0x01);
		break;

		case METERING_CENTER:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x01, 0x02);
		break;

		case METERING_SPOT:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x01, 0x05);
		break;

		default:
			dev_err(&client->dev, "%s: failed: to set_photometry, enum: %d\n", __func__, ctrl->value);
			return -EINVAL;
		break;
	}

	mc7_msg(&client->dev, "%s: done\n", __func__);

	return 0;
}

static int mc7_set_iso(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int err;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	mc7_msg(&client->dev, "%s: Enter : iso %d\n", __func__, ctrl->value);
	
	switch(ctrl->value)
	{
		case ISO_AUTO:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x05, 0x00);
		break;

		case ISO_50:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x05, 0x01);
		break;

		case ISO_100:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x05, 0x02);
		break;

		case ISO_200:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x05, 0x03);
		break;

		case ISO_400:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x05, 0x04);
		break;

		case ISO_800:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x05, 0x05);
		break;

		case ISO_1600:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x05, 0x06);
		break;
#if 0
		case ISO_3200:
			mc7_write_category_parm(client, MC7_8BIT, 0x03, 0x05, 0x06);
		break;
#endif		
		/* This is additional setting for Sports' scene mode */
		case ISO_SPORTS:
		break;
		
		/* This is additional setting for 'Night' scene mode */
		case ISO_NIGHT:
		break;

		/* This is additional setting for video recording mode */
		case ISO_MOVIE:
		break;

		default:
			dev_err(&client->dev, "%s: failed: to set_iso, enum: %d\n", __func__, ctrl->value);
			return -EINVAL;
		break;
	}

	mc7_msg(&client->dev, "%s: done, iso: %d\n", __func__, ctrl->value);

	return 0;
}

static int mc7_set_gamma(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}
 
static int mc7_set_face_lock(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int mc7_set_auto_focus(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int mc7_get_auto_focus_status(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static void mc7_init_parameters(struct v4l2_subdev *sd)
{
	struct mc7_state *state = to_state(sd);

	/* Set initial values for the sensor stream parameters */
	state->strm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	state->strm.parm.capture.timeperframe.numerator = 1;
	state->strm.parm.capture.capturemode = 0;

//	state->framesize_index = MC7_PREVIEW_VGA;
	state->fps = 30; /* Default value */
	
	state->jpeg.enable = 0;
	state->jpeg.quality = 100;
	state->jpeg.main_offset = 0;
	state->jpeg.main_size = 0;
	state->jpeg.thumb_offset = 0;
	state->jpeg.thumb_size = 0;
	state->jpeg.postview_offset = 0;
}

static int mc7_get_fw_data(struct v4l2_subdev *sd)
{
	return 0;
}

//s1_camera [ Defense process by ESD input ] _[
static int mc7_reset(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;

	dev_err(&client->dev, "%s: Enter \n", __func__);

	err = mc7_power_en(0);
	if(err < 0){	
		dev_err(&client->dev, "%s: failed: mc7_power_en(off)\n", __func__);
		return -EIO;
    	}

	mdelay(5);
	
	err = mc7_power_en(1);
	if(err < 0){	
		dev_err(&client->dev, "%s: failed: mc7_power_en(off)\n", __func__);
		return -EIO;
    	}
	
	//err = mc7_init(sd, 0);
	if(err < 0){
		dev_err(&client->dev, "%s: Failed: Camera Initialization\n", __func__);
		return -EIO;
	}

	return 0;
}
// _]

#if 0
/* Sample code */
static const char *mc7_querymenu_wb_preset[] = {
	"WB Tungsten", "WB Fluorescent", "WB sunny", "WB cloudy", NULL
};
#endif

static struct v4l2_queryctrl mc7_controls[] = {
#if 0
	/* Sample code */
	{
		.id = V4L2_CID_WHITE_BALANCE_PRESET,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "White balance preset",
		.minimum = 0,
		.maximum = ARRAY_SIZE(mc7_querymenu_wb_preset) - 2,
		.step = 1,
		.default_value = 0,
	},
#endif
};

const char **mc7_ctrl_get_menu(u32 id)
{
	switch (id) {
#if 0
	/* Sample code */
	case V4L2_CID_WHITE_BALANCE_PRESET:
		return mc7_querymenu_wb_preset;
#endif
	default:
		return v4l2_ctrl_get_menu(id);
	}
}

static inline struct v4l2_queryctrl const *mc7_find_qctrl(int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mc7_controls); i++)
		if (mc7_controls[i].id == id)
			return &mc7_controls[i];

	return NULL;
}

static int mc7_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mc7_controls); i++) {
		if (mc7_controls[i].id == qc->id) {
			memcpy(qc, &mc7_controls[i], sizeof(struct v4l2_queryctrl));
			return 0;
		}
	}

	return -EINVAL;
}

static int mc7_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm)
{
	struct v4l2_queryctrl qctrl;

	qctrl.id = qm->id;
	mc7_queryctrl(sd, &qctrl);

	return v4l2_ctrl_query_menu(qm, &qctrl, mc7_ctrl_get_menu(qm->id));
}

/*
 * Clock configuration
 * Configure expected MCLK from host and return EINVAL if not supported clock
 * frequency is expected
 * 	freq : in Hz
 * 	flag : not supported for now
 */
static int mc7_s_crystal_freq(struct v4l2_subdev *sd, u32 freq, u32 flags)
{
	int err = -EINVAL;

	return err;
}

static int mc7_g_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;

	return err;
}

static int mc7_get_framesize_index(struct v4l2_subdev *sd);
static int mc7_set_framesize_index(struct v4l2_subdev *sd, unsigned int index);
/* Information received: 
 * width, height
 * pixel_format -> to be handled in the upper layer 
 *
 * */
static int mc7_s_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;
	struct mc7_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int framesize_index = -1;

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

	if(fmt->fmt.pix.colorspace == V4L2_COLORSPACE_JPEG)
		state->oprmode = MC7_OPRMODE_IMAGE;
	else
		state->oprmode = MC7_OPRMODE_VIDEO; 


	framesize_index = mc7_get_framesize_index(sd);

	mc7_msg(&client->dev, "%s:framesize_index = %d\n", __func__, framesize_index);
	
	err = mc7_set_framesize_index(sd, framesize_index);
	if(err < 0){
		dev_err(&client->dev, "%s: set_framesize_index failed\n", __func__);
		return -EINVAL;
	}

	if(state->pix.pixelformat == V4L2_PIX_FMT_JPEG){
		state->jpeg.enable = 1;
	} else {
		state->jpeg.enable = 0;
	}

	if(state->oprmode == MC7_OPRMODE_VIDEO)
	{
		if(framesize_index == MC7_PREVIEW_720P)
		{
			state->hd_preview_on = 1;
		}
		else
		{
			state->hd_preview_on = 0;
		}
	}
	return 0;
}

static int mc7_enum_framesizes(struct v4l2_subdev *sd, \
					struct v4l2_frmsizeenum *fsize)
{
	struct mc7_state *state = to_state(sd);
	int num_entries = sizeof(mc7_framesize_list)/sizeof(struct mc7_enum_framesize);	
	struct mc7_enum_framesize *elem;	
	int index = 0;
	int i = 0;

	/* The camera interface should read this value, this is the resolution
 	 * at which the sensor would provide framedata to the camera i/f
 	 *
 	 * In case of image capture, this returns the default camera resolution (VGA)
 	 */
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;

	if(state->pix.pixelformat == V4L2_PIX_FMT_JPEG){
		index = MC7_PREVIEW_VGA;
	} else {
		index = state->framesize_index;
	}

	for(i = 0; i < num_entries; i++){
		elem = &mc7_framesize_list[i];
		if(elem->index == index){
			fsize->discrete.width = mc7_framesize_list[index].width;
			fsize->discrete.height = mc7_framesize_list[index].height;
			return 0;
		}
	}

	return -EINVAL;
}

static int mc7_enum_frameintervals(struct v4l2_subdev *sd, 
					struct v4l2_frmivalenum *fival)
{
	int err = 0;

	return err;
}

static int mc7_enum_fmt(struct v4l2_subdev *sd, struct v4l2_fmtdesc *fmtdesc)
{
	int num_entries;

	num_entries = sizeof(capture_fmts)/sizeof(struct v4l2_fmtdesc);

	if(fmtdesc->index >= num_entries)
		return -EINVAL;

        memset(fmtdesc, 0, sizeof(*fmtdesc));
        memcpy(fmtdesc, &capture_fmts[fmtdesc->index], sizeof(*fmtdesc));

	return 0;
}

static int mc7_try_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
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

/** Gets current FPS value */
static int mc7_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct mc7_state *state = to_state(sd);
	int err = 0;

	state->strm.parm.capture.timeperframe.numerator = 1;
	state->strm.parm.capture.timeperframe.denominator = state->fps;

	memcpy(param, &state->strm, sizeof(param));

	return err;
}

/** Sets the FPS value */
static int mc7_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_state *state = to_state(sd);

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
 * It returns the index (enum mc7_frame_size) of the framesize entry.
 */
static int mc7_get_framesize_index(struct v4l2_subdev *sd)
{
	int i = 0;
	struct mc7_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_enum_framesize *frmsize;

	mc7_msg(&client->dev, "%s: Requested Res: %dx%d\n", __func__, state->pix.width, state->pix.height);

	/* Check for video/image mode */
	for(i = 0; i < (sizeof(mc7_framesize_list)/sizeof(struct mc7_enum_framesize)); i++)
	{
		frmsize = &mc7_framesize_list[i];

		if(frmsize->mode != state->oprmode)
			continue;

		if(state->oprmode == MC7_OPRMODE_IMAGE)
		{
			/* In case of image capture mode, if the given image resolution is not supported,
 			 * return the next higher image resolution. */
 			//printk("frmsize->width(%d) state->pix.width(%d) frmsize->height(%d) state->pix.height(%d)\n", frmsize->width, state->pix.width, frmsize->height, state->pix.height);
			if(frmsize->width == state->pix.width && frmsize->height == state->pix.height)
			{
				//printk("frmsize->index(%d) \n", frmsize->index);
				return frmsize->index;
			}
		}
		else 
		{
			/* In case of video mode, if the given video resolution is not matching, use
 			 * the default rate (currently MC7_PREVIEW_VGA).
 			 */		 
 			//printk("frmsize->width(%d) state->pix.width(%d) frmsize->height(%d) state->pix.height(%d)\n", frmsize->width, state->pix.width, frmsize->height, state->pix.height);
			if(frmsize->width == state->pix.width && frmsize->height == state->pix.height)
			{
				//printk("frmsize->index(%d) \n", frmsize->index);
				return frmsize->index;
		}
	} 
	} 
	/* If it fails, return the default value. */
	return (state->oprmode == MC7_OPRMODE_IMAGE) ? MC7_CAPTURE_3MP : MC7_PREVIEW_VGA;
}


/* This function is called from the s_ctrl api
 * Given the index, it checks if it is a valid index.
 * On success, it returns 0.
 * On Failure, it returns -EINVAL
 */
static int mc7_set_framesize_index(struct v4l2_subdev *sd, unsigned int index)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_state *state = to_state(sd);
	int i = 0;

	for(i = 0; i < (sizeof(mc7_framesize_list)/sizeof(struct mc7_enum_framesize)); i++)
	{
		if(mc7_framesize_list[i].index == index && mc7_framesize_list[i].mode == state->oprmode){
			state->framesize_index = mc7_framesize_list[i].index;	
			state->pix.width = mc7_framesize_list[i].width;
			state->pix.height = mc7_framesize_list[i].height;
			mc7_info(&client->dev, "%s: Camera Res: %dx%d\n", __func__, state->pix.width, state->pix.height);
			return 0;
		} 
	} 
	
	return -EINVAL;
}

static int mc7_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_state *state = to_state(sd);
	struct mc7_userset userset = state->userset;
	int err = -ENOIOCTLCMD;

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
		ctrl->value = userset.sharpness;
		err = 0;
		break;

	case V4L2_CID_CAM_JPEG_MAIN_SIZE:
		ctrl->value = state->jpeg.main_size;
		err = 0;
		break;
	
	case V4L2_CID_CAM_JPEG_MAIN_OFFSET:
		ctrl->value = state->jpeg.main_offset;
		err = 0;
		break;

	case V4L2_CID_CAM_JPEG_THUMB_SIZE:
		ctrl->value = state->jpeg.thumb_size;
		err = 0;
		break;
	
	case V4L2_CID_CAM_JPEG_THUMB_OFFSET:
		ctrl->value = state->jpeg.thumb_offset;
		err = 0;
		break;

	case V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET:
		ctrl->value = state->jpeg.postview_offset;
		err = 0;
		break; 
	
	case V4L2_CID_CAM_JPEG_MEMSIZE:
		ctrl->value = SENSOR_JPEG_SNAPSHOT_MEMSIZE;
		err = 0;
		break;

	//need to be modified
	case V4L2_CID_CAM_JPEG_QUALITY:
		ctrl->value = state->jpeg.quality;
		err = 0;
		break;

	case V4L2_CID_CAMERA_OBJ_TRACKING_STATUS:
		err = mc7_get_object_tracking(sd, ctrl);
		ctrl->value = state->ot_status;
		break;

	case V4L2_CID_CAMERA_SMART_AUTO_STATUS:
		err = mc7_get_smart_auto_status(sd, ctrl);
		ctrl->value = state->sa_status;
		break;

	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
		err = mc7_get_auto_focus_status(sd, ctrl);
		break;

	case V4L2_CID_CAM_DATE_INFO_YEAR:
		ctrl->value = state->dateinfo.year;
		err = 0;
		break; 

	case V4L2_CID_CAM_DATE_INFO_MONTH:
		ctrl->value = state->dateinfo.month;
		err = 0;
		break; 

	case V4L2_CID_CAM_DATE_INFO_DATE:
		ctrl->value = state->dateinfo.date;
		err = 0;
		break; 

	case V4L2_CID_CAM_SENSOR_VER:
		ctrl->value = state->sensor_version;
		err = 0;
		break; 

	case V4L2_CID_CAM_FW_MINOR_VER:
		ctrl->value = state->fw.minor;
		err = 0;
		break; 

	case V4L2_CID_CAM_FW_MAJOR_VER:
		ctrl->value = state->fw.major;
		err = 0;
		break; 

	case V4L2_CID_CAM_PRM_MINOR_VER:
		ctrl->value = state->prm.minor;
		err = 0;
		break; 

	case V4L2_CID_CAM_PRM_MAJOR_VER:
		ctrl->value = state->prm.major;
		err = 0;
		break; 

	case V4L2_CID_CAM_SENSOR_MAKER:
		ctrl->value = state->sensor_info.maker;
		err = 0;
		break; 

	case V4L2_CID_CAM_SENSOR_OPTICAL:
		ctrl->value = state->sensor_info.optical;
		err = 0;
		break; 		

	case V4L2_CID_CAM_AF_VER_LOW:
		ctrl->value = state->af_info.low;
		err = 0;
		break; 

	case V4L2_CID_CAM_AF_VER_HIGH:
		ctrl->value = state->af_info.high;
		err = 0;
		break; 	

	case V4L2_CID_CAM_GAMMA_RG_LOW:
		ctrl->value = state->gamma.rg_low;
		err = 0;
		break; 

	case V4L2_CID_CAM_GAMMA_RG_HIGH:
		ctrl->value = state->gamma.rg_high;
		err = 0;
		break; 		

	case V4L2_CID_CAM_GAMMA_BG_LOW:
		ctrl->value = state->gamma.bg_low;
		err = 0;
		break; 

	case V4L2_CID_CAM_GAMMA_BG_HIGH:
		ctrl->value = state->gamma.bg_high;
		err = 0;
		break; 	
	
	case V4L2_CID_CAM_GET_DUMP_SIZE:
		ctrl->value = state->fw_dump_size;
		err = 0;
		break;		
	
	case V4L2_CID_MAIN_SW_DATE_INFO_YEAR:
		ctrl->value = state->main_sw_dateinfo.year;
		err = 0;
		break; 

	case V4L2_CID_MAIN_SW_DATE_INFO_MONTH:
		ctrl->value = state->main_sw_dateinfo.month;
		err = 0;
		break; 

	case V4L2_CID_MAIN_SW_DATE_INFO_DATE:
		ctrl->value = state->main_sw_dateinfo.date;
		err = 0;
		break; 

	case V4L2_CID_MAIN_SW_FW_MINOR_VER:
		ctrl->value = state->main_sw_fw.minor;
		err = 0;
		break; 

	case V4L2_CID_MAIN_SW_FW_MAJOR_VER:
		ctrl->value = state->main_sw_fw.major;
		err = 0;
		break; 

	case V4L2_CID_MAIN_SW_PRM_MINOR_VER:
		ctrl->value = state->main_sw_prm.minor;
		err = 0;
		break; 

	case V4L2_CID_MAIN_SW_PRM_MAJOR_VER:
		ctrl->value = state->main_sw_prm.major;
		err = 0;
		break; 

	default:
		dev_err(&client->dev, "%s: no such ctrl\n", __func__);
		break;
	}
	
	return err;
}

static int mc7_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_state *state = to_state(sd);
	int err = -ENOIOCTLCMD;
	int offset = 134217728;

	switch (ctrl->id) {
		
#if 1
	case V4L2_CID_CAMERA_AEAWB_LOCK_UNLOCK:
		err = mc7_set_ae_awb(sd, ctrl);
		break;
		
	case V4L2_CID_CAMERA_FLASH_MODE:
		err = mc7_set_flash(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_BRIGHTNESS:
		//dev_err(&client->dev, "%s: V4L2_CID_CAMERA_BRIGHTNESS, runmode %d  \n", __func__, state->runmode);

		if(state->runmode != MC7_RUNMODE_RUNNING)
		{
			state->ev = ctrl->value;
			err = 0;
		}
		else
		{
			err = mc7_set_ev(sd, ctrl);
		}
		break;

	case V4L2_CID_CAMERA_WHITE_BALANCE:
		//dev_err(&client->dev, "%s: V4L2_CID_CAMERA_WHITE_BALANCE, runmode %d	\n", __func__, state->runmode);
		
		if(state->runmode != MC7_RUNMODE_RUNNING)
		{
			state->wb= ctrl->value;
			err = 0;
		}
		else
		{
			err = mc7_set_white_balance(sd, ctrl);		
		}
		break;

	case V4L2_CID_CAMERA_EFFECT:
		//dev_err(&client->dev, "%s: V4L2_CID_CAMERA_EFFECT, runmode %d	\n", __func__, state->runmode);

		if(state->runmode != MC7_RUNMODE_RUNNING)
		{
			state->effect= ctrl->value;
			err = 0;
		}
		else
		{
			err = mc7_set_effect(sd, ctrl);		
		}
		break;		

	case V4L2_CID_CAMERA_ISO:
		//dev_err(&client->dev, "%s: V4L2_CID_CAMERA_ISO, runmode %d	\n", __func__, state->runmode);

		if(state->runmode != MC7_RUNMODE_RUNNING)
		{
			state->iso = ctrl->value;
			err = 0;
		}
		else
		{
			err = mc7_set_iso(sd, ctrl);		
		}
		break;

	case V4L2_CID_CAMERA_METERING:
		//dev_err(&client->dev, "%s: V4L2_CID_CAMERA_METERING, runmode %d  \n", __func__, state->runmode);

		if(state->runmode != MC7_RUNMODE_RUNNING)
		{
			state->metering = ctrl->value;
			err = 0;
		}
		else
		{
			err = mc7_set_metering(sd, ctrl);
		}
		break;

	case V4L2_CID_CAMERA_CONTRAST:
		err = mc7_set_contrast(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_SATURATION:
		err = mc7_set_saturation(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_SHARPNESS:
		err = mc7_set_sharpness(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_WDR:
		err = mc7_set_wdr(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_ANTI_SHAKE:
		err = mc7_set_anti_shake(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_FACE_DETECTION:
		err = mc7_set_face_detection(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_SMART_AUTO:
		err = mc7_set_smart_auto(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_FOCUS_MODE:
		err = mc7_set_focus_mode(sd, ctrl);
		break;
		
	case V4L2_CID_CAMERA_VINTAGE_MODE:
		err = mc7_set_vintage_mode(sd, ctrl);
		break;
		
	case V4L2_CID_CAMERA_BEAUTY_SHOT:
		err = mc7_set_face_beauty(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_FACEDETECT_LOCKUNLOCK:
		err = mc7_set_face_lock(sd, ctrl);
		break;		

	//need to be modified
	case V4L2_CID_CAM_JPEG_QUALITY:
		if(ctrl->value < 0 || ctrl->value > 100){
			err = -EINVAL;
		} else {
			state->jpeg.quality = ctrl->value;
			err = 0;
		}
		break;

	case V4L2_CID_CAMERA_ZOOM:
		err = mc7_set_dzoom(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_TOUCH_AF_START_STOP:
		err = mc7_set_touch_auto_focus(sd, ctrl);
		break;
		
	case V4L2_CID_CAMERA_CAF_START_STOP:
		err = mc7_set_continous_af(sd, ctrl);
		break;	

	case V4L2_CID_CAMERA_OBJECT_POSITION_X:
		state->position.x = ctrl->value;
		err = 0;
		break;

	case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
		state->position.y = ctrl->value;
		err = 0;
		break;

	case V4L2_CID_CAMERA_OBJ_TRACKING_START_STOP:
		err = mc7_set_object_tracking(sd, ctrl);
		break;

	case V4L2_CID_CAMERA_SET_AUTO_FOCUS:
		err = mc7_set_auto_focus(sd, ctrl);
		break;		

	case V4L2_CID_CAMERA_FRAME_RATE:
		state->fps = ctrl->value;
		err = 0;		
		break;
		
	case V4L2_CID_CAMERA_ANTI_BANDING:
		state->anti_banding= ctrl->value;
		err = 0;		
		break;

	case V4L2_CID_CAMERA_SET_GAMMA:
		state->hd_gamma = ctrl->value;
		err = 0;
		break;
	
	case V4L2_CID_CAMERA_SET_SLOW_AE:
		state->hd_slow_ae = ctrl->value;
		err = 0;
		break;

	case V4L2_CID_CAMERA_CAPTURE:
		err = mc7_set_capture_start(sd, ctrl);
		break;

#endif

	case V4L2_CID_CAM_CAPTURE:
		err = mc7_set_capture_config(sd, ctrl);
		break;
	
	/* Used to start / stop preview operation. 
 	 * This call can be modified to START/STOP operation, which can be used in image capture also */
	case V4L2_CID_CAM_PREVIEW_ONOFF:
		if(ctrl->value)
			err = mc7_set_preview_start(sd);
		else
			err = mc7_set_preview_stop(sd);
		break;

	case V4L2_CID_CAM_UPDATE_FW:
		err = mc7_update_fw(sd);
		break;

	case V4L2_CID_CAM_SET_FW_ADDR:
		state->fw_info.addr = ctrl->value;
		err = 0;
		break;

	case V4L2_CID_CAM_SET_FW_SIZE:
		state->fw_info.size = ctrl->value;
		err = 0;
		break;

#if defined(CONFIG_ARIES_NTT) // Modify NTTS1
	case V4L2_CID_CAMERA_AE_AWB_DISABLE_LOCK:
		state->disable_aeawb_lock = ctrl->value;
		err = 0;
		break;
#endif

	case V4L2_CID_CAM_FW_VER:
		err = mc7_get_fw_data(sd);
		break;

	case V4L2_CID_CAM_DUMP_FW:
		err = mc7_dump_fw(sd);
		break;
		
	case V4L2_CID_CAMERA_BATCH_REFLECTION:
		err = mc7_get_batch_reflection_status(sd);
		break;

	case V4L2_CID_CAMERA_EXIF_ORIENTATION:
		state->exif_orientation_info = ctrl->value;
		err = 0;
		break;
		
	//s1_camera [ Defense process by ESD input ] _[
	case V4L2_CID_CAMERA_RESET:
		dev_err(&client->dev, "%s: V4L2_CID_CAMERA_RESET \n", __func__);
		err = 0; //= mc7_reset(sd); //SJKIM temp
		break;
	// _]

	case V4L2_CID_CAMERA_CHECK_DATALINE:
		state->check_dataline = ctrl->value;
		err = 0;
		break;	

	case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
		err = mc7_check_dataline_stop(sd);
		break;
		
	default:
		dev_err(&client->dev, "%s: no such control\n", __func__);
		break;
	}

	if (err < 0)
		dev_err(&client->dev, "%s: vidioc_s_ctrl failed %d, s_ctrl: id(%d), value(%d)\n", __func__, err, (ctrl->id - offset), ctrl->value);

	return err;
}
static int mc7_s_ext_ctrls(struct v4l2_subdev *sd, struct v4l2_ext_controls *ctrls)
{
	struct v4l2_ext_control *ctrl = ctrls->controls;
	int ret = 0;
	int i;

	for (i = 0; i < ctrls->count; i++, ctrl++)	{
		ret = mc7_s_ext_ctrl(sd, ctrl);

		if (ret)	{
			ctrls->error_idx = i;
			break;
		}
	}

	return ret;
}

static int mc7_s_ext_ctrl(struct v4l2_subdev *sd, struct v4l2_ext_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_state *state = to_state(sd);
	int err = -ENOIOCTLCMD;
	long temp = 0;
	struct gps_info_common * tempGPSType = NULL;
	

	switch (ctrl->id) {

	case V4L2_CID_CAMERA_GPS_LATITUDE:
		tempGPSType = (struct gps_info_common *)ctrl->reserved2[1];
		state->gpsInfo.mc7_gps_buf[0] = tempGPSType ->direction;
		state->gpsInfo.mc7_gps_buf[1] = tempGPSType ->dgree;
		state->gpsInfo.mc7_gps_buf[2] = tempGPSType ->minute;
		state->gpsInfo.mc7_gps_buf[3] = tempGPSType ->second;
		
		//dev_err(&client->dev,"gps_info_latiude NS: %d, dgree: %d, minute: %d, second: %d \n",state->gpsInfo.mc7_gps_buf[0], state->gpsInfo.mc7_gps_buf[1], state->gpsInfo.mc7_gps_buf[2], state->gpsInfo.mc7_gps_buf[3]);
		err = 0;
		break;

	case V4L2_CID_CAMERA_GPS_LONGITUDE:
		tempGPSType = (struct gps_info_common *)ctrl->reserved2[1];
		state->gpsInfo.mc7_gps_buf[4] = tempGPSType ->direction;
		state->gpsInfo.mc7_gps_buf[5] = tempGPSType ->dgree;
		state->gpsInfo.mc7_gps_buf[6] = tempGPSType ->minute;
		state->gpsInfo.mc7_gps_buf[7] = tempGPSType ->second;
		
		//dev_err(&client->dev,"gps_info_longitude EW: %d, dgree: %d, minute: %d, second: %d \n", state->gpsInfo.mc7_gps_buf[4], state->gpsInfo.mc7_gps_buf[5], state->gpsInfo.mc7_gps_buf[6], state->gpsInfo.mc7_gps_buf[7]);
		err = 0;
		break;

	case V4L2_CID_CAMERA_GPS_ALTITUDE:
		tempGPSType = (struct gps_info_common *)ctrl->reserved2[1];
		state->gpsInfo.mc7_altitude_buf[0] = tempGPSType ->direction;
		state->gpsInfo.mc7_altitude_buf[1] = (tempGPSType ->dgree)&0x00ff; //lower byte
		state->gpsInfo.mc7_altitude_buf[2] = ((tempGPSType ->dgree)&0xff00)>>8; //upper byte
		state->gpsInfo.mc7_altitude_buf[3] = tempGPSType ->minute;//float

		//dev_err(&client->dev,"gps_info_altitude PLUS_MINUS: %d, dgree_lower: %d, degree_lower: %d, minute: %d \n", state->gpsInfo.mc7_altitude_buf[0], state->gpsInfo.mc7_altitude_buf[1], state->gpsInfo.mc7_altitude_buf[2], state->gpsInfo.mc7_altitude_buf[3]);
		err = 0;
		break;

	case V4L2_CID_CAMERA_GPS_TIMESTAMP:
		temp = *((long *)ctrl->reserved2[1]);
		//dev_err(&client->dev, "%s: V4L2_CID_CAMERA_GPS_TIMESTAMP: %ld\n", __func__, (*((long *)ctrl->reserved)));
		state->gpsInfo.gps_timeStamp = temp;
		err = 0;
		break;
		
	case V4L2_CID_CAMERA_EXIF_TIME_INFO:
		state->exifTimeInfo =(struct tm *)ctrl->reserved2[1];
		err = 0;
		break;
	}

	if (err < 0)
		dev_err(&client->dev, "%s: vidioc_s_ext_ctrl failed %d\n", __func__, err);

	return err;
}

static int mc7_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_state *state = to_state(sd);
	int err = -EINVAL;
	
	unsigned char buf_set_pll[6] = { 0x06, 0x02, 0x0F, 0x24, 0x00, 0x1A };
	unsigned int len_set_pll = 6;

	unsigned char buf_set_start_addr[8] = { 0x08, 0x02, 0x0F, 0x0C, 0x68, 0x07, 0xE0, 0x00 };
	unsigned int len_set_start_addr = 8;

	unsigned char buf_start_loader[5] = { 0x05, 0x02, 0x0F, 0x12, 0x01 };
	unsigned int len_start_loader = 5;
	unsigned char intr_value;
	
	dev_err(&client->dev, "mc7_init\n");
	
	//wait interrupt (boot complete)
	//mdelay(100);
	
	#if 1
	err = mc7_waitfor_int_timeout(client, 0x01, 100, POLL_TIME_MS);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: mc7_waitfor_int_timeout\n", __func__);
		return -EIO;
	}
	else
	{
		//clear interrupt
		mc7_read_category_parm(client, MC7_8BIT, 0x00, 0x1C, &intr_value);
	       dev_err(&client->dev, "mc7_init intr_value :0x%02x\n", intr_value);

	}	
	#endif
	
	//set PLL 
	//err = mc7_i2c_write_multi(client, buf_set_pll, len_set_pll);
	err = mc7_write_category_parm(client, MC7_16BIT, 0x0F, 0x24, 0x001A);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for buf_set_pll\n", __func__);
		return -EIO;
	}
		
       //transfer loader
	err = mc7_transfer_loader(sd);
	if(err < 0){
		dev_err(&client->dev, "%s: Failed: transfer_loader\n", __func__);
		return -EIO;
	}

	//set start address of the loader 
	//err = mc7_i2c_write_multi(client, buf_set_start_addr, len_set_start_addr);
	err = mc7_write_category_parm(client, MC7_32BIT, 0x0F, 0x0C, 0x6807E000);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for buf_set_start_addr\n", __func__);
		return -EIO;
	}
	
	//start loader program
	//err = mc7_i2c_write_multi(client, buf_start_loader, len_start_loader);
	err = mc7_write_category_parm(client, MC7_8BIT, 0x0F, 0x12, 0x01);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for buf_start_loader\n", __func__);
		return -EIO;
	}

	//wait interrupt (startup complete)
	//mdelay(1000);
	
	#if 1
	err = mc7_waitfor_int_timeout(client, 0x01, 1000, POLL_TIME_MS);
	if(err < 0){
		dev_err(&client->dev, "%s: failed: mc7_waitfor_int_timeout\n", __func__);
		return -EIO;
	}
	else
	{
		//clear interrupt
		mc7_read_category_parm(client, MC7_8BIT, 0x00, 0x1C, &intr_value);
	       dev_err(&client->dev, "mc7_init intr_value :0x%02x\n", intr_value);

	}
	#endif
	
	mc7_init_parameters(sd);

	return 0;
}

/*
 * s_config subdev ops
 * With camera device, we need to re-initialize every single opening time therefor,
 * it is not necessary to be initialized on probe time. except for version checking
 * NOTE: version checking is optional
 */
static int mc7_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mc7_state *state = to_state(sd);
	struct mc7_platform_data *pdata;

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
		state->pix.pixelformat = DEFAULT_PIX_FMT;
	else
		state->pix.pixelformat = pdata->pixelformat;

	if (!pdata->freq)
		state->freq = DEFUALT_MCLK;	/* 24MHz default */
	else
		state->freq = pdata->freq;

	return 0;
}

static const struct v4l2_subdev_core_ops mc7_core_ops = {
	.init = mc7_init,	/* initializing API */
	.s_config = mc7_s_config,	/* Fetch platform data */
	.queryctrl = mc7_queryctrl,
	.querymenu = mc7_querymenu,
	.g_ctrl = mc7_g_ctrl,
	.s_ctrl = mc7_s_ctrl,
	.s_ext_ctrls = mc7_s_ext_ctrls,
};

static const struct v4l2_subdev_video_ops mc7_video_ops = {
	.s_crystal_freq = mc7_s_crystal_freq,
	.g_fmt = mc7_g_fmt,
	.s_fmt = mc7_s_fmt,
	.enum_framesizes = mc7_enum_framesizes,
	.enum_frameintervals = mc7_enum_frameintervals,
	.enum_fmt = mc7_enum_fmt,
	.try_fmt = mc7_try_fmt,
	.g_parm = mc7_g_parm,
	.s_parm = mc7_s_parm,
};

static const struct v4l2_subdev_ops mc7_ops = {
	.core = &mc7_core_ops,
	.video = &mc7_video_ops,
};

/*
 * mc7_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int mc7_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct mc7_state *state;
	struct v4l2_subdev *sd;

	state = kzalloc(sizeof(struct mc7_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	state->runmode = MC7_RUNMODE_NOTREADY;
	state->pre_af_status = -1;
	state->cur_af_status = -2;

	sd = &state->sd;
	strcpy(sd->name, MC7_DRIVER_NAME);

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &mc7_ops);

	mc7_info(&client->dev, "5MP camera MC7 loaded.\n");

	return 0;
}

static int mc7_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	mc7_set_af_softlanding(sd);

	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));

	mc7_info(&client->dev, "Unloaded camera sensor MC7.\n");

	return 0;
}

static const struct i2c_device_id mc7_id[] = {
	{ MC7_DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mc7_id);

static struct v4l2_i2c_driver_data v4l2_i2c_data = {
	.name = MC7_DRIVER_NAME,
	.probe = mc7_probe,
	.remove = mc7_remove,
	.id_table = mc7_id,
};

MODULE_DESCRIPTION("NEC MC7-NEC 5MP camera driver");
MODULE_AUTHOR("Tushar Behera <tushar.b@samsung.com>");
MODULE_LICENSE("GPL");
