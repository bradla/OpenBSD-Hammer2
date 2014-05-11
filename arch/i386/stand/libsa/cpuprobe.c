/*	$OpenBSD: cpuprobe.c,v 1.2 2014/03/29 18:09:29 guenther Exp $	*/

/*
 * Copyright (c) 2004 Tom Cosgrove <tom.cosgrove@arches-consulting.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <machine/psl.h>
#include <machine/specialreg.h>

#include "libsa.h"

int amd64_supported;
static int cpu_family, cpu_model, cpu_stepping;
static int psl_check;
static u_int32_t feature_ecx, feature_edx, feature_amd;
static char cpu_brandstr[48];		/* Includes term NUL byte */
static char cpu_vendor[13];		/* 12 chars plus NUL term */

/*
 * cpuid instruction.  request in eax, result in eax, ebx, ecx, edx.
 * requires caller to provide u_int32_t regs[4] array.
 */
u_int32_t
cpuid(u_int32_t eax, u_int32_t *regs)
{
	__asm volatile(
	    "cpuid\n\t"
	    "movl	%%eax, 0(%2)\n\t"
	    "movl	%%ebx, 4(%2)\n\t"
	    "movl	%%ecx, 8(%2)\n\t"
	    "movl	%%edx, 12(%2)\n\t"
	    : "=a" (eax)
	    : "0" (eax), "S" (regs)
	    : "bx", "cx", "dx");

	return eax;
}

void
cpuprobe(void)
{
	u_int32_t cpuid_max, extended_max;
	u_int32_t regs[4];

	/*
	 * The following is a simple check to see if cpuid is supported.
	 * We try to toggle bit 21 (PSL_ID) in eflags.  If it works, then
	 * cpuid is supported.  If not, there's no cpuid, and we don't
	 * try it (don't want /boot to get an invalid opcode exception).
	 *
	 * XXX The NexGen Nx586 does not support this bit, so this is not
	 *     a good method to detect the presence of cpuid on this
	 *     processor.  That's fine: the purpose here is to detect the
	 *     absence of cpuid.  We don't mind if the instruction's not
	 *     there - this is not intended to determine exactly what
	 *     processor is there, just whether it's i386 or amd64.
	 *
	 *     The only thing that would cause us grief is a processor which
	 *     does not support cpuid but which does allow the PSL_ID bit
	 *     in eflags to be toggled.
	 */
	__asm volatile(
	    "pushfl\n\t"
	    "popl	%2\n\t"
	    "xorl	%2, %0\n\t"
	    "pushl	%0\n\t"
	    "popfl\n\t"
	    "pushfl\n\t"
	    "popl	%0\n\t"
	    "xorl	%2, %0\n\t"		/* If %2 == %0, no cpuid */
	    : "=r" (psl_check)
	    : "0" (PSL_ID), "r" (0)
	    : "cc");

	if (psl_check == PSL_ID) {			/* cpuid supported */
		cpuid_max = cpuid(0, regs);		/* Highest std call */

		bcopy(&regs[1], cpu_vendor, sizeof(regs[1]));
		bcopy(&regs[3], cpu_vendor + 4, sizeof(regs[3]));
		bcopy(&regs[2], cpu_vendor + 8, sizeof(regs[2]));
		cpu_vendor[sizeof(cpu_vendor) - 1] = '\0';

		if (cpuid_max >= 1) {
			u_int32_t id;

			id = cpuid(1, regs);		/* Get basic info */
			cpu_stepping = id & 0x000000f;
			cpu_model = (id >> 4) & 0x0000000f;
			cpu_family = (id >> 8) & 0x0000000f;

			feature_ecx = regs[2];
			feature_edx = regs[3];
		}

		extended_max = cpuid(0x80000000, regs);	/* Highest ext  */

		if (extended_max >= 0x80000001) {
			cpuid(0x80000001, regs);
			feature_amd = regs[3];
			if (feature_amd & CPUID_LONG)
				amd64_supported = 1;
		}

		cpu_brandstr[0] = '\0';
		if (extended_max >= 0x80000004) {
			u_int32_t brand_ints[12];

			cpuid(0x80000002, brand_ints);
			cpuid(0x80000003, brand_ints + 4);
			cpuid(0x80000004, brand_ints + 8);

			bcopy(brand_ints, cpu_brandstr,
			    sizeof(cpu_brandstr) - 1);

			cpu_brandstr[sizeof(cpu_brandstr) - 1] = '\0';
		}
	}

	printf("%s", amd64_supported ? " amd64" : " i386");
}

void
dump_cpuinfo(void)
{
	printf("\"%s\", family %d, model %d, step %d\n",
	    cpu_vendor, cpu_family, cpu_model, cpu_stepping);

	if (*cpu_brandstr)
		printf("%s\n", cpu_brandstr);

	printf("features: ecx 0x%x, edx 0x%x, amd 0x%x\n",
	    feature_ecx, feature_edx, feature_amd);

	printf("psl_check: 0x%x\n", psl_check);
}
