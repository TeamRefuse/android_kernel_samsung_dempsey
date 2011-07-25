#ifndef __LINUX_TAOS_H
#define __LINUX_TAOS_H

#include <linux/types.h>

#ifdef __KERNEL__
#define TAOS_OPT "taos-opt"
struct taos_platform_data {
	//int p_out;  /* proximity-sensor-output gpio */
	int als_int;
	int (*power)(bool); /* power to the chip */
	int (*light_adc_value)(void); /* get light level from adc */
};
#endif /* __KERNEL__ */

#endif

