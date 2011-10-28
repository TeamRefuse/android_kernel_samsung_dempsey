/*
 * drivers/mfd/cg2900/ste_stlc2690.c
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
 * Linux Bluetooth HCI H:4 Driver for ST-Ericsson connectivity controller
 * STLC2690.
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

#include <linux/mfd/cg2900.h>
#include <mach/cg2900_devices.h>
#include "hci_defines.h"
#include "stlc2690_chip.h"
#include "cg2900_core.h"
#include "cg2900_debug.h"

#define LINE_BUFFER_LENGTH			128
#define FILENAME_MAX				128

#define WQ_NAME					"stlc2690_wq"
#define PATCH_INFO_FILE				"cg2900_patch_info.fw"
#define FACTORY_SETTINGS_INFO_FILE		"cg2900_settings_info.fw"

/* Supported chips */
#define SUPP_MANUFACTURER			0x30
#define SUPP_REVISION_MIN			0x0500
#define SUPP_REVISION_MAX			0x06FF

/* Size of file chunk ID */
#define FILE_CHUNK_ID_SIZE			1
#define VS_SEND_FILE_CHUNK_ID_POS		4
#define BT_CMD_LEN_POS				3

/* State setting macros */
#define SET_BOOT_STATE(__new_state) \
	CG2900_SET_STATE("boot_state", stlc2690_info->boot_state, __new_state)
#define SET_FILE_LOAD_STATE(__new_state) \
	CG2900_SET_STATE("file_load_state", stlc2690_info->file_load_state, \
			 __new_state)
#define SET_DOWNLOAD_STATE(__new_state) \
	CG2900_SET_STATE("download_state", stlc2690_info->download_state, \
			 __new_state)

/** CHANNEL_BT_CMD - Bluetooth HCI H:4 channel
 * for Bluetooth commands in the ST-Ericsson connectivity controller.
 */
#define CHANNEL_BT_CMD				0x01

/** CHANNEL_BT_ACL - Bluetooth HCI H:4 channel
 * for Bluetooth ACL data in the ST-Ericsson connectivity controller.
 */
#define CHANNEL_BT_ACL				0x02

/** CHANNEL_BT_EVT - Bluetooth HCI H:4 channel
 * for Bluetooth events in the ST-Ericsson connectivity controller.
 */
#define CHANNEL_BT_EVT				0x04

/** CHANNEL_HCI_LOGGER - Bluetooth HCI H:4 channel
 * for logging all transmitted H4 packets (on all channels).
 */
#define CHANNEL_HCI_LOGGER			0xFA

/** CHANNEL_US_CTRL - Bluetooth HCI H:4 channel
 * for user space control of the ST-Ericsson connectivity controller.
 */
#define CHANNEL_US_CTRL				0xFC

/** CHANNEL_CORE - Bluetooth HCI H:4 channel
 * for user space control of the ST-Ericsson connectivity controller.
 */
#define CHANNEL_CORE				0xFD

/**
 * struct stlc2690_work_struct - Work structure for CG2900 Core module.
 * @work:	Work structure.
 * @skb:	Data packet.
 * @data:	Private data for user.
 *
 * This structure is used to pack work for work queue.
 */
struct stlc2690_work_struct {
	struct work_struct	work;
	struct sk_buff		*skb;
	void			*data;
};

/**
 * enum boot_state - BOOT-state for CG2900 Core.
 * @BOOT_NOT_STARTED:			Boot has not yet started.
 * @BOOT_SEND_BD_ADDRESS:		VS Store In FS command with BD address
 *					has been sent.
 * @BOOT_GET_FILES_TO_LOAD:		CG2900 Core is retreiving file to load.
 * @BOOT_DOWNLOAD_PATCH:		CG2900 Core is downloading patches.
 * @BOOT_ACTIVATE_PATCHES_AND_SETTINGS:	CG2900 Core is activating patches and
 *					settings.
 * @BOOT_READY:				CG2900 Core boot is ready.
 * @BOOT_FAILED:			CG2900 Core boot failed.
 */
enum boot_state {
	BOOT_NOT_STARTED,
	BOOT_SEND_BD_ADDRESS,
	BOOT_GET_FILES_TO_LOAD,
	BOOT_DOWNLOAD_PATCH,
	BOOT_ACTIVATE_PATCHES_AND_SETTINGS,
	BOOT_READY,
	BOOT_FAILED
};

/**
 * enum file_load_state - BOOT_FILE_LOAD-state for CG2900 Core.
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
 * struct stlc2690_device_id - Structure for connecting H4 channel to user.
 * @name:		Name of device.
 * @h4_channel:	HCI H:4 channel used by this device.
 */
struct stlc2690_device_id {
	char	*name;
	int	h4_channel;
};

/**
 * struct stlc2690_info - Main info structure for STLC2690.
 * @patch_file_name:	Stores patch file name.
 * @settings_file_name:	Stores settings file name.
 * @fw_file:		Stores firmware file (patch or settings).
 * @file_offset:	Current read offset in firmware file.
 * @chunk_id:		Stores current chunk ID of write file operations.
 * @boot_state:		Current BOOT-state of STLC2690.
 * @file_load_state:	Current BOOT_FILE_LOAD-state of STLC2690.
 * @download_state:	Current BOOT_DOWNLOAD-state of STLC2690.
 * @wq:			STLC2690 workqueue.
 * @chip_dev:		Chip info.
 */
struct stlc2690_info {
	char				*patch_file_name;
	char				*settings_file_name;
	const struct firmware		*fw_file;
	int				file_offset;
	u8				chunk_id;
	enum boot_state			boot_state;
	enum file_load_state		file_load_state;
	enum download_state		download_state;
	struct workqueue_struct		*wq;
	struct cg2900_chip_dev		chip_dev;
};

static struct stlc2690_info *stlc2690_info;

/*
 * reset_cmd_req - Hardcoded HCI Reset command.
 */
static const u8 reset_cmd_req[] = {
	0x00, /* Reserved for H4 channel*/
	HCI_BT_MAKE_FIRST_BYTE_IN_CMD(HCI_BT_OCF_RESET),
	HCI_BT_MAKE_SECOND_BYTE_IN_CMD(HCI_BT_OGF_CTRL_BB, HCI_BT_OCF_RESET),
	0x00
};

/*
 * vs_store_in_fs_cmd_req - Hardcoded HCI Store in FS command.
 */
static const u8 vs_store_in_fs_cmd_req[] = {
	0x00, /* Reserved for H4 channel*/
	HCI_BT_MAKE_FIRST_BYTE_IN_CMD(STLC2690_BT_OCF_VS_STORE_IN_FS),
	HCI_BT_MAKE_SECOND_BYTE_IN_CMD(HCI_BT_OGF_VS,
				       STLC2690_BT_OCF_VS_STORE_IN_FS),
	0x00, /* 1 byte for HCI command length. */
	0x00, /* 1 byte for User_Id. */
	0x00  /* 1 byte for vs_store_in_fs_cmd_req parameter data length. */
};

/*
 * vs_write_file_block_cmd_req -
 * Hardcoded HCI Write File Block vendor specific command
 */
static const u8 vs_write_file_block_cmd_req[] = {
	0x00, /* Reserved for H4 channel*/
	HCI_BT_MAKE_FIRST_BYTE_IN_CMD(STLC2690_BT_OCF_VS_WRITE_FILE_BLOCK),
	HCI_BT_MAKE_SECOND_BYTE_IN_CMD(HCI_BT_OGF_VS,
				       STLC2690_BT_OCF_VS_WRITE_FILE_BLOCK),
	0x00
};

#define NBR_OF_DEVS			6

/*
 * stlc2690_channels() - Array containing available H4 channels for the STLC2690
 * ST-Ericsson Connectivity controller.
 */
struct stlc2690_device_id stlc2690_channels[NBR_OF_DEVS] = {
	{CG2900_BT_CMD,			CHANNEL_BT_CMD},
	{CG2900_BT_ACL,			CHANNEL_BT_ACL},
	{CG2900_BT_EVT,			CHANNEL_BT_EVT},
	{CG2900_HCI_LOGGER,		CHANNEL_HCI_LOGGER},
	{CG2900_US_CTRL,		CHANNEL_US_CTRL},
	{CG2900_CORE,			CHANNEL_CORE}
};

/*
 * Internal functions
 */

/**
 * create_and_send_bt_cmd() - Copy and send sk_buffer.
 * @data:	Data to send.
 * @length:	Length in bytes of data.
 *
 * The create_and_send_bt_cmd() function allocate sk_buffer, copy supplied data
 * to it, and send the sk_buffer to CG2900 Core.
 * Note that the data must contain the H:4 header.
 */
static void create_and_send_bt_cmd(const u8 *data, int length)
{
	struct sk_buff *skb;
	struct cg2900_hci_logger_config *logger_config;
	int err;

	skb = alloc_skb(length, GFP_ATOMIC);
	if (!skb) {
		CG2900_ERR("Couldn't allocate sk_buff with length %d", length);
		return;
	}

	memcpy(skb_put(skb, length), data, length);
	skb->data[0] = CHANNEL_BT_CMD;

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
 * send_bd_address() - Send HCI VS command with BD address to the chip.
 */
static void send_bd_address(void)
{
	u8 tmp[sizeof(vs_store_in_fs_cmd_req) + BT_BDADDR_SIZE];

	/* Send vs_store_in_fs_cmd with BD address. */
	memcpy(tmp, vs_store_in_fs_cmd_req, sizeof(vs_store_in_fs_cmd_req));

	tmp[HCI_BT_CMD_PARAM_LEN_POS] = STLC2690_VS_STORE_IN_FS_USR_ID_SIZE +
					STLC2690_VS_STORE_IN_FS_PARAM_LEN_SIZE +
					BT_BDADDR_SIZE;
	tmp[STLC2690_VS_STORE_IN_FS_USR_ID_POS] =
			STLC2690_VS_STORE_IN_FS_USR_ID_BD_ADDR;
	tmp[STLC2690_VS_STORE_IN_FS_PARAM_LEN_POS] = BT_BDADDR_SIZE;

	/* Now copy the bd address received from user space control app. */
	memcpy(&(tmp[STLC2690_VS_STORE_IN_FS_PARAM_POS]), bd_address,
	       sizeof(bd_address));

	SET_BOOT_STATE(BOOT_SEND_BD_ADDRESS);

	create_and_send_bt_cmd(tmp, sizeof(tmp));
}

/**
 * create_work_item() - Create work item and add it to the work queue.
 * @work_func:	Work function.
 * @skb:	Data packet.
 * @data:	Private data for caller.
 */
static void create_work_item(work_func_t work_func, struct sk_buff *skb,
			     void *data)
{
	struct stlc2690_work_struct *new_work;
	int wq_err = 1;

	new_work = kmalloc(sizeof(*new_work), GFP_ATOMIC);
	if (!new_work) {
		CG2900_ERR("Failed to alloc memory for stlc2690_work_struct!");
		return;
	}

	new_work->skb = skb;
	new_work->data = data;
	INIT_WORK(&new_work->work, work_func);

	wq_err = queue_work(stlc2690_info->wq, &new_work->work);
	if (!wq_err) {
		CG2900_ERR("Failed to queue work_struct because it's already in"
			   " the queue!");
		kfree(new_work);
	}
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
 *   True,  if target file was found,
 *   False, otherwise.
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
		CG2900_ERR("Failed to allocate: file_name 0x%X, "
			   "line_buffer 0x%X",
			   (u32)file_name, (u32)line_buffer);
		goto finished;
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
		 * Check if the line of text is a comment or not,
		 * comments begin with '#'
		 */
		if (*line_buffer == '#')
			continue;

		hci_rev = 0;
		lmp_sub = 0;

		CG2900_DBG("Found a valid line <%s>", line_buffer);

		/*
		 * Check if we can find the correct HCI revision and
		 * LMP subversion as well as a file name in the text line.
		 * Store the filename if the actual file can be found in
		 * the file system.
		 */
		if (sscanf(line_buffer, "%x%x%s", &hci_rev, &lmp_sub,
			   *file_name) == 3
		    && hci_rev == stlc2690_info->chip_dev.chip.hci_revision
		    && lmp_sub ==
				 stlc2690_info->chip_dev.chip.hci_sub_version) {
			CG2900_DBG("File matching chip found\n"
				   "\tFile name = %s\n"
				   "\tHCI Revision = 0x%X\n"
				   "\tLMP PAL Subversion = 0x%X",
				   *file_name, hci_rev, lmp_sub);

			/*
			 * Name has already been stored above. Nothing more to
			 * do.
			 */
			file_found = true;
		} else {
			/* Zero the name buffer so it is clear to next read */
			memset(*file_name, 0x00, FILENAME_MAX + 1);
		}
	}
	kfree(line_buffer);

finished:
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

	/*
	 * Calculate number of bytes to copy;
	 * either max bytes for HCI packet or number of bytes left in file
	 */
	bytes_to_copy = min((int)HCI_BT_SEND_FILE_MAX_CHUNK_SIZE,
			    (int)(stlc2690_info->fw_file->size -
				  stlc2690_info->file_offset));

	if (bytes_to_copy <= 0) {
		/* Nothing more to read in file. */
		SET_DOWNLOAD_STATE(DOWNLOAD_SUCCESS);
		stlc2690_info->chunk_id = 0;
		stlc2690_info->file_offset = 0;
		return;
	}

	logger_config = cg2900_get_hci_logger_config();

	/* There are bytes to transmit. Allocate a sk_buffer. */
	skb = alloc_skb(sizeof(vs_write_file_block_cmd_req) +
			HCI_BT_SEND_FILE_MAX_CHUNK_SIZE +
			FILE_CHUNK_ID_SIZE, GFP_ATOMIC);
	if (!skb) {
		CG2900_ERR("Couldn't allocate sk_buffer");
		SET_BOOT_STATE(BOOT_FAILED);
		cg2900_chip_startup_finished(-EIO);
		return;
	}

	/* Copy the data from the HCI template */
	memcpy(skb_put(skb, sizeof(vs_write_file_block_cmd_req)),
	       vs_write_file_block_cmd_req,
	       sizeof(vs_write_file_block_cmd_req));
	skb->data[0] = CHANNEL_BT_CMD;

	/* Store the chunk ID */
	skb->data[VS_SEND_FILE_CHUNK_ID_POS] = stlc2690_info->chunk_id;
	skb_put(skb, 1);
	stlc2690_info->chunk_id++;

	/* Copy the data from offset position and store the length */
	memcpy(skb_put(skb, bytes_to_copy),
	       &(stlc2690_info->fw_file->data[stlc2690_info->file_offset]),
	       bytes_to_copy);
	skb->data[BT_CMD_LEN_POS] = bytes_to_copy + FILE_CHUNK_ID_SIZE;

	/* Increase offset with number of bytes copied */
	stlc2690_info->file_offset += bytes_to_copy;

	if (logger_config)
		cg2900_send_to_chip(skb, logger_config->bt_cmd_enable);
	else
		cg2900_send_to_chip(skb, false);
}

/**
 * send_settings_file() - Transmit settings file.
 *
 * The send_settings_file() function transmit settings file.
 * The file is read in parts to fit in HCI packets.
 * When finished, close the settings file and send HCI reset to activate
 * settings and patches.
 */
static void send_settings_file(void)
{
	/* Transmit a file part */
	read_and_send_file_part();

	if (stlc2690_info->download_state != DOWNLOAD_SUCCESS)
		return;

	/* Settings file finished. Release used resources */
	CG2900_DBG("Settings file finished, release used resources");

	if (stlc2690_info->fw_file) {
		release_firmware(stlc2690_info->fw_file);
		stlc2690_info->fw_file = NULL;
	}

	SET_FILE_LOAD_STATE(FILE_LOAD_NO_MORE_FILES);

	/* Create and send HCI VS Store In FS command with bd address. */
	send_bd_address();
}

/**
 * send_patch_file - Transmit patch file.
 *
 * The send_patch_file() function transmit patch file. The file is read in parts
 * to fit in HCI packets.
 * When the complete file is transmitted, the file is closed.
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

	if (stlc2690_info->download_state != DOWNLOAD_SUCCESS)
		return;

	/* Patch file finished. Release used resources */
	CG2900_DBG("Patch file finished, release used resources");

	if (stlc2690_info->fw_file) {
		release_firmware(stlc2690_info->fw_file);
		stlc2690_info->fw_file = NULL;
	}

	err = request_firmware(&(stlc2690_info->fw_file),
			       stlc2690_info->settings_file_name,
			       stlc2690_info->chip_dev.dev);
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
 * work_reset_after_error() - Handle reset.
 * @work:	Reference to work data.
 *
 * Handle a reset after received command complete event.
 */
static void work_reset_after_error(struct work_struct *work)
{
	struct stlc2690_work_struct *current_work = NULL;

	if (!work) {
		CG2900_ERR("work == NULL");
		return;
	}

	current_work = container_of(work, struct stlc2690_work_struct, work);

	cg2900_chip_startup_finished(-EIO);

	kfree(current_work);
}

/**
 * work_load_patch_and_settings() - Start loading patches and settings.
 * @work:	Reference to work data.
 */
static void work_load_patch_and_settings(struct work_struct *work)
{
	struct stlc2690_work_struct *current_work;
	int err = 0;
	bool file_found;
	const struct firmware *patch_info;
	const struct firmware *settings_info;

	if (!work) {
		CG2900_ERR("work == NULL");
		return;
	}

	current_work = container_of(work, struct stlc2690_work_struct, work);

	/* Check that we are in the right state */
	if (stlc2690_info->boot_state != BOOT_GET_FILES_TO_LOAD)
		goto finished;

	/* Open patch info file. */
	err = request_firmware(&patch_info, PATCH_INFO_FILE,
			       stlc2690_info->chip_dev.dev);
	if (err) {
		CG2900_ERR("Couldn't get patch info file (%d)", err);
		goto error_handling;
	}

	/*
	 * Now we have the patch info file.
	 * See if we can find the right patch file as well
	 */
	file_found = get_file_to_load(patch_info,
				      &(stlc2690_info->patch_file_name));

	/* Now we are finished with the patch info file */
	release_firmware(patch_info);

	if (!file_found) {
		CG2900_ERR("Couldn't find patch file! Major error!");
		goto error_handling;
	}

	/* Open settings info file. */
	err = request_firmware(&settings_info, FACTORY_SETTINGS_INFO_FILE,
			       stlc2690_info->chip_dev.dev);
	if (err) {
		CG2900_ERR("Couldn't get settings info file (%d)", err);
		goto error_handling;
	}

	/*
	 * Now we have the settings info file.
	 * See if we can find the right settings file as well
	 */
	file_found = get_file_to_load(settings_info,
				      &(stlc2690_info->settings_file_name));

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
	stlc2690_info->chunk_id = 0;
	stlc2690_info->file_offset = 0;
	stlc2690_info->fw_file = NULL;

	/* OK. Now it is time to download the patches */
	err = request_firmware(&(stlc2690_info->fw_file),
			       stlc2690_info->patch_file_name,
			       stlc2690_info->chip_dev.dev);
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
	kfree(current_work);
}

/**
 * work_cont_with_file_download() - A file block has been written.
 * @work:	Reference to work data.
 *
 * Handle a received HCI VS Write File Block Complete event.
 * Normally this means continue to send files to the controller.
 */
static void work_cont_with_file_download(struct work_struct *work)
{
	struct stlc2690_work_struct *current_work;

	if (!work) {
		CG2900_ERR("work == NULL");
		return;
	}

	current_work = container_of(work, struct stlc2690_work_struct, work);

	/* Continue to send patches or settings to the controller */
	if (stlc2690_info->file_load_state == FILE_LOAD_GET_PATCH)
		send_patch_file();
	else if (stlc2690_info->file_load_state ==
			FILE_LOAD_GET_STATIC_SETTINGS)
		send_settings_file();
	else
		CG2900_INFO("No more files to load");

	kfree(current_work);
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
	u8 status;

	CG2900_INFO("Received Reset complete event");

	if (stlc2690_info->boot_state != BOOT_ACTIVATE_PATCHES_AND_SETTINGS)
		return false;

	status = data[0];

	if (HCI_BT_ERROR_NO_ERROR == status) {
		/*
		 * The boot sequence is now finished successfully.
		 * Set states and signal to waiting thread.
		 */
		SET_BOOT_STATE(BOOT_READY);
		cg2900_chip_startup_finished(0);
	} else {
		CG2900_ERR("Received Reset complete event with status 0x%X",
			   status);
		SET_BOOT_STATE(BOOT_FAILED);
		cg2900_chip_startup_finished(-EIO);
	}
	return true;
}

/**
 * handle_vs_store_in_fs_cmd_complete_evt() - Handle a received HCI Command Complete event for a VS StoreInFS command.
 * @data:	Pointer to received HCI data packet.
 *
 * Returns:
 *   True,  if packet was handled internally,
 *   False, otherwise.
 */
static bool handle_vs_store_in_fs_cmd_complete_evt(u8 *data)
{
	u8 status;

	CG2900_INFO("Received Store_in_FS complete event");

	if (stlc2690_info->boot_state != BOOT_SEND_BD_ADDRESS)
		return false;

	status = data[0];

	if (HCI_BT_ERROR_NO_ERROR == status) {
		/* Send HCI Reset command to activate patches */
		SET_BOOT_STATE(BOOT_ACTIVATE_PATCHES_AND_SETTINGS);
		create_and_send_bt_cmd(reset_cmd_req, sizeof(reset_cmd_req));
	} else {
		CG2900_ERR("Command Complete for StoreInFS received with "
			   "error 0x%X", status);
		SET_BOOT_STATE(BOOT_FAILED);
		create_work_item(work_reset_after_error, NULL, NULL);
	}
	/* We have now handled the packet */
	return true;
}

/**
 * handle_vs_write_file_block_cmd_complete_evt() - Handle a received HCI Command Complete event for a VS WriteFileBlock command.
 * @data:	Pointer to received HCI data packet.
 *
 * Returns:
 *   True,  if packet was handled internally,
 *   False, otherwise.
 */
static bool handle_vs_write_file_block_cmd_complete_evt(u8 *data)
{
	u8 status;

	if ((stlc2690_info->boot_state != BOOT_DOWNLOAD_PATCH) ||
	    (stlc2690_info->download_state != DOWNLOAD_PENDING))
		return false;

	status = data[0];
	if (HCI_BT_ERROR_NO_ERROR == status) {
		/* Received good confirmation. Start work to continue. */
		create_work_item(work_cont_with_file_download, NULL, NULL);
	} else {
		CG2900_ERR("Command Complete for WriteFileBlock received with "
			   "error 0x%X", status);
		SET_DOWNLOAD_STATE(DOWNLOAD_FAILED);
		SET_BOOT_STATE(BOOT_FAILED);
		if (stlc2690_info->fw_file) {
			release_firmware(stlc2690_info->fw_file);
			stlc2690_info->fw_file = NULL;
		}
		create_work_item(work_reset_after_error, NULL, NULL);
	}
	/* We have now handled the packet */
	return true;
}

/**
 * handle_rx_data_bt_evt() - Check if received data should be handled.
 * @skb:	Data packet
 *
 * The handle_rx_data_bt_evt() function checks if received data should be
 * handled by STLC2690. If so handle it correctly.
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
	u8 hci_ogf;
	u16 hci_ocf;

	/* First check the event code */
	if (HCI_BT_EVT_CMD_COMPLETE != event_code)
		return false;

	hci_ogf = HCI_BT_ID_TO_OGF(data[HCI_BT_EVT_CMD_COMPL_ID_POS + 1]);
	hci_ocf = HCI_BT_ID_TO_OCF(data[HCI_BT_EVT_CMD_COMPL_ID_POS],
				   data[HCI_BT_EVT_CMD_COMPL_ID_POS + 1]);

	CG2900_DBG_DATA("Command complete: OGF = 0x%02X, OCF = 0x%04X",
			hci_ogf, hci_ocf);
	data += 5; /* Move to first byte after OCF */

	if (hci_ogf == HCI_BT_OGF_CTRL_BB && hci_ocf == HCI_BT_OCF_RESET)
		pkt_handled = handle_reset_cmd_complete_evt(data);
	else if (hci_ogf == HCI_BT_OGF_VS &&
		 hci_ocf == STLC2690_BT_OCF_VS_STORE_IN_FS)
		pkt_handled = handle_vs_store_in_fs_cmd_complete_evt(data);
	else if (hci_ogf == HCI_BT_OGF_VS &&
		 hci_ocf == STLC2690_BT_OCF_VS_WRITE_FILE_BLOCK)
		pkt_handled = handle_vs_write_file_block_cmd_complete_evt(data);

	if (pkt_handled)
		kfree_skb(skb);

	return pkt_handled;
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
	create_work_item(work_load_patch_and_settings, NULL, NULL);

	return 0;
}

/**
 * data_from_chip() - Called when data shall be sent to the chip.
 * @dev:	Chip info.
 * @cg2900_dev:	CG2900 user for this packet.
 * @skb:	Packet received.
 *
 * The data_from_chip() function checks if packet is a response for a packet it
 * itself has transmitted.
 *
 * Returns:
 *   true if packet is handled by this driver.
 *   false otherwise.
 */
static bool data_from_chip(struct cg2900_chip_dev *dev,
			   struct cg2900_device *cg2900_dev,
			   struct sk_buff *skb)
{
	/* Then check if this is a response to data we have sent */
	return handle_rx_data_bt_evt(skb);
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

	for (i = 0; *h4_channel == -1 && i < NBR_OF_DEVS; i++) {
		if (0 == strncmp(name, stlc2690_channels[i].name,
				 CG2900_MAX_NAME_SIZE)) {
			/* Device found. Return H4 channel */
			*h4_channel = stlc2690_channels[i].h4_channel;
			err = 0;
		}
	}

	return err;
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
	CG2900_INFO("check_chip_support");

	/*
	 * Check if this is a CG2690 revision. We do not care about
	 * the sub-version at the moment.
	 * Change this if necessary.
	 */
	if ((dev->chip.manufacturer != SUPP_MANUFACTURER) ||
	    (dev->chip.hci_revision < SUPP_REVISION_MIN) ||
	    (dev->chip.hci_revision > SUPP_REVISION_MAX)) {
		CG2900_DBG("Chip not supported by STLC2690 driver\n"
			   "\tMan: 0x%02X\n"
			   "\tRev: 0x%04X\n"
			   "\tSub: 0x%04X",
			   dev->chip.manufacturer, dev->chip.hci_revision,
			   dev->chip.hci_sub_version);
		return false;
	}

	CG2900_INFO("Chip supported by the STLC2690 driver");

	/* Store needed data */
	dev->user_data = stlc2690_info;
	memcpy(&(stlc2690_info->chip_dev), dev, sizeof(*dev));
	/* Set the callbacks */
	dev->cb.chip_startup = chip_startup;
	dev->cb.data_from_chip = data_from_chip;
	dev->cb.get_h4_channel = get_h4_channel;

	return true;
}

static struct cg2900_id_callbacks stlc2690_id_callbacks = {
	.check_chip_support = check_chip_support
};

/**
 * stlc2690_chip_init() - Initialize module.
 *
 * The stlc2690_chip_init() function initialize the CG2690 driver, then register to
 * the CG2900 Core.
 *
 * Returns:
 *   0 if success.
 *   -ENOMEM for failed alloc or structure creation.
 *   Error codes generated by cg2900_register_chip_driver.
 */
//static int __init stlc2690_chip_init(void)
int stlc2690_chip_init(void)
{
	int err = 0;

	CG2900_INFO("stlc2690_chip_init");

	stlc2690_info = kzalloc(sizeof(*stlc2690_info), GFP_ATOMIC);
	if (!stlc2690_info) {
		CG2900_ERR("Couldn't allocate stlc2690_info");
		err = -ENOMEM;
		goto finished;
	}

	stlc2690_info->wq = create_singlethread_workqueue(WQ_NAME);
	if (!stlc2690_info->wq) {
		CG2900_ERR("Could not create workqueue");
		err = -ENOMEM;
		goto err_handling_free_info;
	}

	/*
	 * Allocate file names that will be used, deallocated in stlc2690_exit.
	 */
	stlc2690_info->patch_file_name = kzalloc(FILENAME_MAX + 1, GFP_ATOMIC);
	if (!stlc2690_info->patch_file_name) {
		CG2900_ERR("Couldn't allocate name buffer for patch file.");
		err = -ENOMEM;
		goto err_handling_destroy_wq;
	}
	/*
	 * Allocate file names that will be used, deallocated in stlc2690_exit.
	 */
	stlc2690_info->settings_file_name = kzalloc(FILENAME_MAX + 1,
						    GFP_ATOMIC);
	if (!stlc2690_info->settings_file_name) {
		CG2900_ERR("Couldn't allocate name buffers settings file.");
		err = -ENOMEM;
		goto err_handling_free_patch_name;
	}

	err = cg2900_register_chip_driver(&stlc2690_id_callbacks);
	if (err) {
		CG2900_ERR("Couldn't register chip driver (%d)", err);
		goto err_handling_free_settings_name;
	}

	goto finished;

err_handling_free_settings_name:
	kfree(stlc2690_info->settings_file_name);
err_handling_free_patch_name:
	kfree(stlc2690_info->patch_file_name);
err_handling_destroy_wq:
	destroy_workqueue(stlc2690_info->wq);
err_handling_free_info:
	kfree(stlc2690_info);
	stlc2690_info = NULL;
finished:
	return err;
}

/**
 * stlc2690_chip_exit() - Remove module.
 */
//static void __exit stlc2690_chip_exit(void)
void stlc2690_chip_exit(void)
{
	CG2900_INFO("stlc2690_chip_exit");

	if (!stlc2690_info)
		return;

	kfree(stlc2690_info->settings_file_name);
	kfree(stlc2690_info->patch_file_name);
	destroy_workqueue(stlc2690_info->wq);
	kfree(stlc2690_info);
	stlc2690_info = NULL;
}

//module_init(stlc2690_chip_init);
//module_exit(stlc2690_chip_exit);

MODULE_AUTHOR("Par-Gunnar Hjalmdahl ST-Ericsson");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Linux STLC2690 Connectivity Device Driver");
