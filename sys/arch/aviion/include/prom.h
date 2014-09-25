/*	$OpenBSD: prom.h,v 1.6 2013/10/10 21:24:59 miod Exp $	*/
/*
 * Copyright (c) 2006, Miodrag Vallat.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MACHINE_PROM_H_
#define _MACHINE_PROM_H_

#include <sys/cdefs.h>

/* SCM trap vector */
#define	SCM_VECTOR		496

/* system calls */
#define	SCM_CHAR		0x00
#define	SCM_OCHAR		0x20
#define	SCM_PTLINE		0x21
#define	SCM_OCRLF		0x26
#define	SCM_HALT		0x63
#define	SCM_STDIO		0x70
#define	SCM_JPSTART		0x100
#define	SCM_REBOOT		0x101
#define	SCM_CPUID		0x102
#define	SCM_MSIZE		0x103
#define	SCM_REVNUM		0x104
#define	SCM_HOSTID		0x107
#define	SCM_INVALID		0x112
#define	SCM_COMMID		0x114

/* 88204 PROMs only system calls */
#define	SCM_SYSID		0x31
#define	SCM_CPUCONFIG		0x107

/* SCM_JPSTART return values */
#define	JPSTART_OK		0
#define	JPSTART_NO_JP		1
#define	JPSTART_SINGLE_JP	2
#define	JPSTART_NOT_IDLE	3
#define	JPSTART_NO_ANSWER	4

struct	scm_cpuconfig {
	u_int32_t	version;
#define	SCM_CPUCONFIG_VERSION	0
	u_int32_t	cpucount;	/* # of CPUs */
	u_int16_t	igang, dgang;	/* # of CMMUs per CPU */
	u_int32_t	isplit, dsplit;	/* CMMU split bits */
	u_int32_t	:32;
};

int	scm_cpuconfig(struct scm_cpuconfig *);
u_int	scm_cpuid(void);
int	scm_getc(void);
void	scm_getenaddr(u_char *);
__dead void scm_halt(void);
u_int	scm_jpstart(cpuid_t, vaddr_t);
u_int	scm_memsize(int);
void	scm_printf(const char *);
u_int	scm_promver(void);
void	scm_putc(int);
void	scm_putcrlf(void);
__dead void scm_reboot(const char *);
u_int	scm_sysid(void);

/*
 * SCM boot device names
 */

/* cied */
#define	SCM_CIEN	0x6369656e
/* cimd */
/* cird */
/* cisc */
#define	SCM_DGEN	0x6467656e
#define	SCM_DGSC	0x64677363
/* hada */
#define	SCM_HKEN	0x686b656e
#define	SCM_INEN	0x696e656e
#define	SCM_INSC	0x696e7363
#define	SCM_NCSC	0x6e637363
/* nvrd */
/* pefn */
/* vitr */

#endif /* _MACHINE_PROM_H_ */
