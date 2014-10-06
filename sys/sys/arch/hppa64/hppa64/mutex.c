/*	$OpenBSD: mutex.c,v 1.8 2012/06/05 11:43:41 jsing Exp $	*/

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

#include <ddb/db_output.h>

static inline int
try_lock(struct mutex *mtx)
{
	volatile int *lock = &mtx->mtx_lock;
	volatile register_t ret = 0;

	asm volatile (
		"ldcw,co 0(%2), %0"
		: "=&r" (ret), "+m" (lock)
		: "r" (lock)
	);

	return ret;
}

void
mtx_init(struct mutex *mtx, int wantipl)
{
	mtx->mtx_lock = MUTEX_UNLOCKED;
	mtx->mtx_wantipl = wantipl;
	mtx->mtx_oldipl = IPL_NONE;
}

void
mtx_enter(struct mutex *mtx)
{
	int s;

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

	mtx->mtx_lock = MUTEX_UNLOCKED;

	if (mtx->mtx_wantipl != IPL_NONE)
		splx(s);
}
