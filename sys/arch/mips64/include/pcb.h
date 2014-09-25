/*      $OpenBSD: pcb.h,v 1.6 2014/03/22 00:01:04 miod Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: Utah Hdr: pcb.h 1.13 89/04/23
 *	from: @(#)pcb.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _MIPS64_PCB_H_
#define _MIPS64_PCB_H_

#include <machine/frame.h>

/*
 * MIPS process control block. This is first in the U-area.
 */
struct pcb {
	struct trap_frame pcb_regs;	/* saved CPU and registers */
	struct {
		register_t val[13];
	} pcb_context;			/* kernel context for resume */
	int	pcb_onfault;		/* for copyin/copyout faults */
	void	*pcb_segtab;		/* copy of pmap pm_segtab */
	uint	pcb_nwired;		/* number of extra wired TLB entries */
	vaddr_t	pcb_wiredva;		/* va of above */
	vaddr_t	pcb_wiredpc;		/* last tracked pc value within above */
};

/*
 * The pcb is augmented with machine-dependent additional data for
 * core dumps. For the MIPS, there is nothing to add.
 */
struct md_coredump {
	long	md_pad[8];
};

#endif	/* !_MIPS64_PCB_H_ */
