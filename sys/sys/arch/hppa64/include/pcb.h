/*	$OpenBSD: pcb.h,v 1.4 2011/11/10 23:15:11 deraadt Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _MACHINE_PCB_H_
#define _MACHINE_PCB_H_

#include <machine/fpu.h>
#include <machine/reg.h>

struct pcb {
	struct hppa_fpstate *pcb_fpstate;	/* not in the trapframe */

	u_int64_t	pcb_ksp;		/* kernel sp for ctxsw */
	u_int64_t	pcb_onfault;		/* SW copy fault handler */
	pa_space_t	pcb_space;		/* copy pmap_space, for asm's sake */
};

struct md_coredump {
	struct reg md_reg;
	struct fpreg md_fpreg;
}; 


#endif /* _MACHINE_PCB_H_ */
