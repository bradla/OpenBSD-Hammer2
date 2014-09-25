/*	$OpenBSD: mutex.c,v 1.6 2013/12/26 21:02:37 miod Exp $	*/

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

#include <sys/param.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <machine/intr.h>

static inline int
try_lock(struct mutex *mtx)
{
#ifdef MULTIPROCESSOR
	int tmp, ret = 0;

        asm volatile (
		".set noreorder\n"
		"1:\n"
		"ll	%0, %2\n"		/* tmp = mtx->mtx_lock */
		"bnez	%0, 2f\n"
		" li	%1, 0\n"		/* ret = 0 */
		"li	%1, 1\n"		/* ret = 1 */
		"sc	%1, %2\n"		/* mtx->mtx_lock = 1 */
		"beqz	%1, 1b\n"		/* update failed */
		" nop\n"
		"2:\n"
		".set reorder\n"
		: "+r"(tmp), "+r"(ret)
		: "m"(mtx->mtx_lock));
	
	return ret;
#else  /* MULTIPROCESSOR */
	mtx->mtx_lock = 1;
	return 1;
#endif /* MULTIPROCESSOR */
}

void
mtx_init(struct mutex *mtx, int wantipl)
{
	mtx->mtx_lock = 0;
	mtx->mtx_wantipl = wantipl;
	mtx->mtx_oldipl = IPL_NONE;
}

void
mtx_enter(struct mutex *mtx)
{
	int s;
	int i = 10000000;
	for (;;) {
		if (mtx->mtx_wantipl != IPL_NONE)
			s = splraise(mtx->mtx_wantipl);
		if (try_lock(mtx)) {
			if (mtx->mtx_wantipl != IPL_NONE)
				mtx->mtx_oldipl = s;
			mtx->mtx_owner = curcpu();
#ifdef DIAGNOSTIC
			curcpu()->ci_mutex_level++;
#endif
			return;
		}
		if (mtx->mtx_wantipl != IPL_NONE)
			splx(s);
		if(i-- <= 0)
			panic("mtx_enter timed out\n");
	}
}

int
mtx_enter_try(struct mutex *mtx)
{
	int s;
	
 	if (mtx->mtx_wantipl != IPL_NONE)
		s = splraise(mtx->mtx_wantipl);
	if (try_lock(mtx)) {
		if (mtx->mtx_wantipl != IPL_NONE)
			mtx->mtx_oldipl = s;
		mtx->mtx_owner = curcpu();
#ifdef DIAGNOSTIC
		curcpu()->ci_mutex_level++;
#endif
		return 1;
	}
	if (mtx->mtx_wantipl != IPL_NONE)
		splx(s);
	return 0;
}

void
mtx_leave(struct mutex *mtx)
{
	int s;

	MUTEX_ASSERT_LOCKED(mtx);
#ifdef DIAGNOSTIC
	curcpu()->ci_mutex_level--;
#endif
	s = mtx->mtx_oldipl;
	mtx->mtx_owner = NULL;
	mtx->mtx_lock = 0;
	if (mtx->mtx_wantipl != IPL_NONE)
		splx(s);
}
