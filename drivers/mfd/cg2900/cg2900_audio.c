/*
 * drivers/mfd/cg2900/cg2900_audio.c
 *
 * Copyright (C) ST-Ericsson SA 2010
 * Authors:
 * Par-Gunnar Hjalmdahl (par-gunnar.p.hjalmdahl@stericsson.com) for ST-Ericsson.
 * Kjell Andersson (kjell.k.andersson@stericsson.com) for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 *
 * Linux Bluetooth Audio Driver for ST-Ericsson CG2900 controller.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/mfd/cg2900.h>
#include <linux/mfd/cg2900_audio.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/skbuff.h>
#include <linux/list.h>
#include <linux/sched.h>

#include "cg2900_debug.h"
#include "hci_defines.h"
#include "cg2900_chip.h"

/* Char device op codes */
#define OP_CODE_SET_DAI_CONF			0x01
#define OP_CODE_GET_DAI_CONF			0x02
#define OP_CODE_CONFIGURE_ENDPOINT		0x03
#define OP_CODE_START_STREAM			0x04
#define OP_CODE_STOP_STREAM			0x05

/* Device names */
#define DEVICE_NAME				"cg2900_audio"

/* Type of channel used */
#define BT_CHANNEL_USED				0x00
#define FM_CHANNEL_USED				0x01

#define MAX_NBR_OF_USERS			10
#define FIRST_USER				1

#define SET_RESP_STATE(__state_var, __new_state) \
	CG2900_SET_STATE("resp_state", __state_var, __new_state)

#define DEFAULT_SCO_HANDLE			0x0008

/* Use a timeout of 5 seconds when waiting for a command response */
#define RESP_TIMEOUT				5000

/**
 * enum chip_resp_state - State when communicating with the CG2900 controller.
 * @IDLE:		No outstanding packets to the controller.
 * @WAITING:		Packet has been sent to the controller. Waiting for
 *			response.
 * @RESP_RECEIVED:	Response from controller has been received but not yet
 *			handled.
 */
enum chip_resp_state {
	IDLE,
	WAITING,
	RESP_RECEIVED
};

/**
 * enum main_state - Main state for the CG2900 Audio driver.
 * @OPENED:	Audio driver has registered to CG2900 Core.
 * @CLOSED:	Audio driver is not registered to CG2900 Core.
 * @RESET:	A reset of CG2900 Core has occurred and no user has re-opened
 *		the audio driver.
 */
enum main_state {
	OPENED,
	CLOSED,
	RESET
};

/**
 * struct char_dev_info - CG2900 character device info structure.
 * @session:		Stored session for the char device.
 * @stored_data:	Data returned when executing last command, if any.
 * @stored_data_len:	Length of @stored_data in bytes.
 * @management_mutex:	Mutex for handling access to char dev management.
 * @rw_mutex:		Mutex for handling access to char dev writes and reads.
 */
struct char_dev_info {
	int		session;
	u8		*stored_data;
	int		stored_data_len;
	struct mutex	management_mutex;
	struct mutex	rw_mutex;
};

/**
 * struct audio_user - CG2900 audio user info structure.
 * @session:	Stored session for the char device.
 * @resp_state:	State for controller communications.
 */
struct audio_user {
	int			session;
	enum chip_resp_state	resp_state;
};

/**
 * struct endpoint_list - List for storing endpoint configuration nodes.
 * @ep_list:		Pointer to first node in list.
 * @management_mutex:	Mutex for handling access to list.
 */
struct endpoint_list {
	struct list_head	ep_list;
	struct mutex		management_mutex;
};

/**
 * struct endpoint_config_node - Node for storing endpoint configuration.
 * @list:		list_head struct.
 * @endpoint_id:	Endpoint ID.
 * @config:		Stored configuration for this endpoint.
 */
struct endpoint_config_node {
	struct list_head			list;
	enum cg2900_audio_endpoint_id		endpoint_id;
	union cg2900_endpoint_config_union	config;
};

/**
 * struct audio_info - Main CG2900 Audio driver info structure.
 * @state:			Current state of the CG2900 Audio driver.
 * @dev:			The misc device created by this driver.
 * @dev_bt:			CG2900 Core device registered by this driver for
 *				the BT audio channel.
 * @dev_fm:			CG2900 Core device registered by this driver for
 *				the FM audio channel.
 * @management_mutex:		Mutex for handling access to CG2900 Audio driver
 *				management.
 * @bt_mutex:			Mutex for handling access to BT audio channel.
 * @fm_mutex:			Mutex for handling access to FM audio channel.
 * @nbr_of_users_active:	Number of sessions open in the CG2900 Audio
 *				driver.
 * @bt_queue:			Received BT events.
 * @fm_queue:			Received FM events.
 * @audio_sessions:		Pointers to currently opened sessions (maps
 *				session ID to user info).
 * @i2s_config:			DAI I2S configuration.
 * @i2s_pcm_config:		DAI PCM_I2S configuration.
 * @i2s_config_known:		@true if @i2s_config has been set,
 *				@false otherwise.
 * @i2s_pcm_config_known:	@true if @i2s_pcm_config has been set,
 *				@false otherwise.
 * @endpoints:			List containing the endpoint configurations.
 */
struct audio_info {
	enum main_state			state;
	struct miscdevice		dev;
	struct cg2900_device		*dev_bt;
	struct cg2900_device		*dev_fm;
	struct mutex			management_mutex;
	struct mutex			bt_mutex;
	struct mutex			fm_mutex;
	int				nbr_of_users_active;
	struct sk_buff_head		bt_queue;
	struct sk_buff_head		fm_queue;
	struct audio_user		*audio_sessions[MAX_NBR_OF_USERS];
	struct cg2900_dai_conf_i2s	i2s_config;
	struct cg2900_dai_conf_i2s_pcm	i2s_pcm_config;
	bool				i2s_config_known;
	bool				i2s_pcm_config_known;
	struct endpoint_list		endpoints;
};

/**
 * struct audio_cb_info - Callback info structure registered in @user_data.
 * @channel:	Stores if this device handles BT or FM events.
 * @user:	Audio user currently awaiting data on the channel.
 */
struct audio_cb_info {
	int			channel;
	struct audio_user	*user;
};

/* cg2900_audio wait queues */
static DECLARE_WAIT_QUEUE_HEAD(wq_bt);
static DECLARE_WAIT_QUEUE_HEAD(wq_fm);

static struct audio_info *audio_info;

static struct audio_cb_info cb_info_bt = {
	.channel = BT_CHANNEL_USED,
	.user = NULL
};
static struct audio_cb_info cb_info_fm = {
	.channel = FM_CHANNEL_USED,
	.user = NULL
};

/*
 *	Internal helper functions
 */

/**
 * read_cb() - Handle data received from STE connectivity driver.
 * @dev:	Device receiving data.
 * @skb:	Buffer with data coming form device.
 */
static void read_cb(struct cg2900_device *dev, struct sk_buff *skb)
{
	struct audio_cb_info *cb_info;

	CG2900_INFO("CG2900 Audio: read_cb");

	if (!dev) {
		CG2900_ERR("NULL supplied as dev");
		return;
	}

	if (!skb) {
		CG2900_ERR("NULL supplied as skb");
		return;
	}

	cb_info = (struct audio_cb_info *)dev->user_data;
	if (!cb_info) {
		CG2900_ERR("NULL supplied as cb_info");
		return;
	}
	if (!(cb_info->user)) {
		CG2900_ERR("NULL supplied as cb_info->user");
		return;
	}

	/* Mark that packet has been received */
	SET_RESP_STATE(cb_info->user->resp_state, RESP_RECEIVED);

	/* Handle packet depending on channel */
	if (cb_info->channel == BT_CHANNEL_USED) {
		skb_queue_tail(&(audio_info->bt_queue), skb);
		wake_up_interruptible(&wq_bt);
	} else if (cb_info->channel == FM_CHANNEL_USED) {
		skb_queue_tail(&(audio_info->fm_queue), skb);
		wake_up_interruptible(&wq_fm);
	} else {
		/* Unhandled channel; free the packet */
		CG2900_ERR("Received callback on bad channel %d",
			   cb_info->channel);
		kfree_skb(skb);
	}
}

/**
 * reset_cb() - Reset callback function.
 * @dev:	CPD device reseting.
 */
static void reset_cb(struct cg2900_device *dev)
{
	CG2900_INFO("CG2900 Audio: reset_cb");
	mutex_lock(&audio_info->management_mutex);
	audio_info->nbr_of_users_active = 0;
	audio_info->state = RESET;
	mutex_unlock(&audio_info->management_mutex);
}

static struct cg2900_callbacks cg2900_cb = {
	.read_cb = read_cb,
	.reset_cb = reset_cb
};

/**
 * get_session_user() - Check that supplied session is within valid range.
 * @session:	Session ID.
 *
 * Returns:
 *   Audio_user if there is no error.
 *   NULL for bad session ID.
 */
static struct audio_user *get_session_user(int session)
{
	struct audio_user *audio_user;

	if (session < FIRST_USER || session >= MAX_NBR_OF_USERS) {
		CG2900_ERR("Calling with invalid session %d", session);
		return NULL;
	}

	audio_user = audio_info->audio_sessions[session];
	if (!audio_user)
		CG2900_ERR("Calling with non-opened session %d", session);
	return audio_user;
}

/**
 * del_endpoint_private() - Deletes an endpoint from @list.
 * @endpoint_id:	Endpoint ID.
 * @list:		List of endpoints.
 *
 * Deletes an endpoint from the supplied endpoint list.
 * This function is not protected by any semaphore.
 */
static void del_endpoint_private(enum cg2900_audio_endpoint_id endpoint_id,
				 struct endpoint_list *list)
{
	struct list_head *cursor, *next;
	struct endpoint_config_node *tmp;

	list_for_each_safe(cursor, next, &(list->ep_list)) {
		tmp = list_entry(cursor, struct endpoint_config_node, list);
		if (tmp->endpoint_id == endpoint_id) {
			list_del(cursor);
			kfree(tmp);
		}
	}
}

/**
 * add_endpoint() - Add endpoint node to @list.
 * @ep_config:	Endpoint configuration.
 * @list:	List of endpoints.
 *
 * Add endpoint node to the supplied list and copies supplied config to node.
 * If a node already exists for the supplied endpoint, the old node is removed
 * and replaced by the new node.
 */
static void add_endpoint(struct cg2900_endpoint_config *ep_config,
			 struct endpoint_list *list)
{
	struct endpoint_config_node *item;

	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (!item) {
		CG2900_ERR("Failed to alloc memory!");
		return;
	}

	/* Store values */
	item->endpoint_id = ep_config->endpoint_id;
	memcpy(&(item->config), &(ep_config->config), sizeof(item->config));

	mutex_lock(&(list->management_mutex));

	/*
	 * Check if endpoint ID already exist in list.
	 * If that is the case, remove it.
	 */
	if (!list_empty(&(list->ep_list)))
		del_endpoint_private(ep_config->endpoint_id, list);

	list_add_tail(&(item->list), &(list->ep_list));

	mutex_unlock(&(list->management_mutex));
}

/**
 * find_endpoint() - Finds endpoint identified by @endpoint_id in @list.
 * @endpoint_id:	Endpoint ID.
 * @list:		List of endpoints.
 *
 * Returns:
 *   Endpoint configuration if there is no error.
 *   NULL if no configuration can be found for @endpoint_id.
 */
static union cg2900_endpoint_config_union *
find_endpoint(enum cg2900_audio_endpoint_id endpoint_id,
	      struct endpoint_list *list)
{
	struct list_head *cursor, *next;
	struct endpoint_config_node *tmp;
	struct endpoint_config_node *ret_ep = NULL;

	mutex_lock(&list->management_mutex);
	list_for_each_safe(cursor, next, &(list->ep_list)) {
		tmp = list_entry(cursor, struct endpoint_config_node, list);
		if (tmp->endpoint_id == endpoint_id) {
			ret_ep = tmp;
			break;
		}
	}
	mutex_unlock(&list->management_mutex);

	if (ret_ep)
		return &(ret_ep->config);
	else
		return NULL;
}

/**
 * flush_endpoint_list() - Deletes all stored endpoints in @list.
 * @list:	List of endpoints.
 */
static void flush_endpoint_list(struct endpoint_list *list)
{
	struct list_head *cursor, *next;
	struct endpoint_config_node *tmp;

	mutex_lock(&list->management_mutex);
	list_for_each_safe(cursor, next, &(list->ep_list)) {
		tmp = list_entry(cursor, struct endpoint_config_node, list);
		list_del(cursor);
		kfree(tmp);
	}
	mutex_unlock(&list->management_mutex);
}

/**
 * receive_fm_legacy_response() - Wait for and handle an FM Legacy response.
 * @audio_user:	Audio user to check for.
 * @rsp_lsb:	LSB of FM command to wait for.
 * @rsp_msb:	MSB of FM command to wait for.
 *
 * This function first waits for FM response (up to 5 seconds) and when one
 * arrives, it checks that it is the one we are waiting for and also that no
 * error has occurred.
 *
 * Returns:
 *   0 if there is no error.
 *   -ECOMM if no response was received.
 *   -EIO for other errors.
 */
static int receive_fm_legacy_response(struct audio_user *audio_user,
				      u8 rsp_lsb, u8 rsp_msb)
{
	int err = 0;
	struct sk_buff *skb;
	u8 status;
	u8 fm_function;
	u8 fm_rsp_hdr_lsb;
	u8 fm_rsp_hdr_msb;

	/*
	 * Wait for callback to receive command complete and then wake us up
	 * again.
	 */
	if (0 >= wait_event_interruptible_timeout(wq_fm,
					audio_user->resp_state == RESP_RECEIVED,
					msecs_to_jiffies(RESP_TIMEOUT))) {
		/* We timed out or an error occurred */
		CG2900_ERR("Error occurred while waiting for return packet.");
		return -ECOMM;
	}

	/* OK, now we should have received answer. Let's check it. */
	skb = skb_dequeue_tail(&audio_info->fm_queue);
	if (!skb) {
		CG2900_ERR("No skb in queue when it should be there");
		return -EIO;
	}

	/* Check if we received the correct event */
	if (CG2900_FM_GEN_ID_LEGACY !=
			skb->data[CG2900_FM_USER_GEN_ID_POS_CMD_CMPL]) {
		CG2900_ERR("Received unknown FM packet. 0x%X %X %X %X %X",
			   skb->data[0], skb->data[1], skb->data[2],
			   skb->data[3], skb->data[4]);
		err = -EIO;
		goto error_handling_free_skb;
	}

	/* FM Legacy Command complete event */
	status = skb->data[CG2900_FM_USER_LEG_STATUS_POS_CMD_CMPL];
	fm_function = skb->data[CG2900_FM_USER_LEG_FUNC_POS_CMD_CMPL];
	fm_rsp_hdr_lsb = skb->data[CG2900_FM_USER_LEG_HDR_POS_CMD_CMPL];
	fm_rsp_hdr_msb = skb->data[CG2900_FM_USER_LEG_HDR_POS_CMD_CMPL + 1];

	if (fm_function != CG2900_FM_CMD_PARAM_WRITECOMMAND ||
	    fm_rsp_hdr_lsb != rsp_lsb || fm_rsp_hdr_msb != rsp_msb) {
		CG2900_ERR("Received unexpected packet func 0x%X "
			   "cmd 0x%02X%02X",
			   fm_function, fm_rsp_hdr_msb, fm_rsp_hdr_lsb);
		err = -EIO;
		goto error_handling_free_skb;
	}

	if (status != CG2900_FM_CMD_STATUS_COMMAND_SUCCEEDED) {
		CG2900_ERR("FM Command failed (%d)", status);
		err = -EIO;
		goto error_handling_free_skb;
	}
	/* Operation succeeded. We are now done */

error_handling_free_skb:
	kfree_skb(skb);
	return err;
}

/**
 * receive_bt_cmd_complete() - Wait for and handle an BT Command Complete event.
 * @audio_user:	Audio user to check for.
 * @rsp_lsb:	LSB of BT command to wait for.
 * @rsp_msb:	MSB of BT command to wait for.
 * @data:	Pointer to buffer if any received data should be stored (except
 *		status).
 * @data_len:	Length of @data in bytes.
 *
 * This function first waits for BT Command Complete event (up to 5 seconds)
 * and when one arrives, it checks that it is the one we are waiting for and
 * also that no error has occurred.
 * If @data is supplied it also copies received data into @data.
 *
 * Returns:
 *   0 if there is no error.
 *   -ECOMM if no response was received.
 *   -EIO for other errors.
 */
static int receive_bt_cmd_complete(struct audio_user *audio_user, u8 rsp_lsb,
				   u8 rsp_msb, void *data, int data_len)
{
	int err = 0;
	struct sk_buff *skb;
	u8 evt_id;
	u8 op_lsb;
	u8 op_msb;
	u8 status;

	/*
	 * Wait for callback to receive command complete and then wake us up
	 * again.
	 */
	if (0 >= wait_event_interruptible_timeout(wq_bt,
					audio_user->resp_state == RESP_RECEIVED,
					msecs_to_jiffies(RESP_TIMEOUT))) {
		/* We timed out or an error occurred */
		CG2900_ERR("Error occurred while waiting for return packet.");
		return -ECOMM;
	}

	/* OK, now we should have received answer. Let's check it. */
	skb = skb_dequeue_tail(&audio_info->bt_queue);
	if (!skb) {
		CG2900_ERR("No skb in queue when it should be there");
		return -EIO;
	}

	evt_id = skb->data[HCI_BT_EVT_ID_POS];
	if (evt_id != HCI_BT_EVT_CMD_COMPLETE) {
		CG2900_ERR("We did not receive the event we expected (0x%X)",
			   evt_id);
		err = -EIO;
		goto error_handling_free_skb;
	}

	op_lsb = skb->data[HCI_BT_EVT_CMD_COMPL_ID_POS];
	op_msb = skb->data[HCI_BT_EVT_CMD_COMPL_ID_POS + 1];
	status = skb->data[HCI_BT_EVT_CMD_COMPL_STATUS_POS];

	if (rsp_lsb != op_lsb || rsp_msb != op_msb) {
		CG2900_ERR("Received cmd complete for unexpected command: "
			   "0x%02X%02X", op_msb, op_lsb);
		err = -EIO;
		goto error_handling_free_skb;
	}

	if (status != HCI_BT_ERROR_NO_ERROR) {
		CG2900_ERR("Received command complete with err %d", status);
		err = -EIO;
		goto error_handling_free_skb;
	}

	/* Copy the rest of the parameters if a buffer has been supplied.
	 * The caller must have set the length correctly.
	 */
	if (data)
		memcpy(data, &(skb->data[HCI_BT_EVT_CMD_COMPL_STATUS_POS + 1]),
		       data_len);

	/* Operation succeeded. We are now done */

error_handling_free_skb:
	kfree_skb(skb);
	return err;
}

/**
 * conn_start_i2s_to_fm_rx() - Start an audio stream connecting FM RX to I2S.
 * @audio_user:		Audio user to check for.
 * @stream_handle:	[out] Pointer where to store the stream handle.
 *
 * This function sets up an FM RX to I2S stream.
 * It does this by first setting the output mode and then the configuration of
 * the External Sample Rate Converter.
 *
 * Returns:
 *   0 if there is no error.
 *   -ECOMM if no response was received.
 *   -ENOMEM upon allocation errors.
 *   Errors from @cg2900_write and @receive_fm_legacy_response.
 *   -EIO for other errors.
 */
static int conn_start_i2s_to_fm_rx(struct audio_user *audio_user,
				   unsigned int *stream_handle)
{
	int err = 0;
	u8 temp_session_id;
	union cg2900_endpoint_config_union *fm_config;
	struct sk_buff *skb;
	u8 *data;

	fm_config = find_endpoint(ENDPOINT_FM_RX,
				  &(audio_info->endpoints));
	if (!fm_config) {
		CG2900_ERR("FM RX not configured before stream start");
		return -EIO;
	}

	if (!(audio_info->i2s_config_known)) {
		CG2900_ERR("I2S DAI not configured before stream start");
		return -EIO;
	}

	/*
	 * Use mutex to assure that only ONE command is sent at any time on each
	 * channel.
	 */
	mutex_lock(&audio_info->fm_mutex);
	mutex_lock(&audio_info->bt_mutex);

	/*
	 * Now set the output mode of the External Sample Rate Converter by
	 * sending HCI_Write command with AUP_EXT_SetMode.
	 */
	CG2900_DBG("FM: AUP_EXT_SetMode");

	skb = cg2900_alloc_skb(CG2900_FM_CMD_LEN_AUP_EXT_SET_MODE, GFP_KERNEL);
	if (!skb) {
		CG2900_ERR("Could not allocate skb");
		err = -ENOMEM;
		goto finished_unlock_mutex;
	}

	/* Enter data into the skb */
	data = skb_put(skb, CG2900_FM_CMD_LEN_AUP_EXT_SET_MODE);

	data[0] = CG2900_FM_CMD_PARAM_LEN_AUP_EXT_SET_MODE;
	data[1] = CG2900_FM_GEN_ID_LEGACY;
	data[2] = CG2900_FM_CMD_LEG_PARAM_WRITE;
	data[3] = CG2900_FM_CMD_PARAM_WRITECOMMAND;
	data[4] = CG2900_FM_CMD_ID_AUP_EXT_SET_MODE_LSB;
	data[5] = CG2900_FM_CMD_ID_AUP_EXT_SET_MODE_MSB;
	data[6] = HCI_SET_U16_DATA_LSB(CG2900_FM_CMD_AUP_EXT_SET_MODE_PARALLEL);
	data[7] = HCI_SET_U16_DATA_MSB(CG2900_FM_CMD_AUP_EXT_SET_MODE_PARALLEL);

	cb_info_fm.user = audio_user;
	SET_RESP_STATE(audio_user->resp_state, WAITING);

	/* Send packet to controller */
	err = cg2900_write(audio_info->dev_fm, skb);
	if (err) {
		CG2900_ERR("Error occurred while transmitting skb (%d)", err);
		goto error_handling_free_skb;
	}

	err = receive_fm_legacy_response(audio_user,
					 CG2900_FM_RSP_ID_AUP_EXT_SET_MODE_LSB,
					 CG2900_FM_RSP_ID_AUP_EXT_SET_MODE_MSB);
	if (err)
		goto finished_unlock_mutex;

	SET_RESP_STATE(audio_user->resp_state, IDLE);

	/*
	 * Now configure the External Sample Rate Converter by sending
	 * HCI_Write command with AUP_EXT_SetControl.
	 */
	CG2900_DBG("FM: AUP_EXT_SetControl");

	skb = cg2900_alloc_skb(CG2900_FM_CMD_LEN_AUP_EXT_SET_CTRL, GFP_KERNEL);
	if (!skb) {
		CG2900_ERR("Could not allocate skb");
		err = -ENOMEM;
		goto finished_unlock_mutex;
	}

	/* Enter data into the skb */
	data = skb_put(skb, CG2900_FM_CMD_LEN_AUP_EXT_SET_CTRL);

	data[0] = CG2900_FM_CMD_PARAM_LEN_AUP_EXT_SET_CTRL;
	data[1] = CG2900_FM_GEN_ID_LEGACY;
	data[2] = CG2900_FM_CMD_LEG_PARAM_WRITE;
	data[3] = CG2900_FM_CMD_PARAM_WRITECOMMAND;
	data[4] = CG2900_FM_CMD_ID_AUP_EXT_SET_CTRL_LSB;
	data[5] = CG2900_FM_CMD_ID_AUP_EXT_SET_CTRL_MSB;
	if (fm_config->fm.sample_rate >= ENDPOINT_SAMPLE_RATE_44_1_KHZ) {
		data[6] = HCI_SET_U16_DATA_LSB(CG2900_FM_CMD_SET_CTRL_CONV_UP);
		data[7] = HCI_SET_U16_DATA_MSB(CG2900_FM_CMD_SET_CTRL_CONV_UP);
	} else {
		data[6] = HCI_SET_U16_DATA_LSB(
				CG2900_FM_CMD_SET_CTRL_CONV_DOWN);
		data[7] = HCI_SET_U16_DATA_MSB(
				CG2900_FM_CMD_SET_CTRL_CONV_DOWN);
	}

	cb_info_fm.user = audio_user;
	SET_RESP_STATE(audio_user->resp_state, WAITING);

	/* Send packet to controller */
	err = cg2900_write(audio_info->dev_fm, skb);
	if (err) {
		CG2900_ERR("Error occurred while transmitting skb (%d)", err);
		goto error_handling_free_skb;
	}

	err = receive_fm_legacy_response(audio_user,
					 CG2900_FM_RSP_ID_AUP_EXT_SET_CTRL_LSB,
					 CG2900_FM_RSP_ID_AUP_EXT_SET_CTRL_MSB);
	if (err)
		goto finished_unlock_mutex;

	/*
	 * Now send HCI_VS_Set_Session_Configuration command
	 */
	CG2900_DBG("BT: HCI_VS_Set_Session_Configuration");

	skb = cg2900_alloc_skb(CG2900_BT_LEN_VS_SET_SESSION_CONFIG, GFP_KERNEL);
	if (!skb) {
		CG2900_ERR("Could not allocate skb");
		err = -ENOMEM;
		goto finished_unlock_mutex;
	}

	/* Enter data into the skb */
	data = skb_put(skb, CG2900_BT_LEN_VS_SET_SESSION_CONFIG);
	/*
	 * Default all bytes to 0 so we don't have to set reserved bytes below.
	 */
	memset(data, 0, CG2900_BT_LEN_VS_SET_SESSION_CONFIG);

	data[0] = CG2900_BT_VS_SET_SESSION_CONFIG_LSB;
	data[1] = CG2900_BT_VS_SET_SESSION_CONFIG_MSB;
	data[2] = CG2900_BT_PARAM_LEN_VS_SET_SESSION_CONFIG;
	data[3] = 1; /* Number of streams */
	data[4] = CG2900_BT_SESSION_MEDIA_TYPE_AUDIO;
	/* Media configuration: Set sample rate and audio channel setup */
	data[5] = CG2900_BT_SESSION_CONF_SET_SAMPLE_RATE(
			fm_config->fm.sample_rate);
	if (audio_info->i2s_config.channel_sel == CHANNEL_SELECTION_BOTH)
		data[5] |= CG2900_BT_MEDIA_CONFIG_STEREO;
	else
		data[5] |= CG2900_BT_MEDIA_CONFIG_MONO;
	/* data[6] - data[10] SBC codec params (not used for FM RX) */
	/* Input Virtual Port (VP) configuration */
	data[11] = CG2900_BT_VP_TYPE_FM;
	/* data[12] - data[23] reserved */
	/* Output Virtual Port (VP) configuration */
	data[24] = CG2900_BT_VP_TYPE_I2S;
	data[25] = CG2900_BT_SESSION_I2S_INDEX_I2S;
	data[26] = audio_info->i2s_config.channel_sel;
	/* data[27] - data[36] reserved */

	cb_info_bt.user = audio_user;
	SET_RESP_STATE(audio_user->resp_state, WAITING);

	/* Send packet to controller */
	err = cg2900_write(audio_info->dev_bt, skb);
	if (err) {
		CG2900_ERR("Error occurred while transmitting skb (%d)", err);
		goto error_handling_free_skb;
	}

	err = receive_bt_cmd_complete(audio_user,
			CG2900_BT_VS_SET_SESSION_CONFIG_LSB,
			CG2900_BT_VS_SET_SESSION_CONFIG_MSB,
			&temp_session_id, CG2900_BT_PARAM_LEN_SESSION_ID);
	if (err)
		goto finished_unlock_mutex;

	/* Store the stream handle (used for start and stop stream) */
	*stream_handle = temp_session_id;
	CG2900_DBG("stream_handle set to %d", *stream_handle);

	SET_RESP_STATE(audio_user->resp_state, IDLE);

	/*
	 * Now start the stream by sending HCI_VS_Session_Control command
	 */
	CG2900_DBG("BT: HCI_VS_Session_Control");

	skb = cg2900_alloc_skb(CG2900_BT_LEN_VS_SESSION_CTRL, GFP_KERNEL);
	if (!skb) {
		CG2900_ERR("Could not allocate skb");
		err = -ENOMEM;
		goto finished_unlock_mutex;
	}

	/* Enter data into the skb */
	data = skb_put(skb, CG2900_BT_LEN_VS_SESSION_CTRL);

	data[0] = CG2900_BT_VS_SESSION_CTRL_LSB;
	data[1] = CG2900_BT_VS_SESSION_CTRL_MSB;
	data[2] = CG2900_BT_PARAM_LEN_VS_SESSION_CTRL;
	data[3] = (u8)(*stream_handle); /* Session ID */
	data[4] = CG2900_BT_SESSION_START;

	cb_info_bt.user = audio_user;
	SET_RESP_STATE(audio_user->resp_state, WAITING);

	/* Send packet to controller */
	err = cg2900_write(audio_info->dev_bt, skb);
	if (err) {
		CG2900_ERR("Error occurred while transmitting skb (%d)", err);
		goto error_handling_free_skb;
	}

	err = receive_bt_cmd_complete(audio_user,
				      CG2900_BT_VS_SESSION_CTRL_LSB,
				      CG2900_BT_VS_SESSION_CTRL_MSB,
				      NULL, 0);

	goto finished_unlock_mutex;

error_handling_free_skb:
	kfree_skb(skb);
finished_unlock_mutex:
	SET_RESP_STATE(audio_user->resp_state, IDLE);
	mutex_unlock(&audio_info->bt_mutex);
	mutex_unlock(&audio_info->fm_mutex);
	return err;
}

/**
 * conn_start_i2s_to_fm_tx() - Start an audio stream connecting FM TX to I2S.
 * @audio_user:		Audio user to check for.
 * @stream_handle:	[out] Pointer where to store the stream handle.
 *
 * This function sets up an I2S to FM TX stream.
 * It does this by first setting the Audio Input source and then setting the
 * configuration and input source of BT sample rate converter.
 *
 * Returns:
 *   0 if there is no error.
 *   -ECOMM if no response was received.
 *   -ENOMEM upon allocation errors.
 *   Errors from @cg2900_write and @receive_fm_legacy_response.
 *   -EIO for other errors.
 */
static int conn_start_i2s_to_fm_tx(struct audio_user *audio_user,
				   unsigned int *stream_handle)
{
	int err = 0;
	u8 temp_session_id;
	union cg2900_endpoint_config_union *fm_config;
	struct sk_buff *skb;
	u8 *data;

	fm_config = find_endpoint(ENDPOINT_FM_TX, &(audio_info->endpoints));
	if (!fm_config) {
		CG2900_ERR("FM TX not configured before stream start");
		return -EIO;
	}

	if (!(audio_info->i2s_config_known)) {
		CG2900_ERR("I2S DAI not configured before stream start");
		return -EIO;
	}

	/*
	 * Use mutex to assure that only ONE command is sent at any time on each
	 * channel.
	 */
	mutex_lock(&audio_info->fm_mutex);
	mutex_lock(&audio_info->bt_mutex);

	/*
	 * Select Audio Input Source by sending HCI_Write command with
	 * AIP_SetMode.
	 */
	CG2900_DBG("FM: AIP_SetMode");

	skb = cg2900_alloc_skb(CG2900_FM_CMD_LEN_AIP_SET_MODE, GFP_KERNEL);
	if (!skb) {
		CG2900_ERR("Could not allocate skb");
		err = -ENOMEM;
		goto finished_unlock_mutex;
	}

	/* Enter data into the skb */
	data = skb_put(skb, CG2900_FM_CMD_LEN_AIP_SET_MODE);

	data[0] = CG2900_FM_CMD_PARAM_LEN_AIP_SET_MODE;
	data[1] = CG2900_FM_GEN_ID_LEGACY;
	data[2] = CG2900_FM_CMD_LEG_PARAM_WRITE;
	data[3] = CG2900_FM_CMD_PARAM_WRITECOMMAND;
	data[4] = CG2900_FM_CMD_ID_AIP_SET_MODE_LSB;
	data[5] = CG2900_FM_CMD_ID_AIP_SET_MODE_MSB;
	data[6] = HCI_SET_U16_DATA_LSB(CG2900_FM_CMD_AIP_SET_MODE_INPUT_DIG);
	data[7] = HCI_SET_U16_DATA_MSB(CG2900_FM_CMD_AIP_SET_MODE_INPUT_DIG);

	cb_info_fm.user = audio_user;
	SET_RESP_STATE(audio_user->resp_state, WAITING);

	/* Send packet to controller */
	err = cg2900_write(audio_info->dev_fm, skb);
	if (err) {
		CG2900_ERR("Error occurred while transmitting skb (%d)", err);
		goto error_handling_free_skb;
	}

	err = receive_fm_legacy_response(audio_user,
					 CG2900_FM_RSP_ID_AIP_SET_MODE_LSB,
					 CG2900_FM_RSP_ID_AIP_SET_MODE_MSB);
	if (err)
		goto finished_unlock_mutex;

	SET_RESP_STATE(audio_user->resp_state, IDLE);

	/*
	 * Now configure the BT sample rate converter by sending HCI_Write
	 * command with AIP_BT_SetControl.
	 */
	CG2900_DBG("FM: AIP_BT_SetControl");

	skb = cg2900_alloc_skb(CG2900_FM_CMD_LEN_AIP_BT_SET_CTRL, GFP_KERNEL);
	if (!skb) {
		CG2900_ERR("Could not allocate skb");
		err = -ENOMEM;
		goto finished_unlock_mutex;
	}

	/* Enter data into the skb */
	data = skb_put(skb, CG2900_FM_CMD_LEN_AIP_BT_SET_CTRL);

	data[0] = CG2900_FM_CMD_PARAM_LEN_AIP_BT_SET_CTRL;
	data[1] = CG2900_FM_GEN_ID_LEGACY;
	data[2] = CG2900_FM_CMD_LEG_PARAM_WRITE;
	data[3] = CG2900_FM_CMD_PARAM_WRITECOMMAND;
	data[4] = CG2900_FM_CMD_ID_AIP_BT_SET_CTRL_LSB;
	data[5] = CG2900_FM_CMD_ID_AIP_BT_SET_CTRL_MSB;
	if (fm_config->fm.sample_rate >= ENDPOINT_SAMPLE_RATE_44_1_KHZ) {
		data[6] = HCI_SET_U16_DATA_LSB(CG2900_FM_CMD_SET_CTRL_CONV_UP);
		data[7] = HCI_SET_U16_DATA_MSB(CG2900_FM_CMD_SET_CTRL_CONV_UP);
	} else {
		data[6] = HCI_SET_U16_DATA_LSB(
				CG2900_FM_CMD_SET_CTRL_CONV_DOWN);
		data[7] = HCI_SET_U16_DATA_MSB(
				CG2900_FM_CMD_SET_CTRL_CONV_DOWN);
	}

	cb_info_fm.user = audio_user;
	SET_RESP_STATE(audio_user->resp_state, WAITING);

	/* Send packet to controller */
	err = cg2900_write(audio_info->dev_fm, skb);
	if (err) {
		CG2900_ERR("Error occurred while transmitting skb (%d)", err);
		goto error_handling_free_skb;
	}

	err = receive_fm_legacy_response(audio_user,
					 CG2900_FM_RSP_ID_AIP_BT_SET_CTRL_LSB,
					 CG2900_FM_RSP_ID_AIP_BT_SET_CTRL_MSB);
	if (err)
		goto finished_unlock_mutex;

	SET_RESP_STATE(audio_user->resp_state, IDLE);


	/*
	 * Now set input of the BT sample rate converter by sending HCI_Write
	 * command with AIP_BT_SetMode.
	 */
	CG2900_DBG("FM: AIP_BT_SetMode");

	skb = cg2900_alloc_skb(CG2900_FM_CMD_LEN_AIP_BT_SET_MODE, GFP_KERNEL);
	if (!skb) {
		CG2900_ERR("Could not allocate skb");
		err = -ENOMEM;
		goto finished_unlock_mutex;
	}

	/* Enter data into the skb */
	data = skb_put(skb, CG2900_FM_CMD_LEN_AIP_BT_SET_MODE);

	data[0] = CG2900_FM_CMD_PARAM_LEN_AIP_BT_SET_MODE;
	data[1] = CG2900_FM_GEN_ID_LEGACY;
	data[2] = CG2900_FM_CMD_LEG_PARAM_WRITE;
	data[3] = CG2900_FM_CMD_PARAM_WRITECOMMAND;
	data[4] = CG2900_FM_CMD_ID_AIP_BT_SET_MODE_LSB;
	data[5] = CG2900_FM_CMD_ID_AIP_BT_SET_MODE_MSB;
	data[6] = HCI_SET_U16_DATA_LSB(CG2900_FM_CMD_AIP_BT_SET_MODE_INPUT_PAR);
	data[7] = HCI_SET_U16_DATA_MSB(CG2900_FM_CMD_AIP_BT_SET_MODE_INPUT_PAR);

	cb_info_fm.user = audio_user;
	SET_RESP_STATE(audio_user->resp_state, WAITING);

	/* Send packet to controller */
	err = cg2900_write(audio_info->dev_fm, skb);
	if (err) {
		CG2900_ERR("Error occurred while transmitting skb (%d)", err);
		goto error_handling_free_skb;
	}

	err = receive_fm_legacy_response(audio_user,
					 CG2900_FM_RSP_ID_AIP_BT_SET_MODE_LSB,
					 CG2900_FM_RSP_ID_AIP_BT_SET_MODE_MSB);
	if (err)
		goto finished_unlock_mutex;

	/*
	 * Now send HCI_VS_Set_Session_Configuration command
	 */
	CG2900_DBG("BT: HCI_VS_Set_Session_Configuration");

	skb = cg2900_alloc_skb(CG2900_BT_LEN_VS_SET_SESSION_CONFIG, GFP_KERNEL);
	if (!skb) {
		CG2900_ERR("Could not allocate skb");
		err = -ENOMEM;
		goto finished_unlock_mutex;
	}

	/* Enter data into the skb */
	data = skb_put(skb, CG2900_BT_LEN_VS_SET_SESSION_CONFIG);
	/*
	 * Default all bytes to 0 so we don't have to set reserved bytes below.
	 */
	memset(data, 0, CG2900_BT_LEN_VS_SET_SESSION_CONFIG);

	data[0] = CG2900_BT_VS_SET_SESSION_CONFIG_LSB;
	data[1] = CG2900_BT_VS_SET_SESSION_CONFIG_MSB;
	data[2] = CG2900_BT_PARAM_LEN_VS_SET_SESSION_CONFIG;
	data[3] = 1; /* Number of streams */
	data[4] = CG2900_BT_SESSION_MEDIA_TYPE_AUDIO;
	/* Media configuration: Set sample rate and audio channel setup */
	data[5] = CG2900_BT_SESSION_CONF_SET_SAMPLE_RATE(
			fm_config->fm.sample_rate);
	if (audio_info->i2s_config.channel_sel == CHANNEL_SELECTION_BOTH)
		data[5] |= CG2900_BT_MEDIA_CONFIG_STEREO;
	else
		data[5] |= CG2900_BT_MEDIA_CONFIG_MONO;
	/* data[6] - data[10] SBC codec params (not used for FM TX) */
	/* Input Virtual Port (VP) configuration */
	data[11] = CG2900_BT_VP_TYPE_I2S;
	data[12] = CG2900_BT_SESSION_I2S_INDEX_I2S;
	data[13] = audio_info->i2s_config.channel_sel;
	/* data[14] - data[23] reserved */
	/* Output Virtual Port (VP) configuration */
	data[24] = CG2900_BT_VP_TYPE_FM;
	/* data[25] - data[36] reserved */

	cb_info_bt.user = audio_user;
	SET_RESP_STATE(audio_user->resp_state, WAITING);

	/* Send packet to controller */
	err = cg2900_write(audio_info->dev_bt, skb);
	if (err) {
		CG2900_ERR("Error occurred while transmitting skb (%d)", err);
		goto error_handling_free_skb;
	}

	err = receive_bt_cmd_complete(audio_user,
				      CG2900_BT_VS_SET_SESSION_CONFIG_LSB,
				      CG2900_BT_VS_SET_SESSION_CONFIG_MSB,
				      &temp_session_id,
				      CG2900_BT_PARAM_LEN_SESSION_ID);
	if (err)
		goto finished_unlock_mutex;

	/* Store the stream handle (used for start and stop stream) */
	*stream_handle = temp_session_id;
	CG2900_DBG("stream_handle set to %d", *stream_handle);

	SET_RESP_STATE(audio_user->resp_state, IDLE);

	/*
	 * Now start the stream by sending HCI_VS_Session_Control command
	 */
	CG2900_DBG("BT: HCI_VS_Session_Control");

	skb = cg2900_alloc_skb(CG2900_BT_LEN_VS_SESSION_CTRL, GFP_KERNEL);
	if (!skb) {
		CG2900_ERR("Could not allocate skb");
		err = -ENOMEM;
		goto finished_unlock_mutex;
	}

	/* Enter data into the skb */
	data = skb_put(skb, CG2900_BT_LEN_VS_SESSION_CTRL);

	data[0] = CG2900_BT_VS_SESSION_CTRL_LSB;
	data[1] = CG2900_BT_VS_SESSION_CTRL_MSB;
	data[2] = CG2900_BT_PARAM_LEN_VS_SESSION_CTRL;
	data[3] = (u8)(*stream_handle); /* Session ID */
	data[4] = CG2900_BT_SESSION_START;

	cb_info_bt.user = audio_user;
	SET_RESP_STATE(audio_user->resp_state, WAITING);

	/* Send packet to controller */
	err = cg2900_write(audio_info->dev_bt, skb);
	if (err) {
		CG2900_ERR("Error occurred while transmitting skb (%d)", err);
		goto error_handling_free_skb;
	}

	err = receive_bt_cmd_complete(audio_user,
				      CG2900_BT_VS_SESSION_CTRL_LSB,
				      CG2900_BT_VS_SESSION_CTRL_MSB,
				      NULL, 0);

	goto finished_unlock_mutex;

error_handling_free_skb:
	kfree_skb(skb);
finished_unlock_mutex:
	SET_RESP_STATE(audio_user->resp_state, IDLE);
	mutex_unlock(&audio_info->bt_mutex);
	mutex_unlock(&audio_info->fm_mutex);
	return err;
}

/**
 * conn_start_pcm_to_sco() - Start an audio stream connecting Bluetooth (e)SCO to PCM_I2S.
 * @audio_user:		Audio user to check for.
 * @stream_handle:	[out] Pointer where to store the stream handle.
 *
 * This function sets up a BT to_from PCM_I2S stream.
 * It does this by first setting the Session configuration and then starting
 * the Audio Stream.
 *
 * Returns:
 *   0 if there is no error.
 *   -ECOMM if no response was received.
 *   -ENOMEM upon allocation errors.
 *   Errors from @cg2900_write and @receive_bt_cmd_complete.
 *   -EIO for other errors.
 */
static int conn_start_pcm_to_sco(struct audio_user *audio_user,
				 unsigned int *stream_handle)
{
	int err = 0;
	u8 temp_session_id;
	union cg2900_endpoint_config_union *bt_config;
	struct sk_buff *skb;
	u8 *data;

	bt_config = find_endpoint(ENDPOINT_BT_SCO_INOUT,
				  &(audio_info->endpoints));
	if (!bt_config) {
		CG2900_ERR("BT not configured before stream start");
		return -EIO;
	}

	if (!(audio_info->i2s_pcm_config_known)) {
		CG2900_ERR("I2S_PCM DAI not configured before stream start");
		return -EIO;
	}

	/*
	 * Use mutex to assure that only ONE command is sent at any time on each
	 * channel.
	 */
	mutex_lock(&audio_info->bt_mutex);

	/*
	 * First send HCI_VS_Set_Session_Configuration command
	 */
	CG2900_DBG("BT: HCI_VS_Set_Session_Configuration");

	skb = cg2900_alloc_skb(CG2900_BT_LEN_VS_SET_SESSION_CONFIG, GFP_KERNEL);
	if (!skb) {
		CG2900_ERR("Could not allocate skb");
		err = -ENOMEM;
		goto finished_unlock_mutex;
	}

	/* Enter data into the skb */
	data = skb_put(skb, CG2900_BT_LEN_VS_SET_SESSION_CONFIG);
	/*
	 * Default all bytes to 0 so we don't have to set reserved bytes below.
	 */
	memset(data, 0, CG2900_BT_LEN_VS_SET_SESSION_CONFIG);

	data[0] = CG2900_BT_VS_SET_SESSION_CONFIG_LSB;
	data[1] = CG2900_BT_VS_SET_SESSION_CONFIG_MSB;
	data[2] = CG2900_BT_PARAM_LEN_VS_SET_SESSION_CONFIG;
	data[3] = 1; /* Number of streams */
	data[4] = CG2900_BT_SESSION_MEDIA_TYPE_AUDIO;
	/* Media configuration: Set sample rate and audio channel setup */
	data[5] = CG2900_BT_SESSION_CONF_SET_SAMPLE_RATE(
			bt_config->sco.sample_rate);
	data[5] |= CG2900_BT_MEDIA_CONFIG_MONO;
	/* data[6] - data[10] SBC codec params (not used for SCO) */
	/* Input Virtual Port (VP) configuration */
	data[11] = CG2900_BT_VP_TYPE_BT_SCO;
	/*
	 * As SCO handle we just use a default value.
	 * The chip will automatically connect the right SCO handle
	 */
	data[12] = HCI_SET_U16_DATA_LSB(DEFAULT_SCO_HANDLE);
	data[13] = HCI_SET_U16_DATA_MSB(DEFAULT_SCO_HANDLE);
	/* data[14] - data[23] reserved */
	/* Output Virtual Port (VP) configuration */
	data[24] = CG2900_BT_VP_TYPE_PCM;
	data[25] = CG2900_BT_SESSION_PCM_INDEX_PCM_I2S;
	if (audio_info->i2s_pcm_config.slot_0_used)
		data[26] |= CG2900_BT_SESSION_CONF_SET_PCM_SLOT_USE(0);
	if (audio_info->i2s_pcm_config.slot_1_used)
		data[26] |= CG2900_BT_SESSION_CONF_SET_PCM_SLOT_USE(1);
	if (audio_info->i2s_pcm_config.slot_2_used)
		data[26] |= CG2900_BT_SESSION_CONF_SET_PCM_SLOT_USE(2);
	if (audio_info->i2s_pcm_config.slot_3_used)
		data[26] |= CG2900_BT_SESSION_CONF_SET_PCM_SLOT_USE(3);
	data[27] = audio_info->i2s_pcm_config.slot_0_start;
	data[28] = audio_info->i2s_pcm_config.slot_1_start;
	data[29] = audio_info->i2s_pcm_config.slot_2_start;
	data[30] = audio_info->i2s_pcm_config.slot_3_start;
	/* data[31] - data[36] reserved */

	cb_info_bt.user = audio_user;
	SET_RESP_STATE(audio_user->resp_state, WAITING);

	/* Send packet to controller */
	err = cg2900_write(audio_info->dev_bt, skb);
	if (err) {
		CG2900_ERR("Error occurred while transmitting skb (%d)", err);
		goto error_handling_free_skb;
	}

	err = receive_bt_cmd_complete(audio_user,
				      CG2900_BT_VS_SET_SESSION_CONFIG_LSB,
				      CG2900_BT_VS_SET_SESSION_CONFIG_MSB,
				      &temp_session_id,
				      CG2900_BT_PARAM_LEN_SESSION_ID);
	if (err)
		goto finished_unlock_mutex;

	*stream_handle = temp_session_id;
	CG2900_DBG("stream_handle set to %d", *stream_handle);

	SET_RESP_STATE(audio_user->resp_state, IDLE);

	/*
	 * Now start the stream by sending HCI_VS_Session_Control command
	 */
	CG2900_DBG("BT: HCI_VS_Session_Control");

	skb = cg2900_alloc_skb(CG2900_BT_LEN_VS_SESSION_CTRL, GFP_KERNEL);
	if (!skb) {
		CG2900_ERR("Could not allocate skb");
		err = -ENOMEM;
		goto finished_unlock_mutex;
	}

	/* Enter data into the skb */
	data = skb_put(skb, CG2900_BT_LEN_VS_SESSION_CTRL);

	data[0] = CG2900_BT_VS_SESSION_CTRL_LSB;
	data[1] = CG2900_BT_VS_SESSION_CTRL_MSB;
	data[2] = CG2900_BT_PARAM_LEN_VS_SESSION_CTRL;
	data[3] = (u8)(*stream_handle); /* Session ID */
	data[4] = CG2900_BT_SESSION_START;

	cb_info_bt.user = audio_user;
	SET_RESP_STATE(audio_user->resp_state, WAITING);

	/* Send packet to controller */
	err = cg2900_write(audio_info->dev_bt, skb);
	if (err) {
		CG2900_ERR("Error occurred while transmitting skb (%d)", err);
		goto error_handling_free_skb;
	}

	err = receive_bt_cmd_complete(audio_user,
				      CG2900_BT_VS_SESSION_CTRL_LSB,
				      CG2900_BT_VS_SESSION_CTRL_MSB,
				      NULL, 0);

	goto finished_unlock_mutex;

error_handling_free_skb:
	kfree_skb(skb);
finished_unlock_mutex:
	SET_RESP_STATE(audio_user->resp_state, IDLE);
	mutex_unlock(&audio_info->bt_mutex);
	return err;
}


/**
 * conn_stop_stream() - Stops an audio stream defined by @stream_handle.
 * @audio_user:		Audio user to check for.
 * @stream_handle:	Handle of the audio stream.
 *
 * This function stops an audio stream defined by a stream handle.
 * It does this by first stopping the Audio Stream and then resetting the
 * Session configuration.
 *
 * Returns:
 *   0 if there is no error.
 *   -ECOMM if no response was received.
 *   -ENOMEM upon allocation errors.
 *   Errors from @cg2900_write and @receive_bt_cmd_complete.
 *   -EIO for other errors.
 */
static int conn_stop_stream(struct audio_user *audio_user,
			    unsigned int stream_handle)
{
	int err = 0;
	union cg2900_endpoint_config_union *bt_config;
	struct sk_buff *skb;
	u8 *data;

	bt_config = find_endpoint(ENDPOINT_BT_SCO_INOUT,
				  &(audio_info->endpoints));
	if (!bt_config) {
		CG2900_ERR("BT not configured before stream start");
		return -EIO;
	}

	/*
	 * Use mutex to assure that only ONE command is sent at any time on each
	 * channel.
	 */
	mutex_lock(&audio_info->bt_mutex);

	/*
	 * Now stop the stream by sending HCI_VS_Session_Control command
	 */
	CG2900_DBG("BT: HCI_VS_Session_Control");

	skb = cg2900_alloc_skb(CG2900_BT_LEN_VS_SESSION_CTRL, GFP_KERNEL);
	if (!skb) {
		CG2900_ERR("Could not allocate skb");
		err = -ENOMEM;
		goto finished_unlock_mutex;
	}

	/* Enter data into the skb */
	data = skb_put(skb, CG2900_BT_LEN_VS_SESSION_CTRL);

	data[0] = CG2900_BT_VS_SESSION_CTRL_LSB;
	data[1] = CG2900_BT_VS_SESSION_CTRL_MSB;
	data[2] = CG2900_BT_PARAM_LEN_VS_SESSION_CTRL;
	data[3] = (u8)stream_handle; /* Session ID */
	data[4] = CG2900_BT_SESSION_STOP;

	cb_info_bt.user = audio_user;
	SET_RESP_STATE(audio_user->resp_state, WAITING);

	/* Send packet to controller */
	err = cg2900_write(audio_info->dev_bt, skb);
	if (err) {
		CG2900_ERR("Error occurred while transmitting skb (%d)", err);
		goto error_handling_free_skb;
	}

	err = receive_bt_cmd_complete(audio_user,
				      CG2900_BT_VS_SESSION_CTRL_LSB,
				      CG2900_BT_VS_SESSION_CTRL_MSB,
				      NULL, 0);
	if (err)
		goto finished_unlock_mutex;

	SET_RESP_STATE(audio_user->resp_state, IDLE);

	/*
	 * Now delete the stream by sending HCI_VS_Reset_Session_Configuration
	 * command
	 */
	CG2900_DBG("BT: HCI_VS_Reset_Session_Configuration");

	skb = cg2900_alloc_skb(CG2900_BT_LEN_VS_RESET_SESSION_CONFIG,
				 GFP_KERNEL);
	if (!skb) {
		CG2900_ERR("Could not allocate skb");
		err = -ENOMEM;
		goto finished_unlock_mutex;
	}

	/* Enter data into the skb */
	data = skb_put(skb, CG2900_BT_LEN_VS_RESET_SESSION_CONFIG);

	data[0] = CG2900_BT_VS_RESET_SESSION_CONFIG_LSB;
	data[1] = CG2900_BT_VS_RESET_SESSION_CONFIG_MSB;
	data[2] = CG2900_BT_PARAM_LEN_VS_SET_SESSION_CONFIG;
	data[3] = (u8)stream_handle; /* Session ID */

	cb_info_bt.user = audio_user;
	SET_RESP_STATE(audio_user->resp_state, WAITING);

	/* Send packet to controller */
	err = cg2900_write(audio_info->dev_bt, skb);
	if (err) {
		CG2900_ERR("Error occurred while transmitting skb (%d)", err);
		goto error_handling_free_skb;
	}

	err = receive_bt_cmd_complete(audio_user,
				CG2900_BT_VS_RESET_SESSION_CONFIG_LSB,
				CG2900_BT_VS_RESET_SESSION_CONFIG_MSB,
				NULL, 0);

	goto finished_unlock_mutex;

error_handling_free_skb:
	kfree_skb(skb);
finished_unlock_mutex:
	SET_RESP_STATE(audio_user->resp_state, IDLE);
	mutex_unlock(&audio_info->bt_mutex);
	return err;
}

/*
 *	External methods
 */
int cg2900_audio_open(unsigned int *session)
{
	int err = 0;
	int i;

	CG2900_INFO("cg2900_audio_open");

	if (!session) {
		CG2900_ERR("NULL supplied as session.");
		return -EINVAL;
	}

	mutex_lock(&audio_info->management_mutex);

	*session = 0;

	/*
	 * First find a free session to use and allocate the session structure.
	 */
	for (i = FIRST_USER;
	     i < MAX_NBR_OF_USERS && audio_info->audio_sessions[i];
	     i++)
		; /* Just loop until found or end reached */

	if (i >= MAX_NBR_OF_USERS) {
		CG2900_ERR("Couldn't find free user");
		err = -EMFILE;
		goto finished;
	}

	audio_info->audio_sessions[i] =
			kzalloc(sizeof(*(audio_info->audio_sessions[0])),
				GFP_KERNEL);
	if (!audio_info->audio_sessions[i]) {
		CG2900_ERR("Could not allocate user");
		err = -ENOMEM;
		goto finished;
	}
	CG2900_DBG("Found free session %d", i);
	*session = i;
	audio_info->nbr_of_users_active++;

	SET_RESP_STATE(audio_info->audio_sessions[*session]->resp_state, IDLE);
	audio_info->audio_sessions[*session]->session = *session;

	if (audio_info->nbr_of_users_active == 1) {
		/*
		 * First user so register to CG2900 Core.
		 * First the BT audio device.
		 */
		audio_info->dev_bt = cg2900_register_user(CG2900_BT_AUDIO,
							  &cg2900_cb);
		if (!audio_info->dev_bt) {
			CG2900_ERR("Failed to register BT audio channel");
			err = -EIO;
			goto error_handling;
		}

		/* Store the callback info structure */
		audio_info->dev_bt->user_data = &cb_info_bt;

		/* Then the FM audio device */
		audio_info->dev_fm = cg2900_register_user(CG2900_FM_RADIO_AUDIO,
							  &cg2900_cb);
		if (!audio_info->dev_fm) {
			CG2900_ERR("Failed to register FM audio channel");
			err = -EIO;
			goto error_handling;
		}

		/* Store the callback info structure */
		audio_info->dev_fm->user_data = &cb_info_fm;

		audio_info->state = OPENED;
	}

	goto finished;

error_handling:
	if (audio_info->dev_bt) {
		cg2900_deregister_user(audio_info->dev_bt);
		audio_info->dev_bt = NULL;
	}
	audio_info->nbr_of_users_active--;
	kfree(audio_info->audio_sessions[*session]);
	audio_info->audio_sessions[*session] = NULL;
finished:
	mutex_unlock(&audio_info->management_mutex);
	return err;
}
EXPORT_SYMBOL(cg2900_audio_open);

int cg2900_audio_close(unsigned int *session)
{
	int err = 0;
	struct audio_user *audio_user;

	CG2900_INFO("cg2900_audio_close");

	if (audio_info->state != OPENED) {
		CG2900_ERR("Audio driver not open");
		return -EIO;
	}

	if (!session) {
		CG2900_ERR("NULL pointer supplied");
		return -EINVAL;
	}

	audio_user = get_session_user(*session);
	if (!audio_user) {
		CG2900_ERR("Invalid session ID");
		return -EINVAL;
	}

	mutex_lock(&audio_info->management_mutex);

	if (!(audio_info->audio_sessions[*session])) {
		CG2900_ERR("Session %d not opened", *session);
		err = -EACCES;
		goto err_unlock_mutex;
	}

	kfree(audio_info->audio_sessions[*session]);
	audio_info->audio_sessions[*session] = NULL;
	audio_info->nbr_of_users_active--;

	if (audio_info->nbr_of_users_active == 0) {
		/* No more sessions open. Deregister from CG2900 Core */
		cg2900_deregister_user(audio_info->dev_fm);
		cg2900_deregister_user(audio_info->dev_bt);
		audio_info->state = CLOSED;
	}

	*session = 0;

err_unlock_mutex:
	mutex_unlock(&audio_info->management_mutex);
	return err;
}
EXPORT_SYMBOL(cg2900_audio_close);

int cg2900_audio_set_dai_config(unsigned int session,
				struct cg2900_dai_config *config)
{
	int err = 0;
	struct audio_user *audio_user;
	struct cg2900_dai_conf_i2s *i2s = NULL;
	struct cg2900_dai_conf_i2s_pcm *i2s_pcm = NULL;
	struct sk_buff *skb = NULL;
	u8 *data = NULL;

	CG2900_INFO("cg2900_audio_set_dai_config");

	if (audio_info->state != OPENED) {
		CG2900_ERR("Audio driver not open");
		return -EIO;
	}

	audio_user = get_session_user(session);
	if (!audio_user)
		return -EINVAL;

	/*
	 * Use mutex to assure that only ONE command is sent at any time on each
	 * channel.
	 */
	mutex_lock(&audio_info->bt_mutex);

	/*
	 * Send following commands for the supported chips.
	 * For CG2900 PG1 = HCI_Cmd_VS_Set_Hardware_Configuration
	 */

	/* Allocate the sk_buffer. The length is actually a max length since
	 * length varies depending on logical transport.
	 */
	skb = cg2900_alloc_skb(CG2900_BT_LEN_VS_SET_HARDWARE_CONFIG,
			       GFP_KERNEL);
	if (!skb) {
		CG2900_ERR("Could not allocate skb");
		err = -ENOMEM;
		goto finished_unlock_mutex;
	}

	/* Format HCI command
	 *
	 * [vp][ltype][direction + mode][Bitclk][PCM duration]
	 * Start with the 2 byte op code.
	 */
	data = skb_put(skb, 2);
	data[0] = CG2900_BT_VS_SET_HARDWARE_CONFIG_LSB;
	data[1] = CG2900_BT_VS_SET_HARDWARE_CONFIG_MSB;

	/* Now create each command depending on received configuration */
	switch (config->port) {
	case PORT_0_I2S:
		i2s = (struct cg2900_dai_conf_i2s *) &config->conf;

		/* We will now have 5 bytes of data (including length field) */
		data = skb_put(skb, 5);

		/* Parameters begin -----*/
		data[0] = 0x04; /* Parameter length */
		data[1] = PORT_PROTOCOL_I2S;
		/* 0 = Virtual port if multiple on vp, PCM /I2S index */
		data[2] = 0x00;
		data[3] = i2s->half_period; /* WS Half period size */
		if (i2s->mode == DAI_MODE_MASTER)
			data[4] = CG2900_BT_HW_CONFIG_I2S_WS_SEL_MASTER;
		else
			data[4] = CG2900_BT_HW_CONFIG_I2S_WS_SEL_SLAVE;

		/* Store the new configuration */
		mutex_lock(&audio_info->management_mutex);
		memcpy(&(audio_info->i2s_config), &(config->conf.i2s),
		       sizeof(config->conf.i2s));
		audio_info->i2s_config_known = true;
		mutex_unlock(&audio_info->management_mutex);
		break;

	case PORT_1_I2S_PCM:
		i2s_pcm = (struct cg2900_dai_conf_i2s_pcm *) &config->conf;

		/* We will now have 7 bytes of data (including length field) */
		data = skb_put(skb, 7);

		/* Parameters begin -----*/
		data[0] = 0x06; /* Parameter total length */
		data[1] = PORT_PROTOCOL_PCM;

		if (i2s_pcm->protocol != PORT_PROTOCOL_PCM) {
			/*
			 * Short solution for PG1 chip, don't support I2S over
			 * the PCM/I2S bus...
			 */
			CG2900_ERR("I2S not supported over the PCM/I2S bus");
			err = -EACCES;
			goto error_handling_free_skb;
		}

		/*
		 * PCM (logical)
		 * 0 = Virtual port if multiple on vp, PCM /I2S index
		 */
		data[2] = 0x00;

		/*
		 * Set PCM direction and mode. They are a bit field in one byte.
		 */
		data[3] = CG2900_BT_HW_CONFIG_PCM_SET_DIR(0,
							  i2s_pcm->slot_0_dir);
		data[3] |= CG2900_BT_HW_CONFIG_PCM_SET_DIR(1,
							   i2s_pcm->slot_1_dir);
		data[3] |= CG2900_BT_HW_CONFIG_PCM_SET_DIR(2,
							   i2s_pcm->slot_2_dir);
		data[3] |= CG2900_BT_HW_CONFIG_PCM_SET_DIR(3,
							   i2s_pcm->slot_3_dir);
		if (i2s_pcm->mode == DAI_MODE_MASTER)
			data[3] |= CG2900_BT_HW_CONFIG_PCM_SET_MODE(
					CG2900_BT_HW_CONFIG_PCM_MODE_MASTER);
		else
			data[3] |= CG2900_BT_HW_CONFIG_PCM_SET_MODE(
					CG2900_BT_HW_CONFIG_PCM_MODE_SLAVE);
		data[4] = i2s_pcm->clk;
		data[5] = HCI_SET_U16_DATA_LSB(i2s_pcm->duration);
		data[6] = HCI_SET_U16_DATA_MSB(i2s_pcm->duration);

		/* Store the new configuration */
		mutex_lock(&audio_info->management_mutex);
		memcpy(&(audio_info->i2s_pcm_config), &(config->conf.i2s_pcm),
		       sizeof(config->conf.i2s_pcm));
		audio_info->i2s_pcm_config_known = true;
		mutex_unlock(&audio_info->management_mutex);
		break;

	default:
		CG2900_ERR("Unknown port configuration %d", config->port);
		err = -EACCES;
		goto error_handling_free_skb;
	};

	cb_info_bt.user = audio_user;
	SET_RESP_STATE(audio_user->resp_state, WAITING);

	/* Send packet to controller */
	err = cg2900_write(audio_info->dev_bt, skb);
	if (err) {
		CG2900_ERR("Error occurred while transmitting skb (%d)", err);
		goto error_handling_free_skb;
	}

	err = receive_bt_cmd_complete(audio_user,
				      CG2900_BT_VS_SET_HARDWARE_CONFIG_LSB,
				      CG2900_BT_VS_SET_HARDWARE_CONFIG_MSB,
				      NULL, 0);

	goto finished_unlock_mutex;

error_handling_free_skb:
	kfree_skb(skb);
finished_unlock_mutex:
	SET_RESP_STATE(audio_user->resp_state, IDLE);
	mutex_unlock(&audio_info->bt_mutex);
	return err;
}
EXPORT_SYMBOL(cg2900_audio_set_dai_config);

int cg2900_audio_get_dai_config(unsigned int session,
				struct cg2900_dai_config *config)
{
	int err = 0;
	struct audio_user *audio_user;

	CG2900_INFO("cg2900_audio_get_dai_config");

	if (audio_info->state != OPENED) {
		CG2900_ERR("Audio driver not open");
		return -EIO;
	}

	if (!config) {
		CG2900_ERR("NULL supplied as config structure");
		return -EINVAL;
	}

	audio_user = get_session_user(session);
	if (!audio_user)
		return -EINVAL;

	/*
	 * Return DAI configuration based on the received port.
	 * If port has not been configured return error.
	 */
	switch (config->port) {
	case PORT_0_I2S:
		mutex_lock(&audio_info->management_mutex);
		if (audio_info->i2s_config_known)
			memcpy(&(config->conf.i2s),
			       &(audio_info->i2s_config),
			       sizeof(config->conf.i2s));
		else
			err = -EIO;
		mutex_unlock(&audio_info->management_mutex);
		break;

	case PORT_1_I2S_PCM:
		mutex_lock(&audio_info->management_mutex);
		if (audio_info->i2s_pcm_config_known)
			memcpy(&(config->conf.i2s_pcm),
			       &(audio_info->i2s_pcm_config),
			       sizeof(config->conf.i2s_pcm));
		else
			err = -EIO;
		mutex_unlock(&audio_info->management_mutex);
		break;

	default:
		CG2900_ERR("Unknown port configuration %d", config->port);
		err = -EIO;
		break;
	};

	return err;
}
EXPORT_SYMBOL(cg2900_audio_get_dai_config);

int cg2900_audio_config_endpoint(unsigned int session,
				 struct cg2900_endpoint_config *config)
{
	struct audio_user *audio_user;

	CG2900_INFO("cg2900_audio_config_endpoint");

	if (audio_info->state != OPENED) {
		CG2900_ERR("Audio driver not open");
		return -EIO;
	}

	if (!config) {
		CG2900_ERR("NULL supplied as configuration structure");
		return -EINVAL;
	}

	audio_user = get_session_user(session);
	if (!audio_user)
		return -EINVAL;

	switch (config->endpoint_id) {
	case ENDPOINT_BT_SCO_INOUT:
	case ENDPOINT_BT_A2DP_SRC:
	case ENDPOINT_FM_RX:
	case ENDPOINT_FM_TX:
		add_endpoint(config, &(audio_info->endpoints));
		break;

	case ENDPOINT_PORT_0_I2S:
	case ENDPOINT_PORT_1_I2S_PCM:
	case ENDPOINT_SLIMBUS_VOICE:
	case ENDPOINT_SLIMBUS_AUDIO:
	case ENDPOINT_BT_A2DP_SNK:
	case ENDPOINT_ANALOG_OUT:
	case ENDPOINT_DSP_AUDIO_IN:
	case ENDPOINT_DSP_AUDIO_OUT:
	case ENDPOINT_DSP_VOICE_IN:
	case ENDPOINT_DSP_VOICE_OUT:
	case ENDPOINT_DSP_TONE_IN:
	case ENDPOINT_BURST_BUFFER_IN:
	case ENDPOINT_BURST_BUFFER_OUT:
	case ENDPOINT_MUSIC_DECODER:
	case ENDPOINT_HCI_AUDIO_IN:
	default:
		CG2900_ERR("Unknown endpoint_id %d", config->endpoint_id);
		return -EACCES;
	}


	return 0;
}
EXPORT_SYMBOL(cg2900_audio_config_endpoint);

int cg2900_audio_start_stream(unsigned int session,
			      enum cg2900_audio_endpoint_id ep_1,
			      enum cg2900_audio_endpoint_id ep_2,
			      unsigned int *stream_handle)
{
	int err;
	struct audio_user *audio_user;

	CG2900_INFO("cg2900_audio_start_stream");

	if (audio_info->state != OPENED) {
		CG2900_ERR("Audio driver not open");
		return -EIO;
	}

	audio_user = get_session_user(session);
	if (!audio_user)
		return -EINVAL;

	/* First handle the endpoints */
	switch (ep_1) {
	case ENDPOINT_PORT_0_I2S:
		switch (ep_2) {
		case ENDPOINT_FM_RX:
			err = conn_start_i2s_to_fm_rx(audio_user,
						      stream_handle);
			break;

		case ENDPOINT_FM_TX:
			err = conn_start_i2s_to_fm_tx(audio_user,
						      stream_handle);
			break;

		default:
			CG2900_ERR("Endpoint config not handled: ep1: %d, "
				   "ep2: %d", ep_1, ep_2);
			return -EINVAL;
		}
		break;

	case ENDPOINT_PORT_1_I2S_PCM:
		switch (ep_2) {
		case ENDPOINT_BT_SCO_INOUT:
			err = conn_start_pcm_to_sco(audio_user, stream_handle);
			break;

		default:
			CG2900_ERR("Endpoint config not handled: ep1: %d, "
				   "ep2: %d", ep_1, ep_2);
			return -EINVAL;
		}
		break;

	case ENDPOINT_BT_SCO_INOUT:
		switch (ep_2) {
		case ENDPOINT_PORT_1_I2S_PCM:
			err = conn_start_pcm_to_sco(audio_user, stream_handle);
			break;

		default:
			CG2900_ERR("Endpoint config not handled: ep1: %d, "
				   "ep2: %d", ep_1, ep_2);
			return -EINVAL;
		}
		break;

	case ENDPOINT_FM_RX:
		switch (ep_2) {
		case ENDPOINT_PORT_0_I2S:
			err = conn_start_i2s_to_fm_rx(audio_user,
						      stream_handle);
			break;

		default:
			CG2900_ERR("Endpoint config not handled: ep1: %d, "
				   "ep2: %d", ep_1, ep_2);
			return -EINVAL;
		}
		break;

	case ENDPOINT_FM_TX:
		switch (ep_2) {
		case ENDPOINT_PORT_0_I2S:
			err = conn_start_i2s_to_fm_tx(audio_user,
						      stream_handle);
			break;

		default:
			CG2900_ERR("Endpoint config not handled: ep1: %d, "
				   "ep2: %d", ep_1, ep_2);
			return -EINVAL;
		}
		break;

	default:
		CG2900_ERR("Endpoint config not handled: ep1: %d, ep2: %d",
			   ep_1, ep_2);
		return -EINVAL;
	}

	return err;
}
EXPORT_SYMBOL(cg2900_audio_start_stream);

int cg2900_audio_stop_stream(unsigned int session, unsigned int stream_handle)
{
	int err = 0;
	struct audio_user *audio_user;

	CG2900_INFO("cg2900_audio_stop_stream");

	if (audio_info->state != OPENED) {
		CG2900_ERR("Audio driver not open");
		return -EIO;
	}

	audio_user = get_session_user(session);
	if (!audio_user)
		return -EINVAL;

	err = conn_stop_stream(audio_user, stream_handle);

	return err;
}
EXPORT_SYMBOL(cg2900_audio_stop_stream);

/*
 *	Character devices for userspace module test
 */

/**
 * audio_dev_open() - Open char device.
 * @inode:	Device driver information.
 * @filp:	Pointer to the file struct.
 *
 * Returns:
 *   0 if there is no error.
 *   -ENOMEM if allocation failed.
 *   Errors from @cg2900_audio_open.
 */
static int audio_dev_open(struct inode *inode, struct file *filp)
{
	int err;
	struct char_dev_info *char_dev_info;

	CG2900_INFO("CG2900 Audio: audio_dev_open");

	/*
	 * Allocate the char dev info structure. It will be stored inside
	 * the file pointer and supplied when file_ops are called.
	 * It's free'd in audio_dev_release.
	 */
	char_dev_info = kzalloc(sizeof(*char_dev_info), GFP_KERNEL);
	if (!char_dev_info) {
		CG2900_ERR("Couldn't allocate char_dev_info");
		return -ENOMEM;
	}
	filp->private_data = char_dev_info;

	mutex_init(&char_dev_info->management_mutex);
	mutex_init(&char_dev_info->rw_mutex);

	mutex_lock(&char_dev_info->management_mutex);
	err = cg2900_audio_open(&char_dev_info->session);
	mutex_unlock(&char_dev_info->management_mutex);
	if (err) {
		CG2900_ERR("Failed to open CG2900 Audio driver (%d)", err);
		goto error_handling_free_mem;
	}

	return 0;

error_handling_free_mem:
	kfree(char_dev_info);
	filp->private_data = NULL;
	return err;
}

/**
 * audio_dev_release() - Release char device.
 * @inode:	Device driver information.
 * @filp:	Pointer to the file struct.
 *
 * Returns:
 *   0 if there is no error.
 *   -EBADF if NULL pointer was supplied in private data.
 *   Errors from @cg2900_audio_close.
 */
static int audio_dev_release(struct inode *inode, struct file *filp)
{
	int err = 0;
	struct char_dev_info *dev = (struct char_dev_info *)filp->private_data;

	CG2900_INFO("CG2900 Audio: audio_dev_release");

	if (!dev) {
		CG2900_ERR("No dev supplied in private data");
		return -EBADF;
	}

	mutex_lock(&dev->management_mutex);
	err = cg2900_audio_close(&dev->session);
	if (err)
		/*
		 * Just print the error. Still free the char_dev_info since we
		 * don't know the filp structure is valid after this call
		 */
		CG2900_ERR("Error when closing CG2900 audio driver (%d)", err);

	mutex_unlock(&dev->management_mutex);

	kfree(dev);
	filp->private_data = NULL;

	return err;
}

/**
 * audio_dev_read() - Return information to the user from last @write call.
 * @filp:	Pointer to the file struct.
 * @buf:	Received buffer.
 * @count:	Size of buffer.
 * @f_pos:	Position in buffer.
 *
 * The audio_dev_read() function returns information from
 * the last @write call to same char device.
 * The data is in the following format:
 *   * OpCode of command for this data
 *   * Data content (Length of data is determined by the command OpCode, i.e.
 *     fixed for each command)
 *
 * Returns:
 *   Bytes successfully read (could be 0).
 *   -EBADF if NULL pointer was supplied in private data.
 *   -EFAULT if copy_to_user fails.
 *   -ENOMEM upon allocation failure.
 */
static ssize_t audio_dev_read(struct file *filp, char __user *buf, size_t count,
			      loff_t *f_pos)
{
	struct char_dev_info *dev = (struct char_dev_info *)filp->private_data;
	unsigned int bytes_to_copy = 0;
	int err = 0;

	CG2900_INFO("CG2900 Audio: audio_dev_read");

	if (!dev) {
		CG2900_ERR("No dev supplied in private data");
		return -EBADF;
	}
	mutex_lock(&dev->rw_mutex);

	if (dev->stored_data_len == 0) {
		/* No data to read */
		bytes_to_copy = 0;
		goto finished;
	}

	bytes_to_copy = min(count, (unsigned int)(dev->stored_data_len));
	if (bytes_to_copy < dev->stored_data_len)
		CG2900_ERR("Not enough buffer to store all data. Throwing away "
			   "rest of data.");

	err = copy_to_user(buf, dev->stored_data, bytes_to_copy);
	/*
	 * Throw away all data, even though not all was copied.
	 * This char device is primarily for testing purposes so we can keep
	 * such a limitation.
	 */
	kfree(dev->stored_data);
	dev->stored_data = NULL;
	dev->stored_data_len = 0;

	if (err) {
		CG2900_ERR("copy_to_user error %d", err);
		err = -EFAULT;
		goto error_handling;
	}

	goto finished;

error_handling:
	mutex_unlock(&dev->rw_mutex);
	return (ssize_t)err;
finished:
	mutex_unlock(&dev->rw_mutex);
	return bytes_to_copy;
}

/**
 * audio_dev_write() - Call CG2900 Audio API function.
 * @filp:	Pointer to the file struct.
 * @buf:	Write buffer.
 * @count:	Size of the buffer write.
 * @f_pos:	Position of buffer.
 *
 * audio_dev_write() function executes supplied data and
 * interprets it as if it was a function call to the CG2900 Audio API.
 * The data is according to:
 *   * OpCode (4 bytes)
 *   * Data according to OpCode (see API). No padding between parameters
 *
 * OpCodes are:
 *   * OP_CODE_SET_DAI_CONF 0x00000001
 *   * OP_CODE_GET_DAI_CONF 0x00000002
 *   * OP_CODE_CONFIGURE_ENDPOINT 0x00000003
 *   * OP_CODE_START_STREAM 0x00000004
 *   * OP_CODE_STOP_STREAM 0x00000005
 *
 * Returns:
 *   Bytes successfully written (could be 0). Equals input @count if successful.
 *   -EBADF if NULL pointer was supplied in private data.
 *   -EFAULT if copy_from_user fails.
 *   Error codes from all CG2900 Audio API functions.
 */
static ssize_t audio_dev_write(struct file *filp, const char __user *buf,
			       size_t count, loff_t *f_pos)
{
	u8 *rec_data;
	struct char_dev_info *dev = (struct char_dev_info *)filp->private_data;
	int err = 0;
	int op_code = 0;
	u8 *curr_data;
	unsigned int stream_handle;
	struct cg2900_dai_config dai_config;
	struct cg2900_endpoint_config ep_config;
	enum cg2900_audio_endpoint_id ep_1;
	enum cg2900_audio_endpoint_id ep_2;
	int bytes_left = count;

	CG2900_INFO("CG2900 Audio: audio_dev_write count %d", count);

	if (!dev) {
		CG2900_ERR("No dev supplied in private data");
		return -EBADF;
	}

	rec_data = kmalloc(count, GFP_KERNEL);
	if (!rec_data) {
		CG2900_ERR("kmalloc failed");
		return -ENOMEM;
	}

	mutex_lock(&dev->rw_mutex);

	err = copy_from_user(rec_data, buf, count);
	if (err) {
		CG2900_ERR("copy_from_user failed (%d)", err);
		err = -EFAULT;
		goto finished_mutex_unlock;
	}

	/* Initialize temporary data pointer used to traverse the packet */
	curr_data = rec_data;

	op_code = curr_data[0];
	CG2900_DBG("op_code %d", op_code);
	/* OpCode is int size to keep data int aligned */
	curr_data += sizeof(unsigned int);
	bytes_left -= sizeof(unsigned int);

	switch (op_code) {
	case OP_CODE_SET_DAI_CONF:
		CG2900_DBG("OP_CODE_SET_DAI_CONF %d", sizeof(dai_config));
		if (bytes_left < sizeof(dai_config)) {
			CG2900_ERR("Not enough data supplied for "
				   "OP_CODE_SET_DAI_CONF");
			err = -EINVAL;
			goto finished_mutex_unlock;
		}
		memcpy(&dai_config, curr_data, sizeof(dai_config));
		CG2900_DBG("dai_config.port %d", dai_config.port);
		err = cg2900_audio_set_dai_config(dev->session, &dai_config);
		break;

	case OP_CODE_GET_DAI_CONF:
		CG2900_DBG("OP_CODE_GET_DAI_CONF %d", sizeof(dai_config));
		if (bytes_left < sizeof(dai_config)) {
			CG2900_ERR("Not enough data supplied for "
				   "OP_CODE_GET_DAI_CONF");
			err = -EINVAL;
			goto finished_mutex_unlock;
		}
		/*
		 * Only need to copy the port really, but let's copy
		 * like this for simplicity. It's only test functionality
		 * after all.
		 */
		memcpy(&dai_config, curr_data, sizeof(dai_config));
		CG2900_DBG("dai_config.port %d", dai_config.port);
		err = cg2900_audio_get_dai_config(dev->session, &dai_config);
		if (!err) {
			/*
			 * Command succeeded. Store data so it can be returned
			 * when calling read.
			 */
			if (dev->stored_data) {
				CG2900_ERR("Data already allocated (%d bytes). "
					   "Throwing it away.",
					   dev->stored_data_len);
				kfree(dev->stored_data);
			}
			dev->stored_data_len = sizeof(op_code) +
					       sizeof(dai_config);
			dev->stored_data = kmalloc(dev->stored_data_len,
						   GFP_KERNEL);
			if (dev->stored_data) {
				memcpy(dev->stored_data, &op_code,
				       sizeof(op_code));
				memcpy(&(dev->stored_data[sizeof(op_code)]),
				       &dai_config, sizeof(dai_config));
			}
		}
		break;

	case OP_CODE_CONFIGURE_ENDPOINT:
		CG2900_DBG("OP_CODE_CONFIGURE_ENDPOINT %d", sizeof(ep_config));
		if (bytes_left < sizeof(ep_config)) {
			CG2900_ERR("Not enough data supplied for "
				   "OP_CODE_CONFIGURE_ENDPOINT");
			err = -EINVAL;
			goto finished_mutex_unlock;
		}
		memcpy(&ep_config, curr_data, sizeof(ep_config));
		CG2900_DBG("ep_config.endpoint_id %d", ep_config.endpoint_id);
		err = cg2900_audio_config_endpoint(dev->session, &ep_config);
		break;

	case OP_CODE_START_STREAM:
		CG2900_DBG("OP_CODE_START_STREAM %d",
			   (sizeof(ep_1) + sizeof(ep_2)));
		if (bytes_left < (sizeof(ep_1) + sizeof(ep_2))) {
			CG2900_ERR("Not enough data supplied for "
				   "OP_CODE_START_STREAM");
			err = -EINVAL;
			goto finished_mutex_unlock;
		}
		memcpy(&ep_1, curr_data, sizeof(ep_1));
		curr_data += sizeof(ep_1);
		memcpy(&ep_2, curr_data, sizeof(ep_2));
		CG2900_DBG("ep_1 %d ep_2 %d", ep_1,
			   ep_2);

		err = cg2900_audio_start_stream(dev->session,
			ep_1, ep_2, &stream_handle);
		if (!err) {
			/*
			 * Command succeeded. Store data so it can be returned
			 * when calling read.
			 */
			if (dev->stored_data) {
				CG2900_ERR("Data already allocated (%d bytes). "
					   "Throwing it away.",
					   dev->stored_data_len);
				kfree(dev->stored_data);
			}
			dev->stored_data_len = sizeof(op_code) +
					       sizeof(stream_handle);
			dev->stored_data = kmalloc(dev->stored_data_len,
						   GFP_KERNEL);
			if (dev->stored_data) {
				memcpy(dev->stored_data, &op_code,
				       sizeof(op_code));
				memcpy(&(dev->stored_data[sizeof(op_code)]),
				       &stream_handle, sizeof(stream_handle));
			}
			CG2900_DBG("stream_handle %d", stream_handle);
		}
		break;

	case OP_CODE_STOP_STREAM:
		if (bytes_left < sizeof(stream_handle)) {
			CG2900_ERR("Not enough data supplied for "
				   "OP_CODE_STOP_STREAM");
			err = -EINVAL;
			goto finished_mutex_unlock;
		}
		CG2900_DBG("OP_CODE_STOP_STREAM %d", sizeof(stream_handle));
		memcpy(&stream_handle, curr_data, sizeof(stream_handle));
		CG2900_DBG("stream_handle %d", stream_handle);
		err = cg2900_audio_stop_stream(dev->session, stream_handle);
		break;

	default:
		CG2900_ERR("Received bad op_code %d", op_code);
		break;
	};

finished_mutex_unlock:
	kfree(rec_data);
	mutex_unlock(&dev->rw_mutex);

	if (err)
		return err;
	else
		return count;
}

/**
 * audio_dev_poll() - Handle POLL call to the interface.
 * @filp:	Pointer to the file struct.
 * @wait:	Poll table supplied to caller.
 *
 * This function is used by the User Space application to see if the device is
 * still open and if there is any data available for reading.
 *
 * Returns:
 *   Mask of current set POLL values
 */
static unsigned int audio_dev_poll(struct file *filp, poll_table *wait)
{
	struct char_dev_info *dev = (struct char_dev_info *)filp->private_data;
	unsigned int mask = 0;

	if (!dev) {
		CG2900_ERR("No dev supplied in private data");
		return POLLERR | POLLRDHUP;
	}

	if (RESET == audio_info->state)
		mask |= POLLERR | POLLRDHUP | POLLPRI;
	else
		/* Unless RESET we can transmit */
		mask |= POLLOUT;

	if (dev->stored_data)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static const struct file_operations char_dev_fops = {
	.open = audio_dev_open,
	.release = audio_dev_release,
	.read = audio_dev_read,
	.write = audio_dev_write,
	.poll = audio_dev_poll
};

/*
 *	Module related methods
 */

/**
 * cg2900_audio_init() - Initialize module.
 *
 * Initialize the module and register misc device.
 *
 * Returns:
 *   0 if there is no error.
 *   -ENOMEM if allocation fails.
 *   -EEXIST if device has already been started.
 *   Error codes from misc_register.
 */
//static int __init cg2900_audio_init(void)
int cg2900_audio_init(void)
{
	int err;

	CG2900_INFO("cg2900_audio_init");

	if (audio_info) {
		CG2900_ERR("ST-Ericsson CG2900 Audio driver already initiated");
		return -EEXIST;
	}

	/* Initialize private data. */
	audio_info = kzalloc(sizeof(*audio_info), GFP_KERNEL);
	if (!audio_info) {
		CG2900_ERR("Could not alloc audio_info struct.");
		return -ENOMEM;
	}

	/* Initiate the mutexes */
	mutex_init(&(audio_info->management_mutex));
	mutex_init(&(audio_info->bt_mutex));
	mutex_init(&(audio_info->fm_mutex));
	mutex_init(&(audio_info->endpoints.management_mutex));

	/* Initiate the SKB queues */
	skb_queue_head_init(&(audio_info->bt_queue));
	skb_queue_head_init(&(audio_info->fm_queue));

	/* Initiate the endpoint list */
	INIT_LIST_HEAD(&(audio_info->endpoints.ep_list));

	/* Prepare and register MISC device */
	audio_info->dev.minor = MISC_DYNAMIC_MINOR;
	audio_info->dev.name = DEVICE_NAME;
	audio_info->dev.fops = &char_dev_fops;
	audio_info->dev.parent = NULL;

	err = misc_register(&(audio_info->dev));
	if (err) {
		CG2900_ERR("Error %d registering misc dev!", err);
		goto error_handling;
	}

	return 0;

error_handling:
	mutex_destroy(&audio_info->management_mutex);
	mutex_destroy(&audio_info->bt_mutex);
	mutex_destroy(&audio_info->fm_mutex);
	mutex_destroy(&audio_info->endpoints.management_mutex);
	kfree(audio_info);
	audio_info = NULL;
	return err;
}

/**
 * cg2900_audio_exit() - Remove module.
 *
 * Remove misc device and free resources.
 */
//static void __exit cg2900_audio_exit(void)
void cg2900_audio_exit(void)
{
	int err;

	CG2900_INFO("cg2900_audio_exit");

	if (!audio_info)
		return;

	err = misc_deregister(&audio_info->dev);
	if (err)
		CG2900_ERR("Error deregistering misc dev (%d)!", err);

	mutex_destroy(&audio_info->management_mutex);
	mutex_destroy(&audio_info->bt_mutex);
	mutex_destroy(&audio_info->fm_mutex);

	flush_endpoint_list(&(audio_info->endpoints));

	skb_queue_purge(&(audio_info->bt_queue));
	skb_queue_purge(&(audio_info->fm_queue));

	mutex_destroy(&audio_info->endpoints.management_mutex);

	kfree(audio_info);
	audio_info = NULL;
}

//module_init(cg2900_audio_init);
//module_exit(cg2900_audio_exit);

MODULE_AUTHOR("Par-Gunnar Hjalmdahl ST-Ericsson");
MODULE_AUTHOR("Kjell Andersson ST-Ericsson");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Linux Bluetooth/FM Audio ST-Ericsson controller");
