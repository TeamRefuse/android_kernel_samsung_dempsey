#if defined(CONFIG_S5PC110_HAWK_BOARD)


/*******************************************************************************

	 Author :	 	R&D Team, North America/GSM, 
				Mobile communication division, 
				SAMSUNG ELECTRONICS
				
	 Desription :	Samsung Customized TAOS Triton ambient light 
				and proximity sensor driver

	Updated By: Aakash Manik
				aakash.manik@samsung.com
				Kernel & BSP
				North America GSM Android
				Samsung Engineering Lab, India
		
	Date:		September 22,2010
				
	Description: Updated I2C Mechanism Kernel version 2.6.32 
				 with backward compatibility upto 2.6.29.
				 Driver Functionality is based on previous version.

	
  *******************************************************************************/

/* Includes */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/timer.h>
#include <asm/uaccess.h>
#include <asm/errno.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <mach/hardware.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <linux/regulator/max8998.h>
#include <mach/max8998_function.h>

#include "taos_triton.h"

#define GPIO_TAOS_ALS_INT 	IRQ_EINT10
//#define TAOS_DEBUG 1

/*********** for debug **********************************************************/
#ifdef TAOS_DEBUG
#define gprintk(fmt, x... ) printk( "%s(%d): " fmt, __FUNCTION__ ,__LINE__, ## x)
#else
#define gprintk(x...) do { } while (0)
#endif
/*******************************************************************************/


static struct taos_data *taos;
struct workqueue_struct *taos_wq;
static struct wake_lock prx_wake_lock;
static struct device *switch_cmd_dev;

static bool light_enable = 0;
static bool proximity_enable = 0;
int autobrightness_mode = OFF;
static state_type cur_state = LIGHT_INIT;

/* global var */
static struct class *lightsensor_class;
static struct i2c_client *opt_i2c_client = NULL;

static TAOS_ALS_FOPS_STATUS taos_als_status = TAOS_ALS_CLOSED;
static TAOS_PRX_FOPS_STATUS taos_prx_status = TAOS_PRX_CLOSED;
static TAOS_CHIP_WORKING_STATUS taos_chip_status = TAOS_CHIP_UNKNOWN;
static TAOS_PRX_DISTANCE_STATUS taos_prox_dist = TAOS_PRX_DIST_UNKNOWN;

#define TAOS_PRX_THRES_IN 		500
#define TAOS_PRX_THRES_OUT 		300 


static int taos_prx_thres_hi = TAOS_PRX_THRES_IN;
static int taos_prx_thres_lo = 0; 

static int cur_adc_value = 0;
static short proximity_value = 0;
static bool lightsensor_test = 0;

static int buffering = 2;
extern int backlight_level;

/* Per-device data */
struct taos_data{
	struct work_struct work_prox;  /* for proximity sensor */
	struct work_struct work_light; /* for light_sensor     */
	struct	i2c_client *client;
	struct	input_dev *input_dev;
	struct hrtimer timer;		//NAGSM_Android_SEL_Kernel_Aakash_20100904 Introducing Timer for polling mechanism in Version1.2
	struct timer_list light_init_timer;
	
	int             irq;
};

//static struct device *devp;
static struct wake_lock prx_wake_lock;

static int taos_opt_suspend(struct i2c_client *client, pm_message_t state);
static int taos_opt_resume(struct i2c_client *client);
static int taos_open_als(struct inode *inode, struct file *file);
static int taos_open_prox(struct inode *inode, struct file *file);
static int taos_release_als(struct inode *inode, struct file *file);
static int taos_release_prox(struct inode *inode, struct file *file);
#if 1 //B1
static int taos_read_als(struct file *file, double *buf, size_t count, loff_t *ppos);
#else
static int taos_read_als(struct file *file, int *buf, size_t count, loff_t *ppos);
#endif
static int taos_read_prox(struct file *file, char *buf, size_t count, loff_t *ppos);

static int taos_chip_on(void);
static int taos_chip_off(void);
static int taos_read_als_data(void);
static int taos_get_lux(void);
static int taos_prox_poll(struct taos_prox_info *prxp);
static int taos_register_dump(void);

static void taos_on(struct taos_data *taos, int type);
static void taos_off(struct taos_data *taos, int type);
static void taos_ldo_enable(void);
static void taos_ldo_disable(void);
static void taos_work_func_prox(struct work_struct *work);
static void taos_work_func_light(struct work_struct *work);
irqreturn_t taos_irq_handler(int irq, void *dev_id);
static enum hrtimer_restart taos_timer_func(struct hrtimer *timer);


static int opt_i2c_read(u8 reg, u8 *val );
static int opt_i2c_write( u8 reg, u8 *val );
static int opt_i2c_write_command( u8 val );

short gp2a_get_proximity_value(void);
short gp2a_get_proximity_enable(void);
bool gp2a_get_lightsensor_status(void);
static double StateToLux(state_type state);

#define MDNIE_TUNINGMODE_FOR_BACKLIGHT

#ifdef MDNIE_TUNINGMODE_FOR_BACKLIGHT
int pre_val = -1;
extern int current_gamma_value;
extern u16 *pmDNIe_Gamma_set[];

typedef enum
{
	mDNIe_UI_MODE,
	mDNIe_VIDEO_MODE,
	mDNIe_VIDEO_WARM_MODE,
	mDNIe_VIDEO_COLD_MODE,
	mDNIe_CAMERA_MODE,
	mDNIe_NAVI
}Lcd_mDNIe_UI;
extern Lcd_mDNIe_UI current_mDNIe_UI;
extern void mDNIe_Mode_set_for_backlight(u16 *buf);
int value_buf[4] = {0};
int IsChangedADC(int val)
{
	int j = 0;
	int ret = 0;
	int adc_index = 0;
	static int adc_index_count = 0;

	adc_index = (adc_index_count)%4;		
	adc_index_count++;

	if(pre_val == -1) //ADC buffer initialize 
	{
		for(j = 0; j<4; j++)
			value_buf[j] = val;
		pre_val = 0;
	}
    else
    {
    	value_buf[adc_index] = val;
	}

	ret = ((value_buf[0] == value_buf[1])&& \
			(value_buf[1] == value_buf[2])&& \
			(value_buf[2] == value_buf[3]))? 1 : 0;
	
	if(adc_index_count == 4)
		adc_index_count = 0;
	return ret;
}
#endif


short gp2a_get_proximity_value(void)
{
	  return proximity_value;
}

short gp2a_get_proximity_enable(void)
{
	  return proximity_enable;
}

bool gp2a_get_lightsensor_status(void)
{
	  return light_enable;
}

EXPORT_SYMBOL(gp2a_get_proximity_value);
EXPORT_SYMBOL(gp2a_get_proximity_enable);
EXPORT_SYMBOL(gp2a_get_lightsensor_status);



static double StateToLux(state_type state)
{
	double lux = 0;

	gprintk("[%s] cur_state:%d\n",__func__,state);
		
	if(state== LIGHT_LEVEL4){
		lux = 3000.0;
	}
	else if(state == LIGHT_LEVEL3){
		lux = 1500.0;
	}
	else if(state== LIGHT_LEVEL2){
		lux = 150.0;
	}
	else if(state == LIGHT_LEVEL1){
		lux = 15.0;
	}
	else {
		lux = 5000.0;
		gprintk("[%s] cur_state fail\n",__func__);
	}
	return lux;
}

static ssize_t lightsensor_file_state_show(struct device *dev, struct device_attribute *attr, char *buf)	
{
	int adc = 0;
	int sum = 0; 	// Temp
	int i = 0;		// Temp
	
	gprintk("called %s \n",__func__);

	if(!light_enable)
	{
		#if 1 //low-pass filter
		for(i = 0; i < 10; i++)
		{
			adc = taos_read_als_data(); //lightsensor_get_adcvalue();
			msleep(20);
			sum += adc;
		}
		adc = sum/10;
		#else
		adc = taos_read_als_data(); 
		#endif		
		gprintk("called %s	- subdued alarm(adc : %d)\n",__func__,adc);
		return sprintf(buf,"%d\n",adc);
	}
	else
	{
		gprintk("called %s	- *#0589#\n",__func__);
	#if 1
	return sprintf(buf,"%d\n",cur_adc_value);
	#else 
		adc = s3c_adc_get_adc_data(ADC_CHANNEL);
		return sprintf(buf,"%d\n",adc);
	#endif
	}
}

static ssize_t lightsensor_file_state_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)		
{	
	int value;
	sscanf(buf, "%d", &value);
	
	gprintk("called %s \n",__func__);

	if(value==1)
	{
		autobrightness_mode = ON;
		printk(KERN_DEBUG "[brightness_mode] BRIGHTNESS_MODE_SENSOR\n");
	}
	else if(value==0) 
	{
		autobrightness_mode = OFF;
		printk(KERN_DEBUG "[brightness_mode] BRIGHTNESS_MODE_USER\n");
		
		#ifdef MDNIE_TUNINGMODE_FOR_BACKLIGHT
		if(pre_val==1)
		{
		//	mDNIe_Mode_set_for_backlight(pmDNIe_Gamma_set[2]);   //Unimplemented	//NAGSM_Android_SEL_Kernel_Aakash_20100922
		}	
		pre_val = -1;
		#endif
	}

	return size;
}

/* for light sensor on/off control from platform */
static ssize_t lightsensor_file_cmd_show(struct device *dev, struct device_attribute *attr, char *buf)				
{
	gprintk("called %s \n",__func__);
	
	return sprintf(buf,"%u\n",light_enable);
}

static ssize_t lightsensor_file_cmd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)	
{
	struct taos_data *taos = dev_get_drvdata(dev);
	int value;
    sscanf(buf, "%d", &value);

	printk(KERN_INFO "[LIGHT_SENSOR] in lightsensor_file_cmd_store, input value = %d \n",value);

	if(value==1)
	{
		//gp2a_on(gp2a,LIGHT);	Light sensor is always on
		lightsensor_test = 1;
		value = ON;
		printk(KERN_INFO "[LIGHT_SENSOR] *#0589# test start, input value = %d \n",value);
	}
	else if(value==0) 
	{
		//gp2a_off(gp2a,LIGHT);	Light sensor is always on
		lightsensor_test = 0;
		value = OFF;
		printk(KERN_INFO "[LIGHT_SENSOR] *#0589# test end, input value = %d \n",value);
	}
	/* temporary test code for proximity sensor */
	else if(value==3 && proximity_enable == OFF)
	{
		taos_on(taos,PROXIMITY);
		printk("[PROXIMITY] Temporary : Power ON\n");
	}
	else if(value==2 && proximity_enable == ON)
	{
		taos_off(taos,PROXIMITY);
		printk("[PROXIMITY] Temporary : Power OFF\n");
	}
	/* for factory simple test mode */
	if(value==7 && light_enable == OFF)
	{
	//	light_init_period = 2;
		taos_on(taos,LIGHT);
		value = 7;
	}

	return size;
}

static DEVICE_ATTR(lightsensor_file_cmd,0666, lightsensor_file_cmd_show, lightsensor_file_cmd_store);
static DEVICE_ATTR(lightsensor_file_state,0666, lightsensor_file_state_show, lightsensor_file_state_store);

#define TAOS_USES_NAME_AS_GP2A // Use the device and driver name as "gp2a" instead of "taos" for backward capability.

/* Work queues */
static struct work_struct taos_als_int_wq;
static struct work_struct taos_prox_int_wq;

/* Als/prox info */
static struct taos_prox_info prox_cur_info;
static struct taos_prox_info *prox_cur_infop = &prox_cur_info;

/* File operations */
static struct file_operations taos_fops_als = 
{
	.owner = THIS_MODULE,
	.open = taos_open_als,
	.release = taos_release_als,
	.read = taos_read_als,
};

/* File operations */
static struct file_operations taos_fops_prox = 
{
	.owner = THIS_MODULE,
	.open = taos_open_prox,
	.release = taos_release_prox,
	.read = taos_read_prox,
};                

static struct miscdevice light_device = 
{
    .minor  = MISC_DYNAMIC_MINOR,
    .name   = "light",
    .fops   = &taos_fops_als,
};

static struct miscdevice proximity_device = 
{
    .minor  = MISC_DYNAMIC_MINOR,
    .name   = "proximity",
    .fops   = &taos_fops_prox,
};

//NAGSM_Android_SEL_Kernel_Aakash_20100922
static int opt_i2c_read(u8 reg , u8 *val)
{
	
	int ret;
	udelay(10);
	//ret = i2c_smbus_read_byte_data(taos->client,reg);


	i2c_smbus_write_byte(taos->client, (CMD_REG | reg));
	
    //printk("%s ret %d i2c transfer value ret1\n", __func__, ret);

	ret = i2c_smbus_read_byte(taos->client);
	*val = ret;
	//printk("%s ret %x\n", __func__, ret);

    return ret;
}


static int opt_i2c_write( u8 reg, u8 *val )
{
	int ret;
	udelay(10);
	ret = i2c_smbus_write_byte_data(taos->client,(CMD_REG | reg),*val);

    //printk("%s ret %x\n", __func__,ret);
    return ret;
}

static int opt_i2c_write_command( u8 val )
{
	int ret;
	udelay(10);
	ret = i2c_smbus_write_byte(taos->client, val);

    gprintk("[TAOS Command] val=[0x%x] - ret=[0x%x]\n", val, ret);
    return ret;
}

static int taos_register_dump(void)
{
	int i=0; 
	int ret = 0;
	u8 chdata[0x20];
	
	for (i = 0; i < 0x20; i++) 
	{
		if ((ret = opt_i2c_read((CNTRL + i), &chdata[i]))< 0)
		{
			gprintk("i2c_smbus_read_byte() to chan0/1/lo/hi"" regs failed in taos_register_dump()\n"); //dev_err(devp, "");			
			return (ret);
		}
	}
	
	printk("** taos register dump *** \n");
	for (i = 0; i < 0x20; i++) 
	{
		printk("(%x)0x%02x, ", i, chdata[i]);
	}
	printk("\n");	
	
	return 0;
}

static void taos_work_func_prox(struct work_struct *work)
{
	int i=0;
	int ret= 0;
	u8 prox_int_thresh[4];

	u8 temp;

	struct taos_data *taos = container_of(work,struct taos_data,work_prox);

	//gprintk("\n");

	/* read proximity value. */
	if ((ret = taos_prox_poll(prox_cur_infop)) < 0) 
	{
		gprintk( "call to prox_poll() failed in taos_prox_poll_timer_func()\n");
		return;
	}

	switch ( taos_prox_dist ) /* HERE : taos_prox_dist => PREVIOUS STATUS */
	{
	case TAOS_PRX_DIST_IN:
		
		gprintk("TAOS_PRX_DIST_IN  ==> OUT \n");

		/* Condition for detecting IN */
		prox_int_thresh[0] = (0x0) 			& 0xff;
		prox_int_thresh[1] = (0x0 >> 8) 		& 0xff;
		prox_int_thresh[2] = (TAOS_PRX_THRES_IN)	 	& 0xff;
		prox_int_thresh[3] = (TAOS_PRX_THRES_IN >> 8) 	& 0xff;

		/* Current status */
		taos_prox_dist = TAOS_PRX_DIST_OUT;	  /* HERE : taos_prox_dist => CURRENT STATUS */
		break;
		
	case TAOS_PRX_DIST_OUT:
	case TAOS_PRX_DIST_UNKNOWN:
	default:
		
		gprintk("TAOS_PRX_DIST_OUT ==> IN \n");
		
		/* Condition for detecting OUT */
		prox_int_thresh[0] = (TAOS_PRX_THRES_OUT) 			& 0xff;
		prox_int_thresh[1] = (TAOS_PRX_THRES_OUT >> 8) 		& 0xff;
		prox_int_thresh[2] = (0x0)	 					& 0xff;
		prox_int_thresh[3] = (0x0 >> 8) 				& 0xff;		
		
		/* Current status */
		taos_prox_dist = TAOS_PRX_DIST_IN;	 /* HERE : taos_prox_dist => CURRENT STATUS */
		break;
	}
	
	for (i = 0; i < 4; i++) 
	{
		ret = opt_i2c_write((CMD_REG|(PRX_MINTHRESHLO + i)), &prox_int_thresh[i]);
		if ( ret < 0)
		{
			gprintk( "i2c_smbus_write_byte_data failed in taos_chip_on, err = %d\n", ret);
		}
	}	
	
		/* Interrupt clearing */

	temp = (CMD_REG|CMD_SPL_FN|CMD_PROXALS_INTCLR);
	ret = opt_i2c_write_command(temp);

	if ( ret < 0)
	{
		gprintk( "i2c_smbus_write_byte_data failed in taos_irq_handler(), err = %d\n", ret);
	}	

	return;
}

static void taos_work_func_light(struct work_struct *work)
{
	int adc=0;
	state_type level_state = LIGHT_INIT;	

	//read value 	
	adc = taos_read_als_data();	
	#if 1 //B1
	gprintk("Optimized[average] adc = %d \n",adc);
	gprintk("cur_state = %d\n",cur_state);
	gprintk("light_enable = %d\n",light_enable);
	#else
	gprintk("taos_work_func_light called adc=[%d]\n", adc);
	#endif
	
	// check level and set bright_level
	if(adc >= 1500)
	{
		level_state = LIGHT_LEVEL4;
		buffering = 4;
	}

	else if(adc >= 1400 && adc < 1500)
	{
		if(buffering == 4)
		{	
			level_state = LIGHT_LEVEL4;
			buffering = 4;
		}
		else if((buffering == 1)||(buffering == 2)||(buffering == 3))
		{
			level_state = LIGHT_LEVEL3;
			buffering = 3;
		}
	}
	
	else if(adc >= 150 && adc < 1400)	//else if(adc >= 1800 && adc < 1900)
	{
		level_state = LIGHT_LEVEL3;
	}

	else if(adc >= 120 && adc < 150)	//else if(adc >= 1200 && adc < 1800)
	{
		if((buffering == 3)||(buffering == 4))
		{	
			level_state = LIGHT_LEVEL3;
			buffering = 3;
		}
		else if((buffering == 1)||(buffering == 2))
		{
			level_state = LIGHT_LEVEL2;
			buffering = 2;
		}
	}

	else if(adc >= 30 && adc < 120)//else if(adc >= 500 && adc < 1200)
	{
		level_state = LIGHT_LEVEL2;
		buffering = 2;
	}

	else if(adc >= 15 && adc < 30)//else if(adc >= 80 && adc < 500)
	{
		if((buffering == 2)||(buffering == 3)||(buffering == 4))
		{	
			level_state = LIGHT_LEVEL2;
			buffering = 2;
		}
		else if(buffering == 1)
		{
			level_state = LIGHT_LEVEL1;
			buffering = 1;
		}
	}
	
	else if(adc < 15)
	{
		level_state = LIGHT_LEVEL1;
		buffering = 1;
	}

	if(!lightsensor_test)
	{
		gprintk("backlight_level = %d\n", backlight_level); //Temp
		cur_state = level_state;	
	}

	//brightness setting 
	#ifdef MDNIE_TUNINGMODE_FOR_BACKLIGHT
		if(autobrightness_mode)
		{        //Unimplemented	//NAGSM_Android_SEL_Kernel_Aakash_20100922
		/*	if((pre_val!=1)&&(current_gamma_value == 24)&&(level_state == LIGHT_LEVEL4)&&(current_mDNIe_UI == mDNIe_UI_MODE))
			{
				mDNIe_Mode_set_for_backlight(pmDNIe_Gamma_set[1]);
				pre_val = 1;
				gprintk("mDNIe_Mode_set_for_backlight - pmDNIe_Gamma_set[1]\n" );
			}*/		//NAGSM_Android_SEL_Kernel_Aakash_20100922
		}
	#endif

	//gprintk("cur_state =[%d], lightsensor_test=[%d] \n",cur_state ,lightsensor_test); //Temp

	return;
}

static enum hrtimer_restart taos_timer_func(struct hrtimer *timer)
{
	ktime_t light_polling_time;		
	struct taos_data *taos = container_of(timer, struct taos_data, timer);
			
	queue_work(taos_wq, &taos->work_light);
	light_polling_time = ktime_set(0,0);
	light_polling_time = ktime_add_us(light_polling_time,500000);
	hrtimer_start(&taos->timer,light_polling_time,HRTIMER_MODE_REL);
	
	return HRTIMER_NORESTART;

}

/*****************************************************************************************
 *  
 *  function    : taos_on 
 *  description : This function is power-on function for optical sensor.
 *
 *  int type    : Sensor type. Two values is available (PROXIMITY,LIGHT).
 *                it support power-on function separately.
 *                
 *                 
 */
static void taos_on(struct taos_data *taos, int type)
{
	ktime_t light_polling_time;
	gprintk(KERN_INFO "[OPTICAL] taos_on(%d)\n",type);
	
	if(type == PROXIMITY)
	{
		#if 0
		gprintk("[PROXIMITY] go nomal mode : power on \n");
		value = 0x18;
		opt_i2c_write((u8)(REGS_CON),&value);

		value = 0x40;
		opt_i2c_write((u8)(REGS_HYS),&value);

		value = 0x01;
		opt_i2c_write((u8)(REGS_OPMOD),&value);
		
		gprintk("enable irq for proximity\n");
		enable_irq(gp2a ->irq);

		value = 0x00;
		opt_i2c_write((u8)(REGS_CON),&value);
		
		value = 0x01;
		opt_i2c_write((u8)(REGS_OPMOD),&value);
		proximity_enable =1;
		#endif	
	}
	if(type == LIGHT)
	{
		gprintk(KERN_INFO "[LIGHT_SENSOR] timer start for light sensor\n");
		light_polling_time = ktime_set(0,0);
		light_polling_time = ktime_add_us(light_polling_time,500000);

	    hrtimer_start(&taos->timer,light_polling_time,HRTIMER_MODE_REL);
		light_enable = 1;
	}
}

/*****************************************************************************************
 *  
 *  function    : taos_off 
 *  description : This function is power-off function for optical sensor.
 *
 *  int type    : Sensor type. Three values is available (PROXIMITY,LIGHT,ALL).
 *                it support power-on function separately.
 *                
 *                 
 */

static void taos_off(struct taos_data *taos, int type)
{
	gprintk(KERN_INFO "[OPTICAL] taos_off(%d)\n",type);
	if(type == PROXIMITY || type == ALL)
	{

		#if 0
		gprintk("[PROXIMITY] go power down mode  \n");
		
		//gprintk("disable irq for proximity \n");
		//disable_irq(gp2a ->irq);
		
		value = 0x00;
		opt_i2c_write((u8)(REGS_OPMOD),&value);
		
		proximity_enable =0;
		proximity_value = 0;		
		#endif
	}

	if(type ==LIGHT)
	{
		gprintk("[LIGHT_SENSOR] timer cancel for light sensor\n");
		hrtimer_cancel(&taos->timer);
		light_enable = 0;

	}
}


irqreturn_t taos_irq_handler(int irq, void *dev_id)
{
	struct taos_data *taos = dev_id;

	printk(KERN_DEBUG, "taos_irq_handler() is called\n");

	queue_work(taos_wq, &taos->work_prox);	
	wake_lock_timeout(&prx_wake_lock, 5*HZ);
	
	return IRQ_HANDLED;
}



static int taos_opt_suspend(struct i2c_client *client, pm_message_t state)
{
	struct taos_data *taos = i2c_get_clientdata(client);

	gprintk("light_enable=[%d], proximity_enable=[%d], taos_prx_status=[%d], taos_als_status=[%d] \n",
		light_enable, proximity_enable, taos_prx_status, taos_als_status);

	if(light_enable)
	{
		gprintk("[%s] : hrtimer_cancel \n",__func__);
		
		if ( taos == NULL)
		{
			printk("[%s] taos_data taso is NULL \n",__func__);
		}
		else
		{
			hrtimer_cancel(&taos->timer);
		}
	}

	if( taos_prx_status == TAOS_PRX_CLOSED && taos_als_status == TAOS_ALS_CLOSED )
	{
		taos_ldo_disable();
	}
	else
	{
	
	}	

	return 0;
}

static int taos_opt_resume(struct i2c_client * client)
{
	struct taos_data *taos = i2c_get_clientdata(client);
	
	gprintk("[%s] : \n",__func__);
	
	///s3c_gpio_cfgpin(GPIO_PS_ON, S3C_GPIO_OUTPUT);
	///gpio_set_value(GPIO_PS_ON, 1);

	taos_ldo_enable();

	/* wake_up source handler */
	if(proximity_enable)
	{
	
	}
	
	if(light_enable)
	{

	}
	
	return 0;
}




/*
 * Device open function - if the driver is loaded in interrupt mode, a valid
 * irq line is expected, and probed for, by requesting, and testing, all the
 * available irqs on the system using a probe interrupt handler. If found, the
 * irq is requested again, for subsequent use/handling by the run-time handler.
 */
static int taos_open_als(struct inode *inode, struct file *file)
{
	int ret = 0;	
	struct taos_data *taos = dev_get_drvdata(switch_cmd_dev);

//	device_released = 0;

	gprintk("[%s] \n",__func__);
	
	if( taos_prx_status == TAOS_PRX_CLOSED )
	{
		taos_chip_on();
	}
	taos_als_status = TAOS_ALS_OPENED;

	#if 1 // B1
	taos_on(taos, LIGHT);
	#endif

	return (ret);
}

/*
 * Device open function - if the driver is loaded in interrupt mode, a valid
 * irq line is expected, and probed for, by requesting, and testing, all the
 * available irqs on the system using a probe interrupt handler. If found, the
 * irq is requested again, for subsequent use/handling by the run-time handler.
 */
static int taos_open_prox(struct inode *inode, struct file *file)
{
	int ret = 0;	
	struct taos_data *taos = dev_get_drvdata(switch_cmd_dev);
	
//	device_released = 0;
	gprintk("[%s] \n",__func__);

	if( taos_als_status == TAOS_ALS_CLOSED )
	{
		taos_chip_on();
	}
	taos_prx_status = TAOS_PRX_OPENED;

	#if 0 // B1
	taos_on(taos, LIGHT);
	#endif

	return (ret);
}


/*
 * Device release
 */
static int taos_release_als(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct taos_data *taos = dev_get_drvdata(switch_cmd_dev);
	
	gprintk("[%s]\n",__func__);
	
	taos_off(taos,LIGHT);	

	if( taos_prx_status == TAOS_PRX_CLOSED )
	{
		#if defined(CONFIG_HAWK_VER_B1_REAL_ADDED_FEATURE) ////NAGSM_Android_HQ_KERNEL_CLEE_20100928 : Setup Hawk Real Board Rev 0.1 Proximity sensor
		
		#else
		taos_chip_off();
		#endif
	}
	taos_als_status = TAOS_ALS_CLOSED;

	return (ret);
}

/*
 * Device release
 */
static int taos_release_prox(struct inode *inode, struct file *file)
{
	int ret = 0;
//	device_released = 1;
//	prox_on = 0;
	
	gprintk("[%s]\n", __func__);

	if( taos_als_status == TAOS_ALS_CLOSED )
	{
		taos_chip_off();
	}
	taos_prx_status = TAOS_PRX_CLOSED;

	return (ret);
}


/*
 	 * ALS_DATA - request for current ambient light data. If correctly
 	 * enabled and valid data is available at the device, the function
 	 * for lux conversion is called, and the result returned if valid.
*/
#define	ADC_BUFFER_NUM			8
static int adc_value_buf[ADC_BUFFER_NUM] = {0, 0, 0, 0, 0, 0};

static int taos_read_als_data(void)
{
		int ret = 0;
		int lux_val=0;
		
		u8 reg_val;
		#if 1 // D1 -average filter	
		int i=0;
		int j=0;
		unsigned int adc_total = 0;
		static int adc_avr_value = 0;
		unsigned int adc_index = 0;
		static unsigned int adc_index_count = 0;
		unsigned int adc_max = 0;
		unsigned int adc_min = 0;
		#endif
	
		if ((ret = opt_i2c_read(CNTRL, &reg_val))< 0)
		{
			gprintk( "opt_i2c_read to cmd reg failed in taos_read_als_data\n");			
			return 0; //r0 return (ret);
		}

		if ((reg_val & (CNTL_ADC_ENBL | CNTL_PWRON)) != (CNTL_ADC_ENBL | CNTL_PWRON))
		{
			return 0; //r0 return (-ENODATA);
		}

		if ((lux_val = taos_get_lux()) < 0)
		{
			gprintk( "taos_get_lux() returned error %d in taos_read_als_data\n", lux_val);
		}

		// gprintk("** Returned lux_val  = [%d]\n", lux_val);

		/* QUICK FIX : multiplu lux_val by 4 to make the value similar to ADC value. */
		//lux_val = lux_val * 5;
		
		if (lux_val >= MAX_LUX)
		{
			lux_val = MAX_LUX; //return (MAX_LUX);
		}
		
		cur_adc_value = lux_val; 
		gprintk("taos_read_als_data cur_adc_value = %d (0x%x)\n", cur_adc_value, cur_adc_value);

		#if 1 // D1 -average filter		
			adc_index = (adc_index_count++)%ADC_BUFFER_NUM;		

			if(cur_state == LIGHT_INIT) //ADC buffer initialize (light sensor off ---> light sensor on)
			{
				for(j = 0; j<ADC_BUFFER_NUM; j++)
					adc_value_buf[j] = lux_val; // value;
			}
		    else
		    {
		    	adc_value_buf[adc_index] = lux_val; //value;
			}
			
			adc_max = adc_value_buf[0];
			adc_min = adc_value_buf[0];

			for(i = 0; i <ADC_BUFFER_NUM; i++)
			{
				adc_total += adc_value_buf[i];

				if(adc_max < adc_value_buf[i])
					adc_max = adc_value_buf[i];
							
				if(adc_min > adc_value_buf[i])
					adc_min = adc_value_buf[i];
			}
			adc_avr_value = (adc_total-(adc_max+adc_min))/(ADC_BUFFER_NUM-2);
			
			if(adc_index_count == ADC_BUFFER_NUM-1)
				adc_index_count = 0;

			return adc_avr_value;
		#else
			return (lux_val);
		#endif

}


/*
 * Device read - reads are permitted only within the range of the accessible
 * registers of the device. The data read is copied safely to user space.
 */
#if 1 //B1
static int taos_read_als(struct file *file, double *buf, size_t count, loff_t *ppos)
#else
static int taos_read_als(struct file *file, int *buf, size_t count, loff_t *ppos)
#endif
{
	#if 1 //B1
	double lux_val = 0;
	#else
	int lux_val = 0;
	#endif
	
	//	gprintk("%s",__func__);

	#if 1 //B1
	lux_val = StateToLux(cur_state);
	#else
	lux_val = taos_read_als_data();
	#endif
	
	put_user(lux_val, buf);
	
	return (1);
}

/*
 * Device read - reads are permitted only within the range of the accessible
 * registers of the device. The data read is copied safely to user space.
*/
static int taos_read_prox(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	char value = 1;	

	ret = taos_prox_poll(prox_cur_infop);
	
	if(prox_cur_infop->prox_event)
	{
		value = 0;
	}
	
	put_user(value,buf);
	
	gprintk("taos_read_prox = [%d]\n ", value);

	return 1; 
}


/*
 	 * ALS_ON - called to set the device in ambient light sense mode.
 	 * configured values of light integration time, inititial interrupt
 	 * filter, gain, and interrupt thresholds (if interrupt driven) are
 	 * initialized. Then power, adc, (and interrupt if needed) are enabled.
*/

static int taos_chip_on(void)
{
	int i = 0;
	int ret = 0;		
	int num_try_init = 0;
	int fail_num = 0;
	u8 temp_val;
	u8 reg_cntrl;
	u8 prox_int_thresh[4];

	u8 temp;

	for ( num_try_init=0; num_try_init<3 ; num_try_init ++)
	{
		fail_num = 0;
	
		temp_val = 	CNTL_REG_CLEAR;
		if ((ret = opt_i2c_write((CMD_REG|CNTRL),&temp_val))< 0)
		{
			gprintk( "opt_i2c_write to clr ctrl reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}		

		temp_val = 0xf6; //working temp_val = 0xde;//temp_val = 100;
		if ((ret = opt_i2c_write((CMD_REG|ALS_TIME), &temp_val))< 0)
		{
			gprintk( "opt_i2c_write to als time reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}

		temp_val = 0xff; //Not working temp_val = 0xff; //temp_val = 0xee;
		if ((ret = opt_i2c_write((CMD_REG|PRX_TIME), &temp_val))< 0) 			
		{
			gprintk( "opt_i2c_write to prx time reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}

		temp_val =0xff; //temp_val =0xf2;
		if ((ret = opt_i2c_write((CMD_REG|WAIT_TIME), &temp_val)) < 0)			
		{
			gprintk( "opt_i2c_write to wait time reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}		

		temp_val = 0x33; // or 0x30
		if ((ret = opt_i2c_write((CMD_REG|INTERRUPT), &temp_val)) < 0)			
		{
			gprintk( "opt_i2c_write to interrupt reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}			

		temp_val = 0x0;
		if ((ret = opt_i2c_write((CMD_REG|PRX_CFG), &temp_val)) < 0)
		{
			gprintk( "opt_i2c_write to prox cfg reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}		
		
		temp_val =0x0a; 
		if ((ret = opt_i2c_write((CMD_REG|PRX_COUNT), &temp_val)) < 0)			
		{
			gprintk( "opt_i2c_write to prox cnt reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}
				
		temp_val =0x21;
		if ((ret = opt_i2c_write((CMD_REG|GAIN), &temp_val)) < 0)			
		{
			gprintk( "opt_i2c_write to prox gain reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}

		/* Setting for proximity interrupt */			
		prox_int_thresh[0] = (taos_prx_thres_lo) 		& 0xff;
		prox_int_thresh[1] = (taos_prx_thres_lo >> 8) 	& 0xff;
		prox_int_thresh[2] = (taos_prx_thres_hi)	 	& 0xff;
		prox_int_thresh[3] = (taos_prx_thres_hi >> 8) 	& 0xff;
		
		for (i = 0; i < 4; i++) 
		{
			ret = opt_i2c_write((CMD_REG|(PRX_MINTHRESHLO + i)), &prox_int_thresh[i]);
			if ( ret < 0)
			{
				gprintk( "opt_i2c_write failed in taos_chip_on, err = %d\n", ret);
				fail_num++; // 
			}
		}				
				
		reg_cntrl = CNTL_INTPROXPON_ENBL ; 
		if ((ret = opt_i2c_write((CMD_REG|CNTRL), &reg_cntrl))< 0)
		{
			gprintk( "opt_i2c_write to ctrl reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}
		mdelay(20); // more than 12 ms

		/* interrupt clearing */


		temp = (CMD_REG|CMD_SPL_FN|CMD_PROXALS_INTCLR);
		ret = opt_i2c_write_command(temp);
		if ( ret < 0)
		{
			gprintk( "opt_i2c_write failed in taos_chip_on, err = %d\n", ret);
			fail_num++; // 
		}
		

		if ( fail_num == 0) 
		{			
			/* interrupt enable */
			s3c_gpio_cfgpin(GPIO_TAOS_ALS, S3C_GPIO_SFN(GPIO_TAOS_ALS_AF));
			s3c_gpio_setpull(GPIO_TAOS_ALS, S3C_GPIO_PULL_UP);
			set_irq_type(GPIO_TAOS_ALS_INT, IRQ_TYPE_EDGE_FALLING);
			
			if ( taos_chip_status != TAOS_CHIP_WORKING)
			{
				enable_irq(GPIO_TAOS_ALS_INT);			
			}
			
			proximity_enable = 1;
			taos_chip_status = TAOS_CHIP_WORKING;
			
			break;	// Init Success 
		}
		else
		{
			printk( "I2C failed in taos_chip_on, # of trial=[%d], # of fail I2C=[%d]\n", num_try_init, fail_num);
		}

	}
			
	return (ret);

}


/*
 	 * PROX_ON - called to set the device in proximity sense mode. After
 	 * clearing device, als and proximity integration times are set, as
 	 * well as the wait time, interrupt filter, pulse count, and gain.
 	 * If in interrupt driven mode, interrupt thresholds are set. Then
 	 * the power, prox, (and interrupt, if needed) bits are enabled. If
 	 * the mode is polled, then the polling kernel timer is started.
*/
/*
 	 * ALS_OFF - called to stop ambient light sense mode of operation.
 	 * Clears the filter history, and clears the control register.
*/
static int taos_chip_off(void)
{
		int ret = 0;
		u8 reg_cntrl;

		gprintk("\n");

		#if 1 // working interrupt
		/* interrupt disable */
		disable_irq(GPIO_TAOS_ALS_INT);
		#endif

		reg_cntrl = CNTL_REG_CLEAR;
		
		if ((ret = opt_i2c_write((CMD_REG | CNTRL), &reg_cntrl))< 0)
		{
			gprintk( "opt_i2c_write to ctrl reg failed in taos_chip_off\n");
			return (ret);
		}
		
		proximity_enable = 0;
		taos_chip_status = TAOS_CHIP_SLEEP;
			
		return (ret);
}


/*
 * Reads and calculates current lux value. The raw ch0 and ch1 values of the
 * ambient light sensed in the last integration cycle are read from the device.
 * Time scale factor array values are adjusted based on the integration time.
 * The raw values are multiplied by a scale factor, and device gain is obtained
 * using gain index. Limit checks are done next, then the ratio of a multiple
 * of ch1 value, to the ch0 value, is calculated. The array triton_lux_datap
 * declared above is then scanned to find the first ratio value that is just
 * above the ratio we just calculated. The ch0 and ch1 multiplier constants in
 * the array are then used along with the time scale factor array values, to
 * calculate the milli-lux value, which is then scaled down to lux.
 */
static int taos_get_lux(void)
{
	int i=0;
	int ret = 0;
	u8 chdata[4];

	int irdata = 0;
	int cleardata = 0;	
	int ratio_for_lux = 0;
	int irfactor = 0;
	int calculated_lux = 0;
	
	int integration_time = 27; //working int integration_time = 100; // settingsettingsettingsettingsettingsettingsettingsettingsettingsettingsettingsetting
	int als_gain = 8			;// settingsettingsettingsettingsettingsettingsettingsettingsettingsettingsettingsetting

	for (i = 0; i < 4; i++) 
	{
		ret = opt_i2c_read( (ALS_CHAN0LO + i),&chdata[i] );
		if ( ret < 0 )
		{
			gprintk( "opt_i2c_read() to ALS_CHAN0LO + i regs failed in taos_get_lux()\n");
			return 0; //r0 return (ret);
		}
	}
	
	cleardata = 256 * chdata[1] + chdata[0];
	irdata = 256 * chdata[3] + chdata[2];
    // Check for valid data return and calculate ratio of channels
    if(cleardata > 0)
        ratio_for_lux = (irdata * 100)  /  cleardata;
    else
        ratio_for_lux = 106;	// if cleardata is less than or equal to 0 we must have an error or dark condition so exit with 106.
   
	//Determine light type and calculate LUX
    if(ratio_for_lux < 7){				// LED Light source
        calculated_lux =(( 245 * cleardata) - (50 * irdata))/100;
        ratio_for_lux = 101;
    }
    if(ratio_for_lux < 25){				// Flourscent
        calculated_lux =(( 240 * cleardata) - (64 * irdata))/100;
        ratio_for_lux = 102;
    }
    if(ratio_for_lux < 35){				// Outdoor lighting SUN
        calculated_lux =(( 334 * cleardata) - (123 * irdata))/100;
        ratio_for_lux = 103;
	}
    if(ratio_for_lux < 100){			//Incadesent Halogen etc.
	    if(cleardata > 16000)
		    calculated_lux = (162*irdata)/100;
	    else
		    calculated_lux = (( 51 * cleardata) - (57 * irdata))/100;
       ratio_for_lux = 104;
    }

    #if 1 // TAOS Debug
    int myratio = (irdata * 100)  /  cleardata;
    gprintk( "chdata[10] = ,%2x%2x, chdata[32]= ,%2x%2x,  cleardata=,%d, irdata=,%d, ratio_for_lux=,%d, myratio=%d, calculated_lux=,%d\n",chdata[1],chdata[0],chdata[3],chdata[2],cleardata,irdata,ratio_for_lux,myratio,calculated_lux);
    //looks like the gain register is not programed properly so adding a register dump here to view the contents of Gain Register.
    //int taos_register_dump(void);
    #endif

    // Check values for over and underflow
    if(calculated_lux > MAX_LUX) calculated_lux = MAX_LUX; // Case where lux is over the top limit which is 32768
    //if(calculated_lux < 0) calculated_lux = 0;           // Never lets lux go to zero or negative
	if(calculated_lux < 0) calculated_lux = 0;             // Never lets lux go negative

	
	return (calculated_lux);
}


/*
 * Proximity poll function - if valid data is available, read and form the ch0
 * and prox data values, check for limits on the ch0 value, and check the prox
 * data against the current thresholds, to set the event status accordingly.
 */
static int taos_prox_poll(struct taos_prox_info *prxp)
{
	int i;
	int ret = 0;
	int event = 0;
	
	u8 status;
	u8 chdata[6];

	//gprintk("\n");	
	
	if ((ret = opt_i2c_read( STATUS , &status))< 0)
	{
		gprintk( "opt_i2c_read() to STATUS regs failed in taos_prox_poll()\n");
		return (ret);
	}	

	for (i = 0; i < 6; i++) 
	{
		if ((ret = opt_i2c_read( (ALS_CHAN0LO + i),&chdata[i]))< 0)
		{
			gprintk( "opt_i2c_read() to (ALS_CHAN0LO + i) regs failed in taos_prox_poll()\n");
			return (ret);
		}
	}
	
	prxp->prox_data = chdata[5];
	prxp->prox_data <<= 8;
	prxp->prox_data |= chdata[4];

	gprintk("prxp->prox_data = [%d (0x%x)]\n", (int) prxp->prox_data, (int) prxp->prox_data);

	if ( prxp->prox_data > TAOS_PRX_THRES_IN)
	{
		prxp->prox_event = 1;
	}
	if ( prxp->prox_data < TAOS_PRX_THRES_OUT)
	{
		prxp->prox_event = 0;
	}		
	
	return (ret);
}

//NAGSM_Android_SEL_Kernel_Aakash_20100922

/*
 * Client probe function - When a valid device is found, the driver's device
 * data structure is updated, and initialization completes successfully.
 */
static int taos_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int irq;
	int ret;
		
	/* Allocate driver_data */
	taos = kzalloc(sizeof(struct taos_data),GFP_KERNEL);
	if(!taos)
	{
		pr_err("kzalloc error\n");
		return -ENOMEM;
	}	

	i2c_set_clientdata(client, taos);

	taos->client 	= client;	
	taos->client->flags = I2C_DF_NOTIFY | I2C_M_IGNORE_NAK;

	
	taos_wq = create_singlethread_workqueue("taos_wq");
    if (!taos_wq)
   	{
	    return -ENOMEM;
   	} 
	INIT_WORK(&taos->work_prox, taos_work_func_prox);	
   	INIT_WORK(&taos->work_light, taos_work_func_light);
	
	/* hrtimer Settings */
	hrtimer_init(&taos->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	taos->timer.function = taos_timer_func;

	printk("Workqueue Settings complete\n");
	printk("TAOS Probe Complete\n");


	ret = misc_register(&light_device);
	if(ret) 
	{
		pr_err(KERN_ERR "misc_register failed - light\n");
	}
	
	/* misc device Settings */
	ret = misc_register(&proximity_device);
	if(ret) 
	{
		pr_err(KERN_ERR "misc_register failed - prox \n");
	}
	gprintk("Misc device register completes. (light, proximity)\n");
	
	/* set platdata */
	//platform_set_drvdata(pdev, taos);

	/* set sysfs for light sensor */
	lightsensor_class = class_create(THIS_MODULE, "lightsensor");
	if (IS_ERR(lightsensor_class))
	{
		pr_err("Failed to create class(lightsensor)!\n");
	}

	switch_cmd_dev = device_create(lightsensor_class, NULL, 0, NULL, "switch_cmd");
	if (IS_ERR(switch_cmd_dev))
	{
		pr_err("Failed to create device(switch_cmd_dev)!\n");
	}

	if (device_create_file(switch_cmd_dev, &dev_attr_lightsensor_file_cmd) < 0)
	{
		pr_err("Failed to create device file(%s)!\n", dev_attr_lightsensor_file_cmd.attr.name);
	}

	if (device_create_file(switch_cmd_dev, &dev_attr_lightsensor_file_state) < 0)
	{
		pr_err("Failed to create device file(%s)!\n", dev_attr_lightsensor_file_state.attr.name);
	}
	dev_set_drvdata(switch_cmd_dev,taos);
	
	/* Input device Settings */
	if(USE_INPUT_DEVICE)
	{
		taos->input_dev = input_allocate_device();
		if (taos->input_dev == NULL) 
		{
			pr_err("Failed to allocate input device\n");
			return -ENOMEM;
		}
		
		taos->input_dev->name = "proximity";	
		set_bit(EV_SYN,taos->input_dev->evbit);
		set_bit(EV_ABS,taos->input_dev->evbit);		
        input_set_abs_params(taos->input_dev, ABS_DISTANCE, 0, 1, 0, 0);
			
		ret = input_register_device(taos->input_dev);
		if (ret) 
		{
			pr_err("Unable to register %s input device\n", taos->input_dev->name);
			input_free_device(taos->input_dev);
			kfree(taos);
			return -1;
		}
	}

	/* wake lock init */
	wake_lock_init(&prx_wake_lock, WAKE_LOCK_SUSPEND, "prx_wake_lock");


	/* interrupt hander register. */
	taos->irq = -1;
	irq = GPIO_TAOS_ALS_INT;	
	
	ret = request_irq(irq, taos_irq_handler, IRQF_DISABLED |  IRQF_NOBALANCING, "proximity_int", taos); 
	if (ret) 
	{
		printk("unable to request irq proximity_int err:: %d\n", ret);
		return ret;
	}		
	disable_irq(GPIO_TAOS_ALS_INT);
	taos->irq = irq;
	
	//printk("TAOS Probe is OK\n");

	return (ret);

}
//NAGSM_Android_SEL_Kernel_Aakash_20100922

static int taos_i2c_remove(struct i2c_client *client)
{
	struct taos_data *taos = i2c_get_clientdata(client);
	destroy_workqueue(taos_wq);
	kfree(taos);
	
	return 0;
}

//NAGSM_Android_SEL_Kernel_Aakash_20100922

static const struct i2c_device_id taos_i2c_id[]={
	{"taos", 0 },	//s3c_device_i2c11 mach-aries
	{}
};

MODULE_DEVICE_TABLE(i2c, ofm_i2c_id);

static struct i2c_driver taos_i2c_driver = {
	.driver	=	{
		.name	="taos",
		.owner	=THIS_MODULE,
	},
	.probe	=taos_i2c_probe,
	.remove	= taos_i2c_remove,
	.suspend = taos_opt_suspend,
	.resume  = taos_opt_resume,
	.id_table	=	taos_i2c_id,
};
//NAGSM_Android_SEL_Kernel_Aakash_20100922


static void taos_ldo_enable(void)
{	
 	//Turn ALS3.0V on
	Set_MAX8998_PM_OUTPUT_Voltage(LDO13, VCC_3p000);		

	Set_MAX8998_PM_REG(ELDO13, 1);
	max8998_ldo_enable_direct(MAX8998_LDO13);

	gprintk("[TAOS] Turn the LDO13 On\n");
}

static void taos_ldo_disable(void)
{	
 	//Turn ALS3.0V on
	Set_MAX8998_PM_REG(ELDO13, 0);

	gprintk("[TAOS] Turn the LDO13 Off\n");
}

/*
 * Driver initialization - device probe is initiated here, to identify
 * a valid device if present on any of the i2c buses and at any address.
 */
static int __init taos_init(void)
{
	int ret;
	
	taos_ldo_enable();

	/* set interrupt pin */	

	///ret = gpio_request(GPIO_PS_ON, "PS_ON");

	///if (ret) {
	///	printk(KERN_ERR "failed to request GPJ1(4) for "
	///		"LEDA_3.0V control\n");
	///	return;
	///}
	///gpio_direction_output(GPIO_PS_ON, 1);
	///gpio_set_value(GPIO_PS_ON, 1);
	///gpio_free(GPIO_PS_ON);

	///s3c_gpio_slp_cfgpin(GPIO_PS_ON, S3C_GPIO_SLP_PREV);

	//NAGSM_Android_SEL_Kernel_Aakash_20100922
	s3c_gpio_cfgpin(GPIO_TAOS_ALS, S3C_GPIO_INPUT);				
	s3c_gpio_setpull(GPIO_TAOS_ALS, S3C_GPIO_PULL_UP);	
	set_irq_type(GPIO_TAOS_ALS_INT, IRQ_TYPE_EDGE_FALLING);		
	//NAGSM_Android_SEL_Kernel_Aakash_20100922

	ret = i2c_add_driver(&taos_i2c_driver);
	
	return ret;
}

/*
 * Driver exit
 */
static void __exit taos_exit(void)
{
	i2c_del_driver(&taos_i2c_driver);
}


MODULE_AUTHOR("R&D Team, North America/GSM, Mobile communication division, SAMSUNG ELECTRONICS");
MODULE_DESCRIPTION("Samsung Customized TAOS Triton ambient light and proximity sensor driver");

late_initcall(taos_init);		//NAGSM_Android_SEL_Kernel_Aakash_20100922
module_exit(taos_exit);


#else /* defined(CONFIG_S5PC110_HAWK_BOARD) */

/*******************************************************************************

	 Author :	 	R&D Team, North America/GSM, 
				Mobile communication division, 
				SAMSUNG ELECTRONICS
				
	 Desription :	Samsung Customized TAOS Triton ambient light 
				and proximity sensor driver
	
  *******************************************************************************/

/* Includes */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/timer.h>
#include <asm/uaccess.h>
#include <asm/errno.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/gpio.h>
#include <linux/i2c/taos_triton.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <mach/hardware.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/regulator/max8998.h>
#include <mach/max8998_function.h>
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
#include <linux/regulator/bh6173.h>  
#endif 



//#define TAOS_DEBUG

/*********** for debug **********************************************************/
#ifdef TAOS_DEBUG
#define gprintk(fmt, x... ) printk( "%s(%d): " fmt, __FUNCTION__ ,__LINE__, ## x)
#else
#define gprintk(x...) do { } while (0)
#endif
/*******************************************************************************/

//defined in gpio-jupiter.h #define GPIO_TAOS_ALS 		S5PC11X_GPH1(2)
//defined in gpio-jupiter.h #define GPIO_TAOS_ALS_AF 	0xff

struct workqueue_struct *taos_wq;
static struct wake_lock prx_wake_lock;
static struct device *switch_cmd_dev;

static bool light_enable = 0;
static bool proximity_enable = 0;
int autobrightness_mode = OFF;
static state_type cur_state = LIGHT_INIT;

/* global var */
static struct class *lightsensor_class;
static struct i2c_client *opt_i2c_client = NULL;

static TAOS_ALS_FOPS_STATUS taos_als_status = TAOS_ALS_CLOSED;
static TAOS_PRX_FOPS_STATUS taos_prx_status = TAOS_PRX_CLOSED;
static TAOS_CHIP_WORKING_STATUS taos_chip_status = TAOS_CHIP_UNKNOWN;
static TAOS_PRX_DISTANCE_STATUS taos_prox_dist = TAOS_PRX_DIST_UNKNOWN;

#define TAOS_PRX_THRES 		12000		// IMPORTANT
static int taos_prx_thres_hi = TAOS_PRX_THRES;
static int taos_prx_thres_lo = 0; 

static int cur_adc_value = 0;
static short proximity_value = 0;
static bool lightsensor_test = 0;

static int buffering = 2;
extern int backlight_level;

/* Work queues */
//static struct work_struct taos_als_int_wq;
//static struct work_struct taos_prox_int_wq;

/* Als/prox info */
static struct taos_prox_info prox_cur_info;
static struct taos_prox_info *prox_cur_infop = &prox_cur_info;

/* Per-device data */
static struct taos_data {
	struct i2c_client 	*client;
	struct input_dev 	*input_dev;
	struct work_struct 	work_prox;  /* for proximity sensor */
	struct work_struct	work_light; /* for light_sensor     */
	int             	irq;
    struct hrtimer 		timer;
	struct timer_list 	light_init_timer;
};

//static struct device *devp;
static struct wake_lock prx_wake_lock;

static int taos_open_als(struct inode *inode, struct file *file);
static int taos_open_prox(struct inode *inode, struct file *file);
static int taos_release_als(struct inode *inode, struct file *file);
static int taos_release_prox(struct inode *inode, struct file *file);
#if 1 //B1
static int taos_read_als(struct file *file, double *buf, size_t count, loff_t *ppos);
#else
static int taos_read_als(struct file *file, int *buf, size_t count, loff_t *ppos);
#endif
static int taos_read_prox(struct file *file, char *buf, size_t count, loff_t *ppos);

static int taos_chip_on(void);
static int taos_chip_off(void);
static int taos_read_als_data(void);
static int taos_get_lux(void);
static int taos_prox_poll(struct taos_prox_info *prxp);
static int taos_register_dump(void);
static int taos_register_dump(void);

static void taos_on(struct taos_data *taos, int type);
static void taos_off(struct taos_data *taos, int type);
static void taos_ldo_enable(void);
static void taos_work_func_prox(struct work_struct *work);
static void taos_work_func_light(struct work_struct *work);
irqreturn_t taos_irq_handler(int irq, void *dev_id);
static enum hrtimer_restart taos_timer_func(struct hrtimer *timer);

static int opt_i2c_read(u8 reg, u8 *val, unsigned int len );
static int opt_i2c_write( u8 reg, u8 *val );
static int opt_taos_i2c_write_command( u8 reg);

short gp2a_get_proximity_value(void);
short gp2a_get_proximity_enable(void);
bool gp2a_get_lightsensor_status(void);
static double StateToLux(state_type state);

#define MDNIE_TUNINGMODE_FOR_BACKLIGHT

#ifdef MDNIE_TUNINGMODE_FOR_BACKLIGHT
int pre_val = -1;
extern int current_gamma_value;
extern u16 *pmDNIe_Gamma_set[];

typedef enum
{
	mDNIe_UI_MODE,
	mDNIe_VIDEO_MODE,
	mDNIe_VIDEO_WARM_MODE,
	mDNIe_VIDEO_COLD_MODE,
	mDNIe_CAMERA_MODE,
	mDNIe_NAVI
}Lcd_mDNIe_UI;

extern Lcd_mDNIe_UI current_mDNIe_UI;
extern void mDNIe_Mode_set_for_backlight(u16 *buf);

int value_buf[4] = {0};
int IsChangedADC(int val)
{
	int j = 0;
	int ret = 0;
	int adc_index = 0;
	static int adc_index_count = 0;

	adc_index = (adc_index_count)%4;		
	adc_index_count++;

	if(pre_val == -1) //ADC buffer initialize 
	{
		for(j = 0; j<4; j++)
			value_buf[j] = val;
		pre_val = 0;
	}
    else
    {
    	value_buf[adc_index] = val;
	}

	ret = ((value_buf[0] == value_buf[1])&& \
			(value_buf[1] == value_buf[2])&& \
			(value_buf[2] == value_buf[3]))? 1 : 0;
	
	if(adc_index_count == 4)
		adc_index_count = 0;
	return ret;
}
#endif


short gp2a_get_proximity_value(void)
{
	 printk("%s proximity_value = %d\n", __func__, proximity_value);
	  return proximity_value;
}

short gp2a_get_proximity_enable(void)
{
	  return proximity_enable;
}

bool gp2a_get_lightsensor_status(void)
{
	  return light_enable;
}

EXPORT_SYMBOL(gp2a_get_proximity_value);
EXPORT_SYMBOL(gp2a_get_proximity_enable);
EXPORT_SYMBOL(gp2a_get_lightsensor_status);

static int opt_i2c_read(u8 reg, u8 *val, unsigned int len )
{
	int err;
	u8 buf0[] = { 0 };
	u8 buf1[] = { 0xff };
	struct i2c_msg msg[2];

	#if 1 // E1
	if( (opt_i2c_client == NULL) || (!opt_i2c_client->adapter) )
	{
		pr_err("opt_i2c_read failed : i2c_client is NULL. (ALS hasn't initialized in a proper manner.)\n"); 
		return -ENODEV;
	}	
	#endif

	buf0[0] = reg;

	msg[0].addr = opt_i2c_client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;	
	msg[0].buf = buf0;

	msg[1].addr = opt_i2c_client->addr;
	msg[1].flags = I2C_M_RD;	
	msg[1].len = len;	
	msg[1].buf = buf1;

	err = i2c_transfer(opt_i2c_client->adapter, &msg[0], 1);
	err = i2c_transfer(opt_i2c_client->adapter, &msg[1], 1);
	
	*val = buf1[0];
    if (err >= 0) return 0;

    printk("%s %d i2c transfer error = %d\n", __func__, __LINE__, err);
    return err;
}


static int opt_i2c_write( u8 reg, u8 *val )
{
    int err;
    struct i2c_msg msg[1];
    unsigned char data[2];
    int retry = 10;

    if( (opt_i2c_client == NULL) || (!opt_i2c_client->adapter) )
	{
        return -ENODEV;
    }

    while(retry--)
    {
	    data[0] = reg;
	    data[1] = *val;

	    msg->addr = opt_i2c_client->addr;
	    msg->flags = I2C_M_WR;
	    msg->len = 2;
	    msg->buf = data;

	    err = i2c_transfer(opt_i2c_client->adapter, msg, 1);

	    if (err >= 0) 
		{
			return 0;
		}
    }

    printk("%s %d i2c transfer error = %d\n", __func__, __LINE__, err);
    return err;
}

static int opt_taos_i2c_write_command( u8 reg)
{
    int err;
    struct i2c_msg msg[1];
    unsigned char data[1]; //unsigned char data[2];
    int retry = 10;

    if( (opt_i2c_client == NULL) || (!opt_i2c_client->adapter) )
	{
        return -ENODEV;
    }

    while(retry--)
    {
	    data[0] = reg;
	    //data[1] = *val;

	    msg->addr = opt_i2c_client->addr;
	    msg->flags = I2C_M_WR;
	    msg->len = 1; //msg->len = 2;
	    msg->buf = data;

	    err = i2c_transfer(opt_i2c_client->adapter, msg, 1);

	    if (err >= 0) 
		{
			return 0;
		}
    }

    printk("%s %d i2c transfer error\n", __func__, __LINE__);
    return err;
}

static double StateToLux(state_type state)
{
	double lux = 0;

	gprintk("[%s] cur_state:%d\n",__func__,state);
		
	if(state== LIGHT_LEVEL4){
		lux = 15000.0;
	}
	else if(state == LIGHT_LEVEL3){
		lux = 9000.0;
	}
	else if(state== LIGHT_LEVEL2){
		lux = 5000.0;
	}
	else if(state == LIGHT_LEVEL1){
		lux = 6.0;
	}
	else {
		lux = 5000.0;
		gprintk("[%s] cur_state fail\n",__func__);
	}
	return lux;
}

static ssize_t lightsensor_file_state_show(struct device *dev, struct device_attribute *attr, char *buf)	
{
	int adc = 0;
	int sum = 0; 	// Temp
	int i = 0;		// Temp
	
	gprintk("called %s \n",__func__);

	if(!light_enable)
	{
		#if 1 //low-pass filter
		for(i = 0; i < 10; i++)
		{
			adc = taos_read_als_data(); //lightsensor_get_adcvalue();
			msleep(20);
			sum += adc;
		}
		adc = sum/10;
		#else
		adc = taos_read_als_data(); 
		#endif		
		gprintk("called %s	- subdued alarm(adc : %d)\n",__func__,adc);
		return sprintf(buf,"%d\n",adc);
	}
	else
	{
		gprintk("called %s	- *#0589#\n",__func__);
	#if 1
	return sprintf(buf,"%d\n",cur_adc_value);
	#else 
		adc = s3c_adc_get_adc_data(ADC_CHANNEL);
		return sprintf(buf,"%d\n",adc);
	#endif
	}
}

static ssize_t lightsensor_file_state_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)		
{	
	int value;
	sscanf(buf, "%d", &value);
	
	gprintk("called %s \n",__func__);

	if(value==1)
	{
		autobrightness_mode = ON;
		printk(KERN_DEBUG "[brightness_mode] BRIGHTNESS_MODE_SENSOR\n");
	}
	else if(value==0) 
	{
		autobrightness_mode = OFF;
		printk(KERN_DEBUG "[brightness_mode] BRIGHTNESS_MODE_USER\n");
		
		#ifdef MDNIE_TUNINGMODE_FOR_BACKLIGHT
		if(pre_val==1)
		{
			mDNIe_Mode_set_for_backlight(pmDNIe_Gamma_set[2]);
		}	
		pre_val = -1;
		#endif
	}

	return size;
}

/* for light sensor on/off control from platform */
static ssize_t lightsensor_file_cmd_show(struct device *dev, struct device_attribute *attr, char *buf)				
{
	gprintk("called %s \n",__func__);
	
	return sprintf(buf,"%u\n",light_enable);
}

static ssize_t lightsensor_file_cmd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)	
{
	struct taos_data *taos = dev_get_drvdata(dev);
	int value;
    sscanf(buf, "%d", &value);

	gprintk("called %s \n",__func__);
	gprintk( "val === %d", &value);
	printk(KERN_INFO "[LIGHT_SENSOR] in lightsensor_file_cmd_store, input value = %d \n",value);

	if(value==1)
	{
		//gp2a_on(gp2a,LIGHT);	Light sensor is always on
		lightsensor_test = 1;
		value = ON;
		printk(KERN_INFO "[LIGHT_SENSOR] *#0589# test start, input value = %d \n",value);
	}
	else if(value==0) 
	{
		//gp2a_off(gp2a,LIGHT);	Light sensor is always on
		lightsensor_test = 0;
		value = OFF;
		printk(KERN_INFO "[LIGHT_SENSOR] *#0589# test end, input value = %d \n",value);
	}
	/* temporary test code for proximity sensor */
	else if(value==3 && proximity_enable == OFF)
	{
		taos_on(taos,PROXIMITY);
		printk("[PROXIMITY] Temporary : Power ON\n");
	}
	else if(value==2 && proximity_enable == ON)
	{
		taos_off(taos,PROXIMITY);
		printk("[PROXIMITY] Temporary : Power OFF\n");
	}
	/* for factory simple test mode */
	if(value==7 && light_enable == OFF)
	{
	//	light_init_period = 2;
		taos_on(taos,LIGHT);
		value = 7;
	}

	return size;
}

/* for light sensor on/off control from platform */
static ssize_t proximity_file_state_show(struct device *dev, struct device_attribute *attr, char *buf)				
{
	int ret = 0;		
	char value = 1;	
	
	gprintk("called %s \n",__func__);	

	if( taos_als_status == TAOS_ALS_CLOSED )
	{
		taos_chip_on();
	}
	
	taos_prx_status = TAOS_PRX_OPENED;	

	ret = taos_prox_poll(prox_cur_infop);
	
	if(prox_cur_infop->prox_event)
	{
		value = 0;
	}	
	
	gprintk("proximity_file_state_show = [%d]\n ", value);	
	
	return sprintf(buf,"%u\n", value);
}

static DEVICE_ATTR(proximity_file_state,0666, proximity_file_state_show, NULL);
static DEVICE_ATTR(lightsensor_file_cmd,0666, lightsensor_file_cmd_show, lightsensor_file_cmd_store);
static DEVICE_ATTR(lightsensor_file_state,0666, lightsensor_file_state_show, lightsensor_file_state_store);

static int taos_register_dump(void)
{
	int i=0; 
	int ret = 0;
	u8 chdata[0x20];
	
	for (i = 0; i < 0x20; i++) 
	{
		if ((ret = opt_i2c_read((CMD_REG |(CNTRL + i)),&chdata[i],1))< 0)
		{
			gprintk("i2c_smbus_read_byte() to chan0/1/lo/hi"" regs failed in taos_register_dump()\n"); //dev_err(devp, "");			
			return (ret);
		}
	}
	
	printk("** taos register dump *** \n");
	for (i = 0; i < 0x20; i++) 
	{
		printk("(%x)0x%02x, ", i, chdata[i]);
	}
	printk("\n");	
	
	return 0;
}

static void taos_work_func_prox(struct work_struct *work)
{
	int i=0;
	int ret= 0;
	u8 prox_int_thresh[4];

	gprintk("\n");

	/* read proximity value. */
	if ((ret = taos_prox_poll(prox_cur_infop)) < 0) 
	{
		gprintk( "call to prox_poll() failed in taos_prox_poll_timer_func()\n");
		return;
	}

	switch ( taos_prox_dist ) /* HERE : taos_prox_dist => PREVIOUS STATUS */
	{
		case TAOS_PRX_DIST_IN:
			
			gprintk("TAOS_PRX_DIST_IN  ==> OUT \n");

			/* Condition for detecting IN */
			prox_int_thresh[0] = (0x0) 			& 0xff;
			prox_int_thresh[1] = (0x0 >> 8) 		& 0xff;
			prox_int_thresh[2] = (TAOS_PRX_THRES)	 	& 0xff;
			prox_int_thresh[3] = (TAOS_PRX_THRES >> 8) 	& 0xff;

			/* Current status */
			taos_prox_dist = TAOS_PRX_DIST_OUT;	  /* HERE : taos_prox_dist => CURRENT STATUS */
			break;
			
		case TAOS_PRX_DIST_OUT:
		case TAOS_PRX_DIST_UNKNOWN:
		default:
			
			gprintk("TAOS_PRX_DIST_OUT ==> IN \n");
			
			/* Condition for detecting OUT */
			prox_int_thresh[0] = (TAOS_PRX_THRES) 			& 0xff;
			prox_int_thresh[1] = (TAOS_PRX_THRES >> 8) 		& 0xff;
			prox_int_thresh[2] = (0x0)	 					& 0xff;
			prox_int_thresh[3] = (0x0 >> 8) 				& 0xff;		
			
			/* Current status */
			taos_prox_dist = TAOS_PRX_DIST_IN;	 /* HERE : taos_prox_dist => CURRENT STATUS */
			break;
	}
	
	for (i = 0; i < 4; i++) 
	{
		ret = opt_i2c_write((CMD_REG|(PRX_MINTHRESHLO + i)), &prox_int_thresh[i]);
		if ( ret < 0)
		{
			gprintk( "i2c_smbus_write_byte_data failed in taos_chip_on, err = %d\n", ret);
		}
	}	
	
		/* Interrupt clearing */
	ret = opt_taos_i2c_write_command((CMD_REG|CMD_SPL_FN|CMD_PROXALS_INTCLR));
	if ( ret < 0)
	{
		gprintk( "i2c_smbus_write_byte_data failed in taos_irq_handler(), err = %d\n", ret);
	}	

	return;
}

static void taos_work_func_light(struct work_struct *work)
{
	int adc=0;
	state_type level_state = LIGHT_INIT;	

	//read value 	
	adc = taos_read_als_data();	
	
	#if 1 //B1
	gprintk("Optimized adc = %d \n",adc);
	gprintk("cur_state = %d\n",cur_state);
	gprintk("light_enable = %d\n",light_enable);
	#else
	gprintk("taos_work_func_light called adc=[%d]\n", adc);
	#endif
	
	// check level and set bright_level
	if(adc >= 2100)
	{
		level_state = LIGHT_LEVEL4;
		buffering = 4;
	}

	else if(adc >= 1900 && adc < 2100)
	{
		if(buffering == 4)
		{	
			level_state = LIGHT_LEVEL4;
			buffering = 4;
		}
		else if((buffering == 1)||(buffering == 2)||(buffering == 3))
		{
			level_state = LIGHT_LEVEL3;
			buffering = 3;
		}
	}
	
	else if(adc >= 1800 && adc < 1900)
	{
		level_state = LIGHT_LEVEL3;
	}

	else if(adc >= 1200 && adc < 1800)
	{
		if((buffering == 3)||(buffering == 4))
		{	
			level_state = LIGHT_LEVEL3;
			buffering = 3;
		}
		else if((buffering == 1)||(buffering == 2))
		{
			level_state = LIGHT_LEVEL2;
			buffering = 2;
		}
	}

	else if(adc >= 500 && adc < 1200)
	{
		level_state = LIGHT_LEVEL2;
		buffering = 2;
	}

	else if(adc >= 80 && adc < 500)
	{
		if((buffering == 2)||(buffering == 3)||(buffering == 4))
		{	
			level_state = LIGHT_LEVEL2;
			buffering = 2;
		}
		else if(buffering == 1)
		{
			level_state = LIGHT_LEVEL1;
			buffering = 1;
		}
	}
	
	else if(adc < 80)
	{
		level_state = LIGHT_LEVEL1;
		buffering = 1;
	}

	if(!lightsensor_test)
	{
		gprintk("backlight_level = %d\n", backlight_level); //Temp
		cur_state = level_state;	
	}

	//brightness setting 
	#ifdef MDNIE_TUNINGMODE_FOR_BACKLIGHT
		if(autobrightness_mode)
		{
			if((pre_val!=1)&&(current_gamma_value == 24)&&(level_state == LIGHT_LEVEL4)&&(current_mDNIe_UI == mDNIe_UI_MODE))
			{
				mDNIe_Mode_set_for_backlight(pmDNIe_Gamma_set[1]);
				pre_val = 1;
				gprintk("mDNIe_Mode_set_for_backlight - pmDNIe_Gamma_set[1]\n" );
			}
		}
	#endif

	//gprintk("cur_state =[%d], lightsensor_test=[%d] \n",cur_state ,lightsensor_test); //Temp

	return;
}

static enum hrtimer_restart taos_timer_func(struct hrtimer *timer)
{
	ktime_t light_polling_time;		
	struct taos_data *taos = container_of(timer, struct taos_data, timer);
			
	queue_work(taos_wq, &taos->work_light);
	light_polling_time = ktime_set(0,0);
	light_polling_time = ktime_add_us(light_polling_time,500000);
	hrtimer_start(&taos->timer,light_polling_time,HRTIMER_MODE_REL);
	
	return HRTIMER_NORESTART;

}

/*****************************************************************************************
 *  
 *  function    : taos_on 
 *  description : This function is power-on function for optical sensor.
 *
 *  int type    : Sensor type. Two values is available (PROXIMITY,LIGHT).
 *                it support power-on function separately.
 *                
 *                 
 */
static void taos_on(struct taos_data *taos, int type)
{
	ktime_t light_polling_time;
	gprintk(KERN_INFO "[OPTICAL] taos_on(%d)\n",type);
	
	if(type == PROXIMITY)
	{
		#if 0
		gprintk("[PROXIMITY] go nomal mode : power on \n");
		value = 0x18;
		opt_i2c_write((u8)(REGS_CON),&value);

		value = 0x40;
		opt_i2c_write((u8)(REGS_HYS),&value);

		value = 0x01;
		opt_i2c_write((u8)(REGS_OPMOD),&value);
		
		gprintk("enable irq for proximity\n");
		enable_irq(gp2a ->irq);

		value = 0x00;
		opt_i2c_write((u8)(REGS_CON),&value);
		
		value = 0x01;
		opt_i2c_write((u8)(REGS_OPMOD),&value);
		proximity_enable =1;
		#endif	
	}
	if(type == LIGHT)
	{
		gprintk(KERN_INFO "[LIGHT_SENSOR] timer start for light sensor\n");
		light_polling_time = ktime_set(0,0);
		light_polling_time = ktime_add_us(light_polling_time,500000);

	    hrtimer_start(&taos->timer,light_polling_time,HRTIMER_MODE_REL);
		light_enable = 1;
	}
}

/*****************************************************************************************
 *  
 *  function    : taos_off 
 *  description : This function is power-off function for optical sensor.
 *
 *  int type    : Sensor type. Three values is available (PROXIMITY,LIGHT,ALL).
 *                it support power-on function separately.
 *                
 *                 
 */

static void taos_off(struct taos_data *taos, int type)
{
	gprintk(KERN_INFO "[OPTICAL] taos_off(%d)\n",type);
	if(type == PROXIMITY || type == ALL)
	{
		#if 0
		gprintk("[PROXIMITY] go power down mode  \n");
		
		//gprintk("disable irq for proximity \n");
		//disable_irq(gp2a ->irq);
		
		value = 0x00;
		opt_i2c_write((u8)(REGS_OPMOD),&value);
		
		proximity_enable =0;
		proximity_value = 0;		
		#endif
	}

	if(type ==LIGHT)
	{
		gprintk("[LIGHT_SENSOR] timer cancel for light sensor\n");
		hrtimer_cancel(&taos->timer);
		light_enable = 0;
	}
}


irqreturn_t taos_irq_handler(int irq, void *dev_id)
{
	struct taos_data *taos = dev_id;

	printk(KERN_DEBUG "taos_irq_handler() is called\n");

	queue_work(taos_wq, &taos->work_prox);	
	wake_lock_timeout(&prx_wake_lock, 5*HZ);
	
	return IRQ_HANDLED;
}

static void taos_ldo_enable(void)
{	
 	//Turn ALS3.0V on

#if 0
	Set_MAX8998_PM_OUTPUT_Voltage(LDO13, VCC_3p000);		

	Set_MAX8998_PM_REG(ELDO13, 1);
	max8998_ldo_enable_direct(MAX8998_LDO13);
#endif 
}

/*
 * Device open function - if the driver is loaded in interrupt mode, a valid
 * irq line is expected, and probed for, by requesting, and testing, all the
 * available irqs on the system using a probe interrupt handler. If found, the
 * irq is requested again, for subsequent use/handling by the run-time handler.
 */
static int taos_open_als(struct inode *inode, struct file *file)
{
	int ret = 0;		
	struct taos_data *taos = i2c_get_clientdata(opt_i2c_client);

//	device_released = 0;

	gprintk("[%s] \n",__func__);
	
	if( taos_prx_status == TAOS_PRX_CLOSED )
	{
		taos_chip_on();
	}
	
	taos_als_status = TAOS_ALS_OPENED;

	#if 1 // B1
	taos_on(taos, LIGHT);
	#endif

	return (ret);
}

/*
 * Device open function - if the driver is loaded in interrupt mode, a valid
 * irq line is expected, and probed for, by requesting, and testing, all the
 * available irqs on the system using a probe interrupt handler. If found, the
 * irq is requested again, for subsequent use/handling by the run-time handler.
 */
static int taos_open_prox(struct inode *inode, struct file *file)
{
	int ret = 0;	
	struct taos_data *taos = i2c_get_clientdata(opt_i2c_client);
	
//	device_released = 0;
	gprintk("[%s] \n",__func__);

	if( taos_als_status == TAOS_ALS_CLOSED )
	{
		taos_chip_on();
	}
	
	taos_prx_status = TAOS_PRX_OPENED;

	#if 0 // B1
	taos_on(taos, LIGHT);
	#endif

	return (ret);
}


/*
 * Device release
 */
static int taos_release_als(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct taos_data *taos = i2c_get_clientdata(opt_i2c_client);
	
	gprintk("%s",__func__);
	
	taos_off(taos,LIGHT);	

	if( taos_prx_status == TAOS_PRX_CLOSED )
	{
		taos_chip_off();
	}
	taos_als_status = TAOS_ALS_CLOSED;

	return (ret);
}

/*
 * Device release
 */
static int taos_release_prox(struct inode *inode, struct file *file)
{
	int ret = 0;
//	device_released = 1;
//	prox_on = 0;
	
	gprintk("%s",__func__);

	if( taos_als_status == TAOS_ALS_CLOSED )
	{
		taos_chip_off();
	}
	taos_prx_status = TAOS_PRX_CLOSED;

	return (ret);
}


/*
 	 * ALS_DATA - request for current ambient light data. If correctly
 	 * enabled and valid data is available at the device, the function
 	 * for lux conversion is called, and the result returned if valid.
*/
#define	ADC_BUFFER_NUM			8
static int adc_value_buf[ADC_BUFFER_NUM] = {0, 0, 0, 0, 0, 0};

static int taos_read_als_data(void)
{
		int ret = 0;
		int lux_val=0;
		
		u8 reg_val;
#if 1 // D1 -average filter	
		int i=0;
		int j=0;
		unsigned int adc_total = 0;
		static int adc_avr_value = 0;
		unsigned int adc_index = 0;
		static unsigned int adc_index_count = 0;
		unsigned int adc_max = 0;
		unsigned int adc_min = 0;
#endif
	
		if ((ret = opt_i2c_read((CMD_REG | CNTRL), &reg_val,1))< 0)
		{
			gprintk( "opt_i2c_read to cmd reg failed in taos_read_als_data\n");			
			return 0; //r0 return (ret);
		}

		if ((reg_val & (CNTL_ADC_ENBL | CNTL_PWRON)) != (CNTL_ADC_ENBL | CNTL_PWRON))
		{
			return 0; //r0 return (-ENODATA);
		}

		if ((lux_val = taos_get_lux()) < 0)
		{
			gprintk( "taos_get_lux() returned error %d in taos_read_als_data\n", lux_val);
		}

		// gprintk("** Returned lux_val  = [%d]\n", lux_val);

		/* QUICK FIX : multiplu lux_val by 4 to make the value similar to ADC value. */
		lux_val = lux_val * 5;
		
		if (lux_val >= MAX_LUX)
		{
			lux_val = MAX_LUX; //return (MAX_LUX);
		}
		
		cur_adc_value = lux_val; 
		gprintk("taos_read_als_data cur_adc_value = %d (0x%x)\n", cur_adc_value, cur_adc_value);

#if 1 // D1 -average filter		
		adc_index = (adc_index_count++)%ADC_BUFFER_NUM;		

		if(cur_state == LIGHT_INIT) //ADC buffer initialize (light sensor off ---> light sensor on)
		{
			for(j = 0; j<ADC_BUFFER_NUM; j++)
				adc_value_buf[j] = lux_val; // value;
		}
		else
		{
			adc_value_buf[adc_index] = lux_val; //value;
		}
		
		adc_max = adc_value_buf[0];
		adc_min = adc_value_buf[0];

		for(i = 0; i <ADC_BUFFER_NUM; i++)
		{
			adc_total += adc_value_buf[i];

			if(adc_max < adc_value_buf[i])
				adc_max = adc_value_buf[i];
						
			if(adc_min > adc_value_buf[i])
				adc_min = adc_value_buf[i];
		}
		adc_avr_value = (adc_total-(adc_max+adc_min))/(ADC_BUFFER_NUM-2);
		
		if(adc_index_count == ADC_BUFFER_NUM-1)
			adc_index_count = 0;

		return adc_avr_value;
#else
		return (lux_val);
#endif

}


/*
 * Device read - reads are permitted only within the range of the accessible
 * registers of the device. The data read is copied safely to user space.
 */
#if 1 //B1
static int taos_read_als(struct file *file, double *buf, size_t count, loff_t *ppos)
#else
static int taos_read_als(struct file *file, int *buf, size_t count, loff_t *ppos)
#endif
{
	#if 1 //B1
	double lux_val = 0;
	#else
	int lux_val = 0;
	#endif
	
	//	gprintk("%s",__func__);

	#if 1 //B1
	lux_val = StateToLux(cur_state);
	#else
	lux_val = taos_read_als_data();
	#endif
	
	put_user(lux_val, buf);
	
	return (1);
}

/*
 * Device read - reads are permitted only within the range of the accessible
 * registers of the device. The data read is copied safely to user space.
*/
static int taos_read_prox(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	char value = 1;	

	ret = taos_prox_poll(prox_cur_infop);
	
	if(prox_cur_infop->prox_event)
	{
		value = 0;
	}
	
	proximity_value = value;
	
	put_user(value,buf);
	
	gprintk("taos_read_prox = [%d]\n ", value);

	return 1; 
}


/*
 	 * ALS_ON - called to set the device in ambient light sense mode.
 	 * configured values of light integration time, inititial interrupt
 	 * filter, gain, and interrupt thresholds (if interrupt driven) are
 	 * initialized. Then power, adc, (and interrupt if needed) are enabled.
*/

static int taos_chip_on(void)
{
#if 1
	int i = 0;
	int ret = 0;		
	int num_try_init = 0;
	int fail_num = 0;
	u8 temp_val;
	u8 reg_cntrl;
	u8 prox_int_thresh[4];

#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
	bh6173_ldo_enable_direct(3);
#endif

	for ( num_try_init=0; num_try_init<3 ; num_try_init ++)
	{
		fail_num = 0;
	
		temp_val = 	CNTL_REG_CLEAR;
		if ((ret = opt_i2c_write((CMD_REG|CNTRL),&temp_val))< 0)
		{
			gprintk( "opt_i2c_write to clr ctrl reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}		

		temp_val = 0xf6; //working temp_val = 0xde;//temp_val = 100;
		if ((ret = opt_i2c_write((CMD_REG|ALS_TIME), &temp_val))< 0)
		{
			gprintk( "opt_i2c_write to als time reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}

		temp_val = 0xee; //Not working temp_val = 0xff; //temp_val = 0xee;
		if ((ret = opt_i2c_write((CMD_REG|PRX_TIME), &temp_val))< 0) 			
		{
			gprintk( "opt_i2c_write to prx time reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}

		temp_val =0xff; //temp_val =0xf2;
		if ((ret = opt_i2c_write((CMD_REG|WAIT_TIME), &temp_val)) < 0)			
		{
			gprintk( "opt_i2c_write to wait time reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}		

		temp_val = 0x33; // or 0x30
		if ((ret = opt_i2c_write((CMD_REG|INTERRUPT), &temp_val)) < 0)			
		{
			gprintk( "opt_i2c_write to interrupt reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}			

		temp_val = 0x0;
		if ((ret = opt_i2c_write((CMD_REG|PRX_CFG), &temp_val)) < 0)
		{
			gprintk( "opt_i2c_write to prox cfg reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}		
		
		temp_val =0x0a; 
		if ((ret = opt_i2c_write((CMD_REG|PRX_COUNT), &temp_val)) < 0)			
		{
			gprintk( "opt_i2c_write to prox cnt reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}
				
		temp_val =0x21;
		if ((ret = opt_i2c_write((CMD_REG|GAIN), &temp_val)) < 0)			
		{
			gprintk( "opt_i2c_write to prox gain reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}

		/* Setting for proximity interrupt. first Out->In case interrupt setting. */			
		prox_int_thresh[0] = (taos_prx_thres_lo) 		& 0xff;
		prox_int_thresh[1] = (taos_prx_thres_lo >> 8) 	& 0xff;
		prox_int_thresh[2] = (taos_prx_thres_hi)	 	& 0xff;
		prox_int_thresh[3] = (taos_prx_thres_hi >> 8) 	& 0xff;
		
		for (i = 0; i < 4; i++) 
		{
			ret = opt_i2c_write((CMD_REG|(PRX_MINTHRESHLO + i)), &prox_int_thresh[i]);
			if ( ret < 0)
			{
				gprintk( "opt_i2c_write failed in taos_chip_on, err = %d\n", ret);
				fail_num++; // 
			}
		}				
				
		reg_cntrl = CNTL_INTPROXPON_ENBL ; 
		if ((ret = opt_i2c_write((CMD_REG|CNTRL), &reg_cntrl))< 0)
		{
			gprintk( "opt_i2c_write to ctrl reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}
		mdelay(20); // more than 12 ms

		/* interrupt clearing */
		ret = opt_taos_i2c_write_command((CMD_REG|CMD_SPL_FN|CMD_PROXALS_INTCLR));
		if ( ret < 0)
		{
			gprintk( "opt_taos_i2c_write_command failed in taos_chip_on, err = %d\n", ret);
			fail_num++; // 
		}
		
		#if 1 //C1		
		if ( fail_num == 0) 
		{			
			/* interrupt enable */
			enable_irq(GPIO_PS_ALS_INT_IRQ);			
			proximity_enable = 1;
			taos_chip_status = TAOS_CHIP_WORKING;
			
			break;	// Init Success 
		}
		else
			printk( "I2C failed in taos_chip_on, # of trial=[%d], # of fail I2C=[%d]\n", num_try_init, fail_num);
		#else		
		/* interrupt enable */
		enable_irq(GPIO_PS_ALS_INT_IRQ);	

		proximity_enable = 1;
		taos_chip_status = TAOS_CHIP_WORKING;

		if ( fail_num == 0) 
			break;	// Init Success 
		else
			printk( "I2C failed in taos_chip_on, # of trial=[%d], # of fail I2C=[%d]\n", num_try_init, fail_num);
		#endif
	}
			
	return (ret);

#else
	int i=0;
	int ret = 0;		
	u8 temp_val;
	u8 reg_cntrl;
	u8 prox_int_thresh[4];
	
	temp_val = 	CNTL_REG_CLEAR;
	if ((ret = opt_i2c_write((CMD_REG|CNTRL),&temp_val))< 0)
	{
		gprintk( "opt_i2c_write to clr ctrl reg failed in taos_chip_on.\n");
		return (ret);
	}		

	temp_val = 0xf6; //working temp_val = 0xde;//temp_val = 100;
	if ((ret = opt_i2c_write((CMD_REG|ALS_TIME), &temp_val))< 0)
	{
		gprintk( "opt_i2c_write to als time reg failed in taos_chip_on.\n");
		return (ret);
	}

	temp_val = 0xee; //Not working temp_val = 0xff; //temp_val = 0xee;
	if ((ret = opt_i2c_write((CMD_REG|PRX_TIME), &temp_val))< 0) 			
	{
		gprintk( "opt_i2c_write to prx time reg failed in taos_chip_on.\n");
		return (ret);
	}

	temp_val =0xff; //temp_val =0xf2;
	if ((ret = opt_i2c_write((CMD_REG|WAIT_TIME), &temp_val)) < 0)			
	{
		gprintk( "opt_i2c_write to wait time reg failed in taos_chip_on.\n");
		return (ret);
	}		

	temp_val = 0x33; // or 0x30
	if ((ret = opt_i2c_write((CMD_REG|INTERRUPT), &temp_val)) < 0)			
	{
		gprintk( "opt_i2c_write to interrupt reg failed in taos_chip_on.\n");
		return (ret);
	}			

	temp_val = 0x0;
	if ((ret = opt_i2c_write((CMD_REG|PRX_CFG), &temp_val)) < 0)
	{
		gprintk( "opt_i2c_write to prox cfg reg failed in taos_chip_on.\n");
		return (ret);
	}		
	
	temp_val =0x0a; 
	if ((ret = opt_i2c_write((CMD_REG|PRX_COUNT), &temp_val)) < 0)			
	{
		gprintk( "opt_i2c_write to prox cnt reg failed in taos_chip_on.\n");
		return (ret);
	}
			
	temp_val =0x21;
	if ((ret = opt_i2c_write((CMD_REG|GAIN), &temp_val)) < 0)			
	{
		gprintk( "opt_i2c_write to prox gain reg failed in taos_chip_on.\n");
		return (ret);
	}

	/* Setting for proximity interrupt */			
	prox_int_thresh[0] = (taos_prx_thres_lo) 		& 0xff;
	prox_int_thresh[1] = (taos_prx_thres_lo >> 8) 	& 0xff;
	prox_int_thresh[2] = (taos_prx_thres_hi)	 	& 0xff;
	prox_int_thresh[3] = (taos_prx_thres_hi >> 8) 	& 0xff;
	
	for (i = 0; i < 4; i++) 
	{
		ret = opt_i2c_write((CMD_REG|(PRX_MINTHRESHLO + i)), &prox_int_thresh[i]);
		if ( ret < 0)
		{
			gprintk( "opt_i2c_write failed in taos_chip_on, err = %d\n", ret);
		}
	}				
			
	reg_cntrl = CNTL_INTPROXPON_ENBL ; 
	if ((ret = opt_i2c_write((CMD_REG|CNTRL), &reg_cntrl))< 0)
	{
		gprintk( "opt_i2c_write to ctrl reg failed in taos_chip_on.\n");
		return (ret);
	}
	mdelay(20); // more than 12 ms

	/* interrupt clearing */
	ret = opt_taos_i2c_write_command((CMD_REG|CMD_SPL_FN|CMD_PROXALS_INTCLR));
	if ( ret < 0)
	{
		gprintk( "opt_taos_i2c_write_command failed in taos_chip_on, err = %d\n", ret);
	}
	
	/* interrupt enable */
	enable_irq(GPIO_PS_ALS_INT_IRQ);	

	proximity_enable = 1;
	taos_chip_status = TAOS_CHIP_WORKING;
		
	return (ret);
#endif

}


/*
 	 * PROX_ON - called to set the device in proximity sense mode. After
 	 * clearing device, als and proximity integration times are set, as
 	 * well as the wait time, interrupt filter, pulse count, and gain.
 	 * If in interrupt driven mode, interrupt thresholds are set. Then
 	 * the power, prox, (and interrupt, if needed) bits are enabled. If
 	 * the mode is polled, then the polling kernel timer is started.
*/
/*
 	 * ALS_OFF - called to stop ambient light sense mode of operation.
 	 * Clears the filter history, and clears the control register.
*/
static int taos_chip_off(void)
{
		int ret = 0;
		u8 reg_cntrl;

		gprintk("\n");

		#if 1 // working interrupt
		/* interrupt disable */
		disable_irq(GPIO_PS_ALS_INT_IRQ);
		#endif

		reg_cntrl = CNTL_REG_CLEAR;
		
		if ((ret = opt_i2c_write((CMD_REG | CNTRL), &reg_cntrl))< 0)
		{
			gprintk( "opt_i2c_write to ctrl reg failed in taos_chip_off\n");
			return (ret);
		}
		
		proximity_enable = 0;
		taos_chip_status = TAOS_CHIP_SLEEP;
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD			
		bh6173_ldo_disable_direct(3);
#endif
			
		return (ret);
}


/*
 * Reads and calculates current lux value. The raw ch0 and ch1 values of the
 * ambient light sensed in the last integration cycle are read from the device.
 * Time scale factor array values are adjusted based on the integration time.
 * The raw values are multiplied by a scale factor, and device gain is obtained
 * using gain index. Limit checks are done next, then the ratio of a multiple
 * of ch1 value, to the ch0 value, is calculated. The array triton_lux_datap
 * declared above is then scanned to find the first ratio value that is just
 * above the ratio we just calculated. The ch0 and ch1 multiplier constants in
 * the array are then used along with the time scale factor array values, to
 * calculate the milli-lux value, which is then scaled down to lux.
 */
static int taos_get_lux(void)
{
	int i=0;
	int ret = 0;
	u8 chdata[4];

	int irdata = 0;
	int cleardata = 0;	
	int ratio_for_lux = 0;
	int irfactor = 0;
	int calculated_lux = 0;
	
	int integration_time = 27; //working int integration_time = 100; // settingsettingsettingsettingsettingsettingsettingsettingsettingsettingsettingsetting
	int als_gain = 8			;// settingsettingsettingsettingsettingsettingsettingsettingsettingsettingsettingsetting

	for (i = 0; i < 4; i++) 
	{
		if ((ret = opt_i2c_read((CMD_REG |(ALS_CHAN0LO + i)),&chdata[i],1))< 0)
		{
			gprintk( "opt_i2c_read() to (CMD_REG |(ALS_CHAN0LO + i) regs failed in taos_get_lux()\n");
			return 0; //r0 return (ret);
		}
	}
	
	cleardata = 256 * chdata[1] + chdata[0];
	irdata = 256 * chdata[3] + chdata[2];

	if ( cleardata != 0)
	{
		ratio_for_lux = (irdata*100) / cleardata;		

		//multiply coefficents by 1000 to irfactor
		if ( ratio_for_lux <30)
		{
			irfactor = (1000 * cleardata) - (1846 * irdata);
		}
		else if ( 30 <= ratio_for_lux && ratio_for_lux <38 )
		{
			irfactor = (1268 * cleardata) - (2740 * irdata);
		}
		else if ( 38 <= ratio_for_lux && ratio_for_lux <45 )
		{
			irfactor = (749 * cleardata) - (1374 * irdata);
		}
		else if ( 45 <= ratio_for_lux && ratio_for_lux <54 )
		{
			irfactor = (477 * cleardata) - (769 * irdata);
		}
		else
		{
			irfactor = 0;
		}
		
		calculated_lux = 1 * 52 * irfactor / ( integration_time * als_gain);

		/* QUICK FIX : Code for saturation case */
		//Saturation value seems to be affected by setting ALS_TIME, 0xfe (=246) which sets max count to 10240.
		if ( cleardata >= 10240 || irdata >= 10240)
		{
			calculated_lux = MAX_LUX * 1000; // Coefficient 1000 for milli lux 
		}		

		// gprintk("Calculated_lux=[%d], cleardata=[%d], irdata=[%d], ratio_for_lux=[%d], irfactor=[%d]\n", calculated_lux, cleardata, irdata, ratio_for_lux, irfactor);
			
		calculated_lux = calculated_lux / 1000;
	}
	else
	{
		calculated_lux = 0 ; // Exceptional case ?
	}
	
	return (calculated_lux);
}


/*
 * Proximity poll function - if valid data is available, read and form the ch0
 * and prox data values, check for limits on the ch0 value, and check the prox
 * data against the current thresholds, to set the event status accordingly.
 */
static int taos_prox_poll(struct taos_prox_info *prxp)
{
	int i;
	int ret = 0;
	int event = 0;
	
	u8 status;
	u8 chdata[2];

	//gprintk("\n");	
	
	if ((ret = opt_i2c_read((CMD_REG | STATUS), &status, 1)) < 0)
	{
		gprintk( "opt_i2c_read() to (CMD_REG | STATUS) regs failed in taos_prox_poll()\n");
		return (ret);
	}	

	// read prox data reg
	for (i = 0; i < 2; i++) 
	{
		if ((ret = opt_i2c_read((CMD_REG |(PRX_LO + i)), &chdata[i], 1))< 0)
		{
			gprintk( "opt_i2c_read() to (CMD_REG |(ALS_CHAN0LO + i) regs failed in taos_prox_poll()\n");
			return (ret);
		}
	}
	
	prxp->prox_data = chdata[1];
	prxp->prox_data <<= 8;
	prxp->prox_data |= chdata[0];

	gprintk("prxp->prox_data = [%d (0x%x)]\n", (int) prxp->prox_data, (int) prxp->prox_data);

	if ( prxp->prox_data > 12000)
	{
		event = 1;
	}
	else
	{
		event = 0;
	}
		
	prxp->prox_event = event;

	return (ret);
}

/* File operations */
static struct file_operations taos_fops_als = 
{
	.owner 		= THIS_MODULE,
	.open 		= taos_open_als,
	.release	= taos_release_als,
	.read 		= taos_read_als,
};

/* File operations */
static struct file_operations taos_fops_prox = 
{
	.owner 		= THIS_MODULE,
	.open 		= taos_open_prox,
	.release 	= taos_release_prox,
	.read 		= taos_read_prox,
};                

static struct miscdevice light_device = 
{
    .minor  = MISC_DYNAMIC_MINOR,
    .name   = "light",
    .fops   = &taos_fops_als,
};

static struct miscdevice proximity_device = 
{
    .minor  = MISC_DYNAMIC_MINOR,
    .name   = "proximity",
    .fops   = &taos_fops_prox,
};

/*
 * Client probe function - When a valid device is found, the driver's device
 * data structure is updated, and initialization completes successfully.
 */
static int taos_probe( struct i2c_client *client, const struct i2c_device_id *id)
{		
	int irq;
	int ret;
	struct taos_data *taos;
	char chip_id = -1;
	
	if(client == NULL)
	{
		pr_err("taos_probe failed : i2c_client is NULL\n"); 
		return -ENODEV;
	}	
	else
		printk(KERN_INFO "%s : client addr = %x\n", __func__, client->addr);
	
	if ( !i2c_check_functionality(client->adapter,I2C_FUNC_SMBUS_BYTE_DATA) ) {
		printk(KERN_INFO "byte op is not permited.\n");
		ret = -ENODEV;
		return ret;
	}

	printk(KERN_INFO "%s : i2c_check_functionality I2C_FUNC_I2C!!\n", __func__); 		
	
	/* Allocate driver_data */
	taos = kzalloc(sizeof(struct taos_data),GFP_KERNEL);
	if(!taos)
	{
		pr_err("kzalloc error\n");
		return -ENOMEM;
	}	
	
	opt_i2c_client = client;
	i2c_set_clientdata(client, taos);
	taos->client = client;
	
	// read chip ID
	if ((ret = opt_i2c_read((CMD_REG | CHIPID), &chip_id, 1))< 0)
	{
		gprintk( "%s : opt_i2c_read is failed to read device ID = %d\n", __func__, ret);			
		return ret; //r0 return (ret);
	}
	
	printk(KERN_INFO "%s : read chip ID successful. ID = %x\n", __func__, chip_id); 	

	/* hrtimer Settings */
	hrtimer_init(&taos->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	taos->timer.function = taos_timer_func;

	taos_wq = create_singlethread_workqueue("taos_wq");
    if (!taos_wq)
   	{
	    return -ENOMEM;
   	}  		

	INIT_WORK(&taos->work_prox, taos_work_func_prox);	
   	INIT_WORK(&taos->work_light, taos_work_func_light);
	gprintk("Workqueue Settings complete.\n");

	ret = misc_register(&light_device);
	if(ret) 
	{
		pr_err(KERN_ERR "misc_register failed - light\n");
	}
	
	/* misc device Settings */
	ret = misc_register(&proximity_device);
	if(ret) 
	{
		pr_err(KERN_ERR "misc_register failed - prox \n");
	}
	gprintk("Misc device register completes. (light, proximity)\n");	
	
	/* set sysfs for light sensor */
	lightsensor_class = class_create(THIS_MODULE, "lightsensor");
	if (IS_ERR(lightsensor_class))
	{
		pr_err("Failed to create class(lightsensor)!\n");
	}

	switch_cmd_dev = device_create(lightsensor_class, NULL, 0, NULL, "switch_cmd");
	if (IS_ERR(switch_cmd_dev))
	{
		pr_err("Failed to create device(switch_cmd_dev)!\n");
	}

	if (device_create_file(switch_cmd_dev, &dev_attr_lightsensor_file_cmd) < 0)
	{
		pr_err("Failed to create device file(%s)!\n", dev_attr_lightsensor_file_cmd.attr.name);
	}

	if (device_create_file(switch_cmd_dev, &dev_attr_lightsensor_file_state) < 0)
	{
		pr_err("Failed to create device file(%s)!\n", dev_attr_lightsensor_file_state.attr.name);
	}	
	
	if (device_create_file(switch_cmd_dev, &dev_attr_proximity_file_state) < 0)
	{
		pr_err("Failed to create device file(%s)!\n", dev_attr_proximity_file_state.attr.name);
	}		
	
	/* init i2c */	
	gprintk("i2c Setting completes.\n");

	/* Input device Settings */
	if(USE_INPUT_DEVICE)
	{
		taos->input_dev = input_allocate_device();
		if (taos->input_dev == NULL) 
		{
			pr_err("Failed to allocate input device\n");
			return -ENOMEM;
		}
		
		taos->input_dev->name = "proximity";	
		set_bit(EV_SYN,taos->input_dev->evbit);
		set_bit(EV_ABS,taos->input_dev->evbit);		
        input_set_abs_params(taos->input_dev, ABS_DISTANCE, 0, 1, 0, 0);
			
		ret = input_register_device(taos->input_dev);
		if (ret) 
		{
			pr_err("Unable to register %s input device\n", taos->input_dev->name);
			input_free_device(taos->input_dev);
			kfree(taos);
			return -1;
		}
	}

	/* wake lock init */
	wake_lock_init(&prx_wake_lock, WAKE_LOCK_SUSPEND, "prx_wake_lock");

	/* interrupt hander register. */
	taos->irq = -1;
	irq = GPIO_PS_ALS_INT_IRQ;	
	
	ret = request_irq(irq, taos_irq_handler, IRQF_DISABLED, "proximity_int", taos); // check and study here...
	if (ret) 
	{
		printk("unable to request irq proximity_int err:: %d\n", ret);
		return ret;
	}		
	disable_irq(irq);
	
	taos->irq = irq;
		
	gprintk("TAOS Probe is OK!!\n");
	
	return (ret);

}

static int __exit taos_remove(struct i2c_client *client)
{
	struct taos_data *taos = i2c_get_clientdata(client);
	
	printk("[%s]\n", __func__);	
	
    if (taos_wq)
		destroy_workqueue(taos_wq);

	if(USE_INPUT_DEVICE)
		input_unregister_device(taos->input_dev);	
	
	misc_deregister(&light_device);
	misc_deregister(&proximity_device);
	
	kfree(taos);
	kfree(client);
	taos = NULL;
	
	opt_i2c_client = NULL;
	
	return 0;
}

static int taos_opt_suspend( struct i2c_client *client, pm_message_t state )
{
	struct taos_data *taos = i2c_get_clientdata(client);
	
	if(light_enable)
	{
		gprintk("[%s] : hrtimer_cancle \n",__func__);
		hrtimer_cancel(&taos->timer);
	}

	gprintk("[%s] : \n",__func__);

	return 0;
}

static int taos_opt_resume( struct i2c_client *client )
{
	struct taos_data *taos = i2c_get_clientdata(client);
	
	gprintk("[%s] : \n",__func__);
	
	return 0;
}

static const struct i2c_device_id tmd2711_id[] = {
	{TAOS_DEVICE_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, tmd2711_id);

/* Driver definition */
static struct i2c_driver opt_i2c_driver = {
	.driver = {
		.name = TAOS_DEVICE_NAME,
	},
	.probe = taos_probe,
	.remove = __exit_p(taos_remove),
	.suspend = taos_opt_suspend,
	.resume = taos_opt_resume,
	.id_table = tmd2711_id,
};

/*
 * Driver initialization - device probe is initiated here, to identify
 * a valid device if present on any of the i2c buses and at any address.
 */
static int __init taos_init(void)
{
	int ret;
	
	gprintk("[%s]\n", __func__);
	//taos_ldo_enable();

	/* set interrupt pin */	
	s3c_gpio_cfgpin(GPIO_PS_ALS_INT, S3C_GPIO_SFN(GPIO_PS_ALS_INT_AF));
	s3c_gpio_setpull(GPIO_PS_ALS_INT, S3C_GPIO_PULL_UP);		
	set_irq_type(GPIO_PS_ALS_INT_IRQ, IRQ_TYPE_EDGE_FALLING);		
	
	return i2c_add_driver(&opt_i2c_driver);
}

/*
 * Driver exit
 */
static void __exit taos_exit(void)
{	
	gprintk("[%s]\n", __func__);
	i2c_del_driver(&opt_i2c_driver);
}

module_init(taos_init);
module_exit(taos_exit);

MODULE_AUTHOR("R&D Team, North America/GSM, Mobile communication division, SAMSUNG ELECTRONICS");
MODULE_DESCRIPTION("Samsung Customized TAOS Triton ambient light and proximity sensor driver");

#endif /* defined(CONFIG_S5PC110_HAWK_BOARD) */


