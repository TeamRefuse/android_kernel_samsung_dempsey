/*
 * arch/arm/mach-ux500/include/mach/cg2900_devices.h
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
 * Board specific device support for the Linux Bluetooth HCI H4 Driver
 * for ST-Ericsson connectivity controller.
 */

#ifndef _CG2900_DEVICES_H_
#define _CG2900_DEVICES_H_

#include <linux/types.h>
#include <linux/skbuff.h>

/**
 * cg2900_devices_enable_chip() - Enable the controller.
 */
extern void cg2900_devices_enable_chip(void);

/**
 * cg2900_devices_disable_chip() - Disable the controller.
 */
extern void cg2900_devices_disable_chip(void);

/**
 * cg2900_devices_set_hci_revision() - Stores HCI revision info for the connected connectivity controller.
 * @hci_version:	HCI version from the controller.
 * @hci_revision:	HCI revision from the controller.
 * @lmp_version:	LMP version from the controller.
 * @lmp_subversion:	LMP subversion from the controller.
 * @manufacturer:	Manufacturer ID from the controller.
 *
 * See Bluetooth specification and white paper for used controller for details
 * about parameters.
 */
extern void cg2900_devices_set_hci_revision(u8 hci_version,
					    u16 hci_revision,
					    u8 lmp_version,
					    u8 lmp_subversion,
					    u16 manufacturer);

/**
 * cg2900_devices_get_power_switch_off_cmd() - Get HCI power switch off command to use based on connected connectivity controller.
 * @op_lsb:	LSB of HCI opcode in generated packet. NULL if not needed.
 * @op_msb:	MSB of HCI opcode in generated packet. NULL if not needed.
 *
 * This command does not add the H4 channel header in front of the message.
 *
 * Returns:
 *   NULL if no command shall be sent,
 *   sk_buffer with command otherwise.
 */
extern struct sk_buff *cg2900_devices_get_power_switch_off_cmd(u8 *op_lsb,
							       u8 *op_msb);

/**
 * cg2900_devices_get_bt_enable_cmd() - Get HCI BT enable command to use based on connected connectivity controller.
 * @op_lsb:	LSB of HCI opcode in generated packet. NULL if not needed.
 * @op_msb:	MSB of HCI opcode in generated packet. NULL if not needed.
 * @bt_enable:	true if Bluetooth IP shall be enabled, false otherwise.
 *
 * This command does not add the H4 channel header in front of the message.
 *
 * Returns:
 *   NULL if no command shall be sent,
 *   sk_buffer with command otherwise.
 */
extern struct sk_buff *cg2900_devices_get_bt_enable_cmd(u8 *op_lsb,
							u8 *op_msb,
							bool bt_enable);

/**
 * cg2900_devices_init() - Initialize the board config.
 *
 * Returns:
 *   0 if there is no error.
 *   Error codes from gpio_request and gpio_direction_output.
 */
extern int cg2900_devices_init(void);

/**
 * cg2900_devices_exit() - Exit function for the board config.
 */
extern void cg2900_devices_exit(void);

#endif /* _CG2900_DEVICES_H_ */
