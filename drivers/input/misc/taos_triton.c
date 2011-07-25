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

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include "taos_triton.h" 
#include <linux/taos.h>
#include <mach/gpio-aries.h>

/* Note about power vs enable/disable:
 *  The chip has two functions, proximity and ambient light sensing.
 *  There is no separate power enablement to the two functions (unlike
 *  the Capella CM3602/3623).
 *  This module implements two drivers: /dev/proximity and /dev/light.
 *  When either driver is enabled (via sysfs attributes), we give power
 *  to the chip.  When both are disabled, we remove power from the chip.
 *  In suspend, we remove power if light is disabled but not if proximity is
 *  enabled (proximity is allowed to wakeup from suspend).
 *
 *  There are no ioctls for either driver interfaces.  Output is via
 *  input device framework and control via sysfs attributes.
 */

/* taos debug */
//#define DEBUG 1
//#define TAOS_DEBUG 1

#define taos_dbgmsg(str, args...) pr_debug("%s: " str, __func__, ##args)

#ifdef TAOS_DEBUG
#define gprintk(fmt, x... ) printk( "%s(%d): " fmt, __FUNCTION__ ,__LINE__, ## x)
#else
#define gprintk(x...) do { } while (0)
#endif


#define ADC_BUFFER_NUM	17

/* sensor type */
#define LIGHT           0
#define PROXIMITY	1
#define ALL		2

struct taos_data;

enum {
	LIGHT_ENABLED = BIT(0),
	PROXIMITY_ENABLED = BIT(1),
};

#define TAOS_PRX_THRES_IN			600
#define TAOS_PRX_THRES_OUT			420

static int taos_prx_thres_hi = TAOS_PRX_THRES_IN;
static int taos_prx_thres_lo = 0; 

/* driver data */
struct taos_data{
	struct input_dev *proximity_input_dev;
	struct input_dev *light_input_dev;
	struct taos_platform_data *pdata;
	struct i2c_client *i2c_client;
	struct class *lightsensor_class;
	struct device *switch_cmd_dev;
	int irq;
	struct work_struct work_light;
	struct work_struct work_prox;	
	u16 prox_data; 					
	int prox_event; 					
	struct mutex prox_mutex;
//	TAOS_PRX_DISTANCE_STATUS taos_prox_dist; 
	struct hrtimer timer;
	ktime_t light_poll_delay;
	int adc_value_buf[ADC_BUFFER_NUM];
	int adc_index_count;
	bool adc_buf_initialized;
	bool on;
	u8 power_state;
	struct mutex power_lock;
	struct wake_lock prx_wake_lock;
	struct workqueue_struct *wq;
	char val_state;
};

struct taos_data *taos_global = NULL ;

static int opt_i2c_write(struct taos_data *taos, u8 reg, u8 *val )
{
	int ret;
		
	udelay(10);
	ret = i2c_smbus_write_byte_data( taos->i2c_client,(CMD_REG | reg),*val);
	
	return ret;
}

static int opt_i2c_read(struct taos_data *taos, u8 reg , u8 *val)
{	
	int ret;

	udelay(10);	
	i2c_smbus_write_byte(taos->i2c_client, (CMD_REG | reg));
	ret = i2c_smbus_read_byte(taos->i2c_client);
	*val = ret;

	return ret;
}

static int opt_i2c_write_command(struct taos_data *taos, u8 val )
{
	int ret;
	
	udelay(10);	
	ret = i2c_smbus_write_byte(taos->i2c_client, val);
	gprintk("[TAOS Command] val=[0x%x] - ret=[0x%x]\n", val, ret);
	
	return ret;
}

static int taos_register_dump(struct taos_data *taos)
{
	int i=0; 
	int ret = 0;
	u8 chdata[0x20];
	
	for (i = 0; i < 0x20; i++) {
		if ((ret = opt_i2c_read(taos, (CNTRL + i), &chdata[i]))< 0) {
			gprintk("i2c_smbus_read_byte() to chan0/1/lo/hi"" regs failed in taos_register_dump()\n"); 
			return (ret);
		}
	}
	
	printk("** taos register dump ** \n");
	for (i = 0; i < 0x20; i++) 
		printk("(%x)0x%02x, ", i, chdata[i]);
	printk("\n");	
	
	return 0;
}

static int taos_chip_on(struct taos_data *taos)
{
	int i = 0;
	int ret = 0;		
	int num_try_init = 0;
	int fail_num = 0;
	u8 temp_val;
	u8 reg_cntrl;
	u8 prox_int_thresh[4];

	u8 temp;

	gprintk("\n");

	for ( num_try_init=0; num_try_init<3 ; num_try_init ++) {
		fail_num = 0;
	
		temp_val = CNTL_REG_CLEAR;
		if ((ret = opt_i2c_write(taos, (CMD_REG|CNTRL),&temp_val))< 0) {
			gprintk( "opt_i2c_write to clr ctrl reg failed in taos_chip_on.\n");
			fail_num++; 
		}		

		temp_val = 0xf6; 
		if ((ret = opt_i2c_write(taos, (CMD_REG|ALS_TIME), &temp_val))< 0) {
			gprintk( "opt_i2c_write to als time reg failed in taos_chip_on.\n");
			fail_num++; 
		}

		temp_val = 0xff; 
		if ((ret = opt_i2c_write(taos, (CMD_REG|PRX_TIME), &temp_val))< 0) {
			gprintk( "opt_i2c_write to prx time reg failed in taos_chip_on.\n");
			fail_num++; 
		}

		temp_val =0xff;
		if ((ret = opt_i2c_write(taos, (CMD_REG|WAIT_TIME), &temp_val)) < 0) {
			gprintk( "opt_i2c_write to wait time reg failed in taos_chip_on.\n");
			fail_num++; 
		}		

		temp_val = 0x33; 
		if ((ret = opt_i2c_write(taos, (CMD_REG|INTERRUPT), &temp_val)) < 0) {
			gprintk( "opt_i2c_write to interrupt reg failed in taos_chip_on.\n");
			fail_num++; 
		}			

		temp_val = 0x0;
		if ((ret = opt_i2c_write(taos, (CMD_REG|PRX_CFG), &temp_val)) < 0) {
			gprintk( "opt_i2c_write to prox cfg reg failed in taos_chip_on.\n");
			fail_num++; 
		}		
		
		temp_val =0x0a; 
		if ((ret = opt_i2c_write(taos, (CMD_REG|PRX_COUNT), &temp_val)) < 0)	{
			gprintk( "opt_i2c_write to prox cnt reg failed in taos_chip_on.\n");
			fail_num++; 
		}
				
		temp_val =0x21; 
		if ((ret = opt_i2c_write(taos, (CMD_REG|GAIN), &temp_val)) < 0) {
			gprintk( "opt_i2c_write to prox gain reg failed in taos_chip_on.\n");
			fail_num++; 
		}

		/* Setting for proximity interrupt */			
		prox_int_thresh[0] = (taos_prx_thres_lo) 		& 0xff;
		prox_int_thresh[1] = (taos_prx_thres_lo >> 8) 	& 0xff;
		prox_int_thresh[2] = (taos_prx_thres_hi)	 		& 0xff;
		prox_int_thresh[3] = (taos_prx_thres_hi >> 8) 	& 0xff;
		
		for (i = 0; i < 4; i++) {
			ret = opt_i2c_write(taos, (CMD_REG|(PRX_MINTHRESHLO + i)), &prox_int_thresh[i]);
			if ( ret < 0) {
				gprintk( "opt_i2c_write failed in taos_chip_on, err = %d\n", ret);
				fail_num++; // 
			}
		}				
				
		reg_cntrl = CNTL_INTPROXPON_ENBL ; 
		if ((ret = opt_i2c_write(taos, (CMD_REG|CNTRL), &reg_cntrl))< 0){
			gprintk( "opt_i2c_write to ctrl reg failed in taos_chip_on.\n");
			fail_num++; // return (ret);
		}
		mdelay(20); // more than 12 ms

		/* interrupt clearing */
		temp = (CMD_REG|CMD_SPL_FN|CMD_PROXALS_INTCLR);
		ret = opt_i2c_write_command(taos, temp);
		if ( ret < 0)
		{
			gprintk( "opt_i2c_write failed in taos_chip_on, err = %d\n", ret);
			fail_num++; // 
		}
		

		if ( fail_num == 0) {	
			/* taos_register_dump(taos); */
			break;	// Init Success 
		}
		else{
			printk( "I2C failed in taos_chip_on, # of trial=[%d], # of fail I2C=[%d]\n", num_try_init, fail_num);
		}

	}
			
	return (ret);

}

static int taos_chip_off(struct taos_data *taos)
{
		int ret = 0;
		u8 reg_cntrl;

		gprintk("\n");

		reg_cntrl = CNTL_REG_CLEAR;		
		if ((ret = opt_i2c_write(taos, (CMD_REG | CNTRL), &reg_cntrl))< 0)
		{
			gprintk( "opt_i2c_write to ctrl reg failed in taos_chip_off\n");
			return (ret);
		}
					
		return (ret);
}

static int taos_get_lux(struct taos_data *taos)
{
	int i=0;
	int ret = 0;
	u8 chdata[4];

	int irdata = 0;
	int cleardata = 0;	
	int ratio_for_lux = 0;
	int irfactor = 0;
	int calculated_lux = 0;
	int compensated_lux = 0;

	int integration_time = 27;
	int als_gain = 8			;

	for (i = 0; i < 4; i++) {
		ret = opt_i2c_read(taos, (ALS_CHAN0LO + i),&chdata[i] );
		
		if ( ret < 0 ) {
			gprintk( "opt_i2c_read() to ALS_CHAN0LO + i regs failed in taos_get_lux()\n");
			return 0; //r0 return (ret);
		}
	}

	cleardata = 256 * chdata[1] + chdata[0];
	irdata = 256 * chdata[3] + chdata[2];

	/* Check for valid data return and calculate ratio of channels */
	if(cleardata > 0)
		ratio_for_lux = (irdata * 100)  /  cleardata;
	else
		ratio_for_lux = 106;	

	/* Determine light type and calculate LUX */
	if(ratio_for_lux < 7){
		calculated_lux =(( 245 * cleardata) - (50 * irdata))/100;
		ratio_for_lux = 101;
	}
	
	if(ratio_for_lux < 25) {
		calculated_lux =(( 240 * cleardata) - (64 * irdata))/100;
		ratio_for_lux = 102;
	}
	
	if(ratio_for_lux < 35) {
		calculated_lux =(( 334 * cleardata) - (123 * irdata))/100;
		ratio_for_lux = 103;
	}
	
	if(ratio_for_lux < 100) {
		if(cleardata > 16000)
			calculated_lux = (162*irdata)/100;
		else
			calculated_lux = (( 51 * cleardata) - (57 * irdata))/100;
			
		ratio_for_lux = 104;
	}
	
#if 1 /* Nathan : Compensation for a quick fix. */
	/* 10->300, 20 -> 500, 120 ->1200 */
	if ( calculated_lux < 0)
		compensated_lux = 0;
	else if ( calculated_lux <= 10 )
		compensated_lux = 30 * calculated_lux;
	else if ( calculated_lux <= 20 )
		compensated_lux = (200 * (calculated_lux-10) )/10 + 300;
	else if ( calculated_lux <= 120 )
		compensated_lux = ((700 * (calculated_lux-20)) / (100)) + 500;
	else
		compensated_lux = (calculated_lux-120) + 1200;

	if ( cleardata >= 10240 ) {
	 	compensated_lux = 30000;
	}
#endif

#if 0 // TAOS Debug
	gprintk( "cdata=[%d] idata=[%d] ratio_for_lux=[%d], calculated_lux=[%d], compensated_lux=[%d]\n", 
		cleardata, irdata, ratio_for_lux, calculated_lux, compensated_lux);	
#endif

#if 1 /* Nathan : Compensation for a quick fix. */
	calculated_lux = compensated_lux;
#endif

	// Check values for over and underflow
	if(calculated_lux > MAX_LUX) 
		calculated_lux = MAX_LUX;

	if(calculated_lux < 0) 
		calculated_lux = 0;

	return (calculated_lux);
}

static void taos_light_enable(struct taos_data *taos)
{
	taos_dbgmsg("starting poll timer, delay %lldns\n",
	ktime_to_ns(taos->light_poll_delay));
	hrtimer_start(&taos->timer, taos->light_poll_delay, HRTIMER_MODE_REL);
}

static void taos_light_disable(struct taos_data *taos)
{
	taos_dbgmsg("cancelling poll timer\n");
	hrtimer_cancel(&taos->timer);
	cancel_work_sync(&taos->work_light);
}

static ssize_t poll_delay_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	return sprintf(buf, "%lld\n", ktime_to_ns(taos->light_poll_delay));
}


static ssize_t poll_delay_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	int64_t new_delay;
	int err;

	err = strict_strtoll(buf, 10, &new_delay);
	if (err < 0)
		return err;

	taos_dbgmsg("new delay = %lldns, old delay = %lldns\n",
		    new_delay, ktime_to_ns(taos->light_poll_delay));
	mutex_lock(&taos->power_lock);
	if (new_delay != ktime_to_ns(taos->light_poll_delay)) {
		taos->light_poll_delay = ns_to_ktime(new_delay);
		if (taos->power_state & LIGHT_ENABLED) {
			taos_light_disable(taos);
			taos_light_enable(taos);
		}
	}
	mutex_unlock(&taos->power_lock);

	return size;
}

static ssize_t light_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",
		       (taos->power_state & LIGHT_ENABLED) ? 1 : 0);
}

static ssize_t proximity_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",
		       (taos->power_state & PROXIMITY_ENABLED) ? 1 : 0);
}

static ssize_t light_enable_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	bool new_value;
	
	gprintk("\n");

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&taos->power_lock);
	taos_dbgmsg("new_value = %d, old state = %d\n",
		    new_value, (taos->power_state & LIGHT_ENABLED) ? 1 : 0);
	if (new_value && !(taos->power_state & LIGHT_ENABLED)) {
		if (!taos->power_state)
			taos->pdata->power(true);
		taos->power_state |= LIGHT_ENABLED;
		taos_light_enable(taos);
	} else if (!new_value && (taos->power_state & LIGHT_ENABLED)) {
		taos_light_disable(taos);
		taos->power_state &= ~LIGHT_ENABLED;
		if (!taos->power_state)
			taos->pdata->power(false);
	}
	mutex_unlock(&taos->power_lock);
	return size;
}

static ssize_t proximity_enable_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	bool new_value;

	gprintk("\n");

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&taos->power_lock);
	taos_dbgmsg("new_value = %d, old state = %d\n",
		    new_value, (taos->power_state & PROXIMITY_ENABLED) ? 1 : 0);
	if (new_value && !(taos->power_state & PROXIMITY_ENABLED)) {
		if (!taos->power_state)
			taos->pdata->power(true);
		taos->power_state |= PROXIMITY_ENABLED;
		
		taos_chip_on(taos);			
		enable_irq(taos->irq);
		enable_irq_wake(taos->irq);
	
	} else if (!new_value && (taos->power_state & PROXIMITY_ENABLED)) {
		disable_irq_wake(taos->irq);
		disable_irq(taos->irq);		
		
		taos->power_state &= ~PROXIMITY_ENABLED;
		if (!taos->power_state)
			taos->pdata->power(false); //taos_chip_off(taos);
	}
	mutex_unlock(&taos->power_lock);
	return size;
}

static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   poll_delay_show, poll_delay_store);

static struct device_attribute dev_attr_light_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	       light_enable_show, light_enable_store);

static struct device_attribute dev_attr_proximity_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	       proximity_enable_show, proximity_enable_store);

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_light_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};

static struct attribute *proximity_sysfs_attrs[] = {
	&dev_attr_proximity_enable.attr,
	NULL
};

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_sysfs_attrs,
};

static int lightsensor_get_adcvalue(struct taos_data *taos)
{
	int i = 0;
	int j = 0;
	unsigned int adc_total = 0;
	int adc_avr_value;
	unsigned int adc_index = 0;
	unsigned int adc_max = 0;
	unsigned int adc_min = 0;
	int value = 0;

	/* get ADC */	
	value = taos_get_lux(taos); 

	adc_index = (taos->adc_index_count++) % ADC_BUFFER_NUM;

	/*ADC buffer initialize (light sensor off ---> light sensor on) */
	if (!taos->adc_buf_initialized) {
		taos->adc_buf_initialized = true;
		for (j = 0; j < ADC_BUFFER_NUM; j++)
			taos->adc_value_buf[j] = value;
	} else
		taos->adc_value_buf[adc_index] = value;

	adc_max = taos->adc_value_buf[0];
	adc_min = taos->adc_value_buf[0];

	for (i = 0; i < ADC_BUFFER_NUM; i++) {
		adc_total += taos->adc_value_buf[i];

		if (adc_max < taos->adc_value_buf[i])
			adc_max = taos->adc_value_buf[i];

		if (adc_min > taos->adc_value_buf[i])
			adc_min = taos->adc_value_buf[i];
	}
	adc_avr_value = (adc_total-(adc_max+adc_min))/(ADC_BUFFER_NUM-2);

	if (taos->adc_index_count == ADC_BUFFER_NUM-1)
		taos->adc_index_count = 0;
	
	return adc_avr_value;
}

static int taos_prox_get_dist(struct taos_data *taos)
{
	int i;
	int ret = 0;	
	u8 status;
	u8 chdata[6];
	int cleardata = 0;	
	
	mutex_lock(&taos->prox_mutex);
	
	if ((ret = opt_i2c_read(taos, STATUS , &status))< 0) {
		gprintk( "opt_i2c_read() to STATUS regs failed in taos_prox_poll()\n");
		mutex_unlock(&taos->prox_mutex);
		return (ret);
	}	

	for (i = 0; i < 6; i++) {
		if ((ret = opt_i2c_read(taos,  (ALS_CHAN0LO + i),&chdata[i]))< 0) {
			gprintk( "opt_i2c_read() to (ALS_CHAN0LO + i) regs failed in taos_prox_poll()\n");
			mutex_unlock(&taos->prox_mutex);
			return (ret);
		}
	}
	
	cleardata = 256 * chdata[1] + chdata[0];
	
	if(cleardata >= 10240 ) {
		printk("(%s) cleardata = [%d], set prox_event to one. \n", __func__, cleardata);
		taos->prox_event = 1;
		mutex_unlock(&taos->prox_mutex);
		return (1);	
	}
		
	taos->prox_data = chdata[5];
	taos->prox_data <<= 8;
	taos->prox_data |= chdata[4];

	gprintk(" taos->prox_data = [%d (0x%x)]\n", (int) taos->prox_data, (int) taos->prox_data);

	if ( taos->prox_data > TAOS_PRX_THRES_IN)
		taos->prox_event = 0;
	if ( taos->prox_data < TAOS_PRX_THRES_OUT)
		taos->prox_event = 1;
		
	mutex_unlock(&taos->prox_mutex);
	return (ret);
}


static void taos_work_func_light(struct work_struct *work)
{
	struct taos_data *taos = container_of(work, struct taos_data,
					      work_light);
	int adc = lightsensor_get_adcvalue(taos);

	//gprintk("(taos_work_func_light input_report_abs) light adc => [%d]\n", adc);
	
	input_report_abs(taos->light_input_dev, ABS_MISC, adc);
	input_sync(taos->light_input_dev);
}

static void taos_work_func_prox(struct work_struct *work)
{
	int i=0;
	int val = 0;
	int ret= 0;
	u8 prox_int_thresh[4];
	u8 temp;

	struct taos_data *taos = container_of(work,struct taos_data,
		work_prox);

	gprintk("\n");

	/* read proximity value. */
	if ((ret = taos_prox_get_dist(taos)) < 0) 
	{
		gprintk( "call to prox_poll() failed in taos_prox_poll_timer_func()\n");
		return;
	}
	
#if 1	
	if ( taos->prox_event ) {
		/* OUT, try to check IN */
		/* Condition for detecting IN */
		prox_int_thresh[0] = (0x0) 			& 0xff;
		prox_int_thresh[1] = (0x0 >> 8) 		& 0xff;
		prox_int_thresh[2] = (TAOS_PRX_THRES_IN)	 	& 0xff;
		prox_int_thresh[3] = (TAOS_PRX_THRES_IN >> 8) 	& 0xff;
		val = 1;	/* 0 is close, 1 is far */
		
	} else {
		/* IN, try to check OUT */
		/* Condition for detecting OUT */
		prox_int_thresh[0] = (TAOS_PRX_THRES_OUT) 			& 0xff;
		prox_int_thresh[1] = (TAOS_PRX_THRES_OUT >> 8) 		& 0xff;
		prox_int_thresh[2] = (0x0)	 					& 0xff;
		prox_int_thresh[3] = (0x0 >> 8) 				& 0xff;	
		val = 0;	/* 0 is close, 1 is far */
	}

#else
	switch ( taos->taos_prox_dist ) /* HERE : taos_prox_dist => PREVIOUS STATUS */
	{
	case TAOS_PRX_DIST_IN:	
		gprintk("TAOS_PRX_DIST_IN  ==> OUT \n");

		/* Condition for detecting IN */
		prox_int_thresh[0] = (0x0) 			& 0xff;
		prox_int_thresh[1] = (0x0 >> 8) 		& 0xff;
		prox_int_thresh[2] = (TAOS_PRX_THRES_IN)	 	& 0xff;
		prox_int_thresh[3] = (TAOS_PRX_THRES_IN >> 8) 	& 0xff;

		/* Current status */
		taos->taos_prox_dist = TAOS_PRX_DIST_OUT;	  /* HERE : taos_prox_dist => CURRENT STATUS */
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
		taos->taos_prox_dist = TAOS_PRX_DIST_IN;	 /* HERE : taos_prox_dist => CURRENT STATUS */
		break;
	}
#endif	
	for (i = 0; i < 4; i++) {
		ret = opt_i2c_write(taos, (CMD_REG|(PRX_MINTHRESHLO + i)), &prox_int_thresh[i]);
		if ( ret < 0) {
			gprintk( "i2c_smbus_write_byte_data failed in taos_chip_on, err = %d\n", ret);
		}
	}	
	
	/* Interrupt clearing */
	temp = (CMD_REG|CMD_SPL_FN|CMD_PROXALS_INTCLR);
	ret = opt_i2c_write_command(taos, temp);

	if ( ret < 0) {
		gprintk( "i2c_smbus_write_byte_data failed in taos_irq_handler(), err = %d\n", ret);
	}	

#if 0
	/* decision part */
	switch ( taos->taos_prox_dist )
	{
	case TAOS_PRX_DIST_IN:
		val = 0;	/* 0 is close, 1 is far */
		break;
		
	case TAOS_PRX_DIST_OUT:
	case TAOS_PRX_DIST_UNKNOWN:
	default:
		val = 1;	/* 0 is close, 1 is far */
		break;
	}
#endif	
	printk("(taos_work_func_prox) input_report_abs => prox val = [%d]\n", val);

	input_report_abs(taos->proximity_input_dev, ABS_DISTANCE, val);
	input_sync(taos->proximity_input_dev);

	return;
}


/* This function is for light sensor.  It operates every a few seconds.
 * It asks for work to be done on a thread because i2c needs a thread
 * context (slow and blocking) and then reschedules the timer to run again.
 */
static enum hrtimer_restart taos_timer_func(struct hrtimer *timer)
{
	struct taos_data *taos = container_of(timer, struct taos_data, timer);
	queue_work(taos->wq, &taos->work_light);
	hrtimer_forward_now(&taos->timer, taos->light_poll_delay);
	return HRTIMER_RESTART;
}

/* interrupt happened due to transition/change of near/far proximity state */
irqreturn_t taos_irq_handler(int irq, void *data)
{
	struct taos_data *ip = data;
	u8 setting;
	
	gprintk("taos interrupt handler is called\n");
	
	queue_work(ip->wq, &ip->work_prox);
	wake_lock_timeout(&ip->prx_wake_lock, 5*HZ);

	return IRQ_HANDLED;
}

static int taos_setup_irq(struct taos_data *taos)
{
	int rc = -EIO;
	struct taos_platform_data *pdata = taos->pdata;
	int irq;

	taos_dbgmsg("start\n");	

	rc = gpio_request(pdata->als_int, "gpio_proximity_out");
	if (rc < 0) {
		pr_err("%s: gpio %d request failed (%d)\n",
			__func__, pdata->als_int, rc);
		return rc;
	}

	rc = gpio_direction_input(pdata->als_int);
	if (rc < 0) {
		pr_err("%s: failed to set gpio %d as input (%d)\n",
			__func__, pdata->als_int, rc);
		goto err_gpio_direction_input;
	}

	irq = gpio_to_irq(pdata->als_int);

	rc = request_threaded_irq(irq, NULL,
			 taos_irq_handler,
			 IRQF_TRIGGER_FALLING,
			 "proximity_int",
			 taos);
	if (rc < 0) {
		pr_err("%s: request_irq(%d) failed for gpio %d (%d)\n",
			__func__, irq,
			pdata->als_int, rc);
		goto err_request_irq;
	}

	/* start with interrupts disabled */
	disable_irq(irq);
	taos->irq = irq;

	taos_dbgmsg("success\n");

	goto done;

err_request_irq:
err_gpio_direction_input:
	gpio_free(pdata->als_int);
done:
	return rc;
}

static ssize_t lightsensor_file_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	int adc = 0;
	int sum = 0;
	int i = 0;
	for (i = 0; i < 10; i++) {
		adc = lightsensor_get_adcvalue(taos);
		msleep(20);
		sum += adc;
	}
	adc = sum/10;

	return sprintf(buf, "%d\n", adc);
}

static DEVICE_ATTR(lightsensor_file_state, 0644, lightsensor_file_state_show,
	NULL);

static ssize_t proximity_adc_show(struct device *dev, struct device_attribute *attr, char *buf)	
{
	int i = 0;
	int j = 0;
	int ret = 0;
	int min = 0;
	int max = 0;
	int avg = 0;	
	int adc = 0;
	u8 status = 0;	
	u8 proxi_hi = 0;
	u8 proxi_low = 0;
	int proxi_adc = 0;
	u8 chdata[6];
	
	struct taos_data *taos = dev_get_drvdata(dev);

	printk("called %s \n",__func__);	
	min = max = avg = 0;
	
	for(i = 0; i<40; i++) {
	
		mutex_lock(&taos->prox_mutex);
		
		if ((ret = opt_i2c_read(taos, STATUS , &status))< 0) {
			printk( "opt_i2c_read() to STATUS regs failed in taos_prox_poll()\n");
			mutex_unlock(&taos->prox_mutex);
			return (ret);
		}	
		
		for (j = 0; j < 6; j++) {
			if ((ret = opt_i2c_read(taos,  (ALS_CHAN0LO + j),&chdata[j]))< 0) {
				printk( "opt_i2c_read() to (ALS_CHAN0LO + j) regs failed in taos_prox_poll()\n");
				mutex_unlock(&taos->prox_mutex);				
				return (ret);
			}
		}
				
		mutex_unlock(&taos->prox_mutex);

		adc = (int) chdata[5];
		adc <<=8;
		adc |= (int) chdata[4];

		printk("[%s][%d] proximity adc = %d\n", __func__, i, adc);

		if(i == 0)	
			min = max = adc;
		if(adc > max)
			max =adc;
		if(adc < min)
			min = adc;

		avg = avg + adc;

		msleep(50);
	}

	avg =  avg / 40;
	proxi_adc = (min << 20) | (max << 10) | (avg);

	printk("[%s] min adc = %d, max adc = %d, avg adc = %d\n", __func__, min, max, avg);
	printk("[%s] proxi_adc = %x\n", __func__, proxi_adc);
	
	return sprintf(buf,"%d\n",proxi_adc);
}

static DEVICE_ATTR(proximity_adc,0644, proximity_adc_show, NULL);

static const struct file_operations light_fops = {
	.owner  = THIS_MODULE,
};

static struct miscdevice light_device = {
    .minor  = MISC_DYNAMIC_MINOR,
    .name   = "light",
    .fops   = &light_fops,
};

#if 1 /* Nathan : Interface for the declaration in mach-aries.c */
int taos_api_power_on(void)
{
	
	int ret =0; 
	
	if ( taos_global ) {
		printk("%s has been called \n", __func__);	
		ret = taos_chip_on(taos_global);
	} else {
		printk("%s has been called, but taos_global is NULL \n", __func__);
		ret = -1;
	}
	
	return ret;
}

int taos_api_power_off(void)
{
	
	int ret =0; 
	
	if ( taos_global ) {
		printk("%s has been called \n", __func__);	
		ret = taos_chip_off(taos_global);
	} else {
		printk("%s has been called, but taos_global is NULL \n", __func__);
		ret = -1;
	}
	
	return ret;
}

int taos_api_get_light_adcvalue(void)
{
	
	int ret =0; 
	
	if ( taos_global ) {
		printk("%s has been called. \n", __func__);	
		ret = lightsensor_get_adcvalue(taos_global);
	} else {
		printk("%s has been called, but taos_global is NULL. \n", __func__);
		ret = -1;
	}
	
	return ret;
}

EXPORT_SYMBOL(taos_api_power_on);
EXPORT_SYMBOL(taos_api_power_off);
EXPORT_SYMBOL(taos_api_get_light_adcvalue);
#endif

static int taos_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct input_dev *input_dev;
	struct taos_data *taos;
	struct taos_platform_data *pdata = client->dev.platform_data;

	if (!pdata) {
		pr_err("%s: missing pdata!\n", __func__);
		return ret;
	}
	if (!pdata->power || !pdata->light_adc_value) {
		pr_err("%s: incomplete pdata!\n", __func__);
		return ret;
	}
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c functionality check failed!\n", __func__);
		return ret;
	}

	taos = kzalloc(sizeof(struct taos_data), GFP_KERNEL);
	if (!taos) {
		pr_err("%s: failed to alloc memory for module data\n",
		       __func__);
		return -ENOMEM;
	}

	taos->pdata = pdata;
	taos->i2c_client = client;
	i2c_set_clientdata(client, taos);

	mutex_init(&taos->prox_mutex);
	taos->prox_event = 1; /* Init */

	taos_global = taos; /* quick fix */
	
	/* wake lock init */
	wake_lock_init(&taos->prx_wake_lock, WAKE_LOCK_SUSPEND,
		       "prx_wake_lock");
	mutex_init(&taos->power_lock);

	ret = taos_setup_irq(taos);
	if (ret) {
		pr_err("%s: could not setup irq\n", __func__);
		goto err_setup_irq;
	}

	/* allocate proximity input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		goto err_input_allocate_device_proximity;
	}
	taos->proximity_input_dev = input_dev;
	input_set_drvdata(input_dev, taos);
	input_dev->name = "proximity_sensor";
	input_set_capability(input_dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	/* nat : set the default value */
	input_report_abs(taos->proximity_input_dev, ABS_DISTANCE, 1);
	input_sync(taos->proximity_input_dev);	

	taos_dbgmsg("registering proximity input device\n");
	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err("%s: could not register input device\n", __func__);
		input_free_device(input_dev);
		goto err_input_register_device_proximity;
	}
	ret = sysfs_create_group(&input_dev->dev.kobj,
				 &proximity_attribute_group);
	if (ret) {
		pr_err("%s: could not create sysfs group\n", __func__);
		goto err_sysfs_create_group_proximity;
	}

	/* hrtimer settings.  we poll for light values using a timer. */
	hrtimer_init(&taos->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	taos->light_poll_delay = ns_to_ktime(200 * NSEC_PER_MSEC);
	taos->timer.function = taos_timer_func;

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	taos->wq = create_singlethread_workqueue("taos_wq");
	if (!taos->wq) {
		ret = -ENOMEM;
		pr_err("%s: could not create workqueue\n", __func__);
		goto err_create_workqueue;
	}
	/* this is the thread function we run on the work queue */
	INIT_WORK(&taos->work_light, taos_work_func_light);
	INIT_WORK(&taos->work_prox, taos_work_func_prox);

	/* allocate lightsensor-level input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		ret = -ENOMEM;
		goto err_input_allocate_device_light;
	}
	input_set_drvdata(input_dev, taos);
	input_dev->name = "light_sensor";
	input_set_capability(input_dev, EV_ABS, ABS_MISC);
	input_set_abs_params(input_dev, ABS_MISC, 0, 1, 0, 0);

	taos_dbgmsg("registering lightsensor-level input device\n");
	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err("%s: could not register input device\n", __func__);
		input_free_device(input_dev);
		goto err_input_register_device_light;
	}
	taos->light_input_dev = input_dev;
	ret = sysfs_create_group(&input_dev->dev.kobj,
				 &light_attribute_group);
	if (ret) {
		pr_err("%s: could not create sysfs group\n", __func__);
		goto err_sysfs_create_group_light;
	}

	/* set sysfs for light sensor */

	ret = misc_register(&light_device);
	if (ret)
		pr_err(KERN_ERR "misc_register failed - light\n");

	taos->lightsensor_class = class_create(THIS_MODULE, "lightsensor");
	if (IS_ERR(taos->lightsensor_class))
		pr_err("Failed to create class(lightsensor)!\n");

	taos->switch_cmd_dev = device_create(taos->lightsensor_class,
		NULL, 0, NULL, "switch_cmd");
	if (IS_ERR(taos->switch_cmd_dev))
		pr_err("Failed to create device(switch_cmd_dev)!\n");

	if (device_create_file(taos->switch_cmd_dev,
		&dev_attr_lightsensor_file_state) < 0)
		pr_err("Failed to create device file(%s)!\n",
			dev_attr_lightsensor_file_state.attr.name);

	/* set sysfs for proximity sensor */
	if (device_create_file(taos->switch_cmd_dev,
			&dev_attr_proximity_adc) < 0)
			pr_err("Failed to create device file(%s)!\n",
				 dev_attr_proximity_adc.attr.name);
	
	dev_set_drvdata(taos->switch_cmd_dev, taos);
	goto done;

	/* error, unwind it all */
err_sysfs_create_group_light:
	input_unregister_device(taos->light_input_dev);
err_input_register_device_light:
err_input_allocate_device_light:
	destroy_workqueue(taos->wq);
err_create_workqueue:
	sysfs_remove_group(&taos->proximity_input_dev->dev.kobj,
			   &proximity_attribute_group);
err_sysfs_create_group_proximity:
	input_unregister_device(taos->proximity_input_dev);
err_input_register_device_proximity:
err_input_allocate_device_proximity:
	free_irq(taos->irq, 0);
	gpio_free(taos->pdata->als_int);
err_setup_irq:
	mutex_destroy(&taos->power_lock);
	wake_lock_destroy(&taos->prx_wake_lock);
	kfree(taos);
done:
	return ret;
}

static int taos_suspend(struct device *dev)
{
	/* We disable power only if proximity is disabled.  If proximity
	   is enabled, we leave power on because proximity is allowed
	   to wake up device.  We remove power without changing
	   gp2a->power_state because we use that state in resume.
	*/
	struct i2c_client *client = to_i2c_client(dev);
	struct taos_data *taos = i2c_get_clientdata(client);

	gprintk("[T759 taos_suspend] taos->power_state=[0x%x], LIGHT_ENABLE=[0x%x]\n", taos->power_state, LIGHT_ENABLED);
	
	if (taos->power_state & LIGHT_ENABLED)
		taos_light_disable(taos);
	if (taos->power_state == LIGHT_ENABLED)
		taos->pdata->power(false);
	return 0;
}

static int taos_resume(struct device *dev)
{
	/* Turn power back on if we were before suspend. */
	struct i2c_client *client = to_i2c_client(dev);
	struct taos_data *taos = i2c_get_clientdata(client);

	gprintk("[T759 taos_resume] taos->power_state=[0x%x], LIGHT_ENABLE=[0x%x]\n", taos->power_state, LIGHT_ENABLED);

	if (taos->power_state == LIGHT_ENABLED)
		taos->pdata->power(true);
	if (taos->power_state & LIGHT_ENABLED)
		taos_light_enable(taos);
	return 0;
}

static int taos_i2c_remove(struct i2c_client *client)
{
	struct taos_data *taos = i2c_get_clientdata(client);
	sysfs_remove_group(&taos->light_input_dev->dev.kobj,
			   &light_attribute_group);
	input_unregister_device(taos->light_input_dev);
	sysfs_remove_group(&taos->proximity_input_dev->dev.kobj,
			   &proximity_attribute_group);
	input_unregister_device(taos->proximity_input_dev);
	free_irq(taos->irq, NULL);
	gpio_free(taos->pdata->als_int);
	
	if (taos->power_state) {
		taos->power_state = 0;
		if (taos->power_state & LIGHT_ENABLED)
			taos_light_disable(taos);
		taos->pdata->power(false);
	}
	destroy_workqueue(taos->wq);
	mutex_destroy(&taos->power_lock);
	wake_lock_destroy(&taos->prx_wake_lock);
	kfree(taos);
	return 0;
}

static const struct i2c_device_id taos_device_id[] = {
	{"gp2a", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, taos_device_id);

static const struct dev_pm_ops taos_pm_ops = {
	.suspend = taos_suspend,
	.resume = taos_resume
};

static struct i2c_driver taos_i2c_driver = {
	.driver = {
		.name = "gp2a",
		.owner = THIS_MODULE,
		.pm = &taos_pm_ops
	},
	.probe		= taos_i2c_probe,
	.remove		= taos_i2c_remove,
	.id_table	= taos_device_id,
};


static int __init taos_init(void)
{
	return i2c_add_driver(&taos_i2c_driver);
}

static void __exit taos_exit(void)
{
	i2c_del_driver(&taos_i2c_driver);
}

module_init(taos_init);
module_exit(taos_exit);

MODULE_AUTHOR("cs201.lee@samsung.com");
MODULE_DESCRIPTION("Optical Sensor driver for taos");
MODULE_LICENSE("GPL");
