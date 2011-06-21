#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>


#include "bma222_acc.h"
#include "bma222.h"


// this proc file system's path is "/proc/driver/bma222"
// usage :	(at the path) type "cat bma222" , it will show short information for current accelation
// 			use it for simple working test only

//#define bma222_PROC_FS

#define DEBUG 1

#ifdef BMA222_PROC_FS

#include <linux/proc_fs.h>

#define DRIVER_PROC_ENTRY		"driver/bma222"

static int bma222_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	char *p = page;
	int len;
	bma222acc_t acc;
	bma222_set_mode( BMA222_MODE_NORMAL );
	bma222_read_accel_xyz(&acc);
	p += sprintf(p,"[BMA222]\nX axis: %d\nY axis: %d\nZ axis: %d\n" , acc.x, acc.y, acc.z);
	len = (p - page) - off;
	if (len < 0) {
		len = 0;
	}

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;
	return len;
}
#endif	//BMA222_PROC_FS

/* add by inter.park */
//extern void enable_acc_pins(void);

struct class *acc_class;

/* no use */
//static int bma222_irq_num = NO_IRQ;

/* create bma222 object */
bma222_t bma222;

/* create bma222 registers object */
bma222regs_t bma222regs;

#if 0
static irqreturn_t bma222_acc_isr( int irq, void *unused, struct pt_regs *regs )
{
	printk( "bma222_acc_isr event occur!!!\n" );
	
	return IRQ_HANDLED;
}
#endif


int bma222_open (struct inode *inode, struct file *filp)
{
	printk("start\n");
	return 0;
}

ssize_t bma222_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

ssize_t bma222_write (struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

int bma222_release (struct inode *inode, struct file *filp)
{
	return 0;
}

int bma222_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,  unsigned long arg)
{
	int err = 0;
	unsigned char data[6];

	/* check cmd */
	if(_IOC_TYPE(cmd) != BMA222_IOC_MAGIC)
	{
#if DEBUG       
		printk("cmd magic type error\n");
#endif
		return -ENOTTY;
	}
	if(_IOC_NR(cmd) > BMA222_IOC_MAXNR)
	{
#if DEBUG
		printk("cmd number error\n");
#endif
		return -ENOTTY;
	}

	if(_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,(void __user*)arg, _IOC_SIZE(cmd));
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
	if(err)
	{
#if DEBUG
		printk("cmd access_ok error\n");
#endif
		return -EFAULT;
	}
	#if 0
	/* check bam222_client */
	if( bma222_client == NULL)
	{
#if DEBUG
		printk("I2C driver not install\n");
#endif
		return -EFAULT;
	}
	#endif

	switch(cmd)
	{
		case BMA222_READ_ACCEL_XYZ:
			err = bma222_read_accel_xyz((bma222acc_t*)data);
			if(copy_to_user((bma222acc_t*)arg,(bma222acc_t*)data,6)!=0)
			{
#if DEBUG
				printk("copy_to error\n");
#endif
				return -EFAULT;
			}
			return err;

		case BMA222_SET_RANGE:
			if(copy_from_user(data,(unsigned char*)arg,1)!=0)
			{
#if DEBUG           
				printk("[BMA222] copy_from_user error\n");
#endif
				return -EFAULT;
			}
			err = bma222_set_range(*data);
			return err;
		
		case BMA222_SET_MODE:
			if(copy_from_user(data,(unsigned char*)arg,1)!=0)
			{
#if DEBUG           
				printk("[BMA222] copy_from_user error\n");
#endif
				return -EFAULT;
			}
			err = bma222_set_mode(*data);
			return err;

		case BMA222_SET_BANDWIDTH:
			if(copy_from_user(data,(unsigned char*)arg,1)!=0)
			{
#if DEBUG
				printk("[BMA222] copy_from_user error\n");
#endif
				return -EFAULT;
			}
			err = bma222_set_bandwidth(*data);
			return err;
		
		default:
			return 0;
	}
}

struct file_operations acc_fops =
{
	.owner   = THIS_MODULE,
	.read    = bma222_read,
	.write   = bma222_write,
	.open    = bma222_open,
	.ioctl   = bma222_ioctl,
	.release = bma222_release,
};


void bma222_chip_init(void)
{
	printk("%s \n",__func__); 
	/*assign register memory to bma222 object */
	bma222.image = &bma222regs;

	bma222.bma222_bus_write = i2c_acc_bma222_write;
	bma222.bma222_bus_read  = i2c_acc_bma222_read;
	/*call init function to set read write functions, read registers */
	bma222_init( &bma222 );

	   printk("%s chip_id : %x\n",__func__,bma222.chip_id ); 
	/* from this point everything is prepared for sensor communication */


	/* set range to 2G mode, other constants: 
	 * 	   			4G: BMA222_RANGE_4G, 
	 * 	    		8G: BMA222_RANGE_8G */

	bma222_set_range(BMA222_RANGE_2G); 

	/* set bandwidth to 25 HZ */
	bma222_set_bandwidth(BMA222_BW_7_81HZ);

	/* for interrupt setting */
//	bma222_set_low_g_threshold( BMA222_HG_THRES_IN_G(0.35, 2) );

//	bma222_set_interrupt_mask( BMA222_INT_LG );

}

static ssize_t bma222_fs_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;
	bma222acc_t acc;
	bma222_read_accel_xyz(&acc);

       printk("x: %d,y: %d,z: %d\n", acc.x, acc.y, acc.z);
	count = sprintf(buf,"%d,%d,%d\n", acc.x, acc.y, acc.z );
	

	return count;
}

static ssize_t bma222_fs_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	//buf[size]=0;
	printk("input data --> %s\n", buf);

	return size;
}
static DEVICE_ATTR(acc_file, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, bma222_fs_read, bma222_fs_write);

int bma222_acc_start(void)
{
	int result;

	struct device *dev_t;
	
	bma222acc_t accels; /* only for test */
	printk("%s \n",__func__); 
	
	result = register_chrdev( BMA222_MAJOR, ACC_DEV_NAME, &acc_fops);

	if (result < 0) 
	{
		return result;
	}
	
	acc_class = class_create (THIS_MODULE, "accelerometer");
	
	if (IS_ERR(acc_class)) 
	{
		unregister_chrdev( BMA222_MAJOR, ACC_DEV_NAME);
		return PTR_ERR( acc_class );
	}

	dev_t = device_create( acc_class, NULL, MKDEV(BMA222_MAJOR, 0), "%s", "accelerometer");

//TEST
	if (device_create_file(dev_t, &dev_attr_acc_file) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_acc_file.attr.name);

	if (IS_ERR(dev_t)) 
	{
		return PTR_ERR(dev_t);
	}
	
	result = i2c_acc_bma222_init();

	if(result)
	{
		return result;
	}

	bma222_chip_init();

	printk("[BMA222] read_xyz ==========================\n");
	bma222_read_accel_xyz( &accels );
	printk("[BMA222] x = %d  /  y =  %d  /  z = %d\n", accels.x, accels.y, accels.z );

	printk("[BMA222] ===================================\n");
	
	/* only for test */
	#if 0
	printk( "before get xyz\n" );
	mdelay(3000);

	while(1)
	{
		bma222_read_accel_xyz( &accels );

		printk( "acc data x = %d  /  y =  %d  /  z = %d\n", accels.x, accels.y, accels.z );
	
		mdelay(100);
	}
	#endif

#ifdef BMA222_PROC_FS
	create_proc_read_entry(DRIVER_PROC_ENTRY, 0, 0, bma222_proc_read, NULL);
#endif	//BMA222_PROC_FS

	//bma222_set_mode(BMA222_MODE_SUSPEND);
	//printk("[BMA222] set_mode BMA222_MODE_SUSPEND\n");
	
	return 0;
}

void bma222_acc_end(void)
{
	unregister_chrdev( BMA222_MAJOR, "accelerometer" );
	
	i2c_acc_bma222_exit();

	device_destroy( acc_class, MKDEV(BMA222_MAJOR, 0) );
	class_destroy( acc_class );
	//unregister_early_suspend(&bma222.early_suspend);
}


static int bma222_accelerometer_probe( struct platform_device* pdev )
{
/* not use interrupt */
#if 0	
	int ret;

	//enable_acc_pins();
	/*
	mhn_gpio_set_direction(MFP_ACC_INT, GPIO_DIR_IN);
	mhn_mfp_set_pull(MFP_ACC_INT, MFP_PULL_HIGH);
	*/

	bma222_irq_num = platform_get_irq(pdev, 0);
	ret = request_irq(bma222_irq_num, (void *)bma222_acc_isr, IRQF_DISABLED, pdev->name, NULL);
	if(ret) {
		printk("[BMA222 ACC] isr register error\n");
		return ret;
	}

	//set_irq_type (bma222_irq_num, IRQT_BOTHEDGE);
	
	/* if( request_irq( IRQ_GPIO( MFP2GPIO(MFP_ACC_INT) ), (void *) bma222_acc_isr, 0, "BMA222_ACC_ISR", (void *)0 ) )
	if(
	{
		printk ("[BMA222 ACC] isr register error\n" );
	}
	else
	{
		printk( "[BMA222 ACC] isr register success!!!\n" );
	}*/
	
	// set_irq_type ( IRQ_GPIO( MFP2GPIO(MFP_ACC_INT) ), IRQT_BOTHEDGE );

	/* if interrupt don't register Process don't stop for polling mode */ 

#endif
	printk("%s \n",__func__); 
	return bma222_acc_start();
}


static int bma222_accelerometer_suspend( struct platform_device* pdev, pm_message_t state )
{
	bma222_set_mode(BMA222_MODE_SUSPEND);
	return 0;
}


static int bma222_accelerometer_resume( struct platform_device* pdev )
{
	bma222_set_mode(BMA222_MODE_NORMAL);

	return 0;
}


static struct platform_device *bma222_accelerometer_device;

static struct platform_driver bma222_accelerometer_driver = {
	.probe 	 = bma222_accelerometer_probe,
	.suspend = bma222_accelerometer_suspend,
	.resume  = bma222_accelerometer_resume,
	.driver  = {
		.name = "bma222-accelerometer", 
	}
};


static int __init bma222_acc_init(void)
{
	int result;
	printk("%s \n",__func__); 

	result = platform_driver_register( &bma222_accelerometer_driver );
	//bma222_acc_start();

	printk("%s result : %d \n",__func__, result); 
	if( result ) 
	{
		return result;
	}

	bma222_accelerometer_device  = platform_device_register_simple( "bma222-accelerometer", -1, NULL, 0 );
	
	if( IS_ERR( bma222_accelerometer_device ) )
	{
		return PTR_ERR( bma222_accelerometer_device );
	}

	return 0;
}


static void __exit bma222_acc_exit(void)
{
	printk("start\n");
	bma222_acc_end();

//	free_irq(bma222_irq_num, NULL);
//	free_irq( IRQ_GPIO( MFP2GPIO( MFP_ACC_INT ) ), (void*)0 );

	platform_device_unregister( bma222_accelerometer_device );
	platform_driver_unregister( &bma222_accelerometer_driver );
}


module_init( bma222_acc_init );
module_exit( bma222_acc_exit );

MODULE_AUTHOR("inter.park");
MODULE_DESCRIPTION("accelerometer driver for BMA222");
MODULE_LICENSE("GPL");
