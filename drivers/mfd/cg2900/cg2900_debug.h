/*
 * drivers/mfd/cg2900/cg2900_debug.h
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
 * Debug functionality for the Linux Bluetooth HCI H:4 Driver for ST-Ericsson
 * CG2900 connectivity controller.
 */

#ifndef _CG2900_DEBUG_H_
#define _CG2900_DEBUG_H_

#include <linux/kernel.h>
#include <linux/types.h>

//#define CG2900_DEFAULT_DEBUG_LEVEL 40
//CG2900_GB
#define CG2900_DEFAULT_DEBUG_LEVEL 0
//CG2900_GB

/* module_param declared in cg2900_core.c */
extern int cg2900_debug_level;

#if defined(NDEBUG) || CG2900_DEFAULT_DEBUG_LEVEL == 0
	#define CG2900_DBG_DATA_CONTENT(fmt, arg...)
	#define CG2900_DBG_DATA(fmt, arg...)
	#define CG2900_DBG(fmt, arg...)
	#define CG2900_INFO(fmt, arg...)
	#define CG2900_ERR(fmt, arg...)
#else
	#define CG2900_DBG_DATA_CONTENT(__prefix, __buf, __len)		\
	do {								\
		if (cg2900_debug_level >= 30)				\
			print_hex_dump_bytes("CG2900 " __prefix ": " ,	\
					     DUMP_PREFIX_NONE, __buf, __len); \
	} while (0)	
	#define CG2900_DBG_DATA(fmt, arg...)				\
	do {								\
		if (cg2900_debug_level >= 25)				\
			printk(KERN_DEBUG "CG2900 %s : " fmt "\n" , __func__ , \
			       ## arg); \
	} while (0)

	#define CG2900_DBG(fmt, arg...)				\
	do {								\
		if (cg2900_debug_level >= 20)				\
			printk(KERN_DEBUG "CG2900 %s : " fmt "\n" , __func__ ,  \
			       ## arg); \
	} while (0)

	#define CG2900_INFO(fmt, arg...)				\
	do {								\
		if (cg2900_debug_level >= 10)				\
			printk(KERN_INFO "CG2900 %s : " fmt "\n" , __func__ ,  \
			       ## arg); \
	} while (0)

	#define CG2900_ERR(fmt, arg...)				\
	do {								\
		if (cg2900_debug_level >= 1)				\
			printk(KERN_ERR  "CG2900 %s : " fmt "\n" , __func__ , \
			       ## arg); \
	} while (0)
#endif /* NDEBUG */

#define CG2900_SET_STATE(__name, __var, __new_state)	\
do {									\
	CG2900_DBG("New %s: 0x%X", __name, (uint32_t)__new_state);	\
	__var = __new_state;				\
} while (0)

#endif /* _CG2900_DEBUG_H_ */
