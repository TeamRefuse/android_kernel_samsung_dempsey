/*
 * drivers/mfd/cg2900/cg2900_core.h
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

#ifndef _CG2900_CORE_H_
#define _CG2900_CORE_H_

#include <linux/skbuff.h>
#include <linux/device.h>

#define CG2900_GPS_DSM_SUPPORT

/* Reserve 1 byte for the HCI H:4 header */
#define CG2900_SKB_RESERVE			1

#define BT_BDADDR_SIZE				6

struct cg2900_h4_channels {
	int	bt_cmd_channel;
	int	bt_acl_channel;
	int	bt_evt_channel;
	int	gnss_channel;
	int	fm_radio_channel;
	int	debug_channel;
	int	ste_tools_channel;
	int	hci_logger_channel;
	int	us_ctrl_channel;
	int	core_channel;
};

struct cg2900_driver_data {
	int	next_free_minor;
};

/**
  * struct cg2900_hci_logger_config - Configures the HCI logger.
  * @bt_cmd_enable: Enable BT command logging.
  * @bt_acl_enable: Enable BT ACL logging.
  * @bt_evt_enable: Enable BT event logging.
  * @gnss_enable: Enable GNSS logging.
  * @fm_radio_enable: Enable FM radio logging.
  * @bt_audio_enable: Enable BT audio command logging.
  * @fm_radio_audio_enable: Enable FM radio audio command logging.
  *
  * Set using cg2900_write on CHANNEL_HCI_LOGGER H4 channel.
  */
struct cg2900_hci_logger_config {
	bool bt_cmd_enable;
	bool bt_acl_enable;
	bool bt_evt_enable;
	bool gnss_enable;
	bool fm_radio_enable;
	bool bt_audio_enable;
	bool fm_radio_audio_enable;
};

/**
 * struct cg2900_chip_info - Chip info structure.
 * @manufacturer:	Chip manufacturer.
 * @hci_revision:	Chip revision, i.e. which chip is this.
 * @hci_sub_version:	Chip sub-version, i.e. which tape-out is this.
 *
 * Note that these values match the Bluetooth Assigned Numbers,
 * see http://www.bluetooth.org/
 */
struct cg2900_chip_info {
	int manufacturer;
	int hci_revision;
	int hci_sub_version;
};

struct cg2900_chip_dev;

/**
 * struct cg2900_chip_callbacks - Callback functions registered by chip handler.
 * @chip_startup:		Called when chip is started up.
 * @chip_shutdown:		Called when chip is shut down.
 * @data_to_chip:		Called when data shall be transmitted to chip.
 *				Return true when CPD shall not send it to chip.
 * @data_from_chip:		Called when data shall be transmitted to user.
 *				Return true when packet is taken care of by
 *				Return chip return handler.
 * @get_h4_channel:		Connects channel name with H:4 channel number.
 * @is_bt_audio_user:		Return true if current packet is for
 *				the BT audio user.
 * @is_fm_audio_user:		Return true if current packet is for
 *				the FM audio user.
 * @last_bt_user_removed:	Last BT channel user has been removed.
 * @last_fm_user_removed:	Last FM channel user has been removed.
 * @last_gnss_user_removed:	Last GNSS channel user has been removed.
 *
 * Note that some callbacks may be NULL. They must always be NULL checked before
 * calling.
 */
struct cg2900_chip_callbacks {
	int (*chip_startup)(struct cg2900_chip_dev *dev);
	int (*chip_shutdown)(struct cg2900_chip_dev *dev);
	bool (*data_to_chip)(struct cg2900_chip_dev *dev,
			     struct cg2900_device *cpd_dev,
			     struct sk_buff *skb);
	bool (*data_from_chip)(struct cg2900_chip_dev *dev,
			       struct cg2900_device *cpd_dev,
			       struct sk_buff *skb);
	int (*get_h4_channel)(char *name, int *h4_channel);
	bool (*is_bt_audio_user)(int h4_channel,
				 const struct sk_buff * const skb);
	bool (*is_fm_audio_user)(int h4_channel,
				 const struct sk_buff * const skb);
	void (*last_bt_user_removed)(struct cg2900_chip_dev *dev);
	void (*last_fm_user_removed)(struct cg2900_chip_dev *dev);
	void (*last_gnss_user_removed)(struct cg2900_chip_dev *dev);
};

/**
 * struct cg2900_chip_dev - Chip handler info structure.
 * @dev:	Parent device from CPD.
 * @chip:	Chip info such as manufacturer.
 * @cb:		Callback structure for the chip handler.
 * @user_data:	Arbitrary data set by chip handler.
 */
struct cg2900_chip_dev {
	struct device			*dev;
	struct cg2900_chip_info		chip;
	struct cg2900_chip_callbacks	cb;
	void				*user_data;
};

/**
 * struct cg2900_id_callbacks - Chip handler identification callbacks.
 * @check_chip_support:	Called when chip is connected. If chip is supported by
 *			driver, return true and fill in @callbacks in @dev.
 *
 * Note that the callback may be NULL. It must always be NULL checked before
 * calling.
 */
struct cg2900_id_callbacks {
	bool (*check_chip_support)(struct cg2900_chip_dev *dev);
};

/**
 * struct cg2900_trans_dev - CG2900 transport info structure.
 * @dev:	Parent device from CG2900 Core.
 * @user_data:	Arbitrary data set by chip handler.
 */
struct cg2900_trans_dev {
	struct device	*dev;
	void		*user_data;
};

/**
 * struct cg2900_trans_callbacks - Callback functions registered by transport.
 * @open:		CG2900 Core needs a transport.
 * @close:		CG2900 Core does not need a transport.
 * @write:		CG2900 Core transmits to the chip.
 * @set_chip_power:	CG2900 Core enables or disables the chip.
 *
 * Note that some callbacks may be NULL. They must always be NULL checked before
 * calling.
 */
struct cg2900_trans_callbacks {
	int (*open)(struct cg2900_trans_dev *dev);
	int (*close)(struct cg2900_trans_dev *dev);
	int (*write)(struct cg2900_trans_dev *dev, struct sk_buff *skb);
	void (*set_chip_power)(bool chip_on);
};

/**
 * cg2900_register_chip_driver() - Register a chip handler.
 * @cb:	Callbacks to call when chip is connected.
 *
 * Returns:
 *   0 if there is no error.
 *   -EINVAL if NULL is supplied as @cb.
 *   -ENOMEM if allocation fails or work queue can't be created.
 */
extern int cg2900_register_chip_driver(struct cg2900_id_callbacks *cb);

/**
 * cg2900_register_trans_driver() - Register a transport driver.
 * @cb:		Callbacks to call when chip is connected.
 * @data:	Arbitrary data used by the transport driver.
 *
 * Returns:
 *   0 if there is no error.
 *   -EINVAL if NULL is supplied as @cb.
 *   -ENOMEM if allocation fails or work queue can't be created.
 */
extern int cg2900_register_trans_driver(struct cg2900_trans_callbacks *cb,
					void *data);

/**
 * cg2900_deregister_trans_driver() - Deregister a transport driver.
 *
 * Returns:
 *   0 if there is no error.
 *   -EINVAL if NULL is supplied as @cb.
 *   -ENOMEM if allocation fails or work queue can't be created.
 */
extern int cg2900_deregister_trans_driver(void);

/**
 * cg2900_chip_startup_finished() - Called from chip handler when start-up is finished.
 * @err:	Result of the start-up.
 *
 * Returns:
 *   0 if there is no error.
 */
extern int cg2900_chip_startup_finished(int err);

/**
 * cg2900_chip_shutdown_finished() - Called from chip handler when shutdown is finished.
 * @err:	Result of the shutdown.
 *
 * Returns:
 *   0 if there is no error.
 */
extern int cg2900_chip_shutdown_finished(int err);

/**
 * cg2900_send_to_chip() - Send data to chip.
 * @skb:	Packet to transmit.
 * @use_logger:	true if hci_logger should copy data content.
 *
 * Returns:
 *   0 if there is no error.
 */
extern int cg2900_send_to_chip(struct sk_buff *skb, bool use_logger);

/**
 * cg2900_get_bt_cmd_dev() - Return user of the BT command H:4 channel.
 *
 * Returns:
 *   User of the BT command H:4 channel.
 *   NULL if no user is registered.
 */
extern struct cg2900_device *cg2900_get_bt_cmd_dev(void);

/**
 * cg2900_get_fm_radio_dev() - Return user of the FM radio H:4 channel.
 *
 * Returns:
 *   User of the FM radio H:4 channel.
 *   NULL if no user is registered.
 */
extern struct cg2900_device *cg2900_get_fm_radio_dev(void);

/**
 * cg2900_get_bt_audio_dev() - Return user of the BT audio H:4 channel.
 *
 * Returns:
 *   User of the BT audio H:4 channel.
 *   NULL if no user is registered.
 */
extern struct cg2900_device *cg2900_get_bt_audio_dev(void);

/**
 * cg2900_get_fm_audio_dev() - Return user of the FM audio H:4 channel.
 *
 * Returns:
 *   User of the FM audio H:4 channel.
 *   NULL if no user is registered.
 */
extern struct cg2900_device *cg2900_get_fm_audio_dev(void);

/**
 * cg2900_get_hci_logger_config() - Return HCI Logger configuration.
 *
 * Returns:
 *   HCI logger configuration.
 *   NULL if CPD has not yet been started.
 */
extern struct cg2900_hci_logger_config *cg2900_get_hci_logger_config(void);

/**
 * cg2900_get_sleep_timeout() - Return sleep timeout in jiffies.
 *
 * Returns:
 *   Sleep timeout in jiffies. 0 means that sleep timeout shall not be used.
 */
extern unsigned long cg2900_get_sleep_timeout(void);

/**
 * cg2900_data_from_chip() - Data received from connectivity controller.
 * @skb: Data packet
 *
 * The cg2900_data_from_chip() function checks which channel
 * the data was received on and send to the right user.
 */
extern void cg2900_data_from_chip(struct sk_buff *skb);

/* module_param declared in cg2900_core.c */
extern u8 bd_address[BT_BDADDR_SIZE];
extern int default_manufacturer;
extern int default_hci_revision;
extern int default_sub_version;
extern int uart_init(void);
extern void uart_exit(void);
extern int cg2900_chip_init(void);
extern void cg2900_chip_exit(void);
extern int stlc2690_chip_init(void);
extern void stlc2690_chip_exit(void);
extern int cg2900_audio_init(void);
extern void cg2900_audio_exit(void);
#ifdef CG2900_GPS_DSM_SUPPORT
extern void enter_dsm(void);
extern void exit_dsm(void);
#endif /* CG2900_GPS_DSM_SUPPORT */

#endif /* _CG2900_CORE_H_ */
