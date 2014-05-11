/*	$OpenBSD: pmap.h,v 1.50 2014/01/30 18:16:41 miod Exp $	*/
/*	$NetBSD: pmap.h,v 1.1 1996/09/30 16:34:29 ws Exp $	*/

/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_POWERPC_PMAP_H_
#define	_POWERPC_PMAP_H_

#include <machine/pte.h>

/*
 * Segment registers
 */
#ifndef	_LOCORE
typedef u_int sr_t;
#endif	/* _LOCORE */
#define	SR_TYPE		0x80000000
#define	SR_SUKEY	0x40000000
#define	SR_PRKEY	0x20000000
#define SR_NOEXEC	0x10000000
#define	SR_VSID		0x00ffffff
/*
 * bit 
 *   3  2 2  2    2 1  1 1  1 1            0
 *   1  8 7  4    0 9  6 5  2 1            0
 *  |XXXX|XXXX XXXX|XXXX XXXX|XXXX XXXX XXXX
 *
 *  bits 28 - 31 contain SR
 *  bits 20 - 27 contain L1 for VtoP translation
 *  bits 12 - 19 contain L2 for VtoP translation
 *  bits  0 - 11 contain page offset
 */
#ifndef _LOCORE
/* V->P mapping data */
#define VP_SR_SIZE	16
#define VP_SR_MASK	(VP_SR_SIZE-1)
#define VP_SR_POS 	28
#define VP_IDX1_SIZE	256
#define VP_IDX1_MASK	(VP_IDX1_SIZE-1)
#define VP_IDX1_POS 	20
#define VP_IDX2_SIZE	256
#define VP_IDX2_MASK	(VP_IDX2_SIZE-1)
#define VP_IDX2_POS 	12

/* functions used by the bus layer for device accesses */
void pmap_kenter_cache(vaddr_t va, paddr_t pa, vm_prot_t prot, int cacheable);
void pmap_kremove_pg(vaddr_t va);

/* cache flags */
#define PMAP_CACHE_DEFAULT	0 	/* WB cache managed mem, devices not */
#define PMAP_CACHE_CI		1 	/* cache inhibit */
#define PMAP_CACHE_WT		2 	/* writethru */
#define PMAP_CACHE_WB		3	/* writeback */

#ifdef	_KERNEL

/*
 * Pmap stuff
 */
struct pmap {
	sr_t pm_sr[16];		/* segments used in this pmap */
	struct pmapvp *pm_vp[VP_SR_SIZE];	/* virtual to physical table */
	u_int32_t pm_exec[16];	/* segments used in this pmap */
	int pm_refs;		/* ref count */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
};

/*
 * Segment handling stuff
 */
#define	PPC_SEGMENT_LENGTH	0x10000000
#define	PPC_SEGMENT_MASK	0xf0000000

/*
 * Some system constants
 */
#ifndef	NPMAPS
#define	NPMAPS		32768	/* Number of pmaps in system */
#endif

typedef	struct pmap *pmap_t;

extern struct pmap kernel_pmap_;
#define	pmap_kernel()	(&kernel_pmap_)
boolean_t pteclrbits(struct vm_page *pg, u_int mask, u_int clear);


#define pmap_clear_modify(page) \
	(pteclrbits((page), PG_PMAP_MOD, TRUE))
#define	pmap_clear_reference(page) \
	(pteclrbits((page), PG_PMAP_REF, TRUE))
#define	pmap_is_modified(page) \
	(pteclrbits((page), PG_PMAP_MOD, FALSE))
#define	pmap_is_referenced(page) \
	(pteclrbits((page), PG_PMAP_REF, FALSE))

#define	pmap_unwire(pm, va)
#define pmap_update(pmap)	/* nothing (yet) */

#define pmap_resident_count(pmap)       ((pmap)->pm_stats.resident_count) 

/*
 * Alternate mapping methods for pool.
 * Really simple. 0x0->0x80000000 contain 1->1 mappings of the physical
 * memory. - XXX
 */
#define pmap_map_direct(pg)		((vaddr_t)VM_PAGE_TO_PHYS(pg))
#define pmap_unmap_direct(va)		PHYS_TO_VM_PAGE((paddr_t)va)
#define	__HAVE_PMAP_DIRECT

void pmap_bootstrap(u_int kernelstart, u_int kernelend);

void pmap_pinit(struct pmap *);
void pmap_release(struct pmap *);

void pmap_real_memory(vaddr_t *start, vsize_t *size);
void switchexit(struct proc *);

int pte_spill_v(struct pmap *pm, u_int32_t va, u_int32_t dsisr, int exec_fault);
#define pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr) ;
int reserve_dumppages(caddr_t p);

#define pmap_unuse_final(p)		/* nothing */
#define	pmap_remove_holes(map)		do { /* nothing */ } while (0)

#define	PMAP_STEAL_MEMORY

#define PG_PMAP_MOD     PG_PMAP0
#define PG_PMAP_REF     PG_PMAP1
#define PG_PMAP_EXE     PG_PMAP2

/*
 * MD flags to pmap_enter:
 */

/* to get just the pa from params to pmap_enter */
#define PMAP_PA_MASK	~((paddr_t)PAGE_MASK)
#define PMAP_NOCACHE	0x1		/* map uncached */

#endif	/* _KERNEL */

struct vm_page_md {
	LIST_HEAD(,pte_desc) pv_list;
};

#define VM_MDPAGE_INIT(pg) do {                 \
	LIST_INIT(&((pg)->mdpage.pv_list)); 	\
} while (0)

#endif	/* _LOCORE */

#endif	/* _POWERPC_PMAP_H_ */
