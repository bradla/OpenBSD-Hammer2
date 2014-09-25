/*	$OpenBSD: intr.h,v 1.18 2014/03/29 18:09:30 guenther Exp $	*/
/* 	$NetBSD: intr.h,v 1.1 1998/08/18 23:55:00 matt Exp $	*/

/*
 * Copyright (c) 1998 Matt Thomas.
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
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MACHINE_INTR_H_
#define _MACHINE_INTR_H_

/* Define the various Interrupt Priority Levels */

/* Interrupt Priority Levels are not mutually exclusive. */

#define IPL_NONE	0x00
#define	IPL_SOFT	0x08
#define	IPL_SOFTCLOCK	0x09
#define	IPL_SOFTNET	0x0a
#define	IPL_SOFTTTY	0x0b
#define IPL_BIO		0x15	/* block I/O */
#define IPL_NET		0x15	/* network */
#define IPL_TTY		0x16	/* terminal */
#define IPL_VM		0x17	/* memory allocation */
#define	IPL_AUDIO	0x15	/* audio */
#define IPL_CLOCK	0x18	/* clock */
#define IPL_STATCLOCK	0x18	/* statclock */
#define	IPL_SCHED	0x1f
#define	IPL_HIGH	0x1f

#define	IPL_MPSAFE	0	/* no "mpsafe" interrupts */

#define	IST_UNUSABLE	-1	/* interrupt cannot be used */
#define	IST_NONE	0	/* none (dummy) */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

#define _splset(reg)						\
({								\
	register int val;					\
	__asm volatile ("mfpr $0x12,%0;mtpr %1,$0x12"		\
				: "=&g" (val)			\
				: "g" (reg));			\
	val;							\
})

#define	_splraise(reg)						\
({								\
	register int val;					\
	__asm volatile ("mfpr $0x12,%0"				\
				: "=&g" (val)			\
				: );				\
	if ((reg) > val) {					\
		__asm volatile ("mtpr %0,$0x12"			\
				:				\
				: "g" (reg));			\
	}							\
	val;							\
})

#define	splx(reg)						\
	__asm volatile ("mtpr %0,$0x12" : : "g" (reg))

#define	spl0()		_splset(IPL_NONE)
#define splsoftclock()	_splraise(IPL_SOFTCLOCK)
#define splsoftnet()	_splraise(IPL_SOFTNET)
#define splbio()	_splraise(IPL_BIO)
#define splnet()	_splraise(IPL_NET)
#define spltty()	_splraise(IPL_TTY)
#define splvm()		_splraise(IPL_VM)
#define splaudio()	_splraise(IPL_AUDIO)
#define splclock()	_splraise(IPL_CLOCK)
#define splstatclock()	_splraise(IPL_STATCLOCK)
#define splhigh()	_splset(IPL_HIGH)
#define	splsched()	splhigh()

/* These are better to use when playing with VAX buses */
#define	spl4()		_splraise(0x14)
#define	spl5()		_splraise(0x15)
#define	spl6()		_splraise(0x16)
#define	spl7()		_splraise(0x17)

/* SPL asserts */
#ifdef DIAGNOSTIC
/*
 * Although this function is implemented in MI code, it must be in this MD
 * header because we don't want this header to include MI includes.
 */
void splassert_fail(int, int, const char *);
extern int splassert_ctl;
void splassert_check(int, const char *);
#define splassert(__wantipl) do {			\
	if (splassert_ctl > 0) {			\
		splassert_check(__wantipl, __func__);	\
	}						\
} while (0)
#define splsoftassert(wantipl) splassert(wantipl)
#else
#define	splassert(wantipl)	do { /* nothing */ } while (0)
#define	splsoftassert(wantipl)	do { /* nothing */ } while (0)
#endif

#define	SI_SOFT			0	/* for IPL_SOFT */
#define	SI_SOFTCLOCK		1	/* for IPL_SOFTCLOCK */
#define	SI_SOFTNET		2	/* for IPL_SOFTNET */
#define	SI_SOFTTTY		3	/* for IPL_SOFTTTY */

#define	SI_NQUEUES		4

#ifndef _LOCORE

#include <machine/mutex.h>
#include <sys/queue.h>

struct soft_intrhand {
	TAILQ_ENTRY(soft_intrhand) sih_list;
	void (*sih_func)(void *);
	void *sih_arg;
	struct soft_intrq *sih_siq;
	int sih_pending;
};

struct soft_intrq {
	TAILQ_HEAD(, soft_intrhand) siq_list;
	int siq_si;
	struct mutex siq_mtx;
};

void	 softintr_disestablish(void *);
void	*softintr_establish(int, void (*)(void *), void *);
void	 softintr_init(void);
void	 softintr_schedule(void *);

#endif	/* _LOCORE */

#endif	/* _VAX_INTR_H */
