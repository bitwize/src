/*	$NetBSD: atomic_init_testset.c,v 1.12 2014/01/29 14:49:35 martin Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * libc glue for atomic operations where the hardware does not provide
 * compare-and-swap.  It's assumed that this will only be used on 32-bit
 * platforms.
 *
 * This should be compiled with '-fno-reorder-blocks -fomit-frame-pointer'
 * if using gcc.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: atomic_init_testset.c,v 1.12 2014/01/29 14:49:35 martin Exp $");

#include "atomic_op_namespace.h"

#include <sys/types.h>
#include <sys/atomic.h>
#include <sys/lock.h>
#include <sys/ras.h>
#include <sys/sysctl.h>

#include <string.h>

#define	I2	__SIMPLELOCK_UNLOCKED, __SIMPLELOCK_UNLOCKED,
#define	I16	I2 I2 I2 I2 I2 I2 I2 I2
#define	I128	I16 I16 I16 I16 I16 I16 I16 I16

static __cpu_simple_lock_t atomic_locks[128] = { I128 };

#ifdef	__HAVE_ASM_ATOMIC_CAS_UP
extern uint32_t _atomic_cas_up(volatile uint32_t *, uint32_t, uint32_t);
#else
static uint32_t _atomic_cas_up(volatile uint32_t *, uint32_t, uint32_t);
#endif
static uint32_t (*_atomic_cas_fn)(volatile uint32_t *, uint32_t, uint32_t) =
    _atomic_cas_up;
RAS_DECL(_atomic_cas);

#ifdef	__HAVE_ASM_ATOMIC_CAS_16_UP
extern uint16_t _atomic_cas_16_up(volatile uint16_t *, uint16_t, uint16_t);
#else
static uint16_t _atomic_cas_16_up(volatile uint16_t *, uint16_t, uint16_t);
#endif
static uint16_t (*_atomic_cas_16_fn)(volatile uint16_t *, uint16_t, uint16_t) =
    _atomic_cas_16_up;
RAS_DECL(_atomic_cas_16);

#ifdef	__HAVE_ASM_ATOMIC_CAS_8_UP
extern uint8_t _atomic_cas_8_up(volatile uint8_t *, uint8_t, uint8_t);
#else
static uint8_t _atomic_cas_8_up(volatile uint8_t *, uint8_t, uint8_t);
#endif
static uint8_t (*_atomic_cas_8_fn)(volatile uint8_t *, uint8_t, uint8_t) =
    _atomic_cas_8_up;
RAS_DECL(_atomic_cas_8);

void	__libc_atomic_init(void) __attribute__ ((visibility("hidden")));

#ifndef	__HAVE_ASM_ATOMIC_CAS_UP
static uint32_t
_atomic_cas_up(volatile uint32_t *ptr, uint32_t old, uint32_t new)
{
	uint32_t ret;

	RAS_START(_atomic_cas);
	ret = *ptr;
	if (__predict_false(ret != old)) {
		return ret;
	}
	*ptr = new;
	RAS_END(_atomic_cas);

	return ret;
}
#endif

#ifndef	__HAVE_ASM_ATOMIC_CAS_16_UP
static uint16_t
_atomic_cas_16_up(volatile uint16_t *ptr, uint16_t old, uint16_t new)
{
	uint16_t ret;

	RAS_START(_atomic_cas_16);
	ret = *ptr;
	if (__predict_false(ret != old)) {
		return ret;
	}
	*ptr = new;
	RAS_END(_atomic_cas_16);

	return ret;
}
#endif

#ifndef	__HAVE_ASM_ATOMIC_CAS_8_UP
static uint8_t
_atomic_cas_8_up(volatile uint8_t *ptr, uint8_t old, uint8_t new)
{
	uint8_t ret;

	RAS_START(_atomic_cas_8);
	ret = *ptr;
	if (__predict_false(ret != old)) {
		return ret;
	}
	*ptr = new;
	RAS_END(_atomic_cas_8);

	return ret;
}
#endif

static uint32_t
_atomic_cas_mp(volatile uint32_t *ptr, uint32_t old, uint32_t new)
{
	__cpu_simple_lock_t *lock;
	uint32_t ret;

	lock = &atomic_locks[((uintptr_t)ptr >> 3) & 127];
	__cpu_simple_lock(lock);
	ret = *ptr;
	if (__predict_true(ret == old)) {
		*ptr = new;
	}
	__cpu_simple_unlock(lock);

	return ret;
}

uint32_t
_atomic_cas_32(volatile uint32_t *ptr, uint32_t old, uint32_t new)
{

	return (*_atomic_cas_fn)(ptr, old, new);
}

uint16_t _atomic_cas_16(volatile uint16_t *, uint16_t, uint16_t);

uint16_t
_atomic_cas_16(volatile uint16_t *ptr, uint16_t old, uint16_t new)
{

	return (*_atomic_cas_16_fn)(ptr, old, new);
}

uint8_t _atomic_cas_8(volatile uint8_t *, uint8_t, uint8_t);

uint8_t
_atomic_cas_8(volatile uint8_t *ptr, uint8_t old, uint8_t new)
{

	return (*_atomic_cas_8_fn)(ptr, old, new);
}

void __section(".text.startup")
__libc_atomic_init(void)
{
	int ncpu, mib[2];
	size_t len;

	_atomic_cas_fn = _atomic_cas_mp;

	mib[0] = CTL_HW;
	mib[1] = HW_NCPU; 
	len = sizeof(ncpu);
	if (sysctl(mib, 2, &ncpu, &len, NULL, 0) == -1)
		return;
	if (ncpu > 1)
		return;
	if (rasctl(RAS_ADDR(_atomic_cas), RAS_SIZE(_atomic_cas),
	    RAS_INSTALL) == 0) {
		_atomic_cas_fn = _atomic_cas_up;
		return;
	}

	if (rasctl(RAS_ADDR(_atomic_cas_16), RAS_SIZE(_atomic_cas_16),
	    RAS_INSTALL) == 0) {
		_atomic_cas_16_fn = _atomic_cas_16_up;
		return;
	}

	if (rasctl(RAS_ADDR(_atomic_cas_8), RAS_SIZE(_atomic_cas_8),
	    RAS_INSTALL) == 0) {
		_atomic_cas_8_fn = _atomic_cas_8_up;
		return;
	}
}

#undef atomic_cas_32
#undef atomic_cas_uint
#undef atomic_cas_ulong
#undef atomic_cas_ptr
#undef atomic_cas_32_ni
#undef atomic_cas_uint_ni
#undef atomic_cas_ulong_ni
#undef atomic_cas_ptr_ni

atomic_op_alias(atomic_cas_32,_atomic_cas_32)
atomic_op_alias(atomic_cas_uint,_atomic_cas_32)
__strong_alias(_atomic_cas_uint,_atomic_cas_32)
atomic_op_alias(atomic_cas_ulong,_atomic_cas_32)
__strong_alias(_atomic_cas_ulong,_atomic_cas_32)
atomic_op_alias(atomic_cas_ptr,_atomic_cas_32)
__strong_alias(_atomic_cas_ptr,_atomic_cas_32)

atomic_op_alias(atomic_cas_32_ni,_atomic_cas_32)
__strong_alias(_atomic_cas_32_ni,_atomic_cas_32)
atomic_op_alias(atomic_cas_uint_ni,_atomic_cas_32)
__strong_alias(_atomic_cas_uint_ni,_atomic_cas_32)
atomic_op_alias(atomic_cas_ulong_ni,_atomic_cas_32)
__strong_alias(_atomic_cas_ulong_ni,_atomic_cas_32)
atomic_op_alias(atomic_cas_ptr_ni,_atomic_cas_32)
__strong_alias(_atomic_cas_ptr_ni,_atomic_cas_32)

__strong_alias(__sync_val_compare_and_swap_4,_atomic_cas_32)
__strong_alias(__sync_val_compare_and_swap_2,_atomic_cas_16)
__strong_alias(__sync_val_compare_and_swap_1,_atomic_cas_8)
