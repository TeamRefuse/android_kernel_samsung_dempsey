/*
 * Copyright 2006-2010, Cypress Semiconductor Corporation.
 * Copyright (C) 2010, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor
 * Boston, MA  02110-1301, USA.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/earlysuspend.h>
#include <linux/input/cypress-touchkey.h>
#if defined CONFIG_S5PC110_DEMPSEY_BOARD 
#include <linux/mfd/max8998.h> 
#include <linux/regulator/consumer.h>
#endif

#ifdef CONFIG_GENERIC_BLN
#include <linux/bln.h>
#endif

#define TOUCH_UPDATE
#if defined(TOUCH_UPDATE)
#include <linux/irq.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <asm/gpio.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <mach/gpio-aries.h>
#endif
#define SCANCODE_MASK		0x07
#define UPDOWN_EVENT_MASK	0x08
#define ESD_STATE_MASK		0x10

#define BACKLIGHT_ON		0x1
#define BACKLIGHT_OFF		0x2

#define DEVICE_NAME "melfas_touchkey"
#if defined CONFIG_S5PC110_DEMPSEY_BOARD 

int touchkey_ldo_on(bool on)
{
	struct regulator *regulator;

	if (on) {
		regulator = regulator_get(NULL, "touch");
		if (IS_ERR(regulator))
			return 0;
		regulator_enable(regulator);
		regulator_put(regulator);
	} else {
		regulator = regulator_get(NULL, "touch");
		if (IS_ERR(regulator))
			return 0;
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);
	}

	return 1;
}

#endif


struct cypress_touchkey_devdata {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct touchkey_platform_data *pdata;
	struct early_suspend early_suspend;
	u8 backlight_on;
	u8 backlight_off;
	bool is_dead;
	bool is_powering_on;
	bool has_legacy_keycode;
};

static struct cypress_touchkey_devdata *blndevdata;

static int i2c_touchkey_read_byte(struct cypress_touchkey_devdata *devdata,
					u8 *val)
{
	int ret;
	int retry = 2;

	while (true) {
		ret = i2c_smbus_read_byte(devdata->client);
		if (ret >= 0) {
			*val = ret;
			return 0;
		}

		dev_err(&devdata->client->dev, "i2c read error\n");
		if (!retry--)
			break;
		msleep(10);
	}

	return ret;
}

static int i2c_touchkey_write_byte(struct cypress_touchkey_devdata *devdata,
					u8 val)
{
	int ret;
	int retry = 2;

	while (true) {
		ret = i2c_smbus_write_byte(devdata->client, val);
		if (!ret)
			return 0;

		dev_err(&devdata->client->dev, "i2c write error\n");
		if (!retry--)
			break;
		msleep(10);
	}

	return ret;
}

static void all_keys_up(struct cypress_touchkey_devdata *devdata)
{
	int i;

	for (i = 0; i < devdata->pdata->keycode_cnt; i++)
		input_report_key(devdata->input_dev,
						devdata->pdata->keycode[i], 0);

	input_sync(devdata->input_dev);
}

static int recovery_routine(struct cypress_touchkey_devdata *devdata)
{
	int ret = -1;
	int retry = 10;
	u8 data;
	int irq_eint;

	if (unlikely(devdata->is_dead)) {
		dev_err(&devdata->client->dev, "%s: Device is already dead, "
				"skipping recovery\n", __func__);
		return -ENODEV;
	}

	irq_eint = devdata->client->irq;

	all_keys_up(devdata);

	disable_irq_nosync(irq_eint);
	while (retry--) {
		devdata->pdata->touchkey_onoff(TOUCHKEY_OFF);
		devdata->pdata->touchkey_onoff(TOUCHKEY_ON);
		ret = i2c_touchkey_read_byte(devdata, &data);
		if (!ret) {
			enable_irq(irq_eint);
			goto out;
		}
		dev_err(&devdata->client->dev, "%s: i2c transfer error retry = "
				"%d\n", __func__, retry);
	}
	devdata->is_dead = true;
	devdata->pdata->touchkey_onoff(TOUCHKEY_OFF);
	dev_err(&devdata->client->dev, "%s: touchkey died\n", __func__);
out:
	return ret;
}

extern unsigned int touch_state_val;
extern void TSP_forced_release(void);

static irqreturn_t touchkey_interrupt_thread(int irq, void *touchkey_devdata)
{
	u8 data;
	int i;
	int ret;
	int scancode;
	struct cypress_touchkey_devdata *devdata = touchkey_devdata;

	ret = i2c_touchkey_read_byte(devdata, &data);
	if (ret || (data & ESD_STATE_MASK)) {
		ret = recovery_routine(devdata);
		if (ret) {
			dev_err(&devdata->client->dev, "%s: touchkey recovery "
					"failed!\n", __func__);
			goto err;
		}
	}

	if (data & UPDOWN_EVENT_MASK) {
		scancode = (data & SCANCODE_MASK) - 1;
		input_report_key(devdata->input_dev,
			devdata->pdata->keycode[scancode], 0);
		input_sync(devdata->input_dev);
		dev_dbg(&devdata->client->dev, "[release] cypress touch key : %d \n",
			devdata->pdata->keycode[scancode]);
	} else {
		if (!touch_state_val) {
			if (devdata->has_legacy_keycode) {
				scancode = (data & SCANCODE_MASK) - 1;
				if (scancode < 0 || scancode >= devdata->pdata->keycode_cnt) {
					dev_err(&devdata->client->dev, "%s: scancode is out of "
						"range\n", __func__);
					goto err;
				}
				if (scancode == 1)
					TSP_forced_release();
				input_report_key(devdata->input_dev,
					devdata->pdata->keycode[scancode], 1);
				dev_dbg(&devdata->client->dev, "[press] cypress touch key : %d \n",
					devdata->pdata->keycode[scancode]);
			} else {
				for (i = 0; i < devdata->pdata->keycode_cnt; i++)
				input_report_key(devdata->input_dev,
					devdata->pdata->keycode[i],
					!!(data & (1U << i)));
			}
			input_sync(devdata->input_dev);
		}
	}
err:
	return IRQ_HANDLED;
}

static irqreturn_t touchkey_interrupt_handler(int irq, void *touchkey_devdata)
{
	struct cypress_touchkey_devdata *devdata = touchkey_devdata;

	if (devdata->is_powering_on) {
		dev_dbg(&devdata->client->dev, "%s: ignoring spurious boot "
					"interrupt\n", __func__);
		return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cypress_touchkey_early_suspend(struct early_suspend *h)
{
	struct cypress_touchkey_devdata *devdata =
		container_of(h, struct cypress_touchkey_devdata, early_suspend);

	devdata->is_powering_on = true;

	if (unlikely(devdata->is_dead))
		return;

	disable_irq(devdata->client->irq);

#ifdef CONFIG_GENERIC_BLN
/*
 * Disallow powering off the touchkey controller
 * while a led notification is ongoing
 */
    if(!bln_is_ongoing())
#endif
	devdata->pdata->touchkey_onoff(TOUCHKEY_OFF);

	all_keys_up(devdata);

    #if defined(CONFIG_S5PC110_DEMPSEY_BOARD)
	touchkey_ldo_on(0);
    #endif

}

static void cypress_touchkey_early_resume(struct early_suspend *h)
{
	struct cypress_touchkey_devdata *devdata =
		container_of(h, struct cypress_touchkey_devdata, early_suspend);
	#if defined(CONFIG_S5PC110_DEMPSEY_BOARD)
		touchkey_ldo_on(1);
    #endif

	devdata->pdata->touchkey_onoff(TOUCHKEY_ON);
#if 0
	if (i2c_touchkey_write_byte(devdata, devdata->backlight_on)) {
		devdata->is_dead = true;
		devdata->pdata->touchkey_onoff(TOUCHKEY_OFF);
		dev_err(&devdata->client->dev, "%s: touch keypad not responding"
				" to commands, disabling\n", __func__);
		return;
	}
#endif
	devdata->is_dead = false;
	enable_irq(devdata->client->irq);
	devdata->is_powering_on = false;
}
#endif

static void enable_touchkey_backlights(void){
       i2c_touchkey_write_byte(blndevdata, blndevdata->backlight_on);
}

static void disable_touchkey_backlights(void){
       i2c_touchkey_write_byte(blndevdata, blndevdata->backlight_off);
}

static void cypress_touchkey_enable_led_notification(void){
	/* is_powering_on signals whether touchkey lights are used for touchmode */
	if (blndevdata->is_powering_on){
		/* reconfigure gpio for sleep mode */
		blndevdata->pdata->touchkey_sleep_onoff(TOUCHKEY_ON);
        #if defined(CONFIG_S5PC110_DEMPSEY_BOARD)
	    touchkey_ldo_on(1);
        #endif
        msleep(50);
		/*
		 * power on the touchkey controller
		 * This is actually not needed, but it is intentionally
		 * left for the case that the early_resume() function
		 * did not power on the touchkey controller for some reasons
		 */
		blndevdata->pdata->touchkey_onoff(TOUCHKEY_ON);

		/* write to i2cbus, enable backlights */
		enable_touchkey_backlights();
	}
	else
		pr_info("%s: cannot set notification led, touchkeys are enabled\n",__FUNCTION__);
}

static void cypress_touchkey_disable_led_notification(void){
	/*
	 * reconfigure gpio for sleep mode, this has to be done
	 * independently from the power status
	 */
	blndevdata->pdata->touchkey_sleep_onoff(TOUCHKEY_OFF);

	/* if touchkeys lights are not used for touchmode */
	if (blndevdata->is_powering_on){
		disable_touchkey_backlights();

		#if 0
		/*
		 * power off the touchkey controller
		 * This is actually not needed, the early_suspend function
		 * should take care of powering off the touchkey controller
		 */
		blndevdata->pdata->touchkey_onoff(TOUCHKEY_OFF);
		#endif
	}
}

static struct bln_implementation cypress_touchkey_bln = {
	.enable = cypress_touchkey_enable_led_notification,
	.disable = cypress_touchkey_disable_led_notification,
};


#if defined(TOUCH_UPDATE)
extern int get_touchkey_firmware(char *version);
extern int ISSP_main();
static int touchkey_update_status = 0;
struct work_struct touch_update_work;
struct workqueue_struct *touchkey_wq;
#define IRQ_TOUCH_INT (IRQ_EINT_GROUP22_BASE + 1)
#define KEYCODE_REG 0x00
#define I2C_M_WR 0              /* for i2c */

static void init_hw(void)
{
#if !defined (CONFIG_S5PC110_DEMPSEY_BOARD) 
	gpio_direction_output(_3_GPIO_TOUCH_EN, 1);
#else		
	touchkey_ldo_on(1);
#endif
	msleep(200);
	s3c_gpio_setpull(_3_GPIO_TOUCH_INT, S3C_GPIO_PULL_NONE);
	set_irq_type(IRQ_TOUCH_INT, IRQF_TRIGGER_FALLING);
	s3c_gpio_cfgpin(_3_GPIO_TOUCH_INT, _3_GPIO_TOUCH_INT_AF);
}

int touchkey_update_open(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t touchkey_update_read(struct file * filp, char *buf, size_t count,
			     loff_t * f_pos)
{
	char data[3] = { 0, };

	get_touchkey_firmware(data);
	put_user(data[1], buf);

	return 1;
}

#if 0
extern int mcsdl_download_binary_file(unsigned char *pData,
				      unsigned short nBinary_length);
ssize_t touchkey_update_write(struct file *filp, const char *buf, size_t count,
			      loff_t * f_pos)
{
	unsigned char *pdata;

	disable_irq(IRQ_TOUCH_INT);
	printk("count = %d\n", count);
	pdata = kzalloc(count, GFP_KERNEL);
	if (pdata == NULL) {
		printk("memory allocate fail \n");
		return 0;
	}
	if (copy_from_user(pdata, buf, count)) {
		printk("copy fail \n");
		kfree(pdata);
		return 0;
	}

	mcsdl_download_binary_file((unsigned char *)pdata,
				   (unsigned short)count);
	kfree(pdata);

	init_hw();
	enable_irq(IRQ_TOUCH_INT);
	return count;
}
#endif

int touchkey_update_release(struct inode *inode, struct file *filp)
{
	return 0;
}

struct file_operations touchkey_update_fops = {
	.owner = THIS_MODULE,
	.read = touchkey_update_read,
      //.write   = touchkey_update_write,
	.open = touchkey_update_open,
	.release = touchkey_update_release,
};

static struct miscdevice touchkey_update_device = {
	.minor = MISC_DYNAMIC_MINOR,
	//.name = "melfas_touchkey",DEVICE_NAME
	.name = DEVICE_NAME,
	.fops = &touchkey_update_fops,
};

static int i2c_touchkey_read(struct cypress_touchkey_devdata *devdata, 
					u8 reg, u8 * val, unsigned int len)
{
	int err;
	int retry = 10;
	struct i2c_msg msg[1];

	while (retry--) {
		msg->addr = devdata->client->addr;
		msg->flags = I2C_M_RD;
		msg->len = len;
		msg->buf = val;
		err = i2c_transfer(devdata->client->adapter, msg, 1);
		if (err >= 0) {
			return 0;
		}
		printk("%s %d i2c transfer error\n", __func__, __LINE__);	/* add by inter.park */
		mdelay(10);
	}
	return err;

}

static ssize_t touch_version_read(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	char data[3] = { 0, };
	int count;
	struct cypress_touchkey_devdata *devdata;

	devdata = dev->platform_data;
	init_hw();

	if (get_touchkey_firmware(data) != 0)
		i2c_touchkey_read(devdata, KEYCODE_REG, data, 3);
	count = sprintf(buf, "0x%x\n", data[1]);

	printk("touch_version_read 0x%x\n", data[1]);
	return count;
}

static ssize_t touch_version_write(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	printk("input data --> %s\n", buf);

	return size;
}

void touchkey_update_func(struct work_struct *p)
{
	int retry = 10;
	touchkey_update_status = 1;
	printk("%s start\n", __FUNCTION__);
	while (retry--) {
		if (ISSP_main() == 0) {
			touchkey_update_status = 0;
			printk("touchkey_update succeeded\n");
			enable_irq(IRQ_TOUCH_INT);
			return;
		}
	}
	touchkey_update_status = -1;
	printk("touchkey_update failed\n");
	return;
}

static ssize_t touch_update_write(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	printk("touchkey firmware update \n");
	if (*buf == 'S') {
		disable_irq(IRQ_TOUCH_INT);
		INIT_WORK(&touch_update_work, touchkey_update_func);
		queue_work(touchkey_wq, &touch_update_work);
	}
	return size;
}

static ssize_t touch_update_read(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int count = 0;

	printk("touch_update_read: touchkey_update_status %d\n",
	       touchkey_update_status);

	if (touchkey_update_status == 0) {
		count = sprintf(buf, "PASS\n");
	} else if (touchkey_update_status == 1) {
		count = sprintf(buf, "Downloading\n");
	} else if (touchkey_update_status == -1) {
		count = sprintf(buf, "Fail\n");
	}

	return count;
}

static int i2c_touchkey_write(struct cypress_touchkey_devdata *devdata, 
						u8 * val, unsigned int len)
{
	int err;
	struct i2c_msg msg[1];
	unsigned char data[2];
	int retry = 2;

	while (retry--) {
		data[0] = *val;
		msg->addr = devdata->client->addr;
		msg->flags = I2C_M_WR;
		msg->len = len;
		msg->buf = data;
		err = i2c_transfer(devdata->client->adapter, msg, 1);
		if (err >= 0)
			return 0;
		printk(KERN_DEBUG "%s %d i2c transfer error\n", __func__,
		       __LINE__);
		mdelay(10);
	}
	return err;
}

struct cypress_touchkey_devdata *tempdata;
static ssize_t touch_led_control(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	u8 data = 0x10;
	if (sscanf(buf, "%d\n", &data) == 1) {
		if (!tempdata->is_powering_on) {
			//printk(KERN_DEBUG "touch_led_control: %d \n", data);
			i2c_touchkey_write(tempdata, &data, sizeof(u8));
		}
	} else
		printk("touch_led_control Error\n");

	return size;
}

static ssize_t touchkey_enable_disable(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
#if 0
	printk("touchkey_enable_disable %c \n", *buf);
	if (*buf == '0') {
		set_touchkey_debug('d');
		disable_irq(IRQ_TOUCH_INT);
		gpio_direction_output(_3_GPIO_TOUCH_EN, 0);
		touchkey_enable = -2;
	} else if (*buf == '1') {
		if (touchkey_enable == -2) {
			set_touchkey_debug('e');
			gpio_direction_output(_3_GPIO_TOUCH_EN, 1);
			touchkey_enable = 1;
			enable_irq(IRQ_TOUCH_INT);
		}
	} else {
		printk("touchkey_enable_disable: unknown command %c \n", *buf);
	}
#endif
	return size;
}

static DEVICE_ATTR(touch_version, 0664,
		   touch_version_read, touch_version_write);
static DEVICE_ATTR(touch_update, 0664,
		   touch_update_read, touch_update_write);
static DEVICE_ATTR(brightness, 0664, NULL,
		   touch_led_control);
static DEVICE_ATTR(enable_disable, 0664, NULL,
		   touchkey_enable_disable);
#endif
static int cypress_touchkey_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct input_dev *input_dev;
	struct cypress_touchkey_devdata *devdata;
	u8 data[3];
	int err;
	int cnt;
#if defined(TOUCH_UPDATE)
	int ret;
	int retry = 10;
#endif

	if (!dev->platform_data) {
		dev_err(dev, "%s: Platform data is NULL\n", __func__);
		return -EINVAL;
	}

	devdata = kzalloc(sizeof(*devdata), GFP_KERNEL);
	if (devdata == NULL) {
		dev_err(dev, "%s: failed to create our state\n", __func__);
		return -ENODEV;
	}

	devdata->client = client;
	i2c_set_clientdata(client, devdata);

	devdata->pdata = client->dev.platform_data;
#if defined(TOUCH_UPDATE)
	tempdata = devdata;
#endif
	if (!devdata->pdata->keycode) {
		dev_err(dev, "%s: Invalid platform data\n", __func__);
		err = -EINVAL;
		goto err_null_keycodes;
	}

	strlcpy(devdata->client->name, DEVICE_NAME, I2C_NAME_SIZE);

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		goto err_input_alloc_dev;
	}

	devdata->input_dev = input_dev;
	dev_set_drvdata(&input_dev->dev, devdata);
	input_dev->name = DEVICE_NAME;
	input_dev->id.bustype = BUS_HOST;

	for (cnt = 0; cnt < devdata->pdata->keycode_cnt; cnt++)
		input_set_capability(input_dev, EV_KEY,
					devdata->pdata->keycode[cnt]);

	err = input_register_device(input_dev);
	if (err)
		goto err_input_reg_dev;

	devdata->is_powering_on = true;

	devdata->pdata->touchkey_onoff(TOUCHKEY_ON);

	err = i2c_master_recv(client, data, sizeof(data));
	if (err < sizeof(data)) {
		if (err >= 0)
			err = -EIO;
		dev_err(dev, "%s: error reading hardware version\n", __func__);
		goto err_read;
	}

	dev_info(dev, "%s: hardware rev1 = %#02x, rev2 = %#02x\n", __func__,
				data[1], data[2]);

	devdata->backlight_on = BACKLIGHT_ON;
	devdata->backlight_off = BACKLIGHT_OFF;

	devdata->has_legacy_keycode = 1;

	err = i2c_touchkey_write_byte(devdata, devdata->backlight_on);
	if (err) {
		dev_err(dev, "%s: touch keypad backlight on failed\n",
				__func__);
		goto err_backlight_on;
	}

	if (request_threaded_irq(client->irq, touchkey_interrupt_handler,
				touchkey_interrupt_thread, IRQF_TRIGGER_FALLING,
				DEVICE_NAME, devdata)) {
		dev_err(dev, "%s: Can't allocate irq.\n", __func__);
		goto err_req_irq;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	devdata->early_suspend.suspend = cypress_touchkey_early_suspend;
	devdata->early_suspend.resume = cypress_touchkey_early_resume;
#endif
	register_early_suspend(&devdata->early_suspend);

	devdata->is_powering_on = false;

#ifdef CONFIG_GENERIC_BLN
    blndevdata = devdata;
    register_bln_implementation(&cypress_touchkey_bln);
#endif


#if defined(TOUCH_UPDATE)
	ret = misc_register(&touchkey_update_device);
	if (ret) {
		printk("%s misc_register fail\n", __FUNCTION__);
	}

	if (device_create_file
	    (touchkey_update_device.this_device, &dev_attr_touch_version) < 0) {
		printk("%s device_create_file fail dev_attr_touch_version\n",
		       __FUNCTION__);
		pr_err("Failed to create device file(%s)!\n",
		       dev_attr_touch_version.attr.name);
	}

	if (device_create_file
	    (touchkey_update_device.this_device, &dev_attr_touch_update) < 0) {
		printk("%s device_create_file fail dev_attr_touch_update\n",
		       __FUNCTION__);
		pr_err("Failed to create device file(%s)!\n",
		       dev_attr_touch_update.attr.name);
	}

	if (device_create_file
	    (touchkey_update_device.this_device, &dev_attr_brightness) < 0) {
		printk("%s device_create_file fail dev_attr_touch_update\n",
		       __FUNCTION__);
		pr_err("Failed to create device file(%s)!\n",
		       dev_attr_brightness.attr.name);
	}

	if (device_create_file
	    (touchkey_update_device.this_device,
	     &dev_attr_enable_disable) < 0) {
		printk("%s device_create_file fail dev_attr_touch_update\n",
		       __FUNCTION__);
		pr_err("Failed to create device file(%s)!\n",
		       dev_attr_enable_disable.attr.name);
	}

	touchkey_wq = create_singlethread_workqueue(DEVICE_NAME);
	if (!touchkey_wq)
		return -ENOMEM;

	while (retry--) {
		if (get_touchkey_firmware(data) == 0)	//melfas need delay for multiple read
			break;
	}
	printk("%s F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__,
	       data[1], data[2]);

	
#if defined CONFIG_S5PC110_DEMPSEY_BOARD 
	
	   // Firmware check & Update
	   if((data[1] < 0x21)){
		   touchkey_update_status=1;
		   while (retry--) {
			   if (ISSP_main() == 0) {
				   printk(KERN_ERR"[TOUCHKEY]Touchkey_update succeeded\n");
				   touchkey_update_status=0;
				   break;
			   }
			   printk(KERN_ERR"touchkey_update failed... retry...\n");
		  }
		   if (retry <= 0) {
			   // disable ldo11
			   touchkey_ldo_on(0);
			   touchkey_update_status=-1;
			   msleep(300);
	
		   }
	
		   init_hw();  //after update, re initalize.
	   	}
#endif

#endif

	return 0;

err_req_irq:
err_backlight_on:
err_read:
	devdata->pdata->touchkey_onoff(TOUCHKEY_OFF);
	input_unregister_device(input_dev);
	goto err_input_alloc_dev;
err_input_reg_dev:
	input_free_device(input_dev);
err_input_alloc_dev:
err_null_keycodes:
	kfree(devdata);
	return err;
}

static int __devexit i2c_touchkey_remove(struct i2c_client *client)
{
	struct cypress_touchkey_devdata *devdata = i2c_get_clientdata(client);

	unregister_early_suspend(&devdata->early_suspend);
	/* If the device is dead IRQs are disabled, we need to rebalance them */
	if (unlikely(devdata->is_dead))
		enable_irq(client->irq);
	else
		devdata->pdata->touchkey_onoff(TOUCHKEY_OFF);
	free_irq(client->irq, devdata);
	all_keys_up(devdata);
	input_unregister_device(devdata->input_dev);
 	
	#if defined(CONFIG_S5PC110_DEMPSEY_BOARD)
	touchkey_ldo_on(0);
	#endif
	
	kfree(devdata);
	return 0;
}

static const struct i2c_device_id cypress_touchkey_id[] = {
	{ CYPRESS_TOUCHKEY_DEV_NAME, 0 },
};

MODULE_DEVICE_TABLE(i2c, cypress_touchkey_id);

struct i2c_driver touchkey_i2c_driver = {
	.driver = {
		.name = "cypress_touchkey_driver",
	},
	.id_table = cypress_touchkey_id,
	.probe = cypress_touchkey_probe,
	.remove = __devexit_p(i2c_touchkey_remove),
};



static int __init touchkey_init(void)
{
	int ret = 0;
#if defined CONFIG_S5PC110_DEMPSEY_BOARD 
	touchkey_ldo_on(1);
#endif
	ret = i2c_add_driver(&touchkey_i2c_driver);
	if (ret)
		pr_err("%s: cypress touch keypad registration failed. (%d)\n",
				__func__, ret);

	return ret;
}

static void __exit touchkey_exit(void)
{
#if defined(TOUCH_UPDATE)
	misc_deregister(&touchkey_update_device);
#endif
	i2c_del_driver(&touchkey_i2c_driver);
}

late_initcall(touchkey_init);
module_exit(touchkey_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("@@@");
MODULE_DESCRIPTION("cypress touch keypad");
