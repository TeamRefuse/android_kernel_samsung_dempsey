#if defined(CONFIG_HAWK_VER_B1_REAL_ADDED_FEATURE) //NAGSM_Android_HQ_KERNEL_CLEE_20100908 : Setup Hawk Real Board Rev 0.1
#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/lcd.h>
#include <linux/backlight.h>

#include <plat/gpio-cfg.h>

#if defined(CONFIG_HAWK_VER_B1_REAL_ADDED_FEATURE) //NAGSM_Android_HQ_KERNEL_CLEE_20100915 : Backlight PWM
#include <linux/pwm.h>
#include <linux/regulator/max8998.h>
#include <mach/max8998_function.h>
#endif
#include "s3cfb.h"

#define DIM_BL	20
#define MIN_BL	30
#define MAX_BL	255

#define CRITICAL_BATTERY_LEVEL 5

#define LCD_TUNNING_VALUE 1 // NAGSM_Android_HQ_KERNEL_CLee

#if defined (LCD_TUNNING_VALUE)
// Values coming from platform
#define MAX_BRIGHTNESS_LEVEL 255
#define LOW_BRIGHTNESS_LEVEL 30
#define DIM_BACKLIGHT_LEVEL 20	
// Values that kernel ernel tries to set.
#define MAX_BACKLIGHT_VALUE 159
#define LOW_BACKLIGHT_VALUE 7
#define DIM_BACKLIGHT_VALUE 7	

static int s5p_bl_convert_to_tuned_value(int intensity);
#endif

#define LCD_TIMER_FOR_CHECKING_ESD 1 // NAGSM_Android_HQ_KERNEL_CLee

#if defined(LCD_TIMER_FOR_CHECKING_ESD)
static struct work_struct lcd_esd_work;
static struct timer_list lcd_esd_polling_timer;
static const unsigned int lcd_esd_polling_time = 1000;
static int lcd_esd_last_status = 1; // 0 : normal, 1: something happen(default, print out first log)
static int lcd_on_progress = 0;
static int lcd_off_progress = 0;
#endif


#define BACKLIGHT_SYSFS_INTERFACE 1 // NAGSM_Android_SEL_KERNEL_Subhransu

#if defined(BACKLIGHT_SYSFS_INTERFACE)
	struct class *ldi_class;
	struct device *ldi_dev;
	static int max_brightness = 0;
	static int saved_brightness = 0;
#endif

/*********** for debug **********************************************************/
#if 0
#define gprintk(fmt, x... ) printk( "%s(%d): " fmt, __FUNCTION__ ,__LINE__, ## x)
#else
#define gprintk(x...) do { } while (0)
#endif
/*******************************************************************************/

static int ldi_enable = 0;
#if defined(CONFIG_HAWK_VER_B1_REAL_ADDED_FEATURE) //NAGSM_Android_HQ_KERNEL_CLEE_20100915 : Backlight PWM
struct pwm_device	*backlight_pwm_dev;
long int bl_freq_count = 100000;
#endif

typedef enum {
	BACKLIGHT_LEVEL_OFF		= 0,
	BACKLIGHT_LEVEL_DIMMING	= 1,
	BACKLIGHT_LEVEL_NORMAL	= 6
} backlight_level_t;

backlight_level_t backlight_level = BACKLIGHT_LEVEL_OFF;
static int bd_brightness = 0;
int current_gamma_value = -1;

static DEFINE_MUTEX(spi_use);

struct s5p_lcd {
	struct spi_device *g_spi;
	struct lcd_device *lcd_dev;
	struct backlight_device *bl_dev;
};

static struct s5p_lcd lcd;

#define SLEEPMSEC		0x1000
#define ENDDEF			0x2000
#define	DEFMASK			0xFF00

/* TMD 3.5 inch WVGA REV 0.4 10.11.11 Excel version */
const unsigned short SEQ_PANEL_LCD_SET_MANUFACTURE_COMMAND_CODE[] = { 
	0xB2, 0x00,
	0x53, 0x00,
	0x54, 0x00,
	0x57, 0x00,
	0x62, 0x05,  
	0x67, 0x70, 
//	0x68, 0x25, 
//	0x69, 0x29, 
	0x6A, 0x03, 
	0x6B, 0x01,
	0x6C, 0x0F, //	0x6C, 0x07,
	0x6D, 0x0F,  //	0x6D, 0x05,  
	0x6F, 0x0B, 
	0x77, 0x53,
	0x78, 0x00,
	0x79, 0x48,
	0x7A, 0x00,
	0x7B, 0xF8, //0xE5, 
	0x7C, 0x1E, 
	0x7D, 0x38, 
	0x7E, 0x1A, 
	0x7F, 0xD0, 
	0x80, 0x00,
	0x81, 0xF8, //0xE5, 
	0x82, 0x00, 
	0x83, 0x15, 
	0x84, 0xED, 
	0x85, 0xD0, 
	0x86, 0x00,
	0x87, 0xF8, //0xE5, 
	0x88, 0xC3, 
	0x89, 0xC8, 
	0x8A, 0x8B, 
	0x8B, 0x10, 
	0x9F, 0x03,
	0xA0, 0x20,
	0xB3, 0x00,
	0xB4, 0x52,
	0xC7, 0x00,
	0xC8, 0x01,
	0xC9, 0x03,
	0xCA, 0x20,
	0xB2, 0x03,

	ENDDEF, 0x0000	
};

static struct s3cfb_lcd s6e63m0 = {
	.width = 480,
	.height = 800,
	.p_width = 52,
	.p_height = 86,
	.bpp = 24,
	.freq = 60,
	
  .timing = {
    .h_fp = 82, 
    .h_bp = 2, 
    .h_sw = 4,  
    .v_fp = 5,  
    .v_fpe = 1,
    .v_bp = 1,
    .v_bpe = 1,
    .v_sw = 2,
    },

	.polarity = {
		.rise_vclk = 1,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 1,
	},
};

int IsLDIEnabled(void);
static void wait_ldi_enable(void);
static void update_brightness(int gamma);
static void tl2796_ldi_standby_on();
static void tl2796_ldi_wakeup();

static int tl2796_spi_write_driver(unsigned char address, unsigned short command)
{
	int ret;
  u16 buf[1]={0, };
  struct spi_message msg;

  struct spi_transfer xfer = {
    .len    = 2,
    .tx_buf = buf,
  };

  buf[0] = (address << 8) | command;
  //gprintk("REG=[%d](0x%x)\n", buf[0], buf[0]); 

  spi_message_init(&msg);
  spi_message_add_tail(&xfer, &msg);
  
	if(!(lcd.g_spi))
	{
	  gprintk("***** !(lcd.g_spi) *****\n");
	  return -EINVAL;
  }	      	

  ret = spi_sync(lcd.g_spi, &msg);
  if (ret < 0)
  {
		pr_err("%s::%d -> spi_sync failed Err=%d\n",__func__,__LINE__,ret);
	}
		
	return ret ;
}
static void tl2796_spi_write(unsigned char address, unsigned short command)
{
  tl2796_spi_write_driver(0x0, address);
  tl2796_spi_write_driver(0x1, command);
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
		
        if(!IsLDIEnabled())
        {
                printk(KERN_ERR "LDI not enabled");
                return 0;
        }

        /* Sanity checking if brightness value is -ve by mistake, then set to minimum value*/
        /*                 brightness value > max value, set to max value */
        if(brightness < 0)
                brightness = 0;

        if(brightness > max_brightness)
                brightness = max_brightness;
        update_brightness(brightness);
		
		saved_brightness = brightness;
        return 0;
}
static DEVICE_ATTR(update_brightness_cmd,0666, update_brightness_cmd_show, upadate_brightness_cmd_store);
#endif

#if defined(LCD_TIMER_FOR_CHECKING_ESD)
static void check_and_recover_esd_wq(struct work_struct *work)
{
		int ret = 0;
		u8 txbuf[1] = {0,};
		u8 rxbuf[1] = {0,};

		tl2796_spi_write(0xB2, 0x00); 			// LDI Data Access
	
		tl2796_spi_write_driver(0x00, 0x54);		// read 0x54 h	
		if( !(lcd.g_spi) )
		{
			printk(KERN_DEBUG, "[LCD] check_and_recover_esd_wq !(lcd.g_spi) \n");
			return -EINVAL;
		}	   
		
		lcd.g_spi->bits_per_word = 8;
		txbuf[0] = 0x21;	rxbuf[0] = 0x0;
		ret = spi_write_then_read(lcd.g_spi, txbuf, sizeof(txbuf), rxbuf, sizeof(rxbuf));	
		lcd.g_spi->bits_per_word = 16;
		
		if ( rxbuf[0] == 0x00)
		{
			// Normal			
			if ( lcd_esd_last_status == 1)
			{
				pr_info("[LCD] Normal (0x%x, 0x%x, 0x%d, 0x%d)\n", rxbuf[0], ret, lcd_on_progress, lcd_off_progress); 
			}
			lcd_esd_last_status = 0; // Normal
			return;
		}
		else
		{
			// Got ESD
			if ( lcd_on_progress || lcd_off_progress )
			{
				// LCD sequence is on going.
				pr_info("[LCD] ESD - SPI is on operation, skip (0x%x, 0x%x, 0x%d, 0x%d)\n", rxbuf[0], ret, lcd_on_progress, lcd_off_progress); 
				lcd_esd_last_status = 1;
			}
			else
			{
				pr_info("[LCD] ESD - Restarting (0x%x, 0x%x, 0x%d, 0x%d)\n", rxbuf[0], ret, lcd_on_progress, lcd_off_progress); 
				tl2796_ldi_standby_on();
				tl2796_ldi_wakeup();
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

static void tl2796_panel_send_sequence(const unsigned short *wbuf)
{
  int i = 0;

  gprintk("#################SPI start##########################\n");
  while ((wbuf[i] & DEFMASK) != ENDDEF) {
    if ((wbuf[i] & DEFMASK) != SLEEPMSEC)
      tl2796_spi_write(wbuf[i], wbuf[i+1]);
    else
      mdelay(wbuf[i+1]);
    i += 2;
  }
  gprintk("#################SPI enb##########################\n");
}

int IsLDIEnabled(void)
{
	return ldi_enable;
}
EXPORT_SYMBOL(IsLDIEnabled);

static void SetLDIEnabledFlag(int OnOff)
{
	ldi_enable = OnOff;
}

static void tl2796_ldi_sleep()
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

static void tl2796_ldi_wakeup()
{
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
  tl2796_panel_send_sequence(SEQ_PANEL_LCD_SET_MANUFACTURE_COMMAND_CODE);
  /* Wait 85ms(min) == (Wait 5 frame(min)) */
  msleep(140); 
}

static void tl2796_ldi_standby_on()
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

static void tl2796_ldi_standby_off()
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

void tl2796_ldi_init(void)
{
#if defined(CONFIG_HAWK_VER_B1_REAL_ADDED_FEATURE) //NAGSM_Android_HQ_KERNEL_CLEE_20100915 : Backlight PWM
	int ret_val_pwm_config1 = 0;
	int ret_val_pwm_config2 = 0;
#endif

#if defined(LCD_TIMER_FOR_CHECKING_ESD)
	lcd_on_progress = 1;
#endif

	tl2796_ldi_standby_on();
	tl2796_ldi_wakeup();

#if defined(LCD_TIMER_FOR_CHECKING_ESD)
	lcd_on_progress = 0;
	mod_timer(&lcd_esd_polling_timer, jiffies + msecs_to_jiffies(lcd_esd_polling_time));
#endif

	SetLDIEnabledFlag(1);	
	printk(KERN_DEBUG "LDI enable ok\n");
	pr_info("%s::%d -> ldi initialized\n",__func__,__LINE__);	
}

void tl2796_ldi_enable(void)
{
  return;
}

void tl2796_ldi_disable(void)
{

#if defined(LCD_TIMER_FOR_CHECKING_ESD)
	del_timer_sync(&lcd_esd_polling_timer);
	lcd_off_progress = 1;
#endif

	tl2796_ldi_sleep();
	tl2796_ldi_standby_off();

#if defined(LCD_TIMER_FOR_CHECKING_ESD)
	lcd_off_progress = 0;
#endif

	SetLDIEnabledFlag(0);
	printk(KERN_DEBUG "LDI disable ok\n");
	pr_info("%s::%d -> ldi disabled\n",__func__,__LINE__);	
}

void s3cfb_set_lcd_info(struct s3cfb_global *ctrl)
{
	s6e63m0.init_ldi = NULL;
	ctrl->lcd = &s6e63m0;
}

//mkh:lcd operations and functions
static int bd_brightness_prev = 0;
static void off_display(void);

static int s5p_lcd_set_power(struct lcd_device *ld, int power)
{	
	printk(KERN_DEBUG, "[s5p_lcd_set_power] power=[%d], bd_brightness=[%d], bd_brightness_prev=[%d]\n", power, bd_brightness, bd_brightness_prev);
	
	if (power)
	{
		//ON 		
		tl2796_ldi_init();
		if ( bd_brightness )
		{
			update_brightness(bd_brightness);	
		}
		else
		{
			update_brightness(bd_brightness_prev);
		}
	}
	else	
	{		
		//OFF 
		bd_brightness_prev = bd_brightness;
		tl2796_ldi_disable();		
		off_display();
	}

	return 0;
}

static struct lcd_ops s5p_lcd_ops = {
	.set_power = s5p_lcd_set_power,
};

//mkh:backlight operations and functions
static void wait_ldi_enable(void)
{
	int i = 0;

	for (i = 0; i < 100; i++)	{
		gprintk("ldi_enable : %d \n", ldi_enable);

		if(IsLDIEnabled())
		{
			//msleep(50); // NAGSM_Android_HQ_KERNEL_CLEE_20100915 : Add margin time.
			break;
		}
		
		msleep(10);
	};
}

#if defined (LCD_TUNNING_VALUE)
static int s5p_bl_convert_to_tuned_value(int intensity)
{
	int tune_value;

	if(intensity >= LOW_BRIGHTNESS_LEVEL)
	{
		tune_value = (intensity - LOW_BRIGHTNESS_LEVEL) * (MAX_BACKLIGHT_VALUE-LOW_BACKLIGHT_VALUE) / (MAX_BRIGHTNESS_LEVEL-LOW_BRIGHTNESS_LEVEL) + LOW_BACKLIGHT_VALUE;
	}
	else if(intensity >= DIM_BACKLIGHT_LEVEL)
	{
		tune_value = (intensity - DIM_BACKLIGHT_LEVEL) * (LOW_BACKLIGHT_VALUE-DIM_BACKLIGHT_VALUE) / (LOW_BRIGHTNESS_LEVEL-DIM_BACKLIGHT_LEVEL) + DIM_BACKLIGHT_VALUE;
	}	
	else if(intensity > 0)
	{
		tune_value = (intensity) * (DIM_BACKLIGHT_VALUE) / (DIM_BACKLIGHT_LEVEL);
	}
	else
	{
		tune_value = intensity;
	}

	gprintk("Passed value : [%d], Converted value : [%d]\n", intensity, tune_value);

	return tune_value;
}
#endif

static void off_display(void)
{
#if defined(CONFIG_HAWK_VER_B1_REAL_ADDED_FEATURE) //NAGSM_Android_HQ_KERNEL_CLEE_20100915 : Backlight PWM
	pwm_config(backlight_pwm_dev, (bl_freq_count * 0)/255, bl_freq_count);
	pwm_enable(backlight_pwm_dev);

	pwm_disable(backlight_pwm_dev);
#endif
	msleep(20);
	bd_brightness = 0;
	backlight_level = BACKLIGHT_LEVEL_OFF;
}

static void update_brightness(int brightness)
{ 
  int brightness_pulse;
  
#if defined (LCD_TUNNING_VALUE)
	int tunned_brightness = 0;
	tunned_brightness = s5p_bl_convert_to_tuned_value(brightness);
	pwm_config(backlight_pwm_dev, (bl_freq_count * tunned_brightness)/MAX_BL, bl_freq_count);
	pwm_enable(backlight_pwm_dev);
#else
	#if defined(CONFIG_HAWK_VER_B1_REAL_ADDED_FEATURE) //NAGSM_Android_HQ_KERNEL_CLEE_20100915 : Backlight PWM
		pwm_config(backlight_pwm_dev, (bl_freq_count * brightness)/255, bl_freq_count);
		pwm_enable(backlight_pwm_dev);
		// gprintk("## brightness = [%ld], (bl_freq_count * brightness)/255 =[%ld], ret_val_pwm_config=[%ld] \n", brightness, (bl_freq_count * brightness)/255, ret_val_pwm_config );
	#endif
#endif

  #if 0 // jmj_test
  gpio_direction_output(GPIO_LCD_BL_EN, 1);
  do {  
    gpio_set_value(GPIO_LCD_BL_EN, 0);
    msleep(1);  

    gpio_set_value(GPIO_LCD_BL_EN, 1);
    msleep(1);  
  }while(--brightness_pulse)
  #endif
}

static int s5p_bl_update_status(struct backlight_device* bd)
{
	int bl = bd->props.brightness;
	backlight_level_t level = BACKLIGHT_LEVEL_OFF;

#if defined(CONFIG_HAWK_VER_B1_REAL_ADDED_FEATURE) //NAGSM_Android_HQ_KERNEL_CLEE_20100915 : Backlight PWM
	wait_ldi_enable();
#endif

	gprintk("\nupdate status brightness[0~255] : (%d) \n",bd->props.brightness);
	
	if(bl == 0) 
	{
		level = BACKLIGHT_LEVEL_OFF;	//lcd off
	}
	else if((bl < MIN_BL) && (bl > 0))
	{
		level = BACKLIGHT_LEVEL_DIMMING;	//dimming
	}
	else
	{
		level = BACKLIGHT_LEVEL_NORMAL;	//normal
	}
		
	bd_brightness = bd->props.brightness;
	backlight_level = level;

	switch (level)	{
	  case BACKLIGHT_LEVEL_OFF:
	    off_display();
	    gprintk("[OFF] s5p_bl_update_status is called. level : (%d), brightness[0~255] : (%d)\n",level, bl);
	    break;
		case BACKLIGHT_LEVEL_DIMMING:
			update_brightness(bl);
			gprintk("[DIMMING] s5p_bl_update_status is called. level : (%d), brightness[0~255] : (%d)\n",level, bl);
			break;
		case BACKLIGHT_LEVEL_NORMAL:
			update_brightness(bl);
			gprintk("[NORMAL] s5p_bl_update_status is called. level : (%d), brightness[0~255] : (%d)\n",level, bl);
			break;
		default:
			break;
	}

	return 0;
}

static int s5p_bl_get_brightness(struct backlight_device* bd)
{
	printk(KERN_DEBUG "\n reading brightness \n");
	gprintk("\n5p_bl_get_brightness is called. bd_brightness [0~255] : (%d) \n",bd->props.brightness);
	return bd_brightness;
}

static struct backlight_ops s5p_bl_ops = {
	.update_status = s5p_bl_update_status,
	.get_brightness = s5p_bl_get_brightness,	
};

static int __init tl2796_probe(struct spi_device *spi)
{
	int ret;

	spi->bits_per_word = 16;
	ret = spi_setup(spi);
	lcd.g_spi = spi;
	lcd.lcd_dev = lcd_device_register("s5p_lcd",&spi->dev,&lcd,&s5p_lcd_ops);
	lcd.bl_dev = backlight_device_register("s5p_bl",&spi->dev,&lcd,&s5p_bl_ops);
	lcd.bl_dev->props.max_brightness = 255;
	dev_set_drvdata(&spi->dev,&lcd);
#if defined(CONFIG_HAWK_VER_B1_REAL_ADDED_FEATURE) //NAGSM_Android_HQ_KERNEL_CLEE_20100915 : Backlight PWM
	backlight_pwm_dev = pwm_request(0, "Hawk_PWM");
	pwm_config(backlight_pwm_dev, (bl_freq_count*70)/100, bl_freq_count);	
	pwm_enable(backlight_pwm_dev);				//NAGSM_Android_SEL_Kernel_Aakash_20100915			
#endif

	SetLDIEnabledFlag(1);

#if defined(BACKLIGHT_SYSFS_INTERFACE)
	max_brightness = lcd.bl_dev->props.max_brightness;	//NAGSM_Android_SEL_KERNEL_Subhransu_
	
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

#if defined(LCD_TIMER_FOR_CHECKING_ESD)
	INIT_WORK(&lcd_esd_work, check_and_recover_esd_wq);

	setup_timer(&lcd_esd_polling_timer, lcd_esd_polling_timer_func, 0);
	mod_timer(&lcd_esd_polling_timer, jiffies + msecs_to_jiffies(lcd_esd_polling_time));

	check_and_recover_esd_wq(&lcd_esd_work); //
#endif

	if (ret < 0)	
	{
		pr_err("%s::%d-> s6e63m0 probe failed Err=%d\n",__func__,__LINE__,ret);
		return 0;
	}
	
	pr_info("%s::%d->s6e63m0 probed successfuly\n",__func__,__LINE__);
	
	return ret;
}

static struct spi_driver tl2796_driver = {
	.driver = {
		.name	= "tl2796",
		.owner	= THIS_MODULE,
	},
	.probe		= tl2796_probe,
	.remove		= __exit_p(tl2796_remove),
	.suspend	= NULL,
	.resume		= NULL,
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


MODULE_AUTHOR("SAMSUNG");
MODULE_DESCRIPTION("tl2796 LDI driver");
MODULE_LICENSE("GPL");

#else /* defined(CONFIG_HAWK_VER_B1_REAL_ADDED_FEATURE) //NAGSM_Android_HQ_KERNEL_CLEE_20100908 : Setup Hawk Real Board Rev 0.1 */

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

#include <plat/gpio-cfg.h>

#include "s3cfb.h"

#include "s6e63m0.h"

#define DIM_BL	20

#define MIN_BL	30

#define MAX_BL	255

#define MAX_GAMMA_VALUE	24	// we have 25 levels. -> 16 levels -> 24 levels
#define CRITICAL_BATTERY_LEVEL 5

#define GAMMASET_CONTROL //for 1.9/2.2 gamma control from platform

/*********** for debug **********************************************************/
#if 0 
#define gprintk(fmt, x... ) printk( "%s(%d): " fmt, __FUNCTION__ ,__LINE__, ## x)
#else
#define gprintk(x...) do { } while (0)
#endif
/*******************************************************************************/

static int ldi_enable = 0;


	
typedef enum {
	BACKLIGHT_LEVEL_OFF		= 0,
	BACKLIGHT_LEVEL_DIMMING	= 1,
	BACKLIGHT_LEVEL_NORMAL	= 6
} backlight_level_t;

backlight_level_t backlight_level = BACKLIGHT_LEVEL_OFF;
static int bd_brightness = 0;

int current_gamma_value = -1;
static int on_19gamma = 0;

static DEFINE_MUTEX(spi_use);

struct s5p_lcd {
	struct spi_device *g_spi;
	struct lcd_device *lcd_dev;
	struct backlight_device *bl_dev;
};

#ifdef GAMMASET_CONTROL
struct class *gammaset_class;
struct device *switch_gammaset_dev;
#endif

#ifdef CONFIG_FB_S3C_TL2796_ACL
static int acl_enable = 0;
static int cur_acl = 0;

struct class *acl_class;
struct device *switch_aclset_dev;
#endif

#ifdef CONFIG_FB_S3C_MDNIE
extern void init_mdnie_class(void);
#endif

static struct s5p_lcd lcd;

#if !defined(CONFIG_ARIES_NTT)
static const unsigned short *p22Gamma_set[] = {        
                      
	s6e63m0_22gamma_30cd,//0                               
	s6e63m0_22gamma_40cd,                         
	s6e63m0_22gamma_70cd,                         
	s6e63m0_22gamma_90cd,                         
	s6e63m0_22gamma_100cd,                     
	s6e63m0_22gamma_110cd,  //5                      
	s6e63m0_22gamma_120cd,                        
	s6e63m0_22gamma_130cd,                        
	s6e63m0_22gamma_140cd,	                      
	s6e63m0_22gamma_150cd,                    
	s6e63m0_22gamma_160cd,   //10                     
	s6e63m0_22gamma_170cd,                        
	s6e63m0_22gamma_180cd,                        
	s6e63m0_22gamma_190cd,	                      
	s6e63m0_22gamma_200cd,                    
	s6e63m0_22gamma_210cd,  //15                      
	s6e63m0_22gamma_220cd,                        
	s6e63m0_22gamma_230cd,                        
	s6e63m0_22gamma_240cd,                        
	s6e63m0_22gamma_250cd,                   
	s6e63m0_22gamma_260cd,  //20                       
	s6e63m0_22gamma_270cd,                        
	s6e63m0_22gamma_280cd,                        
	s6e63m0_22gamma_290cd,                        
	s6e63m0_22gamma_300cd,//24                    
};                                             
                                               
                                                
static const unsigned short *p19Gamma_set[] = {        
	s6e63m0_19gamma_30cd,	//0
	//s6e63m0_19gamma_50cd,                         
	s6e63m0_19gamma_40cd,     
	s6e63m0_19gamma_70cd,

	s6e63m0_19gamma_90cd,
	s6e63m0_19gamma_100cd,
	s6e63m0_19gamma_110cd,	//5
	s6e63m0_19gamma_120cd,
	s6e63m0_19gamma_130cd,
	s6e63m0_19gamma_140cd,
	s6e63m0_19gamma_150cd,
	s6e63m0_19gamma_160cd,	//10
	s6e63m0_19gamma_170cd,
	s6e63m0_19gamma_180cd,
	s6e63m0_19gamma_190cd,
	s6e63m0_19gamma_200cd,
	s6e63m0_19gamma_210cd,	//15
	s6e63m0_19gamma_220cd,
	s6e63m0_19gamma_230cd,
	s6e63m0_19gamma_240cd,
	s6e63m0_19gamma_250cd,
	s6e63m0_19gamma_260cd,	//20
	s6e63m0_19gamma_270cd,
	s6e63m0_19gamma_280cd,
	s6e63m0_19gamma_290cd,
	s6e63m0_19gamma_300cd,	//24
}; 
#else // Modify NTTS1
static const unsigned short *p22Gamma_set[] = {        
	s6e63m0_22gamma_30cd,	 //0                    
	s6e63m0_22gamma_40cd,  
	s6e63m0_22gamma_50cd,
	s6e63m0_22gamma_60cd,
	s6e63m0_22gamma_70cd,	
	s6e63m0_22gamma_80cd,	//5
	s6e63m0_22gamma_90cd,
	s6e63m0_22gamma_100cd,
	s6e63m0_22gamma_110cd,	
	s6e63m0_22gamma_120cd,	
	s6e63m0_22gamma_130cd,	//10
	s6e63m0_22gamma_140cd,
	s6e63m0_22gamma_150cd,
	s6e63m0_22gamma_160cd,	
	s6e63m0_22gamma_170cd,	
	s6e63m0_22gamma_180cd,	//15
	s6e63m0_22gamma_190cd,
	s6e63m0_22gamma_200cd,
	s6e63m0_22gamma_210cd,	
	s6e63m0_22gamma_220cd,	
	s6e63m0_22gamma_230cd,	//20
	s6e63m0_22gamma_240cd,
	s6e63m0_22gamma_260cd,
	s6e63m0_22gamma_280cd,
	s6e63m0_22gamma_300cd, 	//24                   
};                                             
                                               
                                                
static const unsigned short *p19Gamma_set[] = {     
	s6e63m0_19gamma_30cd,	//0                     
	s6e63m0_19gamma_40cd,  
	s6e63m0_19gamma_50cd,
	s6e63m0_19gamma_60cd,
	s6e63m0_19gamma_70cd,	
	s6e63m0_19gamma_80cd,	//5
	s6e63m0_19gamma_90cd,
	s6e63m0_19gamma_100cd,
	s6e63m0_19gamma_110cd,	
	s6e63m0_19gamma_120cd,	
	s6e63m0_19gamma_130cd,	//10
	s6e63m0_19gamma_140cd,
	s6e63m0_19gamma_150cd,
	s6e63m0_19gamma_160cd,	
	s6e63m0_19gamma_170cd,	
	s6e63m0_19gamma_180cd,	//15
	s6e63m0_19gamma_190cd,
	s6e63m0_19gamma_200cd,
	s6e63m0_19gamma_210cd,	
	s6e63m0_19gamma_220cd,	
	s6e63m0_19gamma_230cd,	//20
	s6e63m0_19gamma_240cd,
	s6e63m0_19gamma_260cd,
	s6e63m0_19gamma_280cd,
	s6e63m0_19gamma_300cd, 	//24
}; 
#endif

#ifdef CONFIG_FB_S3C_TL2796_ACL
static const unsigned short *ACL_cutoff_set[] = {
	acl_cutoff_off, //0
	acl_cutoff_8p,
	acl_cutoff_14p,
	acl_cutoff_20p,
	acl_cutoff_24p,
	acl_cutoff_28p, //5
	acl_cutoff_32p,
	acl_cutoff_35p,
	acl_cutoff_37p,
	acl_cutoff_40p, //9
	acl_cutoff_45p, //10
	acl_cutoff_47p, //11
	acl_cutoff_48p, //12
	acl_cutoff_50p, //13
	acl_cutoff_60p, //14
	acl_cutoff_75p, //15
	acl_cutoff_43p, //16
};
#endif


static struct s3cfb_lcd s6e63m0 = {
	.width = 480,
	.height = 800,
	.p_width = 52,
	.p_height = 86,
	.bpp = 24,
	.freq = 60,
	
	.timing = {
		.h_fp = 16,
		.h_bp = 16,
		.h_sw = 2,
		.v_fp = 28,
		.v_fpe = 1,
		.v_bp = 1,
		.v_bpe = 1,
		.v_sw = 2,
	},

	.polarity = {
		.rise_vclk = 1,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 1,
	},
};
static void wait_ldi_enable(void);
static void update_brightness(int gamma);

///////SPI/////////////////////////////////////////////////////////////////////////////////////
static int s6e63m0_spi_write_driver(int reg)
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


	ret = spi_sync(lcd.g_spi, &msg);

	if (ret < 0)
		pr_err("%s::%d -> spi_sync failed Err=%d\n",__func__,__LINE__,ret);
	return ret ;

}

static void s6e63m0_spi_write(unsigned short reg)
{

	s6e63m0_spi_write_driver(reg);


}

static void s6e63m0_panel_send_sequence(const unsigned short *wbuf)
{
	int i = 0;

	mutex_lock(&spi_use);

	gprintk("#################SPI start##########################\n");
	
	while ((wbuf[i] & DEFMASK) != ENDDEF) {
		if ((wbuf[i] & DEFMASK) != SLEEPMSEC){
			s6e63m0_spi_write(wbuf[i]);
			i+=1;}
		else{
			msleep(wbuf[i+1]);
			i+=2;}
	}
	
	gprintk("#################SPI end##########################\n");

	mutex_unlock(&spi_use);
}

int IsLDIEnabled(void)
{
	return ldi_enable;
}
EXPORT_SYMBOL(IsLDIEnabled);

static void SetLDIEnabledFlag(int OnOff)
{
	ldi_enable = OnOff;
}

void tl2796_ldi_init(void)
{
	s6e63m0_panel_send_sequence(s6e63m0_SEQ_SETTING);
	s6e63m0_panel_send_sequence(s6e63m0_SEQ_STANDBY_OFF);

	SetLDIEnabledFlag(1);
	printk(KERN_DEBUG "LDI enable ok\n");
	pr_info("%s::%d -> ldi initialized\n",__func__,__LINE__);	
}


void tl2796_ldi_enable(void)
{
}

void tl2796_ldi_disable(void)
{
	s6e63m0_panel_send_sequence(s6e63m0_SEQ_STANDBY_ON);
	s6e63m0_panel_send_sequence(s6e63m0_SEQ_DISPLAY_OFF);
	SetLDIEnabledFlag(0);
	printk(KERN_DEBUG "LDI disable ok\n");
	pr_info("%s::%d -> ldi disabled\n",__func__,__LINE__);	
}

void s3cfb_set_lcd_info(struct s3cfb_global *ctrl)
{
	s6e63m0.init_ldi = NULL;
	ctrl->lcd = &s6e63m0;

}
//mkh:lcd operations and functions
static int s5p_lcd_set_power(struct lcd_device *ld, int power)
{
	printk(KERN_DEBUG "s5p_lcd_set_power is called: %d", power);

	if (power)	{
		s6e63m0_panel_send_sequence(s6e63m0_SEQ_DISPLAY_ON);
	}
	else	{
		s6e63m0_panel_send_sequence(s6e63m0_SEQ_DISPLAY_OFF);
	}

	return 0;
}

static struct lcd_ops s5p_lcd_ops = {
	.set_power = s5p_lcd_set_power,
};

//mkh:backlight operations and functions

void bl_update_status_gamma(void)
{
	wait_ldi_enable();

	if (current_gamma_value != -1)	{
		gprintk("#################%dgamma start##########################\n", on_19gamma ? 19 : 22);
		update_brightness(current_gamma_value);
		gprintk("#################%dgamma end##########################\n", on_19gamma ? 19 : 22);
	}
}

#ifdef GAMMASET_CONTROL //for 1.9/2.2 gamma control from platform
static ssize_t gammaset_file_cmd_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	gprintk("called %s \n",__func__);

	return sprintf(buf,"%u\n", bd_brightness);
}
static ssize_t gammaset_file_cmd_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
	int value;
	
    sscanf(buf, "%d", &value);

	//printk(KERN_INFO "[gamma set] in gammaset_file_cmd_store, input value = %d \n",value);
	if (!IsLDIEnabled())	{
		printk(KERN_DEBUG "[gamma set] return because LDI is disabled, input value = %d \n", value);
		return size;
	}

	if ((value != 0) && (value != 1))	{
		printk(KERN_DEBUG "\ngammaset_file_cmd_store value(%d) on_19gamma(%d) \n", value,on_19gamma);
		return size;
	}

	if (value != on_19gamma)	{
		on_19gamma = value;
		bl_update_status_gamma();
	}

	return size;
}

static DEVICE_ATTR(gammaset_file_cmd,0666, gammaset_file_cmd_show, gammaset_file_cmd_store);
#endif

#ifdef CONFIG_FB_S3C_TL2796_ACL 
static ssize_t aclset_file_cmd_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	gprintk("called %s \n",__func__);

	return sprintf(buf,"%u\n", acl_enable);
}
static ssize_t aclset_file_cmd_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int value;

	sscanf(buf, "%d", &value);

	printk("[acl set] in aclset_file_cmd_store, input value = %d \n", value);

	if (!IsLDIEnabled())	{
		printk(KERN_DEBUG "[acl set] return because LDI is disabled, input value = %d \n", value);
		return size;
	}
#if 0
	if ((value != 0) && (value != 1))	{
		printk(KERN_DEBUG "\naclset_file_cmd_store value is same : value(%d)\n", value);
		return size;
	}

	if (acl_enable != value)	{
		acl_enable = value;

		if (acl_enable == 1)	{
			s6e63m0_panel_send_sequence(acl_cutoff_init);
			msleep(20);

			switch (current_gamma_value)	{
				case 1:
					s6e63m0_panel_send_sequence(ACL_cutoff_set[0]); //set 0% ACL
					cur_acl = 0;
					//printk(" ACL_cutoff_set Percentage : 0!!\n");
					break;
				case 2:
					s6e63m0_panel_send_sequence(ACL_cutoff_set[1]); //set 12% ACL
					cur_acl = 12;
					//printk(" ACL_cutoff_set Percentage : 12!!\n");
					break;
				case 3:
					s6e63m0_panel_send_sequence(ACL_cutoff_set[2]); //set 22% ACL
					cur_acl = 22;
					//printk(" ACL_cutoff_set Percentage : 22!!\n");
					break;
				case 4:
					s6e63m0_panel_send_sequence(ACL_cutoff_set[3]); //set 30% ACL
					cur_acl = 30;
					//printk(" ACL_cutoff_set Percentage : 30!!\n");
					break;
				case 5:
					s6e63m0_panel_send_sequence(ACL_cutoff_set[4]); //set 35% ACL
					cur_acl = 35;
					//printk(" ACL_cutoff_set Percentage : 35!!\n");
					break;
				default:
					s6e63m0_panel_send_sequence(ACL_cutoff_set[5]); //set 40% ACL
					cur_acl = 40;
					//printk(" ACL_cutoff_set Percentage : 40!!\n");
			}
		}
#else
	if ((value != 0) && (value != 1))	{
		printk(KERN_DEBUG "\naclset_file_cmd_store value is same : value(%d)\n", value);
		return size;
	}

	if (acl_enable != value)	{
		acl_enable = value;

		if (acl_enable == 1)	{
			s6e63m0_panel_send_sequence(acl_cutoff_init);
			msleep(20);

			switch (current_gamma_value)	{
				case 0:
				case 1:
					s6e63m0_panel_send_sequence(ACL_cutoff_set[0]); //set 0% ACL
					cur_acl = 0;
					printk(" ACL_cutoff_set Percentage : 0!!\n");
					break;
				case 2:
				case 3:
				case 4:
				case 5:
				case 6:
				case 7:
				case 8:
				case 9:
				case 10:
				case 11:
				case 12:
					s6e63m0_panel_send_sequence(ACL_cutoff_set[9]); //set 40% ACL
					cur_acl = 40;
					printk(" ACL_cutoff_set Percentage : 40!!\n");
					break;
				case 13:
					s6e63m0_panel_send_sequence(ACL_cutoff_set[16]); //set 43% ACL
					cur_acl = 43;
					printk(" ACL_cutoff_set Percentage : 43!!\n");
					break;
				case 14:
					s6e63m0_panel_send_sequence(ACL_cutoff_set[10]); //set 45% ACL
					cur_acl = 45;
					printk(" ACL_cutoff_set Percentage : 45!!\n");
					break;
				case 15:
					s6e63m0_panel_send_sequence(ACL_cutoff_set[11]); //set 47% ACL
					cur_acl = 47;
					printk(" ACL_cutoff_set Percentage : 47!!\n");
					break;
				case 16:
					s6e63m0_panel_send_sequence(ACL_cutoff_set[12]); //set 48% ACL
					cur_acl = 48;
					printk(" ACL_cutoff_set Percentage : 48!!\n");
					break;
		
				default:
					s6e63m0_panel_send_sequence(ACL_cutoff_set[13]); //set 50% ACL
					cur_acl = 50;
					printk(" ACL_cutoff_set Percentage : 50!!\n");
			}
		}
#endif
		else	{
			//ACL Off
			s6e63m0_panel_send_sequence(ACL_cutoff_set[0]); //ACL OFF
			cur_acl  = 0;
			printk(" ACL_cutoff_set Percentage : 0!!\n");
		}
	}

	return size;
}

static DEVICE_ATTR(aclset_file_cmd,0666, aclset_file_cmd_show, aclset_file_cmd_store);
#endif


#ifdef CONFIG_FB_S3C_MDNIE_TUNINGMODE_FOR_BACKLIGHT
extern void mDNIe_Mode_set_for_backlight(u16 *buf);
extern u16 *pmDNIe_Gamma_set[];
extern int pre_val;
extern int autobrightness_mode;
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
extern int capella_pre_val;
extern int taos_pre_val;
extern int capella_autobrightness_mode;
extern int taos_autobrightness_mode;
#endif
#endif

static void wait_ldi_enable(void)
{
	int i = 0;

	for (i = 0; i < 100; i++)	{
		gprintk("ldi_enable : %d \n", ldi_enable);

		if(IsLDIEnabled())
			break;
		
		msleep(10);
	};
}

static void off_display(void)
{
	msleep(20);
	s6e63m0_panel_send_sequence(s6e63m0_SEQ_DISPLAY_OFF);
	bd_brightness = 0;
	backlight_level = BACKLIGHT_LEVEL_OFF;
	current_gamma_value = -1;
}

static int get_gamma_value_from_bl(int bl)
{
	int gamma_value = 0;
	int gamma_val_x10 = 0;

	if (bl >= MIN_BL)		{

		gamma_val_x10 = 10*(MAX_GAMMA_VALUE-1)*bl/(MAX_BL-MIN_BL) + (10 - 10*(MAX_GAMMA_VALUE-1)*(MIN_BL)/(MAX_BL-MIN_BL)) ;
		gamma_value = (gamma_val_x10+5)/10;
	}	
	else		{
		gamma_value = 0;
	}

	return gamma_value;
}


static void update_brightness(int gamma)
{

	if (on_19gamma)
		s6e63m0_panel_send_sequence(p19Gamma_set[gamma]);
	else
		s6e63m0_panel_send_sequence(p22Gamma_set[gamma]);
		
	s6e63m0_panel_send_sequence(gamma_update); //gamma update


}
static int s5p_bl_update_status(struct backlight_device* bd)
{
	int bl = bd->props.brightness;
	backlight_level_t level = BACKLIGHT_LEVEL_OFF;
	int gamma_value = 0 ;
	int ret = 0; 
	
	wait_ldi_enable();
	
	gprintk("\nupdate status brightness[0~255] : (%d) \n",bd->props.brightness);

	if (!IsLDIEnabled())
		return 0;

	if(bl == 0)
		level = BACKLIGHT_LEVEL_OFF;	//lcd off
	else if((bl < MIN_BL) && (bl > 0))
		level = BACKLIGHT_LEVEL_DIMMING;	//dimming
	else
		level = BACKLIGHT_LEVEL_NORMAL;	//normal

	if (level == BACKLIGHT_LEVEL_OFF)	{
		off_display();
		gprintk("Update status brightness[0~255]:(%d) - LCD OFF \n", bl);
		return 0;
	}	

	gamma_value = get_gamma_value_from_bl(bl);

	
	printk("platform brightness level : %d\n",gamma_value);

	bd_brightness = bd->props.brightness;
	backlight_level = level;

	if (current_gamma_value == gamma_value)
		return 0;
	
	gprintk("Update status brightness[0~255]:(%d) gamma_value:(%d) on_19gamma(%d)\n", bl,gamma_value,on_19gamma);

	if (level == BACKLIGHT_LEVEL_OFF)
		return 0;

#ifdef CONFIG_FB_S3C_MDNIE_TUNINGMODE_FOR_BACKLIGHT
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
		if ((capella_pre_val==1) && (gamma_value < 24) &&(capella_autobrightness_mode)){
			mDNIe_Mode_set_for_backlight(pmDNIe_Gamma_set[2]);
			gprintk("s5p_bl_update_status - pmDNIe_Gamma_set[2]\n" );
			capella_pre_val = -1;
		}

#else
	if ((pre_val==1) && (gamma_value < 24) &&(autobrightness_mode))		{
		mDNIe_Mode_set_for_backlight(pmDNIe_Gamma_set[2]);
		gprintk("s5p_bl_update_status - pmDNIe_Gamma_set[2]\n" );
		pre_val = -1;
	}
#endif
#endif

	switch (level)	{
		case BACKLIGHT_LEVEL_DIMMING:
			update_brightness(0);

#ifdef CONFIG_FB_S3C_TL2796_ACL
			if (acl_enable)		{
#if 0
            if (cur_acl == 0)
						{
							s6e63m0_panel_send_sequence(acl_cutoff_init);
							msleep(20);
						}
#endif
			if (cur_acl != 0)	{
					s6e63m0_panel_send_sequence(ACL_cutoff_set[0]); //set 0% ACL
							gprintk(" ACL_cutoff_set Percentage : 0!!\n");
							cur_acl = 0;
				}
			}
#endif
			gprintk("call s5p_bl_update_status level : %d\n",level);
			break;
		case BACKLIGHT_LEVEL_NORMAL:


#ifdef CONFIG_FB_S3C_TL2796_ACL
			if (acl_enable)		{				
				if ((cur_acl == 0) && (gamma_value != 1))	{
					s6e63m0_panel_send_sequence(acl_cutoff_init);
					msleep(20);
				}

				switch (gamma_value)	{
					case 1:
						if (cur_acl != 0)	{
							s6e63m0_panel_send_sequence(ACL_cutoff_set[0]); //set 0% ACL
							cur_acl = 0;
							gprintk(" ACL_cutoff_set Percentage : 0!!\n");
						}
						break;
					case 2:
					case 3:
					case 4:
					case 5:
					case 6:
					case 7:
					case 8:
					case 9:
					case 10:
					case 11:
					case 12:
						if (cur_acl != 40)	{
							s6e63m0_panel_send_sequence(ACL_cutoff_set[9]); //set 40% ACL
							cur_acl = 40;
							gprintk(" ACL_cutoff_set Percentage : 40!!\n");
						}
						break;

					case 13:
						if (cur_acl != 43)	{
							s6e63m0_panel_send_sequence(ACL_cutoff_set[16]); //set 43% ACL
							cur_acl = 43;
							gprintk(" ACL_cutoff_set Percentage : 43!!\n");
						}
						break;

					case 14:
						if (cur_acl != 45)	{
							s6e63m0_panel_send_sequence(ACL_cutoff_set[10]); //set 45% ACL
							cur_acl = 45;
							gprintk(" ACL_cutoff_set Percentage : 45!!\n");
						}
						break;
					case 15:
						if (cur_acl != 47)	{
							s6e63m0_panel_send_sequence(ACL_cutoff_set[11]); //set 47% ACL
							cur_acl = 47;
							gprintk(" ACL_cutoff_set Percentage : 47!!\n");
						}
						break;
					case 16:
						if (cur_acl != 48)	{
							s6e63m0_panel_send_sequence(ACL_cutoff_set[12]); //set 48% ACL
							cur_acl = 48;
							gprintk(" ACL_cutoff_set Percentage : 48!!\n");
						}
						break;

					default:
						if (cur_acl !=50)	{
							s6e63m0_panel_send_sequence(ACL_cutoff_set[13]); //set 50% ACL
							cur_acl = 50;
							gprintk(" ACL_cutoff_set Percentage : 50!!\n");
						}
				}
			}
#endif
			update_brightness(gamma_value);

			gprintk("#################backlight end##########################\n");

			break;
		default:
			break;
	}

	current_gamma_value = gamma_value;

	return 0;
}


static int s5p_bl_get_brightness(struct backlight_device* bd)
{
	printk(KERN_DEBUG "\n reading brightness \n");
	return bd_brightness;
}

static struct backlight_ops s5p_bl_ops = {
	.update_status = s5p_bl_update_status,
	.get_brightness = s5p_bl_get_brightness,	
};

static int __init tl2796_probe(struct spi_device *spi)
{
	int ret;

	spi->bits_per_word = 9;
	ret = spi_setup(spi);
	lcd.g_spi = spi;
	lcd.lcd_dev = lcd_device_register("s5p_lcd",&spi->dev,&lcd,&s5p_lcd_ops);
	lcd.bl_dev = backlight_device_register("s5p_bl",&spi->dev,&lcd,&s5p_bl_ops);
	lcd.bl_dev->props.max_brightness = 255;
	dev_set_drvdata(&spi->dev,&lcd);

	SetLDIEnabledFlag(1);

#ifdef GAMMASET_CONTROL //for 1.9/2.2 gamma control from platform
	gammaset_class = class_create(THIS_MODULE, "gammaset");
	if (IS_ERR(gammaset_class))
		pr_err("Failed to create class(gammaset_class)!\n");

	switch_gammaset_dev = device_create(gammaset_class, NULL, 0, NULL, "switch_gammaset");
	if (IS_ERR(switch_gammaset_dev))
		pr_err("Failed to create device(switch_gammaset_dev)!\n");

	if (device_create_file(switch_gammaset_dev, &dev_attr_gammaset_file_cmd) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_gammaset_file_cmd.attr.name);
#endif	

#ifdef CONFIG_FB_S3C_TL2796_ACL //ACL On,Off
	acl_class = class_create(THIS_MODULE, "aclset");
	if (IS_ERR(acl_class))
		pr_err("Failed to create class(acl_class)!\n");

	switch_aclset_dev = device_create(acl_class, NULL, 0, NULL, "switch_aclset");
	if (IS_ERR(switch_aclset_dev))
		pr_err("Failed to create device(switch_aclset_dev)!\n");

	if (device_create_file(switch_aclset_dev, &dev_attr_aclset_file_cmd) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_aclset_file_cmd.attr.name);
#endif	

#ifdef CONFIG_FB_S3C_MDNIE
	init_mdnie_class();  //set mDNIe UI mode, Outdoormode
#endif

	if (ret < 0)	{
		pr_err("%s::%d-> s6e63m0 probe failed Err=%d\n",__func__,__LINE__,ret);
		return 0;
	}
	pr_info("%s::%d->s6e63m0 probed successfuly\n",__func__,__LINE__);
	return ret;
}

#if 0
#ifdef CONFIG_PM // add by ksoo (2009.09.07)
int tl2796_suspend(struct platform_device *pdev, pm_message_t state)
{
	pr_info("%s::%d->s6e63m0 suspend called\n",__func__,__LINE__);
	tl2796_ldi_disable();
	return 0;
}

int tl2796_resume(struct platform_device *pdev, pm_message_t state)
{
	pr_info("%s::%d -> s6e63m0 resume called\n",__func__,__LINE__);
	tl2796_ldi_init();
	tl2796_ldi_enable();

	return 0;
}
#endif	/* CONFIG_PM */
#endif

static struct spi_driver tl2796_driver = {
	.driver = {
		.name	= "tl2796",
		.owner	= THIS_MODULE,
	},
	.probe		= tl2796_probe,
	.remove		= __exit_p(tl2796_remove),
#if 0
#ifdef CONFIG_PM
	.suspend	= tl2796_suspend,
	.resume		= tl2796_resume,
#else
	.suspend	= NULL,
	.resume		= NULL,
#endif	/* CONFIG_PM */
#endif
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


MODULE_AUTHOR("SAMSUNG");
MODULE_DESCRIPTION("s6e63m0 LDI driver");
MODULE_LICENSE("GPL");

#endif /* defined(CONFIG_HAWK_VER_B1_REAL_ADDED_FEATURE) //NAGSM_Android_HQ_KERNEL_CLEE_20100908 : Setup Hawk Real Board Rev 0.1 */
