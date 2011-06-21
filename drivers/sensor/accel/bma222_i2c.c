#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/workqueue.h>

//#include <asm/hardware.h>
//#include <asm/arch/gpio.h>
//add by inter.park
#include <mach/hardware.h>
#include <linux/gpio.h>

#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include "bma222_i2c.h"


#define I2C_M_WR				0x00
#define I2C_DF_NOTIFY			0x01


static struct i2c_client *g_client;	

struct bma222_state{
	struct i2c_client	*client;	
};

struct bma222_state *bma222_state;

char i2c_acc_bma222_read(u8 reg, u8 *val, unsigned int len )
{
	int 	 err;
	struct 	 i2c_msg msg[1];
		
	unsigned char data[1];
	if( (g_client == NULL) || (!g_client->adapter) )
	{
		return -ENODEV;
	}
	
	msg->addr 	= g_client->addr;
	msg->flags 	= I2C_M_WR;
	msg->len 	= 1;
	msg->buf 	= data;
	*data       = reg;

	err = i2c_transfer(g_client->adapter, msg, 1);

	if (err >= 0) 
	{
		msg->flags = I2C_M_RD;
		msg->len   = len;
		msg->buf   = val;
		err = i2c_transfer(g_client->adapter, msg, 1);
	}

	if (err >= 0) 
	{
		return 0;
	}
	printk("%s %d i2c transfer error\n", __func__, __LINE__);/* add by inter.park */

	return err;

}
char i2c_acc_bma222_write( u8 reg, u8 *val )
{
	int err;
	struct i2c_msg msg[1];
	unsigned char data[2];

	if( (g_client == NULL) || (!g_client->adapter) ){
		return -ENODEV;
	}
	
	data[0] = reg;
	data[1] = *val;

	msg->addr = g_client->addr;
	msg->flags = I2C_M_WR;
	msg->len = 2;
	msg->buf = data;
	
	err = i2c_transfer(g_client->adapter, msg, 1);

	if (err >= 0) return 0;

	printk("%s %d i2c transfer error\n", __func__, __LINE__);/* add by inter.park */
	return err;
}

static int __devinit bma222_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct bma222_state *bma;
        printk("%s called \n",__func__);
	bma = kzalloc(sizeof(struct bma222_state), GFP_KERNEL);
	if (bma == NULL) {
		pr_err("%s: failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	bma->client = client;
	i2c_set_clientdata(client, bma);
	
	printk("[%s] slave addr = %x\n", __func__, client->addr);
		g_client = client;
        return 0;
}

static int __devexit bma222_remove(struct i2c_client *client)
{	
	g_client = NULL;
	return 0;
}

static const struct i2c_device_id bma222_ids[] = {	
	{ "bma222", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, bma222_ids);

struct i2c_driver acc_bma222_i2c_driver =
{
	.driver	= {
		.name	= "bma222",
	},
	.probe		= bma222_probe,
	.remove		= __devexit_p(bma222_remove),
	.id_table	= bma222_ids,

};

int i2c_acc_bma222_init(void)
{
	int ret;
        printk("%s called \n",__func__);
	if ( (ret = i2c_add_driver(&acc_bma222_i2c_driver)) ) 
	{
		printk("Driver registration failed, module not inserted.\n");
		return ret;
	}

	return 0;
}

void i2c_acc_bma222_exit(void)
{
	printk("[bma222] i2c_exit\n");
	i2c_del_driver(&acc_bma222_i2c_driver); 
}

