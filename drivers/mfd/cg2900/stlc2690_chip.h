/*
 * drivers/mfd/cg2900/stlc2690_chip.h
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
 * Linux Bluetooth defines for ST-Ericsson connectivity controller STLC2690.
 */

#ifndef _STLC2690_CHIP_H_
#define _STLC2690_CHIP_H_

#include "hci_defines.h"

/* Bluetooth Vendor Specific Opcode Command Field (OCF) */
#define STLC2690_BT_OCF_VS_STORE_IN_FS			0x0022
#define STLC2690_BT_OCF_VS_WRITE_FILE_BLOCK		0x002E
#define STLC2690_BT_OCF_VS_READ_REGISTER		0x0104
#define STLC2690_BT_OCF_VS_WRITE_REGISTER		0x0103

/* Defines needed for HCI VS Store In FS cmd creation. */
#define STLC2690_VS_STORE_IN_FS_USR_ID_POS		HCI_BT_CMD_PARAM_POS
#define STLC2690_VS_STORE_IN_FS_USR_ID_SIZE		1
#define STLC2690_VS_STORE_IN_FS_PARAM_LEN_POS		\
	(STLC2690_VS_STORE_IN_FS_USR_ID_POS + \
	 STLC2690_VS_STORE_IN_FS_USR_ID_SIZE)
#define STLC2690_VS_STORE_IN_FS_PARAM_LEN_SIZE		1
#define STLC2690_VS_STORE_IN_FS_PARAM_POS		\
	(STLC2690_VS_STORE_IN_FS_PARAM_LEN_POS + \
	 STLC2690_VS_STORE_IN_FS_PARAM_LEN_SIZE)
#define STLC2690_VS_STORE_IN_FS_USR_ID_BD_ADDR		0xFE

#endif /* _STLC2690_CHIP_H_ */
