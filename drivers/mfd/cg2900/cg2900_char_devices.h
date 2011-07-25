/*
 * drivers/mfd/cg2900/cg2900_char_devices.h
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

#ifndef _CG2900_CHAR_DEVICES_H_
#define _CG2900_CHAR_DEVICES_H_

/**
 * cg2900_char_devices_init() - Initialize char device module.
 * @char_dev_usage:	Bitmask indicating what char devs to enable.
 * @dev:		Parent device for the driver.
 *
 * Returns:
 *   0 if success.
 *   Negative value upon error.
 */
extern int cg2900_char_devices_init(int char_dev_usage, struct device *dev);

/**
 * cg2900_char_devices_exit() - Release the char device module.
 */
extern void cg2900_char_devices_exit(void);

#endif /* _CG2900_CHAR_DEVICES_H_ */
