/*	$OpenBSD: param.h,v 1.14 2013/11/20 23:57:07 miod Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 */

#ifndef	_MACHINE_PARAM_H_
#define	_MACHINE_PARAM_H_

#ifdef _KERNEL				/* XXX */
#ifndef _LOCORE				/* XXX */
#include <machine/cpu.h>		/* XXX */
#endif					/* XXX */
#endif					/* XXX */

#define	_MACHINE	solbourne
#define	MACHINE		"solbourne"
#define	_MACHINE_ARCH	sparc
#define	MACHINE_ARCH	"sparc"
#define	MID_MACHINE	MID_SPARC

#define SUN4_PGSHIFT	13			/* for a sun4 machine */
#define	SUN4CM_PGSHIFT	12			/* for a sun4c or sun4m machine */
#define	PAGE_SHIFT	SUN4_PGSHIFT
#define	PAGE_SIZE	(1 << PAGE_SHIFT)
#define	PAGE_MASK	(PAGE_SIZE - 1)

#define	KERNBASE	0xfd080000

#ifdef _KERNEL

#define	KERNTEXTOFF	0xfd084000	/* start of kernel text */
#define	MSGBUF_PA	PTW1_TO_PHYS(KERNBASE)	/* msgbuf physical address */

#define	NBPG		PAGE_SIZE		/* bytes/page */
#define	PGSHIFT		PAGE_SHIFT		/* LOG2(PAGE_SIZE) */
#define	PGOFSET		PAGE_MASK		/* byte offset into page */

#define UPAGES		1
#define	USPACE		8192		/* total size of u-area */
#define	USPACE_ALIGN	0		/* u-area alignment 0-none */

#define	NMBCLUSTERS	2048		/* map size, max cluster allocation */

#define	MSGBUFSIZE	(1 * PAGE_SIZE)		/* larger than on sparc! */

/*
 * Maximum size of the kernel malloc arena in PAGE_SIZE-sized
 * logical pages.
 */
#define	NKMEMPAGES_MAX_DEFAULT	((64 * 1024 * 1024) >> PAGE_SHIFT)

/*
 * dvmamap manages a range of DVMA addresses intended to create double
 * mappings of physical memory. In a way, `dvmamap' is a submap of the
 * VM map `phys_map'. The difference is the use of the `extent'
 * routines to manage page allocation, allowing DVMA addresses to be
 * allocated and freed from within interrupt routines.
 *
 * Note that `phys_map' can still be used to allocate memory-backed pages
 * in DVMA space.
 */
#ifndef _LOCORE
extern vaddr_t		dvma_base;
extern vaddr_t		dvma_end;
extern struct extent	*dvmamap_extent;

extern caddr_t	kdvma_mapin(caddr_t, int, int);
extern caddr_t	dvma_malloc_space(size_t, void *, int, int);
extern void	dvma_free(caddr_t, size_t, void *);
#define		dvma_malloc(len,kaddr,flags)	dvma_malloc_space(len,kaddr,flags,0)

extern void	delay(unsigned int);
#define	DELAY(n)	delay(n)

extern int cputyp;

#endif /* _LOCORE */

/*
 * Values for the cputyp variable.
 */
#define	CPU_KAP		5

/*
 * Shorthand CPU-type macros.
 * Let compiler optimize away code conditional on constants.
 */
#define CPU_ISSUN4M	(0)
#define CPU_ISSUN4C	(0)
#define CPU_ISSUN4	(0)
#define CPU_ISSUN4OR4C	(0)
#define CPU_ISSUN4COR4M	(0)
#define	CPU_ISKAP	(1)

#endif /* _KERNEL */

#endif /* _MACHINE_PARAM_H_ */
