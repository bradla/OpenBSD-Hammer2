/*	$OpenBSD: rm7000.h,v 1.3 2010/03/01 22:41:09 jasper Exp $ */

/*
 * Copyright (c) 2001-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _MIPS64_RM7000_H_
#define _MIPS64_RM7000_H_

/*
 *  QED RM7000 specific defines.
 */

/*
 *  Performance counters.
 */

#define	PCNT_SRC_CLOCKS		0x00	/* Clock cycles */
#define	PCNT_SRC_INSTR		0x01	/* Total instructions issued */
#define	PCNT_SRC_FPINSTR	0x02	/* Float instructions issued */
#define	PCNT_SRC_IINSTR		0x03	/* Integer instructions issued */
#define	PCNT_SRC_LOAD		0x04	/* Load instructions issued */
#define	PCNT_SRC_STORE		0x05	/* Store instructions issued */
#define	PCNT_SRC_DUAL		0x06	/* Dual issued pairs */
#define	PCNT_SRC_BRPREF		0x07	/* Branch prefetches */
#define	PCNT_SRC_EXTMISS	0x08	/* External cache misses */
#define	PCNT_SRC_STALL		0x09	/* Stall cycles */
#define	PCNT_SRC_SECMISS	0x0a	/* Secondary cache misses */
#define	PCNT_SRC_INSMISS	0x0b	/* Instruction cache misses */
#define	PCNT_SRC_DTAMISS	0x0c	/* Data cache misses */
#define	PCNT_SRC_DTLBMISS	0x0d	/* Data TLB misses */
#define	PCNT_SRC_ITLBMISS	0x0e	/* Instruction TLB misses */
#define	PCNT_SRC_JTLBIMISS	0x0f	/* Joint TLB instruction misses */
#define	PCNT_SRC_JTLBDMISS	0x10	/* Joint TLB data misses */
#define	PCNT_SRC_BRTAKEN	0x11	/* Branches taken */
#define	PCNT_SRC_BRISSUED	0x12	/* Branches issued */
#define	PCNT_SRC_SECWBACK	0x13	/* Secondary cache writebacks */
#define	PCNT_SRC_PRIWBACK	0x14	/* Primary cache writebacks */
#define	PCNT_SRC_DCSTALL	0x15	/* Dcache miss stall cycles */
#define	PCNT_SRC_MISS		0x16	/* Cache misses */
#define	PCNT_SRC_FPEXC		0x17	/* FP possible exception cycles */
#define	PCNT_SRC_MULSLIP	0x18	/* Slip cycles due to mult. busy */
#define	PCNT_SRC_CP0SLIP	0x19	/* CP0 Slip cycles */
#define	PCNT_SRC_LDSLIP		0x1a	/* Slip cycles  due to pend. non-b ld */
#define	PCNT_SRC_WBFULL		0x1b	/* Write buffer full stall cycles  */
#define	PCNT_SRC_CISTALL	0x1c	/* Cache instruction stall cycles  */
#define	PCNT_SRC_MULSTALL	0x1d	/* Multiplier stall cycles  */
#define	PCNT_SRC_ELDSTALL	0x1d	/* Excepion stall due to non-b ld */
#define	PCNT_SRC_MAX		0x1d	/* Maximum PCNT select code */

/*
 *  Counter control bits.
 */

#define	PCNT_CE			0x0400	/* Count enable */
#define	PCNT_UM			0x0200	/* Count in User mode */
#define	PCNT_KM			0x0100	/* Count in kernel mode */

/*
 *  Performance counter system call function codes.
 */
#define	PCNT_FNC_SELECT		0x0001	/* Select counter source */
#define	PCNT_FNC_READ		0x0002	/* Read current value of counter */


#ifdef _KERNEL
__BEGIN_DECLS
int	rm7k_perfcntr(int, long, long, long);
void	rm7k_perfintr(struct trap_frame *);
int	rm7k_watchintr(struct trap_frame *);
void	cp0_setperfcount(int);
void	cp0_setperfctrl(int);
int	cp0_getperfcount(void);
__END_DECLS
#endif /* _KERNEL */

#endif /* _MIPS64_RM7000_H_ */
