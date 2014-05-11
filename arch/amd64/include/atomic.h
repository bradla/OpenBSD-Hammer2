/*	$OpenBSD: atomic.h,v 1.12 2014/03/29 18:09:28 guenther Exp $	*/
/*	$NetBSD: atomic.h,v 1.1 2003/04/26 18:39:37 fvdl Exp $	*/

/*
 * Copyright 2002 (c) Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MACHINE_ATOMIC_H_
#define _MACHINE_ATOMIC_H_

/*
 * Perform atomic operations on memory. Should be atomic with respect
 * to interrupts and multiple processors.
 *
 * void atomic_setbits_int(volatile u_int *a, u_int mask) { *a |= mask; }
 * void atomic_clearbits_int(volatile u_int *a, u_int mas) { *a &= ~mask; }
 */

#if defined(_KERNEL) && !defined(_LOCORE)

#ifdef MULTIPROCESSOR
#define LOCK "lock"
#else
#define LOCK
#endif

static inline unsigned int
_atomic_cas_uint(volatile unsigned int *p, unsigned int e, unsigned int n)
{
	__asm volatile(LOCK " cmpxchgl %2, %1"
	    : "=a" (n), "=m" (*p)
	    : "r" (n), "a" (e), "m" (*p)
	    : "memory");

	return (n);
}
#define atomic_cas_uint(_p, _e, _n) _atomic_cas_uint((_p), (_e), (_n))

static inline unsigned long
_atomic_cas_ulong(volatile unsigned long *p, unsigned long e, unsigned long n)
{
	__asm volatile(LOCK " cmpxchgq %2, %1"
	    : "=a" (n), "=m" (*p)
	    : "r" (n), "a" (e), "m" (*p)
	    : "memory");

	return (n);
}
#define atomic_cas_ulong(_p, _e, _n) _atomic_cas_ulong((_p), (_e), (_n))

static inline void *
_atomic_cas_ptr(volatile void **p, void *e, void *n)
{
	__asm volatile(LOCK " cmpxchgq %2, %1"
	    : "=a" (n), "=m" (*p)
	    : "r" (n), "a" (e), "m" (*p)
	    : "memory");

	return (n);
}
#define atomic_cas_ptr(_p, _e, _n) _atomic_cas_ptr((_p), (_e), (_n))

static __inline u_int64_t
x86_atomic_testset_u64(volatile u_int64_t *ptr, u_int64_t val)
{
	__asm__ volatile ("xchgq %0,(%2)" :"=r" (val):"0" (val),"r" (ptr));
	return val;
}

static __inline u_int32_t
x86_atomic_testset_u32(volatile u_int32_t *ptr, u_int32_t val)
{
	__asm__ volatile ("xchgl %0,(%2)" :"=r" (val):"0" (val),"r" (ptr));
	return val;
}


static __inline void
x86_atomic_setbits_u32(volatile u_int32_t *ptr, u_int32_t bits)
{
	__asm volatile(LOCK " orl %1,%0" :  "=m" (*ptr) : "ir" (bits));
}

static __inline void
x86_atomic_clearbits_u32(volatile u_int32_t *ptr, u_int32_t bits)
{
	__asm volatile(LOCK " andl %1,%0" :  "=m" (*ptr) : "ir" (~bits));
}

/*
 * XXX XXX XXX
 * theoretically 64bit cannot be used as
 * an "i" and thus if we ever try to give
 * these anything from the high dword there
 * is an asm error pending
 */
static __inline void
x86_atomic_setbits_u64(volatile u_int64_t *ptr, u_int64_t bits)
{
	__asm volatile(LOCK " orq %1,%0" :  "=m" (*ptr) : "ir" (bits));
}

static __inline void
x86_atomic_clearbits_u64(volatile u_int64_t *ptr, u_int64_t bits)
{
	__asm volatile(LOCK " andq %1,%0" :  "=m" (*ptr) : "ir" (~bits));
}

#define x86_atomic_testset_ul	x86_atomic_testset_u64
#define x86_atomic_setbits_ul	x86_atomic_setbits_u64
#define x86_atomic_clearbits_ul	x86_atomic_clearbits_u64

#define atomic_setbits_int x86_atomic_setbits_u32
#define atomic_clearbits_int x86_atomic_clearbits_u32

#undef LOCK

#endif /* defined(_KERNEL) && !defined(_LOCORE) */
#endif /* _MACHINE_ATOMIC_H_ */
