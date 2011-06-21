/* linux/drivers/media/video/samsung/tv20/s5p_tv_base.c
 *
 * Entry file for Samsung TVOut driver
 *
 * Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/irq.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <mach/map.h>

#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include <linux/earlysuspend.h>
#include <mach/max8998_function.h>

#if 0
#include <plat/s5pc11x-dvfs.h>
#endif

#ifdef CONFIG_S5PV210_PM
#include <mach/pd.h>
#endif

#include "s5p_tv.h"

#ifdef COFIG_TVOUT_DBG
#define S5P_TV_BASE_DEBUG 1
#endif

#ifdef S5P_TV_BASE_DEBUG
#define BASEPRINTK(fmt, args...) \
	printk("[TVBASE] %s: " fmt, __FUNCTION__ , ## args)
#else
#define BASEPRINTK(fmt, args...)
#endif

#ifdef CONFIG_CPU_S5PV210
#define TVOUT_CLK_INIT(dev, clk, name)
#else
#define TVOUT_CLK_INIT(dev, clk, name) 							\
	clk= clk_get(dev, name);							\
	if(clk == NULL) { 								\
		printk(KERN_ERR  "failed to find %s clock source\n", name);		\
		return -ENOENT;								\
	}										\
	clk_enable(clk)
#endif

#define TVOUT_IRQ_INIT(x, ret, dev, num, jump, ftn, m_name) 				\
	x = platform_get_irq(dev, num); 						\
	if (x <0 ) {									\
		printk(KERN_ERR  "failed to get %s irq resource\n", m_name);		\
		ret = -ENOENT; 								\
		goto jump;								\
	}										\
	ret = request_irq(x, ftn, IRQF_DISABLED, dev->name, dev) ;			\
	if (ret != 0) {									\
		printk(KERN_ERR  "failed to install %s irq (%d)\n", m_name, ret);	\
		goto jump;								\
	}										\
	while(0)


#ifdef CONFIG_CPU_S5PC100
#define I2C_BASE
#endif 

static struct mutex	*mutex_for_fo = NULL;
	
#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)	
extern int s5p_hdcp_encrypt_stop(bool on); //Rajucm
#endif

s5p_tv_status 	s5ptv_status;
s5p_tv_vo 	s5ptv_overlay[2];

#ifdef I2C_BASE
static struct mutex	*mutex_for_i2c= NULL;
static struct work_struct ws_hpd;
spinlock_t slock_hpd;

static struct i2c_driver hdcp_i2c_driver;
static bool hdcp_i2c_drv_state = false;

const static u16 ignore[] = { I2C_CLIENT_END };
const static u16 normal_addr[] = {(S5P_HDCP_I2C_ADDR >> 1), I2C_CLIENT_END };
const static u16 *forces[] = { NULL };

static struct i2c_client_address_data addr_data = {
	.normal_i2c	= normal_addr,
	.probe		= ignore,
	.ignore		= ignore,
	.forces		= forces,
};

/*
 * i2c client drv.  - register client drv
 */
static int hdcp_i2c_attach(struct i2c_adapter *adap, int addr, int kind)
{

	struct i2c_client *c;

	c = kzalloc(sizeof(*c), GFP_KERNEL);

	if (!c)
		return -ENOMEM;

	strcpy(c->name, "s5p_ddc_client");

	c->addr = addr;

	c->adapter = adap;

	c->driver = &hdcp_i2c_driver;

	s5ptv_status.hdcp_i2c_client = c;

	dev_info(&adap->dev, "s5p_ddc_client attached "
		"into s5p_ddc_port successfully\n");

	return i2c_attach_client(c);
}

static int hdcp_i2c_attach_adapter(struct i2c_adapter *adap)
{
	int ret = 0;

	ret = i2c_probe(adap, &addr_data, hdcp_i2c_attach);

	if (ret) {
		dev_err(&adap->dev, 
			"failed to attach s5p_hdcp_port driver\n");
		ret = -ENODEV;
	}

	return ret;
}

static int hdcp_i2c_detach(struct i2c_client *client)
{
	dev_info(&client->adapter->dev, "s5p_ddc_client detached "
		"from s5p_ddc_port successfully\n");

	i2c_detach_client(client);

	return 0;
}

static struct i2c_driver hdcp_i2c_driver = {
	.driver = {
		.name = "s5p_ddc_port",
	},
	.id = I2C_DRIVERID_S5P_HDCP,
	.attach_adapter = hdcp_i2c_attach_adapter,
	.detach_client = hdcp_i2c_detach,
};

static void set_ddc_port(void)
{
	mutex_lock(mutex_for_i2c);
	
	if(s5ptv_status.hpd_status) {

		if (!hdcp_i2c_drv_state)
			/* cable : plugged, drv : unregistered */
			if (i2c_add_driver(&hdcp_i2c_driver))
				printk(KERN_INFO "HDCP port add failed\n");

		/* changed drv. status */
		hdcp_i2c_drv_state = true;
		

		/* cable inserted -> removed */
		__s5p_set_hpd_detection(true, s5ptv_status.hdcp_en, 
			s5ptv_status.hdcp_i2c_client);
		
	} else {

		if (hdcp_i2c_drv_state)
			/* cable : unplugged, drv : registered */
			i2c_del_driver(&hdcp_i2c_driver);
		
		/* changed drv. status */
		hdcp_i2c_drv_state = false;

		/* cable removed -> inserted */
		__s5p_set_hpd_detection(false, s5ptv_status.hdcp_en,
			s5ptv_status.hdcp_i2c_client);
	}
	
	mutex_unlock(mutex_for_i2c);
}
#endif 

#ifdef CONFIG_CPU_S5PC100
static irqreturn_t __s5p_hpd_irq(int irq, void *dev_id)
{

	spin_lock_irq(&slock_hpd);
	
	s5ptv_status.hpd_status = gpio_get_value(S5PC1XX_GPH0(5)) 
		? false:true;

	if(s5ptv_status.hpd_status){
		
		set_irq_type(IRQ_EINT5, IRQ_TYPE_EDGE_RISING);
		
	}else{
		set_irq_type(IRQ_EINT5, IRQ_TYPE_EDGE_FALLING);			

	}

	if (s5ptv_status.hdcp_en)
		schedule_work(&ws_hpd);

	spin_unlock_irq(&slock_hpd);

	BASEPRINTK("hpd_status = %d\n", s5ptv_status.hpd_status);
			
	return IRQ_HANDLED;
}
#endif

#ifdef CONFIG_CPU_S5PV210

//Subhransu20110304
extern void __s5p_tv_poweroff_test(void);
void __s5p_hdmi_phy_power_offtest(void)
{
	s5p_tv_clk_gate(true);
	clk_enable(s5ptv_status.i2c_phy_clk);
	__s5p_tv_poweron();
	__s5p_hdmi_phy_power(false);
	clk_disable(s5ptv_status.i2c_phy_clk);

	__s5p_tv_poweroff_test();
	s5p_tv_clk_gate(false);
}
int tv_power_status = 0; 
int tv_phy_power( bool on )
{

	if (on) {
            __s5p_tv_poweron();
		/* on */
		clk_enable(s5ptv_status.i2c_phy_clk);
		__s5p_hdmi_phy_power(true);
		tv_power_status = 1; 
		
	} else {
		/* 
		 * for preventing hdmi hang up when restart 
		 * switch to internal clk - SCLK_DAC, SCLK_PIXEL 
		 */
		__s5p_tv_clk_change_internal();			
		__s5p_hdmi_phy_power(false);
		clk_disable(s5ptv_status.i2c_phy_clk);
		 __s5p_tv_poweroff();
		 tv_power_status = 0; 
	}

	return 0;
}


int s5p_tv_clk_gate( bool on )
{
	if (on) {
#ifdef CONFIG_S5PV210_PM
		if (s5pv210_pd_enable("vp_pd") < 0) {
			printk(KERN_ERR "[Error]The power is not on for VP\n");
			goto err_pm;
		}
#endif
		clk_enable(s5ptv_status.vp_clk);
#ifdef CONFIG_S5PV210_PM
		if (s5pv210_pd_enable("mixer_pd") < 0) {
			printk(KERN_ERR "[Error]The power is not on for mixer\n");
			goto err_pm;
		}
#endif
		clk_enable(s5ptv_status.mixer_clk);
#ifdef CONFIG_S5PV210_PM
		if (s5pv210_pd_enable("tv_enc_pd") < 0) {
			printk(KERN_ERR "[Error]The power is not on for TV ENC\n");
			goto err_pm;
		}
#endif
		clk_enable(s5ptv_status.tvenc_clk);
#ifdef CONFIG_S5PV210_PM
		if (s5pv210_pd_enable("hdmi_pd") < 0) {
			printk(KERN_ERR "[Error]The power is not on for HDMI\n");
			goto err_pm;
		}
#endif
		clk_enable(s5ptv_status.hdmi_clk);

	} else {

		/* off */
		clk_disable(s5ptv_status.vp_clk);
#ifdef CONFIG_S5PV210_PM
		if (s5pv210_pd_disable("vp_pd") < 0) {
			printk(KERN_ERR "[Error]The power is not off for VP\n");
			goto err_pm;
		}
#endif
		clk_disable(s5ptv_status.mixer_clk);
#ifdef CONFIG_S5PV210_PM
		if (0 != s5pv210_pd_disable("mixer_pd")) {
			printk(KERN_ERR "[Error]The power is not off for mixer\n");
			goto err_pm;
		}
#endif
		clk_disable(s5ptv_status.tvenc_clk);
#ifdef CONFIG_S5PV210_PM
		if (s5pv210_pd_disable("tv_enc_pd") < 0) {
			printk(KERN_ERR "[Error]The power is not off for TV ENC\n");
			goto err_pm;
		}
#endif
		clk_disable(s5ptv_status.hdmi_clk);
#ifdef CONFIG_S5PV210_PM
		if (s5pv210_pd_disable("hdmi_pd") < 0) {
			printk(KERN_ERR "[Error]The power is not off for HDMI\n");
			goto err_pm;
		}
#endif
	}

	return 0;
#ifdef CONFIG_S5PV210_PM
	err_pm:
	return -1;
#endif
}

EXPORT_SYMBOL(s5p_tv_clk_gate);

static int __devinit tv_clk_get(struct platform_device *pdev, struct _s5p_tv_status *ctrl)
{
	/* tvenc clk */
	ctrl->tvenc_clk = clk_get(&pdev->dev, "tvenc");

	if(IS_ERR(ctrl->tvenc_clk)) {
		printk(KERN_ERR  "failed to find %s clock source\n", "tvenc");
		return -ENOENT;
	}

	/* vp clk */
	ctrl->vp_clk = clk_get(&pdev->dev, "vp");

	if(IS_ERR(ctrl->vp_clk)) {
		printk(KERN_ERR  "failed to find %s clock source\n", "vp");
		return -ENOENT;
	}

	/* mixer clk */
	ctrl->mixer_clk = clk_get(&pdev->dev, "mixer");

	if(IS_ERR(ctrl->mixer_clk)) {
		printk(KERN_ERR  "failed to find %s clock source\n", "mixer");
		return -ENOENT;
	}

	/* hdmi clk */
	ctrl->hdmi_clk = clk_get(&pdev->dev, "hdmi");

	if(IS_ERR(ctrl->hdmi_clk)) {
		printk(KERN_ERR  "failed to find %s clock source\n", "hdmi");
		return -ENOENT;
	}

	/* i2c-hdmiphy clk */
	ctrl->i2c_phy_clk= clk_get(&pdev->dev, "i2c-hdmiphy");

	if(IS_ERR(ctrl->i2c_phy_clk)) {
		printk(KERN_ERR  "failed to find %s clock source\n", "i2c-hdmiphy");
		return -ENOENT;
	}
	
	return 0;
}
#else
#define s5p_tv_clk_gate NULL
#define tv_phy_power NULL
#define tv_clk_get NULL
#endif

/*
 * ftn for irq 
 */
static irqreturn_t s5p_tvenc_irq(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

#ifdef CONFIG_TV_FB
static int s5p_tv_open(struct file *file)
{
	/* 
	 * for first open 
	 * when boot up time this parameter is set.
	 */
	if ( s5ptv_status.tvout_output_enable )
		_s5p_tv_if_stop();

	return 0;	
}

static int s5p_tv_release(struct file *file)
{
	s5ptv_status.hdcp_en = false;

	if ( s5ptv_status.tvout_output_enable )
		_s5p_tv_if_stop();
	
	return 0;
}

static int s5p_tv_vid_open(struct file *file)
{
	int ret = 0;
	
	mutex_lock(mutex_for_fo);

	if (s5ptv_status.vp_layer_enable) {
		printk("video layer. already used !!\n");
		ret =  -EBUSY;
	}

	mutex_unlock(mutex_for_fo);
	return ret;	
}

static int s5p_tv_vid_release(struct file *file)
{
	s5ptv_status.vp_layer_enable = false;

	_s5p_vlayer_stop();
		
	return 0;
}
#else

/*
 * ftn for video
 */
static int s5p_tv_v_open(struct file *file)
{
	int ret = 0,err;

	mutex_lock(mutex_for_fo);

	if (s5ptv_status.tvout_output_enable) {
		BASEPRINTK("tvout drv. already used !!\n");
		ret =  -EBUSY;
		goto drv_used;
	}

#ifdef CONFIG_CPU_S5PV210
#ifdef CONFIG_PM_PWR_GATING 
	if((s5ptv_status.hpd_status) && !(s5ptv_status.suspend_status))
	{
		BASEPRINTK("tv is turned on\n");
#endif
#if 0
#ifdef CONFIG_CPU_FREQ
#ifdef CONFIG_CPU_MAX_FREQ_1GHZ // 2010.3.9.
		if((s5ptv_status.hpd_status))
			s5pc110_lock_dvfs_high_level(DVFS_LOCK_TOKEN_4, 3);    
#else
		if((s5ptv_status.hpd_status))
			s5pc110_lock_dvfs_high_level(DVFS_LOCK_TOKEN_4, 2);
#endif
#endif
#endif

#if defined(CONFIG_S5PC110_SIDEKICK_BOARD)	
	err = gpio_request(S5PV210_GPJ3(1),"EAR_SEL");
	udelay(50);
	gpio_direction_output(S5PV210_GPJ3(1),0);
	gpio_set_value(S5PV210_GPJ3(1),0);
	udelay(50);
#elif defined(CONFIG_S5PC110_DEMPSEY_BOARD)
//there is no connected pin
#else
	err = gpio_request(S5PV210_GPJ4(4),"TV_EN");
	udelay(50);
	gpio_direction_output(S5PV210_GPJ4(4),1);
	gpio_set_value(S5PV210_GPJ4(4),1);
	udelay(50);

	err = gpio_request(S5PV210_GPJ2(6),"EAR_SEL");
	udelay(50);
	gpio_direction_output(S5PV210_GPJ2(6),0);
	gpio_set_value(S5PV210_GPJ2(6),0);
	udelay(50);
#endif
	
		s5p_tv_clk_gate( true );
		tv_phy_power( true );
#ifdef CONFIG_PM_PWR_GATING
	}
	else
		BASEPRINTK("tv is off\n");
#endif
#endif
	_s5p_tv_if_init_param();

	s5p_tv_v4l2_init_param();

	mutex_unlock(mutex_for_fo);

	/* c110 test */	//s5ptv_status.hpd_status = true;

  printk("\n\nTV open success\n\n");

#ifdef I2C_BASE
	mutex_lock(mutex_for_i2c);
	/* for ddc(hdcp port) */
	if(s5ptv_status.hpd_status) {
		if (i2c_add_driver(&hdcp_i2c_driver)) 
			BASEPRINTK("HDCP port add failed\n");
		hdcp_i2c_drv_state = true;
	} else 
		hdcp_i2c_drv_state = false;

	mutex_unlock(mutex_for_i2c);
	/* for i2c probing */
	udelay(100);
#endif

	return 0;

drv_used:
	mutex_unlock(mutex_for_fo);
	return ret;
}

int s5p_tv_v_read(struct file *filp, char *buf, size_t count,
		  loff_t *f_pos)
{
	return 0;
}

int s5p_tv_v_write(struct file *filp, const char *buf, size_t
		   count, loff_t *f_pos)
{
	return 0;
}

int s5p_tv_v_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return 0;
}

int s5p_tv_v_release(struct file *filp)
{
#if defined(CONFIG_CPU_S5PV210) && defined(CONFIG_PM_PWR_GATING)
	if((s5ptv_status.hpd_status) && !(s5ptv_status.suspend_status))
	{
#endif
		if(s5ptv_status.vp_layer_enable)
			_s5p_vlayer_stop();
		if(s5ptv_status.tvout_output_enable)
			_s5p_tv_if_stop();
#if defined(CONFIG_CPU_S5PV210) && defined(CONFIG_PM_PWR_GATING)
	}else
		s5ptv_status.vp_layer_enable = false;
#endif 

	s5ptv_status.hdcp_en = false;

	s5ptv_status.tvout_output_enable = false;

	/* 
	 * drv. release
	 *        - just check drv. state reg. or not.
	 */
#ifdef I2C_BASE	 
	mutex_lock(mutex_for_i2c);

	if (hdcp_i2c_drv_state) {
		i2c_del_driver(&hdcp_i2c_driver);
		hdcp_i2c_drv_state = false;
	}

	mutex_unlock(mutex_for_i2c);
#endif

#ifdef CONFIG_CPU_S5PV210
#ifdef CONFIG_PM_PWR_GATING
	if((s5ptv_status.hpd_status) && !(s5ptv_status.suspend_status))
	{
#endif
		s5p_tv_clk_gate(false);
		tv_phy_power( false );
#if 0
#ifdef CONFIG_CPU_FREQ
		if(s5ptv_status.hpd_status)
			s5pc110_unlock_dvfs_high_level(DVFS_LOCK_TOKEN_4);
#endif
#endif    
#ifdef CONFIG_PM_PWR_GATING
	}
#endif
#endif

	return 0;
}

/*
 * ftn for graphic(video output overlay)
 */
 /*
static int check_layer(dev_t dev)
{
	int id = 0;
	int layer = 0;

	id = MINOR(dev);

	if (id < TVOUT_MINOR_GRP0 || id > TVOUT_MINOR_GRP1)
		BASEPRINTK("grp layer invalid\n");
		
	layer = (id == TVOUT_MINOR_GRP0) ? 0:1;

	return layer;

}
*/

static int vo_open(int layer, struct file *file)
{
	int ret = 0;

	mutex_lock(mutex_for_fo);

	/* check tvout path available!! */
	if (!s5ptv_status.tvout_output_enable) {
		BASEPRINTK("check tvout start !!\n");
		ret =  -EACCES;
		goto resource_busy;
	}

	if (s5ptv_status.grp_layer_enable[layer]) {
		BASEPRINTK("grp %d layer is busy!!\n", layer);
		ret =  -EBUSY;
		goto resource_busy;
	}

	/* set layer info.!! */
	s5ptv_overlay[layer].index = layer;

	/* set file private data.!! */
	file->private_data = &s5ptv_overlay[layer];

	mutex_unlock(mutex_for_fo);

	return 0;

resource_busy:
	mutex_unlock(mutex_for_fo);

	return ret;
}

int vo_release(int layer, struct file *filp)
{

	_s5p_grp_stop(layer);

	return 0;
}

/* modified for 2.6.29 v4l2-dev.c */
static int s5p_tv_vo0_open(struct file *file)
{
	vo_open(0, file);
	return 0;
}

static int s5p_tv_vo0_release(struct file *file)
{
	vo_release(0,file);
	return 0;
}

static int s5p_tv_vo1_open(struct file *file)
{
	vo_open(1, file);
	return 0;	
}

static int s5p_tv_vo1_release(struct file *file)
{
	vo_release(1,file);
	return 0;	
}
#endif

#ifdef CONFIG_TV_FB
static int s5ptvfb_alloc_framebuffer(void)
{
	int ret;

	/* alloc for each framebuffer */
	s5ptv_status.fb = framebuffer_alloc(sizeof(struct s5ptvfb_window),
					 s5ptv_status.dev_fb);
	if (!s5ptv_status.fb) {
		dev_err(s5ptv_status.dev_fb, "not enough memory\n");
		ret = -ENOMEM;
		goto err_alloc_fb;
	}

	ret = s5ptvfb_init_fbinfo(5);
	if (ret) {
		dev_err(s5ptv_status.dev_fb,
			"failed to allocate memory for fb for tv\n");
		ret = -ENOMEM;
		goto err_alloc_fb;
	}
#ifndef CONFIG_USER_ALLOC_TVOUT
	if (s5ptvfb_map_video_memory(s5ptv_status.fb)) {
		dev_err(s5ptv_status.dev_fb,
			"failed to map video memory "
			"for default window \n");
		ret = -ENOMEM;
		goto err_alloc_fb;
	}
#endif
	return 0;

err_alloc_fb:
	if (s5ptv_status.fb)
		framebuffer_release(s5ptv_status.fb);

	kfree(s5ptv_status.fb);

	return ret;
}

int s5ptvfb_free_framebuffer(void)
{
#ifndef CONFIG_USER_ALLOC_TVOUT
	if (s5ptv_status.fb)
		s5ptvfb_unmap_video_memory(s5ptv_status.fb);
#endif

	if (s5ptv_status.fb)
		framebuffer_release(s5ptv_status.fb);

	return 0;
}

int s5ptvfb_register_framebuffer(void)
{
	int ret;

	ret = register_framebuffer(s5ptv_status.fb);
	if (ret) {
		dev_err(s5ptv_status.dev_fb, "failed to register "
			"framebuffer device\n");
		return -EINVAL;
	}	
#ifndef CONFIG_FRAMEBUFFER_CONSOLE
#ifndef CONFIG_USER_ALLOC_TVOUT
	s5ptvfb_check_var(&s5ptv_status.fb->var, s5ptv_status.fb);
	s5ptvfb_set_par(s5ptv_status.fb);
	s5ptvfb_draw_logo(s5ptv_status.fb);
#endif
#endif

	return 0;
}

#endif
/*
 * struct for video
 */
#ifdef CONFIG_TV_FB
static struct v4l2_file_operations s5p_tv_fops = {
	.owner		= THIS_MODULE,
	.open		= s5p_tv_open,
	.ioctl		= s5p_tv_ioctl,
	.release	= s5p_tv_release
};
static struct v4l2_file_operations s5p_tv_vid_fops = {
	.owner		= THIS_MODULE,
	.open		= s5p_tv_vid_open,
	.ioctl		= s5p_tv_vid_ioctl,
	.release	= s5p_tv_vid_release
};


 #else
static struct v4l2_file_operations s5p_tv_v_fops = {
	.owner		= THIS_MODULE,
	.open		= s5p_tv_v_open,
	.read		= s5p_tv_v_read,
	.write		= s5p_tv_v_write,
	.ioctl		= s5p_tv_v_ioctl,
	.mmap		= s5p_tv_v_mmap,
	.release	= s5p_tv_v_release
};

/*
 * struct for graphic0
 */
static struct v4l2_file_operations s5p_tv_vo0_fops = {
	.owner		= THIS_MODULE,
	.open		= s5p_tv_vo0_open,
	.ioctl		= s5p_tv_vo_ioctl,
	.release	= s5p_tv_vo0_release
};

/*
 * struct for graphic1
 */
static struct v4l2_file_operations s5p_tv_vo1_fops = {
	.owner		= THIS_MODULE,
	.open		= s5p_tv_vo1_open,
	.ioctl		= s5p_tv_vo_ioctl,
	.release	= s5p_tv_vo1_release
};
#endif


void s5p_tv_vdev_release(struct video_device *vdev)
{
	kfree(vdev);
}

struct video_device s5p_tvout[] = {

#ifdef CONFIG_TV_FB	
	[0] = {
		.name = "S5PC1xx TVOUT ctrl",
		.fops = &s5p_tv_fops,
		.ioctl_ops = &s5p_tv_v4l2_ops,
		.release  = s5p_tv_vdev_release,
		.minor = TVOUT_MINOR_TVOUT,
		.tvnorms = V4L2_STD_ALL_HD,
	},	
	[1] = {
		.name = "S5PC1xx TVOUT for Video",
		.fops = &s5p_tv_vid_fops,
		.ioctl_ops = &s5p_tv_v4l2_vid_ops,
		.release  = s5p_tv_vdev_release,
		.minor = TVOUT_MINOR_VID,
		.tvnorms = V4L2_STD_ALL_HD,
	},
#else
	[0] = {
		.name = "S5PC1xx TVOUT Video",
		.fops = &s5p_tv_v_fops,
		.ioctl_ops = &s5p_tv_v4l2_v_ops,
		.release  = s5p_tv_vdev_release,
		.minor = TVOUT_MINOR_VIDEO,
		.tvnorms = V4L2_STD_ALL_HD,
	},
	[1] = {
		.name = "S5PC1xx TVOUT Overlay0",
		//.type2 = V4L2_CAP_VIDEO_OUTPUT_OVERLAY,
		.fops = &s5p_tv_vo0_fops,
		.ioctl_ops = &s5p_tv_v4l2_vo_ops,
		.release  = s5p_tv_vdev_release,
		.minor = TVOUT_MINOR_GRP0,
		.tvnorms = V4L2_STD_ALL_HD,
	},
	[2] = {
		.name = "S5PC1xx TVOUT Overlay1",
		//.type2 = V4L2_CAP_VIDEO_OUTPUT_OVERLAY,
		.fops = &s5p_tv_vo1_fops,
		.ioctl_ops = &s5p_tv_v4l2_vo_ops,
		.release  = s5p_tv_vdev_release,
		.minor = TVOUT_MINOR_GRP1,
		.tvnorms = V4L2_STD_ALL_HD,
	},
#endif	
};


#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)				//MHL v1 //NAGSM_Android_SEL_Kernel_Aakash_20101130
//MHL v1 //NAGSM_Android_SEL_Kernel_Aakash_20101126
void rcp_cbus_uevent(u8 rcpCode)	//NAGSM_Android_SEL_Kernel_Aakash_20101206
{
	char env_buf[120];
	char *envp[2];
	int env_offset = 0;

	memset(env_buf, 0, sizeof(env_buf));
	BASEPRINTK("\n RCP Message Recvd \n");
	sprintf(env_buf, "MHL_RCP=%x", rcpCode);	//NAGSM_Android_SEL_Kernel_Aakash_20101206
	envp[env_offset++] = env_buf;
	envp[env_offset] = NULL;
	kobject_uevent_env(&(s5p_tvout[0].dev.kobj), KOBJ_CHANGE, envp);
	return;


}
EXPORT_SYMBOL(rcp_cbus_uevent);
//MHL v1 //NAGSM_Android_SEL_Kernel_Aakash_20101126
#endif

void s5p_handle_cable(void)
{
    char env_buf[120];
    char *envp[2];
    int env_offset = 0;

#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)				//MHL v1 //NAGSM_Android_SEL_Kernel_Aakash_20101130

#else
    if(s5ptv_status.tvout_param.out_mode != TVOUT_OUTPUT_HDMI && s5ptv_status.tvout_param.out_mode != TVOUT_OUTPUT_HDMI_RGB)
        return;
#endif

    bool previous_hpd_status = s5ptv_status.hpd_status;
#ifdef CONFIG_HDMI_HPD
    s5ptv_status.hpd_status= s5p_hpd_get_state();
#else
    return;
#endif
    
    memset(env_buf, 0, sizeof(env_buf));

    if(previous_hpd_status == s5ptv_status.hpd_status)
    {
        BASEPRINTK("same hpd_status value: %d\n", previous_hpd_status);
        return;
    }

    if(s5ptv_status.hpd_status)
    {
        BASEPRINTK("\n hdmi cable is connected \n");			
        if(s5ptv_status.suspend_status)
            return;

#if 0
    #ifdef CONFIG_CPU_FREQ
    #ifdef CONFIG_CPU_MAX_FREQ_1GHZ // 2010.3.9.
        s5pc110_lock_dvfs_high_level(DVFS_LOCK_TOKEN_4, 3);
    #else
        s5pc110_lock_dvfs_high_level(DVFS_LOCK_TOKEN_4, 2);    
    #endif
    #endif
#endif

    #ifdef CONFIG_PM_PWR_GATING
        s5p_tv_clk_gate( true );
        tv_phy_power( true );
    #endif
#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)
        s5p_hdcp_encrypt_stop(true); //Rajucm
#endif
        /* tv on */
        if ( s5ptv_status.tvout_output_enable )
                _s5p_tv_if_start();

        /* video layer start */
        if ( s5ptv_status.vp_layer_enable )
                _s5p_vlayer_start();

        /* grp0 layer start */
        if ( s5ptv_status.grp_layer_enable[0] )
                _s5p_grp_start(VM_GPR0_LAYER);

        /* grp1 layer start */
        if ( s5ptv_status.grp_layer_enable[1] )
                _s5p_grp_start(VM_GPR1_LAYER);

        sprintf(env_buf, "HDMI_STATE=online");
        envp[env_offset++] = env_buf;
        envp[env_offset] = NULL;
        kobject_uevent_env(&(s5p_tvout[0].dev.kobj), KOBJ_CHANGE, envp);
	printk("HDMI_STATE=online sent to App \n");
    }
    else{
        BASEPRINTK("\n hdmi cable is disconnected \n");
        
        if(s5ptv_status.suspend_status)
            return;
#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)
        s5p_hdcp_encrypt_stop(false); //Rajucm
#endif
        sprintf(env_buf, "HDMI_STATE=offline");
        envp[env_offset++] = env_buf;
        envp[env_offset] = NULL;
        kobject_uevent_env(&(s5p_tvout[0].dev.kobj), KOBJ_CHANGE, envp);
	printk("HDMI_STATE=offline sent to App \n");

        if ( s5ptv_status.vp_layer_enable ) {
            _s5p_vlayer_stop();
            s5ptv_status.vp_layer_enable = true;

        }

        /* grp0 layer stop */
        if ( s5ptv_status.grp_layer_enable[0] ) {
            _s5p_grp_stop(VM_GPR0_LAYER);
            s5ptv_status.grp_layer_enable[VM_GPR0_LAYER] = true;
        }

        /* grp1 layer stop */
        if ( s5ptv_status.grp_layer_enable[1] ) {
            _s5p_grp_stop(VM_GPR1_LAYER);
            s5ptv_status.grp_layer_enable[VM_GPR0_LAYER] = true;
        }

        /* tv off */
        if ( s5ptv_status.tvout_output_enable ) {
            _s5p_tv_if_stop();
            s5ptv_status.tvout_output_enable = true;
            s5ptv_status.tvout_param_available = true;
        }

    #ifdef CONFIG_PM_PWR_GATING
        /* clk & power off */
        s5p_tv_clk_gate( false );
        tv_phy_power( false );
    #endif

#if 0
    #ifdef CONFIG_CPU_FREQ
        s5pc110_unlock_dvfs_high_level(DVFS_LOCK_TOKEN_4);
    #endif
#endif    
    }
        
}

#define S5P_TVMAX_CTRLS 	ARRAY_SIZE(s5p_tvout)
/*
 *  Probe
 */

//I2C driver add 20100614  kyungrok
#if defined(CONFIG_MACH_S5PC110_P1)
extern struct i2c_driver SII9234_i2c_driver;
extern struct i2c_driver SII9234A_i2c_driver;
extern struct i2c_driver SII9234B_i2c_driver;
extern struct i2c_driver SII9234C_i2c_driver;
#endif
#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)
bool S5p_TvProbe_status = false;//Rajucm
#endif
static int __devinit s5p_tv_probe(struct platform_device *pdev)
{
	int 	irq_num;
	int 	ret;
	int 	i;
	int 	retval;

	s5ptv_status.dev_fb = &pdev->dev;

	__s5p_sdout_probe(pdev, 0);
	__s5p_vp_probe(pdev, 1);	
	__s5p_mixer_probe(pdev, 2);


#ifdef CONFIG_CPU_S5PC100	
	__s5p_hdmi_probe(pdev, 3);
	__s5p_tvclk_probe(pdev, 4);
#endif


#ifdef CONFIG_CPU_S5PV210
	tv_clk_get(pdev, &s5ptv_status);
	s5p_tv_clk_gate( true );
#endif
#ifdef CONFIG_CPU_S5PV210	
	__s5p_hdmi_probe(pdev, 3, 4);
	__s5p_hdcp_init( );
#endif	

#if defined(CONFIG_MACH_S5PC110_P1)
	retval = i2c_add_driver(&SII9234A_i2c_driver);
	if (retval != 0)
		printk("[MHL SII9234A] can't add i2c driver");	
	else
		printk("[MHL SII9234A] add i2c driver");

	retval = i2c_add_driver(&SII9234B_i2c_driver);
	if (retval != 0)
		printk("[MHL SII9234B] can't add i2c driver");	
	else
		printk("[MHL SII9234B] add i2c driver");

	retval = i2c_add_driver(&SII9234C_i2c_driver);
	if (retval != 0)
		printk("[MHL SII9234C] can't add i2c driver");	
	else
		printk("[MHL SII9234C] add i2c driver");
	
	retval = i2c_add_driver(&SII9234_i2c_driver);
	if (retval != 0)
		printk("[MHL SII9234] can't add i2c driver");	
	else
		printk("[MHL SII9234] add i2c driver");
#endif

#ifdef FIX_27M_UNSTABLE_ISSUE /* for smdkc100 pop */
	writel(0x1, S5PC1XX_GPA0_BASE + 0x56c);
#endif

#ifdef I2C_BASE
	/* for dev_dbg err. */
	spin_lock_init(&slock_hpd);

	/* for bh */
	INIT_WORK(&ws_hpd, (void *)set_ddc_port);
#endif
	/* check EINT init state */

#ifdef CONFIG_CPU_S5PC100
	s3c_gpio_cfgpin(S5PC1XX_GPH0(5), S3C_GPIO_SFN(2));
	s3c_gpio_setpull(S5PC1XX_GPH0(5), S3C_GPIO_PULL_UP);

	s5ptv_status.hpd_status = gpio_get_value(S5PC1XX_GPH0(5)) 
		? false:true;
#endif

#ifdef CONFIG_CPU_S5PV210
#ifdef CONFIG_HDMI_HPD
    s5ptv_status.hpd_status= s5p_hpd_get_state();
#else
    s5ptv_status.hpd_status= 1;//0;
#endif    
#endif
	dev_info(&pdev->dev, "hpd status is cable %s\n", 
		s5ptv_status.hpd_status ? "inserted":"removed");


	/* interrupt */
	TVOUT_IRQ_INIT(irq_num, ret, pdev, 0, out, __s5p_mixer_irq, "mixer");
	TVOUT_IRQ_INIT(irq_num, ret, pdev, 1, out_hdmi_irq, __s5p_hdmi_irq , "hdmi");
	TVOUT_IRQ_INIT(irq_num, ret, pdev, 2, out_tvenc_irq, s5p_tvenc_irq, "tvenc");
	
#ifdef CONFIG_CPU_S5PC100	
	TVOUT_IRQ_INIT(irq_num, ret, pdev, 3, out_hpd_irq, __s5p_hpd_irq, "hpd");
	set_irq_type(IRQ_EINT5, IRQ_TYPE_LEVEL_LOW);
#endif


	/* v4l2 video device registration */
	for (i = 0;i < S5P_TVMAX_CTRLS;i++) {
		s5ptv_status.video_dev[i] = &s5p_tvout[i];

		if (video_register_device(s5ptv_status.video_dev[i],
				VFL_TYPE_GRABBER, s5p_tvout[i].minor) != 0) {
				
			dev_err(&pdev->dev, 
				"Couldn't register tvout driver.\n");
			return 0;
		}
	}


#ifdef CONFIG_TV_FB
	mutex_init(&s5ptv_status.fb_lock);

	/* for default start up */
	_s5p_tv_if_init_param();

	s5ptv_status.tvout_param.disp_mode = TVOUT_720P_60;
	s5ptv_status.tvout_param.out_mode  = TVOUT_OUTPUT_HDMI;

#ifndef CONFIG_USER_ALLOC_TVOUT
	s5p_tv_clk_gate(true);
	tv_phy_power( true );
	_s5p_tv_if_set_disp();
#endif

	s5ptvfb_set_lcd_info(&s5ptv_status);

	/* prepare memory */
	if (s5ptvfb_alloc_framebuffer())
		goto err_alloc;

	if (s5ptvfb_register_framebuffer())
		goto err_alloc;
#ifndef CONFIG_USER_ALLOC_TVOUT
	s5ptvfb_display_on(&s5ptv_status);
#endif
#endif

	mutex_for_fo = (struct mutex *)kmalloc(sizeof(struct mutex), GFP_KERNEL);

	if (mutex_for_fo == NULL) {
		dev_err(&pdev->dev, 
			"failed to create mutex handle\n");
		goto out;
	}

#ifdef I2C_BASE
	mutex_for_i2c= (struct mutex *)kmalloc(sizeof(struct mutex), GFP_KERNEL);
	
	if (mutex_for_i2c == NULL) {
		dev_err(&pdev->dev, 
			"failed to create mutex handle\n");
		goto out;
	}
	mutex_init(mutex_for_i2c);
#endif
	mutex_init(mutex_for_fo);

#ifdef CONFIG_CPU_S5PV210
	/* added for phy cut off when boot up */
	clk_enable(s5ptv_status.i2c_phy_clk);
	__s5p_hdmi_phy_power(false);
	clk_disable(s5ptv_status.i2c_phy_clk);
	s5p_tv_clk_gate( false );
#endif
#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)
	S5p_TvProbe_status = true; //Rajucm
#endif
	return 0;

#ifdef CONFIG_TV_FB
err_alloc:
#endif
	
#ifdef CONFIG_CPU_S5PC100
out_hpd_irq:
	free_irq(IRQ_TVENC, pdev);
#endif

out_tvenc_irq:
	free_irq(IRQ_HDMI, pdev);

out_hdmi_irq:
	free_irq(IRQ_MIXER, pdev);

out:
	printk(KERN_ERR "not found (%d). \n", ret);

	return ret;
}

/*
 *  Remove
 */
static int s5p_tv_remove(struct platform_device *pdev)
{
	__s5p_hdmi_release(pdev);
	__s5p_sdout_release(pdev);
	__s5p_mixer_release(pdev);
	__s5p_vp_release(pdev);
#ifdef CONFIG_CPU_S5PC100	
	__s5p_tvclk_release(pdev);
#endif
#ifdef I2C_BASE
	i2c_del_driver(&hdcp_i2c_driver);
#endif

#if defined(CONFIG_MACH_S5PC110_P1)
	i2c_del_driver(&SII9234A_i2c_driver);
	i2c_del_driver(&SII9234B_i2c_driver);
	i2c_del_driver(&SII9234C_i2c_driver);
	i2c_del_driver(&SII9234_i2c_driver);
#endif
	
	clk_disable(s5ptv_status.tvenc_clk);
	clk_disable(s5ptv_status.vp_clk);
	clk_disable(s5ptv_status.mixer_clk);
	clk_disable(s5ptv_status.hdmi_clk);
	clk_disable(s5ptv_status.sclk_hdmi);
	clk_disable(s5ptv_status.sclk_mixer);
	clk_disable(s5ptv_status.sclk_tv);

	clk_put(s5ptv_status.tvenc_clk);
	clk_put(s5ptv_status.vp_clk);
	clk_put(s5ptv_status.mixer_clk);
	clk_put(s5ptv_status.hdmi_clk);
	clk_put(s5ptv_status.sclk_hdmi);
	clk_put(s5ptv_status.sclk_mixer);
	clk_put(s5ptv_status.sclk_tv);

	free_irq(IRQ_MIXER, pdev);
	free_irq(IRQ_HDMI, pdev);
	free_irq(IRQ_TVENC, pdev);
#ifdef CONFIG_CPU_S5PC100	
	free_irq(IRQ_EINT5, pdev);
#endif

	mutex_destroy(mutex_for_fo);
#ifdef I2C_BASE	
	mutex_destroy(mutex_for_i2c);
#endif

	return 0;
}


#ifdef CONFIG_PM
/*
 *  Suspend
 */
int s5p_tv_early_suspend(struct platform_device *dev, pm_message_t state)
{
    BASEPRINTK("(hpd_status = %d)++\n", s5ptv_status.hpd_status);

    mutex_lock(mutex_for_fo);
    s5ptv_status.suspend_status = true;

    if(!(s5ptv_status.hpd_status))
    {
	    mutex_unlock(mutex_for_fo);
	    return 0;
    }
    else
    {
	    /* video layer stop */
	    if ( s5ptv_status.vp_layer_enable ) {
		    _s5p_vlayer_stop();
		    s5ptv_status.vp_layer_enable = true;

	    }

	    /* grp0 layer stop */
	    if ( s5ptv_status.grp_layer_enable[0] ) {
		    _s5p_grp_stop(VM_GPR0_LAYER);
		    s5ptv_status.grp_layer_enable[VM_GPR0_LAYER] = true;
	    }

	    /* grp1 layer stop */
	    if ( s5ptv_status.grp_layer_enable[1] ) {
		    _s5p_grp_stop(VM_GPR1_LAYER);
		    s5ptv_status.grp_layer_enable[VM_GPR0_LAYER] = true;
	    }

	    /* tv off */
	    if ( s5ptv_status.tvout_output_enable ) {
		    _s5p_tv_if_stop();
		    s5ptv_status.tvout_output_enable = true;
		    s5ptv_status.tvout_param_available = true;
	    }

#ifdef CONFIG_PM_PWR_GATING
	    /* clk & power off */
	    s5p_tv_clk_gate( false );
	    if(s5ptv_status.tvout_param.out_mode == TVOUT_OUTPUT_HDMI || s5ptv_status.tvout_param.out_mode == TVOUT_OUTPUT_HDMI_RGB)
		    tv_phy_power( false );
#endif
#if 0
#ifdef CONFIG_CPU_FREQ
	    s5pc110_unlock_dvfs_high_level(DVFS_LOCK_TOKEN_4);
#endif
#endif    
    }

    mutex_unlock(mutex_for_fo);
    BASEPRINTK("()--\n");
    return 0;
}

/*
 *  Resume
 */
int s5p_tv_late_resume(struct platform_device *dev)
{
#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)
    char env_buf[120];
    char *envp[2];
    int env_offset = 0;
#endif
    BASEPRINTK("(hpd_status = %d)++\n", s5ptv_status.hpd_status);

    mutex_lock(mutex_for_fo);
    s5ptv_status.suspend_status = false;

    if(!(s5ptv_status.hpd_status))
    {
	    mutex_unlock(mutex_for_fo);
	    return 0;
    }
    else
    {
#if 0
    #ifdef CONFIG_CPU_FREQ
    #ifdef CONFIG_CPU_MAX_FREQ_1GHZ // 2010.3.9.
        s5pc110_lock_dvfs_high_level(DVFS_LOCK_TOKEN_4, 3);
    #else
        s5pc110_lock_dvfs_high_level(DVFS_LOCK_TOKEN_4, 2);
    #endif
    #endif
#endif
    #ifdef CONFIG_PM_PWR_GATING
        /* clk & power on */
        s5p_tv_clk_gate( true );
        if(s5ptv_status.tvout_param.out_mode == TVOUT_OUTPUT_HDMI || s5ptv_status.tvout_param.out_mode == TVOUT_OUTPUT_HDMI_RGB)
            tv_phy_power( true );
    #endif

	/* tv on */
	if ( s5ptv_status.tvout_output_enable ) {
		_s5p_tv_if_start();
	}

	/* video layer start */
	if ( s5ptv_status.vp_layer_enable )
		_s5p_vlayer_start();

	/* grp0 layer start */
	if ( s5ptv_status.grp_layer_enable[0] )
		_s5p_grp_start(VM_GPR0_LAYER);

	/* grp1 layer start */
	if ( s5ptv_status.grp_layer_enable[1] )
		_s5p_grp_start(VM_GPR1_LAYER);

#ifdef CONFIG_TV_FB
	if ( s5ptv_status.tvout_output_enable ) {
		s5ptvfb_display_on(&s5ptv_status);
		s5ptvfb_set_par(s5ptv_status.fb);
	}
#endif
#if defined (CONFIG_S5PC110_DEMPSEY_BOARD)
        sprintf(env_buf, "HDMI_STATE=online");
        envp[env_offset++] = env_buf;
        envp[env_offset] = NULL;
        kobject_uevent_env(&(s5p_tvout[0].dev.kobj), KOBJ_CHANGE, envp);
#endif
    }
    mutex_unlock(mutex_for_fo);
    BASEPRINTK("()--\n");
    return 0;
}
#else
#define s5p_tv_suspend NULL
#define s5p_tv_resume NULL
#endif

#ifdef CONFIG_PM
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend s5p_tv_early_suspend_desc = {
     .level = EARLY_SUSPEND_LEVEL_STOP_DRAWING,
     .suspend = s5p_tv_early_suspend,
     .resume = s5p_tv_late_resume,
};
#endif
#endif

static struct platform_driver s5p_tv_driver = {
	.probe		= s5p_tv_probe,
	.remove		= s5p_tv_remove,
	#ifdef CONFIG_PM
        #ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= s5p_tv_early_suspend,
	.resume		= s5p_tv_late_resume,
        #endif
	#else
	.suspend 	= NULL,
	.resume  	= NULL,
	#endif
	.driver		= {
		.name	= "s5p-tvout",
		.owner	= THIS_MODULE,
	},
};

static char banner[] __initdata = KERN_INFO "S5PC1XX TVOUT Driver, (c) 2009 Samsung Electronics\n";

int ldo8_status = 0;		//Subhransu20110304
int __init s5p_tv_init(void)
{
	int ret;

	printk(banner);

	Set_MAX8998_PM_REG(ELDO3, 1);    //HDMI power on temp code 20100614 kyungrok
	Set_MAX8998_PM_REG(ELDO8, 1);
	ldo8_status = 1;		//Subhransu20110304

	ret = platform_driver_register(&s5p_tv_driver);

	if (ret) {
		printk(KERN_ERR "Platform Device Register Failed %d\n", ret);
		return -1;
	}

#ifdef CONFIG_PM
#ifdef CONFIG_HAS_EARLYSUSPEND
    register_early_suspend(&s5p_tv_early_suspend_desc);
#endif
#endif
	return 0;
}

static void __exit s5p_tv_exit(void)
{
#ifdef CONFIG_PM
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&s5p_tv_early_suspend_desc);
#endif
#endif
	platform_driver_unregister(&s5p_tv_driver);
}

late_initcall(s5p_tv_init);
module_exit(s5p_tv_exit);

MODULE_AUTHOR("SangPil Moon");
MODULE_DESCRIPTION("SS5PC1XX TVOUT driver");
MODULE_LICENSE("GPL");
