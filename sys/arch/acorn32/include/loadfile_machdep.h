/*	$NetBSD: loadfile_machdep.h,v 1.5 2009/03/14 14:45:51 dsl Exp $	*/

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas and Jason R. Thorpe.
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

#ifndef _ARM32_LOADFILE_MACHDEP_H_
#define _ARM32_LOADFILE_MACHDEP_H_

#define BOOT_ELF32

#define LOAD_KERNEL		(LOAD_ALL & ~LOAD_TEXTA)
#define COUNT_KERNEL		(COUNT_ALL & ~COUNT_TEXTA)

#ifdef _STANDALONE

#define LOADADDR(a)		(((u_long)(a)) + offset)
/* ALIGNENTRY is only for a.out. */
#define READ(f, b, c)		boot32_read((f), (void *)LOADADDR(b), (c))
#define	BCOPY(s, d, c)		boot32_memcpy((void *)LOADADDR(d), (s), (c))
#define	BZERO(d, c)		boot32_memset((void *)LOADADDR(d), 0, (c))
#define	WARN(a)			(void)(printf a, \
				    printf((errno ? ": %s\n" : "\n"), \
				    strerror(errno)))
#define	PROGRESS(a)		(void) printf a
#define	ALLOC(a)		alloc(a)
#define	DEALLOC(a, b)		dealloc(a, b)
/* OKMAGIC is only for a.out. */

extern ssize_t boot32_read(int, void *, size_t);
extern void *boot32_memcpy(void *, const void *, size_t);
extern void *boot32_memset(void *, int, size_t);

#else

#define	LOADADDR(a)		(((u_long)(a)) + offset)
#define	ALIGNENTRY(a)		((u_long)(a))
#define	READ(f, b, c)		read((f), (void *)LOADADDR(b), (c))
#define	BCOPY(s, d, c)		memcpy((void *)LOADADDR(d), (void *)(s), (c))
#define	BZERO(d, c)		memset((void *)LOADADDR(d), 0, (c))
#define	WARN(a)			warn a
#define	PROGRESS(a)		/* nothing */
#define	ALLOC(a)		malloc(a)
#define	DEALLOC(a, b)		free(a)
#define	OKMAGIC(a)		((a) == OMAGIC)

ssize_t vread(int, u_long, u_long *, size_t);
void vcopy(u_long, u_long, u_long *, size_t);
void vzero(u_long, u_long *, size_t);

#endif
#endif

/* end of loadfile_machdep.h */
