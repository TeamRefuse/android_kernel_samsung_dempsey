/*
 * s6e63m0 AMOLED Panel Driver for the Samsung Universal board
 *
 * Derived from drivers/video/omap/lcd-apollon.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/tmd.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-fb.h>
#include <linux/earlysuspend.h>
#include <linux/pwm.h>
#include <mach/gpio-aries.h> /* nat : need to check*/

#define SLEEPMSEC	0x1000
#define ENDDEF			0x2000
#define DEFMASK		0xFF00
#define DIM_BL	20
#define MIN_BL	30
#define MAX_BL	255
#define LCD_TUNNING_VALUE 1		/* nat */

/*********** for debug **********************************************************/
#if 0
#define gprintk(fmt, x... ) printk("%s(%d): " fmt, __FUNCTION__ , __LINE__ , ## x)
#else
#define gprintk(x...) do { } while (0)
#endif
/*******************************************************************************/

#if defined (LCD_TUNNING_VALUE)
#define MAX_BRIGHTNESS_LEVEL 255 /* values received from platform */
#define LOW_BRIGHTNESS_LEVEL 30
#define DIM_BACKLIGHT_LEVEL 20	
#define MAX_BACKLIGHT_VALUE 159  /* values kernel tries to set. */
#define LOW_BACKLIGHT_VALUE 7
#define DIM_BACKLIGHT_VALUE 7	

static int s5p_bl_convert_to_tuned_value(int intensity);
#endif

#define LCD_TIMER_FOR_CHECKING_ESD 1 

#if defined(LCD_TIMER_FOR_CHECKING_ESD)
static struct s5p_lcd *gLcd;
static struct work_struct lcd_esd_work;
static struct timer_list lcd_esd_polling_timer;
static const unsigned int lcd_esd_polling_time = 1000;
static int lcd_esd_last_status = 1; /* 0 : normal, 1: something happen(default, print out first log) */
static int lcd_on_progress = 0;
static int lcd_off_progress = 0;
#endif


#define BACKLIGHT_SYSFS_INTERFACE 1 /* NAGSM_Android_SEL_KERNEL_Subhransu */

#if defined(BACKLIGHT_SYSFS_INTERFACE)
	struct class *ldi_class;
	struct device *ldi_dev;
	static int max_brightness = 0;
	static int saved_brightness = 0;	
#endif

static void update_brightness(struct s5p_lcd *lcd);
static void tl2796_ldi_standby_on(struct s5p_lcd *lcd);
static void tl2796_ldi_wakeup(struct s5p_lcd *lcd);


struct pwm_device	*backlight_pwm_dev; /* nat */
int bl_freq_count = 100000;

struct s5p_lcd {
	int ldi_enable;
	int bl;
	struct mutex	lock;
	struct device *dev;
	struct spi_device *g_spi;
	struct s5p_panel_data	*data;
	struct backlight_device *bl_dev;
	struct lcd_device *lcd_dev;

	struct early_suspend    early_suspend;
};

static int s6e63m0_spi_write_driver(struct s5p_lcd *lcd, u16 reg)
{
	u16 buf[1];
	int ret;
	struct spi_message msg;

	struct spi_transfer xfer = {
		.len	= 2,
		.tx_buf	= buf,
	};

	buf[0] = reg;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	ret = spi_sync(lcd->g_spi, &msg);

	if (ret < 0)
		pr_err("%s error\n", __func__);

	return ret ;
}

static void s6e63m0_panel_send_sequence(struct s5p_lcd *lcd,
	const u16 *wbuf)
{
	int i = 0;

	while ((wbuf[i] & DEFMASK) != ENDDEF) {
		if ((wbuf[i] & DEFMASK) != SLEEPMSEC) {
			s6e63m0_spi_write_driver(lcd, wbuf[i]);
			i += 1;
		} else {
			msleep(wbuf[i+1]);
			i += 2;
		}
	}
}

#if defined(BACKLIGHT_SYSFS_INTERFACE)
static ssize_t update_brightness_cmd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        return sprintf(buf,"%d\n", saved_brightness);
}
static ssize_t upadate_brightness_cmd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
        int brightness = 0;
        sscanf(buf, "%d", &brightness);

	if( !gLcd ) {
		printk(KERN_ERR "[LCD] (%s) gLcd is NULL. \n", __func__);
		return 0;
	}

	if ( ! (gLcd->ldi_enable) ) {
		printk(KERN_ERR "[LCD] (%s) LDI not enabled \n", __func__);
		return 0;
	}

	/* Sanity check */
	if(brightness < 0)
		brightness = 0;

	if(brightness > max_brightness)
		brightness = max_brightness;

	gLcd->bl = brightness;
	update_brightness(gLcd);
			
	saved_brightness = brightness;
	return 0;
}
static DEVICE_ATTR(update_brightness_cmd,0660, update_brightness_cmd_show, upadate_brightness_cmd_store);
#endif

#if defined(LCD_TIMER_FOR_CHECKING_ESD)
static void check_and_recover_esd_wq(struct work_struct *work)
{
		int ret = 0;
		u8 txbuf[1] = {0,};
		u8 rxbuf[1] = {0,};

		if ( !gLcd  ) {
			printk(KERN_ERR "[LCD] (%s) gLcd is NULL \n", __func__);
			return ;
		}
		
		mutex_lock(&gLcd->lock);
		
		s6e63m0_spi_write_driver(gLcd, 0xB2);		/* LDI Data Access */
		s6e63m0_spi_write_driver(gLcd, 0x100);		
		s6e63m0_spi_write_driver(gLcd, 0x54); 		/* read 0x54 h */
		
		if( !(gLcd->g_spi) )	{
			printk(KERN_ERR "[LCD] check_and_recover_esd_wq !(lcd.g_spi) \n");
			mutex_unlock(&gLcd->lock);	
			return;
		}	   		
		gLcd->g_spi->bits_per_word = 8;
		txbuf[0] = 0x21;	rxbuf[0] = 0x0;
		ret = spi_write_then_read(gLcd->g_spi, txbuf, sizeof(txbuf), rxbuf, sizeof(rxbuf));	
		gLcd->g_spi->bits_per_word = 16;

		mutex_unlock(&gLcd->lock);
		
		if ( rxbuf[0] == 0x00) {
			/* Normal */
			if ( lcd_esd_last_status == 1) {
				pr_info("[LCD] Normal (0x%x, 0x%x, 0x%d, 0x%d)\n", rxbuf[0], ret, lcd_on_progress, lcd_off_progress); 
			}
			lcd_esd_last_status = 0;
			return;
		} else {
			/* Got ESD */
			if ( lcd_on_progress || lcd_off_progress )	{
				/* LCD sequence is on process. */
				pr_info("[LCD] ESD - SPI is on operation, skip (0x%x, 0x%x, 0x%d, 0x%d)\n", rxbuf[0], ret, lcd_on_progress, lcd_off_progress); 
				lcd_esd_last_status = 1;
			} else {
				printk(KERN_INFO "[LCD] ESD - Restarting (0x%x, 0x%x, 0x%d, 0x%d)\n", rxbuf[0], ret, lcd_on_progress, lcd_off_progress); 
				mutex_lock(&gLcd->lock);
				tl2796_ldi_standby_on(gLcd);
				tl2796_ldi_wakeup(gLcd);
				mutex_unlock(&gLcd->lock);
				pr_info("[LCD] ESD - Restarting Done ... \n"); 
				lcd_esd_last_status = 1;
			}			
		}
}

static void lcd_esd_polling_timer_func(unsigned long unused)
{
	//pr_info("[BAT]:%s\n", __func__);

	schedule_work(&lcd_esd_work);
	mod_timer(&lcd_esd_polling_timer, jiffies + msecs_to_jiffies(lcd_esd_polling_time));
}

#endif

#if defined (LCD_TUNNING_VALUE)
static int s5p_bl_convert_to_tuned_value(int intensity)
{
	int tune_value;

	if(intensity >= LOW_BRIGHTNESS_LEVEL)
		tune_value = (intensity - LOW_BRIGHTNESS_LEVEL) * (MAX_BACKLIGHT_VALUE-LOW_BACKLIGHT_VALUE) / (MAX_BRIGHTNESS_LEVEL-LOW_BRIGHTNESS_LEVEL) + LOW_BACKLIGHT_VALUE;
	else if(intensity >= DIM_BACKLIGHT_LEVEL)
		tune_value = (intensity - DIM_BACKLIGHT_LEVEL) * (LOW_BACKLIGHT_VALUE-DIM_BACKLIGHT_VALUE) / (LOW_BRIGHTNESS_LEVEL-DIM_BACKLIGHT_LEVEL) + DIM_BACKLIGHT_VALUE;
	else if(intensity > 0)
		tune_value = (intensity) * (DIM_BACKLIGHT_VALUE) / (DIM_BACKLIGHT_LEVEL);
	else
		tune_value = intensity;

	gprintk("Passed value : [%d], Converted value : [%d]\n", intensity, tune_value);

	return tune_value;
}
#endif

static void update_brightness(struct s5p_lcd *lcd)
{
	struct s5p_panel_data *pdata = lcd->data;	
	int brightness =  lcd->bl;
	
#if defined (LCD_TUNNING_VALUE)
	int tunned_brightness = 0;
	tunned_brightness = s5p_bl_convert_to_tuned_value(brightness);
	pwm_config(backlight_pwm_dev, (bl_freq_count * tunned_brightness)/MAX_BL, bl_freq_count);
	pwm_enable(backlight_pwm_dev);
#else
	pwm_config(backlight_pwm_dev, (bl_freq_count * brightness)/255, bl_freq_count);
	pwm_enable(backlight_pwm_dev);
	/* gprintk("## brightness = [%ld], (bl_freq_count * brightness)/255 =[%ld], ret_val_pwm_config=[%ld] \n", brightness, (bl_freq_count * brightness)/255, ret_val_pwm_config ); */
#endif
	gprintk("Update status brightness[0~255]:(%d) \n", lcd->bl);
}

static void tl2796_ldi_sleep(struct s5p_lcd *lcd)
{
  /* SDX Signal "H" -> "L" */
  gprintk("gpio_get_value(GPIO_DISPLAY_SDX)=[%d]\n", gpio_get_value(GPIO_DISPLAY_SDX));
  //gpio_direction_output(GPIO_DISPLAY_SDX, 1);
  gpio_set_value(GPIO_DISPLAY_SDX, 0); 
  gprintk("gpio_get_value(GPIO_DISPLAY_SDX)=[%d]\n", gpio_get_value(GPIO_DISPLAY_SDX));
  /* Wait 85ms(min) == (Wait 5 frame(min)) */
  msleep(100);
  /* RGB signals OFF */
  /* tl2796_ldi_standby_off */
}

static void tl2796_ldi_wakeup(struct s5p_lcd *lcd)
{
	struct s5p_panel_data *pdata = lcd->data;

	/* tl2796_ldi_standby_on */
	/* SDX Signal "L" -> "H" */
	gprintk("gpio_get_value(GPIO_DISPLAY_SDX)=[%d]\n", gpio_get_value(GPIO_DISPLAY_SDX));
	//gpio_direction_output(GPIO_DISPLAY_SDX, 1);
	gpio_set_value(GPIO_DISPLAY_SDX, 1);   
	gprintk("gpio_get_value(GPIO_DISPLAY_SDX)=[%d]\n", gpio_get_value(GPIO_DISPLAY_SDX));
	/* Wait 4us(min) */
	msleep(1);  
	/* Input RGB signals (RGB I/F)   */
	/* Wait 17ms(typ) == (Wait 1 frame(typ)) */
	msleep(20); 
	/* LDI Initial CODE (Serial I/F) */

	s6e63m0_panel_send_sequence(lcd, pdata->standby_on);

	/* Wait 85ms(min) == (Wait 5 frame(min)) */
	msleep(140); 
}

static void tl2796_ldi_standby_on(struct s5p_lcd *lcd)
{
  /* Power on */
  /* Wait 30us(min) */
  msleep(1); 
  /* Hard RESET "L" -> "H" */
  gprintk("gpio_get_value(GPIO_MLCD_RST)=[%d]\n", gpio_get_value(GPIO_MLCD_RST));
  //gpio_direction_output(GPIO_MLCD_RST, 1);
  gpio_set_value(GPIO_MLCD_RST, 1);
  gprintk("gpio_get_value(GPIO_MLCD_RST)=[%d]\n", gpio_get_value(GPIO_MLCD_RST));
  /* Wait 3ms(min) */
  msleep(5); 
}

static void tl2796_ldi_standby_off(struct s5p_lcd *lcd)
{
  /* Hard RESET "H" -> "L"  */
  gprintk("gpio_get_value(GPIO_MLCD_RST)=[%d]\n", gpio_get_value(GPIO_MLCD_RST));
  //gpio_direction_output(GPIO_MLCD_RST, 1);
  gpio_set_value(GPIO_MLCD_RST, 0);
  gprintk("gpio_get_value(GPIO_MLCD_RST)=[%d]\n", gpio_get_value(GPIO_MLCD_RST));
  /* Wait 30us(min) */
  msleep(10); 
  /* Power off */
}

static void tl2796_ldi_enable(struct s5p_lcd *lcd)
{
	struct s5p_panel_data *pdata = lcd->data;

	mutex_lock(&lcd->lock);	
	
	printk(KERN_INFO "(%s) LDI ENABLE\n", __func__);
	
#if defined(LCD_TIMER_FOR_CHECKING_ESD)
	lcd_on_progress = 1;
#endif

	tl2796_ldi_standby_on(lcd);
	tl2796_ldi_wakeup(lcd);	
	
	update_brightness(lcd);
	lcd->ldi_enable = 1;


#if defined(LCD_TIMER_FOR_CHECKING_ESD)
	lcd_on_progress = 0;
	mod_timer(&lcd_esd_polling_timer, jiffies + msecs_to_jiffies(lcd_esd_polling_time));
#endif
	mutex_unlock(&lcd->lock);
}

static void tl2796_ldi_disable(struct s5p_lcd *lcd)
{
	struct s5p_panel_data *pdata = lcd->data;
	mutex_lock(&lcd->lock);	

	printk(KERN_INFO "(%s) LDI DISABLE\n", __func__);
	
#if defined(LCD_TIMER_FOR_CHECKING_ESD)
	del_timer_sync(&lcd_esd_polling_timer);
	lcd_off_progress = 1;
#endif
	
#if 1 /* nat : need to check */
	pwm_config(backlight_pwm_dev, (bl_freq_count * 0)/255, bl_freq_count);
	//pwm_enable(backlight_pwm_dev);
	pwm_disable(backlight_pwm_dev);
#endif

	tl2796_ldi_sleep(lcd);
	tl2796_ldi_standby_off(lcd);
	
	lcd->ldi_enable = 0;
	//s6e63m0_panel_send_sequence(lcd, pdata->standby_on);

#if defined(LCD_TIMER_FOR_CHECKING_ESD)
	lcd_off_progress = 0;
#endif

	mutex_unlock(&lcd->lock);
}


static int s5p_lcd_set_power(struct lcd_device *ld, int power)
{
	struct s5p_lcd *lcd = lcd_get_data(ld);
	struct s5p_panel_data *pdata = lcd->data;

	printk(KERN_DEBUG "s5p_lcd_set_power is called: %d", power);

/*
	if (power)
		s6e63m0_panel_send_sequence(lcd, pdata->display_on);
	else
		s6e63m0_panel_send_sequence(lcd, pdata->display_off);
*/

	return 0;
}

static int s5p_lcd_check_fb(struct lcd_device *lcddev, struct fb_info *fi)
{
	return 0;
}

struct lcd_ops s5p_lcd_ops = {
	.set_power = s5p_lcd_set_power,
	.check_fb = s5p_lcd_check_fb,
};

static int s5p_bl_update_status(struct backlight_device *bd)
{
	struct s5p_lcd *lcd = bl_get_data(bd);
	int bl = bd->props.brightness;

	pr_debug("\nupdate status brightness %d\n",
				bd->props.brightness);

	if (bl < 0 || bl > 255)
		return -EINVAL;

	mutex_lock(&lcd->lock);

	lcd->bl = bl;

	if (lcd->ldi_enable) {
		pr_debug("\n bl :%d\n", bl);
		printk(KERN_INFO "bl : [%d] \n", bl);
		update_brightness(lcd);
	}

	mutex_unlock(&lcd->lock);

	return 0;
}


static int s5p_bl_get_brightness(struct backlight_device *bd)
{
	struct s5p_lcd *lcd = bl_get_data(bd);

	printk(KERN_DEBUG "\n reading brightness\n");

	return lcd->bl;
}

const struct backlight_ops s5p_bl_ops = {
	.update_status = s5p_bl_update_status,
	.get_brightness = s5p_bl_get_brightness,
};

void tl2796_early_suspend(struct early_suspend *h)
{
	struct s5p_lcd *lcd = container_of(h, struct s5p_lcd,
								early_suspend);

	tl2796_ldi_disable(lcd);

	return ;
}
void tl2796_late_resume(struct early_suspend *h)
{
	struct s5p_lcd *lcd = container_of(h, struct s5p_lcd,
								early_suspend);

	tl2796_ldi_enable(lcd);

	return ;
}
static int __devinit tl2796_probe(struct spi_device *spi)
{
	struct s5p_lcd *lcd;
	int ret;

	lcd = kzalloc(sizeof(struct s5p_lcd), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	mutex_init(&lcd->lock);

	spi->bits_per_word = 16; /* nat */

	if (spi_setup(spi)) {
		pr_err("failed to setup spi\n");
		ret = -EINVAL;
		goto err_setup;
	}

	lcd->g_spi = spi;
	lcd->dev = &spi->dev;
	lcd->bl = 255;

	if (!spi->dev.platform_data) {
		dev_err(lcd->dev, "failed to get platform data\n");
		ret = -EINVAL;
		goto err_setup;
	}
	lcd->data = (struct s5p_panel_data *)spi->dev.platform_data;

	if (	!lcd->data->standby_on ) {
		dev_err(lcd->dev, "Invalid platform data\n");
		ret = -EINVAL;
		goto err_setup;
	}

	ret = gpio_request(GPIO_LCD_BL_PWM, "lcd_bl_pwm");
	if (ret < 0) {
		dev_err(lcd->dev, "unable to request gpio for backlight\n");	
		return ret;
	}
	
	s3c_gpio_cfgpin(GPIO_LCD_BL_PWM,  (0x2 << 0));

	backlight_pwm_dev = pwm_request(0, "backlight-pwm");
	if (IS_ERR(backlight_pwm_dev)) {
		dev_err(lcd->dev, "unable to request PWM for backlight\n");		
	} else
		dev_err(lcd->dev, "got pwm for backlight\n");
		
	pwm_config(backlight_pwm_dev, (bl_freq_count*70)/100, bl_freq_count);	
	pwm_enable(backlight_pwm_dev);

#if defined(LCD_TIMER_FOR_CHECKING_ESD)
	gLcd = lcd;

	INIT_WORK(&lcd_esd_work, check_and_recover_esd_wq);

	setup_timer(&lcd_esd_polling_timer, lcd_esd_polling_timer_func, 0);
	mod_timer(&lcd_esd_polling_timer, jiffies + msecs_to_jiffies(lcd_esd_polling_time));

	check_and_recover_esd_wq(&lcd_esd_work); //
#endif

	lcd->bl_dev = backlight_device_register("s5p_bl",
			&spi->dev, lcd, &s5p_bl_ops, NULL);
	if (!lcd->bl_dev) {
		dev_err(lcd->dev, "failed to register backlight\n");
		ret = -EINVAL;
		goto err_setup;
	}

	lcd->bl_dev->props.max_brightness = 255;

	lcd->lcd_dev = lcd_device_register("s5p_lcd",
			&spi->dev, lcd, &s5p_lcd_ops);
	if (!lcd->lcd_dev) {
		dev_err(lcd->dev, "failed to register lcd\n");
		ret = -EINVAL;
		goto err_setup_lcd;
	}
	
#if defined(BACKLIGHT_SYSFS_INTERFACE)
		max_brightness = lcd->bl_dev->props.max_brightness;
		
		// Class and device file creation 
		printk(KERN_ERR "ldi_class create\n");
		ldi_class = class_create(THIS_MODULE, "ldi_class");
		if (IS_ERR(ldi_class))
			pr_err("Failed to create class(ldi_class)!\n");
	
		ldi_dev = device_create(ldi_class, NULL, 0, NULL, "ldi_dev");
		if (IS_ERR(ldi_dev))
			pr_err("Failed to create device(ldi_dev)!\n");
	
		if (device_create_file(ldi_dev, &dev_attr_update_brightness_cmd) < 0)
			pr_err("Failed to create device file(%s)!\n", dev_attr_update_brightness_cmd.attr.name);
#endif


	spi_set_drvdata(spi, lcd);

	tl2796_ldi_enable(lcd);
#ifdef CONFIG_HAS_EARLYSUSPEND
	lcd->early_suspend.suspend = tl2796_early_suspend;
	lcd->early_suspend.resume = tl2796_late_resume;
	lcd->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1;
	register_early_suspend(&lcd->early_suspend);
#endif
	pr_info("tl2796_probe successfully proved\n");

	return 0;

err_setup_lcd:
	backlight_device_unregister(lcd->bl_dev);

err_setup:
	mutex_destroy(&lcd->lock);
	kfree(lcd);

err_alloc:
	return ret;
}

static int __devexit tl2796_remove(struct spi_device *spi)
{
	struct s5p_lcd *lcd = spi_get_drvdata(spi);

	unregister_early_suspend(&lcd->early_suspend);

	backlight_device_unregister(lcd->bl_dev);

	tl2796_ldi_disable(lcd);

	kfree(lcd);

	return 0;
}

static struct spi_driver tl2796_driver = {
	.driver = {
		.name	= "tl2796",
		.owner	= THIS_MODULE,
	},
	.probe		= tl2796_probe,
	.remove		= __devexit_p(tl2796_remove),
};

static int __init tl2796_init(void)
{
	return spi_register_driver(&tl2796_driver);
}
static void __exit tl2796_exit(void)
{
	spi_unregister_driver(&tl2796_driver);
}

module_init(tl2796_init);
module_exit(tl2796_exit);


MODULE_AUTHOR("Kernel Team - North America, Mobile Comm., Samsung Electronics");
MODULE_DESCRIPTION("TMD LDI driver");
MODULE_LICENSE("GPL");

