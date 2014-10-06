/*	$OpenBSD: mutex.h,v 1.5 2014/01/30 15:18:51 kettenis Exp $	*/

/*
 * Copyright (c) 2004 Artur Grabowski <art@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef _MACHINE_MUTEX_H_
#define _MACHINE_MUTEX_H_

#define	MUTEX_LOCKED	{ 0, 0, 0, 0 }
#define	MUTEX_UNLOCKED	{ 1, 1, 1, 1 }

/* Note: mtx_lock must be 16-byte aligned. */
struct mutex {
	volatile int mtx_lock[4];
	int mtx_wantipl;
	int mtx_oldipl;
	void *mtx_owner;
};

/*
 * To prevent lock ordering problems with the kernel lock, we need to
 * make sure we block all interrupts that can grab the kernel lock.
 * The simplest way to achieve this is to make sure mutexes always
 * raise the interrupt priority level to the highest level that has
 * interrupts that grab the kernel lock.
 */
#ifdef MULTIPROCESSOR
#define __MUTEX_IPL(ipl) \
    (((ipl) > IPL_NONE && (ipl) < IPL_AUDIO) ? IPL_AUDIO : (ipl))
#else
#define __MUTEX_IPL(ipl) (ipl)
#endif

#define MUTEX_INITIALIZER(ipl) { MUTEX_UNLOCKED, __MUTEX_IPL((ipl)), 0, NULL }

void __mtx_init(struct mutex *, int);
#define mtx_init(mtx, ipl) __mtx_init((mtx), __MUTEX_IPL((ipl)))

#ifdef DIAGNOSTIC
#define MUTEX_ASSERT_LOCKED(mtx) do {					\
	if ((mtx)->mtx_lock[0] == 1 && (mtx)->mtx_lock[1] == 1 &&	\
	    (mtx)->mtx_lock[2] == 1 && (mtx)->mtx_lock[3] == 1)		\
		panic("mutex %p not held in %s", (mtx), __func__);	\
} while (0)

#define MUTEX_ASSERT_UNLOCKED(mtx) do {					\
	if ((mtx)->mtx_lock[0] != 1 && (mtx)->mtx_lock[1] != 1 &&	\
	    (mtx)->mtx_lock[2] != 1 && (mtx)->mtx_lock[3] != 1)		\
		panic("mutex %p held in %s", (mtx), __func__);		\
} while (0)
#else
#define MUTEX_ASSERT_LOCKED(mtx) do { } while (0)
#define MUTEX_ASSERT_UNLOCKED(mtx) do { } while (0)
#endif

#define MUTEX_OLDIPL(mtx)	(mtx)->mtx_oldipl

#endif
