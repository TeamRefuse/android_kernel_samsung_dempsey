/*
 * drivers/mfd/cg2900/cg2900_core.c
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
 * Linux Bluetooth HCI H:4 Driver for ST-Ericsson CG2900 connectivity
 * controller.
 */

#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/gfp.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>

#include <linux/mfd/cg2900.h>
#include <mach/cg2900_devices.h>
#include "cg2900_core.h"
#include "cg2900_char_devices.h"
#include "cg2900_debug.h"
#include "hci_defines.h"

/* Device names */
#define CG2900_CDEV_NAME		"cg2900_core_test"
#define CG2900_CLASS_NAME		"cg2900_class"
#define CG2900_DEVICE_NAME		"cg2900_driver"
#define CORE_WQ_NAME			"cg2900_core_wq"
#define TEST_INIT_WQ_NAME		"cg2900_test_init_wq"

/* Device Major ID */
#define CG2900_DEVICE_MAJOR_ID	241

#define SET_MAIN_STATE(__core_new_state) \
	CG2900_SET_STATE("main_state", core_info->main_state, \
			 __core_new_state)
#define SET_BOOT_STATE(__core_new_state) \
	CG2900_SET_STATE("boot_state", core_info->boot_state, __core_new_state)
#define SET_TRANSPORT_STATE(__core_new_state) \
	CG2900_SET_STATE("transport_state", core_info->transport_state, \
			 __core_new_state)

#define LOGGER_DIRECTION_TX		0
#define LOGGER_DIRECTION_RX		1

/* Number of bytes to reserve at start of sk_buffer when receiving packet */
#define RX_SKB_RESERVE			8

/*
 * Timeout values
 */
//#define CHIP_STARTUP_TIMEOUT		(15000)	/* ms */
#define CHIP_STARTUP_TIMEOUT		(30000)	/* ms */
#define CHIP_SHUTDOWN_TIMEOUT		(15000)	/* ms */
#define LINE_TOGGLE_DETECT_TIMEOUT	(50)	/* ms */
#define CHIP_READY_TIMEOUT		(100)	/* ms */
#define REVISION_READOUT_TIMEOUT	(500)	/* ms */

/*
 * We can have up to 32 char devs with current bit mask and we also have
 * the parent device here in the transport so that is 33 devices in total.
 */
#define MAX_NBR_OF_DEVS			33

/* Default H4 channels which may change depending on connected controller */
#define HCI_FM_RADIO_H4_CHANNEL		0x08
#define HCI_GNSS_H4_CHANNEL		0x09

#define DELAY_5_SEC			(5 * 1000)

/*
 *	Internal type definitions
 */

/**
 * enum main_state - Main-state for CG2900 Core.
 * @CORE_INITIALIZING:	CG2900 Core initializing.
 * @CORE_IDLE:		No user registered to CG2900 Core.
 * @CORE_BOOTING:	CG2900 Core booting after first user is registered.
 * @CORE_CLOSING:	CG2900 Core closing after last user has deregistered.
 * @CORE_RESETING:	CG2900 Core reset requested.
 * @CORE_ACTIVE:	CG2900 Core up and running with at least one user.
 */
enum main_state {
	CORE_INITIALIZING,
	CORE_IDLE,
	CORE_BOOTING,
	CORE_CLOSING,
	CORE_RESETING,
	CORE_ACTIVE
};

/**
 * enum boot_state - BOOT-state for CG2900 Core.
 * @BOOT_NOT_STARTED:				Boot has not yet started.
 * @BOOT_READ_LOCAL_VERSION_INFORMATION:	ReadLocalVersionInformation
 *						command has been sent.
 * @BOOT_READY:					CG2900 Core boot is ready.
 * @BOOT_FAILED:				CG2900 Core boot failed.
 */
enum boot_state {
	BOOT_NOT_STARTED,
	BOOT_READ_LOCAL_VERSION_INFORMATION,
	BOOT_READY,
	BOOT_FAILED
};

/**
 * enum transport_state - State for the CG2900 transport.
 * @TRANS_INITIALIZING:	Transport initializing.
 * @TRANS_OPENED:	Transport is opened (data can be sent).
 * @TRANS_CLOSED:	Transport is closed (data cannot be sent).
 */
enum transport_state {
	TRANS_INITIALIZING,
	TRANS_OPENED,
	TRANS_CLOSED
};

/**
 * struct cg2900_users - Stores all current users of CG2900 Core.
 * @bt_cmd:		BT command channel user.
 * @bt_acl:		BT ACL channel user.
 * @bt_evt:		BT event channel user.
 * @fm_radio:		FM radio channel user.
 * @gnss GNSS:		GNSS channel user.
 * @debug Debug:	Internal debug channel user.
 * @ste_tools:		ST-E tools channel user.
 * @hci_logger:		HCI logger channel user.
 * @us_ctrl:		User space control channel user.
 * @bt_audio:		BT audio command channel user.
 * @fm_radio_audio:	FM audio command channel user.
 * @core:		Core command channel user.
 * @nbr_of_users:	Number of users currently registered (not including
 *			the HCI logger).
 */
struct cg2900_users {
	struct cg2900_device	*bt_cmd;
	struct cg2900_device	*bt_acl;
	struct cg2900_device	*bt_evt;
	struct cg2900_device	*fm_radio;
	struct cg2900_device	*gnss;
	struct cg2900_device	*debug;
	struct cg2900_device	*ste_tools;
	struct cg2900_device	*hci_logger;
	struct cg2900_device	*us_ctrl;
	struct cg2900_device	*bt_audio;
	struct cg2900_device	*fm_radio_audio;
	struct cg2900_device	*core;
	unsigned int		nbr_of_users;
};

/**
  * struct local_chip_info - Stores local controller info.
  * @version_set:		true if version data is valid.
  * @hci_version:		HCI version of local controller.
  * @hci_revision:		HCI revision of local controller.
  * @lmp_pal_version:		LMP/PAL version of local controller.
  * @manufacturer:		Manufacturer of local controller.
  * @lmp_pal_subversion:	LMP/PAL sub-version of local controller.
  *
  * According to Bluetooth HCI Read Local Version Information command.
  */
struct local_chip_info {
	bool	version_set;
	u8	hci_version;
	u16	hci_revision;
	u8	lmp_pal_version;
	u16	manufacturer;
	u16	lmp_pal_subversion;
};

/**
 * struct chip_handler_item - Structure to store chip handler cb.
 * @list:	list_head struct.
 * @cb:	Chip handler callback struct.
 */
struct chip_handler_item {
	struct list_head		list;
	struct cg2900_id_callbacks	cb;
};

/**
 * struct ccd_work_struct - Work structure for CG2900 Core module.
 * @work:	Work structure.
 * @data:	Pointer to private data.
 *
 * This structure is used to pack work for work queue.
 */
struct ccd_work_struct{
	struct work_struct	work;
	void			*data;
};

/**
 * struct test_char_dev_info - Stores device information.
 * @test_miscdev:	Registered Misc Device.
 * @rx_queue:		RX data queue.
 */
struct test_char_dev_info {
	struct miscdevice	test_miscdev;
	struct sk_buff_head	rx_queue;
};

/**
 * struct trans_info - Stores transport information.
 * @dev:	Transport device.
 * @cb:		Transport cb.
 */
struct trans_info {
	struct cg2900_trans_dev		dev;
	struct cg2900_trans_callbacks	cb;
};

/**
 * struct core_info - Main info structure for CG2900 Core.
 * @users:		Stores all users of CG2900 Core.
 * @local_chip_info:	Stores information of local controller.
 * @main_state:		Current Main-state of CG2900 Core.
 * @boot_state:		Current BOOT-state of CG2900 Core.
 * @transport_state:	Current TRANSPORT-state of CG2900 Core.
 * @wq:			CG2900 Core workqueue.
 * @hci_logger_config:	Stores HCI logger configuration.
 * @dev:		Device structure for STE Connectivity driver.
 * @chip_dev:		Device structure for chip driver.
 * @h4_channels:	HCI H:4 channel used by this device.
 * @test_char_dev:	Stores information of test char dev.
 * @class:		Main class for CG2900 driver.
 * @trans_info:		Stores information about current transport.
 * @timer:		Chip Init timer (for chip Init).
 */
struct core_info {
	struct cg2900_users		users;
	struct local_chip_info		local_chip_info;
	enum main_state			main_state;
	enum boot_state			boot_state;
	enum transport_state		transport_state;
	struct workqueue_struct		*wq;
	struct cg2900_hci_logger_config	hci_logger_config;
	struct device			*dev;
	struct cg2900_chip_dev		chip_dev;
	struct cg2900_h4_channels	h4_channels;
	struct test_char_dev_info	*test_char_dev;
	struct class			*class;
	struct trans_info		*trans_info;
};

/*
 *	Internal variable declarations
 */

/*
 * core_info - Main information object for CG2900 Core.
 */
static struct core_info *core_info;
static struct core_info *bak_core_info;
/* A define to easily switch between normal usage and test purposes. */
#if 0
/* This is test setup */
static int char_dev_usage = CG2900_ALL_CHAR_DEVS;
#else
/* This is normal setup */
static int char_dev_usage = CG2900_CHAR_DEV_GNSS |
			    CG2900_CHAR_DEV_HCI_LOGGER |
			    CG2900_CHAR_DEV_US_CTRL |
			    CG2900_CHAR_DEV_BT_AUDIO |
			    CG2900_CHAR_DEV_FM_RADIO_AUDIO |
			    CG2900_CHAR_DEV_CORE;
#endif

/* Module parameters */
int cg2900_debug_level = CG2900_DEFAULT_DEBUG_LEVEL;
EXPORT_SYMBOL(cg2900_debug_level);

u8 bd_address[] = {0x00, 0xBE, 0xAD, 0xDE, 0x80, 0x00};
EXPORT_SYMBOL(bd_address);
int bd_addr_count = BT_BDADDR_SIZE;

/* Setting default values to ST-E CG2900 */
int default_manufacturer = 0x30;
EXPORT_SYMBOL(default_manufacturer);
int default_hci_revision = 0x0700;
EXPORT_SYMBOL(default_hci_revision);
int default_sub_version = 0x0011;
EXPORT_SYMBOL(default_sub_version);

static int sleep_timeout_ms;

static struct workqueue_struct		*test_init_wq;
/*
 * chip_handlers - List of the register handlers for different chips.
 */
LIST_HEAD(chip_handlers);

/*
 * reset_cmd_req - Hardcoded HCI Reset command.
 */
static const u8 reset_cmd_req[] = {
	0x00, /* Reserved for H4 channel*/
	HCI_BT_RESET_CMD_LSB,
	HCI_BT_RESET_CMD_MSB,
	0x00
};

/*
 * read_local_version_information_cmd_req -
 * Hardcoded HCI Read Local Version Information command
 */
static const u8 read_local_version_information_cmd_req[] = {
	0x00, /* Reserved for H4 channel*/
	HCI_BT_READ_LOCAL_VERSION_CMD_LSB,
	HCI_BT_READ_LOCAL_VERSION_CMD_MSB,
	0x00
};

/*
 * main_wait_queue - Main Wait Queue in CG2900 Core.
 */
static DECLARE_WAIT_QUEUE_HEAD(main_wait_queue);

/*
 * main_wait_queue - Char device Wait Queue in CG2900 Core.
 */
static DECLARE_WAIT_QUEUE_HEAD(char_wait_queue);

/*
 *	Internal functions
 */

/**
 * free_user_dev - Frees user device and also sets it to NULL to inform caller.
 * @dev:	Pointer to user device.
 */
static void free_user_dev(struct cg2900_device **dev)
{
	if (*dev) {
		kfree((*dev)->cb);
		kfree(*dev);
		*dev = NULL;
	}
}

/**
 * handle_reset_of_user - Calls the reset callback and frees the device.
 * @dev:	Pointer to CG2900 device.
 */
static void handle_reset_of_user(struct cg2900_device **dev)
{
	if (*dev) {
		if ((*dev)->cb->reset_cb)
			(*dev)->cb->reset_cb((*dev));
		free_user_dev(dev);
	}
}

/**
 * transmit_skb_to_chip() - Transmit buffer to the transport.
 * @skb:	Data packet.
 * @use_logger:	True if HCI logger shall be used, false otherwise.
 *
 * The transmit_skb_to_chip() function transmit buffer to the transport.
 * If enabled, copy the transmitted data to the HCI logger as well.
 */
static void transmit_skb_to_chip(struct sk_buff *skb, bool use_logger)
{
	int err;
	struct sk_buff *skb_log;
	struct trans_info *trans_info = core_info->trans_info;
	struct cg2900_device *logger;

	CG2900_DBG_DATA("transmit_skb_to_chip %d bytes. First byte 0x%02X",
			skb->len, *(skb->data));

	if (TRANS_CLOSED == core_info->transport_state) {
		CG2900_ERR("Trying to write on a closed channel");
		kfree_skb(skb);
		return;
	}

	/*
	 * If HCI logging is enabled for this channel, copy the data to
	 * the HCI logging output.
	 */
	logger = core_info->users.hci_logger;
	if (!use_logger || !logger)
		goto transmit;

	/*
	 * Alloc a new sk_buff and copy the data into it. Then send it to
	 * the HCI logger.
	 */
	skb_log = alloc_skb(skb->len + 1, GFP_ATOMIC);
	if (!skb_log) {
		CG2900_ERR("Couldn't allocate skb_log");
		goto transmit;
	}

	memcpy(skb_put(skb_log, skb->len), skb->data, skb->len);
	skb_log->data[0] = (u8) LOGGER_DIRECTION_TX;

	if (logger->cb->read_cb)
		logger->cb->read_cb(logger, skb_log);

transmit:

	if (trans_info && trans_info->cb.write) {
		err = trans_info->cb.write(&trans_info->dev, skb);
		if (err)
			CG2900_ERR("Transport write failed (%d)", err);
	} else {
		CG2900_ERR("No way to write to chip");
		err = -EPERM;
	}

	if (err)
		kfree_skb(skb);
}

/**
 * create_and_send_bt_cmd() - Copy and send sk_buffer.
 * @data:	Data to send.
 * @length:	Length in bytes of data.
 *
 * The create_and_send_bt_cmd() function allocates sk_buffer, copy supplied
 * data to it, and send the sk_buffer to the transport.
 * Note that the data must contain the H:4 header.
 */
static void create_and_send_bt_cmd(const u8 *data, int length)
{
	struct sk_buff *skb;

	skb = alloc_skb(length, GFP_ATOMIC);
	if (!skb) {
		CG2900_ERR("Couldn't allocate sk_buff with length %d",
			     length);
		return;
	}

	memcpy(skb_put(skb, length), data, length);
	skb->data[0] = HCI_BT_CMD_H4_CHANNEL;
	transmit_skb_to_chip(skb, core_info->hci_logger_config.bt_cmd_enable);
}

/**
 * chip_not_detected() - Called when it is not possible to detect the chip.
 *
 * This function sets chip information to default values if it is not possible
 * to read out information from the chip. This is common when running module
 * tests.
 */
static void chip_not_detected(void)
{
	struct list_head *cursor;
	struct chip_handler_item *tmp;

	CG2900_ERR("Could not read out revision from the chip. This is "
		   "typical when running stubbed CG2900.\n"
		   "Switching to default value:\n"
		   "\tman 0x%04X\n"
		   "\trev 0x%04X\n"
		   "\tsub 0x%04X",
		   default_manufacturer,
		   default_hci_revision,
		   default_sub_version);

	core_info->chip_dev.chip.manufacturer = default_manufacturer;
	core_info->chip_dev.chip.hci_revision = default_hci_revision;
	core_info->chip_dev.chip.hci_sub_version = default_sub_version;

	memset(&(core_info->chip_dev.cb), 0, sizeof(core_info->chip_dev.cb));

	/* Find the handler for our default chip */
	list_for_each(cursor, &chip_handlers) {
		tmp = list_entry(cursor, struct chip_handler_item, list);
		if (tmp->cb.check_chip_support(&(core_info->chip_dev))) {
			CG2900_INFO("Chip handler found");
			SET_BOOT_STATE(BOOT_READY);
			break;
		}
	}
}

/**
 * enable_hci_logger() - Enable HCI logger for each device.
 * @skb:	Received sk buffer.
 *
 * The enable_hci_logger() change HCI logger configuration for all registered
 * devices.
 *
 * Returns:
 *   0 if there is no error.
 *   -EACCES if bad structure was supplied.
 */
static int enable_hci_logger(struct sk_buff *skb)
{
	struct cg2900_users *users;
	struct cg2900_hci_logger_config	*config;

	if (skb->len != sizeof(*config)) {
		CG2900_ERR("Trying to configure HCI logger with bad structure");
		return -EACCES;
	}

	users = &(core_info->users);
	config = &(core_info->hci_logger_config);

	/* First store the logger config */
	memcpy(config, skb->data, sizeof(*config));

	/* Then go through all devices and set the right settings */
	if (users->bt_cmd)
		users->bt_cmd->logger_enabled = config->bt_cmd_enable;
	if (users->bt_audio)
		users->bt_audio->logger_enabled = config->bt_audio_enable;
	if (users->bt_acl)
		users->bt_acl->logger_enabled = config->bt_acl_enable;
	if (users->bt_evt)
		users->bt_evt->logger_enabled = config->bt_evt_enable;
	if (users->fm_radio)
		users->fm_radio->logger_enabled = config->fm_radio_enable;
	if (users->fm_radio_audio)
		users->fm_radio_audio->logger_enabled =
				config->fm_radio_audio_enable;
	if (users->gnss)
		users->gnss->logger_enabled = config->gnss_enable;

	kfree_skb(skb);
	return 0;
}

/**
 * find_bt_audio_user() - Check if data packet is an audio related packet.
 * @h4_channel:	H4 channel.
 * @dev:	Stored CG2900 device.
 * @skb:	skb with received packet.
 * Returns:
 *   0 - if no error occurred.
 *   -ENXIO - if cg2900_device not found.
 */
static int find_bt_audio_user(int h4_channel, struct cg2900_device **dev,
				  const struct sk_buff * const skb)
{
	if (core_info->chip_dev.cb.is_bt_audio_user &&
	    core_info->chip_dev.cb.is_bt_audio_user(h4_channel, skb)) {
		*dev = core_info->users.bt_audio;
		if (!(*dev)) {
			CG2900_ERR("H:4 channel not registered in core_info: "
				     "0x%X", h4_channel);
			return -ENXIO;
		}
	}
	return 0;
}

/**
 * find_fm_audio_user() - Check if data packet is an audio related packet.
 * @h4_channel:	H4 channel.
 * @dev:	Stored CG2900 device.
 * @skb:	skb with received packet.
 * Returns:
 *   0 if no error occurred.
 *   -ENXIO if cg2900_device not found.
 */
static int find_fm_audio_user(int h4_channel, struct cg2900_device **dev,
				  const struct sk_buff * const skb)
{
	if (core_info->chip_dev.cb.is_fm_audio_user &&
	    core_info->chip_dev.cb.is_fm_audio_user(h4_channel, skb)) {
		*dev = core_info->users.fm_radio_audio;
		if (!(*dev)) {
			CG2900_ERR("H:4 channel not registered in core_info: "
				     "0x%X", h4_channel);
			return -ENXIO;
		}
	}
	return 0;
}

/**
 * find_h4_user() - Get H4 user based on supplied H4 channel.
 * @h4_channel:	H4 channel.
 * @dev:	Stored CG2900 device.
 * @skb:	(optional) skb with received packet. Set to NULL if NA.
 *
 * Returns:
 *   0 if there is no error.
 *   -EINVAL if bad channel is supplied or no user was found.
 *   -ENXIO if channel is audio channel but not registered with CG2900.
 */
static int find_h4_user(int h4_channel, struct cg2900_device **dev,
			const struct sk_buff * const skb)
{
	int err = 0;
	struct cg2900_users *users = &(core_info->users);
	struct cg2900_h4_channels *chan = &(core_info->h4_channels);

   if(core_info != bak_core_info)
   {
      core_info = bak_core_info;
      users = &(core_info->users);
      chan = &(core_info->h4_channels);      
   }
	if (h4_channel == chan->bt_cmd_channel) {
		*dev = users->bt_cmd;
	} else if (h4_channel == chan->bt_acl_channel) {
		*dev = users->bt_acl;
	} else if (h4_channel == chan->bt_evt_channel) {
		*dev = users->bt_evt;
		/* Check if it's event generated by previously sent audio user
		 * command. If so then that event should be dispatched to audio
		 * user*/
		err = find_bt_audio_user(h4_channel, dev, skb);
	} else if (h4_channel == chan->gnss_channel) {
		*dev = users->gnss;
	} else if (h4_channel == chan->fm_radio_channel) {
		*dev = users->fm_radio;
		/* Check if it's an event generated by previously sent audio
		 * user command. If so then that event should be dispatched to
		 * audio user */
		/* Check added by Hemant */
		if (users->fm_radio_audio)
			err = find_fm_audio_user(h4_channel, dev, skb);
	} else if (h4_channel == chan->debug_channel) {
		*dev = users->debug;
	} else if (h4_channel == chan->ste_tools_channel) {
		*dev = users->ste_tools;
	} else if (h4_channel == chan->hci_logger_channel) {
		*dev = users->hci_logger;
	} else if (h4_channel == chan->us_ctrl_channel) {
		*dev = users->us_ctrl;
	} else if (h4_channel == chan->core_channel) {
		*dev = users->core;
	} else {
		*dev = NULL;
		CG2900_ERR("Bad H:4 channel supplied: 0x%X", h4_channel);
		return -EINVAL;
	}

	return err;
}

/**
 * add_h4_user() - Add H4 user to user storage based on supplied H4 channel.
 * @dev:	Stored CG2900 device.
 * @name:	Device name to identify different devices that are using
 *		the same H4 channel.
 *
 * Returns:
 *   0 if there is no error.
 *   -EINVAL if NULL pointer or bad channel is supplied.
 *   -EBUSY if there already is a user for supplied channel.
 */
static int add_h4_user(struct cg2900_device *dev, const char * const name)
{
	int err = 0;
	struct cg2900_users *users = &(core_info->users);
	struct cg2900_hci_logger_config	*config =
			&(core_info->hci_logger_config);
	struct cg2900_h4_channels *chan = &(core_info->h4_channels);

	if (!dev) {
		CG2900_ERR("NULL device supplied");
		return -EINVAL;
	}

	if (dev->h4_channel == chan->bt_cmd_channel) {
		if (!users->bt_cmd &&
		    0 == strncmp(name, CG2900_BT_CMD, CG2900_MAX_NAME_SIZE)) {
			users->bt_cmd = dev;
			users->bt_cmd->logger_enabled = config->bt_cmd_enable;
			(users->nbr_of_users)++;
		} else if (!users->bt_audio &&
			   0 == strncmp(name, CG2900_BT_AUDIO,
					CG2900_MAX_NAME_SIZE)) {
			users->bt_audio = dev;
			users->bt_audio->logger_enabled =
					config->bt_audio_enable;
			(users->nbr_of_users)++;
		} else {
			err = -EBUSY;
			CG2900_ERR("name %s bt_cmd 0x%X  bt_audio 0x%X",
				     name, (int)users->bt_cmd,
				     (int)users->bt_audio);
		}
	} else if (dev->h4_channel == chan->bt_acl_channel) {
		if (!users->bt_acl) {
			users->bt_acl = dev;
			users->bt_acl->logger_enabled = config->bt_acl_enable;
			(users->nbr_of_users)++;
		} else {
			err = -EBUSY;
		}
	} else if (dev->h4_channel == chan->bt_evt_channel) {
		if (!users->bt_evt) {
			users->bt_evt = dev;
			users->bt_evt->logger_enabled = config->bt_evt_enable;
			(users->nbr_of_users)++;
		} else {
			err = -EBUSY;
		}
	} else if (dev->h4_channel == chan->gnss_channel) {
		if (!users->gnss) {
			users->gnss = dev;
			users->gnss->logger_enabled = config->gnss_enable;
			(users->nbr_of_users)++;
		} else {
			err = -EBUSY;
		}
	} else if (dev->h4_channel == chan->fm_radio_channel) {
		if (!users->fm_radio &&
		    0 == strncmp(name, CG2900_FM_RADIO,
				 CG2900_MAX_NAME_SIZE)) {
			users->fm_radio = dev;
			users->fm_radio->logger_enabled =
					config->fm_radio_enable;
			(users->nbr_of_users)++;
		} else if (!users->fm_radio_audio &&
			   0 == strncmp(name, CG2900_FM_RADIO_AUDIO,
					CG2900_MAX_NAME_SIZE)) {
			users->fm_radio_audio = dev;
			users->fm_radio_audio->logger_enabled =
					config->fm_radio_audio_enable;
			(users->nbr_of_users)++;
		} else {
			err = -EBUSY;
		}
	} else if (dev->h4_channel == chan->debug_channel) {
		if (!users->debug)
			users->debug = dev;
		else
			err = -EBUSY;
	} else if (dev->h4_channel == chan->ste_tools_channel) {
		if (!users->ste_tools)
			users->ste_tools = dev;
		else
			err = -EBUSY;
	} else if (dev->h4_channel == chan->hci_logger_channel) {
		if (!users->hci_logger)
			users->hci_logger = dev;
		else
			err = -EBUSY;
	} else if (dev->h4_channel == chan->us_ctrl_channel) {
		if (!users->us_ctrl)
			users->us_ctrl = dev;
		else
			err = -EBUSY;
	} else if (dev->h4_channel == chan->core_channel) {
		if (!users->core) {
			(users->nbr_of_users)++;
			users->core = dev;
		} else {
			err = -EBUSY;
		}
	} else {
		err = -EINVAL;
		CG2900_ERR("Bad H:4 channel supplied: 0x%X", dev->h4_channel);
	}

	if (err)
		CG2900_ERR("H:4 channel 0x%X, not registered (%d)",
			   dev->h4_channel, err);

	return err;
}

/**
 * remove_h4_user() - Remove H4 user from user storage.
 * @dev:	Stored CG2900 device.
 *
 * Returns:
 *   0 if there is no error.
 *   -EINVAL if NULL pointer is supplied, bad channel is supplied, or if there
 *   is no user for supplied channel.
 */
static int remove_h4_user(struct cg2900_device **dev)
{
	int err = 0;
	struct cg2900_users *users = &(core_info->users);
	struct cg2900_h4_channels *chan = &(core_info->h4_channels);
	struct cg2900_chip_callbacks *cb = &(core_info->chip_dev.cb);

	if (!dev || !(*dev)) {
		CG2900_ERR("NULL device supplied");
		return -EINVAL;
	}

	if ((*dev)->h4_channel == chan->bt_cmd_channel) {
		CG2900_DBG("bt_cmd 0x%X bt_audio 0x%X dev 0x%X",
			     (int)users->bt_cmd,
			     (int)users->bt_audio, (int)*dev);

		if (*dev == users->bt_cmd) {
			users->bt_cmd = NULL;
			(users->nbr_of_users)--;
		} else if (*dev == users->bt_audio) {
			users->bt_audio = NULL;
			(users->nbr_of_users)--;
		} else
			err = -EINVAL;

		CG2900_DBG("bt_cmd 0x%X bt_audio 0x%X dev 0x%X",
			     (int)users->bt_cmd,
			     (int)users->bt_audio, (int)*dev);

		/*
		 * If both BT Command channel users are de-registered we
		 * inform the chip handler.
		 */
		if (!users->bt_cmd && !users->bt_audio &&
		    cb->last_bt_user_removed)
			cb->last_bt_user_removed(&(core_info->chip_dev));
	} else if ((*dev)->h4_channel == chan->bt_acl_channel) {
		if (*dev == users->bt_acl) {
			users->bt_acl = NULL;
			(users->nbr_of_users)--;
		} else
			err = -EINVAL;
	} else if ((*dev)->h4_channel == chan->bt_evt_channel) {
		if (*dev == users->bt_evt) {
			users->bt_evt = NULL;
			(users->nbr_of_users)--;
		} else
			err = -EINVAL;
	} else if ((*dev)->h4_channel == chan->gnss_channel) {
		if (*dev == users->gnss) {
			users->gnss = NULL;
			(users->nbr_of_users)--;
		} else
			err = -EINVAL;

		/*
		 * If the GNSS channel user is de-registered we inform
		 * the chip handler.
		 */
		if (users->gnss == NULL && cb->last_gnss_user_removed)
			cb->last_gnss_user_removed(&(core_info->chip_dev));
	} else if ((*dev)->h4_channel == chan->fm_radio_channel) {
		if (*dev == users->fm_radio) {
			users->fm_radio = NULL;
			(users->nbr_of_users)--;
		} else if (*dev == users->fm_radio_audio) {
			users->fm_radio_audio = NULL;
			(users->nbr_of_users)--;
		} else
			err = -EINVAL;

		/*
		 * If both FM Radio channel users are de-registered we inform
		 * the chip handler.
		 */
		if (!users->fm_radio && !users->fm_radio_audio &&
		    cb->last_fm_user_removed)
			cb->last_fm_user_removed(&(core_info->chip_dev));
	} else if ((*dev)->h4_channel == chan->debug_channel) {
		if (*dev == users->debug)
			users->debug = NULL;
		else
			err = -EINVAL;
	} else if ((*dev)->h4_channel == chan->ste_tools_channel) {
		if (*dev == users->ste_tools)
			users->ste_tools = NULL;
		else
			err = -EINVAL;
	} else if ((*dev)->h4_channel == chan->hci_logger_channel) {
		if (*dev == users->hci_logger)
			users->hci_logger = NULL;
		else
			err = -EINVAL;
	} else if ((*dev)->h4_channel == chan->us_ctrl_channel) {
		if (*dev == users->us_ctrl)
			users->us_ctrl = NULL;
		else
			err = -EINVAL;
	} else if ((*dev)->h4_channel == chan->core_channel) {
		if (*dev == users->core) {
			users->core = NULL;
			(users->nbr_of_users)--;
		} else
			err = -EINVAL;
	} else {
		CG2900_ERR("Bad H:4 channel supplied: 0x%X",
			     (*dev)->h4_channel);
		return -EINVAL;
	}

	if (err)
		CG2900_ERR("Trying to remove device that was not registered");

	/*
	 * Free the device even if there is an error with the device.
	 * Also set to NULL to inform caller about the free.
	 */
	free_user_dev(dev);

	return err;
}

/**
 * chip_startup() - Start the connectivity controller and download patches and settings.
 */
static void chip_startup(void)
{
	CG2900_INFO("chip_startup");

	SET_MAIN_STATE(CORE_BOOTING);
	SET_BOOT_STATE(BOOT_NOT_STARTED);

	/*
	 * Transmit HCI reset command to ensure the chip is using
	 * the correct transport
	 */
	create_and_send_bt_cmd(reset_cmd_req, sizeof(reset_cmd_req));
}

/**
 * chip_shutdown() - Reset and power the chip off.
 */
static void chip_shutdown(void)
{
	int err = 0;
	struct trans_info *trans_info = core_info->trans_info;
	struct cg2900_chip_callbacks *cb = &(core_info->chip_dev.cb);

	CG2900_INFO("chip_shutdown");

	/* First do a quick power switch of the chip to assure a good state */
	if (trans_info && trans_info->cb.set_chip_power)
		trans_info->cb.set_chip_power(false);

	/*
	 * Wait 50ms before continuing to be sure that the chip detects
	 * chip power off.
	 */
	schedule_timeout_interruptible(
			msecs_to_jiffies(LINE_TOGGLE_DETECT_TIMEOUT));

	if (trans_info && trans_info->cb.set_chip_power)
		trans_info->cb.set_chip_power(true);

	/* Wait 100ms before continuing to be sure that the chip is ready */
	schedule_timeout_interruptible(msecs_to_jiffies(CHIP_READY_TIMEOUT));

	/*
	 * Let the chip handler finish the reset if any callback is registered.
	 * Otherwise we are finished.
	 */
	if (!cb->chip_shutdown) {
		CG2900_DBG("No registered handler. Finishing shutdown.");
		cg2900_chip_shutdown_finished(err);
		return;
	}

	err = cb->chip_shutdown(&(core_info->chip_dev));
	if (err) {
		CG2900_ERR("chip_shutdown failed (%d). Finishing shutdown.",
			   err);
		cg2900_chip_shutdown_finished(err);
	}
}

/**
 * handle_reset_cmd_complete_evt() - Handle a received HCI Command Complete event for a Reset command.
 * @data:	Pointer to received HCI data packet.
 *
 * Returns:
 *   True,  if packet was handled internally,
 *   False, otherwise.
 */
static bool handle_reset_cmd_complete_evt(u8 *data)
{
	bool pkt_handled = false;
	u8 status = data[0];

	CG2900_INFO("Received Reset complete event with status 0x%X", status);

	if ((core_info->main_state == CORE_BOOTING ||
	     core_info->main_state == CORE_INITIALIZING) &&
	    core_info->boot_state == BOOT_NOT_STARTED) {
		/* Transmit HCI Read Local Version Information command */
		SET_BOOT_STATE(BOOT_READ_LOCAL_VERSION_INFORMATION);
		create_and_send_bt_cmd(read_local_version_information_cmd_req,
			sizeof(read_local_version_information_cmd_req));

		pkt_handled = true;
	}

	return pkt_handled;
}

/**
 * handle_read_local_version_info_cmd_complete_evt() - Handle a received HCI Command Complete event for a ReadLocalVersionInformation command.
 * @data:	Pointer to received HCI data packet.
 *
 * Returns:
 *   True,  if packet was handled internally,
 *   False, otherwise.
 */
static bool handle_read_local_version_info_cmd_complete_evt(u8 *data)
{
	bool chip_handled = false;
	struct list_head *cursor;
	struct chip_handler_item *tmp;
	struct local_chip_info *chip;
	struct cg2900_chip_info *chip_info;
	struct cg2900_chip_callbacks *cb;
	u8 status;
	int err;

	/* Check we're in the right state */
	if ((core_info->main_state != CORE_BOOTING &&
	     core_info->main_state != CORE_INITIALIZING) ||
	     core_info->boot_state != BOOT_READ_LOCAL_VERSION_INFORMATION)
		return false;

	/* We got an answer for our HCI command. Extract data */
	status = data[0];
	err = 0;

	/* We will handle the packet */

	if (HCI_BT_ERROR_NO_ERROR != status) {
		CG2900_ERR("Received Read Local Version Information with "
			     "status 0x%X", status);
		SET_BOOT_STATE(BOOT_FAILED);
		cg2900_reset(NULL);
		return true;
	}

	/* The command worked. Store the data */
	chip = &(core_info->local_chip_info);
	chip->version_set = true;
	chip->hci_version = data[1];
	chip->hci_revision = GET_U16_FROM_L_ENDIAN(data[2], data[3]);
	chip->lmp_pal_version = data[4];
	chip->manufacturer = GET_U16_FROM_L_ENDIAN(data[5], data[6]);
	chip->lmp_pal_subversion = GET_U16_FROM_L_ENDIAN(data[7], data[8]);
	CG2900_DBG("Received Read Local Version Information with:\n"
		     "\thci_version:  0x%X\n"
		     "\thci_revision: 0x%X\n"
		     "\tlmp_pal_version: 0x%X\n"
		     "\tmanufacturer: 0x%X\n"
		     "\tlmp_pal_subversion: 0x%X",
		     chip->hci_version, chip->hci_revision,
		     chip->lmp_pal_version, chip->manufacturer,
		     chip->lmp_pal_subversion);

	cg2900_devices_set_hci_revision(chip->hci_version,
					chip->hci_revision,
					chip->lmp_pal_version,
					chip->lmp_pal_subversion,
					chip->manufacturer);

	/* Received good confirmation. Find handler for the chip. */
	chip_info = &(core_info->chip_dev.chip);
	chip_info->hci_revision = chip->hci_revision;
	chip_info->hci_sub_version = chip->lmp_pal_subversion;
	chip_info->manufacturer = chip->manufacturer;

	memset(&(core_info->chip_dev.cb), 0, sizeof(core_info->chip_dev.cb));

	list_for_each(cursor, &chip_handlers) {
		tmp = list_entry(cursor, struct chip_handler_item, list);
		chip_handled = tmp->cb.check_chip_support(
				&(core_info->chip_dev));
		if (chip_handled) {
			CG2900_INFO("Chip handler found");
			break;
		}
	}

	if (core_info->main_state == CORE_INITIALIZING) {
		/*
		 * We are now finished with the start-up during HwRegistered
		 * operation.
		 */
		SET_BOOT_STATE(BOOT_READY);
		wake_up_interruptible(&main_wait_queue);
	} else if (!chip_handled) {
		CG2900_INFO("No chip handler found. Start-up complete");
		SET_BOOT_STATE(BOOT_READY);
		cg2900_chip_startup_finished(0);
	} else {
		cb = &(core_info->chip_dev.cb);
		if (!cb->chip_startup)
			cg2900_chip_startup_finished(0);
		else {
			err = cb->chip_startup(&(core_info->chip_dev));
			if (err)
				cg2900_chip_startup_finished(err);
		}
	}

	return true;
}

/**
 * handle_rx_data_bt_evt() - Check if data should be handled in CG2900 Core.
 * @skb:	Data packet
 *
 * The handle_rx_data_bt_evt() function checks if received data should be
 * handled in CG2900 Core. If so handle it correctly.
 * Received data is always HCI BT Event.
 *
 * Returns:
 *   True,  if packet was handled internally,
 *   False, otherwise.
 */
static bool handle_rx_data_bt_evt(struct sk_buff *skb)
{
	bool pkt_handled = false;
	u8 *data = &(skb->data[CG2900_SKB_RESERVE]);
	u8 event_code = data[0];
	u8 lsb;
	u8 msb;

	/* First check the event code */
	if (HCI_BT_EVT_CMD_COMPLETE != event_code)
		goto out;

	lsb = data[3];
	msb = data[4];

	CG2900_DBG_DATA("Received Command Complete: LSB = 0x%02X, MSB=0x%02X",
			  lsb, msb);
	data += 5; /* Move to first byte after OCF */

	if (lsb == HCI_BT_RESET_CMD_LSB &&
	    msb == HCI_BT_RESET_CMD_MSB)
		pkt_handled = handle_reset_cmd_complete_evt(data);
	else if (lsb == HCI_BT_READ_LOCAL_VERSION_CMD_LSB &&
		 msb == HCI_BT_READ_LOCAL_VERSION_CMD_MSB)
		pkt_handled =
			handle_read_local_version_info_cmd_complete_evt(data);

	if (pkt_handled)
		kfree_skb(skb);
out:
	return pkt_handled;
}

/**
 * test_char_dev_tx_received() - Handle data received from CG2900 Core.
 * @dev:	Current transport device information.
 * @skb:	Buffer with data coming form device.
 */
static int test_char_dev_tx_received(struct cg2900_trans_dev *dev,
				     struct sk_buff *skb)
{
	skb_queue_tail(&core_info->test_char_dev->rx_queue, skb);
	wake_up_interruptible(&char_wait_queue);
	return 0;
}

/**
 * test_char_dev_open() - User space char device has been opened.
 * @inode:	Device driver information.
 * @filp:	Pointer to the file struct.
 *
 * Returns:
 *   0 if there is no error.
 *   -EACCES if transport already exists.
 *   -ENOMEM if allocation fails.
 *   Errors from create_work_item.
 */
static int test_char_dev_open(struct inode *inode, struct file *filp)
{
	struct cg2900_trans_callbacks cb = {
		.write = test_char_dev_tx_received,
		.open = NULL,
		.close = NULL,
		.set_chip_power = NULL
	};

	CG2900_INFO("test_char_dev_open");
	return cg2900_register_trans_driver(&cb, NULL);
}

/**
 * test_char_dev_release() - User space char device has been closed.
 * @inode:	Device driver information.
 * @filp:	Pointer to the file struct.
 *
 * Returns:
 *   0 if there is no error.
 */
static int test_char_dev_release(struct inode *inode, struct file *filp)
{
	/* Clean the message queue */
	skb_queue_purge(&core_info->test_char_dev->rx_queue);
	return cg2900_deregister_trans_driver();
}

/**
 * test_char_dev_read() - Queue and copy buffer to user space char device.
 * @filp:	Pointer to the file struct.
 * @buf:	Received buffer.
 * @count:	Count of received data in bytes.
 * @f_pos:	Position in buffer.
 *
 * Returns:
 *   >= 0 is number of bytes read.
 *   -EFAULT if copy_to_user fails.
 */
static ssize_t test_char_dev_read(struct file *filp, char __user *buf,
				  size_t count, loff_t *f_pos)
{
	struct sk_buff *skb;
	int bytes_to_copy;
	int err;
	struct sk_buff_head *rx_queue = &core_info->test_char_dev->rx_queue;

	CG2900_INFO("test_char_dev_read");

	if (skb_queue_empty(rx_queue))
		wait_event_interruptible(char_wait_queue,
					 !(skb_queue_empty(rx_queue)));

	skb = skb_dequeue(rx_queue);
	if (!skb) {
		CG2900_INFO("skb queue is empty - return with zero bytes");
		bytes_to_copy = 0;
		goto finished;
	}

	bytes_to_copy = min(count, skb->len);
	err = copy_to_user(buf, skb->data, bytes_to_copy);
	if (err) {
		skb_queue_head(rx_queue, skb);
		return -EFAULT;
	}

	skb_pull(skb, bytes_to_copy);

	if (skb->len > 0)
		skb_queue_head(rx_queue, skb);
	else
		kfree_skb(skb);

finished:
	return bytes_to_copy;
}

/**
 * test_char_dev_write() - Copy buffer from user and write to CG2900 Core.
 * @filp:	Pointer to the file struct.
 * @buf:	Read buffer.
 * @count:	Size of the buffer write.
 * @f_pos:	Position in buffer.
 *
 * Returns:
 *   >= 0 is number of bytes written.
 *   -EFAULT if copy_from_user fails.
 */
static ssize_t test_char_dev_write(struct file *filp, const char __user *buf,
				   size_t count, loff_t *f_pos)
{
	struct sk_buff *skb;

	CG2900_INFO("test_char_dev_write count %d", count);

	/* Allocate the SKB and reserve space for the header */
	skb = alloc_skb(count + RX_SKB_RESERVE, GFP_ATOMIC);
	if (!skb) {
		CG2900_ERR("Failed to alloc skb");
		return -ENOMEM;
	}
	skb_reserve(skb, RX_SKB_RESERVE);

	if (copy_from_user(skb_put(skb, count), buf, count)) {
		kfree_skb(skb);
		return -EFAULT;
	}
	cg2900_data_from_chip(skb);

	return count;
}

/**
 * test_char_dev_poll() - Handle POLL call to the interface.
 * @filp:	Pointer to the file struct.
 * @wait:	Poll table supplied to caller.
 *
 * Returns:
 *   Mask of current set POLL values (0 or (POLLIN | POLLRDNORM))
 */
static unsigned int test_char_dev_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(filp, &char_wait_queue, wait);

	if (!(skb_queue_empty(&core_info->test_char_dev->rx_queue)))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

/*
 * struct test_char_dev_fops - Test char devices file operations.
 * @read:	Function that reads from the char device.
 * @write:	Function that writes to the char device.
 * @poll:	Function that handles poll call to the fd.
 */
static const struct file_operations test_char_dev_fops = {
	.open = test_char_dev_open,
	.release = test_char_dev_release,
	.read = test_char_dev_read,
	.write = test_char_dev_write,
	.poll = test_char_dev_poll
};

/**
 * test_char_dev_create() - Create a char device for testing.
 *
 * Creates a separate char device that will interact directly with userspace
 * test application.
 *
 * Returns:
 *   0 if there is no error.
 *   -ENOMEM if allocation fails.
 *   -EBUSY if device has already been allocated.
 *   Error codes from misc_register.
 */
static int test_char_dev_create(void)
{
	int err;

	if (core_info->test_char_dev) {
		CG2900_ERR("Trying to allocate test_char_dev twice");
		return -EBUSY;
	}

	core_info->test_char_dev = kzalloc(sizeof(*(core_info->test_char_dev)),
					   GFP_KERNEL);
	if (!core_info->test_char_dev) {
		CG2900_ERR("Couldn't allocate test_char_dev");
		return -ENOMEM;
	}

	/* Initialize the RX queue */
	skb_queue_head_init(&core_info->test_char_dev->rx_queue);

	/* Prepare miscdevice struct before registering the device */
	core_info->test_char_dev->test_miscdev.minor = MISC_DYNAMIC_MINOR;
	core_info->test_char_dev->test_miscdev.name = CG2900_CDEV_NAME;
	core_info->test_char_dev->test_miscdev.fops = &test_char_dev_fops;
	core_info->test_char_dev->test_miscdev.parent = core_info->dev;

	err = misc_register(&core_info->test_char_dev->test_miscdev);
	if (err) {
		CG2900_ERR("Error %d registering misc dev!", err);
		kfree(core_info->test_char_dev);
		core_info->test_char_dev = NULL;
		return err;
	}

	return 0;
}

/**
 * test_char_dev_destroy() - Clean up after test_char_dev_create().
 */
static void test_char_dev_destroy(void)
{
	int err;

	if (!core_info->test_char_dev)
		return;

	err = misc_deregister(&core_info->test_char_dev->test_miscdev);
	if (err)
		CG2900_ERR("Error %d deregistering misc dev!", err);

	/* Clean the message queue */
	skb_queue_purge(&core_info->test_char_dev->rx_queue);

	kfree(core_info->test_char_dev);
	core_info->test_char_dev = NULL;
}

/**
 * open_transport() - Open the CG2900 transport for data transfers.
 *
 * Returns:
 *   0 if there is no error,
 *   -EACCES if write to transport failed,
 *   -EIO if transport has not been selected or chip did not answer to commands.
 */
static int open_transport(void)
{
	int err = 0;
	struct trans_info *trans_info = core_info->trans_info;

	CG2900_INFO("open_transport");

	if (trans_info && trans_info->cb.open) {
		err = trans_info->cb.open(&trans_info->dev);
		if (err)
			CG2900_ERR("Transport open failed (%d)", err);
	}

	if (!err)
		SET_TRANSPORT_STATE(TRANS_OPENED);

	return err;
}

/**
 * close_transport() - Close the CG2900 transport for data transfers.
 */
static void close_transport(void)
{
	struct trans_info *trans_info = core_info->trans_info;

	CG2900_INFO("close_transport");

	SET_TRANSPORT_STATE(TRANS_CLOSED);

	if (trans_info && trans_info->cb.close) {
		int err = trans_info->cb.close(&trans_info->dev);
		if (err)
			CG2900_ERR("Transport close failed (%d)", err);
	}
}

/**
 * create_work_item() - Create work item and add it to the work queue.
 * @wq:		work queue struct where the work will be added.
 * @work_func:	Work function.
 * @data:	Private data for the work.
 *
 * Returns:
 *   0 if there is no error.
 *   -EBUSY if not possible to queue work.
 *   -ENOMEM if allocation fails.
 */
static int create_work_item(struct workqueue_struct *wq, work_func_t work_func,
			    void *data)
{
	struct ccd_work_struct *new_work;
	int err;

	new_work = kmalloc(sizeof(*new_work), GFP_ATOMIC);
	if (!new_work) {
		CG2900_ERR("Failed to alloc memory for ccd_work_struct!");
		return -ENOMEM;
	}

	new_work->data = data;
	INIT_WORK(&new_work->work, work_func);

	err = queue_work(wq, &new_work->work);
	if (!err) {
		CG2900_ERR("Failed to queue work_struct because it's already "
			   "in the queue!");
		kfree(new_work);
		return -EBUSY;
	}

	return 0;
}

/**
 * work_hw_registered() - Called when the interface to HW has been established.
 * @work:	Reference to work data.
 *
 * Since there now is a transport identify the connected chip and decide which
 * chip handler to use.
 */
static void work_hw_registered(struct work_struct *work)
{
	struct ccd_work_struct *current_work = NULL;
	bool run_shutdown = true;
	struct cg2900_chip_callbacks *cb;
	struct cg2900_h4_channels *chan;
	struct trans_info *trans_info = core_info->trans_info;

	CG2900_INFO("work_hw_registered");

	if (!work) {
		CG2900_ERR("work == NULL");
		return;
	}

	current_work = container_of(work, struct ccd_work_struct, work);

	SET_MAIN_STATE(CORE_INITIALIZING);
	SET_BOOT_STATE(BOOT_NOT_STARTED);

	/*
	 * This might look strange, but we need to read out
	 * the revision info in order to be able to shutdown the chip properly.
	 */
	if (trans_info && trans_info->cb.set_chip_power)
		trans_info->cb.set_chip_power(true);

	/* Wait 100ms before continuing to be sure that the chip is ready */
	schedule_timeout_interruptible(msecs_to_jiffies(CHIP_READY_TIMEOUT));

	/*
	 * Transmit HCI reset command to ensure the chip is using
	 * the correct transport
	 */
	create_and_send_bt_cmd(reset_cmd_req, sizeof(reset_cmd_req));

	/* Wait up to 500 milliseconds for revision to be read out */
	CG2900_DBG("Wait up to 500 milliseconds for revision to be read.");
	wait_event_interruptible_timeout(main_wait_queue,
		(BOOT_READY == core_info->boot_state),
		msecs_to_jiffies(REVISION_READOUT_TIMEOUT));

	/*
	 * If we are in BOOT_READY we have a good revision.
	 * Otherwise handle this as an error and switch to default handler.
	 */
	if (BOOT_READY != core_info->boot_state) {
		chip_not_detected();
		run_shutdown = false;
	}

	/* Read out the channels for connected chip */
	cb = &(core_info->chip_dev.cb);
	chan = &(core_info->h4_channels);
	if (cb->get_h4_channel) {
		/* Get the H4 channel ID for all channels */
		cb->get_h4_channel(CG2900_BT_CMD, &(chan->bt_cmd_channel));
		cb->get_h4_channel(CG2900_BT_ACL, &(chan->bt_acl_channel));
		cb->get_h4_channel(CG2900_BT_EVT, &(chan->bt_evt_channel));
		cb->get_h4_channel(CG2900_GNSS, &(chan->gnss_channel));
		cb->get_h4_channel(CG2900_FM_RADIO, &(chan->fm_radio_channel));
		cb->get_h4_channel(CG2900_DEBUG, &(chan->debug_channel));
		cb->get_h4_channel(CG2900_STE_TOOLS,
				   &(chan->ste_tools_channel));
		cb->get_h4_channel(CG2900_HCI_LOGGER,
				   &(chan->hci_logger_channel));
		cb->get_h4_channel(CG2900_US_CTRL, &(chan->us_ctrl_channel));
		cb->get_h4_channel(CG2900_CORE, &(chan->core_channel));
	}

	/*
	 * Now it is time to shutdown the controller to reduce
	 * power consumption until any users register
	 */
	if (run_shutdown)
		chip_shutdown();
	else
		cg2900_chip_shutdown_finished(0);

	kfree(current_work);
}

/*
 *	CG2900 API functions
 */

struct cg2900_device *cg2900_register_user(char  *name,
					   struct cg2900_callbacks *cb)
{
	struct cg2900_device *current_dev;
	int err;
	struct trans_info *trans_info = core_info->trans_info;

	CG2900_INFO("cg2900_register_user %s", name);

	BUG_ON(!core_info);

	/* Wait for state CORE_IDLE or CORE_ACTIVE. */
	err = wait_event_interruptible_timeout(main_wait_queue,
			(CORE_IDLE == core_info->main_state ||
			 CORE_ACTIVE == core_info->main_state),
			msecs_to_jiffies(LINE_TOGGLE_DETECT_TIMEOUT));

	if (err <= 0) {
		if (CORE_INITIALIZING == core_info->main_state)
			CG2900_ERR("Transport not opened");
		else
			CG2900_ERR("cg2900_register_user currently busy (0x%X)."
				   " Try again.", core_info->main_state);
		return NULL;
	}

	/* Allocate device */
	current_dev = kzalloc(sizeof(*current_dev), GFP_ATOMIC);
	if (!current_dev) {
		CG2900_ERR("Couldn't allocate current dev");
		goto error_handling;
	}

	if (!core_info->chip_dev.cb.get_h4_channel) {
		CG2900_ERR("No channel handler registered");
		goto error_handling;
	}
	err = core_info->chip_dev.cb.get_h4_channel(name,
						    &(current_dev->h4_channel));
	if (err) {
		CG2900_ERR("Couldn't find H4 channel for %s", name);
		goto error_handling;
	}
	current_dev->dev = core_info->dev;
	current_dev->cb = kmalloc(sizeof(*(current_dev->cb)),
					 GFP_ATOMIC);
	if (!current_dev->cb) {
		CG2900_ERR("Couldn't allocate cb ");
		goto error_handling;
	}
	memcpy((char *)current_dev->cb, (char *)cb,
	       sizeof(*(current_dev->cb)));

	/* Retrieve pointer to the correct CG2900 Core user structure */
	err = add_h4_user(current_dev, name);

	if (!err) {
		CG2900_DBG("H:4 channel 0x%X registered",
			   current_dev->h4_channel);
	} else {
		CG2900_ERR("H:4 channel 0x%X already registered "
			   "or other error (%d)",
			   current_dev->h4_channel, err);
		goto error_handling;
	}

	if (CORE_ACTIVE != core_info->main_state &&
	    core_info->users.nbr_of_users == 1) {
		/* Open transport and start-up the chip */
		if (trans_info && trans_info->cb.set_chip_power)
			trans_info->cb.set_chip_power(true);

		/* Wait 100ms to be sure that the chip is ready */
		schedule_timeout_interruptible(
				msecs_to_jiffies(CHIP_READY_TIMEOUT));

		err = open_transport();
		if (err) {
			/* Close the transport and power off the chip */
			close_transport();
			/*
			 * Remove the user. If there is no error it will be
			 * freed as well.
			 */
			remove_h4_user(&current_dev);
			goto finished;
		}

		chip_startup();

		/* Wait up to 15 seconds for chip to start */
		CG2900_DBG("Wait up to 15 seconds for chip to start..");
		wait_event_interruptible_timeout(main_wait_queue,
			(CORE_ACTIVE == core_info->main_state ||
			 CORE_IDLE   == core_info->main_state),
			msecs_to_jiffies(CHIP_STARTUP_TIMEOUT));
		if (CORE_ACTIVE != core_info->main_state) {
			CG2900_ERR("ST-Ericsson CG2900 driver failed to "
				     "start");

			/* Close the transport and power off the chip */
			close_transport();

			/*
			 * Remove the user. If there is no error it will be
			 * freed as well.
			 */
			remove_h4_user(&current_dev);

			/* Chip shut-down finished, set correct state. */
			SET_MAIN_STATE(CORE_IDLE);
		}
	}
	goto finished;

error_handling:
	free_user_dev(&current_dev);
finished:
	return current_dev;
}
EXPORT_SYMBOL(cg2900_register_user);

void cg2900_deregister_user(struct cg2900_device *dev)
{
	int h4_channel;
	int err = 0;

	CG2900_INFO("cg2900_deregister_user");

	BUG_ON(!core_info);

	if (!dev) {
		CG2900_ERR("Calling with NULL pointer");
		return;
	}

	h4_channel = dev->h4_channel;

	/* Remove the user. If there is no error it will be freed as well */
	err = remove_h4_user(&dev);
	if (err) {
		CG2900_ERR("Trying to deregister non-registered "
			   "H:4 channel 0x%X or other error %d",
			   h4_channel, err);
		return;
	}

	CG2900_DBG("H:4 channel 0x%X deregistered", h4_channel);

	if (0 != core_info->users.nbr_of_users)
		/* This was not the last user, we're done. */
		return;

	if (CORE_IDLE == core_info->main_state)
		/* Chip has already been shut down. */
		return;

	SET_MAIN_STATE(CORE_CLOSING);
	chip_shutdown();

	/* Wait up to 15 seconds for chip to shut-down */
	CG2900_DBG("Wait up to 15 seconds for chip to shut-down..");
	wait_event_interruptible_timeout(main_wait_queue,
				(CORE_IDLE == core_info->main_state),
				msecs_to_jiffies(CHIP_SHUTDOWN_TIMEOUT));

	/* Force shutdown if we timed out */
	if (CORE_IDLE != core_info->main_state) {
		CG2900_ERR("ST-Ericsson Connectivity Controller Driver "
			   "was shut-down with problems.");

		/* Close the transport and power off the chip */
		close_transport();

		/* Chip shut-down finished, set correct state. */
		SET_MAIN_STATE(CORE_IDLE);
	}
}
EXPORT_SYMBOL(cg2900_deregister_user);

int cg2900_reset(struct cg2900_device *dev)
{
	CG2900_INFO("cg2900_reset");

	BUG_ON(!core_info);

	SET_MAIN_STATE(CORE_RESETING);

	/* Shutdown the chip */
	chip_shutdown();

	/*
	 * Inform all registered users about the reset and free the user devices
	 * Don't send reset for debug and logging channels
	 */
	handle_reset_of_user(&(core_info->users.bt_cmd));
	handle_reset_of_user(&(core_info->users.bt_audio));
	handle_reset_of_user(&(core_info->users.bt_acl));
	handle_reset_of_user(&(core_info->users.bt_evt));
	handle_reset_of_user(&(core_info->users.fm_radio));
	handle_reset_of_user(&(core_info->users.fm_radio_audio));
	handle_reset_of_user(&(core_info->users.gnss));
	handle_reset_of_user(&(core_info->users.core));

	core_info->users.nbr_of_users = 0;

	/* Reset finished. We are now idle until first user is registered */
	SET_MAIN_STATE(CORE_IDLE);

	/*
	 * Send wake-up since this might have been called from a failed boot.
	 * No harm done if it is a CG2900 Core user who called.
	 */
	wake_up_interruptible(&main_wait_queue);

	return 0;
}
EXPORT_SYMBOL(cg2900_reset);

struct sk_buff *cg2900_alloc_skb(unsigned int size, gfp_t priority)
{
	struct sk_buff *skb;

	CG2900_INFO("cg2900_alloc_skb");
	CG2900_DBG("size %d bytes", size);

	/* Allocate the SKB and reserve space for the header */
	skb = alloc_skb(size + CG2900_SKB_RESERVE, priority);
	if (skb)
		skb_reserve(skb, CG2900_SKB_RESERVE);

	return skb;
}
EXPORT_SYMBOL(cg2900_alloc_skb);

int cg2900_write(struct cg2900_device *dev, struct sk_buff *skb)
{
	int err = 0;
	u8 *h4_header;
	struct cg2900_chip_callbacks *cb;

	CG2900_DBG_DATA("cg2900_write");
   if(core_info != bak_core_info)
      core_info = bak_core_info;

	BUG_ON(!core_info);

	if (!dev) {
		CG2900_ERR("cg2900_write with no device");
		return -EINVAL;
	}

	if (!skb) {
		CG2900_ERR("cg2900_write with no sk_buffer");
		return -EINVAL;
	}

	CG2900_DBG_DATA("Length %d bytes", skb->len);

	if (core_info->h4_channels.hci_logger_channel == dev->h4_channel) {
		/*
		 * Treat the HCI logger write differently.
		 * A write can only mean a change of configuration.
		 */
		err = enable_hci_logger(skb);
	} else if (core_info->h4_channels.core_channel == dev->h4_channel) {
		CG2900_ERR("Not possible to write data on core channel, "
			   "it only supports enable / disable chip");
		err = -EPERM;
	} else if (CORE_ACTIVE == core_info->main_state) {
		/*
		 * Move the data pointer to the H:4 header position and
		 * store the H4 header.
		 */
		h4_header = skb_push(skb, CG2900_SKB_RESERVE);
		*h4_header = (u8)dev->h4_channel;

		/*
		 * Check if the chip handler wants to handle this packet.
		 * If not, send it to the transport.
		 */
		cb = &(core_info->chip_dev.cb);
		if (!cb->data_to_chip ||
		    !(cb->data_to_chip(&(core_info->chip_dev), dev, skb)))
			transmit_skb_to_chip(skb, dev->logger_enabled);
	} else {
		CG2900_ERR("Trying to transmit data when CG2900 Core is not "
			     "active");
		err = -EACCES;
	}

	return err;
}
EXPORT_SYMBOL(cg2900_write);

bool cg2900_get_local_revision(struct cg2900_rev_data *rev_data)
{
	BUG_ON(!core_info);

	if (!rev_data) {
		CG2900_ERR("Calling with rev_data NULL");
		return false;
	}

	if (!core_info->local_chip_info.version_set)
		return false;

	rev_data->revision = core_info->local_chip_info.hci_revision;
	rev_data->sub_version = core_info->local_chip_info.lmp_pal_subversion;

	return true;
}
EXPORT_SYMBOL(cg2900_get_local_revision);

int cg2900_register_chip_driver(struct cg2900_id_callbacks *cb)
{
	struct chip_handler_item *item;

	CG2900_INFO("cg2900_register_chip_driver");

	if (!cb) {
		CG2900_ERR("NULL supplied as cb");
		return -EINVAL;
	}

	item = kzalloc(sizeof(*item), GFP_ATOMIC);
	if (!item) {
		CG2900_ERR("Failed to alloc memory!");
		return -ENOMEM;
	}

	memcpy(&(item->cb), cb, sizeof(cb));
	list_add_tail(&item->list, &chip_handlers);
	return 0;
}
EXPORT_SYMBOL(cg2900_register_chip_driver);

int cg2900_register_trans_driver(struct cg2900_trans_callbacks *cb, void *data)
{
	int err;

	BUG_ON(!core_info);

	if (core_info->trans_info) {
		CG2900_ERR("trans_info already exists");
		return -EACCES;
	}

	core_info->trans_info = kzalloc(sizeof(*(core_info->trans_info)),
					GFP_KERNEL);
	if (!core_info->trans_info) {
		CG2900_ERR("Could not allocate trans_info");
		return -ENOMEM;
	}

	memcpy(&(core_info->trans_info->cb), cb, sizeof(*cb));
	core_info->trans_info->dev.dev = core_info->dev;
	core_info->trans_info->dev.user_data = data;

	err = create_work_item(core_info->wq, work_hw_registered, NULL);
	if (err) {
		CG2900_ERR("Could not create work item (%d) "
			     "work_hw_registered", err);
	}

	return err;
}
EXPORT_SYMBOL(cg2900_register_trans_driver);

int cg2900_deregister_trans_driver(void)
{
	BUG_ON(!core_info);

	SET_MAIN_STATE(CORE_INITIALIZING);
	SET_TRANSPORT_STATE(TRANS_INITIALIZING);

	if (!core_info->trans_info)
		return -EACCES;

	kfree(core_info->trans_info);
	core_info->trans_info = NULL;

	return 0;
}
EXPORT_SYMBOL(cg2900_deregister_trans_driver);

int cg2900_chip_startup_finished(int err)
{
	CG2900_INFO("cg2900_chip_startup_finished (%d)", err);

	if (err)
		/* Shutdown the chip */
		chip_shutdown();
	else
		SET_MAIN_STATE(CORE_ACTIVE);

	wake_up_interruptible(&main_wait_queue);

	return 0;
}
EXPORT_SYMBOL(cg2900_chip_startup_finished);

int cg2900_chip_shutdown_finished(int err)
{
	CG2900_INFO("cg2900_chip_shutdown_finished (%d)", err);

	/* Close the transport, which will power off the chip */
	close_transport();

	/* Chip shut-down finished, set correct state and wake up the chip. */
	SET_MAIN_STATE(CORE_IDLE);
	wake_up_interruptible(&main_wait_queue);

	return 0;
}
EXPORT_SYMBOL(cg2900_chip_shutdown_finished);

int cg2900_send_to_chip(struct sk_buff *skb, bool use_logger)
{
	transmit_skb_to_chip(skb, use_logger);
	return 0;
}
EXPORT_SYMBOL(cg2900_send_to_chip);

struct cg2900_device *cg2900_get_bt_cmd_dev(void)
{
	if (core_info)
		return core_info->users.bt_cmd;
	else
		return NULL;
}
EXPORT_SYMBOL(cg2900_get_bt_cmd_dev);

struct cg2900_device *cg2900_get_fm_radio_dev(void)
{
	if (core_info)
		return core_info->users.fm_radio;
	else
		return NULL;
}
EXPORT_SYMBOL(cg2900_get_fm_radio_dev);

struct cg2900_device *cg2900_get_bt_audio_dev(void)
{
	if (core_info)
		return core_info->users.bt_audio;
	else
		return NULL;
}
EXPORT_SYMBOL(cg2900_get_bt_audio_dev);

struct cg2900_device *cg2900_get_fm_audio_dev(void)
{
	if (core_info)
		return core_info->users.fm_radio_audio;
	else
		return NULL;
}
EXPORT_SYMBOL(cg2900_get_fm_audio_dev);

struct cg2900_hci_logger_config *cg2900_get_hci_logger_config(void)
{
	if (core_info)
		return &(core_info->hci_logger_config);
	else
		return NULL;
}
EXPORT_SYMBOL(cg2900_get_hci_logger_config);

unsigned long cg2900_get_sleep_timeout(void)
{
	if (!sleep_timeout_ms)
		return 0;

	return msecs_to_jiffies(sleep_timeout_ms);
}
EXPORT_SYMBOL(cg2900_get_sleep_timeout);

void cg2900_data_from_chip(struct sk_buff *skb)
{
	struct cg2900_device *dev = NULL;
	u8 h4_channel;
	int err = 0;
	struct cg2900_chip_callbacks *cb;
	struct sk_buff *skb_log;
	struct cg2900_device *logger;

	CG2900_INFO("cg2900_data_from_chip");

	if (!skb) {
		CG2900_ERR("No data supplied");
		return;
	}

	h4_channel = *(skb->data);

	/*
	 * First check if this is the response for something
	 * we have sent internally.
	 */
	if ((core_info->main_state == CORE_BOOTING ||
	     core_info->main_state == CORE_INITIALIZING) &&
	    (HCI_BT_EVT_H4_CHANNEL == h4_channel) &&
	    handle_rx_data_bt_evt(skb)) {
		CG2900_DBG("Received packet handled internally");
		return;
	}

	/* Find out where to route the data */
	err = find_h4_user(h4_channel, &dev, skb);

	/* Check if the chip handler wants to deal with the packet. */
	cb = &(core_info->chip_dev.cb);
	if (!err && cb->data_from_chip &&
	    cb->data_from_chip(&(core_info->chip_dev), dev, skb))
		return;

	if (err || !dev) {
		CG2900_ERR("H:4 channel: 0x%X,  does not match device",
			     h4_channel);
		kfree_skb(skb);
		return;
	}

	/*
	 * If HCI logging is enabled for this channel, copy the data to
	 * the HCI logging output.
	 */
	logger = core_info->users.hci_logger;
	if (!logger || !dev->logger_enabled)
		goto transmit;

	/*
	 * Alloc a new sk_buffer and copy the data into it.
	 * Then send it to the HCI logger.
	 */
	skb_log = alloc_skb(skb->len + 1, GFP_ATOMIC);
	if (!skb_log) {
		CG2900_ERR("Couldn't allocate skb_log");
		goto transmit;
	}

	memcpy(skb_put(skb_log, skb->len), skb->data, skb->len);
	skb_log->data[0] = (u8) LOGGER_DIRECTION_RX;

	if (logger->cb->read_cb)
		logger->cb->read_cb(logger, skb_log);

transmit:
	/* Remove the H4 header */
	(void)skb_pull(skb, CG2900_SKB_RESERVE);

	/* Call the Read callback */
	if (dev->cb->read_cb)
		dev->cb->read_cb(dev, skb);
	else
		kfree_skb(skb);
}
EXPORT_SYMBOL(cg2900_data_from_chip);

/*
 * Module INIT and EXIT functions
 */

//static int __init cg2900_init(void)
static int cg2900_init(struct work_struct *work)
{
	int err = 0;
	dev_t temp_devt;
	struct cg2900_driver_data *dev_data;
	printk("cg2900_init\n\n\n\n\cg2900_init\n\n\n\n");
	CG2900_INFO("cg2900_init");

//	schedule_timeout_interruptible(msecs_to_jiffies(DELAY_5_SEC));

	err = cg2900_devices_init();
	if (err) {
		CG2900_ERR("Couldn't initialize cg2900_devices");
		return err;
	}

	core_info = kzalloc(sizeof(*core_info), GFP_KERNEL);
	bak_core_info = core_info;
	if (!core_info) {
		CG2900_ERR("Couldn't allocate core_info");
		return -ENOMEM;
	}

	/* Set the internal states */
	core_info->main_state = CORE_INITIALIZING;
	core_info->boot_state = BOOT_NOT_STARTED;
	core_info->transport_state = TRANS_INITIALIZING;

	/* Get the H4 channel ID for all channels */
	core_info->h4_channels.bt_cmd_channel = HCI_BT_CMD_H4_CHANNEL;
	core_info->h4_channels.bt_acl_channel = HCI_BT_ACL_H4_CHANNEL;
	core_info->h4_channels.bt_evt_channel = HCI_BT_EVT_H4_CHANNEL;
#if 0 // Hemant: Swapping GNSS and FM Channels	
	core_info->h4_channels.gnss_channel = HCI_FM_RADIO_H4_CHANNEL;
	core_info->h4_channels.fm_radio_channel = HCI_GNSS_H4_CHANNEL;
#else	
	core_info->h4_channels.gnss_channel = HCI_GNSS_H4_CHANNEL;
	core_info->h4_channels.fm_radio_channel = HCI_FM_RADIO_H4_CHANNEL;
#endif	
	/* ++Hemant:  Add selective debug message */		
	printk(KERN_ERR "cg2900_init: core_info->main_state = 0x%0x, core_info->boot_state = 0x%0x core_info->transport_state = 0x%0x core_info->h4_channels.gnss_channel = 0x%0x\n", core_info->main_state, core_info->boot_state, core_info->transport_state, core_info->h4_channels.gnss_channel);
	/* --Hemant:  Add selective debug message */		

	core_info->wq = create_singlethread_workqueue(CORE_WQ_NAME);
	if (!core_info->wq) {
		CG2900_ERR("Could not create workqueue");
		err = -ENOMEM;
		goto error_handling;
	}

#if 0 //Daniel, to get Major ID dynamically
	/* Create major device number */
	err = alloc_chrdev_region(&temp_devt, 0, MAX_NBR_OF_DEVS,
				  CG2900_DEVICE_NAME);
	if (err) {
		CG2900_ERR("Can't get major number (%d)", err);
		goto error_handling_destroy_wq;
	}
#else //to avoid the Major ID confliction with RIL, use the ID recommended by Samsung Kernel team.
	temp_devt = MKDEV(CG2900_DEVICE_MAJOR_ID, 0);
	err = register_chrdev_region(temp_devt, 1, CG2900_DEVICE_NAME);
	CG2900_ERR("get major number (%d) %d", err, temp_devt);

	if (err) {
		CG2900_ERR("Can't get major number (%d)", err);
		goto error_handling_destroy_wq;
	}
#endif

	/* Create the device class */
	core_info->class = class_create(THIS_MODULE, CG2900_CLASS_NAME);
	if (IS_ERR(core_info->class)) {
		CG2900_ERR("Error creating class");
		err = (int)core_info->class;
		goto error_handling_class_create;
	}

	/* Create the main device */
	dev_data = kzalloc(sizeof(*dev_data), GFP_KERNEL);
	if (!dev_data) {
		CG2900_ERR("Couldn't allocate dev_data");
		err = -ENOMEM;
		goto error_handling_dev_data_alloc;
	}

	core_info->dev = device_create(core_info->class, NULL, temp_devt,
				       dev_data, CG2900_DEVICE_NAME "%d",
				       dev_data->next_free_minor);
	if (IS_ERR(core_info->dev)) {
		CG2900_ERR("Error creating main device");
		err = (int)core_info->dev;
		goto error_handling_dev_create;
	}
	core_info->chip_dev.dev = core_info->dev;
	(dev_data->next_free_minor)++;

	/* Check if verification mode is enabled to stub transport interface. */
	if (char_dev_usage & CG2900_CHAR_TEST_DEV) {
		/* Create and add test char device. */
		if (test_char_dev_create() == 0) {
			CG2900_INFO("Test char device detected, CG2900 Core "
				    "will be stubbed.");
		}
		char_dev_usage &= ~CG2900_CHAR_TEST_DEV;
	}

	/* Initialize the character devices */
	err = cg2900_char_devices_init(char_dev_usage, core_info->dev);
	if (err) {
		CG2900_ERR("cg2900_char_devices_init failed %d", err);
		goto error_handling_register;
	}

	/* Initialize the CG2900 Chip Module */
	err = cg2900_chip_init();
	if (err) {
		CG2900_ERR("cg2900_chip_init failed %d", err);
		goto error_handling_register;
	}

	/* Initialize the STLC2690 Chip Module */
	err = stlc2690_chip_init();
	if (err) {
		CG2900_ERR("stlc2690_chip_init failed %d", err);
		goto error_handling_register;
	}

	/* Initialize the CG2900 Audio Module */
	err = cg2900_audio_init();
	if (err) {
		CG2900_ERR("cg2900_audio_init failed %d", err);
		goto error_handling_register;
	}

	/* Initialize the UART Driver */
	err = uart_init();
	if (err) {
		CG2900_ERR("uart_init failed %d", err);
		goto error_handling_register;
	}

	return 0;

error_handling_register:
	device_destroy(core_info->class, core_info->dev->devt);
	core_info->dev = NULL;
error_handling_dev_create:
	kfree(dev_data);
error_handling_dev_data_alloc:
	class_destroy(core_info->class);
error_handling_class_create:
	unregister_chrdev_region(temp_devt, 32);
error_handling_destroy_wq:
	destroy_workqueue(core_info->wq);
error_handling:
	kfree(core_info);
	core_info = NULL;
	return err;
}

/**
 * cg2900_exit() - Remove module.
 */
static void __exit cg2900_exit(void)
{
	dev_t temp_devt;

	CG2900_INFO("cg2900_exit");

	if (!core_info) {
		CG2900_ERR("CG2900 Core not initiated");
		return;
	}

	/* Remove the UART Module */
	uart_exit();

	/* Remove the CG2900 Audio Module */
	cg2900_audio_exit();

	/* Remove the STLC2690 Chip Module */
	stlc2690_chip_exit();

	/* Remove the CG2900 Chip Module */
	cg2900_chip_exit();


	/* Remove initialized character devices */
	cg2900_char_devices_exit();

	test_char_dev_destroy();

	/* Free the user devices */
	free_user_dev(&(core_info->users.bt_cmd));
	free_user_dev(&(core_info->users.bt_acl));
	free_user_dev(&(core_info->users.bt_evt));
	free_user_dev(&(core_info->users.fm_radio));
	free_user_dev(&(core_info->users.gnss));
	free_user_dev(&(core_info->users.debug));
	free_user_dev(&(core_info->users.ste_tools));
	free_user_dev(&(core_info->users.hci_logger));
	free_user_dev(&(core_info->users.us_ctrl));
	free_user_dev(&(core_info->users.core));

	temp_devt = core_info->dev->devt;
	device_destroy(core_info->class, temp_devt);
	kfree(dev_get_drvdata(core_info->dev));
	class_destroy(core_info->class);
	unregister_chrdev_region(temp_devt, 32);

	destroy_workqueue(core_info->wq);
	destroy_workqueue(test_init_wq);

	kfree(core_info);
	core_info = NULL;

	cg2900_devices_exit();
}

static int __init cg2900_new_init()
{
	int err;

	CG2900_INFO("cg2900_new_init");

	test_init_wq = create_singlethread_workqueue(
			TEST_INIT_WQ_NAME);
	if (!test_init_wq) {
		CG2900_ERR("Could not create Test Init workqueue");
		err = -ENOMEM;
		goto error_handling;
	}
	create_work_item(test_init_wq, cg2900_init, NULL);
	err = 0;

error_handling:
	return err;
}

//module_init(cg2900_init);
module_init(cg2900_new_init);
module_exit(cg2900_exit);

module_param(sleep_timeout_ms, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(sleep_timeout_ms,
		 "Sleep timeout for data transmissions:\n"
		 "\t0 = disable <default>\n"
		 "\t>0 = sleep timeout in milliseconds");

module_param(char_dev_usage, int, S_IRUGO);
MODULE_PARM_DESC(char_dev_usage, "Character devices to enable (bitmask):\n"
		 "\t0x00 = No char devs\n"
		 "\t0x01 = BT\n"
		 "\t0x02 = FM radio\n"
		 "\t0x04 = GNSS\n"
		 "\t0x08 = Debug\n"
		 "\t0x10 = STE tools\n"
		 "\t0x20 = CG2900 Stub\n"
		 "\t0x40 = HCI Logger\n"
		 "\t0x80 = US Ctrl\n"
		 "\t0x100 = CG2900 test\n"
		 "\t0x200 = BT Audio\n"
		 "\t0x400 = FM Audio\n"
		 "\t0x800 = Core");

module_param(cg2900_debug_level, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(cg2900_debug_level,
		 "Debug level. Default 1. Possible values:\n"
		 "\t0  = No debug\n"
		 "\t1  = Error prints\n"
		 "\t10 = General info, e.g. function entries\n"
		 "\t20 = Debug info, e.g. steps in a functionality\n"
		 "\t25 = Data info, i.e. prints when data is transferred\n"
		 "\t30 = Data content, i.e. contents of the transferred data");

module_param_array(bd_address, byte, &bd_addr_count,
		   S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(bd_address,
		 "Bluetooth Device address. "
		 "Default 0x00 0x80 0xDE 0xAD 0xBE 0xEF. "
		 "Enter as comma separated value.");

module_param(default_hci_revision, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(default_hci_revision,
		 "Default HCI revision according to Bluetooth Assigned "
		 "Numbers.");

module_param(default_manufacturer, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(default_manufacturer,
		 "Default Manufacturer according to Bluetooth Assigned "
		 "Numbers.");

module_param(default_sub_version, int, S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(default_sub_version,
		 "Default HCI sub-version according to Bluetooth Assigned "
		 "Numbers.");

MODULE_AUTHOR("Par-Gunnar Hjalmdahl ST-Ericsson");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Linux Bluetooth HCI H:4 CG2900 Connectivity Device Driver");
