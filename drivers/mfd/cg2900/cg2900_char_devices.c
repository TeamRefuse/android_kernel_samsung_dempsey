/*
 * drivers/mfd/cg2900/cg2900_char_devices.c
 *
 * Copyright (C) ST-Ericsson SA 2010
 * Authors:
 * Par-Gunnar Hjalmdahl (par-gunnar.p.hjalmdahl@stericsson.com) for ST-Ericsson.
 * Henrik Possung (henrik.possung@stericsson.com) for ST-Ericsson.
 * Josef Kindberg (josef.kindberg@stericsson.com) for ST-Ericsson.
 * Dariusz Szymszak (dariusz.xd.szymczak@stericsson.com) for ST-Ericsson.
 * Kjell Andersson (kjell.k.andersson@stericsson.com) for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 *
 * Linux Bluetooth HCI H:4 Driver for ST-Ericsson connectivity controller.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/skbuff.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>

#include <linux/mfd/cg2900.h>
#include <mach/cg2900_devices.h>
#include "cg2900_core.h"
#include "cg2900_debug.h"

/* Ioctls */
#define CG2900_CHAR_DEV_IOCTL_RESET		_IOW('U', 210, int)
#define CG2900_CHAR_DEV_IOCTL_CHECK4RESET	_IOR('U', 212, int)
#define CG2900_CHAR_DEV_IOCTL_GET_REVISION	_IOR('U', 213, int)
#define CG2900_CHAR_DEV_IOCTL_GET_SUB_VER	_IOR('U', 214, int)
#ifdef CG2900_GPS_DSM_SUPPORT
#define CG2900_CHAR_DEV_IOCTL_ENTER_DSM	_IOR('U', 215, int)
#define CG2900_CHAR_DEV_IOCTL_EXIT_DSM	_IOR('U', 216, int)
#endif /* CG2900_GPS_DSM_SUPPORT */

#define CG2900_CHAR_DEV_IOCTL_EVENT_RESET	1
#define CG2900_CHAR_DEV_IOCTL_EVENT_CLOSED	2
#define CG2900_CHAR_DEV_MAX_RETRIAL 3
/* Internal type definitions */

/**
 * enum char_reset_state - Reset state.
 * @CG2900_CHAR_IDLE:	Idle state.
 * @CG2900_CHAR_RESET:	Reset state.
 */
enum char_reset_state {
	CG2900_CHAR_IDLE,
	CG2900_CHAR_RESET
};

/**
 * struct char_dev_user - Stores device information.
 * @cpd_dev:		Registered CPD device.
 * @cdev_device:	Registered device struct.
 * @cdev:		Registered Char Device.
 * @devt:		Assigned dev_t.
 * @name:		Name of device.
 * @rx_queue:		Data queue.
 * @rx_wait_queue:	Wait queue.
 * @reset_wait_queue:	Reset Wait queue.
 * @reset_state:	Reset state.
 * @read_mutex:		Read mutex.
 * @write_mutex:	Write mutex.
 */
struct char_dev_user {
	struct cg2900_device	*cpd_dev;
	struct device		*cdev_device;
	struct cdev		cdev;
	dev_t			devt;
	char			*name;
	struct sk_buff_head	rx_queue;
	wait_queue_head_t	rx_wait_queue;
	wait_queue_head_t	reset_wait_queue;
	enum char_reset_state	reset_state;
	struct mutex		read_mutex;
	struct mutex		write_mutex;
};

/**
 * struct char_info - Stores all current users.
 * @bt_cmd_user:	BT command channel user.
 * @bt_acl_user:	BT ACL channel user.
 * @bt_evt_user:	BT event channel user.
 * @fm_radio_user:	FM radio channel user.
 * @gnss_user:		GNSS channel user.
 * @debug_user:		Debug channel user.
 * @ste_tools_user:	ST-E tools channel user.
 * @hci_logger_user:	HCI logger channel user.
 * @us_ctrl_user:	User space control channel user.
 * @bt_audio_user:	BT audio command channel user.
 * @fm_audio_user:	FM audio command channel user.
 * @core_user:		Core user.
 * @open_mutex:		Open mutex (used for both open and release).
 */
struct char_info {
	struct char_dev_user	bt_cmd_user;
	struct char_dev_user	bt_acl_user;
	struct char_dev_user	bt_evt_user;
	struct char_dev_user	fm_radio_user;
	struct char_dev_user	gnss_user;
	struct char_dev_user	debug_user;
	struct char_dev_user	ste_tools_user;
	struct char_dev_user	hci_logger_user;
	struct char_dev_user	us_ctrl_user;
	struct char_dev_user	bt_audio_user;
	struct char_dev_user	fm_audio_user;
	struct char_dev_user	core_user;
	struct mutex		open_mutex;
};

/* Internal variable declarations */

/*
 * char_info - Main information object for char devices.
 */
static struct char_info *char_info;

/* ST-Ericsson CG2900 driver callbacks */

/**
 * char_dev_read_cb() - Handle data received from controller.
 * @dev:	Device receiving data.
 * @skb:	Buffer with data coming from controller.
 *
 * The char_dev_read_cb() function handles data received from STE-CG2900 driver.
 */
static void char_dev_read_cb(struct cg2900_device *dev, struct sk_buff *skb)
{
	struct char_dev_user *char_dev = (struct char_dev_user *)dev->user_data;

	CG2900_INFO("CharDev: char_dev_read_cb");

	if (!char_dev) {
		CG2900_ERR("No char dev! Exiting");
		kfree_skb(skb);
		return;
	}

	skb_queue_tail(&char_dev->rx_queue, skb);

	wake_up_interruptible(&char_dev->rx_wait_queue);
}

/**
 * char_dev_reset_cb() - Handle reset from controller.
 * @dev:	Device resetting.
 *
 * The char_dev_reset_cb() function handles reset from
 * connectivity protocol driver.
 */
static void char_dev_reset_cb(struct cg2900_device *dev)
{
	struct char_dev_user *char_dev = (struct char_dev_user *)dev->user_data;

	CG2900_INFO("CharDev: char_dev_reset_cb");

	if (!char_dev) {
		CG2900_ERR("char_dev == NULL");
		return;
	}

	char_dev->reset_state = CG2900_CHAR_RESET;
	/* The device will be freed by CPD when this function is finished. */
	char_dev->cpd_dev = NULL;

	wake_up_interruptible(&char_dev->rx_wait_queue);
	wake_up_interruptible(&char_dev->reset_wait_queue);
}

/*
 * struct char_cb - Callback structure for CG2900 user.
 * @read_cb:	Callback function called when data is received from the
 *		CG2900 driver.
 * @reset_cb:	Callback function called when the controller has been reset.
 */
static struct cg2900_callbacks char_cb = {
	.read_cb = char_dev_read_cb,
	.reset_cb = char_dev_reset_cb
};

/* File operation functions */

/**
 * char_dev_open() - Open char device.
 * @inode:	Device driver information.
 * @filp:	Pointer to the file struct.
 *
 * The char_dev_open() function opens the char device.
 *
 * Returns:
 *   0 if there is no error.
 *   -EACCES if device was already registered to driver or if registration
 *   failed.
 */
static int char_dev_open(struct inode *inode, struct file *filp)
{
	int err = 0;
	int recover_max_trial = 0; /* ++ daniel - port fail recovery */
	struct char_dev_user *dev;

	mutex_lock(&char_info->open_mutex);
	dev = container_of(inode->i_cdev, struct char_dev_user, cdev);
	filp->private_data = dev;

	CG2900_INFO("CharDev: char_dev_open %s", dev->name);

	if (dev->cpd_dev) {
		CG2900_ERR("Device already registered to CPD");
		err = -EACCES;
		goto error_handling;
	}
	/* First initiate wait queues for this device. */
	init_waitqueue_head(&dev->rx_wait_queue);
	init_waitqueue_head(&dev->reset_wait_queue);

	dev->reset_state = CG2900_CHAR_IDLE;

   while(1)
   { /* ++ daniel - port fail recovery */
   	/* Register to CG2900 Driver */
   	dev->cpd_dev = cg2900_register_user(dev->name, &char_cb);
   	if (dev->cpd_dev)
   	{ /* ++ daniel - port fail recovery */
   	   CG2900_DBG("Register Success");
   		dev->cpd_dev->user_data = dev;
   		recover_max_trial = 0;
   		goto error_handling; /* ++ daniel - port fail recovery */
   	} /* ++ daniel - port fail recovery */
   	else {
   		CG2900_ERR("Couldn't register to CPD for H:4 channel %s",
   			   dev->name); 
   		err = -EACCES;
/* ++ daniel - port fail recovery */   	
   		if(recover_max_trial > 5) 
   		{
   		   dev->cpd_dev = NULL;
   		   recover_max_trial = 0;     
   		   goto error_handling;
   		}//BUG_ON(1);
   		else {
   		   err = 0;
   		   dev->cpd_dev = NULL;
   		   recover_max_trial++;
   		   CG2900_ERR("recover_max_trial = %d", recover_max_trial);
   		   printk(KERN_ERR "recover_max_trial = %d", recover_max_trial);
   		   schedule_timeout_interruptible(msecs_to_jiffies(100));
   		}   		
/* -- daniel - port fail recovery */    		
   	}
   } 

error_handling:
	mutex_unlock(&char_info->open_mutex);
	return err;
}

/**
 * char_dev_release() - Release char device.
 * @inode:	Device driver information.
 * @filp:	Pointer to the file struct.
 *
 * The char_dev_release() function release the char device.
 *
 * Returns:
 *   0 if there is no error.
 *   -EBADF if NULL pointer was supplied in private data.
 */
static int char_dev_release(struct inode *inode, struct file *filp)
{
	int err = 0;
	struct char_dev_user *dev = (struct char_dev_user *)filp->private_data;

	CG2900_INFO("CharDev: char_dev_release");

	if (!dev) {
		CG2900_ERR("Calling with NULL pointer");
		return -EBADF;
	}

	mutex_lock(&char_info->open_mutex);
	mutex_lock(&dev->read_mutex);
	mutex_lock(&dev->write_mutex);

	if (dev->reset_state == CG2900_CHAR_IDLE)
		cg2900_deregister_user(dev->cpd_dev);

	dev->cpd_dev = NULL;
	filp->private_data = NULL;
	wake_up_interruptible(&dev->rx_wait_queue);
	wake_up_interruptible(&dev->reset_wait_queue);

	mutex_unlock(&dev->write_mutex);
	mutex_unlock(&dev->read_mutex);
	mutex_unlock(&char_info->open_mutex);

	return err;
}

/**
 * char_dev_read() - Queue and copy buffer to user.
 * @filp:	Pointer to the file struct.
 * @buf:	Received buffer.
 * @count:	Size of buffer.
 * @f_pos:	Position in buffer.
 *
 * The char_dev_read() function queues and copy the received buffer to
 * the user space char device. If no data is available this function will block.
 *
 * Returns:
 *   Bytes successfully read (could be 0).
 *   -EBADF if NULL pointer was supplied in private data.
 *   -EFAULT if copy_to_user fails.
 *   Error codes from wait_event_interruptible.
 */
static ssize_t char_dev_read(struct file *filp, char __user *buf, size_t count,
			     loff_t *f_pos)
{
	struct char_dev_user *dev = (struct char_dev_user *)filp->private_data;
	struct sk_buff *skb;
	int bytes_to_copy;
	int err = 0;

	CG2900_INFO("CharDev: char_dev_read");

	if (!dev) {
		CG2900_ERR("Calling with NULL pointer");
		return -EBADF;
	}
	mutex_lock(&dev->read_mutex);

	if (skb_queue_empty(&dev->rx_queue)) {
		err = wait_event_interruptible(dev->rx_wait_queue,
				(!(skb_queue_empty(&dev->rx_queue))) ||
				(CG2900_CHAR_RESET == dev->reset_state) ||
				(dev->cpd_dev == NULL));
		if (err) {
			CG2900_ERR("Failed to wait for event");
			goto error_handling;
		}
	}

	if (!dev->cpd_dev) {
		CG2900_DBG("cpd_dev is empty - return with negative bytes");
		err = -EBADF;
		goto error_handling;
	}

	skb = skb_dequeue(&dev->rx_queue);
	if (!skb) {
		CG2900_DBG("skb queue is empty - return with zero bytes");
		bytes_to_copy = 0;
		goto finished;
	}

	bytes_to_copy = min(count, skb->len);

	err = copy_to_user(buf, skb->data, bytes_to_copy);
	if (err) {
		skb_queue_head(&dev->rx_queue, skb);
		err = -EFAULT;
		goto error_handling;
	}

	skb_pull(skb, bytes_to_copy);

	if (skb->len > 0)
	{
		skb_queue_head(&dev->rx_queue, skb);
   }		
	else
		kfree_skb(skb);

	goto finished;

error_handling:
	mutex_unlock(&dev->read_mutex);
	return (ssize_t)err;
finished:
	mutex_unlock(&dev->read_mutex);
	return bytes_to_copy;
}

/**
 * char_dev_write() - Copy buffer from user and write to CG2900 driver.
 * @filp:	Pointer to the file struct.
 * @buf:	Write buffer.
 * @count:	Size of the buffer write.
 * @f_pos:	Position of buffer.
 *
 * Returns:
 *   Bytes successfully written (could be 0).
 *   -EBADF if NULL pointer was supplied in private data.
 *   -EFAULT if copy_from_user fails.
 */
static ssize_t char_dev_write(struct file *filp, const char __user *buf,
			      size_t count, loff_t *f_pos)
{
	struct sk_buff *skb;
	struct char_dev_user *dev = (struct char_dev_user *)filp->private_data;
	int err = 0;

	CG2900_INFO("CharDev: char_dev_write");

	if (!dev) {
		CG2900_ERR("Calling with NULL pointer");
		return -EBADF;
	}
	mutex_lock(&dev->write_mutex);

	skb = cg2900_alloc_skb(count, GFP_ATOMIC);
	if (!skb) {
		CG2900_ERR("Couldn't allocate sk_buff with length %d", count);
		goto error_handling;
	}

	if (copy_from_user(skb_put(skb, count), buf, count)) {
		kfree_skb(skb);
		err = -EFAULT;
		goto error_handling;
	}

	err = cg2900_write(dev->cpd_dev, skb);
	if (err) {
		CG2900_ERR("cg2900_write failed (%d)", err);
		kfree_skb(skb);
		goto error_handling;
	}

	mutex_unlock(&dev->write_mutex);
	return count;

error_handling:
	mutex_unlock(&dev->write_mutex);
	return err;
}

/**
 * char_dev_unlocked_ioctl() - Handle IOCTL call to the interface.
 * @filp:	Pointer to the file struct.
 * @cmd:	IOCTL command.
 * @arg:	IOCTL argument.
 *
 * Returns:
 *   0 if there is no error.
 *   -EBADF if NULL pointer was supplied in private data.
 *   -EINVAL if supplied cmd is not supported.
 *   For cmd CG2900_CHAR_DEV_IOCTL_CHECK4RESET 0x01 is returned if device is
 *   reset and 0x02 is returned if device is closed.
 */
static long char_dev_unlocked_ioctl(struct file *filp, unsigned int cmd,
				    unsigned long arg)
{
	struct char_dev_user *dev = (struct char_dev_user *)filp->private_data;
	struct cg2900_rev_data rev_data;
	int err = 0;

	CG2900_INFO("CharDev: char_dev_unlocked_ioctl cmd %u for %s", cmd,
		    dev->name);
	CG2900_DBG("DIR: %d, TYPE: %d, NR: %d, SIZE: %d",
		     _IOC_DIR(cmd), _IOC_TYPE(cmd), _IOC_NR(cmd),
		     _IOC_SIZE(cmd));

	switch (cmd) {
	case CG2900_CHAR_DEV_IOCTL_RESET:
		if (!dev) {
			err = -EBADF;
			goto error_handling;
		}
		CG2900_INFO("ioctl reset command for device %s", dev->name);
		err = cg2900_reset(dev->cpd_dev);
		break;

	case CG2900_CHAR_DEV_IOCTL_CHECK4RESET:
		if (!dev) {
			CG2900_INFO("ioctl check for reset command for device");
			/* Return positive value if closed */
			err = CG2900_CHAR_DEV_IOCTL_EVENT_CLOSED;
		} else if (dev->reset_state == CG2900_CHAR_RESET) {
			CG2900_INFO("ioctl check for reset command for device "
				    "%s", dev->name);
			/* Return positive value if reset */
			err = CG2900_CHAR_DEV_IOCTL_EVENT_RESET;
		}
		break;

	case CG2900_CHAR_DEV_IOCTL_GET_REVISION:
		CG2900_INFO("ioctl check for local revision info");
		if (cg2900_get_local_revision(&rev_data)) {
			CG2900_DBG("Read revision data revision %d "
				   "sub_version %d",
				   rev_data.revision, rev_data.sub_version);
			err = rev_data.revision;
		} else {
			CG2900_DBG("No revision data available");
			err = -EIO;
		}
		break;

	case CG2900_CHAR_DEV_IOCTL_GET_SUB_VER:
		CG2900_INFO("ioctl check for local sub-version info");
		if (cg2900_get_local_revision(&rev_data)) {
			CG2900_DBG("Read revision data revision %d "
				   "sub_version %d",
				   rev_data.revision, rev_data.sub_version);
			err = rev_data.sub_version;
		} else {
			CG2900_DBG("No revision data available");
			err = -EIO;
		}
		break;

#ifdef CG2900_GPS_DSM_SUPPORT
	case CG2900_CHAR_DEV_IOCTL_ENTER_DSM:
		enter_dsm();
		//CG2900_INFO("ioctl for putting chip in DSM Mode");
		printk("char_dev_unlocked_ioctl - CG2900 - Enter DSM Done!!!\n");
		break;

	case CG2900_CHAR_DEV_IOCTL_EXIT_DSM:
		exit_dsm();
		//CG2900_INFO("ioctl for exiting chip from DSM Mode");
		printk("char_dev_unlocked_ioctl - CG2900 - Exit DSM Done!!!\n");
		break;
#endif /* CG2900_GPS_DSM_SUPPORT */

	default:
		CG2900_ERR("Unknown ioctl command %08X", cmd);
		err = -EINVAL;
		break;
	};

error_handling:
	return err;
}

/**
 * char_dev_poll() - Handle POLL call to the interface.
 * @filp:	Pointer to the file struct.
 * @wait:	Poll table supplied to caller.
 *
 * Returns:
 *   Mask of current set POLL values
 */
static unsigned int char_dev_poll(struct file *filp, poll_table *wait)
{
	struct char_dev_user *dev = (struct char_dev_user *)filp->private_data;
	unsigned int mask = 0;

	if (!dev) {
		CG2900_DBG("Device not open");
		return POLLERR | POLLRDHUP;
	}

	poll_wait(filp, &dev->reset_wait_queue, wait);
	poll_wait(filp, &dev->rx_wait_queue, wait);

	if (!dev->cpd_dev)
		mask |= POLLERR | POLLRDHUP;
	else
		mask |= POLLOUT; /* We can TX unless there is an error */

	if (!(skb_queue_empty(&dev->rx_queue)))
		mask |= POLLIN | POLLRDNORM;

	if (CG2900_CHAR_RESET == dev->reset_state)
		mask |= POLLPRI;

	return mask;
}

/*
 * struct char_dev_fops - Char devices file operations.
 * @read:		Function that reads from the char device.
 * @write:		Function that writes to the char device.
 * @unlocked_ioctl:	Function that performs IO operations with
 *			the char device.
 * @poll:		Function that checks if there are possible operations
 *			with the char device.
 * @open:		Function that opens the char device.
 * @release:		Function that release the char device.
 */
static const struct file_operations char_dev_fops = {
	.read		= char_dev_read,
	.write		= char_dev_write,
	.unlocked_ioctl	= char_dev_unlocked_ioctl,
	.poll		= char_dev_poll,
	.open		= char_dev_open,
	.release	= char_dev_release
};

/**
 * setup_cdev() - Set up the char device structure for device.
 * @dev_usr:	Char device user.
 * @parent:	Parent device pointer.
 * @name:	Name of registered device.
 *
 * The setup_cdev() function sets up the char_dev structure for this device.
 *
 * Returns:
 *   0 if there is no error.
 *   -EINVAL if NULL pointer has been supplied.
 *   Error codes from cdev_add and device_create.
 */
static int setup_cdev(struct char_dev_user *dev_usr, struct device *parent,
		      char *name)
{
	int err = 0;
	struct cg2900_driver_data *driver_data =
		(struct cg2900_driver_data *)dev_get_drvdata(parent);

	CG2900_INFO("setup_cdev");

	if (!driver_data) {
		CG2900_ERR("Received driver data is empty");
		return -EINVAL;
	}

	dev_usr->devt = MKDEV(MAJOR(parent->devt),
			      driver_data->next_free_minor);

	CG2900_INFO("CharDev: setup_cdev");

	/* Store device name */
	dev_usr->name = name;

	cdev_init(&dev_usr->cdev, &char_dev_fops);
	dev_usr->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev_usr->cdev, dev_usr->devt , 1);
	if (err) {
		CG2900_ERR("Failed to add char dev %d, error %d", err,
			   dev_usr->devt);
		return err;
	}

	CG2900_INFO("Added char device %s with major 0x%X and minor 0x%X",
		      name, MAJOR(dev_usr->devt), MINOR(dev_usr->devt));

	/* Create device node in file system. */
	dev_usr->cdev_device = device_create(parent->class, parent,
					     dev_usr->devt, NULL, name);
	if (IS_ERR(dev_usr->cdev_device)) {
		CG2900_ERR("Error adding %s device!", name);
		err = (int)dev_usr->cdev_device;
		goto error_handling_cdev_del;
	}

	/*Init mutexs*/
	mutex_init(&dev_usr->read_mutex);
	mutex_init(&dev_usr->write_mutex);

	skb_queue_head_init(&dev_usr->rx_queue);

	driver_data->next_free_minor++;
	return 0;

error_handling_cdev_del:
	cdev_del(&(dev_usr->cdev));
	CG2900_DBG("setup_cdev returning %d", err);
	return err;
}

/**
 * remove_cdev() - Remove char device structure for device.
 * @dev_usr:	Char device user.
 *
 * The remove_cdev() function releases the char_dev structure for this device.
 */
static void remove_cdev(struct char_dev_user *dev_usr)
{
	CG2900_INFO("CharDev: remove_cdev");

	if (!dev_usr)
		return;

	/*Delete device*/
	cdev_del(&(dev_usr->cdev));

	/* Remove device node in file system. */
	device_destroy(dev_usr->cdev_device->class, dev_usr->devt);

	kfree(dev_usr->cdev_device);
	dev_usr->cdev_device = NULL;

	kfree(dev_usr->name);
	dev_usr->name = NULL;

	skb_queue_purge(&dev_usr->rx_queue);

	mutex_destroy(&dev_usr->read_mutex);
	mutex_destroy(&dev_usr->write_mutex);
}

/* ---------------- External functions -------------- */

int  cg2900_char_devices_init(int char_dev_usage, struct device *dev)
{
	CG2900_INFO("cg2900_char_devices_init");

	if (!dev) {
		CG2900_ERR("NULL supplied for dev");
		return -EINVAL;
	}

	if (!char_dev_usage) {
		CG2900_INFO("No char dev used in CG2900.");
		return -EINVAL;
	}

	CG2900_INFO("CG2900 char devices, char_dev_usage 0x%X", char_dev_usage);

	if (char_info) {
		CG2900_ERR("Char devices already initiated");
		return -EINVAL;
	}

	/* Initialize private data. */
	char_info = kzalloc(sizeof(*char_info), GFP_ATOMIC);
	if (!char_info) {
		CG2900_ERR("Could not alloc char_info struct.");
		return -EINVAL;
	}

	mutex_init(&char_info->open_mutex);

	if (char_dev_usage & CG2900_CHAR_DEV_BT) {
		setup_cdev(&char_info->bt_cmd_user, dev, CG2900_BT_CMD);
		setup_cdev(&char_info->bt_acl_user, dev, CG2900_BT_ACL);
		setup_cdev(&char_info->bt_evt_user, dev, CG2900_BT_EVT);
	}

	if (char_dev_usage & CG2900_CHAR_DEV_FM_RADIO)
		setup_cdev(&char_info->fm_radio_user, dev, CG2900_FM_RADIO);

	if (char_dev_usage & CG2900_CHAR_DEV_GNSS)
		setup_cdev(&char_info->gnss_user, dev, CG2900_GNSS);

	if (char_dev_usage & CG2900_CHAR_DEV_DEBUG)
		setup_cdev(&char_info->debug_user, dev, CG2900_DEBUG);

	if (char_dev_usage & CG2900_CHAR_DEV_STE_TOOLS)
		setup_cdev(&char_info->ste_tools_user, dev, CG2900_STE_TOOLS);

	if (char_dev_usage & CG2900_CHAR_DEV_HCI_LOGGER)
		setup_cdev(&char_info->hci_logger_user, dev, CG2900_HCI_LOGGER);

	if (char_dev_usage & CG2900_CHAR_DEV_US_CTRL)
		setup_cdev(&char_info->us_ctrl_user, dev, CG2900_US_CTRL);

	if (char_dev_usage & CG2900_CHAR_DEV_BT_AUDIO)
		setup_cdev(&char_info->bt_audio_user, dev, CG2900_BT_AUDIO);

	if (char_dev_usage & CG2900_CHAR_DEV_FM_RADIO_AUDIO)
		setup_cdev(&char_info->fm_audio_user, dev,
			   CG2900_FM_RADIO_AUDIO);

	if (char_dev_usage & CG2900_CHAR_DEV_CORE)
		setup_cdev(&char_info->core_user, dev, CG2900_CORE);

	return 0;
}

void cg2900_char_devices_exit(void)
{
	CG2900_INFO("cg2900_char_devices_exit");

	if (!char_info)
		return;

	remove_cdev(&char_info->bt_cmd_user);
	remove_cdev(&char_info->bt_acl_user);
	remove_cdev(&char_info->bt_evt_user);
	remove_cdev(&char_info->fm_radio_user);
	remove_cdev(&char_info->gnss_user);
	remove_cdev(&char_info->debug_user);
	remove_cdev(&char_info->ste_tools_user);
	remove_cdev(&char_info->hci_logger_user);
	remove_cdev(&char_info->us_ctrl_user);
	remove_cdev(&char_info->bt_audio_user);
	remove_cdev(&char_info->fm_audio_user);
	remove_cdev(&char_info->core_user);

	mutex_destroy(&char_info->open_mutex);

	kfree(char_info);
	char_info = NULL;
}

MODULE_AUTHOR("Henrik Possung ST-Ericsson");
MODULE_AUTHOR("Par-Gunnar Hjalmdahl ST-Ericsson");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ST-Ericsson CG2900 Char Devices Driver");
