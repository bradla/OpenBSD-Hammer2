/*	$OpenBSD: cpuconf.h,v 1.7 2011/09/20 22:02:13 miod Exp $	*/
/*	$NetBSD: cpuconf.h,v 1.7 2003/05/23 00:57:24 ichiro Exp $	*/

/*
 * Copyright (c) 2002 Wasabi Systems, Inc.
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

#ifndef _ARM_CPUCONF_H_
#define	_ARM_CPUCONF_H_

/*
 * IF YOU CHANGE THIS FILE, MAKE SURE TO UPDATE THE DEFINITION OF
 * "PMAP_NEEDS_PTE_SYNC" IN <arm/arm/pmap.h> FOR THE CPU TYPE
 * YOU ARE ADDING SUPPORT FOR.
 */

/*
 * Determine which ARM architecture versions are configured.
 */
#if (defined(CPU_ARM8) || defined(CPU_ARM9) ||	\
     defined(CPU_SA1100) || defined(CPU_SA1110) || \
     defined(CPU_IXP12X0) || defined(CPU_XSCALE_IXP425))
#define	ARM_ARCH_4	1
#else
#define	ARM_ARCH_4	0
#endif

#if (defined(CPU_ARM9E) || defined(CPU_ARM10) || 			\
     defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) ||		\
     defined(CPU_XSCALE_PXA2X0))
#define	ARM_ARCH_5	1
#else
#define	ARM_ARCH_5	0
#endif

#if defined(CPU_ARM11)
#define ARM_ARCH_6     1
#else 
#define ARM_ARCH_6     0
#endif

#if defined(CPU_ARMv7)
#define ARM_ARCH_7     1
#else 
#define ARM_ARCH_7     0
#endif

/*
 * Define which MMU classes are configured:
 *
 *	ARM_MMU_GENERIC		Generic ARM MMU, compatible with ARM6.
 *
 *	ARM_MMU_SA1		StrongARM SA-1 MMU.  Compatible with generic
 *				ARM MMU, but has no write-through cache mode.
 *
 *	ARM_MMU_XSCALE		XScale MMU.  Compatible with generic ARM
 *				MMU, but also has several extensions which
 *				require different PTE layout to use.
 *      ARM_MMU_V7		v6/v7 MMU with XP bit enabled subpage
 *				protection is not used, TEX/AP is used instead.
 */

#if (defined(CPU_ARM8) || defined(CPU_ARM9) || defined(CPU_ARM9E) ||	\
     defined(CPU_ARM10) || defined(CPU_ARM11) || defined(CPU_ARMv7) )
#define	ARM_MMU_GENERIC		1
#else
#define	ARM_MMU_GENERIC		0
#endif

#if (defined(CPU_SA1100) || defined(CPU_SA1110) ||\
     defined(CPU_IXP12X0))
#define	ARM_MMU_SA1		1
#else
#define	ARM_MMU_SA1		0
#endif

#if (defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) ||		\
     defined(CPU_XSCALE_PXA2X0) || defined(CPU_XSCALE_IXP425))
#define	ARM_MMU_XSCALE		1
#else
#define	ARM_MMU_XSCALE		0
#endif

#if defined(CPU_ARMv7)
#define ARM_MMU_V7		1
#else
#define ARM_MMU_V7		0
#endif

#define	ARM_NMMUS		(ARM_MMU_GENERIC +	\
				 ARM_MMU_SA1 + ARM_MMU_XSCALE + ARM_MMU_V7)

/*
 * Define features that may be present on a subset of CPUs
 *
 *	ARM_XSCALE_PMU		Performance Monitoring Unit on 80200 and 80321
 */

#if (defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321))
#define ARM_XSCALE_PMU	1
#else
#define ARM_XSCALE_PMU	0
#endif

#endif /* _ARM_CPUCONF_H_ */
