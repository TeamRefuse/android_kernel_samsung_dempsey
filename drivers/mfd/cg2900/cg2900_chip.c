/*
 * drivers/mfd/cg2900/cg2900_chip.c
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
#include <asm/byteorder.h>
#include <linux/mfd/cg2900.h>
#include <mach/cg2900_devices.h>
#include "hci_defines.h"
#include "cg2900_chip.h"
#include "cg2900_core.h"
#include "cg2900_debug.h"

#define LINE_BUFFER_LENGTH			128
#define FILENAME_MAX				128

#define WQ_NAME					"cg2900_chip_wq"
/* ++Hemant: Update the location of firmware files */
#define PATCH_INFO_FILE				"cg2900/cg2900_patch_info.fw"
#define FACTORY_SETTINGS_INFO_FILE		"cg2900/cg2900_settings_info.fw"
/* --Hemant: Update the location of firmware files */

/* Size of file chunk ID */
#define FILE_CHUNK_ID_SIZE			1
#define FILE_CHUNK_ID_POS			4
struct hci_event_hdr {
	__u8	evt;
	__u8	plen;
} __attribute__ ((packed));

struct hci_ev_cmd_status {
	__u8     status;
	__u8     ncmd;
	__le16   opcode;
} __attribute__ ((packed));

#define CG2900_BT_OP_VS_WRITE_FILE_BLOCK		0xFC2E
#define CG2900_BT_OP_VS_BLUETOOTH_ENABLE		0xFF10

/* Times in milliseconds */
#define POWER_SW_OFF_WAIT			500
#define DELAY_BEFORE_SENDING_DISABLE_BT_IP_WAIT			500

/* State setting macros */
#define SET_BOOT_STATE(__cg2900_new_state) \
	CG2900_SET_STATE("boot_state", cg2900_info->boot_state, \
			 __cg2900_new_state)
#define SET_CLOSING_STATE(__cg2900_new_state) \
	CG2900_SET_STATE("closing_state", cg2900_info->closing_state, \
			 __cg2900_new_state)
#define SET_FILE_LOAD_STATE(__cg2900_new_state) \
	CG2900_SET_STATE("file_load_state", cg2900_info->file_load_state, \
			 __cg2900_new_state)
#define SET_DOWNLOAD_STATE(__cg2900_new_state) \
	CG2900_SET_STATE("download_state", cg2900_info->download_state, \
			 __cg2900_new_state)

/** H4_CHANNEL_BT_CMD - Bluetooth HCI H:4 channel
 * for Bluetooth commands in the ST-Ericsson connectivity controller.
 */
#define H4_CHANNEL_BT_CMD		0x01

/** H4_CHANNEL_BT_ACL - Bluetooth HCI H:4 channel
 * for Bluetooth ACL data in the ST-Ericsson connectivity controller.
 */
#define H4_CHANNEL_BT_ACL		0x02

/** H4_CHANNEL_BT_EVT - Bluetooth HCI H:4 channel
 * for Bluetooth events in the ST-Ericsson connectivity controller.
 */
#define H4_CHANNEL_BT_EVT		0x04

/** H4_CHANNEL_FM_RADIO - Bluetooth HCI H:4 channel
 * for FM radio in the ST-Ericsson connectivity controller.
 */
#define H4_CHANNEL_FM_RADIO		0x08

/** H4_CHANNEL_GNSS - Bluetooth HCI H:4 channel
 * for GNSS in the ST-Ericsson connectivity controller.
 */
#define H4_CHANNEL_GNSS			0x09

/** H4_CHANNEL_DEBUG - Bluetooth HCI H:4 channel
 * for internal debug data in the ST-Ericsson connectivity controller.
 */
#define H4_CHANNEL_DEBUG		0x0B

/** H4_CHANNEL_STE_TOOLS - Bluetooth HCI H:4 channel
 * for development tools data in the ST-Ericsson connectivity controller.
 */
#define H4_CHANNEL_STE_TOOLS		0x0D

/** H4_CHANNEL_HCI_LOGGER - Bluetooth HCI H:4 channel
 * for logging all transmitted H4 packets (on all channels).
 */
#define H4_CHANNEL_HCI_LOGGER		0xFA

/** H4_CHANNEL_US_CTRL - Bluetooth HCI H:4 channel
 * for user space control of the ST-Ericsson connectivity controller.
 */
#define H4_CHANNEL_US_CTRL		0xFC

/** H4_CHANNEL_CORE - Bluetooth HCI H:4 channel
 * for user space control of the ST-Ericsson connectivity controller.
 */
#define H4_CHANNEL_CORE			0xFD

/**
 * enum boot_state - BOOT-state for CG2900 chip driver.
 * @BOOT_NOT_STARTED:			Boot has not yet started.
 * @BOOT_SEND_BD_ADDRESS:		VS Store In FS command with BD address
 *					has been sent.
 * @BOOT_GET_FILES_TO_LOAD:		CG2900 chip driver is retrieving file to
 *					load.
 * @BOOT_DOWNLOAD_PATCH:		CG2900 chip driver is downloading
 *					patches.
 * @BOOT_ACTIVATE_PATCHES_AND_SETTINGS:	CG2900 chip driver is activating patches
 *					and settings.
 * @BOOT_DISABLING_BT_IP:	Disable the BT IP.
 * @BOOT_READY:				CG2900 chip driver boot is ready.
 * @BOOT_FAILED:			CG2900 chip driver boot failed.
 */
enum boot_state {
	BOOT_NOT_STARTED,
	BOOT_SEND_BD_ADDRESS,
	BOOT_GET_FILES_TO_LOAD,
	BOOT_DOWNLOAD_PATCH,
	BOOT_ACTIVATE_PATCHES_AND_SETTINGS,
	/* ++ Hemant: Don't disable the BT IP */
	/* BOOT_DISABLING_BT_IP, */
	/* -- Hemant: Don't disable the BT IP */
	BOOT_READY,
	BOOT_FAILED
};

/**
 * enum closing_state - CLOSING-state for CG2900 chip driver.
 * @CLOSING_RESET:		HCI RESET_CMD has been sent.
 * @CLOSING_POWER_SWITCH_OFF:	HCI VS_POWER_SWITCH_OFF command has been sent.
 * @CLOSING_SHUT_DOWN:		We have now shut down the chip.
 */
enum closing_state {
	CLOSING_RESET,
	CLOSING_POWER_SWITCH_OFF,
	CLOSING_SHUT_DOWN
};

/**
 * enum file_load_state - BOOT_FILE_LOAD-state for CG2900 chip driver.
 * @FILE_LOAD_GET_PATCH:		Loading patches.
 * @FILE_LOAD_GET_STATIC_SETTINGS:	Loading static settings.
 * @FILE_LOAD_NO_MORE_FILES:		No more files to load.
 * @FILE_LOAD_FAILED:			File loading failed.
 */
enum file_load_state {
	FILE_LOAD_GET_PATCH,
	FILE_LOAD_GET_STATIC_SETTINGS,
	FILE_LOAD_NO_MORE_FILES,
	FILE_LOAD_FAILED
};

/**
 * enum download_state - BOOT_DOWNLOAD state.
 * @DOWNLOAD_PENDING:	Download in progress.
 * @DOWNLOAD_SUCCESS:	Download successfully finished.
 * @DOWNLOAD_FAILED:	Downloading failed.
 */
enum download_state {
	DOWNLOAD_PENDING,
	DOWNLOAD_SUCCESS,
	DOWNLOAD_FAILED
};

/**
 * enum fm_radio_mode - FM Radio mode.
 * It's needed because some FM do-commands generate interrupts only when
 * the FM driver is in specific mode and we need to know if we should expect
 * the interrupt.
 * @FM_RADIO_MODE_IDLE:	Radio mode is Idle (default).
 * @FM_RADIO_MODE_FMT:	Radio mode is set to FMT (transmitter).
 * @FM_RADIO_MODE_FMR:	Radio mode is set to FMR (receiver).
 */
enum fm_radio_mode {
	FM_RADIO_MODE_IDLE = 0,
	FM_RADIO_MODE_FMT = 1,
	FM_RADIO_MODE_FMR = 2
};

/**
 * struct cg2900_device_id - Structure for connecting H4 channel to named user.
 * @name:		Name of device.
 * @h4_channel:	HCI H:4 channel used by this device.
 */
struct cg2900_device_id {
	char	*name;
	int	h4_channel;
};

/**
 * struct cg2900_skb_data - Structure for storing private data in an sk_buffer.
 * @dev:	CG2900 device for this sk_buffer.
 */
struct cg2900_skb_data {
	struct cg2900_device *dev;
};
#define cg2900_skb_data(__skb) ((struct cg2900_skb_data *)((__skb)->cb))

/**
 * struct cg2900_info - Main info structure for CG2900 chip driver.
 * @patch_file_name:		Stores patch file name.
 * @settings_file_name:		Stores settings file name.
 * @fw_file:			Stores firmware file (patch or settings).
 * @fw_file_rd_offset:		Current read offset in firmware file.
 * @chunk_id:			Stores current chunk ID of write file
 *				operations.
 * @boot_state:			Current BOOT-state of CG2900 chip driver.
 * @closing_state:		Current CLOSING-state of CG2900 chip driver.
 * @file_load_state:		Current BOOT_FILE_LOAD-state of CG2900 chip
 *				driver.
 * @download_state:		Current BOOT_DOWNLOAD-state of CG2900 chip
 *				driver.
 * @wq:				CG2900 chip driver workqueue.
 * @chip_dev:			Chip handler info.
 * @tx_bt_lock:			Spinlock used to protect some global structures
 *				related to internal BT command flow control.
 * @tx_fm_lock:			Spinlock used to protect some global structures
 *				related to internal FM command flow control.
 * @tx_fm_audio_awaiting_irpt:	Indicates if an FM interrupt event related to
 *				audio driver command is expected.
 * @fm_radio_mode:		Current FM radio mode.
 * @tx_nr_pkts_allowed_bt:	Number of packets allowed to send on BT HCI CMD
 *				H4 channel.
 * @audio_bt_cmd_op:		Stores the OpCode of the last sent audio driver
 *				HCI BT CMD.
 * @audio_fm_cmd_id:		Stores the command id of the last sent
 *				HCI FM RADIO command by the fm audio user.
 * @hci_fm_cmd_func:		Stores the command function of the last sent
 *				HCI FM RADIO command by the fm radio user.
 * @tx_queue_bt:		TX queue for HCI BT commands when nr of commands
 *				allowed is 0 (CG2900 internal flow control).
 * @tx_queue_fm:		TX queue for HCI FM commands when nr of commands
 *				allowed is 0 (CG2900 internal flow control).
 */
struct cg2900_info {
	char				*patch_file_name;
	char				*settings_file_name;
	const struct firmware		*fw_file;
	int				fw_file_rd_offset;
	u8				chunk_id;
	enum boot_state			boot_state;
	enum closing_state		closing_state;
	enum file_load_state		file_load_state;
	enum download_state		download_state;
	struct workqueue_struct		*wq;
	struct cg2900_chip_dev		chip_dev;
	spinlock_t			tx_bt_lock;
	spinlock_t			tx_fm_lock;
	bool				tx_fm_audio_awaiting_irpt;
	enum fm_radio_mode		fm_radio_mode;
	int				tx_nr_pkts_allowed_bt;
	u16				audio_bt_cmd_op;
	u16				audio_fm_cmd_id;
	u16				hci_fm_cmd_func;
	struct sk_buff_head		tx_queue_bt;
	struct sk_buff_head		tx_queue_fm;
};

static struct cg2900_info *cg2900_info;

/*
 * hci_reset_cmd_req - Hardcoded HCI Reset command.
 */
static const u8 hci_reset_cmd_req[] = {
	0x00, /* Reserved for H4 channel*/
	HCI_BT_MAKE_FIRST_BYTE_IN_CMD(HCI_BT_OCF_RESET),
	HCI_BT_MAKE_SECOND_BYTE_IN_CMD(HCI_BT_OGF_CTRL_BB, HCI_BT_OCF_RESET),
	0x00
};

/*
 * hci_vs_store_in_fs_cmd_req - Hardcoded HCI Store in FS command.
 */
static const u8 hci_vs_store_in_fs_cmd_req[] = {
	0x00, /* Reserved for H4 channel*/
	HCI_BT_MAKE_FIRST_BYTE_IN_CMD(CG2900_BT_OCF_VS_STORE_IN_FS),
	HCI_BT_MAKE_SECOND_BYTE_IN_CMD(HCI_BT_OGF_VS,
				       CG2900_BT_OCF_VS_STORE_IN_FS),
	0x00, /* 1 byte for HCI command length. */
	0x00, /* 1 byte for User_Id. */
	0x00  /* 1 byte for vs_store_in_fs_cmd_req parameter data length. */
};

/*
 * hci_vs_write_file_block_cmd_req -
 * Hardcoded HCI Write File Block vendor specific command
 */
static const u8 hci_vs_write_file_block_cmd_req[] = {
	0x00, /* Reserved for H4 channel*/
	HCI_BT_MAKE_FIRST_BYTE_IN_CMD(CG2900_BT_OCF_VS_WRITE_FILE_BLOCK),
	HCI_BT_MAKE_SECOND_BYTE_IN_CMD(HCI_BT_OGF_VS,
				       CG2900_BT_OCF_VS_WRITE_FILE_BLOCK),
	0x00
};

/*
 * hci_vs_system_reset_cmd_req -
 * Hardcoded HCI System Reset vendor specific command
 */
static const u8 hci_vs_system_reset_cmd_req[] = {
	0x00, /* Reserved for H4 channel*/
	HCI_BT_MAKE_FIRST_BYTE_IN_CMD(CG2900_BT_OCF_VS_SYSTEM_RESET),
	HCI_BT_MAKE_SECOND_BYTE_IN_CMD(HCI_BT_OGF_VS,
				       CG2900_BT_OCF_VS_SYSTEM_RESET),
	0x00
};

/*
 * hci_vs_enable_bluetooth_cmd_req -
 * Hardcoded Enbale Bluetooth vendor specific command
 */
static u8 hci_vs_enable_bluetooth_cmd_req[] = {
	0x00, /* Reserved for H4 channel*/
	HCI_BT_MAKE_FIRST_BYTE_IN_CMD(CG2900_BT_OCF_VS_BLUETOOTH_ENABLE),
	HCI_BT_MAKE_SECOND_BYTE_IN_CMD(HCI_BT_OGF_VS,
				       CG2900_BT_OCF_VS_BLUETOOTH_ENABLE),
	0x01, /* 1 byte for HCI command length. */
	0x00, /* 1 byte for Enable/disable. */
};

/*


/*
 * cg2900_channels() - Array containing available H4 channels for the CG2900
 * ST-Ericsson Connectivity controller.
 */
struct cg2900_device_id cg2900_channels[] = {
	{CG2900_BT_CMD,			H4_CHANNEL_BT_CMD},
	{CG2900_BT_ACL,			H4_CHANNEL_BT_ACL},
	{CG2900_BT_EVT,			H4_CHANNEL_BT_EVT},
	{CG2900_GNSS,			H4_CHANNEL_GNSS},
	{CG2900_FM_RADIO,		H4_CHANNEL_FM_RADIO},
	{CG2900_DEBUG,			H4_CHANNEL_DEBUG},
	{CG2900_STE_TOOLS,		H4_CHANNEL_STE_TOOLS},
	{CG2900_HCI_LOGGER,		H4_CHANNEL_HCI_LOGGER},
	{CG2900_US_CTRL,		H4_CHANNEL_US_CTRL},
	{CG2900_BT_AUDIO,		H4_CHANNEL_BT_CMD},
	{CG2900_FM_RADIO_AUDIO,		H4_CHANNEL_FM_RADIO},
	{CG2900_CORE,			H4_CHANNEL_CORE}
};

#if 0
/* Internal function declarations */

static int chip_startup(struct cg2900_chip_dev *dev);
static int chip_shutdown(struct cg2900_chip_dev *dev);
static bool data_to_chip(struct cg2900_chip_dev *dev,
			 struct cg2900_device *cpd_dev, struct sk_buff *skb);
static bool data_from_chip(struct cg2900_chip_dev *dev,
			   struct cg2900_device *cpd_dev,
			   struct sk_buff *skb);
static int get_h4_channel(char *name, int *h4_channel);
static bool is_bt_audio_user(int h4_channel, const struct sk_buff * const skb);
static bool is_fm_audio_user(int h4_channel, const struct sk_buff * const skb);
static void last_bt_user_removed(struct cg2900_chip_dev *dev);
static void last_fm_user_removed(struct cg2900_chip_dev *dev);
static bool check_chip_support(struct cg2900_chip_dev *dev);
static void update_flow_ctrl_bt(const struct sk_buff * const skb);
static void update_flow_ctrl_fm(const struct sk_buff * const skb);
static bool fm_irpt_expected(u16 cmd_id);
static bool fm_is_do_cmd_irpt(u16 irpt_val);
static bool handle_rx_data_bt_evt(struct sk_buff *skb);
static void create_work_item(work_func_t work_func);
static bool handle_reset_cmd_complete(u8 *data);
static bool handle_vs_store_in_fs_cmd_complete(u8 *data);
static bool handle_vs_write_file_block_cmd_complete(u8 *data);
static bool handle_vs_power_switch_off_cmd_complete(u8 *data);
static bool handle_vs_system_reset_cmd_complete(u8 *data);
static void work_power_off_chip(struct work_struct *work);
static void work_reset_after_error(struct work_struct *work);
static void work_load_patch_and_settings(struct work_struct *work);
static void work_cont_file_download(struct work_struct *work);
static bool get_file_to_load(const struct firmware *fw, char **file_name_p);
static char *get_text_line(char *wr_buffer, int max_nbr_of_bytes,
			   char *rd_buffer, int *bytes_copied);
static void read_and_send_file_part(void);
static void send_patch_file(void);
static void send_settings_file(void);
static void create_and_send_bt_cmd(const u8 *data, int length);
static void send_bd_address(void);
static void transmit_skb_with_flow_ctrl_bt(struct sk_buff *skb,
					   struct cg2900_device *dev);
static void fm_reset_flow_ctrl(void);
static void fm_parse_cmd(u8 *data, u8 *cmd_func, u16 *cmd_id);
static void fm_parse_event(u8 *data, u8 *event, u8 *cmd_func,
			   u16 *cmd_id, u16 *intr_val);
static void fm_update_mode(u8 *data);
static void transmit_skb_with_flow_ctrl_fm(struct sk_buff *skb,
					   struct cg2900_device *dev);
static void transmit_skb_from_tx_queue_bt(void);
static void transmit_skb_from_tx_queue_fm(void);
#endif

/*
 *	Internal function
 */

/**
 * create_and_send_bt_cmd() - Copy and send sk_buffer.
 * @data:	Data to send.
 * @length:	Length in bytes of data.
 *
 * The create_and_send_bt_cmd() function allocate sk_buffer, copy supplied data
 * to it, and send the sk_buffer to CCD. Note that the data must contain
 * the H:4 header.
 */
static void create_and_send_bt_cmd(const u8 *data, int length)
{
	struct sk_buff *skb;
	struct cg2900_hci_logger_config *logger_config;
	int err;

	skb = alloc_skb(length, GFP_ATOMIC);
	if (!skb) {
		CG2900_ERR("Couldn't alloc sk_buff with length %d", length);
		return;
	}

	memcpy(skb_put(skb, length), data, length);
	skb->data[0] = H4_CHANNEL_BT_CMD;

	logger_config = cg2900_get_hci_logger_config();
	if (logger_config)
		err = cg2900_send_to_chip(skb, logger_config->bt_cmd_enable);
	else
		err = cg2900_send_to_chip(skb, false);

	if (err) {
		CG2900_ERR("Failed to transmit to chip (%d)", err);
		kfree_skb(skb);
	}
}

/**
 * fm_irpt_expected() - check if this FM command will generate an interrupt.
 * @cmd_id:	command identifier.
 *
 * Returns:
 *   true if the command will generate an interrupt.
 *   false if it won't.
 */
static bool fm_irpt_expected(u16 cmd_id)
{
	bool retval = false;

	switch (cmd_id) {
	case CG2900_FM_DO_AIP_FADE_START:
		if (cg2900_info->fm_radio_mode == FM_RADIO_MODE_FMT)
			retval = true;
		break;

	case CG2900_FM_DO_AUP_BT_FADE_START:
	case CG2900_FM_DO_AUP_EXT_FADE_START:
	case CG2900_FM_DO_AUP_FADE_START:
		if (cg2900_info->fm_radio_mode == FM_RADIO_MODE_FMR)
			retval = true;
		break;

	case CG2900_FM_DO_FMR_SETANTENNA:
	case CG2900_FM_DO_FMR_SP_AFSWITCH_START:
	case CG2900_FM_DO_FMR_SP_AFUPDATE_START:
	case CG2900_FM_DO_FMR_SP_BLOCKSCAN_START:
	case CG2900_FM_DO_FMR_SP_PRESETPI_START:
	case CG2900_FM_DO_FMR_SP_SCAN_START:
	case CG2900_FM_DO_FMR_SP_SEARCH_START:
	case CG2900_FM_DO_FMR_SP_SEARCHPI_START:
	case CG2900_FM_DO_FMR_SP_TUNE_SETCHANNEL:
	case CG2900_FM_DO_FMR_SP_TUNE_STEPCHANNEL:
	case CG2900_FM_DO_FMT_PA_SETCTRL:
	case CG2900_FM_DO_FMT_PA_SETMODE:
	case CG2900_FM_DO_FMT_SP_TUNE_SETCHANNEL:
	case CG2900_FM_DO_GEN_ANTENNACHECK_START:
	case CG2900_FM_DO_GEN_GOTOMODE:
	case CG2900_FM_DO_GEN_POWERSUPPLY_SETMODE:
	case CG2900_FM_DO_GEN_SELECTREFERENCECLOCK:
	case CG2900_FM_DO_GEN_SETPROCESSINGCLOCK:
	case CG2900_FM_DO_GEN_SETREFERENCECLOCKPLL:
	case CG2900_FM_DO_TST_TX_RAMP_START:
		retval = true;
		break;

	default:
		break;
	}

	if (retval)
		CG2900_INFO("Following interrupt event expected for this "
			    "Cmd complete evt, cmd_id = 0x%x.", cmd_id);

	return retval;
}

/**
 * fm_is_do_cmd_irpt() - Check if irpt_val is one of the FM DO command related interrupts.
 * @irpt_val:	interrupt value.
 *
 * Returns:
 *   true if it's do-command related interrupt value.
 *   false if it's not.
 */
static bool fm_is_do_cmd_irpt(u16 irpt_val)
{
	if ((irpt_val & CG2900_FM_IRPT_OPERATION_SUCCEEDED) ||
	    (irpt_val & CG2900_FM_IRPT_OPERATION_FAILED)) {
		CG2900_INFO("Irpt evt for FM do-command found, "
			    "irpt_val = 0x%x.", irpt_val);
		return true;
	}

	return false;
}

/**
 * create_work_item() - Create work item and add it to the work queue.
 * @work_func:	Work function.
 *
 * The create_work_item() function creates work item and add it to
 * the work queue.
 */
static void create_work_item(work_func_t work_func)
{
	struct work_struct *new_work;
	int wq_err;

	new_work = kmalloc(sizeof(*new_work), GFP_ATOMIC);
	if (!new_work) {
		CG2900_ERR("Failed to alloc memory for work_struct!");
		return;
	}

	INIT_WORK(new_work, work_func);

	wq_err = queue_work(cg2900_info->wq, new_work);
	if (!wq_err) {
		CG2900_ERR("Failed to queue work_struct because it's already "
			   "in the queue!");
		kfree(new_work);
	}
}

/**
 * fm_reset_flow_ctrl - Clears up internal FM flow control.
 *
 * Resets outstanding commands and clear FM TX list and set CG2900 FM mode to
 * idle.
 */
static void fm_reset_flow_ctrl(void)
{
	CG2900_INFO("fm_reset_flow_ctrl");

	skb_queue_purge(&cg2900_info->tx_queue_fm);

	/* Reset the fm_cmd_id. */
	cg2900_info->audio_fm_cmd_id = CG2900_FM_CMD_NONE;
	cg2900_info->hci_fm_cmd_func = CG2900_FM_CMD_PARAM_NONE;

	cg2900_info->fm_radio_mode = FM_RADIO_MODE_IDLE;
}


/**
 * fm_parse_cmd - Parses a FM command packet.
 * @data:	FM command packet.
 * @cmd_func:	Out: FM legacy command function.
 * @cmd_id:	Out: FM legacy command ID.
 */
static void fm_parse_cmd(u8 *data, u8 *cmd_func, u16 *cmd_id)
{
	*cmd_func = CG2900_FM_CMD_PARAM_NONE;
	*cmd_id   = CG2900_FM_CMD_NONE;

	if (data[CG2900_FM_GEN_ID_POS_CMD] != CG2900_FM_GEN_ID_LEGACY) {
		CG2900_ERR("Not an FM legacy command 0x%X",
			   data[CG2900_FM_GEN_ID_POS_CMD]);
		return;
	}

	*cmd_func = data[4];
	CG2900_DBG("cmd_func 0x%X", *cmd_func);
	if (*cmd_func == CG2900_FM_CMD_PARAM_WRITECOMMAND) {
		*cmd_id = CG2900_GET_FM_CMD_ID(data[5], data[6]);
		CG2900_DBG("cmd_id 0x%X", *cmd_id);
	}
}


/**
 * fm_parse_event - Parses a FM event packet
 * @data:	FM event packet.
 * @event	Out: FM event.
 * @cmd_func:	Out: FM legacy command function.
 * @cmd_id:	Out: FM legacy command ID.
 * @intr_val:	Out: FM interrupt value.
 */
static void fm_parse_event(u8 *data, u8 *event, u8 *cmd_func, u16 *cmd_id,
			   u16 *intr_val)
{
	*cmd_func = CG2900_FM_CMD_PARAM_NONE;
	*cmd_id = CG2900_FM_CMD_NONE;
	*intr_val = 0;
	*event = CG2900_FM_EVENT_UNKNOWN;

	if (data[CG2900_FM_GEN_ID_POS_CMD_CMPL] == CG2900_FM_GEN_ID_LEGACY) {
		/* Command complete */
		*event = CG2900_FM_EVENT_CMD_COMPLETE;
		*cmd_func = data[6];
		CG2900_DBG("cmd_func 0x%X", *cmd_func);
		if (*cmd_func == CG2900_FM_CMD_PARAM_WRITECOMMAND) {
			*cmd_id = CG2900_GET_FM_CMD_ID(data[7], data[8]);
			CG2900_DBG("cmd_id 0x%X", *cmd_id);
		}
	} else if (data[CG2900_FM_GEN_ID_POS_INTERRUPT] ==
			CG2900_FM_GEN_ID_LEGACY) {
		/* Interrupt */
		*event = CG2900_FM_EVENT_INTERRUPT;
		*intr_val = GET_U16_FROM_L_ENDIAN(data[4], data[5]);
		CG2900_DBG("intr_val 0x%X", *intr_val);
	} else {
		CG2900_ERR("Not an FM legacy command 0x%X",
			   data[CG2900_FM_GEN_ID_POS_CMD]);
	}
}

/**
 * fm_update_mode - Updates the FM mode state machine.
 * @data:	FM command packet.
 *
 * Parses a FM command packet and updates the FM mode state machine.
 */
static void fm_update_mode(u8 *data)
{
	u8 cmd_func;
	u16 cmd_id;

	fm_parse_cmd(data, &cmd_func, &cmd_id);

	if (cmd_func == CG2900_FM_CMD_PARAM_WRITECOMMAND &&
	    cmd_id == CG2900_FM_DO_GEN_GOTOMODE) {
		cg2900_info->fm_radio_mode = data[6];
		CG2900_INFO("FM Radio mode changed to 0x%x",
			    cg2900_info->fm_radio_mode);
	}
}


/**
 * transmit_skb_from_tx_queue_bt() - Check flow control info and transmit skb.
 *
 * The transmit_skb_from_tx_queue_bt() function checks if there are tickets
 * available and commands waiting in the TX queue and if so transmits them
 * to the controller.
 * It shall always be called within spinlock_bh.
 */
static void transmit_skb_from_tx_queue_bt(void)
{
	struct cg2900_device *dev;
	struct sk_buff *skb;

	CG2900_INFO("transmit_skb_from_tx_queue_bt");

	/* Dequeue an skb from the head of the list */
	skb = skb_dequeue(&cg2900_info->tx_queue_bt);
	while (skb) {
		if ((cg2900_info->tx_nr_pkts_allowed_bt) <= 0) {
			/*
			 * If no more packets allowed just return, we'll get
			 * back here after next Command Complete/Status event.
			 * Put skb back at head of queue.
			 */
			skb_queue_head(&cg2900_info->tx_queue_bt, skb);
			return;
		}

		(cg2900_info->tx_nr_pkts_allowed_bt)--;
		CG2900_DBG("tx_nr_pkts_allowed_bt = %d",
			   cg2900_info->tx_nr_pkts_allowed_bt);

		dev = cg2900_skb_data(skb)->dev; /* dev is never NULL */

		/*
		 * If it's a command from audio application, store the OpCode,
		 * it'll be used later to decide where to dispatch
		 * the Command Complete event.
		 */
		if (cg2900_get_bt_audio_dev() == dev) {
			cg2900_info->audio_bt_cmd_op =
					HCI_BT_GET_ID(skb->data[1],
							  skb->data[2]);
			CG2900_DBG("Sending cmd from audio driver, saving "
				   "OpCode = 0x%X",
				   cg2900_info->audio_bt_cmd_op);
		}

		cg2900_send_to_chip(skb, dev->logger_enabled);

		/* Dequeue an skb from the head of the list */
		skb = skb_dequeue(&cg2900_info->tx_queue_bt);
	}
}

/**
 * transmit_skb_from_tx_queue_fm() - Check flow control info and transmit skb.
 *
 * The transmit_skb_from_tx_queue_fm() function checks if it possible to
 * transmit and commands waiting in the TX queue and if so transmits them
 * to the controller.
 * It shall always be called within spinlock_bh.
 */
static void transmit_skb_from_tx_queue_fm(void)
{
	struct cg2900_device *dev;
	struct sk_buff *skb;

	CG2900_INFO("transmit_skb_from_tx_queue_fm");

	/* Dequeue an skb from the head of the list */
	skb = skb_dequeue(&cg2900_info->tx_queue_fm);
	while (skb) {
		u16 cmd_id;
		u8 cmd_func;
		bool do_transmit = false;

		if (cg2900_info->audio_fm_cmd_id != CG2900_FM_CMD_NONE ||
		    cg2900_info->hci_fm_cmd_func != CG2900_FM_CMD_PARAM_NONE) {
			/*
			 * There are currently outstanding FM commands.
			 * Wait for them to finish. We will get back here later.
			 * Queue back the skb at head of list.
			 */
			skb_queue_head(&cg2900_info->tx_queue_bt, skb);
			return;
		}

		dev = cg2900_skb_data(skb)->dev; /* dev is never NULL */

		fm_parse_cmd(&(skb->data[0]), &cmd_func, &cmd_id);

		/*
		 * Store the FM command function , it'll be used later to decide
		 * where to dispatch the Command Complete event.
		 */
		if (cg2900_get_fm_audio_dev() == dev) {
			cg2900_info->audio_fm_cmd_id = cmd_id;
			CG2900_DBG("audio_fm_cmd_id 0x%X",
				   cg2900_info->audio_fm_cmd_id);
			do_transmit = true;
		}
		if (cg2900_get_fm_radio_dev() == dev) {
			cg2900_info->hci_fm_cmd_func = cmd_func;
			fm_update_mode(&(skb->data[0]));
			CG2900_DBG("hci_fm_cmd_func 0x%X",
				   cg2900_info->hci_fm_cmd_func);
			do_transmit = true;
		}

		if (do_transmit) {
			/*
			 * We have only one ticket on FM. Just return after
			 * sending the skb.
			 */
			cg2900_send_to_chip(skb, dev->logger_enabled);
			return;
		}

		/*
		 * This packet was neither FM or FM audio. That means that
		 * the user that originally sent it has deregistered.
		 * Just throw it away and check the next skb in the queue.
		 */
		kfree_skb(skb);
		/* Dequeue an skb from the head of the list */
		skb = skb_dequeue(&cg2900_info->tx_queue_fm);
	}
}

/**
 * update_flow_ctrl_bt() - Update number of outstanding commands for BT CMD.
 * @skb:	skb with received packet.
 *
 * The update_flow_ctrl_bt() checks if incoming data packet is
 * BT Command Complete/Command Status Event and if so updates number of tickets
 * and number of outstanding commands. It also calls function to send queued
 * commands (if the list of queued commands is not empty).
 */
static void update_flow_ctrl_bt(const struct sk_buff * const skb)
{
	u8 *data = &(skb->data[CG2900_SKB_RESERVE]);
	u8 event_code = data[0];

	if (HCI_BT_EVT_CMD_COMPLETE == event_code) {
		/*
		 * If it's HCI Command Complete Event then we might get some
		 * HCI tickets back. Also we can decrease the number outstanding
		 * HCI commands (if it's not NOP command or one of the commands
		 * that generate both Command Status Event and Command Complete
		 * Event).
		 * Check if we have any HCI commands waiting in the TX list and
		 * send them if there are tickets available.
		 */
		spin_lock_bh(&(cg2900_info->tx_bt_lock));
		cg2900_info->tx_nr_pkts_allowed_bt =
				data[HCI_BT_EVT_CMD_COMPL_NR_OF_PKTS_POS];
		CG2900_DBG("New tx_nr_pkts_allowed_bt = %d",
			   cg2900_info->tx_nr_pkts_allowed_bt);

		if (!skb_queue_empty(&cg2900_info->tx_queue_bt))
			transmit_skb_from_tx_queue_bt();
		spin_unlock_bh(&(cg2900_info->tx_bt_lock));
	} else if (HCI_BT_EVT_CMD_STATUS == event_code) {
		/*
		 * If it's HCI Command Status Event then we might get some
		 * HCI tickets back. Also we can decrease the number outstanding
		 * HCI commands (if it's not NOP command).
		 * Check if we have any HCI commands waiting in the TX queue and
		 * send them if there are tickets available.
		 */
		spin_lock_bh(&(cg2900_info->tx_bt_lock));
		cg2900_info->tx_nr_pkts_allowed_bt =
				data[HCI_BT_EVT_CMD_STATUS_NR_OF_PKTS_POS];
		CG2900_DBG("New tx_nr_pkts_allowed_bt = %d",
			   cg2900_info->tx_nr_pkts_allowed_bt);

		if (!skb_queue_empty(&cg2900_info->tx_queue_bt))
			transmit_skb_from_tx_queue_bt();
		spin_unlock_bh(&(cg2900_info->tx_bt_lock));
	}
}

/**
 * update_flow_ctrl_fm() - Update packets allowed for FM channel.
 * @skb:	skb with received packet.
 *
 * The update_flow_ctrl_fm() checks if incoming data packet is FM packet
 * indicating that the previous command has been handled and if so update
 * packets. It also calls function to send queued commands (if the list of
 * queued commands is not empty).
 */
static void update_flow_ctrl_fm(const struct sk_buff * const skb)
{
	u8 cmd_func = CG2900_FM_CMD_PARAM_NONE;
	u16 cmd_id = CG2900_FM_CMD_NONE;
	u16 irpt_val = 0;
	u8 event = CG2900_FM_EVENT_UNKNOWN;

	fm_parse_event(&(skb->data[0]), &event, &cmd_func, &cmd_id, &irpt_val);

	if (event == CG2900_FM_EVENT_CMD_COMPLETE) {
		/* FM legacy command complete event */
		spin_lock_bh(&(cg2900_info->tx_fm_lock));
		/*
		 * Check if it's not an write command complete event, because
		 * then it cannot be a DO command.
		 * If it's a write command complete event check that is not a
		 * DO command complete event before setting the outstanding
		 * FM packets to none.
		 */
		if (cmd_func != CG2900_FM_CMD_PARAM_WRITECOMMAND ||
		    !fm_irpt_expected(cmd_id)) {
			cg2900_info->hci_fm_cmd_func = CG2900_FM_CMD_PARAM_NONE;
			cg2900_info->audio_fm_cmd_id = CG2900_FM_CMD_NONE;
			CG2900_DBG("FM cmd outstanding cmd func 0x%x",
				   cg2900_info->hci_fm_cmd_func);
			CG2900_DBG("FM cmd Audio outstanding cmd id 0x%x",
				   cg2900_info->audio_fm_cmd_id);
			transmit_skb_from_tx_queue_fm();

		/*
		 * If there was a write do command complete event check if it is
		 * DO command previously sent by the FM audio user. If that's
		 * the case we need remember that in order to be able to
		 * dispatch the interrupt to the correct user.
		 */
		} else if (cmd_id == cg2900_info->audio_fm_cmd_id) {
			cg2900_info->tx_fm_audio_awaiting_irpt = true;
			CG2900_DBG("FM Audio waiting for interrupt = true.");
		}
		spin_unlock_bh(&(cg2900_info->tx_fm_lock));
	} else if (event == CG2900_FM_EVENT_INTERRUPT) {
		/* FM legacy interrupt */
		if (fm_is_do_cmd_irpt(irpt_val)) {
			/*
			 * If it is an interrupt related to a DO command update
			 * the outstanding flow control and transmit blocked
			 * FM commands.
			 */
			spin_lock_bh(&(cg2900_info->tx_fm_lock));
			cg2900_info->hci_fm_cmd_func = CG2900_FM_CMD_PARAM_NONE;
			cg2900_info->audio_fm_cmd_id = CG2900_FM_CMD_NONE;
			CG2900_DBG("FM cmd outstanding cmd func 0x%x",
				   cg2900_info->hci_fm_cmd_func);
			CG2900_DBG("FM cmd Audio outstanding cmd id 0x%x",
				   cg2900_info->audio_fm_cmd_id);
			cg2900_info->tx_fm_audio_awaiting_irpt = false;
			CG2900_DBG("FM Audio waiting for interrupt = false.");
			transmit_skb_from_tx_queue_fm();
			spin_unlock_bh(&(cg2900_info->tx_fm_lock));
		}
	}
}

/**
 * send_bd_address() - Send HCI VS command with BD address to the chip.
 */
static void send_bd_address(void)
{
	u8 tmp[sizeof(hci_vs_store_in_fs_cmd_req) + BT_BDADDR_SIZE];

	/* Send vs_store_in_fs_cmd with BD address. */
	memcpy(tmp, hci_vs_store_in_fs_cmd_req,
	       sizeof(hci_vs_store_in_fs_cmd_req));

	tmp[HCI_BT_CMD_PARAM_LEN_POS] = CG2900_VS_STORE_IN_FS_USR_ID_SIZE +
					CG2900_VS_STORE_IN_FS_PARAM_LEN_SIZE +
					BT_BDADDR_SIZE;
	tmp[CG2900_VS_STORE_IN_FS_USR_ID_POS] =
			CG2900_VS_STORE_IN_FS_USR_ID_BD_ADDR;
	tmp[CG2900_VS_STORE_IN_FS_PARAM_LEN_POS] = BT_BDADDR_SIZE;

	/* Now copy the BD address received from user space control app. */
	memcpy(&(tmp[CG2900_VS_STORE_IN_FS_PARAM_POS]), bd_address,
	       sizeof(bd_address));

	SET_BOOT_STATE(BOOT_SEND_BD_ADDRESS);

	create_and_send_bt_cmd(tmp, sizeof(tmp));
}

/**
 * get_text_line()- Replacement function for stdio function fgets.
 * @wr_buffer:		Buffer to copy text to.
 * @max_nbr_of_bytes:	Max number of bytes to read, i.e. size of rd_buffer.
 * @rd_buffer:		Data to parse.
 * @bytes_copied:	Number of bytes copied to wr_buffer.
 *
 * The get_text_line() function extracts one line of text from input file.
 *
 * Returns:
 *   Pointer to next data to read.
 */
static char *get_text_line(char *wr_buffer, int max_nbr_of_bytes,
			   char *rd_buffer, int *bytes_copied)
{
	char *curr_wr = wr_buffer;
	char *curr_rd = rd_buffer;
	char in_byte;

	*bytes_copied = 0;

	do {
		*curr_wr = *curr_rd;
		in_byte = *curr_wr;
		curr_wr++;
		curr_rd++;
		(*bytes_copied)++;
	} while ((*bytes_copied <= max_nbr_of_bytes) && (in_byte != '\0') &&
		 (in_byte != '\n'));
	*curr_wr = '\0';
	return curr_rd;
}

/**
 * get_file_to_load() - Parse info file and find correct target file.
 * @fw:		Firmware structure containing file data.
 * @file_name:	(out) Pointer to name of requested file.
 *
 * Returns:
 *   true,  if target file was found,
 *   false, otherwise.
 */
static bool get_file_to_load(const struct firmware *fw, char **file_name)
{
	char *line_buffer;
	char *curr_file_buffer;
	int bytes_left_to_parse = fw->size;
	int bytes_read = 0;
	bool file_found = false;
	u32 hci_rev;
	u32 lmp_sub;

	curr_file_buffer = (char *)&(fw->data[0]);

	line_buffer = kzalloc(LINE_BUFFER_LENGTH, GFP_ATOMIC);
	if (!line_buffer) {
		CG2900_ERR("Failed to allocate line_buffer");
		return false;
	}

	while (!file_found) {
		/* Get one line of text from the file to parse */
		curr_file_buffer = get_text_line(line_buffer,
					 min(LINE_BUFFER_LENGTH,
					     (int)(fw->size - bytes_read)),
					 curr_file_buffer,
					 &bytes_read);

		bytes_left_to_parse -= bytes_read;
		if (bytes_left_to_parse <= 0) {
			/* End of file => Leave while loop */
			CG2900_ERR("Reached end of file. No file found!");
			break;
		}

		/*
		 * Check if the line of text is a comment or not, comments begin
		 * with '#'
		 */
		if (*line_buffer == '#')
			continue;

		hci_rev = 0;
		lmp_sub = 0;

		CG2900_DBG("Found a valid line <%s>", line_buffer);

		/*
		 * Check if we can find the correct HCI revision and
		 * LMP subversion as well as a file name in
		 * the text line.
		 */
		if (sscanf(line_buffer, "%x%x%s", &hci_rev, &lmp_sub,
			   *file_name) == 3
		    && hci_rev == cg2900_info->chip_dev.chip.hci_revision
		    && lmp_sub == cg2900_info->chip_dev.chip.hci_sub_version) {
			CG2900_DBG("File found for chip\n"
				   "\tFile name = %s\n"
				   "\tHCI Revision = 0x%X\n"
				   "\tLMP PAL Subversion = 0x%X",
				   *file_name, hci_rev, lmp_sub);

			/*
			 * Name has already been stored above. Nothing more to
			 * do.
			 */
			file_found = true;
		} else
			/* Zero the name buffer so it is clear to next read */
			memset(*file_name, 0x00, FILENAME_MAX + 1);
	}
	kfree(line_buffer);

	return file_found;
}

/**
 * read_and_send_file_part() - Transmit a part of the supplied file.
 *
 * The read_and_send_file_part() function transmit a part of the supplied file
 * to the controller.
 * If nothing more to read, set the correct states.
 */
static void read_and_send_file_part(void)
{
	int bytes_to_copy;
	struct sk_buff *skb;
	struct cg2900_hci_logger_config *logger_config;

	/* Calculate number of bytes to copy;
	 * either max bytes for HCI packet or number of bytes left in file
	 */
	bytes_to_copy = min((int)HCI_BT_SEND_FILE_MAX_CHUNK_SIZE,
			    (int)(cg2900_info->fw_file->size -
				  cg2900_info->fw_file_rd_offset));

	if (bytes_to_copy <= 0) {
		/* Nothing more to read in file. */
		SET_DOWNLOAD_STATE(DOWNLOAD_SUCCESS);
		cg2900_info->chunk_id = 0;
		cg2900_info->fw_file_rd_offset = 0;
		return;
	}

	/* There is more data to send */
	logger_config = cg2900_get_hci_logger_config();

	/* There are bytes to transmit. Allocate a sk_buffer. */
	skb = alloc_skb(sizeof(hci_vs_write_file_block_cmd_req) +
			HCI_BT_SEND_FILE_MAX_CHUNK_SIZE +
			FILE_CHUNK_ID_SIZE, GFP_ATOMIC);
	if (!skb) {
		CG2900_ERR("Couldn't allocate sk_buffer");
		SET_BOOT_STATE(BOOT_FAILED);
		cg2900_chip_startup_finished(-EIO);
		return;
	}

	/* Copy the data from the HCI template */
	memcpy(skb_put(skb, sizeof(hci_vs_write_file_block_cmd_req)),
	       hci_vs_write_file_block_cmd_req,
	       sizeof(hci_vs_write_file_block_cmd_req));
	skb->data[0] = H4_CHANNEL_BT_CMD;

	/* Store the chunk ID */
	skb->data[FILE_CHUNK_ID_POS] = cg2900_info->chunk_id;
	(void)skb_put(skb, 1);
	cg2900_info->chunk_id++;

	/* Copy the data from offset position and store the length */
	memcpy(skb_put(skb, bytes_to_copy),
	       &(cg2900_info->fw_file->data[cg2900_info->fw_file_rd_offset]),
	       bytes_to_copy);
	skb->data[HCI_BT_CMD_PARAM_LEN_POS] = bytes_to_copy +
			FILE_CHUNK_ID_SIZE;

	/* Increase offset with number of bytes copied */
	cg2900_info->fw_file_rd_offset += bytes_to_copy;

	if (logger_config)
		cg2900_send_to_chip(skb, logger_config->bt_cmd_enable);
	else
		cg2900_send_to_chip(skb, false);
}

/**
 * send_settings_file() - Transmit settings file.
 *
 * The send_settings_file() function transmit settings file.
 * The file is read in parts to fit in HCI packets. When finished,
 * close the settings file and send HCI reset to activate settings and patches.
 */
static void send_settings_file(void)
{
	/* Transmit a file part */
	read_and_send_file_part();

	if (cg2900_info->download_state != DOWNLOAD_SUCCESS)
		return;

	/* Settings file finished. Release used resources */
	CG2900_DBG("Settings file finished, release used resources");
	if (cg2900_info->fw_file) {
		release_firmware(cg2900_info->fw_file);
		cg2900_info->fw_file = NULL;
	}

	SET_FILE_LOAD_STATE(FILE_LOAD_NO_MORE_FILES);

#if 0
	/* Create and send HCI VS Store In FS command with bd address. */
	send_bd_address();
#else
	/* Send HCI SystemReset command to activate patches */
	SET_BOOT_STATE(BOOT_ACTIVATE_PATCHES_AND_SETTINGS);
	create_and_send_bt_cmd(hci_vs_system_reset_cmd_req,
			       sizeof(hci_vs_system_reset_cmd_req));

#endif
}

/**
 * send_patch_file - Transmit patch file.
 *
 * The send_patch_file() function transmit patch file.
 * The file is read in parts to fit in HCI packets. When the complete file is
 * transmitted, the file is closed.
 * When finished, continue with settings file.
 */
static void send_patch_file(void)
{
	int err;

	/*
	 * Transmit a part of the supplied file to the controller.
	 * When nothing more to read, continue to close the patch file.
	 */
	read_and_send_file_part();

	if (cg2900_info->download_state != DOWNLOAD_SUCCESS)
		return;

	/* Patch file finished. Release used resources */
	CG2900_DBG("Patch file finished, release used resources");
	if (cg2900_info->fw_file) {
		release_firmware(cg2900_info->fw_file);
		cg2900_info->fw_file = NULL;
	}
	/* Retrieve the settings file */
	err = request_firmware(&(cg2900_info->fw_file),
			       cg2900_info->settings_file_name,
			       cg2900_info->chip_dev.dev);
	if (err < 0) {
		CG2900_ERR("Couldn't get settings file (%d)", err);
		goto error_handling;
	}
	/* Now send the settings file */
	SET_FILE_LOAD_STATE(FILE_LOAD_GET_STATIC_SETTINGS);
	SET_DOWNLOAD_STATE(DOWNLOAD_PENDING);
	send_settings_file();
	return;

error_handling:
	SET_BOOT_STATE(BOOT_FAILED);
	cg2900_chip_startup_finished(err);
}

/**
 * work_power_off_chip() - Work item to power off the chip.
 * @work:	Reference to work data.
 *
 * The work_power_off_chip() function handles transmission of the HCI command
 * vs_power_switch_off, closes the CCD, and then powers off the chip.
 */
static void work_power_off_chip(struct work_struct *work)
{
	struct sk_buff *skb;
	u8 *h4_header;
	struct cg2900_hci_logger_config *logger_config;

	if (!work) {
		CG2900_ERR("work == NULL");
		return;
	}

	/*
	 * Get the VS Power Switch Off command to use based on connected
	 * connectivity controller
	 */
	skb = cg2900_devices_get_power_switch_off_cmd(NULL, NULL);

	/*
	 * Transmit the received command.
	 * If no command found for the device, just continue
	 */
	if (!skb) {
		CG2900_ERR("Could not retrieve PowerSwitchOff command");
		goto shut_down_chip;
	}

	logger_config = cg2900_get_hci_logger_config();

	CG2900_DBG("Got power_switch_off command. Add H4 header and transmit");

	/*
	 * Move the data pointer to the H:4 header position and store
	 * the H4 header
	 */
	h4_header = skb_push(skb, CG2900_SKB_RESERVE);
	*h4_header = H4_CHANNEL_BT_CMD;

	SET_CLOSING_STATE(CLOSING_POWER_SWITCH_OFF);

	if (logger_config)
		cg2900_send_to_chip(skb, logger_config->bt_cmd_enable);
	else
		cg2900_send_to_chip(skb, false);

	/*
	 * Mandatory to wait 500ms after the power_switch_off command has been
	 * transmitted, in order to make sure that the controller is ready.
	 */
	schedule_timeout_interruptible(msecs_to_jiffies(POWER_SW_OFF_WAIT));

shut_down_chip:
	SET_CLOSING_STATE(CLOSING_SHUT_DOWN);

	(void)cg2900_chip_shutdown_finished(0);

	kfree(work);
}

/**
 * work_reset_after_error() - Handle reset.
 * @work:	Reference to work data.
 *
 * Handle a reset after received Command Complete event.
 */
static void work_reset_after_error(struct work_struct *work)
{
	if (!work) {
		CG2900_ERR("work == NULL");
		return;
	}

	cg2900_chip_startup_finished(-EIO);

	kfree(work);
}

/**
 * work_load_patch_and_settings() - Start loading patches and settings.
 * @work:	Reference to work data.
 */
static void work_load_patch_and_settings(struct work_struct *work)
{
	int err = 0;
	bool file_found;
	const struct firmware *patch_info;
	const struct firmware *settings_info;

	if (!work) {
		CG2900_ERR("work == NULL");
		return;
	}

	/* Check that we are in the right state */
	if (cg2900_info->boot_state != BOOT_GET_FILES_TO_LOAD)
		goto finished;

	/* Open patch info file. */
	err = request_firmware(&patch_info, PATCH_INFO_FILE,
			       cg2900_info->chip_dev.dev);
	if (err) {
		CG2900_ERR("Couldn't get patch info file (%d)", err);
		goto error_handling;
	}

	/*
	 * Now we have the patch info file.
	 * See if we can find the right patch file as well
	 */
	file_found = get_file_to_load(patch_info,
				      &(cg2900_info->patch_file_name));

	/* Now we are finished with the patch info file */
	release_firmware(patch_info);

	if (!file_found) {
		CG2900_ERR("Couldn't find patch file! Major error!");
		goto error_handling;
	}

	/* Open settings info file. */
	err = request_firmware(&settings_info,
			       FACTORY_SETTINGS_INFO_FILE,
			       cg2900_info->chip_dev.dev);
	if (err) {
		CG2900_ERR("Couldn't get settings info file (%d)", err);
		goto error_handling;
	}

	/*
	 * Now we have the settings info file.
	 * See if we can find the right settings file as well.
	 */
	file_found = get_file_to_load(settings_info,
				      &(cg2900_info->settings_file_name));

	/* Now we are finished with the patch info file */
	release_firmware(settings_info);

	if (!file_found) {
		CG2900_ERR("Couldn't find settings file! Major error!");
		goto error_handling;
	}

	/* We now all info needed */
	SET_BOOT_STATE(BOOT_DOWNLOAD_PATCH);
	SET_DOWNLOAD_STATE(DOWNLOAD_PENDING);
	SET_FILE_LOAD_STATE(FILE_LOAD_GET_PATCH);
	cg2900_info->chunk_id = 0;
	cg2900_info->fw_file_rd_offset = 0;
	cg2900_info->fw_file = NULL;

	/* OK. Now it is time to download the patches */
	err = request_firmware(&(cg2900_info->fw_file),
			       cg2900_info->patch_file_name,
			       cg2900_info->chip_dev.dev);
	if (err < 0) {
		CG2900_ERR("Couldn't get patch file (%d)", err);
		goto error_handling;
	}
	send_patch_file();

	goto finished;

error_handling:
	SET_BOOT_STATE(BOOT_FAILED);
	cg2900_chip_startup_finished(-EIO);
finished:
	kfree(work);
}

/**
 * work_cont_file_download() - A file block has been written.
 * @work:	Reference to work data.
 *
 * Handle a received HCI VS Write File Block Complete event.
 * Normally this means continue to send files to the controller.
 */
static void work_cont_file_download(struct work_struct *work)
{
	if (!work) {
		CG2900_ERR("work == NULL");
		return;
	}

	/* Continue to send patches or settings to the controller */
	if (cg2900_info->file_load_state == FILE_LOAD_GET_PATCH)
		send_patch_file();
	else if (cg2900_info->file_load_state == FILE_LOAD_GET_STATIC_SETTINGS)
		send_settings_file();
	else
		CG2900_INFO("No more files to load");

	kfree(work);
}

/**
 * handle_reset_cmd_complete() - Handles HCI Reset Command Complete event.
 * @data:	Pointer to received HCI data packet.
 *
 * Returns:
 *   true,  if packet was handled internally,
 *   false, otherwise.
 */
static bool handle_reset_cmd_complete(u8 *data)
{
	u8 status = data[0];

	CG2900_INFO("Received Reset complete event with status 0x%X", status);

	if (CLOSING_RESET != cg2900_info->closing_state)
		return false;

	if (HCI_BT_ERROR_NO_ERROR != status) {
		/*
		 * Continue in case of error, the chip is going to be shut down
		 * anyway.
		 */
		CG2900_ERR("Command complete for HciReset received with "
			   "error 0x%X !", status);
	}

	create_work_item(work_power_off_chip);

	return true;
}


/**
 * handle_vs_store_in_fs_cmd_complete() - Handles HCI VS StoreInFS Command Complete event.
 * @data:	Pointer to received HCI data packet.
 *
 * Returns:
 *   true,  if packet was handled internally,
 *   false, otherwise.
 */
static bool handle_vs_store_in_fs_cmd_complete(u8 *data)
{
	u8 status = data[0];

	CG2900_INFO("Received Store_in_FS complete event with status 0x%X",
		    status);

	if (cg2900_info->boot_state != BOOT_SEND_BD_ADDRESS)
		return false;

	if (HCI_BT_ERROR_NO_ERROR == status) {
		/* Send HCI SystemReset command to activate patches */
		SET_BOOT_STATE(BOOT_ACTIVATE_PATCHES_AND_SETTINGS);
		create_and_send_bt_cmd(hci_vs_system_reset_cmd_req,
				       sizeof(hci_vs_system_reset_cmd_req));
	} else {
		CG2900_ERR("Command complete for StoreInFS received with error "
			   "0x%X", status);
		SET_BOOT_STATE(BOOT_FAILED);
		create_work_item(work_reset_after_error);
	}

	return true;
}

/**
 * handle_vs_write_file_block_cmd_complete() - Handles HCI VS WriteFileBlock Command Complete event.
 * @data:	Pointer to received HCI data packet.
 *
 * Returns:
 *   true,  if packet was handled internally,
 *   false, otherwise.
 */
static bool handle_vs_write_file_block_cmd_complete(u8 *data)
{
	u8 status = data[0];

	if ((cg2900_info->boot_state != BOOT_DOWNLOAD_PATCH) ||
	    (cg2900_info->download_state != DOWNLOAD_PENDING))
		return false;

	if (HCI_BT_ERROR_NO_ERROR == status) {
		CG2900_DBG_DATA("Command complete for %d ID", data[1]);
		create_work_item(work_cont_file_download);
	} else {
		CG2900_ERR("Command complete for WriteFileBlock received with"
			   " error 0x%X", status);
		SET_DOWNLOAD_STATE(DOWNLOAD_FAILED);
		SET_BOOT_STATE(BOOT_FAILED);
		if (cg2900_info->fw_file) {
			release_firmware(cg2900_info->fw_file);
			cg2900_info->fw_file = NULL;
		}
		create_work_item(work_reset_after_error);
	}

	return true;
}

/**
 * handle_vs_write_file_block_cmd_status() - Handles HCI VS WriteFileBlock Command Status event.
 * @status:	Returned status of WriteFileBlock command.
 *
 * Returns:
 *   true,  if packet was handled internally,
 *   false, otherwise.
 */
static bool handle_vs_write_file_block_cmd_status(u8 status)
{
	if ((cg2900_info->boot_state != BOOT_DOWNLOAD_PATCH) ||
	    (cg2900_info->download_state != DOWNLOAD_PENDING))
		return false;

	/*
	 * Only do something if there is an error. Otherwise we will wait for
	 * CmdComplete.
	 */
	if (HCI_BT_ERROR_NO_ERROR != status) {
		CG2900_ERR("Command status for WriteFileBlock received with"
			   " error 0x%X", status);
		SET_DOWNLOAD_STATE(DOWNLOAD_FAILED);
		SET_BOOT_STATE(BOOT_FAILED);
		if (cg2900_info->fw_file) {
			release_firmware(cg2900_info->fw_file);
			cg2900_info->fw_file = NULL;
		}
		create_work_item(work_reset_after_error);
	}

	return true;
}

/**
 * handle_vs_power_switch_off_cmd_complete() - Handles HCI VS PowerSwitchOff Command Complete event.
 * @data:	Pointer to received HCI data packet.
 *
 * Returns:
 *   true,  if packet was handled internally,
 *   false, otherwise.
 */
static bool handle_vs_power_switch_off_cmd_complete(u8 *data)
{
	u8 status = data[0];

	if (CLOSING_POWER_SWITCH_OFF != cg2900_info->closing_state)
		return false;

	/*
	 * We were waiting for this but we don't need to do anything upon
	 * reception except warn for error status
	 */
	if (HCI_BT_ERROR_NO_ERROR != status)
		CG2900_ERR("Command Complete for PowerSwitchOff received with "
			   "error 0x%X", status);

	return true;
}

/**
 * handle_vs_system_reset_cmd_complete() - Handle HCI VS SystemReset Command Complete event.
 * @data:	Pointer to received HCI data packet.
 *
 * Returns:
 *   true,  if packet was handled internally,
 *   false, otherwise.
 */
static bool handle_vs_system_reset_cmd_complete(u8 *data)
{
	u8 status = data[0];

	if (cg2900_info->boot_state != BOOT_ACTIVATE_PATCHES_AND_SETTINGS)
		return false;

	if (HCI_BT_ERROR_NO_ERROR == status) {
/* ++ Hemant: Don't disable the BT IP */
#if 0		
		schedule_timeout_interruptible(msecs_to_jiffies(DELAY_BEFORE_SENDING_DISABLE_BT_IP_WAIT));
		/* Send HCI BluetoothEnable IP command */
		SET_BOOT_STATE(BOOT_DISABLING_BT_IP);
		hci_vs_enable_bluetooth_cmd_req[4] = 0x00; /* Disable the BT IP */

		create_and_send_bt_cmd(hci_vs_enable_bluetooth_cmd_req,
				       sizeof(hci_vs_enable_bluetooth_cmd_req));
#else
		/*
		 * The boot sequence is now finished successfully.
		 * Set states and signal to waiting thread.
		 */
		SET_BOOT_STATE(BOOT_READY);
		cg2900_chip_startup_finished(0);
#endif
/* -- Hemant: Don't disable the BT IP */
	} else {
		CG2900_ERR("Received Reset complete event with status 0x%X", status);
		SET_BOOT_STATE(BOOT_FAILED);
		cg2900_chip_startup_finished(-EIO);
	}

	return true;
}

/**
 * handle_vs_bluetooth_enable_cmd_status() - Handles HCI VS Bluetooth Enable Command Status event.
 * @status:	Returned status of HCI VS Bluetooth Enable command.
 *
 * Returns:
 *   true,  if packet was handled internally,
 *   false, otherwise.
 */
static bool handle_vs_bluetooth_enable_cmd_status(u8 status)
{
//	if (cg2900_info->boot_state != BOOT_DISABLING_BT_IP)
		return false;

	/*
	 * Only do something if there is an error. Otherwise we will wait for
	 * CmdComplete.
	 */
	if (HCI_BT_ERROR_NO_ERROR != status) {
		CG2900_ERR("Command status for Bluetooth Enable Command received with"
			   " error 0x%X", status);
		SET_BOOT_STATE(BOOT_FAILED);
	}

	return true;
}

/**
 * handle_vs_bluetooth_enable_cmd_complete() - Handle HCI VS Bluetooth Enable Command Complete event.
 * @data:	Pointer to received HCI data packet.
 *
 * Returns:
 *   true,  if packet was handled internally,
 *   false, otherwise.
 */
static bool handle_vs_bluetooth_enable_cmd_complete(u8 *data)
{
	u8 status = data[0];

//	if (cg2900_info->boot_state != BOOT_DISABLING_BT_IP)
		return false;

	if (HCI_BT_ERROR_NO_ERROR == status) {
		/*
		 * The boot sequence is now finished successfully.
		 * Set states and signal to waiting thread.
		 */
		SET_BOOT_STATE(BOOT_READY);
		cg2900_chip_startup_finished(0);
	} else {
		CG2900_ERR("Received BluetoothEnable complete event with status 0x%X",
			   status);
		SET_BOOT_STATE(BOOT_FAILED);
		cg2900_chip_startup_finished(-EIO);
	}

	return true;
}

/**
 * handle_rx_data_bt_evt() - Check if received data should be handled in CG2900 chip driver.
 * @skb:	Data packet
 *
 * The handle_rx_data_bt_evt() function checks if received data should be
 * handled in CG2900 chip driver. If so handle it correctly.
 * Received data is always HCI BT Event.
 *
 * Returns:
 *   True,  if packet was handled internally,
 *   False, otherwise.
 */
static bool handle_rx_data_bt_evt(struct sk_buff *skb)
{
	bool pkt_handled = false;
	/* skb cannot be NULL here so it is safe to de-reference */
	u8 *data = &(skb->data[CG2900_SKB_RESERVE]);
	u8 event_code = data[0];
	u8 hci_ogf = HCI_BT_ID_TO_OGF(data[4]);
	u16 hci_ocf = HCI_BT_ID_TO_OCF(data[3], data[4]);

	/* First check the event code. Only handle Command Complete Event */
	if (HCI_BT_EVT_CMD_COMPLETE != event_code)
		return false;

	CG2900_DBG_DATA("Command complete: OGF = 0x%02X, OCF = 0x%04X",
			hci_ogf, hci_ocf);

	/* Move to first byte after OCF */
	data += 5;

	switch (hci_ogf) {
	case HCI_BT_OGF_LINK_CTRL:
		break;

	case HCI_BT_OGF_LINK_POLICY:
		break;

	case HCI_BT_OGF_CTRL_BB:
		switch (hci_ocf) {
		case HCI_BT_OCF_RESET:
			pkt_handled = handle_reset_cmd_complete(data);
			break;

		default:
			break;
		}; /* switch (hci_ocf) */
		break;

	case HCI_BT_OGF_LINK_INFO:
		break;

	case HCI_BT_OGF_LINK_STATUS:
		break;

	case HCI_BT_OGF_LINK_TESTING:
		break;

	case HCI_BT_OGF_VS:
		switch (hci_ocf) {
		case CG2900_BT_OCF_VS_STORE_IN_FS:
			pkt_handled = handle_vs_store_in_fs_cmd_complete(data);
			break;
		case CG2900_BT_OCF_VS_WRITE_FILE_BLOCK:
			pkt_handled =
				handle_vs_write_file_block_cmd_complete(data);
			break;

		case CG2900_BT_OCF_VS_POWER_SWITCH_OFF:
			pkt_handled =
				handle_vs_power_switch_off_cmd_complete(data);
			break;

		case CG2900_BT_OCF_VS_SYSTEM_RESET:
			pkt_handled = handle_vs_system_reset_cmd_complete(data);
			break;

		case CG2900_BT_OCF_VS_BLUETOOTH_ENABLE:
			pkt_handled = handle_vs_bluetooth_enable_cmd_complete(data);
			break;

		default:
			break;
		}; /* switch (hci_ocf) */
		break;

	default:
		break;
	}; /* switch (hci_ogf) */

	if (pkt_handled)
		kfree_skb(skb);

	return pkt_handled;
}

/**
 * handle_rx_data_bt_status() - Check if received data should be handled in CG2900 chip driver.
 * @skb:	Data packet
 *
 * The handle_rx_data_bt_status() function checks if received data should be
 * handled in CG2900 chip driver. If so handle it correctly.
 * Received data is always HCI BT Event.
 *
 * Returns:
 *   True,  if packet was handled internally,
 *   False, otherwise.
 */
static bool handle_rx_data_bt_status(struct sk_buff *skb)
{
	bool pkt_handled = false;
	/* skb cannot be NULL here so it is safe to de-reference */
	u8 *data = &(skb->data[CG2900_SKB_RESERVE]);
	u8 event_code = data[0];
	struct hci_event_hdr *evt;
	u16 op_code;

	/* First check the event code. Only handle Command Status Event */
	if (HCI_BT_EVT_CMD_STATUS != event_code)
		return false;

	CG2900_INFO("handle_rx_data_bt_status");
	evt = (struct hci_event_hdr *)data;
	data += sizeof(*evt);

	if (HCI_BT_EVT_CMD_STATUS == evt->evt) {
		struct hci_ev_cmd_status *cmd_status;

		cmd_status = (struct hci_ev_cmd_status *)data;

		op_code = le16_to_cpu(cmd_status->opcode);

		CG2900_DBG_DATA("Received Command Status: op_code = 0x%04X",
				op_code);

		if (op_code == CG2900_BT_OP_VS_WRITE_FILE_BLOCK)
			pkt_handled = handle_vs_write_file_block_cmd_status
				(cmd_status->status);

		if (op_code == CG2900_BT_OP_VS_BLUETOOTH_ENABLE)
			pkt_handled = handle_vs_bluetooth_enable_cmd_status
				(cmd_status->status);
	} else
		return false;

	if (pkt_handled)
		kfree_skb(skb);

	return pkt_handled;
}

/**
 * transmit_skb_with_flow_ctrl_bt() - Send the BT skb to the CCD if it is allowed or queue it.
 * @skb:	Data packet.
 * @dev:	Pointer to cg2900_device struct.
 *
 * The cpd_transmit_skb_to_ccd_with_flow_ctrl_bt() function checks if there are
 * tickets available and if so transmits buffer to CCD. Otherwise the skb and
 * user name is stored in a list for later sending.
 * If enabled, copy the transmitted data to the HCI logger as well.
 */
static void transmit_skb_with_flow_ctrl_bt(struct sk_buff *skb,
					   struct cg2900_device *dev)
{
	/*
	 * Because there are more users of some H4 channels (currently audio
	 * application for BT command and FM channel) we need to have an
	 * internal HCI command flow control in CG2900 driver.
	 * So check here how many tickets we have and store skb in a queue if
	 * there are no tickets left. The skb will be sent later when we get
	 * more ticket(s).
	 */
	spin_lock_bh(&(cg2900_info->tx_bt_lock));

	if ((cg2900_info->tx_nr_pkts_allowed_bt) > 0) {
		(cg2900_info->tx_nr_pkts_allowed_bt)--;
		CG2900_DBG("New tx_nr_pkts_allowed_bt = %d",
			   cg2900_info->tx_nr_pkts_allowed_bt);

		/*
		 * If it's command from audio app store the OpCode,
		 * it'll be used later to decide where to dispatch Command
		 * Complete event.
		 */
		if (cg2900_get_bt_audio_dev() == dev) {
			cg2900_info->audio_bt_cmd_op =
					HCI_BT_GET_ID(skb->data[1],
							  skb->data[2]);
			CG2900_DBG("Sending cmd from audio driver, saving "
				   "OpCode = 0x%x",
				   cg2900_info->audio_bt_cmd_op);
		}

		cg2900_send_to_chip(skb, dev->logger_enabled);
	} else {
		CG2900_DBG("Not allowed to send cmd to CCD, "
			    "storing in TX queue.");

		cg2900_skb_data(skb)->dev = dev;
		skb_queue_tail(&cg2900_info->tx_queue_bt, skb);
	}
	spin_unlock_bh(&(cg2900_info->tx_bt_lock));
}

/**
 * transmit_skb_with_flow_ctrl_fm() - Send the FM skb to the CCD if it is allowed or queue it.
 * @skb:	Data packet.
 * @dev:	Pointer to cg2900_device struct.
 *
 * The transmit_skb_with_flow_ctrl_fm() function checks if chip is available and
 * if so transmits buffer to CCD. Otherwise the skb and user name is stored in a
 * list for later sending.
 * Also it updates the FM radio mode if it's FM GOTOMODE command, this is needed
 * to know how to handle some FM DO commands complete events.
 * If enabled, copy the transmitted data to the HCI logger as well.
 */
static void transmit_skb_with_flow_ctrl_fm(struct sk_buff *skb,
					   struct cg2900_device *dev)
{
	u8 cmd_func = CG2900_FM_CMD_PARAM_NONE;
	u16 cmd_id = CG2900_FM_CMD_NONE;

	fm_parse_cmd(&(skb->data[0]), &cmd_func, &cmd_id);

	/*
	 * If there is an FM IP disable or reset send command and also reset
	 * the flow control and audio user.
	 */
	if (cmd_func == CG2900_FM_CMD_PARAM_DISABLE ||
	    cmd_func == CG2900_FM_CMD_PARAM_RESET) {
		spin_lock_bh(&cg2900_info->tx_fm_lock);
		fm_reset_flow_ctrl();
		spin_unlock_bh(&cg2900_info->tx_fm_lock);
		cg2900_send_to_chip(skb, dev->logger_enabled);
		return;
	}

	/*
	 * If there is a FM user and no FM audio user command pending just send
	 * FM command. It is up to the user of the FM channel to handle its own
	 * flow control.
	 */
	spin_lock_bh(&cg2900_info->tx_fm_lock);
	if (cg2900_get_fm_radio_dev() == dev &&
	    cg2900_info->audio_fm_cmd_id == CG2900_FM_CMD_NONE) {
		cg2900_info->hci_fm_cmd_func = cmd_func;
		CG2900_DBG("hci_fm_cmd_func 0x%X",
			   cg2900_info->hci_fm_cmd_func);
		/* If a GotoMode command update FM mode */
		fm_update_mode(&(skb->data[0]));
		cg2900_send_to_chip(skb, dev->logger_enabled);
	} else if (cg2900_get_fm_audio_dev() == dev &&
		   cg2900_info->hci_fm_cmd_func == CG2900_FM_CMD_PARAM_NONE &&
		   cg2900_info->audio_fm_cmd_id == CG2900_FM_CMD_NONE) {
		/*
		 * If it's command from fm audio user store the command id.
		 * It'll be used later to decide where to dispatch
		 * command complete event.
		 */
		cg2900_info->audio_fm_cmd_id = cmd_id;
		CG2900_DBG("audio_fm_cmd_id 0x%X",
			   cg2900_info->audio_fm_cmd_id);
		cg2900_send_to_chip(skb, dev->logger_enabled);
	} else {
		CG2900_DBG("Not allowed to send cmd to CCD, storing in "
			   "TX queue");

		cg2900_skb_data(skb)->dev = dev;
		skb_queue_tail(&cg2900_info->tx_queue_fm, skb);
	}
	spin_unlock_bh(&(cg2900_info->tx_fm_lock));
}

/**
 * chip_startup() - Start the chip.
 * @dev:	Chip info.
 *
 * The chip_startup() function downloads patches and other needed start
 * procedures.
 *
 * Returns:
 *   0 if there is no error.
 */
static int chip_startup(struct cg2900_chip_dev *dev)
{
	/* Start the boot sequence */
	SET_BOOT_STATE(BOOT_GET_FILES_TO_LOAD);
	create_work_item(work_load_patch_and_settings);

	return 0;
}

/**
 * chip_shutdown() - Shut down the chip.
 * @dev:	Chip info.
 *
 * The chip_shutdown() function shuts down the chip by sending PowerSwitchOff
 * command.
 *
 * Returns:
 *   0 if there is no error.
 */
static int chip_shutdown(struct cg2900_chip_dev *dev)
{
	/*
	 * Transmit HCI reset command to ensure the chip is using
	 * the correct transport and to put BT part in reset.
	 */
	SET_CLOSING_STATE(CLOSING_RESET);
	create_and_send_bt_cmd(hci_reset_cmd_req, sizeof(hci_reset_cmd_req));

	return 0;
}

/**
 * data_to_chip() - Called when data shall be sent to the chip.
 * @dev:	Chip info.
 * @cpd_dev:	CG2900 user for this packet.
 * @skb:	Packet to transmit.
 *
 * The data_to_chip() function updates flow control and itself
 * transmits packet to CCD if packet is BT command or FM radio.
 *
 * Returns:
 *   true if packet is handled by this driver.
 *   false otherwise.
 */
static bool data_to_chip(struct cg2900_chip_dev *dev,
			 struct cg2900_device *cpd_dev,
			 struct sk_buff *skb)
{
	bool packet_handled = false;

	if (cpd_dev->h4_channel == H4_CHANNEL_BT_CMD) {
		transmit_skb_with_flow_ctrl_bt(skb, cpd_dev);
		packet_handled = true;
	} else if (cpd_dev->h4_channel == H4_CHANNEL_FM_RADIO) {
		transmit_skb_with_flow_ctrl_fm(skb, cpd_dev);
		packet_handled = true;
	}

	return packet_handled;
}

/**
 * data_from_chip() - Called when data shall be sent to the chip.
 * @dev:	Chip info.
 * @cpd_dev:	CG2900 user for this packet.
 * @skb:	Packet received.
 *
 * The data_from_chip() function updates flow control and checks
 * if packet is a response for a packet it itself has transmitted.
 *
 * Returns:
 *   true if packet is handled by this driver.
 *   false otherwise.
 */
static bool data_from_chip(struct cg2900_chip_dev *dev,
			   struct cg2900_device *cpd_dev,
			   struct sk_buff *skb)
{
	bool packet_handled;
	int h4_channel;

	h4_channel = skb->data[0];

	/* First check if we should update flow control */
	if (h4_channel == H4_CHANNEL_BT_EVT)
		update_flow_ctrl_bt(skb);
	else if (h4_channel == H4_CHANNEL_FM_RADIO)
		update_flow_ctrl_fm(skb);

	/* Then check if this is a response to data we have sent */
	packet_handled = handle_rx_data_bt_evt(skb);

	if (!packet_handled)
		/* Check if this is not the event status for Write File Block Command */
		packet_handled = handle_rx_data_bt_status(skb);

	CG2900_INFO("data_from_chip returning = %d", packet_handled);
	return packet_handled;
}

/**
 * get_h4_channel() - Returns H:4 channel for the name.
 * @name:	Chip info.
 * @h4_channel:	CG2900 user for this packet.
 *
 * Returns:
 *   0 if there is no error.
 *   -ENXIO if channel is not found.
 */
static int get_h4_channel(char *name, int *h4_channel)
{
	int i;
	int err = -ENXIO;

	*h4_channel = -1;

	for (i = 0; *h4_channel == -1 && i < ARRAY_SIZE(cg2900_channels); i++) {
		if (0 == strncmp(name, cg2900_channels[i].name,
				 CG2900_MAX_NAME_SIZE)) {
			/* Device found. Return H4 channel */
			*h4_channel = cg2900_channels[i].h4_channel;
			err = 0;
		}
	}

	return err;
}

/**
 * is_bt_audio_user() - Checks if this packet is for the BT audio user.
 * @h4_channel:	H:4 channel for this packet.
 * @skb:	Packet to check.
 *
 * Returns:
 *   true if packet is for BT audio user.
 *   false otherwise.
 */
static bool is_bt_audio_user(int h4_channel, const struct sk_buff * const skb)
{
	u8 *data = &(skb->data[CG2900_SKB_RESERVE]);
	u8 event_code = data[0];
	bool bt_audio = false;

	if (h4_channel == H4_CHANNEL_BT_EVT) {
		u16 opcode = 0;

		if (HCI_BT_EVT_CMD_COMPLETE == event_code)
			opcode = HCI_BT_EVENT_GET_ID(data[3], data[4]);
		else if (HCI_BT_EVT_CMD_STATUS == event_code)
			opcode = HCI_BT_EVENT_GET_ID(data[4], data[5]);

		if (opcode != 0 && opcode == cg2900_info->audio_bt_cmd_op) {
			CG2900_DBG("BT OpCode match = 0x%04X", opcode);
			cg2900_info->audio_bt_cmd_op = CG2900_BT_OPCODE_NONE;
			bt_audio = true;
		}
	}

	return bt_audio;
}

/**
 * is_fm_audio_user() - Checks if this packet is for the FM audio user.
 * @h4_channel:	H:4 channel for this packet.
 * @skb:	Packet to check.
 *
 * Returns:
 *   true if packet is for BT audio user.
 *   false otherwise.
 */
static bool is_fm_audio_user(int h4_channel, const struct sk_buff * const skb)
{
	u8 cmd_func = CG2900_FM_CMD_PARAM_NONE;
	u16 cmd_id = CG2900_FM_CMD_NONE;
	u16 irpt_val = 0;
	u8 event = CG2900_FM_EVENT_UNKNOWN;
	bool bt_audio = false;

	fm_parse_event(&(skb->data[0]), &event, &cmd_func, &cmd_id, &irpt_val);

	if (h4_channel == H4_CHANNEL_FM_RADIO) {
		/* Check if command complete event FM legacy interface. */
		if ((event == CG2900_FM_EVENT_CMD_COMPLETE) &&
		    (cmd_func == CG2900_FM_CMD_PARAM_WRITECOMMAND) &&
		    (cmd_id == cg2900_info->audio_fm_cmd_id)) {
			CG2900_DBG("FM Audio Function Code match = 0x%04X",
				   cmd_id);
			bt_audio = true;
			goto finished;
		}

		/* Check if Interrupt legacy interface. */
		if ((event == CG2900_FM_EVENT_INTERRUPT) &&
		    (fm_is_do_cmd_irpt(irpt_val)) &&
		    (cg2900_info->tx_fm_audio_awaiting_irpt))
			bt_audio = true;
	}

finished:
	return bt_audio;
}

/**
 * last_bt_user_removed() - Called when last BT user is removed.
 * @dev:	Chip handler info.
 *
 * Clears out TX queue for BT.
 */
static void last_bt_user_removed(struct cg2900_chip_dev *dev)
{
	spin_lock_bh(&cg2900_info->tx_bt_lock);

	skb_queue_purge(&cg2900_info->tx_queue_bt);

	/*
	 * Reset number of packets allowed and number of outstanding
	 * BT commands.
	 */
	cg2900_info->tx_nr_pkts_allowed_bt = 1;
	/* Reset the audio_bt_cmd_op. */
	cg2900_info->audio_bt_cmd_op = CG2900_BT_OPCODE_NONE;
	spin_unlock_bh(&cg2900_info->tx_bt_lock);
}

/**
 * last_fm_user_removed() - Called when last FM user is removed.
 * @dev:	Chip handler info.
 *
 * Clears out TX queue for BT.
 */
static void last_fm_user_removed(struct cg2900_chip_dev *dev)
{
	spin_lock_bh(&cg2900_info->tx_fm_lock);
	fm_reset_flow_ctrl();
	spin_unlock_bh(&cg2900_info->tx_fm_lock);
}

/**
 * check_chip_support() - Checks if connected chip is handled by this driver.
 * @h4_channel:	H:4 channel for this packet.
 * @skb:	Packet to check.
 *
 * If supported return true and fill in @callbacks.
 *
 * Returns:
 *   true if chip is handled by this driver.
 *   false otherwise.
 */
static bool check_chip_support(struct cg2900_chip_dev *dev)
{
	CG2900_INFO("CG2900: check_chip_support");

	/*
	 * Check if this is a CG2900 revision.
	 * We do not care about the sub-version at the moment. Change this if
	 * necessary.
	 */
	if ((dev->chip.manufacturer != CG2900_SUPP_MANUFACTURER) ||
	    (dev->chip.hci_revision != CG2900_PG1_SPECIAL_REV &&
	     (dev->chip.hci_revision < CG2900_SUPP_REVISION_MIN ||
	     dev->chip.hci_revision > CG2900_SUPP_REVISION_MAX))) {
		CG2900_DBG("Chip not supported by CG2900 driver\n"
			   "\tMan: 0x%02X\n\tRev: 0x%04X\n\tSub: 0x%04X",
			   dev->chip.manufacturer, dev->chip.hci_revision,
			   dev->chip.hci_sub_version);
		return false;
	}

	CG2900_INFO("Chip supported by the CG2900 driver");
	/* Store needed data */
	dev->user_data = cg2900_info;
	memcpy(&(cg2900_info->chip_dev), dev, sizeof(*dev));
	/* Set the callbacks */
	dev->cb.chip_shutdown = chip_shutdown;
	dev->cb.chip_startup = chip_startup;
	dev->cb.data_from_chip = data_from_chip;
	dev->cb.data_to_chip = data_to_chip;
	dev->cb.get_h4_channel = get_h4_channel;
	dev->cb.is_bt_audio_user = is_bt_audio_user;
	dev->cb.is_fm_audio_user = is_fm_audio_user;
	dev->cb.last_bt_user_removed = last_bt_user_removed;
	dev->cb.last_fm_user_removed = last_fm_user_removed;

	return true;
}

static struct cg2900_id_callbacks chip_support_callbacks = {
	.check_chip_support = check_chip_support
};

/**
 * cg2900_chip_init() - Initialize module.
 *
 * The cg2900_chip_init() function initializes the CG2900 driver,
 * then registers to the CG2900 Core.
 *
 * Returns:
 *   0 if success.
 *   -ENOMEM for failed alloc or structure creation.
 *   Error codes generated by cg2900_register_chip_driver.
 */
//static int __init cg2900_chip_init(void)
int cg2900_chip_init(void)
{
	int err = 0;

	CG2900_INFO("cg2900_chip_init");

	cg2900_info = kzalloc(sizeof(*cg2900_info), GFP_ATOMIC);
	if (!cg2900_info) {
		CG2900_ERR("Couldn't allocate cg2900_info");
		err = -ENOMEM;
		goto finished;
	}

	/*
	 * Initialize linked lists for HCI BT and FM commands
	 * that can't be sent due to internal CG2900 flow control.
	 */
	skb_queue_head_init(&cg2900_info->tx_queue_bt);
	skb_queue_head_init(&cg2900_info->tx_queue_fm);

	/* Initialize the spin locks */
	spin_lock_init(&(cg2900_info->tx_bt_lock));
	spin_lock_init(&(cg2900_info->tx_fm_lock));

	cg2900_info->tx_nr_pkts_allowed_bt = 1;
	cg2900_info->audio_bt_cmd_op = CG2900_BT_OPCODE_NONE;
	cg2900_info->audio_fm_cmd_id = CG2900_FM_CMD_NONE;
	cg2900_info->hci_fm_cmd_func = CG2900_FM_CMD_PARAM_NONE;
	cg2900_info->fm_radio_mode = FM_RADIO_MODE_IDLE;

	cg2900_info->wq = create_singlethread_workqueue(WQ_NAME);
	if (!cg2900_info->wq) {
		CG2900_ERR("Could not create workqueue");
		err = -ENOMEM;
		goto err_handling_free_info;
	}

	/* Allocate file names that will be used, deallocated in cg2900_exit */
	cg2900_info->patch_file_name = kzalloc(FILENAME_MAX + 1, GFP_ATOMIC);
	if (!cg2900_info->patch_file_name) {
		CG2900_ERR("Couldn't allocate name buffer for patch file.");
		err = -ENOMEM;
		goto err_handling_destroy_wq;
	}
	/* Allocate file names that will be used, deallocated in cg2900_exit */
	cg2900_info->settings_file_name = kzalloc(FILENAME_MAX + 1, GFP_ATOMIC);
	if (!cg2900_info->settings_file_name) {
		CG2900_ERR("Couldn't allocate name buffers settings file.");
		err = -ENOMEM;
		goto err_handling_free_patch_name;
	}

	err = cg2900_register_chip_driver(&chip_support_callbacks);
	if (err) {
		CG2900_ERR("Couldn't register chip driver (%d)", err);
		goto err_handling_free_settings_name;
	}

	goto finished;

err_handling_free_settings_name:
	kfree(cg2900_info->settings_file_name);
err_handling_free_patch_name:
	kfree(cg2900_info->patch_file_name);
err_handling_destroy_wq:
	destroy_workqueue(cg2900_info->wq);
err_handling_free_info:
	kfree(cg2900_info);
	cg2900_info = NULL;
finished:
	return err;
}

/**
 * cg2900_chip_exit() - Remove module.
 */
//static void __exit cg2900_chip_exit(void)
void cg2900_chip_exit(void)
{
	CG2900_INFO("cg2900_chip_exit");

	if (!cg2900_info)
		return;

	kfree(cg2900_info->settings_file_name);
	kfree(cg2900_info->patch_file_name);
	destroy_workqueue(cg2900_info->wq);
	kfree(cg2900_info);
	cg2900_info = NULL;
}

//module_init(cg2900_chip_init);
//module_exit(cg2900_chip_exit);

MODULE_AUTHOR("Par-Gunnar Hjalmdahl ST-Ericsson");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Linux CG2900 Connectivity Device Driver");
