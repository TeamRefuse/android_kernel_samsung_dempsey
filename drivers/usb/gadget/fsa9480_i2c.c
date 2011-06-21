#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <plat/pm.h>
#include <asm/irq.h>
#include <linux/delay.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <mach/param.h>
#include "fsa9480_i2c.h"
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>
#include <mach/max8998_function.h>
#include <linux/switch.h>

#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
#include <linux/timer.h>
//MHL v1 //NAGSM_Android_SEL_Kernel_Aakash_20101119
extern void sii9234_cfg_power(bool on);	
extern bool SiI9234_init(void);		
extern void SiI9234_interrupt_event(void);
//MHL v1 //NAGSM_Android_SEL_Kernel_Aakash_20101119

extern byte mhl_cable_status;
extern int SII9234_i2c_status;
extern int max8893_i2cprobe_status;
extern bool S5p_TvProbe_status; //Rajucm
extern bool vbus_mhl_est_fail; //3355
extern bool mhl_vbus; //3355
extern void FSA9480_CheckAndHookAudioDock(void);//Rajucm
extern void FSA9480_MhlSwitchSel(bool);//Rajucm
extern void FSA9480_MhlTvOff(void);//Rajucm
extern int s5p_hpd_get_state(void);
extern void SiI9234_HW_Reset(void);
extern void mhl_hpd_handle(bool); //Rajucm
#define MHL_INIT_COND (SII9234_i2c_status &&  max8893_i2cprobe_status && S5p_TvProbe_status)
static DECLARE_WAIT_QUEUE_HEAD(fsa9480_MhlWaitEvent);
#endif

extern void otg_phy_init(void);
extern void otg_phy_off(void);
extern struct device *switch_dev;
extern int askonstatus;
extern int inaskonstatus;
extern int BOOTUP;

static int g_tethering;
static int g_dock;

#define DOCK_REMOVED 0
#define HOME_DOCK_INSERTED 1
#define CAR_DOCK_INSERTED 2

int oldusbstatus=0;

int mtp_mode_on = 0;

#define FSA9480_UART 	1
#define FSA9480_USB 	2

#define log_usb_disable 	0
#define log_usb_enable 		1
#define log_usb_active		2

static u8 MicroUSBStatus=0;
static u8 MicroJigUSBOnStatus=0;
static u8 MicroJigUSBOffStatus=0;
static u8 MicroJigUARTOffStatus=0;
static u8 MicroJigUARTOnStatus=0;
u8 MicroTAstatus=0;

/* switch selection */
#define USB_SEL_MASK  				(1 << 0)
#define UART_SEL_MASK				(1 << 1)
#define USB_SAMSUNG_KIES_MASK		(1 << 2)
#define USB_UMS_MASK				(1 << 3)
#define USB_MTP_MASK				(1 << 4)
#if !defined(CONFIG_ARIES_NTT) // disable tethering xmoondash
#define USB_VTP_MASK				(1 << 5)
#endif
#define USB_ASKON_MASK			(1 << 6)

#define DRIVER_NAME  "usb_mass_storage"

struct i2c_driver fsa9480_i2c_driver;
static struct i2c_client *fsa9480_i2c_client = NULL;

struct fsa9480_state {
	struct i2c_client *client;
};

static struct i2c_device_id fsa9480_id[] = {
	{"fsa9480", 0},
	{}
};

static u8 fsa9480_device1 = 0, fsa9480_device2 = 0, fsa9480_adc = 0, fsa9480_interrupt1 =0, fsa9480_interrupt2 =0;
int usb_path = 0;
static int usb_state = 0;
int log_via_usb = log_usb_disable;
static int switch_sel = 0;

int mtp_power_off = 0;

static wait_queue_head_t usb_detect_waitq;
static struct workqueue_struct *fsa9480_workqueue;
static struct work_struct fsa9480_work;
struct switch_dev indicator_dev;
struct delayed_work micorusb_init_work;

#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
struct delayed_work sii9234_initialize_work;
#endif

extern int currentusbstatus;
byte switchinginitvalue[12];
byte uart_message[6];
byte usb_message[5];
int factoryresetstatus=0;
extern int usb_mtp_select(int disable);
extern int usb_switch_select(int enable);
extern int askon_switch_select(int enable);
extern unsigned int charging_mode_get(void);
extern void set_dock_state(int value);

int samsung_kies_mtp_mode_flag;
void FSA9480_Enable_CP_USB(u8 enable);
void FSA9480_Enable_CP_USB_New(u8 enable);
void FSA9480_Enable_CP_USB_Old(u8 enable);

FSA9480_DEV_TY1_TYPE FSA9480_Get_DEV_TYP1(void)
{
	return fsa9480_device1;
}
EXPORT_SYMBOL(FSA9480_Get_DEV_TYP1);


u8 FSA9480_Get_JIG_Status(void)
{
	if(MicroJigUSBOnStatus | MicroJigUSBOffStatus | MicroJigUARTOnStatus | MicroJigUARTOffStatus)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(FSA9480_Get_JIG_Status);


u8 FSA9480_Get_FPM_Status(void)
{
	if(fsa9480_adc == RID_FM_BOOT_ON_UART)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(FSA9480_Get_FPM_Status);


u8 FSA9480_Get_USB_Status(void)
{
	if( MicroUSBStatus | MicroJigUSBOnStatus | MicroJigUSBOffStatus )
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(FSA9480_Get_USB_Status);


u8 FSA9480_Get_TA_Status(void)
{
	if(MicroTAstatus)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(FSA9480_Get_TA_Status);

u8 FSA9480_Get_JIG_UART_Status(void)
{
	if(MicroJigUARTOnStatus | MicroJigUARTOffStatus)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(FSA9480_Get_JIG_UART_Status);

#if defined (CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined (CONFIG_S5PC110_DEMPSEY_BOARD)
u8 FSA9480_Get_JIG_UART_On_Status(void)
{
	if(MicroJigUARTOnStatus)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(FSA9480_Get_JIG_UART_On_Status);
#endif

int get_usb_cable_state(void)
{
	return usb_state;
}
EXPORT_SYMBOL(get_usb_cable_state);


static int fsa9480_read(struct i2c_client *client, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		return -EIO;

	*data = ret & 0xff;
	return 0;
}


static int fsa9480_write(struct i2c_client *client, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(client, reg, data);
}

//NAGSM_Android_SEL_Kernel_Aakash_20101214
void DisableFSA9480Interrupts()
{
        struct i2c_client *client = fsa9480_i2c_client;
        u8 controlReg = 0;
        fsa9480_read(client, REGISTER_CONTROL, &controlReg);
        msleep(5);
        controlReg |= 0x01;
        fsa9480_write(client, REGISTER_CONTROL, controlReg);
} // DisableFSA9480Interrupts()

void EnableFSA9480Interrupts()
{
        struct i2c_client *client = fsa9480_i2c_client;
        u8 controlReg = 0;
        fsa9480_read(client, REGISTER_CONTROL, &controlReg);
        msleep(5);
        controlReg &= 0xFE;
        fsa9480_write(client, REGISTER_CONTROL, controlReg);
} //EnableFSA9480Interrupts()
EXPORT_SYMBOL(EnableFSA9480Interrupts);
EXPORT_SYMBOL(DisableFSA9480Interrupts); //Rajucm: used in sii9234_driver file
//NAGSM_Android_SEL_Kernel_Aakash_20101214

void ap_usb_power_on(int set_vaue)
{
 	byte reg_value=0;
	byte reg_address=0x0D;

	if(set_vaue){

		Get_MAX8998_PM_ADDR(reg_address, &reg_value, 1); // read 0x0D register
		reg_value = reg_value | (0x1 << 7);
		Set_MAX8998_PM_ADDR(reg_address,&reg_value,1);
		printk("[ap_usb_power_on]AP USB Power ON, askon: %d, mtp : %d\n",askonstatus,mtp_mode_on);
			if(mtp_mode_on == 1) {
				samsung_kies_mtp_mode_flag = 1;
				printk("************ [ap_usb_power_on] samsung_kies_mtp_mode_flag:%d, mtp:%d\n", samsung_kies_mtp_mode_flag, mtp_mode_on);
			}
			else {
				samsung_kies_mtp_mode_flag = 0;
				printk("!!!!!!!!!!! [ap_usb_power_on]AP samsung_kies_mtp_mode_flag%d, mtp:%d\n",samsung_kies_mtp_mode_flag, mtp_mode_on);
			}
		}
	else{
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
//3355
    if(vbus_mhl_est_fail == TRUE)
    {
    
      FSA9480_MhlSwitchSel(0);
    }

    mhl_vbus = FALSE;
  //3355  
  #endif 
		Get_MAX8998_PM_ADDR(reg_address, &reg_value, 1); // read 0x0D register
		reg_value = reg_value & ~(0x1 << 7);
		Set_MAX8998_PM_ADDR(reg_address,&reg_value,1);
		printk("[ap_usb_power_on]AP USB Power OFF, askon: %d, mtp : %d\n",askonstatus,mtp_mode_on);
		}
}

		
void usb_api_set_usb_switch(USB_SWITCH_MODE usb_switch)
{
	if(usb_switch == USB_SW_CP)
	{
		//USB_SEL GPIO Set High => CP USB enable
		FSA9480_Enable_CP_USB(1);
	}
	else
	{
		//USB_SEL GPIO Set Low => AP USB enable
		FSA9480_Enable_CP_USB(0);
	}
}


void Ap_Cp_Switch_Config(u16 ap_cp_mode)
{
	switch (ap_cp_mode) {
		case AP_USB_MODE:
			usb_path=1;
			usb_api_set_usb_switch(USB_SW_AP);
			break;
		case AP_UART_MODE:
			gpio_set_value(GPIO_UART_SEL, 1);
			break;
		case CP_USB_MODE:
			usb_path=2;
			usb_api_set_usb_switch(USB_SW_CP);
			break;
		case CP_UART_MODE:
			gpio_set_value(GPIO_UART_SEL, 0);			
			break;
		default:
//			printk("Ap_Cp_Switch_Config error");
			break;
	}
		
}


/* MODEM USB_SEL Pin control */
/* 1 : PDA, 2 : MODEM */
#define SWITCH_PDA		1
#define SWITCH_MODEM		2

void usb_switching_value_update(int value)
{
	if(value == SWITCH_PDA)
		usb_message[0] = 'A';
	else
		usb_message[0] = 'C';		

	usb_message[1] = 'P';
	usb_message[2] = 'U';
	usb_message[3] = 'S';
	usb_message[4] = 'B';

}

void uart_switching_value_update(int value)
{
	if(value == SWITCH_PDA)
		uart_message[0] = 'A';
	else
		uart_message[0] = 'C';

	uart_message[1] = 'P';
	uart_message[2] = 'U';
	uart_message[3] = 'A';
	uart_message[4] = 'R';
	uart_message[5] = 'T';

}


void switching_value_update(void)
{
	int index;
	
	for(index=0;index<5;index++)
		switchinginitvalue[index] = usb_message[index];

	for(index=5;index<11;index++)
		switchinginitvalue[index] = uart_message[index-5];

	switchinginitvalue[11] = '\0';

}


static ssize_t factoryreset_value_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t size)
{
	if(strncmp(buf, "FACTORYRESET", 12) == 0 || strncmp(buf, "factoryreset", 12) == 0)
		factoryresetstatus = 0xAE;

	return size;
}

static DEVICE_ATTR(FactoryResetValue, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, NULL, factoryreset_value_store);


/* for sysfs control (/sys/class/sec/switch/usb_sel) */
static ssize_t usb_sel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
//	struct i2c_client *client = fsa9480_i2c_client;
//	u8 i, pData;

	sprintf(buf, "USB Switch : %s\n", usb_path==SWITCH_PDA?"PDA":"MODEM");

//    	for(i = 0; i <= 0x14; i++)
//		fsa9480_read(client, i, &pData);

	return sprintf(buf, "%s\n", buf);
}


void usb_switch_mode(int sel)
{
	if (sel == SWITCH_PDA)
	{
		DEBUG_FSA9480("[FSA9480] %s: Path = PDA\n", __func__);
		Ap_Cp_Switch_Config(AP_USB_MODE);
	}
	else if (sel == SWITCH_MODEM) 
	{
		DEBUG_FSA9480("[FSA9480] %s: Path = MODEM\n", __func__);
		Ap_Cp_Switch_Config(CP_USB_MODE);
	}
	else
		DEBUG_FSA9480("[FSA9480] Invalid mode...\n");
}
EXPORT_SYMBOL(usb_switch_mode);


void microusb_uart_status(int status)
{
	int uart_sel;
	int usb_sel;

	if(!FSA9480_Get_JIG_UART_Status())	
		return;
	
	if (sec_get_param_value)
		sec_get_param_value(__SWITCH_SEL, &switch_sel);

	uart_sel = (switch_sel & (int)(UART_SEL_MASK)) >> 1;
	usb_sel = switch_sel & (int)(USB_SEL_MASK);

	if(status){
		if(uart_sel)
			Ap_Cp_Switch_Config(AP_UART_MODE);	
		else
			Ap_Cp_Switch_Config(CP_UART_MODE);	
		}
	else{
		if(!usb_sel)
			Ap_Cp_Switch_Config(AP_USB_MODE);
		}
}


static ssize_t usb_sel_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	DEBUG_FSA9480("[FSA9480]%s\n ", __func__);

	if (sec_get_param_value)
		sec_get_param_value(__SWITCH_SEL, &switch_sel);

	if(strncmp(buf, "PDA", 3) == 0 || strncmp(buf, "pda", 3) == 0)
	{
		usb_switch_mode(SWITCH_PDA);
		usb_switching_value_update(SWITCH_PDA);
		switch_sel |= USB_SEL_MASK;
	}

	if(strncmp(buf, "MODEM", 5) == 0 || strncmp(buf, "modem", 5) == 0)
	{
		usb_switch_mode(SWITCH_MODEM);
		usb_switching_value_update(SWITCH_MODEM);		
		switch_sel &= ~USB_SEL_MASK;
	}

	switching_value_update();

	if (sec_set_param_value)
		sec_set_param_value(__SWITCH_SEL, &switch_sel);

	microusb_uart_status(0);

	return size;
}

static DEVICE_ATTR(usb_sel, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, usb_sel_show, usb_sel_store);


int connectivity_switching_init_state=0;
void usb_switch_state(void)
{
	int usb_sel = 0;

	if(!connectivity_switching_init_state)
		return;

	if (sec_get_param_value)
		sec_get_param_value(__SWITCH_SEL, &switch_sel);

	usb_sel = switch_sel & (int)(USB_SEL_MASK);
	
	if(usb_sel)
	{
		usb_switch_mode(SWITCH_PDA);
		usb_switching_value_update(SWITCH_PDA);
	}
	else
	{
		usb_switch_mode(SWITCH_MODEM);
		usb_switching_value_update(SWITCH_MODEM);
	}
}


void uart_insert_switch_state(void)
{
	int usb_sel = 0;

	if(!connectivity_switching_init_state)
		return;
	
	if (sec_get_param_value)
		sec_get_param_value(__SWITCH_SEL, &switch_sel);

	usb_sel = switch_sel & (int)(USB_SEL_MASK);

	if(!usb_sel)
		Ap_Cp_Switch_Config(AP_USB_MODE);
}


void uart_remove_switch_state(void)
{
	int usb_sel = 0;

	if(!connectivity_switching_init_state)
		return;

	if (sec_get_param_value)
		sec_get_param_value(__SWITCH_SEL, &switch_sel);

	usb_sel = switch_sel & (int)(USB_SEL_MASK);

	if(usb_sel)
		Ap_Cp_Switch_Config(AP_USB_MODE);
	else
		Ap_Cp_Switch_Config(CP_USB_MODE);

}


/**********************************************************************
*    Name         : usb_state_show()
*    Description : for sysfs control (/sys/class/sec/switch/usb_state)
*                        return usb state using fsa9480's device1 and device2 register
*                        this function is used only when NPS want to check the usb cable's state.
*    Parameter   :
*                       
*                       
*    Return        : USB cable state's string
*                        USB_STATE_CONFIGURED is returned if usb cable is connected
***********************************************************************/
static ssize_t usb_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int cable_state = get_usb_cable_state();

	sprintf(buf, "%s\n", (cable_state & (CRB_JIG_USB<<8 | CRA_USB<<0 ))?"USB_STATE_CONFIGURED":"USB_STATE_NOTCONFIGURED");

	return sprintf(buf, "%s\n", buf);
} 


/**********************************************************************
*    Name         : usb_state_store()
*    Description : for sysfs control (/sys/class/sec/switch/usb_state)
*                        noting to do.
*    Parameter   :
*                       
*                       
*    Return        : None
*
***********************************************************************/
static ssize_t usb_state_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	DEBUG_FSA9480("[FSA9480]%s\n ", __func__);
	return 0;
}

/*sysfs for usb cable's state.*/
static DEVICE_ATTR(usb_state, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, usb_state_show, usb_state_store);


static int uart_current_owner = 1;
static ssize_t uart_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{	
	if (uart_current_owner)
		return sprintf(buf, "%s[UART Switch] Current UART owner = PDA \n", buf);	
	else			
		return sprintf(buf, "%s[UART Switch] Current UART owner = MODEM \n", buf);
}

static ssize_t uart_switch_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t size)
{	
	if (sec_get_param_value)
		sec_get_param_value(__SWITCH_SEL, &switch_sel);

	if (strncmp(buf, "PDA", 3) == 0 || strncmp(buf, "pda", 3) == 0)	{		
		Ap_Cp_Switch_Config(AP_UART_MODE);
		uart_switching_value_update(SWITCH_PDA);
		uart_current_owner = 1;		
		switch_sel |= UART_SEL_MASK;
		//printk("[UART Switch] Path : PDA\n");	
	}	

	if (strncmp(buf, "MODEM", 5) == 0 || strncmp(buf, "modem", 5) == 0) {		
		Ap_Cp_Switch_Config(CP_UART_MODE);
		uart_switching_value_update(SWITCH_MODEM);
		uart_current_owner = 0;		
		switch_sel &= ~UART_SEL_MASK;
		//printk("[UART Switch] Path : MODEM\n");	
	}

	switching_value_update();	

	if (sec_set_param_value)
		sec_set_param_value(__SWITCH_SEL, &switch_sel);

	return size;
}

static DEVICE_ATTR(uart_sel, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, uart_switch_show, uart_switch_store);


void FSA9480_ChangePathToAudio(u8 enable)
{
	struct i2c_client *client = fsa9480_i2c_client;
	u8 manualsw1;

	if(enable)
	{
		mdelay(10);
		fsa9480_write(client, REGISTER_MANUALSW1, 0x48);			

		mdelay(10);
		fsa9480_write(client, REGISTER_CONTROL, 0x1A);

		fsa9480_read(client, REGISTER_MANUALSW1, &manualsw1);
		printk("Fsa9480 ManualSW1 = 0x%x\n",manualsw1);
	}
	else
	{
		mdelay(10);
		fsa9480_write(client, REGISTER_CONTROL, 0x1E);	
	}
}
EXPORT_SYMBOL(FSA9480_ChangePathToAudio);


// TODO : need these?
static ssize_t DMport_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{	
		
		return sprintf(buf, "%s[UsbMenuSel test] ready!! \n", buf);	
}

static ssize_t DMport_switch_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t size)
{	
#if 0
	int fd;
	int i;
	char dm_buf[] = "DM port test sucess\n";
	int old_fs = get_fs();
	set_fs(KERNEL_DS);
	
		if (strncmp(buf, "DMport", 5) == 0 || strncmp(buf, "dmport", 5) == 0) {		
			
				fd = sys_open("/dev/dm", O_RDWR, 0);
				if(fd < 0){
				//printk("Cannot open the file");
				return fd;}
				for(i=0;i<5;i++)
				{		
				sys_write(fd,dm_buf,sizeof(dm_buf));
				mdelay(1000);
				}
		sys_close(fd);
		set_fs(old_fs);				
		}

		if ((strncmp(buf, "logusb", 6) == 0)||(log_via_usb == log_usb_enable)) {		
			log_via_usb = log_usb_active;
				//printk("denis_test_prink_log_via_usb_1\n");
				mdelay(1000);
				//printk(KERN_INFO"%s: 21143 10baseT link beat good.\n", "denis_test");
			set_fs(old_fs);				
			}
		return size;
#else
		if (strncmp(buf, "ENABLE", 6) == 0)
		{
			usb_switch_select(USBSTATUS_DM);
		}

		if (strncmp(buf, "DISABLE", 6) == 0)
		{
			usb_switch_select(USBSTATUS_SAMSUNG_KIES);			
		}
		
	return size;
#endif
}


static DEVICE_ATTR(DMport, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, DMport_switch_show, DMport_switch_store);


//denis
static ssize_t DMlog_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{	
		
		return sprintf(buf, "%s[DMlog test] ready!! \n", buf);	
}

static ssize_t DMlog_switch_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t size)
{
		if (strncmp(buf, "CPONLY", 6) == 0)
		{
			//printk("DMlog selection : CPONLY\n");
}

		if (strncmp(buf, "APONLY", 6) == 0)
		{
			//printk("DMlog selection : APONLY\n");
		}

		if (strncmp(buf, "CPAP", 4) == 0)
		{
			//printk("DMlog selection : AP+CP\n");
		}
		
		
	return size;
}


static DEVICE_ATTR(DMlog, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, DMlog_switch_show, DMlog_switch_store);


void UsbMenuSelStore(int sel)
{	
	if (sec_get_param_value)
		sec_get_param_value(__SWITCH_SEL, &switch_sel);	

	if(sel == 0){
		switch_sel &= ~(int)USB_UMS_MASK;
		switch_sel &= ~(int)USB_MTP_MASK;
#if !defined(CONFIG_ARIES_NTT) // disable tethering xmoondash
		switch_sel &= ~(int)USB_VTP_MASK;
#endif
		switch_sel &= ~(int)USB_ASKON_MASK;		
		switch_sel |= (int)USB_SAMSUNG_KIES_MASK;	
		}
	else if(sel == 1){
		switch_sel &= ~(int)USB_UMS_MASK;
		switch_sel &= ~(int)USB_SAMSUNG_KIES_MASK;
#if !defined(CONFIG_ARIES_NTT) // disable tethering xmoondash
		switch_sel &= ~(int)USB_VTP_MASK;
#endif
		switch_sel &= ~(int)USB_ASKON_MASK;				
		switch_sel |= (int)USB_MTP_MASK;		
		}
	else if(sel == 2){
		switch_sel &= ~(int)USB_SAMSUNG_KIES_MASK;
		switch_sel &= ~(int)USB_MTP_MASK;
#if !defined(CONFIG_ARIES_NTT) // disable tethering xmoondash
		switch_sel &= ~(int)USB_VTP_MASK;
#endif
		switch_sel &= ~(int)USB_ASKON_MASK;				
		switch_sel |= (int)USB_UMS_MASK;
		}
	else if(sel == 3){
		switch_sel &= ~(int)USB_UMS_MASK;
		switch_sel &= ~(int)USB_MTP_MASK;
		switch_sel &= ~(int)USB_SAMSUNG_KIES_MASK;
		switch_sel &= ~(int)USB_ASKON_MASK;				
#if !defined(CONFIG_ARIES_NTT) // disable tethering xmoondash
		switch_sel |= (int)USB_VTP_MASK;	
#endif
		}
	else if(sel == 4){
		switch_sel &= ~(int)USB_UMS_MASK;
		switch_sel &= ~(int)USB_MTP_MASK;
#if !defined(CONFIG_ARIES_NTT) // disable tethering xmoondash
		switch_sel &= ~(int)USB_VTP_MASK;
#endif
		switch_sel &= ~(int)USB_SAMSUNG_KIES_MASK;				
		switch_sel |= (int)USB_ASKON_MASK;	
		}
	
	if (sec_set_param_value)
		sec_set_param_value(__SWITCH_SEL, &switch_sel);
}

void PathSelStore(int sel)
{	
	if (sec_get_param_value)
		sec_get_param_value(__SWITCH_SEL, &switch_sel);	

	if(sel == AP_USB_MODE){
		switch_sel |= USB_SEL_MASK;	
		}
	else if(sel == CP_USB_MODE){
		switch_sel &= ~USB_SEL_MASK;		
		}
	else if(sel == AP_UART_MODE){
		switch_sel |= UART_SEL_MASK;
		}
	else if(sel == CP_UART_MODE){
		switch_sel &= ~UART_SEL_MASK;	
		}
	
	if (sec_set_param_value)
		sec_set_param_value(__SWITCH_SEL, &switch_sel);
}


static ssize_t UsbMenuSel_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{	
		if (currentusbstatus == USBSTATUS_UMS) 
			return sprintf(buf, "%s[UsbMenuSel] UMS\n", buf);	
		
		else if (currentusbstatus == USBSTATUS_SAMSUNG_KIES) 
			return sprintf(buf, "%s[UsbMenuSel] ACM_MTP\n", buf);	
		
		else if (currentusbstatus == USBSTATUS_MTPONLY) 
			return sprintf(buf, "%s[UsbMenuSel] MTP\n", buf);	
		
		else if (currentusbstatus == USBSTATUS_ASKON) 
			return sprintf(buf, "%s[UsbMenuSel] ASK\n", buf);	
#if !defined(CONFIG_ARIES_NTT) // disable tethering xmoondash
		else if (currentusbstatus == USBSTATUS_VTP) 
			return sprintf(buf, "%s[UsbMenuSel] VTP\n", buf);	
#endif		
		else if (currentusbstatus == USBSTATUS_ADB) 
			return sprintf(buf, "%s[UsbMenuSel] ACM_ADB_UMS\n", buf);	
}


static ssize_t UsbMenuSel_switch_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t size)
{		
		if (strncmp(buf, "KIES", 4) == 0)
		{
			UsbMenuSelStore(0);		
			usb_switch_select(USBSTATUS_SAMSUNG_KIES);
		}

		if (strncmp(buf, "MTP", 3) == 0)
		{
			UsbMenuSelStore(1);					
			usb_switch_select(USBSTATUS_MTPONLY);
		}
		
		if (strncmp(buf, "UMS", 3) == 0)
		{
			UsbMenuSelStore(2);							
			usb_switch_select(USBSTATUS_UMS);
		}
#if !defined(CONFIG_ARIES_NTT) // disable tethering xmoondash
		if (strncmp(buf, "VTP", 3) == 0)
		{
			UsbMenuSelStore(3);							
			usb_switch_select(USBSTATUS_VTP);
		}
#endif
		if (strncmp(buf, "ASKON", 5) == 0)
		{		
			UsbMenuSelStore(4);									
			usb_switch_select(USBSTATUS_ASKON);			
		}

	return size;
}

static DEVICE_ATTR(UsbMenuSel, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, UsbMenuSel_switch_show, UsbMenuSel_switch_store);


extern int inaskonstatus;
static ssize_t AskOnStatus_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(inaskonstatus)
		return sprintf(buf, "%s\n", "Blocking");
	else
		return sprintf(buf, "%s\n", "NonBlocking");
}


static ssize_t AskOnStatus_switch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{		
	return size;
}

static DEVICE_ATTR(AskOnStatus, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, AskOnStatus_switch_show, AskOnStatus_switch_store);


static ssize_t AskOnMenuSel_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{	
		return sprintf(buf, "%s[AskOnMenuSel] Port test ready!! \n", buf);	
}

static ssize_t AskOnMenuSel_switch_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t size)
{		
		if (strncmp(buf, "KIES", 4) == 0)
		{
			askon_switch_select(USBSTATUS_SAMSUNG_KIES);
		}

		if (strncmp(buf, "MTP", 3) == 0)
		{
			askon_switch_select(USBSTATUS_MTPONLY);
		}
		
		if (strncmp(buf, "UMS", 3) == 0)
		{
			askon_switch_select(USBSTATUS_UMS);
		}
#if !defined(CONFIG_ARIES_NTT) // disable tethering xmoondash
		if (strncmp(buf, "VTP", 3) == 0)
		{
			askon_switch_select(USBSTATUS_VTP);
		}
#endif
	return size;
}

static DEVICE_ATTR(AskOnMenuSel, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, AskOnMenuSel_switch_show, AskOnMenuSel_switch_store);


static ssize_t Mtp_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{	
		return sprintf(buf, "%s[Mtp] MtpDeviceOn \n", buf);	
}

static ssize_t Mtp_switch_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t size)
{		
	if (strncmp(buf, "Mtp", 3) == 0)
		{
			if(mtp_mode_on)
				{
				//printk("[Mtp_switch_store]AP USB power on. \n");
#ifdef VODA
				askon_switch_select(USBSTATUS_SAMSUNG_KIES);
#endif
				ap_usb_power_on(1);
				}
		}
	else if (strncmp(buf, "OFF", 3) == 0)
		{
				//printk("[Mtp_switch_store]AP USB power off. \n");
				usb_state = 0;
				usb_mtp_select(1);
		}
	return size;
}

static DEVICE_ATTR(Mtp, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, Mtp_switch_show, Mtp_switch_store);


static int mtpinitstatus=0;
static ssize_t MtpInitStatusSel_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{	
	if(mtpinitstatus == 2)
		return sprintf(buf, "%s\n", "START");
	else
		return sprintf(buf, "%s\n", "STOP");
}

static ssize_t MtpInitStatusSel_switch_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t size)
{
	mtpinitstatus = mtpinitstatus + 1;

	return size;
}

static DEVICE_ATTR(MtpInitStatusSel, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, MtpInitStatusSel_switch_show, MtpInitStatusSel_switch_store);

void UsbIndicator(u8 state)
{
	switch_set_state(&indicator_dev, state);
}

static ssize_t tethering_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (g_tethering)
		return sprintf(buf, "1\n");
	else			
		return sprintf(buf, "0\n");
}

static ssize_t tethering_switch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int usbstatus;
	usbstatus = FSA9480_Get_USB_Status();
	printk("usbstatus = 0x%x, currentusbstatus = 0x%x\n", usbstatus, currentusbstatus);

	if (strncmp(buf, "1", 1) == 0)
	{
		printk("tethering On\n");

		g_tethering = 1;
		usb_switch_select(USBSTATUS_VTP);
		UsbIndicator(0);
	}
	else if (strncmp(buf, "0", 1) == 0)
	{
		printk("tethering Off\n");

		g_tethering = 0;
		usb_switch_select(oldusbstatus);
		if(usbstatus)
			UsbIndicator(1);
	}

	return size;
}

static DEVICE_ATTR(tethering, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, tethering_switch_show, tethering_switch_store);


static ssize_t dock_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (g_dock == 0)
		return sprintf(buf, "0\n");
	else if (g_dock == 1)
		return sprintf(buf, "1\n");
	else if (g_dock == 2)
		return sprintf(buf, "2\n");
}

static ssize_t dock_switch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	if (strncmp(buf, "0", 1) == 0)
	{
		printk("remove dock\n");
		g_dock = 0;
	}
	else if (strncmp(buf, "1", 1) == 0)
	{
		printk("home dock inserted\n");
		g_dock = 1;
	}
	else if (strncmp(buf, "2", 1) == 0)
	{
		printk("car dock inserted\n");
		g_dock = 2;
	}

	return size;
}

static DEVICE_ATTR(dock, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, dock_switch_show, dock_switch_store);




static int askinitstatus=0;
static ssize_t AskInitStatusSel_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{	
	if(askinitstatus == 2)
		return sprintf(buf, "%s\n", "START");
	else
		return sprintf(buf, "%s\n", "STOP");
}

static ssize_t AskInitStatusSel_switch_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t size)
{
	askinitstatus = askinitstatus + 1;

	return size;
}

static DEVICE_ATTR(AskInitStatusSel, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, AskInitStatusSel_switch_show, AskInitStatusSel_switch_store);


// TODO : what should s1_froyo use?
#if 1 //P1_proyo
static ssize_t get_SwitchingInitValue(struct device *dev, struct device_attribute *attr,	char *buf)
{	
	return snprintf(buf, 12, "%s\n", switchinginitvalue);
}
#else //S1
static ssize_t get_SwitchingInitValue(struct device *dev, struct device_attribute *attr,	const char *buf)
{	
	return snprintf(buf, 12, "%d\n", switchinginitvalue);
}
#endif

static DEVICE_ATTR(SwitchingInitValue, S_IRUGO, get_SwitchingInitValue, NULL);


int  FSA9480_PMIC_CP_USB(void)
{
	int usb_sel = 0;

	if (sec_get_param_value)
		sec_get_param_value(__SWITCH_SEL, &switch_sel);

	usb_sel = switch_sel & (int)(USB_SEL_MASK);

	return usb_sel;
}


int check_reg=0;
extern unsigned int HWREV;
extern unsigned int VPLUSVER;

void FSA9480_Enable_CP_USB(u8 enable)
{
#if defined(CONFIG_S5PC110_SIDEKICK_BOARD)
	if( HWREV <= 0x0b)
		FSA9480_Enable_CP_USB_New(enable);
	else
		FSA9480_Enable_CP_USB_Old(enable);
		
#elif defined(CONFIG_S5PC110_HAWK_BOARD)
	if((0x04 <= HWREV) && (0x0a > HWREV))
		FSA9480_Enable_CP_USB_New(enable);
	else
		FSA9480_Enable_CP_USB_Old(enable);
		
#elif defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD)
	if(VPLUSVER == 1)
		FSA9480_Enable_CP_USB_New(enable);
	else
		FSA9480_Enable_CP_USB_Old(enable);
		
#elif defined(CONFIG_S5PC110_DEMPSEY_BOARD)
	if (HWREV >= 0x05)
		FSA9480_Enable_CP_USB_New(enable);
	else
		FSA9480_Enable_CP_USB_Old(enable);
#else
	FSA9480_Enable_CP_USB_Old(enable);
	
#endif
}
	
void FSA9480_Enable_CP_USB_New(u8 enable)
{
	struct i2c_client *client = fsa9480_i2c_client;
	byte reg_value=0;
	byte reg_address=0x0D;

	if(enable)
	{	
		if((fsa9480_interrupt1 & 0x1)  && (fsa9480_device1 != FSA9480_DEV_TY1_DED_CHG) && (fsa9480_device1 == FSA9480_DEV_TY1_USB))
		{
			//printk("[FSA9480_Enable_CP_USB] Enable CP USB\n");
			mdelay(10);
			Get_MAX8998_PM_ADDR(reg_address, &reg_value, 1); // read 0x0D register
			check_reg = reg_value;
			reg_value = ((0x2<<5)|reg_value);
			check_reg = reg_value;
			Set_MAX8998_PM_ADDR(reg_address,&reg_value,1);
			check_reg = reg_value;
				
			mdelay(10);
		}
		else
		{
			Get_MAX8998_PM_ADDR(reg_address, &reg_value, 1); // read 0x0D register
			check_reg = reg_value;
			reg_value = ( 0xBF & reg_value );
			check_reg = reg_value;
			Set_MAX8998_PM_ADDR(reg_address,&reg_value,1);
			check_reg = reg_value;
		
			mdelay(10);
		}


#ifdef CONFIG_S5PC110_T959_BOARD
		//printk("HWREV is 0x%x\n", HWREV);		

		if(HWREV == 0x0A) // [hdlnc] T959 HWREV 0.0
		   fsa9480_write(client, REGISTER_MANUALSW1, 0x90); // D+/- switching by V_Audio_L/R 
		else              // [hdlnc] T959 HWREV 0.1 over
		   fsa9480_write(client, REGISTER_MANUALSW1, 0x48); //D+/- switching by Audio_L/R 
#elif defined(CONFIG_S5PC110_HAWK_BOARD)||defined(CONFIG_S5PC110_SIDEKICK_BOARD)||defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD)
		fsa9480_write(client, REGISTER_MANUALSW1, 0x48);  // D+/- switching by Audio_L/R 
#elif defined(CONFIG_S5PC110_KEPLER_BOARD)
		fsa9480_write(client, REGISTER_MANUALSW1, 0x90);  // D+/- switching by V_Audio_L/R
#else
		fsa9480_write(client, REGISTER_MANUALSW1, 0x90);// D+/- switching by V_Audio_L/R 
#endif	
		mdelay(10);
		fsa9480_write(client, REGISTER_CONTROL, 0x1A);	

	}
	else
	{
		//printk("[FSA9480_Enable_CP_USB] Disable CP USB\n");
		Get_MAX8998_PM_ADDR(reg_address, &reg_value, 1); // read 0x0D register
		check_reg = reg_value;
		reg_value = ( 0xBF & reg_value );
		check_reg = reg_value;
		Set_MAX8998_PM_ADDR(reg_address,&reg_value,1);
		check_reg = reg_value;
			
		mdelay(10);
		
	
		if(askonstatus||mtp_mode_on)
		ap_usb_power_on(0);
		else
		ap_usb_power_on(1);
		mdelay(10);
		fsa9480_write(client, REGISTER_CONTROL, 0x1E);
	}
	
}

void FSA9480_Enable_CP_USB_Old(u8 enable)
{
	struct i2c_client *client = fsa9480_i2c_client;
	byte reg_value=0;
	byte reg_address=0x0D;

	if(enable)
	{
		//printk("[FSA9480_Enable_CP_USB] Enable CP USB\n");
		mdelay(10);
		Get_MAX8998_PM_ADDR(reg_address, &reg_value, 1); // read 0x0D register
		check_reg = reg_value;
		reg_value = ((0x2<<5)|reg_value);
		check_reg = reg_value;
		Set_MAX8998_PM_ADDR(reg_address,&reg_value,1);
		check_reg = reg_value;
			
		mdelay(10);
#ifdef CONFIG_S5PC110_T959_BOARD
		//printk("HWREV is 0x%x\n", HWREV);		

		if(HWREV == 0x0A) // [hdlnc] T959 HWREV 0.0
		   fsa9480_write(client, REGISTER_MANUALSW1, 0x90); // D+/- switching by V_Audio_L/R 
		else              // [hdlnc] T959 HWREV 0.1 over
		   fsa9480_write(client, REGISTER_MANUALSW1, 0x48); //D+/- switching by Audio_L/R 
#elif defined(CONFIG_S5PC110_HAWK_BOARD)||defined(CONFIG_S5PC110_SIDEKICK_BOARD)||defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD)
		fsa9480_write(client, REGISTER_MANUALSW1, 0x48);  // D+/- switching by Audio_L/R 
#elif defined(CONFIG_S5PC110_KEPLER_BOARD)
		fsa9480_write(client, REGISTER_MANUALSW1, 0x90);  // D+/- switching by V_Audio_L/R
#else
		fsa9480_write(client, REGISTER_MANUALSW1, 0x90);// D+/- switching by V_Audio_L/R 
#endif	
		mdelay(10);
		fsa9480_write(client, REGISTER_CONTROL, 0x1A);	

	}
	else
	{
		//printk("[FSA9480_Enable_AP_USB] Enable AP USB\n");
		Get_MAX8998_PM_ADDR(reg_address, &reg_value, 1); // read 0x0D register
	
		if(askonstatus||mtp_mode_on)
		ap_usb_power_on(0);
		else
		ap_usb_power_on(1);
		mdelay(10);
		fsa9480_write(client, REGISTER_CONTROL, 0x1E);
	}
}


void FSA9480_Enable_SPK(u8 enable)
{
	struct i2c_client *client = fsa9480_i2c_client;
	u8 data = 0;
	byte reg_value=0;
	byte reg_address=0x0D;

	if(enable)
	{
		DEBUG_FSA9480("FSA9480_Enable_SPK --- enable\n");
		msleep(10);
		Get_MAX8998_PM_ADDR(reg_address, &reg_value, 1); // read 0x0D register
		check_reg = reg_value;
		reg_value = ((0x2<<5)|reg_value);
		check_reg = reg_value;
		Set_MAX8998_PM_ADDR(reg_address,&reg_value,1);
		check_reg = reg_value;
			
		msleep(10);
#ifdef CONFIG_S5PC110_T959_BOARD
		//printk("HWREV is 0x%x\n", HWREV);		

		if(HWREV == 0x0A) //[hdlnc] T959  HWREV 0.0 
			fsa9480_write(client, REGISTER_MANUALSW1, 0x48);  // D+/- switching by Audio_L/R 
		else              //[hdlnc] T959 HWREV 0.1 over
			fsa9480_write(client, REGISTER_MANUALSW1, 0x90);  // D+/- switching by V_Audio_L/R 
#elif defined(CONFIG_S5PC110_HAWK_BOARD)||defined(CONFIG_S5PC110_SIDEKICK_BOARD)||defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) || defined(CONFIG_S5PC110_DEMPSEY_BOARD)
		fsa9480_write(client, REGISTER_MANUALSW1, 0x90);	// D+/- switching by V_Audio_L/R			
#elif defined(CONFIG_S5PC110_KEPLER_BOARD)
		fsa9480_write(client, REGISTER_MANUALSW1, 0x48);	// D+/- switching by Audio_L/R 
#else
		fsa9480_write(client, REGISTER_MANUALSW1, 0x48);	// D+/- switching by Audio_L/R 
#endif
		msleep(10);
		fsa9480_write(client, REGISTER_CONTROL, 0x1A);	//manual switching

	}
}


extern void askon_gadget_disconnect(void);
extern int s3c_usb_cable(int connected);

extern void vps_status_change(int status);
extern void car_vps_status_change(int status);
byte chip_error=0;
int uUSB_check_finished = 0;
EXPORT_SYMBOL(uUSB_check_finished);

#if defined(CONFIG_S5PC110_DEMPSEY_BOARD)
static void process_sii9234_initialize_work(struct work_struct *work)
{
	printk(KERN_ERR "process_sii9234_initialize_work started\n");
	if((SII9234_i2c_status == 1) && (max8893_i2cprobe_status == 1) )
	{
		/*s3c_gpio_setpin(GPIO_MHL_SEL, 1);
		DisableFSA9480Interrupts();
		sii9234_cfg_power(1);	//Turn On power to SiI9234
		SiI9234_init();*/
		FSA9480_MhlSwitchSel(1);//Rajucm: MHL PowerOn and MHL Switch sel 1
	}
	else
	{
		schedule_delayed_work(&sii9234_initialize_work, HZ);
	}	
}

static u8 FSA9480_AudioDockConnectionStatus=0x00; //flase
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
/* Rajucm: This function is implimented to clear unwanted/garbage interrupts and hook audio dock functionality. 
 *  Note:- Minimise Audio connection time delay as much as possible.
 *  Recomonded to minimize debug prints at audio_dock connection path
 */
void FSA9480_CheckAndHookAudioDock(void)//Rajucm
{
	FSA9480_AudioDockConnectionStatus++;
	FSA9480_MhlSwitchSel(0);//Rajucm
	FSA9480_AudioDockEnable();
}


static struct mutex FSA9480_MhlSwitchSel_lock;
void FSA9480_MhlSwitchSel(bool sw)//Rajucm
{
	struct i2c_client *client = fsa9480_i2c_client;
        DEBUG_FSA9480("[FSA] mutex_lock\n");
	mutex_lock(&FSA9480_MhlSwitchSel_lock);//Rajucm: avoid race condtion between mhl and hpd
	if(sw)
	{
		sii9234_cfg_power(1);	//Turn On power to SiI9234
		SiI9234_init();
	}
	else
	{
		#ifdef CONFIG_HDMI_HPD
		if(s5p_hpd_get_state()) ///Rajucm: here while is critical
		{
			DEBUG_FSA9480("\nSiI9234_HW_Reset is must because, there is problem in HPD state is 1###\n");
			//SiI9234_HW_Reset();
			mhl_hpd_handle(false);
			//msleep(5); //Rajucm: this sleep wiill allow hpd work to clear hpd state
		}	
		#endif
		sii9234_cfg_power(0);	//Turn Off power to SiI9234
	}

	/********************RAJUCM:FSA-MHL-FSA Switching start***************************/
	DisableFSA9480Interrupts();
	{
		s3c_gpio_setpin(GPIO_MHL_SEL, sw);
		//Clear unwanted interrupts during 1k  an 368 ohm  Switching
		i2c_smbus_read_byte_data(client, REGISTER_INTERRUPT1);
		i2c_smbus_read_byte_data(client, REGISTER_INTERRUPT1);
	}
	EnableFSA9480Interrupts();
	/******************RAJUCM:FSA-MHL-FSA Switching End*******************************/
	mutex_unlock(&FSA9480_MhlSwitchSel_lock);//Rajucm
        DEBUG_FSA9480("[FSA] mutex_unlock\n");
}
void FSA9480_MhlTvOff()
{
	struct i2c_client *client = fsa9480_i2c_client;
	u8 interrupt1;
	DisableFSA9480Interrupts();
//	printk(KERN_ERR "%s: started######\n", __func__);
	fsa9480_read(client, REGISTER_INTERRUPT1, &interrupt1);
	s3c_gpio_setpin(GPIO_MHL_SEL, 0);
	do
	{
		msleep(10);
		 fsa9480_read(client, REGISTER_INTERRUPT1, &interrupt1);
	}while(!interrupt1);

	mhl_cable_status =0x08;//MHL_TV_OFF_CABLE_CONNECT;
	EnableFSA9480Interrupts();
//	printk(KERN_ERR "%s: End######\n",__func__);
	 printk("%s:  interrupt1= %d\n", __func__, interrupt1);
}
EXPORT_SYMBOL(FSA9480_MhlTvOff);
EXPORT_SYMBOL(FSA9480_MhlSwitchSel);
EXPORT_SYMBOL(FSA9480_CheckAndHookAudioDock);
#endif
void FSA9480_ProcessDevice(u8 dev1, u8 dev2, u8 attach)
{
	DEBUG_FSA9480("FSA9480_ProcessDevice function!!!!\n");
	// [[ junghyunseok edit for attach/dettach debug 20010617
	printk("[FSA9480]FSA INTR = dev1 : 0x%x, dev2 : 0x%x , Attach : 0x%x\n",dev1, dev2, attach);

	if(!attach && !chip_error && (mtp_mode_on == 1))
		chip_error = 0xAE;

	if(dev1)
	{
		switch(dev1)
		{
			case FSA9480_DEV_TY1_AUD_TY1:
				DEBUG_FSA9480("Audio Type1 ");
				if(attach & FSA9480_INT1_ATTACH)
					DEBUG_FSA9480("FSA9480_DEV_TY1_AUD_TY1 --- ATTACH\n");
				else
					DEBUG_FSA9480("FSA9480_DEV_TY1_AUD_TY1 --- DETACH\n");
				break;

			case FSA9480_DEV_TY1_AUD_TY2:
				DEBUG_FSA9480("Audio Type2 ");
				break;

			case FSA9480_DEV_TY1_USB:
				DEBUG_FSA9480("USB attach or detach: %d\n",attach);
				if(attach & FSA9480_INT1_ATTACH)
				{
					DEBUG_FSA9480("FSA9480_DEV_TY1_USB --- ATTACH\n");
					MicroUSBStatus = 1;
					log_via_usb = log_usb_enable;
#if 0
					if(connectivity_switching_init_state)
						s3c_usb_cable(1);
#endif
					usb_switch_state();
					if(!askonstatus)
						UsbIndicator(1);
					else
						inaskonstatus = 0;				
					uUSB_check_finished = 1;  // finished
				}
				else if(attach & FSA9480_INT1_DETACH)
				{	
					MicroUSBStatus = 0;
					inaskonstatus = 0;
					chip_error = 0;
#if 0
					if(connectivity_switching_init_state)
						s3c_usb_cable(0);
#endif
					UsbIndicator(0);
					askon_gadget_disconnect();
					DEBUG_FSA9480("FSA9480_DEV_TY1_USB --- DETACH\n");
					uUSB_check_finished = 0;  // finished
				}
				break;

			case FSA9480_DEV_TY1_UART:
				DEBUG_FSA9480("UART\n");
				break;

			case FSA9480_DEV_TY1_CAR_KIT:
				DEBUG_FSA9480("Carkit\n");
				break;

			case FSA9480_DEV_TY1_USB_CHG:
				DEBUG_FSA9480("USB\n");
				break;

			case FSA9480_DEV_TY1_DED_CHG:
				{
					if(attach & FSA9480_INT1_ATTACH)
					{
						DEBUG_FSA9480("Dedicated Charger ATTACH\n");
						uUSB_check_finished = 1;  // finished
						//A9480_ChangePathToAudio(TRUE);
					}					
					else if(attach & FSA9480_INT1_DETACH)
					{				
						DEBUG_FSA9480("Dedicated Charger DETACH\n");
						uUSB_check_finished = 0;  // finished
						//A9480_ChangePathToAudio(FALSE);
					}
				}
				break;

			case FSA9480_DEV_TY1_USB_OTG:
				DEBUG_FSA9480("USB OTG\n");
				break;

			default:
				DEBUG_FSA9480("Unknown device\n");
				break;
		}

	}

	if(dev2)
	{
		switch(dev2)
		{
			case FSA9480_DEV_TY2_JIG_USB_ON:
				DEBUG_FSA9480("JIG USB ON attach or detach: %d",attach);
				if(attach & FSA9480_INT1_ATTACH)
				{
					DEBUG_FSA9480("FSA9480_DEV_TY2_JIG_USB_ON --- ATTACH\n");
					MicroJigUSBOnStatus = 1;
#if 0
					if(connectivity_switching_init_state)
						s3c_usb_cable(1);
#endif
					usb_switch_state();
					if(!askonstatus)
						UsbIndicator(1);
					else
						inaskonstatus = 0;				
				}
				else if(attach & FSA9480_INT1_DETACH)
				{
					DEBUG_FSA9480("FSA9480_DEV_TY2_JIG_USB_ON --- DETACH\n");
					chip_error = 0;
					MicroJigUSBOnStatus = 0;
					inaskonstatus = 0;
#if 0
					if(connectivity_switching_init_state)
						s3c_usb_cable(0);							
#endif
					UsbIndicator(0);
					askon_gadget_disconnect();					
				}
				break;

			case FSA9480_DEV_TY2_JIG_USB_OFF:
				if(attach & FSA9480_INT1_ATTACH)
				{
					DEBUG_FSA9480("FSA9480_DEV_TY2_JIG_USB_OFF --- ATTACH\n");
					MicroJigUSBOffStatus = 1;
#if 0
					if(connectivity_switching_init_state)
						s3c_usb_cable(1);
#endif
					usb_switch_state();
					if(!askonstatus)
						UsbIndicator(1);
					else
						inaskonstatus = 0;									
				}
				else if(attach & FSA9480_INT1_DETACH)
				{
					DEBUG_FSA9480("FSA9480_DEV_TY2_JIG_USB_OFF --- DETACH\n");
					chip_error = 0;
					MicroJigUSBOffStatus = 0;
					inaskonstatus = 0;
#if 0
					if(connectivity_switching_init_state)
						s3c_usb_cable(0);
#endif
					UsbIndicator(0);
					askon_gadget_disconnect();					
				}
				DEBUG_FSA9480("JIG USB OFF \n");
				break;

			case FSA9480_DEV_TY2_JIG_UART_ON:
				if(attach & FSA9480_INT1_ATTACH)
				{
					DEBUG_FSA9480("FSA9480_DEV_TY2_JIG_UART_ON --- ATTACH\n");
					MicroJigUARTOnStatus = 1;
					set_dock_state((int)CAR_DOCK_INSERTED);
					FSA9480_Enable_SPK(1);
					g_dock = CAR_DOCK_INSERTED;
				}
				else
				{
					DEBUG_FSA9480("FSA9480_DEV_TY2_JIG_UART_ON --- DETACH\n");
					MicroJigUARTOnStatus = 0;
 			       set_dock_state((int)DOCK_REMOVED);
					g_dock = DOCK_REMOVED;

				}
				DEBUG_FSA9480("JIG UART ON\n");
				break;

			case FSA9480_DEV_TY2_JIG_UART_OFF:
				if(attach & FSA9480_INT1_ATTACH)
				{
					DEBUG_FSA9480("FSA9480_DEV_TY2_JIG_UART_OFF --- ATTACH\n");
					MicroJigUARTOffStatus = 1;
					uart_insert_switch_state();
				}
				else
				{
					DEBUG_FSA9480("FSA9480_DEV_TY2_JIG_UART_OFF --- DETACH\n");
					MicroJigUARTOffStatus = 0;
					uart_remove_switch_state();
				}
				DEBUG_FSA9480("JIT UART OFF\n");
				break;

			case FSA9480_DEV_TY2_PDD:
				DEBUG_FSA9480("PPD \n");
				break;

			case FSA9480_DEV_TY2_TTY:
				DEBUG_FSA9480("TTY\n");
				break;

			case FSA9480_DEV_TY2_AV:
#if defined(CONFIG_S5PC110_DEMPSEY_BOARD)	
				//DEBUG_FSA9480("AudioDock OR MHL\n");
				//MHL v1 //NAGSM_Android_SEL_Kernel_Aakash_20101119 FSA MHL Jack 
				
				printk(KERN_ERR "\nFSA MHL FSA9480_DEV_TY2_AV: %x attach: %x fsa9480_interrupt2: %x FSA9480_AudioDockConnectionStatus %d  mhl_cable_status = %d\n",
									FSA9480_DEV_TY2_AV,attach, fsa9480_interrupt2, FSA9480_AudioDockConnectionStatus, mhl_cable_status);

				if(fsa9480_interrupt2 & 0x01)
				{
#ifdef CONFIG_S5PC110_DEMPSEY_BOARD
					mhl_vbus = TRUE;
#endif
					attach = 0x01;
					fsa9480_interrupt2 = 0;
				}
				else
				{
					mhl_vbus = FALSE;
				}

				//mhl_cable_status = ((!attach) && (mhl_cable_status ==0x08 /*MHL_TV_OFF_CABLE_CONNECT*/)) ?  attach++:0;
				//DEBUG_FSA9480("mhl_cable_status = %d attach =%d\n", mhl_cable_status, attach);
				if(!attach) break; //Rajucm: unknown interrupt causing disconnection				
				if (attach & FSA9480_INT1_ATTACH) 
				{   
					DEBUG_FSA9480("\nFSA MHL ATTACH\n");
					if (FSA9480_AudioDockConnectionStatus) //Rajucm: AudioDock Attch
					{
						DEBUG_FSA9480("%s Already connected####\n",__func__);
					}
					else//Rajucm: MHL Cable Attach
					{
							FSA9480_MhlSwitchSel(1);
					}
				}
				else //Rajucm: Detach mhl cble
				{
					DEBUG_FSA9480("\nFSA MHL DETACH\n");
					if (FSA9480_AudioDockConnectionStatus) //Rajucm: AudioDock Detach
					{
						FSA9480_AudioDockDisable();
					}
					else//Rajucm: MHL Cable Detach
					{
						FSA9480_MhlSwitchSel(0);
						mhl_cable_status=0;
						
					}
				}
				//MHL v1 //NAGSM_Android_SEL_Kernel_Aakash_20101119 FSA MHL Jack
				break;
#else//[hdlnc_2010_11_24]
				if (attach & FSA9480_INT1_DETACH){
					DEBUG_FSA9480("FSA9480_disable_spk\n");					
					set_dock_state((int)DOCK_REMOVED);
					g_dock = DOCK_REMOVED;
				}
				else{
					DEBUG_FSA9480("FSA9480_enable_spk\n");
					set_dock_state((int)HOME_DOCK_INSERTED);
					FSA9480_Enable_SPK(1);
					g_dock = HOME_DOCK_INSERTED;
				}
				break;
#endif//[hdlnc_2010_11_24]
			default:
				DEBUG_FSA9480("Unknown device\n");
				break;
		}
	}

	if((attach == FSA9480_INT1_ATTACH) && (chip_error == 0xAE) && (mtp_mode_on == 1)){
		ap_usb_power_on(1);
		chip_error = 0;
		}

}

EXPORT_SYMBOL(FSA9480_ProcessDevice);


void FSA9480_ReadIntRegister(struct work_struct *work)
{
	struct i2c_client *client = fsa9480_i2c_client;
	u8 interrupt1 ,interrupt2 ,device1, device2, temp;
	fsa9480_interrupt2 =0;

	DEBUG_FSA9480("[FSA9480] %s\n", __func__);

	 fsa9480_read(client, REGISTER_INTERRUPT1, &interrupt1);
 	msleep(5);

	 fsa9480_read(client, REGISTER_INTERRUPT2, &interrupt2);
 	msleep(5);

	 fsa9480_read(client, REGISTER_DEVICETYPE1, &device1);
 	msleep(5);

	 fsa9480_read(client, REGISTER_DEVICETYPE2, &device2);

	usb_state = (device2 << 8) | (device1 << 0);

#if defined(CONFIG_S5PC110_DEMPSEY_BOARD)
//Rajucm: phone Power on with MHL Cable,avoid crash
	while((!MHL_INIT_COND) && (device2&FSA9480_DEV_TY2_AV))
	{	//4Control enters into while loop only when fsa9480 detects MHL-cable @ phone bootup
		i2c_smbus_write_byte_data(client, 0x02, (0x01|i2c_smbus_read_byte_data(client, 0x02)));	//DisableFSA9480Interrupts
		wait_event_interruptible_timeout(fsa9480_MhlWaitEvent, MHL_INIT_COND, msecs_to_jiffies(10*1000)); //10sec:
		printk(KERN_ERR"[FSA9480]####### %s ######### \n", MHL_INIT_COND? "Ready to start MHL":"Sleep untill MHL condition comes true");
	}
#endif
	if(interrupt1 & FSA9480_INT1_ATTACH)
	{
		fsa9480_device1 = device1;
		fsa9480_device2 = device2;
		fsa9480_interrupt1 = interrupt1;

		if(fsa9480_device1 != FSA9480_DEV_TY1_DED_CHG){
			//DEBUG_FSA9480("FSA9480_enable LDO8\n");
			s3c_usb_cable(1);
		}

		if(fsa9480_device1&FSA9480_DEV_TY1_CAR_KIT)
		{
			msleep(5);
			fsa9480_write(client, REGISTER_CARKITSTATUS, 0x02);

			msleep(5);
			fsa9480_read(client, REGISTER_CARKITINT1, &temp);
		}
	}

 	msleep(5);

	 fsa9480_write(client, REGISTER_CONTROL, 0x1E);
	 fsa9480_write(client, REGISTER_INTERRUPTMASK1, 0xFC);
#if defined(CONFIG_ARIES_NTT) // Modify NTTS1
	 //syyoon 20100724	 fix for SC - Ad_10_2nd - 0006. When USB is removed, sometimes attatch value gets 0x00
	 if((fsa9480_device1 == FSA9480_DEV_TY1_USB) && (!interrupt1)){
		 printk("[FSA9480] dev1=usb, attach change is from 0 to 2\n");
		 interrupt1 = FSA9480_INT1_DETACH;
	 }
#endif
	fsa9480_interrupt2 = interrupt2;
	 FSA9480_ProcessDevice(fsa9480_device1, fsa9480_device2, interrupt1);

	if(interrupt1 & FSA9480_INT1_DETACH)
	{
		if(fsa9480_device1 != FSA9480_DEV_TY1_DED_CHG){
			//DEBUG_FSA9480("FSA9480_disable LDO8\n");
			s3c_usb_cable(0);
		}

		fsa9480_device1 = 0;
		fsa9480_device2 = 0;
	}
	
	enable_irq(IRQ_FSA9480_INTB);
}
EXPORT_SYMBOL(FSA9480_ReadIntRegister);



irqreturn_t fsa9480_interrupt(int irq, void *ptr)
{
	printk("%s\n", __func__);
	disable_irq_nosync(IRQ_FSA9480_INTB);
	
	uUSB_check_finished =0;  // reset

	queue_work(fsa9480_workqueue, &fsa9480_work);

	return IRQ_HANDLED; 
}
EXPORT_SYMBOL(fsa9480_interrupt);


void fsa9480_interrupt_init(void)
{		
	s3c_gpio_cfgpin(GPIO_JACK_nINT, S3C_GPIO_SFN(GPIO_JACK_nINT_AF));
	s3c_gpio_setpull(GPIO_JACK_nINT, S3C_GPIO_PULL_NONE);
	set_irq_type(IRQ_FSA9480_INTB, IRQ_TYPE_EDGE_FALLING);

	 if (request_irq(IRQ_FSA9480_INTB, fsa9480_interrupt, IRQF_DISABLED, "FSA9480 Detected", NULL)) 
		 DEBUG_FSA9480("[FSA9480]fail to register IRQ[%d] for FSA9480 USB Switch \n", IRQ_FSA9480_INTB);
}
EXPORT_SYMBOL(fsa9480_interrupt_init);


void fsa9480_chip_init(void)
{
	struct i2c_client *client = fsa9480_i2c_client;

	fsa9480_write(client, HIDDEN_REGISTER_MANUAL_OVERRDES1, 0x01); //RESET

	 mdelay(10);
	 fsa9480_write(client, REGISTER_CONTROL, 0x1E);

	 mdelay(10);

	 fsa9480_write(client, REGISTER_INTERRUPTMASK1, 0xFC);

	 mdelay(10);

	 fsa9480_read(client, REGISTER_DEVICETYPE1, &fsa9480_device1);

	 mdelay(10);

	 fsa9480_read(client, REGISTER_DEVICETYPE2, &fsa9480_device2);
}


// TODO : need it?
#if 0
static int fsa9480_modify(struct i2c_client *client, u8 reg, u8 data, u8 mask)
{
   u8 original_value, modified_value;

   fsa9480_read(client, reg, &original_value);

   mdelay(10);
   
   modified_value = ((original_value&~mask) | data);
   
   fsa9480_write(client, reg, modified_value);

   mdelay(10);

   return 0;
}


void fsa9480_init_status(void)
{
	u8 pData;
	
	fsa9480_modify(&fsa9480_i2c_client,REGISTER_CONTROL,~INT_MASK, INT_MASK);
	
	fsa9480_read(&fsa9480_i2c_client, 0x13, &pData);
}
#endif

u8 FSA9480_Get_I2C_USB_Status(void)
{
	u8 device1, device2;
	
	fsa9480_read(fsa9480_i2c_client, REGISTER_DEVICETYPE1, &device1);
  	msleep(5);
	fsa9480_read(fsa9480_i2c_client, REGISTER_DEVICETYPE2, &device2);

	//printk("[FSA9480] device1 = %x, device2=%x \n", device1, device2);

	if((device1 == FSA9480_DEV_TY1_USB) ||(device2== FSA9480_DEV_TY2_JIG_USB_ON) ||(device2 == FSA9480_DEV_TY2_JIG_USB_OFF) )
		{
		   return 1;
		}
	else
		return 0;
}
EXPORT_SYMBOL(FSA9480_Get_I2C_USB_Status);

int connectivity_indicator_init_state=0;
void connectivity_switching_init(struct work_struct *ignored)
{
	int usb_sel,uart_sel,samsung_kies_sel,ums_sel,mtp_sel,vtp_sel,askon_sel;
	int lpm_mode_check = charging_mode_get();
	byte  iTemp;

	if (sec_get_param_value){
		sec_get_param_value(__SWITCH_SEL, &switch_sel);
		cancel_delayed_work(&micorusb_init_work);
	}
	else{
		schedule_delayed_work(&micorusb_init_work, msecs_to_jiffies(200));		
		return;
	}

	if(BOOTUP){
		BOOTUP = 0; 
		otg_phy_init(); //USB Power on after boot up.
	}

	printk("[FSA9480]connectivity_switching_init = switch_sel : 0x%x\n",switch_sel);

	usb_sel = switch_sel & (int)(USB_SEL_MASK);
	uart_sel = (switch_sel & (int)(UART_SEL_MASK)) >> 1;
	samsung_kies_sel = (switch_sel & (int)(USB_SAMSUNG_KIES_MASK)) >> 2;
	ums_sel = (switch_sel & (int)(USB_UMS_MASK)) >> 3;
	mtp_sel = (switch_sel & (int)(USB_MTP_MASK)) >> 4;
#if !defined(CONFIG_ARIES_NTT) // disable tethering xmoondash
	vtp_sel = (switch_sel & (int)(USB_VTP_MASK)) >> 5;
#endif
	askon_sel = (switch_sel & (int)(USB_ASKON_MASK)) >> 6;

	if((switch_sel == 0x1) || (factoryresetstatus == 0xAE)){
#if 0 //defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD) // TEMP code for CP
		PathSelStore(CP_USB_MODE);
		Ap_Cp_Switch_Config(CP_USB_MODE);	
		usb_switching_value_update(SWITCH_MODEM);
#else
		PathSelStore(AP_USB_MODE);
		Ap_Cp_Switch_Config(AP_USB_MODE);	
		usb_switching_value_update(SWITCH_PDA);
#endif
		PathSelStore(CP_UART_MODE);
		Ap_Cp_Switch_Config(CP_UART_MODE);	
		uart_switching_value_update(SWITCH_MODEM);
	}
	else{
	    if(usb_sel) {
		    Ap_Cp_Switch_Config(AP_USB_MODE);
		    usb_switching_value_update(SWITCH_PDA);
	    }
	    else {
			if(MicroJigUARTOffStatus || MicroJigUARTOnStatus){
			    Ap_Cp_Switch_Config(AP_USB_MODE);
			}
		    else {
			    Ap_Cp_Switch_Config(CP_USB_MODE);
			    usb_switching_value_update(SWITCH_MODEM);
		    }
	    }

	    if(uart_sel) {
		    Ap_Cp_Switch_Config(AP_UART_MODE);
		    uart_switching_value_update(SWITCH_PDA);
	    }
	    else {
		    Ap_Cp_Switch_Config(CP_UART_MODE);
		    uart_switching_value_update(SWITCH_MODEM);
	    }
	}

	/*Turn off usb power when LPM mode*/
	if(lpm_mode_check)
		otg_phy_off();
			
		switching_value_update();	

	if((switch_sel == 1) || (factoryresetstatus == 0xAE)){
#if 1	
		if(lpm_mode_check){	// boot on charge mode
			  Get_MAX8998_PM_ADDR(CHGR2, &iTemp, 1);
              			iTemp = iTemp & 0x3F;  // disable safeout1, safeout2
			ap_usb_power_on(0);
			Set_MAX8998_PM_ADDR(CHGR2, &iTemp, 1); 
		}
		else
#endif
		usb_switch_select(USBSTATUS_ASKON);
		UsbMenuSelStore(4);	
	}
	else{
		if(usb_sel){
			if(samsung_kies_sel){
				printk("[FSA9480]connectivity_switching_init[KIES] \n");
				usb_switch_select(USBSTATUS_SAMSUNG_KIES);
				/*USB Power off till MTP Appl launching*/				
				mtp_mode_on = 0;
//				ap_usb_power_on(0);

			}
			else if(mtp_sel){
				printk("[FSA9480]connectivity_switching_init[MTP] \n");
				usb_switch_select(USBSTATUS_MTPONLY);
				/*USB Power off till MTP Appl launching*/				
				mtp_mode_on = 1;
				ap_usb_power_on(0);
			}
			else if(askon_sel){
				printk("[FSA9480]connectivity_switching_init[ASKON] \n");
				usb_switch_select(USBSTATUS_ASKON);
				ap_usb_power_on(0);
			}		
			else if(ums_sel){
				printk("[FSA9480]connectivity_switching_init[UMS] \n");
				usb_switch_select(USBSTATUS_UMS);
			}
#if !defined(CONFIG_ARIES_NTT) // disable tethering xmoondash
			else if(vtp_sel){
				usb_switch_select(USBSTATUS_VTP);
			}
#endif
	
		}
	}

	if(!FSA9480_Get_USB_Status()) {
		s3c_usb_cable(1);
		mdelay(5);
		s3c_usb_cable(0);
	}
	else{
		printk("askon_sel : %d\n", askon_sel);

		if(askon_sel){
			indicator_dev.state = 0;
		    connectivity_indicator_init_state=1;
		}else{
			indicator_dev.state = 1;
		    connectivity_indicator_init_state=0;
		}
	}

	printk("[FSA9480]connectivity_switching_init = switch_sel : 0x%x\n",switch_sel);
	microusb_uart_status(1);

	connectivity_switching_init_state=1;
}


static ssize_t print_switch_name(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%s\n", DRIVER_NAME);
}

static ssize_t print_switch_state(struct switch_dev *sdev, char *buf)
{
	int usbstatus;

	if( mtp_power_off == 1 )
	{
		printk("USB power off for MTP\n");
		mtp_power_off = 0;
		return sprintf(buf, "%s\n", "RemoveOffline");
	}

	usbstatus = FSA9480_Get_USB_Status();

//TODO : each platform require different noti
#if 0 //froyo default
    if(usbstatus){
        return sprintf(buf, "%s\n", "online");
    }
    else{
        return sprintf(buf, "%s\n", "offline");
    }
#elif 1 //P1
    if(usbstatus){
		if(currentusbstatus == USBSTATUS_VTP)
			return sprintf(buf, "%s\n", "RemoveOffline");
        else if((currentusbstatus== USBSTATUS_UMS) || (currentusbstatus== USBSTATUS_ADB))
            return sprintf(buf, "%s\n", "ums online");
        else
            return sprintf(buf, "%s\n", "InsertOffline");
    }
    else{
        if((currentusbstatus== USBSTATUS_UMS) || (currentusbstatus== USBSTATUS_ADB))
            return sprintf(buf, "%s\n", "ums offline");
        else
            return sprintf(buf, "%s\n", "RemoveOffline");
    }
#else //S1
	if(usbstatus){
		if((currentusbstatus== USBSTATUS_UMS) || (currentusbstatus== USBSTATUS_ADB))
			return sprintf(buf, "%s\n", "InsertOnline");
		else
			return sprintf(buf, "%s\n", "InsertOffline");
	}
	else{
		if((currentusbstatus== USBSTATUS_UMS) || (currentusbstatus== USBSTATUS_ADB))
			return sprintf(buf, "%s\n", "RemoveOnline");
		else
			return sprintf(buf, "%s\n", "RemoveOffline");
	}
#endif
}


static int fsa9480_codec_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct fsa9480_state *state;
	struct device *dev = &client->dev;
	u8 pData;

	DEBUG_FSA9480("[FSA9480] %s\n", __func__);
#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)	
	mutex_init(&FSA9480_MhlSwitchSel_lock);//Rajucm
#endif	
	s3c_gpio_cfgpin(GPIO_USB_SCL_28V, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_USB_SCL_28V, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_USB_SDA_28V, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_USB_SDA_28V, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_UART_SEL, S3C_GPIO_OUTPUT );
	s3c_gpio_setpull(GPIO_UART_SEL, S3C_GPIO_PULL_NONE);

	if (device_create_file(switch_dev, &dev_attr_uart_sel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_uart_sel.attr.name);
	 
	if (device_create_file(switch_dev, &dev_attr_usb_sel) < 0)
		DEBUG_FSA9480("[FSA9480]Failed to create device file(%s)!\n", dev_attr_usb_sel.attr.name);

	if (device_create_file(switch_dev, &dev_attr_usb_state) < 0)
		DEBUG_FSA9480("[FSA9480]Failed to create device file(%s)!\n", dev_attr_usb_state.attr.name);
	
#if 1
	if (device_create_file(switch_dev, &dev_attr_DMport) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_DMport.attr.name);

	if (device_create_file(switch_dev, &dev_attr_DMlog) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_DMlog.attr.name);
#endif

	if (device_create_file(switch_dev, &dev_attr_UsbMenuSel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_UsbMenuSel.attr.name);
	 
	if (device_create_file(switch_dev, &dev_attr_AskOnMenuSel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_AskOnMenuSel.attr.name);

	if (device_create_file(switch_dev, &dev_attr_Mtp) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_Mtp.attr.name);

	if (device_create_file(switch_dev, &dev_attr_SwitchingInitValue) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_SwitchingInitValue.attr.name);		

	if (device_create_file(switch_dev, &dev_attr_FactoryResetValue) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_FactoryResetValue.attr.name);		
	
	if (device_create_file(switch_dev, &dev_attr_AskOnStatus) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_AskOnStatus.attr.name);			
	
	if (device_create_file(switch_dev, &dev_attr_MtpInitStatusSel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_MtpInitStatusSel.attr.name);			
	
	if (device_create_file(switch_dev, &dev_attr_AskInitStatusSel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_AskInitStatusSel.attr.name);	
	
	if (device_create_file(switch_dev, &dev_attr_tethering) < 0)		
		pr_err("Failed to create device file(%s)!\n", dev_attr_tethering.attr.name);

	if (device_create_file(switch_dev, &dev_attr_dock) < 0)		
		pr_err("Failed to create device file(%s)!\n", dev_attr_dock.attr.name);

	 
	 init_waitqueue_head(&usb_detect_waitq); 
	 INIT_WORK(&fsa9480_work, FSA9480_ReadIntRegister);
	 fsa9480_workqueue = create_singlethread_workqueue("fsa9480_workqueue");


#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)
	INIT_DELAYED_WORK(&sii9234_initialize_work, process_sii9234_initialize_work);
#endif
	
	 state = kzalloc(sizeof(struct fsa9480_state), GFP_KERNEL);
	 if(!state) {
		 dev_err(dev, "%s: failed to create fsa9480_state\n", __func__);
		 return -ENOMEM;
	 }

	indicator_dev.name = DRIVER_NAME;
	indicator_dev.print_name = print_switch_name;
	indicator_dev.print_state = print_switch_state;
	switch_dev_register(&indicator_dev);

	state->client = client;
	fsa9480_i2c_client = client;

	i2c_set_clientdata(client, state);
	if(!fsa9480_i2c_client)
	{
		dev_err(dev, "%s: failed to create fsa9480_i2c_client\n", __func__);
		return -ENODEV;
	}

	 /*clear interrupt mask register*/
	fsa9480_read(fsa9480_i2c_client, REGISTER_CONTROL, &pData);
	fsa9480_write(fsa9480_i2c_client, REGISTER_CONTROL, pData & ~INT_MASK);

#if defined (CONFIG_S5PC110_DEMPSEY_BOARD) //Rajucm: mask Interrutts untill mhl mhl precondtions 
	fsa9480_write(fsa9480_i2c_client, REGISTER_CONTROL, pData|INT_MASK);
#endif
	fsa9480_interrupt_init();

	fsa9480_chip_init();
	
	INIT_DELAYED_WORK(&micorusb_init_work, connectivity_switching_init);
	schedule_delayed_work(&micorusb_init_work, msecs_to_jiffies(200));

	 return 0;
}


static int __devexit fsa9480_remove(struct i2c_client *client)
{
	struct fsa9480_state *state = i2c_get_clientdata(client);
#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)	
	mutex_destroy(&FSA9480_MhlSwitchSel_lock);//Rajucm
#endif
	kfree(state);
	return 0;
}


struct i2c_driver fsa9480_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "fsa9480",
	},
	.id_table	= fsa9480_id,
	.probe	= fsa9480_codec_probe,
	.remove	= __devexit_p(fsa9480_remove),
	.command = NULL,
};

