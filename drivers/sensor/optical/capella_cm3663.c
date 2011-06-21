

/*******************************************************************************

	 Author :	 	R&D Team, System S/W, 
				Mobile communication division, 
				SAMSUNG ELECTRONICS
				
	 Desription :	Samsung Customized capella light 
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
//#include <mach/regs-gpio.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/workqueue.h>
//#include <linux/regulator/max8998.h>
//#include <mach/max8998_function.h>
//#include <mach/gpio-demspey_setting.h>
//#include <linux/regulator/max8966.h>
#include <asm/uaccess.h>
#include "capella_cm3663.h"

#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
#include <linux/regulator/bh6173.h>  
#endif 


//#define capella_DEBUG

/*********** for debug **********************************************************/
#ifdef capella_DEBUG
#define gprintk(fmt, x... ) printk( "%s(%d): " fmt, __FUNCTION__ ,__LINE__, ## x)
#else
#define gprintk(x...) do { } while (0)
#endif
/*******************************************************************************/

#define GPIO_PS_EN 0 //S5PV210_GPJ2(3)

struct workqueue_struct *capella_wq;
static struct wake_lock prx_wake_lock;
static struct device *switch_cmd_dev;

static bool light_enable = 0;
static bool proximity_enable = 0;
int capella_autobrightness_mode = OFF;
static state_type cur_state = LIGHT_INIT;

/* global var */
static struct class *lightsensor_class;
static struct i2c_client *opt_i2c_client = NULL;

static capella_ALS_FOPS_STATUS capella_als_status = capella_ALS_CLOSED;
static capella_PRX_FOPS_STATUS capella_prx_status = capella_PRX_CLOSED;
static capella_CHIP_WORKING_STATUS capella_chip_status = capella_CHIP_UNKNOWN;
static capella_PRX_DISTANCE_STATUS capella_prox_dist = capella_PRX_DIST_OUT;



#define capella_PRX_THRES 		12000		// IMPORTANT
static int capella_prx_thres_hi = capella_PRX_THRES;
static int capella_prx_thres_lo = 0; 

static short proximity_value = 0;
static bool lightsensor_test = 0;

static int cur_adc_value = 0;

static int buffering = 2;
extern int backlight_level;

static int capella_als_lux = 0;
#define PROX_THD 0x15
///////////////////////////////////////////////////////////////////////////////////////////

/* Work queues */
static struct work_struct capella_als_int_wq;
static struct work_struct capella_prox_int_wq;

/* Als/prox info */
static struct capella_prox_info prox_cur_info;
static struct capella_prox_info *prox_cur_infop = &prox_cur_info;

///////////////////////////////////////////////////////////////////////////////////////////

/* Per-device data */
static struct capella_data {
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

static int capella_open_als(struct inode *inode, struct file *file);
static int capella_open_prox(struct inode *inode, struct file *file);
static int capella_release_als(struct inode *inode, struct file *file);
static int capella_release_prox(struct inode *inode, struct file *file);
static int capella_read_als(struct file *file, int *buf, size_t count, loff_t *ppos);
static int capella_read_prox(struct file *file, char *buf, size_t count, loff_t *ppos);

static int capella_chip_on(void);
static int capella_chip_off(void);
static int capella_read_als_data(void);
static int capella_prox_poll(struct capella_prox_info *prxp);

static void capella_on(struct capella_data *capella, int type);
static void capella_off(struct capella_data *capella, int type);
static void capella_ldo_enable(void);
static void capella_work_func_prox(struct work_struct *work);
static void capella_work_func_light(struct work_struct *work);
irqreturn_t capella_irq_handler(int irq, void *dev_id);
static enum hrtimer_restart capella_timer_func(struct hrtimer *timer);

static int CM3623_i2c_read(int address, char rxData[]);
static int CM3623_i2c_write(int address, char txData[]);

short cm3663_get_proximity_value(void);
short gp2a_get_proximity_enable(void);
bool gp2a_get_lightsensor_status(void);
static int StateToLux(state_type state);

#define MDNIE_TUNINGMODE_FOR_BACKLIGHT

#ifdef MDNIE_TUNINGMODE_FOR_BACKLIGHT
int capella_pre_val = -1;
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

int capella_value_buf[4] = {0};

#endif

EXPORT_SYMBOL(cm3663_get_proximity_value);

short cm3663_get_proximity_value(void)
{
	  gprintk("%s proximity_value = %d\n", __func__, capella_prox_dist);
	  return capella_prox_dist;

}

short gp2a_get_proximity_enable(void)
{
	  return proximity_enable;
}


/////////////////////////////////////////////////////////////////////////////

static int CM3623_i2c_read(int address, char rxData[])
{
	struct i2c_msg msgs[] = {
		{
			.addr = address >> 1,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = rxData,
		},
	};
	int ret = 0;

	if((ret = i2c_transfer(opt_i2c_client->adapter, msgs, 1)) < 0)
	{
	    printk("%s %d i2c transfer error = %d\n", __func__, __LINE__, ret);
		return -EIO;
	}
	return 0;
}

static int CM3623_i2c_write(int address, char txData[])
{
	struct i2c_msg msgs[] = {
		{
			.addr = address >> 1,
			.flags = 0,
			.len = 1,
			.buf = txData,
		},
	};
	int ret = 0;

	if((ret = i2c_transfer(opt_i2c_client->adapter, msgs, 1)) < 0)
	{
	    printk("%s %d i2c transfer error = %d\n", __func__, __LINE__, ret);
		return -EIO;
	}

	return 0;
}

static int StateToLux(state_type state)
{
	int lux = 0;

	gprintk("[%s] cur_state:%d\n",__func__,state);
		
	if(state== LIGHT_LEVEL4){
		lux = 15000;
	}
	else if(state == LIGHT_LEVEL3){
		lux = 1500;
	}
	else if(state== LIGHT_LEVEL2){
		lux = 150;
	}
	else if(state == LIGHT_LEVEL1){
		lux = 15;
	}
	else {
		lux = 5000;
		gprintk("[%s] cur_state fail\n",__func__);
	}

	return lux;
}

static ssize_t lightsensor_file_state_show(struct device *dev, struct device_attribute *attr, char *buf)	
{
	int adc = 0, sum = 0, i = 0;

	gprintk("called %s \n",__func__);


	if(!light_enable)
	{
		for(i = 0; i < 10; i++)
		{
			adc = capella_read_als_data();
			msleep(20);
			sum += adc;
		}
		adc = sum/10;
		gprintk("called %s  - subdued alarm(adc : %d)\n",__func__,adc);
		return sprintf(buf,"%d\n",adc);
	}
	
	else
	{
		gprintk("called %s	- *#0589#\n",__func__);
		return sprintf(buf,"%d\n",cur_adc_value);
	}
}

static ssize_t lightsensor_file_state_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)		
{	
	int value;
	sscanf(buf, "%d", &value);
	
	gprintk("called %s \n",__func__);

	if(value==1)
	{
		capella_autobrightness_mode = ON;
		printk(KERN_DEBUG "[brightness_mode] BRIGHTNESS_MODE_SENSOR\n");
	}
	else if(value==0) 
	{
		capella_autobrightness_mode = OFF;
		printk(KERN_DEBUG "[brightness_mode] BRIGHTNESS_MODE_USER\n");
		
		#ifdef MDNIE_TUNINGMODE_FOR_BACKLIGHT
		if(capella_pre_val==1)
		{
			mDNIe_Mode_set_for_backlight(pmDNIe_Gamma_set[2]);
		}	
		capella_pre_val = -1;
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
	struct capella_data *capella = dev_get_drvdata(dev);
	int value;
    sscanf(buf, "%d", &value);

	gprintk("called %s \n",__func__);
	gprintk( "val === %d", &value);
	printk(KERN_INFO "[LIGHT_SENSOR] in lightsensor_file_cmd_store, input value = %d \n",value);

	if(value==1)
	{
		//gp2a_on(gp2a,LIGHT);	Light sensor is always on
		
		if( capella_prx_status == capella_PRX_CLOSED )
		{
		//	capella_chip_on();
		}
		
		if( capella_als_status == capella_ALS_CLOSED )
		{
		//	capella_on(capella,LIGHT );
		}
		
		lightsensor_test = 1;
		value = ON;
		printk(KERN_INFO "[LIGHT_SENSOR] *#0589# test start, input value = %d \n",value);
	}
	else if(value==0) 
	{
		//gp2a_off(gp2a,LIGHT);	Light sensor is always on
	
		if( capella_prx_status == capella_PRX_CLOSED )
		{
	//		capella_chip_off();
		}
		
		if(capella_als_status == capella_ALS_OPENED)
		{
	//		capella_off(capella,LIGHT );
		}

		lightsensor_test = 0;
		value = OFF;
		printk(KERN_INFO "[LIGHT_SENSOR] *#0589# test end, input value = %d \n",value);
	}
	/* temporary test code for proximity sensor */
	else if(value==3 && proximity_enable == OFF)
	{
		if( capella_als_status == capella_ALS_CLOSED )
		{
			capella_chip_on();
		}
		if(capella_prx_status == capella_PRX_CLOSED)
		{		
			capella_on(capella,PROXIMITY);
		}	
		capella_prx_status = capella_PRX_OPENED;	
		
		printk("[PROXIMITY] Temporary : Power ON\n");
	}
	else if(value==2 && proximity_enable == ON)
	{
		
			if( capella_als_status == capella_ALS_CLOSED )
			{
				capella_chip_off();
			}
		if(capella_prx_status == capella_PRX_OPENED)
		capella_off(capella,PROXIMITY);
		
			capella_prx_status = capella_PRX_CLOSED;
		

		
		printk("[PROXIMITY] Temporary : Power OFF\n");
	}
	/* for factory simple test mode */
	if(value==7 && light_enable == OFF)
	{
		capella_on(capella,LIGHT);
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



	gprintk("proximity_file_state_show = [%u]\n ", capella_prox_dist);
	return sprintf(buf,"%u\n", capella_prox_dist);
}

static ssize_t proximity_file_value_show(struct device *dev, struct device_attribute *attr, char *buf)				
{
	int ret = 0;		
	char value = 1;	
	char read_buffer[2]; 
	
	gprintk("called %s \n",__func__);	

	if((ret = CM3623_i2c_read(ADDR_READ_FOR_PS, read_buffer)) < 0)
	{
		printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_read for LSB of ALS failed!\n", __func__);
		read_buffer[0] = read_buffer[1];
	}
	else
		read_buffer[1] = read_buffer[0];

	gprintk("proximity_file_value_show = [%u]\n ", read_buffer[0]);
	return sprintf(buf,"%d\n", read_buffer[0]);
}



static DEVICE_ATTR(proximity_file_state,0666, proximity_file_state_show, NULL);
static DEVICE_ATTR(proximity_file_value,0666, proximity_file_value_show, NULL);

static DEVICE_ATTR(lightsensor_file_cmd,0666, lightsensor_file_cmd_show, lightsensor_file_cmd_store);
static DEVICE_ATTR(lightsensor_file_state,0666, lightsensor_file_state_show, lightsensor_file_state_store);

static void capella_work_func_prox(struct work_struct *work)
{
	int ret= 0, i =0;
	char read_buffer[2];

	if(CM3623_i2c_read(ADDR_ALERT_RESPONSE, read_buffer))  //int clear 
	{
		printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_init failed!\n", __func__);
	}

	if((ret = CM3623_i2c_read(ADDR_READ_FOR_PS, read_buffer)) < 0)
	{
		printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_read for LSB of ALS failed!\n", __func__);
	//	enable_irq(GPIO_PS_ALS_INT_IRQ);			
		return;
	}

	if(read_buffer[0] >= PROX_THD)
	{
		capella_prox_dist = capella_PRX_DIST_IN;

	}
	else{
		capella_prox_dist = capella_PRX_DIST_OUT;
	}

	printk(KERN_DEBUG "prxp->prox_data = [(0x%d]\n", (int)read_buffer[0]); 

//	enable_irq(GPIO_PS_ALS_INT_IRQ);			


	return;
}

static void capella_work_func_light(struct work_struct *work)
{
	int adc=0;
	state_type level_state = LIGHT_INIT;	

	//read value 	
	adc = capella_read_als_data();	
	
	#if 1 //B1
	gprintk("Optimized adc = %d \n",adc);
	gprintk("cur_state = %d\n",cur_state);
	gprintk("light_enable = %d\n",light_enable);
	#else
	gprintk("capella_work_func_light called adc=[%d]\n", adc);
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
		if(capella_autobrightness_mode)
		{
			if((capella_pre_val!=1)&&(current_gamma_value == 24)&&(level_state == LIGHT_LEVEL4)&&(current_mDNIe_UI == mDNIe_UI_MODE))
			{
				mDNIe_Mode_set_for_backlight(pmDNIe_Gamma_set[1]);
				capella_pre_val = 1;
				gprintk("mDNIe_Mode_set_for_backlight - pmDNIe_Gamma_set[1]\n" );
			}
		}
	#endif

	//gprintk("cur_state =[%d], lightsensor_test=[%d] \n",cur_state ,lightsensor_test); //Temp

	return;
}

static enum hrtimer_restart capella_timer_func(struct hrtimer *timer)
{
	ktime_t light_polling_time;		
	struct capella_data *capella = container_of(timer, struct capella_data, timer);
			
	queue_work(capella_wq, &capella->work_light);
	light_polling_time = ktime_set(0,0);
	light_polling_time = ktime_add_us(light_polling_time,500000);
	hrtimer_start(&capella->timer,light_polling_time,HRTIMER_MODE_REL);
	
	return HRTIMER_NORESTART;

}

/*****************************************************************************************
 *  
 *  function    : capella_on 
 *  description : This function is power-on function for optical sensor.
 *
 *  int type    : Sensor type. Two values is available (PROXIMITY,LIGHT).
 *                it support power-on function separately.
 *                
 *                 
 */
static void capella_on(struct capella_data *capella, int type)
{
	ktime_t light_polling_time;
	char write_buffer[1];
	gprintk(KERN_INFO "[OPTICAL] capella_on(%d)\n",type);
	
	if(type == PROXIMITY)
	{
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
					bh6173_ldo_enable_direct(3);
#endif

		write_buffer[0] = PROX_THD;
		if(CM3623_i2c_write(ADDR_WRITE_THD_FOR_PS, write_buffer))
		{
			printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_write failed!\n", __func__);
		}

	write_buffer[0] = 0x34;
	if(CM3623_i2c_write(ADDR_WRITE_CMD_FOR_PS, write_buffer))
	{
		printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_write failed!\n", __func__);
	}
	
		proximity_enable = ON;

		enable_irq(GPIO_PS_ALS_INT_IRQ);			


	}
	if(type == LIGHT)
	{
		gprintk(KERN_INFO "[LIGHT_SENSOR] timer start for light sensor\n");
		
		
		write_buffer[0] = 0x04;
		if(CM3623_i2c_write(ADDR_WRITE_CMD_FOR_ALS, write_buffer))
		{
			printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_write failed!\n", __func__);
		}
		
		
		light_polling_time = ktime_set(0,0);
		light_polling_time = ktime_add_us(light_polling_time,500000);
	    hrtimer_start(&capella->timer,light_polling_time,HRTIMER_MODE_REL);
		light_enable = 1;
	}
}

/*****************************************************************************************
 *  
 *  function    : capella_off 
 *  description : This function is power-off function for optical sensor.
 *
 *  int type    : Sensor type. Three values is available (PROXIMITY,LIGHT,ALL).
 *                it support power-on function separately.
 *                
 *                 
 */

static void capella_off(struct capella_data *capella, int type)
{
	char write_buffer[1];
	
	gprintk(KERN_INFO "[OPTICAL] capella_off(%d)\n",type);
	if(type == PROXIMITY || type == ALL)
	{
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
					bh6173_ldo_disable_direct(3);
#endif
		write_buffer[0] = 0x01;
		if(CM3623_i2c_write(ADDR_WRITE_CMD_FOR_PS, write_buffer))
		{
			printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_write failed!\n", __func__);
		}

		proximity_enable = OFF;
		disable_irq(GPIO_PS_ALS_INT_IRQ);

	}

	if(type ==LIGHT)
	{
	
		hrtimer_cancel(&capella->timer);
	
		write_buffer[0] = 0x01;
		if(CM3623_i2c_write(ADDR_WRITE_CMD_FOR_ALS, write_buffer))
		{
			printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_write failed!\n", __func__);
		}
		gprintk("[LIGHT_SENSOR] timer cancel for light sensor\n");
		light_enable = 0;
	}
}


irqreturn_t capella_irq_handler(int irq, void *dev_id)
{
	struct capella_data *capella = dev_id;

	gprintk(KERN_DEBUG "capella_irq_handler() is called\n");

	queue_work(capella_wq, &capella->work_prox);	
	wake_lock_timeout(&prx_wake_lock, 5*HZ);
	
	return IRQ_HANDLED;
}

static void capella_ldo_enable(void)
{	
	int err = 1;
#if 0	
	err = gpio_request(GPIO_PS_EN, "PS_EN");

	if (err) {
		printk(KERN_ERR "failed to request S5PV210_GPJ2(3) for "
			"LEDA_3.0V control\n");
		return;
	}
	gpio_direction_output(GPIO_PS_EN, 1);
	gpio_set_value(GPIO_PS_EN, 0);
	gpio_free(GPIO_PS_EN);

#else


#endif
}

/*
 * Device open function - if the driver is loaded in interrupt mode, a valid
 * irq line is expected, and probed for, by requesting, and testing, all the
 * available irqs on the system using a probe interrupt handler. If found, the
 * irq is requested again, for subsequent use/handling by the run-time handler.
 */
static int capella_open_als(struct inode *inode, struct file *file)
{
	int ret = 0;		
	struct capella_data *capella = i2c_get_clientdata(opt_i2c_client);

	gprintk("[%s] \n",__func__);
	
	if( capella_prx_status == capella_PRX_CLOSED )
	{
		capella_chip_on();
	}
	
	capella_als_status = capella_ALS_OPENED;

	capella_on(capella, LIGHT);

	return (ret);
}

/*
 * Device open function - if the driver is loaded in interrupt mode, a valid
 * irq line is expected, and probed for, by requesting, and testing, all the
 * available irqs on the system using a probe interrupt handler. If found, the
 * irq is requested again, for subsequent use/handling by the run-time handler.
 */
static int capella_open_prox(struct inode *inode, struct file *file)
{
	int ret = 0;	
	struct capella_data *capella = i2c_get_clientdata(opt_i2c_client);
	
	gprintk("[%s] \n",__func__);


	if( capella_als_status == capella_ALS_CLOSED )
	{
		capella_chip_on();
	}
	
	capella_on(capella, PROXIMITY);
	capella_prx_status = capella_PRX_OPENED;

	return (ret);
}

/*
 * Device release
 */
static int capella_release_als(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct capella_data *capella = i2c_get_clientdata(opt_i2c_client);
	
	gprintk("%s",__func__);
	
	capella_off(capella,LIGHT);	
	
	if( capella_prx_status == capella_PRX_CLOSED )
	{
		capella_chip_off();	
	}
	capella_als_status = capella_ALS_CLOSED;
	
	return (ret);
}

/*
 * Device release
 */
static int capella_release_prox(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct capella_data *capella = i2c_get_clientdata(opt_i2c_client);
	
	gprintk("%s",__func__);

	capella_off(capella,PROXIMITY);	

	if( capella_als_status == capella_ALS_CLOSED )
	{
		capella_chip_off();
	}
	capella_prx_status = capella_PRX_CLOSED;

	return (ret);
}


/*
 	 * ALS_DATA - request for current ambient light data. If correctly
 	 * enabled and valid data is available at the device, the function
 	 * for lux conversion is called, and the result returned if valid.
*/
static int capella_read_als_data(void)
{
		u16 lux_val=0;
		char buffer[2];
		long Temp;
	

		if(CM3623_i2c_read(ADDR_READ_LSB_FOR_ALS, &buffer[1]))
		{
			printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_read for LSB of ALS failed!\n", __func__);
			return 0;
		}
	
		if(CM3623_i2c_read(ADDR_READ_MSB_FOR_ALS, buffer))
		{
			printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_read for MSB of ALS failed!\n", __func__);
			return 0;
		}

		lux_val = ((u16)buffer[0] << 8) | (u16)buffer[1];


		if (lux_val < 0)
		{
			gprintk( "capella_get_lux() returned error %d in capella_read_als_data\n", lux_val);
		}

		if (lux_val >= MAX_LUX)
		{
			lux_val = MAX_LUX; //return (MAX_LUX);
		}

		Temp = (long)lux_val;
		Temp *=  7;
		Temp /= 10;
		Temp *= 5; 
		cur_adc_value = Temp;
		
		gprintk("capella_read_als_data cur_adc_value = %d (0x%x)\n", cur_adc_value, cur_adc_value);

		return (lux_val);
}

/*
 * Device read - reads are permitted only within the range of the accessible
 * registers of the device. The data read is copied safely to user space.
 */

static int capella_read_als(struct file *file, int *buf, size_t count, loff_t *ppos)
{
	int lux_val = 0;

	//	gprintk("%s",__func__);
	#if 1 //B1
	lux_val = StateToLux(cur_state);
	#else
	lux_val = capella_read_als_data();
	#endif

	put_user(cur_adc_value, buf);	
//	put_user(lux_val, buf);

	return (1);
}

/*
 * Device read - reads are permitted only within the range of the accessible
 * registers of the device. The data read is copied safely to user space.
*/
static int capella_read_prox(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	int ret = 0;
	char read_buffer[2]; 


	put_user(capella_prox_dist,buf);
	
	gprintk("capella_read_prox = [%d]\n ", capella_prox_dist);

	return 1; 
}


/*
 	 * ALS_ON - called to set the device in ambient light sense mode.
 	 * configured values of light integration time, inititial interrupt
 	 * filter, gain, and interrupt thresholds (if interrupt driven) are
 	 * initialized. Then power, adc, (and interrupt if needed) are enabled.
*/

static int capella_chip_on(void)
{
	char read_buffer[2];
	char write_buffer[1];
	int cnt,ret, i =0;

#if 1
	for( i = 0; i < 3; i++)  //int clear 
	{	if(CM3623_i2c_read(ADDR_ALERT_RESPONSE, read_buffer))
		{
			printk(KERN_DEBUG "[CM3623][%s] :interrupt clear\n", __func__);
		}
	}


	write_buffer[0] = 0x10;
	if(CM3623_i2c_write(ADDR_FOR_INITIALIZATION, write_buffer))
	{
		printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_write failed!\n", __func__);
					
	}


#endif 

#if 0
	for(cnt = 0 ; cnt < 3; cnt++)
	{
		/*
		if(CM3623_i2c_read(ADDR_ALERT_RESPONSE, read_buffer))
		{
			printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_init failed!\n", __func__);
			continue;
		}
		if(CM3623_i2c_read(ADDR_ALERT_RESPONSE, read_buffer))
		{
			printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_init failed!\n", __func__);
			continue;
		}
		*/
		write_buffer[0] = 0x10;
		if(CM3623_i2c_write(ADDR_FOR_INITIALIZATION, write_buffer))
		{
			printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_write failed!\n", __func__);
			continue;
		}
		write_buffer[0] = 0x04;
		if(CM3623_i2c_write(ADDR_WRITE_CMD_FOR_ALS, write_buffer))
		{
			printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_write failed!\n", __func__);
			continue;
		}
		write_buffer[0] = 0x0a;
		if(CM3623_i2c_write(ADDR_WRITE_THD_FOR_PS, write_buffer))
		{
			printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_write failed!\n", __func__);
			continue;
		}
		write_buffer[0] = 0x00;
		if(CM3623_i2c_write(ADDR_WRITE_CMD_FOR_PS, write_buffer))
		{
			printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_write failed!\n", __func__);
			continue;
		}
		printk("capella Probe is OK!!\n");
		break;
	}

//	gpio_set_value(GPIO_PS_EN, 1);

	enable_irq(GPIO_PS_ALS_INT_IRQ);			
	proximity_enable = ON;
#endif
	capella_chip_status = capella_CHIP_WORKING;

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

static int capella_chip_off(void)
{
		int ret = 0;

		char write_buffer[1];

		gprintk("\n");
#if 0
		#if 1 // working interrupt
		// interrupt disable 
		disable_irq(GPIO_PS_ALS_INT_IRQ);
		#endif

		//shut down
		write_buffer[0] = 0x01;
		if(CM3623_i2c_write(ADDR_WRITE_CMD_FOR_ALS, write_buffer))
		{
			printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_write failed!\n", __func__);
		}
		write_buffer[0] = 0x01;
		if(CM3623_i2c_write(ADDR_WRITE_CMD_FOR_PS, write_buffer))
		{
			printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_write failed!\n", __func__);
		}



		proximity_enable = 0;
#endif
		capella_chip_status = capella_CHIP_SLEEP;
			
		return (ret);
}

/*
 * Proximity poll function - if valid data is available, read and form the ch0
 * and prox data values, check for limits on the ch0 value, and check the prox
 * data against the current thresholds, to set the event status accordingly.
 */
 static int proximity_state = 0; 
static int capella_prox_poll(struct capella_prox_info *prxp)
{
	int ret = 0;
	int event = 0;
	char buffer[1];

	//gprintk("\n");	

	if((ret = CM3623_i2c_read(ADDR_READ_FOR_PS, &buffer[0])) < 0)
	{
		printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_read for LSB of ALS failed!\n", __func__);
		return (ret);
	}
	
	prxp->prox_data = (u16)buffer[0];

	gprintk("prxp->prox_data = [%d (0x%x)]\n", (int) prxp->prox_data, (int) prxp->prox_data);

#if 0
	if ( prxp->prox_data > 0x06)
	{
		event = 1;
		proximity_value = 0;
	}
	else
	{
		event = 0;
		proximity_value = 1;
	}
#else
	if(proximity_state) // if current state is detected, 
	{		
		if ( prxp->prox_data < 0x06)
		{			
			event = 0;
			proximity_value = 1;			
			proximity_state = 0; 
		}
		else
		{
			event = 1;  // detect 
			proximity_value = 0;

		}
	}
	else
	{
		if ( prxp->prox_data > 0x06)
		{
			event = 1;
			proximity_value = 0; 
			proximity_state = 1; //detect 
		}
		else
		{
			event = 0;
			proximity_value = 1;
		}
	}		


#endif


		
	prxp->prox_event = event;

	return (ret);
}

/* File operations */
static struct file_operations capella_fops_als = 
{
	.owner 		= THIS_MODULE,
	.open 		= capella_open_als,
	.release	= capella_release_als,
	.read 		= capella_read_als,
};

/* File operations */
static struct file_operations capella_fops_prox = 
{
	.owner 		= THIS_MODULE,
	.open 		= capella_open_prox,
	.release 	= capella_release_prox,
	.read 		= capella_read_prox,
};                

static struct miscdevice light_device = 
{
    .minor  = MISC_DYNAMIC_MINOR,
    .name   = "light",
    .fops   = &capella_fops_als,
};

static struct miscdevice proximity_device = 
{
    .minor  = MISC_DYNAMIC_MINOR,
    .name   = "proximity",
    .fops   = &capella_fops_prox,
};

/*
 * Client probe function - When a valid device is found, the driver's device
 * data structure is updated, and initialization completes successfully.
 */
static int capella_probe( struct i2c_client *client, const struct i2c_device_id *id)
{		
 	int irq;
	int ret;
	struct capella_data *capella;
	char write_buffer[2]; 

	if(client == NULL)
	{
		pr_err("capella_probe failed : i2c_client is NULL\n"); 
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
	capella = kzalloc(sizeof(struct capella_data),GFP_KERNEL);
	if(!capella)
	{
		pr_err("kzalloc error\n");
		return -ENOMEM;
	}	

	//mdelay(1000);
	
	opt_i2c_client = client;
	i2c_set_clientdata(client, capella);
	capella->client = client;
	
	/* hrtimer Settings */
	hrtimer_init(&capella->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	capella->timer.function = capella_timer_func;

	capella_wq = create_singlethread_workqueue("capella_wq");
    if (!capella_wq)
   	{
	    return -ENOMEM;
   	}  		

	INIT_WORK(&capella->work_prox, capella_work_func_prox);	
    INIT_WORK(&capella->work_light, capella_work_func_light);
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
	
	if (device_create_file(switch_cmd_dev, &dev_attr_proximity_file_value) < 0)
	{
		pr_err("Failed to create device file(%s)!\n", dev_attr_proximity_file_value.attr.name);
	}		

	
	/* init i2c */	
	gprintk("i2c Setting completes.\n");

	/* Input device Settings */
	if(USE_INPUT_DEVICE)
	{
		capella->input_dev = input_allocate_device();
		if (capella->input_dev == NULL) 
		{
			pr_err("Failed to allocate input device\n");
			return -ENOMEM;
		}
		
		capella->input_dev->name = "proximity";	
		set_bit(EV_SYN,capella->input_dev->evbit);
		set_bit(EV_ABS,capella->input_dev->evbit);		
        input_set_abs_params(capella->input_dev, ABS_DISTANCE, 0, 1, 0, 0);
			
		ret = input_register_device(capella->input_dev);
		if (ret) 
		{
			pr_err("Unable to register %s input device\n", capella->input_dev->name);
			input_free_device(capella->input_dev);
			kfree(capella);
			return -1;
		}
	}

	/* wake lock init */
	wake_lock_init(&prx_wake_lock, WAKE_LOCK_SUSPEND, "prx_wake_lock");

	
	write_buffer[0] = 0x01;
	if(CM3623_i2c_write(ADDR_WRITE_CMD_FOR_PS, write_buffer))
	{
		printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_write failed!\n", __func__);
	}

	if(CM3623_i2c_write(ADDR_WRITE_CMD_FOR_ALS, write_buffer))
	{
		printk(KERN_ERR "[CM3623][%s] : CM3623_i2c_write failed!\n", __func__);
	}

	capella_chip_status = capella_CHIP_SLEEP;

	/* interrupt hander register. */
	capella->irq = -1;
	irq = GPIO_PS_ALS_INT_IRQ;	
	
	ret = request_irq(irq, capella_irq_handler, IRQF_DISABLED, "proximity_int", capella); // check and study here...
	if (ret) 
	{
		printk("unable to request irq proximity_int err:: %d\n", ret);
		return ret;
	}		
	disable_irq(irq);
	
	capella->irq = irq;
	
	gprintk("capella Probe is OK!!\n");
	
	return (ret);

}

static int __exit capella_remove(struct i2c_client *client)
{
	struct capella_data *capella = i2c_get_clientdata(client);
	
	printk("[%s]\n", __func__);	
	
    if (capella_wq)
		destroy_workqueue(capella_wq);

	if(USE_INPUT_DEVICE)
		input_unregister_device(capella->input_dev);	
	
	misc_deregister(&light_device);
	misc_deregister(&proximity_device);
	
	kfree(capella);
	kfree(client);
	capella = NULL;
	
	opt_i2c_client = NULL;
	
	return 0;
}

static int capella_opt_suspend( struct i2c_client *client, pm_message_t state )
{
	struct capella_data *capella = i2c_get_clientdata(client);
	
	if(light_enable)
	{
		gprintk("[%s] : hrtimer_cancle \n",__func__);
	//	hrtimer_cancel(&capella->timer);
	}
	
	if(proximity_enable)
	{
		
	}

	gprintk("[%s] : \n",__func__);

	return 0;
}

static int capella_opt_resume( struct i2c_client *client )
{
	struct capella_data *capella = i2c_get_clientdata(client);
	ktime_t light_polling_time;

	if(light_enable)
	{
	//	light_polling_time = ktime_set(0,0);
	//	light_polling_time = ktime_add_us(light_polling_time,500000);
	 //   hrtimer_start(&capella->timer,light_polling_time,HRTIMER_MODE_REL);
	}
	
	if(proximity_enable)
	{
		
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
		bh6173_ldo_enable_direct(3);
#endif
	}
	gprintk("[%s] : \n",__func__);
	
	return 0;
}

static const struct i2c_device_id cm3663_id[] = {
	{CAPELLA_DEVICE_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, cm3663_id);

/* Driver definition */
static struct i2c_driver opt_i2c_driver = {
	.driver = {
		.name = CAPELLA_DEVICE_NAME,
	},
	.probe = capella_probe,
	.remove = __exit_p(capella_remove),
	.suspend = capella_opt_suspend,
	.resume = capella_opt_resume,
	.id_table = cm3663_id,
};

/*
 * Driver initialization - device probe is initiated here, to identify
 * a valid device if present on any of the i2c buses and at any address.
 */
static int __init capella_init(void)
{	
	gprintk("[%s]\n", __func__);
	
	capella_ldo_enable();

	/* set interrupt pin */	
	s3c_gpio_cfgpin(GPIO_PS_ALS_INT, S3C_GPIO_SFN(GPIO_PS_ALS_INT_AF));
	s3c_gpio_setpull(GPIO_PS_ALS_INT, S3C_GPIO_PULL_UP);	
	
	set_irq_type(GPIO_PS_ALS_INT_IRQ, IRQ_TYPE_EDGE_FALLING);		
	
	return i2c_add_driver(&opt_i2c_driver);
}

/*
 * Driver exit
 */
static void __exit capella_exit(void)
{	
	gprintk("[%s]\n", __func__);
	
	i2c_del_driver(&opt_i2c_driver);
	
}

module_init(capella_init);
module_exit(capella_exit);

MODULE_AUTHOR("SAMSUNG");
MODULE_DESCRIPTION("Samsung Customized capella light and proximity sensor driver");




