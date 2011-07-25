/*
 * arch/arm/mach-ux500/cg2900_devices.c
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
 * Board specific device support for the Linux Bluetooth HCI H:4 Driver
 * for ST-Ericsson connectivity controller.
 */

#include <linux/string.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <asm-generic/errno-base.h>
#include <mach/cg2900_devices.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio.h>

/** GBF_ENA_RESET_GPIO - GPIO to enable/disable the controller.
 */
#define GBF_ENA_RESET_GPIO		GPIO_GPS_nRST

/** GBF_ENA_RESET_NAME - Name of GPIO for enabling/disabling.
 */
#define GBF_ENA_RESET_NAME		"gbf_ena_reset"

/* Bluetooth Opcode Group Field */
#define HCI_BT_OGF_LINK_CTRL		0x01
#define HCI_BT_OGF_LINK_POLICY		0x02
#define HCI_BT_OGF_CTRL_BB		0x03
#define HCI_BT_OGF_LINK_INFO		0x04
#define HCI_BT_OGF_LINK_STATUS		0x05
#define HCI_BT_OGF_LINK_TESTING		0x06
#define HCI_BT_OGF_VS			0x3F

#define MAKE_FIRST_BYTE_IN_CMD(__ocf)		((u8)(__ocf & 0x00FF))
#define MAKE_SECOND_BYTE_IN_CMD(__ogf, __ocf)	((u8)(__ogf << 2) | \
						((__ocf >> 8) & 0x0003))

/* Bluetooth Opcode Command Field */
#define HCI_BT_OCF_RESET		0x0003
#define HCI_BT_OCF_VS_POWER_SWITCH_OFF	0x0140
#define HCI_BT_OCF_VS_SYSTEM_RESET	0x0312
#define HCI_BT_OCF_VS_BT_ENABLE		0x0310

/* Bluetooth HCI command lengths */
#define HCI_BT_LEN_RESET		0x03
#define HCI_BT_LEN_VS_POWER_SWITCH_OFF	0x09
#define HCI_BT_LEN_VS_SYSTEM_RESET	0x03
#define HCI_BT_LEN_VS_BT_ENABLE		0x04

#define H4_HEADER_LENGTH		0x01

#define STLC2690_HCI_REV		0x0600
#define CG2900_HCI_REV_PG_1			0x0100
#define CG2900_HCI_REV_PG_2			0x0200
#define CG2900_SPECIAL_HCI_REV		0x0700

static u8	cg2900_hci_version;
static u8	cg2900_lmp_version;
static u8	cg2900_lmp_subversion;
static u16	cg2900_hci_revision;
static u16	cg2900_manufacturer;
extern void s3c_setup_uart_cfg_gpio(unsigned char port);

void cg2900_devices_enable_chip(void)
{
	printk(KERN_DEBUG "cg2900_devices_enable_chip");
	gpio_set_value(GBF_ENA_RESET_GPIO, 1);
}
EXPORT_SYMBOL(cg2900_devices_enable_chip);

void cg2900_devices_disable_chip(void)
{
	printk(KERN_DEBUG "cg2900_devices_disable_chip");
	gpio_set_value(GBF_ENA_RESET_GPIO, 0);
}
EXPORT_SYMBOL(cg2900_devices_disable_chip);

void cg2900_devices_set_hci_revision(u8 hci_version,
				     u16 hci_revision,
				     u8 lmp_version,
				     u8 lmp_subversion,
				     u16 manufacturer)
{
	cg2900_hci_version	= hci_version;
	cg2900_hci_revision	= hci_revision;
	cg2900_lmp_version	= lmp_version;
	cg2900_lmp_subversion	= lmp_subversion;
	cg2900_manufacturer	= manufacturer;
}
EXPORT_SYMBOL(cg2900_devices_set_hci_revision);

struct sk_buff *cg2900_devices_get_power_switch_off_cmd(u8 *op_lsb, u8 *op_msb)
{
	struct sk_buff *skb = NULL;
	u8 *data = NULL;

	/* If connected chip does not support the command return NULL */
	if (CG2900_HCI_REV_PG_1 != cg2900_hci_revision &&
		CG2900_HCI_REV_PG_2 != cg2900_hci_revision &&
	    CG2900_SPECIAL_HCI_REV != cg2900_hci_revision)
		goto finished;

	skb = alloc_skb(HCI_BT_LEN_VS_POWER_SWITCH_OFF + H4_HEADER_LENGTH,
			GFP_KERNEL);
	if (!skb) {
		pr_err("Could not allocate skb");
		goto finished;
	}

	skb_reserve(skb, H4_HEADER_LENGTH);
	data = skb_put(skb, HCI_BT_LEN_VS_POWER_SWITCH_OFF);
	data[0] = MAKE_FIRST_BYTE_IN_CMD(HCI_BT_OCF_VS_POWER_SWITCH_OFF);
	data[1] = MAKE_SECOND_BYTE_IN_CMD(HCI_BT_OGF_VS,
					  HCI_BT_OCF_VS_POWER_SWITCH_OFF);
	data[2] = 0x06;
	/* Enter system specific GPIO settings here:
	 * Section data[3-5] is GPIO pull-up selection
	 * Section data[6-8] is GPIO pull-down selection
	 * Each section is a bitfield where
	 * - byte 0 bit 0 is GPIO 0
	 * - byte 0 bit 1 is GPIO 1
	 * - up to
	 * - byte 2 bit 4 which is GPIO 20
	 * where each bit means:
	 * - 0: No pull-up / no pull-down
	 * - 1: Pull-up / pull-down
	 * All GPIOs are set as input.
	 */
	data[3] = 0x00; /* GPIO 0-7 pull-up */
	data[4] = 0x00; /* GPIO 8-15 pull-up */
	data[5] = 0x00; /* GPIO 16-20 pull-up */
	data[6] = 0x00; /* GPIO 0-7 pull-down */
	data[7] = 0x00; /* GPIO 8-15 pull-down */
	data[8] = 0x00; /* GPIO 16-20 pull-down */

	if (op_lsb)
		*op_lsb = data[0];
	if (op_msb)
		*op_msb = data[1];

finished:
	return skb;
}
EXPORT_SYMBOL(cg2900_devices_get_power_switch_off_cmd);

struct sk_buff *cg2900_devices_get_bt_enable_cmd(u8 *op_lsb, u8 *op_msb,
						 bool bt_enable)
{
	struct sk_buff *skb = NULL;
	u8 *data = NULL;

	/* If connected chip does not support the command return NULL */
	if (CG2900_HCI_REV_PG_1 != cg2900_hci_revision &&
		CG2900_HCI_REV_PG_2 != cg2900_hci_revision &&
	    CG2900_SPECIAL_HCI_REV != cg2900_hci_revision)
		goto finished;

	/* CG2900 used */
	skb = alloc_skb(HCI_BT_LEN_VS_BT_ENABLE + H4_HEADER_LENGTH,
			GFP_KERNEL);
	if (!skb) {
		pr_err("Could not allocate skb");
		goto finished;
	}

	skb_reserve(skb, H4_HEADER_LENGTH);
	data = skb_put(skb, HCI_BT_LEN_VS_BT_ENABLE);
	data[0] = MAKE_FIRST_BYTE_IN_CMD(HCI_BT_OCF_VS_BT_ENABLE);
	data[1] = MAKE_SECOND_BYTE_IN_CMD(HCI_BT_OGF_VS,
					  HCI_BT_OCF_VS_BT_ENABLE);
	data[2] = 0x01;
	if (bt_enable)
		data[3] = 0x01;
	else
		data[3] = 0x00;

	if (op_lsb)
		*op_lsb = data[0];
	if (op_msb)
		*op_msb = data[1];

finished:
	return skb;
}
EXPORT_SYMBOL(cg2900_devices_get_bt_enable_cmd);

int cg2900_devices_init(void)
{
	int err = 0;
	printk(KERN_DEBUG "cg2900_devices_init");
	//s3c_setup_uart_cfg_gpio(1);
	err = gpio_request(GBF_ENA_RESET_GPIO, GBF_ENA_RESET_NAME);
	if (err < 0) {
		pr_err("gpio_request failed with err: %d", err);
		goto finished;
	}

	err = gpio_direction_output(GBF_ENA_RESET_GPIO, 1);
	if (err < 0) {
		pr_err("gpio_direction_output failed with err: %d", err);
		goto error_handling;
	}

	goto finished;

error_handling:
	gpio_free(GBF_ENA_RESET_GPIO);

finished:
	cg2900_devices_disable_chip();
	return err;
}
EXPORT_SYMBOL(cg2900_devices_init);

void cg2900_devices_exit(void)
{
	cg2900_devices_disable_chip();
	gpio_free(GBF_ENA_RESET_GPIO);
}
EXPORT_SYMBOL(cg2900_devices_exit);
