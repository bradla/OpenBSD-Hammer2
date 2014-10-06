/*	$OpenBSD: cpufunc.h,v 1.15 2014/03/29 18:09:28 guenther Exp $	*/
/*	$NetBSD: cpufunc.h,v 1.29 2003/09/06 09:08:35 rearnsha Exp $	*/

/*
 * Copyright (c) 1997 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited
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
 *	This product includes software developed by Causality Limited.
 * 4. The name of Causality Limited may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CAUSALITY LIMITED ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CAUSALITY LIMITED BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * cpufunc.h
 *
 * Prototypes for cpu, mmu and tlb related functions.
 */

#ifndef _ARM_CPUFUNC_H_
#define _ARM_CPUFUNC_H_

#ifdef _KERNEL

#include <sys/types.h>
#include <arm/cpuconf.h>

struct cpu_functions {

	/* CPU functions */
	
	u_int	(*cf_id)		(void);
	void	(*cf_cpwait)		(void);

	/* MMU functions */

	u_int	(*cf_control)		(u_int bic, u_int eor);
	void	(*cf_domains)		(u_int domains);
	void	(*cf_setttb)		(u_int ttb);
	u_int	(*cf_dfsr)		(void);
	u_int	(*cf_dfar)		(void);
	u_int	(*cf_ifsr)		(void);
	u_int	(*cf_ifar)		(void);

	/* TLB functions */

	void	(*cf_tlb_flushID)	(void);	
	void	(*cf_tlb_flushID_SE)	(u_int va);	
	void	(*cf_tlb_flushI)	(void);
	void	(*cf_tlb_flushI_SE)	(u_int va);	
	void	(*cf_tlb_flushD)	(void);
	void	(*cf_tlb_flushD_SE)	(u_int va);	

	/*
	 * Cache operations:
	 *
	 * We define the following primitives:
	 *
	 *	icache_sync_all		Synchronize I-cache
	 *	icache_sync_range	Synchronize I-cache range
	 *
	 *	dcache_wbinv_all	Write-back and Invalidate D-cache
	 *	dcache_wbinv_range	Write-back and Invalidate D-cache range
	 *	dcache_inv_range	Invalidate D-cache range
	 *	dcache_wb_range		Write-back D-cache range
	 *
	 *	idcache_wbinv_all	Write-back and Invalidate D-cache,
	 *				Invalidate I-cache
	 *	idcache_wbinv_range	Write-back and Invalidate D-cache,
	 *				Invalidate I-cache range
	 *
	 * Note that the ARM term for "write-back" is "clean".  We use
	 * the term "write-back" since it's a more common way to describe
	 * the operation.
	 *
	 * There are some rules that must be followed:
	 *
	 *	I-cache Synch (all or range):
	 *		The goal is to synchronize the instruction stream,
	 *		so you may beed to write-back dirty D-cache blocks
	 *		first.  If a range is requested, and you can't
	 *		synchronize just a range, you have to hit the whole
	 *		thing.
	 *
	 *	D-cache Write-Back and Invalidate range:
	 *		If you can't WB-Inv a range, you must WB-Inv the
	 *		entire D-cache.
	 *
	 *	D-cache Invalidate:
	 *		If you can't Inv the D-cache, you must Write-Back
	 *		and Invalidate.  Code that uses this operation
	 *		MUST NOT assume that the D-cache will not be written
	 *		back to memory.
	 *
	 *	D-cache Write-Back:
	 *		If you can't Write-back without doing an Inv,
	 *		that's fine.  Then treat this as a WB-Inv.
	 *		Skipping the invalidate is merely an optimization.
	 *
	 *	All operations:
	 *		Valid virtual addresses must be passed to each
	 *		cache operation.
	 */
	void	(*cf_icache_sync_all)	(void);
	void	(*cf_icache_sync_range)	(vaddr_t, vsize_t);

	void	(*cf_dcache_wbinv_all)	(void);
	void	(*cf_dcache_wbinv_range) (vaddr_t, vsize_t);
	void	(*cf_dcache_inv_range)	(vaddr_t, vsize_t);
	void	(*cf_dcache_wb_range)	(vaddr_t, vsize_t);

	void	(*cf_idcache_wbinv_all)	(void);
	void	(*cf_idcache_wbinv_range) (vaddr_t, vsize_t);

	void	(*cf_sdcache_wbinv_all)	(void);
	void	(*cf_sdcache_wbinv_range) (vaddr_t, paddr_t, vsize_t);
	void	(*cf_sdcache_inv_range)	(vaddr_t, paddr_t, vsize_t);
	void	(*cf_sdcache_wb_range)	(vaddr_t, paddr_t, vsize_t);

	/* Other functions */

	void	(*cf_flush_prefetchbuf)	(void);
	void	(*cf_drain_writebuf)	(void);

	void	(*cf_sleep)		(int mode);

	/* Soft functions */
	void	(*cf_context_switch)	(u_int);
	void	(*cf_setup)		(void);
};

extern struct cpu_functions cpufuncs;
extern u_int cputype;

#define cpu_id()		cpufuncs.cf_id()
#define	cpu_cpwait()		cpufuncs.cf_cpwait()

#define cpu_control(c, e)	cpufuncs.cf_control(c, e)
#define cpu_domains(d)		cpufuncs.cf_domains(d)
#define cpu_setttb(t)		cpufuncs.cf_setttb(t)
#define cpu_dfsr()		cpufuncs.cf_dfsr()
#define cpu_dfar()		cpufuncs.cf_dfar()
#define cpu_ifsr()		cpufuncs.cf_ifsr()
#define cpu_ifar()		cpufuncs.cf_ifar()

#define	cpu_tlb_flushID()	cpufuncs.cf_tlb_flushID()
#define	cpu_tlb_flushID_SE(e)	cpufuncs.cf_tlb_flushID_SE(e)
#define	cpu_tlb_flushI()	cpufuncs.cf_tlb_flushI()
#define	cpu_tlb_flushI_SE(e)	cpufuncs.cf_tlb_flushI_SE(e)
#define	cpu_tlb_flushD()	cpufuncs.cf_tlb_flushD()
#define	cpu_tlb_flushD_SE(e)	cpufuncs.cf_tlb_flushD_SE(e)

#define	cpu_icache_sync_all()	cpufuncs.cf_icache_sync_all()
#define	cpu_icache_sync_range(a, s) cpufuncs.cf_icache_sync_range((a), (s))

#define	cpu_dcache_wbinv_all()	cpufuncs.cf_dcache_wbinv_all()
#define	cpu_dcache_wbinv_range(a, s) cpufuncs.cf_dcache_wbinv_range((a), (s))
#define	cpu_dcache_inv_range(a, s) cpufuncs.cf_dcache_inv_range((a), (s))
#define	cpu_dcache_wb_range(a, s) cpufuncs.cf_dcache_wb_range((a), (s))

#define	cpu_idcache_wbinv_all()	cpufuncs.cf_idcache_wbinv_all()
#define	cpu_idcache_wbinv_range(a, s) cpufuncs.cf_idcache_wbinv_range((a), (s))

#define	cpu_sdcache_enabled()	(cpufuncs.cf_sdcache_wbinv_all != cpufunc_nullop)
#define	cpu_sdcache_wbinv_all()	cpufuncs.cf_sdcache_wbinv_all()
#define	cpu_sdcache_wbinv_range(va, pa, s) cpufuncs.cf_sdcache_wbinv_range((va), (pa), (s))
#define	cpu_sdcache_inv_range(va, pa, s) cpufuncs.cf_sdcache_inv_range((va), (pa), (s))
#define	cpu_sdcache_wb_range(va, pa, s) cpufuncs.cf_sdcache_wb_range((va), (pa), (s))

#define	cpu_flush_prefetchbuf()	cpufuncs.cf_flush_prefetchbuf()
#define	cpu_drain_writebuf()	cpufuncs.cf_drain_writebuf()

#define cpu_sleep(m)		cpufuncs.cf_sleep(m)

#define cpu_context_switch(a)		cpufuncs.cf_context_switch(a)
#define cpu_setup(a)			cpufuncs.cf_setup(a)

int	set_cpufuncs		(void);
#define ARCHITECTURE_NOT_PRESENT	1	/* known but not configured */
#define ARCHITECTURE_NOT_SUPPORTED	2	/* not known */

void	cpufunc_nullop		(void);
int	early_abort_fixup	(void *);
int	late_abort_fixup	(void *);
u_int	cpufunc_id		(void);
u_int	cpufunc_control		(u_int clear, u_int bic);
void	cpufunc_domains		(u_int domains);
u_int	cpufunc_dfsr		(void);
u_int	cpufunc_dfar		(void);
u_int	cpufunc_ifsr		(void);
u_int	cpufunc_ifar		(void);

#ifdef CPU_ARM8
void	arm8_setttb		(u_int ttb);
void	arm8_tlb_flushID	(void);
void	arm8_tlb_flushID_SE	(u_int va);
void	arm8_cache_flushID	(void);
void	arm8_cache_flushID_E	(u_int entry);
void	arm8_cache_cleanID	(void);
void	arm8_cache_cleanID_E	(u_int entry);
void	arm8_cache_purgeID	(void);
void	arm8_cache_purgeID_E	(u_int entry);

void	arm8_cache_syncI	(void);
void	arm8_cache_cleanID_rng	(vaddr_t start, vsize_t end);
void	arm8_cache_cleanD_rng	(vaddr_t start, vsize_t end);
void	arm8_cache_purgeID_rng	(vaddr_t start, vsize_t end);
void	arm8_cache_purgeD_rng	(vaddr_t start, vsize_t end);
void	arm8_cache_syncI_rng	(vaddr_t start, vsize_t end);

void	arm8_context_switch	(u_int);

void	arm8_setup		(void);

u_int	arm8_clock_config	(u_int, u_int);
#endif

#if defined(CPU_SA1100) || defined(CPU_SA1110)
void	sa11x0_drain_readbuf	(void);

void	sa11x0_context_switch	(u_int);
void	sa11x0_cpu_sleep	(int mode);
 
void	sa11x0_setup		(void);
#endif

#if defined(CPU_SA1100) || defined(CPU_SA1110)
void	sa1_setttb		(u_int ttb);

void	sa1_tlb_flushID_SE	(u_int va);

void	sa1_cache_flushID	(void);
void	sa1_cache_flushI	(void);
void	sa1_cache_flushD	(void);
void	sa1_cache_flushD_SE	(u_int entry);

void	sa1_cache_cleanID	(void);
void	sa1_cache_cleanD	(void);
void	sa1_cache_cleanD_E	(u_int entry);

void	sa1_cache_purgeID	(void);
void	sa1_cache_purgeID_E	(u_int entry);
void	sa1_cache_purgeD	(void);
void	sa1_cache_purgeD_E	(u_int entry);

void	sa1_cache_syncI		(void);
void	sa1_cache_cleanID_rng	(vaddr_t start, vsize_t end);
void	sa1_cache_cleanD_rng	(vaddr_t start, vsize_t end);
void	sa1_cache_purgeID_rng	(vaddr_t start, vsize_t end);
void	sa1_cache_purgeD_rng	(vaddr_t start, vsize_t end);
void	sa1_cache_syncI_rng	(vaddr_t start, vsize_t end);

#endif

#ifdef CPU_ARM9
void	arm9_setttb			(u_int);

void	arm9_tlb_flushID_SE		(u_int);

void	arm9_icache_sync_all		(void);
void	arm9_icache_sync_range		(vaddr_t, vsize_t);

void	arm9_dcache_wbinv_all		(void);
void	arm9_dcache_wbinv_range		(vaddr_t, vsize_t);
void	arm9_dcache_inv_range		(vaddr_t, vsize_t);
void	arm9_dcache_wb_range		(vaddr_t, vsize_t);

void	arm9_idcache_wbinv_all		(void);
void	arm9_idcache_wbinv_range	(vaddr_t, vsize_t);

void	arm9_context_switch		(u_int);

void	arm9_setup			(void);

extern unsigned arm9_dcache_sets_max;
extern unsigned arm9_dcache_sets_inc;
extern unsigned arm9_dcache_index_max;
extern unsigned arm9_dcache_index_inc;
#endif

#if defined(CPU_ARM9E) || defined(CPU_ARM10)
void	arm10_tlb_flushID_SE	(u_int);
void	arm10_tlb_flushI_SE	(u_int);

void	arm10_context_switch	(u_int);

void	arm9e_setup		(void);
void	arm10_setup		(void);
#endif

#if defined(CPU_ARM9E) || defined (CPU_ARM10)
void	armv5_ec_setttb			(u_int);

void	armv5_ec_icache_sync_all	(void);
void	armv5_ec_icache_sync_range	(vaddr_t, vsize_t);

void	armv5_ec_dcache_wbinv_all	(void);
void	armv5_ec_dcache_wbinv_range	(vaddr_t, vsize_t);
void	armv5_ec_dcache_inv_range	(vaddr_t, vsize_t);
void	armv5_ec_dcache_wb_range	(vaddr_t, vsize_t);

void	armv5_ec_idcache_wbinv_all	(void);
void	armv5_ec_idcache_wbinv_range	(vaddr_t, vsize_t);
#endif

#ifdef CPU_ARM11
void	arm11_setttb		(u_int);

void	arm11_tlb_flushID_SE	(u_int);
void	arm11_tlb_flushI_SE	(u_int);

void	arm11_context_switch	(u_int);

void	arm11_setup		(void);
void	arm11_tlb_flushID	(void);
void	arm11_tlb_flushI	(void);
void	arm11_tlb_flushD	(void);
void	arm11_tlb_flushD_SE	(u_int	va);

void	arm11_drain_writebuf	(void);
void	arm11_cpu_sleep		(int	mode);
#endif


#if defined (CPU_ARM10) || defined(CPU_ARM11)
void	armv5_setttb			(u_int);

void	armv5_icache_sync_all		(void);
void	armv5_icache_sync_range		(vaddr_t, vsize_t);

void	armv5_dcache_wbinv_all		(void);
void	armv5_dcache_wbinv_range	(vaddr_t, vsize_t);
void	armv5_dcache_inv_range		(vaddr_t, vsize_t);
void	armv5_dcache_wb_range		(vaddr_t, vsize_t);

void	armv5_idcache_wbinv_all		(void);
void	armv5_idcache_wbinv_range	(vaddr_t, vsize_t);

extern unsigned armv5_dcache_sets_max;
extern unsigned armv5_dcache_sets_inc;
extern unsigned armv5_dcache_index_max;
extern unsigned armv5_dcache_index_inc;
#endif

#ifdef CPU_ARMv7
void	armv7_setttb		(u_int);

void	armv7_tlb_flushID_SE	(u_int);
void	armv7_tlb_flushI_SE	(u_int);

void	armv7_context_switch	(u_int);

void	armv7_setup		(void);
void	armv7_tlb_flushID	(void);
void	armv7_tlb_flushI	(void);
void	armv7_tlb_flushD	(void);
void	armv7_tlb_flushD_SE	(u_int va);

void	armv7_drain_writebuf	(void);
void	armv7_cpu_sleep		(int mode);

u_int	armv7_periphbase	(void);

void	armv7_icache_sync_all		(void);
void	armv7_icache_sync_range		(vaddr_t, vsize_t);

void	armv7_dcache_wbinv_all		(void);
void	armv7_dcache_wbinv_range	(vaddr_t, vsize_t);
void	armv7_dcache_inv_range		(vaddr_t, vsize_t);
void	armv7_dcache_wb_range		(vaddr_t, vsize_t);

void	armv7_idcache_wbinv_all		(void);
void	armv7_idcache_wbinv_range	(vaddr_t, vsize_t);

extern unsigned armv7_dcache_sets_max;
extern unsigned armv7_dcache_sets_inc;
extern unsigned armv7_dcache_index_max;
extern unsigned armv7_dcache_index_inc;
#endif


#if defined(CPU_ARM9) || defined(CPU_ARM9E) || defined(CPU_ARM10) || \
    defined(CPU_SA1100) || defined(CPU_SA1110) || \
    defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) || \
    defined(CPU_XSCALE_PXA2X0) || defined(CPU_XSCALE_IXP425)

void	armv4_tlb_flushID	(void);
void	armv4_tlb_flushI	(void);
void	armv4_tlb_flushD	(void);
void	armv4_tlb_flushD_SE	(u_int va);

void	armv4_drain_writebuf	(void);
#endif

#if defined(CPU_IXP12X0)
void	ixp12x0_drain_readbuf	(void);
void	ixp12x0_context_switch	(u_int);
void	ixp12x0_setup		(void);
#endif

#if defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) || \
    defined(CPU_XSCALE_PXA2X0) || defined(CPU_XSCALE_IXP425) || \
    (ARM_MMU_XSCALE == 1)
void	xscale_cpwait		(void);

void	xscale_cpu_sleep	(int mode);

u_int	xscale_control		(u_int clear, u_int bic);

void	xscale_setttb		(u_int ttb);

void	xscale_tlb_flushID_SE	(u_int va);

void	xscale_cache_flushID	(void);
void	xscale_cache_flushI	(void);
void	xscale_cache_flushD	(void);
void	xscale_cache_flushD_SE	(u_int entry);

void	xscale_cache_cleanID	(void);
void	xscale_cache_cleanD	(void);
void	xscale_cache_cleanD_E	(u_int entry);

void	xscale_cache_clean_minidata (void);

void	xscale_cache_purgeID	(void);
void	xscale_cache_purgeID_E	(u_int entry);
void	xscale_cache_purgeD	(void);
void	xscale_cache_purgeD_E	(u_int entry);

void	xscale_cache_syncI	(void);
void	xscale_cache_cleanID_rng (vaddr_t start, vsize_t end);
void	xscale_cache_cleanD_rng	(vaddr_t start, vsize_t end);
void	xscale_cache_purgeID_rng (vaddr_t start, vsize_t end);
void	xscale_cache_purgeD_rng	(vaddr_t start, vsize_t end);
void	xscale_cache_syncI_rng	(vaddr_t start, vsize_t end);
void	xscale_cache_flushD_rng	(vaddr_t start, vsize_t end);

void	xscale_context_switch	(u_int);

void	xscale_setup		(void);
#endif	/* CPU_XSCALE_80200 || CPU_XSCALE_80321 || CPU_XSCALE_PXA2X0 || CPU_XSCALE_IXP425 */

#define tlb_flush	cpu_tlb_flushID
#define setttb		cpu_setttb
#define drain_writebuf	cpu_drain_writebuf

/*
 * Macros for manipulating CPU interrupts
 */
/* Functions to manipulate the CPSR. */
static __inline u_int32_t __set_cpsr_c(u_int bic, u_int eor);
static __inline u_int32_t __get_cpsr(void);

static __inline u_int32_t
__set_cpsr_c(u_int bic, u_int eor)
{
	u_int32_t	tmp, ret;

	__asm volatile(
		"mrs	%0, cpsr\n\t"	/* Get the CPSR */
		"bic	%1, %0, %2\n\t"	/* Clear bits */
		"eor	%1, %1, %3\n\t"	/* XOR bits */
		"msr	cpsr_c, %1"	/* Set CPSR control field */
	: "=&r" (ret), "=&r" (tmp)
	: "r" (bic), "r" (eor));

	return ret;
}

static __inline u_int32_t
__get_cpsr()
{
	u_int32_t	ret;

	__asm volatile("mrs	%0, cpsr" : "=&r" (ret));

	return ret;
}

#define disable_interrupts(mask)					\
	(__set_cpsr_c((mask) & (I32_bit | F32_bit), \
		      (mask) & (I32_bit | F32_bit)))

#define enable_interrupts(mask)						\
	(__set_cpsr_c((mask) & (I32_bit | F32_bit), 0))

#define restore_interrupts(old_cpsr)					\
	(__set_cpsr_c((I32_bit | F32_bit), (old_cpsr) & (I32_bit | F32_bit)))

/*
 * Functions to manipulate cpu r13
 * (in arm/arm/setstack.S)
 */

void set_stackptr	(u_int mode, u_int address);
u_int get_stackptr	(u_int mode);

/*
 * Miscellany
 */

int get_pc_str_offset	(void);

/*
 * CPU functions from locore.S
 */

void cpu_reset		(void) __attribute__((__noreturn__));

/*
 * Cache info variables.
 */

/* PRIMARY CACHE VARIABLES */
extern int	arm_picache_size;
extern int	arm_picache_line_size;
extern int	arm_picache_ways;

extern int	arm_pdcache_size;	/* and unified */
extern int	arm_pdcache_line_size;
extern int	arm_pdcache_ways; 

extern int	arm_pcache_type;
extern int	arm_pcache_unified;

extern int	arm_dcache_align;
extern int	arm_dcache_align_mask;

#endif	/* _KERNEL */
#endif	/* _ARM_CPUFUNC_H_ */

/* End of cpufunc.h */
