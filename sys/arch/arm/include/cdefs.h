/*	$NetBSD: cdefs.h,v 1.12 2014/01/29 01:20:03 matt Exp $	*/

#ifndef	_ARM_CDEFS_H_
#define	_ARM_CDEFS_H_

#if (__GNUC__ == 4 && __GNUC_MINOR__ < 1) || __GNUC__ < 4
#error GCC 4.1 or compatible required.
#endif

#if defined (__ARM_ARCH_8A__)
#define _ARM_ARCH_8		/* ARMv8 64-bit in AARCH32 */
#endif

#if defined (_ARM_ARCH_8) || defined (__ARM_ARCH_7__) || \
    defined (__ARM_ARCH_7A__) || defined (__ARM_ARCH_7R__) || \
    defined (__ARM_ARCH_7M__) || defined (__ARM_ARCH_7EM__)
	/* 7R, 7M, 7EM are for non MMU arms */
#define _ARM_ARCH_7
#endif

#if defined (_ARM_ARCH_7) || defined (__ARM_ARCH_6T2__)
#define _ARM_ARCH_T2		/* Thumb2 */
#endif

#if defined (_ARM_ARCH_T2) || defined (__ARM_ARCH_6__) || \
    defined (__ARM_ARCH_6J__) || defined (__ARM_ARCH_6K__) || \
    defined (__ARM_ARCH_6Z__) || defined (__ARM_ARCH_6ZK__) || \
    defined (__ARM_ARCH_6ZM__)
#define _ARM_ARCH_6
#endif

#if defined (_ARM_ARCH_6) || defined (__ARM_ARCH_5T__) || \
    defined (__ARM_ARCH_5TE__) || defined (__ARM_ARCH_5TEJ__)
#define _ARM_ARCH_5T
#endif

#if defined (_ARM_ARCH_6) || defined (_ARM_ARCH_5T) || defined (__ARM_ARCH_5__)
#define _ARM_ARCH_5
#endif

#if defined (_ARM_ARCH_5) || defined (__ARM_ARCH_4T__)
#define _ARM_ARCH_4T
#endif

#if defined (_ARM_ARCH_T2) || \
    (!defined (__thumb__) && \
     (defined (_ARM_ARCH_6) || defined (__ARM_ARCH_5TE__) || \
      defined (__ARM_ARCH_5TEJ__)))
#define	_ARM_ARCH_DWORD_OK
#endif

#if defined(__ARM_EABI__) || defined(__ARM_PCS_AAPCS64)
#define __ALIGNBYTES		(8 - 1)
#else
#define __ALIGNBYTES		(sizeof(int) - 1)
#endif

#endif /* !_ARM_CDEFS_H_ */
