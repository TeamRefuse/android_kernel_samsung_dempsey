/*
 * drivers/mfd/cg2900/hci_defines.h
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
 * Linux Bluetooth HCI defines for ST-Ericsson CG2900 connectivity controller.
 */

#ifndef _BLUETOOTH_DEFINES_H_
#define _BLUETOOTH_DEFINES_H_

#include <linux/types.h>

/* H:4 offset in an HCI packet */
#define HCI_H4_POS				0
#define HCI_H4_SIZE				1

/* Standardized Bluetooth H:4 channels */
#define HCI_BT_CMD_H4_CHANNEL			0x01
#define HCI_BT_ACL_H4_CHANNEL			0x02
#define HCI_BT_SCO_H4_CHANNEL			0x03
#define HCI_BT_EVT_H4_CHANNEL			0x04

/* Bluetooth Opcode Group Field (OGF) */
#define HCI_BT_OGF_LINK_CTRL			0x01
#define HCI_BT_OGF_LINK_POLICY			0x02
#define HCI_BT_OGF_CTRL_BB			0x03
#define HCI_BT_OGF_LINK_INFO			0x04
#define HCI_BT_OGF_LINK_STATUS			0x05
#define HCI_BT_OGF_LINK_TESTING			0x06
#define HCI_BT_OGF_VS				0x3F

/* Bluetooth Opcode Command Field (OCF) */
#define HCI_BT_OCF_READ_LOCAL_VERSION_INFO	0x0001
#define HCI_BT_OCF_RESET			0x0003

/* Bluetooth HCI command OpCodes in LSB/MSB fashion */
#define HCI_BT_RESET_CMD_LSB			0x03
#define HCI_BT_RESET_CMD_MSB			0x0C
#define HCI_BT_READ_LOCAL_VERSION_CMD_LSB	0x01
#define HCI_BT_READ_LOCAL_VERSION_CMD_MSB	0x10

/* Bluetooth Event OpCodes */
#define HCI_BT_EVT_CMD_COMPLETE			0x0E
#define HCI_BT_EVT_CMD_STATUS			0x0F

/* Bluetooth Command offsets */
#define HCI_BT_CMD_ID_POS			1
#define HCI_BT_CMD_PARAM_LEN_POS		3
#define HCI_BT_CMD_PARAM_POS			4
#define HCI_BT_CMD_HDR_SIZE			4

/* Bluetooth Event offsets for CG2900 users, i.e. not including H:4 channel */
#define HCI_BT_EVT_ID_POS			0
#define HCI_BT_EVT_LEN_POS			1
#define HCI_BT_EVT_CMD_COMPL_ID_POS		3
#define HCI_BT_EVT_CMD_STATUS_ID_POS		4
#define HCI_BT_EVT_CMD_COMPL_STATUS_POS		5
#define HCI_BT_EVT_CMD_STATUS_STATUS_POS	2
#define HCI_BT_EVT_CMD_COMPL_NR_OF_PKTS_POS	2
#define HCI_BT_EVT_CMD_STATUS_NR_OF_PKTS_POS	3

#define HCI_BT_EVT_CMD_COMPL_ID_POS_LSB		HCI_BT_EVT_CMD_COMPL_ID_POS
#define HCI_BT_EVT_CMD_COMPL_ID_POS_MSB        (HCI_BT_EVT_CMD_COMPL_ID_POS + 1)
#define HCI_BT_EVT_CMD_STATUS_ID_POS_LSB	HCI_BT_EVT_CMD_STATUS_ID_POS
#define HCI_BT_EVT_CMD_STATUS_ID_POS_MSB      (HCI_BT_EVT_CMD_STATUS_ID_POS + 1)

/* Bluetooth error codes */
#define HCI_BT_ERROR_NO_ERROR			0x00
#define HCI_BT_ERROR_CMD_DISALLOWED		0x0C

/* Bluetooth lengths */
#define HCI_BT_SEND_FILE_MAX_CHUNK_SIZE		254

#define HCI_BT_RESET_LEN			3
#define HCI_BT_RESET_PARAM_LEN			0
#define HCI_BT_CMD_COMPLETE_NO_PARAM_LEN	4

/* Utility macros */
#define GET_U16_FROM_L_ENDIAN(__u8_lsb, __u8_msb)	((u16)(__u8_lsb | \
							 (__u8_msb << 8)))
#define HCI_BT_MAKE_FIRST_BYTE_IN_CMD(__ocf)		((u8)(__ocf & 0x00FF))
#define HCI_BT_MAKE_SECOND_BYTE_IN_CMD(__ogf, __ocf)	((u8)(__ogf << 2) | \
		((__ocf >> 8) & 0x0003))
#define HCI_BT_ID_TO_OCF(__1st_byte, __2nd_byte)	((u16)(__1st_byte | \
		((__2nd_byte & 0x03) << 8)))
#define HCI_BT_ID_TO_OGF(__1st_byte)		((u8)(__1st_byte >> 2))
#define HCI_BT_GET_ID				GET_U16_FROM_L_ENDIAN
#define HCI_BT_EVENT_GET_ID			GET_U16_FROM_L_ENDIAN

/* Macros to set multibyte words to correct HCI endian (always Little Endian) */
#define HCI_SET_U16_DATA_LSB(__u16_data)	((u8)(__u16_data & 0x00FF))
#define HCI_SET_U16_DATA_MSB(__u16_data)	((u8)(__u16_data >> 8))

#endif /* _BLUETOOTH_DEFINES_H_ */
