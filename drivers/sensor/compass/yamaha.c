 
 /*****************************************************************************
 *
 * COPYRIGHT(C) : Samsung Electronics Co.Ltd, 2006-2015 ALL RIGHTS RESERVED
 *
 *****************************************************************************/


//#include <linux/interrupt.h>
#include <linux/i2c.h>
//#include <linux/slab.h>
//#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <mach/hardware.h>
#include <asm/uaccess.h>
#include <linux/delay.h>

#define YAMAHA_GSENSOR_TRANSFORMATION    \
    { { -1,  0,  0}, \
      { 0,  -1,  0}, \
      { 0,  0,  -1} }

#define YAMAHA_MSENSOR_TRANSFORMATION    \
    { { -1,  0,  0}, \
      { 0,  1,   0}, \
      { 0 , 0 ,  -1} }
      
#if defined(CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_FLEMING_BOARD)
#if defined(CONFIG_S5PC110_KEPLER_BOARD)
#define YAMAHA_GSENSOR_TRANSFORMATION_KEPLER    \
    { { 0,  1,  0}, \
      { -1,  0,  0}, \
      { 0,  0,  -1} }
#else
#define YAMAHA_GSENSOR_TRANSFORMATION_KEPLER    \
    { { -1,  0,  0}, \
      { 0,  -1,  0}, \
      { 0,  0,  -1} }
#endif
#define YAMAHA_MSENSOR_TRANSFORMATION_KEPLER    \
    { { 0,  -1,  0}, \
      { -1,  0,   0}, \
      { 0 , 0 ,  -1} }

#define YAMAHA_MSENSOR_TRANSFORMATION_KEPLER_B5     \
    { { 0,  1,  0}, \
      { -1,  0,  0}, \
      { 0 , 0 , 1} }



#elif defined(CONFIG_S5PC110_T959_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
#define YAMAHA_GSENSOR_TRANSFORMATION_KEPLER    \
    { { -1,  0,  0}, \
      { 0,  -1,  0}, \
      { 0,  0,  -1} }

#define YAMAHA_MSENSOR_TRANSFORMATION_KEPLER    \
    { { -1,  0,  0}, \
      { 0,  1,   0}, \
      { 0 , 0 ,  -1} }

#elif defined(CONFIG_S5PC110_HAWK_BOARD) /* NAGSM CS.Lee 2010.07.28 */
#if defined(CONFIG_HAWK_VER_B1_REAL_ADDED_FEATURE) //NAGSM_Android_HQ_KERNEL_CLEE_20100908 : Setup Hawk Real Board Rev 0.1
// Real board with BMA023
#define YAMAHA_GSENSOR_TRANSFORMATION_KEPLER    \
    { { -1,  0,  0}, \
      { 0,  -1,  0}, \
      { 0,  0,  -1} }

#define YAMAHA_MSENSOR_TRANSFORMATION_KEPLER    \
    { { -1,  0,  0}, \
      { 0,  -1,  0}, \
      { 0,  0,  1} }
// Real board with BMA222
#define YAMAHA_GSENSOR_TRANSFORMATION_KEPLER_BMA222    \
    { { 1,  0,  0}, \
      { 0,  1,  0}, \
      { 0,  0,  -1} }

#define YAMAHA_MSENSOR_TRANSFORMATION_KEPLER_BMA222    \
    { { -1,  0,  0}, \
      { 0,  -1,  0}, \
      { 0,  0,  1} }
#else
// Dosirak board
#define YAMAHA_GSENSOR_TRANSFORMATION_KEPLER    \
    { { 0,  -1,  0}, \
      { -1,  0,  0}, \
      { 0,  0,  1} }

#define YAMAHA_MSENSOR_TRANSFORMATION_KEPLER    \
    { { 0,  1,  0}, \
      { -1,  0,  0}, \
      { 0,  0,  1} }
#endif
#endif

#define YAMAHA_IOCTL_GET_MARRAY            _IOR('Y', 0x01, char[9])
#define YAMAHA_IOCTL_GET_GARRAY            _IOR('Y', 0x02, char[9])

extern unsigned int HWREV;

static struct i2c_client *g_client;
struct yamaha_state *yamaha_state;
struct yamaha_state{
	struct i2c_client	*client;	
};

static 
yamaha_i2c_write(int len, const uint8_t *buf)
{
    return i2c_master_send(g_client, buf, len);
}

static int
yamaha_i2c_read(int len, uint8_t *buf)
{
    return i2c_master_recv(g_client, buf, len);
}

static int yamaha_measureRawdata(int32_t *raw) 
{
    uint8_t dat, buf[6];
 	uint8_t rv = 0;
    int i;

    dat = 0xC0;
    if (yamaha_i2c_write(1, &dat) < 0) {
        return -1;
    }

    dat = 0x00;
    if (yamaha_i2c_write(1, &dat) < 0) {
        return -1;
    }

    for (i = 0; i < 13; i++) {
        msleep(1);

        if (yamaha_i2c_read(6, buf) < 0) {
            return -1;
        }
        if (!(buf[0] & 0x80)) {
            break;
        }
    }

    if (buf[0] & 0x80) {
        return -1;
    }

    for (i = 0; i < 3; ++i) {
        raw[2 - i] = ((buf[i << 1] & 0x7) << 8) + buf[(i << 1) | 1];
    }

    if (raw[0] <= 1024 || raw[0] >= 1) {
        rv |= 0x1;
    }
    if (raw[1] <= 1024 || raw[1] >= 1) {
        rv |= 0x2;
    }
    if (raw[2] <= 1024 || raw[2] >= 1) {
        rv |= 0x4;
    }

    return (int)rv;
}

static int yamaha_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t
yamaha_read(struct file *file, char __user * buffer,
		size_t size, loff_t * pos)
{
	int32_t raw[3];

	raw[0] = raw[1] = raw[2] = 0;

	yamaha_measureRawdata(raw);

	return sprintf(buffer, "%d %d %d\n", raw[0], raw[1], raw[2]);
}

static int yamaha_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int 
yamaha_ioctl(struct inode *inode, struct file *file,
	      unsigned int cmd, unsigned long arg)
{
#if defined(CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_FLEMING_BOARD)
	signed char marray[3][3] = YAMAHA_MSENSOR_TRANSFORMATION_KEPLER;
	signed char garray[3][3] = YAMAHA_GSENSOR_TRANSFORMATION_KEPLER;
	signed char marray_B5[3][3] = YAMAHA_MSENSOR_TRANSFORMATION_KEPLER_B5;

#elif  defined(CONFIG_S5PC110_T959_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
	signed char marray[3][3] = YAMAHA_MSENSOR_TRANSFORMATION_KEPLER;
	signed char garray[3][3] = YAMAHA_GSENSOR_TRANSFORMATION_KEPLER;
	
#elif defined(CONFIG_S5PC110_HAWK_BOARD)
	signed char marray[3][3] = YAMAHA_MSENSOR_TRANSFORMATION_KEPLER;
	signed char garray[3][3] = YAMAHA_GSENSOR_TRANSFORMATION_KEPLER;
#if defined(CONFIG_HAWK_VER_B1_REAL_ADDED_FEATURE)
	signed char marray_bma222[3][3] = YAMAHA_MSENSOR_TRANSFORMATION_KEPLER_BMA222;
	signed char garray_bma222[3][3] = YAMAHA_GSENSOR_TRANSFORMATION_KEPLER_BMA222;
#endif	

#else
	signed char marray[3][3] = YAMAHA_MSENSOR_TRANSFORMATION;
	signed char garray[3][3] = YAMAHA_GSENSOR_TRANSFORMATION;
#endif

#if defined(CONFIG_HAWK_VER_B1_REAL_ADDED_FEATURE) //NAGSM_Android_HQ_KERNEL_CLEE_20100930 :  real board and version rev 0.2
	int i=0;
	int j=0;
	extern int HWREV_HAWK;

	if ( HWREV_HAWK >= 2)	// real board and version rev 0.2 (Sept. 30)
	{
		for (i=0; i<3; i++)
		{
			for (j=0; j<3; j++)
			{
				marray[i][j] = marray_bma222[i][j];
				garray[i][j] = garray_bma222[i][j];
			}
		}
	}
	
#endif

	switch (cmd) {
		case YAMAHA_IOCTL_GET_MARRAY:

#if defined(CONFIG_S5PC110_KEPLER_BOARD) ||defined(CONFIG_S5PC110_FLEMING_BOARD)
				if(!(HWREV == 0x08 || HWREV == 0x04 || HWREV == 0x0C || HWREV == 0x02 || HWREV == 0x0A))  // 
				{
					if (copy_to_user((void *)arg, marray_B5, sizeof(marray_B5))) 
					{
						printk("YAMAHA_GSENSOR_TRANSFORMATION_EMUL copy failed\n");
						return -EFAULT;
					}

				}
				else
				{
				if (copy_to_user((void *)arg, marray, sizeof(marray))) 
				{
					printk("YAMAHA_GSENSOR_TRANSFORMATION_EMUL copy failed\n");
					return -EFAULT;
				}
				}
			//	printk("YAMAHA_GSENSOR_TRANSFORMATION_EMUL copy\n");
#elif  defined(CONFIG_S5PC110_T959_BOARD) || defined(CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)

				if (copy_to_user((void *)arg, marray, sizeof(marray))) 
				{
					printk("YAMAHA_GSENSOR_TRANSFORMATION_EMUL copy failed\n");
					return -EFAULT;
				}


#else	
				if (copy_to_user((void *)arg, marray, sizeof(marray))) 
				{
					printk("YAMAHA_MSENSOR_TRANSFORMATION copy failed\n");
					return -EFAULT;
				}
#endif
			break;
		case YAMAHA_IOCTL_GET_GARRAY:
#if defined(CONFIG_S5PC110_KEPLER_BOARD) || defined(CONFIG_S5PC110_FLEMING_BOARD) || defined(CONFIG_S5PC110_T959_BOARD) || defined(CONFIG_S5PC110_HAWK_BOARD) || defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
				if (copy_to_user((void *)arg, garray, sizeof(garray))) 
				{
					printk("YAMAHA_GSENSOR_TRANSFORMATION copy failed\n");
					return -EFAULT;
				}
			
#else			
				if (copy_to_user((void *)arg, garray, sizeof(garray))) 
				{
					printk("YAMAHA_GSENSOR_TRANSFORMATION copy failed\n");
					return -EFAULT;
				}
#endif
			break;
		default:
			return -ENOTTY;
	}

	return 0;
}

static struct file_operations yamaha_fops = {
	.owner = THIS_MODULE,
	.open = yamaha_open,
	.read = yamaha_read,
	.release = yamaha_release,
	.ioctl = yamaha_ioctl,
};

static struct miscdevice yamaha_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "yamaha_compass",
	.fops = &yamaha_fops,
};

static int __devinit yamaha_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct yamaha_state *yamaha;

	yamaha = kzalloc(sizeof(struct yamaha_state), GFP_KERNEL);
	if (yamaha == NULL) {
		pr_err("%s: failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	yamaha->client = client;
	i2c_set_clientdata(client, yamaha);
    g_client = client;

    return 0;
}

static int __devexit yamaha_remove(struct i2c_client *client)
{
	g_client = NULL;
	return 0;
}

static const struct i2c_device_id yamaha_ids[] = {
	{ "yamaha", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, yamaha_ids);

struct i2c_driver yamaha_i2c_driver =
{
	.driver	= {
		.name	= "yamaha",
	},
	.probe		= yamaha_probe,
	.remove		= __devexit_p(yamaha_remove),
	.id_table	= yamaha_ids,

};

static int __init yamaha_init(void)
{
	int err;

	printk("yamaha_init\n");
	err = misc_register(&yamaha_device);
	if (err) {
		printk("yamaha_init Cannot register miscdev:%d\n",err);
		return err;
	}

	if ( (err = i2c_add_driver(&yamaha_i2c_driver)) ) 
	{
		printk("Driver registration failed, module not inserted.\n");
		return err;
	}

	return 0;
}

static void __exit yamaha_exit(void)
{
	misc_deregister(&yamaha_device);
	printk("__exit\n");
}

module_init(yamaha_init);
module_exit(yamaha_exit);

MODULE_AUTHOR("SAMSUNG");
MODULE_DESCRIPTION("yamaha compass driver");
MODULE_LICENSE("GPL");
