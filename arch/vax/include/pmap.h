/*      $OpenBSD: pmap.h,v 1.38 2014/01/30 18:16:41 miod Exp $     */
/*	$NetBSD: pmap.h,v 1.37 1999/08/01 13:48:07 ragge Exp $	   */

/* 
 * Copyright (c) 1987 Carnegie-Mellon University
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * Changed for the VAX port. /IC
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	@(#)pmap.h	7.6 (Berkeley) 5/10/91
 */


#ifndef _MACHINE_PMAP_H_
#define _MACHINE_PMAP_H_

#ifdef _KERNEL

#include <machine/pte.h>
#include <machine/mtpr.h>
#include <machine/pcb.h>

/*
 * Some constants to make life easier.
 */
#define LTOHPS		(PAGE_SHIFT - VAX_PGSHIFT)
#define LTOHPN		(1 << LTOHPS)

/*
 * Pmap structure
 */

struct pmap {
	pt_entry_t	*pm_p1ap;	/* Base of alloced p1 pte space */
	u_int		 pm_count;	/* reference count */
	struct pcb	*pm_pcbs;	/* PCBs using this pmap */
	pt_entry_t	*pm_p0br;	/* page 0 base register */
	u_long		 pm_p0lr;	/* page 0 length register */
	pt_entry_t	*pm_p1br;	/* page 1 base register */
	u_long		 pm_p1lr;	/* page 1 length register */
	struct pmap_statistics	 pm_stats;	/* Some statistics */
};

typedef struct pmap *pmap_t;

/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry_t.
 */

struct pv_entry {
	struct pv_entry *pv_next;	/* next pv_entry */
	struct pmap	*pv_pmap;	/* pmap this entry belongs to */
	vaddr_t		 pv_va;		/* address of the virtual mapping */
};

extern	struct pmap kernel_pmap_store;

#define pmap_kernel()			(&kernel_pmap_store)

/*
 * Real nice (fast) routines to get the virtual address of a physical page
 * (and vice versa).
 */
#define pmap_map_direct(pg)	(VM_PAGE_TO_PHYS(pg) | KERNBASE)
#define pmap_unmap_direct(va)	PHYS_TO_VM_PAGE((va) & ~KERNBASE)
#define	__HAVE_PMAP_DIRECT

#define PMAP_STEAL_MEMORY

/*
 * This is the by far most used pmap routine. Make it inline.
 */

/* Routines that are best to define as macros */
#define pmap_copy(a,b,c,d,e)		/* Dont do anything */
#define pmap_collect(pm)		/* nothing */
#define pmap_update(pm)			/* nothing */
#define pmap_remove(pmap, start, slut)	pmap_protect(pmap, start, slut, 0)
#define pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)
#define pmap_wired_count(pmap)		((pmap)->pm_stats.wired_count)
#define pmap_reference(pmap)		(pmap)->pm_count++

/* These can be done as efficient inline macros */
#define pmap_copy_page(srcpg, dstpg) do {				\
	paddr_t __src = VM_PAGE_TO_PHYS(srcpg);				\
	paddr_t __dst = VM_PAGE_TO_PHYS(dstpg);				\
	__asm__("addl3 $0x80000000,%0,%%r0;addl3 $0x80000000,%1,%%r1;	\
	    movc3 $4096,(%%r0),(%%r1)"					\
	    :: "r"(__src),"r"(__dst):"r0","r1","r2","r3","r4","r5");	\
} while (0)

#define pmap_zero_page(pg) do {						\
	paddr_t __pa = VM_PAGE_TO_PHYS(pg);				\
	__asm__("addl3 $0x80000000,%0,%%r0;movc5 $0,(%%r0),$0,$4096,(%%r0)" \
	    :: "r"(__pa): "r0","r1","r2","r3","r4","r5");		\
} while (0)

#define pmap_proc_iflush(p,va,len)	/* nothing */
#define pmap_unuse_final(p)		/* nothing */

/* Prototypes */
void	pmap_bootstrap(void);
vaddr_t pmap_map(vaddr_t, paddr_t, paddr_t, int);
void	pmap_pinit(pmap_t);

#endif	/* _KERNEL */

struct vm_page_md {
	struct pv_entry *pv_head;
	int		 pv_attr;	/* write/modified bits */
};

#define	VM_MDPAGE_INIT(pg) \
	do { \
		(pg)->mdpage.pv_head = NULL; \
		(pg)->mdpage.pv_attr = 0; \
	} while (0)

#endif /* _MACHINE_PMAP_H_ */
