/*	$OpenBSD: locore.s,v 1.94 2013/06/13 19:33:59 deraadt Exp $	*/
/*	$NetBSD: locore.s,v 1.73 1997/09/13 20:36:48 pk Exp $	*/

/*
 * Copyright (c) 1996 Paul Kranenburg
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
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
 *	This product includes software developed by Harvard University.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by Paul Kranenburg.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)locore.s	8.4 (Berkeley) 12/10/93
 */

#include "assym.h"
#include "ksyms.h"
#include <machine/param.h>
#include <machine/asm.h>
#include <sparc/sparc/intreg.h>
#include <sparc/sparc/timerreg.h>
#include <sparc/sparc/vaddrs.h>
#ifdef notyet
#include <sparc/dev/zsreg.h>
#endif
#include <machine/ctlreg.h>
#include <machine/psl.h>
#include <machine/signal.h>
#include <machine/trap.h>

/*
 * GNU assembler does not understand `.empty' directive; Sun assembler
 * gripes about labels without it.  To allow cross-compilation using
 * the Sun assembler, and because .empty directives are useful documentation,
 * we use this trick.
 */
#ifdef SUN_AS
#define	EMPTY	.empty
#else
#define	EMPTY	/* .empty */
#endif

/* use as needed to align things on longword boundaries */
#define	_ALIGN	.align 4

/*
 * CCFSZ (C Compiler Frame SiZe) is the size of a stack frame required if
 * a function is to call C code.  It should be just 64, but Sun defined
 * their frame with space to hold arguments 0 through 5 (plus some junk),
 * and varargs routines (such as printf) demand this, and gcc uses this
 * area at times anyway.
 */
#define	CCFSZ	96

/*
 * A handy macro for maintaining instrumentation counters.
 * Note that this clobbers %o0 and %o1.  Normal usage is
 * something like:
 *	foointr:
 *		TRAP_SETUP(...)		! makes %o registers safe
 *		INCR(_C_LABEL(cnt)+V_FOO)	! count a foo
 */
#define INCR(what) \
	sethi	%hi(what), %o0; \
	ld	[%o0 + %lo(what)], %o1; \
	inc	%o1; \
	st	%o1, [%o0 + %lo(what)]

/*
 * Another handy macro: load one register window, given `base' address.
 * This can be either a simple register (e.g., %sp) or include an initial
 * offset (e.g., %g6 + PCB_RW).
 */
#define	LOADWIN(addr) \
	ldd	[addr], %l0; \
	ldd	[addr + 8], %l2; \
	ldd	[addr + 16], %l4; \
	ldd	[addr + 24], %l6; \
	ldd	[addr + 32], %i0; \
	ldd	[addr + 40], %i2; \
	ldd	[addr + 48], %i4; \
	ldd	[addr + 56], %i6

/*
 * To return from trap we need the two-instruction sequence
 * `jmp %l1; rett %l2', which is defined here for convenience.
 */
#define	RETT	jmp %l1; rett %l2

	.data
/*
 * The interrupt stack.
 *
 * This is the very first thing in the data segment, and therefore has
 * the lowest kernel stack address.  We count on this in the interrupt
 * trap-frame setup code, since we may need to switch from the kernel
 * stack to the interrupt stack (iff we are not already on the interrupt
 * stack).  One sethi+cmp is all we need since this is so carefully
 * arranged.
 */
	.globl	_C_LABEL(intstack)
	.globl	_C_LABEL(eintstack)
_C_LABEL(intstack):
	.skip	128 * 128		! 16k = 128 128-byte stack frames
_C_LABEL(eintstack):

/*
 * When a process exits and its u. area goes away, we set cpcb to point
 * to this `u.', leaving us with something to use for an interrupt stack,
 * and letting all the register save code have a pcb_uw to examine.
 * This is also carefully arranged (to come just before u0, so that
 * process 0's kernel stack can quietly overrun into it during bootup, if
 * we feel like doing that).
 */
	.globl	_C_LABEL(idle_u)
_C_LABEL(idle_u):
	.skip	USPACE

/*
 * Process 0's u.
 *
 * This must be aligned on an 8 byte boundary.
 */
	.globl	_C_LABEL(u0)
_C_LABEL(u0):	.skip	USPACE
estack0:

#ifdef KGDB
/*
 * Another item that must be aligned, easiest to put it here.
 */
KGDB_STACK_SIZE = 2048
	.globl	_C_LABEL(kgdb_stack)
_C_LABEL(kgdb_stack):
	.skip	KGDB_STACK_SIZE		! hope this is enough
#endif

/*
 * cpcb points to the current pcb (and hence u. area).
 * Initially this is the special one.
 */
	.globl	_C_LABEL(cpcb)
_C_LABEL(cpcb):	.word	_C_LABEL(u0)

curproc = CPUINFO_VA + CPUINFO_CURPROC
/*
 * cputyp is the current cpu type, used to distinguish between
 * the many variations of different sun4* machines. It contains
 * the value CPU_SUN4, CPU_SUN4C, CPU_SUN4D, CPU_SUN4E or CPU_SUN4M.
 */
	.globl	_C_LABEL(cputyp)
_C_LABEL(cputyp):
	.word	CPU_SUN4C
/*
 * cpumod is the current cpu model, used to distinguish between variants
 * in the Sun4 and Sun4M families. See /sys/arch/sparc/include/param.h for
 * possible values.
 */
	.globl	_C_LABEL(cpumod)
_C_LABEL(cpumod):
	.word	1
/*
 * mmumod is the current mmu model, used to distinguish between the
 * various implementations of the SRMMU in the sun4m family of machines.
 * See /sys/arch/sparc/include/param.h for possible values.
 */
	.globl	_C_LABEL(mmumod)
_C_LABEL(mmumod):
	.word	0

#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
_C_LABEL(cputypval):
	.asciz	"sun4c"
	.ascii	"     "
_C_LABEL(cputypvar):
	.asciz	"compatible"
_C_LABEL(cputypvallen) = _C_LABEL(cputypvar) - _C_LABEL(cputypval)
	_ALIGN
#endif

/*
 * nbpg is used by pmap_bootstrap(), pgofset is used internally.
 */
	.globl	_C_LABEL(nbpg)
_C_LABEL(nbpg):
	.word	0
	.globl	_C_LABEL(pgofset)
_C_LABEL(pgofset):
	.word	0

	.globl	_C_LABEL(trapbase)
_C_LABEL(trapbase):
	.word	0

#if !defined(SUN4M)
sun4m_notsup:
	.asciz	"cr .( OpenBSD/sparc: this kernel does not support the sun4m) cr"
#endif
#if !defined(SUN4D)
sun4d_notsup:
	.asciz	"cr .( OpenBSD/sparc: this kernel does not support the sun4d) cr"
#endif
#if !(defined(SUN4C) || defined(SUN4E))
sun4c_notsup:
	.asciz	"cr .( OpenBSD/sparc: this kernel does not support the sun4c) cr"
#endif
#if !defined(SUN4)
sun4_notsup:
	! the extra characters at the end are to ensure the zs fifo drains
	! before we halt. Sick, eh?
	.asciz	"OpenBSD/sparc: this kernel does not support the sun4\n\r \b"
#endif
	_ALIGN

	.text

/*
 * The first thing in the real text segment is the trap vector table,
 * which must be aligned on a 4096 byte boundary.  The text segment
 * starts beyond page 0 of KERNBASE so that there is a red zone
 * between user and kernel space.  Since the boot ROM loads us at
 * 0x4000, it is far easier to start at KERNBASE+0x4000 than to
 * buck the trend.  This is two or four pages in (depending on if
 * pagesize is 8192 or 4096).    We place two items in this area:
 * the message buffer (phys addr 0) and the IE_reg (phys addr 0x2000).
 * because the message buffer is in our "red zone" between user and
 * kernel space we remap it in configure() to another location and
 * invalidate the mapping at KERNBASE.
 */

/*
 * Each trap has room for four instructions, of which one perforce must
 * be a branch.  On entry the hardware has copied pc and npc to %l1 and
 * %l2 respectively.  We use two more to read the psr into %l0, and to
 * put the trap type value into %l3 (with a few exceptions below).
 * We could read the trap type field of %tbr later in the code instead,
 * but there is no need, and that would require more instructions
 * (read+mask, vs 1 `mov' here).
 *
 * I used to generate these numbers by address arithmetic, but gas's
 * expression evaluator has about as much sense as your average slug
 * (oddly enough, the code looks about as slimy too).  Thus, all the
 * trap numbers are given as arguments to the trap macros.  This means
 * there is one line per trap.  Sigh.
 *
 * Note that only the local registers may be used, since the trap
 * window is potentially the last window.  Its `in' registers are
 * the previous window's outs (as usual), but more important, its
 * `out' registers may be in use as the `topmost' window's `in' registers.
 * The global registers are of course verboten (well, until we save
 * them away).
 *
 * Hardware interrupt vectors can be `linked'---the linkage is to regular
 * C code---or rewired to fast in-window handlers.  The latter are good
 * for unbuffered hardware like the Zilog serial chip and the AMD audio
 * chip, where many interrupts can be handled trivially with pseudo-DMA or
 * similar.  Only one `fast' interrupt can be used per level, however, and
 * direct and `fast' interrupts are incompatible.  Routines in intr.c
 * handle setting these, with optional paranoia.
 */

	/* regular vectored traps */
#define	VTRAP(type, label) \
	rd %wim, %l0; mov (type), %l3; b label; mov %psr, %l0

	/* hardware interrupts (can be linked or made `fast') */
#define	HARDINT44C(lev) \
	mov (lev), %l3; b _C_LABEL(sparc_interrupt44c); mov %psr, %l0; nop

	/* hardware interrupts (can be linked or made `fast') */
#define	HARDINT4M(lev) \
	mov (lev), %l3; b _C_LABEL(sparc_interrupt4m); mov %psr, %l0; nop

	/* software interrupts (may not be made direct, sorry---but you
	   should not be using them trivially anyway) */
#define	SOFTINT44C(lev, bit) \
	mov (lev), %l3; mov (bit), %l4; b softintr_sun44c; mov %psr, %l0

	/* There's no SOFTINT4M(): both hard and soft vector the same way */

	/* traps that just call trap() */
#define	TRAP(type)	VTRAP(type, slowtrap)

	/* architecturally undefined traps (cause panic) */
#define	UTRAP(type)	VTRAP(type, slowtrap)

	/* software undefined traps (may be replaced) */
#define	STRAP(type)	VTRAP(type, slowtrap)

/* breakpoint acts differently under kgdb */
#ifdef KGDB
#define	BPT		VTRAP(T_BREAKPOINT, bpt)
#define	BPT_KGDB_EXEC	VTRAP(T_KGDB_EXEC, bpt)
#else
#define	BPT		TRAP(T_BREAKPOINT)
#define	BPT_KGDB_EXEC	TRAP(T_KGDB_EXEC)
#endif

/* special high-speed 1-instruction-shaved-off traps (get nothing in %l3) */
#define	SYSCALL		b _C_LABEL(_syscall); mov %psr, %l0; nop; nop
#define	WINDOW_OF	b window_of; mov %psr, %l0; nop; nop
#define	WINDOW_UF	b window_uf; mov %psr, %l0; nop; nop
#ifdef notyet
#define	ZS_INTERRUPT	b zshard; mov %psr, %l0; nop; nop
#else
#define	ZS_INTERRUPT44C	HARDINT44C(12)
#define	ZS_INTERRUPT4M	HARDINT4M(12)
#endif

	.text
	.globl	start, _C_LABEL(kernel_text)
_C_LABEL(kernel_text):
start:
/*
 * Put sun4 and sun4e traptables first, since they need the most stringent
 * alignment (8192).
 */
#if defined(SUN4)
trapbase_sun4:
	/* trap 0 is special since we cannot receive it */
	b dostart; nop; nop; nop	! 00 = reset (fake)
	VTRAP(T_TEXTFAULT, memfault_sun4)	! 01 = instr. fetch fault
	TRAP(T_ILLINST)			! 02 = illegal instruction
	TRAP(T_PRIVINST)		! 03 = privileged instruction
	TRAP(T_FPDISABLED)		! 04 = fp instr, but EF bit off in psr
	WINDOW_OF			! 05 = window overflow
	WINDOW_UF			! 06 = window underflow
	TRAP(T_ALIGN)			! 07 = address alignment error
	VTRAP(T_FPE, fp_exception)	! 08 = fp exception
	VTRAP(T_DATAFAULT, memfault_sun4)	! 09 = data fetch fault
	TRAP(T_TAGOF)			! 0a = tag overflow
	UTRAP(0x0b)
	UTRAP(0x0c)
	UTRAP(0x0d)
	UTRAP(0x0e)
	UTRAP(0x0f)
	UTRAP(0x10)
	SOFTINT44C(1, IE_L1)		! 11 = level 1 interrupt
	HARDINT44C(2)			! 12 = level 2 interrupt
	HARDINT44C(3)			! 13 = level 3 interrupt
	SOFTINT44C(4, IE_L4)		! 14 = level 4 interrupt
	HARDINT44C(5)			! 15 = level 5 interrupt
	SOFTINT44C(6, IE_L6)		! 16 = level 6 interrupt
	HARDINT44C(7)			! 17 = level 7 interrupt
	HARDINT44C(8)			! 18 = level 8 interrupt
	HARDINT44C(9)			! 19 = level 9 interrupt
	HARDINT44C(10)			! 1a = level 10 interrupt
	HARDINT44C(11)			! 1b = level 11 interrupt
	ZS_INTERRUPT44C			! 1c = level 12 (zs) interrupt
	HARDINT44C(13)			! 1d = level 13 interrupt
	HARDINT44C(14)			! 1e = level 14 interrupt
	VTRAP(15, nmi_sun4)		! 1f = nonmaskable interrupt
	UTRAP(0x20)
	UTRAP(0x21)
	UTRAP(0x22)
	UTRAP(0x23)
	TRAP(T_CPDISABLED)	! 24 = coprocessor instr, EC bit off in psr
	UTRAP(0x25)
	UTRAP(0x26)
	UTRAP(0x27)
	TRAP(T_CPEXCEPTION)	! 28 = coprocessor exception
	UTRAP(0x29)
	UTRAP(0x2a)
	UTRAP(0x2b)
	UTRAP(0x2c)
	UTRAP(0x2d)
	UTRAP(0x2e)
	UTRAP(0x2f)
	UTRAP(0x30)
	UTRAP(0x31)
	UTRAP(0x32)
	UTRAP(0x33)
	UTRAP(0x34)
	UTRAP(0x35)
	UTRAP(0x36)
	UTRAP(0x37)
	UTRAP(0x38)
	UTRAP(0x39)
	UTRAP(0x3a)
	UTRAP(0x3b)
	UTRAP(0x3c)
	UTRAP(0x3d)
	UTRAP(0x3e)
	UTRAP(0x3f)
	UTRAP(0x40)
	UTRAP(0x41)
	UTRAP(0x42)
	UTRAP(0x43)
	UTRAP(0x44)
	UTRAP(0x45)
	UTRAP(0x46)
	UTRAP(0x47)
	UTRAP(0x48)
	UTRAP(0x49)
	UTRAP(0x4a)
	UTRAP(0x4b)
	UTRAP(0x4c)
	UTRAP(0x4d)
	UTRAP(0x4e)
	UTRAP(0x4f)
	UTRAP(0x50)
	UTRAP(0x51)
	UTRAP(0x52)
	UTRAP(0x53)
	UTRAP(0x54)
	UTRAP(0x55)
	UTRAP(0x56)
	UTRAP(0x57)
	UTRAP(0x58)
	UTRAP(0x59)
	UTRAP(0x5a)
	UTRAP(0x5b)
	UTRAP(0x5c)
	UTRAP(0x5d)
	UTRAP(0x5e)
	UTRAP(0x5f)
	UTRAP(0x60)
	UTRAP(0x61)
	UTRAP(0x62)
	UTRAP(0x63)
	UTRAP(0x64)
	UTRAP(0x65)
	UTRAP(0x66)
	UTRAP(0x67)
	UTRAP(0x68)
	UTRAP(0x69)
	UTRAP(0x6a)
	UTRAP(0x6b)
	UTRAP(0x6c)
	UTRAP(0x6d)
	UTRAP(0x6e)
	UTRAP(0x6f)
	UTRAP(0x70)
	UTRAP(0x71)
	UTRAP(0x72)
	UTRAP(0x73)
	UTRAP(0x74)
	UTRAP(0x75)
	UTRAP(0x76)
	UTRAP(0x77)
	UTRAP(0x78)
	UTRAP(0x79)
	UTRAP(0x7a)
	UTRAP(0x7b)
	UTRAP(0x7c)
	UTRAP(0x7d)
	UTRAP(0x7e)
	UTRAP(0x7f)
	SYSCALL			! 80 = sun syscall
	BPT			! 81 = pseudo breakpoint instruction
	TRAP(T_DIV0)		! 82 = divide by zero
	TRAP(T_FLUSHWIN)	! 83 = flush windows
	TRAP(T_CLEANWIN)	! 84 = provide clean windows
	TRAP(T_RANGECHECK)	! 85 = ???
	TRAP(T_FIXALIGN)	! 86 = fix up unaligned accesses
	TRAP(T_INTOF)		! 87 = integer overflow
	SYSCALL			! 88 = svr4 syscall
	SYSCALL			! 89 = bsd syscall
	BPT_KGDB_EXEC		! 8a = enter kernel gdb on kernel startup
	STRAP(0x8b)
	STRAP(0x8c)
	STRAP(0x8d)
	STRAP(0x8e)
	STRAP(0x8f)
	STRAP(0x90)
	STRAP(0x91)
	STRAP(0x92)
	STRAP(0x93)
	STRAP(0x94)
	STRAP(0x95)
	STRAP(0x96)
	STRAP(0x97)
	STRAP(0x98)
	STRAP(0x99)
	STRAP(0x9a)
	STRAP(0x9b)
	STRAP(0x9c)
	STRAP(0x9d)
	STRAP(0x9e)
	STRAP(0x9f)
	STRAP(0xa0)
	STRAP(0xa1)
	STRAP(0xa2)
	STRAP(0xa3)
	STRAP(0xa4)
	STRAP(0xa5)
	STRAP(0xa6)
	STRAP(0xa7)
	STRAP(0xa8)
	STRAP(0xa9)
	STRAP(0xaa)
	STRAP(0xab)
	STRAP(0xac)
	STRAP(0xad)
	STRAP(0xae)
	STRAP(0xaf)
	STRAP(0xb0)
	STRAP(0xb1)
	STRAP(0xb2)
	STRAP(0xb3)
	STRAP(0xb4)
	STRAP(0xb5)
	STRAP(0xb6)
	STRAP(0xb7)
	STRAP(0xb8)
	STRAP(0xb9)
	STRAP(0xba)
	STRAP(0xbb)
	STRAP(0xbc)
	STRAP(0xbd)
	STRAP(0xbe)
	STRAP(0xbf)
	STRAP(0xc0)
	STRAP(0xc1)
	STRAP(0xc2)
	STRAP(0xc3)
	STRAP(0xc4)
	STRAP(0xc5)
	STRAP(0xc6)
	STRAP(0xc7)
	STRAP(0xc8)
	STRAP(0xc9)
	STRAP(0xca)
	STRAP(0xcb)
	STRAP(0xcc)
	STRAP(0xcd)
	STRAP(0xce)
	STRAP(0xcf)
	STRAP(0xd0)
	STRAP(0xd1)
	STRAP(0xd2)
	STRAP(0xd3)
	STRAP(0xd4)
	STRAP(0xd5)
	STRAP(0xd6)
	STRAP(0xd7)
	STRAP(0xd8)
	STRAP(0xd9)
	STRAP(0xda)
	STRAP(0xdb)
	STRAP(0xdc)
	STRAP(0xdd)
	STRAP(0xde)
	STRAP(0xdf)
	STRAP(0xe0)
	STRAP(0xe1)
	STRAP(0xe2)
	STRAP(0xe3)
	STRAP(0xe4)
	STRAP(0xe5)
	STRAP(0xe6)
	STRAP(0xe7)
	STRAP(0xe8)
	STRAP(0xe9)
	STRAP(0xea)
	STRAP(0xeb)
	STRAP(0xec)
	STRAP(0xed)
	STRAP(0xee)
	STRAP(0xef)
	STRAP(0xf0)
	STRAP(0xf1)
	STRAP(0xf2)
	STRAP(0xf3)
	STRAP(0xf4)
	STRAP(0xf5)
	STRAP(0xf6)
	STRAP(0xf7)
	STRAP(0xf8)
	STRAP(0xf9)
	STRAP(0xfa)
	STRAP(0xfb)
	STRAP(0xfc)
	STRAP(0xfd)
	STRAP(0xfe)
	STRAP(0xff)
#endif	/* SUN4 */

#if defined(SUN4) && defined(SUN4E)
	/*
	 * Make sure the sun4e trap table (which is the same as the sun4c
	 * table until we support VME interrupts) is properly page-aligned.
	 */
	.skip	4096
#endif

#if defined(SUN4C) || defined(SUN4E)
trapbase_sun4c:
	/* trap 0 is special since we cannot receive it */
	b dostart; nop; nop; nop	! 00 = reset (fake)
	VTRAP(T_TEXTFAULT, memfault_sun4c)	! 01 = instr. fetch fault
	TRAP(T_ILLINST)			! 02 = illegal instruction
	TRAP(T_PRIVINST)		! 03 = privileged instruction
	TRAP(T_FPDISABLED)		! 04 = fp instr, but EF bit off in psr
	WINDOW_OF			! 05 = window overflow
	WINDOW_UF			! 06 = window underflow
	TRAP(T_ALIGN)			! 07 = address alignment error
	VTRAP(T_FPE, fp_exception)	! 08 = fp exception
	VTRAP(T_DATAFAULT, memfault_sun4c)	! 09 = data fetch fault
	TRAP(T_TAGOF)			! 0a = tag overflow
	UTRAP(0x0b)
	UTRAP(0x0c)
	UTRAP(0x0d)
	UTRAP(0x0e)
	UTRAP(0x0f)
	UTRAP(0x10)
	SOFTINT44C(1, IE_L1)		! 11 = level 1 interrupt
	HARDINT44C(2)			! 12 = level 2 interrupt
	HARDINT44C(3)			! 13 = level 3 interrupt
	SOFTINT44C(4, IE_L4)		! 14 = level 4 interrupt
	HARDINT44C(5)			! 15 = level 5 interrupt
	SOFTINT44C(6, IE_L6)		! 16 = level 6 interrupt
	HARDINT44C(7)			! 17 = level 7 interrupt
	HARDINT44C(8)			! 18 = level 8 interrupt
	HARDINT44C(9)			! 19 = level 9 interrupt
	HARDINT44C(10)			! 1a = level 10 interrupt
	HARDINT44C(11)			! 1b = level 11 interrupt
	ZS_INTERRUPT44C			! 1c = level 12 (zs) interrupt
	HARDINT44C(13)			! 1d = level 13 interrupt
	HARDINT44C(14)			! 1e = level 14 interrupt
	VTRAP(15, nmi_sun4c)		! 1f = nonmaskable interrupt
	UTRAP(0x20)
	UTRAP(0x21)
	UTRAP(0x22)
	UTRAP(0x23)
	TRAP(T_CPDISABLED)	! 24 = coprocessor instr, EC bit off in psr
	UTRAP(0x25)
	UTRAP(0x26)
	UTRAP(0x27)
	TRAP(T_CPEXCEPTION)	! 28 = coprocessor exception
	UTRAP(0x29)
	UTRAP(0x2a)
	UTRAP(0x2b)
	UTRAP(0x2c)
	UTRAP(0x2d)
	UTRAP(0x2e)
	UTRAP(0x2f)
	UTRAP(0x30)
	UTRAP(0x31)
	UTRAP(0x32)
	UTRAP(0x33)
	UTRAP(0x34)
	UTRAP(0x35)
	UTRAP(0x36)
	UTRAP(0x37)
	UTRAP(0x38)
	UTRAP(0x39)
	UTRAP(0x3a)
	UTRAP(0x3b)
	UTRAP(0x3c)
	UTRAP(0x3d)
	UTRAP(0x3e)
	UTRAP(0x3f)
	UTRAP(0x40)
	UTRAP(0x41)
	UTRAP(0x42)
	UTRAP(0x43)
	UTRAP(0x44)
	UTRAP(0x45)
	UTRAP(0x46)
	UTRAP(0x47)
	UTRAP(0x48)
	UTRAP(0x49)
	UTRAP(0x4a)
	UTRAP(0x4b)
	UTRAP(0x4c)
	UTRAP(0x4d)
	UTRAP(0x4e)
	UTRAP(0x4f)
	UTRAP(0x50)
	UTRAP(0x51)
	UTRAP(0x52)
	UTRAP(0x53)
	UTRAP(0x54)
	UTRAP(0x55)
	UTRAP(0x56)
	UTRAP(0x57)
	UTRAP(0x58)
	UTRAP(0x59)
	UTRAP(0x5a)
	UTRAP(0x5b)
	UTRAP(0x5c)
	UTRAP(0x5d)
	UTRAP(0x5e)
	UTRAP(0x5f)
	UTRAP(0x60)
	UTRAP(0x61)
	UTRAP(0x62)
	UTRAP(0x63)
	UTRAP(0x64)
	UTRAP(0x65)
	UTRAP(0x66)
	UTRAP(0x67)
	UTRAP(0x68)
	UTRAP(0x69)
	UTRAP(0x6a)
	UTRAP(0x6b)
	UTRAP(0x6c)
	UTRAP(0x6d)
	UTRAP(0x6e)
	UTRAP(0x6f)
	UTRAP(0x70)
	UTRAP(0x71)
	UTRAP(0x72)
	UTRAP(0x73)
	UTRAP(0x74)
	UTRAP(0x75)
	UTRAP(0x76)
	UTRAP(0x77)
	UTRAP(0x78)
	UTRAP(0x79)
	UTRAP(0x7a)
	UTRAP(0x7b)
	UTRAP(0x7c)
	UTRAP(0x7d)
	UTRAP(0x7e)
	UTRAP(0x7f)
	SYSCALL			! 80 = sun syscall
	BPT			! 81 = pseudo breakpoint instruction
	TRAP(T_DIV0)		! 82 = divide by zero
	TRAP(T_FLUSHWIN)	! 83 = flush windows
	TRAP(T_CLEANWIN)	! 84 = provide clean windows
	TRAP(T_RANGECHECK)	! 85 = ???
	TRAP(T_FIXALIGN)	! 86 = fix up unaligned accesses
	TRAP(T_INTOF)		! 87 = integer overflow
	SYSCALL			! 88 = svr4 syscall
	SYSCALL			! 89 = bsd syscall
	BPT_KGDB_EXEC		! 8a = enter kernel gdb on kernel startup
	STRAP(0x8b)
	STRAP(0x8c)
	STRAP(0x8d)
	STRAP(0x8e)
	STRAP(0x8f)
	STRAP(0x90)
	STRAP(0x91)
	STRAP(0x92)
	STRAP(0x93)
	STRAP(0x94)
	STRAP(0x95)
	STRAP(0x96)
	STRAP(0x97)
	STRAP(0x98)
	STRAP(0x99)
	STRAP(0x9a)
	STRAP(0x9b)
	STRAP(0x9c)
	STRAP(0x9d)
	STRAP(0x9e)
	STRAP(0x9f)
	STRAP(0xa0)
	STRAP(0xa1)
	STRAP(0xa2)
	STRAP(0xa3)
	STRAP(0xa4)
	STRAP(0xa5)
	STRAP(0xa6)
	STRAP(0xa7)
	STRAP(0xa8)
	STRAP(0xa9)
	STRAP(0xaa)
	STRAP(0xab)
	STRAP(0xac)
	STRAP(0xad)
	STRAP(0xae)
	STRAP(0xaf)
	STRAP(0xb0)
	STRAP(0xb1)
	STRAP(0xb2)
	STRAP(0xb3)
	STRAP(0xb4)
	STRAP(0xb5)
	STRAP(0xb6)
	STRAP(0xb7)
	STRAP(0xb8)
	STRAP(0xb9)
	STRAP(0xba)
	STRAP(0xbb)
	STRAP(0xbc)
	STRAP(0xbd)
	STRAP(0xbe)
	STRAP(0xbf)
	STRAP(0xc0)
	STRAP(0xc1)
	STRAP(0xc2)
	STRAP(0xc3)
	STRAP(0xc4)
	STRAP(0xc5)
	STRAP(0xc6)
	STRAP(0xc7)
	STRAP(0xc8)
	STRAP(0xc9)
	STRAP(0xca)
	STRAP(0xcb)
	STRAP(0xcc)
	STRAP(0xcd)
	STRAP(0xce)
	STRAP(0xcf)
	STRAP(0xd0)
	STRAP(0xd1)
	STRAP(0xd2)
	STRAP(0xd3)
	STRAP(0xd4)
	STRAP(0xd5)
	STRAP(0xd6)
	STRAP(0xd7)
	STRAP(0xd8)
	STRAP(0xd9)
	STRAP(0xda)
	STRAP(0xdb)
	STRAP(0xdc)
	STRAP(0xdd)
	STRAP(0xde)
	STRAP(0xdf)
	STRAP(0xe0)
	STRAP(0xe1)
	STRAP(0xe2)
	STRAP(0xe3)
	STRAP(0xe4)
	STRAP(0xe5)
	STRAP(0xe6)
	STRAP(0xe7)
	STRAP(0xe8)
	STRAP(0xe9)
	STRAP(0xea)
	STRAP(0xeb)
	STRAP(0xec)
	STRAP(0xed)
	STRAP(0xee)
	STRAP(0xef)
	STRAP(0xf0)
	STRAP(0xf1)
	STRAP(0xf2)
	STRAP(0xf3)
	STRAP(0xf4)
	STRAP(0xf5)
	STRAP(0xf6)
	STRAP(0xf7)
	STRAP(0xf8)
	STRAP(0xf9)
	STRAP(0xfa)
	STRAP(0xfb)
	STRAP(0xfc)
	STRAP(0xfd)
	STRAP(0xfe)
	STRAP(0xff)
#endif

#if defined(SUN4M)
trapbase_sun4m:
/* trap 0 is special since we cannot receive it */
	b dostart; nop; nop; nop	! 00 = reset (fake)
	VTRAP(T_TEXTFAULT, memfault_sun4m)	! 01 = instr. fetch fault
	TRAP(T_ILLINST)			! 02 = illegal instruction
	TRAP(T_PRIVINST)		! 03 = privileged instruction
	TRAP(T_FPDISABLED)		! 04 = fp instr, but EF bit off in psr
	WINDOW_OF			! 05 = window overflow
	WINDOW_UF			! 06 = window underflow
	TRAP(T_ALIGN)			! 07 = address alignment error
	VTRAP(T_FPE, fp_exception)	! 08 = fp exception
	VTRAP(T_DATAFAULT, memfault_sun4m)	! 09 = data fetch fault
	TRAP(T_TAGOF)			! 0a = tag overflow
	UTRAP(0x0b)
	UTRAP(0x0c)
	UTRAP(0x0d)
	UTRAP(0x0e)
	UTRAP(0x0f)
	UTRAP(0x10)
	HARDINT4M(1)			! 11 = level 1 interrupt
	HARDINT4M(2)			! 12 = level 2 interrupt
	HARDINT4M(3)			! 13 = level 3 interrupt
	HARDINT4M(4)			! 14 = level 4 interrupt
	HARDINT4M(5)			! 15 = level 5 interrupt
	HARDINT4M(6)			! 16 = level 6 interrupt
	HARDINT4M(7)			! 17 = level 7 interrupt
	HARDINT4M(8)			! 18 = level 8 interrupt
	HARDINT4M(9)			! 19 = level 9 interrupt
	HARDINT4M(10)			! 1a = level 10 interrupt
	HARDINT4M(11)			! 1b = level 11 interrupt
	ZS_INTERRUPT4M			! 1c = level 12 (zs) interrupt
	HARDINT4M(13)			! 1d = level 13 interrupt
	HARDINT4M(14)			! 1e = level 14 interrupt
	VTRAP(15, nmi_sun4m)		! 1f = nonmaskable interrupt
	UTRAP(0x20)				! 20 = r-reg access error ???
	VTRAP(T_TEXTFAULT, memfault_sun4m)	! 21 = v8 instr. fetch error
	UTRAP(0x22)
	UTRAP(0x23)
	TRAP(T_CPDISABLED)			! 24 = coprocessor instr, EC off
	UTRAP(0x25)				! 25 = unimplemented cache flush
	UTRAP(0x26)
	UTRAP(0x27)
	TRAP(T_CPEXCEPTION)			! 28 = coprocessor exception
	VTRAP(T_DATAFAULT, memfault_sun4m)	! 29 = v8 data fetch error
	TRAP(T_DIV0)				! 2a = v8 int divide by zero
	VTRAP(T_STOREBUFFAULT, memfault_sun4m)	! 2b = SS store buffer fault
	UTRAP(0x2c)
	UTRAP(0x2d)
	UTRAP(0x2e)
	UTRAP(0x2f)
	UTRAP(0x30)
	UTRAP(0x31)
	UTRAP(0x32)
	UTRAP(0x33)
	UTRAP(0x34)
	UTRAP(0x35)
	UTRAP(0x36)
	UTRAP(0x37)
	UTRAP(0x38)
	UTRAP(0x39)
	UTRAP(0x3a)
	UTRAP(0x3b)
	UTRAP(0x3c)
	UTRAP(0x3d)
	UTRAP(0x3e)
	UTRAP(0x3f)
	UTRAP(0x40)
	UTRAP(0x41)
	UTRAP(0x42)
	UTRAP(0x43)
	UTRAP(0x44)
	UTRAP(0x45)
	UTRAP(0x46)
	UTRAP(0x47)
	UTRAP(0x48)
	UTRAP(0x49)
	UTRAP(0x4a)
	UTRAP(0x4b)
	UTRAP(0x4c)
	UTRAP(0x4d)
	UTRAP(0x4e)
	UTRAP(0x4f)
	UTRAP(0x50)
	UTRAP(0x51)
	UTRAP(0x52)
	UTRAP(0x53)
	UTRAP(0x54)
	UTRAP(0x55)
	UTRAP(0x56)
	UTRAP(0x57)
	UTRAP(0x58)
	UTRAP(0x59)
	UTRAP(0x5a)
	UTRAP(0x5b)
	UTRAP(0x5c)
	UTRAP(0x5d)
	UTRAP(0x5e)
	UTRAP(0x5f)
	UTRAP(0x60)
	UTRAP(0x61)
	UTRAP(0x62)
	UTRAP(0x63)
	UTRAP(0x64)
	UTRAP(0x65)
	UTRAP(0x66)
	UTRAP(0x67)
	UTRAP(0x68)
	UTRAP(0x69)
	UTRAP(0x6a)
	UTRAP(0x6b)
	UTRAP(0x6c)
	UTRAP(0x6d)
	UTRAP(0x6e)
	UTRAP(0x6f)
	UTRAP(0x70)
	UTRAP(0x71)
	UTRAP(0x72)
	UTRAP(0x73)
	UTRAP(0x74)
	UTRAP(0x75)
	UTRAP(0x76)
	UTRAP(0x77)
	UTRAP(0x78)
	UTRAP(0x79)
	UTRAP(0x7a)
	UTRAP(0x7b)
	UTRAP(0x7c)
	UTRAP(0x7d)
	UTRAP(0x7e)
	UTRAP(0x7f)
	SYSCALL			! 80 = sun syscall
	BPT			! 81 = pseudo breakpoint instruction
	TRAP(T_DIV0)		! 82 = divide by zero
	TRAP(T_FLUSHWIN)	! 83 = flush windows
	TRAP(T_CLEANWIN)	! 84 = provide clean windows
	TRAP(T_RANGECHECK)	! 85 = ???
	TRAP(T_FIXALIGN)	! 86 = fix up unaligned accesses
	TRAP(T_INTOF)		! 87 = integer overflow
	SYSCALL			! 88 = svr4 syscall
	SYSCALL			! 89 = bsd syscall
	BPT_KGDB_EXEC		! 8a = enter kernel gdb on kernel startup
	STRAP(0x8b)
	STRAP(0x8c)
	STRAP(0x8d)
	STRAP(0x8e)
	STRAP(0x8f)
	STRAP(0x90)
	STRAP(0x91)
	STRAP(0x92)
	STRAP(0x93)
	STRAP(0x94)
	STRAP(0x95)
	STRAP(0x96)
	STRAP(0x97)
	STRAP(0x98)
	STRAP(0x99)
	STRAP(0x9a)
	STRAP(0x9b)
	STRAP(0x9c)
	STRAP(0x9d)
	STRAP(0x9e)
	STRAP(0x9f)
	STRAP(0xa0)
	STRAP(0xa1)
	STRAP(0xa2)
	STRAP(0xa3)
	STRAP(0xa4)
	STRAP(0xa5)
	STRAP(0xa6)
	STRAP(0xa7)
	STRAP(0xa8)
	STRAP(0xa9)
	STRAP(0xaa)
	STRAP(0xab)
	STRAP(0xac)
	STRAP(0xad)
	STRAP(0xae)
	STRAP(0xaf)
	STRAP(0xb0)
	STRAP(0xb1)
	STRAP(0xb2)
	STRAP(0xb3)
	STRAP(0xb4)
	STRAP(0xb5)
	STRAP(0xb6)
	STRAP(0xb7)
	STRAP(0xb8)
	STRAP(0xb9)
	STRAP(0xba)
	STRAP(0xbb)
	STRAP(0xbc)
	STRAP(0xbd)
	STRAP(0xbe)
	STRAP(0xbf)
	STRAP(0xc0)
	STRAP(0xc1)
	STRAP(0xc2)
	STRAP(0xc3)
	STRAP(0xc4)
	STRAP(0xc5)
	STRAP(0xc6)
	STRAP(0xc7)
	STRAP(0xc8)
	STRAP(0xc9)
	STRAP(0xca)
	STRAP(0xcb)
	STRAP(0xcc)
	STRAP(0xcd)
	STRAP(0xce)
	STRAP(0xcf)
	STRAP(0xd0)
	STRAP(0xd1)
	STRAP(0xd2)
	STRAP(0xd3)
	STRAP(0xd4)
	STRAP(0xd5)
	STRAP(0xd6)
	STRAP(0xd7)
	STRAP(0xd8)
	STRAP(0xd9)
	STRAP(0xda)
	STRAP(0xdb)
	STRAP(0xdc)
	STRAP(0xdd)
	STRAP(0xde)
	STRAP(0xdf)
	STRAP(0xe0)
	STRAP(0xe1)
	STRAP(0xe2)
	STRAP(0xe3)
	STRAP(0xe4)
	STRAP(0xe5)
	STRAP(0xe6)
	STRAP(0xe7)
	STRAP(0xe8)
	STRAP(0xe9)
	STRAP(0xea)
	STRAP(0xeb)
	STRAP(0xec)
	STRAP(0xed)
	STRAP(0xee)
	STRAP(0xef)
	STRAP(0xf0)
	STRAP(0xf1)
	STRAP(0xf2)
	STRAP(0xf3)
	STRAP(0xf4)
	STRAP(0xf5)
	STRAP(0xf6)
	STRAP(0xf7)
	STRAP(0xf8)
	STRAP(0xf9)
	STRAP(0xfa)
	STRAP(0xfb)
	STRAP(0xfc)
	STRAP(0xfd)
	STRAP(0xfe)
	STRAP(0xff)
#endif

#if (defined(SUN4) || defined(SUN4E))
/*
 * Pad the trap table to max page size.
 * Trap table size is 0x100 * 4instr * 4byte/instr = 4096 bytes;
 * need to .skip 4096 to pad to page size iff. the number of trap tables
 * defined above is odd.
 */
/*
 * SUN4 && !SUN4E => pad if only one of 4C and 4M is set
 * SUN4E => pad if 4M is not set
 */
#if (!defined(SUN4E) && (defined(SUN4C) + defined(SUN4M)) == 1) || \
    (defined(SUN4E) && !defined(SUN4M))
	.skip	4096
#endif
#endif

#ifdef DEBUG
/*
 * A hardware red zone is impossible.  We simulate one in software by
 * keeping a `red zone' pointer; if %sp becomes less than this, we panic.
 * This is expensive and is only enabled when debugging.
 */
#define	REDSIZE	(8*96)		/* some room for bouncing */
#define	REDSTACK 2048		/* size of `panic: stack overflow' region */
	.data
_C_LABEL(redzone):
	.word	_C_LABEL(idle_u) + REDSIZE
_C_LABEL(redstack):
	.skip	REDSTACK
	.text
Lpanic_red:
	.asciz	"stack overflow"
	_ALIGN

	/* set stack pointer redzone to base+minstack; alters base */
#define	SET_SP_REDZONE(base, tmp) \
	add	base, REDSIZE, base; \
	sethi	%hi(_C_LABEL(redzone)), tmp; \
	st	base, [tmp + %lo(_C_LABEL(redzone))]

	/* variant with a constant */
#define	SET_SP_REDZONE_CONST(const, tmp1, tmp2) \
	set	(const) + REDSIZE, tmp1; \
	sethi	%hi(_C_LABEL(redzone)), tmp2; \
	st	tmp1, [tmp2 + %lo(_C_LABEL(redzone))]

	/* check stack pointer against redzone (uses two temps) */
#define	CHECK_SP_REDZONE(t1, t2) \
	sethi	%hi(_C_LABEL(redzone)), t1; \
	ld	[t1 + %lo(_C_LABEL(redzone))], t2; \
	cmp	%sp, t2;	/* if sp >= t2, not in red zone */ \
	bgeu	7f; nop;	/* and can continue normally */ \
	/* move to panic stack */ \
	st	%g0, [t1 + %lo(_C_LABEL(redzone))]; \
	set	_C_LABEL(redstack) + REDSTACK - 96, %sp; \
	/* prevent panic() from lowering ipl */ \
	sethi	%hi(_C_LABEL(panicstr)), t2; \
	set	Lpanic_red, t2; \
	st	t2, [t1 + %lo(_C_LABEL(panicstr))]; \
	rd	%psr, t1;		/* t1 = splhigh() */ \
	or	t1, PSR_PIL, t2; \
	wr	t2, 0, %psr; \
	wr	t2, PSR_ET, %psr;	/* turn on traps */ \
	nop; nop; nop; \
	save	%sp, -CCFSZ, %sp;	/* preserve current window */ \
	sethi	%hi(Lpanic_red), %o0; \
	call	_C_LABEL(panic); or %o0, %lo(Lpanic_red), %o0; \
7:

#else

#define	SET_SP_REDZONE(base, tmp)
#define	SET_SP_REDZONE_CONST(const, t1, t2)
#define	CHECK_SP_REDZONE(t1, t2)
#endif

/*
 * The window code must verify user stack addresses before using them.
 * A user stack pointer is invalid if:
 *	- it is not on an 8 byte boundary;
 *	- its pages (a register window, being 64 bytes, can occupy
 *	  two pages) are not readable or writable.
 * We define three separate macros here for testing user stack addresses.
 *
 * PTE_OF_ADDR locates a PTE, branching to a `bad address'
 *	handler if the stack pointer points into the hole in the
 *	address space (i.e., top 3 bits are not either all 1 or all 0);
 * CMP_PTE_USER_READ compares the located PTE against `user read' mode;
 * CMP_PTE_USER_WRITE compares the located PTE against `user write' mode.
 * The compares give `equal' if read or write is OK.
 *
 * Note that the user stack pointer usually points into high addresses
 * (top 3 bits all 1), so that is what we check first.
 *
 * The code below also assumes that PTE_OF_ADDR is safe in a delay
 * slot; it is, at it merely sets its `pte' register to a temporary value.
 */
#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
	/* input: addr, output: pte; aux: bad address label */
#define	PTE_OF_ADDR4_4C(addr, pte, bad, page_offset) \
	sra	addr, PG_VSHIFT, pte; \
	cmp	pte, -1; \
	be,a	1f; andn addr, page_offset, pte; \
	tst	pte; \
	bne	bad; EMPTY; \
	andn	addr, page_offset, pte; \
1:

	/* input: pte; output: condition codes */
#define	CMP_PTE_USER_READ4_4C(pte) \
	lda	[pte] ASI_PTE, pte; \
	srl	pte, PG_PROTSHIFT, pte; \
	andn	pte, (PG_W >> PG_PROTSHIFT), pte; \
	cmp	pte, PG_PROTUREAD

	/* input: pte; output: condition codes */
#define	CMP_PTE_USER_WRITE4_4C(pte) \
	lda	[pte] ASI_PTE, pte; \
	srl	pte, PG_PROTSHIFT, pte; \
	cmp	pte, PG_PROTUWRITE
#endif

/*
 * The Sun4M does not have the memory hole that the 4C does. Thus all
 * we need to do here is clear the page offset from addr.
 */
#if defined(SUN4M)
#define	PTE_OF_ADDR4M(addr, pte, bad, page_offset) \
	andn	addr, page_offset, pte

/*
 * After obtaining the PTE through ASI_SRMMUFP, we read the Sync Fault
 * Status register. This is necessary on Hypersparcs which stores and
 * locks the fault address and status registers if the translation
 * fails (thanks to Chris Torek for finding this quirk).
 */
/* note: pmap currently does not use the PPROT_R_R and PPROT_RW_RW cases */
#define CMP_PTE_USER_READ4M(pte, tmp) \
	/* or	pte, ASI_SRMMUFP_L3, pte; -- ASI_SRMMUFP_L3 == 0 */ \
	lda	[pte] ASI_SRMMUFP, pte; \
	set	SRMMU_SFSR, tmp; \
	and	pte, (SRMMU_TETYPE | SRMMU_PROT_MASK), pte; \
	cmp	pte, (SRMMU_TEPTE | PPROT_RWX_RWX); \
	be	8f; \
	 lda	[tmp] ASI_SRMMU, %g0; \
	cmp	pte, (SRMMU_TEPTE | PPROT_RX_RX); \
8:


/* note: PTE bit 4 set implies no user writes */
#define CMP_PTE_USER_WRITE4M(pte, tmp) \
	/* or	pte, ASI_SRMMUFP_L3, pte; -- ASI_SRMMUFP_L3 == 0 */ \
	lda	[pte] ASI_SRMMUFP, pte; \
	set	SRMMU_SFSR, tmp; \
	lda	[tmp] ASI_SRMMU, %g0; \
	and	pte, (SRMMU_TETYPE | 0x14), pte; \
	cmp	pte, (SRMMU_TEPTE | PPROT_WRITE)
#endif /* 4m */

#if (defined(SUN4D) || defined(SUN4M)) && !(defined(SUN4) || defined(SUN4C) || defined(SUN4E))

#define PTE_OF_ADDR(addr, pte, bad, page_offset, label) \
	PTE_OF_ADDR4M(addr, pte, bad, page_offset)
#define CMP_PTE_USER_WRITE(pte, tmp, label)	CMP_PTE_USER_WRITE4M(pte,tmp)
#define CMP_PTE_USER_READ(pte, tmp, label)	CMP_PTE_USER_READ4M(pte,tmp)

#elif (defined(SUN4) || defined(SUN4C) || defined(SUN4E)) && !(defined(SUN4D) || defined(SUN4M))

#define PTE_OF_ADDR(addr, pte, bad, page_offset,label) \
	PTE_OF_ADDR4_4C(addr, pte, bad, page_offset)
#define CMP_PTE_USER_WRITE(pte, tmp, label)	CMP_PTE_USER_WRITE4_4C(pte)
#define CMP_PTE_USER_READ(pte, tmp, label)	CMP_PTE_USER_READ4_4C(pte)

#else /* both defined, ugh */

#define	PTE_OF_ADDR(addr, pte, bad, page_offset, label) \
label:	b,a	2f; \
	PTE_OF_ADDR4M(addr, pte, bad, page_offset); \
	b,a	3f; \
2: \
	PTE_OF_ADDR4_4C(addr, pte, bad, page_offset); \
3:

#define CMP_PTE_USER_READ(pte, tmp, label) \
label:	b,a	1f; \
	CMP_PTE_USER_READ4M(pte,tmp); \
	b,a	2f; \
1: \
	CMP_PTE_USER_READ4_4C(pte); \
2:

#define CMP_PTE_USER_WRITE(pte, tmp, label) \
label:	b,a	1f; \
	CMP_PTE_USER_WRITE4M(pte,tmp); \
	b,a	2f; \
1: \
	CMP_PTE_USER_WRITE4_4C(pte); \
2:
#endif


/*
 * The calculations in PTE_OF_ADDR and CMP_PTE_USER_* are rather slow:
 * in particular, according to Gordon Irlam of the University of Adelaide
 * in Australia, these consume at least 18 cycles on an SS1 and 37 on an
 * SS2.  Hence, we try to avoid them in the common case.
 *
 * A chunk of 64 bytes is on a single page if and only if:
 *
 *	((base + 64 - 1) & ~(NBPG-1)) == (base & ~(NBPG-1))
 *
 * Equivalently (and faster to test), the low order bits (base & 4095) must
 * be small enough so that the sum (base + 63) does not carry out into the
 * upper page-address bits, i.e.,
 *
 *	(base & (NBPG-1)) < (NBPG - 63)
 *
 * so we allow testing that here.  This macro is also assumed to be safe
 * in a delay slot (modulo overwriting its temporary).
 */
#define	SLT_IF_1PAGE_RW(addr, tmp, page_offset) \
	and	addr, page_offset, tmp; \
	sub	page_offset, 62, page_offset; \
	cmp	tmp, page_offset

/*
 * Every trap that enables traps must set up stack space.
 * If the trap is from user mode, this involves switching to the kernel
 * stack for the current process, and we must also set cpcb->pcb_uw
 * so that the window overflow handler can tell user windows from kernel
 * windows.
 *
 * The number of user windows is:
 *
 *	cpcb->pcb_uw = (cpcb->pcb_wim - 1 - CWP) % nwindows
 *
 * (where pcb_wim = log2(current %wim) and CWP = low 5 bits of %psr).
 * We compute this expression by table lookup in uwtab[CWP - pcb_wim],
 * which has been set up as:
 *
 *	for i in [-nwin+1 .. nwin-1]
 *		uwtab[i] = (nwin - 1 - i) % nwin;
 *
 * (If you do not believe this works, try it for yourself.)
 *
 * We also keep one or two more tables:
 *
 *	for i in 0..nwin-1
 *		wmask[i] = 1 << ((i + 1) % nwindows);
 *
 * wmask[CWP] tells whether a `rett' would return into the invalid window.
 */
	.data
	.skip	32			! alignment byte & negative indices
uwtab:	.skip	32			! u_char uwtab[-31..31];
wmask:	.skip	32			! u_char wmask[0..31];

	.text
/*
 * Things begin to grow uglier....
 *
 * Each trap handler may (always) be running in the trap window.
 * If this is the case, it cannot enable further traps until it writes
 * the register windows into the stack (or, if the stack is no good,
 * the current pcb).
 *
 * ASSUMPTIONS: TRAP_SETUP() is called with:
 *	%l0 = %psr
 *	%l1 = return pc
 *	%l2 = return npc
 *	%l3 = (some value that must not be altered)
 * which means we have 4 registers to work with.
 *
 * The `stackspace' argument is the number of stack bytes to allocate
 * for register-saving, and must be at least -64 (and typically more,
 * for global registers and %y).
 *
 * Trapframes should use -CCFSZ-80.  (80 = sizeof(struct trapframe);
 * see trap.h.  This basically means EVERYONE.  Interrupt frames could
 * get away with less, but currently do not.)
 *
 * The basic outline here is:
 *
 *	if (trap came from kernel mode) {
 *		if (we are in the trap window)
 *			save it away;
 *		%sp = %fp - stackspace;
 *	} else {
 *		compute the number of user windows;
 *		if (we are in the trap window)
 *			save it away;
 *		%sp = (top of kernel stack) - stackspace;
 *	}
 *
 * Again, the number of user windows is:
 *
 *	cpcb->pcb_uw = (cpcb->pcb_wim - 1 - CWP) % nwindows
 *
 * (where pcb_wim = log2(current %wim) and CWP is the low 5 bits of %psr),
 * and this is computed as `uwtab[CWP - pcb_wim]'.
 *
 * NOTE: if you change this code, you will have to look carefully
 * at the window overflow and underflow handlers and make sure they
 * have similar changes made as needed.
 */
#define	CALL_CLEAN_TRAP_WINDOW \
	sethi	%hi(clean_trap_window), %l7; \
	jmpl	%l7 + %lo(clean_trap_window), %l4; \
	 mov	%g7, %l7	/* save %g7 in %l7 for clean_trap_window */

#define	TRAP_SETUP(stackspace) \
	rd	%wim, %l4; \
	mov	1, %l5; \
	sll	%l5, %l0, %l5; \
	btst	PSR_PS, %l0; \
	bz	1f; \
	 btst	%l5, %l4; \
	/* came from kernel mode; cond codes indicate trap window */ \
	bz,a	3f; \
	 add	%fp, stackspace, %sp;	/* want to just set %sp */ \
	CALL_CLEAN_TRAP_WINDOW;		/* but maybe need to clean first */ \
	b	3f; \
	 add	%fp, stackspace, %sp; \
1: \
	/* came from user mode: compute pcb_nw */ \
	sethi	%hi(_C_LABEL(cpcb)), %l6; \
	ld	[%l6 + %lo(_C_LABEL(cpcb))], %l6; \
	ld	[%l6 + PCB_WIM], %l5; \
	and	%l0, 31, %l4; \
	sub	%l4, %l5, %l5; \
	set	uwtab, %l4; \
	ldub	[%l4 + %l5], %l5; \
	st	%l5, [%l6 + PCB_UW]; \
	/* cond codes still indicate whether in trap window */ \
	bz,a	2f; \
	 sethi	%hi(USPACE+(stackspace)), %l5; \
	/* yes, in trap window; must clean it */ \
	CALL_CLEAN_TRAP_WINDOW; \
	sethi	%hi(_C_LABEL(cpcb)), %l6; \
	ld	[%l6 + %lo(_C_LABEL(cpcb))], %l6; \
	sethi	%hi(USPACE+(stackspace)), %l5; \
2: \
	/* trap window is (now) clean: set %sp */ \
	or	%l5, %lo(USPACE+(stackspace)), %l5; \
	add	%l6, %l5, %sp; \
	SET_SP_REDZONE(%l6, %l5); \
3: \
	CHECK_SP_REDZONE(%l6, %l5)

/*
 * Interrupt setup is almost exactly like trap setup, but we need to
 * go to the interrupt stack if (a) we came from user mode or (b) we
 * came from kernel mode on the kernel stack.
 */
#define	INTR_SETUP(stackspace) \
	rd	%wim, %l4; \
	mov	1, %l5; \
	sll	%l5, %l0, %l5; \
	btst	PSR_PS, %l0; \
	bz	1f; \
	 btst	%l5, %l4; \
	/* came from kernel mode; cond codes still indicate trap window */ \
	bz,a	0f; \
	 sethi	%hi(_C_LABEL(eintstack)), %l7; \
	CALL_CLEAN_TRAP_WINDOW; \
	sethi	%hi(_C_LABEL(eintstack)), %l7; \
0:	/* now if %fp >= eintstack, we were on the kernel stack */ \
	cmp	%fp, %l7; \
	bge,a	3f; \
	 add	%l7, stackspace, %sp;	/* so switch to intstack */ \
	b	4f; \
	 add	%fp, stackspace, %sp;	/* else stay on intstack */ \
1: \
	/* came from user mode: compute pcb_nw */ \
	sethi	%hi(_C_LABEL(cpcb)), %l6; \
	ld	[%l6 + %lo(_C_LABEL(cpcb))], %l6; \
	ld	[%l6 + PCB_WIM], %l5; \
	and	%l0, 31, %l4; \
	sub	%l4, %l5, %l5; \
	set	uwtab, %l4; \
	ldub	[%l4 + %l5], %l5; \
	st	%l5, [%l6 + PCB_UW]; \
	/* cond codes still indicate whether in trap window */ \
	bz,a	2f; \
	 sethi	%hi(_C_LABEL(eintstack)), %l7; \
	/* yes, in trap window; must save regs */ \
	CALL_CLEAN_TRAP_WINDOW; \
	sethi	%hi(_C_LABEL(eintstack)), %l7; \
2: \
	add	%l7, stackspace, %sp; \
3: \
	SET_SP_REDZONE_CONST(_C_LABEL(intstack), %l6, %l5); \
4: \
	CHECK_SP_REDZONE(%l6, %l5)

/*
 * Handler for making the trap window shiny clean.
 *
 * On entry:
 *	cpcb->pcb_nw = number of user windows
 *	%l0 = %psr
 *	%l1 must not be clobbered
 *	%l2 must not be clobbered
 *	%l3 must not be clobbered
 *	%l4 = address for `return'
 *	%l7 = saved %g7 (we put this in a delay slot above, to save work)
 *
 * On return:
 *	%wim has changed, along with cpcb->pcb_wim
 *	%g7 has been restored
 *
 * Normally, we push only one window.
 */
clean_trap_window:
	mov	%g5, %l5		! save %g5
	mov	%g6, %l6		! ... and %g6
/*	mov	%g7, %l7		! ... and %g7 (already done for us) */
	sethi	%hi(_C_LABEL(cpcb)), %g6		! get current pcb
	ld	[%g6 + %lo(_C_LABEL(cpcb))], %g6

	/* Figure out whether it is a user window (cpcb->pcb_uw > 0). */
	ld	[%g6 + PCB_UW], %g7
	deccc	%g7
	bge	ctw_user
	 save	%g0, %g0, %g0		! in any case, enter window to save

	/* The window to be pushed is a kernel window. */
	std	%i6, [%sp + (7*8)]
	std	%l0, [%sp + (0*8)]

ctw_merge:
	!! std	%l0, [%sp + (0*8)]	! Done by delay slot or above
	std	%l2, [%sp + (1*8)]
	std	%l4, [%sp + (2*8)]
	std	%l6, [%sp + (3*8)]
	std	%i0, [%sp + (4*8)]
	std	%i2, [%sp + (5*8)]
	std	%i4, [%sp + (6*8)]
	!! std	%i6, [%sp + (7*8)]	! Done above or by StackGhost

	/* Set up new window invalid mask, and update cpcb->pcb_wim. */
	rd	%psr, %g7		! g7 = (junk << 5) + new_cwp
	mov	1, %g5			! g5 = 1 << new_cwp;
	sll	%g5, %g7, %g5
	wr	%g5, 0, %wim		! setwim(g5);
	and	%g7, 31, %g7		! cpcb->pcb_wim = g7 & 31;
	sethi	%hi(_C_LABEL(cpcb)), %g6	! re-get current pcb
	ld	[%g6 + %lo(_C_LABEL(cpcb))], %g6
	st	%g7, [%g6 + PCB_WIM]
	nop
	restore				! back to trap window

	mov	%l5, %g5		! restore g5
	mov	%l6, %g6		! ... and g6
	jmp	%l4 + 8			! return to caller
	 mov	%l7, %g7		! ... and g7
	/* NOTREACHED */


ctw_stackghost:
	!! StackGhost Encrypt
	sethi	%hi(_C_LABEL(cpcb)), %g6		! get current *pcb
	ld	[%g6 + %lo(_C_LABEL(cpcb))], %g6	! dereference *pcb
	ld	[%g6 + PCB_WCOOKIE], %l0	! get window cookie
	xor	%l0, %i7, %i7			! mix in cookie
	b	ctw_merge
	 std	%i6, [%sp + (7*8)]

ctw_user:
	/*
	 * The window to be pushed is a user window.
	 * We must verify the stack pointer (alignment & permissions).
	 * See comments above definition of PTE_OF_ADDR.
	 */
	st	%g7, [%g6 + PCB_UW]	! cpcb->pcb_uw--;
	btst	7, %sp			! if not aligned,
	bne	ctw_invalid		! choke on it
	 EMPTY

	sethi	%hi(_C_LABEL(pgofset)), %g6	! trash %g6=curpcb
	ld	[%g6 + %lo(_C_LABEL(pgofset))], %g6
	PTE_OF_ADDR(%sp, %g7, ctw_invalid, %g6, NOP_ON_4M_1)
	CMP_PTE_USER_WRITE(%g7, %g5, NOP_ON_4M_2) ! likewise if not writable
	bne	ctw_invalid
	 EMPTY
	/* Note side-effect of SLT_IF_1PAGE_RW: decrements %g6 by 62 */
	SLT_IF_1PAGE_RW(%sp, %g7, %g6)
	bl,a	ctw_stackghost		! all ok if only 1
	 std	%l0, [%sp]
	add	%sp, 7*8, %g5		! check last addr too
	add	%g6, 62, %g6		! restore %g6 to `pgofset'
	PTE_OF_ADDR(%g5, %g7, ctw_invalid, %g6, NOP_ON_4M_3)
	CMP_PTE_USER_WRITE(%g7, %g6, NOP_ON_4M_4)
	be,a	ctw_stackghost		! all ok: store <l0,l1> and merge
	 std	%l0, [%sp]

	/*
	 * The window we wanted to push could not be pushed.
	 * Instead, save ALL user windows into the pcb.
	 * We will notice later that we did this, when we
	 * get ready to return from our trap or syscall.
	 *
	 * The code here is run rarely and need not be optimal.
	 */
ctw_invalid:
	/*
	 * Reread cpcb->pcb_uw.  We decremented this earlier,
	 * so it is off by one.
	 */
	sethi	%hi(_C_LABEL(cpcb)), %g6		! re-get current pcb
	ld	[%g6 + %lo(_C_LABEL(cpcb))], %g6

	ld	[%g6 + PCB_UW], %g7	! (number of user windows) - 1
	add	%g6, PCB_RW, %g5

	/* save g7+1 windows, starting with the current one */
1:					! do {
	std	%l0, [%g5 + (0*8)]	!	rw->rw_local[0] = l0;
	std	%l2, [%g5 + (1*8)]	!	...
	std	%l4, [%g5 + (2*8)]
	std	%l6, [%g5 + (3*8)]
	std	%i0, [%g5 + (4*8)]
	std	%i2, [%g5 + (5*8)]
	std	%i4, [%g5 + (6*8)]

	!! StackGhost Encrypt  (PCP)
	! pcb already dereferenced in %g6
	ld	[%g6 + PCB_WCOOKIE], %l0	! get window cookie
	xor	%l0, %i7, %i7			! mix in cookie
	std	%i6, [%g5 + (7*8)]

	deccc	%g7			!	if (n > 0) save(), rw++;
	bge,a	1b			! } while (--n >= 0);
	 save	%g5, 64, %g5

	/* stash sp for bottommost window */
	st	%sp, [%g5 + 64 + (7*8)]

	/* set up new wim */
	rd	%psr, %g7		! g7 = (junk << 5) + new_cwp;
	mov	1, %g5			! g5 = 1 << new_cwp;
	sll	%g5, %g7, %g5
	wr	%g5, 0, %wim		! wim = g5;
	and	%g7, 31, %g7
	st	%g7, [%g6 + PCB_WIM]	! cpcb->pcb_wim = new_cwp;

	/* fix up pcb fields */
	ld	[%g6 + PCB_UW], %g7	! n = cpcb->pcb_uw;
	add	%g7, 1, %g5
	st	%g5, [%g6 + PCB_NSAVED]	! cpcb->pcb_nsaved = n + 1;
	st	%g0, [%g6 + PCB_UW]	! cpcb->pcb_uw = 0;

	/* return to trap window */
1:	deccc	%g7			! do {
	bge	1b			!	restore();
	 restore			! } while (--n >= 0);

	mov	%l5, %g5		! restore g5, g6, & g7, and return
	mov	%l6, %g6
	jmp	%l4 + 8
	 mov	%l7, %g7
	/* NOTREACHED */


/*
 * Each memory access (text or data) fault, from user or kernel mode,
 * comes here.  We read the error register and figure out what has
 * happened.
 *
 * This cannot be done from C code since we must not enable traps (and
 * hence may not use the `save' instruction) until we have decided that
 * the error is or is not an asynchronous one that showed up after a
 * synchronous error, but which must be handled before the sync err.
 *
 * Most memory faults are user mode text or data faults, which can cause
 * signal delivery or ptracing, for which we must build a full trapframe.
 * It does not seem worthwhile to work to avoid this in the other cases,
 * so we store all the %g registers on the stack immediately.
 *
 * On entry:
 *	%l0 = %psr
 *	%l1 = return pc
 *	%l2 = return npc
 *	%l3 = T_TEXTFAULT or T_DATAFAULT
 *
 * Internal:
 *	%l4 = %y, until we call mem_access_fault (then onto trapframe)
 *	%l5 = IE_reg_addr, if async mem error
 *
 */

#if defined(SUN4)
memfault_sun4:
	TRAP_SETUP(-CCFSZ-80)
	INCR(_C_LABEL(uvmexp)+V_FAULTS)

	st	%g1, [%sp + CCFSZ + 20]	! save g1
	rd	%y, %l4			! save y

	/*
	 * registers:
	 * memerr.ctrl	= memory error control reg., error if 0x80 set
	 * memerr.vaddr	= address of memory error
	 * buserr	= basically just like sun4c sync error reg but
	 *		  no SER_WRITE bit (have to figure out from code).
	 */
	set	_C_LABEL(par_err_reg), %o0	! memerr ctrl addr -- XXX mapped?
	ld	[%o0], %o0		! get it
	std	%g2, [%sp + CCFSZ + 24]	! save g2, g3
	ld	[%o0], %o1		! memerr ctrl register
	inc	4, %o0			! now VA of memerr vaddr register
	std	%g4, [%sp + CCFSZ + 32]	! (sneak g4,g5 in here)
	ld	[%o0], %o2		! memerr virt addr
	st	%g0, [%o0]		! NOTE: this clears latching!!!
	btst	ME_REG_IERR, %o1	! memory error?
					! XXX this value may not be correct
					! as I got some parity errors and the
					! correct bits were not on?
	std	%g6, [%sp + CCFSZ + 40]
	bz,a	0f			! no, just a regular fault
	 wr	%l0, PSR_ET, %psr	! (and reenable traps)

	/* memory error = death for now XXX */
	clr	%o3
	clr	%o4
	call	_C_LABEL(memerr4_4c)
	 clr	%o0
	call	_C_LABEL(callrom)
	 nop

0:
	/*
	 * have to make SUN4 emulate SUN4C.   4C code expects
	 * SER in %o1 and the offending VA in %o2, everything else is ok.
	 * (must figure out if SER_WRITE should be set)
	 */
	set	AC_BUS_ERR, %o0		! bus error register
	cmp	%l3, T_TEXTFAULT	! text fault always on PC
	be	normal_mem_fault	! go
	 lduba	[%o0] ASI_CONTROL, %o1	! get its value

#define STORE_BIT 21 /* bit that indicates a store instruction for sparc */
	ld	[%l1], %o3		! offending instruction in %o3 [l1=pc]
	srl	%o3, STORE_BIT, %o3	! get load/store bit (wont fit simm13)
	btst	1, %o3			! test for store operation

	bz	normal_mem_fault	! if (z) is a load (so branch)
	 sethi	%hi(SER_WRITE), %o5     ! damn SER_WRITE wont fit simm13
!	or	%lo(SER_WRITE), %o5, %o5! not necessary since %lo is zero
	or	%o5, %o1, %o1		! set SER_WRITE
#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
	ba,a	normal_mem_fault
	 !!nop				! XXX make efficient later
#endif /* SUN4C || SUN4D || SUN4E || SUN4M */
#endif /* SUN4 */

memfault_sun4c:
#if defined(SUN4C) || defined(SUN4E)
	TRAP_SETUP(-CCFSZ-80)
	INCR(_C_LABEL(uvmexp)+V_FAULTS)		! cnt.v_faults++ (clobbers %o0,%o1)
	
	st	%g1, [%sp + CCFSZ + 20]	! save g1
	rd	%y, %l4			! save y

	/*
	 * We know about the layout of the error registers here.
	 *	addr	reg
	 *	----	---
	 *	a	AC_SYNC_ERR
	 *	a+4	AC_SYNC_VA
	 *	a+8	AC_ASYNC_ERR
	 *	a+12	AC_ASYNC_VA
	 */

#if AC_SYNC_ERR + 4 != AC_SYNC_VA || \
    AC_SYNC_ERR + 8 != AC_ASYNC_ERR || AC_SYNC_ERR + 12 != AC_ASYNC_VA
	help help help		! I, I, I wanna be a lifeguard
#endif
	set	AC_SYNC_ERR, %o0
	std	%g2, [%sp + CCFSZ + 24]	! save g2, g3
	lda	[%o0] ASI_CONTROL, %o1	! sync err reg
	inc	4, %o0
	std	%g4, [%sp + CCFSZ + 32]	! (sneak g4,g5 in here)
	lda	[%o0] ASI_CONTROL, %o2	! sync virt addr
	btst	SER_MEMERR, %o1		! memory error?
	std	%g6, [%sp + CCFSZ + 40]
	bz,a	normal_mem_fault	! no, just a regular fault
 	 wr	%l0, PSR_ET, %psr	! (and reenable traps)

	/*
	 * We got a synchronous memory error.  It could be one that
	 * happened because there were two stores in a row, and the
	 * first went into the write buffer, and the second caused this
	 * synchronous trap; so there could now be a pending async error.
	 * This is in fact the case iff the two va's differ.
	 */
	inc	4, %o0
	lda	[%o0] ASI_CONTROL, %o3	! async err reg
	inc	4, %o0
	lda	[%o0] ASI_CONTROL, %o4	! async virt addr
	cmp	%o2, %o4
	be,a	1f			! no, not an async err
	 wr	%l0, PSR_ET, %psr	! (and reenable traps)

	/*
	 * Handle the async error; ignore the sync error for now
	 * (we may end up getting it again, but so what?).
	 * This code is essentially the same as that at `nmi' below,
	 * but the register usage is different and we cannot merge.
	 */
	sethi	%hi(INTRREG_VA), %l5	! intreg_clr_44c(IE_ALLIE);
	ldub	[%l5 + %lo(INTRREG_VA)], %o0
	andn	%o0, IE_ALLIE, %o0
	stb	%o0, [%l5 + %lo(INTRREG_VA)]

	/*
	 * Now reenable traps and call C code.
	 * %o1 through %o4 still hold the error reg contents.
	 * If memerr() returns, return from the trap.
	 */
	wr	%l0, PSR_ET, %psr
	call	_C_LABEL(memerr4_4c)
	 clr	%o0

	ld	[%sp + CCFSZ + 20], %g1	! restore g1 through g7
	wr	%l0, 0, %psr		! and disable traps, 3 instr delay
	ldd	[%sp + CCFSZ + 24], %g2
	ldd	[%sp + CCFSZ + 32], %g4
	ldd	[%sp + CCFSZ + 40], %g6
	/* now safe to set IE_ALLIE again */
	ldub	[%l5 + %lo(INTRREG_VA)], %o1
	or	%o1, IE_ALLIE, %o1
	stb	%o1, [%l5 + %lo(INTRREG_VA)]
	b	return_from_trap
	 wr	%l4, 0, %y		! restore y

	/*
	 * Trap was a synchronous memory error.
	 * %o1 through %o4 still hold the error reg contents.
	 */
1:
	call	_C_LABEL(memerr4_4c)
	 mov	1, %o0

	ld	[%sp + CCFSZ + 20], %g1	! restore g1 through g7
	ldd	[%sp + CCFSZ + 24], %g2
	ldd	[%sp + CCFSZ + 32], %g4
	ldd	[%sp + CCFSZ + 40], %g6
	wr	%l4, 0, %y		! restore y
	b	return_from_trap
	 wr	%l0, 0, %psr
	/* NOTREACHED */
#endif /* SUN4C || SUN4E */

#if defined(SUN4M)
memfault_sun4m:
	! DANGER: we use the fact that %lo(CPUINFO_VA) is zero
.if CPUINFO_VA & 0x1fff
BARF
.endif
	sethi	%hi(CPUINFO_VA), %l4
	ld	[%l4 + %lo(CPUINFO_VA+CPUINFO_GETSYNCFLT)], %l5
	jmpl	%l5, %l7
	 or	%l4, %lo(CPUINFO_SYNCFLTDUMP), %l4
	TRAP_SETUP(-CCFSZ-80)
	INCR(_C_LABEL(uvmexp)+V_FAULTS)		! cnt.v_faults++ (clobbers %o0,%o1)

	st	%g1, [%sp + CCFSZ + 20]	! save g1
	rd	%y, %l4			! save y

	std	%g2, [%sp + CCFSZ + 24]	! save g2, g3
	std	%g4, [%sp + CCFSZ + 32]	! save g4, g5
	std	%g6, [%sp + CCFSZ + 40]	! save g6, g7

	! retrieve sync fault status/address
	sethi	%hi(CPUINFO_VA+CPUINFO_SYNCFLTDUMP), %o0
	ld	[%o0 + %lo(CPUINFO_VA+CPUINFO_SYNCFLTDUMP)], %o1
	ld	[%o0 + %lo(CPUINFO_VA+CPUINFO_SYNCFLTDUMP+4)], %o2

	wr	%l0, PSR_ET, %psr	! reenable traps

	/* Finish stackframe, call C trap handler */
	std	%l0, [%sp + CCFSZ + 0]	! set tf.tf_psr, tf.tf_pc
	mov	%l3, %o0		! (argument: type)
	st	%l2, [%sp + CCFSZ + 8]	! set tf.tf_npc
	st	%l4, [%sp + CCFSZ + 12]	! set tf.tf_y
	std	%i0, [%sp + CCFSZ + 48]	! tf.tf_out[0], etc
	std	%i2, [%sp + CCFSZ + 56]
	std	%i4, [%sp + CCFSZ + 64]
	std	%i6, [%sp + CCFSZ + 72]
	call	_C_LABEL(mem_access_fault4m)	! mem_access_fault(type,sfsr,sfva,&tf);
	 add	%sp, CCFSZ, %o3		! (argument: &tf)

	ldd	[%sp + CCFSZ + 0], %l0	! load new values
	ldd	[%sp + CCFSZ + 8], %l2
	wr	%l3, 0, %y
	ld	[%sp + CCFSZ + 20], %g1
	ldd	[%sp + CCFSZ + 24], %g2
	ldd	[%sp + CCFSZ + 32], %g4
	ldd	[%sp + CCFSZ + 40], %g6
	ldd	[%sp + CCFSZ + 48], %i0
	ldd	[%sp + CCFSZ + 56], %i2
	ldd	[%sp + CCFSZ + 64], %i4
	ldd	[%sp + CCFSZ + 72], %i6

	b	return_from_trap	! go return
	 wr	%l0, 0, %psr		! (but first disable traps again)
#endif /* SUN4M */

normal_mem_fault:
	/*
	 * Trap was some other error; call C code to deal with it.
	 * Must finish trap frame (psr,pc,npc,%y,%o0..%o7) in case
	 * we decide to deliver a signal or ptrace the process.
	 * %g1..%g7 were already set up above.
	 */
	std	%l0, [%sp + CCFSZ + 0]	! set tf.tf_psr, tf.tf_pc
	mov	%l3, %o0		! (argument: type)
	st	%l2, [%sp + CCFSZ + 8]	! set tf.tf_npc
	st	%l4, [%sp + CCFSZ + 12]	! set tf.tf_y
	mov	%l1, %o3		! (argument: pc)
	std	%i0, [%sp + CCFSZ + 48]	! tf.tf_out[0], etc
	std	%i2, [%sp + CCFSZ + 56]
	mov	%l0, %o4		! (argument: psr)
	std	%i4, [%sp + CCFSZ + 64]
	std	%i6, [%sp + CCFSZ + 72]
	call	_C_LABEL(mem_access_fault)	! mem_access_fault(type, ser, sva,
					!		pc, psr, &tf);
	 add	%sp, CCFSZ, %o5		! (argument: &tf)

	ldd	[%sp + CCFSZ + 0], %l0	! load new values
	ldd	[%sp + CCFSZ + 8], %l2
	wr	%l3, 0, %y
	ld	[%sp + CCFSZ + 20], %g1
	ldd	[%sp + CCFSZ + 24], %g2
	ldd	[%sp + CCFSZ + 32], %g4
	ldd	[%sp + CCFSZ + 40], %g6
	ldd	[%sp + CCFSZ + 48], %i0
	ldd	[%sp + CCFSZ + 56], %i2
	ldd	[%sp + CCFSZ + 64], %i4
	ldd	[%sp + CCFSZ + 72], %i6

	b	return_from_trap	! go return
	 wr	%l0, 0, %psr		! (but first disable traps again)


/*
 * fp_exception has to check to see if we are trying to save
 * the FP state, and if so, continue to save the FP state.
 *
 * We do not even bother checking to see if we were in kernel mode,
 * since users have no access to the special_fp_store instruction.
 *
 * This whole idea was stolen from Sprite.
 */
fp_exception:
	set	special_fp_store, %l4	! see if we came from the special one
	cmp	%l1, %l4		! pc == special_fp_store?
	bne	slowtrap		! no, go handle per usual
	 EMPTY
	sethi	%hi(savefpcont), %l4	! yes, "return" to the special code
	or	%lo(savefpcont), %l4, %l4
	jmp	%l4
	 rett	%l4 + 4

/*
 * slowtrap() builds a trap frame and calls trap().
 * This is called `slowtrap' because it *is*....
 * We have to build a full frame for ptrace(), for instance.
 *
 * Registers:
 *	%l0 = %psr
 *	%l1 = return pc
 *	%l2 = return npc
 *	%l3 = trap code
 */
slowtrap:
	TRAP_SETUP(-CCFSZ-80)
	/*
	 * Phew, ready to enable traps and call C code.
	 */
	mov	%l3, %o0		! put type in %o0 for later
Lslowtrap_reenter:
	wr	%l0, PSR_ET, %psr	! traps on again
	std	%l0, [%sp + CCFSZ]	! tf.tf_psr = psr; tf.tf_pc = ret_pc;
	rd	%y, %l3
	std	%l2, [%sp + CCFSZ + 8]	! tf.tf_npc = return_npc; tf.tf_y = %y;
	st	%g1, [%sp + CCFSZ + 20]
	std	%g2, [%sp + CCFSZ + 24]
	std	%g4, [%sp + CCFSZ + 32]
	std	%g6, [%sp + CCFSZ + 40]
	std	%i0, [%sp + CCFSZ + 48]
	mov	%l0, %o1		! (psr)
	std	%i2, [%sp + CCFSZ + 56]
	mov	%l1, %o2		! (pc)
	std	%i4, [%sp + CCFSZ + 64]
	add	%sp, CCFSZ, %o3		! (&tf)
	call	_C_LABEL(trap)			! trap(type, psr, pc, &tf)
	 std	%i6, [%sp + CCFSZ + 72]

	ldd	[%sp + CCFSZ], %l0	! load new values
	ldd	[%sp + CCFSZ + 8], %l2
	wr	%l3, 0, %y
	ld	[%sp + CCFSZ + 20], %g1
	ldd	[%sp + CCFSZ + 24], %g2
	ldd	[%sp + CCFSZ + 32], %g4
	ldd	[%sp + CCFSZ + 40], %g6
	ldd	[%sp + CCFSZ + 48], %i0
	ldd	[%sp + CCFSZ + 56], %i2
	ldd	[%sp + CCFSZ + 64], %i4
	ldd	[%sp + CCFSZ + 72], %i6
	b	return_from_trap
	 wr	%l0, 0, %psr

/*
 * Do a `software' trap by re-entering the trap code, possibly first
 * switching from interrupt stack to kernel stack.  This is used for
 * scheduling and signal ASTs (which generally occur from softclock or
 * tty or net interrupts) and register window saves (which might occur
 * from anywhere).
 *
 * The current window is the trap window, and it is by definition clean.
 * We enter with the trap type in %o0.  All we have to do is jump to
 * Lslowtrap_reenter above, but maybe after switching stacks....
 */
softtrap:
	sethi	%hi(_C_LABEL(eintstack)), %l7
	cmp	%sp, %l7
	bge	Lslowtrap_reenter
	 EMPTY
	sethi	%hi(_C_LABEL(cpcb)), %l6
	ld	[%l6 + %lo(_C_LABEL(cpcb))], %l6
	set	USPACE-CCFSZ-80, %l5
	add	%l6, %l5, %l7
	SET_SP_REDZONE(%l6, %l5)
	b	Lslowtrap_reenter
	 mov	%l7, %sp

#ifdef KGDB
/*
 * bpt is entered on all breakpoint traps.
 * If this is a kernel breakpoint, we do not want to call trap().
 * Among other reasons, this way we can set breakpoints in trap().
 */
bpt:
	btst	PSR_PS, %l0		! breakpoint from kernel?
	bz	slowtrap		! no, go do regular trap
	 nop

	/*
	 * Build a trap frame for kgdb_trap_glue to copy.
	 * Enable traps but set ipl high so that we will not
	 * see interrupts from within breakpoints.
	 */
	TRAP_SETUP(-CCFSZ-80)
	or	%l0, PSR_PIL, %l4	! splhigh()
	wr	%l4, 0, %psr		! the manual claims that this
	wr	%l4, PSR_ET, %psr	! song and dance is necessary
	std	%l0, [%sp + CCFSZ + 0]	! tf.tf_psr, tf.tf_pc
	mov	%l3, %o0		! trap type arg for kgdb_trap_glue
	rd	%y, %l3
	std	%l2, [%sp + CCFSZ + 8]	! tf.tf_npc, tf.tf_y
	rd	%wim, %l3
	st	%l3, [%sp + CCFSZ + 16]	! tf.tf_wim (a kgdb-only r/o field)
	st	%g1, [%sp + CCFSZ + 20]	! tf.tf_global[1]
	std	%g2, [%sp + CCFSZ + 24]	! etc
	std	%g4, [%sp + CCFSZ + 32]
	std	%g6, [%sp + CCFSZ + 40]
	std	%i0, [%sp + CCFSZ + 48]	! tf.tf_in[0..1]
	std	%i2, [%sp + CCFSZ + 56]	! etc
	std	%i4, [%sp + CCFSZ + 64]
	std	%i6, [%sp + CCFSZ + 72]

	/*
	 * Now call kgdb_trap_glue(); if it returns, call trap().
	 */
	mov	%o0, %l3		! gotta save trap type
	call	_C_LABEL(kgdb_trap_glue) ! kgdb_trap_glue(type, &trapframe)
	 add	%sp, CCFSZ, %o1		! (&trapframe)

	/*
	 * Use slowtrap to call trap---but first erase our tracks
	 * (put the registers back the way they were).
	 */
	mov	%l3, %o0		! slowtrap will need trap type
	ld	[%sp + CCFSZ + 12], %l3
	wr	%l3, 0, %y
	ld	[%sp + CCFSZ + 20], %g1
	ldd	[%sp + CCFSZ + 24], %g2
	ldd	[%sp + CCFSZ + 32], %g4
	b	Lslowtrap_reenter
	 ldd	[%sp + CCFSZ + 40], %g6

/*
 * Enter kernel breakpoint.  Write all the windows (not including the
 * current window) into the stack, so that backtrace works.  Copy the
 * supplied trap frame to the kgdb stack and switch stacks.
 *
 * kgdb_trap_glue(type, tf0)
 *	int type;
 *	struct trapframe *tf0;
 */
	.globl	_C_LABEL(kgdb_trap_glue)
_C_LABEL(kgdb_trap_glue):
	save	%sp, -CCFSZ, %sp

	call	_C_LABEL(write_all_windows)
	 mov	%sp, %l4		! %l4 = current %sp

	/* copy trapframe to top of kgdb stack */
	set	_C_LABEL(kgdb_stack) + KGDB_STACK_SIZE - 80, %l0
					! %l0 = tfcopy -> end_of_kgdb_stack
	mov	80, %l1
1:	ldd	[%i1], %l2
	inc	8, %i1
	deccc	8, %l1
	std	%l2, [%l0]
	bg	1b
	 inc	8, %l0

#ifdef DEBUG
	/* save old red zone and then turn it off */
	sethi	%hi(_C_LABEL(redzone)), %l7
	ld	[%l7 + %lo(_C_LABEL(redzone))], %l6
	st	%g0, [%l7 + %lo(_C_LABEL(redzone))]
#endif
	/* switch to kgdb stack */
	add	%l0, -CCFSZ-80, %sp

	/* if (kgdb_trap(type, tfcopy)) kgdb_rett(tfcopy); */
	mov	%i0, %o0
	call	_C_LABEL(kgdb_trap)
	add	%l0, -80, %o1
	tst	%o0
	bnz,a	kgdb_rett
	 add	%l0, -80, %g1

	/*
	 * kgdb_trap() did not handle the trap at all so the stack is
	 * still intact.  A simple `restore' will put everything back,
	 * after we reset the stack pointer.
	 */
	mov	%l4, %sp
#ifdef DEBUG
	st	%l6, [%l7 + %lo(_C_LABEL(redzone))]	! restore red zone
#endif
	ret
	restore

/*
 * Return from kgdb trap.  This is sort of special.
 *
 * We know that kgdb_trap_glue wrote the window above it, so that we will
 * be able to (and are sure to have to) load it up.  We also know that we
 * came from kernel land and can assume that the %fp (%i6) we load here
 * is proper.  We must also be sure not to lower ipl (it is at splhigh())
 * until we have traps disabled, due to the SPARC taking traps at the
 * new ipl before noticing that PSR_ET has been turned off.  We are on
 * the kgdb stack, so this could be disastrous.
 *
 * Note that the trapframe argument in %g1 points into the current stack
 * frame (current window).  We abandon this window when we move %g1->tf_psr
 * into %psr, but we will not have loaded the new %sp yet, so again traps
 * must be disabled.
 */
kgdb_rett:
	rd	%psr, %g4		! turn off traps
	wr	%g4, PSR_ET, %psr
	/* use the three-instruction delay to do something useful */
	ld	[%g1], %g2		! pick up new %psr
	ld	[%g1 + 12], %g3		! set %y
	wr	%g3, 0, %y
#ifdef DEBUG
	st	%l6, [%l7 + %lo(_C_LABEL(redzone))] ! and restore red zone
#endif
	wr	%g0, 0, %wim		! enable window changes
	nop; nop; nop
	/* now safe to set the new psr (changes CWP, leaves traps disabled) */
	wr	%g2, 0, %psr		! set rett psr (including cond codes)
	/* 3 instruction delay before we can use the new window */
/*1*/	ldd	[%g1 + 24], %g2		! set new %g2, %g3
/*2*/	ldd	[%g1 + 32], %g4		! set new %g4, %g5
/*3*/	ldd	[%g1 + 40], %g6		! set new %g6, %g7

	/* now we can use the new window */
	mov	%g1, %l4
	ld	[%l4 + 4], %l1		! get new pc
	ld	[%l4 + 8], %l2		! get new npc
	ld	[%l4 + 20], %g1		! set new %g1

	/* set up returnee's out registers, including its %sp */
	ldd	[%l4 + 48], %i0
	ldd	[%l4 + 56], %i2
	ldd	[%l4 + 64], %i4
	ldd	[%l4 + 72], %i6

	/* load returnee's window, making the window above it be invalid */
	restore
	restore	%g0, 1, %l1		! move to inval window and set %l1 = 1
	rd	%psr, %l0
	sll	%l1, %l0, %l1
	wr	%l1, 0, %wim		! %wim = 1 << (%psr & 31)
	sethi	%hi(_C_LABEL(cpcb)), %l1
	ld	[%l1 + %lo(_C_LABEL(cpcb))], %l1
	and	%l0, 31, %l0		! CWP = %psr & 31;
	st	%l0, [%l1 + PCB_WIM]	! cpcb->pcb_wim = CWP;
	save	%g0, %g0, %g0		! back to window to reload
	LOADWIN(%sp)
	save	%g0, %g0, %g0		! back to trap window
	/* note, we have not altered condition codes; safe to just rett */
	RETT
#endif

/*
 * syscall() builds a trap frame and calls syscall().
 * sun_syscall is same but delivers sun system call number
 * XXX	should not have to save&reload ALL the registers just for
 *	ptrace...
 */
_C_LABEL(_syscall):
	TRAP_SETUP(-CCFSZ-80)
	wr	%l0, PSR_ET, %psr
	std	%l0, [%sp + CCFSZ + 0]	! tf_psr, tf_pc
	rd	%y, %l3
	std	%l2, [%sp + CCFSZ + 8]	! tf_npc, tf_y
	st	%g1, [%sp + CCFSZ + 20]	! tf_g[1]
	std	%g2, [%sp + CCFSZ + 24]	! tf_g[2], tf_g[3]
	std	%g4, [%sp + CCFSZ + 32]	! etc
	std	%g6, [%sp + CCFSZ + 40]
	mov	%g1, %o0		! (code)
	std	%i0, [%sp + CCFSZ + 48]
	add	%sp, CCFSZ, %o1		! (&tf)
	std	%i2, [%sp + CCFSZ + 56]
	mov	%l1, %o2		! (pc)
	std	%i4, [%sp + CCFSZ + 64]
	call	_C_LABEL(syscall)		! syscall(code, &tf, pc, suncompat)
	 std	%i6, [%sp + CCFSZ + 72]
	! now load em all up again, sigh
	ldd	[%sp + CCFSZ + 0], %l0	! new %psr, new pc
	ldd	[%sp + CCFSZ + 8], %l2	! new npc, new %y
	wr	%l3, 0, %y
	/* see `proc_trampoline' for the reason for this label */
return_from_syscall:
	ld	[%sp + CCFSZ + 20], %g1
	ldd	[%sp + CCFSZ + 24], %g2
	ldd	[%sp + CCFSZ + 32], %g4
	ldd	[%sp + CCFSZ + 40], %g6
	ldd	[%sp + CCFSZ + 48], %i0
	ldd	[%sp + CCFSZ + 56], %i2
	ldd	[%sp + CCFSZ + 64], %i4
	ldd	[%sp + CCFSZ + 72], %i6
	b	return_from_trap
	 wr	%l0, 0, %psr

/*
 * Interrupts.  Software interrupts must be cleared from the software
 * interrupt enable register.  Rather than calling intreg_clr_* for each,
 * we do them in-line before enabling traps.
 *
 * After preliminary setup work, the interrupt is passed to each
 * registered handler in turn.  These are expected to return 1 if they
 * took care of the interrupt, 0 if they didn't, and -1 if the device
 * isn't sure.  If a handler claims the interrupt, we exit
 * (hardware interrupts are latched in the requestor so we'll
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
 * Software interrupts are almost the same with three exceptions:
 * (1) we clear the interrupt from the software interrupt enable
 *     register before calling any handler (we have to clear it first
 *     to avoid an interrupt-losing race),
 * (2) we always call all the registered handlers (there is no way
 *     to tell if the single bit in the software interrupt register
 *     represents one or many requests)
 * (3) we never announce a stray interrupt (because of (1), another
 *     interrupt request can come in while we're in the handler.  If
 *     the handler deals with everything for both the original & the
 *     new request, we'll erroneously report a stray interrupt when
 *     we take the software interrupt for the new request).
 *
 * Inputs:
 *	%l0 = %psr
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
 */
softintr_sun44c:
	/*
	 * Entry point for level 1, 4 or 6 interrupts on sun4/sun4c
	 * which may be software interrupts. Check the interrupt
	 * register to see whether we're dealing software or hardware
	 * interrupt.
	 */
	sethi	%hi(INTRREG_VA), %l6
	ldub	[%l6 + %lo(INTRREG_VA)], %l5
	btst	%l5, %l4		! is IE_L{1,4,6} set?
	bz	sparc_interrupt_common	! if not, must be a hw intr
	 andn	%l5, %l4, %l5		! clear soft intr bit
	stb	%l5, [%l6 + %lo(INTRREG_VA)]

softintr_common:
	INTR_SETUP(-CCFSZ-80)
	std	%g2, [%sp + CCFSZ + 24]	! save registers
	INCR(_C_LABEL(uvmexp)+V_SOFTS)	! uvmexp.softs++; (clobbers %o0,%o1)
	mov	%g1, %l7
	rd	%y, %l6
	std	%g4, [%sp + CCFSZ + 32]
	andn	%l0, PSR_PIL, %l4	! %l4 = psr & ~PSR_PIL |
	sll	%l3, 8, %l5		!	intlev << IPLSHIFT
	std	%g6, [%sp + CCFSZ + 40]
	or	%l5, %l4, %l4		!			;
	wr	%l4, 0, %psr		! the manual claims this
	wr	%l4, PSR_ET, %psr	! song and dance is necessary
	std	%l0, [%sp + CCFSZ + 0]	! set up intrframe/clockframe
	sll	%l3, 2, %l5
	std	%l2, [%sp + CCFSZ + 8]
	set	_C_LABEL(sintrhand), %l4	! %l4 = sintrhand[intlev];
	ld	[%l4 + %l5], %l4
	b	3f
	 st	%fp, [%sp + CCFSZ + 16]

1:	ld	[%l4 + SIH_PENDING], %o0
	tst	%o0
	bz	2f			! if (ih->sih_pending != 0)
	 st	%g0, [%l4 + SIH_PENDING]
	rd	%psr, %o1
	ld	[%l4 + IH_IPL], %o0
	and	%o1, ~PSR_PIL, %o1
	wr	%o1, %o0, %psr
	ld	[%l4 + IH_FUN], %o1
	jmpl	%o1, %o7		!	(void)(*ih->ih_fun)(...)
	 ld	[%l4 + IH_ARG], %o0
	mov	%l4, %l3
	ldd	[%l3 + IH_COUNT], %l4
	inccc	%l5
	addx	%l4, 0, %l4
	std	%l4, [%l3 + IH_COUNT]
	mov	%l3, %l4
2:	ld	[%l4 + IH_NEXT], %l4	!	and ih = ih->ih_next
3:	tst	%l4			! while ih != NULL
	bnz	1b
	 nop
	mov	%l7, %g1
	wr	%l6, 0, %y
	ldd	[%sp + CCFSZ + 24], %g2
	ldd	[%sp + CCFSZ + 32], %g4
	ldd	[%sp + CCFSZ + 40], %g6
	b	return_from_trap
	 wr	%l0, 0, %psr

	/*
	 * _sparc_interrupt{44c,4m} is exported for paranoia checking
	 * (see intr.c).
	 */
	.globl	_C_LABEL(sparc_interrupt4m)
_C_LABEL(sparc_interrupt4m):
#if defined(SUN4D) || defined(SUN4M)
	mov	1, %l4
	sethi	%hi(ICR_PI_PEND), %l5
	ld	[%l5 + %lo(ICR_PI_PEND)], %l5	! get pending interrupts
	sll	%l4, %l3, %l4	! hw intr bits are in the lower halfword

	btst	%l4, %l5	! has pending hw intr at this level?
	bnz	sparc_interrupt_common
	 nop

	! both softint pending and clear bits are in upper halfwords of
	! their respective registers so shift the test bit in %l4 up there
	sll	%l4, 16, %l4

	sethi	%hi(ICR_PI_CLR), %l6
	st	%l4, [%l6 + %lo(ICR_PI_CLR)]	! ack soft intr
	/* Drain hw reg; might be necessary for Ross CPUs */
	sethi	%hi(ICR_PI_PEND), %l6
	ld	[%l6 + %lo(ICR_PI_PEND)], %g0

	b,a	softintr_common
#endif	/* SUN4D || SUN4M */

sparc_interrupt_common:
	.globl	_C_LABEL(sparc_interrupt44c)
_C_LABEL(sparc_interrupt44c):
	INTR_SETUP(-CCFSZ-80)
	std	%g2, [%sp + CCFSZ + 24]	! save registers
	INCR(_C_LABEL(uvmexp)+V_INTR)	! cnt.v_intr++; (clobbers %o0,%o1)
	mov	%g1, %l7
	rd	%y, %l6
	std	%g4, [%sp + CCFSZ + 32]
	andn	%l0, PSR_PIL, %l4	! %l4 = psr & ~PSR_PIL |
	sll	%l3, 8, %l5		!	intlev << IPLSHIFT
	std	%g6, [%sp + CCFSZ + 40]
	or	%l5, %l4, %l4		!			;
	wr	%l4, 0, %psr		! the manual claims this
	wr	%l4, PSR_ET, %psr	! song and dance is necessary
	std	%l0, [%sp + CCFSZ + 0]	! set up intrframe/clockframe
	sll	%l3, 2, %l5
	std	%l2, [%sp + CCFSZ + 8]	! set up intrframe/clockframe
	set	_C_LABEL(intrhand), %l4	! %l4 = intrhand[intlev];
	ld	[%l4 + %l5], %l4
	clr	%l5			! %l5 = 0
	b	3f
	 st	%fp, [%sp + CCFSZ + 16]

1:	rd	%psr, %o1
	ld	[%l4 + IH_IPL], %o0
	and	%o1, ~PSR_PIL, %o1
	wr	%o1, %o0, %psr
	ld	[%l4 + IH_ARG], %o0
	ld	[%l4 + IH_FUN], %o1
	tst	%o0
	bz,a	2f
	 add	%sp, CCFSZ, %o0
2:	jmpl	%o1, %o7		!	handled = (*ih->ih_fun)(...)
	 nop
	cmp	%o0, 1
	bge	4f			!	if (handled >= 1) break
	 or	%o0, %l5, %l5		! 	and %l5 |= handled
	ld	[%l4 + IH_NEXT], %l4	!	and ih = ih->ih_next
3:	tst	%l4
	bnz	1b			! while (ih)
	 nop
	tst	%l5			! if (handled) break
	bnz	5f
	 nop
	call	_C_LABEL(strayintr)	!	strayintr(&intrframe)
	 add	%sp, CCFSZ, %o0
5:	/* all done: restore registers and go return */
	mov	%l7, %g1
	wr	%l6, 0, %y
	ldd	[%sp + CCFSZ + 24], %g2
	ldd	[%sp + CCFSZ + 32], %g4
	ldd	[%sp + CCFSZ + 40], %g6
	b	return_from_trap
	 wr	%l0, 0, %psr
4:
	mov	%l4, %l3
	ldd	[%l3 + IH_COUNT], %l4
	inccc	%l5
	addx	%l4, 0, %l4
	b	5b
	 std	%l4, [%l3 + IH_COUNT]

/*
 * Level 15 interrupt.  An async memory error has occurred;
 * take care of it (typically by panicking, but hey...).
 *	%l0 = %psr
 *	%l1 = return pc
 *	%l2 = return npc
 *	%l3 = 15 * 4 (why? just because!)
 *
 * Internal:
 *	%l4 = %y
 *	%l5 = %g1
 *	%l6 = %g6
 *	%l7 = %g7
 *  g2, g3, g4, g5 go to stack
 *
 * This code is almost the same as that in mem_access_fault,
 * except that we already know the problem is not a `normal' fault,
 * and that we must be extra-careful with interrupt enables.
 */

#if defined(SUN4)
nmi_sun4:
	INTR_SETUP(-CCFSZ-80)
	INCR(_C_LABEL(uvmexp)+V_INTR)	! cnt.v_intr++; (clobbers %o0,%o1)
	/*
	 * Level 15 interrupts are nonmaskable, so with traps off,
	 * disable all interrupts to prevent recursion.
	 */
	sethi	%hi(INTRREG_VA), %o0
	ldub	[%o0 + %lo(INTRREG_VA)], %o1
	andn	%o0, IE_ALLIE, %o1
	stb	%o1, [%o0 + %lo(INTRREG_VA)]
	wr	%l0, PSR_ET, %psr	! okay, turn traps on again

	std	%g2, [%sp + CCFSZ + 0]	! save g2, g3
	rd	%y, %l4			! save y

	std	%g4, [%sp + CCFSZ + 8]	! save g4, g5
	mov	%g1, %l5		! save g1, g6, g7
	mov	%g6, %l6
	mov	%g7, %l7
#if defined(SUN4C) || defined(SUN4E)
	b,a	nmi_common
#endif /* SUN4C */
#endif

#if defined(SUN4C) || defined(SUN4E)
nmi_sun4c:
	INTR_SETUP(-CCFSZ-80)
	INCR(_C_LABEL(uvmexp)+V_INTR)		! cnt.v_intr++; (clobbers %o0,%o1)
	/*
	 * Level 15 interrupts are nonmaskable, so with traps off,
	 * disable all interrupts to prevent recursion.
	 */
	sethi	%hi(INTRREG_VA), %o0
	ldub	[%o0 + %lo(INTRREG_VA)], %o1
	andn	%o0, IE_ALLIE, %o1
	stb	%o1, [%o0 + %lo(INTRREG_VA)]
	wr	%l0, PSR_ET, %psr	! okay, turn traps on again

	std	%g2, [%sp + CCFSZ + 0]	! save g2, g3
	rd	%y, %l4			! save y

	! must read the sync error register too.
	set	AC_SYNC_ERR, %o0
	lda	[%o0] ASI_CONTROL, %o1	! sync err reg
	inc	4, %o0
	lda	[%o0] ASI_CONTROL, %o2	! sync virt addr
	std	%g4, [%sp + CCFSZ + 8]	! save g4,g5
	mov	%g1, %l5		! save g1,g6,g7
	mov	%g6, %l6
	mov	%g7, %l7
	inc	4, %o0
	lda	[%o0] ASI_CONTROL, %o3	! async err reg
	inc	4, %o0
	lda	[%o0] ASI_CONTROL, %o4	! async virt addr
#if defined(SUN4)
	!!b,a	nmi_common
#endif /* SUN4 */
#endif /* SUN4C || SUN4E */

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
nmi_common:
	! and call C code
	call	_C_LABEL(memerr4_4c)
	 clr	%o0

	mov	%l5, %g1		! restore g1 through g7
	ldd	[%sp + CCFSZ + 0], %g2
	ldd	[%sp + CCFSZ + 8], %g4
	wr	%l0, 0, %psr		! re-disable traps
	mov	%l6, %g6
	mov	%l7, %g7

	! set IE_ALLIE again (safe, we disabled traps again above)
	sethi	%hi(INTRREG_VA), %o0
	ldub	[%o0 + %lo(INTRREG_VA)], %o1
	or	%o1, IE_ALLIE, %o1
	stb	%o1, [%o0 + %lo(INTRREG_VA)]
	b	return_from_trap
	 wr	%l4, 0, %y		! restore y
#endif	/* SUN4 || SUN4C || SUN4E */

#if defined(SUN4M)
nmi_sun4m:
	INTR_SETUP(-CCFSZ-80)
	INCR(_C_LABEL(uvmexp)+V_INTR)		! cnt.v_intr++; (clobbers %o0,%o1)
	/*
	 * XXX - we don't handle soft nmi, yet.
	 */
	/*
	 * Level 15 interrupts are nonmaskable, so with traps off,
	 * disable all interrupts to prevent recursion.
	 */
	sethi	%hi(ICR_SI_SET), %o0
	set	SINTR_MA, %o1
	st	%o1, [%o0 + %lo(ICR_SI_SET)]

	/* Now clear the NMI */

	sethi	%hi(ICR_PI_CLR), %o0
	set	PINTR_IC, %o1
	st	%o1, [%o0 + %lo(ICR_PI_CLR)]

	wr	%l0, PSR_ET, %psr	! okay, turn traps on again

	std	%g2, [%sp + CCFSZ + 0]	! save g2, g3
	rd	%y, %l4			! save y
	std	%g4, [%sp + CCFSZ + 8]	! save g4,g5

	/* Finish stackframe, call C trap handler */
	mov	%g1, %l5		! save g1,g6,g7
	mov	%g6, %l6
	mov	%g7, %l7

	call	_C_LABEL(nmi_hard)
	 clr	%o5

	mov	%l5, %g1		! restore g1 through g7
	ldd	[%sp + CCFSZ + 0], %g2
	ldd	[%sp + CCFSZ + 8], %g4
	wr	%l0, 0, %psr		! re-disable traps
	mov	%l6, %g6
	mov	%l7, %g7

	! enable interrupts again (safe, we disabled traps again above)
	sethi	%hi(ICR_SI_CLR), %o0
	set	SINTR_MA, %o1
	st	%o1, [%o0 + %lo(ICR_SI_CLR)]

	b	return_from_trap
	 wr	%l4, 0, %y		! restore y
#endif /* SUN4M */

#ifdef GPROF
	.globl	window_of, winof_user
	.globl	window_uf, winuf_user, winuf_ok, winuf_invalid
	.globl	return_from_trap, rft_kernel, rft_user, rft_invalid
	.globl	softtrap, slowtrap
	.globl	clean_trap_window, _C_LABEL(_syscall)
#endif

/*
 * Window overflow trap handler.
 *	%l0 = %psr
 *	%l1 = return pc
 *	%l2 = return npc
 */
window_of:
#ifdef TRIVIAL_WINDOW_OVERFLOW_HANDLER
	/* a trivial version that assumes %sp is ok */
	/* (for testing only!) */
	save	%g0, %g0, %g0
	std	%l0, [%sp + (0*8)]
	rd	%psr, %l0
	mov	1, %l1
	sll	%l1, %l0, %l0
	wr	%l0, 0, %wim
	std	%l2, [%sp + (1*8)]
	std	%l4, [%sp + (2*8)]
	std	%l6, [%sp + (3*8)]
	std	%i0, [%sp + (4*8)]
	std	%i2, [%sp + (5*8)]
	std	%i4, [%sp + (6*8)]
	std	%i6, [%sp + (7*8)]
	restore
	RETT
#else
	/*
	 * This is similar to TRAP_SETUP, but we do not want to spend
	 * a lot of time, so we have separate paths for kernel and user.
	 * We also know for sure that the window has overflowed.
	 */
	btst	PSR_PS, %l0
	bz	winof_user
	 sethi	%hi(clean_trap_window), %l7

	/*
	 * Overflow from kernel mode.  Call clean_trap_window to
	 * do the dirty work, then just return, since we know prev
	 * window is valid.  clean_trap_windows might dump all *user*
	 * windows into the pcb, but we do not care: there is at
	 * least one kernel window (a trap or interrupt frame!)
	 * above us.
	 */
	jmpl	%l7 + %lo(clean_trap_window), %l4
	 mov	%g7, %l7		! for clean_trap_window

	wr	%l0, 0, %psr		! put back the @%*! cond. codes
	nop				! (let them settle in)
	RETT

winof_user:
	/*
	 * Overflow from user mode.
	 * If clean_trap_window dumps the registers into the pcb,
	 * rft_user will need to call trap(), so we need space for
	 * a trap frame.  We also have to compute pcb_nw.
	 *
	 * SHOULD EXPAND IN LINE TO AVOID BUILDING TRAP FRAME ON
	 * `EASY' SAVES
	 */
	sethi	%hi(_C_LABEL(cpcb)), %l6
	ld	[%l6 + %lo(_C_LABEL(cpcb))], %l6
	ld	[%l6 + PCB_WIM], %l5
	and	%l0, 31, %l3
	sub	%l3, %l5, %l5 		/* l5 = CWP - pcb_wim */
	set	uwtab, %l4
	ldub	[%l4 + %l5], %l5	/* l5 = uwtab[l5] */
	st	%l5, [%l6 + PCB_UW]
	jmpl	%l7 + %lo(clean_trap_window), %l4
	 mov	%g7, %l7		! for clean_trap_window
	sethi	%hi(_C_LABEL(cpcb)), %l6
	ld	[%l6 + %lo(_C_LABEL(cpcb))], %l6
	set	USPACE-CCFSZ-80, %l5
	add	%l6, %l5, %sp		/* over to kernel stack */
	CHECK_SP_REDZONE(%l6, %l5)

	/*
	 * Copy return_from_trap far enough to allow us
	 * to jump directly to rft_user_or_recover_pcb_windows
	 * (since we know that is where we are headed).
	 */
!	and	%l0, 31, %l3		! still set (clean_trap_window
					! leaves this register alone)
	set	wmask, %l6
	ldub	[%l6 + %l3], %l5	! %l5 = 1 << ((CWP + 1) % nwindows)
	b	rft_user_or_recover_pcb_windows
	 rd	%wim, %l4		! (read %wim first)
#endif /* end `real' version of window overflow trap handler */

/*
 * Window underflow trap handler.
 *	%l0 = %psr
 *	%l1 = return pc
 *	%l2 = return npc
 *
 * A picture:
 *
 *	  T R I X
 *	0 0 0 1 0 0 0	(%wim)
 * [bit numbers increase towards the right;
 * `restore' moves right & `save' moves left]
 *
 * T is the current (Trap) window, R is the window that attempted
 * a `Restore' instruction, I is the Invalid window, and X is the
 * window we want to make invalid before we return.
 *
 * Since window R is valid, we cannot use rft_user to restore stuff
 * for us.  We have to duplicate its logic.  YUCK.
 *
 * Incidentally, TRIX are for kids.  Silly rabbit!
 */
window_uf:
#ifdef TRIVIAL_WINDOW_UNDERFLOW_HANDLER
	wr	%g0, 0, %wim		! allow us to enter I
	restore				! to R
	nop
	nop
	restore				! to I
	restore	%g0, 1, %l1		! to X
	rd	%psr, %l0
	sll	%l1, %l0, %l0
	wr	%l0, 0, %wim
	save	%g0, %g0, %g0		! back to I
	LOADWIN(%sp)
	save	%g0, %g0, %g0		! back to R
	save	%g0, %g0, %g0		! back to T
	RETT
#else
	wr	%g0, 0, %wim		! allow us to enter I
	btst	PSR_PS, %l0
	restore				! enter window R
	bz	winuf_user
	 restore			! enter window I

	/*
	 * Underflow from kernel mode.  Just recover the
	 * registers and go (except that we have to update
	 * the blasted user pcb fields).
	 */
	restore	%g0, 1, %l1		! enter window X, then set %l1 to 1
	rd	%psr, %l0		! cwp = %psr & 31;
	and	%l0, 31, %l0
	sll	%l1, %l0, %l1		! wim = 1 << cwp;
	wr	%l1, 0, %wim		! setwim(wim);
	sethi	%hi(_C_LABEL(cpcb)), %l1
	ld	[%l1 + %lo(_C_LABEL(cpcb))], %l1
	st	%l0, [%l1 + PCB_WIM]	! cpcb->pcb_wim = cwp;
	save	%g0, %g0, %g0		! back to window I
	LOADWIN(%sp)
	save	%g0, %g0, %g0		! back to R
	save	%g0, %g0, %g0		! and then to T
	wr	%l0, 0, %psr		! fix those cond codes....
	nop				! (let them settle in)
	RETT

winuf_user:
	/*
	 * Underflow from user mode.
	 *
	 * We cannot use rft_user (as noted above) because
	 * we must re-execute the `restore' instruction.
	 * Since it could be, e.g., `restore %l0,0,%l0',
	 * it is not okay to touch R's registers either.
	 *
	 * We are now in window I.
	 */
	btst	7, %sp			! if unaligned, it is invalid
	bne	winuf_invalid
	 EMPTY

	sethi	%hi(_C_LABEL(pgofset)), %l4
	ld	[%l4 + %lo(_C_LABEL(pgofset))], %l4
	PTE_OF_ADDR(%sp, %l7, winuf_invalid, %l4, NOP_ON_4M_5)
	CMP_PTE_USER_READ(%l7, %l5, NOP_ON_4M_6) ! if first page not readable,
	bne	winuf_invalid		! it is invalid
	 EMPTY
	SLT_IF_1PAGE_RW(%sp, %l7, %l4)	! first page is readable
	bl,a	winuf_ok		! if only one page, enter window X
	 restore %g0, 1, %l1		! and goto ok, & set %l1 to 1
	add	%sp, 7*8, %l5
	add     %l4, 62, %l4
	PTE_OF_ADDR(%l5, %l7, winuf_invalid, %l4, NOP_ON_4M_7)
	CMP_PTE_USER_READ(%l7, %l5, NOP_ON_4M_8) ! check second page too
	be,a	winuf_ok		! enter window X and goto ok
	 restore %g0, 1, %l1		! (and then set %l1 to 1)

winuf_invalid:
	/*
	 * We were unable to restore the window because %sp
	 * is invalid or paged out.  Return to the trap window
	 * and call trap(T_WINUF).  This will save R to the user
	 * stack, then load both R and I into the pcb rw[] area,
	 * and return with pcb_nsaved set to -1 for success, 0 for
	 * failure.  `Failure' indicates that someone goofed with the
	 * trap registers (e.g., signals), so that we need to return
	 * from the trap as from a syscall (probably to a signal handler)
	 * and let it retry the restore instruction later.  Note that
	 * window R will have been pushed out to user space, and thus
	 * be the invalid window, by the time we get back here.  (We
	 * continue to label it R anyway.)  We must also set %wim again,
	 * and set pcb_uw to 1, before enabling traps.  (Window R is the
	 * only window, and it is a user window).
	 */
	save	%g0, %g0, %g0		! back to R
	save	%g0, 1, %l4		! back to T, then %l4 = 1
	sethi	%hi(_C_LABEL(cpcb)), %l6
	ld	[%l6 + %lo(_C_LABEL(cpcb))], %l6
	st	%l4, [%l6 + PCB_UW]	! pcb_uw = 1
	ld	[%l6 + PCB_WIM], %l5	! get log2(%wim)
	sll	%l4, %l5, %l4		! %l4 = old %wim
	wr	%l4, 0, %wim		! window I is now invalid again
	set	USPACE-CCFSZ-80, %l5
	add	%l6, %l5, %sp		! get onto kernel stack
	nop
	CHECK_SP_REDZONE(%l6, %l5)

	/*
	 * Okay, call trap(T_WINUF, psr, pc, &tf).
	 * See `slowtrap' above for operation.
	 */
	wr	%l0, PSR_ET, %psr
	std	%l0, [%sp + CCFSZ + 0]	! tf.tf_psr, tf.tf_pc
	rd	%y, %l3
	std	%l2, [%sp + CCFSZ + 8]	! tf.tf_npc, tf.tf_y
	mov	T_WINUF, %o0
	st	%g1, [%sp + CCFSZ + 20]	! tf.tf_global[1]
	mov	%l0, %o1
	std	%g2, [%sp + CCFSZ + 24]	! etc
	mov	%l1, %o2
	std	%g4, [%sp + CCFSZ + 32]
	add	%sp, CCFSZ, %o3
	std	%g6, [%sp + CCFSZ + 40]
	std	%i0, [%sp + CCFSZ + 48]	! tf.tf_out[0], etc
	std	%i2, [%sp + CCFSZ + 56]
	std	%i4, [%sp + CCFSZ + 64]
	call	_C_LABEL(trap)			! trap(T_WINUF, pc, psr, &tf)
	 std	%i6, [%sp + CCFSZ + 72]	! tf.tf_out[6]

	ldd	[%sp + CCFSZ + 0], %l0	! new psr, pc
	ldd	[%sp + CCFSZ + 8], %l2	! new npc, %y
	wr	%l3, 0, %y
	ld	[%sp + CCFSZ + 20], %g1
	ldd	[%sp + CCFSZ + 24], %g2
	ldd	[%sp + CCFSZ + 32], %g4
	ldd	[%sp + CCFSZ + 40], %g6
	ldd	[%sp + CCFSZ + 48], %i0	! %o0 for window R, etc
	ldd	[%sp + CCFSZ + 56], %i2
	ldd	[%sp + CCFSZ + 64], %i4
	wr	%l0, 0, %psr		! disable traps: test must be atomic
	ldd	[%sp + CCFSZ + 72], %i6
	sethi	%hi(_C_LABEL(cpcb)), %l6
	ld	[%l6 + %lo(_C_LABEL(cpcb))], %l6
	ld	[%l6 + PCB_NSAVED], %l7	! if nsaved is -1, we have our regs
	tst	%l7
	bl,a	1f			! got them
	 wr	%g0, 0, %wim		! allow us to enter windows R, I
	b,a	return_from_trap

	/*
	 * Got 'em.  Load 'em up.
	 */
1:
	mov	%g6, %l3		! save %g6; set %g6 = cpcb
	mov	%l6, %g6
	st	%g0, [%g6 + PCB_NSAVED]	! and clear magic flag
	restore				! from T to R
	restore				! from R to I
	restore	%g0, 1, %l1		! from I to X, then %l1 = 1
	rd	%psr, %l0		! cwp = %psr;
	sll	%l1, %l0, %l1
	wr	%l1, 0, %wim		! make window X invalid
	and	%l0, 31, %l0
	st	%l0, [%g6 + PCB_WIM]	! cpcb->pcb_wim = cwp;
	nop				! unnecessary? old wim was 0...
	save	%g0, %g0, %g0		! back to I

	!!LOADWIN(%g6 + PCB_RW + 64)	! load from rw[1]

	!! StackGhost Decrypt  (PCP)
	! pcb already dereferenced in %g6
	ld	[%g6 + PCB_WCOOKIE], %l0	! get window cookie
	ldd	[%g6 + PCB_RW + 64 + 56], %i6
	xor	%l0, %i7, %i7			! remove cookie

	ldd	[%g6 + PCB_RW + 64], %l0	! load from rw[1]
	ldd	[%g6 + PCB_RW + 64 + 8], %l2
	ldd	[%g6 + PCB_RW + 64 + 16], %l4
	ldd	[%g6 + PCB_RW + 64 + 24], %l6
	ldd	[%g6 + PCB_RW + 64 + 32], %i0
	ldd	[%g6 + PCB_RW + 64 + 40], %i2
	ldd	[%g6 + PCB_RW + 64 + 48], %i4

	save	%g0, %g0, %g0		! back to R

	!! StackGhost Decrypt  (PCP)
	! pcb already dereferenced in %g6
	! (If I was sober, I could potentially re-use the cookie from above)
	ld	[%g6 + PCB_WCOOKIE], %l0	! get window cookie
	ldd	[%g6 + PCB_RW + 56], %i6
	xor	%l0, %i7, %i7			! remove cookie

	!!LOADWIN(%g6 + PCB_RW)		! load from rw[0]
	ldd	[%g6 + PCB_RW], %l0	! load from rw[0]
	ldd	[%g6 + PCB_RW + 8], %l2
	ldd	[%g6 + PCB_RW + 16], %l4
	ldd	[%g6 + PCB_RW + 24], %l6
	ldd	[%g6 + PCB_RW + 32], %i0
	ldd	[%g6 + PCB_RW + 40], %i2
	ldd	[%g6 + PCB_RW + 48], %i4

	save	%g0, %g0, %g0		! back to T

	wr	%l0, 0, %psr		! restore condition codes
	mov	%l3, %g6		! fix %g6
	RETT

	/*
	 * Restoring from user stack, but everything has checked out
	 * as good.  We are now in window X, and %l1 = 1.  Window R
	 * is still valid and holds user values.
	 */
winuf_ok:
	rd	%psr, %l0
	sll	%l1, %l0, %l1
	wr	%l1, 0, %wim		! make this one invalid
	sethi	%hi(_C_LABEL(cpcb)), %l2
	ld	[%l2 + %lo(_C_LABEL(cpcb))], %l2
	and	%l0, 31, %l0
	st	%l0, [%l2 + PCB_WIM]	! cpcb->pcb_wim = cwp;
	save	%g0, %g0, %g0		! back to I

	!! StackGhost Decrypt
	sethi	%hi(_C_LABEL(cpcb)), %l0			! get current *pcb
	ld	[%l0 + %lo(_C_LABEL(cpcb))], %l1		! dereference *pcb
	ld	[%l1 + PCB_WCOOKIE], %l0	! get window cookie
	ldd	[%sp + 56], %i6			! get saved return pointer
	xor	%l0, %i7, %i7			! remove cookie

	!!LOADWIN(%sp)
	ldd	[%sp], %l0
	ldd	[%sp + 8], %l2
	ldd	[%sp + 16], %l4
	ldd	[%sp + 24], %l6
	ldd	[%sp + 32], %i0
	ldd	[%sp + 40], %i2
	ldd	[%sp + 48], %i4


	save	%g0, %g0, %g0		! back to R
	save	%g0, %g0, %g0		! back to T
	wr	%l0, 0, %psr		! restore condition codes
	nop				! it takes three to tangle
	RETT
#endif /* end `real' version of window underflow trap handler */

/*
 * Various return-from-trap routines (see return_from_trap).
 */

/*
 * Return from trap, to kernel.
 *	%l0 = %psr
 *	%l1 = return pc
 *	%l2 = return npc
 *	%l4 = %wim
 *	%l5 = bit for previous window
 */
rft_kernel:
	btst	%l5, %l4		! if (wim & l5)
	bnz	1f			!	goto reload;
	 wr	%l0, 0, %psr		! but first put !@#*% cond codes back

	/* previous window is valid; just rett */
	nop				! wait for cond codes to settle in
	RETT

	/*
	 * Previous window is invalid.
	 * Update %wim and then reload l0..i7 from frame.
	 *
	 *	  T I X
	 *	0 0 1 0 0   (%wim)
	 * [see picture in window_uf handler]
	 *
	 * T is the current (Trap) window, I is the Invalid window,
	 * and X is the window we want to make invalid.  Window X
	 * currently has no useful values.
	 */
1:
	wr	%g0, 0, %wim		! allow us to enter window I
	nop; nop; nop			! (it takes a while)
	restore				! enter window I
	restore	%g0, 1, %l1		! enter window X, then %l1 = 1
	rd	%psr, %l0		! CWP = %psr & 31;
	and	%l0, 31, %l0
	sll	%l1, %l0, %l1		! wim = 1 << CWP;
	wr	%l1, 0, %wim		! setwim(wim);
	sethi	%hi(_C_LABEL(cpcb)), %l1
	ld	[%l1 + %lo(_C_LABEL(cpcb))], %l1
	st	%l0, [%l1 + PCB_WIM]	! cpcb->pcb_wim = l0 & 31;
	save	%g0, %g0, %g0		! back to window I
	LOADWIN(%sp)
	save	%g0, %g0, %g0		! back to window T
	/*
	 * Note that the condition codes are still set from
	 * the code at rft_kernel; we can simply return.
	 */
	RETT

/*
 * Return from trap, to user.  Checks for scheduling trap (`ast') first;
 * will re-enter trap() if set.  Note that we may have to switch from
 * the interrupt stack to the kernel stack in this case.
 *	%l0 = %psr
 *	%l1 = return pc
 *	%l2 = return npc
 *	%l4 = %wim
 *	%l5 = bit for previous window
 *	%l6 = cpcb
 * If returning to a valid window, just set psr and return.
 */
rft_user:
!	sethi	%hi(_C_LABEL(want_ast)), %l7	! (done below)
	ld	[%l7 + %lo(_C_LABEL(want_ast))], %l7
	tst	%l7			! want AST trap?
	bne,a	softtrap		! yes, re-enter trap with type T_AST
	 mov	T_AST, %o0

	btst	%l5, %l4		! if (wim & l5)
	bnz	1f			!	goto reload;
	 wr	%l0, 0, %psr		! restore cond codes
	nop				! (three instruction delay)
	RETT

	/*
	 * Previous window is invalid.
	 * Before we try to load it, we must verify its stack pointer.
	 * This is much like the underflow handler, but a bit easier
	 * since we can use our own local registers.
	 */
1:
	btst	7, %fp			! if unaligned, address is invalid
	bne	rft_invalid
	 EMPTY

	sethi	%hi(_C_LABEL(pgofset)), %l3
	ld	[%l3 + %lo(_C_LABEL(pgofset))], %l3
	PTE_OF_ADDR(%fp, %l7, rft_invalid, %l3, NOP_ON_4M_9)
	CMP_PTE_USER_READ(%l7, %l5, NOP_ON_4M_10)	! try first page
	bne	rft_invalid		! no good
	 EMPTY
	SLT_IF_1PAGE_RW(%fp, %l7, %l3)
	bl,a	rft_user_ok		! only 1 page: ok
	 wr	%g0, 0, %wim
	add	%fp, 7*8, %l5
	add	%l3, 62, %l3
	PTE_OF_ADDR(%l5, %l7, rft_invalid, %l3, NOP_ON_4M_11)
	CMP_PTE_USER_READ(%l7, %l5, NOP_ON_4M_12)	! check 2nd page too
	be,a	rft_user_ok
	 wr	%g0, 0, %wim

	/*
	 * The window we wanted to pull could not be pulled.  Instead,
	 * re-enter trap with type T_RWRET.  This will pull the window
	 * into cpcb->pcb_rw[0] and set cpcb->pcb_nsaved to -1, which we
	 * will detect when we try to return again.
	 */
rft_invalid:
	b	softtrap
	 mov	T_RWRET, %o0

	/*
	 * The window we want to pull can be pulled directly.
	 */
rft_user_ok:
!	wr	%g0, 0, %wim		! allow us to get into it
	wr	%l0, 0, %psr		! fix up the cond codes now
	nop; nop; nop
	restore				! enter window I
	restore	%g0, 1, %l1		! enter window X, then %l1 = 1
	rd	%psr, %l0		! l0 = (junk << 5) + CWP;
	sll	%l1, %l0, %l1		! %wim = 1 << CWP;
	wr	%l1, 0, %wim
	sethi	%hi(_C_LABEL(cpcb)), %l1
	ld	[%l1 + %lo(_C_LABEL(cpcb))], %l1
	and	%l0, 31, %l0
	st	%l0, [%l1 + PCB_WIM]	! cpcb->pcb_wim = l0 & 31;
	save	%g0, %g0, %g0		! back to window I

	!! StackGhost Decrypt
	sethi	%hi(_C_LABEL(cpcb)), %l0			! get current *pcb
	ld	[%l0 + %lo(_C_LABEL(cpcb))], %l1		! dereference *pcb
	ld	[%l1 + PCB_WCOOKIE], %l0	! get window cookie
	ldd	[%sp + 56], %i6			! get saved return pointer
	xor	%l0, %i7, %i7			! remove cookie

	!!LOADWIN(%sp)			! suck hard
	ldd	[%sp], %l0
	ldd	[%sp + 8], %l2
	ldd	[%sp + 16], %l4
	ldd	[%sp + 24], %l6
	ldd	[%sp + 32], %i0
	ldd	[%sp + 40], %i2
	ldd	[%sp + 48], %i4

	save	%g0, %g0, %g0		! back to window T
	RETT

/*
 * Return from trap.  Entered after a
 *	wr	%l0, 0, %psr
 * which disables traps so that we can rett; registers are:
 *
 *	%l0 = %psr
 *	%l1 = return pc
 *	%l2 = return npc
 *
 * (%l3..%l7 anything).
 *
 * If we are returning to user code, we must:
 *  1.  Check for register windows in the pcb that belong on the stack.
 *	If there are any, reenter trap with type T_WINOF.
 *  2.  Make sure the register windows will not underflow.  This is
 *	much easier in kernel mode....
 */
return_from_trap:
!	wr	%l0, 0, %psr		! disable traps so we can rett
! (someone else did this already)
	and	%l0, 31, %l5
	set	wmask, %l6
	ldub	[%l6 + %l5], %l5	! %l5 = 1 << ((CWP + 1) % nwindows)
	btst	PSR_PS, %l0		! returning to userland?
	bnz	rft_kernel		! no, go return to kernel
	 rd	%wim, %l4		! (read %wim in any case)

rft_user_or_recover_pcb_windows:
	/*
	 * (entered with %l4=%wim, %l5=wmask[cwp]; %l0..%l2 as usual)
	 *
	 * check cpcb->pcb_nsaved:
	 * if 0, do a `normal' return to user (see rft_user);
	 * if > 0, cpcb->pcb_rw[] holds registers to be copied to stack;
	 * if -1, cpcb->pcb_rw[0] holds user registers for rett window
	 * from an earlier T_RWRET pseudo-trap.
	 */
	sethi	%hi(_C_LABEL(cpcb)), %l6
	ld	[%l6 + %lo(_C_LABEL(cpcb))], %l6
	ld	[%l6 + PCB_NSAVED], %l7
	tst	%l7
	bz,a	rft_user
	 sethi	%hi(_C_LABEL(want_ast)), %l7	! first instr of rft_user

	bg,a	softtrap		! if (pcb_nsaved > 0)
	 mov	T_WINOF, %o0		!	trap(T_WINOF);

	/*
	 * To get here, we must have tried to return from a previous
	 * trap and discovered that it would cause a window underflow.
	 * We then must have tried to pull the registers out of the
	 * user stack (from the address in %fp==%i6) and discovered
	 * that it was either unaligned or not loaded in memory, and
	 * therefore we ran a trap(T_RWRET), which loaded one set of
	 * registers into cpcb->pcb_pcb_rw[0] (if it had killed the
	 * process due to a bad stack, we would not be here).
	 *
	 * We want to load pcb_rw[0] into the previous window, which
	 * we know is currently invalid.  In other words, we want
	 * %wim to be 1 << ((cwp + 2) % nwindows).
	 */
	wr	%g0, 0, %wim		! enable restores
	mov	%g6, %l3		! save g6 in l3
	mov	%l6, %g6		! set g6 = &u
	st	%g0, [%g6 + PCB_NSAVED]	! clear cpcb->pcb_nsaved
	restore				! enter window I
	restore	%g0, 1, %l1		! enter window X, then %l1 = 1
	rd	%psr, %l0
	sll	%l1, %l0, %l1		! %wim = 1 << CWP;
	wr	%l1, 0, %wim
	and	%l0, 31, %l0
	st	%l0, [%g6 + PCB_WIM]	! cpcb->pcb_wim = CWP;
	nop				! unnecessary? old wim was 0...
	save	%g0, %g0, %g0		! back to window I

	!! StackGhost Decrypt (PCB)
	! pcb already deferenced in %g6
	ld	[%g6 + PCB_WCOOKIE], %l0	! get window cookie
	ldd	[%g6 + PCB_RW + 56], %i6	! get saved return pointer
	xor	%l0, %i7, %i7			! remove cookie

	!LOADWIN(%g6 + PCB_RW)
	ldd	[%g6 + PCB_RW], %l0
	ldd	[%g6 + PCB_RW + 8], %l2
	ldd	[%g6 + PCB_RW + 16], %l4
	ldd	[%g6 + PCB_RW + 24], %l6
	ldd	[%g6 + PCB_RW + 32], %i0
	ldd	[%g6 + PCB_RW + 40], %i2
	ldd	[%g6 + PCB_RW + 48], %i4

	save	%g0, %g0, %g0		! back to window T (trap window)
	wr	%l0, 0, %psr		! cond codes, cond codes everywhere
	mov	%l3, %g6		! restore g6
	RETT

! exported end marker for kernel gdb
	.globl	_C_LABEL(endtrapcode)
_C_LABEL(endtrapcode):

/*
 * init_tables(nwin) int nwin;
 *
 * Set up the uwtab and wmask tables.
 * We know nwin > 1.
 */
init_tables:
	/*
	 * for (i = -nwin, j = nwin - 2; ++i < 0; j--)
	 *	uwtab[i] = j;
	 * (loop runs at least once)
	 */
	set	uwtab, %o3
	sub	%g0, %o0, %o1		! i = -nwin + 1
	inc	%o1
	add	%o0, -2, %o2		! j = nwin - 2;
0:
	stb	%o2, [%o3 + %o1]	! uwtab[i] = j;
1:
	inccc	%o1			! ++i < 0?
	bl	0b			! yes, continue loop
	 dec	%o2			! in any case, j--

	/*
	 * (i now equals 0)
	 * for (j = nwin - 1; i < nwin; i++, j--)
	 *	uwtab[i] = j;
	 * (loop runs at least twice)
	 */
	sub	%o0, 1, %o2		! j = nwin - 1
0:
	stb	%o2, [%o3 + %o1]	! uwtab[i] = j
	inc	%o1			! i++
1:
	cmp	%o1, %o0		! i < nwin?
	bl	0b			! yes, continue
	 dec	%o2			! in any case, j--

	/*
	 * We observe that, for i in 0..nwin-2, (i+1)%nwin == i+1;
	 * for i==nwin-1, (i+1)%nwin == 0.
	 * To avoid adding 1, we run i from 1 to nwin and set
	 * wmask[i-1].
	 *
	 * for (i = j = 1; i < nwin; i++) {
	 *	j <<= 1;	(j now == 1 << i)
	 *	wmask[i - 1] = j;
	 * }
	 * (loop runs at least once)
	 */
	set	wmask - 1, %o3
	mov	1, %o1			! i = 1;
	mov	2, %o2			! j = 2;
0:
	stb	%o2, [%o3 + %o1]	! (wmask - 1)[i] = j;
	inc	%o1			! i++
	cmp	%o1, %o0		! i < nwin?
	bl,a	0b			! yes, continue
	 sll	%o2, 1, %o2		! (and j <<= 1)

	/*
	 * Now i==nwin, so we want wmask[i-1] = 1.
	 */
	mov	1, %o2			! j = 1;
	retl
	 stb	%o2, [%o3 + %o1]	! (wmask - 1)[i] = j;

#ifdef SUN4
/*
 * getidprom(struct idprom *, sizeof(struct idprom))
 */
	.global _C_LABEL(getidprom)
_C_LABEL(getidprom):
	set	AC_IDPROM, %o2
1:	lduba	[%o2] ASI_CONTROL, %o3
	stb	%o3, [%o0]
	inc	%o0
	inc	%o2
	dec	%o1
	cmp	%o1, 0
	bne	1b
	 nop
	retl
	 nop
#endif

dostart:
	/*
	 * Startup.
	 *
	 * We may have been loaded in low RAM, at some address which
	 * is page aligned (PROM_LOADADDR actually) rather than where we
	 * want to run (KERNBASE+PROM_LOADADDR).  Until we get everything set,
	 * we have to be sure to use only pc-relative addressing.
	 */

	/*
	 * Find out if the above is the case.
	 */
0:	call	1f
	 sethi	%hi(0b), %l0		! %l0 = virtual address of 0:
1:	or	%l0, %lo(0b), %l0
	sub	%l0, %o7, %l7		! subtract actual physical address of 0:

	/*
	 * If we're already running at our desired virtual load address,
	 * %l7 will be set to 0, otherwise it will be KERNBASE.
	 * From now on until the end of locore bootstrap code, %l7 will
	 * be used to relocate memory references.
	 */
#define	RELOCATE(l,r)		\
	set	l, r;		\
	sub	r, %l7, r

#if defined(DDB) || NKSYMS > 0
	/*
	 * First, check for DDB arguments. The loader passes `_esym' in %o4.
	 * A DDB magic number is passed in %o5 to allow for bootloaders
	 * that know nothing about DDB symbol loading conventions.
	 * Note: we don't touch %o1-%o3; SunOS bootloaders seem to use them
	 * for their own mirky business.
	 *
	 * Pre-NetBSD 1.3 bootblocks had KERNBASE compiled in, and used
	 * it to compute the value of `_esym'. In order to successfully
	 * boot a kernel built with a different value for KERNBASE using
	 * old bootblocks, we fixup `_esym' here by the difference between
	 * KERNBASE and the old value (known to be 0xf8000000) compiled
	 * into pre-1.3 bootblocks.
	 * We use the magic number passed as the sixth argument to
	 * distinguish bootblock versions.
	 */
	mov	%g0, %l4
	set	0x44444231, %l3
	cmp	%o5, %l3		! chk magic
	be	1f

	set	0x44444230, %l3
	cmp	%o5, %l3		! chk compat magic
	bne	2f

	set	KERNBASE, %l4		! compat magic found
	set	0xf8000000, %l5		! compute correction term:
	sub	%l5, %l4, %l4		!  old KERNBASE (0xf8000000 ) - KERNBASE

1:
	tst	%o4			! do we have the symbols?
	bz	2f
	 sub	%o4, %l4, %o4		! apply compat correction
	
	RELOCATE(_C_LABEL(esym), %l3)
	st	%o4, [%l3]		! store _esym
2:
#endif
	/*
	 * Sun4 passes in the `load address'.  Although possible, its highly
	 * unlikely that OpenBoot would place the prom vector there.
	 */
	set	0x4000, %g7
	cmp	%o0, %g7
	be	is_sun4
	 nop

#if defined(SUN4C) || defined(SUN4D) || defined(SUN4E) || defined(SUN4M)
	mov	%o0, %g7		! save prom vector pointer

	/*
	 * are we on a sun4c, sun4d, sun4e or a sun4m?
	 */
	ld	[%g7 + PV_NODEOPS], %o4	! node = pv->pv_nodeops->no_nextnode(0)
	ld	[%o4 + NO_NEXTNODE], %o4
	call	%o4
	 mov	0, %o0			! node

	!mov	%o0, %l0
	RELOCATE(_C_LABEL(cputypvar), %o1) ! name = "compatible"
	RELOCATE(_C_LABEL(cputypval), %l2) ! buffer ptr (assume buffer long enough)
	ld	[%g7 + PV_NODEOPS], %o4	! (void)pv->pv_nodeops->no_getprop(...)
	ld	[%o4 + NO_GETPROP], %o4
	call	 %o4
	 mov	%l2, %o2
	!RELOCATE(_C_LABEL(cputypval), %l2)	! buffer ptr
	ldub	[%l2 + 4], %o0		! which is it... "sun4c", "sun4m", "sun4d"?
	cmp	%o0, 'c'
	be	is_sun4c
	 nop
	cmp	%o0, 'd'
	be	is_sun4d
	 nop
	cmp	%o0, 'm'
	be	is_sun4m
	 nop
#endif /* SUN4C || SUN4D || SUN4E || SUN4M */

	! ``on a sun4u?  hell no!''
	ld	[%g7 + PV_HALT], %o1	! by this kernel, then halt
	call	%o1
	 nop

is_sun4m:
#if defined(SUN4M)
	set	trapbase_sun4m, %g6
	mov	SUN4CM_PGSHIFT, %g5
	b	start_havetype
	 mov	CPU_SUN4M, %g4
#else
	RELOCATE(sun4m_notsup, %o0)
	ld	[%g7 + PV_EVAL], %o1
	call	%o1			! print a message saying that the
	 nop				! sun4m architecture is not supported
	ld	[%g7 + PV_HALT], %o1	! by this kernel, then halt
	call	%o1
	 nop
	/*NOTREACHED*/
#endif
is_sun4d:
#if defined(SUN4D)
	set	trapbase_sun4m, %g6
	mov	SUN4CM_PGSHIFT, %g5
	b	start_havetype
	 mov	CPU_SUN4D, %g4
#else
	RELOCATE(sun4d_notsup, %o0)
	ld	[%g7 + PV_EVAL], %o1
	call	%o1			! print a message saying that the
	 nop				! sun4d architecture is not supported
	ld	[%g7 + PV_HALT], %o1	! by this kernel, then halt
	call	%o1
	 nop
	/*NOTREACHED*/
#endif
is_sun4c:
#if defined(SUN4C) || defined(SUN4E)
	/*
	 * Assume sun4c for now; bootstrap() will check for the pagesize
	 * and will update the page size variables as well as cputyp.
	 */
	set	trapbase_sun4c, %g6
	mov	SUN4CM_PGSHIFT, %g5

	set	AC_CONTEXT, %g1		! paranoia: set context to kernel
	stba	%g0, [%g1] ASI_CONTROL

	b	start_havetype
	 mov	CPU_SUN4C, %g4
#else
	RELOCATE(sun4c_notsup, %o0)

	ld	[%g7 + PV_ROMVEC_VERS], %o1
	cmp	%o1, 0
	bne	1f
	 nop

	! stupid version 0 rom interface is pv_eval(int length, char *string)
	mov	%o0, %o1
2:	ldub	[%o0], %o4
	tst	%o4
	bne	2b
	 inc	%o0
	dec	%o0
	sub	%o0, %o1, %o0

1:	ld	[%g7 + PV_EVAL], %o2
	call	%o2			! print a message saying that the
	 nop				! sun4c architecture is not supported
	ld	[%g7 + PV_HALT], %o1	! by this kernel, then halt
	call	%o1
	 nop
	/*NOTREACHED*/
#endif
is_sun4:
#if defined(SUN4)
	set	trapbase_sun4, %g6
	mov	SUN4_PGSHIFT, %g5

	set	AC_CONTEXT, %g1		! paranoia: set context to kernel
	stba	%g0, [%g1] ASI_CONTROL

	b	start_havetype
	 mov	CPU_SUN4, %g4
#else
	set	PROM_BASE, %g7

	RELOCATE(sun4_notsup, %o0)
	ld	[%g7 + OLDMON_PRINTF], %o1
	call	%o1			! print a message saying that the
	 nop				! sun4 architecture is not supported
	ld	[%g7 + OLDMON_HALT], %o1 ! by this kernel, then halt
	call	%o1
	 nop
	/*NOTREACHED*/
#endif

start_havetype:
	cmp	%l7, 0
	be	startmap_done

	/*
	 * Step 1: double map low RAM (addresses [0.._end-start-1])
	 * to KERNBASE (addresses [KERNBASE.._end-1]).  None of these
	 * are `bad' aliases (since they are all on segment boundaries)
	 * so we do not have to worry about cache aliasing.
	 *
	 * We map in another couple of segments just to have some
	 * more memory (512K, actually) guaranteed available for
	 * bootstrap code (pmap_bootstrap needs memory to hold MMU
	 * and context data structures). Note: this is only relevant
	 * for 2-level MMU sun4/sun4c machines.
	 */
	clr	%l0			! lowva
	set	KERNBASE, %l1		! highva
	set	_C_LABEL(end) + (2 << 18), %l2	! last va that must be remapped
#if defined(DDB) || NKSYMS > 0
	sethi	%hi(_C_LABEL(esym) - KERNBASE), %o1
	ld	[%o1+%lo(_C_LABEL(esym) - KERNBASE)], %o1
	tst	%o1
	bz	1f
	 nop
	set	(2 << 18), %l2
	add	%l2, %o1, %l2		! last va that must be remapped
1:
#endif
	/*
	 * Need different initial mapping functions for different
	 * types of machines.
	 */
#if defined(SUN4C) || defined(SUN4E)
	cmp	%g4, CPU_SUN4C
	bne	1f
	 set	1 << 18, %l3		! segment size in bytes
0:
	lduba	[%l0] ASI_SEGMAP, %l4	! segmap[highva] = segmap[lowva];
	stba	%l4, [%l1] ASI_SEGMAP
	add	%l3, %l1, %l1		! highva += segsiz;
	cmp	%l1, %l2		! done?
	blu	0b			! no, loop
	 add	%l3, %l0, %l0		! (and lowva += segsz)
	b,a	startmap_done
1:
#endif /* SUN4C || SUN4E */

#if defined(SUN4)
	cmp	%g4, CPU_SUN4
	bne	2f
#if defined(MMU_3L)
	set	AC_IDPROM+1, %l3
	lduba	[%l3] ASI_CONTROL, %l3
	cmp	%l3, 0x24 ! XXX - SUN4_400
	bne	no_3mmu
	 nop

	/*
	 * Three-level sun4 MMU.
	 * Double-map by duplicating a single region entry (which covers
	 * 16MB) corresponding to the kernel's virtual load address.
	 */
	add	%l0, 2, %l0		! get to proper half-word in RG space
	add	%l1, 2, %l1
	lduha	[%l0] ASI_REGMAP, %l4	! regmap[highva] = regmap[lowva];
	stha	%l4, [%l1] ASI_REGMAP
	b,a	startmap_done
no_3mmu:
#endif

	/*
	 * Two-level sun4 MMU.
	 * Double-map by duplicating the required number of segment
	 * entries corresponding to the kernel's virtual load address.
	 */
	 set	1 << 18, %l3		! segment size in bytes
0:
	lduha	[%l0] ASI_SEGMAP, %l4	! segmap[highva] = segmap[lowva];
	stha	%l4, [%l1] ASI_SEGMAP
	add	%l3, %l1, %l1		! highva += segsiz;
	cmp	%l1, %l2		! done?
	blu	0b			! no, loop
	 add	%l3, %l0, %l0		! (and lowva += segsz)
	b,a	startmap_done
2:
#endif /* SUN4 */
#if defined(SUN4M)
	cmp	%g4, CPU_SUN4M		! skip for sun4m!
	bne	3f

	/*
	 * The OBP guarantees us a 16MB mapping using a level 1 PTE at
	 * 0x0.  All we have to do is copy the entry.  Also, we must
	 * check to see if we have a TI Viking in non-mbus mode, and
	 * if so do appropriate flipping and turning off traps before
	 * we dork with MMU passthrough.  -grrr
	 */

	sethi	%hi(0x40000000), %o1	! TI version bit
	rd	%psr, %o0
	andcc	%o0, %o1, %g0
	be	remap_notvik		! is non-TI normal MBUS module
	lda	[%g0] ASI_SRMMU, %o0	! load MMU
	andcc	%o0, 0x800, %g0
	bne	remap_notvik		! It is a viking MBUS module
	nop

	/*
	 * Ok, we have a non-MBus TI Viking, a MicroSparc.
	 * In this scenerio, in order to play with the MMU
	 * passthrough safely, we need turn off traps, flip
	 * the AC bit on in the mmu status register, do our
	 * passthroughs, then restore the mmu reg and %psr
	 */
	rd	%psr, %o4		! saved here till done
	andn	%o4, 0x20, %o5
	wr	%o5, 0x0, %psr
	nop; nop; nop;
	set	SRMMU_CXTPTR, %o0
	lda	[%o0] ASI_SRMMU, %o0	! get context table ptr
	sll	%o0, 4, %o0		! make physical
	lda	[%g0] ASI_SRMMU, %o3	! hold mmu-sreg here
	/* 0x8000 is AC bit in Viking mmu-ctl reg */
	set	0x8000, %o2
	or	%o3, %o2, %o2
	sta	%o2, [%g0] ASI_SRMMU	! AC bit on
	lda	[%o0] ASI_BYPASS, %o1
	srl	%o1, 4, %o1
	sll	%o1, 8, %o1		! get phys addr of l1 entry
	lda	[%o1] ASI_BYPASS, %l4
	srl	%l1, 22, %o2		! note: 22 == RGSHIFT - 2
	add	%o1, %o2, %o1
	sta	%l4, [%o1] ASI_BYPASS
	sta	%o3, [%g0] ASI_SRMMU	! restore mmu-sreg
	wr	%o4, 0x0, %psr		! restore psr
	b,a	startmap_done

	/*
	 * The following is generic and should work on all
	 * MBus based SRMMU's.
	 */
remap_notvik:
	set	SRMMU_CXTPTR, %o0
	lda	[%o0] ASI_SRMMU, %o0	! get context table ptr
	sll	%o0, 4, %o0		! make physical
	lda	[%o0] ASI_BYPASS, %o1
	srl	%o1, 4, %o1
	sll	%o1, 8, %o1		! get phys addr of l1 entry
	lda	[%o1] ASI_BYPASS, %l4
	srl	%l1, 22, %o2		! note: 22 == RGSHIFT - 2
	add	%o1, %o2, %o1
	sta	%l4, [%o1] ASI_BYPASS
	!b,a	startmap_done

3:
#endif /* SUN4M */
	! botch! We should blow up.

startmap_done:
	/*
	 * All set, fix pc and npc.  Once we are where we should be,
	 * we can give ourselves a stack and enable traps.
	 */
	set	1f, %g1
	jmp	%g1
	 nop
1:
	sethi	%hi(_C_LABEL(cputyp)), %o0	! what type of cpu we are on
	st	%g4, [%o0 + %lo(_C_LABEL(cputyp))]

	mov	1, %o0			! nbpg = 1 << pgshift (g5)
	sll	%o0, %g5, %g5
	sethi	%hi(_C_LABEL(nbpg)), %o0		! nbpg = bytes in a page
	st	%g5, [%o0 + %lo(_C_LABEL(nbpg))]

	sub	%g5, 1, %g5
	sethi	%hi(_C_LABEL(pgofset)), %o0	! page offset = bytes in a page - 1
	st	%g5, [%o0 + %lo(_C_LABEL(pgofset))]

	rd	%psr, %g3		! paranoia: make sure ...
	andn	%g3, PSR_ET, %g3	! we have traps off
	wr	%g3, 0, %psr		! so that we can fiddle safely
	nop; nop; nop

	wr	%g0, 0, %wim		! make sure we can set psr
	nop; nop; nop
	wr	%g0, PSR_S|PSR_PS|PSR_PIL, %psr	! set initial psr
	 nop; nop; nop

	wr	%g0, 2, %wim		! set initial %wim (w1 invalid)
	mov	1, %g1			! set pcb_wim (log2(%wim) = 1)
	sethi	%hi(_C_LABEL(u0) + PCB_WIM), %g2
	st	%g1, [%g2 + %lo(_C_LABEL(u0) + PCB_WIM)]

	set	USRSTACK - CCFSZ, %fp	! as if called from user code
	set	estack0 - CCFSZ - 80, %sp ! via syscall(boot_me_up) or somesuch
	rd	%psr, %l0
	wr	%l0, PSR_ET, %psr
	nop; nop; nop

	/* Export actual trapbase */
	sethi	%hi(_C_LABEL(trapbase)), %o0
	st	%g6, [%o0+%lo(_C_LABEL(trapbase))]

	/*
	 * Step 2: clear BSS.  This may just be paranoia; the boot
	 * loader might already do it for us; but what the hell.
	 */
	set	_C_LABEL(edata), %o0		! bzero(edata, end - edata)
	set	_C_LABEL(end), %o1
	call	_C_LABEL(bzero)
	 sub	%o1, %o0, %o1

	/*
	 * Stash prom vectors now, after bzero, as it lives in bss
	 * (which we just zeroed).
	 * This depends on the fact that bzero does not use %g7.
	 */
	sethi	%hi(_C_LABEL(promvec)), %l0
	st	%g7, [%l0 + %lo(_C_LABEL(promvec))]

	/*
	 * Step 3: compute number of windows and set up tables.
	 * We could do some of this later.
	 */
	save	%sp, -64, %sp
	rd	%psr, %g1
	restore
	and	%g1, 31, %g1		! want just the CWP bits
	add	%g1, 1, %o0		! compute nwindows
	sethi	%hi(_C_LABEL(nwindows)), %o1	! may as well tell everyone
	call	init_tables
	 st	%o0, [%o1 + %lo(_C_LABEL(nwindows))]

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
	/*
	 * Some sun4/sun4c models have fewer than 8 windows. For extra
	 * speed, we do not need to save/restore those windows
	 * The save/restore code has 7 "save"'s followed by 7
	 * "restore"'s -- we "nop" out the last "save" and first
	 * "restore"
	 */
	cmp	%o0, 8
	be	1f
noplab:	 nop
	sethi	%hi(noplab), %l0
	ld	[%l0 + %lo(noplab)], %l1
	set	wb1, %l0
	st	%l1, [%l0 + 6*4]
	st	%l1, [%l0 + 7*4]
1:
#endif

#if ((defined(SUN4) || defined(SUN4C) || defined(SUN4E)) && (defined(SUN4D) || defined(SUN4M)))

	/*
	 * Patch instructions at specified labels that start
	 * per-architecture code-paths.
	 */
Lgandul:	nop

#define MUNGE(label) \
	sethi	%hi(label), %o0; \
	st	%l0, [%o0 + %lo(label)]

	sethi	%hi(Lgandul), %o0
	ld	[%o0 + %lo(Lgandul)], %l0	! %l0 = NOP

	cmp	%g4, CPU_SUN4M
	blu,a	1f
	 nop

	! this should be automated!
	MUNGE(NOP_ON_4M_1)
	MUNGE(NOP_ON_4M_2)
	MUNGE(NOP_ON_4M_3)
	MUNGE(NOP_ON_4M_4)
	MUNGE(NOP_ON_4M_5)
	MUNGE(NOP_ON_4M_6)
	MUNGE(NOP_ON_4M_7)
	MUNGE(NOP_ON_4M_8)
	MUNGE(NOP_ON_4M_9)
	MUNGE(NOP_ON_4M_10)
	MUNGE(NOP_ON_4M_11)
	MUNGE(NOP_ON_4M_12)
	b,a	2f

1:
#if 0 /* until NOP_ON_4_4C labels reappear */
	MUNGE(NOP_ON_4_4C_...)
#endif

2:

#undef MUNGE
#endif

	/*
	 * Step 4: change the trap base register, now that our trap handlers
	 * will function (they need the tables we just set up).
	 * This depends on the fact that bzero does not use %g6.
	 */
	wr	%g6, 0, %tbr
	nop; nop; nop			! paranoia


	/*
	 * Ready to run C code; finish bootstrap.
	 */
	call	_C_LABEL(bootstrap)
	 nop

	/*
	 * Call main.
	 */
	call	_C_LABEL(main)
	 clr	%o0			! our frame arg is ignored
	/*NOTREACHED*/


/*
 * The following code is copied to the top of the user stack when each
 * process is exec'ed, and signals are `trampolined' off it.
 *
 * When this code is run, the stack looks like:
 *	[%sp]		64 bytes to which registers can be dumped
 *	[%sp + 64]	signal number (goes in %o0)
 *	[%sp + 64 + 4]	siginfo_t pointer (goes in %o1)
 *	[%sp + 64 + 8]	sigcontext pointer (goes in %o2)
 *	[%sp + 64 + 12]	argument for %o3, currently unsupported (always 0)
 *	[%sp + 64 + 16]	first word of saved state (sigcontext)
 *	    .
 *	    .
 *	    .
 *	[%sp + NNN]	last word of saved state
 * (followed by previous stack contents or top of signal stack).
 * The address of the function to call is in %g1; the old %g1 and %o0
 * have already been saved in the sigcontext.  We are running in a clean
 * window, all previous windows now being saved to the stack.
 *
 * Note that [%sp + 64 + 8] == %sp + 64 + 16.  The copy at %sp+64+8
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
	 * Make room on the stack for 32 %f registers + %fsr.  This comes
	 * out to 33*4 or 132 bytes, but this must be aligned to a multiple
	 * of 8, or 136 bytes.
	 */
	save	%sp, -CCFSZ - 136, %sp
	mov	%g2, %l2		! save globals in %l registers
	mov	%g3, %l3
	mov	%g4, %l4
	mov	%g5, %l5
	mov	%g6, %l6
	mov	%g7, %l7
	/*
	 * Saving the fpu registers is expensive, so do it iff the fsr
	 * stored in the sigcontext shows that the fpu is enabled.
	 */
	ld	[%fp + 64 + 16 + SC_PSR_OFFSET], %l0
	sethi	%hi(PSR_EF), %l1	! FPU enable bit is too high for andcc
	andcc	%l0, %l1, %l0		! %l0 = fpu enable bit
	be	1f			! if not set, skip the saves
	 rd	%y, %l1			! in any case, save %y

	! fpu is enabled, oh well
	st	%fsr, [%sp + CCFSZ + 0]
	std	%f0, [%sp + CCFSZ + 8]
	std	%f2, [%sp + CCFSZ + 16]
	std	%f4, [%sp + CCFSZ + 24]
	std	%f6, [%sp + CCFSZ + 32]
	std	%f8, [%sp + CCFSZ + 40]
	std	%f10, [%sp + CCFSZ + 48]
	std	%f12, [%sp + CCFSZ + 56]
	std	%f14, [%sp + CCFSZ + 64]
	std	%f16, [%sp + CCFSZ + 72]
	std	%f18, [%sp + CCFSZ + 80]
	std	%f20, [%sp + CCFSZ + 88]
	std	%f22, [%sp + CCFSZ + 96]
	std	%f24, [%sp + CCFSZ + 104]
	std	%f26, [%sp + CCFSZ + 112]
	std	%f28, [%sp + CCFSZ + 120]
	std	%f30, [%sp + CCFSZ + 128]

1:
	ldd	[%fp + 64], %o0		! sig, sip
	ld	[%fp + 76], %o3		! arg3
#ifdef SIG_DEBUG
	subcc	%o0, 32, %g0		! signals are 1-32
	bgu	_C_LABEL(suicide)
	 nop
#endif
	call	%g1			! (*sa->sa_handler)(sig,sip,scp,arg3)
	 add	%fp, 64 + 16, %o2	! scp

	/*
	 * Now that the handler has returned, re-establish all the state
	 * we just saved above, then do a sigreturn.
	 */
	tst	%l0			! reload fpu registers?
	be	1f			! if not, skip the loads
	 wr	%l1, %g0, %y		! in any case, restore %y

	ld	[%sp + CCFSZ + 0], %fsr
	ldd	[%sp + CCFSZ + 8], %f0
	ldd	[%sp + CCFSZ + 16], %f2
	ldd	[%sp + CCFSZ + 24], %f4
	ldd	[%sp + CCFSZ + 32], %f6
	ldd	[%sp + CCFSZ + 40], %f8
	ldd	[%sp + CCFSZ + 48], %f10
	ldd	[%sp + CCFSZ + 56], %f12
	ldd	[%sp + CCFSZ + 64], %f14
	ldd	[%sp + CCFSZ + 72], %f16
	ldd	[%sp + CCFSZ + 80], %f18
	ldd	[%sp + CCFSZ + 88], %f20
	ldd	[%sp + CCFSZ + 96], %f22
	ldd	[%sp + CCFSZ + 104], %f24
	ldd	[%sp + CCFSZ + 112], %f26
	ldd	[%sp + CCFSZ + 120], %f28
	ldd	[%sp + CCFSZ + 128], %f30

1:
	mov	%l2, %g2
	mov	%l3, %g3
	mov	%l4, %g4
	mov	%l5, %g5
	mov	%l6, %g6
	mov	%l7, %g7

	restore	%g0, SYS_sigreturn, %g1	! get registers back & set syscall #
	add	%sp, 64 + 16, %o0	! compute scp
	t	ST_SYSCALL		! sigreturn(scp)
	! sigreturn does not return unless it fails
	mov	SYS_exit, %g1		! exit(errno)
	t	ST_SYSCALL

#ifdef SIG_DEBUG
	.globl _C_LABEL(suicide)
_C_LABEL(suicide):
	mov	139, %g1		! obsolete syscall, puke...
	t	ST_SYSCALL
#endif
_C_LABEL(esigcode):

/*
 * Primitives
 */
#if 0
#ifdef GPROF
	.globl	mcount
#define	ENTRY(x) \
	.globl _C_LABEL(x); _C_LABEL(x): ; \
	save	%sp, -CCFSZ, %sp; \
	call	mcount; \
	nop; \
	restore
#else
#define	ENTRY(x)	.globl _C_LABEL(x); _C_LABEL(x):
#endif
#endif
#define	ALTENTRY(x)	.globl _C_LABEL(x); _C_LABEL(x):

/*
 * General-purpose NULL routine.
 */
ENTRY(sparc_noop)
	retl
	 nop

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
	mov	%o1, %o5		! save = toaddr;
	tst	%o2			! maxlen == 0?
	beq,a	Lcstoolong0		! yes, return ENAMETOOLONG
	 sethi	%hi(_C_LABEL(cpcb)), %o4

	set	VM_MIN_KERNEL_ADDRESS, %o4
	cmp	%o0, %o4		! fromaddr < VM_MIN_KERNEL_ADDRESS?
	blu	Lcsdocopyi		! yes, go do it
	 sethi	%hi(_C_LABEL(cpcb)), %o4		! (first instr of copy)

	b	Lcsdone			! no, return EFAULT
	 mov	EFAULT, %o0

/*
 * copyoutstr(fromaddr, toaddr, maxlength, &lencopied)
 *
 * Copy a null terminated string from the kernel
 * address space to the user address space.
 */
ENTRY(copyoutstr)
	! %o0 = fromaddr, %o1 = toaddr, %o2 = maxlen, %o3 = &lencopied
	mov	%o1, %o5		! save = toaddr;
	tst	%o2			! maxlen == 0?
	beq,a	Lcstoolong0		! yes, return ENAMETOOLONG
	 sethi	%hi(_C_LABEL(cpcb)), %o4

	set	VM_MIN_KERNEL_ADDRESS, %o4
	cmp	%o1, %o4		! toaddr < VM_MIN_KERNEL_ADDRESS?
	blu	Lcsdocopyo		! yes, go do it
	 sethi	%hi(_C_LABEL(cpcb)), %o4		! (first instr of copy)

	b	Lcsdone			! no, return EFAULT
	 mov	EFAULT, %o0

Lcsdocopyi:
!	sethi	%hi(_C_LABEL(cpcb)), %o4		! (done earlier)
	ld	[%o4 + %lo(_C_LABEL(cpcb))], %o4	! catch faults
	set	Lcsfaulti, %g1
	b	0f
	 st	%g1, [%o4 + PCB_ONFAULT]

Lcsdocopyo:
!	sethi	%hi(_C_LABEL(cpcb)), %o4		! (done earlier)
	ld	[%o4 + %lo(_C_LABEL(cpcb))], %o4	! catch faults
	set	Lcsfaulto, %g1
	st	%g1, [%o4 + PCB_ONFAULT]

! XXX should do this in bigger chunks when possible
0:					! loop:
	ldsb	[%o0], %g1		!	c = *fromaddr;
	tst	%g1
	stb	%g1, [%o1]		!	*toaddr++ = c;
	be	1f			!	if (c == NULL)
	 inc	%o1			!		goto ok;
	deccc	%o2			!	if (--len > 0) {
	bgu	0b			!		fromaddr++;
	 inc	%o0			!		goto loop;
					!	}
Lcstoolong:				!
	deccc	%o1
	stb	%g0, [%o1]		!	*--toaddr = '\0';
Lcstoolong0:				!
	b	Lcsdone			!	error = ENAMETOOLONG;
	 mov	ENAMETOOLONG, %o0	!	goto done;
1:					! ok:
	clr	%o0			!    error = 0;
Lcsdone:				! done:
	sub	%o1, %o5, %o1		!	len = to - save;
	tst	%o3			!	if (lencopied)
	bnz,a	3f
	 st	%o1, [%o3]		!		*lencopied = len;
3:
	retl				! cpcb->pcb_onfault = 0;
	 st	%g0, [%o4 + PCB_ONFAULT]! return (error);

Lcsfaulti:
	cmp	%o1, %o5		! did we write to the string?
	be	1f
	 nop
	deccc	%o1			! --toaddr
1:
	stb	%g0, [%o1]		! *toaddr = '\0';
	b	Lcsdone			! error = EFAULT;
	 mov	EFAULT, %o0		! goto ret;

Lcsfaulto:
	cmp	%o1, %o5		! did we write to the string?
	be	1f
	 nop
	deccc	%o1	
	stb	%g0, [%o1]		! *--toaddr = '\0';
1:
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
	mov	%o1, %o5		!	to0 = to;
	tst	%o2			! if (maxlength == 0)
	beq,a	2f			!
	 mov	ENAMETOOLONG, %o0	!	ret = ENAMETOOLONG; goto done;

0:					! loop:
	ldsb	[%o0], %o4		!	c = *from;
	tst	%o4
	stb	%o4, [%o1]		!	*to++ = c;
	be	1f			!	if (c == 0)
	 inc	%o1			!		goto ok;
	deccc	%o2			!	if (--len > 0) {
	bgu,a	0b			!		from++;
	 inc	%o0			!		goto loop;
	b	2f			!	}
	 mov	ENAMETOOLONG, %o0	!	ret = ENAMETOOLONG; goto done;
1:					! ok:
	clr	%o0			!	ret = 0;
2:
	sub	%o1, %o5, %o1		!	len = to - to0;
	tst	%o3			!	if (lencopied)
	bnz,a	3f
	 st	%o1, [%o3]		!		*lencopied = len;
3:
	retl
	 nop

/*
 * Copyin(src, dst, len)
 *
 * Copy specified amount of data from user space into the kernel.
 */
ENTRY(copyin)
	set	VM_MIN_KERNEL_ADDRESS, %o3
	cmp	%o0, %o3		! src < VM_MIN_KERNEL_ADDRESS?
	blu,a	Ldocopy			! yes, can try it
	 sethi	%hi(_C_LABEL(cpcb)), %o3

	/* source address points into kernel space: return EFAULT */
	retl
	 mov	EFAULT, %o0

/*
 * Copyout(src, dst, len)
 *
 * Copy specified amount of data from kernel to user space.
 * Just like copyin, except that the `dst' addresses are user space
 * rather than the `src' addresses.
 */
ENTRY(copyout)
	set	VM_MIN_KERNEL_ADDRESS, %o3
	cmp	%o1, %o3		! dst < VM_MIN_KERNEL_ADDRESS?
	blu,a	Ldocopy
	 sethi	%hi(_C_LABEL(cpcb)), %o3

	/* destination address points into kernel space: return EFAULT */
	retl
	 mov	EFAULT, %o0

	/*
	 * ******NOTE****** this depends on bcopy() not using %g7
	 */
Ldocopy:
!	sethi	%hi(_C_LABEL(cpcb)), %o3
	ld	[%o3 + %lo(_C_LABEL(cpcb))], %o3
	set	Lcopyfault, %o4
	mov	%o7, %g7		! save return address
	call	_C_LABEL(bcopy)		! bcopy(src, dst, len)
	 st	%o4, [%o3 + PCB_ONFAULT]

	sethi	%hi(_C_LABEL(cpcb)), %o3
	ld	[%o3 + %lo(_C_LABEL(cpcb))], %o3
	st	%g0, [%o3 + PCB_ONFAULT]
	jmp	%g7 + 8
	 clr	%o0			! return 0

! Copyin or copyout fault.  Clear cpcb->pcb_onfault and return EFAULT.
! Note that although we were in bcopy, there is no state to clean up;
! the only special thing is that we have to return to [g7 + 8] rather than
! [o7 + 8].
Lcopyfault:
	sethi	%hi(_C_LABEL(cpcb)), %o3
	ld	[%o3 + %lo(_C_LABEL(cpcb))], %o3
	st	%g0, [%o3 + PCB_ONFAULT]
	jmp	%g7 + 8
	 mov	EFAULT, %o0


/*
 * Write all user windows presently in the CPU back to the user's stack.
 * We just do `save' instructions until pcb_uw == 0.
 *
 *	p = cpcb;
 *	nsaves = 0;
 *	while (p->pcb_uw > 0)
 *		save(), nsaves++;
 *	while (--nsaves >= 0)
 *		restore();
 */
ENTRY(write_user_windows)
	sethi	%hi(_C_LABEL(cpcb)), %g6
	ld	[%g6 + %lo(_C_LABEL(cpcb))], %g6
	b	2f
	 clr	%g5
1:
	save	%sp, -64, %sp
2:
	ld	[%g6 + PCB_UW], %g7
	tst	%g7
	bg,a	1b
	 inc	%g5
3:
	deccc	%g5
	bge,a	3b
	 restore
	retl
	 nop


	.comm	_C_LABEL(want_resched),4
/*
 * Masterpaddr is the p->p_addr of the last process on the processor.
 * XXX masterpaddr is almost the same as cpcb
 * XXX should delete this entirely
 */
	.comm	_C_LABEL(masterpaddr), 4

/*
 * cpu_switchto(struct proc *oldproc, struct proc *newproc)
 */
ENTRY(cpu_switchto)
	sethi	%hi(_C_LABEL(cpcb)), %g6
	sethi	%hi(_C_LABEL(curproc)), %g7
	ld	[%g6 + %lo(_C_LABEL(cpcb))], %o2
	std	%o6, [%o2 + PCB_SP]	! cpcb->pcb_<sp,pc> = <sp,pc>;
	rd	%psr, %g1		! oldpsr = %psr;
	st	%g1, [%o2 + PCB_PSR]	! cpcb->pcb_psr = oldpsr;
	andn	%g1, PSR_PIL, %g1	! oldpsr &= ~PSR_PIL;

	/*
	 * REGISTER USAGE:
	 *	%g1 = oldpsr (excluding ipl bits)
	 *	%g2 = newpsr
	 *	%g5 = newpcb
	 *	%g6 = %hi(_C_LABEL(cpcb))
	 *	%g7 = %hi(_C_LABEL(curproc))
	 *	%o0 = oldproc
	 *	%o1 = newproc
	 *	%o2 = tmp 3
	 *	%o3 = vm
	 *	%o4 = sswap
	 *	%o5 = <free>
	 */

	/*
	 * Committed to running process p (in o1).
	 */
	mov	%o1, %g3

	mov	SONPROC, %o0			! p->p_stat = SONPROC
	stb	%o0, [%g3 + P_STAT]
	ld	[%g3 + P_ADDR], %g5		! newpcb = p->p_addr;
	ld	[%g5 + PCB_PSR], %g2		! newpsr = newpcb->pcb_psr;
	st	%g3, [%g7 + %lo(_C_LABEL(curproc))]	! curproc = p;

	/*
	 * Save the old process, if any; then load p.
	 */
	tst	%o0
	be,a	Lsw_load		! if no old process, go load
	 wr	%g1, (IPL_CLOCK << 8) | PSR_ET, %psr

	/*
	 * save: write back all windows (including the current one).
	 * XXX	crude; knows nwindows <= 8
	 */
#define	SAVE save %sp, -64, %sp
wb1:	SAVE; SAVE; SAVE; SAVE; SAVE; SAVE; SAVE	/* 7 of each: */
	restore; restore; restore; restore; restore; restore; restore

	/*
	 * Load the new process.  To load, we must change stacks and
	 * alter cpcb and %wim, hence we must disable traps.  %psr is
	 * currently equal to oldpsr (%g1) ^ (IPL_CLOCK << 8);
	 * this means that PSR_ET is on.  Likewise, PSR_ET is on
	 * in newpsr (%g2), although we do not know newpsr's ipl.
	 *
	 * We also must load up the `in' and `local' registers.
	 */
	wr	%g1, (IPL_CLOCK << 8) | PSR_ET, %psr
Lsw_load:
!	wr	%g1, (IPL_CLOCK << 8) | PSR_ET, %psr	! done above
	/* compute new wim */
	ld	[%g5 + PCB_WIM], %o0
	mov	1, %o1
	sll	%o1, %o0, %o0
	wr	%o0, 0, %wim		! %wim = 1 << newpcb->pcb_wim;
	/* now must not change %psr for 3 more instrs */
/*1*/	set	PSR_EF|PSR_EC, %o0
/*2*/	andn	%g2, %o0, %g2		! newpsr &= ~(PSR_EF|PSR_EC);
/*3*/	nop
	/* set new psr, but with traps disabled */
	wr	%g2, PSR_ET, %psr	! %psr = newpsr ^ PSR_ET;
	/* set new cpcb */
	st	%g5, [%g6 + %lo(_C_LABEL(cpcb))]	! cpcb = newpcb;
	/* XXX update masterpaddr too */
	sethi	%hi(_C_LABEL(masterpaddr)), %g7
	st	%g5, [%g7 + %lo(_C_LABEL(masterpaddr))]
	ldd	[%g5 + PCB_SP], %o6	! <sp,pc> = newpcb->pcb_<sp,pc>
	/* load window */
	ldd	[%sp + (0*8)], %l0
	ldd	[%sp + (1*8)], %l2
	ldd	[%sp + (2*8)], %l4
	ldd	[%sp + (3*8)], %l6
	ldd	[%sp + (4*8)], %i0
	ldd	[%sp + (5*8)], %i2
	ldd	[%sp + (6*8)], %i4
	ldd	[%sp + (7*8)], %i6
#ifdef DEBUG
	mov	%g5, %o0
	SET_SP_REDZONE(%o0, %o1)
	CHECK_SP_REDZONE(%o0, %o1)
#endif
	/* finally, enable traps */
	wr	%g2, 0, %psr		! psr = newpsr;

	/*
	 * Now running p.  Make sure it has a context so that it
	 * can talk about user space stuff.  (Its pcb_uw is currently
	 * zero so it is safe to have interrupts going here.)
	 */
	ld	[%g3 + P_VMSPACE], %o3	! vm = p->p_vmspace;
	ld	[%o3 + VM_PMAP], %o3	! pm = vm->vm_map.pmap;
	ld	[%o3 + PMAP_CTX], %o0	! if (pm->pm_ctx != NULL)
	tst	%o0
	bnz,a	Lsw_havectx		!	goto havecontext;
	 ld	[%o3 + PMAP_CTXNUM], %o0	! load context number

	/* p does not have a context: call ctx_alloc to get one */
	save	%sp, -CCFSZ, %sp
	call	_C_LABEL(ctx_alloc)		! ctx_alloc(pm);
	 mov	%i3, %o0

	ret
	 restore

	/* p does have a context: just switch to it */
Lsw_havectx:
	! context is in %o0
#if (defined(SUN4) || defined(SUN4C) || defined(SUN4E)) && (defined(SUN4D) || defined(SUN4M))
	sethi	%hi(_C_LABEL(cputyp)), %o1	! what cpu are we running on?
	ld	[%o1 + %lo(_C_LABEL(cputyp))], %o1
	cmp	%o1, CPU_SUN4M
	bge	1f
	 nop
#endif
#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
	set	AC_CONTEXT, %o1
	retl
	 stba	%o0, [%o1] ASI_CONTROL	! setcontext(vm->vm_map.pmap->pm_ctxnum);
#endif
1:
#if defined(SUN4M)
	/*
	 * Flush caches that need to be flushed on context switch.
	 * We know this is currently only necessary on the sun4m hypersparc.
	 */
	set	CPUINFO_VA+CPUINFO_PURE_VCACHE_FLS, %o2
	ld	[%o2], %o2
	mov	%o7, %g7	! save return address
	jmpl	%o2, %o7	! this function must not clobber %o0 and %g7
	 nop

	set	SRMMU_CXR, %o1
	jmp	%g7 + 8		! (retl, but we saved the ret address in g7)
	 sta	%o0, [%o1] ASI_SRMMU	! setcontext(vm->vm_map.pmap->pm_ctxnum);
#endif

ENTRY(cpu_idle_enter)
	retl
	 nop

ENTRY(cpu_idle_cycle)
	retl
	 nop

ENTRY(cpu_idle_leave)
	retl
	 nop

/*
 * Snapshot the current process so that stack frames are up to date.
 * Only used just before a crash dump.
 */
ENTRY(snapshot)
	std	%o6, [%o0 + PCB_SP]	! save sp
	rd	%psr, %o1		! save psr
	st	%o1, [%o0 + PCB_PSR]

	/*
	 * Just like switch(); same XXX comments apply.
	 * 7 of each.  Minor tweak: the 7th restore is
	 * done after a ret.
	 */
	SAVE; SAVE; SAVE; SAVE; SAVE; SAVE; SAVE
	restore; restore; restore; restore; restore; restore; ret; restore


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
	/* Reset interrupt level */
	rd 	%psr, %o0
	andn	%o0, PSR_PIL, %o0	! psr &= ~PSR_PIL;
	wr	%o0, 0, %psr		! (void) spl0();
	 nop				! psr delay; the next 2 instructions
					! can safely be made part of the
					! required 3 instructions psr delay
	call	%l0			! re-use current frame
	 mov	%l1, %o0

	/*
	 * Here we finish up as in syscall, but simplified.  We need to
	 * fiddle pc and npc a bit, as execve() / setregs() /cpu_set_kpc()
	 * have only set npc, in anticipation that trap.c will advance past
	 * the trap instruction; but we bypass that, so we must do it manually.
	 */
	mov	PSR_S, %l0		! user psr (no need to load it)
	!?wr	%g0, 2, %wim		! %wim = 2
	ld	[%sp + CCFSZ + 4], %l1	! pc
	b	return_from_syscall
	 ld	[%sp + CCFSZ + 8], %l2	! npc

/* probeget is meant to be used during autoconfiguration */

/*
 * probeget(addr, size) caddr_t addr; int size;
 *
 * Read a (byte,short,int) from the given address.
 * Like copyin but our caller is supposed to know what he is doing...
 * the address can be anywhere.
 *
 * We optimize for space, rather than time, here.
 */
ENTRY(probeget)
	! %o0 = addr, %o1 = (1,2,4)
	sethi	%hi(_C_LABEL(cpcb)), %o2
	ld	[%o2 + %lo(_C_LABEL(cpcb))], %o2	! cpcb->pcb_onfault = Lfserr;
	set	Lfserr, %o5
	st	%o5, [%o2 + PCB_ONFAULT]
	btst	1, %o1
	bnz,a	0f			! if (len & 1)
	 ldub	[%o0], %o0		!	value = *(char *)addr;
0:	btst	2, %o1
	bnz,a	0f			! if (len & 2)
	 lduh	[%o0], %o0		!	value = *(short *)addr;
0:	btst	4, %o1
	bnz,a	0f			! if (len & 4)
	 ld	[%o0], %o0		!	value = *(int *)addr;
0:	retl				! made it, clear onfault and return
	 st	%g0, [%o2 + PCB_ONFAULT]

Lfserr:
	st	%g0, [%o2 + PCB_ONFAULT]! error in r/w, clear pcb_onfault
	retl				! and return error indicator
	 mov	-1, %o0

#ifdef SUN4

	/*
	 * This is just like Lfserr, but it's a global label that allows
	 * mem_access_fault() to check to see that we don't want to try to
	 * page in the fault.  It's used by xldcontrolb().
	 */
	 .globl	_C_LABEL(Lfsbail)
Lfsbail:
	st	%g0, [%o2 + PCB_ONFAULT]! error in r/w, clear pcb_onfault
	retl				! and return error indicator
	 mov	-1, %o0

/*
 * int xldcontrolb(caddr_t, pcb)
 *		    %o0     %o1
 *
 * read a byte from the specified address in ASI_CONTROL space.
 */
ENTRY(xldcontrolb)
	!sethi	%hi(_C_LABEL(cpcb)), %o2
	!ld	[%o2 + %lo(_C_LABEL(cpcb))], %o2	! cpcb->pcb_onfault = Lfsbail;
	or	%o1, %g0, %o2		! %o2 = %o1
	set	_C_LABEL(Lfsbail), %o5
	st	%o5, [%o2 + PCB_ONFAULT]
	lduba	[%o0] ASI_CONTROL, %o0	! read
0:	retl
	 st	%g0, [%o2 + PCB_ONFAULT]


#endif	/* SUN4 */

/*
 * copywords(src, dst, nbytes)
 *
 * Copy `nbytes' bytes from src to dst, both of which are word-aligned;
 * nbytes is a multiple of four.  It may, however, be zero, in which case
 * nothing is to be copied.
 */
ENTRY(copywords)
	! %o0 = src, %o1 = dst, %o2 = nbytes
	b	1f
	deccc	4, %o2
0:
	st	%o3, [%o1 + %o2]
	deccc	4, %o2			! while ((n -= 4) >= 0)
1:
	bge,a	0b			!    *(int *)(dst+n) = *(int *)(src+n);
	ld	[%o0 + %o2], %o3
	retl
	nop

/*
 * qcopy(src, dst, nbytes)
 *
 * (q for `quad' or `quick', as opposed to b for byte/block copy)
 *
 * Just like copywords, but everything is multiples of 8.
 */
ENTRY(qcopy)
	b	1f
	deccc	8, %o2
0:
	std	%o4, [%o1 + %o2]
	deccc	8, %o2
1:
	bge,a	0b
	ldd	[%o0 + %o2], %o4
	retl
	nop

/*
 * qzero(addr, nbytes)
 *
 * Zeroes `nbytes' bytes of a quad-aligned virtual address,
 * where nbytes is itself a multiple of 8.
 */
ENTRY(qzero)
	! %o0 = addr, %o1 = len (in bytes)
	clr	%g1
0:
	deccc	8, %o1			! while ((n =- 8) >= 0)
	bge,a	0b
	std	%g0, [%o0 + %o1]	!	*(quad *)(addr + n) = 0;
	retl
	nop

#define	BCOPY_SMALL	32	/* if < 32, copy by bytes */
/*
 * Must not use %g7 (see copyin/copyout above).
 */

/*
 * kcopy() is exactly like old bcopy except that it set pcb_onfault such that
 * when a fault occurs, it is able to return EFAULT to indicate this to the
 * caller.
 */
ENTRY(kcopy)
	sethi	%hi(_C_LABEL(cpcb)), %o5		! cpcb->pcb_onfault = Lkcerr;
	ld	[%o5 + %lo(_C_LABEL(cpcb))], %o5
	set	Lkcerr, %o3
	ld	[%o5 + PCB_ONFAULT], %g1! save current onfault handler
	st	%o3, [%o5 + PCB_ONFAULT]

	cmp	%o2, BCOPY_SMALL
Lkcopy_start:
	bge,a	Lkcopy_fancy	! if >= this many, go be fancy.
	 btst	7, %o0		! (part of being fancy)

	/*
	 * Not much to copy, just do it a byte at a time.
	 */
	deccc	%o2		! while (--len >= 0)
	bl	1f
	 EMPTY
0:
	ldsb	[%o0], %o4	!	*dst++ = *src++;
	inc	%o0
	stb	%o4, [%o1]
	deccc	%o2
	bge	0b
	 inc	%o1
1:
	st	%g1, [%o5 + PCB_ONFAULT]	! restore onfault
	retl
	 mov	0, %o0		! delay slot: return success
	/* NOTREACHED */

	/*
	 * Plenty of data to copy, so try to do it optimally.
	 */
Lkcopy_fancy:
	! check for common case first: everything lines up.
!	btst	7, %o0		! done already
	bne	1f
	 EMPTY
	btst	7, %o1
	be,a	Lkcopy_doubles
	 dec	8, %o2		! if all lined up, len -= 8, goto bcopy_doubles

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
	st	%g1, [%o5 + PCB_ONFAULT]	! restore onfault
	retl
	 mov	0, %o0		! delay slot: return success
	/* NOTREACHED */

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
	/* NOTREACHED */

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
	/* NOTREACHED */

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
	! swap %o4 with %o2 during doubles copy, since %o5 is verboten
	mov     %o2, %o4
Lkcopy_doubles2:
	ldd	[%o0], %o2	! do {
	inc	8, %o0		!	dst += 8, src += 8;
	std	%o2, [%o1]	!	*(double *)dst = *(double *)src;
	deccc	8, %o4		! } while ((len -= 8) >= 0);
	bge	Lkcopy_doubles2
	 inc	8, %o1
	mov	%o4, %o2	! restore len

	! check for a usual case again (save work)
	btst	7, %o2		! if ((len & 7) == 0)
	be	Lkcopy_done	!	goto bcopy_done;

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
	st	%g1, [%o5 + PCB_ONFAULT]! restore onfault
	retl
	 mov	0, %o0		! delay slot: return success
	/* NOTREACHED */

	! mop up trailing byte (if present).
Lkcopy_mopb:
	bne,a	1f
	 ldsb	[%o0], %o4

Lkcopy_done:
	st	%g1, [%o5 + PCB_ONFAULT]	! restore onfault
	retl
	 mov	0, %o0		! delay slot: return success
	/* NOTREACHED */

1:
	stb	%o4, [%o1]
	st	%g1, [%o5 + PCB_ONFAULT]	! restore onfault
	retl
	 mov	0, %o0		! delay slot: return success
	/* NOTREACHED */

Lkcerr:
	st	%g1, [%o5 + PCB_ONFAULT]	! restore onfault
	retl
	 mov	EFAULT, %o0	! delay slot: return error indicator
	/* NOTREACHED */

/*
 * savefpstate(f) struct fpstate *f;
 *
 * Store the current FPU state.  The first `st %fsr' may cause a trap;
 * our trap handler knows how to recover (by `returning' to savefpcont).
 */
ENTRY(savefpstate)
	rd	%psr, %o1		! enable FP before we begin
	set	PSR_EF, %o2
	or	%o1, %o2, %o1
	wr	%o1, 0, %psr
	/* do some setup work while we wait for PSR_EF to turn on */
	set	FSR_QNE, %o5		! QNE = 0x2000, too big for immediate
	clr	%o3			! qsize = 0;
	nop				! (still waiting for PSR_EF)
special_fp_store:
	st	%fsr, [%o0 + FS_FSR]	! f->fs_fsr = getfsr();
	/*
	 * Even if the preceding instruction did not trap, the queue
	 * is not necessarily empty: this state save might be happening
	 * because user code tried to store %fsr and took the FPU
	 * from `exception pending' mode to `exception' mode.
	 * So we still have to check the blasted QNE bit.
	 * With any luck it will usually not be set.
	 */
	ld	[%o0 + FS_FSR], %o4	! if (f->fs_fsr & QNE)
	btst	%o5, %o4
	bnz	Lfp_storeq		!	goto storeq;
	 std	%f0, [%o0 + FS_REGS + (4*0)]	! f->fs_f0 = etc;
Lfp_finish:
	st	%o3, [%o0 + FS_QSIZE]	! f->fs_qsize = qsize;
	std	%f2, [%o0 + FS_REGS + (4*2)]
	std	%f4, [%o0 + FS_REGS + (4*4)]
	std	%f6, [%o0 + FS_REGS + (4*6)]
	std	%f8, [%o0 + FS_REGS + (4*8)]
	std	%f10, [%o0 + FS_REGS + (4*10)]
	std	%f12, [%o0 + FS_REGS + (4*12)]
	std	%f14, [%o0 + FS_REGS + (4*14)]
	std	%f16, [%o0 + FS_REGS + (4*16)]
	std	%f18, [%o0 + FS_REGS + (4*18)]
	std	%f20, [%o0 + FS_REGS + (4*20)]
	std	%f22, [%o0 + FS_REGS + (4*22)]
	std	%f24, [%o0 + FS_REGS + (4*24)]
	std	%f26, [%o0 + FS_REGS + (4*26)]
	std	%f28, [%o0 + FS_REGS + (4*28)]
	retl
	 std	%f30, [%o0 + FS_REGS + (4*30)]

/*
 * Store the (now known nonempty) FP queue.
 * We have to reread the fsr each time in order to get the new QNE bit.
 */
Lfp_storeq:
	add	%o0, FS_QUEUE, %o1	! q = &f->fs_queue[0];
1:
	std	%fq, [%o1 + %o3]	! q[qsize++] = fsr_qfront();
	st	%fsr, [%o0 + FS_FSR]	! reread fsr
	ld	[%o0 + FS_FSR], %o4	! if fsr & QNE, loop
	btst	%o5, %o4
	bnz	1b
	 inc	8, %o3
	b	Lfp_finish		! set qsize and finish storing fregs
	 srl	%o3, 3, %o3		! (but first fix qsize)

/*
 * The fsr store trapped.  Do it again; this time it will not trap.
 * We could just have the trap handler return to the `st %fsr', but
 * if for some reason it *does* trap, that would lock us into a tight
 * loop.  This way we panic instead.  Whoopee.
 */
savefpcont:
	b	special_fp_store + 4	! continue
	 st	%fsr, [%o0 + FS_FSR]	! but first finish the %fsr store

/*
 * Load FPU state.
 */
ENTRY(loadfpstate)
	rd	%psr, %o1		! enable FP before we begin
	set	PSR_EF, %o2
	or	%o1, %o2, %o1
	wr	%o1, 0, %psr
	nop; nop; nop			! paranoia
	ldd	[%o0 + FS_REGS + (4*0)], %f0
	ldd	[%o0 + FS_REGS + (4*2)], %f2
	ldd	[%o0 + FS_REGS + (4*4)], %f4
	ldd	[%o0 + FS_REGS + (4*6)], %f6
	ldd	[%o0 + FS_REGS + (4*8)], %f8
	ldd	[%o0 + FS_REGS + (4*10)], %f10
	ldd	[%o0 + FS_REGS + (4*12)], %f12
	ldd	[%o0 + FS_REGS + (4*14)], %f14
	ldd	[%o0 + FS_REGS + (4*16)], %f16
	ldd	[%o0 + FS_REGS + (4*18)], %f18
	ldd	[%o0 + FS_REGS + (4*20)], %f20
	ldd	[%o0 + FS_REGS + (4*22)], %f22
	ldd	[%o0 + FS_REGS + (4*24)], %f24
	ldd	[%o0 + FS_REGS + (4*26)], %f26
	ldd	[%o0 + FS_REGS + (4*28)], %f28
	ldd	[%o0 + FS_REGS + (4*30)], %f30
	retl
	 ld	[%o0 + FS_FSR], %fsr	! setfsr(f->fs_fsr);

/*
 * intreg_set_44c(int bis);
 * intreg_clr_44c(int bic);
 *
 * Set and clear bits in the interrupt register.
 */

#if defined(SUN4) || defined(SUN4C) || defined(SUN4E)
/*
 * Since there are no read-modify-write instructions for this,
 * and one of the interrupts is nonmaskable, we must disable traps.
 */
ENTRY(intreg_set_44c)
	! %o0 = bits to set
	rd	%psr, %o2
	wr	%o2, PSR_ET, %psr	! disable traps
	nop; nop			! 3-instr delay until ET turns off
	sethi	%hi(INTRREG_VA), %o3
	ldub	[%o3 + %lo(INTRREG_VA)], %o4
	or	%o4, %o0, %o4		! *INTRREG_VA |= bis;
	stb	%o4, [%o3 + %lo(INTRREG_VA)]
	wr	%o2, 0, %psr		! reenable traps
	nop
	retl
	 nop

ENTRY(intreg_clr_44c)
	! %o0 = bits to clear
	rd	%psr, %o2
	wr	%o2, PSR_ET, %psr	! disable traps
	nop; nop
	sethi	%hi(INTRREG_VA), %o3
	ldub	[%o3 + %lo(INTRREG_VA)], %o4
	andn	%o4, %o0, %o4		! *INTRREG_VA &=~ bic;
	stb	%o4, [%o3 + %lo(INTRREG_VA)]
	wr	%o2, 0, %psr		! reenable traps
	nop
	retl
	 nop
#endif	/* SUN4 || SUN4C || SUN4E */

#if defined(SUN4M)
/*
 * sun4m has separate registers for clearing/setting the interrupt mask.
 */
ENTRY(intreg_set_4m)
	set	ICR_SI_SET, %o1
	retl
	 st	%o0, [%o1]

ENTRY(intreg_clr_4m)
	set	ICR_SI_CLR, %o1
	retl
	 st	%o0, [%o1]

/*
 * raise(cpu, level)
 */
ENTRY(raise)
	! *(ICR_PI_SET + cpu*_MAXNBPG) = PINTR_SINTRLEV(level)
	sethi	%hi(1 << 16), %o2
	sll	%o2, %o1, %o2
	set	ICR_PI_SET, %o1
	set	_MAXNBPG, %o3
1:
	subcc	%o0, 1, %o0
	bpos,a	1b
	 add	%o1, %o3, %o1
	retl
	 st	%o2, [%o1]


/*
 * Read Synchronous Fault Status registers.
 * On entry: %l1 == PC, %l3 == fault type, %l4 == storage, %l7 == return address
 * Only use %l5 and %l6.
 * Note: not C callable.
 */
ALTENTRY(srmmu_get_syncflt)
ALTENTRY(hypersparc_get_syncflt)
	set	SRMMU_SFAR, %l5
	lda	[%l5] ASI_SRMMU, %l5	! sync virt addr; must be read first
	st	%l5, [%l4 + 4]		! => dump.sfva
	set	SRMMU_SFSR, %l5
	lda	[%l5] ASI_SRMMU, %l5	! get sync fault status register
	jmp	%l7 + 8			! return to caller
	 st	%l5, [%l4]		! => dump.sfsr

ALTENTRY(viking_get_syncflt)
ALTENTRY(ms1_get_syncflt)
ALTENTRY(swift_get_syncflt)
ALTENTRY(turbosparc_get_syncflt)
ALTENTRY(cypress_get_syncflt)
	cmp	%l3, T_TEXTFAULT
	be,a	1f
	 mov	%l1, %l5		! use PC if type == T_TEXTFAULT

	set	SRMMU_SFAR, %l5
	lda	[%l5] ASI_SRMMU, %l5	! sync virt addr; must be read first
1:
	st	%l5, [%l4 + 4]		! => dump.sfva

	set	SRMMU_SFSR, %l5
	lda	[%l5] ASI_SRMMU, %l5	! get sync fault status register
	jmp	%l7 + 8			! return to caller
	 st	%l5, [%l4]		! => dump.sfsr


/*
 * Read Asynchronous Fault Status registers.
 * On entry: %o0 == &afsr, %o1 == &afar
 * Return 0 if async register are present.
 */
ALTENTRY(srmmu_get_asyncflt)
	set	SRMMU_AFAR, %o4
	lda	[%o4] ASI_SRMMU, %o4	! get async fault address
	set	SRMMU_AFSR, %o3	!
	st	%o4, [%o1]
	lda	[%o3] ASI_SRMMU, %o3	! get async fault status
	st	%o3, [%o0]
	retl
	 clr	%o0			! return value

ALTENTRY(cypress_get_asyncflt)
ALTENTRY(hypersparc_get_asyncflt)
	set	SRMMU_AFSR, %o3		! must read status before fault on HS
	lda	[%o3] ASI_SRMMU, %o3	! get async fault status
	st	%o3, [%o0]
	btst	AFSR_AFO, %o3		! and only read fault address
	bz	1f			! if valid.
	set	SRMMU_AFAR, %o4
	lda	[%o4] ASI_SRMMU, %o4	! get async fault address
	clr	%o0			! return value
	retl
	 st	%o4, [%o1]
1:
	retl
	 clr	%o0			! return value

ALTENTRY(no_asyncflt_regs)
	retl
	 mov	1, %o0			! return value

ALTENTRY(hypersparc_pure_vcache_flush)
	/*
	 * Flush entire on-chip instruction cache, which is
	 * a pure vitually-indexed/virtually-tagged cache.
	 */
	retl
	 sta    %g0, [%g0] ASI_HICACHECLR
	
#endif /* SUN4M */

/*
 * ffs(), using table lookup.
 * The process switch code shares the table, so we just put the
 * whole thing here.
 */
ffstab:
	.byte	-24,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1 /* 00-0f */
	.byte	5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 10-1f */
	.byte	6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 20-2f */
	.byte	5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 30-3f */
	.byte	7,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 40-4f */
	.byte	5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 50-5f */
	.byte	6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 60-6f */
	.byte	5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 70-7f */
	.byte	8,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 80-8f */
	.byte	5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* 10-9f */
	.byte	6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* a0-af */
	.byte	5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* b0-bf */
	.byte	7,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* c0-cf */
	.byte	5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* d0-df */
	.byte	6,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* e0-ef */
	.byte	5,1,2,1,3,1,2,1,4,1,2,1,3,1,2,1	/* f0-ff */

/*
 * We use a table lookup on each byte.
 *
 * In each section below, %o1 is the current byte (0, 1, 2, or 3).
 * The last byte is handled specially: for the first three,
 * if that byte is nonzero, we return the table value
 * (plus 0, 8, or 16 for the byte number), but for the last
 * one, we just return the table value plus 24.  This means
 * that ffstab[0] must be -24 so that ffs(0) will return 0.
 */
ENTRY(ffs)
	set	ffstab, %o2
	andcc	%o0, 0xff, %o1	! get low byte
	bz,a	1f		! try again if 0
	srl	%o0, 8, %o0	! delay slot, get ready for next byte

	retl			! return ffstab[%o1]
	ldsb	[%o2 + %o1], %o0

1:
	andcc	%o0, 0xff, %o1	! byte 1 like byte 0...
	bz,a	2f
	srl	%o0, 8, %o0	! (use delay to prepare for byte 2)

	ldsb	[%o2 + %o1], %o0
	retl			! return ffstab[%o1] + 8
	add	%o0, 8, %o0

2:
	andcc	%o0, 0xff, %o1
	bz,a	3f
	srl	%o0, 8, %o0	! (prepare for byte 3)

	ldsb	[%o2 + %o1], %o0
	retl			! return ffstab[%o1] + 16
	add	%o0, 16, %o0

3:				! just return ffstab[%o0] + 24
	ldsb	[%o2 + %o0], %o0
	retl
	add	%o0, 24, %o0

/*
 * V8 sparc .{,u}{mul,div,rem} replacements.
 * We try to mimic them 100%.  Full 64 bit sources or outputs, and
 * these routines are required to update the condition codes.
 */
.globl _C_LABEL(_mulreplace), _C_LABEL(_mulreplace_end)
_C_LABEL(_mulreplace):
	smulcc	%o0, %o1, %o0
	retl
	 rd	%y, %o1
_C_LABEL(_mulreplace_end):

.globl _C_LABEL(_umulreplace), _C_LABEL(_umulreplace_end)
_C_LABEL(_umulreplace):
	umulcc	%o0, %o1, %o0
	retl
	 rd	%y, %o1
_C_LABEL(_umulreplace_end):

.globl _C_LABEL(_divreplace), _C_LABEL(_divreplace_end)
_C_LABEL(_divreplace):
	sra	%o0, 31, %g1
	wr	%g1, 0, %y
	nop
	nop
	nop
	retl
	 sdivcc	%o0, %o1, %o0
_C_LABEL(_divreplace_end):

.globl _C_LABEL(_udivreplace), _C_LABEL(_udivreplace_end)
_C_LABEL(_udivreplace):
	wr	%g0, 0, %y
	nop
	nop
	nop
	retl
	 udivcc	%o0, %o1, %o0
_C_LABEL(_udivreplace_end):

.globl _C_LABEL(_remreplace), _C_LABEL(_remreplace_end)
_C_LABEL(_remreplace):
	sra	%o0, 31, %g1
	wr	%g1, 0, %y
	nop
	nop
	nop
	sdiv	%o0, %o1, %o2
	smul	%o1, %o2, %o2
	retl
	 subcc	%o0, %o2, %o0
_C_LABEL(_remreplace_end):

.globl _C_LABEL(_uremreplace), _C_LABEL(_uremreplace_end)
_C_LABEL(_uremreplace):
	wr	%g0, 0, %y
	nop
	nop
	nop
	udiv	%o0, %o1, %o2
	umul	%o1, %o2, %o2
	retl
	 subcc	%o0, %o2, %o0
_C_LABEL(_uremreplace_end):

/*
 * Signed multiply, from Appendix E of the Sparc Version 8
 * Architecture Manual.
 *
 * Returns %o0 * %o1 in %o1%o0 (i.e., %o1 holds the upper 32 bits of
 * the 64-bit product).
 *
 * This code optimizes short (less than 13-bit) multiplies.
 */
.globl .mul, _C_LABEL(_mul)
.mul:
_C_LABEL(_mul):
	mov	%o0, %y		! multiplier -> Y
	andncc	%o0, 0xfff, %g0	! test bits 12..31
	be	Lmul_shortway	! if zero, can do it the short way
	 andcc	%g0, %g0, %o4	! zero the partial product and clear N and V

	/*
	 * Long multiply.  32 steps, followed by a final shift step.
	 */
	mulscc	%o4, %o1, %o4	! 1
	mulscc	%o4, %o1, %o4	! 2
	mulscc	%o4, %o1, %o4	! 3
	mulscc	%o4, %o1, %o4	! 4
	mulscc	%o4, %o1, %o4	! 5
	mulscc	%o4, %o1, %o4	! 6
	mulscc	%o4, %o1, %o4	! 7
	mulscc	%o4, %o1, %o4	! 8
	mulscc	%o4, %o1, %o4	! 9
	mulscc	%o4, %o1, %o4	! 10
	mulscc	%o4, %o1, %o4	! 11
	mulscc	%o4, %o1, %o4	! 12
	mulscc	%o4, %o1, %o4	! 13
	mulscc	%o4, %o1, %o4	! 14
	mulscc	%o4, %o1, %o4	! 15
	mulscc	%o4, %o1, %o4	! 16
	mulscc	%o4, %o1, %o4	! 17
	mulscc	%o4, %o1, %o4	! 18
	mulscc	%o4, %o1, %o4	! 19
	mulscc	%o4, %o1, %o4	! 20
	mulscc	%o4, %o1, %o4	! 21
	mulscc	%o4, %o1, %o4	! 22
	mulscc	%o4, %o1, %o4	! 23
	mulscc	%o4, %o1, %o4	! 24
	mulscc	%o4, %o1, %o4	! 25
	mulscc	%o4, %o1, %o4	! 26
	mulscc	%o4, %o1, %o4	! 27
	mulscc	%o4, %o1, %o4	! 28
	mulscc	%o4, %o1, %o4	! 29
	mulscc	%o4, %o1, %o4	! 30
	mulscc	%o4, %o1, %o4	! 31
	mulscc	%o4, %o1, %o4	! 32
	mulscc	%o4, %g0, %o4	! final shift

	! If %o0 was negative, the result is
	!	(%o0 * %o1) + (%o1 << 32))
	! We fix that here.

	tst	%o0
	bge	1f
	 rd	%y, %o0

	! %o0 was indeed negative; fix upper 32 bits of result by subtracting 
	! %o1 (i.e., return %o4 - %o1 in %o1).
	retl
	 sub	%o4, %o1, %o1

1:
	retl
	 mov	%o4, %o1

Lmul_shortway:
	/*
	 * Short multiply.  12 steps, followed by a final shift step.
	 * The resulting bits are off by 12 and (32-12) = 20 bit positions,
	 * but there is no problem with %o0 being negative (unlike above).
	 */
	mulscc	%o4, %o1, %o4	! 1
	mulscc	%o4, %o1, %o4	! 2
	mulscc	%o4, %o1, %o4	! 3
	mulscc	%o4, %o1, %o4	! 4
	mulscc	%o4, %o1, %o4	! 5
	mulscc	%o4, %o1, %o4	! 6
	mulscc	%o4, %o1, %o4	! 7
	mulscc	%o4, %o1, %o4	! 8
	mulscc	%o4, %o1, %o4	! 9
	mulscc	%o4, %o1, %o4	! 10
	mulscc	%o4, %o1, %o4	! 11
	mulscc	%o4, %o1, %o4	! 12
	mulscc	%o4, %g0, %o4	! final shift

	/*
	 *  %o4 has 20 of the bits that should be in the low part of the
	 * result; %y has the bottom 12 (as %y's top 12).  That is:
	 *
	 *	  %o4		    %y
	 * +----------------+----------------+
	 * | -12- |   -20-  | -12- |   -20-  |
	 * +------(---------+------)---------+
	 *  --hi-- ----low-part----
	 *
	 * The upper 12 bits of %o4 should be sign-extended to form the
	 * high part of the product (i.e., highpart = %o4 >> 20).
	 */

	rd	%y, %o5
	sll	%o4, 12, %o0	! shift middle bits left 12
	srl	%o5, 20, %o5	! shift low bits right 20, zero fill at left
	or	%o5, %o0, %o0	! construct low part of result
	retl
	 sra	%o4, 20, %o1	! ... and extract high part of result

/*
 * Unsigned multiply.  Returns %o0 * %o1 in %o1%o0 (i.e., %o1 holds the
 * upper 32 bits of the 64-bit product).
 *
 * This code optimizes short (less than 13-bit) multiplies.  Short
 * multiplies require 25 instruction cycles, and long ones require
 * 45 instruction cycles.
 *
 * On return, overflow has occurred (%o1 is not zero) if and only if
 * the Z condition code is clear, allowing, e.g., the following:
 *
 *	call	.umul
 *	nop
 *	bnz	overflow	(or tnz)
 */
.globl	.umul, _C_LABEL(_umul)
.umul:
_C_LABEL(_umul):
	or	%o0, %o1, %o4
	mov	%o0, %y		! multiplier -> Y
	andncc	%o4, 0xfff, %g0	! test bits 12..31 of *both* args
	be	Lumul_shortway	! if zero, can do it the short way
	 andcc	%g0, %g0, %o4	! zero the partial product and clear N and V

	/*
	 * Long multiply.  32 steps, followed by a final shift step.
	 */
	mulscc	%o4, %o1, %o4	! 1
	mulscc	%o4, %o1, %o4	! 2
	mulscc	%o4, %o1, %o4	! 3
	mulscc	%o4, %o1, %o4	! 4
	mulscc	%o4, %o1, %o4	! 5
	mulscc	%o4, %o1, %o4	! 6
	mulscc	%o4, %o1, %o4	! 7
	mulscc	%o4, %o1, %o4	! 8
	mulscc	%o4, %o1, %o4	! 9
	mulscc	%o4, %o1, %o4	! 10
	mulscc	%o4, %o1, %o4	! 11
	mulscc	%o4, %o1, %o4	! 12
	mulscc	%o4, %o1, %o4	! 13
	mulscc	%o4, %o1, %o4	! 14
	mulscc	%o4, %o1, %o4	! 15
	mulscc	%o4, %o1, %o4	! 16
	mulscc	%o4, %o1, %o4	! 17
	mulscc	%o4, %o1, %o4	! 18
	mulscc	%o4, %o1, %o4	! 19
	mulscc	%o4, %o1, %o4	! 20
	mulscc	%o4, %o1, %o4	! 21
	mulscc	%o4, %o1, %o4	! 22
	mulscc	%o4, %o1, %o4	! 23
	mulscc	%o4, %o1, %o4	! 24
	mulscc	%o4, %o1, %o4	! 25
	mulscc	%o4, %o1, %o4	! 26
	mulscc	%o4, %o1, %o4	! 27
	mulscc	%o4, %o1, %o4	! 28
	mulscc	%o4, %o1, %o4	! 29
	mulscc	%o4, %o1, %o4	! 30
	mulscc	%o4, %o1, %o4	! 31
	mulscc	%o4, %o1, %o4	! 32
	mulscc	%o4, %g0, %o4	! final shift


	/*
	 * Normally, with the shift-and-add approach, if both numbers are
	 * positive you get the correct result.  WIth 32-bit two's-complement
	 * numbers, -x is represented as
	 *
	 *		  x		    32
	 *	( 2  -  ------ ) mod 2  *  2
	 *		   32
	 *		  2
	 *
	 * (the `mod 2' subtracts 1 from 1.bbbb).  To avoid lots of 2^32s,
	 * we can treat this as if the radix point were just to the left
	 * of the sign bit (multiply by 2^32), and get
	 *
	 *	-x  =  (2 - x) mod 2
	 *
	 * Then, ignoring the `mod 2's for convenience:
	 *
	 *   x *  y	= xy
	 *  -x *  y	= 2y - xy
	 *   x * -y	= 2x - xy
	 *  -x * -y	= 4 - 2x - 2y + xy
	 *
	 * For signed multiplies, we subtract (x << 32) from the partial
	 * product to fix this problem for negative multipliers (see mul.s).
	 * Because of the way the shift into the partial product is calculated
	 * (N xor V), this term is automatically removed for the multiplicand,
	 * so we don't have to adjust.
	 *
	 * But for unsigned multiplies, the high order bit wasn't a sign bit,
	 * and the correction is wrong.  So for unsigned multiplies where the
	 * high order bit is one, we end up with xy - (y << 32).  To fix it
	 * we add y << 32.
	 */
	tst	%o1
	bl,a	1f		! if %o1 < 0 (high order bit = 1),
	add	%o4, %o0, %o4	! %o4 += %o0 (add y to upper half)
1:	rd	%y, %o0		! get lower half of product
	retl
	 addcc	%o4, %g0, %o1	! put upper half in place and set Z for %o1==0

Lumul_shortway:
	/*
	 * Short multiply.  12 steps, followed by a final shift step.
	 * The resulting bits are off by 12 and (32-12) = 20 bit positions,
	 * but there is no problem with %o0 being negative (unlike above),
	 * and overflow is impossible (the answer is at most 24 bits long).
	 */
	mulscc	%o4, %o1, %o4	! 1
	mulscc	%o4, %o1, %o4	! 2
	mulscc	%o4, %o1, %o4	! 3
	mulscc	%o4, %o1, %o4	! 4
	mulscc	%o4, %o1, %o4	! 5
	mulscc	%o4, %o1, %o4	! 6
	mulscc	%o4, %o1, %o4	! 7
	mulscc	%o4, %o1, %o4	! 8
	mulscc	%o4, %o1, %o4	! 9
	mulscc	%o4, %o1, %o4	! 10
	mulscc	%o4, %o1, %o4	! 11
	mulscc	%o4, %o1, %o4	! 12
	mulscc	%o4, %g0, %o4	! final shift

	/*
	 * %o4 has 20 of the bits that should be in the result; %y has
	 * the bottom 12 (as %y's top 12).  That is:
	 *
	 *	  %o4		    %y
	 * +----------------+----------------+
	 * | -12- |   -20-  | -12- |   -20-  |
	 * +------(---------+------)---------+
	 *	   -----result-----
	 *
	 * The 12 bits of %o4 left of the `result' area are all zero;
	 * in fact, all top 20 bits of %o4 are zero.
	 */

	rd	%y, %o5
	sll	%o4, 12, %o0	! shift middle bits left 12
	srl	%o5, 20, %o5	! shift low bits right 20
	or	%o5, %o0, %o0
	retl
	 addcc	%g0, %g0, %o1	! %o1 = zero, and set Z

/*
 * delay function
 *
 * void delay(N)  -- delay N microseconds
 *
 * Register usage: %o0 = "N" number of usecs to go (counts down to zero)
 *		   %o1 = "timerblurb" (stays constant)
 *		   %o2 = counter for 1 usec (counts down from %o1 to zero)
 *
 */

ENTRY(delay)			! %o0 = n
	subcc	%o0, %g0, %g0
	be	2f

	sethi	%hi(_C_LABEL(timerblurb)), %o1
	ld	[%o1 + %lo(_C_LABEL(timerblurb))], %o1	! %o1 = timerblurb

	 addcc	%o1, %g0, %o2		! %o2 = cntr (start @ %o1), clear CCs
					! first time through only

					! delay 1 usec
1:	bne	1b			! come back here if not done
	 subcc	%o2, 1, %o2		! %o2 = %o2 - 1 [delay slot]

	subcc	%o0, 1, %o0		! %o0 = %o0 - 1
	bne	1b			! done yet?
	 addcc	%o1, %g0, %o2		! reinit %o2 and CCs  [delay slot]
					! harmless if not branching
2:
	retl				! return
	 nop				! [delay slot]

#if defined(KGDB) || defined(DDB) || defined(DIAGNOSTIC)
/*
 * Write all windows (user or otherwise), except the current one.
 *
 * THIS COULD BE DONE IN USER CODE
 */
ENTRY(write_all_windows)
	/*
	 * g2 = g1 = nwindows - 1;
	 * while (--g1 > 0) save();
	 * while (--g2 > 0) restore();
	 */
	sethi	%hi(_C_LABEL(nwindows)), %g1
	ld	[%g1 + %lo(_C_LABEL(nwindows))], %g1
	dec	%g1
	mov	%g1, %g2

1:	deccc	%g1
	bg,a	1b
	 save	%sp, -64, %sp

2:	deccc	%g2
	bg,a	2b
	 restore

	retl
	nop
#endif /* KGDB */

ENTRY(setjmp)
	std	%sp, [%o0+0]	! stack pointer & return pc
	st	%fp, [%o0+8]	! frame pointer
	retl
	 clr	%o0

Lpanic_ljmp:
	.asciz	"longjmp botch"
	_ALIGN

ENTRY(longjmp)
	mov	1, %g6
	mov	%o0, %g1	! save a in another global register
	ld	[%g1+8], %g7	/* get caller's frame */
1:
	cmp	%fp, %g7	! compare against desired frame
	bl,a	1b		! if below,
	 restore		!    pop frame and loop
	be,a	2f		! if there,
	 ldd	[%g1+0], %o2	!    fetch return %sp and pc, and get out

Llongjmpbotch:
				! otherwise, went too far; bomb out
	save	%sp, -CCFSZ, %sp	/* preserve current window */
	sethi	%hi(Lpanic_ljmp), %o0
	call	_C_LABEL(panic)
	or %o0, %lo(Lpanic_ljmp), %o0;
	unimp	0

2:
	cmp	%o2, %sp	! %sp must not decrease
	bge,a	3f
	 mov	%o2, %sp	! it is OK, put it in place
	b,a	Llongjmpbotch
3:
	jmp	%o3 + 8		! success, return %g6
	 mov	%g6, %o0

	.data
#if defined(DDB) || NKSYMS > 0
	.globl	_C_LABEL(esym)
_C_LABEL(esym):
	.word	0
#endif
	.globl	_C_LABEL(cold)
_C_LABEL(cold):
	.word	1		! cold start flag

	.globl	_C_LABEL(proc0paddr)
_C_LABEL(proc0paddr):
	.word	_C_LABEL(u0)		! KVA of proc0 uarea

! StackGhost:  added 2 symbols to ease debugging
	.globl slowtrap
	.globl winuf_invalid

	.comm	_C_LABEL(nwindows), 4
	.comm	_C_LABEL(promvec), 4
