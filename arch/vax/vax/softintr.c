/*	$OpenBSD: softintr.c,v 1.2 2010/12/21 14:56:24 claudio Exp $	*/
/*	$NetBSD: softintr.c,v 1.2 2003/07/15 00:24:39 lukem Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
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

#include <sys/param.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/atomic.h>
#include <machine/intr.h>
#include <machine/mtpr.h>

void	softintr_dispatch(void);

struct soft_intrq soft_intrq[SI_NQUEUES];

struct soft_intrhand *softnet_intrhand;

void	netintr(void);

/*
 * Initialize the software interrupt system.
 */
void
softintr_init(void)
{
	struct soft_intrq *siq;
	int i;

	for (i = 0; i < SI_NQUEUES; i++) {
		siq = &soft_intrq[i];
		TAILQ_INIT(&siq->siq_list);
		siq->siq_si = IPL_SOFT + i;
		mtx_init(&siq->siq_mtx, IPL_HIGH);
	}

	/* XXX Establish legacy software interrupt handlers. */
	softnet_intrhand = softintr_establish(IPL_SOFTNET,
	    (void (*)(void *))netintr, NULL);
}

/*
 * Process pending software interrupts.  The corresponding queue is
 * computed from the current interrupt level.
 */
void
softintr_dispatch()
{
	struct soft_intrq *siq;
	struct soft_intrhand *sih;
	int si;

	si = mfpr(PR_IPL) - IPL_SOFT;
	siq = &soft_intrq[si];

	for (;;) {
		mtx_enter(&siq->siq_mtx);
		sih = TAILQ_FIRST(&siq->siq_list);
		if (sih == NULL) {
			mtx_leave(&siq->siq_mtx);
			break;
		}

		TAILQ_REMOVE(&siq->siq_list, sih, sih_list);
		sih->sih_pending = 0;

		uvmexp.softs++;

		mtx_leave(&siq->siq_mtx);

		(*sih->sih_func)(sih->sih_arg);
	}
}

/*
 * Register a software interrupt handler.
 */
void *
softintr_establish(int ipl, void (*func)(void *), void *arg)
{
	struct soft_intrhand *sih;
	int si;

	si = ipl - IPL_SOFT;
	if (si < 0 || si > SI_NQUEUES) {
		printf("softintr_establish: unknown soft IPL %d\n", ipl);
		return NULL;
	}

	sih = malloc(sizeof(*sih), M_DEVBUF, M_NOWAIT);
	if (__predict_true(sih != NULL)) {
		sih->sih_func = func;
		sih->sih_arg = arg;
		sih->sih_siq = &soft_intrq[si];
		sih->sih_pending = 0;
	}
	return (sih);
}

/*
 * Unregister a software interrupt handler.
 */
void
softintr_disestablish(void *arg)
{
	struct soft_intrhand *sih = arg;
	struct soft_intrq *siq = sih->sih_siq;

	mtx_enter(&siq->siq_mtx);
	if (sih->sih_pending) {
		TAILQ_REMOVE(&siq->siq_list, sih, sih_list);
		sih->sih_pending = 0;
	}
	mtx_leave(&siq->siq_mtx);

	free(sih, M_DEVBUF);
}

/*
 * Schedule a software interrupt.
 */
void
softintr_schedule(void *arg)
{
	struct soft_intrhand *sih = (struct soft_intrhand *)arg;
	struct soft_intrq *siq = sih->sih_siq;

	mtx_enter(&siq->siq_mtx);
	if (sih->sih_pending == 0) {
		TAILQ_INSERT_TAIL(&siq->siq_list, sih, sih_list);
		sih->sih_pending = 1;
		mtpr(siq->siq_si, PR_SIRR);
	}
	mtx_leave(&siq->siq_mtx);
}
