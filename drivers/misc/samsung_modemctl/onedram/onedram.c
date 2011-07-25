/**
 * Samsung Virtual Network driver using OneDram device
 *
 * Copyright (C) 2010 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

//#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/sched.h>
#if defined (CONFIG_CP_CHIPSET_STE)
#include <linux/delay.h>
#include <mach/param.h>
#include <linux/gpio.h>
#endif
#include <linux/slab.h>
#include <mach/gpio-aries.h>
#include "onedram.h"

#define DRVNAME "onedram"

#define ONEDRAM_REG_OFFSET 0xFFF800
#define ONEDRAM_REG_SIZE 0x800

struct onedram_reg_mapped {
	u32 sem;
	u32 reserved1[7];
	u32 mailbox_AB;  // CP write, AP read
	u32 reserved2[7];
	u32 mailbox_BA;  // AP write, CP read
	u32 reserved3[23];
	u32 check_AB;    // can't read
	u32 reserved4[7];
	u32 check_BA;    // 0: CP read, 1: CP don't read
};

struct onedram_handler {
	struct list_head list;
	void *data;
	void (*handler)(u32, void *);
};

struct onedram_handler_head {
	struct list_head list;
	u32 len;
	spinlock_t lock;
};
static struct onedram_handler_head h_list;

static struct resource onedram_resource = {
	.name = DRVNAME,
	.start = 0,
	.end = -1,
	.flags = IORESOURCE_MEM,
};

struct onedram {
	struct class *class;
	struct device *dev;
	struct cdev cdev;
	dev_t devid;

	wait_queue_head_t waitq;
	struct fasync_struct *async_queue;
	u32 mailbox;

	unsigned long base;
	unsigned long size;
	void __iomem *mmio;

	int irq;

	struct completion comp;
#if defined (CONFIG_CP_CHIPSET_STE)
	struct completion comp_chkbit;
#endif
	atomic_t ref_sem;
	unsigned long flags;

	const struct attribute_group *group;

	struct onedram_reg_mapped *reg;
};
struct onedram *onedram;

#if defined (CONFIG_CP_CHIPSET_STE) 
//#define STE_SWWORKAROUND_MCLKREQ_TIMMING
#endif

#if defined (STE_SWWORKAROUND_MCLKREQ_TIMMING)
#include <asm/system.h>

#define STE_MODEM_INACTIVE_SLEEP     120 // Sleep XYZus (exact time TBD)
#define STE_MODEM_INACTIVE_MAX_SLEEP 170 // Max safe sleep ABCus (exact time TBD)
#define STE_MAX_RETRY                 50 // If we overslept too many times, wait with interrupts off

unsigned long ste_irq_flag;
static int onedram_writemailbox_BA_ste_workaround(struct onedram *od, u32 cmd);
#endif

#if 0 //defined (CONFIG_CP_CHIPSET_STE)
static inline int _get_checkbit(struct onedram *od);
#endif

#if defined (CONFIG_CP_CHIPSET_STE)
#if defined(CONFIG_KERNEL_DEBUG_SEC) 
#include <linux/kernel_sec_common.h>
#define ERRMSG "CP H/W watchdog Crash"
static char cp_errmsg[65];
static void _cp_watchdog_dump(void);
#else
#define _cp_watchdog_dump() do { } while(0)
#endif

#if defined(CONFIG_KERNEL_DEBUG_SEC) 
static void _cp_watchdog_dump(void)
{
	t_kernel_sec_mmu_info mmu_info;

	memset(cp_errmsg, 0, sizeof(cp_errmsg));

	strcpy(cp_errmsg, ERRMSG);

	printk("\nCP Dump Cause - %s\n", cp_errmsg);

	kernel_sec_set_upload_magic_number();
	kernel_sec_get_mmu_reg_dump(&mmu_info);
	kernel_sec_set_upload_cause(UPLOAD_CAUSE_CP_ERROR_FATAL);
	kernel_sec_hw_reset(false);
}
#endif
#endif
static DEFINE_SPINLOCK(onedram_lock);

static unsigned long hw_tmp; /* for hardware */
static inline int _read_sem(struct onedram *od);
static inline void _write_sem(struct onedram *od, int v);

static unsigned long recv_cnt;
static unsigned long send_cnt;
static ssize_t show_debug(struct device *d,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	struct onedram *od = dev_get_drvdata(d);

	if (!od)
		return 0;

	p += sprintf(p, "Semaphore: %d (%d)\n", _read_sem(od), (char)hw_tmp);
	p += sprintf(p, "Mailbox: %x\n", od->reg->mailbox_AB);
	p += sprintf(p, "Reference count: %d\n", atomic_read(&od->ref_sem));
	p += sprintf(p, "Mailbox send: %lu\n", send_cnt);
	p += sprintf(p, "Mailbox recv: %lu\n", recv_cnt);

	#if defined (CONFIG_CP_CHIPSET_STE)
	p += sprintf(p, "Onedram ap interrupt status : %d\n", gpio_get_value(GPIO_nINT_ONEDRAM_AP));
	#endif
	
	return p - buf;
}

static DEVICE_ATTR(debug, 0664, show_debug, NULL);

static struct attribute *onedram_attributes[] = {
	&dev_attr_debug.attr,
	NULL
};

static const struct attribute_group onedram_group = {
	.attrs = onedram_attributes,
};

static inline void _write_sem(struct onedram *od, int v)
{
	od->reg->sem = v;
	hw_tmp = od->reg->sem; /* for hardware */
}

static inline int _read_sem(struct onedram *od)
{
	return od->reg->sem;
}

#if defined (STE_SWWORKAROUND_MCLKREQ_TIMMING)
static int onedram_writemailbox_BA_ste_workaround(struct onedram *od, u32 cmd)
{
	int i = 0 ;
	int ModemActive = 0;
	
	unsigned long long TimeBeforeSleep = 0;
	unsigned long long TimeAfterSleep = 0;
	unsigned long long TimeSleep = 0;

	for ( i=0 ; i<STE_MAX_RETRY; i++)
	{
		local_irq_save(ste_irq_flag);
		#if 0 // Jinwoo Chang request test binary
			gpio_set_value(GPIO_PDA_ACTIVE, GPIO_LEVEL_LOW); 
		#endif
		ModemActive = gpio_get_value(GPIO_PHONE_ACTIVE); 
		if ( ModemActive )
		{
			//**dev_dbg(od->dev, "[%s] mailboxBA OK => ModemActive=[%d], i=[%d]\n",__func__, ModemActive, i);
			//printk("[%s] mailboxBA OK => ModemActive=[%d], i=[%d]\n",__func__, ModemActive, i);
			#if 0 // Jinwoo Chang request test binary
				gpio_set_value(GPIO_PDA_ACTIVE, GPIO_LEVEL_HIGH); 
			#endif
			//_get_checkbit(od);
			od->reg->mailbox_BA = cmd;
			local_irq_restore(ste_irq_flag);

			break;
		}		
		else if (i==STE_MAX_RETRY-1)
		{
			/* Last loop case */
			TimeBeforeSleep = cpu_clock(smp_processor_id());	 // in nano scale
			udelay(STE_MODEM_INACTIVE_SLEEP);				 // in micro scale
			TimeAfterSleep = cpu_clock(smp_processor_id());	  
			TimeSleep = TimeAfterSleep - TimeBeforeSleep;

			if ( TimeSleep > (STE_MODEM_INACTIVE_MAX_SLEEP * 1000) )
			{
				//**dev_dbg(od->dev, "[%s]Oversleep => ModemActive=[%d], i=[%d]\n",__func__, ModemActive, i);	 
				printk("[%s][Warning] Never reach here => ModemActive=[%d], i=[%d]\n",__func__, ModemActive, i); 
			}
			else
			{
				#if 0 // Jinwoo Chang request test binary
					gpio_set_value(GPIO_PDA_ACTIVE, GPIO_LEVEL_HIGH); 
				#endif 
				//_get_checkbit(od);
				od->reg->mailbox_BA = cmd;
				local_irq_restore(ste_irq_flag);
				dev_dbg(od->dev, "[%s] Proper sleep time => ModemActive=[%d], i=[%d]\n",__func__, ModemActive, i);
				//printk("[%s] Proper sleep time => ModemActive=[%d], i=[%d]\n",__func__, ModemActive, i); 
				break;
			}
		}
		else
		{
			TimeBeforeSleep = cpu_clock(smp_processor_id()); // in nano scale
			local_irq_restore(ste_irq_flag);
			
			udelay(STE_MODEM_INACTIVE_SLEEP); 			     // in micro scale

			local_irq_save(ste_irq_flag);
			TimeAfterSleep = cpu_clock(smp_processor_id()); 			
			TimeSleep = TimeAfterSleep - TimeBeforeSleep;
			
			if ( TimeSleep > (STE_MODEM_INACTIVE_MAX_SLEEP * 1000) )
			{
				// Oversleep				
				local_irq_restore(ste_irq_flag);
				
				dev_dbg(od->dev, "[%s]Oversleep => ModemActive=[%d], i=[%d]\n",__func__, ModemActive, i);				
				printk(KERN_DEBUG "[%s] Oversleep ModemActive=[%d], i=[%d]\n",__func__, ModemActive, i);	
				continue;
			}
			else
			{
				#if 0 // Jinwoo Chang request test binary
					gpio_set_value(GPIO_PDA_ACTIVE, GPIO_LEVEL_HIGH); 
				#endif 
				//_get_checkbit(od);
				od->reg->mailbox_BA = cmd;
				local_irq_restore(ste_irq_flag);
				
				dev_dbg(od->dev, "[%s] Proper sleep time => ModemActive=[%d], i=[%d]\n",__func__, ModemActive, i);
				//printk("[%s] Proper sleep time => ModemActive=[%d], i=[%d]\n",__func__, ModemActive, i);	
				break;
			}
		}		
	}	
	
	return 0;
}

	#if 0 // test
	{
		int i,j;long long t1, t2, t;

		printk("[0] ===================\n");
		for (j=0; j<10; j++) {		
			for (i=0; i<1000; i++) {
				t1 = cpu_clock(smp_processor_id()); 
				udelay(1000); 
				t2 = cpu_clock(smp_processor_id()); 
				t = t2 - t1;
			}
			printk("[%d] ===================\n", j);
		}	
	}
	/* result 
	[    0.419248] [0] ===================
	[    1.422023] [0] ===================
	[    2.423903] [1] ===================
	[    3.425780] [2] ===================
	[    4.427670] [3] ===================
	[    5.429546] [4] ===================
	[    6.431439] [5] ===================
	[    7.433317] [6] ===================
	[    8.435207] [7] ===================
	[    9.437083] [8] ===================
	[   10.438976] [9] ===================
	*/
	#endif

#endif

#if 0 //defined (CONFIG_CP_CHIPSET_STE)
static inline int _get_checkbit(struct onedram *od)
{
	int retry = 0;
	unsigned long timeleft;

	while(od->reg->check_BA)
	{	
		timeleft = wait_for_completion_timeout(&od->comp_chkbit, HZ/50);
		
		if (timeleft)
			break;

		if (!od->reg->check_BA)
			break;

		retry++;
		if (retry > 50 ) { /* time out after 1 seconds */
			dev_err(od->dev, "Get ChkBit time out\n");
			return -ETIMEDOUT;
		}
		dev_dbg(od->dev, "[%s]onedram: Waiting ChkBit \n",__func__);
	}
	
	return 0;
}
#endif

static inline int _send_cmd(struct onedram *od, u32 cmd)
{
#if defined (CONFIG_CP_CHIPSET_STE)
	u32 i=0;	
	int retry = 0;
	unsigned long timeleft;
#endif
	if (!od) {
		printk(KERN_ERR "[%s]onedram: Dev is NULL, but try to access\n",__func__);
		return -EFAULT;
	}

	if (!od->reg) {
		dev_err(od->dev, "Failed to send cmd, not initialized\n");
		return -EFAULT;
	}
#if defined (CONFIG_CP_CHIPSET_STE)
	while(od->reg->check_BA)
	{	
	//	timeleft = wait_for_completion_timeout(&od->comp_chkbit, HZ/50);
		udelay(1000);
		
	//	if (timeleft)
	//		break;

		if (!od->reg->check_BA)
			break;

		retry++;
		if (retry > 50 ) { /* time out after 1 seconds */
			dev_err(od->dev, "Get ChkBit time out\n");
			return -ETIMEDOUT;
		}
		dev_dbg(od->dev, "[%s]onedram: Waiting ChkBit \n",__func__);
	}
#endif
	dev_dbg(od->dev, "send %x\n", cmd);
	send_cnt++;

#if defined (STE_SWWORKAROUND_MCLKREQ_TIMMING)
	onedram_writemailbox_BA_ste_workaround( od, cmd );
#else
	od->reg->mailbox_BA = cmd;
#endif

	return 0;
}

static inline int _recv_cmd(struct onedram *od, u32 *cmd)
{
	if (!cmd)
		return -EINVAL;

	if (!od) {
		printk(KERN_ERR "[%s]onedram: Dev is NULL, but try to access\n",__func__);
		return -EFAULT;
	}

	if (!od->reg) {
		dev_err(od->dev, "Failed to read cmd, not initialized\n");
		return -EFAULT;
	}

	recv_cnt++;
	*cmd = od->reg->mailbox_AB;
	return 0;
}

static inline int _get_auth(struct onedram *od, u32 cmd)
{
	unsigned long timeleft;
	int retry = 0;

#if defined (CONFIG_CP_CHIPSET_STE)
	_send_cmd(od, cmd);
	while (!od->reg->sem) /* send cmd every 20m seconds */			
	{	
		timeleft = wait_for_completion_timeout(&od->comp, HZ/50);
		
	//	if (timeleft)
	//		break;

		if (_read_sem(od))
			break;

		retry++;
		if (retry > 50 ) { /* time out after 1 seconds */
			dev_err(od->dev, "get authority time out\n");
			return -ETIMEDOUT;
		}
		
		dev_dbg(od->dev, "[%s]onedram: Waiting Semaphore \n",__func__);
	}
	
	return 0;
#else
	/* send cmd every 20m seconds */
	while (1) {
		_send_cmd(od, cmd);

		timeleft = wait_for_completion_timeout(&od->comp, HZ/50);
#if 0		
		if (timeleft)
			break;
#endif
		if (_read_sem(od))
			break;

		retry++;
		if (retry > 50 ) { /* time out after 1 seconds */
			dev_err(od->dev, "get authority time out\n");
			return -ETIMEDOUT;
		}
	}

	return 0;
#endif
}

static int get_auth(struct onedram *od, u32 cmd)
{
	int r;

	if (!od) {
		printk(KERN_ERR "[%s]onedram: Dev is NULL, but try to access\n",__func__);
		return -EFAULT;
	}

	if (!od->reg) {
		dev_err(od->dev, "Failed to get authority\n");
		return -EFAULT;
	}

	atomic_inc(&od->ref_sem);

	if (_read_sem(od))
		return 0;

	if (cmd)
		r = _get_auth(od, cmd);
	else
		r = -EACCES;

	if (r < 0)
		atomic_dec(&od->ref_sem);

	return r;
}

static int put_auth(struct onedram *od, int release)
{
	if (!od) {
		printk(KERN_ERR "[%s]onedram: Dev is NULL, but try to access\n",__func__);
		return -EFAULT;
	}

	if (!od->reg) {
		dev_err(od->dev, "Failed to put authority\n");
		return -EFAULT;
	}
#if defined (CONFIG_CP_CHIPSET_STE)

	atomic_dec_and_test(&od->ref_sem);
	INIT_COMPLETION(od->comp);
	INIT_COMPLETION(od->comp_chkbit);

	return 0;

#else
	if (release)
		set_bit(0, &od->flags);

	if (atomic_dec_and_test(&od->ref_sem)
			&& test_and_clear_bit(0, &od->flags)) {
		INIT_COMPLETION(od->comp);
		_write_sem(od, 0);
		dev_dbg(od->dev, "rel_sem: %d\n", _read_sem(od));
	}

	return 0;
#endif
}

static int rel_sem(struct onedram *od)
{
	if (!od) {
		printk(KERN_ERR "[%s]onedram: Dev is NULL, but try to access\n",__func__);
		return -EFAULT;
	}

	if (!od->reg) {
		dev_err(od->dev, "Failed to put authority\n");
		return -EFAULT;
	}

	if (atomic_read(&od->ref_sem))
		return -EBUSY;

	INIT_COMPLETION(od->comp);

#if defined (CONFIG_CP_CHIPSET_STE)
	INIT_COMPLETION(od->comp_chkbit);
#endif
	clear_bit(0, &od->flags);
	_write_sem(od, 0);
	dev_dbg(od->dev, "rel_sem: %d\n", _read_sem(od));

	return 0;
}

int onedram_read_mailbox(u32 *mb)
{
	return _recv_cmd(onedram, mb);
}
EXPORT_SYMBOL(onedram_read_mailbox);

int onedram_write_mailbox(u32 mb)
{
	return _send_cmd(onedram, mb);
}
EXPORT_SYMBOL(onedram_write_mailbox);

void onedram_init_mailbox(void)
{
	int r = 0;
	/* flush mailbox before registering onedram irq */
	r = onedram->reg->mailbox_AB;

	/* Set nINT_ONEDRAM_CP to low */
	onedram->reg->mailbox_BA=0x0;
}
EXPORT_SYMBOL(onedram_init_mailbox);

int onedram_get_auth(u32 cmd)
{
	return get_auth(onedram, cmd);
}
EXPORT_SYMBOL(onedram_get_auth);

int onedram_put_auth(int release)
{
	return put_auth(onedram, release);
}
EXPORT_SYMBOL(onedram_put_auth);

int onedram_rel_sem(void)
{
	return rel_sem(onedram);
}
EXPORT_SYMBOL(onedram_rel_sem);

int onedram_read_sem(void)
{
	return _read_sem(onedram);
}
EXPORT_SYMBOL(onedram_read_sem);

void onedram_get_vbase(void** vbase)
{
	*vbase = (void*)onedram->mmio;
}
EXPORT_SYMBOL(onedram_get_vbase);

static unsigned long long old_clock;
static u32 old_mailbox;

static irqreturn_t onedram_irq_handler(int irq, void *data)
{
	struct onedram *od = (struct onedram *)data;
	struct list_head *l;
	unsigned long flags;
	int r;
	u32 mailbox;

	r = onedram_read_mailbox(&mailbox);
	if (r)
		return IRQ_HANDLED;
#if defined (CONFIG_CP_CHIPSET_STE)
	if (old_mailbox == mailbox &&
			old_clock + 100000 > cpu_clock(smp_processor_id()))
		return IRQ_HANDLED;

//	dev_dbg(od->dev, "[%d] recv %x\n", _read_sem(od), mailbox);
#else
//	if (old_mailbox == mailbox &&
//			old_clock + 100000 > cpu_clock(smp_processor_id()))
//		return IRQ_HANDLED;

	dev_dbg(od->dev, "[%d] recv %x\n", _read_sem(od), mailbox);
#endif
	hw_tmp = _read_sem(od); /* for hardware */

	if (h_list.len) {
		spin_lock_irqsave(&h_list.lock, flags);
		list_for_each(l, &h_list.list) {
			struct onedram_handler *h =
				list_entry(l, struct onedram_handler, list);

			if (h->handler)
				h->handler(mailbox, h->data);
		}
		spin_unlock_irqrestore(&h_list.lock, flags);

		spin_lock(&onedram_lock);
		od->mailbox = mailbox;
		spin_unlock(&onedram_lock);
	} else {
		od->mailbox = mailbox;
	}

	if (_read_sem(od))
		complete_all(&od->comp);

#if defined (CONFIG_CP_CHIPSET_STE)
	if (!od->reg->check_BA)
		complete_all(&od->comp_chkbit);
#endif
	wake_up_interruptible(&od->waitq);
	kill_fasync(&od->async_queue, SIGIO, POLL_IN);
	
#if defined (CONFIG_CP_CHIPSET_STE)
	old_clock = cpu_clock(smp_processor_id());
	old_mailbox = mailbox;
#else
//	old_clock = cpu_clock(smp_processor_id());
//	old_mailbox = mailbox;
#endif
	return IRQ_HANDLED;
}

static void onedram_vm_close(struct vm_area_struct *vma)
{
	struct onedram *od = vma->vm_private_data;
	unsigned long offset;
	unsigned long size;

	put_auth(od, 0);

	offset = (vma->vm_pgoff << PAGE_SHIFT) - od->base;
	size = vma->vm_end - vma->vm_start;
	dev_dbg(od->dev, "Rel region: 0x%08lx 0x%08lx\n", offset, size);
	onedram_release_region(offset, size);
}

static struct vm_operations_struct onedram_vm_ops = {
	.close = onedram_vm_close,
};

static int onedram_open(struct inode *inode, struct file *filp)
{
	struct cdev *cdev = inode->i_cdev;
	struct onedram *od = container_of(cdev, struct onedram, cdev);

	filp->private_data = od;
	return 0;
}

static int onedram_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	return 0;
}

static unsigned int onedram_poll(struct file *filp, poll_table *wait)
{
	struct onedram *od;
	u32 data;

	od = filp->private_data;

	poll_wait(filp, &od->waitq, wait);

	spin_lock_irq(&onedram_lock);
	data = od->mailbox;
	spin_unlock_irq(&onedram_lock);

	if (data)
		return POLLIN | POLLRDNORM;

	return 0;
}

static ssize_t onedram_read(struct file *filp, char __user *buf,
		size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	u32 data;
	ssize_t retval;
	struct onedram *od;

	od = filp->private_data;

	if (count < sizeof(u32))
		return -EINVAL;

	add_wait_queue(&od->waitq, &wait);

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);

		spin_lock_irq(&onedram_lock);
		data = od->mailbox;
		od->mailbox = 0;
		spin_unlock_irq(&onedram_lock);

		if (data)
			break;
		else if (filp->f_flags & O_NONBLOCK) {
			retval = -EAGAIN;
			goto out;
		} else if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			goto out;
		}
		schedule();
	}

	#if defined (CONFIG_CP_CHIPSET_STE)
	#if defined(CONFIG_KERNEL_DEBUG_SEC) 
	if(data == 0xabcd00c9)
		_cp_watchdog_dump();
	#endif
	#endif
	retval = put_user(data, (u32 __user *)buf);
	if (!retval)
		retval = sizeof(u32);
out:
	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&od->waitq, &wait);

	return retval;
}

static ssize_t onedram_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	struct onedram *od;

	od = filp->private_data;

	if (count) {
		u32 data;

		if (get_user(data, (u32 __user *)buf))
			return -EFAULT;

		_send_cmd(od, data);
	}

	return count;
}

static int onedram_fasync(int fd, struct file *filp, int on)
{
	struct onedram *od;

	od = filp->private_data;

	return fasync_helper(fd, filp, on, &od->async_queue);
}

static int onedram_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int r;
	struct onedram *od;
	unsigned long size;
	unsigned long pfn;
	unsigned long offset;
	struct resource *res;

	od = filp->private_data;
	if (!od || !vma)
		return -EFAULT;

	atomic_inc(&od->ref_sem);
	if (!_read_sem(od)) {
		atomic_dec(&od->ref_sem);
		return -EPERM;
	}

	size = vma->vm_end - vma->vm_start;
	offset = vma->vm_pgoff << PAGE_SHIFT;
	if (size > od->size - PAGE_ALIGN(ONEDRAM_REG_SIZE) - offset)
		return -EINVAL;

	dev_dbg(od->dev, "Req region: 0x%08lx 0x%08lx\n", offset, size);
	res = onedram_request_region(offset, size, "mmap");
	if (!res)
		return -EBUSY;

	pfn = __phys_to_pfn(od->base + offset);
	r = remap_pfn_range(vma, vma->vm_start, pfn,
			size,
			vma->vm_page_prot);
	if (r)
		return -EAGAIN;

	vma->vm_ops = &onedram_vm_ops;
	vma->vm_private_data = od;
	return 0;
}

#if defined (CONFIG_CP_CHIPSET_STE)
unsigned int KeyValueWhenPowerUp = 0x0;
extern u8 FSA9480_Get_JIG_UART_On_Status(void);
#endif

static int onedram_ioctl(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct cdev *cdev = inode->i_cdev;
	struct onedram *od = container_of(cdev, struct onedram, cdev);
	int r;

	switch (cmd) {
	case ONEDRAM_GET_AUTH:
		r = get_auth(od, arg);
		break;
	case ONEDRAM_PUT_AUTH:
		r = put_auth(od, 0);
		break;
	case ONEDRAM_REL_SEM:
		r = rel_sem(od);
		break;
#if defined (CONFIG_CP_CHIPSET_STE)
	case ONEDRAM_GET_ITP:
	{
	#if 0

		/* 1) ITP mode by key press. */
		if ( sec_get_param_value )
		{
			sec_get_param_value(__PARAM_INT_13, &KeyValueWhenPowerUp);						
		}
		printk("[ITP Mode] 1) Key pressing or 2) JIG \n");
		printk("KeyValueWhenPowerUp = [0x%x], func=[0x%x]\n", KeyValueWhenPowerUp, sec_get_param_value);
#if (defined CONFIG_S5PC110_HAWK_BOARD) || (defined CONFIG_S5PC110_SIDEKICK_BOARD)
	if ( KeyValueWhenPowerUp == 0x16d) /* Device powered up by volume down + Home key + Power key*/
#elif defined (CONFIG_S5PC110_VIBRANTPLUS_BOARD)
	if ( KeyValueWhenPowerUp == 0x164) /* Device powered up by volume down + Power key*/
#else
	if ( KeyValueWhenPowerUp == 0x4) /* Device powered up by volume down + Power key*/
#endif
		{
			r = 0x00000001;
			memcpy(onedram_resource.start+36, &r, 4);			
			printk("Get into M5720 ITP Branch boot. (By key press) \n");
		}
		else
		{
			r = 0x00000000;
			memcpy(onedram_resource.start+36, &r, 4);			
			printk("Get into M5720 Normal modem boot. (By key press) \n");
		}

		/* 2) ITP mode by JIG */
		/*
		if ( FSA9480_Get_JIG_UART_On_Status() )
		{
			r = 0x00000001;
			memcpy(onedram_resource.start+36, &r, 4);	
			printk("Get into M5720 ITP Branch boot. (By JIG) \n");
		}
		else
		{
			r = 0x00000000;
			memcpy(onedram_resource.start+36, &r, 4);		
			printk("Get into M5720 Normal modem boot. (By JIG) \n");
		}
		*/

#else
		r = 0x00000000;
		memcpy(onedram_resource.start+36, &r, 4);			
		printk("Get into M5720 Normal modem boot. (By key press) \n");

#endif 	
		break;
	}
	#endif

	default:
		r = -ENOIOCTLCMD;
		break;
	}

	return r;
}

static const struct file_operations onedram_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = onedram_read,
	.write = onedram_write,
	.poll = onedram_poll,
	.fasync = onedram_fasync,
	.open = onedram_open,
	.release = onedram_release,
	.mmap = onedram_mmap,
	.ioctl = onedram_ioctl,
};

static int _register_chrdev(struct onedram *od)
{
	int r;
	dev_t devid;

	od->class = class_create(THIS_MODULE, DRVNAME);
	if (IS_ERR(od->class)) {
		r = PTR_ERR(od->class);
		od->class = NULL;
		return r;
	}

	r = alloc_chrdev_region(&devid, 0, 1, DRVNAME);
	if (r)
		return r;

	cdev_init(&od->cdev, &onedram_fops);

	r = cdev_add(&od->cdev, devid, 1);
	if (r) {
		unregister_chrdev_region(devid, 1);
		return r;
	}
	od->devid = devid;

	od->dev = device_create(od->class, NULL, od->devid, od, DRVNAME);
	if (IS_ERR(od->dev)) {
		r = PTR_ERR(od->dev);
		od->dev = NULL;
		return r;
	}
	dev_set_drvdata(od->dev, od);

	return 0;
}

static inline int _request_mem(struct onedram *od, struct platform_device *pdev)
{
	struct resource *reso;

	reso = request_mem_region(od->base, od->size, DRVNAME);
	if (!reso) {
		dev_err(&pdev->dev, "Failed to request the mem region:"
				" 0x%08lx (%lu)\n", od->base, od->size);
		return -EBUSY;
	}

	od->mmio = ioremap_nocache(od->base, od->size);
	if (!od->mmio) {
		release_mem_region(od->base, od->size);
		dev_err(&pdev->dev, "Failed to ioremap: 0x%08lx (%lu)\n",
				od->base, od->size);
		return -EBUSY;
	}

	od->reg = (struct onedram_reg_mapped *)(
			(char *)od->mmio + ONEDRAM_REG_OFFSET);
	dev_dbg(&pdev->dev, "Onedram semaphore: %d\n", _read_sem(od));

	onedram_resource.start = (resource_size_t)od->mmio;
	onedram_resource.end = (resource_size_t)od->mmio + od->size - 1;

	return 0;
}

static void _release(struct onedram *od)
{
	if (!od)
		return;

	if (od->irq)
		free_irq(od->irq, od);

	if (od->group)
		sysfs_remove_group(&od->dev->kobj, od->group);

	if (od->dev)
		device_destroy(od->class, od->devid);

	if (od->devid) {
		cdev_del(&od->cdev);
		unregister_chrdev_region(od->devid, 1);
	}

	if (od->mmio) {
		od->reg = NULL;
		iounmap(od->mmio);
		release_mem_region(od->base, od->size);
		onedram_resource.start = 0;
		onedram_resource.end = -1;
	}

	if (od->class)
		class_destroy(od->class);

	kfree(od);
}

struct resource* onedram_request_region(resource_size_t start,
		resource_size_t n, const char *name)
{
	struct resource *res;

	start += onedram_resource.start;
	res = __request_region(&onedram_resource, start, n, name, 0);
	if (!res)
		return NULL;

	return res;
}
EXPORT_SYMBOL(onedram_request_region);

void onedram_release_region(resource_size_t start, resource_size_t n)
{
	start += onedram_resource.start;
	__release_region(&onedram_resource, start, n);
}
EXPORT_SYMBOL(onedram_release_region);

int onedram_register_handler(void (*handler)(u32, void *), void *data)
{
	unsigned long flags;
	struct onedram_handler *hd;

	if (!handler)
		return -EINVAL;

	hd = kzalloc(sizeof(struct onedram_handler), GFP_KERNEL);
	if (!hd)
		return -ENOMEM;

	hd->data = data;
	hd->handler = handler;

	spin_lock_irqsave(&h_list.lock, flags);
	list_add_tail(&hd->list, &h_list.list);
	h_list.len++;
	spin_unlock_irqrestore(&h_list.lock, flags);

	return 0;
}
EXPORT_SYMBOL(onedram_register_handler);

int onedram_unregister_handler(void (*handler)(u32, void *))
{
	unsigned long flags;
	struct list_head *l, *tmp;

	if (!handler)
		return -EINVAL;

	spin_lock_irqsave(&h_list.lock, flags);
	list_for_each_safe(l, tmp, &h_list.list) {
		struct onedram_handler *hd =
			list_entry(l, struct onedram_handler, list);

		if (hd->handler == handler) {
			list_del(&hd->list);
			h_list.len--;
			kfree(hd);
		}
	}
	spin_unlock_irqrestore(&h_list.lock, flags);

	return 0;
}
EXPORT_SYMBOL(onedram_unregister_handler);

static void _unregister_all_handlers(void)
{
	unsigned long flags;
	struct list_head *l, *tmp;

	spin_lock_irqsave(&h_list.lock, flags);
	list_for_each_safe(l, tmp, &h_list.list) {
		struct onedram_handler *hd =
			list_entry(l, struct onedram_handler, list);

		list_del(&hd->list);
		h_list.len--;
		kfree(hd);
	}
	spin_unlock_irqrestore(&h_list.lock, flags);
}

static void _init_data(struct onedram *od)
{
	init_completion(&od->comp);
#if defined (CONFIG_CP_CHIPSET_STE)
	init_completion(&od->comp_chkbit);
#endif
	atomic_set(&od->ref_sem, 0);
	INIT_LIST_HEAD(&h_list.list);
	spin_lock_init(&h_list.lock);
	h_list.len = 0;
	init_waitqueue_head(&od->waitq);
}

static int __devinit onedram_probe(struct platform_device *pdev)
{
	int r;
	int irq;
	struct onedram *od = NULL;
	struct onedram_platform_data *pdata;
	struct resource *res;

	printk("[%s]\n",__func__);
	pdata = pdev->dev.platform_data;
	if (!pdata || !pdata->cfg_gpio) {
		dev_err(&pdev->dev, "No platform data\n");
		r = -EINVAL;
		goto err;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get irq number\n");
		r = -EINVAL;
		goto err;
	}
	irq = res->start;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get mem region\n");
		r = -EINVAL;
		goto err;
	}

	od = kzalloc(sizeof(struct onedram), GFP_KERNEL);
	if (!od) {
		dev_err(&pdev->dev, "failed to allocate device\n");
		r = -ENOMEM;
		goto err;
	}
	onedram = od;
	
	dev_dbg(&pdev->dev, "Onedram dev: %p\n", od);

	od->base = res->start;
	od->size = resource_size(res);
	r = _request_mem(od, pdev);
	if (r)
		goto err;

	/* init mailbox state before registering irq handler */
	onedram_init_mailbox();

	_init_data(od);

	pdata->cfg_gpio();

	r = request_irq(irq, onedram_irq_handler,
			IRQF_TRIGGER_LOW, "onedram", od);
	if (r) {
		dev_err(&pdev->dev, "Failed to allocate an interrupt: %d\n",
				irq);
		goto err;
	}
	od->irq = irq;

	enable_irq_wake(od->irq);

	r = _register_chrdev(od);
	if (r) {
		dev_err(&pdev->dev, "Failed to register chrdev\n");
		goto err;
	}

	r = sysfs_create_group(&od->dev->kobj, &onedram_group);
	if (r) {
		dev_err(&pdev->dev, "Failed to create sysfs files\n");
		goto err;
	}
	od->group = &onedram_group;

	platform_set_drvdata(pdev, od);

	return 0;

err:
	_release(od);
	return r;
}

static int __devexit onedram_remove(struct platform_device *pdev)
{
	struct onedram *od = platform_get_drvdata(pdev);

	/* TODO: need onedram_resource clean? */
	_unregister_all_handlers();
	platform_set_drvdata(pdev, NULL);
	onedram = NULL;
	_release(od);

	return 0;
}

#ifdef CONFIG_PM
static int onedram_suspend(struct platform_device *pdev, pm_message_t state)
{
//	struct onedram *od = platform_get_drvdata(pdev);

	return 0;
}

static int onedram_resume(struct platform_device *pdev)
{
//	struct onedram *od = platform_get_drvdata(pdev);

	return 0;
}
#else
#  define onedram_suspend NULL
#  define onedram_resume NULL
#endif


static struct platform_driver onedram_driver = {
	.probe = onedram_probe,
	.remove = __devexit_p(onedram_remove),
	.suspend = onedram_suspend,
	.resume = onedram_resume,
	.driver = {
		.name = DRVNAME,
	},
};

static int __init onedram_init(void)
{
	printk("[%s]\n",__func__);
	return platform_driver_register(&onedram_driver);
}

static void __exit onedram_exit(void)
{
	platform_driver_unregister(&onedram_driver);
}

module_init(onedram_init);
module_exit(onedram_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Suchang Woo <suchang.woo@samsung.com>");
MODULE_DESCRIPTION("Onedram driver");
