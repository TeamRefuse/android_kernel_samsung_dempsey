/*
 * Android related remote wl declarations
 *
 * Copyright (C) 1999-2011, Broadcom Corporation
 * 
 *         Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: bcm_android_types.h,v 13.1 2009/08/07 06:58:34 Exp $
 *
 */
#ifndef _wlu_android_h
#define _wlu_android_h
#define  __fd_mask unsigned long
typedef struct
	{
	/* XPG4.2 requires this member name.  Otherwise avoid the name
	 * from the global namespace.
	 */
#ifdef __USE_XOPEN
    __fd_mask fds_bits[__FD_SETSIZE / __NFDBITS];
# define __FDS_BITS(set) ((set)->fds_bits)
#else
    __fd_mask __fds_bits[__FD_SETSIZE / __NFDBITS];
# define __FDS_BITS(set) ((set)->__fds_bits)
#endif
	} fd_set1;
#define fd_set fd_set1

#define htons(x) BCMSWAP16(x)
#define htonl(x) BCMSWAP32(x)
#define __FD_ZERO(s) \
	do {                                                                        \
    unsigned int __i;                                                         \
    fd_set *__arr = (s);                                                      \
    for (__i = 0; __i < sizeof (fd_set) / sizeof (__fd_mask); ++__i)          \
	__FDS_BITS(__arr)[__i] = 0;                                            \
	} while (0)
#define __FD_SET(d, s)     (__FDS_BITS (s)[__FDELT(d)] |= __FDMASK(d))
#define __FD_CLR(d, s)     (__FDS_BITS (s)[__FDELT(d)] &= ~__FDMASK(d))
#define __FD_ISSET(d, s)   ((__FDS_BITS (s)[__FDELT(d)] & __FDMASK(d)) != 0)
#define MCL_CURRENT 1
#define MCL_FUTURE 2
#endif /* _wlu_android_h */
