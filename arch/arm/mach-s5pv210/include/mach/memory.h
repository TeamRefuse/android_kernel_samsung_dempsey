/* linux/arch/arm/mach-s5pv210/include/mach/memory.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * S5PV210 - Memory definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#if defined(CONFIG_MACH_SMDKC110) || defined (CONFIG_MACH_S5PC110_ARIES) || defined (CONFIG_MACH_S5PC110_P1)
#define PHYS_OFFSET		UL(0x30000000)
#else
#define PHYS_OFFSET		UL(0x20000000)
#endif
#define CONSISTENT_DMA_SIZE	(SZ_8M + SZ_4M + SZ_2M)

/* Maximum of 256MiB in one bank */
#define MAX_PHYSMEM_BITS	32
#define SECTION_SIZE_BITS	28
#if defined(CONFIG_MACH_SMDKC110) || defined (CONFIG_MACH_S5PC110_ARIES) || defined (CONFIG_MACH_S5PC110_P1)
#define NODE_MEM_SIZE_BITS   28
#endif

#endif /* __ASM_ARCH_MEMORY_H */
