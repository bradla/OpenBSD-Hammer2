/*	$OpenBSD: locore.s,v 1.175 2013/06/15 10:05:58 kettenis Exp $	*/
/*	$NetBSD: locore.s,v 1.137 2001/08/13 06:10:10 jdolecek Exp $	*/

/*
 * Copyright (c) 1996-2001 Eduardo Horvath
 * Copyright (c) 1996 Paul Kranenburg
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College.
 *	All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.
 *	All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *	This product includes software developed by Harvard University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by Paul Kranenburg.
 * 4. Neither the name of the University nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 *	@(#)locore.s	8.4 (Berkeley) 12/10/93
 */

#define HORRID_III_HACK

#undef	NO_VCACHE		/* Map w/D$ disabled */
#undef	DCACHE_BUG		/* Flush D$ around ASI_PHYS accesses */
#undef	NO_TSB			/* Don't use TSB */

.register %g2,
.register %g3,

#include "assym.h"
#include "ksyms.h"
#include <machine/param.h>
#include <sparc64/sparc64/intreg.h>
#include <sparc64/sparc64/timerreg.h>
#include <machine/ctlreg.h>
#include <machine/psl.h>
#include <machine/signal.h>
#include <machine/trap.h>
#include <machine/frame.h>
#include <machine/pmap.h>
#include <machine/asm.h>

#undef	CURPROC
#undef	CPCB
#undef	FPPROC

/* Let us use same syntax as C code */
#define Debugger()	ta	1; nop

/* use as needed to align things on longword boundaries */
#define	_ALIGN	.align 8
#define ICACHE_ALIGN	.align	32

/* Give this real authority: reset the machine */
#if 1
#define NOTREACHED	sir
#else	/* 1 */
#define NOTREACHED
#endif	/* 1 */

	.section	.sun4v_patch, "ax"
	.globl _C_LABEL(sun4v_patch)
_C_LABEL(sun4v_patch):
	.previous

	.section	.sun4v_patch_end, "ax"
	.globl _C_LABEL(sun4v_patch_end)
_C_LABEL(sun4v_patch_end):
	.previous

#ifdef MULTIPROCESSOR
	.section	.sun4v_mp_patch, "ax"
	.globl _C_LABEL(sun4v_mp_patch)
_C_LABEL(sun4v_mp_patch):
	.previous

	.section	.sun4v_mp_patch_end, "ax"
	.globl _C_LABEL(sun4v_mp_patch_end)
_C_LABEL(sun4v_mp_patch_end):
	.previous

	.section	.sun4u_mtp_patch, "ax"
	.globl _C_LABEL(sun4u_mtp_patch)
_C_LABEL(sun4u_mtp_patch):
	.previous

	.section	.sun4u_mtp_patch_end, "ax"
	.globl _C_LABEL(sun4u_mtp_patch_end)
_C_LABEL(sun4u_mtp_patch_end):
	.previous
#endif

/*
 * The UltraSPARC T1 has a "feature" where a LDXA/STXA to ASI_SCRATCHPAD
 * registers may corrupt an unrelated integer register file register.
 * To prevent this, it is required to have a non-store or NOP instruction
 * before any LDXA/STXA to this register.
 */
#define GET_CPUINFO_VA(ci) \
	nop					;\
999:	set	CPUINFO_VA, ci			;\
	.section	.sun4v_mp_patch, "ax"	;\
	.word	999b				;\
	ldxa	[%g0] ASI_SCRATCHPAD, ci	;\
	.previous				;\
	.section	.sun4u_mtp_patch, "ax"	;\
	.word	999b				;\
	ldxa	[%g0] ASI_SCRATCH, ci		;\
	.previous

#define GET_CPCB(pcb) \
	GET_CPUINFO_VA(pcb)			;\
	ldx	[pcb + CI_CPCB], pcb

#define GET_CURPROC(curproc) \
	GET_CPUINFO_VA(curproc)			;\
	ldx	[curproc + CI_CURPROC], curproc

#ifdef SUN4V

#define GET_CPUINFO_PA(ci) \
	mov	0x10, ci				;\
	ldxa	[ci] ASI_SCRATCHPAD, ci

#define GET_MMFSA(mmfsa) \
	GET_CPUINFO_PA(mmfsa)			;\
	add	mmfsa, CI_MMFSA, mmfsa		;\
	ldxa	[mmfsa] ASI_PHYS_CACHED, mmfsa

#endif

#define GET_MMU_CONTEXTID(ctxid, ctx) \
999:	ldxa	[ctx] ASI_DMMU, ctxid 		;\
	.section	.sun4v_patch, "ax" 	;\
	.word	999b				;\
	ldxa	[ctx] ASI_MMU_CONTEXTID, ctxid 	;\
	.previous

#define SET_MMU_CONTEXTID(ctxid, ctx) \
999:	stxa	ctxid, [ctx] ASI_DMMU 		;\
	.section	.sun4v_patch, "ax" 	;\
	.word	999b				;\
	stxa	ctxid, [ctx] ASI_MMU_CONTEXTID 	;\
	.previous

#define NORMAL_GLOBALS() \
999:	wrpr	%g0, PSTATE_KERN, %pstate	;\
	.section	.sun4v_patch, "ax"	;\
	.word	999b				;\
	wrpr	%g0, 0, %gl			;\
	.previous

#define ALTERNATE_GLOBALS() \
999:	wrpr	%g0, PSTATE_KERN|PSTATE_AG, %pstate	;\
	.section	.sun4v_patch, "ax"	;\
	.word	999b				;\
	wrpr	%g0, 1, %gl			;\
	.previous


/*
 * This macro will clear out a cache line before an explicit
 * access to that location.  It's mostly used to make certain
 * loads bypassing the D$ do not get stale D$ data.
 *
 * It uses a register with the address to clear and a temporary
 * which is destroyed.
 */
	.macro DLFLUSH a,t
#ifdef DCACHE_BUG
	andn	\a, 0x1f, \t
	stxa	%g0, [ \t ] ASI_DCACHE_TAG
	membar	#Sync
#endif	/* DCACHE_BUG */
	.endm
/* The following can be used if the pointer is 16-byte aligned */
	.macro DLFLUSH2 t
#ifdef DCACHE_BUG
	stxa	%g0, [ \t ] ASI_DCACHE_TAG
	membar	#Sync
#endif	/* DCACHE_BUG */
	.endm

/*
 * A handy macro for maintaining instrumentation counters.
 * Note that this clobbers %o0, %o1 and %o2.  Normal usage is
 * something like:
 *	foointr:
 *		TRAP_SETUP ...		! makes %o registers safe
 *		INCR _C_LABEL(cnt)+V_FOO	! count a foo
 */
	.macro INCR what
	sethi	%hi(\what), %o0
	or	%o0, %lo(\what), %o0
99:
	lduw	[%o0], %o1
	add	%o1, 1, %o2
	casa	[%o0] ASI_P, %o1, %o2
	cmp	%o1, %o2
	bne,pn	%icc, 99b
	 nop
	.endm

/*
 * A couple of handy macros to save and restore globals to/from
 * locals.  Since udivrem uses several globals, and it's called
 * from vsprintf, we need to do this before and after doing a printf.
 */
	.macro GLOBTOLOC
	.irpc n,1234567
		mov	%g\n, %l\n
	.endr
	.endm

	.macro LOCTOGLOB
	.irpc n,1234567
		mov	%l\n, %g\n
	.endr
	.endm

/*
 * some macros to load and store a register window
 */
	.macro	SPILL storer,base,size,asi

	.irpc n,01234567
		\storer %l\n, [\base + (\n * \size)] \asi
	.endr
	.irpc n,01234567
		\storer %i\n, [\base + ((8+\n) * \size)] \asi
	.endr
	.endm

	.macro FILL loader, base, size, asi
	.irpc n,01234567
		\loader [\base + (\n * \size)] \asi, %l\n
	.endr

	.irpc n,01234567
		\loader [\base + ((8+\n) * \size)] \asi, %i\n
	.endr
	.endm

/* Load strings address into register; NOTE: hidden local label 99 */
#define LOAD_ASCIZ(reg, s)	\
	set	99f, reg ;	\
	.data ;			\
99:	.asciz	s ;		\
	_ALIGN ;		\
	.text

/*
 * Handy stack conversion macros.
 * Correctly switch to a 64-bit stack
 * regardless of the current stack.
 */

	.macro STACKFRAME size
	save	%sp, \size, %sp
	add	%sp, -BIAS, %o0		! Convert to 64-bits
	andcc	%sp, 1, %g0		! 64-bit stack?
	movz	%icc, %o0, %sp
	.endm


	.data
	.globl	_C_LABEL(data_start)
_C_LABEL(data_start):					! Start of data segment
#define DATA_START	_C_LABEL(data_start)

/*
 * Process 0's u.
 *
 * This must be aligned on an 8 byte boundary.
 */
	.globl	_C_LABEL(u0)
_C_LABEL(u0):	.xword	0
estack0:	.xword	0

/*
 * This stack is used for bootstrapping and spinning up CPUs.
 */
	.space	4096
	.align	16
tmpstack:

#ifdef DEBUG
/*
 * This stack is used when we detect kernel stack corruption.
 */
	.space	USPACE
	.align	16
panicstack:
#endif	/* DEBUG */

/*
 * romp is the prom entry pointer
 */
	.globl	romp
romp:	.xword	0

/*
 * cputyp is the current cpu type, used to distinguish between
 * the many variations of different sun4* machines. It contains
 * the value CPU_SUN4U or CPU_SUN4V.
 */
	.globl	_C_LABEL(cputyp)
_C_LABEL(cputyp):
	.word	CPU_SUN4U

	.globl _C_LABEL(cold)
_C_LABEL(cold):
	.word 1

	_ALIGN

	.text

/*
 * The v9 trap frame is stored in the special trap registers.  The
 * register window is only modified on window overflow, underflow,
 * and clean window traps, where it points to the register window
 * needing service.  Traps have space for 8 instructions, except for
 * the window overflow, underflow, and clean window traps which are
 * 32 instructions long, large enough to in-line.
 *
 * The spitfire CPU (Ultra I) has 4 different sets of global registers.
 * (blah blah...)
 *
 * I used to generate these numbers by address arithmetic, but gas's
 * expression evaluator has about as much sense as your average slug
 * (oddly enough, the code looks about as slimy too).  Thus, all the
 * trap numbers are given as arguments to the trap macros.  This means
 * there is one line per trap.  Sigh.
 *
 * Hardware interrupt vectors can be `linked'---the linkage is to regular
 * C code---or rewired to fast in-window handlers.  The latter are good
 * for unbuffered hardware like the Zilog serial chip and the AMD audio
 * chip, where many interrupts can be handled trivially with pseudo-DMA
 * or similar.  Only one `fast' interrupt can be used per level, however,
 * and direct and `fast' interrupts are incompatible.  Routines in intr.c
 * handle setting these, with optional paranoia.
 */

/*
 *	TA8 -- trap align for 8 instruction traps
 *	TA32 -- trap align for 32 instruction traps
 */
	.macro TA8
	.align 32
	.endm

	.macro TA32
	.align 128
	.endm

/*
 * v9 trap macros:
 *
 *	We have a problem with v9 traps; we have no registers to put the
 *	trap type into.  But we do have a %tt register which already has
 *	that information.  Trap types in these macros are all dummys.
 */
	/* regular vectored traps */
#ifdef DEBUG
	.macro VTRAP type, label
	sethi	%hi(DATA_START),%g1
	rdpr	%tt,%g2
	or	%g1,0x28,%g1
	b	\label
	stx	%g2,[%g1]
	NOTREACHED
	TA8
	.endm
#else	/* DEBUG */
	.macro VTRAP type, label
	ba,a,pt	%icc,\label
	nop
	NOTREACHED
	TA8
	.endm
#endif	/* DEBUG */
	/* hardware interrupts (can be linked or made `fast') */
	.macro HARDINT4U lev
	VTRAP \lev, _C_LABEL(sparc_interrupt)
	.endm

	/* software interrupts (may not be made direct, sorry---but you
	   should not be using them trivially anyway) */
	.macro SOFTINT4U lev, bit
	HARDINT4U lev
	.endm

	/* traps that just call trap() */
	.macro TRAP type
	VTRAP \type, slowtrap
	.endm

	/* architecturally undefined traps (cause panic) */
	.macro	UTRAP type
#ifndef DEBUG
	sir
#endif	/* DEBUG */
	VTRAP \type, slowtrap
	.endm

	/* software undefined traps (may be replaced) */
	.macro STRAP type
	VTRAP \type, slowtrap
	.endm

#define	SYSCALL		VTRAP 0x100, syscall_setup
#define	ZS_INTERRUPT4U	HARDINT4U 12

/*
 * Macro to clear %tt so we don't get confused with old traps.
 */
	.macro CLRTT n
#ifdef DEBUG
#if 0	/* for henric, but not yet */
	wrpr	%g0, 0x1ff - \n, %tt
#else	/* 0 */
	wrpr	%g0, 0x1ff, %tt
#endif	/* 0 */
#endif	/* DEBUG */
	.endm

	.macro UCLEANWIN
	rdpr %cleanwin, %o7		! 024-027 = clean window trap
	inc %o7				!	This handler is in-lined and cannot fault
#ifdef DEBUG
	set	0xbadcafe, %l0		! DEBUG -- compiler should not rely on zero-ed registers.
#else	/* DEBUG */
	clr	%l0
#endif	/* DEBUG */
	wrpr %g0, %o7, %cleanwin	!       Nucleus (trap&IRQ) code does not need clean windows

	mov %l0,%l1; mov %l0,%l2	!	Clear out %l0-%l8 and %o0-%o8 and inc %cleanwin and done
	mov %l0,%l3; mov %l0,%l4
	mov %l0, %l5
	mov %l0, %l6; mov %l0, %l7; mov %l0, %o0; mov %l0, %o1

	mov %l0, %o2; mov %l0, %o3; mov %l0, %o4; mov %l0, %o5;
	mov %l0, %o6; mov %l0, %o7
	CLRTT 5
	retry; nop; NOTREACHED; TA32
	.endm

	.macro KCLEANWIN
	clr	%l0
#ifdef DEBUG
	set	0xbadbeef, %l0		! DEBUG
#endif	/* DEBUG */
	mov %l0, %l1; mov %l0, %l2	! 024-027 = clean window trap
	rdpr %cleanwin, %o7		!	This handler is in-lined and cannot fault
	inc %o7; mov %l0, %l3	!       Nucleus (trap&IRQ) code does not need clean windows
	wrpr %g0, %o7, %cleanwin	!	Clear out %l0-%l8 and %o0-%o8 and inc %cleanwin and done
	mov %l0, %l4; mov %l0, %l5; mov %l0, %l6; mov %l0, %l7
	mov %l0, %o0; mov %l0, %o1; mov %l0, %o2; mov %l0, %o3

	mov %l0, %o4; mov %l0, %o5; mov %l0, %o6; mov %l0, %o7
	CLRTT 8
	retry; nop; TA32
	.endm

	.macro IMMU_MISS n
	ldxa	[%g0] ASI_IMMU_8KPTR, %g2!	Load IMMU 8K TSB pointer
	ldxa	[%g0] ASI_IMMU, %g1	!	Load IMMU tag target register
	ldda	[%g2] ASI_NUCLEUS_QUAD_LDD, %g4!Load TSB tag:data into %g4
	brgez,pn %g5, instr_miss	!	Entry invalid?  Punt
	 cmp	%g1, %g4		!	Compare TLB tags
	bne,pn %xcc, instr_miss		!	Got right tag?
	 nop
	CLRTT \n
	stxa	%g5, [%g0] ASI_IMMU_DATA_IN!	Enter new mapping
	retry				!	Try new mapping
1:
	sir
	TA32
	.endm

	.macro DMMU_MISS n
	ldxa	[%g0] ASI_DMMU_8KPTR, %g2!	Load DMMU 8K TSB pointer
	ldxa	[%g0] ASI_DMMU, %g1	!	Load DMMU tag target register
	ldda	[%g2] ASI_NUCLEUS_QUAD_LDD, %g4!Load TSB tag:data into %g4
	brgez,pn %g5, data_miss		!	Entry invalid?  Punt
	 xor	%g1, %g4, %g4		!	Compare TLB tags
	brnz,pn	%g4, data_miss		!	Got right tag?
	 nop
	CLRTT \n
	stxa	%g5, [%g0] ASI_DMMU_DATA_IN!	Enter new mapping
	retry				!	Try new mapping
1:
	sir
	TA32
	.endm

	.macro DMMU_PROT dprot
	ba,a,pt	%xcc, dmmu_write_fault
	nop
	TA32
	.endm
/*
 * Here are some often repeated traps as macros.
 */

	! spill a 64-bit user register window
	.macro USPILL64 label, as
\label:
	wr	%g0, \as, %asi
	stxa	%l0, [%sp + BIAS + ( 0*8)] %asi
	stxa	%l1, [%sp + BIAS + ( 1*8)] %asi
	stxa	%l2, [%sp + BIAS + ( 2*8)] %asi
	stxa	%l3, [%sp + BIAS + ( 3*8)] %asi
	stxa	%l4, [%sp + BIAS + ( 4*8)] %asi
	stxa	%l5, [%sp + BIAS + ( 5*8)] %asi
	stxa	%l6, [%sp + BIAS + ( 6*8)] %asi
	stxa	%l7, [%sp + BIAS + ( 7*8)] %asi
	stxa	%i0, [%sp + BIAS + ( 8*8)] %asi
	stxa	%i1, [%sp + BIAS + ( 9*8)] %asi
	stxa	%i2, [%sp + BIAS + (10*8)] %asi
	stxa	%i3, [%sp + BIAS + (11*8)] %asi
	stxa	%i4, [%sp + BIAS + (12*8)] %asi
	stxa	%i5, [%sp + BIAS + (13*8)] %asi
	stxa	%i6, [%sp + BIAS + (14*8)] %asi
	GET_CPCB(%g5)
	ldx	[%g5 + PCB_WCOOKIE], %g5
	xor	%g5, %i7, %g5		! stackghost
	stxa	%g5, [%sp + BIAS + (15*8)] %asi
	saved
	CLRTT 1
	retry
	NOTREACHED
	TA32
	.endm

	! spill a 64-bit kernel register window
	.macro SPILL64 label, as
\label:
	wr	%g0, \as, %asi
	SPILL	stxa, %sp+BIAS, 8, %asi
	saved
	CLRTT 1
	retry
	NOTREACHED
	TA32
	.endm

	! spill a 32-bit register window
	.macro SPILL32 label, as
\label:
	wr	%g0, \as, %asi
	srl	%sp, 0, %sp ! fixup 32-bit pointers
	SPILL	stwa, %sp, 4, %asi
	saved
	CLRTT 2
	retry
	NOTREACHED
	TA32
	.endm

	! Spill either 32-bit or 64-bit register window.
	.macro SPILLBOTH label64,label32, as
	andcc	%sp, 1, %g0
	bnz,pt	%xcc, \label64+4	! Is it a v9 or v8 stack?
	 wr	%g0, \as, %asi
	ba,pt	%xcc, \label32+8
	 srl	%sp, 0, %sp ! fixup 32-bit pointers
	NOTREACHED
	TA32
	.endm

	! fill a 64-bit user register window
	.macro UFILL64 label, as
\label:
	wr	%g0, \as, %asi
	FILL	ldxa, %sp+BIAS, 8, %asi
	GET_CPCB(%g5)
	ldx	[%g5 + PCB_WCOOKIE], %g5
	xor	%g5, %i7, %i7		! stackghost
	restored
	CLRTT 3
	retry
	NOTREACHED
	TA32
	.endm

	! fill a 64-bit kernel register window
	.macro FILL64 label, as
\label:
	wr	%g0, \as, %asi
	FILL	ldxa, %sp+BIAS, 8, %asi
	restored
	CLRTT 3
	retry
	NOTREACHED
	TA32
	.endm

	! fill a 32-bit register window
	.macro FILL32 label, as
\label:
	wr	%g0, \as, %asi
	srl	%sp, 0, %sp ! fixup 32-bit pointers
	FILL	lda, %sp, 4, %asi
	restored
	CLRTT 4
	retry
	NOTREACHED
	TA32
	.endm

	! fill either 32-bit or 64-bit register window.
	.macro FILLBOTH label64,label32, as
	andcc	%sp, 1, %i0
	bnz	(\label64)+4 ! See if it's a v9 stack or v8
	 wr	%g0, \as, %asi
	ba	(\label32)+8
	 srl	%sp, 0, %sp ! fixup 32-bit pointers
	NOTREACHED
	TA32
	.endm

#ifdef SUN4V

	.macro	sun4v_tl0_reserved	count
	.rept	\count
	ba,a,pt	%xcc, slowtrap
	 nop
	.align	32
	.endr
	.endm

#define sun4v_tl0_unused sun4v_tl0_reserved

	.macro	sun4v_tl1_reserved	count
	.rept	\count
	ba,a,pt	%xcc, slowtrap
	 nop
	.align	32
	.endr
	.endm

#define sun4v_tl1_unused sun4v_tl1_reserved

	.macro sun4v_tl1_kspill_normal
	ba,a,pt	%xcc,kspill_normal
	 nop
	.align 128
	.endm

	.macro sun4v_tl1_uspill_normal
	ba,a,pt	%xcc,pcbspill_normals
	 nop
	.align 128
	.endm

	.macro sun4v_tl1_uspill_other
	ba,a,pt	%xcc,pcbspill_others
	 nop
	.align 128
	.endm

#endif

	.globl	start, _C_LABEL(kernel_text)
	_C_LABEL(kernel_text) = start		! for kvm_mkdb(8)
start:
	/* Traps from TL=0 -- traps from user mode */
	.globl	_C_LABEL(trapbase)
_C_LABEL(trapbase):
	b dostart; nop; TA8	! 000 = reserved -- Use it to boot
	/* We should not get the next 5 traps */
	UTRAP 0x001		! 001 = POR Reset -- ROM should get this
	UTRAP 0x002		! 002 = WDR -- ROM should get this
	UTRAP 0x003		! 003 = XIR -- ROM should get this
	UTRAP 0x004		! 004 = SIR -- ROM should get this
	UTRAP 0x005		! 005 = RED state exception
	UTRAP 0x006; UTRAP 0x007
	VTRAP T_INST_EXCEPT, textfault	! 008 = instr. access exept
	VTRAP T_TEXTFAULT, textfault	! 009 = instr. access MMU miss
	VTRAP T_INST_ERROR, textfault	! 00a = instr. access err
	UTRAP 0x00b; UTRAP 0x00c; UTRAP 0x00d; UTRAP 0x00e; UTRAP 0x00f
	TRAP T_ILLINST			! 010 = illegal instruction
	TRAP T_PRIVINST		! 011 = privileged instruction
	UTRAP 0x012			! 012 = unimplemented LDD
	UTRAP 0x013			! 013 = unimplemented STD
	UTRAP 0x014; UTRAP 0x015; UTRAP 0x016; UTRAP 0x017; UTRAP 0x018
	UTRAP 0x019; UTRAP 0x01a; UTRAP 0x01b; UTRAP 0x01c; UTRAP 0x01d
	UTRAP 0x01e; UTRAP 0x01f
	TRAP T_FPDISABLED		! 020 = fp instr, but EF bit off in psr
	TRAP T_FP_IEEE_754		! 021 = ieee 754 exception
	TRAP T_FP_OTHER			! 022 = other fp exception
	TRAP T_TAGOF			! 023 = tag overflow
	UCLEANWIN			! 024-027 = clean window trap
	TRAP T_DIV0			! 028 = divide by zero
	UTRAP 0x029			! 029 = internal processor error
	UTRAP 0x02a; UTRAP 0x02b; UTRAP 0x02c; UTRAP 0x02d; UTRAP 0x02e; UTRAP 0x02f
	VTRAP T_DATAFAULT, datafault	! 030 = data fetch fault
	UTRAP 0x031			! 031 = data MMU miss -- no MMU
	VTRAP T_DATA_ERROR, datafault	! 032 = data access error
	VTRAP T_DATA_PROT, datafault	! 033 = data protection fault
	TRAP T_ALIGN			! 034 = address alignment error -- we could fix it inline...
	TRAP T_LDDF_ALIGN		! 035 = LDDF address alignment error -- we could fix it inline...
	TRAP T_STDF_ALIGN		! 036 = STDF address alignment error -- we could fix it inline...
	TRAP T_PRIVACT			! 037 = privileged action
	TRAP T_LDQF_ALIGN		! 038 = LDDF address alignment error
	TRAP T_STQF_ALIGN		! 039 = STQF address alignment error
	UTRAP 0x03a; UTRAP 0x03b; UTRAP 0x03c;
	UTRAP 0x03d; UTRAP 0x03e; UTRAP 0x03f;
	VTRAP T_ASYNC_ERROR, datafault	! 040 = data fetch fault
	SOFTINT4U 1, IE_L1		! 041 = level 1 interrupt
	HARDINT4U 2			! 042 = level 2 interrupt
	HARDINT4U 3			! 043 = level 3 interrupt
	SOFTINT4U 4, IE_L4		! 044 = level 4 interrupt
	HARDINT4U 5			! 045 = level 5 interrupt
	SOFTINT4U 6, IE_L6		! 046 = level 6 interrupt
	HARDINT4U 7			! 047 = level 7 interrupt
	HARDINT4U 8			! 048 = level 8 interrupt
	HARDINT4U 9			! 049 = level 9 interrupt
	HARDINT4U 10			! 04a = level 10 interrupt
	HARDINT4U 11			! 04b = level 11 interrupt
	ZS_INTERRUPT4U			! 04c = level 12 (zs) interrupt
	HARDINT4U 13			! 04d = level 13 interrupt
	HARDINT4U 14			! 04e = level 14 interrupt
	HARDINT4U 15			! 04f = nonmaskable interrupt
	UTRAP 0x050; UTRAP 0x051; UTRAP 0x052; UTRAP 0x053; UTRAP 0x054; UTRAP 0x055
	UTRAP 0x056; UTRAP 0x057; UTRAP 0x058; UTRAP 0x059; UTRAP 0x05a; UTRAP 0x05b
	UTRAP 0x05c; UTRAP 0x05d; UTRAP 0x05e; UTRAP 0x05f
	VTRAP 0x060, interrupt_vector; ! 060 = interrupt vector
	TRAP T_PA_WATCHPT		! 061 = physical address data watchpoint
	TRAP T_VA_WATCHPT		! 062 = virtual address data watchpoint
	VTRAP T_ECCERR, cecc_catch	! 063 = Correctable ECC error
ufast_IMMU_miss:			! 064 = fast instr access MMU miss
	IMMU_MISS 6
ufast_DMMU_miss:			! 068 = fast data access MMU miss
	DMMU_MISS 7
ufast_DMMU_protection:			! 06c = fast data access MMU protection
	DMMU_PROT udprot
	UTRAP 0x070			! Implementation dependent traps
	UTRAP 0x071; UTRAP 0x072; UTRAP 0x073; UTRAP 0x074; UTRAP 0x075; UTRAP 0x076
	UTRAP 0x077; UTRAP 0x078; UTRAP 0x079; UTRAP 0x07a; UTRAP 0x07b; UTRAP 0x07c
	UTRAP 0x07d; UTRAP 0x07e; UTRAP 0x07f
user_uspill:
	USPILL64 uspill8,ASI_AIUS	! 0x080 spill_0_normal -- used to save user windows in user mode
	SPILL32 uspill4,ASI_AIUS	! 0x084 spill_1_normal
	SPILLBOTH uspill8,uspill4,ASI_AIUS		! 0x088 spill_2_normal
#ifdef DEBUG
	sir
#endif	/* DEBUG */
	UTRAP 0x08c; TA32	! 0x08c spill_3_normal
user_kspill:
	UTRAP 0x090; TA32	! 0x090 spill_4_normal -- used to save supervisor windows
	SPILL64 kspill8,ASI_N	! 0x094 spill_5_normal
	SPILL32 kspill4,ASI_N	! 0x098 spill_6_normal
	SPILLBOTH kspill8,kspill4,ASI_N	! 0x09c spill_7_normal
user_uspillk:
	USPILL64 uspillk8,ASI_AIUS	! 0x0a0 spill_0_other -- used to save user windows in supervisor mode
	SPILL32 uspillk4,ASI_AIUS	! 0x0a4 spill_1_other
	SPILLBOTH uspillk8,uspillk4,ASI_AIUS	! 0x0a8 spill_2_other
	UTRAP 0x0ac; TA32	! 0x0ac spill_3_other
	UTRAP 0x0b0; TA32	! 0x0b0 spill_4_other
	UTRAP 0x0b4; TA32	! 0x0b4 spill_5_other
	UTRAP 0x0b8; TA32	! 0x0b8 spill_6_other
	UTRAP 0x0bc; TA32	! 0x0bc spill_7_other
user_ufill:
	UFILL64 ufill8,ASI_AIUS ! 0x0c0 fill_0_normal -- used to fill windows when running user mode
	FILL32 ufill4,ASI_AIUS	! 0x0c4 fill_1_normal
	FILLBOTH ufill8,ufill4,ASI_AIUS	! 0x0c8 fill_2_normal
	UTRAP 0x0cc; TA32	! 0x0cc fill_3_normal
user_kfill:
	UTRAP 0x0d0; TA32	! 0x0d0 fill_4_normal -- used to fill windows when running supervisor mode
	FILL64 kfill8,ASI_N	! 0x0d4 fill_5_normal
	FILL32 kfill4,ASI_N	! 0x0d8 fill_6_normal
	FILLBOTH kfill8,kfill4,ASI_N	! 0x0dc fill_7_normal
user_ufillk:
	UFILL64 ufillk8,ASI_AIUS	! 0x0e0 fill_0_other
	FILL32 ufillk4,ASI_AIUS	! 0x0e4 fill_1_other
	FILLBOTH ufillk8,ufillk4,ASI_AIUS	! 0x0e8 fill_2_other
	UTRAP 0x0ec; TA32	! 0x0ec fill_3_other
	UTRAP 0x0f0; TA32	! 0x0f0 fill_4_other
	UTRAP 0x0f4; TA32	! 0x0f4 fill_5_other
	UTRAP 0x0f8; TA32	! 0x0f8 fill_6_other
	UTRAP 0x0fc; TA32	! 0x0fc fill_7_other
user_syscall:
	SYSCALL			! 0x100 = sun syscall
	TRAP T_BREAKPOINT	! 0x101 = pseudo breakpoint instruction
	STRAP 0x102; STRAP 0x103; STRAP 0x104; STRAP 0x105; STRAP 0x106; STRAP 0x107
	SYSCALL			! 0x108 = svr4 syscall
	SYSCALL			! 0x109 = bsd syscall
	TRAP T_KGDB_EXEC	! 0x10a = enter kernel gdb on kernel startup
	STRAP 0x10b; STRAP 0x10c; STRAP 0x10d; STRAP 0x10e; STRAP 0x10f;
	STRAP 0x110; STRAP 0x111; STRAP 0x112; STRAP 0x113; STRAP 0x114; STRAP 0x115; STRAP 0x116; STRAP 0x117
	STRAP 0x118; STRAP 0x119; STRAP 0x11a; STRAP 0x11b; STRAP 0x11c; STRAP 0x11d; STRAP 0x11e; STRAP 0x11f
	STRAP 0x120; STRAP 0x121; STRAP 0x122; STRAP 0x123; STRAP 0x124; STRAP 0x125; STRAP 0x126; STRAP 0x127
	STRAP 0x128; STRAP 0x129; STRAP 0x12a; STRAP 0x12b; STRAP 0x12c; STRAP 0x12d; STRAP 0x12e; STRAP 0x12f
	STRAP 0x130; STRAP 0x131; STRAP 0x132; STRAP 0x133; STRAP 0x134; STRAP 0x135; STRAP 0x136; STRAP 0x137
	STRAP 0x138; STRAP 0x139; STRAP 0x13a; STRAP 0x13b; STRAP 0x13c; STRAP 0x13d; STRAP 0x13e; STRAP 0x13f
	SYSCALL			! 0x140 SVID syscall (Solaris 2.7)
	SYSCALL			! 0x141 SPARC International syscall
	SYSCALL			! 0x142	OS Vendor syscall
	SYSCALL			! 0x143 HW OEM syscall
	STRAP 0x144; STRAP 0x145; STRAP 0x146; STRAP 0x147
	STRAP 0x148; STRAP 0x149; STRAP 0x14a; STRAP 0x14b; STRAP 0x14c; STRAP 0x14d; STRAP 0x14e; STRAP 0x14f
	STRAP 0x150; STRAP 0x151; STRAP 0x152; STRAP 0x153; STRAP 0x154; STRAP 0x155; STRAP 0x156; STRAP 0x157
	STRAP 0x158; STRAP 0x159; STRAP 0x15a; STRAP 0x15b; STRAP 0x15c; STRAP 0x15d; STRAP 0x15e; STRAP 0x15f
	STRAP 0x160; STRAP 0x161; STRAP 0x162; STRAP 0x163; STRAP 0x164; STRAP 0x165; STRAP 0x166; STRAP 0x167
	STRAP 0x168; STRAP 0x169; STRAP 0x16a; STRAP 0x16b; STRAP 0x16c; STRAP 0x16d; STRAP 0x16e; STRAP 0x16f
	STRAP 0x170; STRAP 0x171; STRAP 0x172; STRAP 0x173; STRAP 0x174; STRAP 0x175; STRAP 0x176; STRAP 0x177
	STRAP 0x178; STRAP 0x179; STRAP 0x17a; STRAP 0x17b; STRAP 0x17c; STRAP 0x17d; STRAP 0x17e; STRAP 0x17f
	! Traps beyond 0x17f are reserved
	UTRAP 0x180; UTRAP 0x181; UTRAP 0x182; UTRAP 0x183; UTRAP 0x184; UTRAP 0x185; UTRAP 0x186; UTRAP 0x187
	UTRAP 0x188; UTRAP 0x189; UTRAP 0x18a; UTRAP 0x18b; UTRAP 0x18c; UTRAP 0x18d; UTRAP 0x18e; UTRAP 0x18f
	UTRAP 0x190; UTRAP 0x191; UTRAP 0x192; UTRAP 0x193; UTRAP 0x194; UTRAP 0x195; UTRAP 0x196; UTRAP 0x197
	UTRAP 0x198; UTRAP 0x199; UTRAP 0x19a; UTRAP 0x19b; UTRAP 0x19c; UTRAP 0x19d; UTRAP 0x19e; UTRAP 0x19f
	UTRAP 0x1a0; UTRAP 0x1a1; UTRAP 0x1a2; UTRAP 0x1a3; UTRAP 0x1a4; UTRAP 0x1a5; UTRAP 0x1a6; UTRAP 0x1a7
	UTRAP 0x1a8; UTRAP 0x1a9; UTRAP 0x1aa; UTRAP 0x1ab; UTRAP 0x1ac; UTRAP 0x1ad; UTRAP 0x1ae; UTRAP 0x1af
	UTRAP 0x1b0; UTRAP 0x1b1; UTRAP 0x1b2; UTRAP 0x1b3; UTRAP 0x1b4; UTRAP 0x1b5; UTRAP 0x1b6; UTRAP 0x1b7
	UTRAP 0x1b8; UTRAP 0x1b9; UTRAP 0x1ba; UTRAP 0x1bb; UTRAP 0x1bc; UTRAP 0x1bd; UTRAP 0x1be; UTRAP 0x1bf
	UTRAP 0x1c0; UTRAP 0x1c1; UTRAP 0x1c2; UTRAP 0x1c3; UTRAP 0x1c4; UTRAP 0x1c5; UTRAP 0x1c6; UTRAP 0x1c7
	UTRAP 0x1c8; UTRAP 0x1c9; UTRAP 0x1ca; UTRAP 0x1cb; UTRAP 0x1cc; UTRAP 0x1cd; UTRAP 0x1ce; UTRAP 0x1cf
	UTRAP 0x1d0; UTRAP 0x1d1; UTRAP 0x1d2; UTRAP 0x1d3; UTRAP 0x1d4; UTRAP 0x1d5; UTRAP 0x1d6; UTRAP 0x1d7
	UTRAP 0x1d8; UTRAP 0x1d9; UTRAP 0x1da; UTRAP 0x1db; UTRAP 0x1dc; UTRAP 0x1dd; UTRAP 0x1de; UTRAP 0x1df
	UTRAP 0x1e0; UTRAP 0x1e1; UTRAP 0x1e2; UTRAP 0x1e3; UTRAP 0x1e4; UTRAP 0x1e5; UTRAP 0x1e6; UTRAP 0x1e7
	UTRAP 0x1e8; UTRAP 0x1e9; UTRAP 0x1ea; UTRAP 0x1eb; UTRAP 0x1ec; UTRAP 0x1ed; UTRAP 0x1ee; UTRAP 0x1ef
	UTRAP 0x1f0; UTRAP 0x1f1; UTRAP 0x1f2; UTRAP 0x1f3; UTRAP 0x1f4; UTRAP 0x1f5; UTRAP 0x1f6; UTRAP 0x1f7
	UTRAP 0x1f8; UTRAP 0x1f9; UTRAP 0x1fa; UTRAP 0x1fb; UTRAP 0x1fc; UTRAP 0x1fd; UTRAP 0x1fe; UTRAP 0x1ff

	/* Traps from TL>0 -- traps from supervisor mode */
trapbase_priv:
	UTRAP 0x000		! 000 = reserved -- Use it to boot
	/* We should not get the next 5 traps */
	UTRAP 0x001		! 001 = POR Reset -- ROM should get this
	UTRAP 0x002		! 002 = WDR Watchdog -- ROM should get this
	UTRAP 0x003		! 003 = XIR -- ROM should get this
	UTRAP 0x004		! 004 = SIR -- ROM should get this
	UTRAP 0x005		! 005 = RED state exception
	UTRAP 0x006; UTRAP 0x007
ktextfault:
	VTRAP T_INST_EXCEPT, textfault	! 008 = instr. access exept
	VTRAP T_TEXTFAULT, textfault	! 009 = instr. access MMU miss -- no MMU
	VTRAP T_INST_ERROR, textfault	! 00a = instr. access err
	UTRAP 0x00b; UTRAP 0x00c; UTRAP 0x00d; UTRAP 0x00e; UTRAP 0x00f
	TRAP T_ILLINST			! 010 = illegal instruction
	TRAP T_PRIVINST		! 011 = privileged instruction
	UTRAP 0x012			! 012 = unimplemented LDD
	UTRAP 0x013			! 013 = unimplemented STD
	UTRAP 0x014; UTRAP 0x015; UTRAP 0x016; UTRAP 0x017; UTRAP 0x018
	UTRAP 0x019; UTRAP 0x01a; UTRAP 0x01b; UTRAP 0x01c; UTRAP 0x01d
	UTRAP 0x01e; UTRAP 0x01f
	TRAP T_FPDISABLED		! 020 = fp instr, but EF bit off in psr
	TRAP T_FP_IEEE_754		! 021 = ieee 754 exception
	TRAP T_FP_OTHER			! 022 = other fp exception
	TRAP T_TAGOF			! 023 = tag overflow
	KCLEANWIN			! 024-027 = clean window trap
	TRAP T_DIV0			! 028 = divide by zero
	UTRAP 0x029			! 029 = internal processor error
	UTRAP 0x02a; UTRAP 0x02b; UTRAP 0x02c; UTRAP 0x02d; UTRAP 0x02e; UTRAP 0x02f
kdatafault:
	VTRAP T_DATAFAULT, winfault	! 030 = data fetch fault
	UTRAP 0x031			! 031 = data MMU miss -- no MMU
	VTRAP T_DATA_ERROR, winfault	! 032 = data fetch fault
	VTRAP T_DATA_PROT, winfault	! 033 = data fetch fault
	VTRAP T_ALIGN, checkalign	! 034 = address alignment error -- we could fix it inline...
!	sir; nop; TA8	! DEBUG -- trap all kernel alignment errors
	TRAP T_LDDF_ALIGN		! 035 = LDDF address alignment error -- we could fix it inline...
	TRAP T_STDF_ALIGN		! 036 = STDF address alignment error -- we could fix it inline...
	TRAP T_PRIVACT			! 037 = privileged action
	UTRAP 0x038; UTRAP 0x039; UTRAP 0x03a; UTRAP 0x03b; UTRAP 0x03c;
	UTRAP 0x03d; UTRAP 0x03e; UTRAP 0x03f;
	VTRAP T_ASYNC_ERROR, winfault	! 040 = data fetch fault
	SOFTINT4U 1, IE_L1		! 041 = level 1 interrupt
	HARDINT4U 2			! 042 = level 2 interrupt
	HARDINT4U 3			! 043 = level 3 interrupt
	SOFTINT4U 4, IE_L4		! 044 = level 4 interrupt
	HARDINT4U 5			! 045 = level 5 interrupt
	SOFTINT4U 6, IE_L6		! 046 = level 6 interrupt
	HARDINT4U 7			! 047 = level 7 interrupt
	HARDINT4U 8			! 048 = level 8 interrupt
	HARDINT4U 9			! 049 = level 9 interrupt
	HARDINT4U 10			! 04a = level 10 interrupt
	HARDINT4U 11			! 04b = level 11 interrupt
	ZS_INTERRUPT4U			! 04c = level 12 (zs) interrupt
	HARDINT4U 13			! 04d = level 13 interrupt
	HARDINT4U 14			! 04e = level 14 interrupt
	HARDINT4U 15			! 04f = nonmaskable interrupt
	UTRAP 0x050; UTRAP 0x051; UTRAP 0x052; UTRAP 0x053; UTRAP 0x054; UTRAP 0x055
	UTRAP 0x056; UTRAP 0x057; UTRAP 0x058; UTRAP 0x059; UTRAP 0x05a; UTRAP 0x05b
	UTRAP 0x05c; UTRAP 0x05d; UTRAP 0x05e; UTRAP 0x05f
	VTRAP 0x060, interrupt_vector; ! 060 = interrupt vector
	TRAP T_PA_WATCHPT		! 061 = physical address data watchpoint
	TRAP T_VA_WATCHPT		! 062 = virtual address data watchpoint
	VTRAP T_ECCERR, cecc_catch	! 063 = Correctable ECC error
kfast_IMMU_miss:			! 064 = fast instr access MMU miss
	IMMU_MISS 9
kfast_DMMU_miss:			! 068 = fast data access MMU miss
	DMMU_MISS 10
kfast_DMMU_protection:			! 06c = fast data access MMU protection
	DMMU_PROT kdprot
	UTRAP 0x070			! Implementation dependent traps
	UTRAP 0x071; UTRAP 0x072; UTRAP 0x073; UTRAP 0x074; UTRAP 0x075; UTRAP 0x076
	UTRAP 0x077; UTRAP 0x078; UTRAP 0x079; UTRAP 0x07a; UTRAP 0x07b; UTRAP 0x07c
	UTRAP 0x07d; UTRAP 0x07e; UTRAP 0x07f
nucleus_uspill:
	USPILL64 1,ASI_AIUS	! 0x080 spill_0_normal -- used to save user windows
	SPILL32 2,ASI_AIUS	! 0x084 spill_1_normal
	SPILLBOTH 1b,2b,ASI_AIUS	! 0x088 spill_2_normal
	UTRAP 0x08c; TA32	! 0x08c spill_3_normal
nucleus_kspill:
	UTRAP 0x090; TA32	! 0x090 spill_4_normal -- used to save supervisor windows
	SPILL64 1,ASI_N		! 0x094 spill_5_normal
	SPILL32 2,ASI_N		! 0x098 spill_6_normal
	SPILLBOTH 1b,2b,ASI_N	! 0x09c spill_7_normal
nucleus_uspillk:
	USPILL64 1,ASI_AIUS	! 0x0a0 spill_0_other -- used to save user windows in nucleus mode
	SPILL32 2,ASI_AIUS	! 0x0a4 spill_1_other
	SPILLBOTH 1b,2b,ASI_AIUS	! 0x0a8 spill_2_other
	UTRAP 0x0ac; TA32	! 0x0ac spill_3_other
	UTRAP 0x0b0; TA32	! 0x0b0 spill_4_other
	UTRAP 0x0b4; TA32	! 0x0b4 spill_5_other
	UTRAP 0x0b8; TA32	! 0x0b8 spill_6_other
	UTRAP 0x0bc; TA32	! 0x0bc spill_7_other
nucleus_ufill:
	UFILL64 1,ASI_AIUS	! 0x0c0 fill_0_normal -- used to fill windows when running nucleus mode from user
	FILL32 2,ASI_AIUS	! 0x0c4 fill_1_normal
	FILLBOTH 1b,2b,ASI_AIUS	! 0x0c8 fill_2_normal
	UTRAP 0x0cc; TA32	! 0x0cc fill_3_normal
nucleus_sfill:
	UTRAP 0x0d0; TA32	! 0x0d0 fill_4_normal -- used to fill windows when running nucleus mode from supervisor
	FILL64 1,ASI_N		! 0x0d4 fill_5_normal
	FILL32 2,ASI_N		! 0x0d8 fill_6_normal
	FILLBOTH 1b,2b,ASI_N	! 0x0dc fill_7_normal
nucleus_kfill:
	UFILL64 1,ASI_AIUS	! 0x0e0 fill_0_other -- used to fill user windows when running nucleus mode -- will we ever use this?
	FILL32 2,ASI_AIUS	! 0x0e4 fill_1_other
	FILLBOTH 1b,2b,ASI_AIUS	! 0x0e8 fill_2_other
	UTRAP 0x0ec; TA32	! 0x0ec fill_3_other
	UTRAP 0x0f0; TA32	! 0x0f0 fill_4_other
	UTRAP 0x0f4; TA32	! 0x0f4 fill_5_other
	UTRAP 0x0f8; TA32	! 0x0f8 fill_6_other
	UTRAP 0x0fc; TA32	! 0x0fc fill_7_other
nucleus_syscall:
	SYSCALL			! 0x100 = sun syscall
	TRAP T_BREAKPOINT	! 0x101 = pseudo breakpoint instruction
	STRAP 0x102; STRAP 0x103; STRAP 0x104; STRAP 0x105; STRAP 0x106; STRAP 0x107
	SYSCALL			! 0x108 = svr4 syscall
	SYSCALL			! 0x109 = bsd syscall
	TRAP T_KGDB_EXEC	! 0x10a = enter kernel gdb on kernel startup
	STRAP 0x10b; STRAP 0x10c; STRAP 0x10d; STRAP 0x10e; STRAP 0x10f;
	STRAP 0x110; STRAP 0x111; STRAP 0x112; STRAP 0x113; STRAP 0x114; STRAP 0x115; STRAP 0x116; STRAP 0x117
	STRAP 0x118; STRAP 0x119; STRAP 0x11a; STRAP 0x11b; STRAP 0x11c; STRAP 0x11d; STRAP 0x11e; STRAP 0x11f
	STRAP 0x120; STRAP 0x121; STRAP 0x122; STRAP 0x123; STRAP 0x124; STRAP 0x125; STRAP 0x126; STRAP 0x127
	STRAP 0x128; STRAP 0x129; STRAP 0x12a; STRAP 0x12b; STRAP 0x12c; STRAP 0x12d; STRAP 0x12e; STRAP 0x12f
	STRAP 0x130; STRAP 0x131; STRAP 0x132; STRAP 0x133; STRAP 0x134; STRAP 0x135; STRAP 0x136; STRAP 0x137
	STRAP 0x138; STRAP 0x139; STRAP 0x13a; STRAP 0x13b; STRAP 0x13c; STRAP 0x13d; STRAP 0x13e; STRAP 0x13f
	STRAP 0x140; STRAP 0x141; STRAP 0x142; STRAP 0x143; STRAP 0x144; STRAP 0x145; STRAP 0x146; STRAP 0x147
	STRAP 0x148; STRAP 0x149; STRAP 0x14a; STRAP 0x14b; STRAP 0x14c; STRAP 0x14d; STRAP 0x14e; STRAP 0x14f
	STRAP 0x150; STRAP 0x151; STRAP 0x152; STRAP 0x153; STRAP 0x154; STRAP 0x155; STRAP 0x156; STRAP 0x157
	STRAP 0x158; STRAP 0x159; STRAP 0x15a; STRAP 0x15b; STRAP 0x15c; STRAP 0x15d; STRAP 0x15e; STRAP 0x15f
	STRAP 0x160; STRAP 0x161; STRAP 0x162; STRAP 0x163; STRAP 0x164; STRAP 0x165; STRAP 0x166; STRAP 0x167
	STRAP 0x168; STRAP 0x169; STRAP 0x16a; STRAP 0x16b; STRAP 0x16c; STRAP 0x16d; STRAP 0x16e; STRAP 0x16f
	STRAP 0x170; STRAP 0x171; STRAP 0x172; STRAP 0x173; STRAP 0x174; STRAP 0x175; STRAP 0x176; STRAP 0x177
	STRAP 0x178; STRAP 0x179; STRAP 0x17a; STRAP 0x17b; STRAP 0x17c; STRAP 0x17d; STRAP 0x17e; STRAP 0x17f
	! Traps beyond 0x17f are reserved
	UTRAP 0x180; UTRAP 0x181; UTRAP 0x182; UTRAP 0x183; UTRAP 0x184; UTRAP 0x185; UTRAP 0x186; UTRAP 0x187
	UTRAP 0x188; UTRAP 0x189; UTRAP 0x18a; UTRAP 0x18b; UTRAP 0x18c; UTRAP 0x18d; UTRAP 0x18e; UTRAP 0x18f
	UTRAP 0x190; UTRAP 0x191; UTRAP 0x192; UTRAP 0x193; UTRAP 0x194; UTRAP 0x195; UTRAP 0x196; UTRAP 0x197
	UTRAP 0x198; UTRAP 0x199; UTRAP 0x19a; UTRAP 0x19b; UTRAP 0x19c; UTRAP 0x19d; UTRAP 0x19e; UTRAP 0x19f
	UTRAP 0x1a0; UTRAP 0x1a1; UTRAP 0x1a2; UTRAP 0x1a3; UTRAP 0x1a4; UTRAP 0x1a5; UTRAP 0x1a6; UTRAP 0x1a7
	UTRAP 0x1a8; UTRAP 0x1a9; UTRAP 0x1aa; UTRAP 0x1ab; UTRAP 0x1ac; UTRAP 0x1ad; UTRAP 0x1ae; UTRAP 0x1af
	UTRAP 0x1b0; UTRAP 0x1b1; UTRAP 0x1b2; UTRAP 0x1b3; UTRAP 0x1b4; UTRAP 0x1b5; UTRAP 0x1b6; UTRAP 0x1b7
	UTRAP 0x1b8; UTRAP 0x1b9; UTRAP 0x1ba; UTRAP 0x1bb; UTRAP 0x1bc; UTRAP 0x1bd; UTRAP 0x1be; UTRAP 0x1bf
	UTRAP 0x1c0; UTRAP 0x1c1; UTRAP 0x1c2; UTRAP 0x1c3; UTRAP 0x1c4; UTRAP 0x1c5; UTRAP 0x1c6; UTRAP 0x1c7
	UTRAP 0x1c8; UTRAP 0x1c9; UTRAP 0x1ca; UTRAP 0x1cb; UTRAP 0x1cc; UTRAP 0x1cd; UTRAP 0x1ce; UTRAP 0x1cf
	UTRAP 0x1d0; UTRAP 0x1d1; UTRAP 0x1d2; UTRAP 0x1d3; UTRAP 0x1d4; UTRAP 0x1d5; UTRAP 0x1d6; UTRAP 0x1d7
	UTRAP 0x1d8; UTRAP 0x1d9; UTRAP 0x1da; UTRAP 0x1db; UTRAP 0x1dc; UTRAP 0x1dd; UTRAP 0x1de; UTRAP 0x1df
	UTRAP 0x1e0; UTRAP 0x1e1; UTRAP 0x1e2; UTRAP 0x1e3; UTRAP 0x1e4; UTRAP 0x1e5; UTRAP 0x1e6; UTRAP 0x1e7
	UTRAP 0x1e8; UTRAP 0x1e9; UTRAP 0x1ea; UTRAP 0x1eb; UTRAP 0x1ec; UTRAP 0x1ed; UTRAP 0x1ee; UTRAP 0x1ef
	UTRAP 0x1f0; UTRAP 0x1f1; UTRAP 0x1f2; UTRAP 0x1f3; UTRAP 0x1f4; UTRAP 0x1f5; UTRAP 0x1f6; UTRAP 0x1f7
	UTRAP 0x1f8; UTRAP 0x1f9; UTRAP 0x1fa; UTRAP 0x1fb; UTRAP 0x1fc; UTRAP 0x1fd; UTRAP 0x1fe; UTRAP 0x1ff

#ifdef SUN4V

	.align	0x8000
	.globl	_C_LABEL(trapbase_sun4v)
_C_LABEL(trapbase_sun4v):
	sun4v_tl0_reserved 8				! 0x0-0x7
	VTRAP T_INST_EXCEPT, sun4v_tl0_itsb_miss	! 0x8
	VTRAP T_TEXTFAULT, sun4v_tl0_itsb_miss		! 0x9
	sun4v_tl0_reserved 6				! 0xa-0xf
	TRAP T_ILLINST					! 0x10
	TRAP T_PRIVINST					! 0x11
	sun4v_tl0_reserved 14				! 0x12-0x1f
	TRAP T_FPDISABLED				! 0x20
	TRAP T_FP_IEEE_754				! 0x21
	TRAP T_FP_OTHER					! 0x22
	TRAP T_TAGOF					! 0x23
	UCLEANWIN					! 0x24-0x27
	TRAP T_DIV0					! 0x28
	sun4v_tl0_reserved 7				! 0x29-0x2f
	VTRAP T_DATAFAULT, sun4v_datatrap		! 0x30
	VTRAP T_DATA_MMU_MISS, sun4v_tl0_dtsb_miss	! 0x31
	sun4v_tl0_reserved 2				! 0x32-0x33
	TRAP T_ALIGN					! 0x34
	TRAP T_LDDF_ALIGN				! 0x35
	TRAP T_STDF_ALIGN				! 0x36
	TRAP T_PRIVACT					! 0x37
	TRAP T_LDQF_ALIGN				! 0x38
	TRAP T_STQF_ALIGN				! 0x39
	sun4v_tl0_reserved 7				! 0x3a-0x40
	HARDINT4U 1					! 0x41
	HARDINT4U 2					! 0x42
	HARDINT4U 3					! 0x43
	HARDINT4U 4					! 0x44
	HARDINT4U 5					! 0x45
	HARDINT4U 6					! 0x46
	HARDINT4U 7					! 0x47
	HARDINT4U 8					! 0x48
	HARDINT4U 9					! 0x49
	HARDINT4U 10					! 0x4a
	HARDINT4U 11					! 0x4b
	HARDINT4U 12					! 0x4c
	HARDINT4U 13					! 0x4d
	HARDINT4U 14					! 0x4e
	HARDINT4U 15					! 0x4f
	sun4v_tl0_reserved 18				! 0x50-0x61
	TRAP T_VA_WATCHPT				! 0x62
	sun4v_tl0_reserved 9				! 0x63-0x6b
	VTRAP 0x6c, sun4v_tl0_dtsb_prot			! 0x6c
	sun4v_tl0_reserved 15				! 0x6d-0x7b
	VTRAP 0x7c, sun4v_cpu_mondo			! 0x7c
	VTRAP 0x7c, sun4v_dev_mondo			! 0x7d
	TRAP 0x7e					! 0x7e
	TRAP 0x7f					! 0x7f
	USPILL64 uspill8v, ASI_AIUS			! 0x80
	SPILL32 uspill4v, ASI_AIUS			! 0x84
	SPILLBOTH uspill8v,uspill4v, ASI_AIUS		! 0x88
	sun4v_tl0_unused 4				! 0x8c
	sun4v_tl0_unused 4				! 0x90
	SPILL64 kspill8v, ASI_N				! 0x94
	SPILL32 kspill4v, ASI_N				! 0x98
	SPILLBOTH kspill8v, kspill4v, ASI_N		! 0x9c
	USPILL64 uspillk8v, ASI_AIUS			! 0xa0
	SPILL32 uspillk4v, ASI_AIUS			! 0xa4
	SPILLBOTH uspillk8v, uspillk4v, ASI_AIUS	! 0xa8
	sun4v_tl0_unused 4				! 0xac
	sun4v_tl0_unused 16				! 0xb0-0xbc
	UFILL64 ufill8v, ASI_AIUS			! 0xc0
	FILL32 ufill4v, ASI_AIUS			! 0xc4
	FILLBOTH ufill8v, ufill4v, ASI_AIUS		! 0xc8
	sun4v_tl0_unused 4				! 0xcc
	sun4v_tl0_unused 4				! 0xd0
	FILL64 kfill8v, ASI_N				! 0xd4
	FILL32 kfill4v, ASI_N				! 0xd8
	FILLBOTH kfill8v, kfill4v, ASI_N		! 0xdc
	UFILL64 ufillk8v, ASI_AIUS			! 0xe0
	FILL32 ufillk4v, ASI_AIUS			! 0xe4
	FILLBOTH ufillk8v, ufillk4v, ASI_AIUS		! 0xe8
	sun4v_tl0_unused 4				! 0xef
	sun4v_tl0_unused 16				! 0xf0-0xfc
	SYSCALL						! 0x100
	TRAP T_BREAKPOINT				! 0x101
	sun4v_tl0_unused 6				! 0x102-0x107
	SYSCALL						! 0x108
	SYSCALL						! 0x109
	sun4v_tl0_unused 54				! 0x10a-0x13f
	SYSCALL						! 0x140
	SYSCALL						! 0x141
	SYSCALL						! 0x142
	SYSCALL						! 0x143
	sun4v_tl0_unused 60				! 0x144-0x17f
	sun4v_tl0_reserved 128				! 0x180-0x1ff

	sun4v_tl1_reserved 8				! 0x0-0x7
	TRAP T_INST_EXCEPT				! 0x8
	TRAP T_TEXTFAULT				! 0x9
	sun4v_tl1_reserved 6				! 0xa-0xf
	TRAP T_ILLINST					! 0x10
	TRAP T_PRIVINST					! 0x11
	sun4v_tl1_reserved 14				! 0x12-0x1f
	TRAP T_FPDISABLED				! 0x20
	TRAP T_FP_IEEE_754				! 0x21
	TRAP T_FP_OTHER					! 0x22
	TRAP T_TAGOF					! 0x23
	KCLEANWIN					! 0x24-0x27
	TRAP T_DIV0					! 0x28
	sun4v_tl1_reserved 7				! 0x29-0x2f
	VTRAP T_DATAFAULT, sun4v_tl1_ptbl_miss		! 0x30
	VTRAP T_DATA_MMU_MISS, sun4v_tl1_dtsb_miss	! 0x31
	VTRAP T_DATA_ERROR, sun4v_tl1_ptbl_miss
	VTRAP T_DATA_PROT, sun4v_tl1_ptbl_miss
!	sun4v_tl1_reserved 2				! 0x32-0x33
	VTRAP T_ALIGN, sun4v_tl1_ptbl_miss		! 0x34
	TRAP T_LDDF_ALIGN				! 0x35
	TRAP T_STDF_ALIGN				! 0x36
	TRAP T_PRIVACT					! 0x37
	TRAP T_LDQF_ALIGN				! 0x38
	TRAP T_STQF_ALIGN				! 0x39
	sun4v_tl1_reserved 40				! 0x3a-0x61
	TRAP T_VA_WATCHPT				! 0x62
	sun4v_tl1_reserved 9				! 0x63-0x6b
	VTRAP 0x6c, sun4v_tl1_dtsb_prot			! 0x6c
	sun4v_tl1_reserved 19				! 0x6d-0x7f
	sun4v_tl1_uspill_normal				! 0x80
	sun4v_tl1_uspill_normal				! 0x84
	sun4v_tl1_uspill_normal				! 0x88
	sun4v_tl1_unused 4				! 0x8c
	sun4v_tl1_unused 4				! 0x90
	sun4v_tl1_kspill_normal				! 0x94
	sun4v_tl1_kspill_normal				! 0x98
	sun4v_tl1_kspill_normal				! 0x9c
	sun4v_tl1_uspill_other				! 0xa0
	sun4v_tl1_uspill_other				! 0xa4
	sun4v_tl1_uspill_other				! 0xa8
	sun4v_tl1_unused 4				! 0xac
	sun4v_tl1_unused 16				! 0xb0-0xbc
	sun4v_tl1_unused 64				! 0xc0-0xfc
	sun4v_tl1_unused 128
	sun4v_tl1_reserved 128

#endif

#ifdef DEBUG
	.macro CHKREG r
	ldx	[%o0 + 8*1], %o1
	cmp	\r, %o1
	stx	%o0, [%o0]
	tne	1
	.endm
	.data
globreg_debug:
	.xword	-1, 0, 0, 0, 0, 0, 0, 0
	.text
globreg_set:
	save	%sp, -CC64FSZ, %sp
	set	globreg_debug, %o0
	.irpc n,01234567
		stx	%g\n, [%o0 + 8*\n]
	.endr
	ret
	 restore
globreg_check:
	save	%sp, -CC64FSZ, %sp
	rd	%pc, %o7
	set	globreg_debug, %o0
	ldx	[%o0], %o1
	brnz,pn	%o1, 1f		! Don't re-execute this
	.irpc n,1234567
		CHKREG %g\n
	.endr
	nop
1:	ret
	 restore

	/*
	 * Checkpoint:	 store a byte value at DATA_START+0x21
	 *		uses two temp regs
	 */
	.macro CHKPT r1,r2,val
	sethi	%hi(DATA_START), \r1
	mov	\val, \r2
	stb	\r2, [\r1 + 0x21]
	.endm

	/*
	 * Debug routine:
	 *
	 * If datafault manages to get an unaligned pmap entry
	 * we come here.  We want to save as many regs as we can.
	 * %g3 has the sfsr, and %g7 the result of the wstate
	 * both of which we can toast w/out much lossage.
	 *
	 */
	.data
pmap_dumpflag:
	.xword	0		! semaphore
	.globl	pmap_dumparea	! Get this into the kernel syms
pmap_dumparea:
	.space	(32*8)		! room to save 32 registers
pmap_edumparea:
	.text
pmap_screwup:
	rd	%pc, %g3
	sub	%g3, (pmap_edumparea-pmap_dumparea), %g3	! pc relative addressing 8^)
	ldstub	[%g3+( 0*0x8)], %g3
	tst	%g3		! Semaphore set?
	tnz	%xcc, 1; nop		! Then trap
	set	pmap_dumparea, %g3
	stx	%g3, [%g3+( 0*0x8)]	! set semaphore
	stx	%g1, [%g3+( 1*0x8)]	! Start saving regs
	stx	%g2, [%g3+( 2*0x8)]
	stx	%g3, [%g3+( 3*0x8)]	! Redundant, I know...
	stx	%g4, [%g3+( 4*0x8)]
	stx	%g5, [%g3+( 5*0x8)]
	stx	%g6, [%g3+( 6*0x8)]
	stx	%g7, [%g3+( 7*0x8)]
	stx	%i0, [%g3+( 8*0x8)]
	stx	%i1, [%g3+( 9*0x8)]
	stx	%i2, [%g3+(10*0x8)]
	stx	%i3, [%g3+(11*0x8)]
	stx	%i4, [%g3+(12*0x8)]
	stx	%i5, [%g3+(13*0x8)]
	stx	%i6, [%g3+(14*0x8)]
	stx	%i7, [%g3+(15*0x8)]
	stx	%l0, [%g3+(16*0x8)]
	stx	%l1, [%g3+(17*0x8)]
	stx	%l2, [%g3+(18*0x8)]
	stx	%l3, [%g3+(19*0x8)]
	stx	%l4, [%g3+(20*0x8)]
	stx	%l5, [%g3+(21*0x8)]
	stx	%l6, [%g3+(22*0x8)]
	stx	%l7, [%g3+(23*0x8)]
	stx	%o0, [%g3+(24*0x8)]
	stx	%o1, [%g3+(25*0x8)]
	stx	%o2, [%g3+(26*0x8)]
	stx	%o3, [%g3+(27*0x8)]
	stx	%o4, [%g3+(28*0x8)]
	stx	%o5, [%g3+(29*0x8)]
	stx	%o6, [%g3+(30*0x8)]
	stx	%o7, [%g3+(31*0x8)]
	ta	1; nop		! Break into the debugger

#else	/* DEBUG */
	.macro CHKPT r1,r2,val
	.endm
#endif	/* DEBUG */

#ifdef DEBUG_NOTDEF
/*
 * A hardware red zone is impossible.  We simulate one in software by
 * keeping a `red zone' pointer; if %sp becomes less than this, we panic.
 * This is expensive and is only enabled when debugging.
 */
#define	REDSIZE	(USIZ)		/* Mark used portion of user structure out of bounds */
#define	REDSTACK 2048		/* size of `panic: stack overflow' region */
	.data
	_ALIGN
redzone:
	.xword	0
redstack:
	.space	REDSTACK
eredstack:
Lpanic_red:
	.asciz	"kernel stack overflow"
	_ALIGN
	.text

	! set stack pointer redzone to base+minstack; alters base
.macro	SET_SP_REDZONE base, tmp
	add	\base, REDSIZE, \base
	sethi	%hi(_C_LABEL(redzone)), \tmp
	stx	\base, [\tmp + %lo(_C_LABEL(redzone))]
	.endm

	! variant with a constant
.macro	SET_SP_REDZONE_CONST const,  tmp1,  tmp2
	set	(\const) + REDSIZE, \tmp1
	sethi	%hi(_C_LABEL(redzone)), \tmp2
	stx	\tmp1, [\tmp2 + %lo(_C_LABEL(redzone))]
	.endm

	! check stack pointer against redzone (uses two temps)
.macro	CHECK_SP_REDZONE t1,  t2
	sethi	KERNBASE, \t1
	cmp	%sp, \t1
	blu,pt	%xcc, 7f
	 sethi	%hi(_C_LABEL(redzone)), \t1
	ldx	[\t1 + %lo(_C_LABEL(redzone))], \t2
	cmp	%sp, \t2	! if sp >= \t2, not in red zone
	blu	panic_red
	nop	! and can continue normally
7:
	.endm

panic_red:
	/* move to panic stack */
	stx	%g0, [t1 + %lo(_C_LABEL(redzone))];
	set	eredstack - BIAS, %sp;
	/* prevent panic() from lowering ipl */
	sethi	%hi(_C_LABEL(panicstr)), t2;
	set	Lpanic_red, t2;
	st	t2, [t1 + %lo(_C_LABEL(panicstr))];
	wrpr	g0, 15, %pil		/* t1 = splhigh() */
	save	%sp, -CCF64SZ, %sp;	/* preserve current window */
	sethi	%hi(Lpanic_red), %o0;
	call	_C_LABEL(panic);
	 or %o0, %lo(Lpanic_red), %o0;


#else	/* DEBUG_NOTDEF */

.macro	SET_SP_REDZONE base, tmp
.endm
.macro	SET_SP_REDZONE_CONST const, t1, t2
.endm
.macro	CHECK_SP_REDZONE t1, t2
.endm
#endif	/* DEBUG_NOTDEF */

#define TRACESIZ	0x01000
	.globl	_C_LABEL(trap_trace)
	.globl	_C_LABEL(trap_trace_ptr)
	.globl	_C_LABEL(trap_trace_end)
	.globl	_C_LABEL(trap_trace_dis)
	.data
_C_LABEL(trap_trace_dis):
	.word	1, 1		! Starts disabled.  DDB turns it on.
_C_LABEL(trap_trace_ptr):
	.word	0, 0, 0, 0
_C_LABEL(trap_trace):
	.space	TRACESIZ
_C_LABEL(trap_trace_end):
	.space	0x20		! safety margin

/*
 * v9 machines do not have a trap window.
 *
 * When we take a trap the trap state is pushed on to the stack of trap
 * registers, interrupts are disabled, then we switch to an alternate set
 * of global registers.
 *
 * The trap handling code needs to allocate a trap frame on the kernel, or
 * for interrupts, the interrupt stack, save the out registers to the trap
 * frame, then switch to the normal globals and save them to the trap frame
 * too.
 *
 * XXX it would be good to save the interrupt stack frame to the kernel
 * stack so we wouldn't have to copy it later if we needed to handle a AST.
 *
 * Since kernel stacks are all on one page and the interrupt stack is entirely
 * within the locked TLB, we can use physical addressing to save out our
 * trap frame so we don't trap during the TRAP_SETUP operation.  There
 * is unfortunately no supportable method for issuing a non-trapping save.
 *
 * However, if we use physical addresses to save our trapframe, we will need
 * to clear out the data cache before continuing much further.
 *
 * In short, what we need to do is:
 *
 *	all preliminary processing is done using the alternate globals
 *
 *	When we allocate our trap windows we must give up our globals because
 *	their state may have changed during the save operation
 *
 *	we need to save our normal globals as soon as we have a stack
 *
 * Finally, we may now call C code.
 *
 * This macro will destroy %g5-%g7.  %g0-%g4 remain unchanged.
 *
 * In order to properly handle nested traps without lossage, alternate
 * global %g6 is used as a kernel stack pointer.  It is set to the last
 * allocated stack pointer (trapframe) and the old value is stored in
 * tf_kstack.  It is restored when returning from a trap.  It is cleared
 * on entering user mode.
 */

 /*
  * Other misc. design criteria:
  *
  * When taking an address fault, fault info is in the sfsr, sfar,
  * TLB_TAG_ACCESS registers.  If we take another address fault
  * while trying to handle the first fault then that information,
  * the only information that tells us what address we trapped on,
  * can potentially be lost.  This trap can be caused when allocating
  * a register window with which to handle the trap because the save
  * may try to store or restore a register window that corresponds
  * to part of the stack that is not mapped.  Preventing this trap,
  * while possible, is much too complicated to do in a trap handler,
  * and then we will need to do just as much work to restore the processor
  * window state.
  *
  * Possible solutions to the problem:
  *
  * Since we have separate AG, MG, and IG, we could have all traps
  * above level-1 preserve AG and use other registers.  This causes
  * a problem for the return from trap code which is coded to use
  * alternate globals only.
  *
  * We could store the trapframe and trap address info to the stack
  * using physical addresses.  Then we need to read it back using
  * physical addressing, or flush the D$.
  *
  * We could identify certain registers to hold address fault info.
  * This means that these registers need to be preserved across all
  * fault handling.  But since we only have 7 useable globals, that
  * really puts a cramp in our style.
  *
  * Finally, there is the issue of returning from kernel mode to user
  * mode.  If we need to issue a restore of a user window in kernel
  * mode, we need the window control registers in a user mode setup.
  * If the trap handlers notice the register windows are in user mode,
  * they will allocate a trapframe at the bottom of the kernel stack,
  * overwriting the frame we were trying to return to.  This means that
  * we must complete the restoration of all registers *before* switching
  * to a user-mode window configuration.
  *
  * Essentially we need to be able to write re-entrant code w/no stack.
  */
	.data
trap_setup_msg:
	.asciz	"TRAP_SETUP: tt=%x osp=%x nsp=%x tl=%x tpc=%x\n"
	_ALIGN
intr_setup_msg:
	.asciz	"INTR_SETUP: tt=%x osp=%x nsp=%x tl=%x tpc=%x\n"
	_ALIGN
	.text

	.macro	TRAP_SETUP stackspace
	GET_CPCB(%g6)
	sethi	%hi((\stackspace)), %g5
	sethi	%hi(USPACE), %g7		! Always multiple of page size
	or	%g5, %lo((\stackspace)), %g5

	sra	%g5, 0, %g5			! Sign extend the damn thing

	add	%g6, %g7, %g6
	rdpr	%wstate, %g7			! Find if we're from user mode

	sub	%g7, WSTATE_KERN, %g7		! Compare & leave in register
	movrz	%g7, %sp, %g6			! Select old (kernel) stack or base of kernel stack
	btst	1, %g6				! Fixup 64-bit stack if necessary
	bnz,pt	%icc, 1f
	 add	%g6, %g5, %g6			! Allocate a stack frame
	inc	-BIAS, %g6
	nop
	nop
1:
	SPILL stx, %g6 + CC64FSZ + BIAS + TF_L, 8, ! save local + in
	save	%g6, 0, %sp			! If we fault we should come right back here
	stx	%i0, [%sp + CC64FSZ + BIAS + TF_O + (0*8)] ! Save out registers to trap frame
	stx	%i1, [%sp + CC64FSZ + BIAS + TF_O + (1*8)]
	stx	%i2, [%sp + CC64FSZ + BIAS + TF_O + (2*8)]
	stx	%i3, [%sp + CC64FSZ + BIAS + TF_O + (3*8)]
	stx	%i4, [%sp + CC64FSZ + BIAS + TF_O + (4*8)]
	stx	%i5, [%sp + CC64FSZ + BIAS + TF_O + (5*8)]

	stx	%i6, [%sp + CC64FSZ + BIAS + TF_O + (6*8)]
	brz,pt	%g7, 1f			! If we were in kernel mode start saving globals
	 stx	%i7, [%sp + CC64FSZ + BIAS + TF_O + (7*8)]

	! came from user mode -- switch to kernel mode stack
	rdpr	%canrestore, %g5		! Fixup register window state registers
	wrpr	%g0, 0, %canrestore
	wrpr	%g0, %g5, %otherwin
	wrpr	%g0, WSTATE_KERN, %wstate	! Enable kernel mode window traps -- now we can trap again

	mov	CTX_PRIMARY, %g7
	SET_MMU_CONTEXTID(%g0, %g7)		! Switch MMU to kernel primary context
	sethi	%hi(KERNBASE), %g5
	membar	#Sync				! XXXX Should be taken care of by flush
	flush	%g5				! Some convenient address that won't trap
1:
	.endm
	
/*
 * Interrupt setup is almost exactly like trap setup, but we need to
 * go to the interrupt stack if (a) we came from user mode or (b) we
 * came from kernel mode on the kernel stack.
 *
 * We don't guarantee that any registers are preserved during this operation,
 * so we can be more efficient.
 */
	.macro	INTR_SETUP stackspace
	rdpr	%wstate, %g7			! Find if we're from user mode

	GET_CPUINFO_VA(%g6)
	sethi	%hi(EINTSTACK-INTSTACK), %g4
	sub	%g6, BIAS, %g6			! Base of interrupt stack
	dec	%g4				! Make it into a mask

	sub	%g6, %sp, %g1			! Offset from interrupt stack
	sethi	%hi((\stackspace)), %g5

	or	%g5, %lo((\stackspace)), %g5

	andn	%g1, %g4, %g4			! Are we out of the interrupt stack range?
	xor	%g7, WSTATE_KERN, %g3

	sra	%g5, 0, %g5			! Sign extend the damn thing
	or	%g3, %g4, %g4			! Definitely not off the interrupt stack

	movrz	%g4, %sp, %g6

	add	%g6, %g5, %g5			! Allocate a stack frame
	btst	1, %g6
	bnz,pt	%icc, 1f
	 mov	%g5, %g6

	add	%g5, -BIAS, %g6

1:
	SPILL stx, %g6 + CC64FSZ + BIAS + TF_L, 8,  ! save local+in to trap frame
	save	%g6, 0, %sp			! If we fault we should come right back here
	stx	%i0, [%sp + CC64FSZ + BIAS + TF_O + (0*8)] ! Save out registers to trap frame
	stx	%i1, [%sp + CC64FSZ + BIAS + TF_O + (1*8)]
	stx	%i2, [%sp + CC64FSZ + BIAS + TF_O + (2*8)]
	stx	%i3, [%sp + CC64FSZ + BIAS + TF_O + (3*8)]
	stx	%i4, [%sp + CC64FSZ + BIAS + TF_O + (4*8)]

	stx	%i5, [%sp + CC64FSZ + BIAS + TF_O + (5*8)]
	stx	%i6, [%sp + CC64FSZ + BIAS + TF_O + (6*8)]
	brz,pt	%g3, 1f				! If we were in kernel mode start saving globals
	 stx	%i7, [%sp + CC64FSZ + BIAS + TF_O + (7*8)]

	! came from user mode -- switch to kernel mode stack
	rdpr	%canrestore, %g5		! Fixup register window state registers
	wrpr	%g0, 0, %canrestore
	wrpr	%g0, %g5, %otherwin
	wrpr	%g0, WSTATE_KERN, %wstate	! Enable kernel mode window traps -- now we can trap again

	mov	CTX_PRIMARY, %g7
	SET_MMU_CONTEXTID(%g0, %g7)		! Switch MMU to kernel primary context
	sethi	%hi(KERNBASE), %g5
	membar	#Sync				! XXXX Should be taken care of by flush
	flush	%g5				! Some convenient address that won't trap
1:
	.endm

/*
 * This is the MMU protection handler.  It's too big to fit
 * in the trap table so I moved it here.  It's relatively simple.
 * It looks up the page mapping in the page table associated with
 * the trapping context.  It checks to see if the S/W writable bit
 * is set.  If so, it sets the H/W write bit, marks the tte modified,
 * and enters the mapping into the MMU.  Otherwise it does a regular
 * data fault.
 *
 *
 */
	ICACHE_ALIGN
dmmu_write_fault:
	mov	TLB_TAG_ACCESS, %g3
	sethi	%hi(0x1fff), %g6			! 8K context mask
	ldxa	[%g3] ASI_DMMU, %g3			! Get fault addr from Tag Target
	sethi	%hi(_C_LABEL(ctxbusy)), %g4
	or	%g6, %lo(0x1fff), %g6
	ldx	[%g4 + %lo(_C_LABEL(ctxbusy))], %g4
	srax	%g3, HOLESHIFT, %g5			! Check for valid address
	and	%g3, %g6, %g6				! Isolate context
	
	inc	%g5					! (0 or -1) -> (1 or 0)
	sllx	%g6, 3, %g6				! Make it into an offset into ctxbusy
	ldx	[%g4+%g6], %g4				! Load up our page table.
	srlx	%g3, STSHIFT, %g6
	cmp	%g5, 1
	bgu,pn %xcc, winfix				! Error!
	 srlx	%g3, PDSHIFT, %g5
	and	%g6, STMASK, %g6
	sll	%g6, 3, %g6
	
	and	%g5, PDMASK, %g5
	sll	%g5, 3, %g5
	add	%g6, %g4, %g4
	DLFLUSH %g4,%g6
	ldxa	[%g4] ASI_PHYS_CACHED, %g4
	DLFLUSH2 %g6
	srlx	%g3, PTSHIFT, %g6			! Convert to ptab offset
	and	%g6, PTMASK, %g6
	add	%g5, %g4, %g5
	brz,pn	%g4, winfix				! NULL entry? check somewhere else
	
	 nop	
	ldxa	[%g5] ASI_PHYS_CACHED, %g4
	sll	%g6, 3, %g6
	brz,pn	%g4, winfix				! NULL entry? check somewhere else
	 add	%g6, %g4, %g6
1:
	ldxa	[%g6] ASI_PHYS_CACHED, %g4
	brgez,pn %g4, winfix				! Entry invalid?  Punt
	 or	%g4, SUN4U_TLB_MODIFY|SUN4U_TLB_ACCESS|SUN4U_TLB_W, %g7
		! Update the modified bit
	
	btst	SUN4U_TLB_REAL_W|SUN4U_TLB_W, %g4	! Is it a ref fault?
	bz,pn	%xcc, winfix				! No -- really fault
#ifdef DEBUG
	/* Make sure we don't try to replace a kernel translation */
	/* This should not be necessary */
	sllx	%g3, 64-13, %g2				! Isolate context bits
	sethi	%hi(KERNBASE), %g5			! Don't need %lo
	brnz,pt	%g2, 0f					! Ignore context != 0
	 set	0x0800000, %g2				! 8MB
	sub	%g3, %g5, %g5
	cmp	%g5, %g2
	tlu	%xcc, 1; nop
	blu,pn	%xcc, winfix				! Next instruction in delay slot is unimportant
0:
#endif	/* DEBUG */
	/* Need to check for and handle large pages. */
	 srlx	%g4, 61, %g5				! Isolate the size bits
	ldxa	[%g0] ASI_DMMU_8KPTR, %g2		! Load DMMU 8K TSB pointer
	andcc	%g5, 0x3, %g5				! 8K?
	bnz,pn	%icc, winfix				! We punt to the pmap code since we can't handle policy
	 ldxa	[%g0] ASI_DMMU, %g1			! Hard coded for unified 8K TSB		Load DMMU tag target register
	casxa	[%g6] ASI_PHYS_CACHED, %g4, %g7		!  and write it out
	
	membar	#StoreLoad
	cmp	%g4, %g7
	bne,pn	%xcc, 1b
	 or	%g4, SUN4U_TLB_MODIFY|SUN4U_TLB_ACCESS|SUN4U_TLB_W, %g4
		! Update the modified bit

1:
#ifdef MULTIPROCESSOR
	ld	[%g2], %g6
	btst	(TSB_TAG_LOCKED >> 32), %g6
	bnz,pn	%icc, 1b
	 or	%g6, (TSB_TAG_LOCKED >> 32), %g5
	casa	[%g2] ASI_NUCLEUS, %g6, %g5
	cmp	%g6, %g5
	bne,pn	%icc, 1b
	 nop
	membar  #StoreStore
#endif
	stx	%g4, [%g2 + 8]				! Update TSB entry data
	mov	SFSR, %g7
	stx	%g1, [%g2]				! Update TSB entry tag
	nop
#ifdef DEBUG
	set	DATA_START, %g6	! debug
	stx	%g1, [%g6+0x40]	! debug
	set	0x88, %g5	! debug
	stx	%g4, [%g6+0x48]	! debug -- what we tried to enter in TLB
	stb	%g5, [%g6+0x8]	! debug
#endif	/* DEBUG */
	mov	DEMAP_PAGE_SECONDARY, %g1		! Secondary flush
	mov	DEMAP_PAGE_NUCLEUS, %g5			! Nucleus flush
	stxa	%g0, [%g7] ASI_DMMU			! clear out the fault
	membar	#Sync
	sllx	%g3, (64-13), %g7			! Need to demap old entry first
	andn	%g3, 0xfff, %g6
	movrz	%g7, %g5, %g1				! Pick one
	or	%g6, %g1, %g6
	stxa	%g6, [%g6] ASI_DMMU_DEMAP		! Do the demap
	membar	#Sync					! No real reason for this XXXX
	
	stxa	%g4, [%g0] ASI_DMMU_DATA_IN		! Enter new mapping
	membar	#Sync
	retry

/*
 * Each memory data access fault from a fast access miss handler comes here.
 * We will quickly check if this is an original prom mapping before going
 * to the generic fault handler
 *
 * We will assume that %pil is not lost so we won't bother to save it
 * unless we're in an interrupt handler.
 *
 * On entry:
 *	We are on one of the alternate set of globals
 *	%g1 = MMU tag target
 *	%g2 = 8Kptr
 *	%g3 = TLB TAG ACCESS
 *
 * On return:
 *
 */
	ICACHE_ALIGN
data_miss:
	mov	TLB_TAG_ACCESS, %g3			! Get real fault page
	sethi	%hi(0x1fff), %g6			! 8K context mask
	ldxa	[%g3] ASI_DMMU, %g3			! from tag access register
	sethi	%hi(_C_LABEL(ctxbusy)), %g4
	or	%g6, %lo(0x1fff), %g6
	ldx	[%g4 + %lo(_C_LABEL(ctxbusy))], %g4
	srax	%g3, HOLESHIFT, %g5			! Check for valid address
	and	%g3, %g6, %g6				! Isolate context
	
	inc	%g5					! (0 or -1) -> (1 or 0)
	sllx	%g6, 3, %g6				! Make it into an offset into ctxbusy
	ldx	[%g4+%g6], %g4				! Load up our page table.
#ifdef DEBUG
	/* Make sure we don't try to replace a kernel translation */
	/* This should not be necessary */
	brnz,pt	%g6, 1f			! If user context continue miss
	sethi	%hi(KERNBASE), %g7			! Don't need %lo
	set	0x0800000, %g6				! 8MB
	sub	%g3, %g7, %g7
	cmp	%g7, %g6
	sethi	%hi(DATA_START), %g7
	mov	6, %g6		! debug
	stb	%g6, [%g7+0x20]	! debug
	tlu	%xcc, 1; nop
	blu,pn	%xcc, winfix				! Next instruction in delay slot is unimportant
	 mov	7, %g6		! debug
	stb	%g6, [%g7+0x20]	! debug
1:	
#endif	/* DEBUG */
	srlx	%g3, STSHIFT, %g6
	cmp	%g5, 1
	bgu,pn %xcc, winfix				! Error!
	 srlx	%g3, PDSHIFT, %g5
	and	%g6, STMASK, %g6
	
	sll	%g6, 3, %g6
	and	%g5, PDMASK, %g5
	sll	%g5, 3, %g5
	add	%g6, %g4, %g4
	ldxa	[%g4] ASI_PHYS_CACHED, %g4
	srlx	%g3, PTSHIFT, %g6			! Convert to ptab offset
	and	%g6, PTMASK, %g6
	add	%g5, %g4, %g5
	brz,pn	%g4, data_nfo				! NULL entry? check somewhere else
	
	 nop
	ldxa	[%g5] ASI_PHYS_CACHED, %g4
	sll	%g6, 3, %g6
	brz,pn	%g4, data_nfo				! NULL entry? check somewhere else
	 add	%g6, %g4, %g6
1:
	ldxa	[%g6] ASI_PHYS_CACHED, %g4
	brgez,pn %g4, data_nfo				! Entry invalid?  Punt
	 or	%g4, SUN4U_TLB_ACCESS, %g7		! Update the access bit
	
	btst	SUN4U_TLB_ACCESS, %g4			! Need to update access git?
	bne,pt	%xcc, 1f
	 nop
	casxa	[%g6] ASI_PHYS_CACHED, %g4, %g7		!  and write it out
	cmp	%g4, %g7
	bne,pn	%xcc, 1b
	 or	%g4, SUN4U_TLB_ACCESS, %g4		! Update the modified bit

1:
#ifdef MULTIPROCESSOR
	ld	[%g2], %g6
	btst	(TSB_TAG_LOCKED >> 32), %g6
	bnz,pn	%icc, 1b
	 or	%g6, (TSB_TAG_LOCKED >> 32), %g5
	casa	[%g2] ASI_NUCLEUS, %g6, %g5
	cmp	%g6, %g5
	bne,pn	%icc, 1b
	 nop
	membar  #StoreStore
#endif
	stx	%g4, [%g2 + 8]				! Update TSB entry data
	stx	%g1, [%g2]				! Update TSB entry tag

#ifdef DEBUG
	set	DATA_START, %g6	! debug
	stx	%g3, [%g6+8]	! debug
	set	0xa, %g5	! debug
	stx	%g4, [%g6]	! debug -- what we tried to enter in TLB
	stb	%g5, [%g6+0x20]	! debug
#endif	/* DEBUG */
#if 0
	/* This was a miss -- should be nothing to demap. */
	sllx	%g3, (64-13), %g6			! Need to demap old entry first
	mov	DEMAP_PAGE_SECONDARY, %g1		! Secondary flush
	mov	DEMAP_PAGE_NUCLEUS, %g5			! Nucleus flush
	movrz	%g6, %g5, %g1				! Pick one
	andn	%g3, 0xfff, %g6
	or	%g6, %g1, %g6
	stxa	%g6, [%g6] ASI_DMMU_DEMAP		! Do the demap
	membar	#Sync					! No real reason for this XXXX
#endif	/* 0 */
	stxa	%g4, [%g0] ASI_DMMU_DATA_IN		! Enter new mapping
	membar	#Sync
	CLRTT
	retry
	NOTREACHED
/*
 * We had a data miss but did not find a mapping.  Insert
 * a NFO mapping to satisfy speculative loads and return.
 * If this had been a real load, it will re-execute and
 * result in a data fault or protection fault rather than
 * a TLB miss.  We insert an 8K TTE with the valid and NFO
 * bits set.  All others should zero.  The TTE looks like this:
 *
 *	0x9000000000000000
 *
 */
data_nfo:
	sethi	%hi(0x90000000), %g4			! V(0x8)|NFO(0x1)
	sllx	%g4, 32, %g4
	stxa	%g4, [%g0] ASI_DMMU_DATA_IN		! Enter new mapping
	membar	#Sync
	CLRTT
	retry	

/*
 * Handler for making the trap window shiny clean.
 *
 * If the store that trapped was to a kernel address, panic.
 *
 * If the store that trapped was to a user address, stick it in the PCB.
 * Since we don't want to force user code to use the standard register
 * convention if we don't have to, we will not assume that %fp points to
 * anything valid.
 *
 * On entry:
 *	We are on one of the alternate set of globals
 *	%g1 = %tl - 1, tstate[tl-1], scratch	- local
 *	%g2 = %tl				- local
 *	%g3 = MMU tag access			- in
 *	%g4 = %cwp				- local
 *	%g5 = scratch				- local
 *	%g6 = cpcb				- local
 *	%g7 = scratch				- local
 *
 * On return:
 *
 * NB:	 remove most of this from main codepath & cleanup I$
 */
winfault:
#ifdef DEBUG
	sethi	%hi(DATA_START), %g7			! debug
!	stx	%g0, [%g7]				! debug This is a real fault -- prevent another trap from watchdoging
	set	0x10, %g4				! debug
	stb	%g4, [%g7 + 0x20]			! debug
	CHKPT %g4,%g7,0x19
#endif	/* DEBUG */
	mov	TLB_TAG_ACCESS, %g3	! Get real fault page from tag access register
	ldxa	[%g3] ASI_DMMU, %g3	! And put it into the non-MMU alternate regs
winfix:
	rdpr	%tl, %g2
	subcc	%g2, 1, %g1
	brlez,pt	%g1, datafault	! Don't go below trap level 1
	 nop

	CHKPT %g4,%g7,0x20
	wrpr	%g1, 0, %tl		! Pop a trap level
	rdpr	%tt, %g7		! Read type of prev. trap
	rdpr	%tstate, %g4		! Try to restore prev %cwp if we were executing a restore
	andn	%g7, 0x3f, %g5		!   window fill traps are all 0b 0000 11xx xxxx

#if 1
	cmp	%g7, 0x68		! If we took a datafault just before this trap
	bne,pt	%icc, winfixfill	! our stack's probably bad so we need to switch somewhere else
	 nop

	!!
	!! Double data fault -- bad stack?
	!!
	wrpr	%g2, %tl	! Restore trap level.
	sir			! Just issue a reset and don't try to recover.
	mov	%fp, %l6		! Save the frame pointer
	set	EINTSTACK+USPACE+CC64FSZ-BIAS, %fp ! Set the frame pointer to the middle of the idle stack
	add	%fp, -CC64FSZ, %sp	! Create a stackframe
	wrpr	%g0, 15, %pil		! Disable interrupts, too
	wrpr	%g0, %g0, %canrestore	! Our stack is hozed and our PCB
	wrpr	%g0, 7, %cansave	!  probably is too, so blow away
	ba	slowtrap		!  all our register windows.
	 wrpr	%g0, 0x101, %tt
#endif	/* 1 */

winfixfill:
	cmp	%g5, 0x0c0		!   so we mask lower bits & compare to 0b 0000 1100 0000
	bne,pt	%icc, winfixspill	! Dump our trap frame -- we will retry the fill when the page is loaded
	 cmp	%g5, 0x080		!   window spill traps are all 0b 0000 10xx xxxx

	!!
	!! This was a fill
	!!
	btst	TSTATE_PRIV, %g4	! User mode?
	and	%g4, CWP, %g5		! %g4 = %cwp of trap
	wrpr	%g7, 0, %tt
	bz,a,pt	%icc, datafault		! We were in user mode -- normal fault
	 wrpr	%g5, %cwp		! Restore cwp from before fill trap -- regs should now be consistent

	/*
	 * We're in a pickle here.  We were trying to return to user mode
	 * and the restore of the user window failed, so now we have one valid
	 * kernel window and a user window state.  If we do a TRAP_SETUP now,
	 * our kernel window will be considered a user window and cause a
	 * fault when we try to save it later due to an invalid user address.
	 * If we return to where we faulted, our window state will not be valid
	 * and we will fault trying to enter user with our primary context of zero.
	 *
	 * What we'll do is arrange to have us return to return_from_trap so we will
	 * start the whole business over again.  But first, switch to a kernel window
	 * setup.  Let's see, canrestore and otherwin are zero.  Set WSTATE_KERN and
	 * make sure we're in kernel context and we're done.
	 */

#if 0 /* Need to switch over to new stuff to fix WDR bug */
	wrpr	%g5, %cwp				! Restore cwp from before fill trap -- regs should now be consistent
	wrpr	%g2, %g0, %tl				! Restore trap level -- we need to reuse it
	set	return_from_trap, %g4
	set	CTX_PRIMARY, %g7
	wrpr	%g4, 0, %tpc
	stxa	%g0, [%g7] ASI_DMMU
	inc	4, %g4
	membar	#Sync
	flush	%g4					! Isn't this convenient?
	wrpr	%g0, WSTATE_KERN, %wstate
	wrpr	%g0, 0, %canrestore			! These should be zero but
	wrpr	%g0, 0, %otherwin			! clear them just in case
	rdpr	%ver, %g5
	and	%g5, CWP, %g5
	wrpr	%g0, 0, %cleanwin
	dec	1, %g5					! NWINDOWS-1-1
	wrpr	%g5, 0, %cansave			! Invalidate all windows
	CHKPT %g5,%g7,0xe
!	flushw						! DEBUG
	ba,pt	%icc, datafault
	 wrpr	%g4, 0, %tnpc
#else	/* 0 - Need to switch over to new stuff to fix WDR bug */
	wrpr	%g2, %g0, %tl				! Restore trap level
	cmp	%g2, 3
	tne	%icc, 1
	rdpr	%tt, %g5
	wrpr	%g0, 1, %tl				! Revert to TL==1 XXX what if this wasn't in rft_user? Oh well.
	wrpr	%g5, %g0, %tt				! Set trap type correctly
	CHKPT %g5,%g7,0xe
/*
 * Here we need to implement the beginning of datafault.
 * TRAP_SETUP expects to come from either kernel mode or
 * user mode with at least one valid register window.  It
 * will allocate a trap frame, save the out registers, and
 * fix the window registers to think we have one user
 * register window.
 *
 * However, under these circumstances we don't have any
 * valid register windows, so we need to clean up the window
 * registers to prevent garbage from being saved to either
 * the user stack or the PCB before calling the datafault
 * handler.
 *
 * We could simply jump to datafault if we could somehow
 * make the handler issue a `saved' instruction immediately
 * after creating the trapframe.
 *
 * The fillowing is duplicated from datafault:
 */
	wrpr	%g0, PSTATE_KERN|PSTATE_AG, %pstate	! We need to save volatile stuff to AG regs
#ifdef DEBUG
	set	DATA_START, %g7				! debug
	set	0x20, %g6				! debug
	stx	%g0, [%g7]				! debug
	stb	%g6, [%g7 + 0x20]			! debug
	CHKPT %g4,%g7,0xf
#endif	/* DEBUG */
	wr	%g0, ASI_DMMU, %asi			! We need to re-load trap info
	ldxa	[%g0 + TLB_TAG_ACCESS] %asi, %g1	! Get fault address from tag access register
	ldxa	[SFAR] %asi, %g2			! sync virt addr; must be read first
	ldxa	[SFSR] %asi, %g3			! get sync fault status register
	stxa	%g0, [SFSR] %asi			! Clear out fault now
	membar	#Sync					! No real reason for this XXXX

	TRAP_SETUP -CC64FSZ-TF_SIZE
	saved						! Blow away that one register window we didn't ever use.
	ba,a,pt	%icc, Ldatafault_internal		! Now we should return directly to user mode
	 nop
#endif	/* 0 - Need to switch over to new stuff to fix WDR bug */
winfixspill:
	bne,a,pt	%xcc, datafault				! Was not a spill -- handle it normally
	 wrpr	%g2, 0, %tl				! Restore trap level for now XXXX

	!!
	!! This was a spill
	!!
#if 1
	btst	TSTATE_PRIV, %g4			! From user mode?
!	cmp	%g2, 2					! From normal execution? take a fault.
	wrpr	%g2, 0, %tl				! We need to load the fault type so we can
	rdpr	%tt, %g5				! overwrite the lower trap and get it to the fault handler
	wrpr	%g1, 0, %tl
	wrpr	%g5, 0, %tt				! Copy over trap type for the fault handler
	and	%g4, CWP, %g5				! find %cwp from trap

	be,a,pt	%xcc, datafault				! Let's do a regular datafault.  When we try a save in datafault we'll
	 wrpr	%g5, 0, %cwp				!  return here and write out all dirty windows.
#endif	/* 1 */
	wrpr	%g2, 0, %tl				! Restore trap level for now XXXX
	GET_CPCB(%g6)
#ifdef DEBUG
	set	0x12, %g5				! debug
	sethi	%hi(DATA_START), %g7			! debug
	stb	%g5, [%g7 + 0x20]			! debug
	CHKPT %g5,%g7,0x11
#endif	/* DEBUG */

	/*
	 * Traverse kernel map to find paddr of cpcb and only us ASI_PHYS_CACHED to
	 * prevent any faults while saving the windows.  BTW if it isn't mapped, we
	 * will trap and hopefully panic.
	 */

!	ba	0f					! DEBUG -- don't use phys addresses
	 wr	%g0, ASI_NUCLEUS, %asi			! In case of problems finding PA
	sethi	%hi(_C_LABEL(ctxbusy)), %g1
	ldx	[%g1 + %lo(_C_LABEL(ctxbusy))], %g1	! Load start of ctxbusy
#ifdef DEBUG
	srax	%g6, HOLESHIFT, %g7			! Check for valid address
	brz,pt	%g7, 1f					! Should be zero or -1
	 addcc	%g7, 1, %g7					! Make -1 -> 0
	tnz	%xcc, 1					! Invalid address??? How did this happen?
1:
#endif	/* DEBUG */
	srlx	%g6, STSHIFT, %g7
	ldx	[%g1], %g1				! Load pointer to kernel_pmap
	and	%g7, STMASK, %g7
	sll	%g7, 3, %g7
	add	%g7, %g1, %g1
	DLFLUSH %g1,%g7
	ldxa	[%g1] ASI_PHYS_CACHED, %g1		! Load pointer to directory
	DLFLUSH2 %g7

	srlx	%g6, PDSHIFT, %g7			! Do page directory
	and	%g7, PDMASK, %g7
	sll	%g7, 3, %g7
	brz,pn	%g1, 0f
	 add	%g7, %g1, %g1
	DLFLUSH %g1,%g7
	ldxa	[%g1] ASI_PHYS_CACHED, %g1
	DLFLUSH2 %g7

	srlx	%g6, PTSHIFT, %g7			! Convert to ptab offset
	and	%g7, PTMASK, %g7
	brz	%g1, 0f
	 sll	%g7, 3, %g7
	add	%g1, %g7, %g7
	DLFLUSH %g7,%g1
	ldxa	[%g7] ASI_PHYS_CACHED, %g7		! This one is not
	DLFLUSH2 %g1
	brgez	%g7, 0f
	 srlx	%g7, PGSHIFT, %g7			! Isolate PA part
	sll	%g6, 32-PGSHIFT, %g6			! And offset
	sllx	%g7, PGSHIFT+17, %g7			! There are 17 bits to the left of the PA in the TTE
	srl	%g6, 32-PGSHIFT, %g6
	srax	%g7, 17, %g7
	or	%g7, %g6, %g6				! Then combine them to form PA

	wr	%g0, ASI_PHYS_CACHED, %asi		! Use ASI_PHYS_CACHED to prevent possible page faults
0:
	/*
	 * Now save all user windows to cpcb.
	 */
	CHKPT %g5,%g7,0x12
	rdpr	%otherwin, %g7
	brnz,pt	%g7, 1f
	 rdpr	%canrestore, %g5
	rdpr	%cansave, %g1
	add	%g5, 1, %g7				! add the %cwp window to the list to save
!	movrnz	%g1, %g5, %g7				! If we're issuing a save
!	mov	%g5, %g7				! DEBUG
	wrpr	%g0, 0, %canrestore
	wrpr	%g7, 0, %otherwin			! Still in user mode -- need to switch to kernel mode
1:
	mov	%g7, %g1
	CHKPT %g5,%g7,0x13
	add	%g6, PCB_NSAVED, %g7
	DLFLUSH %g7,%g5
	lduba	[%g6 + PCB_NSAVED] %asi, %g7		! Start incrementing pcb_nsaved
	DLFLUSH2 %g5

#ifdef DEBUG
	wrpr	%g0, 5, %tl
#endif	/* DEBUG */
	mov	%g6, %g5
	brz,pt	%g7, winfixsave				! If it's in use, panic
	 saved						! frob window registers

	/* PANIC */
!	CHKPT %g4,%g7,0x10	! Checkpoint
!	sir						! Force a watchdog
#ifdef DEBUG
	wrpr	%g2, 0, %tl
#endif	/* DEBUG */
	mov	%g7, %o2
	rdpr	%ver, %o1
	sethi	%hi(2f), %o0
	and	%o1, CWP, %o1
	wrpr	%g0, %o1, %cleanwin
	dec	1, %o1
	wrpr	%g0, %o1, %cansave			! kludge away any more window problems
	wrpr	%g0, 0, %canrestore
	wrpr	%g0, 0, %otherwin
	or	%lo(2f), %o0, %o0
	wrpr	%g0, WSTATE_KERN, %wstate
#ifdef DEBUG
	set	panicstack-CC64FSZ-BIAS, %sp		! Use panic stack.
#else	/* DEBUG */
	set	estack0, %sp
	ldx	[%sp], %sp
	add	%sp, -CC64FSZ-BIAS, %sp			! Overwrite proc 0's stack.
#endif	/* DEBUG */
	ta	1; nop					! This helps out traptrace.
	call	_C_LABEL(panic)				! This needs to be fixed properly but we should panic here
	 mov	%g1, %o1
	NOTREACHED
	.data
2:
	.asciz	"winfault: double invalid window at %p, nsaved=%d"
	_ALIGN
	.text
3:
	saved
	save
winfixsave:
	sllx	%g7, 7, %g5
	add	%g6, %g5, %g5
	SPILL	stxa, %g5 + PCB_RW, 8, %asi	! Save the window in the pcb

	sllx	%g7, 3, %g5
	add	%g6, %g5, %g5
	stxa	%sp, [%g5 + PCB_RWSP] %asi

!	rdpr	%otherwin, %g1	! Check to see if we's done
	dec	%g1
	wrpr	%g0, 7, %cleanwin			! BUGBUG -- we should not hardcode this, but I have no spare globals
	brnz,pt	%g1, 3b
	 inc	%g7					! inc pcb_nsaved

	/* fix up pcb fields */
	stba	%g7, [%g6 + PCB_NSAVED] %asi		! cpcb->pcb_nsaved = n
	CHKPT %g5,%g1,0x14
#if 0
	mov	%g7, %g5				! fixup window registers
5:
	dec	%g5
	brgz,a,pt	%g5, 5b
	 restore
#else	/* 0 */
	/*
	 * We just issued a bunch of saves, so %cansave is now 0,
	 * probably (if we were doing a flushw then we may have
	 * come in with only partially full register windows and
	 * it may not be 0).
	 *
	 * %g7 contains the count of the windows we just finished
	 * saving.
	 *
	 * What we need to do now is move some of the windows from
	 * %canrestore to %cansave.  What we should do is take
	 * min(%canrestore, %g7) and move that over to %cansave.
	 *
	 * %g7 is the number of windows we flushed, so we should
	 * use that as a base.  Clear out %otherwin, set %cansave
	 * to min(%g7, NWINDOWS - 2), set %cleanwin to %canrestore
	 * + %cansave and the rest follows:
	 *
	 * %otherwin = 0
	 * %cansave = NWINDOWS - 2 - %canrestore
	 */
	wrpr	%g0, 0, %otherwin
	rdpr	%canrestore, %g1
	sub	%g1, %g7, %g1				! Calculate %canrestore - %g7
	movrlz	%g1, %g0, %g1				! Clamp at zero
	wrpr	%g1, 0, %canrestore			! This is the new canrestore
	rdpr	%ver, %g5
	and	%g5, CWP, %g5				! NWINDOWS-1
	dec	%g5					! NWINDOWS-2
	wrpr	%g5, 0, %cleanwin			! Set cleanwin to max, since we're in-kernel
	sub	%g5, %g1, %g5				! NWINDOWS-2-%canrestore
	wrpr	%g5, 0, %cansave
#endif	/* 0 */

	CHKPT %g5,%g1,0x15
!	rdpr	%tl, %g2				! DEBUG DEBUG -- did we trap somewhere?
	sub	%g2, 1, %g1
	rdpr	%tt, %g2
	wrpr	%g1, 0, %tl				! We will not attempt to re-execute the spill, so dump our trap frame permanently
	wrpr	%g2, 0, %tt				! Move trap type from fault frame here, overwriting spill
	CHKPT %g2,%g5,0x16

	/* Did we save a user or kernel window ? */
!	srax	%g3, 48, %g7				! User or kernel store? (TAG TARGET)
	sllx	%g3, (64-13), %g7			! User or kernel store? (TAG ACCESS)
	sethi	%hi((2*NBPG)-8), %g7
	brnz,pt	%g7, 1f					! User fault -- save windows to pcb
	 or	%g7, %lo((2*NBPG)-8), %g7

	and	%g4, CWP, %g4				! %g4 = %cwp of trap
	wrpr	%g4, 0, %cwp				! Kernel fault -- restore %cwp and force and trap to debugger
#ifdef DEBUG
	set	DATA_START, %g7				! debug
	set	0x11, %g6				! debug
	stb	%g6, [%g7 + 0x20]			! debug
	CHKPT %g2,%g1,0x17
!	sir
#endif	/* DEBUG */
	!!
	!! Here we managed to fault trying to access a kernel window
	!! This is a bug.  Switch to the interrupt stack if we aren't
	!! there already and then trap into the debugger or panic.
	!!
	sethi	%hi(EINTSTACK-BIAS), %g6
	btst	1, %sp
	bnz,pt	%icc, 0f
	 mov	%sp, %g1
	add	%sp, -BIAS, %g1
0:
	or	%g6, %lo(EINTSTACK-BIAS), %g6
	set	(EINTSTACK-INTSTACK), %g7	! XXXXXXXXXX This assumes kernel addresses are unique from user addresses
	sub	%g6, %g1, %g2				! Determine if we need to switch to intr stack or not
	dec	%g7					! Make it into a mask
	andncc	%g2, %g7, %g0				! XXXXXXXXXX This assumes kernel addresses are unique from user addresses */ \
	movz	%xcc, %g1, %g6				! Stay on interrupt stack?
	add	%g6, -CCFSZ, %g6			! Allocate a stack frame
	mov	%sp, %l6				! XXXXX Save old stack pointer
	mov	%g6, %sp
	ta	1; nop					! Enter debugger
	NOTREACHED
1:
#if 1
	/* Now we need to blast away the D$ to make sure we're in sync */
dlflush1:
	stxa	%g0, [%g7] ASI_DCACHE_TAG
	brnz,pt	%g7, 1b
	 dec	8, %g7
#endif	/* 1 */

#ifdef DEBUG
	CHKPT %g2,%g1,0x18
	set	DATA_START, %g7				! debug
	set	0x19, %g6				! debug
	stb	%g6, [%g7 + 0x20]			! debug
#endif	/* DEBUG */
	/*
	 * If we had WSTATE_KERN then we had at least one valid kernel window.
	 * We should re-execute the trapping save.
	 */
	rdpr	%wstate, %g3
	mov	%g3, %g3
	cmp	%g3, WSTATE_KERN
	bne,pt	%icc, 1f
	 nop
	retry						! Now we can complete the save
1:
	/*
	 * Since we had a WSTATE_USER, we had no valid kernel windows.  This should
	 * only happen inside TRAP_SETUP or INTR_SETUP. Emulate
	 * the instruction, clean up the register windows, then done.
	 */
	rdpr	%cwp, %g1
	inc	%g1
	rdpr	%tstate, %g2
	wrpr	%g1, %cwp
	andn	%g2, CWP, %g2
	wrpr	%g1, %g2, %tstate
	wrpr	%g0, PSTATE_KERN|PSTATE_AG, %pstate
	mov	%g6, %sp
	done

/*
 * Each memory data access fault, from user or kernel mode,
 * comes here.
 *
 * We will assume that %pil is not lost so we won't bother to save it
 * unless we're in an interrupt handler.
 *
 * On entry:
 *	We are on one of the alternate set of globals
 *	%g1 = MMU tag target
 *	%g2 = %tl
 *
 * On return:
 *
 */
datafault:
	wrpr	%g0, PSTATE_KERN|PSTATE_AG, %pstate	! We need to save volatile stuff to AG regs
#ifdef DEBUG
	set	DATA_START, %g7				! debug
	set	0x20, %g6				! debug
	stx	%g0, [%g7]				! debug
	stb	%g6, [%g7 + 0x20]			! debug
	CHKPT %g4,%g7,0xf
#endif	/* DEBUG */
	wr	%g0, ASI_DMMU, %asi			! We need to re-load trap info
	ldxa	[%g0 + TLB_TAG_ACCESS] %asi, %g1	! Get fault address from tag access register
	ldxa	[SFAR] %asi, %g2			! sync virt addr; must be read first
	ldxa	[SFSR] %asi, %g3			! get sync fault status register
	stxa	%g0, [SFSR] %asi			! Clear out fault now
	membar	#Sync					! No real reason for this XXXX

	TRAP_SETUP -CC64FSZ-TF_SIZE
Ldatafault_internal:
	INCR _C_LABEL(uvmexp)+V_FAULTS			! cnt.v_faults++ (clobbers %o0,%o1,%o2) should not fault
!	ldx	[%sp + CC64FSZ + BIAS + TF_FAULT], %g1		! DEBUG make sure this has not changed
	mov	%g1, %o0				! Move these to the out regs so we can save the globals
	mov	%g2, %o4
	mov	%g3, %o5

	ldxa	[%g0] ASI_AFAR, %o2			! get async fault address
	ldxa	[%g0] ASI_AFSR, %o3			! get async fault status
	mov	-1, %g7
	stxa	%g7, [%g0] ASI_AFSR			! And clear this out, too
	membar	#Sync					! No real reason for this XXXX

	wrpr	%g0, PSTATE_KERN, %pstate		! Get back to normal globals

	stx	%g1, [%sp + CC64FSZ + BIAS + TF_G + (1*8)]	! save g1
	rdpr	%tt, %o1				! find out what trap brought us here
	stx	%g2, [%sp + CC64FSZ + BIAS + TF_G + (2*8)]	! save g2
	rdpr	%tstate, %g1
	stx	%g3, [%sp + CC64FSZ + BIAS + TF_G + (3*8)]	! (sneak g3 in here)
	rdpr	%tpc, %g2
	stx	%g4, [%sp + CC64FSZ + BIAS + TF_G + (4*8)]	! sneak in g4
	rdpr	%tnpc, %g3
	stx	%g5, [%sp + CC64FSZ + BIAS + TF_G + (5*8)]	! sneak in g5
	rd	%y, %g4					! save y
	stx	%g6, [%sp + CC64FSZ + BIAS + TF_G + (6*8)]	! sneak in g6
	mov	%g2, %o7				! Make the fault address look like the return address
	stx	%g7, [%sp + CC64FSZ + BIAS + TF_G + (7*8)]	! sneak in g7

#ifdef DEBUG
	set	DATA_START, %g7				! debug
	set	0x21, %g6				! debug
	stb	%g6, [%g7 + 0x20]			! debug
#endif	/* DEBUG */
	sth	%o1, [%sp + CC64FSZ + BIAS + TF_TT]
	stx	%g1, [%sp + CC64FSZ + BIAS + TF_TSTATE]		! set tf.tf_psr, tf.tf_pc
	stx	%g2, [%sp + CC64FSZ + BIAS + TF_PC]		! set tf.tf_npc
	stx	%g3, [%sp + CC64FSZ + BIAS + TF_NPC]

	rdpr	%pil, %g5
	stb	%g5, [%sp + CC64FSZ + BIAS + TF_PIL]
	stb	%g5, [%sp + CC64FSZ + BIAS + TF_OLDPIL]

#if 1
	rdpr	%tl, %g7
	dec	%g7
	movrlz	%g7, %g0, %g7
	CHKPT %g1,%g3,0x21
	wrpr	%g0, %g7, %tl		! Revert to kernel mode
#else	/* 1 */
	CHKPT %g1,%g3,0x21
	wrpr	%g0, 0, %tl		! Revert to kernel mode
#endif	/* 1 */
	/* Finish stackframe, call C trap handler */
	flushw						! Get this clean so we won't take any more user faults

	GET_CPUINFO_VA(%g7)

	/*
	 * Right now the registers have the following values:
	 *
	 *	%o0 -- MMU_TAG_ACCESS
	 *	%o1 -- TT
	 *	%o2 -- afar
	 *	%o3 -- afsr
	 *	%o4 -- sfar
	 *	%o5 -- sfsr
	 */
	
	cmp	%o1, T_DATA_ERROR
	st	%g4, [%sp + CC64FSZ + BIAS + TF_Y]
	wr	%g0, ASI_PRIMARY_NOFAULT, %asi	! Restore default ASI
	be,pn	%icc, data_error
	 wrpr	%g0, PSTATE_INTR, %pstate	! reenable interrupts

	mov	%o0, %o3			! (argument: trap address)
	mov	%g2, %o2			! (argument: trap pc)
	call	_C_LABEL(data_access_fault)	! data_access_fault(&tf, type, 
						!	pc, addr, sfva, sfsr)
	 add	%sp, CC64FSZ + BIAS, %o0	! (argument: &tf)

data_recover:
	CHKPT %o1,%o2,1
	wrpr	%g0, PSTATE_KERN, %pstate		! disable interrupts
	b	return_from_trap			! go return
	 ldx	[%sp + CC64FSZ + BIAS + TF_TSTATE], %g1		! Load this for return_from_trap
	NOTREACHED

data_error:
	call	_C_LABEL(data_access_error)	! data_access_error(&tf, type, 
						!	afva, afsr, sfva, sfsr)
	 add	%sp, CC64FSZ + BIAS, %o0	! (argument: &tf)
	ba	data_recover
	 nop
	NOTREACHED

/*
 * Each memory instruction access fault from a fast access handler comes here.
 * We will quickly check if this is an original prom mapping before going
 * to the generic fault handler
 *
 * We will assume that %pil is not lost so we won't bother to save it
 * unless we're in an interrupt handler.
 *
 * On entry:
 *	We are on one of the alternate set of globals
 *	%g1 = MMU tag target
 *	%g2 = TSB entry ptr
 *	%g3 = TLB Tag Access
 *
 * On return:
 *
 */

	ICACHE_ALIGN
instr_miss:
	mov	TLB_TAG_ACCESS, %g3			! Get real fault page
	sethi	%hi(0x1fff), %g7			! 8K context mask
	ldxa	[%g3] ASI_IMMU, %g3			! from tag access register
	sethi	%hi(_C_LABEL(ctxbusy)), %g4
	or	%g7, %lo(0x1fff), %g7
	ldx	[%g4 + %lo(_C_LABEL(ctxbusy))], %g4
	srax	%g3, HOLESHIFT, %g5			! Check for valid address
	and	%g3, %g7, %g6				! Isolate context
	sllx	%g6, 3, %g6				! Make it into an offset into ctxbusy
	inc	%g5					! (0 or -1) -> (1 or 0)
	
	ldx	[%g4+%g6], %g4				! Load up our page table.
#ifdef DEBUG
	/* Make sure we don't try to replace a kernel translation */
	/* This should not be necessary */
	brnz,pt	%g6, 1f					! If user context continue miss
	sethi	%hi(KERNBASE), %g7			! Don't need %lo
	set	0x0800000, %g6				! 8MB
	sub	%g3, %g7, %g7
	cmp	%g7, %g6
	mov	6, %g6		! debug
	sethi	%hi(DATA_START), %g7
	stb	%g6, [%g7+0x30]	! debug
	tlu	%xcc, 1; nop
	blu,pn	%xcc, textfault				! Next instruction in delay slot is unimportant
	 mov	7, %g6		! debug
	stb	%g6, [%g7+0x30]	! debug
1:	
#endif	/* DEBUG */
	srlx	%g3, STSHIFT, %g6
	cmp	%g5, 1
	bgu,pn %xcc, textfault				! Error!
	 srlx	%g3, PDSHIFT, %g5
	and	%g6, STMASK, %g6
	sll	%g6, 3, %g6
	and	%g5, PDMASK, %g5
	nop

	sll	%g5, 3, %g5
	add	%g6, %g4, %g4
	ldxa	[%g4] ASI_PHYS_CACHED, %g4
	srlx	%g3, PTSHIFT, %g6			! Convert to ptab offset
	and	%g6, PTMASK, %g6
	add	%g5, %g4, %g5
	brz,pn	%g4, textfault				! NULL entry? check somewhere else
	 nop
	
	ldxa	[%g5] ASI_PHYS_CACHED, %g4
	sll	%g6, 3, %g6
	brz,pn	%g4, textfault				! NULL entry? check somewhere else
	 add	%g6, %g4, %g6		
1:
	ldxa	[%g6] ASI_PHYS_CACHED, %g4
	brgez,pn %g4, textfault
	 nop

	/* Check if it's an executable mapping. */
	andcc	%g4, SUN4U_TLB_EXEC, %g0
	bz,pn	%xcc, textfault
	 nop


	or	%g4, SUN4U_TLB_ACCESS, %g7		! Update accessed bit
	btst	SUN4U_TLB_ACCESS, %g4			! Need to update access bit?
	bne,pt	%xcc, 1f
	 nop
	casxa	[%g6] ASI_PHYS_CACHED, %g4, %g7		!  and store it
	cmp	%g4, %g7
	bne,pn	%xcc, 1b
	 or	%g4, SUN4U_TLB_ACCESS, %g4		! Update accessed bit

1:
#ifdef MULTIPROCESSOR
	ld	[%g2], %g6
	btst	(TSB_TAG_LOCKED >> 32), %g6
	bnz,pn	%icc, 1b
	 or	%g6, (TSB_TAG_LOCKED >> 32), %g5
	casa	[%g2] ASI_NUCLEUS, %g6, %g5
	cmp	%g6, %g5
	bne,pn	%icc, 1b
	 nop
	membar	#StoreStore
#endif
	stx	%g4, [%g2 + 8]				! Update TSB entry data
	stx	%g1, [%g2]				! Update TSB entry tag
#ifdef DEBUG
	set	DATA_START, %g6	! debug
	stx	%g3, [%g6+8]	! debug
	set	0xaa, %g3	! debug
	stx	%g4, [%g6]	! debug -- what we tried to enter in TLB
	stb	%g3, [%g6+0x20]	! debug
#endif	/* DEBUG */
#if 1
	/* This was a miss -- should be nothing to demap. */
	sllx	%g3, (64-13), %g6			! Need to demap old entry first
	mov	DEMAP_PAGE_SECONDARY, %g1		! Secondary flush
	mov	DEMAP_PAGE_NUCLEUS, %g5			! Nucleus flush
	movrz	%g6, %g5, %g1				! Pick one
	andn	%g3, 0xfff, %g6
	or	%g6, %g1, %g6
	stxa	%g6, [%g6] ASI_DMMU_DEMAP		! Do the demap
	membar	#Sync					! No real reason for this XXXX
#endif	/* 1 */
	stxa	%g4, [%g0] ASI_IMMU_DATA_IN		! Enter new mapping
	membar	#Sync
	CLRTT
	retry
	NOTREACHED
	!!
	!!  Check our prom mappings -- temporary
	!!

/*
 * Each memory text access fault, from user or kernel mode,
 * comes here.
 *
 * We will assume that %pil is not lost so we won't bother to save it
 * unless we're in an interrupt handler.
 *
 * On entry:
 *	We are on one of the alternate set of globals
 *	%g1 = MMU tag target
 *	%g2 = %tl
 *	%g3 = %tl - 1
 *
 * On return:
 *
 */


textfault:
	wrpr	%g0, PSTATE_KERN|PSTATE_AG, %pstate	! We need to save volatile stuff to AG regs
	wr	%g0, ASI_IMMU, %asi
	ldxa	[%g0 + TLB_TAG_ACCESS] %asi, %g1	! Get fault address from tag access register
	ldxa	[SFSR] %asi, %g3			! get sync fault status register
	membar	#LoadStore
	stxa	%g0, [SFSR] %asi			! Clear out old info
	membar	#Sync					! No real reason for this XXXX

	TRAP_SETUP -CC64FSZ-TF_SIZE
	INCR _C_LABEL(uvmexp)+V_FAULTS			! cnt.v_faults++ (clobbers %o0,%o1,%o2)

	mov	%g3, %o3

	wrpr	%g0, PSTATE_KERN, %pstate		! Switch to normal globals
	ldxa	[%g0] ASI_AFSR, %o4			! get async fault status
	ldxa	[%g0] ASI_AFAR, %o5			! get async fault address
	mov	-1, %o0
	stxa	%o0, [%g0] ASI_AFSR			! Clear this out
	membar	#Sync					! No real reason for this XXXX
	stx	%g1, [%sp + CC64FSZ + BIAS + TF_G + (1*8)]	! save g1
	stx	%g2, [%sp + CC64FSZ + BIAS + TF_G + (2*8)]	! save g2
	stx	%g3, [%sp + CC64FSZ + BIAS + TF_G + (3*8)]	! (sneak g3 in here)
	rdpr	%tt, %o1				! Find out what caused this trap
	stx	%g4, [%sp + CC64FSZ + BIAS + TF_G + (4*8)]	! sneak in g4
	rdpr	%tstate, %g1
	stx	%g5, [%sp + CC64FSZ + BIAS + TF_G + (5*8)]	! sneak in g5
	rdpr	%tpc, %o2				! sync virt addr; must be read first
	stx	%g6, [%sp + CC64FSZ + BIAS + TF_G + (6*8)]	! sneak in g6
	rdpr	%tnpc, %g3
	stx	%g7, [%sp + CC64FSZ + BIAS + TF_G + (7*8)]	! sneak in g7
	rd	%y, %g4					! save y

	/* Finish stackframe, call C trap handler */
	stx	%g1, [%sp + CC64FSZ + BIAS + TF_TSTATE]		! set tf.tf_psr, tf.tf_pc
	sth	%o1, [%sp + CC64FSZ + BIAS + TF_TT]! debug

	stx	%o2, [%sp + CC64FSZ + BIAS + TF_PC]
	stx	%g3, [%sp + CC64FSZ + BIAS + TF_NPC]		! set tf.tf_npc

	rdpr	%pil, %g5
	stb	%g5, [%sp + CC64FSZ + BIAS + TF_PIL]
	stb	%g5, [%sp + CC64FSZ + BIAS + TF_OLDPIL]

	rdpr	%tl, %g7
	dec	%g7
	movrlz	%g7, %g0, %g7
	CHKPT %g1,%g3,0x22
	wrpr	%g0, %g7, %tl		! Revert to kernel mode

	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore default ASI
	flushw						! Get rid of any user windows so we don't deadlock

	GET_CPUINFO_VA(%g7)

	/* Use trap type to see what handler to call */
	cmp	%o1, T_INST_ERROR
	be,pn	%xcc, text_error
	 st	%g4, [%sp + CC64FSZ + BIAS + TF_Y]		! set tf.tf_y

	wrpr	%g0, PSTATE_INTR, %pstate	! reenable interrupts
	call	_C_LABEL(text_access_fault)	! mem_access_fault(&tf, type, pc, sfsr)
	 add	%sp, CC64FSZ + BIAS, %o0	! (argument: &tf)
text_recover:
	CHKPT %o1,%o2,2
	wrpr	%g0, PSTATE_KERN, %pstate	! disable interrupts
	b	return_from_trap		! go return
	 ldx	[%sp + CC64FSZ + BIAS + TF_TSTATE], %g1	! Load this for return_from_trap
	NOTREACHED

text_error:
	wrpr	%g0, PSTATE_INTR, %pstate	! reenable interrupts
	call	_C_LABEL(text_access_error)	! mem_access_fault(&tfm type, sfva [pc], sfsr,
						!		afva, afsr);
	 add	%sp, CC64FSZ + BIAS, %o0	! (argument: &tf)
	ba	text_recover
	 nop
	NOTREACHED

#ifdef SUN4V

/*
 * Traps for sun4v.
 */

sun4v_tl1_dtsb_miss:
	GET_MMFSA(%g1)
	add	%g1, 0x48, %g3
	ldxa	[%g3] ASI_PHYS_CACHED, %g3
	add	%g1, 0x50, %g6
	ldxa	[%g6] ASI_PHYS_CACHED, %g6
	sethi	%hi(_C_LABEL(ctxbusy)), %g4
	ldx	[%g4 + %lo(_C_LABEL(ctxbusy))], %g4

	sllx	%g6, 3, %g6				! Make it into an offset into ctxbusy
	ldx	[%g4 + %g6], %g4			! Load up our page table.

	srax	%g3, HOLESHIFT, %g5			! Check for valid address
	brz,pt	%g5, 0f					! Should be zero or -1
	 inc	%g5					! Make -1 -> 0
	brnz,pn	%g5, sun4v_tl1_ptbl_miss		! Error! In hole!
0:
	srlx	%g3, STSHIFT, %g6
	and	%g6, STMASK, %g6			! Index into pm_segs
	sll	%g6, 3, %g6
	add	%g4, %g6, %g4
	ldxa	[%g4] ASI_PHYS_CACHED, %g4		! Load page directory pointer

	srlx	%g3, PDSHIFT, %g6
	and	%g6, PDMASK, %g6
	sll	%g6, 3, %g6
	brz,pn	%g4, sun4v_tl1_ptbl_miss		! NULL entry? check somewhere else
	 add	%g4, %g6, %g4
	ldxa	[%g4] ASI_PHYS_CACHED, %g4		! Load page table pointer

	srlx	%g3, PTSHIFT, %g6			! Convert to ptab offset
	and	%g6, PTMASK, %g6
	sll	%g6, 3, %g6
	brz,pn	%g4, sun4v_tl1_ptbl_miss		! NULL entry? check somewhere else
	 add	%g4, %g6, %g6
1:
	ldxa	[%g6] ASI_PHYS_CACHED, %g4
	brgez,pn %g4, sun4v_tl1_ptbl_miss		! Entry invalid?  Punt
	 or	%g4, SUN4V_TLB_ACCESS, %g7		! Update the access bit

	btst	SUN4V_TLB_ACCESS, %g4			! Need to update access git?
	bne,pt	%xcc, 2f
	 nop
	casxa	[%g6] ASI_PHYS_CACHED, %g4, %g7		!  and write it out
	cmp	%g4, %g7
	bne,pn	%xcc, 1b
	 or	%g4, SUN4V_TLB_ACCESS, %g4		! Update the modified bit
2:
	sethi	%hi(_C_LABEL(tsb_dmmu)), %g2
	ldx	[%g2 + %lo(_C_LABEL(tsb_dmmu))], %g2

	mov	%g1, %g7
	/* Construct TSB tag word. */
	add	%g1, 0x50, %g6
	ldxa	[%g6] ASI_PHYS_CACHED, %g6
	mov	%g3, %g1
	srlx	%g1, 22, %g1
	sllx	%g6, 48, %g6
	or	%g1, %g6, %g1

	srlx	%g3, PTSHIFT, %g3
	sethi	%hi(_C_LABEL(tsbsize)), %g5
	mov	512, %g6
	ld	[%g5 + %lo(_C_LABEL(tsbsize))], %g5
	sllx	%g6, %g5, %g5
	sub	%g5, 1, %g5
	and	%g3, %g5, %g3
	sllx	%g3, 4, %g3
	add	%g2, %g3, %g2

3:
	ld	[%g2], %g3
	btst	(TSB_TAG_LOCKED >> 32), %g3
	bnz,pn	%icc, 3b
	 or	%g3, (TSB_TAG_LOCKED >> 32), %g5
	casa	[%g2] ASI_NUCLEUS, %g3, %g5
	cmp	%g3, %g5
	bne,pn	%icc, 3b
	 nop
	membar	#StoreStore
	stx	%g4, [%g2 + 8]
	stx	%g1, [%g2]		! unlock

	retry
	NOTREACHED

sun4v_tl1_dtsb_prot:
	GET_MMFSA(%g1)
	add	%g1, 0x48, %g3
	ldxa	[%g3] ASI_PHYS_CACHED, %g3
	add	%g1, 0x50, %g6
	ldxa	[%g6] ASI_PHYS_CACHED, %g6
	sethi	%hi(_C_LABEL(ctxbusy)), %g4
	ldx	[%g4 + %lo(_C_LABEL(ctxbusy))], %g4

	sllx	%g6, 3, %g6				! Make it into an offset into ctxbusy
	ldx	[%g4 + %g6], %g4			! Load up our page table.

	srax	%g3, HOLESHIFT, %g5			! Check for valid address
	brz,pt	%g5, 0f					! Should be zero or -1
	 inc	%g5					! Make -1 -> 0
	brnz,pn	%g5, sun4v_tl1_ptbl_miss		! Error! In hole!
0:
	srlx	%g3, STSHIFT, %g6
	and	%g6, STMASK, %g6			! Index into pm_segs
	sll	%g6, 3, %g6
	add	%g4, %g6, %g4
	ldxa	[%g4] ASI_PHYS_CACHED, %g4		! Load page directory pointer

	srlx	%g3, PDSHIFT, %g6
	and	%g6, PDMASK, %g6
	sll	%g6, 3, %g6
	brz,pn	%g4, sun4v_tl1_ptbl_miss		! NULL entry? check somewhere else
	 add	%g4, %g6, %g4
	ldxa	[%g4] ASI_PHYS_CACHED, %g4		! Load page table pointer

	srlx	%g3, PTSHIFT, %g6			! Convert to ptab offset
	and	%g6, PTMASK, %g6
	sll	%g6, 3, %g6
	brz,pn	%g4, sun4v_tl1_ptbl_miss		! NULL entry? check somewhere else
	 add	%g4, %g6, %g6
1:
	ldxa	[%g6] ASI_PHYS_CACHED, %g4
	brgez,pn %g4, sun4v_tl1_ptbl_miss		! Entry invalid?  Punt
	 or	%g4, SUN4V_TLB_MODIFY|SUN4V_TLB_ACCESS|SUN4V_TLB_W, %g7
		! Update the modified bit

#	btst	SUN4V_TLB_REAL_W|SUN4V_TLB_W, %g4	! Is it a ref fault?
	mov	1, %g2
	sllx	%g2, 61, %g2
	or	%g2, SUN4V_TLB_W, %g2
	btst	%g2, %g4
	bz,pn	%xcc, sun4v_tl1_ptbl_miss		! No -- really fault
	 nop
	casxa	[%g6] ASI_PHYS_CACHED, %g4, %g7		!  and write it out
	cmp	%g4, %g7
	bne,pn	%xcc, 1b
	 or	%g4, SUN4V_TLB_MODIFY|SUN4V_TLB_ACCESS|SUN4V_TLB_W, %g4
		! Update the modified bit
2:
	sethi	%hi(_C_LABEL(tsb_dmmu)), %g2
	ldx	[%g2 + %lo(_C_LABEL(tsb_dmmu))], %g2

	mov	%g1, %g7
	/* Construct TSB tag word. */
	add	%g1, 0x50, %g6
	ldxa	[%g6] ASI_PHYS_CACHED, %g6
	mov	%g3, %g1
	srlx	%g1, 22, %g1
	sllx	%g6, 48, %g6
	or	%g1, %g6, %g1

	srlx	%g3, PTSHIFT, %g3
	sethi	%hi(_C_LABEL(tsbsize)), %g5
	mov	512, %g6
	ld	[%g5 + %lo(_C_LABEL(tsbsize))], %g5
	sllx	%g6, %g5, %g5
	sub	%g5, 1, %g5
	and	%g3, %g5, %g3
	sllx	%g3, 4, %g3
	add	%g2, %g3, %g2

3:
	ld	[%g2], %g3
	btst	(TSB_TAG_LOCKED >> 32), %g3
	bnz,pn	%icc, 3b
	 or	%g3, (TSB_TAG_LOCKED >> 32), %g5
	casa	[%g2] ASI_NUCLEUS, %g3, %g5
	cmp	%g3, %g5
	bne,pn	%icc, 3b
	 nop
	membar	#StoreStore
	stx	%g4, [%g2 + 8]
	stx	%g1, [%g2]		! unlock

	mov	%o0, %g1
	mov	%o1, %g2
	mov	%o2, %g3

#define MAP_DTLB	0x1
#define MAP_ITLB	0x2
#define MMU_UNMAP_ADDR	0x84
	add	%g7, 0x48, %o0
	ldxa	[%o0] ASI_PHYS_CACHED, %o0
	add	%g7, 0x50, %o1
	ldxa	[%o1] ASI_PHYS_CACHED, %o1
	mov	MAP_DTLB, %o2
	ta	MMU_UNMAP_ADDR

	mov	%g1, %o0
	mov	%g2, %o1
	mov	%g3, %o2

	retry
	NOTREACHED


sun4v_tl1_ptbl_miss:
	rdpr	%tpc, %g1

	set	rft_user_fault_start, %g2
	cmp	%g1, %g2
	blu,pt	%xcc, 1f
	 set	rft_user_fault_end, %g2
	cmp	%g1, %g2
	bgeu,pt	%xcc, 1f
	 nop

	/* Fixup %cwp. */
	rdpr	%cwp, %g1
	inc	%g1
	wrpr	%g1, %cwp

	rdpr	%tt, %g1
	wrpr	1, %tl
	wrpr	%g1, %tt
	rdpr	%cwp, %g1
	set	TSTATE_KERN, %g2
	wrpr	%g1, %g2, %tstate
	set	return_from_trap, %g1
	wrpr	%g1, %tpc
	add	%g1, 4, %g1
	wrpr	%g1, %tnpc
	wrpr	%g0, 1, %gl

	ba,pt %xcc, sun4v_datatrap
	 wrpr	WSTATE_KERN, %wstate

1:
	rdpr	%tstate, %g3
	rdpr	%tt, %g4

	rdpr	%tl, %g1
	dec	%g1
	wrpr	%g1, %tl
	rdpr	%tt, %g2
	inc	%g1
	wrpr	%g1, %tl

	wrpr	%g0, %g3, %tstate
	wrpr	%g0, %g4, %tt

	andn	%g2, 0x00f, %g3
	cmp	%g3, 0x080
	be,pn	%icc, flush_normals
	 nop
	cmp	%g3, 0x0a0
	be,pn	%icc, flush_others
	 nop
	cmp	%g3, 0x0c0
	be,pn	%icc, ufill_trap
	 nop

	Debugger()
	NOTREACHED

flush_others:
	set	pcbspill_others, %g1
	wrpr	%g1, %tnpc
	done
	NOTREACHED

flush_normals:
ufill_trap:
	/*
	 * Rearrange our trap state such that it appears as if we got
	 * this trap directly from user mode.  Then process it at TL = 1.
	 * We'll take the spill/fill trap again once we return to user mode.
	 */
	rdpr	%tt, %g1
	rdpr	%tstate, %g3
	wrpr	%g0, 1, %tl
	wrpr	%g0, %g1, %tt
	rdpr	%tstate, %g2
	wrpr	%g0, 2, %tl
	and	%g2, TSTATE_CWP, %g2
	andn	%g3, TSTATE_CWP, %g3
	wrpr	%g2, %g3, %tstate
	set	sun4v_datatrap, %g4
	wrpr	%g0, %g4, %tnpc
	done

sun4v_tl0_dtsb_miss:
	GET_MMFSA(%g1)
	add	%g1, 0x48, %g3
	ldxa	[%g3] ASI_PHYS_CACHED, %g3
	add	%g1, 0x50, %g6
	ldxa	[%g6] ASI_PHYS_CACHED, %g6
	sethi	%hi(_C_LABEL(ctxbusy)), %g4
	ldx	[%g4 + %lo(_C_LABEL(ctxbusy))], %g4

	sllx	%g6, 3, %g6				! Make it into an offset into ctxbusy
	ldx	[%g4 + %g6], %g4			! Load up our page table.

	srax	%g3, HOLESHIFT, %g5			! Check for valid address
	brz,pt	%g5, 0f					! Should be zero or -1
	 inc	%g5					! Make -1 -> 0
	brnz,pn	%g5, sun4v_datatrap			! Error! In hole!
0:
	srlx	%g3, STSHIFT, %g6
	and	%g6, STMASK, %g6			! Index into pm_segs
	sll	%g6, 3, %g6
	add	%g4, %g6, %g4
	ldxa	[%g4] ASI_PHYS_CACHED, %g4		! Load page directory pointer

	srlx	%g3, PDSHIFT, %g6
	and	%g6, PDMASK, %g6
	sll	%g6, 3, %g6
	brz,pn	%g4, sun4v_datatrap			! NULL entry? check somewhere else
	 add	%g4, %g6, %g4
	ldxa	[%g4] ASI_PHYS_CACHED, %g4		! Load page table pointer

	srlx	%g3, PTSHIFT, %g6			! Convert to ptab offset
	and	%g6, PTMASK, %g6
	sll	%g6, 3, %g6
	brz,pn	%g4, sun4v_datatrap			! NULL entry? check somewhere else
	 add	%g4, %g6, %g6
1:
	ldxa	[%g6] ASI_PHYS_CACHED, %g4
	brgez,pn %g4, sun4v_datatrap			! Entry invalid?  Punt
	 or	%g4, SUN4V_TLB_ACCESS, %g7		! Update the access bit

	btst	SUN4V_TLB_ACCESS, %g4			! Need to update access git?
	bne,pt	%xcc, 2f
	 nop
	casxa	[%g6] ASI_PHYS_CACHED, %g4, %g7		!  and write it out
	cmp	%g4, %g7
	bne,pn	%xcc, 1b
	 or	%g4, SUN4V_TLB_ACCESS, %g4		! Update the modified bit
2:
	sethi	%hi(_C_LABEL(tsb_dmmu)), %g2
	ldx	[%g2 + %lo(_C_LABEL(tsb_dmmu))], %g2

	mov	%g1, %g7
	/* Construct TSB tag word. */
	add	%g1, 0x50, %g6
	ldxa	[%g6] ASI_PHYS_CACHED, %g6
	mov	%g3, %g1
	srlx	%g1, 22, %g1
	sllx	%g6, 48, %g6
	or	%g1, %g6, %g1

	srlx	%g3, PTSHIFT, %g3
	sethi	%hi(_C_LABEL(tsbsize)), %g5
	mov	512, %g6
	ld	[%g5 + %lo(_C_LABEL(tsbsize))], %g5
	sllx	%g6, %g5, %g5
	sub	%g5, 1, %g5
	and	%g3, %g5, %g3
	sllx	%g3, 4, %g3
	add	%g2, %g3, %g2

3:
	ld	[%g2], %g3
	btst	(TSB_TAG_LOCKED >> 32), %g3
	bnz,pn	%icc, 3b
	 or	%g3, (TSB_TAG_LOCKED >> 32), %g5
	casa	[%g2] ASI_NUCLEUS, %g3, %g5
	cmp	%g3, %g5
	bne,pn	%icc, 3b
	 nop
	membar	#StoreStore
	stx	%g4, [%g2 + 8]
	stx	%g1, [%g2]		! unlock

	retry
	NOTREACHED

sun4v_tl0_dtsb_prot:
	GET_MMFSA(%g1)
	add	%g1, 0x48, %g3
	ldxa	[%g3] ASI_PHYS_CACHED, %g3
	add	%g1, 0x50, %g6
	ldxa	[%g6] ASI_PHYS_CACHED, %g6
	sethi	%hi(_C_LABEL(ctxbusy)), %g4
	ldx	[%g4 + %lo(_C_LABEL(ctxbusy))], %g4

	sllx	%g6, 3, %g6				! Make it into an offset into ctxbusy
	ldx	[%g4 + %g6], %g4			! Load up our page table.

	srax	%g3, HOLESHIFT, %g5			! Check for valid address
	brz,pt	%g5, 0f					! Should be zero or -1
	 inc	%g5					! Make -1 -> 0
	brnz,pn	%g5, sun4v_datatrap			! Error! In hole!
0:
	srlx	%g3, STSHIFT, %g6
	and	%g6, STMASK, %g6			! Index into pm_segs
	sll	%g6, 3, %g6
	add	%g4, %g6, %g4
	ldxa	[%g4] ASI_PHYS_CACHED, %g4		! Load page directory pointer

	srlx	%g3, PDSHIFT, %g6
	and	%g6, PDMASK, %g6
	sll	%g6, 3, %g6
	brz,pn	%g4, sun4v_datatrap			! NULL entry? check somewhere else
	 add	%g4, %g6, %g4
	ldxa	[%g4] ASI_PHYS_CACHED, %g4		! Load page table pointer

	srlx	%g3, PTSHIFT, %g6			! Convert to ptab offset
	and	%g6, PTMASK, %g6
	sll	%g6, 3, %g6
	brz,pn	%g4, sun4v_datatrap			! NULL entry? check somewhere else
	 add	%g4, %g6, %g6
1:
	ldxa	[%g6] ASI_PHYS_CACHED, %g4
	brgez,pn %g4, sun4v_datatrap			! Entry invalid?  Punt
	 or	%g4, SUN4V_TLB_MODIFY|SUN4V_TLB_ACCESS|SUN4V_TLB_W, %g7
		! Update the modified bit

#	btst	SUN4V_TLB_REAL_W|SUN4V_TLB_W, %g4	! Is it a ref fault?
	mov	1, %g2
	sllx	%g2, 61, %g2
	or	%g2, SUN4V_TLB_W, %g2
	btst	%g2, %g4
	bz,pn	%xcc, sun4v_datatrap			! No -- really fault
	 nop
	casxa	[%g6] ASI_PHYS_CACHED, %g4, %g7		!  and write it out
	cmp	%g4, %g7
	bne,pn	%xcc, 1b
	 or	%g4, SUN4V_TLB_MODIFY|SUN4V_TLB_ACCESS|SUN4V_TLB_W, %g4
		! Update the modified bit
2:
	sethi	%hi(_C_LABEL(tsb_dmmu)), %g2
	ldx	[%g2 + %lo(_C_LABEL(tsb_dmmu))], %g2

	mov	%g1, %g7
	/* Construct TSB tag word. */
	add	%g1, 0x50, %g6
	ldxa	[%g6] ASI_PHYS_CACHED, %g6
	mov	%g3, %g1
	srlx	%g1, 22, %g1
	sllx	%g6, 48, %g6
	or	%g1, %g6, %g1

	srlx	%g3, PTSHIFT, %g3
	sethi	%hi(_C_LABEL(tsbsize)), %g5
	mov	512, %g6
	ld	[%g5 + %lo(_C_LABEL(tsbsize))], %g5
	sllx	%g6, %g5, %g5
	sub	%g5, 1, %g5
	and	%g3, %g5, %g3
	sllx	%g3, 4, %g3
	add	%g2, %g3, %g2

3:
	ld	[%g2], %g3
	btst	(TSB_TAG_LOCKED >> 32), %g3
	bnz,pn	%icc, 3b
	 or	%g3, (TSB_TAG_LOCKED >> 32), %g5
	casa	[%g2] ASI_NUCLEUS, %g3, %g5
	cmp	%g3, %g5
	bne,pn	%icc, 3b
	 nop
	membar	#StoreStore
	stx	%g4, [%g2 + 8]
	stx	%g1, [%g2]		! unlock

	mov	%o0, %g1
	mov	%o1, %g2
	mov	%o2, %g3

#define MAP_DTLB	0x1
#define MMU_UNMAP_ADDR	0x84
	add	%g7, 0x48, %o0
	ldxa	[%o0] ASI_PHYS_CACHED, %o0
	add	%g7, 0x50, %o1
	ldxa	[%o1] ASI_PHYS_CACHED, %o1
	mov	MAP_DTLB, %o2
	ta	MMU_UNMAP_ADDR

	mov	%g1, %o0
	mov	%g2, %o1
	mov	%g3, %o2

	retry
	NOTREACHED

sun4v_tl0_itsb_miss:
	GET_MMFSA(%g1)
	add	%g1, 0x8, %g3
	ldxa	[%g3] ASI_PHYS_CACHED, %g3
	add	%g1, 0x10, %g6
	ldxa	[%g6] ASI_PHYS_CACHED, %g6
	sethi	%hi(_C_LABEL(ctxbusy)), %g4
	ldx	[%g4 + %lo(_C_LABEL(ctxbusy))], %g4

	sllx	%g6, 3, %g6				! Make it into an offset into ctxbusy
	ldx	[%g4 + %g6], %g4			! Load up our page table.

	srax	%g3, HOLESHIFT, %g5			! Check for valid address
	brz,pt	%g5, 0f					! Should be zero or -1
	 inc	%g5					! Make -1 -> 0
	brnz,pn	%g5, sun4v_texttrap			! Error! In hole!
0:
	srlx	%g3, STSHIFT, %g6
	and	%g6, STMASK, %g6			! Index into pm_segs
	sll	%g6, 3, %g6
	add	%g4, %g6, %g4
	ldxa	[%g4] ASI_PHYS_CACHED, %g4		! Load page directory pointer

	srlx	%g3, PDSHIFT, %g6
	and	%g6, PDMASK, %g6
	sll	%g6, 3, %g6
	brz,pn	%g4, sun4v_texttrap			! NULL entry? check somewhere else
	 add	%g4, %g6, %g4
	ldxa	[%g4] ASI_PHYS_CACHED, %g4		! Load page table pointer

	srlx	%g3, PTSHIFT, %g6			! Convert to ptab offset
	and	%g6, PTMASK, %g6
	sll	%g6, 3, %g6
	brz,pn	%g4, sun4v_texttrap			! NULL entry? check somewhere else
	 add	%g4, %g6, %g6
1:
	ldxa	[%g6] ASI_PHYS_CACHED, %g4
	brgez,pn %g4, sun4v_texttrap			! Entry invalid?  Punt
	 or	%g4, SUN4V_TLB_ACCESS, %g7		! Update the access bit

	btst	SUN4V_TLB_EXEC, %g4
	bz,pn	%xcc, sun4v_texttrap
	 nop
	btst	SUN4V_TLB_ACCESS, %g4			! Need to update access git?
	bne,pt	%xcc, 2f
	 nop
	casxa	[%g6] ASI_PHYS_CACHED, %g4, %g7		!  and write it out
	cmp	%g4, %g7
	bne,pn	%xcc, 1b
	 or	%g4, SUN4V_TLB_ACCESS, %g4		! Update the modified bit
2:
	sethi	%hi(_C_LABEL(tsb_dmmu)), %g2
	ldx	[%g2 + %lo(_C_LABEL(tsb_dmmu))], %g2

	mov	%g1, %g7
	/* Construct TSB tag word. */
	add	%g1, 0x10, %g6
	ldxa	[%g6] ASI_PHYS_CACHED, %g6
	mov	%g3, %g1
	srlx	%g1, 22, %g1
	sllx	%g6, 48, %g6
	or	%g1, %g6, %g1

	srlx	%g3, PTSHIFT, %g3
	sethi	%hi(_C_LABEL(tsbsize)), %g5
	mov	512, %g6
	ld	[%g5 + %lo(_C_LABEL(tsbsize))], %g5
	sllx	%g6, %g5, %g5
	sub	%g5, 1, %g5
	and	%g3, %g5, %g3
	sllx	%g3, 4, %g3
	add	%g2, %g3, %g2

3:
	ld	[%g2], %g3
	btst	(TSB_TAG_LOCKED >> 32), %g3
	bnz,pn	%icc, 3b
	 or	%g3, (TSB_TAG_LOCKED >> 32), %g5
	casa	[%g2] ASI_NUCLEUS, %g3, %g5
	cmp	%g3, %g5
	bne,pn	%icc, 3b
	 nop
	membar	#StoreStore
	stx	%g4, [%g2 + 8]
	stx	%g1, [%g2]		! unlock

	retry
	NOTREACHED

kspill_normal:
	wrpr	0x90, %tt

	GET_CPUINFO_PA(%g1)
	wr	%g0, ASI_PHYS_CACHED, %asi

	SPILL	stxa, %g1 + CI_RW, 8, %asi
	saved

	stxa	%sp, [%g1 + CI_RWSP] %asi

	retry
	NOTREACHED

/*
 * Spill user windows into the PCB.
 */
pcbspill_normals:
	ba,pt	%xcc, pcbspill
	 wrpr	0x80, %tt

pcbspill_others:
	wrpr	0xa0, %tt

pcbspill:
	GET_CPUINFO_PA(%g6)
	wr	%g0, ASI_PHYS_CACHED, %asi
	ldxa	[%g6 + CI_CPCB] %asi, %g6

	sethi	%hi(_C_LABEL(ctxbusy)), %g1
	ldx	[%g1 + %lo(_C_LABEL(ctxbusy))], %g1
	ldx	[%g1], %g1

	srlx	%g6, STSHIFT, %g7
	and	%g7, STMASK, %g7
	sll	%g7, 3, %g7
	add	%g7, %g1, %g1
	ldxa	[%g1] ASI_PHYS_CACHED, %g1		! Load pointer to directory

	srlx	%g6, PDSHIFT, %g7			! Do page directory
	and	%g7, PDMASK, %g7
	sll	%g7, 3, %g7
	brz,pn	%g1, pcbspill_fail
	 add	%g7, %g1, %g1
	ldxa	[%g1] ASI_PHYS_CACHED, %g1

	srlx	%g6, PTSHIFT, %g7			! Convert to ptab offset
	and	%g7, PTMASK, %g7
	brz	%g1, pcbspill_fail
	 sll	%g7, 3, %g7
	add	%g1, %g7, %g7
	ldxa	[%g7] ASI_PHYS_CACHED, %g7		! This one is not
	brgez	%g7, pcbspill_fail
	 srlx	%g7, PGSHIFT, %g7			! Isolate PA part
	sll	%g6, 32-PGSHIFT, %g6			! And offset
	sllx	%g7, PGSHIFT+8, %g7			! There are 8 bits to the left of the PA in the TTE
	srl	%g6, 32-PGSHIFT, %g6
	srax	%g7, 8, %g7
	or	%g7, %g6, %g6				! Then combine them to form PA

!	wr	%g0, ASI_PHYS_CACHED, %asi		! Use ASI_PHYS_CACHED to prevent possible page faults
	
	lduba	[%g6 + PCB_NSAVED] %asi, %g7
	sllx	%g7, 7, %g5
	add	%g6, %g5, %g5
	SPILL	stxa, %g5 + PCB_RW, 8, %asi
	saved

	sllx	%g7, 3, %g5
	add	%g6, %g5, %g5
	stxa	%sp, [%g5 + PCB_RWSP] %asi

	inc	%g7
	stba	%g7, [%g6 + PCB_NSAVED] %asi

	retry
	NOTREACHED

pcbspill_fail:
	Debugger()
	NOTREACHED


sun4v_datatrap:
	GET_MMFSA(%g3)
	add	%g3, 0x48, %g1
	ldxa	[%g1] ASI_PHYS_CACHED, %g1
	add	%g3, 0x50, %g2
	ldxa	[%g2] ASI_PHYS_CACHED, %g2

	TRAP_SETUP -CC64FSZ-TF_SIZE
	or	%g1, %g2, %o3
	mov	%g1, %o4

	rdpr	%tt, %g4
	rdpr	%tstate, %g1
	rdpr	%tpc, %g2
	rdpr	%tnpc, %g3

	stx	%g1, [%sp + CC64FSZ + BIAS + TF_TSTATE]
	mov	%g4, %o1		! (type)
	stx	%g2, [%sp + CC64FSZ + BIAS + TF_PC]
	rd	%y, %g5
	stx	%g3, [%sp + CC64FSZ + BIAS + TF_NPC]
	st	%g5, [%sp + CC64FSZ + BIAS + TF_Y]
	mov	%g2, %o2		! (pc)
	sth	%o1, [%sp + CC64FSZ + BIAS + TF_TT]! debug

	cmp	%o1, T_FDMMU_PROT
	bne,pn	%icc, 1f
	 mov	SFSR_FV, %o5
	or	%o5, SFSR_W, %o5

1:
	wrpr	%g0, PSTATE_KERN, %pstate		! Get back to normal globals
	wrpr	%g0, 0, %gl

	stx	%g1, [%sp + CC64FSZ + BIAS + TF_G + (1*8)]
	stx	%g2, [%sp + CC64FSZ + BIAS + TF_G + (2*8)]
	add	%sp, CC64FSZ + BIAS, %o0		! (&tf)
	stx	%g3, [%sp + CC64FSZ + BIAS + TF_G + (3*8)]
	stx	%g4, [%sp + CC64FSZ + BIAS + TF_G + (4*8)]
	stx	%g5, [%sp + CC64FSZ + BIAS + TF_G + (5*8)]
	rdpr	%pil, %g5
	stx	%g6, [%sp + CC64FSZ + BIAS + TF_G + (6*8)]
	stx	%g7, [%sp + CC64FSZ + BIAS + TF_G + (7*8)]
	stb	%g5, [%sp + CC64FSZ + BIAS + TF_PIL]
	stb	%g5, [%sp + CC64FSZ + BIAS + TF_OLDPIL]

	/*
	 * Phew, ready to enable traps and call C code.
	 */
	wrpr	%g0, 0, %tl

	GET_CPUINFO_VA(%g7)
	ldx	[%g7 + CI_RWSP], %g2
	brz,pt	%g2, 1f
	 nop

	ldx	[%g7 + CI_RW + (0*8)], %l0
	ldx	[%g7 + CI_RW + (1*8)], %l1
	ldx	[%g7 + CI_RW + (2*8)], %l2
	ldx	[%g7 + CI_RW + (3*8)], %l3
	ldx	[%g7 + CI_RW + (4*8)], %l4
	ldx	[%g7 + CI_RW + (5*8)], %l5
	ldx	[%g7 + CI_RW + (6*8)], %l6
	ldx	[%g7 + CI_RW + (7*8)], %l7
	stx	%l0, [%g2 + BIAS + (0*8)]
	stx	%l1, [%g2 + BIAS + (1*8)]
	stx	%l2, [%g2 + BIAS + (2*8)]
	stx	%l3, [%g2 + BIAS + (3*8)]
	stx	%l4, [%g2 + BIAS + (4*8)]
	stx	%l5, [%g2 + BIAS + (5*8)]
	stx	%l6, [%g2 + BIAS + (6*8)]
	stx	%l7, [%g2 + BIAS + (7*8)]
	ldx	[%g7 + CI_RW + (8*8)], %l0
	ldx	[%g7 + CI_RW + (9*8)], %l1
	ldx	[%g7 + CI_RW + (10*8)], %l2
	ldx	[%g7 + CI_RW + (11*8)], %l3
	ldx	[%g7 + CI_RW + (12*8)], %l4
	ldx	[%g7 + CI_RW + (13*8)], %l5
	ldx	[%g7 + CI_RW + (14*8)], %l6
	ldx	[%g7 + CI_RW + (15*8)], %l7
	stx	%l0, [%g2 + BIAS + (8*8)]
	stx	%l1, [%g2 + BIAS + (9*8)]
	stx	%l2, [%g2 + BIAS + (10*8)]
	stx	%l3, [%g2 + BIAS + (11*8)]
	stx	%l4, [%g2 + BIAS + (12*8)]
	stx	%l5, [%g2 + BIAS + (13*8)]
	stx	%l6, [%g2 + BIAS + (14*8)]
	stx	%l7, [%g2 + BIAS + (15*8)]

	stx	%g0, [%g7 + CI_RWSP]
1:

	wr	%g0, ASI_PRIMARY_NOFAULT, %asi	! Restore default ASI
	wrpr	%g0, PSTATE_INTR, %pstate	! traps on again
	call	_C_LABEL(data_access_fault)	! data_acces_fault(tf, type, ...)
	 nop

	ba,a,pt	%icc, return_from_trap
	 nop
	NOTREACHED

sun4v_texttrap:
	GET_MMFSA(%g3)
	add	%g3, 0x08, %g1
	ldxa	[%g1] ASI_PHYS_CACHED, %g1
	add	%g3, 0x10, %g2
	ldxa	[%g2] ASI_PHYS_CACHED, %g2

	TRAP_SETUP -CC64FSZ-TF_SIZE

	or	%g1, %g2, %o2
	clr	%o3

	rdpr	%tt, %g4
	rdpr	%tstate, %g1
	rdpr	%tpc, %g2
	rdpr	%tnpc, %g3

	stx	%g1, [%sp + CC64FSZ + BIAS + TF_TSTATE]
	mov	%g4, %o1		! (type)
	stx	%g2, [%sp + CC64FSZ + BIAS + TF_PC]
	rd	%y, %g5
	stx	%g3, [%sp + CC64FSZ + BIAS + TF_NPC]
	st	%g5, [%sp + CC64FSZ + BIAS + TF_Y]
	sth	%o1, [%sp + CC64FSZ + BIAS + TF_TT]! debug

	wrpr	%g0, PSTATE_KERN, %pstate		! Get back to normal globals
	wrpr	%g0, 0, %gl

	stx	%g1, [%sp + CC64FSZ + BIAS + TF_G + (1*8)]
	stx	%g2, [%sp + CC64FSZ + BIAS + TF_G + (2*8)]
	add	%sp, CC64FSZ + BIAS, %o0		! (&tf)
	stx	%g3, [%sp + CC64FSZ + BIAS + TF_G + (3*8)]
	stx	%g4, [%sp + CC64FSZ + BIAS + TF_G + (4*8)]
	stx	%g5, [%sp + CC64FSZ + BIAS + TF_G + (5*8)]
	rdpr	%pil, %g5
	stx	%g6, [%sp + CC64FSZ + BIAS + TF_G + (6*8)]
	stx	%g7, [%sp + CC64FSZ + BIAS + TF_G + (7*8)]
	stb	%g5, [%sp + CC64FSZ + BIAS + TF_PIL]
	stb	%g5, [%sp + CC64FSZ + BIAS + TF_OLDPIL]

	/*
	 * Phew, ready to enable traps and call C code.
	 */
	wrpr	%g0, 0, %tl

	GET_CPUINFO_VA(%g7)
	ldx	[%g7 + CI_RWSP], %g2
	brz,pt	%g2, 1f
	 nop

	ldx	[%g7 + CI_RW + (0*8)], %l0
	ldx	[%g7 + CI_RW + (1*8)], %l1
	ldx	[%g7 + CI_RW + (2*8)], %l2
	ldx	[%g7 + CI_RW + (3*8)], %l3
	ldx	[%g7 + CI_RW + (4*8)], %l4
	ldx	[%g7 + CI_RW + (5*8)], %l5
	ldx	[%g7 + CI_RW + (6*8)], %l6
	ldx	[%g7 + CI_RW + (7*8)], %l7
	stx	%l0, [%g2 + BIAS + (0*8)]
	stx	%l1, [%g2 + BIAS + (1*8)]
	stx	%l2, [%g2 + BIAS + (2*8)]
	stx	%l3, [%g2 + BIAS + (3*8)]
	stx	%l4, [%g2 + BIAS + (4*8)]
	stx	%l5, [%g2 + BIAS + (5*8)]
	stx	%l6, [%g2 + BIAS + (6*8)]
	stx	%l7, [%g2 + BIAS + (7*8)]
	ldx	[%g7 + CI_RW + (8*8)], %l0
	ldx	[%g7 + CI_RW + (9*8)], %l1
	ldx	[%g7 + CI_RW + (10*8)], %l2
	ldx	[%g7 + CI_RW + (11*8)], %l3
	ldx	[%g7 + CI_RW + (12*8)], %l4
	ldx	[%g7 + CI_RW + (13*8)], %l5
	ldx	[%g7 + CI_RW + (14*8)], %l6
	ldx	[%g7 + CI_RW + (15*8)], %l7
	stx	%l0, [%g2 + BIAS + (8*8)]
	stx	%l1, [%g2 + BIAS + (9*8)]
	stx	%l2, [%g2 + BIAS + (10*8)]
	stx	%l3, [%g2 + BIAS + (11*8)]
	stx	%l4, [%g2 + BIAS + (12*8)]
	stx	%l5, [%g2 + BIAS + (13*8)]
	stx	%l6, [%g2 + BIAS + (14*8)]
	stx	%l7, [%g2 + BIAS + (15*8)]

	stx	%g0, [%g7 + CI_RWSP]
1:

	wr	%g0, ASI_PRIMARY_NOFAULT, %asi	! Restore default ASI
	wrpr	%g0, PSTATE_INTR, %pstate	! traps on again
	call	_C_LABEL(text_access_fault)	! text_access_fault(tf, type, ...)
	 nop

	ba,a,pt	%icc, return_from_trap
	 nop
	NOTREACHED

#endif

/*
 * We're here because we took an alignment fault in NUCLEUS context.
 * This could be a kernel bug or it could be due to saving or restoring
 * a user window to/from an invalid stack pointer.
 * 
 * If the latter is the case, we could try to emulate unaligned accesses, 
 * but we really don't know where to store the registers since we can't 
 * determine if there's a stack bias.  Or we could store all the regs 
 * into the PCB and punt, until the user program uses up all the CPU's
 * register windows and we run out of places to store them.  So for
 * simplicity we'll just blow them away and enter the trap code which
 * will generate a bus error.  Debugging the problem will be a bit
 * complicated since lots of register windows will be lost, but what
 * can we do?
 * 
 * XXX The trap code generates SIGKILL for now.
 */
checkalign:
	rdpr	%tl, %g2
	subcc	%g2, 1, %g1
	bneg,pn	%icc, slowtrap		! Huh?
	 nop

	wrpr	%g1, 0, %tl
	rdpr	%tt, %g7
	rdpr	%tstate, %g4
	andn	%g7, 0x07f, %g5		! Window spill traps are all 0b 0000 10xx xxxx
	cmp	%g5, 0x080		! Window fill traps are all 0b 0000 11xx xxxx
	bne,a,pn %icc, slowtrap
	 nop

	/*
         * %g1 -- current tl
	 * %g2 -- original tl
	 * %g4 -- tstate
         * %g7 -- tt
	 */

	and	%g4, CWP, %g5
	wrpr	%g5, %cwp		! Go back to the original register window

	rdpr	%otherwin, %g6
	rdpr	%cansave, %g5
	add	%g5, %g6, %g5
	wrpr	%g0, 0, %otherwin	! Just blow away all user windows
	wrpr	%g5, 0, %cansave
	rdpr	%canrestore, %g5
	wrpr	%g5, 0, %cleanwin

	wrpr	%g0, T_ALIGN, %tt	! This was an alignment fault 
	/*
	 * Now we need to determine if this was a userland store/load or not.
	 * Userland stores occur in anything other than the kernel spill/fill
	 * handlers (trap type 0x9x/0xdx).
	 */
	and	%g7, 0xff0, %g5
	cmp	%g5, 0x90
	bz,pn	%icc, slowtrap
	 nop
	cmp	%g5, 0xd0
	bz,pn	%icc, slowtrap
	 nop
	and	%g7, 0xfc0, %g5
	wrpr	%g5, 0, %tt
	ba,a,pt	%icc, slowtrap
	 nop

/*
 * slowtrap() builds a trap frame and calls trap().
 * This is called `slowtrap' because it *is*....
 * We have to build a full frame for ptrace(), for instance.
 *
 * Registers:
 *
 */
slowtrap:
	TRAP_SETUP -CC64FSZ-TF_SIZE

	rdpr	%tt, %g4
	rdpr	%tstate, %g1
	rdpr	%tpc, %g2
	rdpr	%tnpc, %g3

Lslowtrap_reenter:
	stx	%g1, [%sp + CC64FSZ + BIAS + TF_TSTATE]
	mov	%g4, %o1		! (type)
	stx	%g2, [%sp + CC64FSZ + BIAS + TF_PC]
	rd	%y, %g5
	stx	%g3, [%sp + CC64FSZ + BIAS + TF_NPC]
	mov	%g1, %o3		! (pstate)
	st	%g5, [%sp + CC64FSZ + BIAS + TF_Y]
	mov	%g2, %o2		! (pc)
	sth	%o1, [%sp + CC64FSZ + BIAS + TF_TT]! debug

	NORMAL_GLOBALS()

	stx	%g1, [%sp + CC64FSZ + BIAS + TF_G + (1*8)]
	stx	%g2, [%sp + CC64FSZ + BIAS + TF_G + (2*8)]
	add	%sp, CC64FSZ + BIAS, %o0		! (&tf)
	stx	%g3, [%sp + CC64FSZ + BIAS + TF_G + (3*8)]
	stx	%g4, [%sp + CC64FSZ + BIAS + TF_G + (4*8)]
	stx	%g5, [%sp + CC64FSZ + BIAS + TF_G + (5*8)]
	rdpr	%pil, %g5
	stx	%g6, [%sp + CC64FSZ + BIAS + TF_G + (6*8)]
	stx	%g7, [%sp + CC64FSZ + BIAS + TF_G + (7*8)]
	stb	%g5, [%sp + CC64FSZ + BIAS + TF_PIL]
	stb	%g5, [%sp + CC64FSZ + BIAS + TF_OLDPIL]

	/*
	 * Phew, ready to enable traps and call C code.
	 */
	wrpr	%g0, 0, %tl

	GET_CPUINFO_VA(%g7)
#ifdef SUN4V
	ldx	[%g7 + CI_RWSP], %g2
	brz,pt	%g2, 1f
	 nop

	ldx	[%g7 + CI_RW + (0*8)], %l0
	ldx	[%g7 + CI_RW + (1*8)], %l1
	ldx	[%g7 + CI_RW + (2*8)], %l2
	ldx	[%g7 + CI_RW + (3*8)], %l3
	ldx	[%g7 + CI_RW + (4*8)], %l4
	ldx	[%g7 + CI_RW + (5*8)], %l5
	ldx	[%g7 + CI_RW + (6*8)], %l6
	ldx	[%g7 + CI_RW + (7*8)], %l7
	stx	%l0, [%g2 + BIAS + (0*8)]
	stx	%l1, [%g2 + BIAS + (1*8)]
	stx	%l2, [%g2 + BIAS + (2*8)]
	stx	%l3, [%g2 + BIAS + (3*8)]
	stx	%l4, [%g2 + BIAS + (4*8)]
	stx	%l5, [%g2 + BIAS + (5*8)]
	stx	%l6, [%g2 + BIAS + (6*8)]
	stx	%l7, [%g2 + BIAS + (7*8)]
	ldx	[%g7 + CI_RW + (8*8)], %l0
	ldx	[%g7 + CI_RW + (9*8)], %l1
	ldx	[%g7 + CI_RW + (10*8)], %l2
	ldx	[%g7 + CI_RW + (11*8)], %l3
	ldx	[%g7 + CI_RW + (12*8)], %l4
	ldx	[%g7 + CI_RW + (13*8)], %l5
	ldx	[%g7 + CI_RW + (14*8)], %l6
	ldx	[%g7 + CI_RW + (15*8)], %l7
	stx	%l0, [%g2 + BIAS + (8*8)]
	stx	%l1, [%g2 + BIAS + (9*8)]
	stx	%l2, [%g2 + BIAS + (10*8)]
	stx	%l3, [%g2 + BIAS + (11*8)]
	stx	%l4, [%g2 + BIAS + (12*8)]
	stx	%l5, [%g2 + BIAS + (13*8)]
	stx	%l6, [%g2 + BIAS + (14*8)]
	stx	%l7, [%g2 + BIAS + (15*8)]

	stx	%g0, [%g7 + CI_RWSP]
1:
#endif

	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore default ASI
	wrpr	%g0, PSTATE_INTR, %pstate	! traps on again
	call	_C_LABEL(trap)			! trap(tf, type, pc, pstate)
	 nop

	ba,a,pt	%icc, return_from_trap
	 nop
	NOTREACHED

/*
 * Do a `software' trap by re-entering the trap code, possibly first
 * switching from interrupt stack to kernel stack.  This is used for
 * scheduling and signal ASTs (which generally occur from softclock or
 * tty or net interrupts).
 *
 * We enter with the trap type in %g1.  All we have to do is jump to
 * Lslowtrap_reenter above, but maybe after switching stacks....
 *
 * We should be running alternate globals.  The normal globals and
 * out registers were just loaded from the old trap frame.
 *
 *	Input Params:
 *	%g1 = tstate
 *	%g2 = tpc
 *	%g3 = tnpc
 *	%g4 = tt == T_AST
 */
softtrap:
	GET_CPUINFO_VA(%g5)
	sethi	%hi(EINTSTACK-INTSTACK), %g7
	sub	%g5, BIAS, %g5
	dec	%g7

	sub	%g5, %sp, %g5
	andncc	%g5, %g7, %g0
	bnz,pt	%xcc, Lslowtrap_reenter
	 nop
	GET_CPCB(%g7)
	set	USPACE-CC64FSZ-TF_SIZE-BIAS, %g5
	add	%g7, %g5, %g6
	stx	%g1, [%g6 + CC64FSZ + BIAS + TF_FAULT]		! Generate a new trapframe
	stx	%i0, [%g6 + CC64FSZ + BIAS + TF_O + (0*8)]	!	but don't bother with
	stx	%i1, [%g6 + CC64FSZ + BIAS + TF_O + (1*8)]	!	locals and ins
	stx	%i2, [%g6 + CC64FSZ + BIAS + TF_O + (2*8)]
	stx	%i3, [%g6 + CC64FSZ + BIAS + TF_O + (3*8)]
	stx	%i4, [%g6 + CC64FSZ + BIAS + TF_O + (4*8)]
	stx	%i5, [%g6 + CC64FSZ + BIAS + TF_O + (5*8)]
	stx	%i6, [%g6 + CC64FSZ + BIAS + TF_O + (6*8)]
	stx	%i7, [%g6 + CC64FSZ + BIAS + TF_O + (7*8)]
#ifdef DEBUG
	ldx	[%sp + CC64FSZ + BIAS + TF_I + (0*8)], %l0	! Copy over the rest of the regs
	ldx	[%sp + CC64FSZ + BIAS + TF_I + (1*8)], %l1	! But just dirty the locals
	ldx	[%sp + CC64FSZ + BIAS + TF_I + (2*8)], %l2
	ldx	[%sp + CC64FSZ + BIAS + TF_I + (3*8)], %l3
	ldx	[%sp + CC64FSZ + BIAS + TF_I + (4*8)], %l4
	ldx	[%sp + CC64FSZ + BIAS + TF_I + (5*8)], %l5
	ldx	[%sp + CC64FSZ + BIAS + TF_I + (6*8)], %l6
	ldx	[%sp + CC64FSZ + BIAS + TF_I + (7*8)], %l7
	stx	%l0, [%g6 + CC64FSZ + BIAS + TF_I + (0*8)]
	stx	%l1, [%g6 + CC64FSZ + BIAS + TF_I + (1*8)]
	stx	%l2, [%g6 + CC64FSZ + BIAS + TF_I + (2*8)]
	stx	%l3, [%g6 + CC64FSZ + BIAS + TF_I + (3*8)]
	stx	%l4, [%g6 + CC64FSZ + BIAS + TF_I + (4*8)]
	stx	%l5, [%g6 + CC64FSZ + BIAS + TF_I + (5*8)]
	stx	%l6, [%g6 + CC64FSZ + BIAS + TF_I + (6*8)]
	stx	%l7, [%g6 + CC64FSZ + BIAS + TF_I + (7*8)]
	ldx	[%sp + CC64FSZ + BIAS + TF_L + (0*8)], %l0
	ldx	[%sp + CC64FSZ + BIAS + TF_L + (1*8)], %l1
	ldx	[%sp + CC64FSZ + BIAS + TF_L + (2*8)], %l2
	ldx	[%sp + CC64FSZ + BIAS + TF_L + (3*8)], %l3
	ldx	[%sp + CC64FSZ + BIAS + TF_L + (4*8)], %l4
	ldx	[%sp + CC64FSZ + BIAS + TF_L + (5*8)], %l5
	ldx	[%sp + CC64FSZ + BIAS + TF_L + (6*8)], %l6
	ldx	[%sp + CC64FSZ + BIAS + TF_L + (7*8)], %l7
	stx	%l0, [%g6 + CC64FSZ + BIAS + TF_L + (0*8)]
	stx	%l1, [%g6 + CC64FSZ + BIAS + TF_L + (1*8)]
	stx	%l2, [%g6 + CC64FSZ + BIAS + TF_L + (2*8)]
	stx	%l3, [%g6 + CC64FSZ + BIAS + TF_L + (3*8)]
	stx	%l4, [%g6 + CC64FSZ + BIAS + TF_L + (4*8)]
	stx	%l5, [%g6 + CC64FSZ + BIAS + TF_L + (5*8)]
	stx	%l6, [%g6 + CC64FSZ + BIAS + TF_L + (6*8)]
	stx	%l7, [%g6 + CC64FSZ + BIAS + TF_L + (7*8)]
#endif	/* DEBUG */
	ba,pt	%xcc, Lslowtrap_reenter
	 mov	%g6, %sp

/*
 * syscall_setup() builds a trap frame and calls syscall().
 * sun_syscall is same but delivers sun system call number
 * XXX	should not have to save&reload ALL the registers just for
 *	ptrace...
 */
syscall_setup:
	TRAP_SETUP -CC64FSZ-TF_SIZE

#ifdef DEBUG
	rdpr	%tt, %o1	! debug
	sth	%o1, [%sp + CC64FSZ + BIAS + TF_TT]! debug
#endif	/* DEBUG */

	NORMAL_GLOBALS()

	stx	%g1, [%sp + CC64FSZ + BIAS + TF_G + ( 1*8)]
	mov	%g1, %o1			! code
	rdpr	%tpc, %o2			! (pc)
	stx	%g2, [%sp + CC64FSZ + BIAS + TF_G + ( 2*8)]
	rdpr	%tstate, %g1
	stx	%g3, [%sp + CC64FSZ + BIAS + TF_G + ( 3*8)]
	rdpr	%tnpc, %o3
	stx	%g4, [%sp + CC64FSZ + BIAS + TF_G + ( 4*8)]
	rd	%y, %o4
	stx	%g5, [%sp + CC64FSZ + BIAS + TF_G + ( 5*8)]
	stx	%g6, [%sp + CC64FSZ + BIAS + TF_G + ( 6*8)]
	wrpr	%g0, 0, %tl			! return to tl=0
	stx	%g7, [%sp + CC64FSZ + BIAS + TF_G + ( 7*8)]
	add	%sp, CC64FSZ + BIAS, %o0	! (&tf)

	stx	%g1, [%sp + CC64FSZ + BIAS + TF_TSTATE]
	stx	%o2, [%sp + CC64FSZ + BIAS + TF_PC]
	stx	%o3, [%sp + CC64FSZ + BIAS + TF_NPC]
	st	%o4, [%sp + CC64FSZ + BIAS + TF_Y]

	rdpr	%pil, %g5
	stb	%g5, [%sp + CC64FSZ + BIAS + TF_PIL]
	stb	%g5, [%sp + CC64FSZ + BIAS + TF_OLDPIL]

	wr	%g0, ASI_PRIMARY_NOFAULT, %asi	! Restore default ASI

	GET_CPUINFO_VA(%g7)
	call	_C_LABEL(syscall)		! syscall(&tf, code, pc)
	 wrpr	%g0, PSTATE_INTR, %pstate	! turn on interrupts

	/* see `proc_trampoline' for the reason for this label */
return_from_syscall:
	wrpr	%g0, PSTATE_KERN, %pstate	! Disable intterrupts
	wrpr	%g0, 0, %tl			! Return to tl==0
	ba,a,pt	%icc, return_from_trap
	 nop
	NOTREACHED

/*
 * interrupt_vector:
 *
 * Spitfire chips never get level interrupts directly from H/W.
 * Instead, all interrupts come in as interrupt_vector traps.
 * The interrupt number or handler address is an 11 bit number
 * encoded in the first interrupt data word.  Additional words
 * are application specific and used primarily for cross-calls.
 *
 * The interrupt vector handler then needs to identify the
 * interrupt source from the interrupt number and arrange to
 * invoke the interrupt handler.  This can either be done directly
 * from here, or a softint at a particular level can be issued.
 *
 * To call an interrupt directly and not overflow the trap stack,
 * the trap registers should be saved on the stack, registers
 * cleaned, trap-level decremented, the handler called, and then
 * the process must be reversed.
 *
 * To simplify life all we do here is issue an appropriate softint.
 *
 * Note:	It is impossible to identify or change a device's
 *		interrupt number until it is probed.  That's the
 *		purpose for all the funny interrupt acknowledge
 *		code.
 *
 */

/*
 * Vectored interrupts:
 *
 * When an interrupt comes in, interrupt_vector uses the interrupt
 * vector number to lookup the appropriate intrhand from the intrlev
 * array.  It then looks up the interrupt level from the intrhand
 * structure.  It uses the level to index the intrpending array,
 * which is 8 slots for each possible interrupt level (so we can
 * shift instead of multiply for address calculation).  It hunts for
 * any available slot at that level.  Available slots are NULL.
 *
 * NOTE: If no slots are available, we issue an un-vectored interrupt,
 * but it will probably be lost anyway.
 *
 * Then interrupt_vector uses the interrupt level in the intrhand
 * to issue a softint of the appropriate level.  The softint handler
 * figures out what level interrupt it's handling and pulls the first
 * intrhand pointer out of the intrpending array for that interrupt
 * level, puts a NULL in its place, clears the interrupt generator,
 * and invokes the interrupt handler.
 */

#ifdef DEBUG
#define INTRDEBUG_VECTOR	0x1
#define INTRDEBUG_LEVEL		0x2
#define INTRDEBUG_FUNC		0x4
#define INTRDEBUG_SPUR		0x8
	.globl	_C_LABEL(intrdebug)
_C_LABEL(intrdebug):	.word 0x0
/*
 * Note: we use the local label `97' to branch forward to, to skip
 * actual debugging code following a `intrdebug' bit test.
 */
#endif	/* DEBUG */
	.text
interrupt_vector:
	ldxa	[%g0] ASI_IRSR, %g1
	mov	IRDR_0H, %g2
	ldxa	[%g2] ASI_IRDR, %g2	! Get interrupt number
	membar	#Sync

	sethi	%hi(KERNBASE), %g3
	btst	IRSR_BUSY, %g1
	bz,pn	%icc, 3f		! Spurious interrupt
	 cmp	%g2, %g3
#ifdef MULTIPROCESSOR
	blu,pt	%xcc, Lsoftint_regular
	 and	%g2, MAXINTNUM-1, %g5	! XXX make sun4us work
	mov	IRDR_1H, %g3
	ldxa	[%g3] ASI_IRDR, %g3     ! Get IPI handler arg0
	mov	IRDR_2H, %g5
	ldxa	[%g5] ASI_IRDR, %g5     ! Get IPI handler arg1

	stxa	%g0, [%g0] ASI_IRSR	! Ack IRQ
	membar	#Sync			! Should not be needed due to retry

	jmpl	%g2, %g0
	 nop
	Debugger()
	NOTREACHED
#else
	bgeu,pn	%xcc, 3f
	 and	%g2, MAXINTNUM-1, %g5	! XXX make sun4us work
#endif

Lsoftint_regular:
	stxa	%g0, [%g0] ASI_IRSR	! Ack IRQ
	membar	#Sync			! Should not be needed due to retry

	sethi	%hi(_C_LABEL(intrlev)), %g3
	or	%g3, %lo(_C_LABEL(intrlev)), %g3
	sllx	%g5, 3, %g5		! Calculate entry number
	ldx	[%g3 + %g5], %g5	! We have a pointer to the handler
#ifdef DEBUG
	brnz,pt %g5, 1f
	 nop
	STACKFRAME -CC64FSZ		! Get a clean register window
	mov	%g2, %o1

	LOAD_ASCIZ(%o0, "interrupt_vector: vector %lx NULL\r\n")
	GLOBTOLOC
	call	prom_printf
	 clr	%g4
	LOCTOGLOB
	restore
	 nop
1:	
#endif	/* DEBUG */
	
	brz,pn	%g5, 3f			! NULL means it isn't registered yet.  Skip it.
	 nop

setup_sparcintr:
	ldx	[%g5+IH_PEND], %g6	! Check if already in use
	brnz,pn	%g6, ret_from_intr_vector ! Skip it if it's running
	 ldub	[%g5+IH_PIL], %g6	! Read interrupt mask
	GET_CPUINFO_VA(%g1)
	sll	%g6, 3+3, %g3	! Find start of table for this IPL
	add	%g1, CI_INTRPENDING, %g1
	add	%g1, %g3, %g1
1:
	ldx	[%g1], %g3		! Load list head
	add	%g5, IH_PEND, %g7
	casxa	[%g7] ASI_N, %g0, %g3
	brnz,pn	%g3, ret_from_intr_vector
	 nop
	stx	%g5, [%g1]

#ifdef DEBUG
	set	_C_LABEL(intrdebug), %g7
	ld	[%g7], %g7
	btst	INTRDEBUG_VECTOR, %g7
	bz,pt	%icc, 97f
	 nop

	STACKFRAME -CC64FSZ		! Get a clean register window
	LOAD_ASCIZ(%o0,\
	    "interrupt_vector: number %lx softint mask %lx pil %lu slot %p\r\n")
	mov	%g2, %o1
	rdpr	%pil, %o3
	mov	%g1, %o4
	GLOBTOLOC
	clr	%g4
	call	prom_printf
	 mov	%g6, %o2
	LOCTOGLOB
	restore
97:
#endif	/* DEBUG */	/* DEBUG */
	mov	1, %g7
	sll	%g7, %g6, %g6
	wr	%g6, 0, SET_SOFTINT	! Invoke a softint

ret_from_intr_vector:
	CLRTT
	retry
	NOTREACHED

3:
#ifdef DEBUG
	set	_C_LABEL(intrdebug), %g7
	ld	[%g7], %g7
	btst	INTRDEBUG_SPUR, %g7
	bz,pt	%icc, 97f
	 nop
#endif	/* DEBUG */
	STACKFRAME -CC64FSZ		! Get a clean register window
	LOAD_ASCIZ(%o0, "interrupt_vector: spurious vector %lx at pil %d\r\n")
	mov	%g2, %o1
	GLOBTOLOC
	clr	%g4
	call	prom_printf
	 rdpr	%pil, %o2
	LOCTOGLOB
	restore
97:
	ba,a	ret_from_intr_vector
	 nop				! XXX spitfire bug?

#ifdef SUN4V

sun4v_cpu_mondo:
	mov	0x3c0, %g1
	ldxa	[%g1] ASI_QUEUE, %g2

	GET_CPUINFO_PA(%g3)
	add	%g3, CI_CPUMQ, %g3
	ldxa	[%g3] ASI_PHYS_CACHED, %g3
	ldxa	[%g3 + %g2] ASI_PHYS_CACHED, %g4
	add	%g2, 8, %g5
	ldxa	[%g3 + %g5] ASI_PHYS_CACHED, %g5
	add	%g2, 16, %g6
	ldxa	[%g3 + %g6] ASI_PHYS_CACHED, %g6
	add	%g2, 64, %g2
	and	%g2, 0x7ff, %g2
	stxa	%g2, [%g1] ASI_QUEUE
	membar	#Sync

	mov	%g4, %g2
	mov	%g5, %g3
	mov	%g6, %g5
	jmpl	%g2, %g0
	 nop			! No store here!
	retry
	NOTREACHED

sun4v_dev_mondo:
	mov	0x3d0, %g1
	ldxa	[%g1] ASI_QUEUE, %g2

	GET_CPUINFO_PA(%g3)
	add	%g3, CI_DEVMQ, %g3
	ldxa	[%g3] ASI_PHYS_CACHED, %g3
	ldxa	[%g3 + %g2] ASI_PHYS_CACHED, %g4
	add	%g2, 64, %g2
	and	%g2, 0x7ff, %g2
	stxa	%g2, [%g1] ASI_QUEUE
	membar	#Sync

	and	%g4, 0x7ff, %g4
	sllx	%g4, 3, %g5

	sethi	%hi(_C_LABEL(intrlev)), %g3
	or	%g3, %lo(_C_LABEL(intrlev)), %g3
	ldx	[%g3 + %g5], %g5	! We have a pointer to the handler

	brnz,pt	%g5, setup_sparcintr
	 nop

	ba,a	3b
	 nop

#endif

#ifdef MULTIPROCESSOR
NENTRY(sun4u_ipi_tlb_page_demap)
	rdpr	%pstate, %g1
	andn	%g1, PSTATE_IE, %g2
	wrpr	%g2, %pstate				! disable interrupts

	rdpr	%tl, %g2
	brnz	%g2, 1f
	 add	%g2, 1, %g4
	wrpr	%g0, %g4, %tl				! Switch to traplevel > 0
1:
	mov	CTX_PRIMARY, %g4
	andn	%g3, 0xfff, %g3				! drop unused va bits
	ldxa	[%g4] ASI_DMMU, %g6			! Save primary context
	sethi	%hi(KERNBASE), %g7
	membar	#LoadStore
	stxa	%g5, [%g4] ASI_DMMU			! Insert context to demap
	membar	#Sync
	or	%g3, DEMAP_PAGE_PRIMARY, %g3
	stxa	%g0, [%g3] ASI_DMMU_DEMAP
	stxa	%g0, [%g3] ASI_IMMU_DEMAP
	membar	#Sync
	flush	%g7
	stxa	%g6, [%g4] ASI_DMMU
	membar	#Sync
	flush	%g7

	wrpr	%g2, %tl
	wrpr	%g1, %pstate
	ba,a	ret_from_intr_vector
	 nop

NENTRY(sun4u_ipi_tlb_context_demap)
	rdpr	%pstate, %g1
	andn	%g1, PSTATE_IE, %g2
	wrpr	%g2, %pstate				! disable interrupts

	rdpr	%tl, %g2
	brnz	%g2, 1f
	 add	%g2, 1, %g4
	wrpr	%g0, %g4, %tl				! Switch to traplevel > 0
1:
	mov	CTX_PRIMARY, %g4
	sethi	%hi(KERNBASE), %g7
	ldxa	[%g4] ASI_DMMU, %g6			! Save primary context
	membar	#LoadStore
	stxa	%g3, [%g4] ASI_DMMU			! Insert context to demap
	membar	#Sync
	set	DEMAP_CTX_PRIMARY, %g3
	stxa	%g0, [%g3] ASI_DMMU_DEMAP
	stxa	%g0, [%g3] ASI_IMMU_DEMAP
	membar	#Sync
	flush	%g7
	stxa	%g6, [%g4] ASI_DMMU
	membar	#Sync
	flush	%g7

	wrpr	%g2, %tl
	wrpr	%g1, %pstate
	ba,a	ret_from_intr_vector
	 nop

#ifdef SUN4V
NENTRY(sun4v_ipi_tlb_page_demap)
	mov	%o0, %g1
	mov	%o1, %g2
	mov	%o2, %g4
	mov	%g3, %o0
	mov	%g5, %o1
	mov	MAP_DTLB|MAP_ITLB, %o2
	ta	MMU_UNMAP_ADDR
	mov	%g1, %o0
	mov	%g2, %o1
	mov	%g4, %o2

	retry

NENTRY(sun4v_ipi_tlb_context_demap)
	NOTREACHED
#endif

NENTRY(ipi_save_fpstate)
	GET_CPUINFO_VA(%g1)
	ldx	[%g1 + CI_FPPROC], %g2
	cmp	%g2, %g3
	bne,pn	%xcc, 3f

	 mov	CTX_SECONDARY, %g2
	GET_MMU_CONTEXTID(%g6, %g2)
	membar	#LoadStore
	SET_MMU_CONTEXTID(%g0, %g2)
	membar	#Sync

	ldx	[%g3 + P_FPSTATE], %g3

	rdpr	%pstate, %g2		! enable FP before we begin
	rd	%fprs, %g4
	wr	%g0, FPRS_FEF, %fprs
	or	%g2, PSTATE_PEF, %g2
	wrpr	%g2, 0, %pstate

	stx	%fsr, [%g3 + FS_FSR]	! f->fs_fsr = getfsr();

	rd	%gsr, %g2		! Save %gsr
	st	%g2, [%g3 + FS_GSR]

	add	%g3, FS_REGS, %g3	! This is zero...
	btst	FPRS_DL, %g4		! Lower FPU clean?
	bz,a,pt	%icc, 1f		! Then skip it
	 add	%g3, 128, %g3		! Skip a block

	membar	#Sync
	stda	%f0, [%g3] ASI_BLK_S	! f->fs_f0 = etc;
	inc	BLOCK_SIZE, %g3
	stda	%f16, [%g3] ASI_BLK_S
	inc	BLOCK_SIZE, %g3
1:
	btst	FPRS_DU, %g4		! Upper FPU clean?
	bz,pt	%icc, 2f		! Then skip it
	 nop

	membar	#Sync
	stda	%f32, [%g3] ASI_BLK_S
	inc	BLOCK_SIZE, %g3
	stda	%f48, [%g3] ASI_BLK_S
2:
	membar	#Sync			! Finish operation so we can
	wr	%g0, FPRS_FEF, %fprs	! Mark FPU clean

	stx	%g0, [%g1 + CI_FPPROC]	! fpproc = NULL
	mov	CTX_SECONDARY, %g2
	SET_MMU_CONTEXTID(%g6, %g2)
	membar	#Sync
3:
	ba	ret_from_intr_vector
	 nop

NENTRY(ipi_drop_fpstate)
	rdpr	%pstate, %g1
	wr	%g0, FPRS_FEF, %fprs
	or	%g1, PSTATE_PEF, %g1
	wrpr	%g1, 0, %pstate
	GET_CPUINFO_VA(%g1)
	ldx	[%g1 + CI_FPPROC], %g5
	cmp	%g5, %g3
	bne,pn	%xcc, 1f
	 nop
	stx	%g0, [%g1 + CI_FPPROC]		! fpproc = NULL
1:
	ba	ret_from_intr_vector
	 nop

NENTRY(ipi_softint)
	ba	ret_from_intr_vector
	 wr	%g3, 0, SET_SOFTINT
#endif

/*
 * Ultra1 and Ultra2 CPUs use soft interrupts for everything.  What we do
 * on a soft interrupt, is we should check which bits in ASR_SOFTINT(0x16)
 * are set, handle those interrupts, then clear them by setting the
 * appropriate bits in ASR_CLEAR_SOFTINT(0x15).
 *
 * We have an array of 8 interrupt vector slots for each of 15 interrupt
 * levels.  If a vectored interrupt can be dispatched, the dispatch
 * routine will place a pointer to an intrhand structure in one of
 * the slots.  The interrupt handler will go through the list to look
 * for an interrupt to dispatch.  If it finds one it will pull it off
 * the list, free the entry, and call the handler.  The code is like
 * this:
 *
 *	for (i=0; i<8; i++)
 *		if (ih = intrpending[intlev][i]) {
 *			intrpending[intlev][i] = NULL;
 *			if ((*ih->ih_fun)(ih->ih_arg ? ih->ih_arg : &frame))
 *				return;
 *			strayintr(&frame);
 *			return;
 *		}
 *
 * Otherwise we go back to the old style of polled interrupts.
 *
 * After preliminary setup work, the interrupt is passed to each
 * registered handler in turn.  These are expected to return nonzero if
 * they took care of the interrupt.  If a handler claims the interrupt,
 * we exit (hardware interrupts are latched in the requestor so we'll
 * just take another interrupt in the unlikely event of simultaneous
 * interrupts from two different devices at the same level).  If we go
 * through all the registered handlers and no one claims it, we report a
 * stray interrupt.  This is more or less done as:
 *
 *	for (ih = intrhand[intlev]; ih; ih = ih->ih_next)
 *		if ((*ih->ih_fun)(ih->ih_arg ? ih->ih_arg : &frame))
 *			return;
 *	strayintr(&frame);
 *
 * Inputs:
 *	%l0 = %tstate
 *	%l1 = return pc
 *	%l2 = return npc
 *	%l3 = interrupt level
 *	(software interrupt only) %l4 = bits to clear in interrupt register
 *
 * Internal:
 *	%l4, %l5: local variables
 *	%l6 = %y
 *	%l7 = %g1
 *	%g2..%g7 go to stack
 *
 * An interrupt frame is built in the space for a full trapframe;
 * this contains the psr, pc, npc, and interrupt level.
 *
 * The level of this interrupt is determined by:
 *
 *       IRQ# = %tt - 0x40
 */

	.globl _C_LABEL(sparc_interrupt)	! This is for interrupt debugging
_C_LABEL(sparc_interrupt):
	/*
	 * If this is a %tick softint, clear it then call interrupt_vector.
	 */
	rd	SOFTINT, %g1
	set	(TICK_INT|STICK_INT), %g2
	andcc	%g2, %g1, %g2
	bz,pt	%icc, 0f
	 GET_CPUINFO_VA(%g7)
	wr	%g2, 0, CLEAR_SOFTINT
	ba,pt	%icc, setup_sparcintr
	 add	%g7, CI_TICKINTR, %g5
0:
	INTR_SETUP -CC64FSZ-TF_SIZE-8

	NORMAL_GLOBALS()

	/* Save normal globals */
	stx	%g1, [%sp + CC64FSZ + BIAS + TF_G + ( 1*8)]
	stx	%g2, [%sp + CC64FSZ + BIAS + TF_G + ( 2*8)]
	stx	%g3, [%sp + CC64FSZ + BIAS + TF_G + ( 3*8)]
	stx	%g4, [%sp + CC64FSZ + BIAS + TF_G + ( 4*8)]
	stx	%g5, [%sp + CC64FSZ + BIAS + TF_G + ( 5*8)]
	stx	%g6, [%sp + CC64FSZ + BIAS + TF_G + ( 6*8)]
	stx	%g7, [%sp + CC64FSZ + BIAS + TF_G + ( 7*8)]

999:	flushw			! Do not remove this instruction -- causes interrupt loss
	.section	.sun4v_patch, "ax"
	.word	999b
	nop
	.previous

	GET_CPUINFO_VA(%g7)
#ifdef SUN4V
	ldx	[%g7 + CI_RWSP], %g2
	brz,pt	%g2, 1f
	 nop

	ldx	[%g7 + CI_RW + (0*8)], %l0
	ldx	[%g7 + CI_RW + (1*8)], %l1
	ldx	[%g7 + CI_RW + (2*8)], %l2
	ldx	[%g7 + CI_RW + (3*8)], %l3
	ldx	[%g7 + CI_RW + (4*8)], %l4
	ldx	[%g7 + CI_RW + (5*8)], %l5
	ldx	[%g7 + CI_RW + (6*8)], %l6
	ldx	[%g7 + CI_RW + (7*8)], %l7
	stx	%l0, [%g2 + BIAS + (0*8)]
	stx	%l1, [%g2 + BIAS + (1*8)]
	stx	%l2, [%g2 + BIAS + (2*8)]
	stx	%l3, [%g2 + BIAS + (3*8)]
	stx	%l4, [%g2 + BIAS + (4*8)]
	stx	%l5, [%g2 + BIAS + (5*8)]
	stx	%l6, [%g2 + BIAS + (6*8)]
	stx	%l7, [%g2 + BIAS + (7*8)]
	ldx	[%g7 + CI_RW + (8*8)], %l0
	ldx	[%g7 + CI_RW + (9*8)], %l1
	ldx	[%g7 + CI_RW + (10*8)], %l2
	ldx	[%g7 + CI_RW + (11*8)], %l3
	ldx	[%g7 + CI_RW + (12*8)], %l4
	ldx	[%g7 + CI_RW + (13*8)], %l5
	ldx	[%g7 + CI_RW + (14*8)], %l6
	ldx	[%g7 + CI_RW + (15*8)], %l7
	stx	%l0, [%g2 + BIAS + (8*8)]
	stx	%l1, [%g2 + BIAS + (9*8)]
	stx	%l2, [%g2 + BIAS + (10*8)]
	stx	%l3, [%g2 + BIAS + (11*8)]
	stx	%l4, [%g2 + BIAS + (12*8)]
	stx	%l5, [%g2 + BIAS + (13*8)]
	stx	%l6, [%g2 + BIAS + (14*8)]
	stx	%l7, [%g2 + BIAS + (15*8)]

	stx	%g0, [%g7 + CI_RWSP]
1:
#endif

	rd	%y, %l6
	INCR _C_LABEL(uvmexp)+V_INTR	! cnt.v_intr++; (clobbers %o0,%o1,%o2)
	rdpr	%tt, %l5		! Find out our current IPL
	rdpr	%tstate, %l0
	rdpr	%tpc, %l1
	rdpr	%tnpc, %l2
	wrpr	%g0, 0, %tl

	! Dump our trap frame now we have taken the IRQ
	stw	%l6, [%sp + CC64FSZ + BIAS + TF_Y]	! Silly, but we need to save this for rft
	sth	%l5, [%sp + CC64FSZ + BIAS + TF_TT]! debug
	stx	%l0, [%sp + CC64FSZ + BIAS + TF_TSTATE]	! set up intrframe/clockframe
	stx	%l1, [%sp + CC64FSZ + BIAS + TF_PC]
	btst	TSTATE_PRIV, %l0		! User mode?
	stx	%l2, [%sp + CC64FSZ + BIAS + TF_NPC]
	stx	%fp, [%sp + CC64FSZ + BIAS + TF_KSTACK]	!  old frame pointer
	
	sub	%l5, 0x40, %l6			! Convert to interrupt level
	stb	%l6, [%sp + CC64FSZ + BIAS + TF_PIL]	! set up intrframe/clockframe
	rdpr	%pil, %o1
	stb	%o1, [%sp + CC64FSZ + BIAS + TF_OLDPIL]	! old %pil
	clr	%l5			! Zero handled count
	mov	1, %l3			! Ack softint
	sll	%l3, %l6, %l3		! Generate IRQ mask

	wrpr	%l6, %pil

	/*
	 * Set handled_intr_level and save the old one so we can restore it
	 * later.
	 */
	ld	[%g7 + CI_HANDLED_INTR_LEVEL], %l7
	st	%l6, [%g7 + CI_HANDLED_INTR_LEVEL]
	st	%l7, [%sp + CC64FSZ + BIAS + TF_SIZE]

sparc_intr_retry:
	wr	%l3, 0, CLEAR_SOFTINT	! (don't clear possible %tick IRQ)
	wrpr	%g0, PSTATE_INTR, %pstate	! Reenable interrupts
	sll	%l6, 3+3, %l2
	add	%g7, CI_INTRPENDING, %l4
	mov	8, %l7
	add	%l2, %l4, %l4

1:
	membar	#StoreLoad		! Make sure any failed casxa instructions complete
	ldx	[%l4], %l2		! Check a slot
	brz,pn	%l2, intrcmplt		! Empty list?

	 clr	%l7
	membar	#LoadStore
	casxa	[%l4] ASI_N, %l2, %l7	! Grab the entire list
	cmp	%l7, %l2
	bne,pn	%icc, 1b
	 nop

2:
	ldx	[%l2 + IH_PEND], %l7	! Load next pending
	add	%l2, IH_PEND, %l3
	clr	%l4
	casxa	[%l3] ASI_N, %l7, %l4	! Unlink from list
	cmp	%l7, %l4
	bne,pn	%xcc, 2b		! Retry?
	 add	%sp, CC64FSZ+BIAS, %o0	! tf = %sp + CC64FSZ + BIAS

	ldx	[%l2 + IH_ACK], %l1	! ih->ih_ack

	! At this point, the current ih could already be added
	! back to the pending table.

	call	_C_LABEL(intr_handler)
	 mov	%l2, %o1

	brz,pn	%l1, 0f
	 add	%l5, %o0, %l5		! Add handler return value
	ldx	[%l2 + IH_COUNT], %o0	! ih->ih_count.ec_count++;
	inc	%o0
	stx	%o0, [%l2 + IH_COUNT]

	jmpl	%l1, %o7		! (*ih->ih_ack)(ih)
	 mov	%l2, %o0
0:
	brnz,pn	%l7, 2b			! 'Nother?
	 mov	%l7, %l2

intrcmplt:
	/*
	 * Re-read SOFTINT to see if any new  pending interrupts
	 * at this level.
	 */
	mov	1, %l3			! Ack softint
	rd	SOFTINT, %l7		! %l5 contains #intr handled.
	sll	%l3, %l6, %l3		! Generate IRQ mask
	btst	%l3, %l7		! leave mask in %l3 for retry code
	bnz,pn	%icc, sparc_intr_retry
	 mov	1, %l5			! initialize intr count for next run

#ifdef DEBUG
	set	_C_LABEL(intrdebug), %o2
	ld	[%o2], %o2
	btst	INTRDEBUG_FUNC, %o2
	bz,a,pt	%icc, 97f
	 nop

	STACKFRAME -CC64FSZ		! Get a clean register window
	LOAD_ASCIZ(%o0, "sparc_interrupt:  done\r\n")
	GLOBTOLOC
	call	prom_printf
	 nop
	LOCTOGLOB
	restore
97:
#endif	/* DEBUG */

	/* Restore old handled_intr_level */
	ld	[%sp + CC64FSZ + BIAS + TF_SIZE], %l7
	st	%l7, [%g7 + CI_HANDLED_INTR_LEVEL]

	ldub	[%sp + CC64FSZ + BIAS + TF_OLDPIL], %l3	! restore old %pil
	wrpr	%g0, PSTATE_KERN, %pstate	! Disable interrupts
	wrpr	%l3, 0, %pil

	CHKPT %o1,%o2,5
	ba,a,pt	%icc, return_from_trap
	 nop

	.globl	return_from_trap, rft_kernel, rft_user
	.globl	softtrap, slowtrap
	.globl	syscall

/*
 * Various return-from-trap routines (see return_from_trap).
 */

/*
 * Return from trap.
 * registers are:
 *
 *	[%sp + CC64FSZ + BIAS] => trap frame
 *
 * We must load all global, out, and trap registers from the trap frame.
 *
 * If returning to kernel, we should be at the proper trap level because
 * we don't touch %tl.
 *
 * When returning to user mode, the trap level does not matter, as it
 * will be set explicitly.
 *
 * If we are returning to user code, we must:
 *  1.  Check for register windows in the pcb that belong on the stack.
 *	If there are any, reload them
 */
return_from_trap:
#ifdef DEBUG
	!! Make sure we don't have pc == npc == 0 or we suck.
	ldx	[%sp + CC64FSZ + BIAS + TF_PC], %g2
	ldx	[%sp + CC64FSZ + BIAS + TF_NPC], %g3
	orcc	%g2, %g3, %g0
	tz	%icc, 1
#endif	/* DEBUG */
	ldx	[%sp + CC64FSZ + BIAS + TF_TSTATE], %g1
	btst	TSTATE_PRIV, %g1			! returning to userland?
	!!
	!! Let all pending interrupts drain before returning to userland
	!!
	bnz,pn	%icc, 1f				! Returning to userland?
	 nop
	wrpr	%g0, PSTATE_INTR, %pstate
	wrpr	%g0, %g0, %pil				! Lower IPL
1:
	wrpr	%g0, PSTATE_KERN, %pstate		! Disable IRQs

	/* Restore normal globals */
	ldx	[%sp + CC64FSZ + BIAS + TF_G + (1*8)], %g1
	ldx	[%sp + CC64FSZ + BIAS + TF_G + (2*8)], %g2
	ldx	[%sp + CC64FSZ + BIAS + TF_G + (3*8)], %g3
	ldx	[%sp + CC64FSZ + BIAS + TF_G + (4*8)], %g4
	ldx	[%sp + CC64FSZ + BIAS + TF_G + (5*8)], %g5
	ldx	[%sp + CC64FSZ + BIAS + TF_G + (6*8)], %g6
	bnz,pn	%icc, 2f
	 nop
	ldx	[%sp + CC64FSZ + BIAS + TF_G + (7*8)], %g7
2:
	ALTERNATE_GLOBALS()

	/* Restore outs */
	ldx	[%sp + CC64FSZ + BIAS + TF_O + (0*8)], %i0
	ldx	[%sp + CC64FSZ + BIAS + TF_O + (1*8)], %i1
	ldx	[%sp + CC64FSZ + BIAS + TF_O + (2*8)], %i2
	ldx	[%sp + CC64FSZ + BIAS + TF_O + (3*8)], %i3
	ldx	[%sp + CC64FSZ + BIAS + TF_O + (4*8)], %i4
	ldx	[%sp + CC64FSZ + BIAS + TF_O + (5*8)], %i5
	ldx	[%sp + CC64FSZ + BIAS + TF_O + (6*8)], %i6
	ldx	[%sp + CC64FSZ + BIAS + TF_O + (7*8)], %i7
	/* Now load trap registers into alternate globals */
	ld	[%sp + CC64FSZ + BIAS + TF_Y], %g4
	ldx	[%sp + CC64FSZ + BIAS + TF_TSTATE], %g1		! load new values
	wr	%g4, 0, %y
	ldx	[%sp + CC64FSZ + BIAS + TF_PC], %g2
	ldx	[%sp + CC64FSZ + BIAS + TF_NPC], %g3

	/* Returning to user mode or kernel mode? */
	btst	TSTATE_PRIV, %g1		! returning to userland?
	bz,pt	%icc, rft_user
	 nop

/*
 * Return from trap, to kernel.
 *
 * We will assume, for the moment, that all kernel traps are properly stacked
 * in the trap registers, so all we have to do is insert the (possibly modified)
 * register values into the trap registers then do a retry.
 *
 */
rft_kernel:
	rdpr	%tl, %g4			! Grab a set of trap registers
	inc	%g4
	wrpr	%g4, %g0, %tl
	wrpr	%g3, 0, %tnpc
	wrpr	%g2, 0, %tpc
	wrpr	%g1, 0, %tstate

	rdpr	%canrestore, %g2
	brnz	%g2, 1f
	 nop

	wr	%g0, ASI_NUCLEUS, %asi
	rdpr	%cwp, %g1
	dec	%g1
	wrpr	%g1, %cwp
	FILL	ldxa, %sp+BIAS, 8, %asi
	restored
	inc	%g1
	wrpr	%g1, %cwp
1:
	restore

	rdpr	%tstate, %g1			! Since we may have trapped our regs may be toast
	rdpr	%cwp, %g2
	andn	%g1, CWP, %g1
	wrpr	%g1, %g2, %tstate		! Put %cwp in %tstate
	retry					! We should allow some way to distinguish retry/done
	NOTREACHED

/*
 * Return from trap, to user.  Checks for scheduling trap (`ast') first;
 * will re-enter trap() if set.  Note that we may have to switch from
 * the interrupt stack to the kernel stack in this case.
 *	%g1 = %tstate
 *	%g2 = return %pc
 *	%g3 = return %npc
 * If returning to a valid window, just set psr and return.
 */
rft_user:
	GET_CURPROC(%g7)
	lduw	[%g7 + P_MD_ASTPENDING], %g7	! want AST trap?
	brnz,pn	%g7, softtrap			! yes, re-enter trap with type T_AST
	 mov	T_AST, %g4

	/*
	 * NB: only need to do this after a cache miss
	 */
	/*
	 * Now check to see if any regs are saved in the pcb and restore them.
	 *
	 * Here we need to undo the damage caused by switching to a kernel 
	 * stack.
	 *
	 * We will use alternate globals %g4..%g7 because %g1..%g3 are used
	 * by the data fault trap handlers and we don't want possible conflict.
	 */

	GET_CPCB(%g6)
 	ldub	[%g6 + PCB_NSAVED], %g7		! Any saved reg windows?
	brnz,pn	%g7, softtrap
	 mov	T_RWRET, %g4

	/*
	 * Set up our return trapframe so we can recover if we trap from here
	 * on in.
	 */
	wrpr	%g0, 1, %tl			! Set up the trap state
	wrpr	%g2, 0, %tpc
	wrpr	%g3, 0, %tnpc
	rdpr	%cwp, %g7
	andn	%g1, CWP, %g1
	wrpr	%g1, %g7, %tstate

	/* XXX Rewrite sun4u code to handle faults like sun4v. */
	sethi	%hi(_C_LABEL(cputyp)), %g2
	ld	[%g2 + %lo(_C_LABEL(cputyp))], %g2
	cmp	%g2, CPU_SUN4V
	bne,pt	%icc, 1f
	 nop

	rdpr	%otherwin, %g2
	brnz	%g2, 1f
	 nop

	wr	%g0, ASI_AIUS, %asi
	rdpr	%cwp, %g1
	dec	%g1
	wrpr	%g1, 0, %cwp
rft_user_fault_start:
	FILL	ldxa, %sp+BIAS, 8, %asi
	ldx	[%g6 + PCB_WCOOKIE], %g7
	xor	%g7, %i7, %i7		! stackghost
rft_user_fault_end:
	restored
	inc	%g1
	wrpr	%g1, 0, %cwp

	rdpr	%canrestore, %g7
	wrpr	%g7, 0, %otherwin
	wrpr	%g0, 0, %canrestore
1:
	rdpr	%otherwin, %g7			! restore register window controls
	wrpr	%g7, 0, %canrestore
	wrpr	%g0, 0, %otherwin
	wrpr	WSTATE_USER, %wstate		! Need to know where our sp points
	wrpr	%g7, 0, %cleanwin		! Force cleanup of kernel windows

	restore

	rdpr	%tstate, %g1
	rdpr	%cwp, %g7			! Find our cur window
	andn	%g1, CWP, %g1			! Clear it from %tstate
	wrpr	%g1, %g7, %tstate		! Set %tstate with %cwp

	mov	CTX_SECONDARY, %g1		! Restore the user context
	GET_MMU_CONTEXTID(%g4, %g1)
	mov	CTX_PRIMARY, %g2
	SET_MMU_CONTEXTID(%g4, %g2)
	sethi	%hi(KERNBASE), %g7		! Should not be needed due to retry
	membar	#Sync				! Should not be needed due to retry
	flush	%g7				! Should not be needed due to retry

#ifdef DEBUG
	GET_CPCB(%g5)
	ldub	[%g5 + PCB_NSAVED], %g5		! Any saved reg windows?
	tst	%g5
	tnz	%icc, 1; nop			! Debugger if we still have saved windows!
#endif	/* DEBUG */
	wrpr	%g0, 0, %pil			! Enable all interrupts
	retry

! exported end marker for kernel gdb
	.globl	_C_LABEL(endtrapcode)
_C_LABEL(endtrapcode):

#ifdef DDB
!!!
!!! Dump the DTLB to phys address in %o0 and print it
!!!
!!! Only toast a few %o registers
!!!
	.globl	dump_dtlb
dump_dtlb:
	clr	%o1
	add	%o1, (64*8), %o3
1:
	ldxa	[%o1] ASI_DMMU_TLB_TAG, %o2
	membar	#Sync
	stx	%o2, [%o0]
	membar	#Sync
	inc	8, %o0
	ldxa	[%o1] ASI_DMMU_TLB_DATA, %o4
	membar	#Sync
	inc	8, %o1
	stx	%o4, [%o0]
	cmp	%o1, %o3
	membar	#Sync
	bl	1b
	 inc	8, %o0

	retl
	 nop
#endif /* DDB */	/* DDB */
#if defined(DDB)
	.globl	print_dtlb
print_dtlb:
	save	%sp, -CC64FSZ, %sp
	clr	%l1
	add	%l1, (64*8), %l3
	clr	%l2
1:
	ldxa	[%l1] ASI_DMMU_TLB_TAG, %o2
	membar	#Sync
	mov	%l2, %o1
	ldxa	[%l1] ASI_DMMU_TLB_DATA, %o3
	membar	#Sync
	inc	%l2
	set	2f, %o0
	call	_C_LABEL(db_printf)
	 inc	8, %l1

	ldxa	[%l1] ASI_DMMU_TLB_TAG, %o2
	membar	#Sync
	mov	%l2, %o1
	ldxa	[%l1] ASI_DMMU_TLB_DATA, %o3
	membar	#Sync
	inc	%l2
	set	3f, %o0
	call	_C_LABEL(db_printf)
	 inc	8, %l1

	cmp	%l1, %l3
	bl	1b
	 inc	8, %l0

	ret
	 restore
	.data
2:
	.asciz	"%2d:%016lx %016lx "
3:
	.asciz	"%2d:%016lx %016lx\r\n"
	.text
#endif	/* defined(DDB) */

	.align	8
dostart:
	mov	1, %g1
	sllx	%g1, 63, %g1
	wr	%g1, TICK_CMPR	! Clear and disable %tick_cmpr
	/*
	 * Startup.
	 *
	 * The Sun FCODE bootloader is nice and loads us where we want
	 * to be.  We have a full set of mappings already set up for us.
	 *
	 * I think we end up having an entire 16M allocated to us.
	 *
	 * We enter with the prom entry vector in %o0, dvec in %o1,
	 * and the bootops vector in %o2.
	 *
	 * All we need to do is:
	 *
	 *	1:	Save the prom vector
	 *
	 *	2:	Create a decent stack for ourselves
	 *
	 *	3:	Install the permanent 4MB kernel mapping
	 *
	 *	4:	Call the C language initialization code
	 *
	 */

	/*
	 * Set the psr into a known state:
	 * Set supervisor mode, interrupt level >= 13, traps enabled
	 */
	wrpr	%g0, 13, %pil
	wrpr	%g0, PSTATE_INTR|PSTATE_PEF, %pstate
	wr	%g0, FPRS_FEF, %fprs		! Turn on FPU

#if defined(DDB) || NKSYMS > 0
	/*
	 * First, check for DDB arguments.  A pointer to an argument
	 * is passed in %o1 who's length is passed in %o2.  Our
	 * bootloader passes in a magic number as the first argument,
	 * followed by esym as argument 2, and ssym as argument 3,
	 * so check that %o3 >= 12.
	 */
	cmp	%o2, 12
	blt	1f			! Not enough args
	 nop
	
	set	0x44444230, %l3
	ldx	[%o1], %l4
	cmp	%l3, %l4		! chk magic
	bne	%xcc, 1f
	 nop

	ldx	[%o1+8], %l4
	sethi	%hi(_C_LABEL(esym)), %l3	! store esym
	stx	%l4, [%l3 + %lo(_C_LABEL(esym))]

	ldx	[%o1+16], %l4
	sethi	%hi(_C_LABEL(ssym)), %l3	! store ssym
	stx	%l4, [%l3 + %lo(_C_LABEL(ssym))]
1:
#endif	/* defined(DDB) || NKSYMS > 0 */
	/*
	 * Step 1: Save rom entry pointer
	 */

	mov	%o4, %g7	! save prom vector pointer
	set	romp, %o5
	stx	%o4, [%o5]	! It's initialized data, I hope

#if 0
	/*
	 * Disable the DCACHE entirely for debug.
	 */
	ldxa	[%g0] ASI_MCCR, %o1
	andn	%o1, MCCR_DCACHE_EN, %o1
	stxa	%o1, [%g0] ASI_MCCR
	membar	#Sync
#endif	/* 0 */

	/*
	 * Switch to temporary stack.
	 */
	set	tmpstack-CC64FSZ-BIAS, %sp

	/*
	 * Ready to run C code; finish bootstrap.
	 */
1:
	set	0x2000, %o0			! fixed: 8192 contexts
	call	_C_LABEL(bootstrap)
	 clr	%g4				! Clear data segment pointer

	/*
	 * pmap_bootstrap should have allocated a stack for proc 0 and
	 * stored the start and end in u0 and estack0.  Switch to that
	 * stack now.
	 */

	sethi	%hi(_C_LABEL(cpus)), %g2
	ldx	[%g2 + %lo(_C_LABEL(cpus))], %g2
	ldx	[%g2 + CI_PADDR], %g2		! Load the interrupt stack's PA

/*
 * Initialize a CPU.  This is used both for bootstrapping the first CPU
 * and spinning up each subsequent CPU.  Basically:
 *
 *	Install trap table.
 *	Switch to the initial stack.
 *	Call the routine passed in in cpu_info->ci_spinup.
 */

_C_LABEL(cpu_initialize):

	wrpr	%g0, 0, %tl			! Make sure we're not in NUCLEUS mode
	flushw

	/* Change the trap base register */
	set	_C_LABEL(trapbase), %l1
#ifdef SUN4V
	sethi	%hi(_C_LABEL(cputyp)), %l0
	ld	[%l0 + %lo(_C_LABEL(cputyp))], %l0
	cmp	%l0, CPU_SUN4V
	bne,pt	%icc, 1f
	 nop
	set	_C_LABEL(trapbase_sun4v), %l1
	GET_MMFSA(%o1)
1:
#endif
	call	_C_LABEL(prom_set_trap_table)	! Now we should be running 100% from our handlers
	 mov	%l1, %o0
	wrpr	%l1, 0, %tba			! Make sure the PROM didn't foul up.
	wrpr	%g0, WSTATE_KERN, %wstate

	/*
	 * Switch to our initial stack.
	 */

	GET_CPUINFO_VA(%g7)
	ldx	[%g7 + CI_INITSTACK], %l0
	add	%l0, -BIAS-CC64FSZ, %sp

	/*
	 * Call our startup routine.
	 */

	ldx	[%g7 + CI_SPINUP], %o1
	call	%o1				! Call routine
	 nop
	NOTREACHED

	set	1f, %o0				! Main should never come back here
	call	_C_LABEL(panic)
	 nop
	.data
1:
	.asciz	"main() returned\r\n"
	_ALIGN
	.text

ENTRY(sun4u_set_tsbs)

	/* Set the dmmu tsb */
	sethi	%hi(0x1fff), %o2
	set	_C_LABEL(tsb_dmmu), %o0
	ldx	[%o0], %o0
	set	_C_LABEL(tsbsize), %o1
	or	%o2, %lo(0x1fff), %o2
	ld	[%o1], %o1
	andn	%o0, %o2, %o0			! Mask off size and split bits
	or	%o0, %o1, %o0			! Make a TSB pointer
	set	TSB, %o2
	stxa	%o0, [%o2] ASI_DMMU		! Install data TSB pointer
	membar	#Sync

	/* Set the immu tsb */
	sethi	%hi(0x1fff), %o2
	set	_C_LABEL(tsb_immu), %o0
	ldx	[%o0], %o0
	set	_C_LABEL(tsbsize), %o1
	or	%o2, %lo(0x1fff), %o2
	ld	[%o1], %o1
	andn	%o0, %o2, %o0			! Mask off size and split bits
	or	%o0, %o1, %o0			! Make a TSB pointer
	set	TSB, %o2
	stxa	%o0, [%o2] ASI_IMMU		! Install insn TSB pointer
	membar	#Sync

	retl
	 nop


#ifdef MULTIPROCESSOR
ENTRY(cpu_mp_startup)
	mov	%o0, %g2

	wrpr	%g0, 13, %pil
	wrpr	%g0, PSTATE_INTR|PSTATE_PEF, %pstate
	wr	%g0, FPRS_FEF, %fprs		! Turn on FPU

	set	tmpstack-CC64FSZ-BIAS, %sp

	call	_C_LABEL(pmap_bootstrap_cpu)
	 nop

	ba,a,pt	%xcc, cpu_initialize
	 nop
#endif

/*
 * openfirmware(cell* param);
 *
 * OpenFirmware entry point
 */
	.align 8
	.globl	_C_LABEL(openfirmware)
	.proc 1
	FTYPE(openfirmware)
_C_LABEL(openfirmware):
	sethi	%hi(romp), %o4
	ldx	[%o4+%lo(romp)], %o4
	save	%sp, -CC64FSZ, %sp
	rdpr	%pil, %i2
	mov	PIL_HIGH, %i3
	cmp	%i3, %i2
	movle	%icc, %i2, %i3
	wrpr	%g0, %i3, %pil
	mov	%i0, %o0
	mov	%g1, %l1
	mov	%g2, %l2
	mov	%g3, %l3
	mov	%g4, %l4
	mov	%g5, %l5
	mov	%g6, %l6
	mov	%g7, %l7
	rdpr	%pstate, %l0
	jmpl	%i4, %o7
	 wrpr	%g0, PSTATE_PROM|PSTATE_IE, %pstate
	wrpr	%l0, %g0, %pstate
	mov	%l1, %g1
	mov	%l2, %g2
	mov	%l3, %g3
	mov	%l4, %g4
	mov	%l5, %g5
	mov	%l6, %g6
	mov	%l7, %g7
	wrpr	%i2, 0, %pil
	ret
	 restore	%o0, %g0, %o0

/*
 * tlb_flush_pte(vaddr_t va, int ctx)
 *
 * Flush tte from both IMMU and DMMU.
 *
 */
	.align 8
	.globl	_C_LABEL(sp_tlb_flush_pte)
	.proc 1
	FTYPE(sp_tlb_flush_pte)
_C_LABEL(sp_tlb_flush_pte):
#ifdef DEBUG
	set	DATA_START, %o4				! Forget any recent TLB misses
	stx	%g0, [%o4]
	stx	%g0, [%o4+16]
#endif	/* DEBUG */
#ifdef DEBUG
	set	pmapdebug, %o3
	lduw	[%o3], %o3
!	movrz	%o1, -1, %o3				! Print on either pmapdebug & PDB_DEMAP or ctx == 0
	btst	0x0020, %o3
	bz,pt	%icc, 2f
	 nop
	save	%sp, -CC64FSZ, %sp
	set	1f, %o0
	mov	%i1, %o1
	andn	%i0, 0xfff, %o3
	or	%o3, 0x010, %o3
	call	_C_LABEL(printf)
	 mov	%i0, %o2
	restore
	.data
1:
	.asciz	"tlb_flush_pte:	demap ctx=%x va=%08x res=%x\r\n"
	_ALIGN
	.text
2:
#endif	/* DEBUG */
#ifdef HORRID_III_HACK
	rdpr	%pstate, %o5
	andn	%o5, PSTATE_IE, %o4
	wrpr	%o4, %pstate				! disable interrupts

	rdpr	%tl, %o3
	brnz	%o3, 1f
	 add	%o3, 1, %g2
	wrpr	%g0, %g2, %tl				! Switch to traplevel > 0
1:	
	mov	CTX_PRIMARY, %o2
	andn	%o0, 0xfff, %g2				! drop unused va bits
	ldxa	[%o2] ASI_DMMU, %g1			! Save primary context
	sethi	%hi(KERNBASE), %o4
	membar	#LoadStore
	stxa	%o1, [%o2] ASI_DMMU			! Insert context to demap
	membar	#Sync
	or	%g2, DEMAP_PAGE_PRIMARY, %g2		! Demap page from primary context only
#else
	mov	CTX_SECONDARY, %o2
	andn	%o0, 0xfff, %g2				! drop unused va bits
	ldxa	[%o2] ASI_DMMU, %g1			! Save secondary context
	sethi	%hi(KERNBASE), %o4
	membar	#LoadStore
	stxa	%o1, [%o2] ASI_DMMU			! Insert context to demap
	membar	#Sync
	or	%g2, DEMAP_PAGE_SECONDARY, %g2		! Demap page from secondary context only
#endif
	stxa	%g2, [%g2] ASI_DMMU_DEMAP		! Do the demap
	membar	#Sync
	stxa	%g2, [%g2] ASI_IMMU_DEMAP		! to both TLBs
	membar	#Sync					! No real reason for this XXXX
	flush	%o4
	stxa	%g1, [%o2] ASI_DMMU			! Restore asi
	membar	#Sync					! No real reason for this XXXX
	flush	%o4
#ifdef HORRID_III_HACK
	wrpr	%g0, %o3, %tl				! Restore traplevel
	wrpr	%o5, %pstate				! Restore interrupts
#endif
	retl
	 nop

/*
 * tlb_flush_ctx(int ctx)
 *
 * Flush entire context from both IMMU and DMMU.
 *
 */
	.align 8
	.globl	_C_LABEL(sp_tlb_flush_ctx)
	.proc 1
	FTYPE(sp_tlb_flush_ctx)
_C_LABEL(sp_tlb_flush_ctx):
#ifdef DEBUG
	set	DATA_START, %o4				! Forget any recent TLB misses
	stx	%g0, [%o4]
#endif	/* DEBUG */
#ifdef DIAGNOSTIC
	brnz,pt	%o0, 2f
	 nop
	set	1f, %o0
	call	panic
	 nop
	.data
1:
	.asciz	"tlb_flush_ctx:	attempted demap of NUCLEUS context\r\n"
	_ALIGN
	.text
2:
#endif	/* DIAGNOSTIC */
#ifdef HORRID_III_HACK
	rdpr	%pstate, %o5
	andn	%o5, PSTATE_IE, %o4
	wrpr	%o4, %pstate				! disable interrupts

	rdpr	%tl, %o3
	brnz	%o3, 1f
	 add	%o3, 1, %g2
	wrpr	%g0, %g2, %tl				! Switch to traplevel > 0
1:	
	mov	CTX_PRIMARY, %o2
	sethi	%hi(KERNBASE), %o4
	ldxa	[%o2] ASI_DMMU, %g1		! Save primary context
	membar	#LoadStore
	stxa	%o0, [%o2] ASI_DMMU		! Insert context to demap
	membar	#Sync
	set	DEMAP_CTX_PRIMARY, %g2		! Demap context from primary context only
#else
	mov	CTX_SECONDARY, %o2
	sethi	%hi(KERNBASE), %o4
	ldxa	[%o2] ASI_DMMU, %g1		! Save secondary context
	membar	#LoadStore
	stxa	%o0, [%o2] ASI_DMMU		! Insert context to demap
	membar	#Sync
	set	DEMAP_CTX_SECONDARY, %g2	! Demap context from secondary context only
#endif
	stxa	%g2, [%g2] ASI_DMMU_DEMAP		! Do the demap
	membar	#Sync					! No real reason for this XXXX
	stxa	%g2, [%g2] ASI_IMMU_DEMAP		! Do the demap
	membar	#Sync
	stxa	%g1, [%o2] ASI_DMMU		! Restore secondary asi
	membar	#Sync					! No real reason for this XXXX
	flush	%o4
#ifdef HORRID_III_HACK
	wrpr	%g0, %o3, %tl				! Restore traplevel
	wrpr	%o5, %pstate				! Restore interrupts
#endif
	retl
	 nop

/*
 * dcache_flush_page(paddr_t pa)
 *
 * Clear one page from D$.
 *
 */
	.align 8
	.globl	_C_LABEL(us_dcache_flush_page)
	.proc 1
	FTYPE(us_dcache_flush_page)
_C_LABEL(us_dcache_flush_page):

	!! Try using cache_flush_phys for a change.

	mov	-1, %o1		! Generate mask for tag: bits [29..2]
	srlx	%o0, 13-2, %o2	! Tag is VA bits <40:13> in bits <29:2>
	clr	%o4
	srl	%o1, 2, %o1	! Now we have bits <29:0> set
	set	(2*NBPG), %o5
	ba,pt	%icc, 1f
	 andn	%o1, 3, %o1	! Now we have bits <29:2> set
	
	.align 8
1:
	ldxa	[%o4] ASI_DCACHE_TAG, %o3
	mov	%o4, %o0
	deccc	16, %o5
	bl,pn	%icc, 2f
	
	 inc	16, %o4
	xor	%o3, %o2, %o3
	andcc	%o3, %o1, %g0
	bne,pt	%xcc, 1b
	 membar	#LoadStore
	
dlflush2:
	stxa	%g0, [%o0] ASI_DCACHE_TAG
	ba,pt	%icc, 1b
	 membar	#StoreLoad
2:

	wr	%g0, ASI_PRIMARY_NOFAULT, %asi
	sethi	%hi(KERNBASE), %o5
	flush	%o5
	retl
	 membar	#Sync

	.align 8
	.globl  _C_LABEL(us3_dcache_flush_page)
	.proc 1
	FTYPE(us3_dcache_flush_page)
_C_LABEL(us3_dcache_flush_page):
	ldxa    [%g0] ASI_MCCR, %o1
	btst    MCCR_DCACHE_EN, %o1
	bz,pn   %icc, 1f
	 nop
	sethi   %hi(PAGE_SIZE), %o4
	or      %g0, (PAGE_SIZE - 1), %o3
	andn    %o0, %o3, %o0
2:
	subcc   %o4, 32, %o4
	stxa    %g0, [%o0 + %o4] ASI_DCACHE_INVALIDATE
	membar  #Sync
	bne,pt  %icc, 2b
	 nop
1:
	retl
	 nop

	.globl no_dcache_flush_page
ENTRY(no_dcache_flush_page)
	retl
	 nop

/*
 * cache_flush_virt(va, len)
 *
 * Clear everything in that va range from D$.
 *
 */
	.align 8
	.globl	_C_LABEL(cache_flush_virt)
	.proc 1
	FTYPE(cache_flush_virt)
_C_LABEL(cache_flush_virt):
	brz,pn	%o1, 2f		! What? nothing to clear?
	 add	%o0, %o1, %o2
	mov	0x1ff, %o3
	sllx	%o3, 5, %o3	! Generate mask for VA bits
	and	%o0, %o3, %o0
	and	%o2, %o3, %o2
	sub	%o2, %o1, %o4	! End < start? need to split flushes.
	brlz,pn	%o4, 1f
	 movrz	%o4, %o3, %o4	! If start == end we need to wrap

	!! Clear from start to end
1:
dlflush3:
	stxa	%g0, [%o0] ASI_DCACHE_TAG
	dec	16, %o4
	brgz,pt	%o4, 1b
	 inc	16, %o0
2:
	sethi	%hi(KERNBASE), %o5
	flush	%o5
	membar	#Sync
	retl
	 nop

/*
 *	cache_flush_phys(paddr_t, psize_t, int);
 *
 *	Clear a set of paddrs from the D$ and if param3 is
 *	non-zero, E$.  (E$ is not supported yet).
 */

		.align 8
	.globl	_C_LABEL(cache_flush_phys)
	.proc 1
	FTYPE(cache_flush_phys)
_C_LABEL(cache_flush_phys):
#ifdef DEBUG
	tst	%o2		! Want to clear E$?
	tnz	1		! Error!
#endif	/* DEBUG */
	add	%o0, %o1, %o1	! End PA

	!!
	!! D$ tags match pa bits 40-13.
	!! Generate a mask for them.
	!!

	mov	-1, %o2		! Generate mask for tag: bits [40..13]
	srl	%o2, 5, %o2	! 32-5 = [27..0]
	sllx	%o2, 13, %o2	! 27+13 = [40..13]

	and	%o2, %o0, %o0	! Mask away uninteresting bits
	and	%o2, %o1, %o1	! (probably not necessary)

	set	(2*NBPG), %o5
	clr	%o4
1:
	ldxa	[%o4] ASI_DCACHE_TAG, %o3
	sllx	%o3, 40-29, %o3	! Shift D$ tag into place
	and	%o3, %o2, %o3	! Mask out trash
	cmp	%o0, %o3
	blt,pt	%xcc, 2f	! Too low
	cmp	%o1, %o3
	bgt,pt	%xcc, 2f	! Too high
	 nop

	membar	#LoadStore
dlflush4:
	stxa	%g0, [%o4] ASI_DCACHE_TAG ! Just right
2:
	membar	#StoreLoad
	dec	16, %o5
	brgz,pt	%o5, 1b
	 inc	16, %o4

	sethi	%hi(KERNBASE), %o5
	flush	%o5
	membar	#Sync
	retl
	 nop

/*
 * XXXXX Still needs lotsa cleanup after sendsig is complete and offsets are known
 *
 * The following code is copied to the top of the user stack when each
 * process is exec'ed, and signals are `trampolined' off it.
 *
 * When this code is run, the stack looks like:
 *	[%sp]			128 bytes to which registers can be dumped
 *	[%sp + 128]		signal number (goes in %o0)
 *	[%sp + 128 + 4]		signal code (ignored)
 *	[%sp + 128 + 8]		siginfo pointer(goes in %o1)
 *	[%sp + 128 + 16]	first word of saved state (sigcontext)
 *	    .
 *	    .
 *	    .
 *	[%sp + NNN]		last word of saved state
 *	[%sp + ...]		siginfo structure
 * (followed by previous stack contents or top of signal stack).
 * The address of the function to call is in %g1; the old %g1 and %o0
 * have already been saved in the sigcontext.  We are running in a clean
 * window, all previous windows now being saved to the stack.
 *
 * XXX this is bullshit
 * Note that [%sp + 128 + 8] == %sp + 128 + 16.  The copy at %sp+128+8
 * will eventually be removed, with a hole left in its place, if things
 * work out.
 */
	.globl	_C_LABEL(sigcode)
	.globl	_C_LABEL(esigcode)
_C_LABEL(sigcode):
	/*
	 * XXX  the `save' and `restore' below are unnecessary: should
	 *	replace with simple arithmetic on %sp
	 *
	 * Make room on the stack for 64 %f registers + %fsr.  This comes
	 * out to 64*4+8 or 264 bytes, but this must be aligned to a multiple
	 * of 64, or 320 bytes.
	 */
	save	%sp, -CC64FSZ - 320, %sp
	mov	%g2, %l2		! save globals in %l registers
	mov	%g3, %l3
	mov	%g4, %l4
	mov	%g5, %l5
	mov	%g6, %l6
	mov	%g7, %l7
	/*
	 * Saving the fpu registers is expensive, so do it iff it is
	 * enabled and dirty.
	 */
	rd	%fprs, %l0
	btst	FPRS_DL|FPRS_DU, %l0	! All clean?
	bz,pt	%icc, 2f
	 btst	FPRS_DL, %l0		! test dl
	bz,pt	%icc, 1f
	 btst	FPRS_DU, %l0		! test du

	! fpu is enabled, oh well
	stx	%fsr, [%sp + CC64FSZ + BIAS + 0]
	add	%sp, BIAS+CC64FSZ+BLOCK_SIZE, %l0	! Generate a pointer so we can
	andn	%l0, BLOCK_ALIGN, %l0	! do a block store
	stda	%f0, [%l0] ASI_BLK_P
	inc	BLOCK_SIZE, %l0
	stda	%f16, [%l0] ASI_BLK_P
1:
	bz,pt	%icc, 2f
	 add	%sp, BIAS+CC64FSZ+BLOCK_SIZE, %l0	! Generate a pointer so we can
	andn	%l0, BLOCK_ALIGN, %l0	! do a block store
	add	%l0, 2*BLOCK_SIZE, %l0	! and skip what we already stored
	stda	%f32, [%l0] ASI_BLK_P
	inc	BLOCK_SIZE, %l0
	stda	%f48, [%l0] ASI_BLK_P
2:
	membar	#Sync
	rd	%fprs, %l0		! reload fprs copy, for checking after
	rd	%y, %l1			! in any case, save %y
	lduw	[%fp + BIAS + 128], %o0	! sig
	ldx	[%fp + BIAS + 128 + 8], %o1	! siginfo
	call	%g1			! (*sa->sa_handler)(sig, sip, scp)
	 add	%fp, BIAS + 128 + 16, %o2	! scp
	wr	%l1, %g0, %y		! in any case, restore %y

	/*
	 * Now that the handler has returned, re-establish all the state
	 * we just saved above, then do a sigreturn.
	 */
	btst	FPRS_DL|FPRS_DU, %l0	! All clean?
	bz,pt	%icc, 2f
	 btst	FPRS_DL, %l0		! test dl
	bz,pt	%icc, 1f
	 btst	FPRS_DU, %l0		! test du

	ldx	[%sp + CC64FSZ + BIAS + 0], %fsr
	add	%sp, BIAS+CC64FSZ+BLOCK_SIZE, %l0	! Generate a pointer so we can
	andn	%l0, BLOCK_ALIGN, %l0	! do a block load
	ldda	[%l0] ASI_BLK_P, %f0
	inc	BLOCK_SIZE, %l0
	ldda	[%l0] ASI_BLK_P, %f16
1:
	bz,pt	%icc, 2f
	 nop
	add	%sp, BIAS+CC64FSZ+BLOCK_SIZE, %l0	! Generate a pointer so we can
	andn	%l0, BLOCK_ALIGN, %l0	! do a block load
	inc	2*BLOCK_SIZE, %l0	! and skip what we already loaded
	ldda	[%l0] ASI_BLK_P, %f32
	inc	BLOCK_SIZE, %l0
	ldda	[%l0] ASI_BLK_P, %f48
2:
	mov	%l2, %g2
	mov	%l3, %g3
	mov	%l4, %g4
	mov	%l5, %g5
	mov	%l6, %g6
	mov	%l7, %g7
	membar	#Sync

	restore	%g0, SYS_sigreturn, %g1 ! get registers back & set syscall #
	add	%sp, BIAS + 128 + 16, %o0	! compute scp
!	andn	%o0, 0x0f, %o0
	t	ST_SYSCALL		! sigreturn(scp)
	! sigreturn does not return unless it fails
	mov	SYS_exit, %g1		! exit(errno)
	t	ST_SYSCALL
_C_LABEL(esigcode):


/*
 * Primitives
 */
#ifdef ENTRY
#undef ENTRY
#endif	/* ENTRY */

#ifdef GPROF
	.globl	_mcount
#define	ENTRY(x) \
	.globl _C_LABEL(x); _C_LABEL(x): ; \
	.data; \
	.align 8; \
0:	.uaword 0; .uaword 0; \
	.text;	\
	save	%sp, -CC64FSZ, %sp; \
	sethi	%hi(0b), %o0; \
	call	_mcount; \
	or	%o0, %lo(0b), %o0; \
	restore
#else	/* GPROF */
#define	ENTRY(x)	.globl _C_LABEL(x); _C_LABEL(x):
#endif	/* GPROF */
#define	ALTENTRY(x)	.globl _C_LABEL(x); _C_LABEL(x):

/*
 * getfp() - get stack frame pointer
 */
ENTRY(getfp)
	retl
	 mov %fp, %o0

/*
 * copyinstr(fromaddr, toaddr, maxlength, &lencopied)
 *
 * Copy a null terminated string from the user address space into
 * the kernel address space.
 */
ENTRY(copyinstr)
	! %o0 = fromaddr, %o1 = toaddr, %o2 = maxlen, %o3 = &lencopied
	brgz,pt	%o2, 1f					! Make sure len is valid
	 nop
	retl
	 mov	ENAMETOOLONG, %o0
1:
	GET_CPCB(%o4)			! catch faults
	set	Lcsfault, %o5
	membar	#Sync
	stx	%o5, [%o4 + PCB_ONFAULT]

	mov	%o1, %o5		!	save = toaddr;
! XXX should do this in bigger chunks when possible
0:					! loop:
	ldsba	[%o0] ASI_AIUS, %g1	!	c = *fromaddr;
	stb	%g1, [%o1]		!	*toaddr++ = c;
	inc	%o1
	brz,a,pn	%g1, Lcsdone	!	if (c == NULL)
	 clr	%o0			!		{ error = 0; done; }
	deccc	%o2			!	if (--len > 0) {
	bg,pt	%icc, 0b		!		fromaddr++;
	 inc	%o0			!		goto loop;
	ba,pt	%xcc, Lcsdone		!	}
	 mov	ENAMETOOLONG, %o0	!	error = ENAMETOOLONG;
	NOTREACHED

/*
 * copyoutstr(fromaddr, toaddr, maxlength, &lencopied)
 *
 * Copy a null terminated string from the kernel
 * address space to the user address space.
 */
ENTRY(copyoutstr)
	! %o0 = fromaddr, %o1 = toaddr, %o2 = maxlen, %o3 = &lencopied
	brgz,pt	%o2, 1f					! Make sure len is valid
	 nop
	retl
	 mov	ENAMETOOLONG, %o0
1:
	GET_CPCB(%o4)			! catch faults
	set	Lcsfault, %o5
	membar	#Sync
	stx	%o5, [%o4 + PCB_ONFAULT]

	mov	%o1, %o5		!	save = toaddr;
! XXX should do this in bigger chunks when possible
0:					! loop:
	ldsb	[%o0], %g1		!	c = *fromaddr;
	stba	%g1, [%o1] ASI_AIUS	!	*toaddr++ = c;
	inc	%o1
	brz,a,pn	%g1, Lcsdone	!	if (c == NULL)
	 clr	%o0			!		{ error = 0; done; }
	deccc	%o2			!	if (--len > 0) {
	bg,pt	%icc, 0b		!		fromaddr++;
	 inc	%o0			!		goto loop;
					!	}
	mov	ENAMETOOLONG, %o0	!	error = ENAMETOOLONG;
Lcsdone:				! done:
	sub	%o1, %o5, %o1		!	len = to - save;
	brnz,a	%o3, 1f			!	if (lencopied)
	 stx	%o1, [%o3]		!		*lencopied = len;
1:
	retl				! cpcb->pcb_onfault = 0;
	 stx	%g0, [%o4 + PCB_ONFAULT]! return (error);

Lcsfault:
	b	Lcsdone			! error = EFAULT;
	 mov	EFAULT, %o0		! goto ret;

/*
 * copystr(fromaddr, toaddr, maxlength, &lencopied)
 *
 * Copy a null terminated string from one point to another in
 * the kernel address space.  (This is a leaf procedure, but
 * it does not seem that way to the C compiler.)
 */
ENTRY(copystr)
	brgz,pt	%o2, 0f	! Make sure len is valid
	 mov	%o1, %o5		!	to0 = to;
	retl
	 mov	ENAMETOOLONG, %o0
0:					! loop:
	ldsb	[%o0], %o4		!	c = *from;
	tst	%o4
	stb	%o4, [%o1]		!	*to++ = c;
	be	1f			!	if (c == 0)
	 inc	%o1			!		goto ok;
	deccc	%o2			!	if (--len > 0) {
	bg,a	0b			!		from++;
	 inc	%o0			!		goto loop;
	b	2f			!	}
	 mov	ENAMETOOLONG, %o0	!	ret = ENAMETOOLONG; goto done;
1:					! ok:
	clr	%o0			!	ret = 0;
2:
	sub	%o1, %o5, %o1		!	len = to - to0;
	tst	%o3			!	if (lencopied)
	bnz,a	3f
	 stx	%o1, [%o3]		!		*lencopied = len;
3:
	retl
	 nop
#ifdef DIAGNOSTIC
4:
	sethi	%hi(5f), %o0
	call	_C_LABEL(panic)
	 or	%lo(5f), %o0, %o0
	.data
5:
	.asciz	"copystr"
	_ALIGN
	.text
#endif	/* DIAGNOSTIC */

/*
 * copyin(src, dst, len)
 *
 * Copy specified amount of data from user space into the kernel.
 *
 * This is a modified version of bcopy that uses ASI_AIUS.  When
 * bcopy is optimized to use block copy ASIs, this should be also.
 */

#define	BCOPY_SMALL	32	/* if < 32, copy by bytes */

ENTRY(copyin)
!	flushw			! Make sure we don't have stack probs & lose hibits of %o
	GET_CPCB(%o3)
	wr	%g0, ASI_AIUS, %asi
	set	Lcopyfault, %o4
!	mov	%o7, %g7		! save return address
	membar	#Sync
	stx	%o4, [%o3 + PCB_ONFAULT]
	cmp	%o2, BCOPY_SMALL
Lcopyin_start:
	bge,a	Lcopyin_fancy	! if >= this many, go be fancy.
	 btst	7, %o0		! (part of being fancy)

	/*
	 * Not much to copy, just do it a byte at a time.
	 */
	deccc	%o2		! while (--len >= 0)
	bl	1f
0:
	 inc	%o0
	ldsba	[%o0 - 1] %asi, %o4!	*dst++ = (++src)[-1];
	stb	%o4, [%o1]
	deccc	%o2
	bge	0b
	 inc	%o1
1:
	ba	Lcopyin_done
	 clr	%o0
	NOTREACHED

	/*
	 * Plenty of data to copy, so try to do it optimally.
	 */
Lcopyin_fancy:
	! check for common case first: everything lines up.
!	btst	7, %o0		! done already
	bne	1f
!	 XXX check no delay slot
	btst	7, %o1
	be,a	Lcopyin_doubles
	 dec	8, %o2		! if all lined up, len -= 8, goto copyin_doubes

	! If the low bits match, we can make these line up.
1:
	xor	%o0, %o1, %o3	! t = src ^ dst;
	btst	1, %o3		! if (t & 1) {
	be,a	1f
	 btst	1, %o0		! [delay slot: if (src & 1)]

	! low bits do not match, must copy by bytes.
0:
	ldsba	[%o0] %asi, %o4	!	do {
	inc	%o0		!		(++dst)[-1] = *src++;
	inc	%o1
	deccc	%o2
	bnz	0b		!	} while (--len != 0);
	 stb	%o4, [%o1 - 1]
	ba	Lcopyin_done
	 clr	%o0
	NOTREACHED

	! lowest bit matches, so we can copy by words, if nothing else
1:
	be,a	1f		! if (src & 1) {
	 btst	2, %o3		! [delay slot: if (t & 2)]

	! although low bits match, both are 1: must copy 1 byte to align
	ldsba	[%o0] %asi, %o4	!	*dst++ = *src++;
	stb	%o4, [%o1]
	inc	%o0
	inc	%o1
	dec	%o2		!	len--;
	btst	2, %o3		! } [if (t & 2)]
1:
	be,a	1f		! if (t & 2) {
	 btst	2, %o0		! [delay slot: if (src & 2)]
	dec	2, %o2		!	len -= 2;
0:
	ldsha	[%o0] %asi, %o4	!	do {
	sth	%o4, [%o1]	!		*(short *)dst = *(short *)src;
	inc	2, %o0		!		dst += 2, src += 2;
	deccc	2, %o2		!	} while ((len -= 2) >= 0);
	bge	0b
	 inc	2, %o1
	b	Lcopyin_mopb	!	goto mop_up_byte;
	 btst	1, %o2		! } [delay slot: if (len & 1)]
	NOTREACHED

	! low two bits match, so we can copy by longwords
1:
	be,a	1f		! if (src & 2) {
	 btst	4, %o3		! [delay slot: if (t & 4)]

	! although low 2 bits match, they are 10: must copy one short to align
	ldsha	[%o0] %asi, %o4	!	(*short *)dst = *(short *)src;
	sth	%o4, [%o1]
	inc	2, %o0		!	dst += 2;
	inc	2, %o1		!	src += 2;
	dec	2, %o2		!	len -= 2;
	btst	4, %o3		! } [if (t & 4)]
1:
	be,a	1f		! if (t & 4) {
	 btst	4, %o0		! [delay slot: if (src & 4)]
	dec	4, %o2		!	len -= 4;
0:
	lduwa	[%o0] %asi, %o4	!	do {
	st	%o4, [%o1]	!		*(int *)dst = *(int *)src;
	inc	4, %o0		!		dst += 4, src += 4;
	deccc	4, %o2		!	} while ((len -= 4) >= 0);
	bge	0b
	 inc	4, %o1
	b	Lcopyin_mopw	!	goto mop_up_word_and_byte;
	 btst	2, %o2		! } [delay slot: if (len & 2)]
	NOTREACHED

	! low three bits match, so we can copy by doublewords
1:
	be	1f		! if (src & 4) {
	 dec	8, %o2		! [delay slot: len -= 8]
	lduwa	[%o0] %asi, %o4	!	*(int *)dst = *(int *)src;
	st	%o4, [%o1]
	inc	4, %o0		!	dst += 4, src += 4, len -= 4;
	inc	4, %o1
	dec	4, %o2		! }
1:
Lcopyin_doubles:
	ldxa	[%o0] %asi, %g1	! do {
	stx	%g1, [%o1]	!	*(double *)dst = *(double *)src;
	inc	8, %o0		!	dst += 8, src += 8;
	deccc	8, %o2		! } while ((len -= 8) >= 0);
	bge	Lcopyin_doubles
	 inc	8, %o1

	! check for a usual case again (save work)
	btst	7, %o2		! if ((len & 7) == 0)
	be	Lcopyin_done	!	goto copyin_done;

	 btst	4, %o2		! if ((len & 4) == 0)
	be,a	Lcopyin_mopw	!	goto mop_up_word_and_byte;
	 btst	2, %o2		! [delay slot: if (len & 2)]
	lduwa	[%o0] %asi, %o4	!	*(int *)dst = *(int *)src;
	st	%o4, [%o1]
	inc	4, %o0		!	dst += 4;
	inc	4, %o1		!	src += 4;
	btst	2, %o2		! } [if (len & 2)]

1:
	! mop up trailing word (if present) and byte (if present).
Lcopyin_mopw:
	be	Lcopyin_mopb	! no word, go mop up byte
	 btst	1, %o2		! [delay slot: if (len & 1)]
	ldsha	[%o0] %asi, %o4	! *(short *)dst = *(short *)src;
	be	Lcopyin_done	! if ((len & 1) == 0) goto done;
	 sth	%o4, [%o1]
	ldsba	[%o0 + 2] %asi, %o4	! dst[2] = src[2];
	stb	%o4, [%o1 + 2]
	ba	Lcopyin_done
	 clr	%o0
	NOTREACHED

	! mop up trailing byte (if present).
Lcopyin_mopb:
	be,a	Lcopyin_done
	 nop
	ldsba	[%o0] %asi, %o4
	stb	%o4, [%o1]

Lcopyin_done:
	GET_CPCB(%o3)
	membar	#Sync
	stx	%g0, [%o3 + PCB_ONFAULT]
	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore ASI
	retl
	 clr	%o0			! return 0

/*
 * copyout(src, dst, len)
 *
 * Copy specified amount of data from kernel to user space.
 * Just like copyin, except that the `dst' addresses are user space
 * rather than the `src' addresses.
 *
 * This is a modified version of bcopy that uses ASI_AIUS.  When
 * bcopy is optimized to use block copy ASIs, this should be also.
 */
 /*
  * This needs to be reimplemented to really do the copy.
  */
ENTRY(copyout)
	/*
	 * ******NOTE****** this depends on bcopy() not using %g7
	 */
Ldocopy:
	GET_CPCB(%o3)
	wr	%g0, ASI_AIUS, %asi
	set	Lcopyfault, %o4
!	mov	%o7, %g7		! save return address
	membar	#Sync
	stx	%o4, [%o3 + PCB_ONFAULT]
	cmp	%o2, BCOPY_SMALL
Lcopyout_start:
	membar	#StoreStore
	bge,a	Lcopyout_fancy	! if >= this many, go be fancy.
	 btst	7, %o0		! (part of being fancy)

	/*
	 * Not much to copy, just do it a byte at a time.
	 */
	deccc	%o2		! while (--len >= 0)
	bl	1f
!	 XXX check no delay slot
0:
	inc	%o0
	ldsb	[%o0 - 1], %o4!	(++dst)[-1] = *src++;
	stba	%o4, [%o1] %asi
	deccc	%o2
	bge	0b
	 inc	%o1
1:
	ba	Lcopyout_done
	 clr	%o0
	NOTREACHED

	/*
	 * Plenty of data to copy, so try to do it optimally.
	 */
Lcopyout_fancy:
	! check for common case first: everything lines up.
!	btst	7, %o0		! done already
	bne	1f
!	 XXX check no delay slot
	btst	7, %o1
	be,a	Lcopyout_doubles
	 dec	8, %o2		! if all lined up, len -= 8, goto copyout_doubes

	! If the low bits match, we can make these line up.
1:
	xor	%o0, %o1, %o3	! t = src ^ dst;
	btst	1, %o3		! if (t & 1) {
	be,a	1f
	 btst	1, %o0		! [delay slot: if (src & 1)]

	! low bits do not match, must copy by bytes.
0:
	ldsb	[%o0], %o4	!	do {
	inc	%o0		!		(++dst)[-1] = *src++;
	inc	%o1
	deccc	%o2
	bnz	0b		!	} while (--len != 0);
	 stba	%o4, [%o1 - 1] %asi
	ba	Lcopyout_done
	 clr	%o0
	NOTREACHED

	! lowest bit matches, so we can copy by words, if nothing else
1:
	be,a	1f		! if (src & 1) {
	 btst	2, %o3		! [delay slot: if (t & 2)]

	! although low bits match, both are 1: must copy 1 byte to align
	ldsb	[%o0], %o4	!	*dst++ = *src++;
	stba	%o4, [%o1] %asi
	inc	%o0
	inc	%o1
	dec	%o2		!	len--;
	btst	2, %o3		! } [if (t & 2)]
1:
	be,a	1f		! if (t & 2) {
	 btst	2, %o0		! [delay slot: if (src & 2)]
	dec	2, %o2		!	len -= 2;
0:
	ldsh	[%o0], %o4	!	do {
	stha	%o4, [%o1] %asi	!		*(short *)dst = *(short *)src;
	inc	2, %o0		!		dst += 2, src += 2;
	deccc	2, %o2		!	} while ((len -= 2) >= 0);
	bge	0b
	 inc	2, %o1
	b	Lcopyout_mopb	!	goto mop_up_byte;
	 btst	1, %o2		! } [delay slot: if (len & 1)]
	NOTREACHED

	! low two bits match, so we can copy by longwords
1:
	be,a	1f		! if (src & 2) {
	 btst	4, %o3		! [delay slot: if (t & 4)]

	! although low 2 bits match, they are 10: must copy one short to align
	ldsh	[%o0], %o4	!	(*short *)dst = *(short *)src;
	stha	%o4, [%o1] %asi
	inc	2, %o0		!	dst += 2;
	inc	2, %o1		!	src += 2;
	dec	2, %o2		!	len -= 2;
	btst	4, %o3		! } [if (t & 4)]
1:
	be,a	1f		! if (t & 4) {
	 btst	4, %o0		! [delay slot: if (src & 4)]
	dec	4, %o2		!	len -= 4;
0:
	lduw	[%o0], %o4	!	do {
	sta	%o4, [%o1] %asi	!		*(int *)dst = *(int *)src;
	inc	4, %o0		!		dst += 4, src += 4;
	deccc	4, %o2		!	} while ((len -= 4) >= 0);
	bge	0b
	 inc	4, %o1
	b	Lcopyout_mopw	!	goto mop_up_word_and_byte;
	 btst	2, %o2		! } [delay slot: if (len & 2)]
	NOTREACHED

	! low three bits match, so we can copy by doublewords
1:
	be	1f		! if (src & 4) {
	 dec	8, %o2		! [delay slot: len -= 8]
	lduw	[%o0], %o4	!	*(int *)dst = *(int *)src;
	sta	%o4, [%o1] %asi
	inc	4, %o0		!	dst += 4, src += 4, len -= 4;
	inc	4, %o1
	dec	4, %o2		! }
1:
Lcopyout_doubles:
	ldx	[%o0], %g1	! do {
	stxa	%g1, [%o1] %asi	!	*(double *)dst = *(double *)src;
	inc	8, %o0		!	dst += 8, src += 8;
	deccc	8, %o2		! } while ((len -= 8) >= 0);
	bge	Lcopyout_doubles
	 inc	8, %o1

	! check for a usual case again (save work)
	btst	7, %o2		! if ((len & 7) == 0)
	be	Lcopyout_done	!	goto copyout_done;

	 btst	4, %o2		! if ((len & 4) == 0)
	be,a	Lcopyout_mopw	!	goto mop_up_word_and_byte;
	 btst	2, %o2		! [delay slot: if (len & 2)]
	lduw	[%o0], %o4	!	*(int *)dst = *(int *)src;
	sta	%o4, [%o1] %asi
	inc	4, %o0		!	dst += 4;
	inc	4, %o1		!	src += 4;
	btst	2, %o2		! } [if (len & 2)]

1:
	! mop up trailing word (if present) and byte (if present).
Lcopyout_mopw:
	be	Lcopyout_mopb	! no word, go mop up byte
	 btst	1, %o2		! [delay slot: if (len & 1)]
	ldsh	[%o0], %o4	! *(short *)dst = *(short *)src;
	be	Lcopyout_done	! if ((len & 1) == 0) goto done;
	 stha	%o4, [%o1] %asi
	ldsb	[%o0 + 2], %o4	! dst[2] = src[2];
	stba	%o4, [%o1 + 2] %asi
	ba	Lcopyout_done
	 clr	%o0
	NOTREACHED

	! mop up trailing byte (if present).
Lcopyout_mopb:
	be,a	Lcopyout_done
	 nop
	ldsb	[%o0], %o4
	stba	%o4, [%o1] %asi

Lcopyout_done:
	GET_CPCB(%o3)
	membar	#Sync
	stx	%g0, [%o3 + PCB_ONFAULT]
!	jmp	%g7 + 8		! Original instr
	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore ASI
	membar	#StoreStore|#StoreLoad
	retl			! New instr
	 clr	%o0			! return 0

! Copyin or copyout fault.  Clear cpcb->pcb_onfault and return EFAULT.
! Note that although we were in bcopy, there is no state to clean up;
! the only special thing is that we have to return to [g7 + 8] rather than
! [o7 + 8].
Lcopyfault:
	GET_CPCB(%o3)
	stx	%g0, [%o3 + PCB_ONFAULT]
	membar	#StoreStore|#StoreLoad
	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore ASI
	retl
	 mov	EFAULT, %o0

/*
 * cpu_switchto(struct proc *old, struct proc *new)
 *
 * Save the context of "old" and switch to "new".
 */
ENTRY(cpu_switchto)
	save	%sp, -CC64FSZ, %sp
	rdpr	%pstate, %o1		! oldpstate = %pstate;
	wrpr	%g0, PSTATE_INTR, %pstate ! make sure we're on normal globals

	ldx	[%g7 + CI_CPCB], %l5

	/*
	 * Register usage:
	 *
	 *	%i0 = oldproc
	 *	%i1 = newproc
	 *	%l1 = newpcb
	 *	%l2 = newpstate
	 *	%l5 = cpcb
	 *	%o0 = tmp 1
	 *	%o1 = oldpstate
	 *	%o2 = tmp 2
	 *	%o3 = vm
	 *	%o4 = sswap
	 *	%o5 = <free>
	 */

	/*
	 * Committed to running process p.
	 */
#if defined(MULTIPROCESSOR)
	/*
	 * p->p_cpu = curcpu();
	 */
	ldx	[%g7 + CI_SELF], %o0
	stx	%o0, [%i1 + P_CPU]
#endif	/* defined(MULTIPROCESSOR) */
	mov	SONPROC, %o0			! newproc->p_stat = SONPROC
	stb	%o0, [%i1 + P_STAT]
	ldx	[%i1 + P_ADDR], %l1		! newpcb = newpeoc->p_addr;

	flushw				! save all register windows except this one

	/*
	 * Not the old process.  Save the old process, if any;
	 * then load p.
	 */
	brz,pn	%i0, Lsw_load		! if no old process, go load
	 wrpr	%g0, PSTATE_KERN, %pstate

	stx	%i6, [%l5 + PCB_SP]	! cpcb->pcb_sp = sp;
	stx	%i7, [%l5 + PCB_PC]	! cpcb->pcb_pc = pc;
	sth	%o1, [%l5 + PCB_PSTATE]	! cpcb->pcb_pstate = oldpstate;
	rdpr	%cwp, %o2		! Useless
	stb	%o2, [%l5 + PCB_CWP]

	/*
	 * Load the new process.  To load, we must change stacks and
	 * alter cpcb and the window control registers, hence we must
	 * disable interrupts.
	 *
	 * We also must load up the `in' and `local' registers.
	 */
Lsw_load:
	/* set new cpcb */
	stx	%i1, [%g7 + CI_CURPROC]	! curproc = newproc;
	stx	%l1, [%g7 + CI_CPCB]	! cpcb = newpcb;

	ldx	[%l1 + PCB_SP], %i6
	ldx	[%l1 + PCB_PC], %i7

	/* finally, enable traps */
	wrpr	%g0, PSTATE_INTR, %pstate

	/*
	 * Now running p.  Make sure it has a context so that it
	 * can talk about user space stuff.  (Its pcb_uw is currently
	 * zero so it is safe to have interrupts going here.)
	 */
	ldx	[%i1 + P_VMSPACE], %o3		! vm = newproc->p_vmspace;
	sethi	%hi(_C_LABEL(kernel_pmap_)), %o1
	mov	CTX_SECONDARY, %l5		! Recycle %l5
	ldx	[%o3 + VM_PMAP], %o2		! if (vm->vm_pmap != kernel_pmap_)
	or	%o1, %lo(_C_LABEL(kernel_pmap_)), %o1
	cmp	%o2, %o1
	bz,pn	%xcc, Lsw_havectx		! Don't replace kernel context!
	 ld	[%o2 + PM_CTX], %o0
	brnz,pt	%o0, Lsw_havectx		!	goto havecontext;
	 nop
	
	/* p does not have a context: call ctx_alloc to get one */
	call	_C_LABEL(ctx_alloc)		! ctx_alloc(&vm->vm_pmap);
	 mov	%o2, %o0

	set	DEMAP_CTX_SECONDARY, %o1	! This context has been recycled
	stxa	%o0, [%l5] ASI_DMMU		! so we need to invalidate
	membar	#Sync
	stxa	%o1, [%o1] ASI_DMMU_DEMAP	! whatever bits of it may
	stxa	%o1, [%o1] ASI_IMMU_DEMAP	! be left in the TLB
	membar	#Sync
	/* p does have a context: just switch to it */
Lsw_havectx:
	! context is in %o0
	/*
	 * We probably need to flush the cache here.
	 */
	SET_MMU_CONTEXTID(%o0, %l5)		! Maybe we should invalidate the old context?
	membar	#Sync				! Maybe we should use flush here?
	flush	%sp

!	wrpr	%g0, 0, %cleanwin	! DEBUG
	clr	%g4		! This needs to point to the base of the data segment
	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore default ASI
	wrpr	%g0, PSTATE_INTR, %pstate
	ret
	 restore

/*
 * Snapshot the current process so that stack frames are up to date.
 * Only used just before a crash dump.
 */
ENTRY(snapshot)
	rdpr	%pstate, %o1		! save psr
	stx	%o6, [%o0 + PCB_SP]	! save sp
	rdpr	%pil, %o2
	sth	%o1, [%o0 + PCB_PSTATE]
	rdpr	%cwp, %o3
	stb	%o2, [%o0 + PCB_PIL]
	stb	%o3, [%o0 + PCB_CWP]

	flushw
	save	%sp, -CC64FSZ, %sp
	flushw
	ret
	 restore

/*
 * cpu_set_kpc() and cpu_fork() arrange for proc_trampoline() to run
 * after after a process gets chosen in switch(). The stack frame will
 * contain a function pointer in %l0, and an argument to pass to it in %l2.
 *
 * If the function *(%l0) returns, we arrange for an immediate return
 * to user mode. This happens in two known cases: after execve(2) of init,
 * and when returning a child to user mode after a fork(2).
 */
ENTRY(proc_trampoline)
#ifdef MULTIPROCESSOR
	save	%sp, -CC64FSZ, %sp
	call	_C_LABEL(proc_trampoline_mp)
	 nop
	restore
#endif
	wrpr	%g0, 0, %pil		! Reset interrupt level
	call	%l0			! re-use current frame
	 mov	%l1, %o0

	/*
	 * Here we finish up as in syscall, but simplified.  We need to
	 * fiddle pc and npc a bit, as execve() / setregs() /cpu_set_kpc()
	 * have only set npc, in anticipation that trap.c will advance past
	 * the trap instruction; but we bypass that, so we must do it manually.
	 */
!	save	%sp, -CC64FSZ, %sp		! Save a kernel frame to emulate a syscall
#if 0
	/* This code doesn't seem to work, but it should. */
	ldx	[%sp + CC64FSZ + BIAS + TF_TSTATE], %g1
	ldx	[%sp + CC64FSZ + BIAS + TF_NPC], %g2	! pc = tf->tf_npc from execve/fork
	andn	%g1, CWP, %g1			! Clear the CWP bits
	add	%g2, 4, %g3			! npc = pc+4
	rdpr	%cwp, %g5			! Fixup %cwp in %tstate
	stx	%g3, [%sp + CC64FSZ + BIAS + TF_NPC]
	or	%g1, %g5, %g1
	stx	%g2, [%sp + CC64FSZ + BIAS + TF_PC]
	stx	%g1, [%sp + CC64FSZ + BIAS + TF_TSTATE]
#else	/* 0 */
	mov	PSTATE_USER, %g1		! XXXX user pstate (no need to load it)
	sllx	%g1, TSTATE_PSTATE_SHIFT, %g1	! Shift it into place
	rdpr	%cwp, %g5			! Fixup %cwp in %tstate
	or	%g1, %g5, %g1
	stx	%g1, [%sp + CC64FSZ + BIAS + TF_TSTATE]
#endif	/* 0 */
	CHKPT %o3,%o4,0x35
	ba,a,pt	%icc, return_from_trap
	 nop

#ifdef DDB

/*
 * The following probably need to be changed, but to what I don't know.
 */

/*
 * u_int64_t
 * probeget(addr, asi, size)
 *	paddr_t addr;
 *	int asi;
 *	int size;
 *
 * Read a (byte,short,int,long) from the given address.
 * Like copyin but our caller is supposed to know what he is doing...
 * the address can be anywhere.
 *
 * We optimize for space, rather than time, here.
 */
ENTRY(probeget)
	mov	%o2, %o4
	! %o0 = addr, %o1 = asi, %o4 = (1,2,4)
	GET_CPCB(%o2)			! cpcb->pcb_onfault = Lfsprobe;
	set	_C_LABEL(Lfsprobe), %o5
	stx	%o5, [%o2 + PCB_ONFAULT]
	or	%o0, 0x9, %o3		! if (PHYS_ASI(asi)) {
	sub	%o3, 0x1d, %o3
	brz,a	%o3, 0f
	 mov	%g0, %o5
	DLFLUSH %o0,%o5		!	flush cache line
					! }
0:
	btst	1, %o4
	wr	%o1, 0, %asi
	membar	#Sync
	bz	0f			! if (len & 1)
	 btst	2, %o4
	ba,pt	%icc, 1f
	 lduba	[%o0] %asi, %o0		!	value = *(char *)addr;
0:
	bz	0f			! if (len & 2)
	 btst	4, %o4
	ba,pt	%icc, 1f
	 lduha	[%o0] %asi, %o0		!	value = *(short *)addr;
0:
	bz	0f			! if (len & 4)
	 btst	8, %o4
	ba,pt	%icc, 1f
	 lda	[%o0] %asi, %o0		!	value = *(int *)addr;
0:
	ldxa	[%o0] %asi, %o0		!	value = *(long *)addr;
1:	
	membar	#Sync
	brz	%o5, 1f			! if (cache flush addr != 0)
	 nop
	DLFLUSH2 %o5			!	flush cache line again
1:
	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore default ASI	
	stx	%g0, [%o2 + PCB_ONFAULT]
	retl				! made it, clear onfault and return
	 membar	#StoreStore|#StoreLoad

	/*
	 * Fault handler for probeget
	 */
	.globl	_C_LABEL(Lfsprobe)
_C_LABEL(Lfsprobe):
	stx	%g0, [%o2 + PCB_ONFAULT]! error in r/w, clear pcb_onfault
	mov	-1, %o1
	wr	%g0, ASI_PRIMARY_NOFAULT, %asi		! Restore default ASI	
	membar	#StoreStore|#StoreLoad
	retl				! and return error indicator
	 mov	-1, %o0

#endif	/* DDB */

/*
 * pmap_zero_page(pa)
 *
 * Zero one page physically addressed
 *
 * Block load/store ASIs do not exist for physical addresses,
 * so we won't use them.
 *
 * While we do the zero operation, we also need to blast away
 * the contents of the D$.  We will execute a flush at the end
 * to sync the I$.
 */
	.data
paginuse:
	.word	0
	.text
ENTRY(pmap_zero_phys)
	set	NBPG, %o2		! Loop count
	clr	%o1
1:
	dec	8, %o2
	stxa	%g0, [%o0] ASI_PHYS_CACHED
	inc	8, %o0
dlflush5:
	stxa	%g0, [%o1] ASI_DCACHE_TAG
	brgz	%o2, 1b
	 inc	16, %o1

	sethi	%hi(KERNBASE), %o3
	flush	%o3
	retl
	 nop
/*
 * pmap_copy_page(src, dst)
 *
 * Copy one page physically addressed
 *
 * We also need to blast the D$ and flush like
 * pmap_zero_page.
 */
ENTRY(pmap_copy_phys)
	set	NBPG, %o3
	add	%o3, %o0, %o3
1:
	ldxa	[%o0] ASI_PHYS_CACHED, %o4
	inc	8, %o0
	cmp	%o0, %o3
	stxa	%o4, [%o1] ASI_PHYS_CACHED
	blu,pt	%xcc, 1b
	 inc	8, %o1
	retl
	 nop

/*
 * extern int64_t pseg_get(struct pmap* %o0, vaddr_t addr %o1);
 *
 * Return TTE at addr in pmap.  Uses physical addressing only.
 * pmap->pm_physaddr must by the physical address of pm_segs
 *
 */
ENTRY(pseg_get)
	ldx	[%o0 + PM_PHYS], %o2			! pmap->pm_segs

	srax	%o1, HOLESHIFT, %o3			! Check for valid address
	brz,pt	%o3, 0f					! Should be zero or -1
	 inc	%o3					! Make -1 -> 0
	brnz,pn	%o3, 1f					! Error! In hole!
0:
	srlx	%o1, STSHIFT, %o3
	and	%o3, STMASK, %o3			! Index into pm_segs
	sll	%o3, 3, %o3
	add	%o2, %o3, %o2
	DLFLUSH %o2,%o3
	ldxa	[%o2] ASI_PHYS_CACHED, %o2		! Load page directory pointer
	DLFLUSH2 %o3

	srlx	%o1, PDSHIFT, %o3
	and	%o3, PDMASK, %o3
	sll	%o3, 3, %o3
	brz,pn	%o2, 1f					! NULL entry? check somewhere else
	 add	%o2, %o3, %o2
	DLFLUSH %o2,%o3
	ldxa	[%o2] ASI_PHYS_CACHED, %o2		! Load page table pointer
	DLFLUSH2 %o3

	srlx	%o1, PTSHIFT, %o3			! Convert to ptab offset
	and	%o3, PTMASK, %o3
	sll	%o3, 3, %o3
	brz,pn	%o2, 1f					! NULL entry? check somewhere else
	 add	%o2, %o3, %o2
	DLFLUSH %o2,%o3
	ldxa	[%o2] ASI_PHYS_CACHED, %o0
	DLFLUSH2 %o3
	brgez,pn %o0, 1f				! Entry invalid?  Punt
	 nop
	retl
	 nop
1:
	retl
	 clr	%o0

/*
 * extern int pseg_set(struct pmap* %o0, vaddr_t addr %o1, int64_t tte %o2,
 *			paddr_t spare %o3);
 *
 * Set a pseg entry to a particular TTE value.  Returns 0 on success,
 * 1 if it needs to fill a pseg, 2 if it succeeded but didn't need the
 * spare page, and -1 if the address is in the virtual hole.
 * (NB: nobody in pmap checks for the virtual hole, so the system will hang.)
 * Allocate a page, pass the phys addr in as the spare, and try again.
 * If spare is not NULL it is assumed to be the address of a zeroed physical
 * page that can be used to generate a directory table or page table if needed.
 *
 */
ENTRY(pseg_set)
	!!
	!! However we managed to get here we now have:
	!!
	!! %o0 = *pmap
	!! %o1 = addr
	!! %o2 = tte
	!! %o3 = spare
	!!
	srax	%o1, HOLESHIFT, %o4			! Check for valid address
	brz,pt	%o4, 0f					! Should be zero or -1
	 inc	%o4					! Make -1 -> 0
	brz,pt	%o4, 0f
	 nop
#ifdef DEBUG
	ta	1					! Break into debugger
#endif	/* DEBUG */
	mov	-1, %o0					! Error -- in hole!
	retl
	 mov	-1, %o1
0:
	ldx	[%o0 + PM_PHYS], %o4			! pmap->pm_segs
	srlx	%o1, STSHIFT, %o5
	and	%o5, STMASK, %o5
	sll	%o5, 3, %o5
	add	%o4, %o5, %o4
2:
	DLFLUSH %o4,%g1
	ldxa	[%o4] ASI_PHYS_CACHED, %o5		! Load page directory pointer
	DLFLUSH2 %g1

	brnz,a,pt	%o5, 0f				! Null pointer?
	 mov	%o5, %o4
	brz,pn	%o3, 1f					! Have a spare?
	 mov	%o3, %o5
	casxa	[%o4] ASI_PHYS_CACHED, %g0, %o5
	brnz,pn	%o5, 2b					! Something changed?
	DLFLUSH %o4, %o5
	mov	%o3, %o4
	clr	%o3					! Mark spare as used
0:
	srlx	%o1, PDSHIFT, %o5
	and	%o5, PDMASK, %o5
	sll	%o5, 3, %o5
	add	%o4, %o5, %o4
2:
	DLFLUSH %o4,%g1
	ldxa	[%o4] ASI_PHYS_CACHED, %o5		! Load table directory pointer
	DLFLUSH2 %g1

	brnz,a,pt	%o5, 0f				! Null pointer?
	 mov	%o5, %o4
	brz,pn	%o3, 1f					! Have a spare?
	 mov	%o3, %o5
	casxa	[%o4] ASI_PHYS_CACHED, %g0, %o5
	brnz,pn	%o5, 2b					! Something changed?
	DLFLUSH %o4, %o4
	mov	%o3, %o4
	clr	%o3					! Mark spare as used
0:
	srlx	%o1, PTSHIFT, %o5			! Convert to ptab offset
	and	%o5, PTMASK, %o5
	sll	%o5, 3, %o5
	add	%o5, %o4, %o4
	stxa	%o2, [%o4] ASI_PHYS_CACHED		! Easier than shift+or
	DLFLUSH %o4, %o4
	mov	2, %o0					! spare unused?
	retl
	 movrz	%o3, %g0, %o0				! No. return 0
1:
	retl
	 mov	1, %o0


/*
 * kernel bcopy/memcpy
 * Assumes regions do not overlap; has no useful return value.
 *
 * Must not use %g7 (see copyin/copyout above).
 */
ENTRY(memcpy) /* dest, src, size */
	/*
	 * Swap args for bcopy.  Gcc generates calls to memcpy for
	 * structure assignments.
	 */
	mov	%o0, %o3
	mov	%o1, %o0
	mov	%o3, %o1
	ba,pt	%xcc, Lbcopy_start
	 cmp	%o2, BCOPY_SMALL
ENTRY(bcopy) /* src, dest, size */
	/*
	 * Check for overlaps and punt.
	 *
	 * If src <= dest <= src+len we have a problem.
	 */

	sub	%o1, %o0, %o3

	cmp	%o3, %o2
	blu,pn	%xcc, Lovbcopy
	 cmp	%o2, BCOPY_SMALL
Lbcopy_start:
	bge,pt	%xcc, 2f	! if >= this many, go be fancy.
	 nop

	mov	%o1, %o5	! Save memcpy return value
	/*
	 * Not much to copy, just do it a byte at a time.
	 */
	deccc	%o2		! while (--len >= 0)
	bl	1f
!	 XXX check no delay slot
0:
	inc	%o0
	ldsb	[%o0 - 1], %o4	!	(++dst)[-1] = *src++;
	stb	%o4, [%o1]
	deccc	%o2
	bge	0b
	 inc	%o1
1:
	retl
	 mov	%o5, %o0
	NOTREACHED

	/*
	 * Overlapping bcopies -- punt.
	 */
Lovbcopy:

	/*
	 * Since src comes before dst, and the regions might overlap,
	 * we have to do the copy starting at the end and working backwards.
	 *
	 * We could optimize this, but it almost never happens.
	 */
	mov	%o1, %o5	! Retval
	add	%o2, %o0, %o0	! src += len
	add	%o2, %o1, %o1	! dst += len
	
	deccc	%o2
	bl,pn	%xcc, 1f
	 dec	%o0
0:
	dec	%o1
	ldsb	[%o0], %o4
	dec	%o0
	
	deccc	%o2
	bge,pt	%xcc, 0b
	 stb	%o4, [%o1]
1:
	retl
	 mov	%o5, %o0

	/*
	 * Plenty of data to copy, so try to do it optimally.
	 */
2:
Lbcopy_fancy:

	!!
	!! First align the output to a 8-byte entity
	!! 

	save	%sp, -CC64FSZ, %sp
	
	mov	%i0, %l0
	mov	%i1, %l1
	
	mov	%i2, %l2
	btst	1, %l1
	
	bz,pt	%icc, 4f
	 btst	2, %l1
	ldub	[%l0], %l4				! Load 1st byte
	
	deccc	1, %l2
	ble,pn	%xcc, Lbcopy_finish			! XXXX
	 inc	1, %l0
	
	stb	%l4, [%l1]				! Store 1st byte
	inc	1, %l1					! Update address
	btst	2, %l1
4:	
	bz,pt	%icc, 4f
	
	 btst	1, %l0
	bz,a	1f
	 lduh	[%l0], %l4				! Load short

	ldub	[%l0], %l4				! Load bytes
	
	ldub	[%l0+1], %l3
	sllx	%l4, 8, %l4
	or	%l3, %l4, %l4
	
1:	
	deccc	2, %l2
	ble,pn	%xcc, Lbcopy_finish			! XXXX
	 inc	2, %l0
	sth	%l4, [%l1]				! Store 1st short
	
	inc	2, %l1
4:
	btst	4, %l1
	bz,pt	%xcc, 4f
	
	 btst	3, %l0
	bz,a,pt	%xcc, 1f
	 lduw	[%l0], %l4				! Load word -1

	btst	1, %l0
	bz,a,pt	%icc, 2f
	 lduh	[%l0], %l4
	
	ldub	[%l0], %l4
	
	lduh	[%l0+1], %l3
	sllx	%l4, 16, %l4
	or	%l4, %l3, %l4
	
	ldub	[%l0+3], %l3
	sllx	%l4, 8, %l4
	ba,pt	%icc, 1f
	 or	%l4, %l3, %l4
	
2:
	lduh	[%l0+2], %l3
	sllx	%l4, 16, %l4
	or	%l4, %l3, %l4
	
1:	
	deccc	4, %l2
	ble,pn	%xcc, Lbcopy_finish		! XXXX
	 inc	4, %l0
	
	st	%l4, [%l1]				! Store word
	inc	4, %l1
4:
	!!
	!! We are now 32-bit aligned in the dest.
	!!
Lbcopy_common:	

	and	%l0, 7, %l4				! Shift amount
	andn	%l0, 7, %l0				! Source addr
	
	brz,pt	%l4, Lbcopy_noshift8			! No shift version...

	 sllx	%l4, 3, %l4				! In bits
	mov	8<<3, %l3
	
	ldx	[%l0], %o0				! Load word -1
	sub	%l3, %l4, %l3				! Reverse shift
	deccc	12*8, %l2				! Have enough room?
	
	sllx	%o0, %l4, %o0
	bl,pn	%xcc, 2f
	 and	%l3, 0x38, %l3
Lbcopy_unrolled8:

	/*
	 * This is about as close to optimal as you can get, since
	 * the shifts require EU0 and cannot be paired, and you have
	 * 3 dependent operations on the data.
	 */ 

!	ldx	[%l0+0*8], %o0				! Already done
!	sllx	%o0, %l4, %o0				! Already done
	ldx	[%l0+1*8], %o1
	ldx	[%l0+2*8], %o2
	ldx	[%l0+3*8], %o3
	ldx	[%l0+4*8], %o4
	ba,pt	%icc, 1f
	 ldx	[%l0+5*8], %o5
	.align	8
1:
	srlx	%o1, %l3, %g1
	inc	6*8, %l0
	
	sllx	%o1, %l4, %o1
	or	%g1, %o0, %g6
	ldx	[%l0+0*8], %o0
	
	stx	%g6, [%l1+0*8]
	srlx	%o2, %l3, %g1

	sllx	%o2, %l4, %o2
	or	%g1, %o1, %g6
	ldx	[%l0+1*8], %o1
	
	stx	%g6, [%l1+1*8]
	srlx	%o3, %l3, %g1
	
	sllx	%o3, %l4, %o3
	or	%g1, %o2, %g6
	ldx	[%l0+2*8], %o2
	
	stx	%g6, [%l1+2*8]
	srlx	%o4, %l3, %g1
	
	sllx	%o4, %l4, %o4	
	or	%g1, %o3, %g6
	ldx	[%l0+3*8], %o3
	
	stx	%g6, [%l1+3*8]
	srlx	%o5, %l3, %g1
	
	sllx	%o5, %l4, %o5
	or	%g1, %o4, %g6
	ldx	[%l0+4*8], %o4

	stx	%g6, [%l1+4*8]
	srlx	%o0, %l3, %g1
	deccc	6*8, %l2				! Have enough room?

	sllx	%o0, %l4, %o0				! Next loop
	or	%g1, %o5, %g6
	ldx	[%l0+5*8], %o5
	
	stx	%g6, [%l1+5*8]
	bge,pt	%xcc, 1b
	 inc	6*8, %l1

Lbcopy_unrolled8_cleanup:	
	!!
	!! Finished 8 byte block, unload the regs.
	!! 
	srlx	%o1, %l3, %g1
	inc	5*8, %l0
	
	sllx	%o1, %l4, %o1
	or	%g1, %o0, %g6
		
	stx	%g6, [%l1+0*8]
	srlx	%o2, %l3, %g1
	
	sllx	%o2, %l4, %o2
	or	%g1, %o1, %g6
		
	stx	%g6, [%l1+1*8]
	srlx	%o3, %l3, %g1
	
	sllx	%o3, %l4, %o3
	or	%g1, %o2, %g6
		
	stx	%g6, [%l1+2*8]
	srlx	%o4, %l3, %g1
	
	sllx	%o4, %l4, %o4	
	or	%g1, %o3, %g6
		
	stx	%g6, [%l1+3*8]
	srlx	%o5, %l3, %g1
	
	sllx	%o5, %l4, %o5
	or	%g1, %o4, %g6
		
	stx	%g6, [%l1+4*8]
	inc	5*8, %l1
	
	mov	%o5, %o0				! Save our unused data
	dec	5*8, %l2
2:
	inccc	12*8, %l2
	bz,pn	%icc, Lbcopy_complete
	
	!! Unrolled 8 times
Lbcopy_aligned8:	
!	ldx	[%l0], %o0				! Already done
!	sllx	%o0, %l4, %o0				! Shift high word
	
	 deccc	8, %l2					! Pre-decrement
	bl,pn	%xcc, Lbcopy_finish
1:
	ldx	[%l0+8], %o1				! Load word 0
	inc	8, %l0
	
	srlx	%o1, %l3, %g6
	or	%g6, %o0, %g6				! Combine
	
	stx	%g6, [%l1]				! Store result
	 inc	8, %l1
	
	deccc	8, %l2
	bge,pn	%xcc, 1b
	 sllx	%o1, %l4, %o0	

	btst	7, %l2					! Done?
	bz,pt	%xcc, Lbcopy_complete

	!!
	!! Loadup the last dregs into %o0 and shift it into place
	!! 
	 srlx	%l3, 3, %g6				! # bytes in %o0
	dec	8, %g6					!  - 8
	!! n-8 - (by - 8) -> n - by
	subcc	%l2, %g6, %g0				! # bytes we need
	ble,pt	%icc, Lbcopy_finish
	 nop
	ldx	[%l0+8], %o1				! Need another word
	srlx	%o1, %l3, %o1
	ba,pt	%icc, Lbcopy_finish
	 or	%o0, %o1, %o0				! All loaded up.
	
Lbcopy_noshift8:
	deccc	6*8, %l2				! Have enough room?
	bl,pn	%xcc, 2f
	 nop
	ba,pt	%icc, 1f
	 nop
	.align	32
1:	
	ldx	[%l0+0*8], %o0
	ldx	[%l0+1*8], %o1
	ldx	[%l0+2*8], %o2
	stx	%o0, [%l1+0*8]
	stx	%o1, [%l1+1*8]
	stx	%o2, [%l1+2*8]

	
	ldx	[%l0+3*8], %o3
	ldx	[%l0+4*8], %o4
	ldx	[%l0+5*8], %o5
	inc	6*8, %l0
	stx	%o3, [%l1+3*8]
	deccc	6*8, %l2
	stx	%o4, [%l1+4*8]
	stx	%o5, [%l1+5*8]
	bge,pt	%xcc, 1b
	 inc	6*8, %l1
2:
	inc	6*8, %l2
1:	
	deccc	8, %l2
	bl,pn	%icc, 1f				! < 0 --> sub word
	 nop
	ldx	[%l0], %g6
	inc	8, %l0
	stx	%g6, [%l1]
	bg,pt	%icc, 1b				! Exactly 0 --> done
	 inc	8, %l1
1:
	btst	7, %l2					! Done?
	bz,pt	%xcc, Lbcopy_complete
	 clr	%l4
	ldx	[%l0], %o0
Lbcopy_finish:
	
	brz,pn	%l2, 2f					! 100% complete?
	 cmp	%l2, 8					! Exactly 8 bytes?
	bz,a,pn	%xcc, 2f
	 stx	%o0, [%l1]

	btst	4, %l2					! Word store?
	bz	%xcc, 1f
	 srlx	%o0, 32, %g6				! Shift high word down
	stw	%g6, [%l1]
	inc	4, %l1
	mov	%o0, %g6				! Operate on the low bits
1:
	btst	2, %l2
	mov	%g6, %o0
	bz	1f
	 srlx	%o0, 16, %g6
	
	sth	%g6, [%l1]				! Store short
	inc	2, %l1
	mov	%o0, %g6				! Operate on low bytes
1:
	mov	%g6, %o0
	btst	1, %l2					! Byte aligned?
	bz	2f
	 srlx	%o0, 8, %g6

	stb	%g6, [%l1]				! Store last byte
	inc	1, %l1					! Update address
2:	
Lbcopy_complete:
	ret
	 restore %i1, %g0, %o0
	
/*
 * bzero(addr, len)
 *
 * XXXXX To produce more efficient code, we do not allow lengths
 * greater than 0x80000000000000000, which are negative numbers.
 * This should not really be an issue since the VA hole should
 * cause any such ranges to fail anyway.
 */
ENTRY(bzero)
	! %o0 = addr, %o1 = len
	mov	%o1, %o2
	clr	%o1			! Initialize our pattern
/*
 * memset(addr, c, len)
 *
 */
ENTRY(memset)
	! %o0 = addr, %o1 = pattern, %o2 = len
	mov	%o0, %o4		! Save original pointer

Lbzero_internal:
	btst	7, %o0			! Word aligned?
	bz,pn	%xcc, 0f
	 nop
	inc	%o0
	deccc	%o2			! Store up to 7 bytes
	bge,a,pt	%xcc, Lbzero_internal
	 stb	%o1, [%o0 - 1]

	retl				! Duplicate Lbzero_done
	 mov	%o4, %o0
0:
	/*
	 * Duplicate the pattern so it fills 64-bits.
	 */
	andcc	%o1, 0x0ff, %o1		! No need to extend zero
	bz,pt	%icc, 1f
	 sllx	%o1, 8, %o3		! sigh.  all dependent instructions.
	or	%o1, %o3, %o1
	sllx	%o1, 16, %o3
	or	%o1, %o3, %o1
	sllx	%o1, 32, %o3
	 or	%o1, %o3, %o1
1:	
	 deccc	8, %o2
Lbzero_longs:
	bl,pn	%xcc, Lbzero_cleanup	! Less than 8 bytes left
	 nop
3:	
	inc	8, %o0
	deccc	8, %o2
	bge,pt	%xcc, 3b
	 stx	%o1, [%o0 - 8]		! Do 1 longword at a time

	/*
	 * Len is in [-8..-1] where -8 => done, -7 => 1 byte to zero,
	 * -6 => two bytes, etc.  Mop up this remainder, if any.
	 */
Lbzero_cleanup:	
	btst	4, %o2
	bz,pt	%xcc, 5f		! if (len & 4) {
	 nop
	stw	%o1, [%o0]		!	*(int *)addr = 0;
	inc	4, %o0			!	addr += 4;
5:	
	btst	2, %o2
	bz,pt	%xcc, 7f		! if (len & 2) {
	 nop
	sth	%o1, [%o0]		!	*(short *)addr = 0;
	inc	2, %o0			!	addr += 2;
7:	
	btst	1, %o2
	bnz,a	%icc, Lbzero_done	! if (len & 1)
	 stb	%o1, [%o0]		!	*addr = 0;
Lbzero_done:
	retl
	 mov	%o4, %o0		! Restore pointer for memset (ugh)

/*
 * kcopy() is exactly like bcopy except that it set pcb_onfault such that
 * when a fault occurs, it is able to return EFAULT to indicate this to the
 * caller.
 */
ENTRY(kcopy)
	GET_CPCB(%o5)			! cpcb->pcb_onfault = Lkcerr;
	set	Lkcerr, %o3
	ldx	[%o5 + PCB_ONFAULT], %g1! save current onfault handler
	membar	#LoadStore
	stx	%o3, [%o5 + PCB_ONFAULT]
	membar	#StoreStore|#StoreLoad

	cmp	%o2, BCOPY_SMALL
Lkcopy_start:
	bge,a	Lkcopy_fancy	! if >= this many, go be fancy.
	 btst	7, %o0		! (part of being fancy)

	/*
	 * Not much to copy, just do it a byte at a time.
	 */
	deccc	%o2		! while (--len >= 0)
	bl	1f
!	 XXX check no delay slot
0:
	ldsb	[%o0], %o4	!	*dst++ = *src++;
	inc	%o0
	stb	%o4, [%o1]
	deccc	%o2
	bge	0b
	 inc	%o1
1:
	membar	#Sync		! Make sure all fauls are processed
	stx	%g1, [%o5 + PCB_ONFAULT]! restore fault handler
	membar	#StoreStore|#StoreLoad
	retl
	 clr	%o0
	NOTREACHED

	/*
	 * Plenty of data to copy, so try to do it optimally.
	 */
Lkcopy_fancy:
	! check for common case first: everything lines up.
!	btst	7, %o0		! done already
	bne	1f
!	 XXX check no delay slot
	btst	7, %o1
	be,a	Lkcopy_doubles
	 dec	8, %o2		! if all lined up, len -= 8, goto kcopy_doubes

	! If the low bits match, we can make these line up.
1:
	xor	%o0, %o1, %o3	! t = src ^ dst;
	btst	1, %o3		! if (t & 1) {
	be,a	1f
	 btst	1, %o0		! [delay slot: if (src & 1)]

	! low bits do not match, must copy by bytes.
0:
	ldsb	[%o0], %o4	!	do {
	inc	%o0		!		*dst++ = *src++;
	stb	%o4, [%o1]
	deccc	%o2
	bnz	0b		!	} while (--len != 0);
	 inc	%o1
	membar	#Sync		! Make sure all traps are taken
	stx	%g1, [%o5 + PCB_ONFAULT]! restore fault handler
	membar	#StoreStore|#StoreLoad
	retl
	 clr	%o0
	NOTREACHED

	! lowest bit matches, so we can copy by words, if nothing else
1:
	be,a	1f		! if (src & 1) {
	 btst	2, %o3		! [delay slot: if (t & 2)]

	! although low bits match, both are 1: must copy 1 byte to align
	ldsb	[%o0], %o4	!	*dst++ = *src++;
	inc	%o0
	stb	%o4, [%o1]
	dec	%o2		!	len--;
	inc	%o1
	btst	2, %o3		! } [if (t & 2)]
1:
	be,a	1f		! if (t & 2) {
	 btst	2, %o0		! [delay slot: if (src & 2)]
	dec	2, %o2		!	len -= 2;
0:
	ldsh	[%o0], %o4	!	do {
	inc	2, %o0		!		dst += 2, src += 2;
	sth	%o4, [%o1]	!		*(short *)dst = *(short *)src;
	deccc	2, %o2		!	} while ((len -= 2) >= 0);
	bge	0b
	 inc	2, %o1
	b	Lkcopy_mopb	!	goto mop_up_byte;
	 btst	1, %o2		! } [delay slot: if (len & 1)]
	NOTREACHED

	! low two bits match, so we can copy by longwords
1:
	be,a	1f		! if (src & 2) {
	 btst	4, %o3		! [delay slot: if (t & 4)]

	! although low 2 bits match, they are 10: must copy one short to align
	ldsh	[%o0], %o4	!	(*short *)dst = *(short *)src;
	inc	2, %o0		!	dst += 2;
	sth	%o4, [%o1]
	dec	2, %o2		!	len -= 2;
	inc	2, %o1		!	src += 2;
	btst	4, %o3		! } [if (t & 4)]
1:
	be,a	1f		! if (t & 4) {
	 btst	4, %o0		! [delay slot: if (src & 4)]
	dec	4, %o2		!	len -= 4;
0:
	ld	[%o0], %o4	!	do {
	inc	4, %o0		!		dst += 4, src += 4;
	st	%o4, [%o1]	!		*(int *)dst = *(int *)src;
	deccc	4, %o2		!	} while ((len -= 4) >= 0);
	bge	0b
	 inc	4, %o1
	b	Lkcopy_mopw	!	goto mop_up_word_and_byte;
	 btst	2, %o2		! } [delay slot: if (len & 2)]
	NOTREACHED

	! low three bits match, so we can copy by doublewords
1:
	be	1f		! if (src & 4) {
	 dec	8, %o2		! [delay slot: len -= 8]
	ld	[%o0], %o4	!	*(int *)dst = *(int *)src;
	inc	4, %o0		!	dst += 4, src += 4, len -= 4;
	st	%o4, [%o1]
	dec	4, %o2		! }
	inc	4, %o1
1:
Lkcopy_doubles:
	ldx	[%o0], %g5	! do {
	inc	8, %o0		!	dst += 8, src += 8;
	stx	%g5, [%o1]	!	*(double *)dst = *(double *)src;
	deccc	8, %o2		! } while ((len -= 8) >= 0);
	bge	Lkcopy_doubles
	 inc	8, %o1

	! check for a usual case again (save work)
	btst	7, %o2		! if ((len & 7) == 0)
	be	Lkcopy_done	!	goto kcopy_done;

	 btst	4, %o2		! if ((len & 4) == 0)
	be,a	Lkcopy_mopw	!	goto mop_up_word_and_byte;
	 btst	2, %o2		! [delay slot: if (len & 2)]
	ld	[%o0], %o4	!	*(int *)dst = *(int *)src;
	inc	4, %o0		!	dst += 4;
	st	%o4, [%o1]
	inc	4, %o1		!	src += 4;
	btst	2, %o2		! } [if (len & 2)]

1:
	! mop up trailing word (if present) and byte (if present).
Lkcopy_mopw:
	be	Lkcopy_mopb	! no word, go mop up byte
	 btst	1, %o2		! [delay slot: if (len & 1)]
	ldsh	[%o0], %o4	! *(short *)dst = *(short *)src;
	be	Lkcopy_done	! if ((len & 1) == 0) goto done;
	 sth	%o4, [%o1]
	ldsb	[%o0 + 2], %o4	! dst[2] = src[2];
	stb	%o4, [%o1 + 2]
	membar	#Sync		! Make sure all traps are taken
	stx	%g1, [%o5 + PCB_ONFAULT]! restore fault handler
	membar	#StoreStore|#StoreLoad
	retl
	 clr	%o0
	NOTREACHED

	! mop up trailing byte (if present).
Lkcopy_mopb:
	bne,a	1f
	 ldsb	[%o0], %o4

Lkcopy_done:
	membar	#Sync		! Make sure all traps are taken
	stx	%g1, [%o5 + PCB_ONFAULT]! restore fault handler
	membar	#StoreStore|#StoreLoad
	retl
	 clr	%o0
	NOTREACHED

1:
	stb	%o4, [%o1]
	membar	#Sync		! Make sure all traps are taken
	stx	%g1, [%o5 + PCB_ONFAULT]! restore fault handler
	membar	#StoreStore|#StoreLoad
	retl
	 clr	%o0
	NOTREACHED

Lkcerr:
	stx	%g1, [%o5 + PCB_ONFAULT]! restore fault handler
	membar	#StoreStore|#StoreLoad
	retl				! and return error indicator
	 mov	EFAULT, %o0
	NOTREACHED

ENTRY(memmove) /* dest, src, size */
	/*
	 * Swap args and continue to ovbcopy.
	 */
	mov	%o0, %o3
	mov	%o1, %o0
	mov	%o3, %o1
/*
 * ovbcopy(src, dst, len): like bcopy, but regions may overlap.
 */
ENTRY(ovbcopy)
	cmp	%o0, %o1	! src < dst?
	bgeu	Lbcopy_start	! no, go copy forwards as via bcopy
	 cmp	%o2, BCOPY_SMALL! (check length for doublecopy first)

	/*
	 * Since src comes before dst, and the regions might overlap,
	 * we have to do the copy starting at the end and working backwards.
	 */
	add	%o2, %o0, %o0	! src += len
	add	%o2, %o1, %o1	! dst += len
	bge,a	Lback_fancy	! if len >= BCOPY_SMALL, go be fancy
	 btst	3, %o0

	/*
	 * Not much to copy, just do it a byte at a time.
	 */
	deccc	%o2		! while (--len >= 0)
	bl	1f
!	 XXX check no delay slot
0:
	dec	%o0		!	*--dst = *--src;
	ldsb	[%o0], %o4
	dec	%o1
	deccc	%o2
	bge	0b
	 stb	%o4, [%o1]
1:
	retl
	 nop

	/*
	 * Plenty to copy, try to be optimal.
	 * We only bother with word/halfword/byte copies here.
	 */
Lback_fancy:
!	btst	3, %o0		! done already
	bnz	1f		! if ((src & 3) == 0 &&
	 btst	3, %o1		!     (dst & 3) == 0)
	bz,a	Lback_words	!	goto words;
	 dec	4, %o2		! (done early for word copy)

1:
	/*
	 * See if the low bits match.
	 */
	xor	%o0, %o1, %o3	! t = src ^ dst;
	btst	1, %o3
	bz,a	3f		! if (t & 1) == 0, can do better
	 btst	1, %o0

	/*
	 * Nope; gotta do byte copy.
	 */
2:
	dec	%o0		! do {
	ldsb	[%o0], %o4	!	*--dst = *--src;
	dec	%o1
	deccc	%o2		! } while (--len != 0);
	bnz	2b
	 stb	%o4, [%o1]
	retl
	 nop

3:
	/*
	 * Can do halfword or word copy, but might have to copy 1 byte first.
	 */
!	btst	1, %o0		! done earlier
	bz,a	4f		! if (src & 1) {	/* copy 1 byte */
	 btst	2, %o3		! (done early)
	dec	%o0		!	*--dst = *--src;
	ldsb	[%o0], %o4
	dec	%o1
	stb	%o4, [%o1]
	dec	%o2		!	len--;
	btst	2, %o3		! }

4:
	/*
	 * See if we can do a word copy ((t&2) == 0).
	 */
!	btst	2, %o3		! done earlier
	bz,a	6f		! if (t & 2) == 0, can do word copy
	 btst	2, %o0		! (src&2, done early)

	/*
	 * Gotta do halfword copy.
	 */
	dec	2, %o2		! len -= 2;
5:
	dec	2, %o0		! do {
	ldsh	[%o0], %o4	!	src -= 2;
	dec	2, %o1		!	dst -= 2;
	deccc	2, %o2		!	*(short *)dst = *(short *)src;
	bge	5b		! } while ((len -= 2) >= 0);
	 sth	%o4, [%o1]
	b	Lback_mopb	! goto mop_up_byte;
	 btst	1, %o2		! (len&1, done early)

6:
	/*
	 * We can do word copies, but we might have to copy
	 * one halfword first.
	 */
!	btst	2, %o0		! done already
	bz	7f		! if (src & 2) {
	 dec	4, %o2		! (len -= 4, done early)
	dec	2, %o0		!	src -= 2, dst -= 2;
	ldsh	[%o0], %o4	!	*(short *)dst = *(short *)src;
	dec	2, %o1
	sth	%o4, [%o1]
	dec	2, %o2		!	len -= 2;
				! }

7:
Lback_words:
	/*
	 * Do word copies (backwards), then mop up trailing halfword
	 * and byte if any.
	 */
!	dec	4, %o2		! len -= 4, done already
0:				! do {
	dec	4, %o0		!	src -= 4;
	dec	4, %o1		!	src -= 4;
	ld	[%o0], %o4	!	*(int *)dst = *(int *)src;
	deccc	4, %o2		! } while ((len -= 4) >= 0);
	bge	0b
	 st	%o4, [%o1]

	/*
	 * Check for trailing shortword.
	 */
	btst	2, %o2		! if (len & 2) {
	bz,a	1f
	 btst	1, %o2		! (len&1, done early)
	dec	2, %o0		!	src -= 2, dst -= 2;
	ldsh	[%o0], %o4	!	*(short *)dst = *(short *)src;
	dec	2, %o1
	sth	%o4, [%o1]	! }
	btst	1, %o2

	/*
	 * Check for trailing byte.
	 */
1:
Lback_mopb:
!	btst	1, %o2		! (done already)
	bnz,a	1f		! if (len & 1) {
	 ldsb	[%o0 - 1], %o4	!	b = src[-1];
	retl
	 nop
1:
	retl			!	dst[-1] = b;
	 stb	%o4, [%o1 - 1]	! }


/*
 * clearfpstate()
 *
 * Drops the current fpu state, without saving it.
 */
ENTRY(clearfpstate)
	rdpr	%pstate, %o1		! enable FPU
	wr	%g0, FPRS_FEF, %fprs
	or	%o1, PSTATE_PEF, %o1
	retl
	 wrpr	%o1, 0, %pstate

/*
 * savefpstate(struct fpstate *f)
 *
 * Store the current FPU state.
 *
 * Since the kernel may need to use the FPU and we have problems atomically
 * testing and enabling the FPU, we leave here with the FPRS_FEF bit set.
 * Normally this should be turned on in loadfpstate().
 */
 /* XXXXXXXXXX  Assume caller created a proper stack frame */
ENTRY(savefpstate)
	rdpr	%pstate, %o1		! enable FP before we begin
	rd	%fprs, %o5
	wr	%g0, FPRS_FEF, %fprs
	or	%o1, PSTATE_PEF, %o1
	wrpr	%o1, 0, %pstate

	stx	%fsr, [%o0 + FS_FSR]	! f->fs_fsr = getfsr();

	rd	%gsr, %o4		! Save %gsr
	st	%o4, [%o0 + FS_GSR]

	add	%o0, FS_REGS, %o2	! This is zero...
	btst	FPRS_DL, %o5		! Lower FPU clean?
	bz,a,pt	%icc, 1f		! Then skip it
	 add	%o2, 128, %o2		! Skip a block

	membar	#Sync
	stda	%f0, [%o2] ASI_BLK_P	! f->fs_f0 = etc;
	inc	BLOCK_SIZE, %o2
	stda	%f16, [%o2] ASI_BLK_P
	inc	BLOCK_SIZE, %o2
1:
	btst	FPRS_DU, %o5		! Upper FPU clean?
	bz,pt	%icc, 2f		! Then skip it
	 nop

	membar	#Sync
	stda	%f32, [%o2] ASI_BLK_P
	inc	BLOCK_SIZE, %o2
	stda	%f48, [%o2] ASI_BLK_P
2:
	membar	#Sync			! Finish operation so we can
	retl
	 wr	%g0, FPRS_FEF, %fprs	! Mark FPU clean

/*
 * Load FPU state.
 */
 /* XXXXXXXXXX  Should test to see if we only need to do a partial restore */
ENTRY(loadfpstate)
	rdpr	%pstate, %o1		! enable FP before we begin
	ld	[%o0 + FS_GSR], %o4	! Restore %gsr
	set	PSTATE_PEF, %o2
	wr	%g0, FPRS_FEF, %fprs
	or	%o1, %o2, %o1
	wrpr	%o1, 0, %pstate
	ldx	[%o0 + FS_FSR], %fsr	! setfsr(f->fs_fsr);
	add	%o0, FS_REGS, %o3	! This is zero...
	wr	%o4, %g0, %gsr
	membar	#Sync
	ldda	[%o3] ASI_BLK_P, %f0
	inc	BLOCK_SIZE, %o3
	ldda	[%o3] ASI_BLK_P, %f16
	inc	BLOCK_SIZE, %o3
	ldda	[%o3] ASI_BLK_P, %f32
	inc	BLOCK_SIZE, %o3
	ldda	[%o3] ASI_BLK_P, %f48
	membar	#Sync			! Make sure loads are complete
	retl
	 wr	%g0, FPRS_FEF, %fprs	! Clear dirty bits

/* XXX belongs elsewhere (ctlreg.h?) */
#define	AFSR_CECC_ERROR		0x100000	/* AFSR Correctable ECC err */
#define	DATAPATH_CE		0x100		/* Datapath Correctable Err */

	.data
	_ALIGN
	.globl	_C_LABEL(cecclast), _C_LABEL(ceccerrs)
_C_LABEL(cecclast):
	.xword 0
_C_LABEL(ceccerrs):
	.word 0
	_ALIGN
	.text

/*
 * ECC Correctable Error handler - this doesn't do much except intercept
 * the error and reset the status bits.
 */
ENTRY(cecc_catch)
	ldxa	[%g0] ASI_AFSR, %g1			! g1 = AFSR
	ldxa	[%g0] ASI_AFAR, %g2			! g2 = AFAR

	sethi	%hi(cecclast), %g1			! cecclast = AFAR
	or	%g1, %lo(cecclast), %g1
	stx	%g2, [%g1]

	sethi	%hi(ceccerrs), %g1			! get current count
	or	%g1, %lo(ceccerrs), %g1
	lduw	[%g1], %g2				! g2 = ceccerrs

	ldxa	[%g0] ASI_DATAPATH_ERR_REG_READ, %g3	! Read UDB-Low status
	andcc	%g3, DATAPATH_CE, %g4			! Check CE bit
	be,pn	%xcc, 1f				! Don't clear unless
	 nop						!  necessary
	stxa	%g4, [%g0] ASI_DATAPATH_ERR_REG_WRITE	! Clear CE bit in UDBL
	membar	#Sync					! sync store
	inc	%g2					! ceccerrs++
1:	mov	0x18, %g5
	ldxa	[%g5] ASI_DATAPATH_ERR_REG_READ, %g3	! Read UDB-High status
	andcc	%g3, DATAPATH_CE, %g4			! Check CE bit
	be,pn	%xcc, 1f				! Don't clear unless
	 nop						!  necessary
	stxa	%g4, [%g5] ASI_DATAPATH_ERR_REG_WRITE	! Clear CE bit in UDBH
	membar	#Sync					! sync store
	inc	%g2					! ceccerrs++
1:	set	AFSR_CECC_ERROR, %g3
	stxa	%g3, [%g0] ASI_AFSR			! Clear CE in AFSR
	stw	%g2, [%g1]				! set ceccerrs
	membar	#Sync					! sync store
        CLRTT
        retry
        NOTREACHED

/*
 * send_softint(cpu, level, intrhand)
 *
 * Send a softint with an intrhand pointer so we can cause a vectored
 * interrupt instead of a polled interrupt.  This does pretty much the
 * same as interrupt_vector.  If intrhand is NULL then it just sends
 * a polled interrupt.  If cpu is -1 then send it to this CPU, if it's
 * -2 send it to any CPU, otherwise send it to a particular CPU.
 *
 * XXXX Dispatching to different CPUs is not implemented yet.
 */
ENTRY(send_softint)
	rdpr	%pstate, %g1
	andn	%g1, PSTATE_IE, %o3
	wrpr	%o3, 0, %pstate

	brz,pn	%o2, 1f
	 add	%g7, CI_INTRPENDING, %o3

	ldx	[%o2 + IH_PEND], %o5
	brnz,pn	%o5, 1f
	 sll	%o1, 3+3, %o5	! Find start of table for this IPL
	add	%o3, %o5, %o3

	ldx	[%o3], %o5		! Load list head
	add	%o2, IH_PEND, %o4
	casxa	[%o4] ASI_N, %g0, %o5
	brnz,pn	%o5, 1f
	 nop
	stx	%o2, [%o3]

	mov	1, %o3			! Change from level to bitmask
	sllx	%o3, %o1, %o3
	wr	%o3, 0, SET_SOFTINT	! SET_SOFTINT
1:
	retl
	 wrpr	%g1, 0, %pstate		! restore interrupts

/*
 * Flush user windows to memory.
 */
ENTRY(write_user_windows)
	rdpr	%otherwin, %g1
	brz	%g1, 3f
	clr	%g2
1:
	save	%sp, -CC64FSZ, %sp
	rdpr	%otherwin, %g1
	brnz	%g1, 1b
	 inc	%g2
2:
	dec	%g2
	brnz	%g2, 2b
	 restore
3:
	retl
	 nop

/*
 * Clear the Nonpriviliged Trap (NPT( bit of %tick such that it can be
 * read from userland.  This requires us to read the current value and
 * write it back with the bit cleared.  As a result we will lose a
 * couple of ticks.  In order to limit the number of lost ticks, we
 * block interrupts and make sure the instructions to read and write
 * %tick live in the same cache line.  We tag on an extra read to work
 * around a Blackbird (UltraSPARC-II) errata (see below).
 */
ENTRY(tick_enable)
	rdpr	%pstate, %o0
	andn	%o0, PSTATE_IE, %o1
	wrpr	%o1, 0, %pstate		! disable interrupts
	rdpr	%tick, %o2
	brgez,pn %o2, 1f
	 clr	%o1
	mov	1, %o1
	sllx	%o1, 63, %o1
	ba,pt	%xcc, 1f
	 nop
	.align	64
1:	rdpr	%tick, %o2
	wrpr	%o2, %o1, %tick
	rdpr	%tick, %g0

	retl
	 wrpr	%o0, 0, %pstate		! restore interrupts

/*
 * On Blackbird (UltraSPARC-II) CPUs, writes to %tick_cmpr may fail.
 * The workaround is to do a read immediately after the write and make
 * sure those two instructions are in the same cache line.
 */
ENTRY(tickcmpr_set)
	ba	1f
	 mov	8, %o2			! Initial step size
	.align	64
1:	wr	%o0, 0, %tick_cmpr
	rd	%tick_cmpr, %g0

	rd	%tick, %o1		! Read current %tick
	sllx	%o1, 1, %o1
	srlx	%o1, 1, %o1

	cmp	%o0, %o1		! Make sure the value we wrote to
	bg,pt	%xcc, 2f		!   %tick_cmpr was in the future.
	 add	%o0, %o2, %o0		! If not, add the step size, double
	ba,pt	%xcc, 1b		!   the step size and try again.
	 sllx	%o2, 1, %o2
2:
	retl
	 nop

ENTRY(sys_tickcmpr_set)
	ba	1f
	 mov	8, %o2			! Initial step size
	.align	64
1:	wr	%o0, 0, %sys_tick_cmpr
	rd	%sys_tick_cmpr, %g0

	rd	%sys_tick, %o1		! Read current %sys_tick
	sllx	%o1, 1, %o1
	srlx	%o1, 1, %o1

	cmp	%o0, %o1		! Make sure the value we wrote to
	bg,pt	%xcc, 2f		!   %sys_tick_cmpr was in the future.
	 add	%o0, %o2, %o0		! If not, add the step size, double
	ba,pt	%xcc, 1b		!   the step size and try again.
	 sllx	%o2, 1, %o2
2:
	retl
	 nop

/*
 * Support for the STICK logic found on the integrated PCI host bridge
 * of Hummingbird (UltraSPARC-IIe).  The chip designers made the
 * brilliant decision to split the 64-bit counters into two 64-bit
 * aligned 32-bit registers, making atomic access impossible.  This
 * means we have to check for wraparound in various places.  Sigh.
 */

#define STICK_CMP_LOW	0x1fe0000f060
#define STICK_CMP_HIGH	0x1fe0000f068
#define STICK_REG_LOW	0x1fe0000f070
#define STICK_REG_HIGH	0x1fe0000f078

ENTRY(stick)
	setx	STICK_REG_LOW, %o1, %o3
0:
	ldxa	[%o3] ASI_PHYS_NON_CACHED, %o0
	add	%o3, (STICK_REG_HIGH - STICK_REG_LOW), %o4
	ldxa	[%o4] ASI_PHYS_NON_CACHED, %o1
	ldxa	[%o3] ASI_PHYS_NON_CACHED, %o2
	cmp	%o2, %o0		! Check for wraparound
	blu,pn	%icc, 0b
	 sllx	%o1, 33, %o1		! Clear the MSB
	srlx	%o1, 1, %o1
	retl
	 or	%o2, %o1, %o0

ENTRY(stickcmpr_set)
	setx	STICK_CMP_HIGH, %o1, %o3
	mov	8, %o2			! Initial step size
1:
	srlx	%o0, 32, %o1
	stxa	%o1, [%o3] ASI_PHYS_NON_CACHED
	add	%o3, (STICK_CMP_LOW - STICK_CMP_HIGH), %o4
	stxa	%o0, [%o4] ASI_PHYS_NON_CACHED

	add	%o3, (STICK_REG_LOW - STICK_CMP_HIGH), %o4
	ldxa	[%o4] ASI_PHYS_NON_CACHED, %o1
	add	%o3, (STICK_REG_HIGH - STICK_CMP_HIGH), %o4
	ldxa	[%o4] ASI_PHYS_NON_CACHED, %o5
	sllx	%o5, 32, %o5
	or	%o1, %o5, %o1

	cmp	%o0, %o1		! Make sure the value we wrote
	bg,pt	%xcc, 2f		!   was in the future
	 add	%o0, %o2, %o0		! If not, add the step size, double
	ba,pt	%xcc, 1b		!   the step size and try again.
	 sllx	%o2, 1, %o2
2:
	retl
	 nop

#define MICROPERSEC	(1000000)
	.data
	.align	16
	.globl	_C_LABEL(cpu_clockrate)
_C_LABEL(cpu_clockrate):
	!! Pretend we have a 200MHz clock -- cpu_attach will fix this
	.xword	200000000
	!! Here we'll store cpu_clockrate/1000000 so we can calculate usecs
	.xword	0
	.text

/*
 * delay function
 *
 * void delay(N)  -- delay N microseconds
 *
 * Register usage: %o0 = "N" number of usecs to go (counts down to zero)
 *		   %o1 = "timerblurb" (stays constant)
 *		   %o2 = counter for 1 usec (counts down from %o1 to zero)
 *
 *
 *	cpu_clockrate should be tuned during CPU probe to the CPU clockrate in Hz
 *
 */
ENTRY(delay)			! %o0 = n
	rdpr	%tick, %o1					! Take timer snapshot
	sethi	%hi(_C_LABEL(cpu_clockrate)), %o2
	sethi	%hi(MICROPERSEC), %o3
	ldx	[%o2 + %lo(_C_LABEL(cpu_clockrate) + 8)], %o4	! Get scale factor
	brnz,pt	%o4, 0f
	 or	%o3, %lo(MICROPERSEC), %o3

	!! Calculate ticks/usec
	ldx	[%o2 + %lo(_C_LABEL(cpu_clockrate))], %o4	! No, we need to calculate it
	udivx	%o4, %o3, %o4
	stx	%o4, [%o2 + %lo(_C_LABEL(cpu_clockrate) + 8)]	! Save it so we don't need to divide again
0:

	mulx	%o0, %o4, %o0					! Convert usec -> ticks
	rdpr	%tick, %o2					! Top of next itr
1:
	sub	%o2, %o1, %o3					! How many ticks have gone by?
	sub	%o0, %o3, %o4					! Decrement count by that much
	movrgz	%o3, %o4, %o0					! But only if we're decrementing
	mov	%o2, %o1					! Remember last tick
	brgz,pt	%o0, 1b						! Done?
	 rdpr	%tick, %o2					! Get new tick

	retl
	 nop

ENTRY(setjmp)
	save	%sp, -CC64FSZ, %sp	! Need a frame to return to.
	flushw
	stx	%fp, [%i0+0]	! 64-bit stack pointer
	stx	%i7, [%i0+8]	! 64-bit return pc
	ret
	 restore	%g0, 0, %o0

ENTRY(longjmp)
	save	%sp, -CC64FSZ, %sp	! prepare to restore to (old) frame
	flushw
	mov	1, %i2
	ldx	[%i0+0], %fp	! get return stack
	ldx	[%i0+8], %i7	! get rpc
	ret
	 restore	%i2, 0, %o0

#ifdef DDB
	/*
	 * Debug stuff.  Dump the trap registers into buffer & set tl=0.
	 *
	 *  %o0 = *ts
	 */
	ENTRY(savetstate)
	mov	%o0, %o1
	CHKPT %o4,%o3,0x28
	rdpr	%tl, %o0
	brz	%o0, 2f
	 mov	%o0, %o2
1:
	rdpr	%tstate, %o3
	stx	%o3, [%o1]
	deccc	%o2
	inc	8, %o1
	rdpr	%tpc, %o4
	stx	%o4, [%o1]
	inc	8, %o1
	rdpr	%tnpc, %o5
	stx	%o5, [%o1]
	inc	8, %o1
	rdpr	%tt, %o4
	stx	%o4, [%o1]
	inc	8, %o1
	bnz	1b
	 wrpr	%o2, 0, %tl
2:
	retl
	 nop

	/*
	 * Debug stuff.  Resore trap registers from buffer.
	 *
	 *  %o0 = %tl
	 *  %o1 = *ts
	 *
	 * Maybe this should be re-written to increment tl instead of decrementing.
	 */
	ENTRY(restoretstate)
	CHKPT %o4,%o3,0x36
	flushw			! Make sure we don't have stack probs & lose hibits of %o
	brz,pn	%o0, 2f
	 mov	%o0, %o2
	CHKPT %o4,%o3,0x29
	wrpr	%o0, 0, %tl
1:
	ldx	[%o1], %o3
	deccc	%o2
	inc	8, %o1
	wrpr	%o3, 0, %tstate
	ldx	[%o1], %o4
	inc	8, %o1
	wrpr	%o4, 0, %tpc
	ldx	[%o1], %o5
	inc	8, %o1
	wrpr	%o5, 0, %tnpc
	ldx	[%o1], %o4
	inc	8, %o1
	wrpr	%o4, 0, %tt
	bnz	1b
	 wrpr	%o2, 0, %tl
2:
	CHKPT %o4,%o3,0x30
	retl
	 wrpr	%o0, 0, %tl

	/*
	 * Switch to context in %o0
	 */
	ENTRY(switchtoctx)
	set	DEMAP_CTX_SECONDARY, %o3
	stxa	%o3, [%o3] ASI_DMMU_DEMAP
	membar	#Sync
	mov	CTX_SECONDARY, %o4
	stxa	%o3, [%o3] ASI_IMMU_DEMAP
	membar	#Sync
	stxa	%o0, [%o4] ASI_DMMU		! Maybe we should invalidate the old context?
	membar	#Sync				! No real reason for this XXXX
	sethi	%hi(KERNBASE), %o2
	flush	%o2
	retl
	 nop

#endif /* DDB */	/* DDB */

	.data
	_ALIGN
#if defined(DDB) || NKSYMS > 0
	.globl	_C_LABEL(esym)
_C_LABEL(esym):
	.xword	0
	.globl	_C_LABEL(ssym)
_C_LABEL(ssym):
	.xword	0
#endif	/* defined(DDB) || NKSYMS > 0 */
	.globl	_C_LABEL(proc0paddr)
_C_LABEL(proc0paddr):
	.xword	_C_LABEL(u0)		! KVA of proc0 uarea

#ifdef DEBUG
	.comm	_C_LABEL(trapdebug), 4
	.comm	_C_LABEL(pmapdebug), 4
#endif	/* DEBUG */

	.globl	_C_LABEL(dlflush_start)
_C_LABEL(dlflush_start):
	.xword	dlflush1
	.xword	dlflush2
	.xword	dlflush3
	.xword	dlflush4
	.xword	dlflush5
	.xword	0
