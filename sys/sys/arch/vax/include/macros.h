/*	$OpenBSD: macros.h,v 1.22 2014/03/29 18:09:30 guenther Exp $ */
/*	$NetBSD: macros.h,v 1.20 2000/07/19 01:02:52 matt Exp $	*/

/*
 * Copyright (c) 1994, 1998, 2000 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 /* All bugs are subject to removal without further notice */

#ifndef _MACHINE_MACROS_H_
#define _MACHINE_MACROS_H_

/* Here general macros are supposed to be stored */

static __inline__ int
ffs(int reg)
{
	register int val;

	__asm__ volatile ("ffs	$0,$32,%1,%0;"
		"	bneq	1f;"
		"	mnegl	$1,%0;"
		"1:	incl	%0"
			: "=&r" (val)
			: "r" (reg) );
	return	val;
}

static __inline__ size_t
strlen(const char *cp)
{
	register size_t ret;

	__asm__ volatile("locc $0,$65535,(%1);subl3 %%r0,$65535,%0"
			: "=r" (ret)
			: "r" (cp)
			: "r0","r1","cc" );
	return	ret;
}

#if 0
static __inline__ char *
strncat(char *cp, const char *c2, size_t count)
{
	__asm__ volatile("locc $0,%2,(%1);subl3 %%r0,%2,%%r2;"
			   "locc $0,$65535,(%0);movc3 %%r2,(%1),(%%r1);"
			   "movb $0,(%%r3)"
			:
			: "r" (cp), "r" (c2), "g"(count)
			: "r0","r1","r2","r3","r4","r5","memory","cc");
	return	cp;
}
#endif

static __inline__ char *
strncpy(char *cp, const char *c2, size_t len)
{
	__asm__ volatile("movl %2,%%r2;locc $0,%%r2,(%1);beql 1f;"
			   "subl3 %%r0,%2,%%r2;clrb (%0)[%%r2];1:"
			   "movc3 %%r2,(%1),(%0)"
			:
			: "r" (cp), "r" (c2), "g"(len)
			: "r0","r1","r2","r3","r4","r5","memory","cc");
	return	cp;
}

static __inline__ void *
memchr(const void *cp, int c, size_t len)
{
	void *ret;
	__asm__ volatile("locc %2,%3,(%1);bneq 1f;clrl %%r1;1:movl %%r1,%0"
			: "=g"(ret)
			: "r" (cp), "r" (c), "g"(len)
			: "r0","r1","cc");
	return	ret;
}

static __inline__ int
strcmp(const char *cp, const char *c2)
{
	register int ret;
	__asm__ volatile("locc $0,$65535,(%1);subl3 %%r0,$65535,%%r0;"
			   "incl %%r0;cmpc3 %%r0,(%1),(%2);beql 1f;"
			   "movl $1,%%r2;cmpb (%%r1),(%%r3);bcc 1f;"
			   "movl $-1,%%r2;1:movl %%r2,%0"
			: "=g"(ret)
			: "r" (cp), "r" (c2)
			: "r0","r1","r2","r3","cc");
	return	ret;
}
/* End nya */

#if 0 /* unused, but no point in deleting it since it _is_ an instruction */
static __inline__ int
locc(int mask, char *cp, size_t size)
{
	register ret;

	__asm__ volatile("locc %1,%2,(%3);movl %%r0,%0"
			: "=r" (ret)
			: "r" (mask),"r"(size),"r"(cp)
			: "r0","r1" );
	return	ret;
}
#endif

static __inline__ int
scanc(u_int size, const u_char *cp, const u_char *table, int mask)
{
	register int ret;

	__asm__ volatile("scanc %1,(%2),(%3),%4;movl %%r0,%0"
			: "=g"(ret)
			: "r"(size),"r"(cp),"r"(table),"r"(mask)
			: "r0","r1","r2","r3" );
	return ret;
}

static __inline__ int
skpc(int mask, size_t size, u_char *cp)
{
	register int ret;

	__asm__ volatile("skpc %1,%2,(%3);movl %%r0,%0"
			: "=g"(ret)
			: "r"(mask),"r"(size),"r"(cp)
			: "r0","r1" );
	return	ret;
}

#define	cpu_switchto(o, n) \
	__asm__ volatile( \
	    "movl %0, %%r0; movl %1, %%r1; movpsl -(%%sp); jsb __cpu_switchto" \
	    :: "g"(o), "g"(n) : "r0", "r1");

/*
 * Interlock instructions. Used both in multiprocessor environments to
 * lock between CPUs and in uniprocessor systems when locking is required
 * between I/O devices and the master CPU.
 */
/*
 * Insqti() locks and inserts an element into the end of a queue.
 * Returns -1 if interlock failed, 1 if inserted OK and 0 if first in queue.
 */
static __inline__ int
insqti(void *entry, void *header) {
	register int ret;

	__asm__ volatile(
		"	mnegl $1,%0;"
		"	insqti (%1),(%2);"
		"	bcs 1f;			# failed insert"
		"	beql 2f;		# jump if first entry"
		"	movl $1,%0;"
		"	brb 1f;"
		"2:	clrl %0;"
		"	1:;"
			: "=&g"(ret)
			: "r"(entry), "r"(header)
			: "memory");

	return ret;
}

/*
 * Remqhi() removes an element from the head of the queue.
 * Returns -1 if interlock failed, 0 if queue empty, address of the 
 * removed element otherwise.
 */
static __inline__ void *
remqhi(void *header) {
	register void *ret;

	__asm__ volatile(
		"	remqhi (%1),%0;"
		"	bcs 1f;			# failed interlock"
		"	bvs 2f;			# nothing was removed"
		"	brb 3f;"
		"1:	mnegl $1,%0;"
		"	brb 3f;"
		"2:	clrl %0;"
		"	3:;"
			: "=&g"(ret)
			: "r"(header)
			: "memory");

	return ret;
}
#define	ILCK_FAILED	-1	/* Interlock failed */
#define	Q_EMPTY		0	/* Queue is/was empty */
#define	Q_OK		1	/* Inserted OK */

#endif	/* _MACHINE_MACROS_H_ */
