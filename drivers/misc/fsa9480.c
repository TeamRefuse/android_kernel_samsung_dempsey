/*
 * driver/misc/fsa9480.c - FSA9480 micro USB switch device driver
 *
 * Copyright (C) 2010 Samsung Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 * Wonguk Jeong <wonguk.jeong@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/fsa9480.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <mach/param.h>
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD) // mr work 
#include <linux/delay.h>
#endif
#include <linux/gpio.h>
/* FSA9480 I2C registers */
#define FSA9480_REG_DEVID		0x01
#define FSA9480_REG_CTRL		0x02
#define FSA9480_REG_INT1		0x03
#define FSA9480_REG_INT2		0x04
#define FSA9480_REG_INT1_MASK		0x05
#define FSA9480_REG_INT2_MASK		0x06
#define FSA9480_REG_ADC			0x07
#define FSA9480_REG_TIMING1		0x08
#define FSA9480_REG_TIMING2		0x09
#define FSA9480_REG_DEV_T1		0x0a
#define FSA9480_REG_DEV_T2		0x0b
#define FSA9480_REG_BTN1		0x0c
#define FSA9480_REG_BTN2		0x0d
#define FSA9480_REG_CK			0x0e
#define FSA9480_REG_CK_INT1		0x0f
#define FSA9480_REG_CK_INT2		0x10
#define FSA9480_REG_CK_INTMASK1		0x11
#define FSA9480_REG_CK_INTMASK2		0x12
#define FSA9480_REG_MANSW1		0x13
#define FSA9480_REG_MANSW2		0x14

/* Control */
#define CON_SWITCH_OPEN		(1 << 4)
#define CON_RAW_DATA		(1 << 3)
#define CON_MANUAL_SW		(1 << 2)
#define CON_WAIT		(1 << 1)
#define CON_INT_MASK		(1 << 0)
#define CON_MASK		(CON_SWITCH_OPEN | CON_RAW_DATA | \
				CON_MANUAL_SW | CON_WAIT)

/* Device Type 1 */
#define DEV_USB_OTG		(1 << 7)
#define DEV_DEDICATED_CHG	(1 << 6)
#define DEV_USB_CHG		(1 << 5)
#define DEV_CAR_KIT		(1 << 4)
#define DEV_UART		(1 << 3)
#define DEV_USB			(1 << 2)
#define DEV_AUDIO_2		(1 << 1)
#define DEV_AUDIO_1		(1 << 0)

#define DEV_T1_USB_MASK		(DEV_USB_OTG | DEV_USB)
#define DEV_T1_UART_MASK	(DEV_UART)
#define DEV_T1_CHARGER_MASK	(DEV_DEDICATED_CHG | DEV_USB_CHG | DEV_CAR_KIT)

/* Device Type 2 */
#define DEV_AV			(1 << 6)
#define DEV_TTY			(1 << 5)
#define DEV_PPD			(1 << 4)
#define DEV_JIG_UART_OFF	(1 << 3)
#define DEV_JIG_UART_ON		(1 << 2)
#define DEV_JIG_USB_OFF		(1 << 1)
#define DEV_JIG_USB_ON		(1 << 0)

#define DEV_T2_USB_MASK		(DEV_JIG_USB_OFF | DEV_JIG_USB_ON)
#define DEV_T2_UART_MASK	DEV_JIG_UART_OFF
#define DEV_T2_JIG_MASK		(DEV_JIG_USB_OFF | DEV_JIG_USB_ON | \
				DEV_JIG_UART_OFF)

/*
 * Manual Switch
 * D- [7:5] / D+ [4:2]
 * 000: Open all / 001: USB / 010: AUDIO / 011: UART / 100: V_AUDIO
 */
#define SW_VAUDIO		((4 << 5) | (4 << 2))
#define SW_UART			((3 << 5) | (3 << 2))
#define SW_AUDIO		((2 << 5) | (2 << 2))
#define SW_DHOST		((1 << 5) | (1 << 2))
#define SW_AUTO			((0 << 5) | (0 << 2))

/* Interrupt 1 */
#define INT_DETACH		(1 << 1)
#define INT_ATTACH		(1 << 0)
//#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) 	//Build Error
// add hdlnc_opk
static u8 MicroUSBStatus=0;
static u8 MicroJigUSBOnStatus=0;
static u8 MicroJigUSBOffStatus=0;
static u8 MicroJigUARTOffStatus=0;
static u8 MicroJigUARTOnStatus=0;
u8 MicroTAstatus=0;
u8 MicroJigstatus=0;
//#endif

struct fsa9480_usbsw {
	struct i2c_client		*client;
	struct fsa9480_platform_data	*pdata;
	int				dev1;
	int				dev2;
	int				mansw;
	int 			dev_id;
};

static struct fsa9480_usbsw *local_usbsw;
#ifdef CONFIG_VIDEO_MHL_V1
#include<linux/power_supply.h>
#define POWER_SUPPLY_TYPE_MHL POWER_SUPPLY_TYPE_USB
#define POWER_SUPPLY_TYPE_NONE 0
bool gv_mhl_sw_state = 0;
//static struct workqueue_struct *fsa_mhl_workqueue;
//static struct work_struct mhl_start_work;

extern u8 mhl_cable_status;
extern bool mhl_vbus; //3355
extern void sii9234_cfg_power(bool on);	
extern bool SiI9234_init(void);	
static u8 FSA9480_AudioDockConnectionStatus=0x00;
static struct mutex FSA9480_MhlSwitchSel_lock;
static int gv_intr2=0;
void DisableFSA9480Interrupts(void)
{
	struct i2c_client *client = local_usbsw->client;
	int value,ret;

	value = i2c_smbus_read_byte_data(client, FSA9480_REG_CTRL);
	value |= 0x01;

	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

} 

void EnableFSA9480Interrupts(void)
{
	struct i2c_client *client = local_usbsw->client;
	int value,ret;

	value = i2c_smbus_read_byte_data(client, FSA9480_REG_CTRL);
	value &= 0xFE;

	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
} 


#if 0
//Rajucm: This function set the fsa audio dock status and update the audio driver state variable
static void FSA9480_AudioDockEnable(void)
{	
	set_dock_state((int)HOME_DOCK_INSERTED);
	FSA9480_Enable_SPK(1);
	g_dock = HOME_DOCK_INSERTED;
}
//Rajucm: This function will reset the fsa audio dock status and update the audio driver state variable
static void FSA9480_AudioDockDisable(void)
{
	DEBUG_FSA9480("FSA9480_Disable_Audio_Dock####\n");
	FSA9480_AudioDockConnectionStatus = 0;
	set_dock_state((int)DOCK_REMOVED);
	g_dock = DOCK_REMOVED;
}
#endif
/* Rajucm: This function is implimented to clear unwanted/garbage interrupts and hook audio dock functionality. 
 *  Note:- Minimise Audio connection time delay as much as possible.
 *  Recomonded to minimize debug prints at audio_dock connection path
 */
void FSA9480_CheckAndHookAudioDock(void)//Rajucm
{
#if 0
	FSA9480_AudioDockConnectionStatus++;
	FSA9480_MhlSwitchSel(0);//Rajucm
	FSA9480_AudioDockEnable();
#endif
}

void FSA9480_MhlSwitchSel(bool sw) //Rajucm
{
	struct i2c_client *client = local_usbsw->client;
	struct power_supply *mhl_power_supply = power_supply_get_by_name("battery");
	union power_supply_propval value;
	
    printk(KERN_ERR "[FSA] mutex_lock\n");
	//mutex_lock(&FSA9480_MhlSwitchSel_lock);//Rajucm: avoid race condtion between mhl and hpd
	if(sw)
	{
                 //Rajucm: TODO
		if (gv_intr2&0x1) //cts: fix for 3355
		{
			mhl_vbus = true;
			//value.intval = POWER_SUPPLY_TYPE_MHL;
			gv_intr2=0;

			//Rajucm: mhl charching implimentation,
			//if (mhl_vbus && (mhl_power_supply!=NULL) && (mhl_power_supply->set_property(mhl_power_supply, POWER_SUPPLY_PROP_ONLINE, &value)))
			//	printk("Fail to turn %s MHL Charging\n",sw?"on":"off");
		}
		else
			mhl_vbus = false;

		sii9234_cfg_power(1);	//Turn On power to SiI9234
		SiI9234_init();
	}
	else
	{
                //Rajucm: TODO
               // value.intval = POWER_SUPPLY_TYPE_NONE;
               // if (mhl_vbus && (mhl_power_supply!=NULL) && (mhl_power_supply->set_property(mhl_power_supply, POWER_SUPPLY_PROP_ONLINE, &value)))
		  //  printk("Fail to turn %s MHL Charging\n",sw?"on":"off");

                mhl_vbus = false;
		sii9234_cfg_power(0);	//Turn Off power to SiI9234
	}

	/********************RAJUCM:FSA-MHL-FSA Switching start***************************/
	i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, 0x01 | i2c_smbus_read_byte_data(client, FSA9480_REG_CTRL));
	{
		gpio_set_value(GPIO_MHL_SEL, sw);
		//Clear unwanted interrupts during 1k  an 368 ohm  Switching
		i2c_smbus_read_byte_data(client, FSA9480_REG_INT1);
		i2c_smbus_read_byte_data(client, FSA9480_REG_INT1);
	}
	i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, 0xFE & i2c_smbus_read_byte_data(client, FSA9480_REG_CTRL));
	/******************RAJUCM:FSA-MHL-FSA Switching End*******************************/
	//mutex_unlock(&FSA9480_MhlSwitchSel_lock);//Rajucm

    printk("[FSA] mutex_unlock\n");
}
EXPORT_SYMBOL(FSA9480_MhlSwitchSel);

void FSA9480_MhlTvOff(void)
{
	struct i2c_client *client =  local_usbsw->client;
	int intr1;
	i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, 0x01 | i2c_smbus_read_byte_data(client, FSA9480_REG_CTRL));
	//printk(KERN_ERR "%s: started######\n", __func__);
	intr1 = i2c_smbus_read_word_data(client, FSA9480_REG_INT1);
	//gpio_set_value(GPIO_MHL_SEL, 0);
	gpio_set_value_cansleep(GPIO_MHL_SEL, 0);

	do {
		msleep(10);
		intr1 = i2c_smbus_read_byte_data(client, FSA9480_REG_INT1);
	}while(!intr1);

	mhl_cable_status =0x08;//MHL_TV_OFF_CABLE_CONNECT;
	i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, 0xFE & i2c_smbus_read_byte_data(client, FSA9480_REG_CTRL));
	//printk(KERN_ERR "%s: End######\n",__func__);
	 printk("%s:  interrupt1= %d\n", __func__, intr1);
}
EXPORT_SYMBOL(FSA9480_MhlTvOff);
#endif  //CONFIG_VIDEO_MHL_V1
//#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) 	//Build Error
extern int max8998_check_vdcin();

/* need?
u8 FSA9480_Get_FPM_Status(void)
{
	if(fsa9480_adc == RID_FM_BOOT_ON_UART)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(FSA9480_Get_FPM_Status);

FSA9480_DEV_TY1_TYPE FSA9480_Get_DEV_TYP1(void)
{
	return fsa9480_device1;
}
EXPORT_SYMBOL(FSA9480_Get_DEV_TYP1);
*/
/* need?
u8 FSA9480_Get_TA_Status(void)
{
	if(MicroTAstatus)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(FSA9480_Get_TA_Status);
*/
u8 FSA9480_Get_JIG_UART_Status(void)
{
	if(MicroJigUARTOnStatus | MicroJigUARTOffStatus)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(FSA9480_Get_JIG_UART_Status);

// for max8998_function.c (froyo version)
u8 FSA9480_Get_JIG_Status(void)
{
//	if(MicroJigUSBOnStatus | MicroJigUSBOffStatus | MicroJigUARTOnStatus | MicroJigUARTOffStatus)
	if(MicroJigstatus)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(FSA9480_Get_JIG_Status);

u8 FSA9480_Get_USB_Status(void)
{
	if( MicroUSBStatus | MicroJigUSBOnStatus | MicroJigUSBOffStatus )
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(FSA9480_Get_USB_Status);

//NAGSM_Android_SEL_Kernel_20110421
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) // mr work
// for drivers/onedram_svn/onedram.c(Froyo version) 
u8 FSA9480_Get_JIG_UART_On_Status(void)
{
	if(MicroJigUARTOnStatus)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(FSA9480_Get_JIG_UART_On_Status);
#endif

int FSA9480_Get_I2C_USB_Status(void)
{
	struct i2c_client *client = local_usbsw->client;
	unsigned char dev1, dev2;
	int result;
	
	dev1 = i2c_smbus_read_byte_data(client,FSA9480_REG_DEV_T1);
	dev2 = i2c_smbus_read_byte_data(client,FSA9480_REG_DEV_T2);

       result = dev2 << 8 | dev1; 
	return result;
	
/*		
	if((dev1 & DEV_USB)||(dev2 & DEV_T2_USB_MASK))
		return 1;
	else 
		return 0;

*/		
}
EXPORT_SYMBOL(FSA9480_Get_I2C_USB_Status);
/*
int get_usb_cable_state(void)
{
	return usb_state;
}
*/
// ]] hdlnc_opk

#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
extern u16 askonstatus;
extern u16 inaskonstatus;
#endif
//#endif	Build Error

static ssize_t fsa9480_show_control(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct fsa9480_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int value;

	value = i2c_smbus_read_byte_data(client, FSA9480_REG_CTRL);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	return sprintf(buf, "CONTROL: %02x\n", value);
}

static ssize_t fsa9480_show_device_type(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct fsa9480_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int value;

	value = i2c_smbus_read_byte_data(client, FSA9480_REG_DEV_T1);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	return sprintf(buf, "DEVICE_TYPE: %02x\n", value);
}

static ssize_t fsa9480_show_manualsw(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fsa9480_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	unsigned int value;

	value = i2c_smbus_read_byte_data(client, FSA9480_REG_MANSW1);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	if(usbsw->dev_id == 0)
		value = value & ~0x03;

	if (value == SW_VAUDIO)
		return sprintf(buf, "VAUDIO\n");
	else if (value == SW_UART)
		return sprintf(buf, "UART\n");
	else if (value == SW_AUDIO)
		return sprintf(buf, "AUDIO\n");
	else if (value == SW_DHOST)
		return sprintf(buf, "DHOST\n");
	else if (value == SW_AUTO)
		return sprintf(buf, "AUTO\n");
	else
		return sprintf(buf, "%x", value);
}

static ssize_t fsa9480_set_manualsw(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct fsa9480_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	unsigned int value;
	unsigned int path = 0;
	int ret;

	value = i2c_smbus_read_byte_data(client, FSA9480_REG_CTRL);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	if ((value & ~CON_MANUAL_SW) !=
			(CON_SWITCH_OPEN | CON_RAW_DATA | CON_WAIT))
		return 0;

	if (!strncmp(buf, "VAUDIO", 6)) {
		path = SW_VAUDIO;
		value &= ~CON_MANUAL_SW;
	} else if (!strncmp(buf, "UART", 4)) {
		path = SW_UART;
		value &= ~CON_MANUAL_SW;
	} else if (!strncmp(buf, "AUDIO", 5)) {
		path = SW_AUDIO;
		value &= ~CON_MANUAL_SW;
	} else if (!strncmp(buf, "DHOST", 5)) {
		path = SW_DHOST;
		value &= ~CON_MANUAL_SW;
	} else if (!strncmp(buf, "AUTO", 4)) {
		path = SW_AUTO;
		value |= CON_MANUAL_SW;
	} else {
		dev_err(dev, "Wrong command\n");
		return 0;
	}

	if(usbsw->dev_id == 0)
		path = (path | 0x03);

	usbsw->mansw = path;

	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_MANSW1, path);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return count;
}

static DEVICE_ATTR(control, S_IRUGO, fsa9480_show_control, NULL);
static DEVICE_ATTR(device_type, S_IRUGO, fsa9480_show_device_type, NULL);
static DEVICE_ATTR(switch, S_IRUGO | S_IWUSR,
		fsa9480_show_manualsw, fsa9480_set_manualsw);

static struct attribute *fsa9480_attributes[] = {
	&dev_attr_control.attr,
	&dev_attr_device_type.attr,
	&dev_attr_switch.attr,
	NULL
};

static const struct attribute_group fsa9480_group = {
	.attrs = fsa9480_attributes,
};

void fsa9480_manual_switching(int path)
{
	struct i2c_client *client = local_usbsw->client;
	unsigned int value;
	unsigned int data = 0;
	int ret;

	value = i2c_smbus_read_byte_data(client, FSA9480_REG_CTRL);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	if ((value & ~CON_MANUAL_SW) !=
			(CON_SWITCH_OPEN | CON_RAW_DATA | CON_WAIT))
		return;

	if (path == SWITCH_PORT_VAUDIO) {
		data = SW_VAUDIO;
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_UART) {
		data = SW_UART;
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_AUDIO) {
		data = SW_AUDIO;
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_USB) {
		data = SW_DHOST;
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_AUTO) {
		data = SW_AUTO;
		value |= CON_MANUAL_SW;
	} else {
		printk("%s: wrong path (%d)\n", __func__, path);
		return;
	}

	if(local_usbsw->dev_id == 0)
	{
		local_usbsw->mansw = (data | 0x03);
		data |= 0x03;
	}
	else
	local_usbsw->mansw = data;

	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_MANSW1, data);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)// mr work
	msleep(10);
#endif
	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

}
EXPORT_SYMBOL(fsa9480_manual_switching);
//HDLNC_OPK_20110307
void fsa9480_enable_spk()
{
	#if defined(CONFIG_S5PC110_KEPLER_BOARD)
		fsa9480_manual_switching(SWITCH_PORT_AUDIO);
	#elif defined(CONFIG_S5PC110_HAWK_BOARD)
		fsa9480_manual_switching(SWITCH_PORT_VAUDIO);
	#elif defined(CONFIG_S5PC110_DEMPSEY_BOARD)						// mr work 
		fsa9480_manual_switching(SWITCH_PORT_VAUDIO);
	#else
		fsa9480_manual_switching(SWITCH_PORT_AUDIO);
	#endif
	
}
//HDLNC_OPK_20110307
static void fsa9480_detect_dev(struct fsa9480_usbsw *usbsw)
{
	int device_type, ret;
	unsigned char val1, val2;
	struct fsa9480_platform_data *pdata = usbsw->pdata;
	struct i2c_client *client = usbsw->client;

	device_type = i2c_smbus_read_word_data(client, FSA9480_REG_DEV_T1);
	if (device_type < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, device_type);

	val1 = device_type & 0xff;
	val2 = device_type >> 8;

	dev_info(&client->dev, "dev1: 0x%x, dev2: 0x%x\n", val1, val2);

	/* Attached */
	if (val1 || val2) {
		/* USB */
		if (val1 & DEV_T1_USB_MASK || val2 & DEV_T2_USB_MASK) {
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)// mr work 
			if ( val2 & DEV_JIG_USB_ON )
				MicroJigstatus = 1;
#endif
			if (pdata->usb_cb)
				pdata->usb_cb(FSA9480_ATTACHED);
			if (usbsw->mansw) {
				if(usbsw->dev_id == 0)
					usbsw->mansw = usbsw->mansw | 0x03;

				ret = i2c_smbus_write_byte_data(client,
					FSA9480_REG_MANSW1, usbsw->mansw);
				if (ret < 0)
					dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
			}
		/* UART */
		} else if (val1 & DEV_T1_UART_MASK || val2 & DEV_T2_UART_MASK) {
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)// mr work 
			MicroJigstatus = 1;
#endif
			if (pdata->uart_cb)
				pdata->uart_cb(FSA9480_ATTACHED);
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)// mr work
//parkhj
			if (pdata->charger_cb)
				pdata->charger_cb(FSA9480_ATTACHED);
#endif
			
			if (usbsw->mansw) {
				if(usbsw->dev_id == 0)
				{
					ret = i2c_smbus_write_byte_data(client,	FSA9480_REG_MANSW1, (SW_UART | 0x03));
					if (ret < 0)
						dev_err(&client->dev,
							"%s: err %d\n", __func__, ret);

				}
				else
				{
					ret = i2c_smbus_write_byte_data(client,
						FSA9480_REG_MANSW1, SW_UART);
					if (ret < 0)
						dev_err(&client->dev,
							"%s: err %d\n", __func__, ret);
				}
			}
		/* CHARGER */
		} else if (val1 & DEV_T1_CHARGER_MASK) {
			if (pdata->charger_cb)
				pdata->charger_cb(FSA9480_ATTACHED);
		/* JIG */
		} else if (val2 & DEV_T2_JIG_MASK) {
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD)	|| defined(CONFIG_S5PC110_DEMPSEY_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)// mr work 
			MicroJigstatus = 1; 
#endif
			if (pdata->jig_cb)
				pdata->jig_cb(FSA9480_ATTACHED);
		/* Desk Dock */
		} else if (val2 & DEV_AV) {

#ifdef CONFIG_VIDEO_MHL_V1
		printk(KERN_ERR "FSA MHL Attach");
		printk("mhl_cable_status = %d \n", mhl_cable_status);

		FSA9480_MhlSwitchSel(1);
#else
			
			if (pdata->deskdock_cb)
				pdata->deskdock_cb(FSA9480_ATTACHED);
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD)	|| defined(CONFIG_S5PC110_DEMPSEY_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)// mr work
			fsa9480_enable_spk();	//HDLNC_OPK_20110307
			/* S1
			ret = i2c_smbus_write_byte_data(client,
					FSA9480_REG_MANSW1, SW_DHOST);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_read_byte_data(client,
					FSA9480_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,
					FSA9480_REG_CTRL, ret & ~CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);
			*/
#else
			ret = i2c_smbus_write_byte_data(client,
					FSA9480_REG_MANSW1, SW_VAUDIO);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_read_byte_data(client,
					FSA9480_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,
					FSA9480_REG_CTRL, ret & ~CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);
#endif
		/* Car Dock */
#endif
		} else if (val2 & DEV_JIG_UART_ON) {
			printk("%s] DEV_JIG_UART_ON\n",__func__);

			if (pdata->cardock_cb)
				pdata->cardock_cb(FSA9480_ATTACHED);
			
			MicroJigUARTOnStatus = 1;
#if defined(CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) 	// || defined(CONFIG_S5PC110_DEMPSEY_BOARD) // mr work 
		fsa9480_enable_spk();	 //HDLNC_OPK_20110307
#elif defined(CONFIG_S5PC110_HAWK_BOARD)|| defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)// mr work //subhransu revisit
			if(max8998_check_vdcin())
			{
				fsa9480_enable_spk();	 //HDLNC_OPK_20110307
			}
			else
			{
				if (usbsw->mansw) 
				{
					ret = i2c_smbus_write_byte_data(client,
						FSA9480_REG_MANSW1, SW_UART);
					if (ret < 0)
						dev_err(&client->dev,
							"%s: err %d\n", __func__, ret);
				}
			}
#endif

	
		}
	/* Detached */
	} else {
		/* USB */
		if (usbsw->dev1 & DEV_T1_USB_MASK ||
				usbsw->dev2 & DEV_T2_USB_MASK) {
			if ( usbsw->dev2 & DEV_JIG_USB_ON )
				MicroJigstatus = 0;
#ifndef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE				
			if (pdata->usb_cb)
				pdata->usb_cb(FSA9480_DETACHED);
#else
			if (pdata->usb_cb){
				askonstatus=0;
				inaskonstatus=0;
				pdata->usb_cb(FSA9480_DETACHED);
				}
#endif
		/* UART */
		} else if (usbsw->dev1 & DEV_T1_UART_MASK ||
				usbsw->dev2 & DEV_T2_UART_MASK) {
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)// mr work
			MicroJigstatus = 0;
#endif
			if (pdata->uart_cb)
				pdata->uart_cb(FSA9480_DETACHED);
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)// mr work
//parkhj			
			if (pdata->charger_cb)
				pdata->charger_cb(FSA9480_DETACHED);
#endif
			
		/* CHARGER */
		} else if (usbsw->dev1 & DEV_T1_CHARGER_MASK) {
			if (pdata->charger_cb)
				pdata->charger_cb(FSA9480_DETACHED);
		/* JIG */
		} else if (usbsw->dev2 & DEV_T2_JIG_MASK) {
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)// mr work 
			MicroJigstatus = 0; 
#endif
			if (pdata->jig_cb)
				pdata->jig_cb(FSA9480_DETACHED);
		/* Desk Dock */
		} else if (usbsw->dev2 & DEV_AV) {

#ifdef CONFIG_VIDEO_MHL_V1
		printk("FSA MHL Detach\n");
		FSA9480_MhlSwitchSel(0);
#else 		
			if (pdata->deskdock_cb)
				pdata->deskdock_cb(FSA9480_DETACHED);

			ret = i2c_smbus_read_byte_data(client,
					FSA9480_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,
					FSA9480_REG_CTRL, ret | CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);
		/* Car Dock */
#endif		

		} else if (usbsw->dev2 & DEV_JIG_UART_ON) {
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)// mr work 
			MicroJigUARTOnStatus = 0;
#endif
			if (pdata->cardock_cb)
				pdata->cardock_cb(FSA9480_DETACHED);
#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)// mr work 
			ret = i2c_smbus_read_byte_data(client,FSA9480_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,
					FSA9480_REG_CTRL, ret | CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);
#endif	
		}
	}

	usbsw->dev1 = val1;
	usbsw->dev2 = val2;
}

static void fsa9480_reg_init(struct fsa9480_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	unsigned int ctrl = CON_MASK;
	int ret;

			
	usbsw->dev_id = i2c_smbus_read_byte_data(client, FSA9480_REG_DEVID);
	if (usbsw->dev_id < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, usbsw->dev_id);
	printk("fsa9480_reg_init = %d\n", usbsw->dev_id);


	/* mask interrupts (unmask attach/detach only) */
	ret = i2c_smbus_write_word_data(client, FSA9480_REG_INT1_MASK, 0x1ffc);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	/* mask all car kit interrupts */
	ret = i2c_smbus_write_word_data(client, FSA9480_REG_CK_INTMASK1, 0x07ff);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	/* ADC Detect Time: 500ms */
	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_TIMING1, 0x6);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	usbsw->mansw = i2c_smbus_read_byte_data(client, FSA9480_REG_MANSW1);
	if (usbsw->mansw < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, usbsw->mansw);

	if (usbsw->mansw)
		ctrl &= ~CON_MANUAL_SW;	/* Manual Switching Mode */

	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, ctrl);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	#ifdef CONFIG_VIDEO_MHL_V1 //Rajucm: enable av charging interrupt
	i2c_smbus_write_byte_data(client, FSA9480_REG_INT2_MASK,
	0xFE & i2c_smbus_read_byte_data(client, FSA9480_REG_INT2_MASK));
	#endif
}

static irqreturn_t fsa9480_irq_thread(int irq, void *data)
{
	struct fsa9480_usbsw *usbsw = data;
	struct i2c_client *client = usbsw->client;
	int intr;

	/* read and clear interrupt status bits */
	intr = i2c_smbus_read_word_data(client, FSA9480_REG_INT1);
	if (intr < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, intr);
	} else if (intr == 0) {
		/* interrupt was fired, but no status bits were set,
		so device was reset. In this case, the registers were
		reset to defaults so they need to be reinitialised. */
		fsa9480_reg_init(usbsw);
	}

	/* device detection */
	fsa9480_detect_dev(usbsw);

	return IRQ_HANDLED;
}

static int fsa9480_irq_init(struct fsa9480_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	int ret;

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
			fsa9480_irq_thread, IRQF_TRIGGER_FALLING,
			"fsa9480 micro USB", usbsw);
		if (ret) {
			dev_err(&client->dev, "failed to reqeust IRQ\n");
			return ret;
		}

		ret = enable_irq_wake(client->irq);
		if (ret < 0)
			dev_err(&client->dev,
				"failed to enable wakeup src %d\n", ret);
	}

	return 0;
}

static int __devinit fsa9480_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct fsa9480_usbsw *usbsw;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	usbsw = kzalloc(sizeof(struct fsa9480_usbsw), GFP_KERNEL);
	if (!usbsw) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	usbsw->client = client;
	usbsw->pdata = client->dev.platform_data;
	if (!usbsw->pdata)
		goto fail1;

	i2c_set_clientdata(client, usbsw);

	local_usbsw = usbsw;  // temp

	if (usbsw->pdata->cfg_gpio)
		usbsw->pdata->cfg_gpio();

	fsa9480_reg_init(usbsw);

	ret = fsa9480_irq_init(usbsw);
	if (ret)
		goto fail1;

	ret = sysfs_create_group(&client->dev.kobj, &fsa9480_group);
	if (ret) {
		dev_err(&client->dev,
				"failed to create fsa9480 attribute group\n");
		goto fail2;
	}

	if (usbsw->pdata->reset_cb)
		usbsw->pdata->reset_cb();

	/* device detection */
	fsa9480_detect_dev(usbsw);

	// set fsa9480 init flag.
	if (usbsw->pdata->set_init_flag)
		usbsw->pdata->set_init_flag();

	return 0;

fail2:
	if (client->irq)
		free_irq(client->irq, usbsw);
fail1:
	i2c_set_clientdata(client, NULL);
	kfree(usbsw);
	return ret;
}

static int __devexit fsa9480_remove(struct i2c_client *client)
{
	struct fsa9480_usbsw *usbsw = i2c_get_clientdata(client);

	if (client->irq) {
		disable_irq_wake(client->irq);
		free_irq(client->irq, usbsw);
	}
	i2c_set_clientdata(client, NULL);

	sysfs_remove_group(&client->dev.kobj, &fsa9480_group);
	kfree(usbsw);
	return 0;
}

#ifdef CONFIG_PM
static int fsa9480_resume(struct i2c_client *client)
{
	struct fsa9480_usbsw *usbsw = i2c_get_clientdata(client);

	/* device detection */
	fsa9480_detect_dev(usbsw);

	return 0;
}

#else

#define fsa9480_suspend NULL
#define fsa9480_resume NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id fsa9480_id[] = {
	{"fsa9480", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, fsa9480_id);

static struct i2c_driver fsa9480_i2c_driver = {
	.driver = {
		.name = "fsa9480",
	},
	.probe = fsa9480_probe,
	.remove = __devexit_p(fsa9480_remove),
	.resume = fsa9480_resume,
	.id_table = fsa9480_id,
};

static int __init fsa9480_init(void)
{
	return i2c_add_driver(&fsa9480_i2c_driver);
}
module_init(fsa9480_init);

static void __exit fsa9480_exit(void)
{
	i2c_del_driver(&fsa9480_i2c_driver);
}
module_exit(fsa9480_exit);

MODULE_AUTHOR("Minkyu Kang <mk7.kang@samsung.com>");
MODULE_DESCRIPTION("FSA9480 USB Switch driver");
MODULE_LICENSE("GPL");
