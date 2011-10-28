/*
 * include/linux/mfd/cg2900.h
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

#ifndef _CG2900_H_
#define _CG2900_H_

#include <linux/skbuff.h>
#include <linux/bitops.h>

#define CG2900_MAX_NAME_SIZE 30

/*
 * Channel names to use when registering to CG2900 driver
 */

/** CG2900_BT_CMD - Bluetooth HCI H4 channel for Bluetooth commands.
 */
#define CG2900_BT_CMD		"cg2900_bt_cmd"

/** CG2900_BT_ACL - Bluetooth HCI H4 channel for Bluetooth ACL data.
 */
#define CG2900_BT_ACL		"cg2900_bt_acl"

/** CG2900_BT_EVT - Bluetooth HCI H4 channel for Bluetooth events.
 */
#define CG2900_BT_EVT		"cg2900_bt_evt"

/** CG2900_FM_RADIO - Bluetooth HCI H4 channel for FM radio.
 */
#define CG2900_FM_RADIO		"cg2900_fm_radio"

/** CG2900_GNSS - Bluetooth HCI H4 channel for GNSS.
 */
#define CG2900_GNSS		"cg2900_gnss"

/** CG2900_DEBUG - Bluetooth HCI H4 channel for internal debug data.
 */
#define CG2900_DEBUG		"cg2900_debug"

/** CG2900_STE_TOOLS - Bluetooth HCI H4 channel for development tools data.
 */
#define CG2900_STE_TOOLS	"cg2900_ste_tools"

/** CG2900_HCI_LOGGER - BT channel for logging all transmitted H4 packets.
 * Data read is copy of all data transferred on the other channels.
 * Only write allowed is configuration of the HCI Logger.
 */
#define CG2900_HCI_LOGGER	"cg2900_hci_logger"

/** CG2900_US_CTRL - Channel for user space init and control of CG2900.
 */
#define CG2900_US_CTRL		"cg2900_us_ctrl"

/** CG2900_BT_AUDIO - HCI Channel for BT audio configuration commands.
 * Maps to Bluetooth command and event channels.
 */
#define CG2900_BT_AUDIO		"cg2900_bt_audio"

/** CG2900_FM_RADIO_AUDIO - HCI channel for FM audio configuration commands.
 * Maps to FM Radio channel.
 */
#define CG2900_FM_RADIO_AUDIO	"cg2900_fm_audio"

/** CG2900_CORE- Channel for keeping ST-Ericsson CG2900 enabled.
 * Opening this channel forces the chip to stay powered.
 * No data can be written to or read from this channel.
 */
#define CG2900_CORE		"cg2900_core"

/*
 * Char device bits to use in bitmask when starting module
 */

/** CG2900_NO_CHAR_DEV - Module parameter for no char devices to be initiated.
 */
#define CG2900_NO_CHAR_DEV			0x00000000

/** CG2900_CHAR_DEV_BT - Module parameter for character device Bluetooth.
 */
#define CG2900_CHAR_DEV_BT			BIT(0)

/** CG2900_CHAR_DEV_FM_RADIO - Module parameter for character device FM radio.
 */
#define CG2900_CHAR_DEV_FM_RADIO		BIT(1)

/** CG2900_CHAR_DEV_GNSS - Module parameter for character device GNSS.
 */
#define CG2900_CHAR_DEV_GNSS			BIT(2)

/** CG2900_CHAR_DEV_DEBUG - Module parameter for character device Debug.
 */
#define CG2900_CHAR_DEV_DEBUG			BIT(3)

/** CG2900_CHAR_DEV_STE_TOOLS - Module parameter for character device STE tools.
 */
#define CG2900_CHAR_DEV_STE_TOOLS		BIT(4)

/** CG2900_CHAR_DEV_CCD_DEBUG - Module parameter for character device CCD debug.
 */
#define CG2900_CHAR_DEV_CCD_DEBUG		BIT(5)

/** CG2900_CHAR_DEV_HCI_LOGGER - Module parameter for char device HCI logger.
 */
#define CG2900_CHAR_DEV_HCI_LOGGER		BIT(6)

/** CG2900_CHAR_DEV_US_CTRL - Module parameter for char device user space ctrl.
 */
#define CG2900_CHAR_DEV_US_CTRL			BIT(7)

/** CG2900_CHAR_TEST_DEV - Module parameter for character device test device.
 */
#define CG2900_CHAR_TEST_DEV			BIT(8)

/** CG2900_CHAR_DEV_BT_AUDIO - Module parameter for char device BT CMD audio.
 */
#define CG2900_CHAR_DEV_BT_AUDIO		BIT(9)

/** CG2900_CHAR_DEV_FM_RADIO_AUDIO - Module parameter for char device FM audio.
 */
#define CG2900_CHAR_DEV_FM_RADIO_AUDIO		BIT(10)

/** CG2900_CHAR_DEV_CORE - Module parameter for character device core.
 */
#define CG2900_CHAR_DEV_CORE			BIT(11)

/** CG2900_CHAR_TEST_DEV - Module parameter for all char devs to be initiated.
 */
#define CG2900_ALL_CHAR_DEVS			0xFFFFFFFF

struct cg2900_callbacks;

/**
 * struct cg2900_device - Device structure for CG2900 user.
 * @h4_channel:		HCI H:4 channel used by this device.
 * @cb:			Callback functions registered by this device.
 * @logger_enabled:	true if HCI logger is enabled for this channel,
 *			false otherwise.
 * @user_data:		Arbitrary data used by caller.
 * @dev:		Parent device this driver is connected to.
 *
 * Defines data needed to access an HCI channel.
 */
struct cg2900_device {
	int				h4_channel;
	struct cg2900_callbacks		*cb;
	bool				logger_enabled;
	void				*user_data;
	struct device			*dev;
};

/**
 * struct cg2900_callbacks - Callback structure for CG2900 user.
 * @read_cb:	Callback function called when data is received from
 *		the connectivity controller.
 * @reset_cb:	Callback function called when the connectivity controller has
 *		been reset.
 *
 * Defines the callback functions provided from the caller.
 */
struct cg2900_callbacks {
	void (*read_cb) (struct cg2900_device *dev, struct sk_buff *skb);
	void (*reset_cb) (struct cg2900_device *dev);
};

/**
 * struct cg2900_rev_data - Contains revision data for the local controller.
 * @revision:		Revision of the controller, e.g. to indicate that it is
 *			a CG2900 controller.
 * @sub_version:	Subversion of the controller, e.g. to indicate a certain
 *			tape-out of the controller.
 *
 * The values to match retrieved values to each controller may be retrieved from
 * the manufacturer.
 */
struct cg2900_rev_data {
	int revision;
	int sub_version;
};

/**
 * cg2900_register_user() - Register CG2900 user.
 * @name:	Name of HCI H:4 channel to register to.
 * @cb:		Callback structure to use for the H:4 channel.
 *
 * Returns:
 *   Pointer to CG2900 device structure if successful.
 *   NULL upon failure.
 */
extern struct cg2900_device *cg2900_register_user(char *name,
						  struct cg2900_callbacks *cb);

/**
 * cg2900_deregister_user() - Remove registration of CG2900 user.
 * @dev:	CG2900 device.
 */
extern void cg2900_deregister_user(struct cg2900_device *dev);

/**
 * cg2900_reset() - Reset the CG2900 controller.
 * @dev:	CG2900 device.
 *
 * Returns:
 *   0 if there is no error.
 *   -EACCES if driver has not been initialized.
 */
extern int cg2900_reset(struct cg2900_device *dev);

/**
 * cg2900_alloc_skb() - Alloc an sk_buff structure for CG2900 handling.
 * @size:	Size in number of octets.
 * @priority:	Allocation priority, e.g. GFP_KERNEL.
 *
 * Returns:
 *   Pointer to sk_buff buffer structure if successful.
 *   NULL upon allocation failure.
 */
extern struct sk_buff *cg2900_alloc_skb(unsigned int size, gfp_t priority);

/**
 * cg2900_write() - Send data to the connectivity controller.
 * @dev: CG2900 device.
 * @skb: Data packet.
 *
 * The cg2900_write() function sends data to the connectivity controller.
 * If the return value is 0 the skb will be freed by the driver,
 * otherwise it won't be freed.
 *
 * Returns:
 *   0 if there is no error.
 *   -EACCES if driver has not been initialized or trying to write while driver
 *   is not active.
 *   -EINVAL if NULL pointer was supplied.
 *   -EPERM if operation is not permitted, e.g. trying to write to a channel
 *   that doesn't handle write operations.
 *   Error codes returned from core_enable_hci_logger.
 */
extern int cg2900_write(struct cg2900_device *dev, struct sk_buff *skb);

/**
 * cg2900_get_local_revision() - Read revision of the connected controller.
 * @rev_data:	Revision data structure to fill. Must be allocated by caller.
 *
 * The cg2900_get_local_revision() function returns the revision data of the
 * local controller if available. If data is not available, e.g. because the
 * controller has not yet been started this function will return false.
 *
 * Returns:
 *   true if revision data is available.
 *   false if no revision data is available.
 */
extern bool cg2900_get_local_revision(struct cg2900_rev_data *rev_data);

#endif /* _CG2900_H_ */
